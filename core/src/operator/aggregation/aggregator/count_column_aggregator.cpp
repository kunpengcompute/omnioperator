/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: Count aggregate
 */

#include "count_column_aggregator.h"

namespace omniruntime {
namespace op {
template <bool addIf>
VECTORIZE_LOOP NO_INLINE void AddConditionalCountRaw(int64_t &res, const size_t rowCount, const NullsHelper &condition)
{
    if (rowCount > 0) {
        for (size_t i = 0; i < rowCount; ++i) {
            res += (condition[i] == addIf);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state,
                                                         std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    const CountState *countState = CountState::ConstCastState(state + aggStateOffset);
    static_cast<Vector<int64_t> *>(vectors[0])->SetValue(rowIndex, countState->count);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
                                                              std::vector<BaseVector *> &vectors, int32_t rowOffset,
                                                              int32_t rowCount)
{
    auto countVec = static_cast<Vector<int64_t> *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *countState = CountState::CastState(groupStates[rowIndex] + aggStateOffset);
        countVec->SetValue(rowIndex, countState->count);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
                                                                 std::vector<BaseVector *> &vectors)
{
    auto spillCountVec = static_cast<Vector<int64_t> *>(vectors[0]);

    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *countState = CountState::CastState(groupStates[rowIndex] + aggStateOffset);
        spillCountVec->SetValue(rowIndex, countState->count);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <bool RAW_IN>
void CountColumnAggregator<IN_ID, OUT_ID>::ProcessSingleInternalFunction(AggregateState *state, BaseVector *vector,
                                                                         const int32_t rowOffset,
                                                                         const int32_t rowCount,
                                                                         const std::shared_ptr<NullsHelper> nullMap)
{
    auto *countState = CountState::CastState(state);
    if constexpr (RAW_IN) {
        if (nullMap == nullptr) {
            countState->count += rowCount;
        } else {
            AddConditionalCountRaw<false>(countState->count, rowCount, *nullMap);
        }
    } else {
        int64_t noUsed{};

        if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
            auto *ptr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(vector));
            ptr += rowOffset;
            if (nullMap == nullptr) {
                Add<int64_t, int64_t, int64_t, CountAllOp>(&(countState->count), noUsed, ptr, rowCount);
            } else {
                AddConditional<int64_t, int64_t, int64_t, CountAllConditionalOp<false>>(&countState->count, noUsed, ptr,
                                                                                        rowCount, *nullMap);
            }
        } else {
            auto *ptr = reinterpret_cast<int64_t *>(GetValuesFromDict<OMNI_LONG>(vector));
            auto *indexMap = GetIdsFromDict<OMNI_LONG>(vector) + rowOffset;
            if (nullMap == nullptr) {
                AddDict<int64_t, int64_t, int64_t, CountAllOp>(&countState->count, noUsed, ptr, rowCount, indexMap);
            } else {
                AddDictConditional<int64_t, int64_t, int64_t, CountAllConditionalOp<false>>(
                    &countState->count, noUsed, ptr, rowCount, *nullMap, indexMap);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
                                                                 const int32_t rowOffset, const int32_t rowCount,
                                                                 const std::shared_ptr<NullsHelper> nullMap)
{
    return (this->*processSingleInternalPtr)(state, vector, rowOffset, rowCount, nullMap);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
SIMD_ALWAYS_INLINE void CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternalFunctionForRawInput(
    std::vector<AggregateState *> &rowStates, const std::shared_ptr<NullsHelper> nullMap)
{
    if (nullMap == nullptr) {
        for (AggregateState *states : rowStates) {
            auto *countState = CountState::CastState(states + aggStateOffset);
            ++countState->count;
        }
    } else {
        size_t rowCount = rowStates.size();
        for (size_t i = 0; i < rowCount; ++i) {
            if (!(*nullMap)[i]) {
                auto *countState = CountState::CastState(rowStates[i] + aggStateOffset);
                ++countState->count;
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <bool RAW_IN>
VECTORIZE_LOOP void
CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternalFunction(std::vector<AggregateState *> &rowStates,
                                                                   BaseVector *vector, const int32_t rowOffset,
                                                                   const std::shared_ptr<NullsHelper> nullMap)
{
    if constexpr (RAW_IN) {
        ProcessGroupInternalFunctionForRawInput(rowStates, nullMap);
    } else {
        size_t rowCount = rowStates.size();
        int64_t unsedFlag = 0;

        if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
            auto *ptr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(vector));
            ptr += rowOffset;
            if (nullMap == nullptr) {
                for (size_t i = 0; i < rowCount; ++i) {
                    auto *countState = CountState::CastState(rowStates[i] + aggStateOffset);
                    CountAllOp(&(countState->count), unsedFlag, ptr[i], 1LL);
                }
            } else {
                for (size_t i = 0; i < rowCount; ++i) {
                    auto *countState = CountState::CastState(rowStates[i] + aggStateOffset);
                    CountAllConditionalOp<false>(&(countState->count), unsedFlag, ptr[i], 1LL, (*nullMap)[i]);
                }
            }
        } else {
            auto *ptr = reinterpret_cast<int64_t *>(GetValuesFromDict<OMNI_LONG>(vector));
            auto *indexMap = GetIdsFromDict<OMNI_LONG>(vector) + rowOffset;
            if (nullMap == nullptr) {
                for (size_t i = 0; i < rowCount; ++i) {
                    auto *countState = CountState::CastState(rowStates[i] + aggStateOffset);
                    CountAllOp(&(countState->count), unsedFlag, ptr[indexMap[i]], 0LL);
                }
            } else {
                for (size_t i = 0; i < rowCount; ++i) {
                    auto *countState = CountState::CastState(rowStates[i] + aggStateOffset);
                    CountAllConditionalOp<false>(&(countState->count), unsedFlag, ptr[indexMap[i]], 0LL, (*nullMap)[i]);
                }
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
                                                                BaseVector *vector, const int32_t rowOffset,
                                                                const std::shared_ptr<NullsHelper> nullMap)
{
    return (this->*processGroupInternalPtr)(rowStates, vector, rowOffset, nullMap);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
                                                               int32_t rowCount, int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto *state = CountState::CastState(row.state + aggStateOffset);

        auto countVector = static_cast<Vector<int64_t> *>(batch->Get(firstVecIdx));
        auto count = countVector->GetValue(index);
        state->count += count;
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CountColumnAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
                                                                 const std::shared_ptr<NullsHelper> nullMap,
                                                                 const bool aggFilter)
{
    int rowCount = originVector->GetSize();
    auto countVector = reinterpret_cast<Vector<int64_t> *>(VectorHelper::CreateFlatVector(OMNI_LONG, rowCount));
    if (nullMap != nullptr) {
        for (int index = 0; index < rowCount; ++index) {
            if ((*nullMap)[index]) {
                countVector->SetValue(index, 0);
            } else {
                countVector->SetValue(index, 1);
            }
        }
    } else {
        int64_t *valueAddr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(countVector));
        // each element is initialized to 1 directly.
        std::fill_n(valueAddr, rowCount, 1);
    }
    result->Append(countVector);
}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances
template class CountColumnAggregator<OMNI_NONE, OMNI_LONG>;

template class CountColumnAggregator<OMNI_BOOLEAN, OMNI_LONG>;

template class CountColumnAggregator<OMNI_BYTE, OMNI_LONG>;

template class CountColumnAggregator<OMNI_SHORT, OMNI_LONG>;

template class CountColumnAggregator<OMNI_DATE32, OMNI_LONG>;

template class CountColumnAggregator<OMNI_TIME32, OMNI_LONG>;

template class CountColumnAggregator<OMNI_INT, OMNI_LONG>;

template class CountColumnAggregator<OMNI_LONG, OMNI_LONG>;

template class CountColumnAggregator<OMNI_DATE64, OMNI_LONG>;

template class CountColumnAggregator<OMNI_TIME64, OMNI_LONG>;

template class CountColumnAggregator<OMNI_TIMESTAMP, OMNI_LONG>;

template class CountColumnAggregator<OMNI_DOUBLE, OMNI_LONG>;

template class CountColumnAggregator<OMNI_DECIMAL64, OMNI_LONG>;

template class CountColumnAggregator<OMNI_DECIMAL128, OMNI_LONG>;

template class CountColumnAggregator<OMNI_CONTAINER, OMNI_LONG>;

template class CountColumnAggregator<OMNI_VARCHAR, OMNI_LONG>;

template class CountColumnAggregator<OMNI_CHAR, OMNI_LONG>;

template class CountColumnAggregator<OMNI_INVALID, OMNI_LONG>;
} // namespace op
} // namespace omniruntime
