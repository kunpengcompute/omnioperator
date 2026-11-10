/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: hash join spill session, sticky writers, and build/probe handoff.
 */
#include "join_spill_state.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "join_sub_partitioner.h"
#include "memory/memory_manager.h"
#include "operator/config/operator_config.h"
#include "operator/spill/spiller.h"
#include "type/data_type.h"
#include "util/bit_util.h"
#include "util/debug.h"
#include "util/global_log.h"
#include "util/omni_exception.h"
#include "util/type_util.h"
#include "vector/unsafe_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"

namespace omniruntime::op {
namespace {

std::string NormalizeLocalSpillDir(const std::string &path)
{
    if (path.rfind("file://", 0) == 0) {
        return path.substr(7);
    }
    if (path.rfind("file:", 0) == 0) {
        return path.substr(5);
    }
    return path;
}

bool MkdirIfNeeded(const std::string &path)
{
    if (path.empty()) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_WRITE_FAILED",
            "Join spill directory is empty.");
    }
    if (mkdir(path.c_str(), 0750) == 0 || errno == EEXIST) {
        return true;
    }
    const int err = errno;
    throw omniruntime::exception::OmniException("JOIN_SPILL_WRITE_FAILED",
        "Failed to create join spill directory '" + path + "': " + std::strerror(err));
}

omniruntime::vec::BaseVector *FlattenColumnForJoinSpill(omniruntime::vec::BaseVector *col, int32_t rowCount)
{
    (void)rowCount;
    if (col == nullptr) {
        return nullptr;
    }
    const auto enc = col->GetEncoding();
    if (enc == omniruntime::vec::OMNI_DICTIONARY) {
        auto *flat = omniruntime::vec::VectorHelper::DecodeDictionaryVector(col);
        delete col;
        return flat;
    }
    if (enc == omniruntime::vec::OMNI_ENCODING_CONST) {
        auto *flat = omniruntime::vec::VectorHelper::DecodeConstVector(col);
        delete col;
        return flat;
    }
    return col;
}

std::vector<uint32_t> SortedUnique(const std::vector<uint32_t> &ids)
{
    std::vector<uint32_t> out = ids;
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

vec::BaseVector *CreateAllFalseVisitedFlag(int32_t rowCount)
{
    auto *flags = new vec::Vector<bool>(rowCount);
    for (int32_t i = 0; i < rowCount; ++i) {
        flags->SetValue(i, false);
    }
    return flags;
}

/// Split a build-spill batch with a trailing BOOLEAN visited flag into the original
/// build columns (for AddVecBatch) and the flag column (caller owns the flag).
/// If the batch is not buildColCount+1 wide, return it unchanged (no flag).
std::pair<vec::VectorBatch *, vec::BaseVector *> PeelVisitedFlagColumn(vec::VectorBatch *batch,
    int32_t buildColCount)
{
    UniqueVecBatch owner(batch);
    if (batch == nullptr) {
        return {nullptr, nullptr};
    }
    const int32_t vecCount = batch->GetVectorCount();
    if (vecCount != buildColCount + 1) {
        return {owner.release(), nullptr};
    }
    const int32_t rowCount = batch->GetRowCount();
    UniqueVecBatch buildColumns(new vec::VectorBatch(rowCount));
    buildColumns->ResizeVectorCount(buildColCount);
    for (int32_t col = 0; col < buildColCount; ++col) {
        buildColumns->SetVector(col, batch->Get(col));
        batch->SetVector(col, nullptr);
    }
    auto *visitedFlag = batch->Get(buildColCount);
    batch->SetVector(buildColCount, nullptr);
    owner.reset();
    return {buildColumns.release(), visitedFlag};
}

} // namespace

void FlattenBatchForJoinSpill(omniruntime::vec::VectorBatch *batch)
{
    if (batch == nullptr) {
        return;
    }
    const int32_t rows = batch->GetRowCount();
    const int32_t n = batch->GetVectorCount();
    for (int32_t i = 0; i < n; ++i) {
        batch->SetVector(i, FlattenColumnForJoinSpill(batch->Get(i), rows));
    }
}

