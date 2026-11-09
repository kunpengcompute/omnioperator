/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: NameStruct function implementation
 */

#include "NameStruct.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "type/data_operations.h"
#include "util/type_util.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void NameStructFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                               BaseVector *&result, ExecutionContext *context) const {
    auto *rowType = dynamic_cast<const RowType *>(outputType.get());
    if (rowType == nullptr) {
        OMNI_THROW("NameStruct Error", "Output type must be RowType");
    }
    const size_t fieldCount = static_cast<size_t>(rowType->Size());
    auto isNameValueLayout = [&](const std::vector<BaseVector *> &selected) {
        if (selected.size() != 2 * fieldCount) {
            return false;
        }
        for (size_t i = 0; i < fieldCount; ++i) {
            if (selected[2 * i] == nullptr || !TypeUtil::IsStringType(selected[2 * i]->GetTypeId())) {
                return false;
            }
        }
        return true;
    };
    // Pop only this call. A parent comparison leaves the other operand underneath.
    // ExprEval sets the count. A direct call leaves it at 0, so keep the top fields
    // and put any older stack entries back.
    std::vector<BaseVector *> callArgs;
    int32_t paramNums = context == nullptr ? 0 : context->getInputParamsNUms();
    if (paramNums <= 0) {
        std::vector<BaseVector *> popped;
        while (!args.empty()) {
            popped.push_back(args.top());
            args.pop();
        }
        auto takeTop = [&](size_t count) {
            std::vector<BaseVector *> selected(popped.begin(), popped.begin() + count);
            std::reverse(selected.begin(), selected.end());
            return selected;
        };
        size_t keep = 0;
        if (fieldCount > 0 && popped.size() >= 2 * fieldCount && isNameValueLayout(takeTop(2 * fieldCount))) {
            keep = 2 * fieldCount;
        } else if (fieldCount > 0 && popped.size() >= fieldCount) {
            keep = fieldCount;
        } else {
            OMNI_THROW("NameStruct Error", "Argument count mismatch: expected " +
                std::to_string(fieldCount) + " or " + std::to_string(2 * fieldCount) +
                " but stack has " + std::to_string(popped.size()));
        }
        callArgs = takeTop(keep);
        for (size_t i = popped.size(); i-- > keep;) {
            args.push(popped[i]);
        }
    } else {
        if (static_cast<size_t>(paramNums) > args.size()) {
            OMNI_THROW("NameStruct Error", "Argument count mismatch: expected " +
                std::to_string(paramNums) + " but stack has " + std::to_string(args.size()));
        }
        callArgs.reserve(static_cast<size_t>(paramNums));
        for (int32_t i = 0; i < paramNums; ++i) {
            callArgs.push_back(args.top());
            args.pop();
        }
        std::reverse(callArgs.begin(), callArgs.end());
    }

    std::vector<BaseVector *> argVectors;
    std::vector<BaseVector *> nameVectors;
    if (callArgs.size() == fieldCount) {
        argVectors = std::move(callArgs);
    } else if (isNameValueLayout(callArgs)) {
        argVectors.reserve(fieldCount);
        nameVectors.reserve(fieldCount);
        for (size_t i = 0; i < fieldCount; ++i) {
            nameVectors.push_back(callArgs[2 * i]);
            argVectors.push_back(callArgs[2 * i + 1]);
        }
    } else {
        OMNI_THROW("NameStruct Error", "Argument count mismatch: expected " +
            std::to_string(fieldCount) + " or " + std::to_string(2 * fieldCount) +
            " but got " + std::to_string(callArgs.size()));
    }
    int32_t size = argVectors[0]->GetSize();
    for (size_t i = 1; i < argVectors.size(); ++i) {
        if (argVectors[i]->GetSize() != size) {
            OMNI_THROW("NameStruct Error", "All arguments must have the same size");
        }
    }
    result = VectorHelper::CreateComplexVector(const_cast<DataType *>(outputType.get()), size);
    auto *resultRow = static_cast<RowVector *>(result);
    std::vector<int64_t> mapRunningOffsets(argVectors.size(), 0);
    for (int32_t row = 0; row < size; ++row) {
        for (size_t childIdx = 0; childIdx < argVectors.size(); ++childIdx) {
            BaseVector *srcVec = argVectors[childIdx];
            BaseVector *dstVec = resultRow->ChildAt(static_cast<int32_t>(childIdx)).get();
            int32_t readRow = (srcVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
            if (srcVec->IsNull(readRow)) {
                dstVec->SetNull(row);
            } else {
                DataTypeId fieldTypeId = rowType->Type(static_cast<int>(childIdx))->GetId();
                int64_t *pOffset = (fieldTypeId == OMNI_MAP) ? &mapRunningOffsets[childIdx] : nullptr;
                CopyFieldAtRow(srcVec, dstVec, row, fieldTypeId, readRow, pOffset);
            }
        }
    }
    for (auto *vec : argVectors) {
        if (vec != nullptr && !vec->GetIsField()) {
            delete vec;
        }
    }
    for (auto *vec : nameVectors) {
        if (vec != nullptr && !vec->GetIsField()) {
            delete vec;
        }
    }
}

