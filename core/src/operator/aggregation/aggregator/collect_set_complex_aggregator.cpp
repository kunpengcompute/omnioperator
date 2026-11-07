// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// Description: collect_set for complex types (ARRAY, ROW only; Map not supported).

#include "collect_set_complex_aggregator.h"
#include "operator/util/function_type.h"
#include "type/data_type.h"
#include "vector/row_vector.h"
#include "memory/thread_memory_manager.h"

namespace omniruntime {
namespace op {

using namespace omniruntime::vec;
using namespace omniruntime::type;

static type::DataType *GetLeafElementTypeFromOutput(const type::DataTypePtr &outputType)
{
    if (outputType == nullptr) {
        return nullptr;
    }
    type::DataTypePtr current = outputType;
    while (current != nullptr && current->GetId() == OMNI_ARRAY) {
        current = current->asArray().ElementType();
    }
    return current != nullptr ? current.get() : nullptr;
}

CollectSetComplexAggregator::~CollectSetComplexAggregator()
{
    for (int64_t addr : allocatedSetAddrs_) {
        ComplexSetType *set = reinterpret_cast<ComplexSetType *>(addr);
        if (set != nullptr) {
            for (BaseVector *p : *set) {
                if (p != nullptr) {
                    ReleaseComplexSliceCopy(p, targetColTypeId_);
                }
            }
            delete set;
        }
    }
    allocatedSetAddrs_.clear();
}

std::unique_ptr<Aggregator> CollectSetComplexAggregator::Create(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull,
    type::DataTypeId targetColTypeId)
{
    if (targetColTypeId == type::OMNI_MAP) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "CollectSet does not support Map type");
    }
    if (targetColTypeId != type::OMNI_ARRAY && targetColTypeId != type::OMNI_ROW) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "CollectSetComplexAggregator: target column must be Array or Row type, got " +
                std::to_string(static_cast<int>(targetColTypeId)));
    }
    type::DataTypePtr outputType = outputTypes.GetSize() >= 1 ? outputTypes.GetType(0) : nullptr;
    if (outputType == nullptr || outputType->GetId() != OMNI_ARRAY) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "CollectSetComplexAggregator: output must be Array type");
    }
    type::DataTypePtr targetColDataType = outputType->asArray().ElementType();
    return std::unique_ptr<Aggregator>(new CollectSetComplexAggregator(inputTypes, outputTypes, channels,
        rawIn, partialOut, isOverflowAsNull, targetColTypeId, targetColDataType));
}

CollectSetComplexAggregator::CollectSetComplexAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
    type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_COLLECT_SET, inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull)
    , targetColTypeId_(targetColTypeId)
    , targetColDataType_(std::move(targetColDataType))
{
}

void CollectSetComplexAggregator::InitState(AggregateState *state)
{
    ComplexSetState *setState = ComplexSetState::CastState(state + aggStateOffset);
    setState->setAddr = reinterpret_cast<int64_t>(new ComplexSetType(
        ComplexSliceSetComparator(targetColTypeId_, targetColDataType_.get())));
    allocatedSetAddrs_.insert(setState->setAddr);
}

void CollectSetComplexAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (AggregateState *s : groupStates) {
        InitState(s);
    }
}

void CollectSetComplexAggregator::DestroyState(AggregateState *state)
{
    ComplexSetState *setState = ComplexSetState::CastState(state + aggStateOffset);
    int64_t addr = setState->setAddr;
    if (addr == 0) {
        return;
    }
    ComplexSetType *set = reinterpret_cast<ComplexSetType *>(addr);
    for (BaseVector *p : *set) {
        if (p != nullptr) {
            ReleaseComplexSliceCopy(p, targetColTypeId_);
        }
    }
    delete set;
    setState->setAddr = 0;
    // Hash-set erase by key: O(1) average, keeps bulk destruction linear in the
    // number of groups (std::find + erase(it) would be O(n) per state).
    allocatedSetAddrs_.erase(addr);
}

void CollectSetComplexAggregator::DestroyAllStates()
{
    for (int64_t addr : allocatedSetAddrs_) {
        ComplexSetType *set = reinterpret_cast<ComplexSetType *>(addr);
        if (set != nullptr) {
            for (BaseVector *p : *set) {
                if (p != nullptr) {
                    ReleaseComplexSliceCopy(p, targetColTypeId_);
                }
            }
            delete set;
        }
    }
    allocatedSetAddrs_.clear();
}