JoinSpillState::JoinSpillState(const std::string &spillDir, const type::DataTypes &buildTypes,
    const type::DataTypes &probeTypes, uint32_t numSubPartitions)
    : buildTypes_(buildTypes),
      probeTypes_(probeTypes),
      spillDir_(NormalizeLocalSpillDir(spillDir)),
      numSubPartitions_(std::max(1U, numSubPartitions)),
      buildFiles_(numSubPartitions_),
      buildWriters_(numSubPartitions_),
      pendingVisitedFlags_(numSubPartitions_)
{}

JoinSpillState::~JoinSpillState()
{
    try {
        CloseBuildWriters();
    } catch (...) {
    }
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        RemoveBuildSpillFilesLocked();
    } catch (...) {
    }
    // Build and probe files all live under sessionDir_. Removing it also clears files left by a failed
    // restore or probe writer close, and the run/sp directories.
    try {
        if (!sessionDir_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(sessionDir_, error);
            if (error) {
                LogWarn("Failed to remove join spill session directory %s: %s.", sessionDir_.c_str(),
                    error.message().c_str());
            }
        }
    } catch (...) {
    }
    for (uint32_t sp = 0; sp < pendingVisitedFlags_.size(); ++sp) {
        ClearPendingVisitedFlagsLocked(sp);
    }
}

void JoinSpillState::EnsureSessionDirLocked()
{
    if (!sessionDir_.empty()) {
        return;
    }
    MkdirIfNeeded(spillDir_);
    // mkdtemp never reuses an existing directory, so the destructor's remove_all cannot reach another
    // process's session (separate PID namespaces) or files left by a crashed process with the same pid.
    std::string pattern = spillDir_ + "/join-spill-" + std::to_string(static_cast<int>(getpid())) + "-XXXXXX";
    if (mkdtemp(pattern.data()) == nullptr) {
        const int err = errno;
        throw omniruntime::exception::OmniException("JOIN_SPILL_WRITE_FAILED",
            "Failed to create join spill session directory '" + pattern + "': " + std::strerror(err));
    }
    sessionDir_ = std::move(pattern);
}

uint64_t JoinSpillState::NextRunId()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return nextRunId_++;
}

std::string JoinSpillState::MakeSubPartitionDirLocked(const char *side, uint64_t runId, uint32_t subPartition)
{
    EnsureSessionDirLocked();
    const std::string sideDir = sessionDir_ + "/" + side;
    MkdirIfNeeded(sideDir);
    const std::string runDir = sideDir + "/run" + std::to_string(runId);
    MkdirIfNeeded(runDir);
    const std::string subPartitionDir = runDir + "/sp" + std::to_string(subPartition);
    MkdirIfNeeded(subPartitionDir);
    return subPartitionDir;
}

std::string JoinSpillState::ProbeSubPartitionDir(uint64_t runId, uint32_t subPartition)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return MakeSubPartitionDirLocked("probe", runId, subPartition);
}

void JoinSpillState::SpillBuildSubPartition(uint64_t runId, uint32_t subPartition, vec::VectorBatch *batch)
{
    if (batch == nullptr || batch->GetRowCount() <= 0) {
        return;
    }
    if (subPartition >= numSubPartitions_) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_WRITE_FAILED",
            "Build spill sub-partition out of range.");
    }
    const bool needFlag = NeedsVisitedFlagSpill();
    const type::DataTypes spillTypes = BuildSpillTypes();
    std::lock_guard<std::mutex> lock(mutex_);
    if (!buildSpillWaveRunId_.has_value()) {
        buildSpillWaveRunId_ = runId;
    }
    FlattenBatchForJoinSpill(batch);
    // On-disk schema is BuildSpillTypes(): SameSide/FULL append a trailing BOOLEAN
    // when the batch still has only the in-memory build columns (all false until probed).
    if (needFlag && batch->GetVectorCount() == buildTypes_.GetSize()) {
        batch->Append(CreateAllFalseVisitedFlag(batch->GetRowCount()));
    }
    if (buildWriters_[subPartition] == nullptr) {
        const std::string path = MakeSubPartitionDirLocked("build", *buildSpillWaveRunId_, subPartition);
        buildWriters_[subPartition] = std::make_unique<SpillWriter>(spillTypes, path);
    }
    SpillWriter *writer = buildWriters_[subPartition].get();
    const uint64_t vecBatchSize = Spiller::CollectVecBatchSize(BuildSpillTypes(), batch);
    AccountSpilledBytesLocked(vecBatchSize);
    auto status = writer->WriteVecBatch(batch, vecBatchSize);
    if (status != ErrorCode::SUCCESS) {
        throw omniruntime::exception::OmniException(
            "JOIN_SPILL_WRITE_FAILED", "Failed to spill build side input.");
    }
    buildSpilledRows_ += static_cast<uint64_t>(batch->GetRowCount());
    buildSpilledBytes_ += vecBatchSize;
}