void NameStructFunction::CopyFieldAtRow(BaseVector *srcVec, BaseVector *dstVec, int32_t dstRow,
                                        DataTypeId typeId, int32_t srcRow,
                                        int64_t *mapRunningOffset) const {
    switch (typeId) {
        case OMNI_BOOLEAN: {
            bool v = GetValueFromVector<bool>(srcVec, srcRow);
            SetValueToVector<bool>(dstVec, dstRow, v);
            break;
        }
        case OMNI_BYTE: {
            int8_t v = GetValueFromVector<int8_t>(srcVec, srcRow);
            SetValueToVector<int8_t>(dstVec, dstRow, v);
            break;
        }
        case OMNI_SHORT: {
            int16_t v = GetValueFromVector<int16_t>(srcVec, srcRow);
            SetValueToVector<int16_t>(dstVec, dstRow, v);
            break;
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            int32_t v = GetValueFromVector<int32_t>(srcVec, srcRow);
            SetValueToVector<int32_t>(dstVec, dstRow, v);
            break;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            int64_t v = GetValueFromVector<int64_t>(srcVec, srcRow);
            SetValueToVector<int64_t>(dstVec, dstRow, v);
            break;
        }
        case OMNI_FLOAT: {
            float v = GetValueFromVector<float>(srcVec, srcRow);
            SetValueToVector<float>(dstVec, dstRow, v);
            break;
        }
        case OMNI_DOUBLE: {
            double v = GetValueFromVector<double>(srcVec, srcRow);
            SetValueToVector<double>(dstVec, dstRow, v);
            break;
        }
        case OMNI_DECIMAL128: {
            Decimal128 v = GetValueFromVector<Decimal128>(srcVec, srcRow);
            SetValueToVector<Decimal128>(dstVec, dstRow, v);
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            std::string_view v = GetStringValueFromVector(srcVec, srcRow);
            SetStringValueToVector(dstVec, dstRow, const_cast<std::string_view &>(v));
            break;
        }
        case OMNI_ARRAY: {
            auto *srcArray = static_cast<ArrayVector *>(srcVec);
            auto *dstArray = static_cast<ArrayVector *>(dstVec);
            BaseVector *elemSlice = srcArray->GetValue(srcRow);
            dstArray->SetValue(dstRow, elemSlice);
            delete elemSlice;
            break;
        }
        case OMNI_MAP: {
            auto *srcMap = static_cast<MapVector *>(srcVec);
            auto *dstMap = static_cast<MapVector *>(dstVec);
            int64_t mapEntryCount = srcMap->GetSize(srcRow);
            int64_t srcStartOffset = srcMap->GetOffset(srcRow);
            int64_t totalBefore = mapRunningOffset ? *mapRunningOffset : 0;
            if (mapEntryCount > 0) {
                BaseVector *keySlice = srcMap->GetKeyVector()->Slice(static_cast<int32_t>(srcStartOffset),
                                                                     static_cast<int32_t>(mapEntryCount));
                BaseVector *valSlice = srcMap->GetValueVector()->Slice(static_cast<int32_t>(srcStartOffset),
                                                                       static_cast<int32_t>(mapEntryCount));
                VectorHelper::ExpandElementVector(dstMap->GetKeyVector().get(),
                                                  dstMap->GetKeyVector()->GetTypeId(),
                                                  static_cast<int32_t>(totalBefore + mapEntryCount));
                VectorHelper::ExpandElementVector(dstMap->GetValueVector().get(),
                                                  dstMap->GetValueVector()->GetTypeId(),
                                                  static_cast<int32_t>(totalBefore + mapEntryCount));
                VectorHelper::AppendVector(dstMap->GetKeyVector().get(), static_cast<int32_t>(totalBefore),
                                           keySlice, static_cast<int32_t>(mapEntryCount));
                VectorHelper::AppendVector(dstMap->GetValueVector().get(), static_cast<int32_t>(totalBefore),
                                           valSlice, static_cast<int32_t>(mapEntryCount));
                delete keySlice;
                delete valSlice;
            }
            dstMap->SetOffset(dstRow, static_cast<int32_t>(totalBefore));
            dstMap->SetSize(dstRow, static_cast<int32_t>(mapEntryCount));
            if (mapRunningOffset) {
                *mapRunningOffset += mapEntryCount;
            }
            break;
        }
        case OMNI_ROW: {
            auto *srcRowVec = static_cast<RowVector *>(srcVec);
            auto *dstRowVec = static_cast<RowVector *>(dstVec);
            for (int32_t i = 0; i < srcRowVec->ChildSize(); ++i) {
                CopyFieldAtRow(srcRowVec->ChildAt(i).get(), dstRowVec->ChildAt(i).get(), dstRow,
                               dstRowVec->ChildAt(i)->GetTypeId(), srcRow, nullptr);
            }
            break;
        }
        default:
            OMNI_THROW("NameStruct Error", "Unsupported field type: " + TypeUtil::TypeToString(typeId));
    }
}

