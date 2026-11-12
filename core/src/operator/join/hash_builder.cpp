/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#include "hash_builder.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <memory>
#include <utility>
#include "join_spill_state.h"
#include "vector/vector_helper.h"
#include "memory/memory_manager.h"

using omniruntime::vec::VectorHelper;
using omniruntime::vec::VectorBatch;

namespace omniruntime {
namespace op {
void HashBuilderOperatorFactory::SetJoinSpillPolicy(bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg)
{
    joinSpillEnabled_ = joinSpillEnabled;
    joinSubPartCfg_ = joinSubPartCfg;
}

void HashBuilderOperatorFactory::SyncSpillBindingsToState()
{
    if (joinSpillState_ == nullptr) {
        return;
    }
    joinSpillState_->SetBuildHashCols(buildHashCols);
    if (spillConfig_ != nullptr) {
        joinSpillState_->BindSpillConfig(spillConfig_);
    }
    joinSpillState_->SetSpillMemThresholdOverride(joinSpillMemThresholdForTest_);
}

void HashBuilderOperatorFactory::SetJoinSpillState(std::shared_ptr<JoinSpillState> joinSpillState)
{
    joinSpillState_ = std::move(joinSpillState);
    SyncSpillBindingsToState();
}

void HashBuilderOperatorFactory::SetSpillConfig(std::shared_ptr<SparkSpillConfig> spillConfig)
{
    spillConfig_ = std::move(spillConfig);
    SyncSpillBindingsToState();
}

void HashBuilderOperatorFactory::SetJoinSpillMemThresholdForTest(int64_t bytes)
{
    joinSpillMemThresholdForTest_ = bytes;
    SyncSpillBindingsToState();
}

HashBuilderOperatorFactory::HashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
    const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount)
    : buildTypes(buildTypes),
      buildHashCols(std::vector<int32_t>(buildHashCols, buildHashCols + buildHashColsCount)),
      operatorIndex(0)
{
    if (operatorCount <= 0 || operatorCount > 10000) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", "operatorCount is not in the acceptable range [1, 10000].");
    }
    if (joinType != OMNI_JOIN_TYPE_FULL) {
        hashTablesVariants = InitVariant<RowRefList>(buildHashColsCount, operatorCount, joinType);
    } else {
        hashTablesVariants = InitVariant<RowRefListWithFlags>(buildHashColsCount, operatorCount, joinType);
    }
}

HashBuilderOperatorFactory::HashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide,
    const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount)
    : buildTypes(buildTypes),
      buildHashCols(std::vector<int32_t>(buildHashCols, buildHashCols + buildHashColsCount)),
      operatorIndex(0)
{
    if (operatorCount <= 0 || operatorCount > 10000) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", "operatorCount is not in the acceptable range [1, 10000].");
    }
    if (joinType == OMNI_JOIN_TYPE_FULL || (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT)
        || (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT)) {
        hashTablesVariants = InitVariant<RowRefListWithFlags>(buildHashColsCount, operatorCount, joinType, buildSide);
    } else {
        hashTablesVariants = InitVariant<RowRefList>(buildHashColsCount, operatorCount, joinType, buildSide);
    }
}

template <class RowRefListType>
HashTableVariants *HashBuilderOperatorFactory::InitVariant(int32_t buildHashColsCount, int32_t operatorCount,
    JoinType joinType, BuildSide buildSide, bool isMultiCols)
{
    if (buildHashColsCount == 1) {
        auto type = buildTypes.GetIds()[buildHashCols[0]];
        switch (type) {
            case OMNI_BOOLEAN:
            case OMNI_BYTE:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int8_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_SHORT:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int16_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_FLOAT:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int32_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
            case OMNI_DOUBLE:
            case OMNI_DATE64:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int64_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_DECIMAL128:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<Decimal128, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_ARRAY:
            default:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<StringRef, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
        }
    } else {
        int32_t lengthCount = GetTypeLength(buildHashColsCount, buildTypes, buildHashCols);
        if (0 < lengthCount && lengthCount <= BITS_OF_INT) {
            return new HashTableVariants{std::in_place_type<JoinHashTableVariants<int32_t, RowRefListType>>,
                operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide, true };
        } else if (BITS_OF_INT < lengthCount && lengthCount <= BITS_OF_LONG) {
            return new HashTableVariants{std::in_place_type<JoinHashTableVariants<int64_t, RowRefListType>>,
                operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide, true };
        } else if (BITS_OF_LONG < lengthCount && lengthCount <= BITS_OF_LONGLONG) {
            return new HashTableVariants{std::in_place_type<JoinHashTableVariants<int128_t, RowRefListType>>,
                operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide, true };
        }
        return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<StringRef, RowRefListType>>,
            operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
    }
}

