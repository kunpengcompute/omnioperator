/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * @Description: sort merge join scanner implementations
 */

#include "sort_merge_join_scanner.h"
#include <memory>
#include "operator/pages_index.h"

using namespace omniruntime::vec;
namespace omniruntime {
namespace op {
SortMergeJoinScanner::SortMergeJoinScanner(const DataTypes &streamedTableKeysTypes, int32_t *streamedTableKeysCols,
    int32_t keyColsCount, DynamicPagesIndex *streamedTablePagesIndex, const DataTypes &bufferedTableKeysTypes,
    int32_t *bufferedTableKeysCols, DynamicPagesIndex *bufferedTablePagesIndex, JoinType joinType,
    bool onlyBufferedFirstMatch)
    : streamedTableKeysTypes(streamedTableKeysTypes),
      joinType(joinType),
      streamedTableKeysCols(streamedTableKeysCols),
      bufferedTableKeysCols(bufferedTableKeysCols),
      keyColsCount(keyColsCount),
      onlyBufferedFirstMatch(onlyBufferedFirstMatch),
      streamedPagesIndexPosition(-1),
      bufferedPagesIndexPosition(0),
      streamedPagesIndex(streamedTablePagesIndex),
      bufferedPagesIndex(bufferedTablePagesIndex),
      preStreamedValueAddress(-1),
      preStreamedPagesIndexPosition(-1),
      preStatus(std::make_unique<InitialJoinStatus>())
{
    switch (joinType) {
        case JoinType::OMNI_JOIN_TYPE_INNER:
            scanFindNextRow = &SortMergeJoinScanner::InnerJoin<OMNI_JOIN_TYPE_INNER>;
            break;
        case JoinType::OMNI_JOIN_TYPE_LEFT_SEMI:
            scanFindNextRow = &SortMergeJoinScanner::InnerJoin<OMNI_JOIN_TYPE_LEFT_SEMI>;
            break;
        case JoinType::OMNI_JOIN_TYPE_LEFT:
            scanFindNextRow = &SortMergeJoinScanner::LeftOuterJoin<OMNI_JOIN_TYPE_LEFT>;
            break;
        case JoinType::OMNI_JOIN_TYPE_LEFT_ANTI:
            scanFindNextRow = &SortMergeJoinScanner::LeftOuterJoin<OMNI_JOIN_TYPE_LEFT_ANTI>;
            break;
        case JoinType::OMNI_JOIN_TYPE_FULL:
            scanFindNextRow = &SortMergeJoinScanner::FullOuterJoin;
            break;
        default:
            scanFindNextRow = &SortMergeJoinScanner::ErrorJoin;
            break;
    }
    if (joinType == JoinType::OMNI_JOIN_TYPE_LEFT || joinType == JoinType::OMNI_JOIN_TYPE_LEFT_ANTI) {
        cachedStreamedKeyColumns.resize(keyColsCount);
    }

    keyCompareFuncs.resize(streamedTableKeysTypes.GetSize());
    for (int i = 0; i < streamedTableKeysTypes.GetSize(); ++i) {
        auto colTypeId = streamedTableKeysTypes.GetIds()[i];
        switch (colTypeId) {
            case OMNI_BOOLEAN:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<bool, true, false, true>);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<int32_t, true, false, true>);
                break;
            case OMNI_SHORT:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<int16_t, true, false, true>);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<int64_t, true, false, true>);
                break;
            case OMNI_DOUBLE:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<double, true, false, true>);
                break;
            case OMNI_FLOAT:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<float, true, false, true>);
                break;
            case OMNI_VARBINARY:
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<std::string_view, true, false, true>);
                break;
            case OMNI_DECIMAL128:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<Decimal128, true, false, true>);
                break;
            case OMNI_BYTE:
                keyCompareFuncs[i] = (OperatorUtil::CompareValue<int8_t, true, false, true>);
                break;
            case OMNI_ARRAY:
                keyCompareFuncs[i] = (OperatorUtil::CompareArrayValue);
                break;
            case OMNI_ROW:
                keyCompareFuncs[i] = (OperatorUtil::CompareStructValue);
                break;
            default:
                throw omniruntime::exception::OmniException("SortMergeJoinScanner",
                    "Unsupported data type in smj: " + colTypeId);
        }
    }
}

SortMergeJoinScanner::~SortMergeJoinScanner() {}

