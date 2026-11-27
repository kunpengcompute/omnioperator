/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */
#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "vector/array_vector.h"
#include "vector/vector_helper.h"
#include "type/data_operations.h"

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class ArrayFlattenFunction : public VectorFunction {
    public:
        explicit ArrayFlattenFunction () {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
            ExecutionContext *context) const override
        {
            BaseVector* inputVec = args.top();
            args.pop();

            auto *outerArray = dynamic_cast<ArrayVector *>(inputVec);
            if (!outerArray) {
                delete inputVec;
                OMNI_THROW("ArrayFlattenFunction Error:", "Expected ArrayVector input");
            }

            const int32_t rowSize = outerArray->GetSize();
            auto *middleArray = dynamic_cast<ArrayVector *>(outerArray->GetElementVector().get());
            if (!middleArray) {
                delete inputVec;
                OMNI_THROW("ArrayFlattenFunction Error:", "Expected nested ArrayVector");
            }

            auto innerElementVec = middleArray->GetElementVector();
            DataTypeId innerTypeId = innerElementVec ? innerElementVec->GetTypeId() : OMNI_INT;

            int64_t totalFlatElements = 0;
            for (int32_t row = 0; row < rowSize; ++row) {
                if (outerArray->IsNull(row)) {
                    continue;
                }
                int64_t outerStart = outerArray->GetOffset(row);
                int64_t outerLen = outerArray->GetSize(row);
                for (int64_t mid = outerStart; mid < outerStart + outerLen; ++mid) {
                    if (!middleArray->IsNull(mid)) {
                        totalFlatElements += middleArray->GetSize(mid);
                    }
                }
            }

            std::shared_ptr<BaseVector> flatElements;
            if (TypeUtil::IsStringType(innerTypeId)) {
                using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
                flatElements = std::shared_ptr<BaseVector>(
                    new VarcharVector(static_cast<int32_t>(totalFlatElements), context->GetPool()));
            } else {
                flatElements = std::shared_ptr<BaseVector>(
                    VectorHelper::CreateFlatVector(context->GetPool(), static_cast<int32_t>(innerTypeId),
                        static_cast<int32_t>(totalFlatElements)));
            }

            auto *resultArray = new ArrayVector(rowSize, flatElements, context->GetPool());
            int64_t flatOffset = 0;

            for (int32_t row = 0; row < rowSize; ++row) {
                resultArray->SetOffset(row, static_cast<int32_t>(flatOffset));

                if (outerArray->IsNull(row)) {
                    resultArray->SetNull(row);
                    resultArray->SetOffset(row + 1, static_cast<int32_t>(flatOffset));
                    continue;
                }

                int64_t outerStart = outerArray->GetOffset(row);
                int64_t outerLen = outerArray->GetSize(row);

                for (int64_t mid = outerStart; mid < outerStart + outerLen; ++mid) {
                    if (middleArray->IsNull(mid)) {
                        continue;
                    }
                    int64_t innerStart = middleArray->GetOffset(mid);
                    int64_t innerLen = middleArray->GetSize(mid);

                    if (innerLen > 0 && innerElementVec) {
                        BaseVector *srcSlice = innerElementVec->Slice(
                            static_cast<int>(innerStart), static_cast<int>(innerLen), false);
                        VectorHelper::AppendVector(flatElements.get(),
                            static_cast<int32_t>(flatOffset), srcSlice, static_cast<int32_t>(innerLen));
                        delete srcSlice;
                    }
                    flatOffset += innerLen;
                }

                resultArray->SetOffset(row + 1, static_cast<int32_t>(flatOffset));
            }

            result = resultArray;
            delete inputVec;
        }
    };
}