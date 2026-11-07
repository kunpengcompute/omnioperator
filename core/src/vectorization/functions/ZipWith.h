/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: zip_with lambda function for array operations
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include <vector>
#include <algorithm>
#include <memory>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class ZipWithVectorFunction : public VectorFunction {
    public:
        explicit ZipWithVectorFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *rightBaseVec = args.top();
            args.pop();
            auto rightGuard = MakeVectorArgGuard(rightBaseVec);
            BaseVector *leftBaseVec = args.top();
            args.pop();
            auto leftGuard = MakeVectorArgGuard(leftBaseVec);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                throw OmniException("ZIP_WITH_ERROR", "Lambda expression is null for zip_with function");
            }
            if (lambdaExpr->GetParamNum() != 2) {
                throw OmniException("ZIP_WITH_ERROR", "zip_with requires lambda with exactly 2 parameters");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();

            ArrayVector *leftArrVec = dynamic_cast<ArrayVector *>(leftBaseVec);
            ArrayVector *rightArrVec = dynamic_cast<ArrayVector *>(rightBaseVec);
            if (!leftArrVec || !rightArrVec) {
                throw OmniException("ZIP_WITH_ERROR", "Input vectors are not valid ArrayVectors");
            }

            int32_t rowSize = leftArrVec->GetSize();
            result = new ArrayVector(rowSize);
            auto *dstArrVec = dynamic_cast<ArrayVector *>(result);

            int32_t totalElements = 0;
            for (int32_t i = 0; i < rowSize; ++i) {
                if (leftArrVec->IsNull(i) || rightArrVec->IsNull(i)) {
                    dstArrVec->SetNull(i);
                    continue;
                }
                int64_t leftSize = leftArrVec->GetSize(i);
                int64_t rightSize = rightArrVec->GetSize(i);
                int64_t maxSize = std::max(leftSize, rightSize);
                totalElements += static_cast<int32_t>(maxSize);
                dstArrVec->SetOffset(i + 1, totalElements);
            }

            if (totalElements == 0) {
                auto leftEleVec = leftArrVec->GetElementVector();
                DataTypeId eleTypeId = leftEleVec ? leftEleVec->GetTypeId() : OMNI_INT;
                VectorHelper::EmptyArrayProjection(dstArrVec, eleTypeId);
                return;
            }

            auto leftEleTypeId = leftArrVec->GetElementVector()->GetTypeId();
            auto rightEleTypeId = rightArrVec->GetElementVector()->GetTypeId();

            std::unique_ptr<BaseVector> paddedLeftHolder(BuildPaddedVector(leftArrVec, leftEleTypeId, totalElements, dstArrVec, rowSize));
            std::unique_ptr<BaseVector> paddedRightHolder(BuildPaddedVector(rightArrVec, rightEleTypeId, totalElements, dstArrVec, rowSize));
            BaseVector *paddedLeft = paddedLeftHolder.get();
            BaseVector *paddedRight = paddedRightHolder.get();

            paddedLeft->SetIsField(true);
            paddedRight->SetIsField(true);

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(paddedLeft);
            lambdaEval.lambdaParams_.push_back(paddedRight);

            context->SetResultRowSize(totalElements);
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(rowSize);
            BaseVector *flatResultVec = lambdaEval.GetResult();
            if (flatResultVec == nullptr) {
                throw OmniException("ZIP_WITH_ERROR", "Lambda execute return null result");
            }

            dstArrVec->SetElementVectorFromRaw(flatResultVec);
        }

    private:
        BaseVector *BuildPaddedVector(ArrayVector *srcArr, DataTypeId eleTypeId, int32_t totalElements,
                                      ArrayVector *resultArr, int32_t rowSize) const
        {
            BaseVector *padded = VectorHelper::CreateFlatVector(static_cast<int32_t>(eleTypeId), totalElements);
            BaseVector *srcElements = srcArr->GetElementVector().get();

            int32_t dstIdx = 0;
            for (int32_t row = 0; row < rowSize; ++row) {
                if (resultArr->IsNull(row)) {
                    continue;
                }
                int64_t srcSize = srcArr->GetSize(row);
                int64_t resultSize = resultArr->GetSize(row);
                int64_t srcOffset = srcArr->GetOffset(row);

                CopyElements(srcElements, static_cast<int32_t>(srcOffset), padded, dstIdx,
                             static_cast<int32_t>(srcSize), eleTypeId);

                for (int64_t i = srcSize; i < resultSize; ++i) {
                    padded->SetNull(dstIdx + static_cast<int32_t>(i));
                }
                dstIdx += static_cast<int32_t>(resultSize);
            }
            return padded;
        }

        void CopyElements(BaseVector *src, int32_t srcOffset, BaseVector *dst, int32_t dstOffset,
                           int32_t count, DataTypeId typeId) const
        {
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    CopyElementsTyped<int32_t>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    CopyElementsTyped<int64_t>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_DOUBLE:
                    CopyElementsTyped<double>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_FLOAT:
                    CopyElementsTyped<float>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_SHORT:
                    CopyElementsTyped<int16_t>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_BYTE:
                    CopyElementsTyped<int8_t>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_BOOLEAN:
                    CopyElementsTyped<bool>(src, srcOffset, dst, dstOffset, count);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    CopyStringElements(src, srcOffset, dst, dstOffset, count);
                    break;
                default:
                    throw OmniException("ZIP_WITH_ERROR",
                        "Unsupported element type: " + std::to_string(typeId));
            }
        }

        template <typename T>
        void CopyElementsTyped(BaseVector *src, int32_t srcOffset, BaseVector *dst, int32_t dstOffset,
                                int32_t count) const
        {
            auto *srcVec = static_cast<Vector<T> *>(src);
            auto *dstVec = static_cast<Vector<T> *>(dst);
            for (int32_t i = 0; i < count; ++i) {
                if (src->IsNull(srcOffset + i)) {
                    dst->SetNull(dstOffset + i);
                } else {
                    dstVec->SetValue(dstOffset + i, srcVec->GetValue(srcOffset + i));
                }
            }
        }

        void CopyStringElements(BaseVector *src, int32_t srcOffset, BaseVector *dst, int32_t dstOffset,
                                 int32_t count) const
        {
            auto *srcVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(src);
            auto *dstVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(dst);
            for (int32_t i = 0; i < count; ++i) {
                if (src->IsNull(srcOffset + i)) {
                    dst->SetNull(dstOffset + i);
                } else {
                    dstVec->SetValue(dstOffset + i, srcVec->GetValue(srcOffset + i));
                }
            }
        }
    };
}
