/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include <vector>
#include <string_view>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class TransformVectorFunction : public VectorFunction {
    public:
        explicit TransformVectorFunction () {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *arrBaseVec = args.top();
            args.pop();
            auto arrGuard = MakeVectorArgGuard(arrBaseVec);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                throw OmniException("TRANSFORM_ERROR", "Lambda expression is null for transform function");
            }
            if (lambdaExpr->GetParamNum() != 1) {
                throw OmniException("TRANSFORM_ERROR", "Transform only support lambda with single parameter");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();
            if (lambdaBody->GetType() == expressions::ExprType::PARAM_REF_E) {
                result = arrGuard.release();
                return;
            }

            if (arrBaseVec == nullptr) {
                OMNI_THROW("Transform Error:", "transform received null vector argument");
            }

            ArrayVector *srcArrVec = dynamic_cast<ArrayVector*>(arrBaseVec);
            if (!srcArrVec) {
                OMNI_THROW("TRANSFORM_ERROR", "Input vector is not a valid ArrayVector");
            }
            auto srcEleTypeId = srcArrVec->GetElementVector()->GetTypeId();


            int32_t arrRowSize = srcArrVec->GetSize();
            result = new ArrayVector(arrRowSize);
            auto *dstArrVec = dynamic_cast<ArrayVector *>(result);
            if (!dstArrVec) {
                OMNI_THROW("Transform Error:", "Result vector is not an ArrayVector");
            }

            for (int i = 0; i < dstArrVec->GetSize(); ++i) {
                if (srcArrVec->IsNull(i)) {
                    dstArrVec->SetNull(i);
                }
                dstArrVec->SetOffset(i+1,srcArrVec->GetOffset(i+1));
            }

            if (arrRowSize == 0 || srcArrVec->GetNullCount() == arrRowSize) {
                if (srcEleTypeId == OMNI_ARRAY) {
                    auto emptyInnerVec = new ArrayVector(0);
                    dstArrVec->SetElementVector(std::shared_ptr<BaseVector>(emptyInnerVec));
                } else {
                    VectorHelper::EmptyArrayProjection(dstArrVec, srcEleTypeId);
                }
                return;
            }

            BaseVector *flatElementVec = srcArrVec->GetElementVector().get();
            flatElementVec->SetIsField(true);
            if (flatElementVec == nullptr || flatElementVec->GetSize() == 0) {
                if (srcEleTypeId == OMNI_ARRAY) {
                    auto emptyInnerVec = new ArrayVector(0);
                    dstArrVec->SetElementVector(std::shared_ptr<BaseVector>(emptyInnerVec));
                } else {
                    VectorHelper::EmptyArrayProjection(dstArrVec, srcEleTypeId);
                }
                return;
            }

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(flatElementVec);

            context->SetResultRowSize(flatElementVec->GetSize());
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(dstArrVec->GetSize());
            BaseVector *flatResultVec = lambdaEval.GetResult();
            if (flatResultVec == nullptr) {
                throw OmniException("TRANSFORM_ERROR", "Lambda execute return null result");
            }

            dstArrVec->SetElementVectorFromRaw(flatResultVec);
        }
    };
}