template <typename T>
T NameStructFunction::GetValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<T> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<T> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("NameStruct Error", "Unsupported encoding type");
}

std::string_view NameStructFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("NameStruct Error", "Unsupported encoding type for string encoding=" +
        std::to_string(static_cast<int>(encoding)) + " type=" +
        std::to_string(static_cast<int>(vec->GetTypeId())));
}

template <typename T>
void NameStructFunction::SetValueToVector(BaseVector *vec, int32_t row, const T &value) const {
    auto *resultVec = static_cast<Vector<T> *>(vec);
    resultVec->SetValue(row, value);
}

void NameStructFunction::SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const {
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
}

template int8_t NameStructFunction::GetValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t NameStructFunction::GetValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t NameStructFunction::GetValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t NameStructFunction::GetValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float NameStructFunction::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double NameStructFunction::GetValueFromVector<double>(BaseVector *, int32_t) const;
template bool NameStructFunction::GetValueFromVector<bool>(BaseVector *, int32_t) const;
template omniruntime::type::Decimal128 NameStructFunction::GetValueFromVector<omniruntime::type::Decimal128>(BaseVector *, int32_t) const;

template void NameStructFunction::SetValueToVector<int8_t>(BaseVector *, int32_t, const int8_t &) const;
template void NameStructFunction::SetValueToVector<int16_t>(BaseVector *, int32_t, const int16_t &) const;
template void NameStructFunction::SetValueToVector<int32_t>(BaseVector *, int32_t, const int32_t &) const;
template void NameStructFunction::SetValueToVector<int64_t>(BaseVector *, int32_t, const int64_t &) const;
template void NameStructFunction::SetValueToVector<float>(BaseVector *, int32_t, const float &) const;
template void NameStructFunction::SetValueToVector<double>(BaseVector *, int32_t, const double &) const;
template void NameStructFunction::SetValueToVector<bool>(BaseVector *, int32_t, const bool &) const;
template void NameStructFunction::SetValueToVector<omniruntime::type::Decimal128>(BaseVector *, int32_t, const omniruntime::type::Decimal128 &) const;
}
