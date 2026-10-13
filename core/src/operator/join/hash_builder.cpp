/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#include "hash_builder.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>
#include <memory>
#include <utility>
#include <thread>
#include "broadcast_hash_table_cache.h"
#include "join_spill_state.h"
#include "operator/spill/spiller.h"
#include "vector/vector_helper.h"
#include "operator/reclaim_context.h"

using omniruntime::vec::VectorHelper;
using omniruntime::vec::VectorBatch;

namespace omniruntime {
namespace op {
void HashBuilderOperatorFactory::SetJoinSpillPolicy(bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg)
{
    joinSpillEnabled_ = joinSpillEnabled;
    joinSubPartCfg_ = joinSubPartCfg;
}

void HashBuilderOperatorFactory::SetJoinSpillSubPartitionPolicy(bool enabled, uint64_t,
    JoinSubPartitionConfig cfg, bool passiveReclaimEnabled,
    bool forceBuildGroupSpillAfterNoMoreInput, int32_t)
{
    SetJoinSpillPolicy(enabled, cfg);
    passiveReclaimEnabled_ = passiveReclaimEnabled;
    forceBuildGroupSpillAfterNoMoreInput_ = forceBuildGroupSpillAfterNoMoreInput;
}

void HashBuilderOperatorFactory::SetJoinSpillHandoffSessionEnabled(bool enabled)
{
    if (joinSpillState_ != nullptr) joinSpillState_->SetHandoffSessionEnabled(enabled);
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
    std::visit([&](auto &&arg) {
        joinSpillState_->SetSpillJoinType(arg.GetJoinType(), arg.GetBuildSide());
    }, *hashTablesVariants);
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

/// operatorCount is equal to HashTableCount
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
    return new HashBuilderOperatorFactory(joinType, buildSide, *buildTypes, buildHashCols.data(), buildHashColsCount,
        operatorCount);
}

HashBuilderOperatorFactory::HashBuilderOperatorFactory(
    const DataTypes &buildTypes, HashTableVariants *cachedVariants)
    : buildTypes(buildTypes),
      hashTablesVariants(cachedVariants),
      operatorIndex(0),
      prebuilt_(true),
      ownsVariants_(false)
{
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateFromCachedVariants(
    std::shared_ptr<const HashJoinNode> planNode, HashTableVariants* cachedVariants)
{
    auto* factory = new HashBuilderOperatorFactory(*planNode->RightOutputType(), cachedVariants);
    return factory;
}

void HashBuilderOperatorFactory::PublishCachedVariants()
{
    auto ticket = cacheBuildTicket_;
    auto slot = cacheLeaseSlot_.lock();
    if (ticket == nullptr || slot == nullptr) {
        throw std::logic_error("Broadcast cache build lost its ticket or probe lease");
    }
    // Cache readers may request dynamic filters independently. Finalize this
    // shared metadata while the builder still has exclusive ownership, so
    // readers never lazily mutate a published table.
    std::visit([](auto &table) { table.BuildDynamicFilters(); }, *hashTablesVariants);
    // Allocation may fail here; the factory still owns its table and the
    // ticket's destructor then wakes waiters. Everything below is a
    // non-allocating ownership transfer.
    auto prepared = ticket->prepare();
    cacheBuildTicket_.reset();
    auto* variants = ReleaseVariants();
    MarkCachePinned();
    ticket->publish(prepared, variants, this);
    slot->lease = std::move(prepared);
}

void HashBuilderOperatorFactory::SetDynamicFilterPushdownEnabled(bool enabled)
{
    dynamicFilterPushdownEnabled_ = enabled;
    // Cache publication already finalized shared filter metadata. This flag
    // only controls task-local use and non-cached build-time generation.
}

void HashBuilderOperatorFactory::InjectCachedVariants(
    HashTableVariants* cachedVariants, const std::string& broadcastHashTableId)
{
    if (ownsVariants_ && hashTablesVariants != nullptr) {
        delete hashTablesVariants;
    }
    hashTablesVariants = cachedVariants;
    ownsVariants_ = false;
    prebuilt_ = true;
    broadcastHashTableId_ = broadcastHashTableId;
}

Operator *HashBuilderOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    /// operatorIndex is start by 0, so partitionIdex is start by 0
    int32_t partitionIndex =
        operatorIndex++ % std::visit([&](auto &&arg) { return arg.GetHashTableCount(); }, *hashTablesVariants);
    auto *op = new HashBuilderOperator(pool, this->buildTypes, hashTablesVariants, partitionIndex, joinSpillEnabled_,
        joinSubPartCfg_, buildHashCols, joinSpillState_.get(), passiveReclaimEnabled_,
        forceBuildGroupSpillAfterNoMoreInput_,
        prebuilt_, broadcastHashTableId_, this, broadcastParallelBuildPolicy_, dynamicFilterPushdownEnabled_);
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

HashBuilderOperator::~HashBuilderOperator()
{
    if (spillableRegistered_) pool()->unregisterSpillable(this);
}

HashBuilderOperator::HashBuilderOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
    const DataTypes &buildTypes, HashTableVariants *hashTables,
    int32_t partitionIndex, bool joinSpillEnabled, JoinSubPartitionConfig joinSubPartCfg,
    std::vector<int32_t> buildHashCols, JoinSpillState *joinSpillState, bool passiveReclaimEnabled,
    bool forceBuildGroupSpillAfterNoMoreInput, bool prebuilt, std::string broadcastHashTableId,
    HashBuilderOperatorFactory *ownerFactory, BroadcastParallelBuildPolicy broadcastParallelBuildPolicy,
    bool dynamicFilterPushdownEnabled)
    : Operator(pool),
      buildTypes(buildTypes),
      partitionIndex(partitionIndex),
      hashTablesVariants(hashTables),
      joinSpillEnabled_(joinSpillEnabled),
      passiveReclaimEnabled_(passiveReclaimEnabled),
      forceBuildGroupSpillAfterNoMoreInput_(forceBuildGroupSpillAfterNoMoreInput),
      joinSubPartCfg_(joinSubPartCfg),
      broadcastParallelBuildPolicy_(broadcastParallelBuildPolicy),
      buildHashCols_(std::move(buildHashCols)),
      joinSpillState_(joinSpillState),
      prebuilt_(prebuilt),
      broadcastHashTableId_(std::move(broadcastHashTableId)),
      ownerFactory_(ownerFactory),
      dynamicFilterPushdownEnabled_(dynamicFilterPushdownEnabled)
{
    SetOperatorName(opNameForHashBuilder);
    // Cached BHJ build state uses the cache entry's Spark-accounted pool;
    // task-local joins keep the task pool. A cached probe must not rebind it.
    if (!prebuilt_) {
        auto *tablePool = pool;
        if (!broadcastHashTableId_.empty()) {
            if (ownerFactory_ == nullptr) {
                throw std::logic_error("Broadcast cache build has no owning factory");
            }
            cachePool_ = ownerFactory_->GetCachePool();
            tablePool = cachePool_;
        }
        std::visit([tablePool](auto &&v) { v.SetPool(tablePool); }, *hashTablesVariants);
    }
    if (pool != nullptr && joinSpillEnabled_ && passiveReclaimEnabled_ &&
        joinSpillState_ != nullptr && joinSpillState_->IsHandoffSessionEnabled() && !prebuilt_) {
        pool->registerSpillable(this);
        spillableRegistered_ = true;
    }
}

uint32_t HashBuilderOperator::ComputeParallelBuildThreads(uint32_t rowCount, uint64_t estimatedBuildBytes) const
{
    if (!broadcastParallelBuildPolicy_.enabled || rowCount == 0) {
        return 1;
    }
    const uint32_t maxThreads = std::max(1u, static_cast<uint32_t>(std::thread::hardware_concurrency()));
    uint32_t numThreads = maxThreads;
    if (broadcastParallelBuildPolicy_.targetBytesPerThread > 0) {
        numThreads = static_cast<uint32_t>(
            std::max<uint64_t>(1, estimatedBuildBytes / broadcastParallelBuildPolicy_.targetBytesPerThread));
        numThreads = std::min(numThreads, maxThreads);
    }
    return std::max(1u, numThreads);
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
        joinSpillState_->SpillBuildSubPartition(runId, subPartition, subBatch.get(), pool());
    }
    ResetInputVecBatch();
}

/// A build remains reclaimable while it still owns the pre-handoff slot-0 batches.
uint64_t HashBuilderOperator::reclaimableBytes() const
{
    if (!spillableRegistered_ || joinSpillState_ == nullptr || buildSpillTriggered_ ||
        restoreDriving_ || isInNonReclaimableSection()) {
        return 0;
    }
    return std::visit([&](auto &&arg) -> uint64_t {
        uint64_t bytes = 0;
        for (auto *batch : arg.GetResidentInputVecBatches(0)) {
            if (batch != nullptr) bytes += batch->EstimateFlatSize();
        }
        return bytes;
    }, *hashTablesVariants);
}

int64_t HashBuilderOperator::reclaim(int64_t targetBytes)
{
    if (targetBytes <= 0 || pool() == nullptr || reclaimableBytes() == 0) return 0;
    NonReclaimableSection guard(this);
    const int64_t before = pool()->currentBytes();
    SpillAndClearResidentBuild();
    return std::max<int64_t>(0, before - pool()->currentBytes());
}

/// Spill the in-memory build (partition on first spill) and clear resident data.
void HashBuilderOperator::SpillAndClearResidentBuild()
{
    OwnedVecBatchList resident(std::visit([&](auto &&arg) {
        return arg.TakeOwnershipOfResidentBatches(0);
    }, *hashTablesVariants));
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
            joinSpillState_->SpillBuildSubPartition(runId, subPartition, subBatch.get(), pool());
        }
        resident.Free(source);
    }
    ResetSlot0();
    joinSpillState_->MarkRuntimePartitioned();
    buildSpillTriggered_ = true;
}

