/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: Count aggregate
 */
#pragma once
#include "typed_aggregator.h"

namespace omniruntime {
namespace op {
// mask not null, agg vec not null
inline uint8_t AddMask(NullsHelper &nullMap, const size_t length, const uint8_t *__restrict maskPtr)
{
    uint8_t nonZero = 0;
    for (size_t i = 0; i < length; ++i) {
        const auto v = maskPtr[i];
        nullMap.SetNull(i, !v);
        nonZero |= v;
    }
    return nonZero;
}

inline uint8_t AddDictMask(NullsHelper &nullMap, const size_t length, const uint8_t *__restrict maskPtr,
    const int32_t *__restrict indexMap)
{
    uint8_t nonZero = 0;
    for (size_t i = 0; i < length; ++i) {
        const auto v = maskPtr[indexMap[i]];
        nullMap.SetNull(i, !v);
        nonZero |= v;
    }
    return nonZero;
}

// mask nullable, agg vec not null OR
// mask not null, agg vec nullable: in this case maskNullMap is actually agg vec nullMap
inline uint8_t AddMask(NullsHelper &nullMap, const size_t length, const uint8_t *__restrict maskPtr,
    const NullsHelper &maskNullMap)
{
    uint8_t nonZero = 0;
    for (size_t i = 0; i < length; ++i) {
        const auto v = !maskNullMap[i] && maskPtr[i];
        nullMap.SetNull(i, !v);
        nonZero |= v;
    }
    return nonZero;
}

inline uint8_t AddDictMask(NullsHelper &nullMap, const size_t length, const uint8_t *__restrict maskPtr,
    const NullsHelper &maskNullMap, const int32_t *__restrict indexMap)
{
    uint8_t nonZero = 0;
    for (size_t i = 0; i < length; ++i) {
        const auto v = !maskNullMap[i] && maskPtr[indexMap[i]];
        nullMap.SetNull(i, !v);
        nonZero |= v;
    }
    return nonZero;
}

// mask nullable, agg vec nullable
inline uint8_t AddMask(NullsHelper &nullMap, const size_t length, const uint8_t *__restrict maskPtr,
    const NullsHelper &maskNullMap, const NullsHelper &vecNullMap)
{
    uint8_t nonZero = 0;
    for (size_t i = 0; i < length; ++i) {
        const auto v = !maskNullMap[i] && !vecNullMap[i] && maskPtr[i];
        nullMap.SetNull(i, !v);
        nonZero |= v;
    }
    return nonZero;
}

inline uint8_t AddDictMask(NullsHelper &nullMap, const size_t length, const uint8_t *__restrict maskPtr,
    const NullsHelper &maskNullMap, const NullsHelper &vecNullMap, const int32_t *__restrict indexMap)
{
    uint8_t nonZero = 0;
    for (size_t i = 0; i < length; ++i) {
        const auto v = !maskNullMap[i] && !vecNullMap[i] && maskPtr[indexMap[i]];
        nullMap.SetNull(i, !v);
        nonZero |= v;
    }
    return nonZero;
}

class TypedMaskColAggregator : public TypedAggregator {
public:
    ~TypedMaskColAggregator() override = default;

    void SetExecutionContext(ExecutionContext *executionContext) override
    {
        realAggregator->SetExecutionContext(executionContext);
        // Rebind the reusable null-map buffer to the pool-bound allocator so its
        // AllocateReuse() routes through the per-task pool instead of nullBridge.
        nullMapBuffer = std::make_shared<AlignedBuffer<uint8_t>>(0, executionContext->GetAllocator());
    }

    void ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowOffset,
        const int32_t rowCount) override
    {
        const std::shared_ptr<NullsHelper> nullMap = GenerateNullMap(vectorBatch, rowOffset, rowCount);
        if (nullMap == nullptr) {
            return;
        }

        BaseVector *vector = nullptr;
        auto aggChannels = realAggregator->GetInputChannels();
        if (aggChannels.size() > 0 && aggChannels[0] >= 0) {
            vector = vectorBatch->Get(aggChannels[0]);
        }

        realAggregator->ProcessSingleInternal(state + aggStateOffset, vector, rowOffset, rowCount, nullMap);
    }

