/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: join result builder implementations
 */

#ifndef __SORT_MERGE_JOIN_RESULT_BUILDER_H__
#define __SORT_MERGE_JOIN_RESULT_BUILDER_H__

#include <string>
#include "dynamic_pages_index.h"
#include "vector/vector.h"
#include "type/data_types.h"
#include "expression/expressions.h"
#include "operator/filter/filter_and_project.h"
#include "operator/join/common_join.h"

namespace omniruntime {
namespace op {
class JoinResultBuilder {
public:
    JoinResultBuilder(const std::vector<DataTypePtr>& leftTableOutputTypes, int32_t* leftTableOutputCols,
        int32_t leftTableOutputColsCount, int32_t originalLeftTableColsCount, DynamicPagesIndex* leftTablePagesIndex,
        const std::vector<DataTypePtr>& rightTableOutputTypes, int32_t* rightTableOutputCols,
        int32_t rightTableOutputColsCount, int32_t originalRightTableColsCount, DynamicPagesIndex* rightTablePagesIndex,
        std::string& filter, JoinType joinType, OverflowConfig* overflowConfig,
        const config::QueryConfig &queryConfig = config::QueryConfig{});
    JoinResultBuilder(const std::vector<DataTypePtr>& leftTableOutputTypes, int32_t* leftTableOutputCols,
        int32_t leftTableOutputColsCount, int32_t originalLeftTableColsCount, DynamicPagesIndex* leftTablePagesIndex,
        const std::vector<DataTypePtr>& rightTableOutputTypes, int32_t* rightTableOutputCols,
        int32_t rightTableOutputColsCount, int32_t originalRightTableColsCount, DynamicPagesIndex* rightTablePagesIndex,
        Expr* filter, JoinType joinType, OverflowConfig* overflowConfig,
        const config::QueryConfig &queryConfig = config::QueryConfig{});

    void ParsingAndOrganizationResultsForLeftTable(int32_t leftBatchId, int32_t leftRowId);

    void ParsingAndOrganizationResultsForRightTable(int32_t rightBatchId, int32_t rightRowId);

    void PaddingLeftTableNull(int32_t rightBatchId, int32_t rightRowId);

    void PaddingRightTableNull(int32_t leftBatchId, int32_t leftRowId);

    int32_t AddJoinValueAddresses();

    int32_t ConstructInnerJoinOutput();

    int32_t ConstructLeftJoinOutput();

    int32_t ConstructFullJoinOutput();

    int32_t ConstructLeftSemiJoinOutput();

    int32_t ConstructLeftAntiJoinOutput();

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch);

    void Finish();

    ALWAYS_INLINE std::vector<int8_t> &GetPreKeyMatched()
    {
        return isPreKeyMatched;
    }

    ALWAYS_INLINE std::vector<int32_t> &GetStartBufferedBatchIds()
    {
        return startBufferedBatchIds;
    }

    ALWAYS_INLINE std::vector<int64_t> &GetBufferedTableValueAddresses()
    {
        return bufferedTableValueAddresses;
    }

    ALWAYS_INLINE std::vector<int64_t> &GetStreamedTableValueAddresses()
    {
        return streamedTableValueAddresses;
    }

    ALWAYS_INLINE std::vector<int8_t> &GetSameBufferedKeyMatched()
    {
        return isSameBufferedKeyMatched;
    }

    // Called by the operator after the scanner produced the current address window
    // and before materialization: the scanner's pending pre-row key comparison still
    // reads this streamed batch, so the builder's streamed watermark must never free
    // it. INT32_MAX means nothing is pinned.
    ALWAYS_INLINE void SetStreamedPinBatchId(int32_t batchId)
    {
        streamedPinBatchId = batchId;
    }

    // Live-reference floor for the streamed watermark. During a duplicated-key
    // matched group the scanner re-reads the group's FIRST streamed batch for key
    // comparisons on every FindNextJoinRows call, while addresses of the whole group
    // are consumed by the builder over several output windows. The per-window
    // streamed pin only snapshots the latest pre-row, so the group head batch would
    // lose protection after the first window. Tracking the minimum decoded batch id
    // over every address CONSUMED so far (published by GetMatchedValueAddresses and
    // already walked by AddJoinValueAddresses) pins the whole group head: a group
    // spans batches [head..tail] only if its addresses do, so min consumed batch is
    // always <= any batch the scanner may still compare against. Reset together with
    // the address arrays in Clear().
    ALWAYS_INLINE void TrackConsumedStreamedBatch(int32_t batchId)
    {
        if (batchId >= 0 && batchId < minConsumedStreamedBatchId) {
            minConsumedStreamedBatchId = batchId;
        }
    }