void HashBuilderOperator::ResetSlot0()
{
    std::visit([&](auto &&arg) { arg.ReclaimBuildPartition(0); }, *hashTablesVariants);
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

bool HashBuilderOperator::TryReserveSlot0Table(const char *site)
{
    if (pool() == nullptr) return true;
    const uint64_t estimated = std::visit([](auto &&arg) {
        return arg.EstimateBuildReservation(0);
    }, *hashTablesVariants);
    const uint64_t padded = estimated + estimated / 10;
    const int64_t request = static_cast<int64_t>(std::min<uint64_t>(
        padded, static_cast<uint64_t>(std::numeric_limits<int64_t>::max())));
    const int64_t quantized = omniruntime::mem::OmniTaskMemoryPool::quantizedReservationSize(request);
    if (quantized < 0) return false;
    if (pool()->availableReservation() >= quantized) return true;
    ReservationRequestSection requestContext(this, reclaimableName(), site, request);
    NonReclaimableSection guard(this);
    return pool()->maybeReserve(request);
}

/// Restore still builds target's single slot; pool admission selects sized or adaptive open.
void HashBuilderOperator::PrepareAndBuildSlot0ForRestore()
{
    lastRestoreUsedAdaptiveOpen_ = ResidentRowCountSlot0() > 0 &&
        !TryReserveSlot0Table("HashBuilderOperator::PrepareAndBuildSlot0ForRestore");
    if (lastRestoreUsedAdaptiveOpen_) PrepareAndBuildSlot0Adaptive();
    else PrepareAndBuildSlot0();
    if (pool() != nullptr) pool()->releaseUnusedReservation();
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
    joinSpillState_->FlushHeldBuildTable(hashTablesVariants, joinSubPartCfg_, pool());
}

bool HashBuilderOperator::TryProduceNextRestoreRound()
{
    const auto nextId = joinSpillState_->NextRestorePartitionId();
    if (!nextId.has_value()) {
        return false;
    }
    ResetSlot0();
    joinSpillState_->RestoreBuildSubPartition(hashTablesVariants, *nextId, pool());
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
        if (forceBuildGroupSpillAfterNoMoreInput_ && ResidentRowCountSlot0() > 0) {
            SpillAndClearResidentBuild();
        } else if (ResidentRowCountSlot0() > 0 &&
                   !TryReserveSlot0Table("HashBuilderOperator::DriveHandoffRestore")) {
            SpillAndClearResidentBuild();
        }
        TryPublishEmptyTableHandoff();
        if (joinSpillState_->GetHandoffPhase() == JoinHandoffPhase::Idle) {
            PrepareAndBuildSlot0();
            if (pool() != nullptr) pool()->releaseUnusedReservation();
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
    // When using a pre-built table from cache, we must still consume (and discard)
    // the broadcast input to keep the pipeline in a valid state.
    if (prebuilt_) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    UpdateAddInputInfo(rowCount);
    if (!broadcastHashTableId_.empty()) {
        // The cached table can outlive this task and its allocation listener.
        auto cacheOwnedBatch = VectorHelper::CopyVectorBatchToPool(vecBatch, cachePool_);
        VectorHelper::FreeVecBatch(vecBatch);
        vecBatch = cacheOwnedBatch.release();
    }
    if (joinSpillEnabled_ && joinSpillState_ != nullptr) {
        // The target's single slot remains resident until pool reclaim or EOB admission.
        if (buildSpillTriggered_) {
            AddLatchedInput(vecBatch);
            return 0;
        }
        std::visit([&](auto &&arg) { arg.AddVecBatch(0, vecBatch); }, *hashTablesVariants);
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
    // Pre-built path: hash table already ready from executor-level cache; just mark done.
    if (prebuilt_) {
        SetStatus(OMNI_STATUS_FINISHED);
        // The builder set the shared table's status before cache publication.
        // Readers only finish their task-local operator; even writing the same
        // value into the shared non-atomic status would race with other readers.
        refreshSpillStats();
        return 0;
    }

    if (joinSpillEnabled_ && joinSpillState_ != nullptr) {
        const int32_t rc = DriveHandoffRestore();
        refreshSpillStats();
        return rc;
    }

    const uint32_t rowCount = std::visit(
        [&](auto &&arg) { return arg.GetPartitionRowCount(partitionIndex); }, *hashTablesVariants);
    const uint64_t estimatedBuildBytes = static_cast<uint64_t>(rowCount) * 32;
    const uint32_t numThreads = ComputeParallelBuildThreads(rowCount, estimatedBuildBytes);
    bool builtParallel = false;
    if (broadcastParallelBuildPolicy_.enabled && numThreads > 1) {
        builtParallel = std::visit(
            [&](auto &&arg) {
                return arg.TryBuildHashTableParallel(partitionIndex, numThreads,
                    broadcastParallelBuildPolicy_.minTableRowsForParallelJoinBuild);
            },
            *hashTablesVariants);
    }
    if (!builtParallel) {
        std::visit(
            [&](auto &&arg) {
                arg.Prepare(partitionIndex);
                arg.BuildHashTable(partitionIndex);
            },
            *hashTablesVariants);
    }

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
    if (dynamicFilterPushdownEnabled_) {
        std::visit([&](auto &&arg) { arg.BuildDynamicFilters(); }, *hashTablesVariants);
    }
    SetStatus(OMNI_STATUS_FINISHED);
    std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);

    // BHJ cache: publish the built hash table to the executor-level cache so that
    // subsequent tasks on this executor can reuse it without rebuilding.
    if (!broadcastHashTableId_.empty()) {
        if (ownerFactory_ == nullptr) {
            throw std::logic_error("Broadcast cache builder has no owning factory");
        }
        ownerFactory_->PublishCachedVariants();
    }
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
