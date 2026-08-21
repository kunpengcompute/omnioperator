/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Hash Aggregation Header
 */
#ifndef GROUP_AGGREGATION_H
#define GROUP_AGGREGATION_H

#include "definitions.h"
#include "aggregation.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "operator/hash_util.h"
#include "operator/execution_context.h"
#include "operator/aggregation/aggregator/only_aggregator_factory.h"
#include "operator/util/operator_util.h"
#include "operator/hashmap/column_marshaller.h"
#include "operator/hashmap/array_map.h"
#include "operator/hashmap/vector_analyzer.h"
#ifdef OMNI_SVEHT32_HASH_AGG
#include "operator/hashmap/sve_agg_aos_hash_table32.h"
#include "operator/hashmap/sve_agg_aos_hash_table32_pair.h"
#endif
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/config/operator_config.h"
#include "operator/filter/filter_and_project.h"
#include "operator/pages_index.h"
#include "operator/spill/spiller.h"
#include "group_aggregation_sort.h"
#include "vector/mixed_vector.h"

namespace omniruntime::op {
using namespace vec;

class HashAggregationOperatorFactory;

class HashAggregationOperator;

class HashAggregationOperator : public AggregationCommonOperator {
public:
    HashAggregationOperator(std::vector<ColumnIndex> &groupByCols, std::vector<std::vector<int32_t>> &aggInputCols,
        uint32_t aggInputColsSize, std::vector<DataTypes> &aggInputTypes, std::vector<DataTypes> &aggOutputTypes,
        std::vector<std::unique_ptr<Aggregator>> &&aggs, std::vector<bool> &inputRaws,
        std::vector<bool> &outputPartials, const std::vector<int8_t> &hasAggFilters,
        const OperatorConfig &operatorConfig, std::vector<uint32_t> aggFuncTypesVector, AggregationNode::Step step,
        bool mixedInputExpected = false, bool mixedOutputEnabled = false)
        : AggregationCommonOperator(std::move(aggs), inputRaws, outputPartials),
          groupByCols(groupByCols),
          aggInputCols(aggInputCols),
          aggInputColsSize(aggInputColsSize),
          aggInputTypes(aggInputTypes),
          aggOutputTypes(aggOutputTypes),
          hasAggFilters(hasAggFilters),
          operatorConfig(operatorConfig),
          aggregationStep(step),
          mixedInputExpected(mixedInputExpected),
          mixedOutputEnabled(mixedOutputEnabled)
    {
        isStepPartials = isPartialOutput(step) && !groupByCols.empty();
        for (auto i : aggFuncTypesVector) {
            if (i == OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL || i == OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL
                || i == OMNI_AGGREGATION_TYPE_LAST_IGNORENULL || i == OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL) {
                isStepPartials = false;
                break;
            }
        }
        for (auto hasFilter: hasAggFilters) {
            if (hasFilter == 1) {
                aggFiltersCount++;
            }
        }
    }

    ~HashAggregationOperator() override = default;

    int32_t AddInput(VectorBatch *data) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Init() override;

    OmniStatus Close() override;

    template<typename Serialize>
    void Emplace(Serialize &emplaceKey, VectorBatch *vecBatch, BaseVector **groupVectors, int32_t groupColNum);

    void EmplaceToArrayMap(VectorBatch *vecBatch, BaseVector *groupVector);

    template<typename T, typename GroupMap>
    void InsertValueToArrayMap(GroupMap &arrayMap, BaseVector *groupVector,
                               int32_t rowIdx);

#ifdef OMNI_SVEHT32_HASH_AGG
    int32_t OutputFixedInt32SveAos(VectorBatch **outputVecBatch);

    int32_t OutputFixedInt32PairSveAos(VectorBatch **outputVecBatch);
#endif

    uint64_t GetSpilledBytes() override;

    uint64_t GetUsedMemBytes() override;

    uint64_t GetTotalMemBytes() override;

    std::vector<uint64_t> GetSpecialMetricsInfo() override;

    uint64_t GetHashMapUniqueKeys() override;

    VectorBatch *AlignSchema(VectorBatch *inputVecBatch) override;

    bool isPartialOutput(AggregationNode::Step step) {
        return step == AggregationNode::Step::K_PARTIAL ||
               step == AggregationNode::Step::K_INTERMEDIATE;
    }

    void SetRowCountPerBatch(int32_t rowCount)
    {
        this->rowsPerBatch = rowCount;
    }

    bool IsStepPartials() const
    {
        return isStepPartials;
    }

private:
    int32_t InitMaxRowCountAndOutputTypes();

    void InitSpillInfos();

    ErrorCode SpillHashMap();

    ErrorCode SpillToDisk();

    void SetVectors(VectorBatch *output, const std::vector<DataTypePtr> &types, int32_t rowCount);