    void ProcessGroupFilter(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowOffset,
        const int32_t filterIndex) override
    {
        int32_t rowCount = vectorBatch->GetRowCount();
        std::shared_ptr<NullsHelper> nullMap = GenerateNullMap(vectorBatch, rowOffset, rowCount);
        if (nullMap == nullptr) {
            return;
        }

        BaseVector *vector = nullptr;
        auto aggChannels = realAggregator->GetInputChannels();
        if (aggChannels.size() > 0 && aggChannels[0] >= 0) {
            vector = vectorBatch->Get(aggChannels[0]);
        }

        Vector<bool> *booleanVector = static_cast<Vector<bool> *>(vectorBatch->Get(filterIndex));

        bool needFilterJude = false;
        for (int32_t start = 0, end = rowCount - 1; start <= end; ++start, --end) {
            if (!booleanVector->GetValue(start) || !booleanVector->GetValue(end)) {
                needFilterJude = true;
                break;
            }
        }

        auto *filterPtr = unsafe::UnsafeVector::GetRawValues(booleanVector);
        filterPtr += rowOffset;
        if (needFilterJude) {
            // nullmapPtr can filter row which no need to aggregate
            // the nullMap: true means null
            // booleanVector: false means one row has been filtered
            auto nullmapPtr = *nullMap;

            for (int i = 0; i < rowCount; ++i) {
                nullmapPtr.SetNull(i, not filterPtr[i]);
            }
        }

        realAggregator->ProcessSingleInternal(state + aggStateOffset, vector, rowOffset, rowCount, nullMap);
    }

    void ProcessGroup(std::vector<AggregateState *> &rowStates, VectorBatch *vectorBatch,
        const int32_t rowOffset) override
    {
        const size_t rowCount = rowStates.size();
        const std::shared_ptr<NullsHelper> nullMap = GenerateNullMap(vectorBatch, rowOffset, rowCount);
        if (nullMap == nullptr) {
            return;
        }

        BaseVector *vector = nullptr;
        auto aggChannels = realAggregator->GetInputChannels();
        if (aggChannels.size() > 0 && aggChannels[0] >= 0) {
            vector = vectorBatch->Get(aggChannels[0]);
        }

        realAggregator->ProcessGroupInternal(rowStates, vector, rowOffset, nullMap);
    }

    void ProcessGroupFilter(std::vector<AggregateState *> &rowStates, const size_t aggIdx, VectorBatch *vectorBatch,
        const int32_t filterStart, const int32_t rowOffset) override
    {
        const size_t rowCount = rowStates.size();
        std::shared_ptr<NullsHelper> nullMap = GenerateNullMap(vectorBatch, rowOffset, rowCount);
        if (nullMap == nullptr) {
            return;
        }

        BaseVector *vector = nullptr;
        auto aggChannels = realAggregator->GetInputChannels();
        if (aggChannels.size() > 0 && aggChannels[0] >= 0) {
            vector = vectorBatch->Get(aggChannels[0]);
        }
        auto booleanVector = static_cast<Vector<bool> *>(vectorBatch->Get(filterStart + aggIdx));

        bool needFilterJude = false;
        for (int32_t start = 0, end = rowCount - 1; start <= end; ++start, --end) {
            if (!booleanVector->GetValue(start) || !booleanVector->GetValue(end)) {
                needFilterJude = true;
                break;
            }
        }

        auto *filterPtr = unsafe::UnsafeVector::GetRawValues(booleanVector);
        filterPtr += rowOffset;
        if (needFilterJude) {
            // nullMap can filter row which no need to aggregate
            // the nullMap: true means need filter
            // booleanVector: false means one row has been filtered
            auto nullmapPtr = *nullMap;

            for (int i = 0; i < rowCount; ++i) {
                nullmapPtr.SetNull(i, not filterPtr[i]);
            }
        }

        realAggregator->ProcessGroupInternal(rowStates, vector, rowOffset, nullMap);
    }

