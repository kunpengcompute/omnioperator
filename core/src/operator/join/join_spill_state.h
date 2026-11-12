/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: hash join spill session, sticky writers, and build/probe handoff.
 */
#ifndef OMNI_RUNTIME_JOIN_SPILL_STATE_H
#define OMNI_RUNTIME_JOIN_SPILL_STATE_H

#include <cstdint>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "join_hash_table_variants.h"
#include "plannode/planNode.h"
#include "operator/spill/spill_merger.h"
#include "type/data_types.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

namespace omniruntime::op {
class SpillWriter;
class SparkSpillConfig;
struct JoinSubPartitionConfig;

// Lifecycle of the build table from the probe's point of view.
// Idle: the builder still owns the input; nothing has been handed off.
// ProbePathA: an empty in-memory table was published and the probe spills its
// input by sub-partition. Flushing a held full table returns to this phase.
// RestoreRound: the probe holds an in-memory table. That table is either the
// resident full table (including an empty build) or one restored spill
// partition. Held-table flush is allowed only in this phase. Which table is in
// hand is JoinSpillTableInHandKind, not the phase name.
// Complete: probe ingest and every restore round are finished.
enum class JoinHandoffPhase {
    Idle,
    ProbePathA,
    RestoreRound,
    Complete,
};

enum class JoinBuildRuntimeLayout {
    Single,
    Partitioned,
};

enum class JoinSpillTableInHandKind {
    None,
    FullTable,
    RestoreSmallTable,
};

struct JoinSpillTableInHand {
    JoinSpillTableInHandKind kind = JoinSpillTableInHandKind::None;
    std::optional<uint32_t> restorePartitionId;
};

struct JoinHandoffSnapshot {
    uint32_t numDistinct = 0;
    std::vector<uint32_t> spillPartitionIds;
    std::optional<uint32_t> restoredPartitionId;
    uint64_t handoffGeneration = 0;
    JoinBuildRuntimeLayout runtimeLayout = JoinBuildRuntimeLayout::Single;
};

class JoinSpillState {
public:
    JoinSpillState(const std::string &spillDir, const type::DataTypes &buildTypes,
        const type::DataTypes &probeTypes, uint32_t numSubPartitions);
    ~JoinSpillState();

    uint64_t NextRunId();

    void SpillBuildSubPartition(uint64_t runId, uint32_t subPartition, vec::VectorBatch *batch);
    void CloseBuildWriters();

    void RestoreBuildSubPartition(HashTableVariants *hashTables, uint32_t subPartition);
    /// After RestoreBuildSubPartition + BuildHashTable, write peeled visited flags into the table.
    void ApplyPendingVisitedFlags(HashTableVariants *hashTables, uint32_t subPartition);
    bool HasPendingBuildSpill(uint32_t subPartition) const;

    std::string ProbeSubPartitionDir(uint64_t runId, uint32_t subPartition);

    uint64_t BuildSpilledRows() const
    {
        return buildSpilledRows_;
    }

    uint64_t BuildSpilledBytes() const
    {
        return buildSpilledBytes_;
    }

    uint32_t BuildSpilledFiles() const
    {
        return buildSpilledFiles_;
    }

    uint32_t NumSubPartitions() const
    {
        return numSubPartitions_;
    }

    bool HasAnyPendingBuildSpill() const;
    std::vector<uint32_t> CollectBuildSpillPartitionIds() const;

    void SetInitialRuntimeLayout(JoinBuildRuntimeLayout layout);
    JoinBuildRuntimeLayout GetRuntimeLayout() const;
    void MarkRuntimePartitioned();
    bool IsRuntimePartitioned() const
    {
        return GetRuntimeLayout() == JoinBuildRuntimeLayout::Partitioned;
    }

    void SetHashTableHandoff(uint32_t numDistinct, const std::vector<uint32_t> &spillPartitionIds,
        const std::optional<uint32_t> &restoredPartitionId = std::nullopt);
    bool TryGetHandoffForProbe(JoinHandoffSnapshot *out);
    JoinHandoffSnapshot WaitHandoffForProbe();