void JoinSpillState::CloseBuildWriters()
{
    std::lock_guard<std::mutex> lock(mutex_);
    CloseBuildWritersLocked();
}

void JoinSpillState::CloseBuildWritersLocked()
{
    bool closeFailed = false;
    for (uint32_t sp = 0; sp < buildWriters_.size(); ++sp) {
        if (buildWriters_[sp] == nullptr) {
            continue;
        }
        ErrorCode status = ErrorCode::WRITE_FAILED;
        try {
            status = buildWriters_[sp]->Close();
        } catch (...) {
            status = ErrorCode::WRITE_FAILED;
        }
        if (status == ErrorCode::SUCCESS) {
            buildFiles_[sp].emplace_back(buildWriters_[sp]->GetSpillFileInfo());
            ++buildSpilledFiles_;
        } else {
            closeFailed = true;
            RemoveJoinSpillFile(buildWriters_[sp]->GetSpillFileInfo().filePath);
        }
        buildWriters_[sp].reset();
    }
    buildSpillWaveRunId_.reset();
    if (closeFailed) {
        throw omniruntime::exception::OmniException(
            "JOIN_SPILL_WRITE_FAILED", "Failed to close one or more build side spill writers.");
    }
}

void JoinSpillState::ClearPendingVisitedFlagsLocked(uint32_t subPartition)
{
    if (subPartition >= pendingVisitedFlags_.size()) {
        return;
    }
    for (auto *flag : pendingVisitedFlags_[subPartition]) {
        delete flag;
    }
    pendingVisitedFlags_[subPartition].clear();
}

void JoinSpillState::RestoreBuildSubPartition(HashTableVariants *hashTables, uint32_t subPartition)
{
    if (hashTables == nullptr) {
        return;
    }
    if (subPartition >= numSubPartitions_) {
        throw omniruntime::exception::OmniException(
            "JOIN_SPILL_RESTORE_FAILED", "Restore sub-partition is out of range.");
    }
    const bool needFlag = NeedsVisitedFlagSpill();
    const type::DataTypes onDiskBuildTypes = BuildSpillTypes();
    const int32_t buildColCount = buildTypes_.GetSize();
    std::vector<SpillFileInfo> files;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (subPartition >= buildFiles_.size()) {
            return;
        }
        ClearPendingVisitedFlagsLocked(subPartition);
        files.swap(buildFiles_[subPartition]);
    }
    std::visit(
        [&](auto &&arg) {
            for (const auto &file : files) {
                SpillReader reader(onDiskBuildTypes, file.filePath, file.fileLength, file.totalRowCount, false);
                bool isEnd = false;
                while (!isEnd) {
                    std::unique_ptr<vec::VectorBatch> batch = nullptr;
                    auto status = reader.ReadVecBatch(batch, isEnd);
                    if (status != ErrorCode::SUCCESS) {
                        throw omniruntime::exception::OmniException(
                            "JOIN_SPILL_READ_FAILED", "Failed to restore build side spill file.");
                    }
                    if (isEnd || batch == nullptr) {
                        break;
                    }
                    UniqueVecBatch toAdd(batch.release());
                    if (needFlag) {
                        auto peeled = PeelVisitedFlagColumn(toAdd.release(), buildColCount);
                        toAdd.reset(peeled.first);
                        std::lock_guard<std::mutex> lock(mutex_);
                        pendingVisitedFlags_[subPartition].push_back(peeled.second);
                    }
                    arg.AddVecBatch(0, toAdd.get());
                    toAdd.release();
                }
            }
        },
        *hashTables);
}

