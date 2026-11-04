/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayRepeat function implementation
 */

#include "ArrayRepeatFunction.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

static int32_t GetIntValue(BaseVector *vec, int32_t row)
{
    if (vec->GetTypeId() == OMNI_BOOLEAN) {
        if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
            return static_cast<int32_t>(static_cast<ConstVector<bool> *>(vec)->GetConstValue());
        }
        return static_cast<int32_t>(static_cast<Vector<bool> *>(vec)->GetValue(row));
    }
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<int32_t> *>(vec)->GetConstValue();
    }
    return static_cast<Vector<int32_t> *>(vec)->GetValue(row);
}

static bool IsVecNull(BaseVector *vec, int32_t row)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return vec->IsNull(0);
    }
    return vec->IsNull(row);
}

void ArrayRepeatFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArrayRepeatFunction Error:", "Expected 2 arguments (element, count)");
    }

    auto *countVec = args.top();
    args.pop();
    auto *elementVec = args.top();
    args.pop();

    if (countVec == nullptr) {
        delete elementVec;
        OMNI_THROW("ArrayRepeatFunction Error:", "Count vector is null");
    }

    int32_t rowSize = context->GetResultRowSize();
    DataTypeId elementTypeId = elementVec != nullptr ? elementVec->GetTypeId() : OMNI_NONE;

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessRepeat<int8_t>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_SHORT:
            ProcessRepeat<int16_t>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            ProcessRepeat<int32_t>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            ProcessRepeat<int64_t>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_FLOAT:
            ProcessRepeat<float>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_DOUBLE:
            ProcessRepeat<double>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_BOOLEAN:
            ProcessRepeat<bool>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_DECIMAL128:
            ProcessRepeat<Decimal128>(elementVec, countVec, result, rowSize, elementTypeId);
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            ProcessRepeatVarchar(elementVec, countVec, result, rowSize);
            break;
        case OMNI_NONE:
            ProcessRepeatAllNull(countVec, result, rowSize);
            break;
        default:
            delete countVec;
            delete elementVec;
            OMNI_THROW("ArrayRepeatFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }

    delete countVec;
    delete elementVec;
}

template <typename T>
void ArrayRepeatFunction::ProcessRepeat(BaseVector *elementVec, BaseVector *countVec, BaseVector *&result,
    int32_t rowSize, DataTypeId elementTypeId) const
{
    int64_t totalElements = 0;
    std::vector<int32_t> counts(rowSize, 0);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsVecNull(countVec, row)) {
            counts[row] = -1;
            continue;
        }
        int32_t count = GetIntValue(countVec, row);
        if (count < 0) {
            count = 0;
        }
        if (count > MAX_REPEAT_COUNT) {
            OMNI_THROW("ArrayRepeatFunction Error:",
                "Count argument must be less than or equal to " + std::to_string(MAX_REPEAT_COUNT));
        }
        counts[row] = count;
        totalElements += count;
    }

    auto elementResult = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(static_cast<int32_t>(elementTypeId), static_cast<int32_t>(totalElements)));
    auto *typedElementResult = dynamic_cast<Vector<T> *>(elementResult.get());
    if (!typedElementResult) {
        OMNI_THROW("ArrayRepeatFunction Error:", "Failed to create element vector");
    }

    auto *arrayResult = new ArrayVector(rowSize, elementResult);
    int64_t offset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        if (counts[row] == -1) {
            arrayResult->SetNull(row);
            arrayResult->SetOffset(row, static_cast<int32_t>(offset));
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
            continue;
        }

        int32_t count = counts[row];
        arrayResult->SetOffset(row, static_cast<int32_t>(offset));

        bool elemIsNull = IsVecNull(elementVec, row);

        if (elemIsNull) {
            for (int32_t i = 0; i < count; ++i) {
                typedElementResult->SetNull(static_cast<int32_t>(offset + i));
            }
        } else {
            T value;
            if (elementVec->GetEncoding() == OMNI_ENCODING_CONST) {
                value = static_cast<ConstVector<T> *>(elementVec)->GetConstValue();
            } else {
                value = static_cast<Vector<T> *>(elementVec)->GetValue(row);
            }
            for (int32_t i = 0; i < count; ++i) {
                typedElementResult->SetValue(static_cast<int32_t>(offset + i), value);
            }
        }

        offset += count;
        arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
    }

    result = arrayResult;
}