    void MarkProbePathAFinished();
    bool IsProbePathAFinished() const;
    std::optional<uint32_t> NextRestorePartitionId();
    void ProbeFinished(uint32_t partitionId);
    void AbandonProbe();
    void AppendSpilledHashTablePartitions(const std::vector<uint32_t> &newIds);
    std::vector<uint32_t> SpillPartitionIds() const;
    bool BuildCanAdvanceRestore() const;

    JoinHandoffPhase GetHandoffPhase() const;
    uint64_t HandoffGeneration() const;

    // Held-table flush support (post-handoff, probe-held build table).
    void SetSpillJoinType(JoinType joinType, BuildSide buildSide = OMNI_BUILD_UNKNOWN);
    bool NeedsVisitedFlagSpill() const;
    const type::DataTypes &BuildSpillTypes() const;

    void SetTableInHandNone();
    void SetTableInHandFullTable();
    void SetTableInHandRestoreSmallTable(uint32_t partitionId);
    JoinSpillTableInHand TableInHand() const;

    void SetHasMoreProbeInput(bool hasMore);
    bool HasMoreProbeInput() const;
    void SetProbeInNrs(bool inNrs);
    bool ProbeInNrs() const;

    bool CanFlushHeldTable() const;
    void PublishHeldTableFlush(const std::vector<uint32_t> &ids);
    /// Spill the probe-held build table and publish spilled partition ids.
    void FlushHeldBuildTable(HashTableVariants *hashTables, const JoinSubPartitionConfig &cfg);
    bool WasRestorePartitionFlushed(uint32_t partitionId) const;

    void BindSpillConfig(std::shared_ptr<SparkSpillConfig> spillConfig);
    /// Counts build and probe spill bytes and throws once they pass maxSpillBytes.
    void AccountSpilledBytes(uint64_t bytes);
    void SetSpillMemThresholdOverride(std::optional<int64_t> bytes);
    void SetBuildHashCols(std::vector<int32_t> hashCols);
    bool AccountedMemoryOverThreshold() const;
    int64_t SpillMemThreshold() const;

    const type::DataTypes &BuildTypes() const
    {
        return buildTypes_;
    }

    const type::DataTypes &ProbeTypes() const
    {
        return probeTypes_;
    }

private:
    void EnsureSessionDirLocked();
    std::string MakeSubPartitionDirLocked(const char *side, uint64_t runId, uint32_t subPartition);
    void CloseBuildWritersLocked();
    void AccountSpilledBytesLocked(uint64_t bytes);
    void RemoveBuildSpillFilesLocked();
    void PublishHandoffLocked(uint32_t numDistinct, const std::vector<uint32_t> &spillPartitionIds,
        const std::optional<uint32_t> &restoredPartitionId);
    void ValidateHandoffLocked(uint32_t numDistinct, const std::vector<uint32_t> &spillPartitionIds,
        const std::optional<uint32_t> &restoredPartitionId) const;
    void RebuildBuildSpillTypesLocked();
    bool IsHeldTableJoinAllowed() const;
    void ClearPendingVisitedFlagsLocked(uint32_t subPartition);

    type::DataTypes buildTypes_;
    type::DataTypes probeTypes_;
    std::string spillDir_;
    std::string sessionDir_;
    uint32_t numSubPartitions_ = 1;

    mutable std::mutex mutex_;
    std::vector<std::vector<SpillFileInfo>> buildFiles_;
    std::vector<std::unique_ptr<SpillWriter>> buildWriters_;
    std::optional<uint64_t> buildSpillWaveRunId_;
    uint64_t nextRunId_ = 0;
    uint64_t buildSpilledRows_ = 0;
    uint64_t buildSpilledBytes_ = 0;
    uint64_t cappedSpillBytes_ = 0;
    uint32_t buildSpilledFiles_ = 0;

