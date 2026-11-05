// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// Description: collect_list for complex types (ARRAY, MAP, ROW); spill and ProcessAlignAggSchema.

#include "collect_list_complex_aggregator.h"
#include "operator/util/function_type.h"
#include "type/data_type.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "memory/thread_memory_manager.h"

namespace omniruntime {
namespace op {

using namespace omniruntime::vec;
using namespace omniruntime::type;

// For Array<ComplexType> (e.g. Array<Array<int>>), return the leaf element type (e.g. int) for building the inner element vector.
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

CollectListComplexAggregator::~CollectListComplexAggregator()
{
    for (int64_t addr : allocatedListAddrs_) {
        ComplexListType *list = reinterpret_cast<ComplexListType *>(addr);
        if (list != nullptr) {
            for (BaseVector *p : *list) {
                if (p != nullptr) {
                    ReleaseComplexSliceCopy(p, targetColTypeId_);
                }
            }
            delete list;
        }
    }
    allocatedListAddrs_.clear();
}

std::unique_ptr<Aggregator> CollectListComplexAggregator::Create(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull,
    type::DataTypeId targetColTypeId)
{
    type::DataTypePtr outputType = outputTypes.GetSize() >= 1 ? outputTypes.GetType(0) : nullptr;
    if (outputType == nullptr || outputType->GetId() != OMNI_ARRAY) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "CollectListComplexAggregator: output must be Array type");
    }
    type::DataTypePtr targetColDataType = outputType->asArray().ElementType();
    return std::unique_ptr<Aggregator>(new CollectListComplexAggregator(inputTypes, outputTypes, channels,
        rawIn, partialOut, isOverflowAsNull, targetColTypeId, targetColDataType));
}

CollectListComplexAggregator::CollectListComplexAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
    type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_COLLECT_LIST, inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull)
    , targetColTypeId_(targetColTypeId)
    , targetColDataType_(std::move(targetColDataType))
{
}

void CollectListComplexAggregator::InitState(AggregateState *state)
{
    ComplexListState *listState = ComplexListState::CastState(state + aggStateOffset);
    listState->listAddr = reinterpret_cast<int64_t>(new ComplexListType());
    allocatedListAddrs_.insert(listState->listAddr);
}

void CollectListComplexAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (AggregateState *s : groupStates) {
        InitState(s);
    }
}

void CollectListComplexAggregator::DestroyState(AggregateState *state)
{
    ComplexListState *listState = ComplexListState::CastState(state + aggStateOffset);
    int64_t addr = listState->listAddr;
    if (addr == 0) {
        return;
    }
    ComplexListType *list = reinterpret_cast<ComplexListType *>(addr);
    for (BaseVector *p : *list) {
        if (p != nullptr) {
            ReleaseComplexSliceCopy(p, targetColTypeId_);
        }
    }
    delete list;
    listState->listAddr = 0;
    allocatedListAddrs_.erase(addr);
}

void CollectListComplexAggregator::DestroyAllStates()
{
    for (int64_t addr : allocatedListAddrs_) {
        ComplexListType *list = reinterpret_cast<ComplexListType *>(addr);
        if (list != nullptr) {
            for (BaseVector *p : *list) {
                if (p != nullptr) {
                    ReleaseComplexSliceCopy(p, targetColTypeId_);
                }
            }
            delete list;
        }
    }
    allocatedListAddrs_.clear();
}

// Append one complex slice (from input row) to the group's list.
static void AppendComplexSliceToList(ComplexListType *list, BaseVector *slice, type::DataTypeId colTypeId,
    type::DataType *colDataType)
{
    if (IsComplexSliceNull(slice, colTypeId)) {
        return;
    }
    BaseVector *owned = CopyComplexSliceToOwned(slice, colTypeId, colDataType);
    if (owned != nullptr) {
        list->push_back(owned);
    }
}

// Iterate over elements of an array-of-complex row (final merge): for each element get slice and append to list.
static void MergeArrayRowIntoList(ComplexListType *list, BaseVector *rowSlice, type::DataTypeId colTypeId,
    type::DataType *colDataType)
{
    if (rowSlice == nullptr || IsComplexSliceNull(rowSlice, colTypeId)) {
        return;
    }
    int32_t n = 0;
    switch (colTypeId) {
        case OMNI_ARRAY: {
            auto *arr = static_cast<ArrayVector *>(rowSlice);
            n = static_cast<int32_t>(arr->GetSize());
            for (int32_t j = 0; j < n; j++) {
                BaseVector *elem = arr->GetValue(j);
                AppendComplexSliceToList(list, elem, colTypeId, colDataType);
            }
            break;
        }
        case OMNI_MAP: {
            auto *mapVec = static_cast<MapVector *>(rowSlice);
            n = static_cast<int32_t>(mapVec->GetSize());
            for (int32_t j = 0; j < n; j++) {
                BaseVector *elem = mapVec->Slice(j, 1);
                AppendComplexSliceToList(list, elem, colTypeId, colDataType);
                delete elem;
            }
            break;
        }
        case OMNI_ROW: {
            auto *rowVec = static_cast<RowVector *>(rowSlice);
            n = rowVec->GetSize();
            for (int32_t j = 0; j < n; j++) {
                BaseVector *elem = rowVec->Slice(j, 1);
                AppendComplexSliceToList(list, elem, colTypeId, colDataType);
                delete elem;
            }
            break;
        }
        default:
            break;
    }
}

void CollectListComplexAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector == nullptr || rowCount <= 0) {
        return;
    }
    ComplexListState *listState = ComplexListState::CastState(state);
    ComplexListType *list = reinterpret_cast<ComplexListType *>(listState->listAddr);
    if (list == nullptr) {
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
            AppendComplexSliceToList(list, slice, targetColTypeId_, colDataType);
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
                MergeArrayRowIntoList(list, rowSlice.get(), targetColTypeId_, colDataType);
            }
        }
    }
}

void CollectListComplexAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
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
            ComplexListState *listState = ComplexListState::CastState(rowStates[i] + aggStateOffset);
            ComplexListType *list = reinterpret_cast<ComplexListType *>(listState->listAddr);
            if (list == nullptr) {
                continue;
            }
            BaseVector *slice = GetComplexColSlice(colVector, targetColTypeId_, rowIdx);
            AppendComplexSliceToList(list, slice, targetColTypeId_, colDataType);
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
            ComplexListState *listState = ComplexListState::CastState(rowStates[i] + aggStateOffset);
            ComplexListType *list = reinterpret_cast<ComplexListType *>(listState->listAddr);
            if (list == nullptr) {
                continue;
            }
            std::shared_ptr<BaseVector> rowSlice = arrayVector->GetArrayAt(rowIdx, false);
            if (rowSlice != nullptr) {
                MergeArrayRowIntoList(list, rowSlice.get(), targetColTypeId_, colDataType);
            }
        }
    }
}

// Build one output row (array of complex values) from list and write to vectors[0] at rowIndex.
void CollectListComplexAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    BaseVector *v = vectors[0];
    const ComplexListState *listState = ComplexListState::ConstCastState(state + aggStateOffset);
    const ComplexListType *list = reinterpret_cast<const ComplexListType *>(listState->listAddr);
    if (list == nullptr || list->empty()) {
        type::DataTypePtr outputType = GetOutputTypes().GetType(0);
        type::DataType *elemType = GetLeafElementTypeFromOutput(outputType);
        if (elemType == nullptr) {
            v->SetNull(rowIndex);
            return;
        }
        BaseVector *elemVec = VectorHelper::CreateComplexVector(elemType, 0);
        if (targetColTypeId_ == OMNI_ARRAY) {
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
    for (BaseVector *s : *list) {
        totalElem += static_cast<int32_t>(s->GetSize());
    }
    BaseVector *elemVec = VectorHelper::CreateComplexVector(elemType, totalElem);
    int32_t offset = 0;
    for (BaseVector *s : *list) {
        int32_t sz = static_cast<int32_t>(s->GetSize());
        if (sz > 0) {
            VectorHelper::AppendVector(elemVec, offset, s, sz);
            offset += sz;
        }
    }
    if (targetColTypeId_ == OMNI_ARRAY) {
        // Array<Array<T>>: output element type is Array; SetValue expects ArrayVector.
        ArrayVector *innerArr = new ArrayVector(static_cast<int64_t>(list->size()), std::shared_ptr<BaseVector>(elemVec));
        for (size_t i = 0; i < list->size(); i++) {
            int32_t sz = static_cast<int32_t>((*list)[i]->GetSize());
            innerArr->SetSize(static_cast<int32_t>(i), sz);
            innerArr->SetNotNull(static_cast<int32_t>(i));
        }
        static_cast<ArrayVector *>(v)->SetValue(rowIndex, innerArr);
        delete innerArr;
    } else {
        // Array<Map<K,V>> or Array<Row>: output element type is Map/Row; SetValue expects MapVector/RowVector.
        static_cast<ArrayVector *>(v)->SetValue(rowIndex, elemVec);
        delete elemVec;
    }
}

void CollectListComplexAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        ExtractValues(groupStates[rowIndex], vectors, rowOffset + rowIndex);
    }
}

std::vector<DataTypePtr> CollectListComplexAggregator::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.push_back(GetOutputTypes().GetType(0));
    return spillTypes;
}

void CollectListComplexAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    ExtractValuesBatch(groupStates, vectors, 0, static_cast<int32_t>(groupStates.size()));
}

void CollectListComplexAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
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
        ComplexListState *listState = ComplexListState::CastState(row.state + aggStateOffset);
        ComplexListType *list = reinterpret_cast<ComplexListType *>(listState->listAddr);
        if (list == nullptr) {
            continue;
        }
        MergeArrayRowIntoList(list, rowSlice.get(), targetColTypeId_, colDataType);
    }
}

void CollectListComplexAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool /* aggFilter */)
{
    const int32_t rowCount = (originVector != nullptr) ? originVector->GetSize() : 0;
    if (rowCount == 0) {
        BaseVector *emptyArr = VectorHelper::CreateComplexVector(GetOutputTypes().GetType(0).get(), 0);
        result->Append(emptyArr);  // batch takes ownership; do not delete
        return;
    }
    if (!IsInputRaw()) {
        BaseVector *sliced = VectorHelper::SliceVector(originVector, 0, rowCount);
        result->Append(sliced);  // batch takes ownership; do not delete
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
                // ArrayVector::SetValue expects ArrayVector source for Array<Array<T>>; wrap element slice.
                ArrayVector *wrapped = new ArrayVector(1, std::shared_ptr<BaseVector>(owned));
                wrapped->SetSize(0, static_cast<int32_t>(owned->GetSize()));
                wrapped->SetNotNull(0);
                arrayVector->SetValue(i, wrapped);
                delete wrapped;
            } else {
                arrayVector->SetNull(i);
                arrayVector->SetSize(i, 0);
            }
        }
        if (slice != nullptr) {
            ReleaseComplexSliceCopy(slice, targetColTypeId_);
        }
    }
    result->Append(arrayVector);  // batch takes ownership; do not delete
}

}  // namespace op
}  // namespace omniruntime