void JoinSpillState::ApplyPendingVisitedFlags(HashTableVariants *hashTables, uint32_t subPartition)
{
    if (hashTables == nullptr || !NeedsVisitedFlagSpill()) {
        return;
    }
    std::vector<vec::BaseVector *> flags;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (subPartition >= pendingVisitedFlags_.size()) {
            return;
        }
        flags = std::move(pendingVisitedFlags_[subPartition]);
        pendingVisitedFlags_[subPartition].clear();
    }
    if (flags.empty()) {
        return;
    }
    OwnedFlagColumns owned(std::move(flags));
    // This branch keeps a single hash-table slot; restore always rebuilds slot 0.
    std::visit([&](auto &&arg) { arg.ApplyVisitedFlagsFromColumns(0, owned.flags); }, *hashTables);
}

bool JoinSpillState::HasPendingBuildSpill(uint32_t subPartition) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (subPartition >= buildFiles_.size()) {
        return false;
    }
    if (!buildFiles_[subPartition].empty()) {
        return true;
    }
    return subPartition < buildWriters_.size() && buildWriters_[subPartition] != nullptr;
}

bool JoinSpillState::HasAnyPendingBuildSpill() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t sp = 0; sp < buildFiles_.size(); ++sp) {
        if (!buildFiles_[sp].empty()) {
            return true;
        }
        if (sp < buildWriters_.size() && buildWriters_[sp] != nullptr) {
            return true;
        }
    }
    return false;
}

std::vector<uint32_t> JoinSpillState::CollectBuildSpillPartitionIds() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> ids;
    for (uint32_t sp = 0; sp < buildFiles_.size(); ++sp) {
        if (!buildFiles_[sp].empty() || (sp < buildWriters_.size() && buildWriters_[sp] != nullptr)) {
            ids.push_back(sp);
        }
    }
    return SortedUnique(ids);
}

void JoinSpillState::SetInitialRuntimeLayout(JoinBuildRuntimeLayout layout)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    runtimeLayout_ = layout;
}

JoinBuildRuntimeLayout JoinSpillState::GetRuntimeLayout() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return runtimeLayout_;
}

void JoinSpillState::MarkRuntimePartitioned()
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    runtimeLayout_ = JoinBuildRuntimeLayout::Partitioned;
}

void JoinSpillState::PublishHandoffLocked(uint32_t numDistinct, const std::vector<uint32_t> &spillPartitionIds,
    const std::optional<uint32_t> &restoredPartitionId)
{
    pendingHandoff_.numDistinct = numDistinct;
    pendingHandoff_.spillPartitionIds = spillPartitionIds;
    pendingHandoff_.restoredPartitionId = restoredPartitionId;
    pendingHandoff_.handoffGeneration = ++handoffGeneration_;
    pendingHandoff_.runtimeLayout = runtimeLayout_;
    handoffReady_ = true;
    handoffCv_.notify_all();
}

void JoinSpillState::ValidateHandoffLocked(uint32_t numDistinct, const std::vector<uint32_t> &spillPartitionIds,
    const std::optional<uint32_t> &restoredPartitionId) const
{
    if (numDistinct > 0 && !spillPartitionIds.empty()) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Non-empty build table handoff requires empty spillPartitionIds.");
    }
    if (restoredPartitionId.has_value()) {
        if (!spillPartitionIds.empty()) {
            throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
                "Restore round handoff requires empty spillPartitionIds.");
        }
        if (!activeRestorePartitionId_.has_value() || *restoredPartitionId != *activeRestorePartitionId_) {
            throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
                "Restore handoff partition id mismatch.");
        }
        if (activeRestoreRoundComplete_) {
            throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
                "Restore handoff after the current restore round already completed.");
        }
    }
}

