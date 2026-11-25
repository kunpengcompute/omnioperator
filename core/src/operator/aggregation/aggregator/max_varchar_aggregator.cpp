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
 * Description: Max aggregate for varchar
 */

#include "max_varchar_aggregator.h"

#include "min_varchar_aggregator.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
                                                        int32_t rowIndex)
{
    const VarcharState *varcharState = VarcharState::ConstCastState(state + aggStateOffset);
    auto v = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[0]);
    if (varcharState->realValue == 0) {
        // Note: due to issue #614 we should call SetValueNull on VarcharVector vector not Vector base class
        v->SetNull(rowIndex);
    } else {
        std::string_view val(reinterpret_cast<char *>(varcharState->realValue),
                             static_cast<int64_t>(varcharState->len));
        v->SetValue(rowIndex, val);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
                                                             std::vector<BaseVector *> &vectors, int32_t rowOffset,
                                                             int32_t rowCount)
{
    auto v = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        VarcharState *state = VarcharState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->realValue == 0) {
            // Note: due to issue #614 we should call SetValueNull on VarcharVector vector not Vector base class
            v->SetNull(rowIndex);
        } else {
            std::string_view val(reinterpret_cast<char *>(state->realValue), static_cast<int64_t>(state->len));
            v->SetValue(rowIndex, val);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
                                                                std::vector<BaseVector *> &vectors)
{
    auto v = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[0]);
    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        VarcharState *state = VarcharState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->realValue == 0) {
            // Note: due to issue #614 we should call SetValueNull on VarcharVector vector not Vector base class
            v->SetNull(rowIndex);
        } else {
            std::string_view val(reinterpret_cast<char *>(state->realValue), static_cast<int64_t>(state->len));
            v->SetValue(rowIndex, val);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
                                                                const int32_t rowOffset, const int32_t rowCount,
                                                                const std::shared_ptr<NullsHelper> nullMap)
{
    VarcharState *varcharState = VarcharState::CastState(state);
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        if (nullMap == nullptr) {
            AddChar<MaxCharOp>(state, vector, rowOffset, rowCount);
        } else {
            AddConditionalChar<MaxCharOp>(state, vector, rowOffset, rowCount, *nullMap);
        }
    } else {
        if (nullMap == nullptr) {
            AddDictChar<MaxDictCharOp>(state, vector, rowOffset, rowCount);
        } else {
            AddDictConditionalChar<MaxDictCharOp>(state, vector, rowOffset, rowCount, *nullMap);
        }
    }
    SaveState(varcharState);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
                                                               BaseVector *vector, const int32_t rowOffset,
                                                               const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        if (nullMap == nullptr) {
            AddUseRowIndexChar<MaxCharOp>(rowStates, aggStateOffset, vector, rowOffset);
        } else {
            AddConditionalUseRowIndexChar<MaxCharOp>(rowStates, aggStateOffset, vector, rowOffset, *nullMap);
        }
    } else {
        if (nullMap == nullptr) {
            AddDictUseRowIndexChar<MaxDictCharOp>(rowStates, aggStateOffset, rowOffset, vector);
        } else {
            AddDictConditionalUseRowIndexChar<MaxDictCharOp>(rowStates, aggStateOffset, rowOffset, vector, *nullMap);
        }
    }

    for (AggregateState *states : rowStates) {
        SaveState(states + aggStateOffset);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
                                                              int32_t rowCount, int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto varcharVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(batch->Get(firstVecIdx));
        if (!varcharVec->IsNull(index)) {
            VarcharState *varcharState = VarcharState::CastState(row.state + aggStateOffset);
            if (varcharState->realValue == 0) {
                auto strView = varcharVec->GetValue(index);
                auto *res = strView.data();
                varcharState->len = static_cast<int32_t>(strView.size());
                varcharState->needUpdate = true;
                varcharState->realValue = reinterpret_cast<int64_t>(res);
                SaveState(varcharState);
            } else {
                MaxCharOp(varcharState, varcharVec, index);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
                                                                const std::shared_ptr<NullsHelper> nullMap,
                                                                const bool aggFilter)
{
    // note: type relationship matches only (IN_ID == OMNI_CHAR || IN_ID == OMNI_VARCHAR) and
    // (OUT_ID == OMNI_CHAR || OUT_ID == OMNI_VARCHAR)
    if (!aggFilter) {
        int rowCount = originVector->GetSize();
        auto *maxVector = VectorHelper::SliceVector(originVector, 0, rowCount);
        result->Append(maxVector);
        return;
    }

    // handle agg filter, nullMap != nullptr
    if (originVector->GetEncoding() == OMNI_DICTIONARY) {
        ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<std::string_view>>>(result, originVector, nullMap);
    } else {
        ProcessAlignAggSchemaInternal<Vector<std::string_view>>(result, originVector, nullMap);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <typename T>
void MaxVarcharAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
                                                                        const std::shared_ptr<NullsHelper> nullMap)
{
    int rowCount = originVector->GetSize();
    auto maxVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateFlatVector(OUT_ID, rowCount));
    auto vector = reinterpret_cast<T *>(originVector);
    for (int index = 0; index < rowCount; ++index) {
        if ((*nullMap)[index]) {
            maxVector->SetNull(index);
        } else {
            std::string_view val = vector->GetValue(index);
            maxVector->SetValue(index, val);
        }
    }
    result->Append(maxVector);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> void MaxVarcharAggregator<IN_ID, OUT_ID>::SaveState(VarcharState *state)
{
    if (not state->needUpdate) {
        return;
    }

    int32_t len = state->len;
    uint8_t *ptr = reinterpret_cast<uint8_t *>(arenaAllocator->Allocate(len));
    std::copy(reinterpret_cast<uint8_t *>(state->realValue), reinterpret_cast<uint8_t *>(state->realValue) + len, ptr);
    state->realValue = reinterpret_cast<int64_t>(ptr);
    // reset update state
    state->needUpdate = false;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxVarcharAggregator<IN_ID, OUT_ID>::SaveState(AggregateState *state)
{
    SaveState(VarcharState::CastState(state));
}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances
template class MaxVarcharAggregator<OMNI_CHAR, OMNI_CHAR>;

template class MaxVarcharAggregator<OMNI_CHAR, OMNI_VARCHAR>;

template class MaxVarcharAggregator<OMNI_VARCHAR, OMNI_CHAR>;

template class MaxVarcharAggregator<OMNI_VARCHAR, OMNI_VARCHAR>;
} // namespace op
} // namespace omniruntime
