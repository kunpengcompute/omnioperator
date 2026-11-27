/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Coalesce function implementation for vectorized conditional expressions
 */

#include "Coalesce.h"
#include "vector/vector.h"
#include <algorithm>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

void CoalesceFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                            BaseVector *&result, ExecutionContext *context) const {
    std::vector<BaseVector *> argVectors;
    argVectors.push_back(args.top());
    args.pop();
    argVectors.push_back(args.top());
    args.pop();
    std::reverse(argVectors.begin(), argVectors.end());
    // Per-task pool is threaded to all CoalesceXxx helpers via DispatchCoalesce.
    DispatchCoalesce(argVectors, outputType, result, context->GetPool());

    for (auto* vec : argVectors) {
        delete vec;
    }
}

void CoalesceFunction::DispatchCoalesce(const std::vector<BaseVector *> &argVectors,
                                        const DataTypePtr &outputType, BaseVector *&result,
                                        omniruntime::mem::OmniTaskMemoryPool *pool) const {
    DataTypeId outputTypeId = outputType->GetId();

    if (TypeUtil::IsStringType(outputTypeId)) {
        CoalesceString(argVectors, result, outputType, pool);
    } else if (outputTypeId == OMNI_BOOLEAN) {
        CoalesceBoolean(argVectors, result, outputType, pool);
    } else if (outputTypeId == OMNI_ARRAY) {
        CoalesceArray(argVectors, result, outputType, pool);
    } else if (outputTypeId == OMNI_MAP) {
        CoalesceMap(argVectors, result, outputType, pool);
    } else if (outputTypeId == OMNI_ROW) {
        CoalesceRow(argVectors, result, outputType, pool);
    } else {
        switch (outputTypeId) {
            case OMNI_BYTE:
                CoalesceNumeric<int8_t>(argVectors, result, outputType, pool);
                break;
            case OMNI_SHORT:
                CoalesceNumeric<int16_t>(argVectors, result, outputType, pool);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                CoalesceNumeric<int32_t>(argVectors, result, outputType, pool);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                CoalesceNumeric<int64_t>(argVectors, result, outputType, pool);
                break;
            case OMNI_FLOAT:
                CoalesceNumeric<float>(argVectors, result, outputType, pool);
                break;
            case OMNI_DOUBLE:
                CoalesceNumeric<double>(argVectors, result, outputType, pool);
                break;
            default:
                OMNI_THROW("Coalesce function Error",
                        "Unsupported output type: " + TypeUtil::TypeToString(outputTypeId));
        }
    }
}

template<typename T>
void CoalesceFunction::CoalesceNumeric(const std::vector<BaseVector *> &argVectors,
                                       BaseVector *&result, const DataTypePtr &outputType,
                                       omniruntime::mem::OmniTaskMemoryPool *pool) const {
    auto size = argVectors[0]->GetSize();
    result = VectorHelper::CreateFlatVector(pool, outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        bool found = false;
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                T value = GetValueFromVector<T>(argVectors[argIdx], row);
                SetValueToVector(result, row, value);
                found = true;
                break;
            }
        }
        if (!found) {
            result->SetNull(row);
        }
    }
}

void CoalesceFunction::CoalesceString(const std::vector<BaseVector *> &argVectors,
                                     BaseVector *&result, const DataTypePtr &outputType,
                                     omniruntime::mem::OmniTaskMemoryPool *pool) const {
    auto size = argVectors[0]->GetSize();
    result = VectorHelper::CreateFlatVector(pool, outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        bool found = false;
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                std::string_view value = GetStringValueFromVector(argVectors[argIdx], row);
                SetStringValueToVector(result, row, value);
                found = true;
                break;
            }
        }
        if (!found) {
            result->SetNull(row);
        }
    }
}

void CoalesceFunction::CoalesceBoolean(const std::vector<BaseVector *> &argVectors,
                                      BaseVector *&result, const DataTypePtr &outputType,
                                      omniruntime::mem::OmniTaskMemoryPool *pool) const {
    auto size = argVectors[0]->GetSize();
    result = VectorHelper::CreateFlatVector(pool, outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        bool found = false;
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                bool value = GetValueFromVector<bool>(argVectors[argIdx], row);
                SetValueToVector(result, row, value);
                found = true;
                break;
            }
        }
        if (!found) {
            result->SetNull(row);
        }
    }
}

