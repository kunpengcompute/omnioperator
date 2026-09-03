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

    bool CanFreeRightBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId);

    VectorBatch *NewEmptyVectorBatch(int32_t rowCount) const;

    void EnsureBuildVectorBatchCapacity(int32_t rowCount);

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

    void SetRightNullRangeForColumn(BaseVector *vector, const DataTypePtr &dataType, int32_t startRow, int32_t endRow);
};
}
}
#endif // __SORT_MERGE_JOIN_RESULT_BUILDER_H__
