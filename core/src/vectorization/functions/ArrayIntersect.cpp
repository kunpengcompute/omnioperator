/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayIntersect function implementation
 */

#include "ArrayIntersect.h"
#include "type/decimal128.h"
#include <iostream>
#include <unordered_set>
#include <cmath>
#include <string>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {

template <typename T>
struct NaNAwareHash {
    size_t operator()(T v) const
    {
        if (std::isnan(v)) {
            return 0;
        }
        return std::hash<T>{}(v);
    }
};

template <typename T>
struct NaNAwareEqual {
    bool operator()(T a, T b) const
    {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        return a == b;
    }
};

struct Decimal128Hash {
    size_t operator()(const Decimal128 &v) const
    {
        auto h1 = std::hash<int64_t>{}(v.HighBits());
        auto h2 = std::hash<uint64_t>{}(v.LowBits());
        return h1 ^ (h2 << 1);
    }
};

/// Core intersection logic for primitive types (int, long, float, double, bool, Decimal128).
/// Uses unordered_set with customizable Hash and Equal for NaN-aware float/double handling.
/// Returns the final offset (total elements written to resultElem).
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
int64_t ProcessTypedRows(ArrayVector *leftArray, BaseVector *leftElem,
    ArrayVector *rightArray, BaseVector *rightElem,
    ArrayVector *resultArray, BaseVector *resultElem,
    int32_t rowSize)
{
    auto *typedLeftElem = dynamic_cast<Vector<T> *>(leftElem);
    auto *typedRightElem = dynamic_cast<Vector<T> *>(rightElem);
    auto *typedResultElem = dynamic_cast<Vector<T> *>(resultElem);
    if (typedLeftElem == nullptr || typedRightElem == nullptr || typedResultElem == nullptr) {
        OMNI_THROW("ArrayIntersect error:", "Failed to cast element vectors to expected type");
    }

    int64_t currentOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, currentOffset);

        if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }
        resultArray->SetNotNull(row);

        int64_t rightOff = rightArray->GetOffset(row);
        int64_t rightSz = rightArray->GetSize(row);
        std::unordered_set<T, Hash, Equal> rightSet;
        bool rightHasNull = false;

        for (int64_t i = rightOff; i < rightOff + rightSz; ++i) {
            if (rightElem->IsNull(static_cast<int32_t>(i))) {
                rightHasNull = true;
            } else {
                rightSet.insert(typedRightElem->GetValue(static_cast<int32_t>(i)));
            }
        }

        int64_t leftOff = leftArray->GetOffset(row);
        int64_t leftSz = leftArray->GetSize(row);
        std::unordered_set<T, Hash, Equal> outputSet;
        bool outputHasNull = false;

        for (int64_t i = leftOff; i < leftOff + leftSz; ++i) {
            if (leftElem->IsNull(static_cast<int32_t>(i))) {
                if (rightHasNull && !outputHasNull) {
                    resultElem->SetNull(static_cast<int32_t>(currentOffset));
                    currentOffset++;
                    outputHasNull = true;
                }
            } else {
                T val = typedLeftElem->GetValue(static_cast<int32_t>(i));
                if (rightSet.count(val) > 0 && outputSet.insert(val).second) {
                    resultElem->SetNotNull(static_cast<int32_t>(currentOffset));
                    typedResultElem->SetValue(static_cast<int32_t>(currentOffset), val);
                    currentOffset++;
                }
            }
        }
    }
    resultArray->SetOffset(rowSize, currentOffset);
    return currentOffset;
}

