/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayAppend function implementation
 */

#include "ArrayAppendFunction.h"
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

void ArrayAppendFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArrayAppendFunction Error:", "Expected 2 arguments (array, element)");
    }

    auto *elementVec = args.top();
    args.pop();
    auto *arrayArg = args.top();
    args.pop();

    if (arrayArg == nullptr) {
        delete elementVec;
        OMNI_THROW("ArrayAppendFunction Error:", "Array vector is null");
    }

    if (arrayArg->GetTypeId() != OMNI_ARRAY) {
        delete arrayArg;
        delete elementVec;
        OMNI_THROW("ArrayAppendFunction Error:", "First argument is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (!arrayVec) {
        delete arrayArg;
        delete elementVec;
        OMNI_THROW("ArrayAppendFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    DataTypeId elementTypeId = elementVec != nullptr ? elementVec->GetTypeId() : OMNI_NONE;

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessAppend<int8_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_SHORT:
            ProcessAppend<int16_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            ProcessAppend<int32_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            ProcessAppend<int64_t>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_FLOAT:
            ProcessAppend<float>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_DOUBLE:
            ProcessAppend<double>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_BOOLEAN:
            ProcessAppend<bool>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_DECIMAL128:
            ProcessAppend<Decimal128>(arrayVec, elementVec, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            ProcessAppendVarchar(arrayVec, elementVec, result, rowSize, context->GetPool());
            break;
        case OMNI_NONE: {
            // Element type is NONE (all nulls), append null to each non-null array row
            auto srcElementVec = arrayVec->GetElementVector();
            int64_t *srcOffsets = arrayVec->GetOffsets();
            int64_t totalSrcElements = srcElementVec ? srcElementVec->GetSize() : 0;

            int32_t nonNullRows = 0;
            for (int32_t row = 0; row < rowSize; ++row) {
                if (!arrayVec->IsNull(row)) {
                    nonNullRows++;
                }
            }
            int64_t totalOutputElements = totalSrcElements + nonNullRows;

            auto *arrayResult = new ArrayVector(rowSize, context->GetPool());
            int64_t outputOffset = 0;

            if (totalOutputElements > 0 && srcElementVec) {
                DataTypeId srcElemTypeId = srcElementVec->GetTypeId();
                BaseVector *newElementVector = nullptr;
                if (srcElemTypeId == OMNI_VARCHAR || srcElemTypeId == OMNI_CHAR) {
                    newElementVector = VectorHelper::CreateStringVector(context->GetPool(),
                        static_cast<uint32_t>(totalOutputElements));
                } else {
                    newElementVector = VectorHelper::CreateFlatVector(context->GetPool(),
                        static_cast<int32_t>(srcElemTypeId),
                        static_cast<int32_t>(totalOutputElements));
                }

                for (int32_t row = 0; row < rowSize; ++row) {
                    arrayResult->SetOffset(row, static_cast<int32_t>(outputOffset));
                    if (arrayVec->IsNull(row)) {
                        arrayResult->SetNull(row);
                        continue;
                    }
                    arrayResult->SetNotNull(row);
                    int64_t startOffset = srcOffsets[row];
                    int64_t endOffset = srcOffsets[row + 1];
                    for (int64_t i = startOffset; i < endOffset; ++i) {
                        if (srcElementVec->IsNull(static_cast<int32_t>(i))) {
                            newElementVector->SetNull(static_cast<int32_t>(outputOffset));
                        } else {
                            newElementVector->SetNotNull(static_cast<int32_t>(outputOffset));
                            VectorHelper::CopyValue(srcElementVec.get(),
                                static_cast<int32_t>(i), newElementVector, static_cast<int32_t>(outputOffset));
                        }
                        outputOffset++;
                    }
                    newElementVector->SetNull(static_cast<int32_t>(outputOffset));
                    outputOffset++;
                }
                arrayResult->SetOffset(rowSize, static_cast<int32_t>(outputOffset));
                arrayResult->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
            } else {
                for (int32_t row = 0; row < rowSize; ++row) {
                    arrayResult->SetOffset(row, 0);
                    if (arrayVec->IsNull(row)) {
                        arrayResult->SetNull(row);
                    } else {
                        arrayResult->SetNotNull(row);
                    }
                }
                arrayResult->SetOffset(rowSize, 0);
            }
            result = arrayResult;
            delete arrayArg;
            delete elementVec;
            return;
        }
        default:
            delete arrayArg;
            delete elementVec;
            OMNI_THROW("ArrayAppendFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }

    delete arrayArg;
    delete elementVec;
}

template <typename T>
void ArrayAppendFunction::ProcessAppend(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
    int32_t rowSize, DataTypeId elementTypeId, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto srcElementVec = arrayVec->GetElementVector();
    int64_t *srcOffsets = arrayVec->GetOffsets();

    int64_t totalSrcElements = srcElementVec ? srcElementVec->GetSize() : 0;
    int32_t nonNullRows = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!arrayVec->IsNull(row)) {
            nonNullRows++;
        }
    }
    int64_t totalOutputElements = totalSrcElements + nonNullRows;

    auto resultElementVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(pool, static_cast<int32_t>(elementTypeId),
            static_cast<int32_t>(totalOutputElements > 0 ? totalOutputElements : 1)));
    auto *typedResultElementVec = dynamic_cast<Vector<T> *>(resultElementVec.get());
    if (!typedResultElementVec) {
        OMNI_THROW("ArrayAppendFunction Error:", "Failed to create result element vector");
    }

    Vector<T> *typedSrcElementVec = nullptr;
    if (srcElementVec) {
        typedSrcElementVec = dynamic_cast<Vector<T> *>(srcElementVec.get());
    }

    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);
    int64_t outputOffset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        arrayResult->SetOffset(row, static_cast<int32_t>(outputOffset));

        if (arrayVec->IsNull(row)) {
            arrayResult->SetNull(row);
            continue;
        }

        arrayResult->SetNotNull(row);

        int64_t startOffset = srcOffsets[row];
        int64_t endOffset = srcOffsets[row + 1];
        if (typedSrcElementVec) {
            for (int64_t i = startOffset; i < endOffset; ++i) {
                if (typedSrcElementVec->IsNull(static_cast<int32_t>(i))) {
                    typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
                } else {
                    typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                        typedSrcElementVec->GetValue(static_cast<int32_t>(i)));
                }
                outputOffset++;
            }
        }

        if (IsElementNull(elementVec, row)) {
            typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
        } else {
            typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                GetElementValue<T>(elementVec, row));
        }
        outputOffset++;
    }

    arrayResult->SetOffset(rowSize, static_cast<int32_t>(outputOffset));
    result = arrayResult;
}