int32_t GetTypeLength(int buildHashColsCount, DataTypes& buildTypes, std::vector<int32_t>& buildHashCols)
{
    int32_t lengthCount = 0;
    for (int i = 0; i < buildHashColsCount; i++) {
        switch (buildTypes.GetIds()[buildHashCols[i]]) {
            case OMNI_BOOLEAN:
            case OMNI_BYTE:
                lengthCount += BITS_OF_BYTE;
                break;
            case OMNI_SHORT:
                lengthCount += BITS_OF_SHORT;
                break;
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_FLOAT:
                lengthCount += BITS_OF_INT;
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
            case OMNI_DOUBLE:
            case OMNI_DATE64:
                lengthCount += BITS_OF_LONG;
                break;
            default:
                return NOT_EXPECTED_TYPE;
        }
    }
    return lengthCount;
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(JoinType joinType,
    const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount)
{
    return new HashBuilderOperatorFactory(joinType, buildTypes, buildHashCols, buildHashColsCount, operatorCount);
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(JoinType joinType,
    BuildSide buildSide, const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount,
    int32_t operatorCount)
{
    return new HashBuilderOperatorFactory(joinType, buildSide, buildTypes, buildHashCols, buildHashColsCount,
        operatorCount);
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, int32_t operatorCount)
{
    // Extract necessary information from planNode
    auto joinType = planNode->GetJoinType();
    auto buildSide = planNode->GetBuildSide();
    auto buildTypes = planNode->RightOutputType();

    auto buildKeysSize = planNode->RightKeys().size();
    std::vector<int32_t> buildHashCols;
    for (size_t index = 0; index < buildKeysSize; index++) {
        auto key = dynamic_cast<FieldExpr *>(planNode->RightKeys()[index]);
        buildHashCols.emplace_back(key->colVal);
    }

    auto buildHashColsCount = (int32_t) buildHashCols.size();
    return new HashBuilderOperatorFactory(joinType, buildSide, *buildTypes, buildHashCols.data(), buildHashColsCount, operatorCount);
}

Operator *HashBuilderOperatorFactory::CreateOperator()
{
    int32_t partitionIndex =
        operatorIndex++ % std::visit([&](auto &&arg) { return arg.GetHashTableCount(); }, *hashTablesVariants);
    auto *op = new HashBuilderOperator(this->buildTypes, hashTablesVariants, partitionIndex, joinSpillEnabled_,
        joinSubPartCfg_, buildHashCols, joinSpillState_.get(), spillConfig_.get(), joinSpillMemThresholdForTest_);
    op->KeepJoinSpillState(joinSpillState_);
    return op;
}

void HashBuilderOperator::KeepJoinSpillState(std::shared_ptr<JoinSpillState> state)
{
    joinSpillStateKeep_ = std::move(state);
    if (joinSpillState_ == nullptr) {
        joinSpillState_ = joinSpillStateKeep_.get();
    }
}

HashBuilderOperator::~HashBuilderOperator() = default;

HashBuilderOperator::HashBuilderOperator(const DataTypes &buildTypes, HashTableVariants *hashTables,
    int32_t partitionIndex, bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg,
    std::vector<int32_t> buildHashCols, JoinSpillState *joinSpillState, SparkSpillConfig *spillConfig,
    std::optional<int64_t> spillMemThresholdOverride)
    : buildTypes(buildTypes),
      partitionIndex(partitionIndex),
      hashTablesVariants(hashTables),
      joinSpillEnabled_(joinSpillEnabled),
      joinSubPartCfg_(joinSubPartCfg),
      buildHashCols_(std::move(buildHashCols)),
      joinSpillState_(joinSpillState),
      spillConfig_(spillConfig),
      spillMemThresholdOverride_(spillMemThresholdOverride)
{
    SetOperatorName(opNameForHashBuilder);
}

void HashBuilderOperator::AddLatchedInput(omniruntime::vec::VectorBatch *vecBatch)
{
    UniqueVecBatch owned(vecBatch);
    JoinSubPartitioner partitioner(joinSubPartCfg_);
    const auto buckets = partitioner.PartitionRowsByKeyColumns(owned.get(), buildTypes, buildHashCols_);
    std::vector<std::vector<int32_t>> positions(joinSubPartCfg_.numSubPartitions);
    for (size_t row = 0; row < buckets.size(); ++row) {
        positions[buckets[row]].push_back(static_cast<int32_t>(row));
    }
    const uint64_t runId = joinSpillState_->NextRunId();
    const int32_t vectorCount = owned->GetVectorCount();
    for (uint32_t subPartition = 0; subPartition < positions.size(); ++subPartition) {
        auto &bucketPositions = positions[subPartition];
        if (bucketPositions.empty()) {
            continue;
        }
        UniqueVecBatch subBatch(new omniruntime::vec::VectorBatch(static_cast<int32_t>(bucketPositions.size())));
        subBatch->ResizeVectorCount(vectorCount);
        for (int32_t col = 0; col < vectorCount; ++col) {
            subBatch->SetVector(col, omniruntime::vec::VectorHelper::CopyPositionsVector(
                owned->Get(col), bucketPositions.data(), 0, static_cast<int32_t>(bucketPositions.size())));
        }
        joinSpillState_->SpillBuildSubPartition(runId, subPartition, subBatch.get());
    }
    ResetInputVecBatch();
}

/// After the current batch is held, spill when accounted memory crosses the threshold.
void HashBuilderOperator::MaybeTriggerSpill()
{
    if (!joinSpillEnabled_) {
        return;
    }
    const int64_t accounted = mem::MemoryManager::GetGlobalAccountedMemory();
    int64_t threshold = 0;
    if (spillConfig_ != nullptr) {
        threshold = spillMemThresholdOverride_.has_value() ? *spillMemThresholdOverride_ :
            spillConfig_->GetSpillMemThreshold();
    }
    if (joinSpillState_ == nullptr || buildSpillTriggered_ || spillConfig_ == nullptr) {
        LogJoinSpillMemCheckpoint("build_addinput", accounted, threshold, accounted >= threshold, "skip",
            joinSpillState_ == nullptr        ? "reason=no_state" :
            buildSpillTriggered_            ? "reason=already_spilled" :
                                              "reason=no_spill_config");
        return;
    }
    if (!spillConfig_->IsSpillEnabled()) {
        LogJoinSpillMemCheckpoint("build_addinput", accounted, threshold, accounted >= threshold, "skip",
            "reason=spill_disabled");
        return;
    }
    if (accounted < threshold) {
        LogJoinSpillMemCheckpoint("build_addinput", accounted, threshold, false, "check", "reason=under_threshold");
        return;
    }
    LogJoinSpillMemCheckpoint("build_addinput", accounted, threshold, true, "build_spill", nullptr);
    SpillAndClearResidentBuild();
}

/// Before Prepare/BuildHashTable: held batches are already in GetGlobalAccountedMemory
/// (process-wide, not reclaimableBytes). Forecast HT directory + row-list arena that
/// open would allocate; spill if accounted + those two would cross the join mem threshold.
void HashBuilderOperator::MaybeSpillBeforeOpenSlot0()
{
    if (!joinSpillEnabled_ || buildSpillTriggered_ || joinSpillState_ == nullptr || spillConfig_ == nullptr) {
        return;
    }
    if (!spillConfig_->IsSpillEnabled()) {
        return;
    }
    const uint32_t rows = ResidentRowCountSlot0();
    if (rows == 0) {
        return;
    }
    const int64_t accounted = mem::MemoryManager::GetGlobalAccountedMemory();
    const int64_t estHt = std::visit(
        [](auto &&arg) { return arg.EstimateHashTableInitBytes(0); }, *hashTablesVariants);
    const int64_t estArena = std::visit(
        [](auto &&arg) { return arg.EstimateRowRefArenaBytes(0); }, *hashTablesVariants);
    const int64_t forecast = accounted + estHt + estArena;
    const int64_t threshold = joinSpillState_->SpillMemThreshold();
    char detail[128];
    std::snprintf(detail, sizeof(detail), "reason=eob_open_ht rows=%u ht=%ld arena=%ld forecast=%ld", rows, estHt,
        estArena, forecast);
    if (forecast < threshold) {
        LogJoinSpillMemCheckpoint("build_eob_open", accounted, threshold, false, "check", detail);
        return;
    }
    LogJoinSpillMemCheckpoint("build_eob_open", accounted, threshold, true, "build_spill", detail);
    SpillAndClearResidentBuild();
}

/// Spill the in-memory build (partition on first spill) and clear resident data.
void HashBuilderOperator::SpillAndClearResidentBuild()
{
    OwnedVecBatchList resident(std::visit([&](auto &&arg) { return arg.TakeResidentBatches(0); }, *hashTablesVariants));
    const uint64_t runId = joinSpillState_->NextRunId();
    JoinSubPartitioner partitioner(joinSubPartCfg_);
    for (auto *source : resident.batches) {
        if (source == nullptr || source->GetRowCount() <= 0) {
            resident.Free(source);
            continue;
        }
        const auto buckets = partitioner.PartitionRowsByKeyColumns(source, buildTypes, buildHashCols_);
        std::vector<std::vector<int32_t>> positions(joinSubPartCfg_.numSubPartitions);
        for (size_t row = 0; row < buckets.size(); ++row) {
            positions[buckets[row]].push_back(static_cast<int32_t>(row));
        }
        const int32_t vectorCount = source->GetVectorCount();
        for (uint32_t subPartition = 0; subPartition < positions.size(); ++subPartition) {
            auto &bucketPositions = positions[subPartition];
            if (bucketPositions.empty()) {
                continue;
            }
            UniqueVecBatch subBatch(new omniruntime::vec::VectorBatch(static_cast<int32_t>(bucketPositions.size())));
            subBatch->ResizeVectorCount(vectorCount);
            for (int32_t col = 0; col < vectorCount; ++col) {
                subBatch->SetVector(col, omniruntime::vec::VectorHelper::CopyPositionsVector(
                    source->Get(col), bucketPositions.data(), 0, static_cast<int32_t>(bucketPositions.size())));
            }
            joinSpillState_->SpillBuildSubPartition(runId, subPartition, subBatch.get());
        }
        resident.Free(source);
    }
    ResetSlot0();
    joinSpillState_->MarkRuntimePartitioned();
    buildSpillTriggered_ = true;
}

void HashBuilderOperator::ResetSlot0()
{
    std::visit([&](auto &&arg) { arg.ResetPartition(0); }, *hashTablesVariants);
}

uint32_t HashBuilderOperator::ResidentRowCountSlot0() const
{
    return std::visit([&](auto &&arg) -> uint32_t { return arg.GetTotalRowCount(0); }, *hashTablesVariants);
}

uint32_t HashBuilderOperator::DistinctCountSlot0() const
{
    return std::visit(
        [&](auto &&arg) -> uint32_t {
            if (arg.GetHashTableTypes(0) == HashTableImplementationType::NORMAL_HASH_TABLE &&
                arg.GetHashTable(0) != nullptr) {
                return static_cast<uint32_t>(arg.GetHashTable(0)->GetElementsSize());
            }
            if (arg.GetArrayTable(0) != nullptr) {
                return static_cast<uint32_t>(arg.GetArrayTable(0)->GetElementsSize());
            }
            return arg.GetTotalRowCount(0);
        },
        *hashTablesVariants);
}

void HashBuilderOperator::PrepareAndBuildSlot0()
{
    std::visit(
        [&](auto &&arg) {
            arg.Prepare(0);
            arg.BuildHashTable(0);
        },
        *hashTablesVariants);
}

void HashBuilderOperator::PrepareAndBuildSlot0Adaptive()
{
    std::visit(
        [&](auto &&arg) {
            arg.Prepare(0);
            arg.BuildHashTableAdaptive(0);
        },
        *hashTablesVariants);
}

/// After RestoreBuildSubPartition, slot 0 holds only the current SP.
/// Size the HT from those rows when forecast stays under the join spill threshold;
/// use Adaptive only when accounted + HT + arena would cross it.
void HashBuilderOperator::PrepareAndBuildSlot0ForRestore()
{
    lastRestoreUsedAdaptiveOpen_ = false;
    const uint32_t rows = ResidentRowCountSlot0();
    if (rows == 0) {
        PrepareAndBuildSlot0();
        return;
    }
    const int64_t accounted = mem::MemoryManager::GetGlobalAccountedMemory();
    const int64_t estHt = std::visit(
        [](auto &&arg) { return arg.EstimateHashTableInitBytes(0); }, *hashTablesVariants);
    const int64_t estArena = std::visit(
        [](auto &&arg) { return arg.EstimateRowRefArenaBytes(0); }, *hashTablesVariants);
    const int64_t forecast = accounted + estHt + estArena;
    const int64_t threshold = joinSpillState_ != nullptr ? joinSpillState_->SpillMemThreshold() : 0;
    const bool useAdaptive = threshold > 0 && forecast >= threshold;
    char detail[160];
    std::snprintf(detail, sizeof(detail),
        "reason=restore_open_sp rows=%u ht=%ld arena=%ld forecast=%ld adaptive=%d", rows, estHt, estArena,
        forecast, useAdaptive ? 1 : 0);
    LogJoinSpillMemCheckpoint("build_restore_open", accounted, threshold, useAdaptive,
        useAdaptive ? "adaptive" : "sized", detail);
    lastRestoreUsedAdaptiveOpen_ = useAdaptive;
    if (useAdaptive) {
        PrepareAndBuildSlot0Adaptive();
    } else {
        PrepareAndBuildSlot0();
    }
}

/// Publish an empty hash table plus spilled sub-partition ids for probe restore.
/// If some SPs already spilled but resident build remains, empty-table handoff would
/// drop those rows: restore queue = ids only, and LookupOuterJoin is skipped.
void HashBuilderOperator::TryPublishEmptyTableHandoff()
{
    if (joinSpillState_ == nullptr) {
        return;
    }
    joinSpillState_->CloseBuildWriters();
    auto ids = joinSpillState_->CollectBuildSpillPartitionIds();
    const uint32_t leftover = ResidentRowCountSlot0();
    if (!ids.empty() && leftover > 0) {
        const int64_t accounted = mem::MemoryManager::GetGlobalAccountedMemory();
        const int64_t threshold = joinSpillState_->SpillMemThreshold();
        char detail[128];
        std::snprintf(detail, sizeof(detail), "reason=empty_handoff_leftover rows=%u", leftover);
        LogJoinSpillMemCheckpoint("build_eob_open", accounted, threshold, true, "build_spill", detail);
        SpillAndClearResidentBuild();
        joinSpillState_->CloseBuildWriters();
        ids = joinSpillState_->CollectBuildSpillPartitionIds();
    }
    if (ids.empty()) {
        return;
    }
    joinSpillState_->SetHashTableHandoff(0, ids);
    std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);
}

void HashBuilderOperator::FlushHeldBuildTable()
{
    if (joinSpillState_ == nullptr) {
        return;
    }
    joinSpillState_->FlushHeldBuildTable(hashTablesVariants, joinSubPartCfg_);
}

bool HashBuilderOperator::TryProduceNextRestoreRound()
{
    const auto nextId = joinSpillState_->NextRestorePartitionId();
    if (!nextId.has_value()) {
        return false;
    }
    ResetSlot0();
    joinSpillState_->RestoreBuildSubPartition(hashTablesVariants, *nextId);
    PrepareAndBuildSlot0ForRestore();
    // Restore peels the on-disk visited column; apply it only after the table exists.
    joinSpillState_->ApplyPendingVisitedFlags(hashTablesVariants, *nextId);
    const uint32_t numDistinct = DistinctCountSlot0();
    joinSpillState_->SetHashTableHandoff(numDistinct, {}, nextId);
    std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);
    return true;
}

