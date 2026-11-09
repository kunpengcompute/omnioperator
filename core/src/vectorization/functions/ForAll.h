/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include <vector>
#include <memory>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class ForAllVectorFunction : public VectorFunction {
    public:
        explicit ForAllVectorFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *arrBaseVec = args.top();
            args.pop();
            auto arrGuard = MakeVectorArgGuard(arrBaseVec);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                throw OmniException("FORALL_ERROR", "Lambda expression is null for forall function");
            }
            if (lambdaExpr->GetParamNum() != 1) {
                throw OmniException("FORALL_ERROR", "forall only supports lambda with single parameter");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();

            if (arrBaseVec == nullptr) {
                OMNI_THROW("FORALL_ERROR", "forall received null vector argument");
            }

            ArrayVector *srcArrVec = dynamic_cast<ArrayVector *>(arrBaseVec);
            if (!srcArrVec) {
                OMNI_THROW("FORALL_ERROR", "Input vector is not a valid ArrayVector");
            }

            int32_t arrRowSize = srcArrVec->GetSize();
            result = new Vector<bool>(arrRowSize);
            auto *boolResult = static_cast<Vector<bool> *>(result);

            if (arrRowSize == 0) {
                return;
            }

            if (srcArrVec->GetNullCount() == arrRowSize) {
                for (int32_t i = 0; i < arrRowSize; ++i) {
                    result->SetNull(i);
                }
                return;
            }

            BaseVector *flatElementVec = srcArrVec->GetElementVector().get();
            if (flatElementVec == nullptr || flatElementVec->GetSize() == 0) {
                for (int32_t i = 0; i < arrRowSize; ++i) {
                    if (srcArrVec->IsNull(i)) {
                        result->SetNull(i);
                    } else {
                        boolResult->SetValue(i, true);
                    }
                }
                return;
            }

            std::unique_ptr<BaseVector> lambdaInputHolder(flatElementVec->Slice(0, flatElementVec->GetSize()));
            BaseVector *lambdaInput = lambdaInputHolder.get();
            lambdaInput->SetIsField(true);

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(lambdaInput);

            context->SetResultRowSize(lambdaInput->GetSize());
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(arrRowSize);
            std::unique_ptr<BaseVector> lambdaResultVecHolder(lambdaEval.GetResult());
            BaseVector *lambdaResultVec = lambdaResultVecHolder.get();
            if (lambdaResultVec == nullptr) {
                delete result;
                throw OmniException("FORALL_ERROR", "Lambda execute return null result");
            }

            bool isConst = (lambdaResultVec->GetEncoding() == OMNI_ENCODING_CONST);

            for (int32_t i = 0; i < arrRowSize; ++i) {
                if (srcArrVec->IsNull(i)) {
                    result->SetNull(i);
                    continue;
                }

                int64_t offset = srcArrVec->GetOffset(i);
                int64_t size = srcArrVec->GetOffset(i + 1) - offset;

                if (size == 0) {
                    boolResult->SetValue(i, true);
                    continue;
                }

                bool allMatch = true;
                bool hasNull = false;

                for (int64_t j = 0; j < size; ++j) {
                    int64_t idx = offset + j;
                    if (isConst) {
                        if (lambdaResultVec->IsNull(0)) {
                            hasNull = true;
                            continue;
                        }
                        if (!static_cast<ConstVector<bool> *>(lambdaResultVec)->GetConstValue()) {
                            allMatch = false;
                            break;
                        }
                    } else {
                        if (lambdaResultVec->IsNull(idx)) {
                            hasNull = true;
                            continue;
                        }
                        if (!static_cast<Vector<bool> *>(lambdaResultVec)->GetValue(idx)) {
                            allMatch = false;
                            break;
                        }
                    }
                }

                if (!allMatch) {
                    boolResult->SetValue(i, false);
                } else if (hasNull) {
                    result->SetNull(i);
                } else {
                    boolResult->SetValue(i, true);
                }
            }
        }
    };
}