int32_t SortMergeJoinScanner::GetStreamedPinBatchId() const
{
    // -1 doubles as both the unset sentinel and (via (uint32)-1<<32|(uint32)-1) the
    // synthetic null-row address. Address 0 (batch0/row0) is a REAL row and must pin.
    if (preStreamedValueAddress == -1) {
        return INT32_MAX;
    }
    const int32_t batchId = static_cast<int32_t>(DecodeSliceIndex(static_cast<uint64_t>(preStreamedValueAddress)));
    return batchId >= 0 ? batchId : INT32_MAX;
}

int64_t SortMergeJoinScanner::FindNextJoinRows()
{
    (this->*scanFindNextRow)();
    return preStatus->GenerateStatus();
}

int32_t SortMergeJoinScanner::GetMatchedValueAddresses(std::vector<int8_t> &isMatched,
    std::vector<int64_t> &streamedTblValueAddresses, std::vector<int64_t> &bufferedTblValueAddresses,
    std::vector<int8_t> &isBufferedKeyMatched)
{
    if (isMatched.empty() && streamedTblValueAddresses.empty() && bufferedTblValueAddresses.empty() &&
        isBufferedKeyMatched.empty()) {
        // Transfer the result buffers without copying. swap also gives the scanner
        // any capacity retained by the empty destination buffers for the next scan.
        isMatched.swap(isPreKeyMatched);
        streamedTblValueAddresses.swap(streamedValueAddress);
        bufferedTblValueAddresses.swap(bufferedValueAddress);
        isBufferedKeyMatched.swap(isSameBufferedKeyMatched);
        return 0;
    }

    isMatched.insert(isMatched.end(), isPreKeyMatched.begin(), isPreKeyMatched.end());
    streamedTblValueAddresses.insert(streamedTblValueAddresses.end(), streamedValueAddress.begin(),
        streamedValueAddress.end());
    bufferedTblValueAddresses.insert(bufferedTblValueAddresses.end(), bufferedValueAddress.begin(),
        bufferedValueAddress.end());
    isBufferedKeyMatched.insert(isBufferedKeyMatched.end(), isSameBufferedKeyMatched.begin(),
        isSameBufferedKeyMatched.end());
    isPreKeyMatched.clear();
    streamedValueAddress.clear();
    bufferedValueAddress.clear();
    isSameBufferedKeyMatched.clear();
    return 0;
}

template <JoinType templateJoinType> void SortMergeJoinScanner::InnerJoin()
{
    if (preStatus->NewStreamedDataAdded()) { // streamedCode == NEED_DATA
        // new streamed add data
        if (!IsValidAddedStreamedData()) {
            // streamed data is not valid
            return;
        }
        if (!AdvancedStreamedWithNullFreeJoinKey()) {
            // streamed data hit the end, change join status to
            // streamed NEED_DATA
            preStatus->TransToNeedStreamedData(HasResult());
            return;
        }
        if (PreKeyMatched()) {
            // The new streamed row has the same join key as the previous row, so return the same matches.
            SavePrevMatchingRows<templateJoinType>(1);
            return RunInnerJoin<templateJoinType>();
        }
    } else if (preStatus->NewBufferedDataAdded()) { // bufferedCode == NEED_DATA
        // new buffered add data
        if (!IsValidAddedBufferedData()) {
            // buffered data is not valid
            return;
        }
        if (!AdvancedBufferedToRowWithNullFreeJoinKey()) {
            // buffered data hit the end, change join status to
            // buffered NEED_DATA
            preStatus->TransToNeedBufferedData(HasResult());
            return;
        }
    } else { // INVALID 、 NEED_SCAN
        if (!AdvancedStreamedWithNullFreeJoinKey()) {
            // streamed data hit the end, change join status to
            // streamed NEED_DATA
            preStatus->TransToNeedStreamedData(HasResult());
            return;
        }
        if (PreKeyMatched()) {
            SavePrevMatchingRows<templateJoinType>(1);
            return RunInnerJoin<templateJoinType>();
        }
    }
    if (!FindMatchingRows<templateJoinType>()) {
        return;
    }
    return RunInnerJoin<templateJoinType>();
}

