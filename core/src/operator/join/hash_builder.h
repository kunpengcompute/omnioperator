/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#ifndef __HASH_BUILDER_H__
#define __HASH_BUILDER_H__

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "plannode/planNode.h"
#include "operator/operator_factory.h"
#include "operator/operator.h"
#include "operator/spillable_operator.h"
#include "util/config/QueryConfig.h"
#include "operator/config/operator_config.h"
#include "join_hash_table_variants.h"
#include "broadcast_hash_table_cache.h"
#include "common_join.h"
#include "join_sub_partitioner.h"

namespace omniruntime {
namespace op {
class JoinSpillState;

struct BroadcastParallelBuildPolicy {
    bool enabled = false;
    uint32_t minTableRowsForParallelJoinBuild = 1'000;
    uint64_t targetBytesPerThread = 16UL << 20;

    static BroadcastParallelBuildPolicy FromQueryConfig(const config::QueryConfig &queryConfig)
    {
        BroadcastParallelBuildPolicy policy;
        policy.enabled = queryConfig.broadcastParallelBuildEnabled();
        policy.minTableRowsForParallelJoinBuild = queryConfig.minTableRowsForParallelJoinBuild();
        policy.targetBytesPerThread = queryConfig.broadcastParallelBuildTargetBytesPerThread();
        return policy;
    }
};

class HashBuilderOperatorFactory : public OperatorFactory {
public:
    HashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes, const int32_t *buildHashCols,
        int32_t buildHashColsCount, int32_t operatorCount);
    HashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide, const DataTypes &buildTypes,
        const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    ~HashBuilderOperatorFactory()
    {
        // Only delete if this factory owns the variants (not injected from cache).
        if (ownsVariants_) {
            delete hashTablesVariants;
        }
    }
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
        const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide,
        const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    /// One hash table slot. Pool reclaim may spill resident input from this slot;
    /// restore also rebuilds into slot 0.
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(
        std::shared_ptr<const HashJoinNode> planNode, int32_t operatorCount = 1);

    /**
     * Create a factory wrapping a pre-built hash table from the executor-level cache.
     * The factory does NOT own `cachedVariants`; the cache retains ownership.
     * Operators created from this factory skip AddInput/BuildHashTable entirely.
     */
    static HashBuilderOperatorFactory *CreateFromCachedVariants(
        std::shared_ptr<const HashJoinNode> planNode, HashTableVariants* cachedVariants);

    /**
     * Replace this factory's hash table with a pre-built one from the executor-level cache.
     * The factory does NOT take ownership of `cachedVariants`; the cache retains ownership.
     */
    void InjectCachedVariants(HashTableVariants* cachedVariants, const std::string& broadcastHashTableId = "");

    omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;
    void SetJoinSpillSubPartitionPolicy(bool enabled, uint64_t, JoinSubPartitionConfig cfg,
        bool passiveReclaimEnabled = false, bool forceBuildGroupSpillAfterNoMoreInput = false, int32_t = 10);
    void SetJoinSpillHandoffSessionEnabled(bool enabled);
    void SetJoinSpillPolicy(bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg);
    void SetJoinSpillState(std::shared_ptr<JoinSpillState> joinSpillState);
    void SetBroadcastParallelBuildPolicy(BroadcastParallelBuildPolicy policy)
    {
        broadcastParallelBuildPolicy_ = policy;
    }

    void SetDynamicFilterPushdownEnabled(bool enabled);

    /// Keep one build row per join key. Only valid when the probe never needs
    /// duplicate build rows: semi/anti/existence joins without a join filter.
    void SetDistinctBuild(bool enabled);

    bool IsDynamicFilterPushdownEnabled() const
    {
        return dynamicFilterPushdownEnabled_;
    }

    const BroadcastParallelBuildPolicy &GetBroadcastParallelBuildPolicy() const
    {
        return broadcastParallelBuildPolicy_;
    }

    void SetSpillConfig(std::shared_ptr<SparkSpillConfig> spillConfig);
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

    /** Set the broadcast hash table id for post-build cache registration. */
    void SetBroadcastHashTableId(std::string id)
    {
        broadcastHashTableId_ = std::move(id);
    }

    const std::string& GetBroadcastHashTableId() const
    {
        return broadcastHashTableId_;
    }

    /** True when this factory holds a pre-built table injected from cache. */
    bool IsPrebuilt() const
    {
        return prebuilt_;
    }

    /** Pinning factory lifetime is owned by BroadcastHashTableCache after publish. */
    void MarkCachePinned()
    {
        cachePinned_ = true;
    }

    bool IsCachePinned() const
    {
        return cachePinned_;
    }

    void SetCacheBuildTicket(std::shared_ptr<BroadcastHashTableCache::BuildTicket> ticket)
    {
        cacheBuildTicket_ = std::move(ticket);
    }

    void SetCacheLeaseSlot(std::weak_ptr<BroadcastCacheLeaseSlot> slot)
    {
        cacheLeaseSlot_ = std::move(slot);
    }

    omniruntime::mem::OmniTaskMemoryPool* GetCachePool()
    {
        if (cacheBuildTicket_ == nullptr) {
            throw std::logic_error("Broadcast cache build has no ticket");
        }
        return cacheBuildTicket_->pool();
    }

    // Called after build completion; the probe factory retains the lease slot.
    void PublishCachedVariants();

