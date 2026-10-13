/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min_by aggregate
 */

#include "minby_varchar_aggregator.h"
#include "minmax_by_align_schema_helper.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    auto *minByVarcharState = MinByVarcharState<targetValueType>::ConstCastState(state + aggStateOffset);

    if (this->outputPartial) {
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        auto sortKeyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[1]);
        if (minByVarcharState->GetStrKeyAddress() == 0) {
            targetValueVector->SetNull(rowIndex);
            sortKeyVector->SetNull(rowIndex);
            return;
        }

        std::string_view val(reinterpret_cast<char *>(minByVarcharState->GetStrKeyAddress()), minByVarcharState->GetStrKeyLen());
        sortKeyVector->SetValue(rowIndex, val);

        if (minByVarcharState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
        } else {
            targetValueVector->SetValue(rowIndex, minByVarcharState->targetValue);
        }

        // release data after extracting
        minByVarcharState->ReleaseSortKey();

    } else {
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        if (minByVarcharState->GetStrKeyAddress() == 0) {
            targetValueVector->SetNull(rowIndex);
            return;
        }
        if (minByVarcharState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
            return;
        }
        targetValueVector->SetValue(rowIndex, minByVarcharState->targetValue);

        // release data after extracting
        minByVarcharState->ReleaseSortKey();
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (this->outputPartial) {
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        auto sortKeyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[1]);

        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *minByVarcharState = MinByVarcharState<targetValueType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (minByVarcharState->GetStrKeyAddress() == 0) {
                targetValueVector->SetNull(rowIndex);
                sortKeyVector->SetNull(rowIndex);
                continue;
            }

            std::string_view val(reinterpret_cast<char *>(minByVarcharState->GetStrKeyAddress()), minByVarcharState->GetStrKeyLen());
            sortKeyVector->SetValue(rowIndex, val);
            if (minByVarcharState->targetIsNull) {
                targetValueVector->SetNull(rowIndex);
            } else {
                targetValueVector->SetValue(rowIndex, minByVarcharState->targetValue);
            }

            // release data after extracting
            minByVarcharState->ReleaseSortKey();
        }

    } else {
        // output final result, only one col
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *minByVarcharState = MinByVarcharState<targetValueType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (minByVarcharState->GetStrKeyAddress() == 0) {
                targetValueVector->SetNull(rowIndex);
                continue;
            }
            if (minByVarcharState->targetIsNull) {
                targetValueVector->SetNull(rowIndex);
                continue;
            }
            targetValueVector->SetValue(rowIndex, minByVarcharState->targetValue);

            // release data after extracting
            minByVarcharState->ReleaseSortKey();
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID> std::vector<DataTypePtr> MinByVarcharAggregator<COL1_ID, COL2_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.emplace_back(GetOutputTypes().GetType(0));
    spillTypes.emplace_back(GetInputTypes().GetType(1));
    return spillTypes;
}


