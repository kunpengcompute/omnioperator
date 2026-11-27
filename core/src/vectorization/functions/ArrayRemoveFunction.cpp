/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayRemove function implementation
 */

#include "ArrayRemoveFunction.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

static bool IsElementNull(BaseVector *vec, int32_t row)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return vec->IsNull(0);
    }
    return vec->IsNull(row);
}

template <typename T>
static T GetElementValue(BaseVector *vec, int32_t row)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<T> *>(vec)->GetConstValue();
    }
    return static_cast<Vector<T> *>(vec)->GetValue(row);
}

template <typename T>
static bool IsEqual(T a, T b)
{
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        return a == b;
    } else {
        return a == b;
    }
}

void ArrayRemoveFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArrayRemoveFunction Error:", "Expected 2 arguments (array, element)");
    }

    auto *elementVec = args.top();
    args.pop();
    auto *arrayArg = args.top();
    args.pop();

    if (arrayArg == nullptr) {
        delete elementVec;
        OMNI_THROW("ArrayRemoveFunction Error:", "Array vector is null");
    }

    if (arrayArg->GetTypeId() != OMNI_ARRAY) {
        delete arrayArg;
        delete elementVec;
        OMNI_THROW("ArrayRemoveFunction Error:", "First argument is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (!arrayVec) {
        delete arrayArg;
        delete elementVec;
        OMNI_THROW("ArrayRemoveFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    DataTypeId elementTypeId = elementVec != nullptr ? elementVec->GetTypeId() : OMNI_NONE;

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessRemove<int8_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_SHORT:
            ProcessRemove<int16_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            ProcessRemove<int32_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            ProcessRemove<int64_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_FLOAT:
            ProcessRemove<float>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_DOUBLE:
            ProcessRemove<double>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_BOOLEAN:
            ProcessRemove<bool>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_DECIMAL128:
            ProcessRemove<Decimal128>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            ProcessRemoveVarchar(arrayVec, elementVec, result, rowSize, context->GetPool());
            break;
        case OMNI_NONE:
            result = arrayArg;
            delete elementVec;
            return;
        default:
            delete arrayArg;
            delete elementVec;
            OMNI_THROW("ArrayRemoveFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }

    delete arrayArg;
    delete elementVec;
}

template <typename T>
void ArrayRemoveFunction::ProcessRemove(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
    int32_t rowSize, DataTypeId elementTypeId, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto srcElementVec = arrayVec->GetElementVector();
    auto *typedSrcElementVec = dynamic_cast<Vector<T> *>(srcElementVec.get());
    if (!typedSrcElementVec) {
        OMNI_THROW("ArrayRemoveFunction Error:", "Source element vector type mismatch");
    }

    int64_t *srcOffsets = arrayVec->GetOffsets();

    // First pass: count total output elements to allocate result
    int64_t totalOutputElements = 0;
    std::vector<std::vector<int64_t>> keepIndices(rowSize);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row) || IsElementNull(elementVec, row)) {
            int64_t startOffset = srcOffsets[row];
            int64_t endOffset = srcOffsets[row + 1];
            totalOutputElements += (endOffset - startOffset);
            continue;
        }

        T removeValue = GetElementValue<T>(elementVec, row);
        int64_t startOffset = srcOffsets[row];
        int64_t endOffset = srcOffsets[row + 1];

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (typedSrcElementVec->IsNull(static_cast<int32_t>(i))) {
                keepIndices[row].push_back(i);
                totalOutputElements++;
            } else {
                T elemValue = typedSrcElementVec->GetValue(static_cast<int32_t>(i));
                if (!IsEqual<T>(elemValue, removeValue)) {
                    keepIndices[row].push_back(i);
                    totalOutputElements++;
                }
            }
        }
    }

    auto resultElementVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(pool, static_cast<int32_t>(elementTypeId), static_cast<int32_t>(totalOutputElements)));
    auto *typedResultElementVec = dynamic_cast<Vector<T> *>(resultElementVec.get());
    if (!typedResultElementVec) {
        OMNI_THROW("ArrayRemoveFunction Error:", "Failed to create result element vector");
    }

    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);
    int64_t outputOffset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        arrayResult->SetOffset(row, static_cast<int32_t>(outputOffset));

        if (arrayVec->IsNull(row)) {
            arrayResult->SetNull(row);
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(outputOffset));
            continue;
        }

        if (IsElementNull(elementVec, row)) {
            // If element to remove is null, return the original array unchanged
            int64_t startOffset = srcOffsets[row];
            int64_t endOffset = srcOffsets[row + 1];
            for (int64_t i = startOffset; i < endOffset; ++i) {
                if (typedSrcElementVec->IsNull(static_cast<int32_t>(i))) {
                    typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
                } else {
                    typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                        typedSrcElementVec->GetValue(static_cast<int32_t>(i)));
                }
                outputOffset++;
            }
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(outputOffset));
            continue;
        }

        for (int64_t idx : keepIndices[row]) {
            if (typedSrcElementVec->IsNull(static_cast<int32_t>(idx))) {
                typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
            } else {
                typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                    typedSrcElementVec->GetValue(static_cast<int32_t>(idx)));
            }
            outputOffset++;
        }
        arrayResult->SetOffset(row + 1, static_cast<int32_t>(outputOffset));
    }

    result = arrayResult;
}

