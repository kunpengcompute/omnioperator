/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Inner supported aggregators header
 */
#ifndef AGGREGATOR_H
#define AGGREGATOR_H

#include "operator/aggregation/definitions.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "type/decimal128.h"
#include "type/base_operations.h"
#include "vector/vector.h"
#include "vector/vector_common.h"
#include "operator/execution_context.h"
#include "operator/util/function_type.h"
#include "operator/join/row_ref.h"
#include "util/type_util.h"
#include "util/config_util.h"
#include "state_flag_operation.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::exception;
using namespace omniruntime::vec;

struct ColumnIndex {
    int32_t idx;
    type::DataTypePtr input;
    type::DataTypePtr output;
};

using AggregateState = uint8_t;

struct DecimalAverageState {
    int64_t count;
    int64_t overflow;
    type::int128_t val;
};

struct DecimalSumState {
    int64_t overflow;
    type::int128_t val;
};

// still 32B per new
struct KeyValue {
    union {
        char *keyAddr;
        uint8_t *rowAddr;
    };
    size_t keyLen;
    int64_t hashValue;
    AggregateState *value;
};
static_assert(sizeof(KeyValue) == 32, "KeyValue spill staging footprint must remain 32 bytes");

struct UnspillRowInfo {
    AggregateState *state;
    VectorBatch *batch;
    int32_t rowIdx;
};

// Avg decimal and overflow is decode/encode in continuous memory
static inline void DecodeAvgDecimal(op::DecimalAverageState *statePtr, type::int128_t &val, int64_t &overflow,
    int64_t &count)
{
    count = statePtr->count;
    overflow = statePtr->overflow;
    val = statePtr->val;
}

static inline void EncodeAvgDecimal(op::DecimalAverageState *statePtr, const type::int128_t &val,
    const int64_t &overflow, const int64_t &count)
{
    statePtr->count = count;
    statePtr->overflow = overflow;
    statePtr->val = val;
}

template <typename T> int32_t ALWAYS_INLINE Compare(const T &leftVal, const T &rightVal)
{
    return (leftVal > rightVal ? 1 : (leftVal < rightVal ? -1 : 0));
}

class Aggregator {
public:
    /* Initiate this aggregator, such as setting default values for states.
     * @param aggregateType indicates which aggregate function this aggregator stands for
     * @param inputTypes indicates this aggregator's input data types(support multi-input)
     * it can use normal vector or container vector
     * @param outputTypes indicates this aggregator's output data types(support multi-input)
     * it can use normal vector or container vector
     * @param channels indicates this aggregator's input channels for VectorBatch.
     * @param inputRaw indicates this aggregator's input data type
     * true for raw input, false for intermeidate input, default value as true.
     * @param outputPartial indicates this aggregator's output data type.
     * true for intermeidate output, false for final output, default value as false.
     * @param isOverflowAsNull indicates aggregator handle overflow calculation result
     * true overflow as null value, false throw exception, default value as false.
     *
     */
    Aggregator(const FunctionType aggregateType, const type::DataTypes &inputTypes, const type::DataTypes &outputTypes,
        const std::vector<int32_t> &channels, const bool inputRaw = true, const bool outputPartial = false,
        const bool isOverflowAsNull = false)
        : type(aggregateType),
          inputTypes(inputTypes),
          outputTypes(outputTypes),
          inputRaw(inputRaw),
          outputPartial(outputPartial),
          isOverflowAsNull(isOverflowAsNull),
          channels(channels)
    {}

    virtual ~Aggregator() = default;

    virtual void SetExecutionContext(ExecutionContext *executionContext)
    {
        this->executionContext = executionContext;
        this->arenaAllocator = executionContext->GetArena();
    }

