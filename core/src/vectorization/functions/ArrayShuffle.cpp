/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayShuffle function implementation
 */

#include "ArrayShuffle.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <random>
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

int64_t GetSeedFromVector(BaseVector *seedVec)
{
    if (seedVec->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<int64_t> *>(seedVec)->GetConstValue();
    }
    return dynamic_cast<Vector<int64_t> *>(seedVec)->GetValue(0);
}

} // anonymous namespace

void ArrayShuffleImpl::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    bool hasSeed = (args.size() >= 2);

    BaseVector *seedArg = nullptr;
    if (hasSeed) {
        seedArg = args.top();
        args.pop();
    }
    auto seedGuard = MakeVectorArgGuard(seedArg);

    auto *arrayArg = args.top();
    args.pop();
    auto arrayGuard = MakeVectorArgGuard(arrayArg);

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVec == nullptr) {
        OMNI_THROW("ArrayShuffle error:", "First argument must be ARRAY type");
    }

    int64_t seed = 0;
    if (hasSeed && seedArg != nullptr && !seedArg->IsNull(0)) {
        seed = GetSeedFromVector(seedArg);
    } else {
        std::random_device rd;
        seed = static_cast<int64_t>(rd());
    }

    std::mt19937 randGen(static_cast<uint32_t>(seed));

    int32_t rowSize = context->GetResultRowSize();
    auto inputElementVector = arrayVec->GetElementVector();

    int64_t totalElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!arrayVec->IsNull(row)) {
            totalElements += arrayVec->GetSize(row);
        }
    }

    BaseVector *newElementVector = nullptr;
    if (inputElementVector != nullptr && totalElements > 0) {
        DataTypeId elemTypeId = inputElementVector->GetTypeId();
        newElementVector = CreateElementVectorByType(elemTypeId, totalElements);
    } else {
        newElementVector = VectorHelper::CreateFlatVector(OMNI_INT, 1);
    }

    auto *resultArray = new ArrayVector(rowSize);

    int64_t currentOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, static_cast<int32_t>(currentOffset));

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }
        resultArray->SetNotNull(row);

        int64_t arrSize = arrayVec->GetSize(row);
        int64_t arrOffset = arrayVec->GetOffset(row);

        if (arrSize == 0) {
            continue;
        }

        std::vector<int64_t> indices(static_cast<size_t>(arrSize));
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), randGen);

        for (int64_t i = 0; i < arrSize; ++i) {
            int32_t srcIdx = static_cast<int32_t>(arrOffset + indices[static_cast<size_t>(i)]);
            int32_t dstIdx = static_cast<int32_t>(currentOffset + i);
            CopyElementValue(inputElementVector.get(), srcIdx, newElementVector, dstIdx);
        }

        currentOffset += arrSize;
    }
    resultArray->SetOffset(rowSize, static_cast<int32_t>(currentOffset));

    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
    result = resultArray;
}

} // namespace omniruntime::vectorization
