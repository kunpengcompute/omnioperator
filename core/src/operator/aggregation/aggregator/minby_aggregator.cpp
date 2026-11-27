/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min_by aggregate
 */

#include "minby_aggregator.h"
#include "minmax_by_align_schema_helper.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    if (this->outputPartial) {
        auto *minByState = MinByState<targetValueType, sortKeyType>::ConstCastState(state + aggStateOffset);

        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        auto sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);
        if (minByState->isEmpty) {
            targetValueVector->SetNull(rowIndex);
            sortKeyVector->SetNull(rowIndex);
            return;
        }

        if (minByState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
        } else {
            targetValueVector->SetValue(rowIndex, minByState->targetValue);
        }
        sortKeyVector->SetValue(rowIndex, minByState->sortKey);
    } else {
        // output final result, only one col
        auto *minByState = MinByState<targetValueType, sortKeyType>::ConstCastState(state + aggStateOffset);
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        if (minByState->isEmpty) {
            // min_by empty table，final result is null
            targetValueVector->SetNull(rowIndex);
            return;
        }
        if (minByState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
            return;
        }
        targetValueVector->SetValue(rowIndex, minByState->targetValue);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (this->outputPartial) {
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        auto sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);
        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *minByState = MinByState<targetValueType, sortKeyType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (minByState->isEmpty) {
                targetValueVector->SetNull(rowIndex);
                sortKeyVector->SetNull(rowIndex);
                continue;
            }
            if (minByState->targetIsNull) {
                targetValueVector->SetNull(rowIndex);
            } else {
                targetValueVector->SetValue(rowIndex, minByState->targetValue);
            }
            sortKeyVector->SetValue(rowIndex, minByState->sortKey);
        }
    } else {
        // output final result, only one col
        auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *minByState = MinByState<targetValueType, sortKeyType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (minByState->isEmpty) {
                // min_by empty table，final result is null
                targetValueVector->SetNull(rowIndex);
                continue;
            }
            if (minByState->targetIsNull) {
                targetValueVector->SetNull(rowIndex);
                continue;
            }
            targetValueVector->SetValue(rowIndex, minByState->targetValue);
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID> std::vector<DataTypePtr> MinByAggregator<COL1_ID, COL2_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.emplace_back(std::make_shared<DataType>(COL1_ID));
    spillTypes.emplace_back(std::make_shared<DataType>(COL2_ID));
    return spillTypes;
}