void ArrayRemoveFunction::ProcessRemoveVarchar(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
    int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    auto srcElementVec = arrayVec->GetElementVector();
    auto *typedSrcElementVec = dynamic_cast<VarcharVector *>(srcElementVec.get());
    if (!typedSrcElementVec) {
        OMNI_THROW("ArrayRemoveFunction Error:", "Source element vector type mismatch for varchar");
    }

    int64_t *srcOffsets = arrayVec->GetOffsets();

    // First pass: count total output elements
    int64_t totalOutputElements = 0;
    std::vector<std::vector<int64_t>> keepIndices(rowSize);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row) || IsElementNull(elementVec, row)) {
            int64_t startOffset = srcOffsets[row];
            int64_t endOffset = srcOffsets[row + 1];
            totalOutputElements += (endOffset - startOffset);
            continue;
        }

        std::string_view removeValue;
        if (elementVec->GetEncoding() == OMNI_ENCODING_CONST) {
            auto *constVec = dynamic_cast<ConstVector<std::string_view> *>(elementVec);
            if (constVec != nullptr) {
                removeValue = constVec->GetConstValue();
            }
        } else {
            auto *varcharVec = dynamic_cast<VarcharVector *>(elementVec);
            if (varcharVec != nullptr) {
                removeValue = varcharVec->GetValue(row);
            }
        }

        int64_t startOffset = srcOffsets[row];
        int64_t endOffset = srcOffsets[row + 1];

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (typedSrcElementVec->IsNull(static_cast<int32_t>(i))) {
                keepIndices[row].push_back(i);
                totalOutputElements++;
            } else {
                std::string_view elemValue = typedSrcElementVec->GetValue(static_cast<int32_t>(i));
                if (elemValue != removeValue) {
                    keepIndices[row].push_back(i);
                    totalOutputElements++;
                }
            }
        }
    }

    auto resultElementVec = std::shared_ptr<BaseVector>(
        new VarcharVector(static_cast<int32_t>(totalOutputElements), pool));
    auto *typedResultElementVec = dynamic_cast<VarcharVector *>(resultElementVec.get());
    if (!typedResultElementVec) {
        OMNI_THROW("ArrayRemoveFunction Error:", "Failed to create result varchar vector");
    }

    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);
    int64_t outputOffset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        arrayResult->SetOffset(row, static_cast<int32_t>(outputOffset));

        if (arrayVec->IsNull(row)) {
            arrayResult->SetNull(row);
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(outputOffset));
            continue;
        }

        if (IsElementNull(elementVec, row)) {
            int64_t startOffset = srcOffsets[row];
            int64_t endOffset = srcOffsets[row + 1];
            for (int64_t i = startOffset; i < endOffset; ++i) {
                if (typedSrcElementVec->IsNull(static_cast<int32_t>(i))) {
                    typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
                } else {
                    typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                        typedSrcElementVec->GetValue(static_cast<int32_t>(i)));
                }
                outputOffset++;
            }
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(outputOffset));
            continue;
        }

        for (int64_t idx : keepIndices[row]) {
            if (typedSrcElementVec->IsNull(static_cast<int32_t>(idx))) {
                typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
            } else {
                typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                    typedSrcElementVec->GetValue(static_cast<int32_t>(idx)));
            }
            outputOffset++;
        }
        arrayResult->SetOffset(row + 1, static_cast<int32_t>(outputOffset));
    }

    result = arrayResult;
}

template void ArrayRemoveFunction::ProcessRemove<int8_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<int16_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<int32_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<int64_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<float>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<double>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<bool>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayRemoveFunction::ProcessRemove<Decimal128>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;

} // namespace omniruntime::vectorization
