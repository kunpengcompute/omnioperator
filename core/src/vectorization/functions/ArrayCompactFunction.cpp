/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayCompact function implementation for removing null elements from array
 */

#include "ArrayCompactFunction.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "type/string_Impl.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {
type::DataTypePtr ResolveArrayElementDataType(const type::DataTypePtr &outputType, const ArrayVector *arrayVec,
    const std::shared_ptr<BaseVector> &inputElementVector)
{
    auto fromArrayDataType = [](const type::DataTypePtr &dt) -> type::DataTypePtr {
        if (dt == nullptr || dt->GetId() != OMNI_ARRAY) {
            return nullptr;
        }
        auto *arrTy = dynamic_cast<ArrayType *>(dt.get());
        return arrTy != nullptr ? arrTy->ElementType() : nullptr;
    };
    if (auto t = fromArrayDataType(outputType)) {
        return t;
    }
    if (auto t = fromArrayDataType(arrayVec->GetDataType())) {
        return t;
    }
    if (inputElementVector != nullptr) {
        return VectorHelper::GetDataType(inputElementVector.get());
    }
    return nullptr;
}
} // namespace

void ArrayCompactFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("ArrayCompactFunction Error:", "No arguments provided");
    }

    auto inputArg = args.top();
    args.pop();
    auto inputGuard = MakeVectorArgGuard(inputArg);

    if (inputArg == nullptr) {
        OMNI_THROW("ArrayCompactFunction Error:", "Input vector is null");
    }

    if (inputArg->GetTypeId() != OMNI_ARRAY) {
        OMNI_THROW("ArrayCompactFunction Error:", "Input is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(inputArg);
    if (arrayVec == nullptr) {
        OMNI_THROW("ArrayCompactFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto inputElementVector = arrayVec->GetElementVector();

    auto *resultArray = new ArrayVector(rowSize);

    if (inputElementVector == nullptr) {
        for (int32_t row = 0; row < rowSize; ++row) {
            resultArray->SetOffset(row, 0);
            if (arrayVec->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetNotNull(row);
            }
        }
        resultArray->SetOffset(rowSize, 0);
        result = resultArray;
        return;
    }

    int64_t totalElements = inputElementVector->GetSize();
    DataTypeId elementTypeId = inputElementVector->GetTypeId();

    // First pass: count non-null elements to determine output size
    int64_t nonNullCount = 0;
    int64_t *offsets = arrayVec->GetOffsets();
    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            continue;
        }
        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];
        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (!inputElementVector->IsNull(static_cast<int32_t>(i))) {
                nonNullCount++;
            }
        }
    }

    BaseVector *newElementVector = nullptr;
    const bool complexElement = elementTypeId == OMNI_ARRAY || elementTypeId == OMNI_MAP || elementTypeId == OMNI_ROW;
    if (elementTypeId == OMNI_VARCHAR || elementTypeId == OMNI_CHAR) {
        newElementVector = VectorHelper::CreateStringVector(nonNullCount > 0 ? nonNullCount : 1);
    } else if (complexElement) {
        auto elemDt = ResolveArrayElementDataType(outputType, arrayVec, inputElementVector);
        if (elemDt == nullptr) {
            OMNI_THROW("ArrayCompactFunction Error:",
                "Cannot resolve element type for nested array_compact (array/map/struct elements)");
        }
        const int32_t outElemSize = nonNullCount > 0 ? static_cast<int32_t>(nonNullCount) : 1;
        newElementVector = VectorHelper::CreateComplexVector(elemDt.get(), outElemSize);
    } else {
        newElementVector = VectorHelper::CreateFlatVector(elementTypeId,
            nonNullCount > 0 ? nonNullCount : 1);
    }

    int64_t newElementIndex = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, newElementIndex);

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (!inputElementVector->IsNull(static_cast<int32_t>(i))) {
                newElementVector->SetNotNull(newElementIndex);
                VectorHelper::CopyValue(inputElementVector.get(),
                    static_cast<int32_t>(i), newElementVector, newElementIndex);
                newElementIndex++;
            }
        }
    }

    resultArray->SetOffset(rowSize, newElementIndex);
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
    result = resultArray;
}

} // namespace omniruntime::vectorization