    virtual void ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, int32_t rowIndex)
    {
        throw OmniException("Not implemented",
            "ProcessGroup(AggregateState &, VectorBatch *, int32_t) not implemented for " +
            std::to_string(as_integer(type)));
    }

    virtual std::vector<DataTypePtr> GetSpillType()
    {
        throw OmniException("UNSUPPORTED_ERROR",
            "GetSpillType not implemented for " + std::to_string(as_integer(type)));
    }

    // for no groupby aggregation
    virtual void ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowOffset,
        const int32_t rowCount)
    {
#ifdef DEBUG
        LogWarn("Using not-optimized aggregator api for aggregator %d", as_integer(type));
#endif
        int32_t end = rowOffset + rowCount;
        for (int32_t i = rowOffset; i < end; ++i) {
            ProcessGroup(state + aggStateOffset, vectorBatch, i);
        }
    }

    virtual void AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
        const int32_t filterIndex) = 0;

    virtual void AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch) = 0;

    static bool DoNeedHandleAggFilter(Vector<bool> *filterVec, const int32_t rowOffset, const int32_t size)
    {
        int32_t rowEnd = rowOffset + size;
        for (int32_t start = rowOffset, end = rowEnd - 1; start <= end; ++start, --end) {
            if (!filterVec->GetValue(start) || !filterVec->GetValue(end)) {
                return true;
            }
        }
        return false;
    }

    // for no groupby aggregation  with filter
    virtual void ProcessGroupFilter(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowOffset,
        const int32_t filterIndex)
    {
#ifdef DEBUG
        LogWarn("Using not-optimized aggregator api for aggregator %d", as_integer(type));
#endif

        int32_t rowEnd = rowOffset + vectorBatch->GetRowCount();
        auto filterVec = static_cast<Vector<bool> *>(vectorBatch->Get(filterIndex));
        bool needFilterJude = DoNeedHandleAggFilter(filterVec, rowOffset, vectorBatch->GetRowCount());
        if (needFilterJude) {
            for (int32_t i = rowOffset; i < rowEnd; ++i) {
                if (filterVec->GetValue(i)) {
                    ProcessGroup(state + aggStateOffset, vectorBatch, i);
                }
            }
        } else {
            for (int32_t i = rowOffset; i < rowEnd; ++i) {
                ProcessGroup(state + aggStateOffset, vectorBatch, i);
            }
        }
    }

    // for groupby hash aggregation
    virtual void ProcessGroup(std::vector<AggregateState *> &rowStates, VectorBatch *vectorBatch,
        const int32_t rowOffset)
    {
#ifdef DEBUG
        LogWarn("Using not-optimized aggregator api for aggregator %d", as_integer(type));
#endif

        int32_t rowIndex = rowOffset;
        size_t rowCount = rowStates.size();

        for (size_t i = 0; i < rowCount; ++i) {
            ProcessGroup(rowStates[i] + aggStateOffset, vectorBatch, rowIndex++);
        }
    }

    virtual void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex)
    {
        throw OmniException("UNSUPPORTED_ERROR",
            "ProcessGroupUnspill not implemented for " + std::to_string(as_integer(type)));
    }

    // for groupby hash aggregation
    virtual void ProcessGroupFilter(std::vector<AggregateState *> &rowStates, const size_t aggIdx,
        VectorBatch *vectorBatch, const int32_t filterOffset, const int32_t rowOffset)
    {
#ifdef DEBUG
        LogWarn("Using not-optimized aggregator api for aggregator %d", as_integer(type));
#endif
        auto filterVecIdx = static_cast<int32_t>(filterOffset);
        auto filterVec = static_cast<Vector<bool> *>(vectorBatch->Get(filterVecIdx));
        auto rowCount = static_cast<int32_t>(rowStates.size());
        bool needFilterJude = DoNeedHandleAggFilter(filterVec, rowOffset, rowCount);

        int32_t rowIndex = rowOffset;
        if (needFilterJude) {
            for (int32_t i = 0; i < rowCount; ++i) {
                if (filterVec->GetValue(i)) {
                    ProcessGroup(rowStates[i] + aggStateOffset, vectorBatch, rowIndex++);
                    continue;
                }
                rowIndex++;
            }
        } else {
            for (int32_t i = 0; i < rowCount; ++i) {
                ProcessGroup(rowStates[i] + aggStateOffset, vectorBatch, rowIndex++);
            }
        }
    }

    virtual void InitState(AggregateState *state)
    {
        throw OmniException("not implement", "InitState");
    }

    virtual void SetStateOffset(int32_t offset)
    {
        aggStateOffset = offset;
    }

    virtual size_t GetStateSize() = 0;

    virtual void InitStates(std::vector<AggregateState *> &groupStates)
    {
        throw OmniException("not implement", "InitStates");
    }

    // Optional: free state-owned resources before state buffer is freed. Default no-op; overridden by collect_set/collect_list style aggregators.
    virtual void DestroyState(AggregateState *state) {}

    // set result to output vector
    virtual void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
        const int32_t rowIndex) = 0;

    virtual void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) = 0;

    virtual void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
        std::vector<BaseVector *> &vectors) = 0;

    virtual bool IsTypedAggregator()
    {
        return false;
    }

    virtual bool IsInputRaw() const
    {
        return this->inputRaw;
    }

    virtual bool IsOutputPartial() const
    {
        return this->outputPartial;
    }

    virtual bool IsOverflowAsNull() const
    {
        return this->isOverflowAsNull;
    }

    virtual void SetStatisticalAggregate(bool statisticalAggregate)
    {
        this->isStatisticalAggregate = statisticalAggregate;
    }

    virtual bool IsStatisticalAggregate() const
    {
        return this->isStatisticalAggregate;
    }

    virtual FunctionType GetType() const
    {
        return type;
    }

    virtual const type::DataTypes &GetInputTypes() const
    {
        return inputTypes;
    }

    virtual const type::DataTypes &GetOutputTypes() const
    {
        return outputTypes;
    }

    virtual const std::vector<int32_t> &GetInputChannels() const
    {
        return channels;
    }

    const ExecutionContext *GetExecutionContext() const
    {
        return executionContext;
    }

public:
    static constexpr int32_t INVALID_MASK_COL = -1;

protected:
    const FunctionType type;
    type::DataTypes inputTypes;
    type::DataTypes outputTypes;
    const bool inputRaw;
    const bool outputPartial;
    const bool isOverflowAsNull;
    bool isStatisticalAggregate = false;
    const std::vector<int32_t> channels;
    ExecutionContext *executionContext = nullptr;
    SimpleArenaAllocator *arenaAllocator = nullptr;
    int32_t aggStateOffset;
};
} // end of namespace op
} // end of namespace omniruntime
#endif
