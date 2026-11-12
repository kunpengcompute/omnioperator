/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#ifndef __HASH_BUILDER_H__
#define __HASH_BUILDER_H__

#include <memory>
#include <optional>
#include <string>

#include "plannode/planNode.h"
#include "operator/operator_factory.h"
#include "operator/operator.h"
#include "operator/config/operator_config.h"
#include "join_hash_table_variants.h"
#include "common_join.h"
#include "join_sub_partitioner.h"

namespace omniruntime {
namespace op {
class JoinSpillState;

class HashBuilderOperatorFactory : public OperatorFactory {
public:
    HashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes, const int32_t *buildHashCols,
        int32_t buildHashColsCount, int32_t operatorCount);
    HashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide, const DataTypes &buildTypes,
        const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    ~HashBuilderOperatorFactory()
    {
        delete hashTablesVariants;
    }
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
        const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide,
        const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    /// One hash table slot. Spill still ingests into this slot until memory NeedSpill;
    /// restore also rebuilds into slot 0.
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(
        std::shared_ptr<const HashJoinNode> planNode, int32_t operatorCount = 1);
    omniruntime::op::Operator *CreateOperator() override;
    void SetJoinSpillPolicy(bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg);
    void SetJoinSpillState(std::shared_ptr<JoinSpillState> joinSpillState);

    void SetSpillConfig(std::shared_ptr<SparkSpillConfig> spillConfig);
    void SetJoinSpillMemThresholdForTest(int64_t bytes);
    const std::vector<int32_t> &GetBuildHashCols() const
    {
        return buildHashCols;
    }

    std::shared_ptr<JoinSpillState> GetJoinSpillState() const
    {
        return joinSpillState_;
    }

    const DataTypes &GetBuildTypes() const
    {
        return buildTypes;
    }

    HashTableVariants *GetHashTablesVariants()
    {
        return hashTablesVariants;
    }

private:
    DataTypes buildTypes;
    std::vector<int32_t> buildHashCols;
    HashTableVariants *hashTablesVariants;
    std::atomic<int32_t> operatorIndex;
    bool joinSpillEnabled_ = false;
    JoinSubPartitionConfig joinSubPartCfg_;
    std::shared_ptr<JoinSpillState> joinSpillState_;
    std::shared_ptr<SparkSpillConfig> spillConfig_;
    std::optional<int64_t> joinSpillMemThresholdForTest_;

    void SyncSpillBindingsToState();

    template <class RowRefListType>
    HashTableVariants *InitVariant(int32_t buildHashColsCount, int32_t operatorCount, JoinType joinType,
        BuildSide buildSide = OMNI_BUILD_UNKNOWN, bool isMultiCols = false);
};

class HashBuilderOperator : public Operator {
public:
    HashBuilderOperator(const DataTypes &buildTypes, HashTableVariants *hashTables, int32_t partitionIndex,
        bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg, std::vector<int32_t> buildHashCols,
        JoinSpillState *joinSpillState, SparkSpillConfig *spillConfig,
        std::optional<int64_t> spillMemThresholdOverride);

    ~HashBuilderOperator();

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    BlockingReason IsBlocked(ContinueFuture *future) override;

    // for test
    DataTypes &GetBuildTypes()
    {
        return buildTypes;
    }

    HashTableImplementationType GetHashTableType()
    {
        return std::visit(
            [&](auto &&arg) -> HashTableImplementationType { return arg.GetHashTableTypes(partitionIndex); },
            *hashTablesVariants);
    }

    uint32_t GetHashTableSize()
    {
        return std::visit([&](auto &&arg) -> uint32_t { return arg.GetHashTableSize(); }, *hashTablesVariants);
    }

    uint32_t GetHashTableCount()
    {
        return std::visit([&](auto &&arg) -> uint32_t { return arg.GetHashTableCount(); }, *hashTablesVariants);
    }

    void KeepJoinSpillState(std::shared_ptr<JoinSpillState> state);

    bool TryProduceNextRestoreRound();
    /// Spill the probe-held build table and publish spilled partition ids.
    void FlushHeldBuildTable();

    /// True if the last restore open chose BuildHashTableAdaptive (forecast >= threshold).
    bool LastRestoreUsedAdaptiveOpen() const
    {
        return lastRestoreUsedAdaptiveOpen_;
    }

private:
    void AddLatchedInput(omniruntime::vec::VectorBatch *vecBatch);
    void MaybeTriggerSpill();
    void MaybeSpillBeforeOpenSlot0();
    void SpillAndClearResidentBuild();
    void TryPublishEmptyTableHandoff();
    int32_t DriveHandoffRestore();
    void PrepareAndBuildSlot0();
    void PrepareAndBuildSlot0Adaptive();
    void PrepareAndBuildSlot0ForRestore();
    void ResetSlot0();
    uint32_t DistinctCountSlot0() const;
    uint32_t ResidentRowCountSlot0() const;

    DataTypes buildTypes;
    int32_t partitionIndex;
    HashTableVariants *hashTablesVariants;
    bool joinSpillEnabled_ = false;
    bool buildSpillTriggered_ = false;
    bool restoreDriving_ = false;
    bool lastRestoreUsedAdaptiveOpen_ = false;
    JoinSubPartitionConfig joinSubPartCfg_;
    std::vector<int32_t> buildHashCols_;
    JoinSpillState *joinSpillState_ = nullptr;
    std::shared_ptr<JoinSpillState> joinSpillStateKeep_;
    SparkSpillConfig *spillConfig_ = nullptr;
    std::optional<int64_t> spillMemThresholdOverride_;
};

int32_t GetTypeLength(int buildHashColsCount, DataTypes& buildTypes, std::vector<int32_t>& buildHashCols);
} // end of op
} // end of omniruntime
#endif