void CoalesceFunction::CoalesceArray(const std::vector<BaseVector *> &argVectors,
                                     BaseVector *&result, const DataTypePtr &outputType,
                                     omniruntime::mem::OmniTaskMemoryPool *pool) const {
    auto size = argVectors[0]->GetSize();

    result = VectorHelper::CreateComplexVector(pool, outputType.get(), size);
    auto *resultArray = static_cast<ArrayVector *>(result);

    for (int32_t row = 0; row < size; ++row) {
        bool found = false;
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                auto *srcArray = static_cast<ArrayVector *>(argVectors[argIdx]);
                BaseVector *elemSlice = srcArray->GetValue(row);
                resultArray->SetValue(row, elemSlice);
                delete elemSlice;
                found = true;
                break;
            }
        }
        if (!found) {
            resultArray->SetNull(row);
        }
    }
}

void CoalesceFunction::CoalesceMap(const std::vector<BaseVector *> &argVectors,
                                   BaseVector *&result, const DataTypePtr &outputType,
                                   omniruntime::mem::OmniTaskMemoryPool *pool) const {
    auto size = argVectors[0]->GetSize();
    std::vector<int32_t> sourceIdx(size, -1);
    for (int32_t row = 0; row < size; ++row) {
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                sourceIdx[row] = static_cast<int32_t>(argIdx);
                break;
            }
        }
    }

    auto *firstSrcMap = static_cast<MapVector *>(argVectors[0]);

    result = VectorHelper::CreateComplexVector(pool, outputType.get(), size);
    auto *resultMap = static_cast<MapVector *>(result);

    int64_t totalElements = 0;

    for (int32_t row = 0; row < size; ++row) {
        if (sourceIdx[row] < 0) {
            resultMap->SetNull(row);
            continue;
        }

        auto *srcMap = static_cast<MapVector *>(argVectors[sourceIdx[row]]);
        int64_t mapEntryCount = srcMap->GetSize(row);
        int64_t srcStartOffset = srcMap->GetOffset(row);

        if (mapEntryCount > 0) {
            auto keyVec = resultMap->GetKeyVector();
            auto valVec = resultMap->GetValueVector();

            BaseVector *keySlice = srcMap->GetKeyVector()->Slice(srcStartOffset, mapEntryCount);
            BaseVector *valSlice = srcMap->GetValueVector()->Slice(srcStartOffset, mapEntryCount);

            VectorHelper::ExpandElementVector(keyVec.get(), keyVec->GetTypeId(),
                                              totalElements + mapEntryCount);
            VectorHelper::AppendVector(keyVec.get(), totalElements,
                                       keySlice,
                                       static_cast<int32_t>(mapEntryCount));

            VectorHelper::ExpandElementVector(valVec.get(), valVec->GetTypeId(),
                                              totalElements + mapEntryCount);
            VectorHelper::AppendVector(valVec.get(), totalElements,
                                       valSlice,
                                       static_cast<int32_t>(mapEntryCount));

            delete keySlice;
            delete valSlice;
        }

        resultMap->SetSize(row, static_cast<int32_t>(mapEntryCount));
        totalElements += mapEntryCount;
    }
}