    ALWAYS_INLINE int32_t GetMinConsumedStreamedBatchId() const
    {
        return minConsumedStreamedBatchId;
    }

    ALWAYS_INLINE bool HasNext()
    {
        return buildVectorBatchRowCount != 0;
    }

    ALWAYS_INLINE void Clear()
    {
        if (buildVectorBatch != nullptr) {
            VectorHelper::FreeVecBatch(buildVectorBatch);
            buildVectorBatch = nullptr;
        }
        buildVectorBatchRowCount = 0;
        addressOffset = 0;
        isPreRowMatched = false;
        isPreKeyMatched.clear();
        bufferedTableValueAddresses.clear();
        streamedTableValueAddresses.clear();
        isSameBufferedKeyMatched.clear();
        leftOutputBatchIds.clear();
        leftOutputRowIds.clear();
        rightOutputBatchIds.clear();
        rightOutputRowIds.clear();
        minConsumedStreamedBatchId = INT32_MAX;
    }

    ~JoinResultBuilder();

    bool IsJoinPositionEligible(int32_t leftBatchId, int32_t leftRowId, int32_t rightBatchId, int32_t rightRowId) const;
    int32_t CollectRowsInfo(std::vector<std::pair<int32_t, int32_t>> &leftMeta,
        std::vector<std::pair<int32_t, int32_t>> &rightMeta,
        std::vector<bool> &canFreeRightBatches, int32_t inputSize, int32_t &counter);

    ALWAYS_INLINE bool NeedDoFilter() const
    {
        return simpleFilter != nullptr;
    }

    void GroupByMeta(std::vector<std::pair<int32_t, std::vector<int32_t>>> &output,
        const std::vector<std::pair<int32_t, int32_t>> &meta)
    {
        auto metaSize = meta.size();
        if (metaSize == 0) {
            return;
        }
        auto prevBatchId = meta[0].first;
        std::vector<int32_t> rows{meta[0].second};

        for (size_t i = 1; i < metaSize; i++) {
            auto &tmpPair = meta[i];
            auto batchId = tmpPair.first;
            auto rowId = tmpPair.second;
            if (prevBatchId != batchId) {
                output.emplace_back(std::make_pair(prevBatchId, rows));
                rows.clear();
                prevBatchId = batchId;
            }
            rows.emplace_back(rowId);
        }

        output.emplace_back(std::make_pair(prevBatchId, rows));
    }

private:
    struct LeftAntiJoinHandler {
        bool hasSameBufferedRow = false;
        bool printThisStreamRowOutFlag = true; // default this row should be out
    };

    void JoinFilterCodeGen(OverflowConfig *overflowConfig);
    void JoinFilterExprCodeGen(OverflowConfig *overflowConfig);

    void FreeVectorBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId);

