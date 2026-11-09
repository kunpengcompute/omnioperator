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

    class ArrayFilterVectorFunction : public VectorFunction {
    public:
        explicit ArrayFilterVectorFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *arrBaseVec = args.top();
            args.pop();
            auto arrGuard = MakeVectorArgGuard(arrBaseVec);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                OMNI_THROW("FILTER_ERROR", "Lambda expression is null for filter function");
            }
            if (lambdaExpr->GetParamNum() != 1) {
                OMNI_THROW("FILTER_ERROR", "filter only supports lambda with single parameter");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();

            if (arrBaseVec == nullptr) {
                OMNI_THROW("FILTER_ERROR", "filter received null vector argument");
            }

            ArrayVector *srcArrVec = dynamic_cast<ArrayVector *>(arrBaseVec);
            if (!srcArrVec) {
                OMNI_THROW("FILTER_ERROR", "Input vector is not a valid ArrayVector");
            }

            std::shared_ptr<BaseVector> elementVecHolder = srcArrVec->GetElementVector();
            auto srcEleTypeId = elementVecHolder->GetTypeId();

            int32_t arrRowSize = srcArrVec->GetSize();
            result = new ArrayVector(arrRowSize);
            auto *dstArrVec = static_cast<ArrayVector *>(result);

            if (arrRowSize == 0 || srcArrVec->GetNullCount() == arrRowSize) {
                for (int32_t i = 0; i < arrRowSize; ++i) {
                    dstArrVec->SetNull(i);
                }
                VectorHelper::EmptyArrayProjection(dstArrVec, srcEleTypeId);
                return;
            }

            BaseVector *flatElementVec = elementVecHolder.get();
            if (flatElementVec == nullptr || flatElementVec->GetSize() == 0) {
                for (int32_t i = 0; i < arrRowSize; ++i) {
                    if (srcArrVec->IsNull(i)) {
                        dstArrVec->SetNull(i);
                    }
                }
                VectorHelper::EmptyArrayProjection(dstArrVec, srcEleTypeId);
                return;
            }
            flatElementVec->SetIsField(true);

            int32_t elemSize = flatElementVec->GetSize();
            std::unique_ptr<BaseVector> lambdaInputHolder(flatElementVec->Slice(0, elemSize));
            BaseVector *lambdaInputVec = lambdaInputHolder.get();

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(lambdaInputVec);

            context->SetResultRowSize(elemSize);
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(arrRowSize);
            BaseVector *boolResultVec = lambdaEval.GetResult();
            if (boolResultVec == nullptr) {
                delete result;
                OMNI_THROW("FILTER_ERROR", "Lambda execute return null result");
            }

            std::unique_ptr<BaseVector> boolResultVecHolder(boolResultVec);

            std::vector<int> selectedIndices;
            int32_t currentOffset = 0;
            bool isConst = (boolResultVec->GetEncoding() == OMNI_ENCODING_CONST);
            int32_t boolSize = static_cast<int32_t>(boolResultVec->GetSize());

            for (int32_t i = 0; i < arrRowSize; ++i) {
                if (srcArrVec->IsNull(i)) {
                    dstArrVec->SetNull(i);
                    dstArrVec->SetOffset(i + 1, currentOffset);
                    continue;
                }
                int64_t srcOffset = srcArrVec->GetOffset(i);
                int64_t srcSize = srcArrVec->GetOffset(i + 1) - srcOffset;
                if (srcOffset < 0 || srcSize < 0 || srcOffset + srcSize > elemSize) {
                    OMNI_THROW("FILTER_ERROR", "Invalid array offsets in filter");
                }
                int32_t filteredCount = 0;
                for (int64_t j = 0; j < srcSize; ++j) {
                    int64_t idx = srcOffset + j;
                    if (idx < 0 || idx >= elemSize) {
                        continue;
                    }
                    if (!isConst && idx >= boolSize) {
                        continue;
                    }
                    bool selected = false;
                    if (isConst) {
                        if (!boolResultVec->IsNull(0)) {
                            selected = static_cast<ConstVector<bool> *>(boolResultVec)->GetConstValue();
                        }
                    } else {
                        if (!boolResultVec->IsNull(static_cast<int>(idx))) {
                            selected = static_cast<Vector<bool> *>(boolResultVec)->GetValue(static_cast<int>(idx));
                        }
                    }
                    if (selected) {
                        selectedIndices.push_back(static_cast<int>(idx));
                        filteredCount++;
                    }
                }
                currentOffset += filteredCount;
                dstArrVec->SetOffset(i + 1, currentOffset);
            }

            int32_t selectedCount = static_cast<int32_t>(selectedIndices.size());
            if (selectedCount == 0) {
                VectorHelper::EmptyArrayProjection(dstArrVec, srcEleTypeId);
            } else {
                BaseVector *filteredElements = flatElementVec->CopyPositions(
                    selectedIndices.data(), 0, selectedCount);
                dstArrVec->SetElementVectorFromRaw(filteredElements);
            }
        }
    };
}
