/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Cardinality function for Array and Map types
 */

#include "CardinalityFunction.h"
#include "vectorization/SelectivityVector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vectorization/VectorReaders.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

void CardinalityFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("CardinalityFunction Error:", "No arguments provided");
    }
    auto inputArg = args.top();
    args.pop();
    auto inputGuard = MakeVectorArgGuard(inputArg);
    if (inputArg == nullptr) {
        OMNI_THROW("CardinalityFunction Error:", "Input vector is null");
    }
    int32_t rowSize = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_LONG, rowSize);
    auto *resultVec = static_cast<Vector<int64_t> *>(result);
    DataTypeId inputTypeId = inputArg->GetTypeId();
    if (inputTypeId == OMNI_ARRAY) {
        ProcessArrayCardinality(inputArg, resultVec, rowSize);
    } else if (inputTypeId == OMNI_MAP) {
        ProcessMapCardinality(inputArg, resultVec, rowSize);
    } else {
        OMNI_THROW("CardinalityFunction Error:", "Unsupported input type: " + std::to_string(inputTypeId));
    }
}

void CardinalityFunction::ProcessArrayCardinality(BaseVector *arrayArg, Vector<int64_t> *resultVec, int32_t rowSize) const
{
    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (!arrayVec) {
        OMNI_THROW("CardinalityFunction Error:", "Input is not an ArrayVector");
    }
    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            resultVec->SetNull(row);
        } else {
            resultVec->SetValue(row, static_cast<int64_t>(arrayVec->GetSize(row)));
        }
    }
}

void CardinalityFunction::ProcessMapCardinality(BaseVector *mapArg, Vector<int64_t> *resultVec, int32_t rowSize) const
{
    MapVectorReader mapReader(mapArg);
    for (int32_t row = 0; row < rowSize; ++row) {
        if (mapReader.containsNull(row)) {
            resultVec->SetNull(row);
        } else {
            resultVec->SetValue(row, mapReader.GetSize(row));
        }
    }
}

} // namespace omniruntime::vectorization