void ArrayAppendFunction::ProcessAppendVarchar(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
    int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    auto srcElementVec = arrayVec->GetElementVector();
    int64_t *srcOffsets = arrayVec->GetOffsets();

    int64_t totalSrcElements = srcElementVec ? srcElementVec->GetSize() : 0;
    int32_t nonNullRows = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!arrayVec->IsNull(row)) {
            nonNullRows++;
        }
    }
    int64_t totalOutputElements = totalSrcElements + nonNullRows;

    auto resultElementVec = std::shared_ptr<BaseVector>(
        new VarcharVector(static_cast<int32_t>(totalOutputElements > 0 ? totalOutputElements : 1), pool));
    auto *typedResultElementVec = dynamic_cast<VarcharVector *>(resultElementVec.get());
    if (!typedResultElementVec) {
        OMNI_THROW("ArrayAppendFunction Error:", "Failed to create result varchar vector");
    }

    VarcharVector *typedSrcElementVec = nullptr;
    if (srcElementVec) {
        typedSrcElementVec = dynamic_cast<VarcharVector *>(srcElementVec.get());
    }

    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);
    int64_t outputOffset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        arrayResult->SetOffset(row, static_cast<int32_t>(outputOffset));

        if (arrayVec->IsNull(row)) {
            arrayResult->SetNull(row);
            continue;
        }

        arrayResult->SetNotNull(row);

        int64_t startOffset = srcOffsets[row];
        int64_t endOffset = srcOffsets[row + 1];
        if (typedSrcElementVec) {
            for (int64_t i = startOffset; i < endOffset; ++i) {
                if (typedSrcElementVec->IsNull(static_cast<int32_t>(i))) {
                    typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
                } else {
                    typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset),
                        typedSrcElementVec->GetValue(static_cast<int32_t>(i)));
                }
                outputOffset++;
            }
        }

        if (IsElementNull(elementVec, row)) {
            typedResultElementVec->SetNull(static_cast<int32_t>(outputOffset));
        } else {
            std::string_view appendValue;
            if (elementVec->GetEncoding() == OMNI_ENCODING_CONST) {
                auto *constVec = dynamic_cast<ConstVector<std::string_view> *>(elementVec);
                if (constVec != nullptr) {
                    appendValue = constVec->GetConstValue();
                }
            } else {
                auto *varcharVec = dynamic_cast<VarcharVector *>(elementVec);
                if (varcharVec != nullptr) {
                    appendValue = varcharVec->GetValue(row);
                }
            }
            typedResultElementVec->SetValue(static_cast<int32_t>(outputOffset), appendValue);
        }
        outputOffset++;
    }

    arrayResult->SetOffset(rowSize, static_cast<int32_t>(outputOffset));
    result = arrayResult;
}

template void ArrayAppendFunction::ProcessAppend<int8_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<int16_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<int32_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<int64_t>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<float>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<double>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<bool>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayAppendFunction::ProcessAppend<Decimal128>(ArrayVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;

} // namespace omniruntime::vectorization
