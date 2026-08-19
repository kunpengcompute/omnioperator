/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Binary arithmetic vector function
 */

#include "vectorization/functions/TryArithmetic.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

#include "type/decimal_operations.h"
#include "util/debug.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::expressions;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

enum class ArithmeticError {
    NONE,
    OVERFLOW,
    DIVIDE_BY_ZERO,
};

template <typename T>
struct CheckedResult {
    T value{};
    ArithmeticError error{ArithmeticError::NONE};

    bool ok() const
    {
        return error == ArithmeticError::NONE;
    }
};

int32_t RowIndex(BaseVector *vector, int32_t row)
{
    return vector->GetEncoding() == OMNI_ENCODING_CONST ? 0 : row;
}

bool IsNullAt(BaseVector *vector, int32_t row)
{
    return vector->IsNull(RowIndex(vector, row));
}

template <typename T>
T ReadValue(BaseVector *vector, int32_t row)
{
    const auto index = RowIndex(vector, row);
    switch (vector->GetEncoding()) {
        case OMNI_FLAT:
            return static_cast<Vector<T> *>(vector)->GetValue(index);
        case OMNI_DICTIONARY:
            return static_cast<Vector<DictionaryContainer<T>> *>(vector)->GetValue(index);
        case OMNI_ENCODING_CONST:
            return static_cast<ConstVector<T> *>(vector)->GetConstValue();
        default:
            OMNI_THROW("BinaryArithmetic Error:", "Unsupported input vector encoding");
    }
    return T{};
}

template <typename T>
void WriteValue(BaseVector *result, int32_t row, const T &value)
{
    auto *flatResult = static_cast<Vector<T> *>(result);
    flatResult->SetValue(row, value);
    flatResult->SetNotNull(row);
}