int32_t HashBuilderOperator::DriveHandoffRestore()
{
    restoreDriving_ = true;
    if (joinSpillState_->GetHandoffPhase() == JoinHandoffPhase::Idle) {
        MaybeSpillBeforeOpenSlot0();
        TryPublishEmptyTableHandoff();
        if (joinSpillState_->GetHandoffPhase() == JoinHandoffPhase::Idle) {
            PrepareAndBuildSlot0();
            const uint32_t numDistinct = DistinctCountSlot0();
            joinSpillState_->SetHashTableHandoff(numDistinct, {});
            std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);
        }
    }
    if (joinSpillState_->GetHandoffPhase() == JoinHandoffPhase::Complete) {
        SetStatus(OMNI_STATUS_FINISHED);
        std::visit([&](auto &&arg) {
            arg.SetStatus(OMNI_STATUS_FINISHED);
            arg.ReleaseBuildState();
        }, *hashTablesVariants);
        return 0;
    }
    if (joinSpillState_->BuildCanAdvanceRestore()) {
        if (!TryProduceNextRestoreRound() &&
            joinSpillState_->GetHandoffPhase() == JoinHandoffPhase::Complete) {
            SetStatus(OMNI_STATUS_FINISHED);
            std::visit([&](auto &&arg) {
                arg.SetStatus(OMNI_STATUS_FINISHED);
                arg.ReleaseBuildState();
            }, *hashTablesVariants);
            return 0;
        }
    }
    SetStatus(OMNI_STATUS_NORMAL);
    return 0;
}