template <JoinType templateJoinType> void SortMergeJoinScanner::LeftOuterJoin()
{
    if (streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition)) {
        preStatus->Set(JoinTableCode::SCAN_FINISHED, JoinTableCode::SCAN_FINISHED, HasResult());
        return;
    }
    if (preStatus->NewStreamedDataAdded()) {
        // new streamed add data
        if (!AdvancedStreamedJoinKey()) {
            // streamed data hit the end, change join status to
            // streamed NEED_DATA
            preStatus->TransToNeedStreamedData(HasResult());
            return;
        }
        if (PreKeyMatchedWithNullValue()) {
            // The new streamed row has the same join key as the previous row, so return the same matches.
            SavePrevMatchingRows<templateJoinType>(1);
            return RunLeftOuterJoin<templateJoinType>();
        }
    } else if (preStatus->NewBufferedDataAdded()) {
        if (!bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
            if (!AdvancedBufferedToRowWithNullFreeJoinKey()) {
                // buffered data hit the end, change join status to
                // buffered NEED_DATA
                preStatus->TransToNeedBufferedData(HasResult());
                return;
            }
            curBufferRowMatchFlag = false;
        }
    } else {
        if (!streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition)) {
            if (!AdvancedStreamedJoinKey()) {
                // streamed data hit the end, change join status to
                // streamed NEED_DATA
                preStatus->TransToNeedStreamedData(HasResult());
                return;
            }
            if (PreKeyMatchedWithNullValue()) {
                SavePrevMatchingRows<templateJoinType>(1);
                return RunLeftOuterJoin<templateJoinType>();
            }
        }
    }
    if (!LeftOuterFindJoinRows<templateJoinType>()) {
        return;
    }
    return RunLeftOuterJoin<templateJoinType>();
}

void SortMergeJoinScanner::FullOuterJoin()
{
    if (streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition) &&
        bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
        preStatus->Set(JoinTableCode::SCAN_FINISHED, JoinTableCode::SCAN_FINISHED, HasResult());
        if (streamedLastScanFinished || bufferedPagesIndex->IsEmptyBatch()) {
            return;
        }
    }
    if (preStatus->NewStreamedDataAdded()) {
        // new streamed add data
        if (!streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition)) {
            if (!AdvancedStreamedJoinKey()) {
                // streamed data hit the end, change join status to streamed NEED_DATA
                preStatus->TransToNeedStreamedData(HasResult());
                return;
            } else if (PreKeyMatchedWithNullValue()) {
                // The new streamed row has the same join key as the previous row, so return the same matches.
                SavePrevMatchingRowsForFullOuter(streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition), 1);
                return RunFullOuterJoin();
            }
        }
    } else if (preStatus->NewBufferedDataAdded()) {
        // new buffered add data
        if (!bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
            if (!AdvancedBufferedJoinKey()) {
                // buffered data hit the end, change join status to buffered NEED_DATA
                preStatus->TransToNeedBufferedData(HasResult());
                return;
            }
        }
    } else {
        if (!streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition)) {
            if (!AdvancedStreamedJoinKey()) {
                // streamed data hit the end, change join status to streamed NEED_DATA
                preStatus->TransToNeedStreamedData(HasResult());
                return;
            }
            if (PreKeyMatchedWithNullValue()) {
                SavePrevMatchingRowsForFullOuter(streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition), 1);
                return RunFullOuterJoin();
            }
        }
    }
    if (!FullOuterFindJoinRows()) {
        return;
    }
    return RunFullOuterJoin();
}

void SortMergeJoinScanner::ErrorJoin()
{
    LogError("Unsupported join type: %u.", joinType);
    preStatus->Set(JoinTableCode::SCAN_FINISHED, JoinTableCode::SCAN_FINISHED, false);
}

template <JoinType templateJoinType> void SortMergeJoinScanner::RunInnerJoin()
{
    while (true) {
        if (!AdvancedStreamedWithNullFreeJoinKey()) {
            preStatus->TransToNeedStreamedData(HasResult());
            break;
        }
        if (PreKeyMatched()) {
            SavePrevMatchingRows<templateJoinType>(1);
            continue;
        }
        if (!FindMatchingRows<templateJoinType>()) {
            break;
        }
    }
}

template <JoinType templateJoinType> void SortMergeJoinScanner::RunLeftOuterJoin()
{
    while (true) {
        if (!AdvancedStreamedJoinKey()) {
            preStatus->TransToNeedStreamedData(HasResult());
            break;
        }
        if (PreKeyMatchedWithNullValue()) {
            SavePrevMatchingRows<templateJoinType>(1);
            continue;
        }
        if (!LeftOuterFindJoinRows<templateJoinType>()) {
            break;
        }
    }
}

