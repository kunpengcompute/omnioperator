/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: sort merge join v3 core implementations
 */
#ifndef OMNI_RUNTIME_SORT_MERGE_JOIN_V3_H
#define OMNI_RUNTIME_SORT_MERGE_JOIN_V3_H

#include <vector>
#include "operator/join/common_join.h"
#include "operator/pages_index.h"
#include "operator/status.h"
#include "util/debug.h"

namespace omniruntime::op {
using EqualFunc = bool (*)(vec::BaseVector *left, int32_t leftPosition, vec::BaseVector *right, int32_t rightPosition);
using CompareFunc = omniruntime::op::OperatorUtil::CompareFunc;

static int32_t NULL_INDEX = std::numeric_limits<int32_t>::min();

// [start, end)
struct IndexRange {
public:
    IndexRange(int32_t start, int32_t end) : start(start), end(end) {}

    int32_t start;
    int32_t end;
};

class SortMergeJoinOperatorV3 {
public:
    SortMergeJoinOperatorV3(JoinType joinType, const std::string &filter,
        const config::QueryConfig &queryConfig = config::QueryConfig{})
        : joinType(joinType), filter(filter), executionContext(new ExecutionContext())
    {
        executionContext->SetConfig(queryConfig);
    }

    ~SortMergeJoinOperatorV3()
    {
        delete streamPagesIndex;
        delete[] streamJoinColTypes;
        delete[] streamSortAscendings;
        delete[] streamSortNullFirsts;
        delete[] streamOutputColTypeIds;

        delete bufferPagesIndex;
        delete[] bufferJoinColTypes;
        delete[] bufferSortAscendings;
        delete[] bufferSortNullFirsts;
        delete[] bufferOutputColTypeIds;

        delete[] streamJoinRows;
        delete[] streamFilterRows;
        delete[] streamJoinColumns;
        delete[] streamFilterColumns;
        delete[] streamOutputColumns;
        delete[] bufferJoinRows;
        delete[] bufferFilterRows;
        delete[] bufferJoinColumns;
        delete[] bufferFilterColumns;
        delete[] bufferOutputColumns;

        delete executionContext;
        delete simpleFilter;
        delete[] streamFilterCols;
        delete[] streamFilterColTypes;
        delete[] bufferFilterCols;
        delete[] bufferFilterColTypes;
        delete[] values;
        delete[] nulls;
        delete[] lengths;
    }

    void ConfigStreamInfo(const DataTypes &streamTypes, const std::vector<int32_t> &streamJoinCols,
        const std::vector<int32_t> &streamOutputCols, int32_t originalStreamColCount);

    void ConfigBufferInfo(const DataTypes &bufferTypes, const std::vector<int32_t> &bufferJoinCols,
        const std::vector<int32_t> &bufferOutputCols, int32_t originalBufferColCount);

    void JoinFilterCodeGen(OverflowConfig *overflowConfig);

    int32_t AddStreamInput(omniruntime::vec::VectorBatch *vecBatch);

    int32_t AddBufferInput(omniruntime::vec::VectorBatch *vecBatch);

    OmniStatus GetOutput(omniruntime::vec::VectorBatch **outputVecBatch);

    OmniStatus Close()
    {
        return OMNI_STATUS_NORMAL;
    }

    std::vector<type::DataTypePtr> GetOutputTypes()
    {
        return this->outputTypes;
    }

private:
    void Prepare();
    void PrepareForStream();
    void PrepareForBuffer();
    void DivideRanges(vec::BaseVector **joinRows, std::vector<int32_t> &rowIdxes, int32_t joinColCount,
        std::vector<IndexRange> &ranges);
    void DivideRanges(PagesIndex *pagesIndex, BaseVector ***joinColumns, int32_t joinColCount,
        std::vector<IndexRange> &ranges);
    OmniStatus ProbeNullRange();
    OmniStatus ProbeNonNullRanges(bool hasFilter);
    template <JoinType joinType, bool hasJoinFilter> OmniStatus ProbeNonNullRanges();
    template <JoinType joinType, bool hasJoinFilter>
    void HandleMatch(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    template <bool hasJoinFilter> void HandleMatchForInnerJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    template <bool hasJoinFilter> void HandleMatchForLeftJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    template <bool hasJoinFilter> void HandleMatchForRightJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    template <bool hasJoinFilter> void HandleMatchForFullJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    template <bool hasJoinFilter> void HandleMatchForLeftSemiJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    template <bool hasJoinFilter> void HandleMatchForLeftAntiJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses);
    void BuildOutput(vec::VectorBatch **outputVecBatch, int32_t rowCount);
    void ConstructResultColumns(bool isStream, VectorBatch *output, int32_t rowCount);