void JoinSpillState::SetHashTableHandoff(uint32_t numDistinct, const std::vector<uint32_t> &spillPartitionIds,
    const std::optional<uint32_t> &restoredPartitionId)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (handoffPhase_ == JoinHandoffPhase::Complete) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Cannot handoff after join handoff phase Complete.");
    }
    ValidateHandoffLocked(numDistinct, spillPartitionIds, restoredPartitionId);

    if (restoredPartitionId.has_value()) {
        handoffPhase_ = JoinHandoffPhase::RestoreRound;
        PublishHandoffLocked(numDistinct, {}, restoredPartitionId);
        return;
    }

    if (numDistinct == 0 && !spillPartitionIds.empty()) {
        // Empty-table handoff is normally published once from Idle. A held full table that is torn
        // under pressure re-publishes from RestoreRound and switches back to empty-table ingest.
        if (handoffPhase_ != JoinHandoffPhase::Idle && handoffPhase_ != JoinHandoffPhase::RestoreRound) {
            throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
                "Empty-table handoff can be published only from Idle or a held full table.");
        }
        spillPartitionIds_ = SortedUnique(spillPartitionIds);
        for (uint32_t id : spillPartitionIds_) {
            if (id >= numSubPartitions_) {
                throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
                    "Empty-table handoff id out of range.");
            }
        }
        restoreQueue_ = spillPartitionIds_;
        probePathAFinished_ = false;
        activeRestorePartitionId_ = std::nullopt;
        activeRestoreRoundComplete_ = true;
        handoffPhase_ = JoinHandoffPhase::ProbePathA;
        PublishHandoffLocked(0, spillPartitionIds_, std::nullopt);
        return;
    }

    handoffPhase_ = JoinHandoffPhase::RestoreRound;
    probePathAFinished_ = true;
    restoreQueue_.clear();
    spillPartitionIds_.clear();
    activeRestorePartitionId_ = std::nullopt;
    activeRestoreRoundComplete_ = false;
    PublishHandoffLocked(numDistinct, {}, std::nullopt);
}

bool JoinSpillState::TryGetHandoffForProbe(JoinHandoffSnapshot *out)
{
    if (out == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (!handoffReady_) {
        return false;
    }
    *out = pendingHandoff_;
    return true;
}

JoinHandoffSnapshot JoinSpillState::WaitHandoffForProbe()
{
    std::unique_lock<std::mutex> lock(handoffMutex_);
    handoffCv_.wait(lock, [this]() { return handoffReady_ || handoffPhase_ == JoinHandoffPhase::Complete; });
    if (!handoffReady_) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Join handoff completed without a pending handoff.");
    }
    return pendingHandoff_;
}

void JoinSpillState::AppendSpilledHashTablePartitions(const std::vector<uint32_t> &newIds)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    spillPartitionIds_ = SortedUnique(spillPartitionIds_);
    for (uint32_t id : newIds) {
        spillPartitionIds_.push_back(id);
    }
    spillPartitionIds_ = SortedUnique(spillPartitionIds_);
    restoreQueue_ = SortedUnique(restoreQueue_);
    for (uint32_t id : newIds) {
        restoreQueue_.push_back(id);
    }
    restoreQueue_ = SortedUnique(restoreQueue_);
}

std::vector<uint32_t> JoinSpillState::SpillPartitionIds() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return spillPartitionIds_;
}

void JoinSpillState::MarkProbePathAFinished()
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (handoffPhase_ != JoinHandoffPhase::ProbePathA) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "MarkProbePathAFinished outside ProbePathA phase.");
    }
    probePathAFinished_ = true;
    handoffReady_ = false;
    handoffCv_.notify_all();
}

bool JoinSpillState::IsProbePathAFinished() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return probePathAFinished_;
}

std::optional<uint32_t> JoinSpillState::NextRestorePartitionId()
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (!probePathAFinished_ || !activeRestoreRoundComplete_) {
        return std::nullopt;
    }
    if (restoreQueue_.empty()) {
        handoffPhase_ = JoinHandoffPhase::Complete;
        return std::nullopt;
    }
    const uint32_t nextId = restoreQueue_.front();
    restoreQueue_.erase(restoreQueue_.begin());
    activeRestorePartitionId_ = nextId;
    activeRestoreRoundComplete_ = false;
    handoffPhase_ = JoinHandoffPhase::RestoreRound;
    handoffReady_ = false;
    return nextId;
}

void JoinSpillState::ProbeFinished(uint32_t partitionId)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (!activeRestorePartitionId_.has_value()) {
        if (handoffPhase_ == JoinHandoffPhase::RestoreRound && restoreQueue_.empty() && probePathAFinished_) {
            handoffPhase_ = JoinHandoffPhase::Complete;
            handoffReady_ = false;
            return;
        }
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "ProbeFinished without an active restore round.");
    }
    if (*activeRestorePartitionId_ != partitionId) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "ProbeFinished partition id mismatch.");
    }
    activeRestorePartitionId_ = std::nullopt;
    activeRestoreRoundComplete_ = true;
    handoffReady_ = false;
    if (restoreQueue_.empty() && probePathAFinished_) {
        handoffPhase_ = JoinHandoffPhase::Complete;
    }
}

