/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: SortArray function implementation for sorting array elements
 */

#include "SortArrayFunction.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {

bool GetAscendingFlag(BaseVector *ascArg)
{
    if (ascArg->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<bool> *>(ascArg)->GetConstValue();
    }
    return dynamic_cast<Vector<bool> *>(ascArg)->GetValue(0);
}

template <typename T>
struct NaNAwareLess {
    bool operator()(const T &a, const T &b) const
    {
        bool aIsNaN = std::isnan(a);
        bool bIsNaN = std::isnan(b);
        if (aIsNaN && bIsNaN) {
            return false;
        }
        if (aIsNaN) {
            return false;
        }
        if (bIsNaN) {
            return true;
        }
        return a < b;
    }
};

template <typename T>
struct NaNAwareGreater {
    bool operator()(const T &a, const T &b) const
    {
        bool aIsNaN = std::isnan(a);
        bool bIsNaN = std::isnan(b);
        if (aIsNaN && bIsNaN) {
            return false;
        }
        if (aIsNaN) {
            return true;
        }
        if (bIsNaN) {
            return false;
        }
        return a > b;
    }
};

} // anonymous namespace

BaseVector *SortArrayFunction::CreateElementVectorByType(DataTypeId typeId, int64_t size,
    omniruntime::mem::OmniTaskMemoryPool *pool)
{
    int64_t allocSize = size > 0 ? size : 0;
    if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
        return VectorHelper::CreateStringVector(pool, static_cast<uint32_t>(allocSize));
    }
    return VectorHelper::CreateFlatVector(pool, typeId, static_cast<int32_t>(allocSize));
}

void SortArrayFunction::CopyElementValue(BaseVector *srcElem, int32_t srcIdx,
    BaseVector *dstElem, int32_t dstIdx)
{
    if (srcElem->IsNull(srcIdx)) {
        dstElem->SetNull(dstIdx);
    } else {
        dstElem->SetNotNull(dstIdx);
        VectorHelper::CopyValue(srcElem, srcIdx, dstElem, dstIdx);
    }
}

template <typename T>
void SortArrayFunction::SortTypedElements(BaseVector *srcElem, BaseVector *dstElem,
    int64_t offset, int64_t arrSize, bool ascending) const
{
    auto *srcVec = dynamic_cast<Vector<T> *>(srcElem);
    auto *dstVec = dynamic_cast<Vector<T> *>(dstElem);

    std::vector<size_t> indices(static_cast<size_t>(arrSize));
    for (size_t i = 0; i < static_cast<size_t>(arrSize); ++i) {
        indices[i] = i;
    }

    bool nullsFirst = ascending;
    std::sort(indices.begin(), indices.end(),
        [&](size_t a, size_t b) -> bool {
            int32_t idxA = static_cast<int32_t>(offset + static_cast<int64_t>(a));
            int32_t idxB = static_cast<int32_t>(offset + static_cast<int64_t>(b));
            bool aIsNull = srcElem->IsNull(idxA);
            bool bIsNull = srcElem->IsNull(idxB);

            if (aIsNull && bIsNull) {
                return false;
            }
            if (aIsNull) {
                return nullsFirst;
            }
            if (bIsNull) {
                return !nullsFirst;
            }

            T valA = srcVec->GetValue(idxA);
            T valB = srcVec->GetValue(idxB);

            if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                if (ascending) {
                    return NaNAwareLess<T>{}(valA, valB);
                } else {
                    return NaNAwareGreater<T>{}(valA, valB);
                }
            } else {
                if (ascending) {
                    return valA < valB;
                } else {
                    return valA > valB;
                }
            }
        });

    for (size_t i = 0; i < static_cast<size_t>(arrSize); ++i) {
        int32_t srcIdx = static_cast<int32_t>(offset + static_cast<int64_t>(indices[i]));
        int32_t dstIdx = static_cast<int32_t>(i);
        if (srcElem->IsNull(srcIdx)) {
            dstElem->SetNull(dstIdx);
        } else {
            dstElem->SetNotNull(dstIdx);
            dstVec->SetValue(dstIdx, srcVec->GetValue(srcIdx));
        }
    }
}

