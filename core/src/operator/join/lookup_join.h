/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * @Description: lookup join implementations
 */
#ifndef __LOOKUP_JOIN_H__
#define __LOOKUP_JOIN_H__

#include <memory>
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/filter/filter_and_project.h"
#include "operator/join/taper_join_hash_table_variants.h"
#include "operator/hashmap/row_container.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "hash_builder.h"
#include "common_join.h"

namespace omniruntime {
namespace op {
class LookupJoinOutputBuilder {
public:
    LookupJoinOutputBuilder(std::vector<int32_t> &probeOutputCols, const int32_t *probeOutputTypes,
        std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, int32_t outputRowSize);
    LookupJoinOutputBuilder(std::vector<int32_t> &probeOutputCols, const int32_t *probeOutputTypes,
                            std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, int32_t outputRowSize,
                            std::vector<int32_t> &outputList);
    ~LookupJoinOutputBuilder() = default;
    void AppendRow(int32_t probePosition, BaseVector ***array, uint64_t address);
    void BuildOutput(BaseVector **probeOutputColumns, JoinType joinType,
                     bool isShuffleExchangeBuildPlan, VectorBatch **outputVecBatch, BuildSide buildSide);
    void ConstructProbeColumns(VectorBatch *vectorBatch, BaseVector **probeAllColumns, int32_t rowCount);
    template <bool isInnerJoin, bool isShuffleExchangeBuildPlan>
    bool ConstructBuildColumnWithTaper(VectorBatch *vectorBatch, int32_t rowCount);
    template <bool isInnerJoin, bool isShuffleExchangeBuildPlan>
    void ConstructBuildColumns(VectorBatch *vectorBatch, int32_t rowCount);
    template<bool isMatched> void AppendExistenceRow(int32_t probePosition);
    void ConstructExistenceColumn(VectorBatch *vectorBatch);
    void PadNullProbeRows(VectorBatch* vectorBatch, int32_t nullRowCount);

    ALWAYS_INLINE bool IsFull()
    {
        return probeRowCount >= maxRowCount;
    }

    ALWAYS_INLINE bool WillFull(int32_t rowCount)
    {
        return probeRowCount + rowCount >= maxRowCount;
    }

    ALWAYS_INLINE bool IsEmpty()
    {
        return probeRowCount <= 0;
    }

    ALWAYS_INLINE void Clear()
    {
        probeRowOffset = 0;
        probeRowCount = 0;
        probePositions.clear();
        buildRefs.clear();
        buildArrays.clear();
        existJoinBuildIndex.clear();
#ifdef OMNI_USE_TAPER_JOIN
        taperRC_ = nullptr;
        taperStoredColIndices_.clear();
        taperNeedsUnvisited_ = false;
#endif
    }

#ifdef OMNI_USE_TAPER_JOIN
    void AppendRowTaper(int32_t probePosition, omniruntime::vec::BaseVector*** array, uint64_t address, char* rowPtr);
    void AppendRowsTaper(int32_t probePosition, char* const* rows, int32_t count);
    void AppendRowsTaperBatched(const int32_t* positions, char* const* rows, int32_t count);
    void SetTaperOutput(const omniruntime::op::RowContainer* rc, const std::vector<int32_t>& storedCols);
    ALWAYS_INLINE void SetTaperNeedsUnvisited() { taperNeedsUnvisited_ = true; }
#endif

    static const uint32_t SHIFT_SIZE_32 = 32;
    static ALWAYS_INLINE uint64_t EncodeAddress(uint32_t rowId, uint32_t vectorBatchId)
    {
        return (static_cast<uint64_t>(rowId) << SHIFT_SIZE_32) | vectorBatchId;
    }

    static ALWAYS_INLINE uint32_t DecodeRowId(uint64_t sliceAddress)
    {
        return static_cast<uint32_t>(sliceAddress >> SHIFT_SIZE_32);
    }

