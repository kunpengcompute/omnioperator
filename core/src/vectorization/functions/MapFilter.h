/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "type/data_operations.h"
#include <vector>
#include <memory>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class MapFilterVectorFunction : public VectorFunction {
    public:
        explicit MapFilterVectorFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *mapBaseVec = args.top();
            args.pop();
            auto mapGuard = MakeVectorArgGuard(mapBaseVec);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                OMNI_THROW("MAP_FILTER_ERROR", "lambdaExpr is null");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();

            int32_t mapRowSize = mapBaseVec->GetSize();
            MapVector *srcMapVec = dynamic_cast<MapVector *>(mapBaseVec);
            if (!srcMapVec) {
                OMNI_THROW("MAP_FILTER_ERROR", "Input vector is not a valid MapVector");
            }
            auto srcKeyTypeId = srcMapVec->GetKeyVector()->GetTypeId();
            auto srcValueTypeId = srcMapVec->GetValueVector()->GetTypeId();

            result = new MapVector(mapRowSize);
            auto *dstMapVec = static_cast<MapVector *>(result);

            if (mapRowSize == 0 || srcMapVec->GetNullCount() == mapRowSize) {
                for (int32_t i = 0; i < mapRowSize; ++i) {
                    dstMapVec->SetNull(i);
                }
                VectorHelper::EmptyMapProjection(dstMapVec, srcKeyTypeId, srcValueTypeId);
                return;
            }

            BaseVector *keyElementVec = srcMapVec->GetKeyVector().get();
            BaseVector *valueElementVec = srcMapVec->GetValueVector().get();
            if (keyElementVec == nullptr || keyElementVec->GetSize() == 0 ||
                valueElementVec == nullptr || valueElementVec->GetSize() == 0) {
                for (int32_t i = 0; i < mapRowSize; ++i) {
                    if (srcMapVec->IsNull(i)) {
                        dstMapVec->SetNull(i);
                    }
                }
                VectorHelper::EmptyMapProjection(dstMapVec, srcKeyTypeId, srcValueTypeId);
                return;
            }
            std::unique_ptr<BaseVector> keyForLambdaHolder(keyElementVec->Slice(0, keyElementVec->GetSize()));
            std::unique_ptr<BaseVector> valueForLambdaHolder(valueElementVec->Slice(0, valueElementVec->GetSize()));
            BaseVector *keyForLambda = keyForLambdaHolder.get();
            BaseVector *valueForLambda = valueForLambdaHolder.get();
            keyForLambda->SetIsField(true);
            valueForLambda->SetIsField(true);

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(keyForLambda);
            lambdaEval.lambdaParams_.push_back(valueForLambda);

            context->SetResultRowSize(keyElementVec->GetSize());
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(mapRowSize);
            std::unique_ptr<BaseVector> boolResultVecHolder(lambdaEval.GetResult());
            BaseVector *boolResultVec = boolResultVecHolder.get();
            if (boolResultVec == nullptr) {
                delete result;
                OMNI_THROW("MAP_FILTER_ERROR", "Lambda execute return null result");
            }

            std::vector<int> selectedIndices;
            int32_t currentOffset = 0;
            bool isConst = (boolResultVec->GetEncoding() == OMNI_ENCODING_CONST);

            for (int32_t i = 0; i < mapRowSize; ++i) {
                if (srcMapVec->IsNull(i)) {
                    dstMapVec->SetNull(i);
                    dstMapVec->SetOffset(i + 1, currentOffset);
                    continue;
                }
                int64_t srcOffset = srcMapVec->GetOffset(i);
                int64_t srcSize = srcMapVec->GetOffset(i + 1) - srcOffset;
                int32_t filteredCount = 0;
                for (int64_t j = 0; j < srcSize; ++j) {
                    int64_t idx = srcOffset + j;
                    bool selected = false;
                    if (isConst) {
                        if (!boolResultVec->IsNull(0)) {
                            selected = static_cast<ConstVector<bool> *>(boolResultVec)->GetConstValue();
                        }
                    } else {
                        if (!boolResultVec->IsNull(idx)) {
                            selected = static_cast<Vector<bool> *>(boolResultVec)->GetValue(idx);
                        }
                    }
                    if (selected) {
                        selectedIndices.push_back(static_cast<int>(idx));
                        filteredCount++;
                    }
                }
                currentOffset += filteredCount;
                dstMapVec->SetOffset(i + 1, currentOffset);
            }

            int32_t selectedCount = static_cast<int32_t>(selectedIndices.size());
            if (selectedCount == 0) {
                VectorHelper::EmptyMapProjection(dstMapVec, srcKeyTypeId, srcValueTypeId);
            } else {
                BaseVector *filteredKeys = keyElementVec->CopyPositions(
                    selectedIndices.data(), 0, selectedCount);
                BaseVector *filteredValues = valueElementVec->CopyPositions(
                    selectedIndices.data(), 0, selectedCount);
                dstMapVec->AddKeys(filteredKeys);
                dstMapVec->AddValues(filteredValues);
            }
        }
    };
}
