/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
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
 * Description: Inner supported aggregators header
 */
#include "typed_aggregator.h"
namespace omniruntime {
namespace op {
TypedAggregator::TypedAggregator(const FunctionType aggregateType, const DataTypes &inputTypes,
                                 const DataTypes &outputTypes, const std::vector<int32_t> &channels,
                                 const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : Aggregator(aggregateType, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
{
}

BaseVector *TypedAggregator::GetVector(VectorBatch *vectorBatch, const int32_t rowOffset, const int32_t rowCount,
                                       std::shared_ptr<NullsHelper> *nullMap, const size_t channelIdx)
{
#ifdef DEBUG
    if (channelIdx < 0 || channelIdx >= channels.size()) {
        throw OmniException("Illegal Argument", "Aggregator channel index" + std::to_string(channelIdx) +
                                                    " out of range [0, " + std::to_string(channels.size()) + ") for " +
                                                    std::to_string(as_integer(type)));
    }
#endif

    auto channel = channels[channelIdx];
#ifdef DEBUG
    if (channel < 0 || channel >= vectorBatch->GetVectorCount()) {
        throw OmniException("Illegal Argument", "Aggregator channel " + std::to_string(channel) + " out of range [0, " +
                                                    std::to_string(vectorBatch->GetVectorCount()) + ") for " +
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

bool TypedAggregator::CheckTypes(const std::string &aggName, const DataTypes &inputTypes, const DataTypes &outputTypes,
                                 const DataTypeId inId, const DataTypeId outId)
{
    if (inputTypes.GetSize() <= 0 || outputTypes.GetSize() <= 0) {
        LogError("Error in %s aggregator: Input or output DataTypes is empty", aggName.c_str());
        return false;
    }

    if (!CheckType(inputTypes.GetType(0)->GetId(), inId)) {
        LogError("Error in %s aggregator: Expecting %s input type. Got %s", aggName.c_str(),
                 TypeUtil::TypeToStringLog(inId).c_str(),
                 TypeUtil::TypeToStringLog(inputTypes.GetType(0)->GetId()).c_str());
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
} // namespace op
} // namespace omniruntime