void SortArrayFunction::SortStringElements(BaseVector *srcElem, BaseVector *dstElem,
    int64_t offset, int64_t arrSize, bool ascending) const
{
    using StringVec = Vector<LargeStringContainer<std::string_view>>;
    auto *srcVec = dynamic_cast<StringVec *>(srcElem);
    auto *dstVec = dynamic_cast<StringVec *>(dstElem);

    std::vector<size_t> indices(static_cast<size_t>(arrSize));
    for (size_t i = 0; i < static_cast<size_t>(arrSize); ++i) {
        indices[i] = i;
    }

    bool nullsFirst = ascending;
    std::sort(indices.begin(), indices.end(),
        [&](size_t a, size_t b) -> bool {
            int32_t idxA = static_cast<int32_t>(offset + static_cast<int64_t>(a));
            int32_t idxB = static_cast<int32_t>(offset + static_cast<int64_t>(b));
            bool aIsNull = srcElem->IsNull(idxA);
            bool bIsNull = srcElem->IsNull(idxB);

            if (aIsNull && bIsNull) {
                return false;
            }
            if (aIsNull) {
                return nullsFirst;
            }
            if (bIsNull) {
                return !nullsFirst;
            }

            std::string_view valA = srcVec->GetValue(idxA);
            std::string_view valB = srcVec->GetValue(idxB);

            if (ascending) {
                return valA < valB;
            } else {
                return valA > valB;
            }
        });

    for (size_t i = 0; i < static_cast<size_t>(arrSize); ++i) {
        int32_t srcIdx = static_cast<int32_t>(offset + static_cast<int64_t>(indices[i]));
        int32_t dstIdx = static_cast<int32_t>(i);
        if (srcElem->IsNull(srcIdx)) {
            dstElem->SetNull(dstIdx);
        } else {
            dstElem->SetNotNull(dstIdx);
            dstVec->SetValue(dstIdx, srcVec->GetValue(srcIdx));
        }
    }
}

void SortArrayFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    bool ascending = true;
    BaseVector *ascArg = nullptr;

    if (args.size() >= 2) {
        ascArg = args.top();
        args.pop();
        if (ascArg != nullptr && !ascArg->IsNull(0)) {
            ascending = GetAscendingFlag(ascArg);
        }
    }

    auto *arrayArg = args.top();
    args.pop();

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVec == nullptr) {
        delete ascArg;
        delete arrayArg;
        OMNI_THROW("SortArrayFunction error:", "First argument must be ARRAY type");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto inputElementVector = arrayVec->GetElementVector();

    int64_t totalElements = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!arrayVec->IsNull(row)) {
            totalElements += arrayVec->GetSize(row);
        }
    }

    DataTypeId elemTypeId = OMNI_INT;
    if (inputElementVector != nullptr && totalElements > 0) {
        elemTypeId = inputElementVector->GetTypeId();
    } else if (inputElementVector != nullptr) {
        elemTypeId = inputElementVector->GetTypeId();
    }

    auto *resultArray = new ArrayVector(rowSize, context->GetPool());

    if (totalElements == 0) {
        int64_t currentOffset = 0;
        for (int32_t row = 0; row < rowSize; ++row) {
            resultArray->SetOffset(row, static_cast<int32_t>(currentOffset));
            if (arrayVec->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetNotNull(row);
            }
        }
        resultArray->SetOffset(rowSize, static_cast<int32_t>(currentOffset));
        BaseVector *emptyElem = CreateElementVectorByType(elemTypeId, 0, context->GetPool());
        resultArray->SetElementVector(std::shared_ptr<BaseVector>(emptyElem));
        result = resultArray;
        delete ascArg;
        delete arrayArg;
        return;
    }

    BaseVector *newElementVector = CreateElementVectorByType(elemTypeId, totalElements, context->GetPool());

    int64_t currentOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, static_cast<int32_t>(currentOffset));

        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            continue;
        }
        resultArray->SetNotNull(row);

        int64_t arrSize = arrayVec->GetSize(row);
        int64_t arrOffset = arrayVec->GetOffset(row);

        if (arrSize == 0) {
            continue;
        }

        BaseVector *tempSortedElem = CreateElementVectorByType(elemTypeId, arrSize, context->GetPool());

        switch (elemTypeId) {
            case OMNI_BYTE:
                SortTypedElements<int8_t>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_SHORT:
                SortTypedElements<int16_t>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                SortTypedElements<int32_t>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                SortTypedElements<int64_t>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_FLOAT:
                SortTypedElements<float>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_DOUBLE:
                SortTypedElements<double>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_BOOLEAN:
                SortTypedElements<bool>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_DECIMAL128:
                SortTypedElements<Decimal128>(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY:
                SortStringElements(inputElementVector.get(), tempSortedElem,
                    arrOffset, arrSize, ascending);
                break;
            default:
                delete tempSortedElem;
                delete newElementVector;
                delete resultArray;
                delete ascArg;
                delete arrayArg;
                OMNI_THROW("SortArrayFunction error:",
                    "Unsupported element type: " + std::to_string(elemTypeId));
        }

        for (int64_t i = 0; i < arrSize; ++i) {
            CopyElementValue(tempSortedElem, static_cast<int32_t>(i),
                newElementVector, static_cast<int32_t>(currentOffset + i));
        }

        delete tempSortedElem;
        currentOffset += arrSize;
    }
    resultArray->SetOffset(rowSize, static_cast<int32_t>(currentOffset));

    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
    result = resultArray;

    delete ascArg;
    delete arrayArg;
}

} // namespace omniruntime::vectorization
