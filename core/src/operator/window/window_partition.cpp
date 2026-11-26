/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * @Description: window partition implementations
 */

#include "window_partition.h"
#include <memory>
#include "window_function.h"
#include "operator/pages_hash_strategy.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace std;

namespace omniruntime {
namespace op {
WindowPartition::WindowPartition(const type::DataTypes &sourceTypes, PagesIndex *pagesIndex, int32_t partitionStart,
                                 int32_t partitionEnd, int32_t *outputChannels, int32_t outputChannelsCount,
                                 vector<unique_ptr<WindowFunction>> &windowFunctions,
                                 PagesHashStrategy *peerGroupHashStrategy)
    : sourceTypes(sourceTypes), pagesIndex(pagesIndex), partitionStart(partitionStart), partitionEnd(partitionEnd),
      outputChannels(outputChannels), outputChannelsCount(outputChannelsCount),
      peerGroupHashStrategy(peerGroupHashStrategy), currentPosition(partitionStart), peerGroupStart(0), peerGroupEnd(0)
{
    for (auto &windowFunction : windowFunctions) {
        this->windowFunctions.push_back(windowFunction.get());
    }
    this->windowIndex = make_unique<WindowIndex>(pagesIndex, partitionStart, partitionEnd);
    for (auto &windowFunction : windowFunctions) {
        windowFunction->Reset(windowIndex.get());
    }
    UpdatePeerGroup();
}

WindowPartition::~WindowPartition() = default;

void WindowPartition::ProcessNextRow(VectorBatch *inputVecBatchForAgg, VectorBatch *outputVecBatch, int32_t index)
{
    int32_t channel = outputChannelsCount;

    // check for new peer group
    if (currentPosition == peerGroupEnd) {
        UpdatePeerGroup();
    }

    for (auto &windowFunction : windowFunctions) {
        unique_ptr<Range> range = GetFrameRange(windowFunction->GetWindowFrameInfo());
        windowFunction->ProcessRow(inputVecBatchForAgg, outputVecBatch->Get(channel), index,
                                   peerGroupStart - partitionStart, peerGroupEnd - partitionStart - 1,
                                   range->GetStart(), range->GetEnd());
        channel++;
    }

    currentPosition++;
}

bool PositionEqualsPosition(PagesIndex *pagesIndex, PagesHashStrategy *partitionHashStrategy, int32_t leftPosition,
                            int32_t rightPosition)
{
    uint64_t leftValueAddress = pagesIndex->GetValueAddresses()[leftPosition];
    int32_t leftColumnIndex = static_cast<int32_t>(DecodeSliceIndex(leftValueAddress));
    int32_t leftColumnPosition = static_cast<int32_t>(DecodePosition(leftValueAddress));

    uint64_t rightValueAddress = pagesIndex->GetValueAddresses()[rightPosition];
    int32_t rightColumnIndex = static_cast<int32_t>(DecodeSliceIndex(rightValueAddress));
    int32_t rightColumnPosition = static_cast<int32_t>(DecodePosition(rightValueAddress));

    return partitionHashStrategy->PositionEqualsPosition(leftColumnIndex, leftColumnPosition, rightColumnIndex,
                                                         rightColumnPosition);
}

void WindowPartition::UpdatePeerGroup()
{
    peerGroupStart = currentPosition;
    peerGroupEnd = peerGroupStart + 1;
    while ((peerGroupEnd < partitionEnd) &&
           PositionEqualsPosition(pagesIndex, peerGroupHashStrategy, peerGroupStart, peerGroupEnd)) {
        peerGroupEnd++;
    }
}

std::unique_ptr<Range> WindowPartition::GetFrameRange(WindowFrameInfo *frameInfo)
{
    int32_t rowPosition = currentPosition - partitionStart;
    int32_t endPosition = partitionEnd - partitionStart - 1;

    // handle empty frame
    if (IsEmptyFrame(frameInfo, rowPosition, endPosition)) {
        return std::make_unique<Range>(-1, -1);
    }

    int32_t frameStart = GetFrameStart(frameInfo, rowPosition, endPosition);
    int32_t frameEnd = GetFrameEnd(frameInfo, rowPosition, endPosition);

    return std::make_unique<Range>(frameStart, frameEnd);
}

int32_t WindowPartition::GetFrameStart(WindowFrameInfo *frameInfo, int32_t rowPosition, int32_t endPosition)
{
    int32_t frameStart;

    if (frameInfo->GetStartType() == OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING) {
        frameStart = 0;
    } else if (frameInfo->GetStartType() == OMNI_FRAME_BOUND_PRECEDING) {
        frameStart = Preceding(rowPosition, GetStartValue(frameInfo));
    } else if (frameInfo->GetStartType() == OMNI_FRAME_BOUND_FOLLOWING) {
        frameStart = Following(rowPosition, endPosition, GetStartValue(frameInfo));
    } else if (frameInfo->GetType() == OMNI_FRAME_TYPE_RANGE) {
        frameStart = peerGroupStart - partitionStart;
    } else {
        frameStart = rowPosition;
    }

    return frameStart;
}

int32_t WindowPartition::GetFrameEnd(WindowFrameInfo *frameInfo, int32_t rowPosition, int32_t endPosition)
{
    int32_t frameEnd;

    if (frameInfo->GetEndType() == OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING) {
        frameEnd = endPosition;
    } else if (frameInfo->GetEndType() == OMNI_FRAME_BOUND_PRECEDING) {
        frameEnd = Preceding(rowPosition, GetEndValue(frameInfo));
    } else if (frameInfo->GetEndType() == OMNI_FRAME_BOUND_FOLLOWING) {
        frameEnd = Following(rowPosition, endPosition, GetEndValue(frameInfo));
    } else if (frameInfo->GetType() == OMNI_FRAME_TYPE_RANGE) {
        frameEnd = peerGroupEnd - partitionStart - 1;
    } else {
        frameEnd = rowPosition;
    }

    return frameEnd;
}

bool WindowPartition::IsEmptyFrame(WindowFrameInfo *frameInfo, int32_t rowPosition, int32_t endPosition)
{
    FrameBoundType startType = frameInfo->GetStartType();
    FrameBoundType endType = frameInfo->GetEndType();

    int32_t positions = endPosition - rowPosition;

    if ((startType == OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING) && (endType == OMNI_FRAME_BOUND_PRECEDING)) {
        return GetEndValue(frameInfo) > rowPosition;
    }

    if ((startType == OMNI_FRAME_BOUND_FOLLOWING) && (endType == OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING)) {
        return GetStartValue(frameInfo) > positions;
    }

    if (startType != endType) {
        return false;
    }

    FrameBoundType boundType = frameInfo->GetStartType();
    if ((boundType != OMNI_FRAME_BOUND_PRECEDING) && (boundType != OMNI_FRAME_BOUND_FOLLOWING)) {
        return false;
    }

    int64_t start = GetStartValue(frameInfo);
    int64_t end = GetEndValue(frameInfo);

    if (boundType == OMNI_FRAME_BOUND_PRECEDING) {
        return (start < end) || ((start > rowPosition) && (end > rowPosition));
    }

    return (start > end) || (start > positions);
}

int32_t WindowPartition::Preceding(int32_t rowPosition, int64_t value)
{
    if (value > rowPosition) {
        return 0;
    }
    return (rowPosition - value);
}

int32_t WindowPartition::Following(int32_t rowPosition, int32_t endPosition, int64_t value)
{
    if (value > (endPosition - rowPosition)) {
        return endPosition;
    }
    return (rowPosition + value);
}

int64_t WindowPartition::GetStartValue(WindowFrameInfo *frameInfo)
{
    std::string rangeType("windowFrameStart");
    return GetFrameValue(frameInfo->GetStartChannel(), rangeType);
}

int64_t WindowPartition::GetEndValue(WindowFrameInfo *frameInfo)
{
    std::string rangeType("windowFrameEnd");
    return GetFrameValue(frameInfo->GetEndChannel(), rangeType);
}

int64_t WindowPartition::GetFrameValue(int32_t channel, std::string &valueTypeName)
{
    int64_t value = 0L;

    uint64_t valueAddress = pagesIndex->GetValueAddresses()[this->currentPosition];
    auto vecBatchIndex = static_cast<int32_t>(DecodeSliceIndex(valueAddress));
    auto rowIndex = static_cast<int32_t>(DecodePosition(valueAddress));

    BaseVector ***columnVectors = pagesIndex->GetColumns();
    BaseVector *columnVector = columnVectors[channel][vecBatchIndex];
    int32_t typeId = sourceTypes.GetType(channel)->GetId();

    if (columnVector->IsNull(rowIndex)) {
        LogError("%s column value is null(position=%d).", valueTypeName.data(), currentPosition);
        return value;
    }

    switch (typeId) {
        case OMNI_BYTE:
            if (columnVector->GetEncoding() == OMNI_DICTIONARY) {
                value = static_cast<Vector<DictionaryContainer<int8_t>> *>(columnVector)->GetValue(rowIndex);
            } else {
                value = static_cast<Vector<int8_t> *>(columnVector)->GetValue(rowIndex);
            }
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            if (columnVector->GetEncoding() == OMNI_DICTIONARY) {
                value = static_cast<Vector<DictionaryContainer<int32_t>> *>(columnVector)->GetValue(rowIndex);
            } else {
                value = static_cast<Vector<int32_t> *>(columnVector)->GetValue(rowIndex);
            }
            break;
        case OMNI_SHORT:
            if (columnVector->GetEncoding() == OMNI_DICTIONARY) {
                value = static_cast<Vector<DictionaryContainer<int16_t>> *>(columnVector)->GetValue(rowIndex);
            } else {
                value = static_cast<Vector<int16_t> *>(columnVector)->GetValue(rowIndex);
            }
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            if (columnVector->GetEncoding() == OMNI_DICTIONARY) {
                value = static_cast<Vector<DictionaryContainer<int64_t>> *>(columnVector)->GetValue(rowIndex);
            } else {
                value = static_cast<Vector<int64_t> *>(columnVector)->GetValue(rowIndex);
            }
            break;

        default:
            std::string omniExceptionInfo = "Error in WindowPartition::GetFrameValue, Invalid data type " +
                                            std::to_string(typeId) + "for column " + valueTypeName.data();
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }

    return value;
}
} // namespace op
} // namespace omniruntime
