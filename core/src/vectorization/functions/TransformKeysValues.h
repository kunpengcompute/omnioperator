/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include <vector>
#include <string_view>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    template <bool isKeys>
    class TransformKeysValuesVectorFunction : public VectorFunction {
    public:
        explicit TransformKeysValuesVectorFunction () {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *arrBaseVec = args.top();
            args.pop();
            auto arrGuard = MakeVectorArgGuard(arrBaseVec);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                throw OmniException("TRANSFORM_KEYS_VALUES_ERROR", "lambdaExpr is null");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();
            if (lambdaBody->GetType() == expressions::ExprType::PARAM_REF_E) {
                result = arrGuard.release();
                return;
            }
            int32_t mapRowSize = arrBaseVec->GetSize();
            MapVector *srcMapVec = dynamic_cast<MapVector*>(arrBaseVec);
            if (!srcMapVec) {
                OMNI_THROW("TRANSFORM_KEYS_VALUES_ERROR", "Input vector is not a valid MapVector");
            }
            auto srcKeyEleTypeId = srcMapVec->GetKeyVector()->GetTypeId();
            auto srcValueEleTypeId = srcMapVec->GetValueVector()->GetTypeId();

            result = new MapVector(mapRowSize);
            auto *dstMapVec = static_cast<MapVector *>(result);

            for (int32_t i = 0; i < mapRowSize; ++i) {
                if (srcMapVec->IsNull(i)) {
                    dstMapVec->SetNull(i);
                }
                dstMapVec->SetOffset(i+1,srcMapVec->GetOffset(i+1));
            }

            if (mapRowSize == 0 || srcMapVec->GetNullCount() == mapRowSize) {
                VectorHelper::EmptyMapProjection(dstMapVec, srcKeyEleTypeId, srcValueEleTypeId);
                return;
            }

            BaseVector *keyElementVec = nullptr;
            BaseVector *valueElementVec = nullptr;
            keyElementVec = srcMapVec->GetKeyVector().get();
            valueElementVec = srcMapVec->GetValueVector().get();
            if (keyElementVec == nullptr || keyElementVec->GetSize() == 0 ||
                valueElementVec == nullptr || valueElementVec->GetSize() == 0) {
                VectorHelper::EmptyMapProjection(dstMapVec, srcKeyEleTypeId, srcValueEleTypeId);
                return;
            }
            keyElementVec->SetIsField(true);
            valueElementVec->SetIsField(true);

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(keyElementVec);
            lambdaEval.lambdaParams_.push_back(valueElementVec);

            context->SetResultRowSize(keyElementVec->GetSize());
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(mapRowSize);
            BaseVector *flatResultVec = lambdaEval.GetResult();
            if (flatResultVec == nullptr) {
                throw OmniException("TRANSFORM_ERROR", "Lambda execute return null result");
            }

            if (isKeys) {
                dstMapVec->SetValueVector(srcMapVec->GetValueVector());
                dstMapVec->AddKeys(flatResultVec);
            } else {
                dstMapVec->SetKeyVector(srcMapVec->GetKeyVector());
                dstMapVec->AddValues(flatResultVec);
            }
        }
    };
}