    // Buffered-side half of FreeVectorBatches with the identical fresh-key gate;
    // used by the left-join path whose streamed side frees via the pin watermark.
    void FreeRightVectorBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId);

    bool CanFreeRightBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId);

    VectorBatch *NewEmptyVectorBatch(int32_t rowCount) const;

    void EnsureBuildVectorBatchCapacity(int32_t rowCount);

    void CopyOutputSegment(BaseVector *inputVector, const DataTypePtr &inputType, const int32_t *rows,
        int32_t rowCount, BaseVector *outputVector, int32_t outputRowId);

    void CopyOutputSelection(const std::vector<int32_t> &batchIds, const std::vector<int32_t> &rowIds,
        DynamicPagesIndex *pagesIndex, const std::vector<DataTypePtr> &outputTypes, const int32_t *outputCols,
        int32_t outputColsCount, int32_t columnOffset, int32_t rowBase);

    // Build a LEFT JOIN output batch without copying the streamed payload when the
    // whole output window is one consecutive, right-null-padded run from a single
    // flat source batch. All other shapes retain the materializing path.
    bool TryBuildLeftUnmatchedSliceOutput(int32_t rowBase, bool batchFull);

    static bool CanSliceLeftOutputVector(BaseVector *vector, const DataTypePtr &dataType);

    void UpdateLeftAntiJoinHandler(int32_t addressPosition, std::vector<int8_t> &isSameBufferedKeyMatched,
        int32_t inputSize);

    ALWAYS_INLINE void PaddingLeftTableNull()
    {
        for (int columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
            auto vector = buildVectorBatch->Get(columnIdx);
            auto typeId = leftTableOutputTypes[columnIdx]->GetId();
            if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetNull(buildRowCount);
            } else {
                vector->SetNull(buildRowCount);
            }
        }
    }

    ALWAYS_INLINE void PaddingRightTableNull()
    {
        for (int columnIdx = 0; columnIdx < rightTableOutputColsCount; columnIdx++) {
            int32_t buildColumnIdx = leftTableOutputColsCount + columnIdx;
            auto vector = buildVectorBatch->Get(buildColumnIdx);
            auto typeId = rightTableOutputTypes[columnIdx]->GetId();
            if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetNull(buildRowCount);
            } else {
                vector->SetNull(buildRowCount);
            }
        }
    }
    std::vector<DataTypePtr> leftTableOutputTypes;
    int32_t *leftTableOutputCols;
    int32_t leftTableOutputColsCount;
    int32_t originalLeftTableColsCount;
    DynamicPagesIndex *leftTablePagesIndex;
    std::vector<DataTypePtr> rightTableOutputTypes;
    int32_t *rightTableOutputCols;
    int32_t rightTableOutputColsCount;
    int32_t originalRightTableColsCount;
    DynamicPagesIndex *rightTablePagesIndex;
    std::string filterExpStr;
    Expr* filterExpr = nullptr;

    int32_t lastUnMatchedStreamedBatchId = -1;
    int32_t lastUnMatchedBufferedBatchId = -1;
    // Streamed batch the scanner still reads via its pending pre-row state; set
    // by the operator each cycle before materialization (see SetStreamedPinBatchId).
    int32_t streamedPinBatchId = INT32_MAX;
    // Minimum streamed batch id over all consumed addresses (see TrackConsumedStreamedBatch).
    int32_t minConsumedStreamedBatchId = INT32_MAX;

    bool isFinished = false;
    int32_t maxRowCount;

    int64_t preStreamedRowAddress = INT64_MAX;
    int32_t preStreamedBatchId = INT32_MAX;
    int32_t preStreamedRowId = INT32_MAX;
    bool preLeftTableRowMatchedOut = false; // current left row matched out or pad null

    int32_t buildVectorBatchRowCount = 0;
    VectorBatch *buildVectorBatch = nullptr;

    ExecutionContext *executionContext = nullptr;
    SimpleFilter *simpleFilter = nullptr;
    int32_t *streamedColsInFilter = nullptr;
    int32_t *bufferedColsInFilter = nullptr;
    int32_t streamedColsCountInFilter = 0;
    int32_t bufferedColsCountInFilter = 0;
    int64_t *values = nullptr;
    bool *nulls = nullptr;
    int32_t *lengths = nullptr;
    JoinType joinType;
    // Scratch reused across segments and columns of one builder. Every slot is
    // rewritten before SetValues reads it, so stale nulls cannot leak between columns.
    std::vector<std::string_view> batchVarcharValues;
    std::vector<uint8_t> batchVarcharNulls;

    bool isPreRowMatched = false;
    int32_t addressOffset = 0;
    int32_t buildRowCount = 0;
    std::vector<int8_t> isPreKeyMatched;
    std::vector<int32_t> startBufferedBatchIds;
    // store the matched valueAddress
    std::vector<int64_t> streamedTableValueAddresses;
    std::vector<int64_t> bufferedTableValueAddresses;
    std::vector<int8_t> isSameBufferedKeyMatched;
    LeftAntiJoinHandler leftAntiJoinHandler;
    std::vector<DataTypePtr> allTypes;

    // Two-phase left/full join: phase-1 output indices (batchId, rowId) per output row
    std::vector<int32_t> leftOutputBatchIds;
    std::vector<int32_t> leftOutputRowIds;
    std::vector<int32_t> rightOutputBatchIds;
    std::vector<int32_t> rightOutputRowIds;

    void SetRightNullRangeForColumn(BaseVector *vector, const DataTypePtr &dataType, int32_t startRow,
        int32_t endRow);

    // Gathers one flat VARCHAR segment into the scratch views and writes it with a
    // single SetValues. Returns false (output untouched) for non-flat source encodings.
    bool TryBatchVarcharSegment(BaseVector *inputVector, BaseVector *outputVector, const int32_t *rows,
        int32_t rowCount, int32_t outputRowId);
};
}
}
#endif // __SORT_MERGE_JOIN_RESULT_BUILDER_H__
