/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Max by aggregate
 */

#include "maxby_aggregator.h"
#include "minmax_by_align_schema_helper.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    if (this->outputPartial) {
        auto *maxByState = MaxByState<targetValueType, sortKeyType>::ConstCastState(state + aggStateOffset);

        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        auto sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);
        if (maxByState->isEmpty) {
            targetValueVector->SetNull(rowIndex);
            sortKeyVector->SetNull(rowIndex);
            return;
        }

        if (maxByState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
        } else {
            targetValueVector->SetValue(rowIndex, maxByState->targetValue);
        }
        sortKeyVector->SetValue(rowIndex, maxByState->sortKey);
    } else {
        // final output, only one col
        auto *maxByState = MaxByState<targetValueType, sortKeyType>::ConstCastState(state + aggStateOffset);
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        if (maxByState->isEmpty) {
            // max_by empty table，final result is null
            targetValueVector->SetNull(rowIndex);
            return;
        }
        if (maxByState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
            return;
        }
        targetValueVector->SetValue(rowIndex, maxByState->targetValue);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (this->outputPartial) {
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        auto sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);
        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *maxByState = MaxByState<targetValueType, sortKeyType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (maxByState->isEmpty) {
                targetValueVector->SetNull(rowIndex);
                sortKeyVector->SetNull(rowIndex);
                continue;
            }
            if (maxByState->targetIsNull) {
                targetValueVector->SetNull(rowIndex);
            } else {
                targetValueVector->SetValue(rowIndex, maxByState->targetValue);
            }
            sortKeyVector->SetValue(rowIndex, maxByState->sortKey);
        }
    } else {
        // output final result, only one col
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *maxByState = MaxByState<targetValueType, sortKeyType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (maxByState->isEmpty) {
                // max_by empty table，final result is null
                targetValueVector->SetNull(rowIndex);
                continue;
            }
            if (maxByState->targetIsNull) {
                targetValueVector->SetNull(rowIndex);
                continue;
            }
            targetValueVector->SetValue(rowIndex, maxByState->targetValue);
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID> std::vector<DataTypePtr> MaxByAggregator<COL1_ID, COL2_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.emplace_back(std::make_shared<DataType>(COL1_ID));
    spillTypes.emplace_back(std::make_shared<DataType>(COL2_ID));
    return spillTypes;
}


