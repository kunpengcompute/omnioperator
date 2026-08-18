/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectSet aggregation for VARCHAR/CHAR/VARBINARY.
 */

#include "collect_set_varchar_aggregator.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/nulls_buffer.h"
#include <algorithm>

namespace omniruntime::op {

static std::shared_ptr<NullsHelper> SliceNullMapForSingleRow(const std::shared_ptr<NullsHelper> &nullMap,
    int32_t rowIdx)
{
    if (nullMap == nullptr) {
        return nullptr;
    }
    auto nullsBuffer = std::make_shared<vec::NullsBuffer>(1);
    auto rowNullMap = std::make_shared<NullsHelper>(nullsBuffer);
    if ((*nullMap)[rowIdx]) {
        rowNullMap->SetNull(0, true);
    }
    return rowNullMap;
}

#pragma pack(push, 1)
struct CollectSetVarcharState {
    int64_t uniqueValuesAddr;
};
#pragma pack(pop)

static CollectSetVarcharState *CastState(AggregateState *state) {
    return reinterpret_cast<CollectSetVarcharState *>(state);
}
static const CollectSetVarcharState *ConstCastState(const AggregateState *state) {
    return reinterpret_cast<const CollectSetVarcharState *>(state);
}

CollectSetVarcharAggregator::CollectSetVarcharAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_COLLECT_SET, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
{
    auto outType = outputTypes.GetType(0);
    if (outType->GetId() == type::OMNI_ARRAY) {
        elementTypeId_ = outType->asArray().ElementType()->GetId();
    } else {
        elementTypeId_ = type::OMNI_VARCHAR;
    }
}

CollectSetVarcharAggregator::~CollectSetVarcharAggregator() {
    for (int64_t addr : allocatedUniqueValuesAddrs_) {
        delete reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(addr);
    }
    allocatedUniqueValuesAddrs_.clear();
}

void CollectSetVarcharAggregator::InitState(AggregateState *state) {
    CollectSetVarcharState *s = CastState(state + aggStateOffset);
    auto *uniqueValues = new DefaultHashMap<std::string, int8_t>();
    uniqueValues->Reset();
    s->uniqueValuesAddr = reinterpret_cast<int64_t>(uniqueValues);
    allocatedUniqueValuesAddrs_.push_back(s->uniqueValuesAddr);
}

void CollectSetVarcharAggregator::InitStates(std::vector<AggregateState *> &groupStates) {
    for (auto *st : groupStates) {
        InitState(st);
    }
}

void CollectSetVarcharAggregator::DestroyState(AggregateState *state) {
    CollectSetVarcharState *s = CastState(state + aggStateOffset);
    int64_t addr = s->uniqueValuesAddr;
    if (addr == 0) return;
    delete reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(addr);
    s->uniqueValuesAddr = 0;
    auto it = std::find(allocatedUniqueValuesAddrs_.begin(), allocatedUniqueValuesAddrs_.end(), addr);
    if (it != allocatedUniqueValuesAddrs_.end()) {
        allocatedUniqueValuesAddrs_.erase(it);
    }
}

void CollectSetVarcharAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) {
    auto *v = static_cast<ArrayVector *>(vectors[0]);
    const CollectSetVarcharState *s = ConstCastState(state + aggStateOffset);
    auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(s->uniqueValuesAddr);
    if (uniqueValues->GetElementsSize() == 0) {
        auto *elementVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
            VectorHelper::CreateVector(OMNI_FLAT, elementTypeId_, 0));
        v->SetValue(rowIndex, elementVector);
        delete elementVector;
        return;
    }
    int32_t elementSize = static_cast<int32_t>(uniqueValues->GetElementsSize());
    auto *elementVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, elementTypeId_, elementSize));
    int32_t index = 0;
    uniqueValues->ForEachKV([&](const std::string &key, const int8_t &) {
        elementVector->SetValue(index, std::string_view(key));
        index++;
    });
    v->SetValue(rowIndex, elementVector);
    delete elementVector;
}

void CollectSetVarcharAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) {
    auto *v = static_cast<ArrayVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        const CollectSetVarcharState *s = ConstCastState(groupStates[rowIndex] + aggStateOffset);
        auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(s->uniqueValuesAddr);
        if (uniqueValues->GetElementsSize() == 0) {
            auto *elementVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
                VectorHelper::CreateVector(OMNI_FLAT, elementTypeId_, 0));
            v->SetValue(rowOffset + rowIndex, elementVector);
            delete elementVector;
            continue;
        }
        int32_t elementSize = static_cast<int32_t>(uniqueValues->GetElementsSize());
        auto *elementVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
            VectorHelper::CreateVector(OMNI_FLAT, elementTypeId_, elementSize));
        int32_t index = 0;
        uniqueValues->ForEachKV([&](const std::string &key, const int8_t &) {
            elementVector->SetValue(index, std::string_view(key));
            index++;
        });
        v->SetValue(rowOffset + rowIndex, elementVector);
        delete elementVector;
    }
}

std::vector<DataTypePtr> CollectSetVarcharAggregator::GetSpillType() {
    return { GetOutputTypes().GetType(0) };
}

void CollectSetVarcharAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) {
    ExtractValuesBatch(groupStates, vectors, 0, static_cast<int32_t>(groupStates.size()));
}

static void UpdatePartialStateVarchar(DefaultHashMap<std::string, int8_t> *uniqueValues, BaseVector *vector,
    int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap, bool isDictionary, int32_t baseRowIndex) {
    if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
        if (vector->IsNull(0)) {
            return;
        }
        std::string_view constVal = static_cast<vec::ConstVector<std::string_view> *>(vector)->GetConstValue();
        for (int32_t i = 0; i < rowCount; i++) {
            if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                !vector->IsNull(baseRowIndex + i)) {
                uniqueValues->Emplace(std::string(constVal));
            }
        }
    } else if (isDictionary) {
        auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector);
        for (int32_t i = 0; i < rowCount; i++) {
            if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                !vector->IsNull(baseRowIndex + i)) {
                std::string_view sv = dictVec->GetValue(i);
                uniqueValues->Emplace(std::string(sv));
            }
        }
    } else {
        auto *flatVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
        for (int32_t i = 0; i < rowCount; i++) {
            if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                !vector->IsNull(baseRowIndex + i)) {
                std::string_view sv = flatVec->GetValue(i);
                uniqueValues->Emplace(std::string(sv));
            }
        }
    }
}

static void UpdateFinalStateVarchar(DefaultHashMap<std::string, int8_t> *uniqueValues, ArrayVector *arrayVector,
    int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap, int32_t baseRowIndex) {
    for (int32_t i = 0; i < rowCount; i++) {
        if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
            !arrayVector->IsNull(baseRowIndex + i)) {
            std::shared_ptr<BaseVector> elemHolder = arrayVector->GetArrayAt(i, false);
            BaseVector *elemVec = elemHolder.get();
            if (elemVec == nullptr || elemVec->GetSize() == 0) continue;
            auto *strElemVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec);
            for (int32_t j = 0, n = elemVec->GetSize(); j < n; j++) {
                if (!elemVec->IsNull(j)) {
                    uniqueValues->Emplace(std::string(strElemVec->GetValue(j)));
                }
            }
        }
    }
}

void CollectSetVarcharAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    if (rowCount == 0) return;
    CollectSetVarcharState *s = CastState(state);
    auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(s->uniqueValuesAddr);
    bool isDictionary = vector->GetEncoding() == vec::OMNI_DICTIONARY;
    if (IsInputRaw()) {
        UpdatePartialStateVarchar(uniqueValues, vector, rowCount, nullMap, isDictionary, rowOffset);
    } else {
        UpdateFinalStateVarchar(uniqueValues, static_cast<ArrayVector *>(vector), rowCount, nullMap, rowOffset);
    }
}

void CollectSetVarcharAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) {
    size_t rowCount = rowStates.size();
    if (rowCount == 0) return;
    bool isDictionary = vector->GetEncoding() == vec::OMNI_DICTIONARY;
    int32_t offset = rowOffset;
    for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        CollectSetVarcharState *s = CastState(rowStates[rowIndex] + aggStateOffset);
        auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(s->uniqueValuesAddr);
        if (IsInputRaw()) {
            BaseVector *sliced = vector->Slice(offset, 1);
            auto rowNullMap = SliceNullMapForSingleRow(nullMap, offset);
            UpdatePartialStateVarchar(uniqueValues, sliced, 1, rowNullMap, isDictionary, 0);
            delete sliced;
        } else {
            auto *arrayVector = reinterpret_cast<ArrayVector *>(vector);
            BaseVector *sliced = arrayVector->Slice(offset, 1);
            auto rowNullMap = SliceNullMapForSingleRow(nullMap, offset);
            UpdateFinalStateVarchar(uniqueValues, static_cast<ArrayVector *>(sliced), 1, rowNullMap, 0);
            delete sliced;
        }
        ++offset;
    }
}

void CollectSetVarcharAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) {
    int32_t arrayVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        UnspillRowInfo &row = unspillRows[rowIdx];
        if (row.batch == nullptr || arrayVecIdx >= row.batch->GetVectorCount()) continue;
        BaseVector *arrayVec = row.batch->Get(arrayVecIdx);
        if (arrayVec == nullptr || arrayVec->IsNull(row.rowIdx)) continue;
        ArrayVector *arrVec = static_cast<ArrayVector *>(arrayVec);
        std::shared_ptr<BaseVector> elementVecHolder = arrVec->GetArrayAt(row.rowIdx, false);
        BaseVector *elementVec = elementVecHolder.get();
        if (elementVec == nullptr || elementVec->GetSize() == 0) continue;
        CollectSetVarcharState *s = CastState(row.state + aggStateOffset);
        auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(s->uniqueValuesAddr);
        auto *strElemVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(elementVec);
        for (int32_t j = 0, n = elementVec->GetSize(); j < n; j++) {
            if (!elementVec->IsNull(j)) {
                uniqueValues->Emplace(std::string(strElemVec->GetValue(j)));
            }
        }
    }
}

void CollectSetVarcharAggregator::ProcessAlignAggSchema(VectorBatch *vecBatch, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool /* aggFilter */) {
    const int32_t rowCount = (originVector != nullptr) ? originVector->GetSize() : 0;
    if (rowCount == 0) {
        auto *emptyArr = static_cast<ArrayVector *>(VectorHelper::CreateComplexVector(GetOutputTypes().GetType(0).get(), 0));
        vecBatch->Append(emptyArr);
        return;
    }
    if (!IsInputRaw()) {
        vecBatch->Append(VectorHelper::SliceVector(originVector, 0, rowCount));
        return;
    }
    int32_t nonNullCount = 0;
    for (int32_t i = 0; i < rowCount; i++) {
        if ((nullMap == nullptr || !(*nullMap)[i]) && !originVector->IsNull(i)) {
            nonNullCount++;
        }
    }
    auto *arrayVector = static_cast<ArrayVector *>(VectorHelper::CreateComplexVector(GetOutputTypes().GetType(0).get(), rowCount));
    auto *elementVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, elementTypeId_, nonNullCount));
    int32_t elemIdx = 0;
    bool isDictionary = originVector->GetEncoding() == OMNI_DICTIONARY;
    if (isDictionary) {
        auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(originVector);
        for (int32_t i = 0; i < rowCount; i++) {
            if ((nullMap != nullptr && (*nullMap)[i]) || originVector->IsNull(i)) {
                arrayVector->SetNull(i);
                arrayVector->SetSize(i, 0);
            } else {
                elementVector->SetValue(elemIdx, dictVec->GetValue(i));
                elemIdx++;
                arrayVector->SetSize(i, 1);
            }
        }
    } else {
        auto *flatVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(originVector);
        for (int32_t i = 0; i < rowCount; i++) {
            if ((nullMap != nullptr && (*nullMap)[i]) || originVector->IsNull(i)) {
                arrayVector->SetNull(i);
                arrayVector->SetSize(i, 0);
            } else {
                elementVector->SetValue(elemIdx, flatVec->GetValue(i));
                elemIdx++;
                arrayVector->SetSize(i, 1);
            }
        }
    }
    arrayVector->SetElementVector(std::shared_ptr<BaseVector>(elementVector));
    vecBatch->Append(arrayVector);
}

}  // namespace omniruntime::op