void JoinSpillState::AbandonProbe()
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (handoffPhase_ == JoinHandoffPhase::Complete) {
        return;
    }
    restoreQueue_.clear();
    activeRestorePartitionId_ = std::nullopt;
    activeRestoreRoundComplete_ = true;
    probePathAFinished_ = true;
    handoffReady_ = false;
    handoffPhase_ = JoinHandoffPhase::Complete;
    handoffCv_.notify_all();
}

bool JoinSpillState::BuildCanAdvanceRestore() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (handoffPhase_ == JoinHandoffPhase::Complete) {
        return true;
    }
    return probePathAFinished_ && activeRestoreRoundComplete_;
}

JoinHandoffPhase JoinSpillState::GetHandoffPhase() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return handoffPhase_;
}

uint64_t JoinSpillState::HandoffGeneration() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return handoffGeneration_;
}

void JoinSpillState::SetSpillJoinType(JoinType joinType, BuildSide buildSide)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    spillJoinType_ = joinType;
    spillJoinBuildSide_ = buildSide;
    RebuildBuildSpillTypesLocked();
}

void JoinSpillState::RebuildBuildSpillTypesLocked()
{
    needsVisitedFlagSpill_ = false;
    if (spillJoinType_.has_value()) {
        const JoinType joinType = *spillJoinType_;
        if (joinType == OMNI_JOIN_TYPE_FULL) {
            needsVisitedFlagSpill_ = true;
        } else if (spillJoinBuildSide_.has_value()) {
            const BuildSide buildSide = *spillJoinBuildSide_;
            needsVisitedFlagSpill_ =
                (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT) ||
                (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT);
        }
    }
    if (needsVisitedFlagSpill_) {
        auto types = buildTypes_.Get();
        types.emplace_back(BooleanType());
        buildSpillTypes_ = type::DataTypes(types);
    } else {
        buildSpillTypes_ = buildTypes_;
    }
    buildSpillTypesReady_ = true;
}

bool JoinSpillState::NeedsVisitedFlagSpill() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return needsVisitedFlagSpill_;
}

const type::DataTypes &JoinSpillState::BuildSpillTypes() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return buildSpillTypesReady_ ? buildSpillTypes_ : buildTypes_;
}

void JoinSpillState::SetTableInHandNone()
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    tableInHandKind_ = JoinSpillTableInHandKind::None;
    tableInHandRestoreId_.reset();
}

void JoinSpillState::SetTableInHandFullTable()
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    tableInHandKind_ = JoinSpillTableInHandKind::FullTable;
    tableInHandRestoreId_.reset();
}

void JoinSpillState::SetTableInHandRestoreSmallTable(uint32_t partitionId)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (partitionId >= numSubPartitions_) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Held restore-table partition id out of range.");
    }
    tableInHandKind_ = JoinSpillTableInHandKind::RestoreSmallTable;
    tableInHandRestoreId_ = partitionId;
}

JoinSpillTableInHand JoinSpillState::TableInHand() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    JoinSpillTableInHand view;
    view.kind = tableInHandKind_;
    view.restorePartitionId = tableInHandRestoreId_;
    return view;
}

void JoinSpillState::SetHasMoreProbeInput(bool hasMore)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    hasMoreProbeInput_ = hasMore;
}

bool JoinSpillState::HasMoreProbeInput() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return hasMoreProbeInput_;
}

void JoinSpillState::SetProbeInNrs(bool inNrs)
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    probeInNrs_ = inNrs;
}

bool JoinSpillState::ProbeInNrs() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return probeInNrs_;
}

bool JoinSpillState::IsHeldTableJoinAllowed() const
{
    if (!spillJoinType_.has_value()) {
        return false;
    }
    const JoinType joinType = *spillJoinType_;
    if (joinType == OMNI_JOIN_TYPE_INNER || joinType == OMNI_JOIN_TYPE_FULL) {
        return true;
    }
    if (!spillJoinBuildSide_.has_value()) {
        return false;
    }
    const BuildSide buildSide = *spillJoinBuildSide_;
    if (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_RIGHT) {
        return true;
    }
    if (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_LEFT) {
        return true;
    }
    if (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT) {
        return true;
    }
    if (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT) {
        return true;
    }
    if ((joinType == OMNI_JOIN_TYPE_LEFT_SEMI || joinType == OMNI_JOIN_TYPE_LEFT_ANTI ||
            joinType == OMNI_JOIN_TYPE_EXISTENCE) && buildSide == OMNI_BUILD_RIGHT) {
        return true;
    }
    return false;
}