template <typename T>
T FromUnsignedBits(std::make_unsigned_t<T> value)
{
    T result;
    static_assert(sizeof(result) == sizeof(value));
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

template <typename T>
T WrapAdd(T left, T right)
{
    using Unsigned = std::make_unsigned_t<T>;
    const auto value = static_cast<uint64_t>(static_cast<Unsigned>(left)) +
        static_cast<uint64_t>(static_cast<Unsigned>(right));
    return FromUnsignedBits<T>(static_cast<Unsigned>(value));
}

template <typename T>
T WrapSubtract(T left, T right)
{
    using Unsigned = std::make_unsigned_t<T>;
    const auto value = static_cast<uint64_t>(static_cast<Unsigned>(left)) -
        static_cast<uint64_t>(static_cast<Unsigned>(right));
    return FromUnsignedBits<T>(static_cast<Unsigned>(value));
}

template <typename T>
T WrapMultiply(T left, T right)
{
    using Unsigned = std::make_unsigned_t<T>;
    const auto value = static_cast<uint64_t>(static_cast<Unsigned>(left)) *
        static_cast<uint64_t>(static_cast<Unsigned>(right));
    return FromUnsignedBits<T>(static_cast<Unsigned>(value));
}

template <typename T>
CheckedResult<T> EvalPrimitive(T left, T right, ArithmeticOp operation, ArithmeticEvalMode evalMode)
{
    CheckedResult<T> result;
    if constexpr (std::is_integral_v<T>) {
        if (evalMode == ArithmeticEvalMode::LEGACY) {
            switch (operation) {
                case ArithmeticOp::ADD:
                    result.value = WrapAdd(left, right);
                    return result;
                case ArithmeticOp::SUBTRACT:
                    result.value = WrapSubtract(left, right);
                    return result;
                case ArithmeticOp::MULTIPLY:
                    result.value = WrapMultiply(left, right);
                    return result;
                case ArithmeticOp::DIVIDE:
                    if (right == 0) {
                        result.error = ArithmeticError::DIVIDE_BY_ZERO;
                    } else if (left == std::numeric_limits<T>::min() && right == static_cast<T>(-1)) {
                        result.value = left;
                    } else {
                        result.value = left / right;
                    }
                    return result;
                case ArithmeticOp::INVALID:
                    OMNI_THROW("BinaryArithmetic Error:", "Invalid arithmetic operation");
            }
        }

        bool overflow = false;
        switch (operation) {
            case ArithmeticOp::ADD:
                overflow = __builtin_add_overflow(left, right, &result.value);
                break;
            case ArithmeticOp::SUBTRACT:
                overflow = __builtin_sub_overflow(left, right, &result.value);
                break;
            case ArithmeticOp::MULTIPLY:
                overflow = __builtin_mul_overflow(left, right, &result.value);
                break;
            case ArithmeticOp::DIVIDE:
                if (right == 0) {
                    result.error = ArithmeticError::DIVIDE_BY_ZERO;
                    return result;
                }
                if (left == std::numeric_limits<T>::min() && right == static_cast<T>(-1)) {
                    result.error = ArithmeticError::OVERFLOW;
                    return result;
                }
                result.value = left / right;
                break;
            case ArithmeticOp::INVALID:
                OMNI_THROW("BinaryArithmetic Error:", "Invalid arithmetic operation");
        }
        if (overflow) {
            result.error = ArithmeticError::OVERFLOW;
        }
        return result;
    }

    switch (operation) {
        case ArithmeticOp::ADD:
            result.value = left + right;
            break;
        case ArithmeticOp::SUBTRACT:
            result.value = left - right;
            break;
        case ArithmeticOp::MULTIPLY:
            result.value = left * right;
            break;
        case ArithmeticOp::DIVIDE:
            if (right == static_cast<T>(0)) {
                result.error = ArithmeticError::DIVIDE_BY_ZERO;
                return result;
            }
            result.value = left / right;
            break;
        case ArithmeticOp::INVALID:
            OMNI_THROW("BinaryArithmetic Error:", "Invalid arithmetic operation");
    }
    return result;
}

const DecimalDataType &GetDecimalType(const DataTypePtr &dataType)
{
    auto *decimalType = dataType == nullptr ? nullptr : dynamic_cast<DecimalDataType *>(dataType.get());
    if (decimalType == nullptr) {
        OMNI_THROW("BinaryArithmetic Error:", "Expression decimal type is missing precision and scale");
    }
    return *decimalType;
}

void ValidateDecimalPhysicalType(DataTypeId physicalType, const DecimalDataType &decimalType)
{
    OMNI_CHECK(decimalType.GetId() == OMNI_DECIMAL64 || decimalType.GetId() == OMNI_DECIMAL128,
        "Binary arithmetic logical decimal type has an invalid type id");
    const bool isDecimal64 = decimalType.GetId() == OMNI_DECIMAL64;
    const bool compatibleDecimal64Storage = physicalType == OMNI_DECIMAL64 || physicalType == OMNI_LONG;
    const bool compatibleDecimal128Storage = physicalType == OMNI_DECIMAL128;
    OMNI_CHECK((isDecimal64 && compatibleDecimal64Storage) ||
        (!isDecimal64 && compatibleDecimal128Storage),
        "Binary arithmetic decimal type does not match its vector storage type");
    OMNI_CHECK(isDecimal64 == (decimalType.GetPrecision() <= MAX_DECIMAL64_DIGITS),
        "Binary arithmetic decimal type does not match its precision");
}

void ValidateDecimalVector(const BaseVector *vector, const DecimalDataType &logicalType)
{
    ValidateDecimalPhysicalType(vector->GetTypeId(), logicalType);
    const auto &metadata = vector->GetDataType();
    auto *vectorDecimalType = metadata == nullptr ? nullptr : dynamic_cast<DecimalDataType *>(metadata.get());
    if (vectorDecimalType != nullptr) {
        OMNI_CHECK(*vectorDecimalType == logicalType,
            "Binary arithmetic vector decimal metadata does not match expression type");
    }
}

Decimal128Wrapper<> ReadDecimal(BaseVector *vector, int32_t row, const DecimalDataType &logicalType)
{
    if (logicalType.GetId() == OMNI_DECIMAL64) {
        return Decimal128Wrapper<>(ReadValue<int64_t>(vector, row));
    }
    return Decimal128Wrapper<>(ReadValue<Decimal128>(vector, row));
}

CheckedResult<Decimal128Wrapper<>> EvalDecimal(BaseVector *leftVector, BaseVector *rightVector, int32_t row,
    const DecimalDataType &leftType, const DecimalDataType &rightType, const DecimalDataType &outputType,
    ArithmeticOp operation)
{
    auto left = ReadDecimal(leftVector, row, leftType).SetScale(leftType.GetScale());
    auto right = ReadDecimal(rightVector, row, rightType).SetScale(rightType.GetScale());

    Decimal128Wrapper<> value;
    auto outputScale = outputType.GetScale();
    switch (operation) {
        case ArithmeticOp::ADD:
            DecimalOperations::InternalDecimalAddWithResultScale(left, leftType.GetScale(), leftType.GetPrecision(),
                right, rightType.GetScale(), rightType.GetPrecision(), outputScale, value);
            break;
        case ArithmeticOp::SUBTRACT:
            DecimalOperations::InternalDecimalSubtractWithResultScale(left, leftType.GetScale(),
                leftType.GetPrecision(), right, rightType.GetScale(), rightType.GetPrecision(), outputScale, value);
            break;
        case ArithmeticOp::MULTIPLY:
            DecimalOperations::InternalDecimalMultiplyWithResultScale(left, leftType.GetScale(),
                leftType.GetPrecision(), right, rightType.GetScale(), rightType.GetPrecision(), outputScale, value);
            break;
        case ArithmeticOp::DIVIDE:
            DecimalOperations::InternalDecimalDivide(left, leftType.GetScale(), leftType.GetPrecision(), right,
                rightType.GetScale(), rightType.GetPrecision(), value, outputScale);
            break;
        case ArithmeticOp::INVALID:
            OMNI_THROW("BinaryArithmetic Error:", "Invalid arithmetic operation");
    }

    CheckedResult<Decimal128Wrapper<>> result;
    const auto status = value.IsOverflow(outputType.GetPrecision());
    if (status == OpStatus::DIVIDE_BY_ZERO) {
        result.error = ArithmeticError::DIVIDE_BY_ZERO;
    } else if (status != OpStatus::SUCCESS) {
        result.error = ArithmeticError::OVERFLOW;
    } else {
        result.value = value;
    }
    return result;
}

template <typename T>
void ApplyPrimitive(BaseVector *left, BaseVector *right, BaseVector *result, int32_t rowSize,
    ArithmeticOp operation, ArithmeticEvalMode evalMode)
{
    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(left, row) || IsNullAt(right, row)) {
            result->SetNull(row);
            continue;
        }
        const auto checked = EvalPrimitive(
            ReadValue<T>(left, row), ReadValue<T>(right, row), operation, evalMode);
        if (!checked.ok()) {
            if (evalMode == ArithmeticEvalMode::ANSI) {
                OMNI_THROW("BinaryArithmetic Error:", "ANSI arithmetic failed");
            }
            result->SetNull(row);
            continue;
        }
        WriteValue<T>(result, row, checked.value);
    }
}

