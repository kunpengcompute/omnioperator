/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: If conditional function for vectorized expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_helper.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class IfFunction : public VectorFunction {
public:
    explicit IfFunction() {}

    /// Creates the result vector for an if expression, mirroring the structure of
    /// sourceVec (used for complex types whose children must be allocated upfront).
    static void CreateResultVector(DataTypeId typeId, int32_t size, BaseVector *sourceVec,
        BaseVector *&result)
    {
        switch (typeId) {
            case OMNI_ARRAY :
            {
                auto *srcArray = static_cast<ArrayVector *>(sourceVec);
                auto elemVec = std::shared_ptr<BaseVector>(
                    VectorHelper::CreateFlatVector(srcArray->GetElementVector()->GetTypeId(), 0));
                result = new ArrayVector(size, elemVec);
                break;
            }
            case OMNI_MAP :
            {
                auto *srcMap = static_cast<MapVector *>(sourceVec);
                auto keyVec = std::shared_ptr<BaseVector>(
                    VectorHelper::CreateFlatVector(srcMap->GetKeyVector()->GetTypeId(), 0));
                auto valVec = std::shared_ptr<BaseVector>(
                    VectorHelper::CreateFlatVector(srcMap->GetValueVector()->GetTypeId(), 0));
                result = new MapVector(size, keyVec, valVec);
                break;
            }
            case OMNI_ROW :
            {
                auto *srcRow = static_cast<RowVector *>(sourceVec);
                auto *resRow = new RowVector(size);
                for (int32_t c = 0; c < srcRow->ChildSize(); ++c) {
                    auto childTypeId = srcRow->ChildAt(c)->GetTypeId();
                    resRow->AddChild(VectorHelper::CreateFlatVector(childTypeId, size));
                }
                result = resRow;
                break;
            }
            default :
                result = VectorHelper::CreateFlatVector(typeId, size);
                break;
        }
    }

    static void CopyArrayValue(BaseVector *source, BaseVector *result, int32_t srcRow, int32_t dstRow)
    {
        auto *srcArray = static_cast<ArrayVector *>(source);
        auto *resArray = static_cast<ArrayVector *>(result);
        BaseVector *elemSlice = srcArray->GetValue(srcRow);
        resArray->SetValue(dstRow, elemSlice);
        delete elemSlice;
    }

    static void CopyMapValue(BaseVector *source, BaseVector *result, int32_t srcRow, int32_t dstRow)
    {
        auto *srcMap = static_cast<MapVector *>(source);
        auto *resMap = static_cast<MapVector *>(result);
        int64_t mapEntryCount = srcMap->GetSize(srcRow);
        int64_t srcStartOffset = srcMap->GetOffset(srcRow);
        int64_t resStartOffset = resMap->GetOffset(dstRow);

        if (mapEntryCount > 0) {
            BaseVector *resKeys = const_cast<BaseVector *>(resMap->GetKeyVector().get());
            VectorHelper::ExpandElementVector(resKeys, resKeys->GetTypeId(),
                static_cast<int32_t>(resStartOffset + mapEntryCount));
            BaseVector *srcKeySlice = srcMap->GetKeyVector()->Slice(
                static_cast<int>(srcStartOffset), static_cast<int>(mapEntryCount));
            VectorHelper::AppendVector(resKeys, static_cast<int32_t>(resStartOffset),
                srcKeySlice, static_cast<int32_t>(mapEntryCount));
            delete srcKeySlice;

            BaseVector *resValues = const_cast<BaseVector *>(resMap->GetValueVector().get());
            VectorHelper::ExpandElementVector(resValues, resValues->GetTypeId(),
                static_cast<int32_t>(resStartOffset + mapEntryCount));
            BaseVector *srcValSlice = srcMap->GetValueVector()->Slice(
                static_cast<int>(srcStartOffset), static_cast<int>(mapEntryCount));
            VectorHelper::AppendVector(resValues, static_cast<int32_t>(resStartOffset),
                srcValSlice, static_cast<int32_t>(mapEntryCount));
            delete srcValSlice;
        }
        resMap->SetSize(dstRow, static_cast<int32_t>(mapEntryCount));
    }

    static void CopyRowValue(BaseVector *source, BaseVector *result, int32_t srcRow, int32_t dstRow)
    {
        auto *srcRowVec = static_cast<RowVector *>(source);
        auto *resRow = static_cast<RowVector *>(result);

        for (int32_t c = 0; c < srcRowVec->ChildSize(); ++c) {
            BaseVector *srcChild = srcRowVec->ChildAt(c).get();
            BaseVector *resChild = resRow->ChildAt(c).get();
            if (srcChild->IsNull(srcRow)) {
                resChild->SetNull(dstRow);
            } else {
                VectorHelper::CopyValue(srcChild, srcRow, resChild, dstRow);
            }
        }
    }

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        auto falseVec = args.top();
        args.pop();
        auto trueVec = args.top();
        args.pop();
        auto condVec = args.top();
        args.pop();
        auto *boolVec = static_cast<Vector<bool> *>(condVec);
        auto size = context->GetResultRowSize();
        DataTypeId typeId = outputType->GetId();
        CreateResultVector(typeId, size, trueVec, result);

        bool condHasNull = boolVec->HasNull();
        bool trueHasNull = trueVec->HasNull();
        bool falseHasNull = falseVec->HasNull();
        bool anyNull = condHasNull || trueHasNull || falseHasNull;

        switch (typeId) {
            case OMNI_BOOLEAN:
                ApplyTyped<bool>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_BYTE:
                ApplyTyped<int8_t>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_SHORT:
                ApplyTyped<int16_t>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                ApplyTyped<int32_t>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                ApplyTyped<int64_t>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_FLOAT:
                ApplyTyped<float>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_DOUBLE:
                ApplyTyped<double>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_VARCHAR:
            case OMNI_VARBINARY:
                ApplyString(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_DECIMAL128:
                ApplyTyped<Decimal128>(boolVec, trueVec, falseVec, result, size, anyNull, condHasNull);
                break;
            case OMNI_ARRAY:
                ApplyComplex(boolVec, trueVec, falseVec, result, size, typeId, anyNull, condHasNull);
                break;
            case OMNI_MAP:
                ApplyComplex(boolVec, trueVec, falseVec, result, size, typeId, anyNull, condHasNull);
                break;
            case OMNI_ROW:
                ApplyComplex(boolVec, trueVec, falseVec, result, size, typeId, anyNull, condHasNull);
                break;
            default:
                OMNI_THROW("If expr Error", "Unsupported output type Id:" + std::to_string(typeId));
        }

        delete falseVec;
        delete trueVec;
        delete condVec;
    }

private:
    template <typename T>
    void ApplyTyped(Vector<bool> *boolVec, BaseVector *trueVec, BaseVector *falseVec,
        BaseVector *result, int32_t size, bool anyNull, bool condHasNull) const
    {
        bool trueIsFlat = (trueVec->GetEncoding() == OMNI_FLAT);
        bool falseIsFlat = (falseVec->GetEncoding() == OMNI_FLAT);

        auto *resVec = static_cast<Vector<T> *>(result);
        T *resData = resVec->GetValuesBuffer();

        T *trueData = nullptr;
        T *falseData = nullptr;
        if (trueIsFlat) {
            trueData = static_cast<Vector<T> *>(trueVec)->GetValuesBuffer();
        }
        if (falseIsFlat) {
            falseData = static_cast<Vector<T> *>(falseVec)->GetValuesBuffer();
        }

        if (!anyNull && trueIsFlat && falseIsFlat) {
            for (int32_t row = 0; row < size; ++row) {
                resData[row] = boolVec->GetValue(row) ? trueData[row] : falseData[row];
            }
            return;
        }

        for (int32_t row = 0; row < size; ++row) {
            BaseVector *temp = nullptr;
            if (condHasNull && boolVec->IsNull(row)) {
                temp = falseVec;
            } else {
                temp = boolVec->GetValue(row) ? trueVec : falseVec;
            }

            if (temp->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            if (trueIsFlat && temp == trueVec) {
                resData[row] = static_cast<Vector<T> *>(trueVec)->GetValue(row);
            } else if (falseIsFlat && temp == falseVec) {
                resData[row] = static_cast<Vector<T> *>(falseVec)->GetValue(row);
            } else {
                resData[row] = VectorHelper::GetValueFromVector<T>(temp, row);
            }
        }
    }

    void ApplyString(Vector<bool> *boolVec, BaseVector *trueVec, BaseVector *falseVec,
        BaseVector *result, int32_t size, bool anyNull, bool condHasNull) const
    {
        auto *resVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);

        if (!anyNull) {
            bool trueIsFlat = (trueVec->GetEncoding() == OMNI_FLAT);
            bool falseIsFlat = (falseVec->GetEncoding() == OMNI_FLAT);

            if (trueIsFlat && falseIsFlat) {
                auto *trueStrVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(trueVec);
                auto *falseStrVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(falseVec);
                for (int32_t row = 0; row < size; ++row) {
                    resVec->SetValue(row, boolVec->GetValue(row) ? trueStrVec->GetValue(row) : falseStrVec->GetValue(row));
                }
                return;
            }
        }

        for (int32_t row = 0; row < size; ++row) {
            BaseVector *temp = nullptr;
            if (condHasNull && boolVec->IsNull(row)) {
                temp = falseVec;
            } else {
                temp = boolVec->GetValue(row) ? trueVec : falseVec;
            }

            if (temp->IsNull(row)) {
                resVec->SetNull(row);
                continue;
            }

            auto res = VectorHelper::GetStringValueFromVector(temp, row);
            resVec->SetValue(row, res);
        }
    }

    void ApplyComplex(Vector<bool> *boolVec, BaseVector *trueVec, BaseVector *falseVec,
        BaseVector *result, int32_t size, DataTypeId typeId, bool anyNull, bool condHasNull) const
    {
        for (int32_t row = 0; row < size; ++row) {
            BaseVector *temp = nullptr;
            if (condHasNull && boolVec->IsNull(row)) {
                temp = falseVec;
            } else {
                temp = boolVec->GetValue(row) ? trueVec : falseVec;
            }

            if (temp->IsNull(row)) {
                if (typeId == OMNI_ARRAY) {
                    static_cast<ArrayVector *>(result)->SetNull(row);
                } else if (typeId == OMNI_MAP) {
                    static_cast<MapVector *>(result)->SetNull(row);
                } else {
                    result->SetNull(row);
                }
                continue;
            }

            switch (typeId) {
                case OMNI_ARRAY:
                    CopyArrayValue(temp, result, row, row);
                    break;
                case OMNI_MAP:
                    CopyMapValue(temp, result, row, row);
                    break;
                case OMNI_ROW:
                    CopyRowValue(temp, result, row, row);
                    break;
                default:
                    break;
            }
        }
    }
};
}