int32_t HashBuilderOperator::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    UpdateAddInputInfo(rowCount);
    if (joinSpillEnabled_ && joinSpillState_ != nullptr) {
        // Hold the build in one hash table until memory NeedSpill; do not partition the batch first.
        if (buildSpillTriggered_) {
            AddLatchedInput(vecBatch);
            return 0;
        }
        std::visit([&](auto &&arg) { arg.AddVecBatch(0, vecBatch); }, *hashTablesVariants);
        MaybeTriggerSpill();
        return 0;
    }
    std::visit([&](auto &&arg) { arg.AddVecBatch(partitionIndex, vecBatch); }, *hashTablesVariants);
    return 0;
}

int32_t HashBuilderOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    (void)outputVecBatch;
    // Assign, do not add: these counters are session totals. Refresh on entry
    // and again before return. DriveHandoffRestore and CloseBuildWriters can
    // add files after the first assign. The driver snapshots stats before Close().
    auto refreshSpillStats = [this]() {
        if (joinSpillState_ == nullptr) {
            return;
        }
        stats_.spilledBytes = joinSpillState_->BuildSpilledBytes();
        stats_.spilledRows = joinSpillState_->BuildSpilledRows();
        stats_.spilledFiles = joinSpillState_->BuildSpilledFiles();
    };
    refreshSpillStats();
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    if (this->isFinished()) {
        return 0;
    }
    if (joinSpillEnabled_ && joinSpillState_ != nullptr) {
        const int32_t rc = DriveHandoffRestore();
        refreshSpillStats();
        return rc;
    }

    std::visit(
        [&](auto &&arg) {
            arg.Prepare(partitionIndex);
            arg.BuildHashTable(partitionIndex);
        },
        *hashTablesVariants);
    if (UNLIKELY(IsDebugEnable())) {
        int32_t hashTableSize = 0;
        auto hasgTableType =
            std::visit([&](auto &&arg) { return arg.GetHashTableTypes(partitionIndex); }, *hashTablesVariants);
        if (hasgTableType == HashTableImplementationType::NORMAL_HASH_TABLE) {
            hashTableSize = std::visit([&](auto &&arg) { return arg.GetHashTable(partitionIndex)->GetElementsSize(); },
                *hashTablesVariants);
        } else {
            hashTableSize = std::visit([&](auto &&arg) { return arg.GetArrayTable(partitionIndex)->GetElementsSize(); },
                *hashTablesVariants);
        }
        UpdateGetOutputInfo(hashTableSize);
    }
    SetStatus(OMNI_STATUS_FINISHED);
    std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);
    refreshSpillStats();
    return 0;
}

BlockingReason HashBuilderOperator::IsBlocked(ContinueFuture *future)
{
    (void)future;
    if (!joinSpillEnabled_ || joinSpillState_ == nullptr || !restoreDriving_) {
        return BlockingReason::kNotBlocked;
    }
    const auto phase = joinSpillState_->GetHandoffPhase();
    if (phase == JoinHandoffPhase::Complete) {
        return BlockingReason::kNotBlocked;
    }
    if (!joinSpillState_->BuildCanAdvanceRestore()) {
        return BlockingReason::kWaitForJoinProbe;
    }
    return BlockingReason::kNotBlocked;
}

OmniStatus HashBuilderOperator::Close()
{
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}
} // end of op
} // end of omniruntime