void SortMergeJoinScanner::RunFullOuterJoin()
{
    while (true) {
        if (!AdvancedStreamedJoinKey()) {
            preStatus->TransToNeedStreamedData(HasResult());
            break;
        }
        if (PreKeyMatchedWithNullValue()) {
            SavePrevMatchingRowsForFullOuter(streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition), 1);
            continue;
        }

        if (!FullOuterFindJoinRows()) {
            break;
        }
    }
}

template <JoinType templateJoinType> bool SortMergeJoinScanner::FindMatchingRows()
{
    while (CurBufferedHasNull()) { // skip buffered null value
        if (!AdvancedBufferedJoinKey()) {
            preStatus->TransToNeedBufferedData(HasResult());
            return false;
        }
    }

    auto comp = FindNextMatchPos<templateJoinType>();
    if (NeedBufferedData()) {
        preStatus->TransToNeedBufferedData(HasResult());
        bufferedPagesIndexPosition--;
        return false;
    }
    if (comp > 0) {
        preStatus->TransToNeedBufferedData(HasResult());
        return false;
    }
    if (comp < 0) {
        preStatus->TransToNeedStreamedData(HasResult());
        return false;
    }

    BufferMatchingRows<templateJoinType>();
    return true;
}

template <JoinType templateJoinType> bool SortMergeJoinScanner::LeftOuterFindJoinRows()
{
    // handle stream has Null && buffered no data situation
    if (HandleLeftOuterStreamedNullAndBufferedDataFinishedSituation<templateJoinType>()) {
        return true;
    }

    while (CurBufferedHasNull()) { // skip buffered null value
        if (!AdvancedBufferedJoinKey()) {
            preStatus->TransToNeedBufferedData(HasResult());
            return false;
        }
    }

    if (NeedBufferedData()) { // if this buffered batch last row == stream row, so need add next buffered batch
        preStatus->TransToNeedBufferedData(HasResult());
        if (!curBufferRowMatchFlag) {
            bufferedPagesIndexPosition--;
        }
        return false;
    }
    const auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    const auto streamedBatchId = DecodeSliceIndex(streamedValueAddr);
    const auto streamedRowId = DecodePosition(streamedValueAddr);
    const auto bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
    const auto bufferedBatchId = DecodeSliceIndex(bufferedValueAddr);
    const auto bufferedRowId = DecodePosition(bufferedValueAddr);
    latestCompareStat = CompareRowKeys(streamedBatchId, streamedRowId, bufferedBatchId, bufferedRowId);
    if (latestCompareStat > 0) {
        // The streamed row stays fixed while the buffered side catches up.
        CacheStreamedKeyColumns(streamedBatchId);
    }
    while (latestCompareStat > 0) { // skip buffered compare >0 value condition
        if (AdvancedBufferedJoinKey()) {
            latestCompareStat = CompareCurrentBufferedRowWithCachedStreamed(streamedRowId);
        } else {
            if (bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
                BufferMissingRows<templateJoinType>();
                return true;
            }
            preStatus->TransToNeedBufferedData(HasResult());
            return false;
        }
    }

    if (latestCompareStat < 0 && !curStreamRowMatchFlag) {
        BufferMissingRows<templateJoinType>();
        curStreamRowMatchFlag = true;
        if (bufferedPagesIndexPosition > 0) {
            bufferedPagesIndexPosition--;
        }
    } else if (latestCompareStat == 0) {
        BufferMatchingRows<templateJoinType>();
        curStreamRowMatchFlag = true;
        if (preBufferedPagesIndexPosition == bufferedPagesIndexPosition) {
            curBufferRowMatchFlag = true;
            if (bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
                return true;
            }
            preStatus->TransToNeedBufferedData(HasResult());
            return false;
        } else {
            curBufferRowMatchFlag = false;
        }
    }
    return true;
}