// Add slice to std::set (deduplication via set ordering). Always copy and insert;
// avoid set->find(slice) because slice is a temporary view that may not compare correctly
// with owned elements (e.g. different encodings, slice vs owned layout).
static void AddComplexSliceToSet(ComplexSetType *set, BaseVector *slice, type::DataTypeId colTypeId,
    type::DataType *colDataType)
{
    if (IsComplexSliceNull(slice, colTypeId)) {
        return;
    }
    BaseVector *owned = CopyComplexSliceToOwned(slice, colTypeId, colDataType);
    if (owned != nullptr) {
        auto result = set->insert(owned);
        if (!result.second) {
            ReleaseComplexSliceCopy(owned, colTypeId);
        }
    }
}

// Merge array-of-complex row into set with deduplication. Only OMNI_ARRAY and OMNI_ROW (no Map).
// For array<struct>: GetArrayAt returns RowVector (element vector), not ArrayVector. Use row iteration.
// For array<array<...>>: GetArrayAt returns ArrayVector slice. Use array iteration.
static void MergeArrayRowIntoSet(ComplexSetType *set, BaseVector *rowSlice, type::DataTypeId colTypeId,
    type::DataType *colDataType)
{
    if (rowSlice == nullptr || IsComplexSliceNull(rowSlice, colTypeId)) {
        return;
    }
    int32_t n = 0;
    type::DataTypeId elemTypeId = colDataType != nullptr ? colDataType->GetId() : type::OMNI_INT;
    if (colTypeId == OMNI_ARRAY && elemTypeId == OMNI_ROW) {
        // array<struct>: rowSlice is RowVector from GetArrayAt, iterate over struct rows
        auto *rowVec = static_cast<RowVector *>(rowSlice);
        n = rowVec->GetSize();
        for (int32_t j = 0; j < n; j++) {
            BaseVector *elem = rowVec->Slice(j, 1);
            AddComplexSliceToSet(set, elem, OMNI_ROW, colDataType);
            delete elem;
        }
        return;
    }
    switch (colTypeId) {
        case OMNI_ARRAY: {
            auto *arr = static_cast<ArrayVector *>(rowSlice);
            n = static_cast<int32_t>(arr->GetSize());
            for (int32_t j = 0; j < n; j++) {
                BaseVector *elem = arr->GetValue(j);
                AddComplexSliceToSet(set, elem, colTypeId, colDataType);
                delete elem;
            }
            break;
        }
        case OMNI_ROW: {
            auto *rowVec = static_cast<RowVector *>(rowSlice);
            n = rowVec->GetSize();
            for (int32_t j = 0; j < n; j++) {
                BaseVector *elem = rowVec->Slice(j, 1);
                AddComplexSliceToSet(set, elem, colTypeId, colDataType);
                delete elem;
            }
            break;
        }
        default:
            break;
    }
}

void CollectSetComplexAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector == nullptr || rowCount <= 0) {
        return;
    }
    ComplexSetState *setState = ComplexSetState::CastState(state + aggStateOffset);
    ComplexSetType *set = reinterpret_cast<ComplexSetType *>(setState->setAddr);
    if (set == nullptr) {
        return;
    }
    BaseVector *colVector = vector;
    type::DataType *colDataType = targetColDataType_.get();

    if (IsInputRaw()) {
        for (int32_t i = 0; i < rowCount; i++) {
            int32_t rowIndex = rowOffset + i;
            if (nullMap != nullptr && (*nullMap)[i]) {
                continue;
            }
            BaseVector *slice = GetComplexColSlice(colVector, targetColTypeId_, rowIndex);
            AddComplexSliceToSet(set, slice, targetColTypeId_, colDataType);
            if (slice != nullptr) {
                ReleaseComplexSliceCopy(slice, targetColTypeId_);
            }
        }
    } else {
        auto *arrayVector = static_cast<ArrayVector *>(colVector);
        for (int32_t i = 0; i < rowCount; i++) {
            int32_t rowIndex = rowOffset + i;
            if (nullMap != nullptr && (*nullMap)[i]) {
                continue;
            }
            if (arrayVector->IsNull(rowIndex)) {
                continue;
            }
            std::shared_ptr<BaseVector> rowSlice = arrayVector->GetArrayAt(rowIndex, false);
            if (rowSlice != nullptr) {
                MergeArrayRowIntoSet(set, rowSlice.get(), targetColTypeId_, colDataType);
            }
        }
    }
}

void CollectSetComplexAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
    BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector == nullptr || rowStates.empty()) {
        return;
    }
    BaseVector *colVector = vector;
    type::DataType *colDataType = targetColDataType_.get();
    const size_t rowCount = rowStates.size();

    if (IsInputRaw()) {
        for (size_t i = 0; i < rowCount; i++) {
            int32_t rowIdx = static_cast<int32_t>(rowOffset + i);
            if (nullMap != nullptr && (*nullMap)[i]) {
                continue;
            }
            ComplexSetState *setState = ComplexSetState::CastState(rowStates[i] + aggStateOffset);
            ComplexSetType *set = reinterpret_cast<ComplexSetType *>(setState->setAddr);
            if (set == nullptr) {
                continue;
            }
            BaseVector *slice = GetComplexColSlice(colVector, targetColTypeId_, rowIdx);
            AddComplexSliceToSet(set, slice, targetColTypeId_, colDataType);
            if (slice != nullptr) {
                ReleaseComplexSliceCopy(slice, targetColTypeId_);
            }
        }
    } else {
        auto *arrayVector = static_cast<ArrayVector *>(colVector);
        for (size_t i = 0; i < rowCount; i++) {
            int32_t rowIdx = static_cast<int32_t>(rowOffset + i);
            if (nullMap != nullptr && (*nullMap)[i]) {
                continue;
            }
            if (arrayVector->IsNull(rowIdx)) {
                continue;
            }
            ComplexSetState *setState = ComplexSetState::CastState(rowStates[i] + aggStateOffset);
            ComplexSetType *set = reinterpret_cast<ComplexSetType *>(setState->setAddr);
            if (set == nullptr) {
                continue;
            }
            std::shared_ptr<BaseVector> rowSlice = arrayVector->GetArrayAt(rowIdx, false);
            if (rowSlice != nullptr) {
                MergeArrayRowIntoSet(set, rowSlice.get(), targetColTypeId_, colDataType);
            }
        }
    }
}

void CollectSetComplexAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    BaseVector *v = vectors[0];
    const ComplexSetState *setState = ComplexSetState::ConstCastState(state + aggStateOffset);
    const ComplexSetType *set = reinterpret_cast<const ComplexSetType *>(setState->setAddr);
    if (set == nullptr || set->empty()) {
        // Spark: empty set is [] not null
        type::DataTypePtr outputType = GetOutputTypes().GetType(0);
        type::DataType *elemType = GetLeafElementTypeFromOutput(outputType);
        if (elemType == nullptr) {
            v->SetNull(rowIndex);
            return;
        }
        BaseVector *elemVec = VectorHelper::CreateComplexVector(elemType, 0);
        type::DataType *outputElemType = (outputType->GetId() == OMNI_ARRAY)
            ? outputType->asArray().ElementType().get() : nullptr;
        if (outputElemType != nullptr && outputElemType->GetId() == OMNI_ARRAY) {
            ArrayVector *innerArr = new ArrayVector(static_cast<int64_t>(0), std::shared_ptr<BaseVector>(elemVec));
            static_cast<ArrayVector *>(v)->SetValue(rowIndex, innerArr);
            delete innerArr;
        } else {
            static_cast<ArrayVector *>(v)->SetValue(rowIndex, elemVec);
            delete elemVec;
        }
        return;
    }
    type::DataTypePtr outputType = GetOutputTypes().GetType(0);
    type::DataType *elemType = GetLeafElementTypeFromOutput(outputType);
    if (elemType == nullptr) {
        v->SetNull(rowIndex);
        return;
    }
    int32_t totalElem = 0;
    for (BaseVector *s : *set) {
        totalElem += static_cast<int32_t>(s->GetSize());
    }
    BaseVector *elemVec = VectorHelper::CreateComplexVector(elemType, totalElem);
    int32_t offset = 0;
    size_t idx = 0;
    // For struct (OMNI_ROW), use CopyValue per row to avoid RowVector::Append issues with rawChildren_ layout.
    if (elemType->GetId() == OMNI_ROW) {
        for (BaseVector *s : *set) {
            int32_t sz = static_cast<int32_t>(s->GetSize());
            for (int32_t i = 0; i < sz; i++) {
                VectorHelper::CopyValue(s, i, elemVec, offset + i);
            }
            offset += sz;
            idx++;
        }
    } else {
        for (BaseVector *s : *set) {
            int32_t sz = static_cast<int32_t>(s->GetSize());
            if (sz > 0) {
                VectorHelper::AppendVector(elemVec, offset, s, sz);
                offset += sz;
            }
            idx++;
        }
    }
    type::DataType *outputElemType = (outputType->GetId() == OMNI_ARRAY)
        ? outputType->asArray().ElementType().get() : nullptr;
    if (outputElemType != nullptr && outputElemType->GetId() == OMNI_ARRAY) {
        // Array<Array<T>>: output element type is Array; SetValue expects ArrayVector.
        ArrayVector *innerArr = new ArrayVector(static_cast<int64_t>(set->size()), std::shared_ptr<BaseVector>(elemVec));
        idx = 0;
        for (BaseVector *s : *set) {
            int32_t sz = static_cast<int32_t>(s->GetSize());
            innerArr->SetSize(static_cast<int32_t>(idx), sz);
            innerArr->SetNotNull(static_cast<int32_t>(idx));
            idx++;
        }
        static_cast<ArrayVector *>(v)->SetValue(rowIndex, innerArr);
        delete innerArr;
    } else {
        // Array<struct> or Array<Map>: pass elemVec (RowVector/MapVector) directly.
        static_cast<ArrayVector *>(v)->SetValue(rowIndex, elemVec);
        delete elemVec;
    }
}

void CollectSetComplexAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        ExtractValues(groupStates[rowIndex], vectors, rowOffset + rowIndex);
    }
}

std::vector<DataTypePtr> CollectSetComplexAggregator::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.push_back(GetOutputTypes().GetType(0));
    return spillTypes;
}

void CollectSetComplexAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    ExtractValuesBatch(groupStates, vectors, 0, static_cast<int32_t>(groupStates.size()));
}

void CollectSetComplexAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
    int32_t rowCount, int32_t &vectorIndex)
{
    int32_t arrayVecIdx = vectorIndex++;
    type::DataType *colDataType = targetColDataType_.get();
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        UnspillRowInfo &row = unspillRows[rowIdx];
        if (row.batch == nullptr || arrayVecIdx >= row.batch->GetVectorCount()) {
            continue;
        }
        BaseVector *arrayVec = row.batch->Get(arrayVecIdx);
        if (arrayVec == nullptr || arrayVec->IsNull(row.rowIdx)) {
            continue;
        }
        ArrayVector *arrVec = static_cast<ArrayVector *>(arrayVec);
        std::shared_ptr<BaseVector> rowSlice = arrVec->GetArrayAt(row.rowIdx, false);
        if (rowSlice == nullptr) {
            continue;
        }
        ComplexSetState *setState = ComplexSetState::CastState(row.state + aggStateOffset);
        ComplexSetType *set = reinterpret_cast<ComplexSetType *>(setState->setAddr);
        if (set == nullptr) {
            continue;
        }
        MergeArrayRowIntoSet(set, rowSlice.get(), targetColTypeId_, colDataType);
    }
}

void CollectSetComplexAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool /* aggFilter */)
{
    const int32_t rowCount = (originVector != nullptr) ? originVector->GetSize() : 0;
    if (rowCount == 0) {
        BaseVector *emptyArr = VectorHelper::CreateComplexVector(GetOutputTypes().GetType(0).get(), 0);
        result->Append(emptyArr);
        return;
    }
    if (!IsInputRaw()) {
        BaseVector *sliced = VectorHelper::SliceVector(originVector, 0, rowCount);
        result->Append(sliced);
        return;
    }
    type::DataTypePtr outputType = GetOutputTypes().GetType(0);
    ArrayVector *arrayVector = static_cast<ArrayVector *>(
        VectorHelper::CreateComplexVector(outputType.get(), rowCount));
    for (int32_t i = 0; i < rowCount; i++) {
        if ((nullMap != nullptr && (*nullMap)[i]) || originVector->IsNull(i)) {
            arrayVector->SetNull(i);
            arrayVector->SetSize(i, 0);
            continue;
        }
        BaseVector *slice = GetComplexColSlice(originVector, targetColTypeId_, i);
        if (IsComplexSliceNull(slice, targetColTypeId_)) {
            arrayVector->SetNull(i);
            arrayVector->SetSize(i, 0);
        } else {
            BaseVector *owned = CopyComplexSliceToOwned(slice, targetColTypeId_, targetColDataType_.get());
            if (owned != nullptr) {
                type::DataType *elemType = outputType->GetId() == OMNI_ARRAY
                    ? outputType->asArray().ElementType().get() : nullptr;
                if (elemType != nullptr && elemType->GetId() == OMNI_ARRAY) {
                    ArrayVector *wrapped = new ArrayVector(1, std::shared_ptr<BaseVector>(owned));
                    wrapped->SetSize(0, static_cast<int32_t>(owned->GetSize()));
                    wrapped->SetNotNull(0);
                    arrayVector->SetValue(i, wrapped);
                    delete wrapped;
                } else {
                    arrayVector->SetValue(i, owned);
                    delete owned;
                }
            } else {
                arrayVector->SetNull(i);
                arrayVector->SetSize(i, 0);
            }
        }
        if (slice != nullptr) {
            ReleaseComplexSliceCopy(slice, targetColTypeId_);
        }
    }
    result->Append(arrayVector);
}

}  // namespace op
}  // namespace omniruntime
