/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TRY arithmetic vector functions
 */

#include "vectorization/functions/TryArithmetic.h"

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "type/decimal_operations.h"
#include "util/debug.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
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
            OMNI_THROW("TryArithmetic Error:", "Unsupported input vector encoding");
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
CheckedResult<T> EvalPrimitive(T left, T right, TryArithmeticOp operation)
{
    CheckedResult<T> result;
    if constexpr (std::is_integral_v<T>) {
        bool overflow = false;
        switch (operation) {
            case TryArithmeticOp::ADD:
                overflow = __builtin_add_overflow(left, right, &result.value);
                break;
            case TryArithmeticOp::SUBTRACT:
                overflow = __builtin_sub_overflow(left, right, &result.value);
                break;
            case TryArithmeticOp::MULTIPLY:
                overflow = __builtin_mul_overflow(left, right, &result.value);
                break;
            case TryArithmeticOp::DIVIDE:
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
        }
        if (overflow) {
            result.error = ArithmeticError::OVERFLOW;
        }
        return result;
    }

    switch (operation) {
        case TryArithmeticOp::ADD:
            result.value = left + right;
            break;
        case TryArithmeticOp::SUBTRACT:
            result.value = left - right;
            break;
        case TryArithmeticOp::MULTIPLY:
            result.value = left * right;
            break;
        case TryArithmeticOp::DIVIDE:
            if (right == static_cast<T>(0)) {
                result.error = ArithmeticError::DIVIDE_BY_ZERO;
                return result;
            }
            result.value = left / right;
            break;
    }
    return result;
}

const DecimalDataType &GetDecimalType(const BaseVector *vector)
{
    const auto &dataType = vector->GetDataType();
    auto *decimalType = dataType == nullptr ? nullptr : dynamic_cast<DecimalDataType *>(dataType.get());
    if (decimalType == nullptr) {
        OMNI_THROW("TryArithmetic Error:", "Decimal precision and scale metadata is missing");
    }
    return *decimalType;
}

const DecimalDataType &GetDecimalType(const DataTypePtr &dataType)
{
    auto *decimalType = dataType == nullptr ? nullptr : dynamic_cast<DecimalDataType *>(dataType.get());
    if (decimalType == nullptr) {
        OMNI_THROW("TryArithmetic Error:", "Decimal output precision and scale metadata is missing");
    }
    return *decimalType;
}

void ValidateDecimalPhysicalType(DataTypeId physicalType, const DecimalDataType &decimalType)
{
    const bool isDecimal64 = physicalType == OMNI_DECIMAL64;
    const bool isDecimal128 = physicalType == OMNI_DECIMAL128;
    OMNI_CHECK(isDecimal64 || isDecimal128, "TRY decimal requires a decimal physical type");
    OMNI_CHECK(decimalType.GetId() == physicalType,
        "TRY decimal metadata type does not match its vector physical type");
    OMNI_CHECK(isDecimal64 == (decimalType.GetPrecision() <= MAX_DECIMAL64_DIGITS),
        "TRY decimal physical type does not match its precision");
}

void ValidateDecimalVector(const BaseVector *vector)
{
    ValidateDecimalPhysicalType(vector->GetTypeId(), GetDecimalType(vector));
}

Decimal128Wrapper<> ReadDecimal(BaseVector *vector, int32_t row)
{
    if (vector->GetTypeId() == OMNI_DECIMAL64) {
        return Decimal128Wrapper<>(ReadValue<int64_t>(vector, row));
    }
    return Decimal128Wrapper<>(ReadValue<Decimal128>(vector, row));
}

CheckedResult<Decimal128Wrapper<>> EvalDecimal(BaseVector *leftVector, BaseVector *rightVector, int32_t row,
    const DecimalDataType &outputType, TryArithmeticOp operation)
{
    const auto &leftType = GetDecimalType(leftVector);
    const auto &rightType = GetDecimalType(rightVector);
    auto left = ReadDecimal(leftVector, row).SetScale(leftType.GetScale());
    auto right = ReadDecimal(rightVector, row).SetScale(rightType.GetScale());

    Decimal128Wrapper<> value;
    auto outputScale = outputType.GetScale();
    switch (operation) {
        case TryArithmeticOp::ADD:
            DecimalOperations::InternalDecimalAddWithResultScale(left, leftType.GetScale(), leftType.GetPrecision(),
                right, rightType.GetScale(), rightType.GetPrecision(), outputScale, value);
            break;
        case TryArithmeticOp::SUBTRACT:
            DecimalOperations::InternalDecimalSubtractWithResultScale(left, leftType.GetScale(),
                leftType.GetPrecision(), right, rightType.GetScale(), rightType.GetPrecision(), outputScale, value);
            break;
        case TryArithmeticOp::MULTIPLY:
            DecimalOperations::InternalDecimalMultiplyWithResultScale(left, leftType.GetScale(),
                leftType.GetPrecision(), right, rightType.GetScale(), rightType.GetPrecision(), outputScale, value);
            break;
        case TryArithmeticOp::DIVIDE:
            DecimalOperations::InternalDecimalDivide(left, leftType.GetScale(), leftType.GetPrecision(), right,
                rightType.GetScale(), rightType.GetPrecision(), value, outputScale);
            break;
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
    TryArithmeticOp operation)
{
    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(left, row) || IsNullAt(right, row)) {
            result->SetNull(row);
            continue;
        }
        const auto checked = EvalPrimitive(ReadValue<T>(left, row), ReadValue<T>(right, row), operation);
        if (!checked.ok()) {
            result->SetNull(row);
            continue;
        }
        WriteValue<T>(result, row, checked.value);
    }
}