void CoalesceFunction::CoalesceRow(const std::vector<BaseVector *> &argVectors,
                                   BaseVector *&result, const DataTypePtr &outputType,
                                   omniruntime::mem::OmniTaskMemoryPool *pool) const {
    auto size = argVectors[0]->GetSize();

    std::vector<int32_t> sourceIdx(size, -1);
    for (int32_t row = 0; row < size; ++row) {
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                sourceIdx[row] = static_cast<int32_t>(argIdx);
                break;
            }
        }
    }

    result = VectorHelper::CreateComplexVector(pool, outputType.get(), size);
    auto *resultRow = static_cast<RowVector *>(result);

    for (int32_t row = 0; row < size; ++row) {
        if (sourceIdx[row] < 0) {
            resultRow->SetNull(row);
            for (int32_t childIdx = 0; childIdx < resultRow->ChildSize(); ++childIdx) {
                resultRow->ChildAt(childIdx)->SetNull(row);
            }
            continue;
        }

        auto *srcRow = static_cast<RowVector *>(argVectors[sourceIdx[row]]);

        for (int32_t childIdx = 0; childIdx < resultRow->ChildSize(); ++childIdx) {
            BaseVector *srcChild = srcRow->ChildAt(childIdx).get();
            BaseVector *dstChild = resultRow->ChildAt(childIdx).get();

            if (srcChild->IsNull(row)) {
                dstChild->SetNull(row);
            } else {
                DataTypeId childTypeId = srcChild->GetTypeId();
                switch (childTypeId) {
                    case OMNI_BOOLEAN: {
                        auto val = static_cast<Vector<bool> *>(srcChild)->GetValue(row);
                        static_cast<Vector<bool> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_BYTE: {
                        auto val = static_cast<Vector<int8_t> *>(srcChild)->GetValue(row);
                        static_cast<Vector<int8_t> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_SHORT: {
                        auto val = static_cast<Vector<int16_t> *>(srcChild)->GetValue(row);
                        static_cast<Vector<int16_t> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_INT:
                    case OMNI_DATE32: {
                        auto val = static_cast<Vector<int32_t> *>(srcChild)->GetValue(row);
                        static_cast<Vector<int32_t> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_LONG:
                    case OMNI_TIMESTAMP:
                    case OMNI_DECIMAL64: {
                        auto val = static_cast<Vector<int64_t> *>(srcChild)->GetValue(row);
                        static_cast<Vector<int64_t> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_FLOAT: {
                        auto val = static_cast<Vector<float> *>(srcChild)->GetValue(row);
                        static_cast<Vector<float> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_DOUBLE: {
                        auto val = static_cast<Vector<double> *>(srcChild)->GetValue(row);
                        static_cast<Vector<double> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    case OMNI_VARCHAR:
                    case OMNI_CHAR:
                    case OMNI_VARBINARY: {
                        std::string_view strVal = GetStringValueFromVector(srcChild, row);
                        SetStringValueToVector(dstChild, row, strVal);
                        break;
                    }
                    case OMNI_DECIMAL128: {
                        auto val = static_cast<Vector<Decimal128> *>(srcChild)->GetValue(row);
                        static_cast<Vector<Decimal128> *>(dstChild)->SetValue(row, val);
                        break;
                    }
                    default:
                        OMNI_THROW("Coalesce function Error",
                                   "Unsupported child type in ROW: " +
                                   TypeUtil::TypeToString(childTypeId));
                }
            }
        }
    }
}

template<typename T>
T CoalesceFunction::GetValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<T> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<T> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("Coalesce function Error", "Unsupported encoding type");
    }
}

std::string_view CoalesceFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("Coalesce function Error", "Unsupported encoding type for string");
    }
}

template<typename T>
void CoalesceFunction::SetValueToVector(BaseVector *vec, int32_t row, const T &value) const {
    auto *resultVec = static_cast<Vector<T> *>(vec);
    resultVec->SetValue(row, value);
}

void CoalesceFunction::SetStringValueToVector(BaseVector *vec, int32_t row,
                                             const std::string_view &value) const {
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
    resultVec->SetNotNull(row);
}

// Explicit template instantiations
template void CoalesceFunction::CoalesceNumeric<int8_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;
template void CoalesceFunction::CoalesceNumeric<int16_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;
template void CoalesceFunction::CoalesceNumeric<int32_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;
template void CoalesceFunction::CoalesceNumeric<int64_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;
template void CoalesceFunction::CoalesceNumeric<float>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;
template void CoalesceFunction::CoalesceNumeric<double>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;

template int8_t CoalesceFunction::GetValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t CoalesceFunction::GetValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t CoalesceFunction::GetValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t CoalesceFunction::GetValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float CoalesceFunction::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double CoalesceFunction::GetValueFromVector<double>(BaseVector *, int32_t) const;
template bool CoalesceFunction::GetValueFromVector<bool>(BaseVector *, int32_t) const;

template void CoalesceFunction::SetValueToVector<int8_t>(BaseVector *, int32_t, const int8_t &) const;
template void CoalesceFunction::SetValueToVector<int16_t>(BaseVector *, int32_t, const int16_t &) const;
template void CoalesceFunction::SetValueToVector<int32_t>(BaseVector *, int32_t, const int32_t &) const;
template void CoalesceFunction::SetValueToVector<int64_t>(BaseVector *, int32_t, const int64_t &) const;
template void CoalesceFunction::SetValueToVector<float>(BaseVector *, int32_t, const float &) const;
template void CoalesceFunction::SetValueToVector<double>(BaseVector *, int32_t, const double &) const;
template void CoalesceFunction::SetValueToVector<bool>(BaseVector *, int32_t, const bool &) const;

}