bool JoinSpillState::CanFlushHeldTable() const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (!IsHeldTableJoinAllowed()) {
        return false;
    }
    if (tableInHandKind_ == JoinSpillTableInHandKind::None) {
        return false;
    }
    // Debounce: a restore bucket flushed once in this session is not flushed again, so a
    // persistent memory pressure cannot loop (tear -> replay -> tear) on the same bucket.
    if (tableInHandKind_ == JoinSpillTableInHandKind::RestoreSmallTable &&
        tableInHandRestoreId_.has_value() &&
        flushedRestorePartitions_.count(*tableInHandRestoreId_) > 0) {
        return false;
    }
    if (probeInNrs_ || !hasMoreProbeInput_) {
        return false;
    }
    // Post-handoff only: Idle is build-ingest; ProbePathA is empty-table ingest.
    if (handoffPhase_ != JoinHandoffPhase::RestoreRound) {
        return false;
    }
    return true;
}

void JoinSpillState::PublishHeldTableFlush(const std::vector<uint32_t> &ids)
{
    const auto sorted = SortedUnique(ids);
    if (sorted.empty()) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Held-table flush requires non-empty spill partition ids.");
    }
    for (uint32_t id : sorted) {
        if (id >= numSubPartitions_) {
            throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
                "Held-table flush id out of range.");
        }
    }

    JoinSpillTableInHandKind kind;
    {
        std::lock_guard<std::mutex> lock(handoffMutex_);
        kind = tableInHandKind_;
    }
    if (kind == JoinSpillTableInHandKind::None) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Held-table flush without a table in hand.");
    }
    if (kind == JoinSpillTableInHandKind::FullTable) {
        // Re-publish as an empty-table handoff (RestoreRound -> ProbePathA).
        SetHashTableHandoff(0, sorted);
    } else {
        // Keep RestoreRound; re-queue the flushed bucket for a re-run restore round.
        AppendSpilledHashTablePartitions(sorted);
    }

    std::lock_guard<std::mutex> lock(handoffMutex_);
    if (kind == JoinSpillTableInHandKind::RestoreSmallTable && tableInHandRestoreId_.has_value()) {
        flushedRestorePartitions_.insert(*tableInHandRestoreId_);
    }
    tableInHandKind_ = JoinSpillTableInHandKind::None;
    tableInHandRestoreId_.reset();
}

void JoinSpillState::BindSpillConfig(std::shared_ptr<SparkSpillConfig> spillConfig)
{
    spillConfig_ = std::move(spillConfig);
}

void JoinSpillState::AccountSpilledBytesLocked(uint64_t bytes)
{
    if (bytes == 0 || spillConfig_ == nullptr) {
        return;
    }
    const uint64_t limit = spillConfig_->GetMaxSpillBytes();
    if (cappedSpillBytes_ > limit || bytes > limit - cappedSpillBytes_) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_WRITE_FAILED",
            "Join spill exceeded maxSpillBytes.");
    }
    cappedSpillBytes_ += bytes;
}

void JoinSpillState::AccountSpilledBytes(uint64_t bytes)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AccountSpilledBytesLocked(bytes);
}

void JoinSpillState::RemoveBuildSpillFilesLocked()
{
    for (auto &files : buildFiles_) {
        for (const auto &file : files) {
            RemoveJoinSpillFile(file.filePath);
        }
        files.clear();
    }
}

void RemoveJoinSpillFile(const std::string &path)
{
    if (path.empty()) {
        return;
    }
    if (std::remove(path.c_str()) != 0 && errno != ENOENT) {
        LogWarn("Failed to remove join spill file %s.", path.c_str());
    }
}

void JoinSpillState::SetSpillMemThresholdOverride(std::optional<int64_t> bytes)
{
    spillMemThresholdOverride_ = bytes;
}

void JoinSpillState::SetBuildHashCols(std::vector<int32_t> hashCols)
{
    buildHashCols_ = std::move(hashCols);
}