    /**
     * Transfer ownership of hashTablesVariants to the caller (the cache).
     * After this call the factory no longer owns or deletes the variants.
     */
    HashTableVariants* ReleaseVariants()
    {
        ownsVariants_ = false;
        return hashTablesVariants;
    }

private:
    DataTypes buildTypes;
    std::vector<int32_t> buildHashCols;
    HashTableVariants *hashTablesVariants;
    std::atomic<int32_t> operatorIndex;
    bool joinSpillEnabled_ = false;
    bool passiveReclaimEnabled_ = false;
    bool forceBuildGroupSpillAfterNoMoreInput_ = false;
    JoinSubPartitionConfig joinSubPartCfg_;
    std::shared_ptr<JoinSpillState> joinSpillState_;
    BroadcastParallelBuildPolicy broadcastParallelBuildPolicy_;

    // BHJ cache support
    std::string broadcastHashTableId_;
    bool prebuilt_ = false;   // true when variants were injected from cache
    bool ownsVariants_ = true; // false when variants are owned by cache
    bool cachePinned_ = false; // true when cache owns this factory after publish
    std::shared_ptr<BroadcastHashTableCache::BuildTicket> cacheBuildTicket_;
    std::weak_ptr<BroadcastCacheLeaseSlot> cacheLeaseSlot_;
    bool dynamicFilterPushdownEnabled_ = false;
    std::shared_ptr<SparkSpillConfig> spillConfig_;

    void SyncSpillBindingsToState();

    template <class RowRefListType>
    HashTableVariants *InitVariant(int32_t buildHashColsCount, int32_t operatorCount, JoinType joinType,
        BuildSide buildSide = OMNI_BUILD_UNKNOWN, bool isMultiCols = false);

    // Constructor for cache-reuse path: takes pre-built variants directly, no InitVariant call.
    HashBuilderOperatorFactory(const DataTypes &buildTypes, HashTableVariants *cachedVariants);
};

class HashBuilderOperator : public Operator, public SpillableOperator {
public:
    HashBuilderOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const DataTypes &buildTypes,
        HashTableVariants *hashTables, int32_t partitionIndex,
        bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg, std::vector<int32_t> buildHashCols,
        JoinSpillState *joinSpillState, bool passiveReclaimEnabled,
        bool forceBuildGroupSpillAfterNoMoreInput, bool prebuilt, std::string broadcastHashTableId,
        HashBuilderOperatorFactory *ownerFactory, BroadcastParallelBuildPolicy broadcastParallelBuildPolicy,
        bool dynamicFilterPushdownEnabled);

    ~HashBuilderOperator();

    uint64_t reclaimableBytes() const override;
    int64_t reclaim(int64_t targetBytes) override;
    bool isInNonReclaimableSection() const noexcept override
    {
        return Operator::isInNonReclaimableSection();
    }
    Operator *operatorBase() noexcept override { return this; }
    const char *reclaimableName() const noexcept override { return "HashBuilderOperator"; }

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

    uint32_t GetResidentBuildPartitionCount() const
    {
        return std::visit([&](auto &&arg) -> uint32_t {
            return arg.GetResidentBuildPartitionCount();
        }, *hashTablesVariants);
    }

    void KeepJoinSpillState(std::shared_ptr<JoinSpillState> state);

    bool TryProduceNextRestoreRound();
    /// Spill the probe-held build table and publish spilled partition ids.
    void FlushHeldBuildTable();

    /// True if the last restore open chose BuildHashTableAdaptive after reservation denial.
    bool LastRestoreUsedAdaptiveOpen() const
    {
        return lastRestoreUsedAdaptiveOpen_;
    }

private:
    uint32_t ComputeParallelBuildThreads(uint32_t rowCount, uint64_t estimatedBuildBytes) const;
    void AddLatchedInput(omniruntime::vec::VectorBatch *vecBatch);
    void AddBuildBatch(int32_t slot, omniruntime::vec::VectorBatch *vecBatch);
    void SpillAndClearResidentBuild();
    void TryPublishEmptyTableHandoff();
    int32_t DriveHandoffRestore();
    void PrepareAndBuildSlot0();
    void PrepareAndBuildSlot0Adaptive();
    void PrepareAndBuildSlot0ForRestore();
    bool TryReserveSlot0Table(const char *site);
    void ResetSlot0();
    uint32_t DistinctCountSlot0() const;
    uint32_t ResidentRowCountSlot0() const;

    DataTypes buildTypes;
    int32_t partitionIndex;
    HashTableVariants *hashTablesVariants;
    bool joinSpillEnabled_ = false;
    bool passiveReclaimEnabled_ = false;
    bool forceBuildGroupSpillAfterNoMoreInput_ = false;
    bool spillableRegistered_ = false;
    bool buildSpillTriggered_ = false;
    bool restoreDriving_ = false;
    bool lastRestoreUsedAdaptiveOpen_ = false;
    JoinSubPartitionConfig joinSubPartCfg_;
    BroadcastParallelBuildPolicy broadcastParallelBuildPolicy_;
    std::vector<int32_t> buildHashCols_;
    JoinSpillState *joinSpillState_ = nullptr;
    std::shared_ptr<JoinSpillState> joinSpillStateKeep_;

    // BHJ cache support: skip build and publish when pre-built from cache.
    const bool prebuilt_ = false;
    const std::string broadcastHashTableId_;
    // Back-pointer to factory so we can release its ownership after cache publish.
    HashBuilderOperatorFactory *ownerFactory_ = nullptr;
    omniruntime::mem::OmniTaskMemoryPool *cachePool_ = nullptr; // owned by entry memory
    const bool dynamicFilterPushdownEnabled_ = false;
};

int32_t GetTypeLength(int buildHashColsCount, DataTypes& buildTypes, std::vector<int32_t>& buildHashCols);
} // end of op
} // end of omniruntime
#endif