void ApplyDecimal(BaseVector *left, BaseVector *right, const DecimalDataType &leftType,
    const DecimalDataType &rightType, const DecimalDataType &outputType, BaseVector *result,
    int32_t rowSize, ArithmeticOp operation, ArithmeticEvalMode evalMode)
{
    const bool outputIsDecimal64 = outputType.GetId() == OMNI_DECIMAL64;
    ValidateDecimalPhysicalType(outputType.GetId(), outputType);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(left, row) || IsNullAt(right, row)) {
            result->SetNull(row);
            continue;
        }
        const auto checked = EvalDecimal(left, right, row, leftType, rightType, outputType, operation);
        if (!checked.ok()) {
            if (evalMode == ArithmeticEvalMode::ANSI) {
                OMNI_THROW("BinaryArithmetic Error:", "ANSI decimal arithmetic failed");
            }
            result->SetNull(row);
            continue;
        }
        if (outputIsDecimal64) {
            WriteValue<int64_t>(result, row, static_cast<int64_t>(checked.value.ToInt128()));
        } else {
            WriteValue<Decimal128>(result, row, checked.value.ToDecimal128());
        }
    }
}

DataTypePtr CloneArithmeticType(const DataTypePtr &dataType)
{
    OMNI_CHECK(dataType != nullptr, "Binary arithmetic expression type must not be null");
    if (dataType->GetId() == OMNI_DECIMAL64) {
        const auto &decimalType = GetDecimalType(dataType);
        return std::make_shared<Decimal64DataType>(decimalType.GetPrecision(), decimalType.GetScale());
    }
    if (dataType->GetId() == OMNI_DECIMAL128) {
        const auto &decimalType = GetDecimalType(dataType);
        return std::make_shared<Decimal128DataType>(decimalType.GetPrecision(), decimalType.GetScale());
    }
    return std::make_shared<DataType>(dataType->GetId());
}

bool IsPrimitiveNumeric(DataTypeId typeId)
{
    return typeId == OMNI_BYTE || typeId == OMNI_SHORT || typeId == OMNI_INT || typeId == OMNI_LONG ||
        typeId == OMNI_FLOAT || typeId == OMNI_DOUBLE;
}

} // namespace