bool JoinSpillState::AccountedMemoryOverThreshold() const
{
    if (spillConfig_ == nullptr || !spillConfig_->IsSpillEnabled()) {
        return false;
    }
    return mem::MemoryManager::GetGlobalAccountedMemory() >= SpillMemThreshold();
}

int64_t JoinSpillState::SpillMemThreshold() const
{
    if (spillMemThresholdOverride_.has_value()) {
        return *spillMemThresholdOverride_;
    }
    if (spillConfig_ == nullptr) {
        return 0;
    }
    return spillConfig_->GetSpillMemThreshold();
}

bool JoinSpillState::WasRestorePartitionFlushed(uint32_t partitionId) const
{
    std::lock_guard<std::mutex> lock(handoffMutex_);
    return flushedRestorePartitions_.count(partitionId) > 0;
}

void JoinSpillState::FlushHeldBuildTable(HashTableVariants *hashTables, const JoinSubPartitionConfig &cfg)
{
    if (hashTables == nullptr) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Held-table flush without a hash table.");
    }
    const auto hand = TableInHand();
    if (hand.kind == JoinSpillTableInHandKind::None) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Held-table flush without a table in hand.");
    }
    if (NeedsVisitedFlagSpill()) {
        std::visit([&](auto &&arg) { arg.AppendVisitedFlagsToResident(0); }, *hashTables);
    }
    OwnedVecBatchList resident(std::visit([&](auto &&arg) { return arg.TakeResidentBatches(0); }, *hashTables));
    const uint64_t runId = NextRunId();
    std::vector<uint32_t> ids;
    if (hand.kind == JoinSpillTableInHandKind::FullTable) {
        JoinSubPartitioner partitioner(cfg);
        for (auto *source : resident.batches) {
            if (source == nullptr || source->GetRowCount() <= 0) {
                resident.Free(source);
                continue;
            }
            const auto buckets = partitioner.PartitionRowsByKeyColumns(source, buildTypes_, buildHashCols_);
            std::vector<std::vector<int32_t>> positions(cfg.numSubPartitions);
            for (size_t row = 0; row < buckets.size(); ++row) {
                positions[buckets[row]].push_back(static_cast<int32_t>(row));
            }
            const int32_t vectorCount = source->GetVectorCount();
            for (uint32_t subPartition = 0; subPartition < positions.size(); ++subPartition) {
                auto &bucketPositions = positions[subPartition];
                if (bucketPositions.empty()) {
                    continue;
                }
                UniqueVecBatch subBatch(new vec::VectorBatch(static_cast<int32_t>(bucketPositions.size())));
                subBatch->ResizeVectorCount(vectorCount);
                for (int32_t col = 0; col < vectorCount; ++col) {
                    subBatch->SetVector(col, vec::VectorHelper::CopyPositionsVector(
                        source->Get(col), bucketPositions.data(), 0, static_cast<int32_t>(bucketPositions.size())));
                }
                SpillBuildSubPartition(runId, subPartition, subBatch.get());
            }
            resident.Free(source);
        }
        MarkRuntimePartitioned();
        CloseBuildWriters();
        ids = CollectBuildSpillPartitionIds();
    } else {
        const uint32_t dest = *hand.restorePartitionId;
        for (auto *source : resident.batches) {
            if (source != nullptr) {
                SpillBuildSubPartition(runId, dest, source);
            }
            resident.Free(source);
        }
        CloseBuildWriters();
        ids = {dest};
    }
    std::visit([&](auto &&arg) { arg.ResetPartition(0); }, *hashTables);
    if (ids.empty()) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_HANDOFF_INVALID",
            "Held-table flush produced no spill partition ids.");
    }
    PublishHeldTableFlush(ids);
}

void LogJoinSpillMemCheckpoint(const char *site, int64_t accounted, int64_t threshold, bool overThreshold,
    const char *action, const char *detail)
{
    if (!IsDebugEnable()) {
        return;
    }
    std::cout << "[OmniRuntime][JoinSpill][mem_checkpoint] site=" << (site != nullptr ? site : "?")
              << " accounted=" << accounted << " threshold=" << threshold << " over=" << (overThreshold ? 1 : 0)
              << " action=" << (action != nullptr ? action : "?");
    if (detail != nullptr && detail[0] != '\0') {
        std::cout << " " << detail;
    }
    std::cout << std::endl;
    std::cout.flush();
}

} // namespace omniruntime::op