bool SortMergeJoinScanner::FullOuterFindJoinRows()
{
    // handle streamed is finished
    if (!HandleFullOuterStreamedIsFinishedSituation()) {
        return false;
    }

    // handle buffered is finished
    if (HandleFullOuterBufferedIsFinishedSituation()) {
        return true;
    }

    // handle streamed null value
    if (HandleFullOuterStreamedNullValue()) {
        return true;
    }

    // handle buffered null value
    if (!HandleFullOuterBufferedNullValue()) {
        return false;
    }

    // need buffered data?
    if (!HandleFullOuterNeedBufferedData()) {
        return false;
    }

    latestCompareStat = CompareCurRowKeys();
    if (latestCompareStat < 0) {
        if (curStreamRowMatchFlag) {
            return true; // go to the next RunFullOuterJoin circle
        }
        BufferMissingRowsForFullOuter();
        curStreamRowMatchFlag = true;
        bufferedPagesIndexPosition--;
    } else if (latestCompareStat > 0 && !curBufferRowMatchFlag) {
        StreamMissingRowsForCompareValue();
        curBufferRowMatchFlag = true;
        streamedPagesIndexPosition--;
        if (preBufferedPagesIndexPosition != bufferedPagesIndexPosition) {
            bufferedPagesIndexPosition--; // not hit buffered end, bufferedPagesIndexPosition need -1
        }
    } else {
        BufferMatchingRowsForFullOuter();
        curStreamRowMatchFlag = true;
        curBufferRowMatchFlag = true;
        if (preBufferedPagesIndexPosition != bufferedPagesIndexPosition) {
            bufferedPagesIndexPosition--; // not hit buffered end, bufferedPagesIndexPosition need -1
        }
    }

    if (!AdvancedBufferedJoinKey()) {
        if (bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
            return true;
        }
        preStatus->TransToNeedBufferedData(HasResult());
        return false;
    }
    return true;
}

bool SortMergeJoinScanner::IsValidAddedStreamedData()
{
    if (streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition) &&
        bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
        preStatus->Set(JoinTableCode::SCAN_FINISHED, JoinTableCode::SCAN_FINISHED, false);
        return false;
    }
    if (streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition)) {
        preStatus->Set(JoinTableCode::SCAN_FINISHED, JoinTableCode::NEED_DATA, false);
        return false;
    }
    return true;
}

bool SortMergeJoinScanner::IsValidAddedBufferedData()
{
    if (streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition) &&
        bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
        preStatus->Set(JoinTableCode::SCAN_FINISHED, JoinTableCode::SCAN_FINISHED, false);
        return false;
    }
    if (bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition)) {
        preStatus->Set(JoinTableCode::NEED_DATA, JoinTableCode::SCAN_FINISHED, false);
        return false;
    }
    return true;
}

static ALWAYS_INLINE bool HasNext(int32_t pos, DynamicPagesIndex *pagesIndex)
{
    return pos < pagesIndex->GetPositionCount() - 1;
}

/**
 * Advance the streamed iterator until we find a row with join key that does not contain nulls.
 * @return true if the streamed iterator returned a row and false otherwise.
 */
bool SortMergeJoinScanner::AdvancedStreamedWithNullFreeJoinKey()
{
    bool found = false;
    while (!found && HasNext(streamedPagesIndexPosition, streamedPagesIndex)) {
        // Advance the streamed side of the join until we find the next row whose join key contains
        // no nulls or we hit the end of the streamed iterator.
        streamedPagesIndexPosition++;
        found = !CurStreamedHasNull();
    }

    return found;
}

bool SortMergeJoinScanner::AdvancedStreamedJoinKey()
{
    if (HasNext(streamedPagesIndexPosition, streamedPagesIndex)) {
        streamedPagesIndexPosition++;
        curStreamRowMatchFlag = false;
        return true;
    }
    return false;
}

/**
 * Advance the buffered iterator until we find a row with join key that does not contain nulls.
 * @return true if the buffered iterator returned a row and false otherwise.
 */
bool SortMergeJoinScanner::AdvancedBufferedToRowWithNullFreeJoinKey()
{
    bool found = false;
    while (!found && HasNext(bufferedPagesIndexPosition, bufferedPagesIndex)) {
        bufferedPagesIndexPosition++;
        curBufferRowMatchFlag = false;
        found = !CurBufferedHasNull();
    }

    return found;
}

bool SortMergeJoinScanner::AdvancedBufferedJoinKey()
{
    if (HasNext(bufferedPagesIndexPosition, bufferedPagesIndex)) {
        bufferedPagesIndexPosition++;
        curBufferRowMatchFlag = false;
        return true;
    }
    return false;
}