    template<typename Deserialize>
    int32_t Output(Deserialize &deserializeHashmap, VectorBatch **outputVecBatch);

    void SetGroupByColumnsHandleType(HandleType t);

    friend class HashAggregationOperatorFactory;

    template<typename V, typename D>
    friend void FillValueImpl(BaseVector *vector, int32_t rowIndex, const AggregateState &state);

    friend void FillVarcharValue(BaseVector *vector, int32_t rowIndex, const AggregateState &state);

    void InitState(int64_t aggStateAddress);

    void ProcessStates(VectorBatch *vecBatch);

#ifdef OMNI_SVEHT32_HASH_AGG
    void EmplaceFixedInt32SveAos(VectorBatch *vecBatch, BaseVector *groupVector);

    void EmplaceFixedInt32PairSveAos(VectorBatch *vecBatch, BaseVector *groupVector0, BaseVector *groupVector1);

    AggregateState *AllocateAggState();

    AggregateState *GetOrCreateNullGroupState32(std::vector<AggregateState *> &newGroupStates);

    uint32_t RegisterSveAggState(AggregateState *state);

    uint32_t RegisterSvePairAggState(AggregateState *state);

    bool HasSveAggData() const;

    bool HasSvePairAggData() const;

    bool IsActiveSveHandle() const;

    void FallbackSveAggToFixedInt32();

    void FallbackSvePairAggToSerializeIfEmpty();

    void ProcessStatesWithNewGroups(VectorBatch *vecBatch, std::vector<AggregateState *> &newGroupStates);
#endif

    template<typename T>
    inline void InsertAggStatesToArrayMap(T *hashes, int32_t vecLanes, bool *isAssigned,
                                    int64_t *slots, mem::SimpleArenaAllocator &arenaAllocator,
                                   int32_t probePosition);

    template<typename T, bool hasNull>
    void ArrayGroupProbeSIMD(BaseVector *groupVector, VectorBatch *vecBatch);

    void ResizeArrayMap(int64_t oldMin);

    void MoveEntryArrayTableToHashMap(int64_t minValue);

    void PrepareSerializeMarshallers(BaseVector **groupVectors, int32_t groupColNum);

    void FallbackNormalizeKeyToSerialize(BaseVector **groupVectors, int32_t groupColNum);

    bool TryEmplaceNormalizeKey(VectorBatch *vecBatch, BaseVector **groupVectors, int32_t groupColNum);

    template<bool hasAgg, typename T>
    void TraverseArrayMapGetOutput(BaseVector *groupVector,
                                   std::vector<AggregateState *> *states, int64_t minValue);

    template<bool hasAgg>
    void TraverseArrayMap(BaseVector *groupVector, std::vector<AggregateState *> *states);

    void TraverseArrayMapToGetOneResult(VectorBatch *output);

    void GetOutputFromDisk(VectorBatch **outputVecBatch);

    VectorBatch *GetOutputFromDiskWithoutAgg(VectorBatch *output);

    VectorBatch *GetOutputFromDiskWithAgg(VectorBatch *output);

    void SetStateOutputVecBatch(VectorBatch *outputVecBatch, int32_t rowCount, int32_t groupColNum, int32_t aggNum);

    void CalcAndSetStatesSize();

    void PrepareSerializeHandlers(BaseVector **groupVectors, int32_t groupColNum);

    int32_t AddMixedInput(MixedVectorBatch *mixedBatch);

    int32_t OutputMixed(VectorBatch **outputVecBatch);

    int32_t OutputMixedFromDisk(VectorBatch **outputVecBatch);

    VectorBatch *BuildMixedBatchFromDiskGroups(int32_t rowCount,
        const std::vector<std::string> &groupKeys,
        const std::vector<AggregateState *> &groupStates);

    VectorBatch *AlignSchemaMixed(VectorBatch *inputVecBatch);

    bool CanUseMixedStateSerde() const;

    ALWAYS_INLINE size_t GetElementsSize();

    ALWAYS_INLINE void ResetHashmap();

    std::vector<ColumnIndex> groupByCols;
    std::vector<std::vector<int32_t>> aggInputCols;
    uint32_t aggInputColsSize;
    std::vector<DataTypes> aggInputTypes;
    std::vector<DataTypes> aggOutputTypes;
    std::vector<type::DataTypePtr> outputTypes;
    HandleType groupByColumnsHandleType = HandleType::serialize;
    std::unique_ptr<TaperColumnSerializeHandler> serialize = nullptr;
    std::unique_ptr<DefaultArrayMap<AggregateState>> arrayTable = nullptr;