/// Intersection logic for string/varchar/varbinary types.
/// Uses std::string in sets because string_view hash/comparison works on content.
int64_t ProcessStringRows(ArrayVector *leftArray, BaseVector *leftElem,
    ArrayVector *rightArray, BaseVector *rightElem,
    ArrayVector *resultArray, BaseVector *resultElem,
    int32_t rowSize)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    auto *typedLeftElem = dynamic_cast<StringVector *>(leftElem);
    auto *typedRightElem = dynamic_cast<StringVector *>(rightElem);
    auto *typedResultElem = dynamic_cast<StringVector *>(resultElem);
    if (typedLeftElem == nullptr || typedRightElem == nullptr || typedResultElem == nullptr) {
        OMNI_THROW("ArrayIntersect error:", "Failed to cast element vectors to string type");
    }

    int64_t currentOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, currentOffset);

        if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }
        resultArray->SetNotNull(row);

        int64_t rightOff = rightArray->GetOffset(row);
        int64_t rightSz = rightArray->GetSize(row);
        std::unordered_set<std::string> rightSet;
        bool rightHasNull = false;

        for (int64_t i = rightOff; i < rightOff + rightSz; ++i) {
            if (rightElem->IsNull(static_cast<int32_t>(i))) {
                rightHasNull = true;
            } else {
                std::string_view sv = typedRightElem->GetValue(static_cast<int32_t>(i));
                rightSet.insert(std::string(sv));
            }
        }

        int64_t leftOff = leftArray->GetOffset(row);
        int64_t leftSz = leftArray->GetSize(row);
        std::unordered_set<std::string> outputSet;
        bool outputHasNull = false;

        for (int64_t i = leftOff; i < leftOff + leftSz; ++i) {
            if (leftElem->IsNull(static_cast<int32_t>(i))) {
                if (rightHasNull && !outputHasNull) {
                    resultElem->SetNull(static_cast<int32_t>(currentOffset));
                    currentOffset++;
                    outputHasNull = true;
                }
            } else {
                std::string_view sv = typedLeftElem->GetValue(static_cast<int32_t>(i));
                std::string val(sv);
                if (rightSet.count(val) > 0 && outputSet.insert(val).second) {
                    resultElem->SetNotNull(static_cast<int32_t>(currentOffset));
                    typedResultElem->SetValue(static_cast<int32_t>(currentOffset), sv);
                    currentOffset++;
                }
            }
        }
    }
    resultArray->SetOffset(rowSize, currentOffset);
    return currentOffset;
}

} // anonymous namespace

void ArrayIntersectImpl::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    auto *rightArg = args.top();
    args.pop();
    auto rightGuard = MakeVectorArgGuard(rightArg);
    auto *leftArg = args.top();
    args.pop();
    auto leftGuard = MakeVectorArgGuard(leftArg);

    auto *leftArray = dynamic_cast<ArrayVector *>(leftArg);
    auto *rightArray = dynamic_cast<ArrayVector *>(rightArg);
    if (leftArray == nullptr || rightArray == nullptr) {
        OMNI_THROW("ArrayIntersect error:", "Both arguments must be ARRAY type");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto leftElementVector = leftArray->GetElementVector();
    auto rightElementVector = rightArray->GetElementVector();

    DataTypeId elementTypeId = OMNI_NONE;
    if (leftElementVector != nullptr) {
        elementTypeId = leftElementVector->GetTypeId();
    } else if (rightElementVector != nullptr) {
        elementTypeId = rightElementVector->GetTypeId();
    }

    int64_t maxElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!leftArray->IsNull(row) && !rightArray->IsNull(row)) {
            maxElements += leftArray->GetSize(row);
        }
    }

    auto *resultArray = new ArrayVector(rowSize);
    BaseVector *newElementVector = nullptr;

    if (elementTypeId != OMNI_NONE) {
        if (elementTypeId == OMNI_VARCHAR || elementTypeId == OMNI_CHAR || elementTypeId == OMNI_VARBINARY) {
            newElementVector = VectorHelper::CreateStringVector(maxElements > 0 ? maxElements : 1);
        } else {
            newElementVector = VectorHelper::CreateFlatVector(elementTypeId, maxElements > 0 ? maxElements : 1);
        }
    }

    if (newElementVector != nullptr && leftElementVector != nullptr && rightElementVector != nullptr) {
        switch (elementTypeId) {
            case OMNI_BYTE:
                ProcessTypedRows<int8_t>(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_SHORT:
                ProcessTypedRows<int16_t>(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                ProcessTypedRows<int32_t>(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                ProcessTypedRows<int64_t>(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_FLOAT:
                ProcessTypedRows<float, NaNAwareHash<float>, NaNAwareEqual<float>>(
                    leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_DOUBLE:
                ProcessTypedRows<double, NaNAwareHash<double>, NaNAwareEqual<double>>(
                    leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_BOOLEAN:
                ProcessTypedRows<bool>(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY:
                ProcessStringRows(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            case OMNI_DECIMAL128:
                ProcessTypedRows<Decimal128, Decimal128Hash>(leftArray, leftElementVector.get(),
                    rightArray, rightElementVector.get(), resultArray, newElementVector, rowSize);
                break;
            default:
                OMNI_THROW("ArrayIntersect error:",
                    "Unsupported element type: " + std::to_string(elementTypeId));
        }
    } else {
        for (int32_t row = 0; row < rowSize; ++row) {
            resultArray->SetOffset(row, 0);
            if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetNotNull(row);
            }
        }
        resultArray->SetOffset(rowSize, 0);
    }

    if (newElementVector != nullptr) {
        resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
    } else if (elementTypeId != OMNI_NONE) {
        VectorHelper::EmptyArrayProjection(resultArray, elementTypeId);
    }

    result = resultArray;
}

} // namespace omniruntime::vectorization