template <JoinType templateJoinType> void SortMergeJoinScanner::BufferMatchingRows()
{
    auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    preStreamedValueAddress = streamedValueAddr;
    preStreamedPagesIndexPosition = streamedPagesIndexPosition;
    preBufferedValueAddress.clear();
    preBufferedKeyMatched.clear();
    int64_t bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
    if constexpr (templateJoinType == OMNI_JOIN_TYPE_INNER || templateJoinType == OMNI_JOIN_TYPE_LEFT) {
        preBufferedValueAddress.emplace_back(bufferedValueAddr);
    } else {
        if (!onlyBufferedFirstMatch || preBufferedValueAddress.empty()) {
            preBufferedKeyMatched.emplace_back(!preBufferedValueAddress.empty());
            preBufferedValueAddress.emplace_back(bufferedValueAddr);
        }
    }
    preBufferedPagesIndexPosition = bufferedPagesIndexPosition;

    auto streamBatchId = DecodeSliceIndex(streamedValueAddr);
    auto streamRowId = DecodePosition(streamedValueAddr);
    while (AdvancedBufferedToRowWithNullFreeJoinKey()) {
        bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
        auto bufferBatchId = DecodeSliceIndex(bufferedValueAddr);
        auto bufferRowId = DecodePosition(bufferedValueAddr);
        latestCompareStat = CompareRowKeys(streamBatchId, streamRowId, bufferBatchId, bufferRowId);
        if (latestCompareStat != 0) {
            break;
        }
        if constexpr (templateJoinType == OMNI_JOIN_TYPE_INNER || templateJoinType == OMNI_JOIN_TYPE_LEFT) {
            preBufferedValueAddress.emplace_back(bufferedValueAddr);
        } else {
            if (!onlyBufferedFirstMatch || preBufferedValueAddress.empty()) {
                preBufferedKeyMatched.emplace_back(!preBufferedValueAddress.empty());
                preBufferedValueAddress.emplace_back(bufferedValueAddr);
            }
        }
        preBufferedPagesIndexPosition = bufferedPagesIndexPosition;
    }
    SavePrevMatchingRows<templateJoinType>(0);
}

void SortMergeJoinScanner::BufferMatchingRowsForFullOuter()
{
    auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    preStreamedValueAddress = streamedValueAddr;
    preStreamedPagesIndexPosition = streamedPagesIndexPosition;
    preBufferedValueAddress.clear();
    int64_t bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
    preBufferedValueAddress.emplace_back(bufferedValueAddr);
    preBufferedPagesIndexPosition = bufferedPagesIndexPosition;

    auto streamBatchId = DecodeSliceIndex(streamedValueAddr);
    auto streamRowId = DecodePosition(streamedValueAddr);
    while (AdvancedBufferedJoinKey()) {
        bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
        auto bufferBatchId = DecodeSliceIndex(bufferedValueAddr);
        auto bufferRowId = DecodePosition(bufferedValueAddr);
        latestCompareStat = CompareRowKeys(streamBatchId, streamRowId, bufferBatchId, bufferRowId);
        if (latestCompareStat != 0) {
            break;
        }
        preBufferedValueAddress.emplace_back(bufferedValueAddr);
        preBufferedPagesIndexPosition = bufferedPagesIndexPosition;
    }
    SavePrevMatchingRowsForFullOuter(streamedValueAddr, 0);
}

template <JoinType templateJoinType>
bool SortMergeJoinScanner::HandleLeftOuterStreamedNullAndBufferedDataFinishedSituation()
{
    if (CurStreamedHasNull() || bufferedPagesIndex->IsEmptyBatch() ||
        (bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition) && curBufferRowMatchFlag)) {
        if (!curStreamRowMatchFlag) {
            BufferMissingRows<templateJoinType>();
        }
        return true;
    }
    return false;
}

template <JoinType templateJoinType> void SortMergeJoinScanner::BufferMissingRows()
{
    auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    preStreamedValueAddress = streamedValueAddr;
    preStreamedPagesIndexPosition = streamedPagesIndexPosition;
    preBufferedValueAddress.clear();
    auto nullValueAddress = EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG); // null row flag
    preBufferedValueAddress.emplace_back(nullValueAddress);

    if constexpr (templateJoinType == OMNI_JOIN_TYPE_LEFT_SEMI || templateJoinType == OMNI_JOIN_TYPE_LEFT_ANTI) {
        preBufferedKeyMatched.clear();
        preBufferedKeyMatched.emplace_back(0);
        isSameBufferedKeyMatched.emplace_back(0);
    }

    bufferedValueAddress.emplace_back(nullValueAddress);
    streamedValueAddress.emplace_back(streamedValueAddr);
    isPreKeyMatched.emplace_back(0);
    preBufferedPagesIndexPosition = bufferedPagesIndexPosition;
}