void ArrayRepeatFunction::ProcessRepeatVarchar(BaseVector *elementVec, BaseVector *countVec, BaseVector *&result,
    int32_t rowSize) const
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    int64_t totalElements = 0;
    std::vector<int32_t> counts(rowSize, 0);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsVecNull(countVec, row)) {
            counts[row] = -1;
            continue;
        }
        int32_t count = GetIntValue(countVec, row);
        if (count < 0) {
            count = 0;
        }
        if (count > MAX_REPEAT_COUNT) {
            OMNI_THROW("ArrayRepeatFunction Error:",
                "Count argument must be less than or equal to " + std::to_string(MAX_REPEAT_COUNT));
        }
        counts[row] = count;
        totalElements += count;
    }

    auto elementResult = std::shared_ptr<BaseVector>(
        new VarcharVector(static_cast<int32_t>(totalElements)));
    auto *typedElementResult = dynamic_cast<VarcharVector *>(elementResult.get());
    if (!typedElementResult) {
        OMNI_THROW("ArrayRepeatFunction Error:", "Failed to create varchar element vector");
    }

    auto *arrayResult = new ArrayVector(rowSize, elementResult);
    int64_t offset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        if (counts[row] == -1) {
            arrayResult->SetNull(row);
            arrayResult->SetOffset(row, static_cast<int32_t>(offset));
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
            continue;
        }

        int32_t count = counts[row];
        arrayResult->SetOffset(row, static_cast<int32_t>(offset));

        bool elemIsNull = IsVecNull(elementVec, row);

        if (elemIsNull) {
            for (int32_t i = 0; i < count; ++i) {
                typedElementResult->SetNull(static_cast<int32_t>(offset + i));
            }
        } else {
            std::string_view value = VectorHelper::GetStringValueFromVector(elementVec, row);
            for (int32_t i = 0; i < count; ++i) {
                typedElementResult->SetValue(static_cast<int32_t>(offset + i), value);
            }
        }

        offset += count;
        arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
    }

    result = arrayResult;
}

void ArrayRepeatFunction::ProcessRepeatAllNull(BaseVector *countVec, BaseVector *&result, int32_t rowSize) const
{
    int64_t totalElements = 0;
    std::vector<int32_t> counts(rowSize, 0);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsVecNull(countVec, row)) {
            counts[row] = -1;
            continue;
        }
        int32_t count = GetIntValue(countVec, row);
        if (count < 0) {
            count = 0;
        }
        if (count > MAX_REPEAT_COUNT) {
            OMNI_THROW("ArrayRepeatFunction Error:",
                "Count argument must be less than or equal to " + std::to_string(MAX_REPEAT_COUNT));
        }
        counts[row] = count;
        totalElements += count;
    }

    auto elementResult = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(static_cast<int32_t>(OMNI_INT), static_cast<int32_t>(totalElements)));
    auto *typedElementResult = dynamic_cast<Vector<int32_t> *>(elementResult.get());
    if (!typedElementResult) {
        OMNI_THROW("ArrayRepeatFunction Error:", "Failed to create element vector for all-null");
    }

    auto *arrayResult = new ArrayVector(rowSize, elementResult);
    int64_t offset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        if (counts[row] == -1) {
            arrayResult->SetNull(row);
            arrayResult->SetOffset(row, static_cast<int32_t>(offset));
            arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
            continue;
        }

        int32_t count = counts[row];
        arrayResult->SetOffset(row, static_cast<int32_t>(offset));

        for (int32_t i = 0; i < count; ++i) {
            typedElementResult->SetNull(static_cast<int32_t>(offset + i));
        }

        offset += count;
        arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
    }

    result = arrayResult;
}

template void ArrayRepeatFunction::ProcessRepeat<int8_t>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<int16_t>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<int32_t>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<int64_t>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<float>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<double>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<bool>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;
template void ArrayRepeatFunction::ProcessRepeat<Decimal128>(BaseVector *, BaseVector *, BaseVector *&,
    int32_t, DataTypeId) const;

} // namespace omniruntime::vectorization