    static ALWAYS_INLINE uint32_t DecodeVectorBatchId(uint64_t sliceAddress)
    {
        return static_cast<uint32_t>(sliceAddress);
    }

private:
    ALWAYS_INLINE void ConstructProbeColumnsFromPositions(VectorBatch *vectorBatch, BaseVector **probeOutputColumns,
        int32_t rowCount)
    {
        auto probeOutputColsCount = probeOutputCols.size();
        auto tempIndex = probePositions.data() + probeRowOffset;
        int32_t probePositionsTmp[rowCount];
        for (int32_t i = 0; i < rowCount; ++i) {
            probePositionsTmp[i] = tempIndex[i];
        }
        for (size_t j = 0; j < probeOutputColsCount; ++j) {
            auto column = probeOutputColumns[j];
            auto type = probeOutputTypes[j];
            // we want to keep only one level dictionary vector here
            // if the data is non-dictionary, we build dictionary to avoid data copy
            BaseVector *probeColumn = nullptr;
            if (column->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                probeColumn = VectorHelper::SliceVector(column, 0, rowCount);
            } else if (column->GetEncoding() == vec::OMNI_ENCODING_ARRAY || column->GetEncoding() == vec::OMNI_ENCODING_STRUCT) {
                probeColumn = column->CopyPositions(probePositionsTmp, 0, rowCount);
            } else if (column->GetEncoding() == vec::OMNI_DICTIONARY) {
                probeColumn = VectorHelper::CopyPositionsVector(column, probePositionsTmp, 0, rowCount);
            } else if (column->GetEncoding() == vec::OMNI_ENCODING_MAP) {
                probeColumn = column->CopyPositions(probePositionsTmp, 0, rowCount);
            } else {
                probeColumn = VectorHelper::CreateDictionaryVector(probePositionsTmp, rowCount, column, type);
            }
            vectorBatch->Append(probeColumn);
        }
    }

    ALWAYS_INLINE void ConstructProbeColumnsFromReuse(VectorBatch *vectorBatch, BaseVector **probeOutputColumns,
        int32_t rowCount)
    {
        auto probeOutputColsCount = probeOutputCols.size();
        for (size_t j = 0; j < probeOutputColsCount; ++j) {
            auto column = probeOutputColumns[j];
            auto resultColumn = VectorHelper::SliceVector(column, 0, rowCount);
            vectorBatch->Append(resultColumn);
        }
    }

    ALWAYS_INLINE void ConstructProbeColumnsFromSlice(VectorBatch *vectorBatch, BaseVector **probeOutputColumns,
        int32_t rowCount)
    {
        auto probeOutputColsCount = probeOutputCols.size();
        auto offset = probePositions[probeRowOffset];
        for (size_t j = 0; j < probeOutputColsCount; ++j) {
            auto column = probeOutputColumns[j];
            auto resultColumn = VectorHelper::SliceVector(column, offset, rowCount);
            vectorBatch->Append(resultColumn);
        }
    }

    int32_t maxRowCount = 0;
    std::vector<int32_t> probeOutputCols;
    const int32_t *probeOutputTypes;
    std::vector<int32_t> buildOutputCols;
    std::vector<int32_t> outputList;
    DataTypes buildOutputTypes;
    int32_t probeRowCount = 0;
    int32_t probeRowOffset = 0;
    std::vector<int32_t> probePositions;
    std::vector<uint64_t> buildRefs;
    std::vector<BaseVector ***> buildArrays;
    std::vector<bool> existJoinBuildIndex;
#ifdef OMNI_USE_TAPER_JOIN
    const RowContainer* taperRC_ = nullptr;
    std::vector<int32_t> taperStoredColIndices_;
    bool taperNeedsUnvisited_ = false;
#endif
};

class LookupJoinOperatorFactory : public OperatorFactory {
public:
    LookupJoinOperatorFactory(const type::DataTypes &probeTypes, int32_t *probeOutputCols, int32_t probeOutputColsCount,
        int32_t *probeHashCols, int32_t probeHashColsCount, int32_t *buildOutputCols, int32_t buildOutputColsCount,
        const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables,
        omniruntime::expressions::Expr *filterExpr, bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig);
    LookupJoinOperatorFactory(const type::DataTypes &probeTypes, int32_t *probeOutputCols, int32_t probeOutputColsCount,
        int32_t *probeHashCols, int32_t probeHashColsCount, int32_t *buildOutputCols, int32_t buildOutputColsCount,
        const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables,
        omniruntime::expressions::Expr *filterExpr, int32_t originalProbeColsCount,
        bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig, int32_t *outputList = nullptr);
    ~LookupJoinOperatorFactory() override;
    static LookupJoinOperatorFactory *CreateLookupJoinOperatorFactory(const DataTypes &probeTypes,
        int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *probeHashCols, int32_t probeHashColsCount,
        int32_t *buildOutputCols, int32_t buildOutputColsCount, const DataTypes &buildOutputTypes,
        int64_t hashBuilderFactoryAddr, omniruntime::expressions::Expr *filterExpr,
        bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig);
    // this is only for LookupJoinWithExprOperator
    static LookupJoinOperatorFactory *CreateLookupJoinOperatorFactory(const DataTypes &probeTypes,
        int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *probeHashCols, int32_t probeHashColsCount,
        int32_t *buildOutputCols, int32_t buildOutputColsCount, const DataTypes &buildOutputTypes,
        int64_t hashBuilderFactoryAddr, omniruntime::expressions::Expr *filterExpr, int32_t originalProbeColsCount,
        bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig, int32_t *outputList = nullptr);
    static LookupJoinOperatorFactory *CreateLookupJoinOperatorFactory(std::shared_ptr<const HashJoinNode> planNode,
        HashBuilderOperatorFactory* hashBuilderOperatorFactory, const config::QueryConfig &queryConfig);
    Operator *CreateOperator() override;

private:
    void CommonInitActions(const type::DataTypes &probeTypes, int32_t *probeOutputCols, int32_t probeOutputColsCount,
        int32_t *probeHashCols, int32_t probeHashColsCount, int32_t *buildOutputCols, int32_t buildOutputColsCount,
        const type::DataTypes &buildOutputTypes, int32_t *outputList = nullptr);
    void JoinFilterCodeGen(Expr *filterExpr, OverflowConfig *overflowConfig);