    std::unique_ptr<TaperGroupbySingleFixHandler<int16_t>> fixedInt16 = nullptr;
    std::unique_ptr<TaperGroupbySingleFixHandler<int32_t>> fixedInt32 = nullptr;
#ifdef OMNI_SVEHT32_HASH_AGG
    std::unique_ptr<hashmap::SveAggAosHashTable32> fixedInt32SveAos = nullptr;
    std::vector<AggregateState *> fixedInt32SveAosStates;
    AggregateState *nullGroupState32 = nullptr;
    bool hasNullGroupState32 = false;
    bool sveAosNullGroupOutput = false;
    bool sveAosFallbackToFixedInt32 = false;
    std::unique_ptr<hashmap::SveAggAosHashTable32Pair> fixedInt32PairSveAos = nullptr;
    std::vector<AggregateState *> fixedInt32PairSveAosStates;
    bool svePairFallbackToSerialize = false;
#endif
    std::unique_ptr<TaperGroupbySingleFixHandler<int64_t>> fixedInt64 = nullptr;
    std::unique_ptr<TaperGroupbySingleFixHandler<int32_t, true>> packedInt32 = nullptr;
    std::unique_ptr<TaperGroupbySingleFixHandler<int64_t, true>> packedInt64 = nullptr;
    std::unique_ptr<TaperGroupbySingleFixHandler<omniruntime::type::int128_t, true>> packedInt128 = nullptr;
    std::unique_ptr<NormalizeKeyHandler<
        TaperFixedKeyMap<omniruntime::type::int128_t, AggregateState *>>> normalizeKey = nullptr;
    std::unique_ptr<NormalizeKeyWithoutAggHandler<
        TaperFixedKeySet<omniruntime::type::int128_t>>> normalizeKeyWithoutAgg = nullptr;
    bool isInited = false;

    OutputState outputState;

    template<typename Deserialize>
    void TraverseHashmapToGetOneResult(Deserialize &deserializeHashmap, VectorBatch *output);

    int32_t rowsPerBatch;
    std::vector<int8_t> hasAggFilters;
    int32_t aggFiltersCount = 0;
    int64_t memoryChunkSize = 0;

    // for spill
    bool isStepPartials = true;
    OperatorConfig operatorConfig;
    AggregationNode::Step aggregationStep;
    bool mixedInputExpected = false;
    bool mixedOutputEnabled = false;
    bool mixedStateSerdeSupported = false;
    SpillMerger *spillMerger = nullptr;
    Spiller *spiller = nullptr;
    std::vector<SortOrder> sortOrders;
    std::vector<int32_t> groupByCloIdx;
    uint64_t spillTotalRowCount = 0;
    uint64_t spillRowOffset = 0;
    std::unique_ptr<AggregateState[]> groupStates = nullptr;
    std::vector<AggregateState *> rowStates;
    uint64_t spilledBytes = 0;
    uint64_t usedMemBytes = 0;
    uint64_t totalMemBytes = 0;
    std::vector<type::DataTypePtr> aggTypes;
    std::vector<type::DataTypePtr> spillTypes;
    OutputState spillOutputState;
    bool hasSpill = false;
    bool canOutputMixed_ = false;
    std::unique_ptr<AggregationSort> aggregationSort = nullptr;
    int32_t totalAggStatesSize = 0;
    VectorAnalyzer *vectorAnalyzer = nullptr;
    std::vector<AggregateState *> rowsAggStates;
    std::vector<AggregateState *> currentRowStates;
    std::vector<AggregateState *> newGroupStates;
    int8_t resizeArrayMapCnt = 0;

    // 缓存 agg merge ops/sizes，跨批次复用
    std::vector<const MixedStateSerdeOps*> cachedMergeOps_;
    std::vector<int32_t> cachedMergeStateSizes_;
    bool mergeOpsCached_ = false;
};

class HashAggregationOperatorFactory : public AggregationCommonOperatorFactory {
public:
    /*
     * @param groupByCol      the col index which is used as group column in VectorBatch
     * @param groupInputTypes all the group types
     * @param aggsCols        aggsCols contains all aggregators 's all agg col index
     * @param aggInputTypes   input types of all aggregators
     * @param aggOutputTypes  output types of all aggregators
     * @param aggFuncTypes    func types of aggregators
     * @param maskColsVector  mask col index in VectorBatch
     * @param inputRaws       whether the input VectorBatch is raw, the input raw is true in the first stage
     * @param outputPartials  whether the output VectorBatch is partial result
     * @param operatorConfig  the operator config
     */
    HashAggregationOperatorFactory(std::vector<uint32_t> &groupByCol, const DataTypes &groupInputTypes,
        std::vector<std::vector<uint32_t>> &aggsCols, std::vector<DataTypes> &aggInputTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
        std::vector<uint32_t> &maskColsVector, std::vector<bool> inputRaws, std::vector<bool> outputPartials,
        const OperatorConfig &operatorConfig)
        : AggregationCommonOperatorFactory(inputRaws, outputPartials, maskColsVector,
        operatorConfig.GetOverflowConfig()->IsOverflowAsNull(), operatorConfig.IsStatisticalAggregate()),
          groupByColsVector(groupByCol),
          groupByTypes(groupInputTypes),
          aggsInputColsVector(aggsCols),
          aggInputTypes(aggInputTypes),
          aggOutputTypes(aggOutputTypes),
          aggFuncTypesVector(aggFuncTypes),
          operatorConfig(operatorConfig)
    {}