    virtual void SetStateOffset(int32_t offset)
    {
        Aggregator::SetStateOffset(offset);
        realAggregator->SetStateOffset(offset);
    }

    size_t GetStateSize() override
    {
        return realAggregator->GetStateSize();
    }

    std::vector<DataTypePtr> GetSpillType() override
    {
        return realAggregator->GetSpillType();
    }

    void InitState(AggregateState *state) override
    {
        realAggregator->InitState(state);
    }

    void InitStates(std::vector<AggregateState *> &groupStates) override
    {
        realAggregator->InitStates(groupStates);
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
        realAggregator->ExtractValuesForSpill(groupStates, vectors);
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        realAggregator->ExtractValues(state, vectors, rowIndex);
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        realAggregator->ExtractValuesBatch(groupStates, vectors, rowOffset, rowCount);
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        realAggregator->ProcessAlignAggSchema(result, originVector, nullMap, aggFilter);
    }

    bool IsInputRaw() const override
    {
        return realAggregator->IsInputRaw();
    }

    bool IsOutputPartial() const override
    {
        return realAggregator->IsOutputPartial();
    }

    bool IsOverflowAsNull() const override
    {
        return realAggregator->IsOverflowAsNull();
    }

    FunctionType GetType() const override
    {
        return realAggregator->GetType();
    }

    const DataTypes &GetInputTypes() const override
    {
        return realAggregator->GetInputTypes();
    }

    const DataTypes &GetOutputTypes() const override
    {
        return realAggregator->GetOutputTypes();
    }

    const std::vector<int32_t> &GetInputChannels() const override
    {
        return realAggregator->GetInputChannels();
    }

    static std::unique_ptr<Aggregator> Create(int32_t maskColumnId, std::unique_ptr<Aggregator> realAggregator)
    {
        return std::unique_ptr<TypedMaskColAggregator>(
            new TypedMaskColAggregator(maskColumnId, std::move(realAggregator)));
    }

protected:
    TypedMaskColAggregator(int32_t maskColumnId, std::unique_ptr<Aggregator> realAggregator)
        : TypedAggregator(realAggregator->GetType(), realAggregator->GetInputTypes(), realAggregator->GetOutputTypes(),
        realAggregator->GetInputChannels(), realAggregator->IsInputRaw(), realAggregator->IsOutputPartial(),
        realAggregator->IsOverflowAsNull()),
          maskColumnId(maskColumnId)
    {
        this->realAggregator =
            std::unique_ptr<TypedAggregator>(static_cast<TypedAggregator *>(realAggregator.release()));
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {}

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap)
    {}

private:
    std::shared_ptr<NullsHelper> GenerateNullMap(VectorBatch *vectorBatch, const int32_t rowOffset,
        const size_t rowCount)
    {
        if (maskColumnId < 0 || maskColumnId >= vectorBatch->GetVectorCount()) {
            throw OmniException("Illegal Arguement", "Aggregator maskColumnId " + std::to_string(maskColumnId) +
                " out of range [0, " + std::to_string(vectorBatch->GetVectorCount()) + ") for masked aggregator");
        }
        auto maskVector = vectorBatch->Get(maskColumnId);
        auto maskNullMap = maskVector->HasNull() ? unsafe::UnsafeBaseVector::GetNullsHelper(maskVector) : nullptr;
        if (maskNullMap != nullptr) {
            *maskNullMap += rowOffset;
        }

        std::shared_ptr<NullsHelper> aggNullMap = nullptr;
        auto aggChannels = realAggregator->GetInputChannels();
        if (aggChannels.size() > 0) {
            auto aggColumnId = aggChannels[0];
            if (aggColumnId >= vectorBatch->GetVectorCount()) {
                throw OmniException("Illegal Arguement", "Aggregator columnId " + std::to_string(aggColumnId) +
                    " out of range [0, " + std::to_string(vectorBatch->GetVectorCount()) + ") for masked aggregator");
            } else if (aggColumnId >= 0) {
                auto aggVector = vectorBatch->Get(aggColumnId);
                aggNullMap = aggVector->HasNull() ? unsafe::UnsafeBaseVector::GetNullsHelper(aggVector) : nullptr;
                if (aggNullMap != nullptr) {
                    *aggNullMap += rowOffset;
                }
            }
        }

        nullMapBuffer->AllocateReuse(NullsBuffer::CalculateNbytes(rowCount), true);
        auto nullMap = std::make_shared<NullsHelper>(std::make_shared<NullsBuffer>(rowCount, nullMapBuffer));
        bool hasValidRows;
        if (maskVector->GetEncoding() == OMNI_DICTIONARY) {
            hasValidRows = GenerateNullMapDict(nullMap, rowOffset, rowCount, maskVector, maskNullMap, aggNullMap);
        } else {
            hasValidRows = GenerateNullMapFlat(nullMap, rowOffset, rowCount, maskVector, maskNullMap, aggNullMap);
        }

        return hasValidRows ? nullMap : nullptr;
    }