    mutable std::mutex handoffMutex_;
    std::condition_variable handoffCv_;
    JoinHandoffPhase handoffPhase_ = JoinHandoffPhase::Idle;
    JoinBuildRuntimeLayout runtimeLayout_ = JoinBuildRuntimeLayout::Single;
    JoinHandoffSnapshot pendingHandoff_;
    bool handoffReady_ = false;
    uint64_t handoffGeneration_ = 0;
    std::vector<uint32_t> spillPartitionIds_;
    std::vector<uint32_t> restoreQueue_;
    bool probePathAFinished_ = false;
    std::optional<uint32_t> activeRestorePartitionId_;
    bool activeRestoreRoundComplete_ = true;

    std::optional<JoinType> spillJoinType_;
    std::optional<BuildSide> spillJoinBuildSide_;
    type::DataTypes buildSpillTypes_;
    bool buildSpillTypesReady_ = false;
    bool needsVisitedFlagSpill_ = false;
    std::vector<std::vector<vec::BaseVector *>> pendingVisitedFlags_;

    JoinSpillTableInHandKind tableInHandKind_ = JoinSpillTableInHandKind::None;
    std::optional<uint32_t> tableInHandRestoreId_;
    bool hasMoreProbeInput_ = false;
    bool probeInNrs_ = false;
    std::unordered_set<uint32_t> flushedRestorePartitions_;

    std::shared_ptr<SparkSpillConfig> spillConfig_;
    std::optional<int64_t> spillMemThresholdOverride_;
    std::vector<int32_t> buildHashCols_;
};

void FlattenBatchForJoinSpill(omniruntime::vec::VectorBatch *batch);
/// Best-effort delete; a missing file is not an error.
void RemoveJoinSpillFile(const std::string &path);

struct VecBatchDeleter {
    void operator()(omniruntime::vec::VectorBatch *batch) const noexcept
    {
        omniruntime::vec::VectorHelper::FreeVecBatch(batch);
    }
};

using UniqueVecBatch = std::unique_ptr<omniruntime::vec::VectorBatch, VecBatchDeleter>;

/// Frees leftover batches on every exit, including exceptions after TakeResidentBatches.
struct OwnedVecBatchList {
    std::vector<omniruntime::vec::VectorBatch *> batches;

    OwnedVecBatchList() = default;
    explicit OwnedVecBatchList(std::vector<omniruntime::vec::VectorBatch *> &&src) : batches(std::move(src)) {}
    OwnedVecBatchList(const OwnedVecBatchList &) = delete;
    OwnedVecBatchList &operator=(const OwnedVecBatchList &) = delete;
    OwnedVecBatchList(OwnedVecBatchList &&) = default;
    OwnedVecBatchList &operator=(OwnedVecBatchList &&) = default;

    ~OwnedVecBatchList()
    {
        for (auto *batch : batches) {
            omniruntime::vec::VectorHelper::FreeVecBatch(batch);
        }
    }

    void Free(omniruntime::vec::VectorBatch *batch)
    {
        if (batch == nullptr) {
            return;
        }
        for (auto &slot : batches) {
            if (slot == batch) {
                omniruntime::vec::VectorHelper::FreeVecBatch(slot);
                slot = nullptr;
                return;
            }
        }
    }
};

struct OwnedFlagColumns {
    std::vector<omniruntime::vec::BaseVector *> flags;

    explicit OwnedFlagColumns(std::vector<omniruntime::vec::BaseVector *> &&src) : flags(std::move(src)) {}
    OwnedFlagColumns(const OwnedFlagColumns &) = delete;
    OwnedFlagColumns &operator=(const OwnedFlagColumns &) = delete;

    ~OwnedFlagColumns()
    {
        for (auto *flag : flags) {
            delete flag;
        }
        flags.clear();
    }
};

/// E2E debug (IsDebugEnable): proactive spill memory checkpoint at build/probe AddInput and
/// probe restore batch boundary. action: skip | check | build_spill | held_flush
void LogJoinSpillMemCheckpoint(const char *site, int64_t accounted, int64_t threshold, bool overThreshold,
    const char *action, const char *detail = nullptr);

} // namespace omniruntime::op

#endif // OMNI_RUNTIME_JOIN_SPILL_STATE_H