    HashAggregationOperatorFactory(std::vector<uint32_t> &groupByCol, const DataTypes &groupInputTypes,
        std::vector<std::vector<uint32_t>> &aggsCols, std::vector<DataTypes> &aggInputTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
        std::vector<uint32_t> &maskColsVector, std::vector<bool> inputRaws, std::vector<bool> outputPartials,
        bool overflowAsNull = false, bool isStatisticalAggregate = false)
        : AggregationCommonOperatorFactory(inputRaws, outputPartials, maskColsVector, overflowAsNull),
          groupByColsVector(groupByCol),
          groupByTypes(groupInputTypes),
          aggsInputColsVector(aggsCols),
          aggInputTypes(aggInputTypes),
          aggOutputTypes(aggOutputTypes),
          aggFuncTypesVector(aggFuncTypes)
    {}

    /*
     * @param groupByCol      the col index which is used as group column in VectorBatch
     * @param groupInputTypes all the group types
     * @param aggsCols        aggsCols contains all aggregators 's all agg col index
     * @param aggInputTypes   input types of all aggregators
     * @param aggOutputTypes  output types of all aggregators
     * @param aggFuncTypes    func types of aggregators
     * @param maskColsVector  mask col index in VectorBatch
     * @param inputRaws       whether the input VectorBatch is raw, the input raw is true in the first stage
     * @param outputPartials  whether the output VectorBatch is partial result
     * @param hasAggFilters   whether handle the agg filter when AddInput
     * @param operatorConfig  the operator config
     * this is for HashAggregationWithExprOperatorFactory
     */
    HashAggregationOperatorFactory(std::vector<uint32_t> &groupByCol, const DataTypes &groupInputTypes,
        std::vector<std::vector<uint32_t>> &aggsCols, std::vector<DataTypes> &aggInputTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
        std::vector<uint32_t> &maskColsVector, std::vector<bool> inputRaws, std::vector<bool> outputPartials,
        const std::vector<int8_t> &hasAggFilters, const OperatorConfig &operatorConfig, AggregationNode::Step step,
        bool mixedInputExpected = false, bool mixedOutputEnabled = false)
        : AggregationCommonOperatorFactory(inputRaws, outputPartials, maskColsVector,
        operatorConfig.GetOverflowConfig()->IsOverflowAsNull(), operatorConfig.IsStatisticalAggregate()),
          groupByColsVector(groupByCol),
          groupByTypes(groupInputTypes),
          aggsInputColsVector(aggsCols),
          aggInputTypes(aggInputTypes),
          aggOutputTypes(aggOutputTypes),
          aggFuncTypesVector(aggFuncTypes),
          hasAggFilters(hasAggFilters),
          operatorConfig(operatorConfig),
          step(step),
          mixedInputExpected(mixedInputExpected),
          mixedOutputEnabled(mixedOutputEnabled)
    {}

    ~HashAggregationOperatorFactory() override = default;

    Operator *CreateOperator() override;

    OmniStatus Init() override;

    OmniStatus Close() override;

    void SetNormalizedKeyEnabledForFactory(bool enabled)
    {
        normalizedKeyEnabled = enabled;
    }

private:
    std::vector<uint32_t> groupByColsVector;
    std::vector<int32_t> groupByColIndices;
    DataTypes groupByTypes;
    std::vector<std::vector<uint32_t>> aggsInputColsVector;
    std::vector<std::vector<int32_t>> aggsInputCols;
    std::vector<std::vector<uint32_t>> aggOutputColsVector;
    std::vector<DataTypes> aggInputTypes;
    std::vector<DataTypes> aggOutputTypes;
    std::vector<uint32_t> aggFuncTypesVector;
    std::vector<std::unique_ptr<AggregatorFactory>> aggregatorFactories;
    HandleType handleType;
    std::vector<int8_t> hasAggFilters;
    OperatorConfig operatorConfig;
    AggregationNode::Step step;
    bool normalizedKeyEnabled = false;
    bool mixedInputExpected = false;
    bool mixedOutputEnabled = false;
    void ChooseGroupByType();
};
} // end of namespace omniruntime::op
#endif