void SortMergeJoinScanner::BufferMissingRowsForFullOuter()
{
    auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    preStreamedValueAddress = streamedValueAddr;
    preStreamedPagesIndexPosition = streamedPagesIndexPosition;
    preBufferedValueAddress.clear();
    auto nullValueAddress = EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG); // null row flag
    preBufferedValueAddress.emplace_back(nullValueAddress);
    bufferedValueAddress.emplace_back(nullValueAddress);
    streamedValueAddress.emplace_back(streamedValueAddr);
    isPreKeyMatched.emplace_back(0);
    preBufferedPagesIndexPosition = bufferedPagesIndexPosition;
}

void SortMergeJoinScanner::StreamMissingRowsForCompareValue()
{
    preStreamedValueAddress = EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG); // null row flag
    preStreamedPagesIndexPosition = -1;
    preBufferedValueAddress.clear();
    int64_t bufferedValueAddr;
    do {
        bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
        preBufferedValueAddress.emplace_back(bufferedValueAddr);
        preBufferedPagesIndexPosition = bufferedPagesIndexPosition;
    } while (AdvancedBufferedJoinKey() && ((latestCompareStat = CompareCurRowKeys()) > 0));
    SavePrevMatchingRowsForFullOuter(preStreamedValueAddress, 0);
}

bool SortMergeJoinScanner::HandleFullOuterStreamedIsFinishedSituation()
{
    if (streamedPagesIndex->IsEmptyBatch() || streamedPagesIndex->IsDataFinish(streamedPagesIndexPosition)) {
        streamedLastScanFinished = true;
        if (!curBufferRowMatchFlag) {
            StreamMissingRowsForStreamIsFinished();
            curBufferRowMatchFlag = true;
        }
        preStatus->TransToNeedBufferedData(HasResult());
        return false;
    }
    return true;
}

bool SortMergeJoinScanner::HandleFullOuterBufferedIsFinishedSituation()
{
    if (bufferedPagesIndex->IsEmptyBatch() ||
        (bufferedPagesIndex->IsDataFinish(bufferedPagesIndexPosition) && curBufferRowMatchFlag)) {
        if (!curStreamRowMatchFlag) {
            BufferMissingRowsForFullOuter();
            curStreamRowMatchFlag = true;
        }
        return true;
    }
    return false;
}

bool SortMergeJoinScanner::HandleFullOuterStreamedNullValue()
{
    if (CurStreamedHasNull()) {
        BufferMissingRowsForFullOuter();
        curStreamRowMatchFlag = true;
        return true;
    }
    return false;
}

bool SortMergeJoinScanner::HandleFullOuterBufferedNullValue()
{
    if (CurBufferedHasNull()) {
        StreamMissingRowsForNullBuffered();
        if (preBufferedPagesIndexPosition == bufferedPagesIndexPosition) { // hit buffered end
            preStatus->TransToNeedBufferedData(HasResult());
            return false;
        }
    }
    return true;
}

bool SortMergeJoinScanner::HandleFullOuterNeedBufferedData()
{
    if (NeedBufferedData()) { // if this buffered batch last row == stream row, so need add next buffered batch
        preStatus->TransToNeedBufferedData(HasResult());
        bufferedPagesIndexPosition--;
        return false;
    }
    return true;
}

void SortMergeJoinScanner::StreamMissingRowsForStreamIsFinished()
{
    preStreamedValueAddress = EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG); // null row flag
    preStreamedPagesIndexPosition = -1;
    preBufferedValueAddress.clear();
    int64_t bufferedValueAddr;
    do {
        bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
        preBufferedValueAddress.emplace_back(bufferedValueAddr);
    } while (AdvancedBufferedJoinKey());
    SavePrevMatchingRowsForFullOuter(preStreamedValueAddress, 0);
}

void SortMergeJoinScanner::StreamMissingRowsForNullBuffered()
{
    preStreamedValueAddress = EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG); // null row flag
    preStreamedPagesIndexPosition = -1;
    preBufferedValueAddress.clear();
    int64_t bufferedValueAddr;
    do {
        bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndexPosition);
        preBufferedValueAddress.emplace_back(bufferedValueAddr);
        preBufferedPagesIndexPosition = bufferedPagesIndexPosition;
        curBufferRowMatchFlag = true;
    } while (AdvancedBufferedJoinKey() && CurBufferedHasNull());
    SavePrevMatchingRowsForFullOuter(preStreamedValueAddress, 0);
}

