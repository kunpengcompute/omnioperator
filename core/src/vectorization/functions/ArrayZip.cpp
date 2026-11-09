/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayZip function implementation
 */

#include "ArrayZip.h"
#include <iostream>
#include <algorithm>
#include <vector>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {

BaseVector *CreateElementVectorByType(DataTypeId typeId, int64_t size)
{
    int64_t allocSize = size > 0 ? size : 1;
    if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
        return VectorHelper::CreateStringVector(static_cast<uint32_t>(allocSize));
    }
    return VectorHelper::CreateFlatVector(typeId, static_cast<int32_t>(allocSize));
}

void CopyElementValue(BaseVector *srcElem, int32_t srcIdx, BaseVector *dstElem, int32_t dstIdx)
{
    if (srcElem->IsNull(srcIdx)) {
        dstElem->SetNull(dstIdx);
    } else {
        dstElem->SetNotNull(dstIdx);
        VectorHelper::CopyValue(srcElem, srcIdx, dstElem, dstIdx);
    }
}

} // anonymous namespace

void ArrayZipImpl::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    // Pop exactly numArrays_ arguments from the stack (reverse order)
    std::vector<BaseVector *> inputArgs(numArrays_);
    std::vector<VectorArgGuard> argGuards;
    argGuards.reserve(numArrays_);
    for (int32_t i = numArrays_ - 1; i >= 0; --i) {
        if (args.empty()) {
            OMNI_THROW("ArrayZip error:", "Not enough arguments on stack");
        }
        inputArgs[i] = args.top();
        args.pop();
        argGuards.emplace_back(MakeVectorArgGuard(inputArgs[i]));
    }

    // Cast all inputs to ArrayVector
    std::vector<ArrayVector *> inputArrays(numArrays_);
    for (int32_t i = 0; i < numArrays_; ++i) {
        inputArrays[i] = dynamic_cast<ArrayVector *>(inputArgs[i]);
        if (inputArrays[i] == nullptr) {
            OMNI_THROW("ArrayZip error:", "All arguments must be ARRAY type");
        }
    }

    int32_t rowSize = context->GetResultRowSize();

    // Get element vectors and their type ids from each input array
    std::vector<std::shared_ptr<BaseVector>> inputElemVectors(numArrays_);
    std::vector<DataTypeId> elementTypeIds(numArrays_, OMNI_NONE);
    for (int32_t i = 0; i < numArrays_; ++i) {
        inputElemVectors[i] = inputArrays[i]->GetElementVector();
        if (inputElemVectors[i] != nullptr) {
            elementTypeIds[i] = inputElemVectors[i]->GetTypeId();
        }
    }

    // First pass: calculate total elements (max array size per non-null row)
    int64_t totalElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        bool anyNull = false;
        for (int32_t i = 0; i < numArrays_; ++i) {
            if (inputArrays[i]->IsNull(row)) {
                anyNull = true;
                break;
            }
        }
        if (!anyNull) {
            int64_t maxSize = 0;
            for (int32_t i = 0; i < numArrays_; ++i) {
                maxSize = std::max(maxSize, static_cast<int64_t>(inputArrays[i]->GetSize(row)));
            }
            totalElements += maxSize;
        }
    }

    // Create child vectors for the result RowVector (one per input array)
    int64_t allocSize = totalElements > 0 ? totalElements : 1;
    std::vector<std::shared_ptr<BaseVector>> childVectors(numArrays_);
    for (int32_t i = 0; i < numArrays_; ++i) {
        if (elementTypeIds[i] != OMNI_NONE) {
            childVectors[i] = std::shared_ptr<BaseVector>(
                CreateElementVectorByType(elementTypeIds[i], allocSize));
        } else {
            childVectors[i] = std::shared_ptr<BaseVector>(
                VectorHelper::CreateFlatVector(OMNI_INT, static_cast<int32_t>(allocSize)));
        }
    }

    auto *resultArray = new ArrayVector(rowSize);

    // Second pass: fill data
    int64_t currentOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, static_cast<int32_t>(currentOffset));

        bool anyNull = false;
        for (int32_t i = 0; i < numArrays_; ++i) {
            if (inputArrays[i]->IsNull(row)) {
                anyNull = true;
                break;
            }
        }

        if (anyNull) {
            resultArray->SetNull(row);
            continue;
        }
        resultArray->SetNotNull(row);

        int64_t maxSize = 0;
        for (int32_t i = 0; i < numArrays_; ++i) {
            maxSize = std::max(maxSize, static_cast<int64_t>(inputArrays[i]->GetSize(row)));
        }

        for (int64_t j = 0; j < maxSize; ++j) {
            int32_t dstIdx = static_cast<int32_t>(currentOffset + j);

            for (int32_t i = 0; i < numArrays_; ++i) {
                int64_t arrOff = inputArrays[i]->GetOffset(row);
                int64_t arrSz = inputArrays[i]->GetSize(row);

                if (j < arrSz && inputElemVectors[i] != nullptr) {
                    int32_t srcIdx = static_cast<int32_t>(arrOff + j);
                    CopyElementValue(inputElemVectors[i].get(), srcIdx,
                        childVectors[i].get(), dstIdx);
                } else {
                    childVectors[i]->SetNull(dstIdx);
                }
            }
        }

        currentOffset += maxSize;
    }
    resultArray->SetOffset(rowSize, static_cast<int32_t>(currentOffset));

    // Build the RowVector from child vectors and set as element of result array
    auto *rowVector = new RowVector(static_cast<int32_t>(allocSize), childVectors);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(rowVector));

    result = resultArray;
}

} // namespace omniruntime::vectorization