BinaryArithmeticFunction::BinaryArithmeticFunction(ArithmeticOp operation, ArithmeticEvalMode evalMode,
    const DataTypePtr &leftType, const DataTypePtr &rightType, const DataTypePtr &resultType)
    : operation_(operation), evalMode_(evalMode), leftType_(CloneArithmeticType(leftType)),
      rightType_(CloneArithmeticType(rightType)), resultType_(CloneArithmeticType(resultType))
{}

void BinaryArithmeticFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    OMNI_CHECK(args.size() == 2, "Binary arithmetic requires exactly two arguments");
    OMNI_CHECK(outputType != nullptr && *outputType == *resultType_,
        "Binary arithmetic output type does not match expression result type");
    std::unique_ptr<BaseVector> right(args.top());
    args.pop();
    std::unique_ptr<BaseVector> left(args.top());
    args.pop();

    const bool leftIsDecimal = leftType_->isDecimal();
    const bool rightIsDecimal = rightType_->isDecimal();
    const bool outputIsDecimal = resultType_->isDecimal();
    OMNI_CHECK(leftIsDecimal == rightIsDecimal && leftIsDecimal == outputIsDecimal,
        "Binary arithmetic input and output type families are incompatible");

    if (leftIsDecimal && rightIsDecimal) {
        ValidateDecimalVector(left.get(), GetDecimalType(leftType_));
        ValidateDecimalVector(right.get(), GetDecimalType(rightType_));
        ValidateDecimalPhysicalType(resultType_->GetId(), GetDecimalType(resultType_));
    } else {
        OMNI_CHECK(left->GetTypeId() == leftType_->GetId() && right->GetTypeId() == rightType_->GetId(),
            "Binary arithmetic primitive vector storage does not match expression type");
    }

    const auto rowSize = context->GetResultRowSize();
    auto resultHolder = std::unique_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(resultType_->GetId(), rowSize));
    if (outputIsDecimal) {
        VectorHelper::SetVectorDataType(resultHolder.get(), resultType_.get());
        ApplyDecimal(left.get(), right.get(), GetDecimalType(leftType_), GetDecimalType(rightType_),
            GetDecimalType(resultType_), resultHolder.get(), rowSize, operation_, evalMode_);
        result = resultHolder.release();
        return;
    }

    OMNI_CHECK(leftType_->GetId() == rightType_->GetId() && leftType_->GetId() == resultType_->GetId(),
        "Binary arithmetic primitive types must match");
    switch (resultType_->GetId()) {
        case OMNI_BYTE:
            ApplyPrimitive<int8_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_, evalMode_);
            break;
        case OMNI_SHORT:
            ApplyPrimitive<int16_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_, evalMode_);
            break;
        case OMNI_INT:
            ApplyPrimitive<int32_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_, evalMode_);
            break;
        case OMNI_LONG:
            ApplyPrimitive<int64_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_, evalMode_);
            break;
        case OMNI_FLOAT:
            ApplyPrimitive<float>(left.get(), right.get(), resultHolder.get(), rowSize, operation_, evalMode_);
            break;
        case OMNI_DOUBLE:
            ApplyPrimitive<double>(left.get(), right.get(), resultHolder.get(), rowSize, operation_, evalMode_);
            break;
        default:
            OMNI_THROW("BinaryArithmetic Error:", "Unsupported primitive type");
    }
    result = resultHolder.release();
}

std::shared_ptr<VectorFunction> CreateBinaryArithmeticFunction(ArithmeticOp operation,
    ArithmeticEvalMode evalMode, const DataTypePtr &leftType, const DataTypePtr &rightType,
    const DataTypePtr &resultType)
{
    if (operation == ArithmeticOp::INVALID || leftType == nullptr || rightType == nullptr || resultType == nullptr) {
        return nullptr;
    }
    const bool allDecimal = leftType->isDecimal() && rightType->isDecimal() && resultType->isDecimal();
    const bool samePrimitive = IsPrimitiveNumeric(leftType->GetId()) && leftType->GetId() == rightType->GetId() &&
        leftType->GetId() == resultType->GetId();
    if (!allDecimal && !samePrimitive) {
        return nullptr;
    }
    return std::make_shared<BinaryArithmeticFunction>(operation, evalMode, leftType, rightType, resultType);
}

} // namespace omniruntime::vectorization