    bool GenerateNullMapDict(std::shared_ptr<NullsHelper> nullMap, const int32_t rowOffset, const size_t rowCount,
        BaseVector *maskVector, const std::shared_ptr<NullsHelper> maskNullMap,
        const std::shared_ptr<NullsHelper> aggNullMap)
    {
        uint8_t hasValidRows;
        const int32_t *indexMap = GetIdsFromDict<OMNI_BOOLEAN>(maskVector) + rowOffset;
        uint8_t *maskPtr = reinterpret_cast<uint8_t *>(GetValuesFromDict<OMNI_BOOLEAN>(maskVector));

        if (maskNullMap == nullptr) {
            if (aggNullMap == nullptr) {
                hasValidRows = AddDictMask(*nullMap, rowCount, maskPtr, indexMap);
            } else {
                hasValidRows = AddDictMask(*nullMap, rowCount, maskPtr, *aggNullMap, indexMap);
            }
        } else {
            if (aggNullMap == nullptr) {
                hasValidRows = AddDictMask(*nullMap, rowCount, maskPtr, *maskNullMap, indexMap);
            } else {
                hasValidRows = AddDictMask(*nullMap, rowCount, maskPtr, *maskNullMap, *aggNullMap, indexMap);
            }
        }

        return hasValidRows != 0;
    }

    bool GenerateNullMapFlat(std::shared_ptr<NullsHelper> nullMap, const int32_t rowOffset, const size_t rowCount,
        BaseVector *maskVector, const std::shared_ptr<NullsHelper> maskNullMap,
        const std::shared_ptr<NullsHelper> aggNullMap)
    {
        uint8_t hasValidRows;
        uint8_t *maskPtr = reinterpret_cast<uint8_t *>(GetValuesFromVector<OMNI_BOOLEAN>(maskVector));
        maskPtr += rowOffset;

        if (maskNullMap == nullptr) {
            if (aggNullMap == nullptr) {
                hasValidRows = AddMask(*nullMap, rowCount, maskPtr);
            } else {
                hasValidRows = AddMask(*nullMap, rowCount, maskPtr, *aggNullMap);
            }
        } else {
            if (aggNullMap == nullptr) {
                hasValidRows = AddMask(*nullMap, rowCount, maskPtr, *maskNullMap);
            } else {
                hasValidRows = AddMask(*nullMap, rowCount, maskPtr, *maskNullMap, *aggNullMap);
            }
        }

        return hasValidRows != 0;
    }

    int32_t maskColumnId;
    std::unique_ptr<TypedAggregator> realAggregator;
    // define nullmap buffer as member variable tor reduce number of memory allocaitons
    std::shared_ptr<AlignedBuffer<uint8_t>> nullMapBuffer = std::make_shared<AlignedBuffer<uint8_t>>();
};
}
}