template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
    auto sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);

    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *maxByState = MaxByState<targetValueType, sortKeyType>::CastState(groupStates[rowIndex] + aggStateOffset);
        if (maxByState->isEmpty) {
            // dont need spill any states
			sortKeyVector->SetNull(rowIndex);
			targetValueVector->SetNull(rowIndex);
            continue;
        }
        if (maxByState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
        } else {
            targetValueVector->SetValue(rowIndex, maxByState->targetValue);
        }
        sortKeyVector->SetValue(rowIndex, maxByState->sortKey);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID> void MaxByAggregator<COL1_ID, COL2_ID>::InitState(AggregateState *state)
{
    auto *maxByState = MaxByState<targetValueType, sortKeyType>::CastState(state + aggStateOffset);
    maxByState->isEmpty = true;
    maxByState->targetIsNull = false;
    maxByState->sortKey = GetSortKeyMin<sortKeyType>();
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto groupState : groupStates) {
        auto *maxByState = MaxByState<targetValueType, sortKeyType>::CastState(groupState + aggStateOffset);
        maxByState->isEmpty = true;
        maxByState->targetIsNull = false;
        maxByState->sortKey = GetSortKeyMin<sortKeyType>();
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *maxByState = MaxByState<targetValueType, sortKeyType>::CastState(state);

    auto *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    auto *col2Vector = this->curVectorBatch->Get(this->channels[1]);

    int col1Size = col1Vector->GetSize();
    int col2Size = col2Vector->GetSize();

    if (col1Size != col2Size) {
        std::string omniExceptionInfo = "col1Size != col2Size: " + std::to_string(col1Size) + " != " + std::to_string(col2Size);
        throw omniruntime::exception::OmniException("Error in MaxByAggregator::ProcessSingleInternal():", omniExceptionInfo);
    }

    for (int i = 0; i < col2Size; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        if (col2Vector->IsNull(rowOffset + i)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        sortKeyType key = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowOffset + i);
        if (key >= maxByState->sortKey) {
            maxByState->isEmpty = false;
            maxByState->sortKey = key;
            maxByState->targetIsNull = col1Vector->IsNull(rowOffset + i);
            if (!maxByState->targetIsNull) {
                auto targetVal = VectorHelper::GetFlatValue<COL1_ID>(col1Vector, rowOffset + i);
                if constexpr (IsSupportedStringMaxByType(COL1_ID)) {
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        maxByState->targetValue = std::string_view(copy, targetVal.size());
                        maxByState->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetVal.size() + 1];
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        copy[targetVal.size()] = '\0';
                        maxByState->targetValue = std::string_view(copy, targetVal.size());
                        maxByState->SetTargetValueOwned(true);
                    }
                } else {
                    maxByState->targetValue = targetVal;
                }
            }
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    // input vector must be <targetValueType, sortKeyType>
    auto *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    auto *col2Vector = this->curVectorBatch->Get(this->channels[1]);

    const size_t rowCount = rowStates.size();
    if (rowCount != col1Vector->GetSize() || rowCount != col2Vector->GetSize()) {
        std::string omniExceptionInfo = "rowStates count must be equal to base vec size";
        throw omniruntime::exception::OmniException("Error in MaxByAggregator::ProcessGroupInternal():", omniExceptionInfo);
    }

    if (rowCount <= 0) {
        // empty table, no result
        return;
    }

    for (size_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        if (col2Vector->IsNull(rowOffset + i)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        auto *maxByState = MaxByState<targetValueType, sortKeyType>::CastState(rowStates[i] + aggStateOffset);
        sortKeyType key = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowOffset + i);
        if (key >= maxByState->sortKey) {
            maxByState->isEmpty = false;
            maxByState->sortKey = key;
            maxByState->targetIsNull = col1Vector->IsNull(rowOffset + i);
            if (!maxByState->targetIsNull) {
                auto targetVal = VectorHelper::GetFlatValue<COL1_ID>(col1Vector, rowOffset + i);
                if constexpr (IsSupportedStringMaxByType(COL1_ID)) {
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        maxByState->targetValue = std::string_view(copy, targetVal.size());
                        maxByState->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetVal.size() + 1];
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        copy[targetVal.size()] = '\0';
                        maxByState->targetValue = std::string_view(copy, targetVal.size());
                        maxByState->SetTargetValueOwned(true);
                    }
                } else {
                    maxByState->targetValue = targetVal;
                }
            }
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto targetValueVecIdx = vectorIndex++;
    auto sortKeyVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto targetValueVector = static_cast<targetValueTypeVec *>(batch->Get(targetValueVecIdx));
        auto *state = MaxByState<targetValueType, sortKeyType>::CastState(row.state + aggStateOffset);
        auto sortKeyVector = static_cast<sortKeyTypeVec *>(batch->Get(sortKeyVecIdx));
        if (sortKeyVector->IsNull(index)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        if (targetValueVector->IsNull(index)) {
            continue;  // Spark: only consider rows with non-null target; skip null target row
        }
        auto sortKey = sortKeyVector->GetValue(index);
        if (sortKey >= state->sortKey) {
            state->isEmpty = false;
            state->sortKey = sortKey;
            state->targetIsNull = false;
            state->targetValue = targetValueVector->GetValue(index);
        }
    }
}