template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto targetValueVector = static_cast<targetValueTypeVec *>(vectors[0]);
    auto sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);

    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *minByState = MinByState<targetValueType, sortKeyType>::CastState(groupStates[rowIndex] + aggStateOffset);
        if (minByState->isEmpty) {
            // dont need spill any states
			sortKeyVector->SetNull(rowIndex);
			targetValueVector->SetNull(rowIndex);
            continue;
        }
        if (minByState->targetIsNull) {
            targetValueVector->SetNull(rowIndex);
        } else {
            targetValueVector->SetValue(rowIndex, minByState->targetValue);
        }
        sortKeyVector->SetValue(rowIndex, minByState->sortKey);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID> void MinByAggregator<COL1_ID, COL2_ID>::InitState(AggregateState *state)
{
    auto *minByState = MinByState<targetValueType, sortKeyType>::CastState(state + aggStateOffset);
    minByState->isEmpty = true;
    minByState->targetIsNull = false;
    minByState->sortKey = GetSortKeyMax<sortKeyType>();
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto groupState : groupStates) {
        auto *minByState = MinByState<targetValueType, sortKeyType>::CastState(groupState + aggStateOffset);
        minByState->isEmpty = true;
        minByState->targetIsNull = false;
        minByState->sortKey = GetSortKeyMax<sortKeyType>();
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *minByState = MinByState<targetValueType, sortKeyType>::CastState(state);

    auto *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    auto *col2Vector = this->curVectorBatch->Get(this->channels[1]);

    int col1Size = col1Vector->GetSize();
    int col2Size = col2Vector->GetSize();

    if (col1Size != col2Size) {
        std::string omniExceptionInfo = "col1Size != col2Size: " + std::to_string(col1Size) + " != " + std::to_string(col2Size);
        throw omniruntime::exception::OmniException("Error in MinByAggregator::ProcessSingleInternal(): ", omniExceptionInfo);
    }

    for (int i = 0; i < col2Size; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        if (col2Vector->IsNull(rowOffset + i)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        sortKeyType key = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowOffset + i);
        if (key <= minByState->sortKey) {
            minByState->isEmpty = false;
            minByState->sortKey = key;
            minByState->targetIsNull = col1Vector->IsNull(rowOffset + i);
            if (!minByState->targetIsNull) {
                auto targetVal = VectorHelper::GetFlatValue<COL1_ID>(col1Vector, rowOffset + i);
                if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        minByState->targetValue = std::string_view(copy, targetVal.size());
                        minByState->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetVal.size() + 1];
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        copy[targetVal.size()] = '\0';
                        minByState->targetValue = std::string_view(copy, targetVal.size());
                        minByState->SetTargetValueOwned(true);
                    }
                } else {
                    minByState->targetValue = targetVal;
                }
            }
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    // input vector must be <targetValueType, sortKeyType>
    auto *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    auto *col2Vector = this->curVectorBatch->Get(this->channels[1]);

    const size_t rowCount = rowStates.size();
    if (rowCount != col1Vector->GetSize() || rowCount != col2Vector->GetSize()) {
        std::string omniExceptionInfo = "rowStates count must be equal to base vec size";
        throw omniruntime::exception::OmniException("Error in MinByAggregator::ProcessGroupInternal(): ", omniExceptionInfo);
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
        auto *minByState = MinByState<targetValueType, sortKeyType>::CastState(rowStates[i] + aggStateOffset);
        sortKeyType key = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowOffset + i);
        if (key <= minByState->sortKey) {
            minByState->isEmpty = false;
            minByState->sortKey = key;
            minByState->targetIsNull = col1Vector->IsNull(rowOffset + i);
            if (!minByState->targetIsNull) {
                auto targetVal = VectorHelper::GetFlatValue<COL1_ID>(col1Vector, rowOffset + i);
                if constexpr (IsSupportedStringMinByType(COL1_ID)) {
                    if (this->arenaAllocator != nullptr) {
                        char *copy = reinterpret_cast<char *>(this->arenaAllocator->Allocate(targetVal.size()));
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        minByState->targetValue = std::string_view(copy, targetVal.size());
                        minByState->SetTargetValueOwned(false);
                    } else {
                        char *copy = new char[targetVal.size() + 1];
                        std::memcpy(copy, targetVal.data(), targetVal.size());
                        copy[targetVal.size()] = '\0';
                        minByState->targetValue = std::string_view(copy, targetVal.size());
                        minByState->SetTargetValueOwned(true);
                    }
                } else {
                    minByState->targetValue = targetVal;
                }
            }
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto targetValueVecIdx = vectorIndex++;
    auto sortKeyVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto targetValueVector = static_cast<targetValueTypeVec *>(batch->Get(targetValueVecIdx));
        auto *state = MinByState<targetValueType, sortKeyType>::CastState(row.state + aggStateOffset);
        auto sortKeyVector = static_cast<sortKeyTypeVec *>(batch->Get(sortKeyVecIdx));
        if (sortKeyVector->IsNull(index)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        if (targetValueVector->IsNull(index)) {
            continue;  // Spark: only consider rows with non-null target; skip null target row
        }
        auto sortKey = sortKeyVector->GetValue(index);
        if (sortKey <= state->sortKey) {
            state->isEmpty = false;
            state->sortKey = sortKey;
            state->targetIsNull = false;
            state->targetValue = targetValueVector->GetValue(index);
        }
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    MinMaxByAlignAggSchema<COL1_ID, COL2_ID>(result, inputVecBatch, channels, inputRaw, this->pool_);
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    MinMaxByAlignAggSchemaWithFilter<COL1_ID, COL2_ID>(result, inputVecBatch, channels, inputRaw, filterIndex,
        this->pool_);
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
void MinByAggregator<COL1_ID, COL2_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        MinMaxByAlignAppendEmptyPartial2<COL1_ID, COL2_ID>(result, this->pool_);
    }
}

template <DataTypeId COL1_ID, DataTypeId COL2_ID>
MinByAggregator<COL1_ID, COL2_ID>::MinByAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_MIN_BY, inputTypes, outputTypes, channels, inputRaw, outputPartial,
    isOverflowAsNull)
{}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances

// COL2 = OMNI_BOOLEAN
template class MinByAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_BYTE, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_SHORT, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_INT, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_LONG, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_FLOAT, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_CHAR, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_BOOLEAN>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_BOOLEAN>;

