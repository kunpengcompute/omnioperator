/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: Inner supported aggregators header
 */
#include "typed_aggregator.h"
namespace omniruntime {
namespace op {
TypedAggregator::TypedAggregator(const FunctionType aggregateType, const DataTypes &inputTypes,
    const DataTypes &outputTypes, const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
    const bool isOverflowAsNull)
    : Aggregator(aggregateType, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
{}

// Binds the `allocator` field to the execution context's pool-bound Allocator
// so operator-side GetAllocator() singleton calls are eliminated.
void TypedAggregator::SetExecutionContext(ExecutionContext *executionContext)
{
    Aggregator::SetExecutionContext(executionContext);
    allocator = executionContext->GetAllocator();
}


BaseVector *TypedAggregator::GetVector(VectorBatch *vectorBatch, const int32_t rowOffset, const int32_t rowCount,
    std::shared_ptr<NullsHelper> *nullMap, const size_t channelIdx)
{
#ifdef DEBUG
    if (channelIdx < 0 || channelIdx >= channels.size()) {
        throw OmniException("Illegal Arguement", "Aggregator channel index" + std::to_string(channelIdx) +
            " out of range [0, " + std::to_string(channels.size()) + ") for " + std::to_string(as_integer(type)));
    }
#endif

    auto channel = channels[channelIdx];
#ifdef DEBUG
    if (channel < 0 || channel >= vectorBatch->GetVectorCount()) {
        throw OmniException("Illegal Arguement", "Aggregator channel " + std::to_string(channel) +
            " out of range [0, " + std::to_string(vectorBatch->GetVectorCount()) + ") for " +
            std::to_string(as_integer(type)));
    }
#endif

    auto vector = vectorBatch->Get(channel);

    auto nullsHelper = vector->HasNull() ? unsafe::UnsafeBaseVector::GetNullsHelper(vector) : nullptr;
    if (nullsHelper != nullptr) {
        *nullsHelper += rowOffset;
    }
    *nullMap = nullsHelper;
    return vector;
}

BaseVector* TypedAggregator::GetVector(VectorBatch *vectorBatch, const int32_t rowOffset, const int32_t rowCount,
    std::shared_ptr<NullsHelper> *nullMap) {
    std::vector<BaseVector*> baseVectors;

    std::vector<BaseVector*> vectors;
    for (size_t i = 0; i < channels.size(); ++i) {
        vectors.emplace_back(vectorBatch->Get(channels[i]));
    }

    if (channels.size() == 1) {
        auto channel = channels[0];
        auto vector = vectorBatch->Get(channel);
        auto nullsHelper = vector->HasNull() ? unsafe::UnsafeBaseVector::GetNullsHelper(vector) : nullptr;
        if (nullsHelper != nullptr) {
            *nullsHelper += rowOffset;
        }
        *nullMap = nullsHelper;
        return vector;
    } else {
        // NullsBuffer is only consumed on this multi-channel path; constructing it
        // lazily here keeps the single-channel fast path (the 2.26M-hit common case)
        // free of a wasted allocation. Pool-accounted via the aggregator's per-task
        // allocator (executionContext->GetAllocator()).
        auto nullsBuffer = std::make_shared<NullsBuffer>(rowCount, nullptr, 0, allocator);
        for (size_t i = rowOffset; i < rowCount; ++i) {
            bool isNull = std::any_of(vectors.begin(), vectors.end(), [i](BaseVector* baseVector) {
                return baseVector->IsNull(i);
            });
            nullsBuffer->SetNull(i, isNull);
        }

        auto channel = channels[0];
        auto vector = vectorBatch->Get(channel);

        auto nullsHelper = nullsBuffer->HasNull() ? std::make_shared<NullsHelper>(nullsBuffer) : nullptr;
        if (nullsHelper != nullptr) {
            *nullsHelper += rowOffset;
        }
        *nullMap = nullsHelper;
        return vector;
    }
}


bool TypedAggregator::CheckTypes(const std::string &aggName, const DataTypes &inputTypes, const DataTypes &outputTypes,
    const DataTypeId inId, const DataTypeId outId)
{
    if (inputTypes.GetSize() <= 0 || outputTypes.GetSize() <= 0) {
        LogError("Error in %s aggregator: Input or output DataTypes is empty", aggName.c_str());
        return false;
    }

    if (!CheckType(inputTypes.GetType(0)->GetId(), inId)) {
        LogError("Error in %s aggregator: Expecting %s input type. Got %s", aggName.c_str(),
            TypeUtil::TypeToStringLog(inId).c_str(), TypeUtil::TypeToStringLog(inputTypes.GetType(0)->GetId()).c_str());
        return false;
    }
    if (!CheckType(outputTypes.GetType(0)->GetId(), outId)) {
        LogError("Error in %s aggregator: Expecting %s input type. Got %s", aggName.c_str(),
            TypeUtil::TypeToStringLog(outId).c_str(),
            TypeUtil::TypeToStringLog(outputTypes.GetType(0)->GetId()).c_str());
        return false;
    }

    return true;
}
}
}