    int32_t CompareJoinKeys()
    {
        int32_t result = 0;
        for (int32_t colIdx = 0; colIdx < bufferJoinColCount; colIdx++) {
            result = compareFuncs[colIdx](curStreamRow[colIdx], curStreamRowIdx, curBufferRow[colIdx], curBufferRowIdx);
            if (result != 0) {
                break;
            }
        }
        return result;
    }

    bool IsFull()
    {
        return probeRowCount >= maxRowCount;
    }

    void AppendStreamValuesAndBufferNulls(uint32_t rangeIdx)
    {
        auto &streamNullRange = streamRanges[rangeIdx];
        auto streamNullRangeStart = streamNullRange.start;
        auto streamNullRangeEnd = streamNullRange.end;
        auto streamNullCount = streamNullRangeEnd - streamNullRangeStart;
        if (streamNullCount > 0) {
            for (int32_t i = streamNullRangeStart; i < streamNullRangeEnd; i++) {
                streamIndexes.emplace_back(i);
            }
            bufferIndexes.insert(bufferIndexes.end(), streamNullCount, NULL_INDEX);
            probeRowCount += streamNullCount;
        }
    }

    void AppendStreamValues(uint32_t rangeIdx)
    {
        auto &streamNullRange = streamRanges[rangeIdx];
        auto streamNullRangeStart = streamNullRange.start;
        auto streamNullRangeEnd = streamNullRange.end;
        auto streamValueCount = streamNullRangeEnd - streamNullRangeStart;
        for (int32_t i = streamNullRangeStart; i < streamNullRangeEnd; i++) {
            streamIndexes.emplace_back(i);
        }
        probeRowCount += streamValueCount;
    }

    void AppendBufferValuesAndStreamNulls(uint32_t rangeIdx)
    {
        auto &bufferNullRange = bufferRanges[rangeIdx];
        auto bufferNullRangeStart = bufferNullRange.start;
        auto bufferNullRangeEnd = bufferNullRange.end;
        auto bufferNullCount = bufferNullRangeEnd - bufferNullRangeStart;
        if (bufferNullCount > 0) {
            for (int32_t i = bufferNullRangeStart; i < bufferNullRangeEnd; i++) {
                bufferIndexes.emplace_back(i);
            }
            streamIndexes.insert(streamIndexes.end(), bufferNullCount, NULL_INDEX);
            probeRowCount += bufferNullCount;
        }
    }

    void UpdateStreamRangeCursor()
    {
        curPosInStreamRange = streamRanges[curStreamRangeIdx].start;
        curStreamRowIdx = streamRowIdxes[curPosInStreamRange];
        curStreamRow = streamJoinRows + curPosInStreamRange * streamJoinColCount;
    }

    void UpdateStreamRangeCursor(int32_t curPos)
    {
        curStreamRowIdx = streamRowIdxes[curPos];
        curStreamRow = streamJoinRows + curPos * streamJoinColCount;
    }

    void UpdateBufferRangeCursor()
    {
        curPosInBufferRange = bufferRanges[curBufferRangeIdx].start;
        curBufferRowIdx = bufferRowIdxes[curPosInBufferRange];
        curBufferRow = bufferJoinRows + curPosInBufferRange * bufferJoinColCount;
    }

    void CopyPositionStreamRow(vec::BaseVector **streamRow, uint32_t streamRowIdx)
    {
        for (int32_t i = 0; i < streamFilterColCount; i++) {
            auto colIdx = streamFilterCols[i];
            auto streamVec = streamRow[i];
            nulls[colIdx] = streamVec->IsNull(streamRowIdx);
            values[colIdx] = OperatorUtil::GetValuePtrAndLengthFromRawVector(streamVec, streamRowIdx, lengths + colIdx,
                streamFilterColTypes[i]);
        }
    }