// COL2 = OMNI_BYTE
template class MinByAggregator<OMNI_BOOLEAN, OMNI_BYTE>;
template class MinByAggregator<OMNI_BYTE, OMNI_BYTE>;
template class MinByAggregator<OMNI_SHORT, OMNI_BYTE>;
template class MinByAggregator<OMNI_INT, OMNI_BYTE>;
template class MinByAggregator<OMNI_LONG, OMNI_BYTE>;
template class MinByAggregator<OMNI_FLOAT, OMNI_BYTE>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_BYTE>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_BYTE>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_BYTE>;
template class MinByAggregator<OMNI_CHAR, OMNI_BYTE>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_BYTE>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_BYTE>;

// COL2 = OMNI_SHORT
template class MinByAggregator<OMNI_BOOLEAN, OMNI_SHORT>;
template class MinByAggregator<OMNI_BYTE, OMNI_SHORT>;
template class MinByAggregator<OMNI_SHORT, OMNI_SHORT>;
template class MinByAggregator<OMNI_INT, OMNI_SHORT>;
template class MinByAggregator<OMNI_LONG, OMNI_SHORT>;
template class MinByAggregator<OMNI_FLOAT, OMNI_SHORT>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_SHORT>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_SHORT>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_SHORT>;
template class MinByAggregator<OMNI_CHAR, OMNI_SHORT>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_SHORT>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_SHORT>;

// COL2 = OMNI_INT
template class MinByAggregator<OMNI_BOOLEAN, OMNI_INT>;
template class MinByAggregator<OMNI_BYTE, OMNI_INT>;
template class MinByAggregator<OMNI_SHORT, OMNI_INT>;
template class MinByAggregator<OMNI_INT, OMNI_INT>;
template class MinByAggregator<OMNI_LONG, OMNI_INT>;
template class MinByAggregator<OMNI_FLOAT, OMNI_INT>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_INT>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_INT>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_INT>;
template class MinByAggregator<OMNI_CHAR, OMNI_INT>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_INT>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_INT>;

// COL2 = OMNI_LONG
template class MinByAggregator<OMNI_BOOLEAN, OMNI_LONG>;
template class MinByAggregator<OMNI_BYTE, OMNI_LONG>;
template class MinByAggregator<OMNI_SHORT, OMNI_LONG>;
template class MinByAggregator<OMNI_INT, OMNI_LONG>;
template class MinByAggregator<OMNI_LONG, OMNI_LONG>;
template class MinByAggregator<OMNI_FLOAT, OMNI_LONG>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_LONG>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_LONG>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_LONG>;
template class MinByAggregator<OMNI_CHAR, OMNI_LONG>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_LONG>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_LONG>;

// COL2 = OMNI_FLOAT
template class MinByAggregator<OMNI_BOOLEAN, OMNI_FLOAT>;
template class MinByAggregator<OMNI_BYTE, OMNI_FLOAT>;
template class MinByAggregator<OMNI_SHORT, OMNI_FLOAT>;
template class MinByAggregator<OMNI_INT, OMNI_FLOAT>;
template class MinByAggregator<OMNI_LONG, OMNI_FLOAT>;
template class MinByAggregator<OMNI_FLOAT, OMNI_FLOAT>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_FLOAT>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_FLOAT>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_FLOAT>;
template class MinByAggregator<OMNI_CHAR, OMNI_FLOAT>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_FLOAT>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_FLOAT>;

// COL2 = OMNI_DOUBLE
template class MinByAggregator<OMNI_BOOLEAN, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_BYTE, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_SHORT, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_INT, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_LONG, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_FLOAT, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_CHAR, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_DOUBLE>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_DOUBLE>;

// COL2 = OMNI_DECIMAL64
template class MinByAggregator<OMNI_BOOLEAN, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_BYTE, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_SHORT, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_INT, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_LONG, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_FLOAT, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_CHAR, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_DECIMAL64>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_DECIMAL64>;

// COL2 = OMNI_DECIMAL128
template class MinByAggregator<OMNI_BOOLEAN, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_BYTE, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_SHORT, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_INT, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_LONG, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_FLOAT, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_DOUBLE, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_CHAR, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_VARCHAR, OMNI_DECIMAL128>;
template class MinByAggregator<OMNI_VARBINARY, OMNI_DECIMAL128>;
} // namespace op
} // namespace omniruntime