    DataTypes probeTypes;                 // all types for probe
    std::vector<int32_t> probeOutputCols; // output columns for probe
    std::vector<int32_t> probeHashCols;   // join columns for probe
    std::vector<int32_t> probeHashColTypes;
    std::vector<int32_t> buildOutputCols; // output columns for build
    std::vector<int32_t> outputList;
    DataTypes buildOutputTypes;           // output column types for build
    HashTableVariants *hashTables;
    bool isShuffleExchangeBuildPlan;
    int32_t rowSize; // estimation of rowSize
    SimpleFilter *simpleFilter = nullptr;
    // this is for lookup join with expression operator when join key and join filter both are expressions
    int32_t originalProbeColsCount;
};

class LookupJoinOperator : public Operator {
public:
    LookupJoinOperator(const type::DataTypes &probeTypes, std::vector<int32_t> &probeOutputCols,
        std::vector<int32_t> &probeHashCols, std::vector<int32_t> &probeHashColTypes,
        std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables,
        SimpleFilter *simpleFilter, int32_t originalProbeColsCount,
        int32_t outputRowSize, bool isShuffleExchangeBuildPlan);

    LookupJoinOperator(const type::DataTypes &probeTypes, std::vector<int32_t> &probeOutputCols,
                       std::vector<int32_t> &probeHashCols, std::vector<int32_t> &probeHashColTypes,
                       std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables,
                       SimpleFilter *simpleFilter, int32_t originalProbeColsCount,
                       int32_t outputRowSize, bool isShuffleExchangeBuildPlan, std::vector<int32_t> &outputList);
    ~LookupJoinOperator() override;
    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;
    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;
    OmniStatus Close() override;
    BlockingReason IsBlocked(ContinueFuture* future) override;

private:
    void InitFirst();

    template<typename T, bool hasJoinFilter, JoinType joinType, bool hasNull>
    void ArrayJoinProbeSIMDNeon(BaseVector ***buildColumns, size_t probeHashColsCount,
                                                    T &&arg, ExecutionContext *contextPtr);

    template <typename T, bool hasJoinFilter, JoinType joinType>
    void DealWithProbeMatchResult(int64_t *matchRowsData, int64_t *matchSlotsData, int64_t *noMatchRowsData,
                                  int32_t matchRowCnt, int32_t noMatchRowCnt, T&& arg, ExecutionContext* contextPtr,
                                  BaseVector*** buildColumns);

    template<typename T, typename KeyType, bool hasJoinFilter, JoinType joinType, bool hasNull>
    void DealWithProbeMatchResultNeon(KeyType *hashes, KeyType *matches, int32_t len, int32_t rowStart,
                                      const int64_t *slots, const bool *isAssigned,
                                      T &&arg, ExecutionContext *contextPtr, BaseVector ***buildColumns);