    void CopyPositionBufferRow(vec::BaseVector **bufferRow, uint32_t bufferRowIdx)
    {
        for (int32_t i = 0; i < bufferFilterColCount; i++) {
            auto colIdx = bufferFilterCols[i];
            auto bufferVec = bufferRow[i];
            nulls[colIdx] = bufferVec->IsNull(bufferRowIdx);
            values[colIdx] = OperatorUtil::GetValuePtrAndLengthFromRawVector(bufferVec, bufferRowIdx, lengths + colIdx,
                bufferFilterColTypes[i]);
        }
    }

    bool IsJoinPositionEligible()
    {
        return simpleFilter->Evaluate(values, nulls, lengths, reinterpret_cast<int64_t>(executionContext));
    }

    type::DataTypes streamTypes;
    std::vector<int32_t> streamJoinCols;
    std::vector<int32_t> streamOutputCols;
    int32_t originalStreamColCount;
    PagesIndex *streamPagesIndex = nullptr;
    int32_t streamJoinColCount;
    int32_t *streamJoinColTypes = nullptr;
    int32_t *streamSortAscendings = nullptr;
    int32_t *streamSortNullFirsts = nullptr;
    int32_t streamOutputColCount;
    int32_t *streamOutputColTypeIds = nullptr;
    vec::BaseVector ***streamJoinColumns = nullptr;
    vec::BaseVector ***streamOutputColumns = nullptr;
    vec::BaseVector **streamJoinRows = nullptr;
    vec::BaseVector **streamFilterRows = nullptr;
    std::vector<int32_t> streamRowIdxes;

    type::DataTypes bufferTypes;
    std::vector<int32_t> bufferJoinCols;
    std::vector<int32_t> bufferOutputCols;
    int32_t originalBufferColCount;
    PagesIndex *bufferPagesIndex = nullptr;
    int32_t bufferJoinColCount;
    int32_t *bufferJoinColTypes = nullptr;
    int32_t *bufferSortAscendings = nullptr;
    int32_t *bufferSortNullFirsts = nullptr;
    int32_t bufferOutputColCount;
    int32_t *bufferOutputColTypeIds = nullptr;
    vec::BaseVector ***bufferJoinColumns = nullptr;
    vec::BaseVector ***bufferOutputColumns = nullptr;
    vec::BaseVector **bufferJoinRows = nullptr;
    vec::BaseVector **bufferFilterRows = nullptr;
    std::vector<int32_t> bufferRowIdxes;

    // for join probe
    std::vector<IndexRange> streamRanges;
    int32_t preStreamRangeIdx = -1;
    int32_t curStreamRangeIdx = 0;
    int32_t curPosInStreamRange;
    int32_t curStreamVecBatchIdx;
    int32_t curStreamRowIdx;
    vec::BaseVector **curStreamRow = nullptr;
    std::vector<IndexRange> bufferRanges;
    int32_t preBufferRangeIdx = -1;
    int32_t curBufferRangeIdx = 0;
    int32_t curPosInBufferRange;
    int32_t curBufferVecBatchIdx;
    int32_t curBufferRowIdx;
    vec::BaseVector **curBufferRow = nullptr;

    std::vector<type::DataTypePtr> outputTypes;
    JoinType joinType;
    std::vector<EqualFunc> equalFuncs;
    std::vector<CompareFunc> compareFuncs;

    std::vector<int32_t> streamIndexes;
    std::vector<int32_t> bufferIndexes;
    int32_t maxRowCount = 0;
    int32_t probeRowCount = 0;
    int32_t probeOffset = 0;

    // for join filter
    std::string filter;
    ExecutionContext *executionContext = nullptr;
    SimpleFilter *simpleFilter = nullptr;
    int32_t *streamFilterCols = nullptr;
    int32_t *streamFilterColTypes = nullptr;
    int32_t *bufferFilterCols = nullptr;
    int32_t *bufferFilterColTypes = nullptr;
    int32_t streamFilterColCount = 0;
    int32_t bufferFilterColCount = 0;
    vec::BaseVector ***streamFilterColumns = nullptr;
    vec::BaseVector ***bufferFilterColumns = nullptr;
    int64_t *values = nullptr;
    bool *nulls = nullptr;
    int32_t *lengths = nullptr;

    bool isFirst = true;
    bool skipNullRange = false;

    OmniStatus status = OMNI_STATUS_NORMAL;
};
}

#endif // OMNI_RUNTIME_SORT_MERGE_JOIN_V3_H
