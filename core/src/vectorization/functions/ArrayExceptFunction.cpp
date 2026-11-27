/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayExcept function implementation for computing set difference of two arrays
 */

#include "ArrayExceptFunction.h"
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
void ArrayExceptFunction::ProcessTyped(ArrayVector *leftArray, ArrayVector *rightArray,
    ArrayVector *resultArray, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto leftElementVector = leftArray->GetElementVector();
    auto rightElementVector = rightArray->GetElementVector();
    auto *leftElemVec = dynamic_cast<Vector<T> *>(leftElementVector.get());
    auto *rightElemVec = dynamic_cast<Vector<T> *>(rightElementVector.get());
    int64_t *leftOffsets = leftArray->GetOffsets();
    int64_t *rightOffsets = rightArray->GetOffsets();

    int64_t maxElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!leftArray->IsNull(row) && !rightArray->IsNull(row)) {
            maxElements += (leftOffsets[row + 1] - leftOffsets[row]);
        }
    }

    auto *newElementVector = new Vector<T>(maxElements > 0 ? maxElements : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t rightStart = rightOffsets[row];
        int64_t rightEnd = rightOffsets[row + 1];

        std::unordered_set<T> rightSet;
        bool rightHasNull = false;

        for (int64_t i = rightStart; i < rightEnd; ++i) {
            if (rightElementVector->IsNull(static_cast<int32_t>(i))) {
                rightHasNull = true;
            } else {
                rightSet.insert(rightElemVec->GetValue(static_cast<int32_t>(i)));
            }
        }

        int64_t leftStart = leftOffsets[row];
        int64_t leftEnd = leftOffsets[row + 1];

        std::unordered_set<T> outputSet;
        bool outputHasNull = false;

        for (int64_t i = leftStart; i < leftEnd; ++i) {
            if (leftElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!outputHasNull && !rightHasNull) {
                    outputHasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                T val = leftElemVec->GetValue(static_cast<int32_t>(i));
                if (rightSet.find(val) == rightSet.end()) {
                    if (outputSet.find(val) == outputSet.end()) {
                        outputSet.insert(val);
                        newElementVector->SetNotNull(newElementIndex);
                        newElementVector->SetValue(newElementIndex, val);
                        newElementIndex++;
                    }
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

template <>
void ArrayExceptFunction::ProcessTyped<float>(ArrayVector *leftArray, ArrayVector *rightArray,
    ArrayVector *resultArray, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto leftElementVector = leftArray->GetElementVector();
    auto rightElementVector = rightArray->GetElementVector();
    auto *leftElemVec = dynamic_cast<Vector<float> *>(leftElementVector.get());
    auto *rightElemVec = dynamic_cast<Vector<float> *>(rightElementVector.get());
    int64_t *leftOffsets = leftArray->GetOffsets();
    int64_t *rightOffsets = rightArray->GetOffsets();

    int64_t maxElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!leftArray->IsNull(row) && !rightArray->IsNull(row)) {
            maxElements += (leftOffsets[row + 1] - leftOffsets[row]);
        }
    }

    auto *newElementVector = new Vector<float>(maxElements > 0 ? maxElements : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t rightStart = rightOffsets[row];
        int64_t rightEnd = rightOffsets[row + 1];

        std::unordered_set<float, FloatHash, FloatEqual> rightSet;
        bool rightHasNull = false;

        for (int64_t i = rightStart; i < rightEnd; ++i) {
            if (rightElementVector->IsNull(static_cast<int32_t>(i))) {
                rightHasNull = true;
            } else {
                rightSet.insert(rightElemVec->GetValue(static_cast<int32_t>(i)));
            }
        }

        int64_t leftStart = leftOffsets[row];
        int64_t leftEnd = leftOffsets[row + 1];

        std::unordered_set<float, FloatHash, FloatEqual> outputSet;
        bool outputHasNull = false;

        for (int64_t i = leftStart; i < leftEnd; ++i) {
            if (leftElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!outputHasNull && !rightHasNull) {
                    outputHasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                float val = leftElemVec->GetValue(static_cast<int32_t>(i));
                if (rightSet.find(val) == rightSet.end()) {
                    if (outputSet.find(val) == outputSet.end()) {
                        outputSet.insert(val);
                        newElementVector->SetNotNull(newElementIndex);
                        newElementVector->SetValue(newElementIndex, val);
                        newElementIndex++;
                    }
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

template <>
void ArrayExceptFunction::ProcessTyped<double>(ArrayVector *leftArray, ArrayVector *rightArray,
    ArrayVector *resultArray, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto leftElementVector = leftArray->GetElementVector();
    auto rightElementVector = rightArray->GetElementVector();
    auto *leftElemVec = dynamic_cast<Vector<double> *>(leftElementVector.get());
    auto *rightElemVec = dynamic_cast<Vector<double> *>(rightElementVector.get());
    int64_t *leftOffsets = leftArray->GetOffsets();
    int64_t *rightOffsets = rightArray->GetOffsets();

    int64_t maxElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!leftArray->IsNull(row) && !rightArray->IsNull(row)) {
            maxElements += (leftOffsets[row + 1] - leftOffsets[row]);
        }
    }

    auto *newElementVector = new Vector<double>(maxElements > 0 ? maxElements : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t rightStart = rightOffsets[row];
        int64_t rightEnd = rightOffsets[row + 1];

        std::unordered_set<double, DoubleHash, DoubleEqual> rightSet;
        bool rightHasNull = false;

        for (int64_t i = rightStart; i < rightEnd; ++i) {
            if (rightElementVector->IsNull(static_cast<int32_t>(i))) {
                rightHasNull = true;
            } else {
                rightSet.insert(rightElemVec->GetValue(static_cast<int32_t>(i)));
            }
        }

        int64_t leftStart = leftOffsets[row];
        int64_t leftEnd = leftOffsets[row + 1];

        std::unordered_set<double, DoubleHash, DoubleEqual> outputSet;
        bool outputHasNull = false;

        for (int64_t i = leftStart; i < leftEnd; ++i) {
            if (leftElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!outputHasNull && !rightHasNull) {
                    outputHasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                double val = leftElemVec->GetValue(static_cast<int32_t>(i));
                if (rightSet.find(val) == rightSet.end()) {
                    if (outputSet.find(val) == outputSet.end()) {
                        outputSet.insert(val);
                        newElementVector->SetNotNull(newElementIndex);
                        newElementVector->SetValue(newElementIndex, val);
                        newElementIndex++;
                    }
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

void ArrayExceptFunction::ProcessString(ArrayVector *leftArray, ArrayVector *rightArray,
    ArrayVector *resultArray, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto leftElementVector = leftArray->GetElementVector();
    auto rightElementVector = rightArray->GetElementVector();
    auto *leftStrVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        leftElementVector.get());
    auto *rightStrVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        rightElementVector.get());
    int64_t *leftOffsets = leftArray->GetOffsets();
    int64_t *rightOffsets = rightArray->GetOffsets();

    int64_t maxElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!leftArray->IsNull(row) && !rightArray->IsNull(row)) {
            maxElements += (leftOffsets[row + 1] - leftOffsets[row]);
        }
    }

    auto *newElementVector = new Vector<LargeStringContainer<std::string_view>>(
        maxElements > 0 ? maxElements : 1, pool);
    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t rightStart = rightOffsets[row];
        int64_t rightEnd = rightOffsets[row + 1];

        std::unordered_set<std::string> rightSet;
        bool rightHasNull = false;

        for (int64_t i = rightStart; i < rightEnd; ++i) {
            if (rightElementVector->IsNull(static_cast<int32_t>(i))) {
                rightHasNull = true;
            } else {
                std::string_view sv = rightStrVec->GetValue(static_cast<int32_t>(i));
                rightSet.emplace(sv.data(), sv.size());
            }
        }

        int64_t leftStart = leftOffsets[row];
        int64_t leftEnd = leftOffsets[row + 1];

        std::unordered_set<std::string> outputSet;
        bool outputHasNull = false;

        for (int64_t i = leftStart; i < leftEnd; ++i) {
            if (leftElementVector->IsNull(static_cast<int32_t>(i))) {
                if (!outputHasNull && !rightHasNull) {
                    outputHasNull = true;
                    newElementVector->SetNull(newElementIndex);
                    newElementIndex++;
                }
            } else {
                std::string_view sv = leftStrVec->GetValue(static_cast<int32_t>(i));
                std::string val(sv.data(), sv.size());
                if (rightSet.find(val) == rightSet.end()) {
                    if (outputSet.find(val) == outputSet.end()) {
                        outputSet.insert(val);
                        newElementVector->SetNotNull(newElementIndex);
                        newElementVector->SetValue(newElementIndex, sv);
                        newElementIndex++;
                    }
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
}

void ArrayExceptFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArrayExceptFunction Error:", "Requires exactly 2 arguments");
    }

    auto *rightArg = args.top();
    args.pop();
    auto *leftArg = args.top();
    args.pop();

    if (leftArg == nullptr || rightArg == nullptr) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArrayExceptFunction Error:", "Input vector is null");
    }

    if (leftArg->GetTypeId() != OMNI_ARRAY || rightArg->GetTypeId() != OMNI_ARRAY) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArrayExceptFunction Error:", "Input is not an array type");
    }

    auto *leftArray = dynamic_cast<ArrayVector *>(leftArg);
    auto *rightArray = dynamic_cast<ArrayVector *>(rightArg);
    if (leftArray == nullptr || rightArray == nullptr) {
        delete leftArg;
        delete rightArg;
        OMNI_THROW("ArrayExceptFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto leftElementVector = leftArray->GetElementVector();

    auto *resultArray = new ArrayVector(rowSize, context->GetPool());

    if (leftElementVector == nullptr) {
        for (int32_t row = 0; row < rowSize; ++row) {
            resultArray->SetOffset(row, 0);
            if (leftArray->IsNull(row) || rightArray->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetNotNull(row);
            }
        }
        resultArray->SetOffset(rowSize, 0);
        result = resultArray;
        delete leftArg;
        delete rightArg;
        return;
    }

    DataTypeId elementTypeId = leftElementVector->GetTypeId();

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessTyped<int8_t>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_SHORT:
            ProcessTyped<int16_t>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_INT:
            ProcessTyped<int32_t>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_LONG:
            ProcessTyped<int64_t>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_FLOAT:
            ProcessTyped<float>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_DOUBLE:
            ProcessTyped<double>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_BOOLEAN:
            ProcessTyped<bool>(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            ProcessString(leftArray, rightArray, resultArray, rowSize, context->GetPool());
            break;
        default:
            delete resultArray;
            delete leftArg;
            delete rightArg;
            OMNI_THROW("ArrayExceptFunction Error:", "Unsupported element type");
    }

    result = resultArray;
    delete leftArg;
    delete rightArg;
}

} // namespace omniruntime::vectorization