template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
    auto sortKeyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[1]);

    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *minByVarcharState = MinByVarcharState<targetValueType>::CastState(groupStates[rowIndex] + aggStateOffset);
        if (minByVarcharState->GetStrKeyAddress() == 0) {
            sortKeyVector->SetNull(rowIndex);
            targetValueVector->SetNull(rowIndex);
            continue;
        }
        minByVarcharState->SaveSortKey();
        if (minByVarcharState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
        } else {
            targetValueVector->SetValue(rowIndex, minByVarcharState->targetValue);
        }
        std::string_view val(reinterpret_cast<char *>(minByVarcharState->GetStrKeyAddress()), minByVarcharState->GetStrKeyLen());
        sortKeyVector->SetValue(rowIndex, val);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID> void MinByVarcharAggregator<COL1_ID, COL2_ID>::InitState(AggregateState *state)
{
    auto *minByVarcharState = MinByVarcharState<targetValueType>::CastState(state + aggStateOffset);
    minByVarcharState->ClearStrKey();
    minByVarcharState->targetIsNull = false;
    minByVarcharState->ClearTargetValue();
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *state : groupStates) {
        InitState(state);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *minByVarcharState = MinByVarcharState<targetValueType>::CastState(state);
    auto *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    auto *col2Vector = this->curVectorBatch->Get(this->channels[1]);

    for (int32_t i = rowOffset; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        if (col2Vector->IsNull(i)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        bool targetNull = col1Vector->IsNull(i);
        targetValueType targetVal = VectorHelper::GetFlatValue<COL1_ID>(col1Vector, i);
        auto strView = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, i);
        if (minByVarcharState->GetStrKeyAddress() == 0) {
            minByVarcharState->SetStrKey(reinterpret_cast<int64_t>(strView.data()), strView.size());
            minByVarcharState->targetIsNull = targetNull;
            if (!targetNull) {
                if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                    minByVarcharState->ReleaseTargetValueIfOwned();
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                        minByVarcharState->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetVal.size() + 1];
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        copy[targetVal.size()] = '\0';
                        minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                        minByVarcharState->SetTargetValueOwned(true);
                    }
                } else {
                    minByVarcharState->targetValue = targetVal;
                }
            }
        } else {
            const auto *curVal = strView.data();
            int32_t curLen = strView.size();
            auto result = memcmp((const char *)minByVarcharState->GetStrKeyAddress(), curVal, std::min(minByVarcharState->GetStrKeyLen(), curLen));
            if (result > 0 || (result == 0 && minByVarcharState->GetStrKeyLen() >= curLen)) {
                minByVarcharState->SetStrKey(reinterpret_cast<int64_t>(strView.data()), curLen);
                minByVarcharState->targetIsNull = targetNull;
                if (!targetNull) {
                    if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                        minByVarcharState->ReleaseTargetValueIfOwned();
                        if (this->arenaAllocator != nullptr) {
                            char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                            std::memcpy(copy, targetVal.data(), targetVal.size());
                            minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                            minByVarcharState->SetTargetValueOwned(false);
                        } else {
                            char *copy = new char[targetVal.size() + 1];
                            std::memcpy(copy, targetVal.data(), targetVal.size());
                            copy[targetVal.size()] = '\0';
                            minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                            minByVarcharState->SetTargetValueOwned(true);
                        }
                    } else {
                        minByVarcharState->targetValue = targetVal;
                    }
                }
            }
        }
    }
    minByVarcharState->SaveSortKey();
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    auto *col2Vector = this->curVectorBatch->Get(this->channels[1]);

    const size_t rowCount = rowStates.size();
    if (rowCount != col1Vector->GetSize() || rowCount != col2Vector->GetSize()) {
        std::string omniExceptionInfo = "rowStates count must be equal to base vec size";
        throw omniruntime::exception::OmniException("Error in MinByVarcharAggregator::ProcessGroupInternal(): ", omniExceptionInfo);
    }
    if (rowCount <= 0) {
        return;
    }

    for (size_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        int32_t rowIdx = static_cast<int32_t>(rowOffset + i);
        if (col2Vector->IsNull(rowIdx)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        auto *minByVarcharState = MinByVarcharState<targetValueType>::CastState(rowStates[i] + aggStateOffset);
        bool targetNull = col1Vector->IsNull(rowIdx);
        targetValueType targetVal = VectorHelper::GetFlatValue<COL1_ID>(col1Vector, rowIdx);
        auto strView = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowIdx);
        if (minByVarcharState->GetStrKeyAddress() == 0) {
            minByVarcharState->SetStrKey(reinterpret_cast<int64_t>(strView.data()), strView.size());
            minByVarcharState->targetIsNull = targetNull;
            if (!targetNull) {
                if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                    minByVarcharState->ReleaseTargetValueIfOwned();
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                        minByVarcharState->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetVal.size() + 1];
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        copy[targetVal.size()] = '\0';
                        minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                        minByVarcharState->SetTargetValueOwned(true);
                    }
                } else {
                    minByVarcharState->targetValue = targetVal;
                }
            }
        } else {
            const auto *curVal = strView.data();
            int32_t curLen = strView.size();
            auto result = memcmp((const char *)minByVarcharState->GetStrKeyAddress(), curVal, std::min(minByVarcharState->GetStrKeyLen(), curLen));
            if (result > 0 || (result == 0 && minByVarcharState->GetStrKeyLen() >= curLen)) {
                minByVarcharState->SetStrKey(reinterpret_cast<int64_t>(strView.data()), curLen);
                minByVarcharState->targetIsNull = targetNull;
                if (!targetNull) {
                    if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                        minByVarcharState->ReleaseTargetValueIfOwned();
                        if (this->arenaAllocator != nullptr) {
                            char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                            std::memcpy(copy, targetVal.data(), targetVal.size());
                            minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                            minByVarcharState->SetTargetValueOwned(false);
                        } else {
                            char *copy = new char[targetVal.size() + 1];
                            std::memcpy(copy, targetVal.data(), targetVal.size());
                            copy[targetVal.size()] = '\0';
                            minByVarcharState->targetValue = std::string_view(copy, targetVal.size());
                            minByVarcharState->SetTargetValueOwned(true);
                        }
                    } else {
                        minByVarcharState->targetValue = targetVal;
                    }
                }
            }
        }
    }

    // copy the smallest key in case it will be released
    for (size_t i = 0; i < rowCount; i++) {
        auto *minByVarcharState = MinByVarcharState<targetValueType>::CastState(rowStates[i] + aggStateOffset);
        minByVarcharState->SaveSortKey();
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto targetValueVecIdx = vectorIndex++;
    auto sortKeyVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto targetValueVector = static_cast<targetValueTypeVec *>(batch->Get(targetValueVecIdx));
        auto *state = MinByVarcharState<targetValueType>::CastState(row.state + aggStateOffset);
        auto sortKeyVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(batch->Get(sortKeyVecIdx));
        if (sortKeyVector->IsNull(index)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        auto sortKey = sortKeyVector->GetValue(index);
        bool shouldUpdate = false;
        if (state->GetStrKeyAddress() == 0) {
            shouldUpdate = true;
        } else {
            const auto *curVal = sortKey.data();
            int32_t curLen = static_cast<int32_t>(sortKey.size());
            int cmp = memcmp(reinterpret_cast<const char *>(state->GetStrKeyAddress()), curVal,
                std::min(state->GetStrKeyLen(), curLen));
            if (cmp > 0 || (cmp == 0 && state->GetStrKeyLen() >= curLen)) {
                shouldUpdate = true;
            }
        }
        if (shouldUpdate) {
            state->targetIsNull = targetValueVector->IsNull(index);
            if (!state->targetIsNull) {
                auto targetValue = targetValueVector->GetValue(index);
                if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                    state->ReleaseTargetValueIfOwned();
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetValue.size()));
                        std::memcpy(copy, targetValue.data(), targetValue.size());
                        state->targetValue = std::string_view(copy, targetValue.size());
                        state->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetValue.size() + 1];
                        std::memcpy(copy, targetValue.data(), targetValue.size());
                        copy[targetValue.size()] = '\0';
                        state->targetValue = std::string_view(copy, targetValue.size());
                        state->SetTargetValueOwned(true);
                    }
                } else {
                    state->targetValue = targetValue;
                }
            }
            state->SetStrKey(reinterpret_cast<int64_t>(sortKey.data()), static_cast<int32_t>(sortKey.size()));
            state->SaveSortKey();
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    MinMaxByAlignAggSchema<COL1_ID, COL2_ID>(result, inputVecBatch, channels, inputRaw, this->pool_);
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::AlignAggSchemaWithFilter(VectorBatch *result,
    VectorBatch *inputVecBatch, const int32_t filterIndex)
{
    MinMaxByAlignAggSchemaWithFilter<COL1_ID, COL2_ID>(result, inputVecBatch, channels, inputRaw, filterIndex,
        this->pool_);
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByVarcharAggregator<COL1_ID, COL2_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        MinMaxByAlignAppendEmptyPartial2<COL1_ID, COL2_ID>(result, this->pool_);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
MinByVarcharAggregator<COL1_ID, COL2_ID>::MinByVarcharAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_MIN_BY, inputTypes, outputTypes, channels, inputRaw, outputPartial,
    isOverflowAsNull)
{}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances

// COL2 = OMNI_VARCHAR
template class MinByVarcharAggregator<OMNI_BOOLEAN, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_BYTE, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_SHORT, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_INT, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_LONG, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_FLOAT, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_DOUBLE, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_DECIMAL64, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_DECIMAL128, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_VARCHAR, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_CHAR, OMNI_VARCHAR>;
template class MinByVarcharAggregator<OMNI_VARBINARY, OMNI_VARCHAR>;

// COL2 = OMNI_CHAR
template class MinByVarcharAggregator<OMNI_BOOLEAN, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_BYTE, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_SHORT, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_INT, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_LONG, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_FLOAT, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_DOUBLE, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_DECIMAL64, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_DECIMAL128, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_VARCHAR, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_CHAR, OMNI_CHAR>;
template class MinByVarcharAggregator<OMNI_VARBINARY, OMNI_CHAR>;

// COL2 = OMNI_VARBINARY
template class MinByVarcharAggregator<OMNI_BOOLEAN, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_BYTE, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_SHORT, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_INT, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_LONG, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_FLOAT, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_DOUBLE, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_DECIMAL64, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_DECIMAL128, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_VARCHAR, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_CHAR, OMNI_VARBINARY>;
template class MinByVarcharAggregator<OMNI_VARBINARY, OMNI_VARBINARY>;

} // namespace op
} // namespace omniruntime