template <JoinType templateJoinType> int32_t SortMergeJoinScanner::FindNextMatchPos()
{
    auto compare = CompareCurRowKeys();
    auto cur = streamedPagesIndexPosition;
    while ((compare > 0 && AdvancedBufferedToRowWithNullFreeJoinKey()) ||
        (compare < 0 && AdvancedStreamedWithNullFreeJoinKey())) {
        compare = CompareCurRowKeys();
        // cur-stream-key duplicate with pre-key, also save although buffer not match cur-stream-key
        if (compare != 0 && (streamedPagesIndexPosition > cur) && PreKeyMatched()) {
            SavePrevMatchingRows<templateJoinType>(1);
        }
    }
    return compare;
}

bool SortMergeJoinScanner::PreKeyMatched()
{
    if (preStreamedValueAddress == -1 || preBufferedValueAddress.empty()) {
        return false;
    }
    auto curValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    bool isMatched = CompareStreamedRowKeys(preStreamedValueAddress, curValueAddr) == 0;
    return isMatched;
}

bool SortMergeJoinScanner::PreKeyMatchedWithNullValue()
{
    if (preStreamedValueAddress == -1 || preBufferedValueAddress.empty()) {
        return false;
    }
    if (streamedPagesIndex->HaveNull(preStreamedPagesIndexPosition) ||
        streamedPagesIndex->HaveNull(streamedPagesIndexPosition)) {
        // if one of them contain NULL return mismatch
        return false;
    }
    auto curValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    bool isMatched = CompareStreamedRowKeys(preStreamedValueAddress, curValueAddr) == 0;
    return isMatched;
}

// isMatched = 1 means true, 0 means false
template <JoinType templateJoinType> void SortMergeJoinScanner::SavePrevMatchingRows(int8_t isMatched)
{
    auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    auto matchedRowCount = preBufferedValueAddress.size();
    bufferedValueAddress.insert(bufferedValueAddress.end(), preBufferedValueAddress.begin(),
        preBufferedValueAddress.end());
    streamedValueAddress.insert(streamedValueAddress.end(), matchedRowCount, streamedValueAddr);
    isPreKeyMatched.insert(isPreKeyMatched.end(), matchedRowCount, isMatched);
    if constexpr (templateJoinType == OMNI_JOIN_TYPE_LEFT_SEMI || templateJoinType == OMNI_JOIN_TYPE_LEFT_ANTI) {
        isSameBufferedKeyMatched.insert(isSameBufferedKeyMatched.end(), preBufferedKeyMatched.begin(),
            preBufferedKeyMatched.end());
    }
}

// isMatched = 1 means true, 0 means false
void SortMergeJoinScanner::SavePrevMatchingRowsForFullOuter(int64_t inValueAddress, int8_t isMatched)
{
    bufferedValueAddress.insert(bufferedValueAddress.end(), preBufferedValueAddress.begin(),
        preBufferedValueAddress.end());
    streamedValueAddress.insert(streamedValueAddress.end(), preBufferedValueAddress.size(), inValueAddress);
    isPreKeyMatched.insert(isPreKeyMatched.end(), preBufferedValueAddress.size(), isMatched);
}

bool SortMergeJoinScanner::NeedBufferedData()
{
    if (bufferedPagesIndex->IsDataFinish()) {
        return false;
    }
    auto streamedValueAddr = streamedPagesIndex->GetValueAddresses(streamedPagesIndexPosition);
    auto bufferedValueAddr = bufferedPagesIndex->GetValueAddresses(bufferedPagesIndex->GetPositionCount() - 1);
    if (bufferedValueAddr > -1) {
        auto streamedBatchId = DecodeSliceIndex(streamedValueAddr);
        auto streamedRowId = DecodePosition(streamedValueAddr);
        auto bufferedBatchId = DecodeSliceIndex(bufferedValueAddr);
        auto bufferedRowId = DecodePosition(bufferedValueAddr);
        if (CompareRowKeys(streamedBatchId, streamedRowId, bufferedBatchId, bufferedRowId) >= 0) {
            return true;
        }
    }
    return false;
}
}
}