    template<typename T, bool hasJoinFilter, JoinType joinType, bool hasNull>
    void ArrayJoinProbe(BaseVector ***buildColumns, size_t probeHashColsCount,
                        T &&arg, ExecutionContext *contextPtr);
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForInnerJoin();
    template <bool hasJoinFilter, JoinType joinType> void TaperArrayJoinProbeSIMD();
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForOppositeSideOuterJoin();
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForSameSideOuterJoin();
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForFullJoin();
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForLeftSemiJoin();
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForLeftAntiJoin();
    template <bool hasJoinFilter, bool singleHT> void ProbeBatchForExistenceJoin();

    bool IsTaperTable();

    bool EvaluateBuildFilter(char* row, const RowContainer* rc, ExecutionContext* contextPtr);

    template <bool singleHT, typename Variant>
    void TaperFindBatch(Variant& varg, int32_t inputRowCount);
    template <bool hasJoinFilter> void ProbeJoinPosition(int32_t probePosition);
    bool BuildJoinPosition(uint32_t partition, uint32_t buildRowIdx,
                           uint32_t buildBatchIdx, ExecutionContext *contextPtr);
    void PrepareCurrentProbe();
    void PrepareSerializers();
    void PopulateProbeHashes();
    void PopulateProbeNulls();
    void ProcessProbe();

    template <bool hasJoinFilter>
    ALWAYS_INLINE void InitForProbe(uint32_t partition, bool initSize = true)
    {
        if constexpr (hasJoinFilter) {
            if (initSize) {
                probeFilterColsSize = probeFilterCols.size();
                buildFilterColsSize = buildFilterCols.size();
            }
            buildFilterColPtrs = GetBuildFilterColPtrs(partition);
        }
    }

    ALWAYS_INLINE std::vector<BaseVector **> *GetBuildFilterColPtrs(uint32_t partition)
    {
        return &tableBuildFilterColPtrs[partition];
    }

    void PushBackProbeSerializer(VectorSerializerIgnoreNull &serializer)
    {
        probeSerializers.push_back(serializer);
    }

    DataTypes probeTypes;
    std::vector<int32_t> probeOutputCols;
    std::vector<int32_t> probeHashCols;
    std::vector<int32_t> probeHashColTypes;
    std::vector<int32_t> buildOutputCols;
    DataTypes probeOutputTypes;
    DataTypes buildOutputTypes;
    HashTableVariants *hashTables;

    omniruntime::vec::BaseVector **probeHashColumns = nullptr; // Vector *[join column count]
    omniruntime::vec::BaseVector **probeOutputColumns = nullptr;
    std::vector<int64_t> curProbeHashes;
    std::vector<int8_t> curProbeNulls;

    std::unique_ptr<LookupJoinOutputBuilder> outputBuilder;
    omniruntime::vec::VectorBatch *curInputBatch = nullptr;
    int32_t curProbePosition = 0;
    JoinType joinType;
    BuildSide buildSide;
    uint32_t partitionMask = 0;
    bool isSingleHT;
    bool probeSimd;
    bool isShuffleExchangeBuildPlan;

    // this is for join filter
    SimpleFilter *simpleFilter = nullptr;
    int32_t originalProbeColsCount;
    std::vector<int32_t> probeFilterCols;
    std::vector<int32_t> buildFilterCols;
    int32_t *probeFilterTypeIds = nullptr;
    int32_t *buildFilterTypeIds = nullptr;
    omniruntime::vec::BaseVector **probeFilterColumns = nullptr;
    int64_t *values = nullptr;
    bool *nulls = nullptr;
    int32_t *lengths = nullptr;
    std::vector<std::vector<BaseVector **>> tableBuildFilterColPtrs;
    bool firstVecBatch = true;
    std::vector<VectorSerializerIgnoreNull> probeSerializers;
    std::vector<BaseVector **> *buildFilterColPtrs = nullptr;
    size_t probeFilterColsSize = 0;
    size_t buildFilterColsSize = 0;
#ifdef OMNI_USE_TAPER_JOIN
    std::vector<omniruntime::vec::BaseVector**> taperRowPtrs;
    omniruntime::op::RowContainer* taperRC_ = nullptr;
    std::vector<int32_t> taperStoredColIndices_;
    bool taperNeedsUnvisited_ = false;
    std::vector<char*> taperChainHeads_;
    omniruntime::vec::VectorBatch* taperChainHeadsBatch_ = nullptr;
#endif
};
} // end of op
} // end of omniruntime
#endif
