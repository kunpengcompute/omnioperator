/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArraysOverlap function implementation for checking overlap between two arrays
 */

#include "ArraysOverlapFunction.h"
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_set>
#include "vectorization/SelectivityVector.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {

template <typename T>
struct FloatingPointEqual {
    static bool IsEqual(const T &a, const T &b)
    {
        return a == b;
    }
};

template <>
struct FloatingPointEqual<float> {
    static bool IsEqual(const float &a, const float &b)
    {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        return a == b;
    }
};

template <>
struct FloatingPointEqual<double> {
    static bool IsEqual(const double &a, const double &b)
    {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        return a == b;
    }
};

} // anonymous namespace

void ArraysOverlapFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArraysOverlapFunction Error:", "Expected 2 arguments");
    }

    auto rightArg = args.top();
    args.pop();
    auto leftArg = args.top();
    args.pop();

    if (leftArg == nullptr || rightArg == nullptr) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArraysOverlapFunction Error:", "Input vector is null");
    }

    if (leftArg->GetTypeId() != OMNI_ARRAY || rightArg->GetTypeId() != OMNI_ARRAY) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArraysOverlapFunction Error:", "Input is not an array type");
    }

    auto *leftArrayVec = dynamic_cast<ArrayVector *>(leftArg);
    auto *rightArrayVec = dynamic_cast<ArrayVector *>(rightArg);
    if (!leftArrayVec || !rightArrayVec) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArraysOverlapFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();

    result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_BOOLEAN, rowSize);
    if (!result) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArraysOverlapFunction Error:", "Failed to create result vector");
    }

    auto leftElementVec = leftArrayVec->GetElementVector();
    DataTypeId elementTypeId = leftElementVec->GetTypeId();

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessArraysOverlap<int8_t>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_SHORT:
            ProcessArraysOverlap<int16_t>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            ProcessArraysOverlap<int32_t>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            ProcessArraysOverlap<int64_t>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_FLOAT:
            ProcessArraysOverlap<float>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_DOUBLE:
            ProcessArraysOverlap<double>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_BOOLEAN:
            ProcessArraysOverlap<bool>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_DECIMAL128:
            ProcessArraysOverlap<Decimal128>(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            ProcessArraysOverlapVarchar(leftArrayVec, rightArrayVec, result, rowSize);
            break;
        default:
            delete leftArg;
            delete rightArg;
            OMNI_THROW("ArraysOverlapFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }

    delete leftArg;
    delete rightArg;
}

template <typename T>
void ArraysOverlapFunction::ProcessArraysOverlap(ArrayVector *leftArrayVec, ArrayVector *rightArrayVec,
    BaseVector *result, int32_t rowSize) const
{
    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    if (!resultVec) {
        OMNI_THROW("ArraysOverlapFunction Error:", "Result vector type mismatch");
    }

    auto leftElementVec = leftArrayVec->GetElementVector();
    auto rightElementVec = rightArrayVec->GetElementVector();
    auto *typedLeftElements = dynamic_cast<Vector<T> *>(leftElementVec.get());
    auto *typedRightElements = dynamic_cast<Vector<T> *>(rightElementVec.get());
    if (!typedLeftElements || !typedRightElements) {
        OMNI_THROW("ArraysOverlapFunction Error:", "Element vector type mismatch");
    }

    int64_t *leftOffsets = leftArrayVec->GetOffsets();
    int64_t *rightOffsets = rightArrayVec->GetOffsets();

    for (int32_t row = 0; row < rowSize; ++row) {
        if (leftArrayVec->IsNull(row) || rightArrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        int64_t leftStart = leftOffsets[row];
        int64_t leftEnd = leftOffsets[row + 1];
        int64_t rightStart = rightOffsets[row];
        int64_t rightEnd = rightOffsets[row + 1];

        int64_t leftLen = leftEnd - leftStart;
        int64_t rightLen = rightEnd - rightStart;

        if (leftLen == 0 || rightLen == 0) {
            resultVec->SetValue(row, false);
            continue;
        }

        bool hasNull = false;
        bool found = false;

        for (int64_t i = leftStart; i < leftEnd && !found; ++i) {
            if (typedLeftElements->IsNull(static_cast<int32_t>(i))) {
                hasNull = true;
                continue;
            }
            T leftVal = typedLeftElements->GetValue(static_cast<int32_t>(i));

            for (int64_t j = rightStart; j < rightEnd; ++j) {
                if (typedRightElements->IsNull(static_cast<int32_t>(j))) {
                    hasNull = true;
                    continue;
                }
                T rightVal = typedRightElements->GetValue(static_cast<int32_t>(j));
                if (FloatingPointEqual<T>::IsEqual(leftVal, rightVal)) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            resultVec->SetValue(row, true);
        } else if (hasNull) {
            resultVec->SetNull(row);
        } else {
            resultVec->SetValue(row, false);
        }
    }
}

void ArraysOverlapFunction::ProcessArraysOverlapVarchar(ArrayVector *leftArrayVec, ArrayVector *rightArrayVec,
    BaseVector *result, int32_t rowSize) const
{
    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    if (!resultVec) {
        OMNI_THROW("ArraysOverlapFunction Error:", "Result vector type mismatch");
    }

    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    auto leftElementVec = leftArrayVec->GetElementVector();
    auto rightElementVec = rightArrayVec->GetElementVector();
    auto *typedLeftElements = dynamic_cast<VarcharVector *>(leftElementVec.get());
    auto *typedRightElements = dynamic_cast<VarcharVector *>(rightElementVec.get());
    if (!typedLeftElements || !typedRightElements) {
        OMNI_THROW("ArraysOverlapFunction Error:", "Varchar element vector type mismatch");
    }

    int64_t *leftOffsets = leftArrayVec->GetOffsets();
    int64_t *rightOffsets = rightArrayVec->GetOffsets();

    for (int32_t row = 0; row < rowSize; ++row) {
        if (leftArrayVec->IsNull(row) || rightArrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        int64_t leftStart = leftOffsets[row];
        int64_t leftEnd = leftOffsets[row + 1];
        int64_t rightStart = rightOffsets[row];
        int64_t rightEnd = rightOffsets[row + 1];

        int64_t leftLen = leftEnd - leftStart;
        int64_t rightLen = rightEnd - rightStart;

        if (leftLen == 0 || rightLen == 0) {
            resultVec->SetValue(row, false);
            continue;
        }

        bool hasNull = false;
        std::unordered_set<std::string> rightSet;

        for (int64_t i = rightStart; i < rightEnd; ++i) {
            if (typedRightElements->IsNull(static_cast<int32_t>(i))) {
                hasNull = true;
                continue;
            }
            std::string_view val = typedRightElements->GetValue(static_cast<int32_t>(i));
            rightSet.insert(std::string(val));
        }

        bool found = false;
        for (int64_t i = leftStart; i < leftEnd; ++i) {
            if (typedLeftElements->IsNull(static_cast<int32_t>(i))) {
                hasNull = true;
                continue;
            }
            std::string_view val = typedLeftElements->GetValue(static_cast<int32_t>(i));
            if (rightSet.count(std::string(val)) > 0) {
                found = true;
                break;
            }
        }

        if (found) {
            resultVec->SetValue(row, true);
        } else if (hasNull) {
            resultVec->SetNull(row);
        } else {
            resultVec->SetValue(row, false);
        }
    }
}

template void ArraysOverlapFunction::ProcessArraysOverlap<int8_t>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<int16_t>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<int32_t>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<int64_t>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<float>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<double>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<bool>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;
template void ArraysOverlapFunction::ProcessArraysOverlap<Decimal128>(ArrayVector *, ArrayVector *, BaseVector *, int32_t) const;

} // namespace omniruntime::vectorization
