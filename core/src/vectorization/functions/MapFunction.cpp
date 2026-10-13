/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MapFunction implementation for constructing MAP vectors
 */

#include "MapFunction.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "type/data_operations.h"
#include "util/type_util.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void MapFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                        BaseVector *&result, ExecutionContext *context) const
{
    std::vector<BaseVector *> allArgs;
    while (!args.empty()) {
        allArgs.push_back(args.top());
        args.pop();
    }
    std::reverse(allArgs.begin(), allArgs.end());

    size_t argCount = allArgs.size();
    if (argCount % 2 != 0) {
        OMNI_THROW("MapFunction Error", "map requires an even number of arguments (>= 2)");
    }

    auto *mapType = dynamic_cast<const MapType *>(outputType.get());
    if (mapType == nullptr) {
        OMNI_THROW("MapFunction Error", "Output type must be MapType");
    }

    // MAP() with 0 arguments: create empty map for each row (Spark semantics)
    if (argCount == 0) {
        int32_t rowCount = context->GetResultRowSize();
        result = new MapVector(rowCount, context->GetPool());
        auto *dstMap = static_cast<MapVector *>(result);
        // Uses pool-bound overload (VectorHelper::CreateComplexVector); pool carried by ExecutionContext via GetPool().
        BaseVector *keyResult = VectorHelper::CreateComplexVector(
            context->GetPool(), const_cast<DataType *>(mapType->Key().get()), 0);
        BaseVector *valResult = VectorHelper::CreateComplexVector(
            context->GetPool(), const_cast<DataType *>(mapType->Value().get()), 0);
        dstMap->SetKeyVector(std::shared_ptr<BaseVector>(keyResult));
        dstMap->SetValueVector(std::shared_ptr<BaseVector>(valResult));
        for (int32_t row = 0; row < rowCount; ++row) {
            dstMap->SetSize(row, 0);
        }
        return;
    }

    size_t mapSize = argCount / 2;
    int32_t rowCount = allArgs[0]->GetSize();

    DataTypeId keyTypeId = mapType->Key()->GetId();
    DataTypeId valueTypeId = mapType->Value()->GetId();

    for (size_t row = 0; row < static_cast<size_t>(rowCount); ++row) {
        for (size_t i = 0; i < mapSize; ++i) {
            BaseVector *keyVec = allArgs[i * 2];
            int32_t readRow = (keyVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : static_cast<int32_t>(row);
            if (keyVec->IsNull(readRow)) {
                OMNI_THROW("MapFunction Error", "Cannot use null as map key!");
            }
        }
    }

    int64_t totalEntries = static_cast<int64_t>(rowCount) * static_cast<int64_t>(mapSize);

    // Uses pool-bound overload (VectorHelper::CreateComplexVector).
    BaseVector *keyResult = VectorHelper::CreateComplexVector(
        context->GetPool(), const_cast<DataType *>(mapType->Key().get()), static_cast<int32_t>(totalEntries));
    BaseVector *valResult = VectorHelper::CreateComplexVector(
        context->GetPool(), const_cast<DataType *>(mapType->Value().get()), static_cast<int32_t>(totalEntries));

    result = new MapVector(rowCount, std::shared_ptr<BaseVector>(keyResult),
                           std::shared_ptr<BaseVector>(valResult), context->GetPool());
    auto *dstMap = static_cast<MapVector *>(result);

    int64_t runningKeyOffset = 0;
    int64_t runningValOffset = 0;

    for (int32_t row = 0; row < rowCount; ++row) {
        dstMap->SetOffset(row, static_cast<int32_t>(runningKeyOffset));

        int32_t validCount = 0;
        for (size_t i = 0; i < mapSize; ++i) {
            BaseVector *curKey = allArgs[i * 2];
            int32_t keyRow = (curKey->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;

            bool isDuplicate = false;
            for (size_t j = i + 1; j < mapSize; ++j) {
                BaseVector *laterKey = allArgs[j * 2];
                int32_t laterRow = (laterKey->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
                if (ElementsEqual(curKey, keyRow, laterKey, laterRow, keyTypeId)) {
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate) {
                int32_t dstIdx = static_cast<int32_t>(runningKeyOffset + validCount);

                BaseVector *valVec = allArgs[i * 2 + 1];
                int32_t valRow = (valVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;

                CopyElement(curKey, keyRow, keyResult, dstIdx, keyTypeId, nullptr);

                if (valVec->IsNull(valRow)) {
                    valResult->SetNull(dstIdx);
                } else {
                    CopyElement(valVec, valRow, valResult, dstIdx, valueTypeId, nullptr);
                }
                validCount++;
            }
        }

        dstMap->SetSize(row, validCount);
        runningKeyOffset += validCount;
    }

    for (auto *vec : allArgs) {
        if (vec != nullptr && !vec->GetIsField()) {
            delete vec;
        }
    }
}

void MapFunction::CopyElement(BaseVector *src, int32_t srcRow, BaseVector *dst, int32_t dstRow,
                               DataTypeId typeId, int64_t *runningOffset) const
{
    switch (typeId) {
        case OMNI_BOOLEAN: {
            CopyValue<bool>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_BYTE: {
            CopyValue<int8_t>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_SHORT: {
            CopyValue<int16_t>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            CopyValue<int32_t>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            CopyValue<int64_t>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_FLOAT: {
            CopyValue<float>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_DOUBLE: {
            CopyValue<double>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_DECIMAL128: {
            CopyValue<Decimal128>(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            CopyStringValue(src, srcRow, dst, dstRow);
            break;
        }
        case OMNI_ARRAY: {
            auto *srcArray = static_cast<ArrayVector *>(src);
            auto *dstArray = static_cast<ArrayVector *>(dst);
            BaseVector *elemSlice = srcArray->GetValue(srcRow);
            dstArray->SetValue(dstRow, elemSlice);
            delete elemSlice;
            break;
        }
        case OMNI_MAP: {
            auto *srcMap = static_cast<MapVector *>(src);
            auto *dstMap = static_cast<MapVector *>(dst);
            int64_t entryCount = srcMap->GetSize(srcRow);
            int64_t srcStart = srcMap->GetOffset(srcRow);
            if (entryCount > 0) {
                BaseVector *keySlice = srcMap->GetKeyVector()->Slice(
                    static_cast<int32_t>(srcStart), static_cast<int32_t>(entryCount));
                BaseVector *valSlice = srcMap->GetValueVector()->Slice(
                    static_cast<int32_t>(srcStart), static_cast<int32_t>(entryCount));
                int64_t dstStart = dstMap->GetOffset(dstRow);
                VectorHelper::ExpandElementVector(dstMap->GetKeyVector().get(),
                    dstMap->GetKeyVector()->GetTypeId(),
                    static_cast<int32_t>(dstStart + entryCount));
                VectorHelper::ExpandElementVector(dstMap->GetValueVector().get(),
                    dstMap->GetValueVector()->GetTypeId(),
                    static_cast<int32_t>(dstStart + entryCount));
                VectorHelper::AppendVector(dstMap->GetKeyVector().get(),
                    static_cast<int32_t>(dstStart), keySlice, static_cast<int32_t>(entryCount));
                VectorHelper::AppendVector(dstMap->GetValueVector().get(),
                    static_cast<int32_t>(dstStart), valSlice, static_cast<int32_t>(entryCount));
                delete keySlice;
                delete valSlice;
            }
            dstMap->SetOffset(dstRow, static_cast<int32_t>(dstMap->GetOffset(dstRow)));
            dstMap->SetSize(dstRow, static_cast<int32_t>(entryCount));
            break;
        }
        case OMNI_ROW: {
            auto *srcRow2 = static_cast<RowVector *>(src);
            auto *dstRow2 = static_cast<RowVector *>(dst);
            for (int32_t i = 0; i < srcRow2->ChildSize(); ++i) {
                CopyElement(srcRow2->ChildAt(i).get(), srcRow, dstRow2->ChildAt(i).get(), dstRow,
                            dstRow2->ChildAt(i)->GetTypeId(), nullptr);
            }
            break;
        }
        default:
            OMNI_THROW("MapFunction Error", "Unsupported type: " + TypeUtil::TypeToString(typeId));
    }
}

template <typename T>
void MapFunction::CopyValue(BaseVector *src, int32_t srcRow, BaseVector *dst, int32_t dstRow) const
{
    T value = ReadValue<T>(src, srcRow);
    static_cast<Vector<T> *>(dst)->SetValue(dstRow, value);
}

void MapFunction::CopyStringValue(BaseVector *src, int32_t srcRow, BaseVector *dst, int32_t dstRow) const
{
    std::string_view value = ReadStringValue(src, srcRow);
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(dst)->SetValue(dstRow, value);
}

template <typename T>
T MapFunction::ReadValue(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<T> *>(vec)->GetConstValue();
    }
    if (encoding == OMNI_DICTIONARY) {
        return static_cast<Vector<DictionaryContainer<T>> *>(vec)->GetValue(row);
    }
    return static_cast<Vector<T> *>(vec)->GetValue(row);
}

std::string_view MapFunction::ReadStringValue(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<std::string_view> *>(vec)->GetConstValue();
    }
    if (encoding == OMNI_DICTIONARY) {
        return static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec)->GetValue(row);
    }
    return static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec)->GetValue(row);
}

bool MapFunction::ElementsEqual(BaseVector *vec1, int32_t row1, BaseVector *vec2, int32_t row2,
                                 DataTypeId typeId) const
{
    if (vec1->IsNull(row1) && vec2->IsNull(row2)) return true;
    if (vec1->IsNull(row1) || vec2->IsNull(row2)) return false;

    switch (typeId) {
        case OMNI_BOOLEAN: return ValuesEqual<bool>(vec1, row1, vec2, row2);
        case OMNI_BYTE: return ValuesEqual<int8_t>(vec1, row1, vec2, row2);
        case OMNI_SHORT: return ValuesEqual<int16_t>(vec1, row1, vec2, row2);
        case OMNI_INT:
        case OMNI_DATE32: return ValuesEqual<int32_t>(vec1, row1, vec2, row2);
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: return ValuesEqual<int64_t>(vec1, row1, vec2, row2);
        case OMNI_FLOAT: return ValuesEqual<float>(vec1, row1, vec2, row2);
        case OMNI_DOUBLE: return ValuesEqual<double>(vec1, row1, vec2, row2);
        case OMNI_DECIMAL128: return ValuesEqual<Decimal128>(vec1, row1, vec2, row2);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: return StringValuesEqual(vec1, row1, vec2, row2);
        default: return false;
    }
}

template <typename T>
bool MapFunction::ValuesEqual(BaseVector *vec1, int32_t row1, BaseVector *vec2, int32_t row2) const
{
    return ReadValue<T>(vec1, row1) == ReadValue<T>(vec2, row2);
}

bool MapFunction::StringValuesEqual(BaseVector *vec1, int32_t row1, BaseVector *vec2, int32_t row2) const
{
    return ReadStringValue(vec1, row1) == ReadStringValue(vec2, row2);
}

template bool MapFunction::ReadValue<bool>(BaseVector *, int32_t) const;
template int8_t MapFunction::ReadValue<int8_t>(BaseVector *, int32_t) const;
template int16_t MapFunction::ReadValue<int16_t>(BaseVector *, int32_t) const;
template int32_t MapFunction::ReadValue<int32_t>(BaseVector *, int32_t) const;
template int64_t MapFunction::ReadValue<int64_t>(BaseVector *, int32_t) const;
template float MapFunction::ReadValue<float>(BaseVector *, int32_t) const;
template double MapFunction::ReadValue<double>(BaseVector *, int32_t) const;
template Decimal128 MapFunction::ReadValue<Decimal128>(BaseVector *, int32_t) const;

template void MapFunction::CopyValue<bool>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<int8_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<int16_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<int32_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<int64_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<float>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<double>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template void MapFunction::CopyValue<Decimal128>(BaseVector *, int32_t, BaseVector *, int32_t) const;

template bool MapFunction::ValuesEqual<bool>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<int8_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<int16_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<int32_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<int64_t>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<float>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<double>(BaseVector *, int32_t, BaseVector *, int32_t) const;
template bool MapFunction::ValuesEqual<Decimal128>(BaseVector *, int32_t, BaseVector *, int32_t) const;
}
