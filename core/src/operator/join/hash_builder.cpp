/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#include "hash_builder.h"
#include <iostream>
#include <vector>
#include <memory>
#include <utility>
#include "broadcast_hash_table_cache.h"
#include "join_spill_state.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
void HashBuilderOperatorFactory::SetJoinSpillSubPartitionPolicy(
    bool joinSpillEnabled, uint64_t maxSpillRunRows, JoinSubPartitionConfig joinSubPartCfg)
{
    joinSpillEnabled_ = joinSpillEnabled;
    joinMaxSpillRunRows_ = maxSpillRunRows;
    joinSubPartCfg_ = joinSubPartCfg;
}

void HashBuilderOperatorFactory::SetJoinSpillState(std::shared_ptr<JoinSpillState> joinSpillState)
{
    joinSpillState_ = std::move(joinSpillState);
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
      ownsVariants_(false),
      prebuilt_(true)
{
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateFromCachedVariants(
    std::shared_ptr<const HashJoinNode> planNode, HashTableVariants* cachedVariants)
{
    auto* factory = new HashBuilderOperatorFactory(*planNode->RightOutputType(), cachedVariants);
    return factory;
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

Operator *HashBuilderOperatorFactory::CreateOperator()
{
    /// operatorIndex is start by 0, so partitionIdex is start by 0
    int32_t partitionIndex =
        operatorIndex++ % std::visit([&](auto &&arg) { return arg.GetHashTableCount(); }, *hashTablesVariants);
    return new HashBuilderOperator(this->buildTypes, hashTablesVariants, partitionIndex, joinSpillEnabled_,
        joinMaxSpillRunRows_, joinSubPartCfg_, buildHashCols, joinSpillState_.get(),
        prebuilt_, broadcastHashTableId_, this);
}

HashBuilderOperator::HashBuilderOperator(const DataTypes &buildTypes, HashTableVariants *hashTables,
    int32_t partitionIndex, bool joinSpillEnabled, uint64_t joinMaxSpillRunRows,
    JoinSubPartitionConfig joinSubPartCfg, std::vector<int32_t> buildHashCols, JoinSpillState *joinSpillState,
    bool prebuilt, std::string broadcastHashTableId, HashBuilderOperatorFactory* ownerFactory)
    : buildTypes(buildTypes),
      partitionIndex(partitionIndex),
      hashTablesVariants(hashTables),
      joinSpillEnabled_(joinSpillEnabled),
      joinMaxSpillRunRows_(joinMaxSpillRunRows),
      joinSubPartCfg_(joinSubPartCfg),
      useJoinSubPartitioning_(joinSpillEnabled && joinMaxSpillRunRows > 0 && joinSubPartCfg.IsEnabled() &&
          std::visit([&](auto &&arg) { return arg.GetHashTableCount(); }, *hashTables) ==
              joinSubPartCfg.numSubPartitions),
      buildHashCols_(std::move(buildHashCols)),
      joinSpillState_(joinSpillState),
      prebuilt_(prebuilt),
      broadcastHashTableId_(std::move(broadcastHashTableId)),
      ownerFactory_(ownerFactory)
{
    SetOperatorName(opNameForHashBuilder);
}

bool HashBuilderOperator::UseJoinSubPartitioning() const
{
    return useJoinSubPartitioning_;
}

/// add each subpartition data in buildSize to the each HashTable.
bool HashBuilderOperator::AddSubPartitionedInput(omniruntime::vec::VectorBatch *vecBatch)
{
    if (!UseJoinSubPartitioning()) {
        return false;
    }
    const bool spillCurrentBatch = joinSpillState_ != nullptr &&
        static_cast<uint64_t>(vecBatch->GetRowCount()) >= joinMaxSpillRunRows_;
    const uint64_t runId = spillCurrentBatch ? joinSpillState_->NextRunId() : 0;

    // here we will do the sub partition spilt for source vectorbatch
    JoinSubPartitioner partitioner(joinSubPartCfg_);
    const auto buckets = partitioner.PartitionRowsByKeyColumns(vecBatch, buildTypes, buildHashCols_);
    std::vector<std::vector<int32_t>> positions(joinSubPartCfg_.numSubPartitions);
    for (size_t row = 0; row < buckets.size(); ++row) {
        positions[buckets[row]].push_back(static_cast<int32_t>(row));
    }

    const int32_t vectorCount = vecBatch->GetVectorCount();
    for (uint32_t subPartition = 0; subPartition < positions.size(); ++subPartition) {
        auto &bucketPositions = positions[subPartition];
        if (bucketPositions.empty()) {
            continue;
        }
        auto *subBatch = new omniruntime::vec::VectorBatch(bucketPositions.size());
        subBatch->ResizeVectorCount(vectorCount);
        for (int32_t col = 0; col < vectorCount; ++col) {
            subBatch->SetVector(col, omniruntime::vec::VectorHelper::CopyPositionsVector(
                vecBatch->Get(col), bucketPositions.data(), 0, static_cast<int32_t>(bucketPositions.size())));
        }
        // keep subpartiton 0 in memory for compute, other subpartition should be spilled to disk
        if (spillCurrentBatch && subPartition != 0) {
            joinSpillState_->SpillBuildSubPartition(runId, subPartition, subBatch);
            // free the batch in memory
            omniruntime::vec::VectorHelper::FreeVecBatch(subBatch);
        } else {
            std::visit([&](auto &&arg) { arg.AddVecBatch(static_cast<int32_t>(subPartition), subBatch); },
                *hashTablesVariants);
        }
    }
    if (UNLIKELY(IsDebugEnable())) {
        std::cout << "[OmniRuntime][JoinSubPartitioner][build] partitioned_input_rows=" << vecBatch->GetRowCount()
                  << " outputSubPartitions=" << positions.size();
        if (spillCurrentBatch) {
            std::cout << " runId=" << runId << " activeSubPartition=0 spilledSubPartitions="
                      << (positions.size() > 0 ? positions.size() - 1 : 0);
        }
        std::cout << "\n";
        std::cout.flush();
    }
    omniruntime::vec::VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    return true;
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
    if (AddSubPartitionedInput(vecBatch)) {
        return 0;
    }
    std::visit([&](auto &&arg) { arg.AddVecBatch(partitionIndex, vecBatch); }, *hashTablesVariants);
    return 0;
}

int32_t HashBuilderOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
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
        std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);
        return 0;
    }

    std::visit(
        [&](auto &&arg) {
            if (UseJoinSubPartitioning()) {
                if (joinSpillState_ != nullptr) {
                    joinSpillState_->ReplayBuildInput(hashTablesVariants);
                }
                for (uint32_t partition = 0; partition < arg.GetHashTableCount(); ++partition) {
                    arg.Prepare(static_cast<int32_t>(partition));
                    arg.BuildHashTable(static_cast<int32_t>(partition));
                }
                if (UNLIKELY(IsDebugEnable())) {
                    std::cout << "[OmniRuntime][JoinSubPartitioner][build] built_subpartition_hash_tables="
                              << arg.GetHashTableCount() << "\n";
                    std::cout.flush();
                }
            } else {
                arg.Prepare(partitionIndex);
                arg.BuildHashTable(partitionIndex);
            }
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

    // BHJ cache: publish the built hash table to the executor-level cache so that
    // subsequent tasks on this executor can reuse it without rebuilding.
    if (!broadcastHashTableId_.empty()) {
        // Release ownership in the factory so its destructor won't double-free.
        if (ownerFactory_ != nullptr) {
            ownerFactory_->ReleaseVariants();
            ownerFactory_->MarkCachePinned();
            BroadcastHashTableCache::getInstance().publish(
                broadcastHashTableId_, hashTablesVariants, ownerFactory_);
        } else {
            BroadcastHashTableCache::getInstance().publish(broadcastHashTableId_, hashTablesVariants, nullptr);
        }
    }
    return 0;
}

OmniStatus HashBuilderOperator::Close()
{
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}
} // end of op
} // end of omniruntime