void ApplyDecimal(BaseVector *left, BaseVector *right, const DataTypePtr &outputType, BaseVector *result,
    int32_t rowSize, TryArithmeticOp operation)
{
    const auto &decimalOutputType = GetDecimalType(outputType);
    const bool outputIsDecimal64 = outputType->GetId() == OMNI_DECIMAL64;
    ValidateDecimalPhysicalType(outputType->GetId(), decimalOutputType);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(left, row) || IsNullAt(right, row)) {
            result->SetNull(row);
            continue;
        }
        const auto checked = EvalDecimal(left, right, row, decimalOutputType, operation);
        if (!checked.ok()) {
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

void RegisterPrimitiveTryFunction(const std::string &name, TryArithmeticOp operation, DataTypeId inputType,
    DataTypeId outputType)
{
    VectorFunction::RegisterVectorFunction(name, {inputType, inputType}, outputType,
        std::make_shared<TryArithmeticFunction>(operation));
}

} // namespace

void TryArithmeticFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    OMNI_CHECK(args.size() == 2, "TRY arithmetic requires exactly two arguments");
    std::unique_ptr<BaseVector> right(args.top());
    args.pop();
    std::unique_ptr<BaseVector> left(args.top());
    args.pop();

    const bool leftIsDecimal = left->GetTypeId() == OMNI_DECIMAL64 || left->GetTypeId() == OMNI_DECIMAL128;
    const bool rightIsDecimal = right->GetTypeId() == OMNI_DECIMAL64 || right->GetTypeId() == OMNI_DECIMAL128;
    const bool outputIsDecimal =
        outputType->GetId() == OMNI_DECIMAL64 || outputType->GetId() == OMNI_DECIMAL128;
    OMNI_CHECK(left->GetTypeId() == right->GetTypeId() || (leftIsDecimal && rightIsDecimal),
        "TRY arithmetic input types are incompatible");
    OMNI_CHECK(leftIsDecimal == rightIsDecimal && leftIsDecimal == outputIsDecimal,
        "TRY arithmetic input and output type families are incompatible");

    if (leftIsDecimal && rightIsDecimal) {
        ValidateDecimalVector(left.get());
        ValidateDecimalVector(right.get());
        ValidateDecimalPhysicalType(outputType->GetId(), GetDecimalType(outputType));
    }

    const auto rowSize = context->GetResultRowSize();
    auto resultHolder = std::unique_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(outputType->GetId(), rowSize));
    if (outputType->GetId() == OMNI_DECIMAL64 || outputType->GetId() == OMNI_DECIMAL128) {
        VectorHelper::SetVectorDataType(resultHolder.get(), outputType.get());
        ApplyDecimal(left.get(), right.get(), outputType, resultHolder.get(), rowSize, operation_);
        result = resultHolder.release();
        return;
    }

    OMNI_CHECK(left->GetTypeId() == outputType->GetId(), "TRY primitive arithmetic must preserve physical type");
    switch (outputType->GetId()) {
        case OMNI_BYTE:
            ApplyPrimitive<int8_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_);
            break;
        case OMNI_SHORT:
            ApplyPrimitive<int16_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_);
            break;
        case OMNI_INT:
            ApplyPrimitive<int32_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_);
            break;
        case OMNI_LONG:
            ApplyPrimitive<int64_t>(left.get(), right.get(), resultHolder.get(), rowSize, operation_);
            break;
        case OMNI_FLOAT:
            ApplyPrimitive<float>(left.get(), right.get(), resultHolder.get(), rowSize, operation_);
            break;
        case OMNI_DOUBLE:
            ApplyPrimitive<double>(left.get(), right.get(), resultHolder.get(), rowSize, operation_);
            break;
        default:
            OMNI_THROW("TryArithmetic Error:", "Unsupported primitive type");
    }
    result = resultHolder.release();
}

void RegisterTryArithmeticFunctions(const std::string &prefix)
{
    const std::vector<std::pair<std::string, TryArithmeticOp>> functions = {
        {prefix + "try_add", TryArithmeticOp::ADD},
        {prefix + "try_subtract", TryArithmeticOp::SUBTRACT},
        {prefix + "try_multiply", TryArithmeticOp::MULTIPLY},
        {prefix + "try_divide", TryArithmeticOp::DIVIDE},
    };
    const std::vector<DataTypeId> primitiveTypes = {
        OMNI_BYTE, OMNI_SHORT, OMNI_INT, OMNI_LONG, OMNI_FLOAT, OMNI_DOUBLE,
    };
    const std::vector<DataTypeId> decimalTypes = {OMNI_DECIMAL64, OMNI_DECIMAL128};

    for (const auto &[name, operation] : functions) {
        for (const auto type : primitiveTypes) {
            RegisterPrimitiveTryFunction(name, operation, type, type);
        }
        for (const auto leftType : decimalTypes) {
            for (const auto rightType : decimalTypes) {
                for (const auto outputType : decimalTypes) {
                    VectorFunction::RegisterVectorFunction(name, {leftType, rightType}, outputType,
                        std::make_shared<TryArithmeticFunction>(operation));
                }
            }
        }
    }
}

} // namespace omniruntime::vectorization
