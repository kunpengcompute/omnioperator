/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayDistinct function implementation for removing duplicate elements from array
 */

#include "ArrayDistinctFunction.h"
#include "vector/vector_helper.h"
#include "type/string_Impl.h"
#include <unordered_set>
#include <cmath>
#include <functional>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {

struct FloatHash {
    size_t operator()(float val) const
    {
        if (std::isnan(val)) {
            return std::hash<float>{}(std::numeric_limits<float>::quiet_NaN());
        }
        return std::hash<float>{}(val);
    }
};

struct FloatEqual {
    bool operator()(float a, float b) const
    {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        return a == b;
    }
};

struct DoubleHash {
    size_t operator()(double val) const
    {
        if (std::isnan(val)) {
            return std::hash<double>{}(std::numeric_limits<double>::quiet_NaN());
        }
        return std::hash<double>{}(val);
    }
};

struct DoubleEqual {
    bool operator()(double a, double b) const
    {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        return a == b;
    }
};

} // anonymous namespace

template <typename T>
void ArrayDistinctFunction::ProcessTyped(ArrayVector *arrayVec, ArrayVector *resultArray,
    int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto inputElementVector = arrayVec->GetElementVector();
    auto *inputElemVec = dynamic_cast<Vector<T> *>(inputElementVector.get());
    int64_t *offsets = arrayVec->GetOffsets();

    int64_t totalUniqueCount = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            continue;
        }
        totalUniqueCount += (offsets[row + 1] - offsets[row]);
    }

    auto *newElementVector = new Vector<T>(totalUniqueCount > 0 ? totalUniqueCount : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];

        std::unordered_set<T> uniqueSet;
        bool hasNull = false;

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (inputElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!hasNull) {
                    hasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                T val = inputElemVec->GetValue(static_cast<int32_t>(i));
                if (uniqueSet.find(val) == uniqueSet.end()) {
                    uniqueSet.insert(val);
                    newElementVector->SetNotNull(newElementIndex);
                    newElementVector->SetValue(newElementIndex, val);
                    newElementIndex++;
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

template <>
void ArrayDistinctFunction::ProcessTyped<float>(ArrayVector *arrayVec, ArrayVector *resultArray,
    int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto inputElementVector = arrayVec->GetElementVector();
    auto *inputElemVec = dynamic_cast<Vector<float> *>(inputElementVector.get());
    int64_t *offsets = arrayVec->GetOffsets();

    int64_t totalUniqueCount = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            continue;
        }
        totalUniqueCount += (offsets[row + 1] - offsets[row]);
    }

    auto *newElementVector = new Vector<float>(totalUniqueCount > 0 ? totalUniqueCount : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];

        std::unordered_set<float, FloatHash, FloatEqual> uniqueSet;
        bool hasNull = false;

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (inputElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!hasNull) {
                    hasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                float val = inputElemVec->GetValue(static_cast<int32_t>(i));
                if (uniqueSet.find(val) == uniqueSet.end()) {
                    uniqueSet.insert(val);
                    newElementVector->SetNotNull(newElementIndex);
                    newElementVector->SetValue(newElementIndex, val);
                    newElementIndex++;
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

template <>
void ArrayDistinctFunction::ProcessTyped<double>(ArrayVector *arrayVec, ArrayVector *resultArray,
    int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto inputElementVector = arrayVec->GetElementVector();
    auto *inputElemVec = dynamic_cast<Vector<double> *>(inputElementVector.get());
    int64_t *offsets = arrayVec->GetOffsets();

    int64_t totalUniqueCount = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            continue;
        }
        totalUniqueCount += (offsets[row + 1] - offsets[row]);
    }

    auto *newElementVector = new Vector<double>(totalUniqueCount > 0 ? totalUniqueCount : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];

        std::unordered_set<double, DoubleHash, DoubleEqual> uniqueSet;
        bool hasNull = false;

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (inputElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!hasNull) {
                    hasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                double val = inputElemVec->GetValue(static_cast<int32_t>(i));
                if (uniqueSet.find(val) == uniqueSet.end()) {
                    uniqueSet.insert(val);
                    newElementVector->SetNotNull(newElementIndex);
                    newElementVector->SetValue(newElementIndex, val);
                    newElementIndex++;
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

void ArrayDistinctFunction::ProcessString(ArrayVector *arrayVec, ArrayVector *resultArray,
    int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto inputElementVector = arrayVec->GetElementVector();
    auto *inputStrVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        inputElementVector.get());
    int64_t *offsets = arrayVec->GetOffsets();

    int64_t totalElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!arrayVec->IsNull(row)) {
            totalElements += (offsets[row + 1] - offsets[row]);
        }
    }

    auto *newElementVector = new Vector<LargeStringContainer<std::string_view>>(
        totalElements > 0 ? totalElements : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];

        std::unordered_set<std::string> uniqueSet;
        bool hasNull = false;

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (inputElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!hasNull) {
                    hasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                std::string_view sv = inputStrVec->GetValue(static_cast<int32_t>(i));
                std::string val(sv.data(), sv.size());
                if (uniqueSet.find(val) == uniqueSet.end()) {
                    uniqueSet.insert(val);
                    newElementVector->SetNotNull(newElementIndex);
                    newElementVector->SetValue(newElementIndex, sv);
                    newElementIndex++;
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

void ArrayDistinctFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("ArrayDistinctFunction Error:", "No arguments provided");
    }

    auto inputArg = args.top();
    args.pop();

    if (inputArg == nullptr) {
        OMNI_THROW("ArrayDistinctFunction Error:", "Input vector is null");
    }

    if (inputArg->GetTypeId() != OMNI_ARRAY) {
        delete inputArg;
        OMNI_THROW("ArrayDistinctFunction Error:", "Input is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(inputArg);
    if (arrayVec == nullptr) {
        delete inputArg;
        OMNI_THROW("ArrayDistinctFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto inputElementVector = arrayVec->GetElementVector();

    auto *resultArray = new ArrayVector(rowSize, context->GetPool());

    if (inputElementVector == nullptr) {
        for (int32_t row = 0; row < rowSize; ++row) {
            resultArray->SetOffset(row, 0);
            if (arrayVec->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetNotNull(row);
            }
        }
        resultArray->SetOffset(rowSize, 0);
        result = resultArray;
        delete inputArg;
        return;
    }

    DataTypeId elementTypeId = inputElementVector->GetTypeId();

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessTyped<int8_t>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_SHORT:
            ProcessTyped<int16_t>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_INT:
            ProcessTyped<int32_t>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_LONG:
            ProcessTyped<int64_t>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_FLOAT:
            ProcessTyped<float>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_DOUBLE:
            ProcessTyped<double>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_BOOLEAN:
            ProcessTyped<bool>(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            ProcessString(arrayVec, resultArray, rowSize, context->GetPool());
            break;
        default:
            delete resultArray;
            delete inputArg;
            OMNI_THROW("ArrayDistinctFunction Error:", "Unsupported element type");
    }

    result = resultArray;
    delete inputArg;
}

} // namespace omniruntime::vectorization