template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    MinMaxByAlignAggSchema<COL1_ID, COL2_ID>(result, inputVecBatch, channels, inputRaw, this->pool_);
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    MinMaxByAlignAggSchemaWithFilter<COL1_ID, COL2_ID>(result, inputVecBatch, channels, inputRaw, filterIndex,
        this->pool_);
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MaxByAggregator<COL1_ID, COL2_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        MinMaxByAlignAppendEmptyPartial2<COL1_ID, COL2_ID>(result, this->pool_);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
MaxByAggregator<COL1_ID, COL2_ID>::MaxByAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_MAX_BY, inputTypes, outputTypes, channels, inputRaw, outputPartial,
    isOverflowAsNull)
{}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances

// COL2 = OMNI_BOOLEAN
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_BYTE, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_SHORT, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_INT, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_LONG, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_CHAR, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_BOOLEAN>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_BOOLEAN>;

// COL2 = OMNI_BYTE
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_BYTE>;
template class MaxByAggregator<OMNI_BYTE, OMNI_BYTE>;
template class MaxByAggregator<OMNI_SHORT, OMNI_BYTE>;
template class MaxByAggregator<OMNI_INT, OMNI_BYTE>;
template class MaxByAggregator<OMNI_LONG, OMNI_BYTE>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_BYTE>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_BYTE>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_BYTE>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_BYTE>;
template class MaxByAggregator<OMNI_CHAR, OMNI_BYTE>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_BYTE>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_BYTE>;

// COL2 = OMNI_SHORT
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_SHORT>;
template class MaxByAggregator<OMNI_BYTE, OMNI_SHORT>;
template class MaxByAggregator<OMNI_SHORT, OMNI_SHORT>;
template class MaxByAggregator<OMNI_INT, OMNI_SHORT>;
template class MaxByAggregator<OMNI_LONG, OMNI_SHORT>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_SHORT>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_SHORT>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_SHORT>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_SHORT>;
template class MaxByAggregator<OMNI_CHAR, OMNI_SHORT>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_SHORT>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_SHORT>;

// COL2 = OMNI_INT
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_INT>;
template class MaxByAggregator<OMNI_BYTE, OMNI_INT>;
template class MaxByAggregator<OMNI_SHORT, OMNI_INT>;
template class MaxByAggregator<OMNI_INT, OMNI_INT>;
template class MaxByAggregator<OMNI_LONG, OMNI_INT>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_INT>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_INT>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_INT>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_INT>;
template class MaxByAggregator<OMNI_CHAR, OMNI_INT>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_INT>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_INT>;

// COL2 = OMNI_LONG
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_LONG>;
template class MaxByAggregator<OMNI_BYTE, OMNI_LONG>;
template class MaxByAggregator<OMNI_SHORT, OMNI_LONG>;
template class MaxByAggregator<OMNI_INT, OMNI_LONG>;
template class MaxByAggregator<OMNI_LONG, OMNI_LONG>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_LONG>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_LONG>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_LONG>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_LONG>;
template class MaxByAggregator<OMNI_CHAR, OMNI_LONG>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_LONG>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_LONG>;

// COL2 = OMNI_FLOAT
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_BYTE, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_SHORT, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_INT, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_LONG, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_CHAR, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_FLOAT>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_FLOAT>;

// COL2 = OMNI_DOUBLE
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_BYTE, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_SHORT, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_INT, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_LONG, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_CHAR, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_DOUBLE>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_DOUBLE>;

// COL2 = OMNI_DECIMAL64
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_BYTE, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_SHORT, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_INT, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_LONG, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_CHAR, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_DECIMAL64>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_DECIMAL64>;

// COL2 = OMNI_DECIMAL128
template class MaxByAggregator<OMNI_BOOLEAN, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_BYTE, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_SHORT, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_INT, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_LONG, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_FLOAT, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_DOUBLE, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_CHAR, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_VARCHAR, OMNI_DECIMAL128>;
template class MaxByAggregator<OMNI_VARBINARY, OMNI_DECIMAL128>;

} // namespace op
} // namespace omniruntime
