/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#ifndef __HASH_BUILDER_H__
#define __HASH_BUILDER_H__

#include <memory>
#include <string>

#include "plannode/planNode.h"
#include "operator/operator_factory.h"
#include "operator/operator.h"
#include "util/config/QueryConfig.h"
#include "join_hash_table_variants.h"
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
    /// when Join run without spill, we only need one HashBuilderOperator
    /// But when Join run with spill, we need muti HashBuilderOperator that every join sub partition o
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

    omniruntime::op::Operator *CreateOperator() override;
    void SetJoinSpillSubPartitionPolicy(bool joinSpillEnabled, uint64_t maxSpillRunRows,
        JoinSubPartitionConfig joinSubPartCfg);
    void SetJoinSpillState(std::shared_ptr<JoinSpillState> joinSpillState);
    void SetBroadcastParallelBuildPolicy(BroadcastParallelBuildPolicy policy)
    {
        broadcastParallelBuildPolicy_ = policy;
    }

    const BroadcastParallelBuildPolicy &GetBroadcastParallelBuildPolicy() const
    {
        return broadcastParallelBuildPolicy_;
    }

    std::shared_ptr<JoinSpillState> GetJoinSpillState() const
    {
        return joinSpillState_;
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
    uint64_t joinMaxSpillRunRows_ = 0;
    JoinSubPartitionConfig joinSubPartCfg_;
    std::shared_ptr<JoinSpillState> joinSpillState_;
    BroadcastParallelBuildPolicy broadcastParallelBuildPolicy_;

    // BHJ cache support
    std::string broadcastHashTableId_;
    bool prebuilt_ = false;   // true when variants were injected from cache
    bool ownsVariants_ = true; // false when variants are owned by cache
    bool cachePinned_ = false; // true when cache owns this factory after publish

    template <class RowRefListType>
    HashTableVariants *InitVariant(int32_t buildHashColsCount, int32_t operatorCount, JoinType joinType,
        BuildSide buildSide = OMNI_BUILD_UNKNOWN, bool isMultiCols = false);

    // Constructor for cache-reuse path: takes pre-built variants directly, no InitVariant call.
    HashBuilderOperatorFactory(const DataTypes &buildTypes, HashTableVariants *cachedVariants);
};

class HashBuilderOperator : public Operator {
public:
    HashBuilderOperator(const DataTypes &buildTypes, HashTableVariants *hashTables, int32_t partitionIndex,
        bool joinSpillEnabled, uint64_t joinMaxSpillRunRows, JoinSubPartitionConfig joinSubPartCfg,
        std::vector<int32_t> buildHashCols, JoinSpillState *joinSpillState,
        bool prebuilt = false, std::string broadcastHashTableId = "",
        HashBuilderOperatorFactory* ownerFactory = nullptr,
        BroadcastParallelBuildPolicy broadcastParallelBuildPolicy = {});

    ~HashBuilderOperator() = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

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

private:
    /// True when join spill sub-partition layout applies (computed once in ctor). Spill/replay still require
    /// \c joinSpillState_ and (for spill) row count >= \c joinMaxSpillRunRows_ at the call site.
    bool UseJoinSubPartitioning() const;
    uint32_t ComputeParallelBuildThreads(uint32_t rowCount, uint64_t estimatedBuildBytes) const;
    bool AddSubPartitionedInput(omniruntime::vec::VectorBatch *vecBatch);

    DataTypes buildTypes;
    int32_t partitionIndex;
    HashTableVariants *hashTablesVariants;
    bool joinSpillEnabled_ = false;
    uint64_t joinMaxSpillRunRows_ = 0;
    uint64_t cumulativeBuildRows_ = 0;
    bool joinSpillBoundaryLogged_ = false;
    bool joinSubPartitionLogged_ = false;
    JoinSubPartitionConfig joinSubPartCfg_;
    /// Cached at construction; matches the former \c UseJoinSubPartitioning() predicate (no spill-state / batch).
    const bool useJoinSubPartitioning_;
    BroadcastParallelBuildPolicy broadcastParallelBuildPolicy_;
    std::vector<int32_t> buildHashCols_;
    JoinSpillState *joinSpillState_ = nullptr;

    // BHJ cache support: skip build and publish when pre-built from cache.
    const bool prebuilt_ = false;
    const std::string broadcastHashTableId_;
    // Back-pointer to factory so we can release its ownership after cache publish.
    HashBuilderOperatorFactory* ownerFactory_ = nullptr;
};

int32_t GetTypeLength(int buildHashColsCount, DataTypes& buildTypes, std::vector<int32_t>& buildHashCols);
} // end of op
} // end of omniruntime
#endif
