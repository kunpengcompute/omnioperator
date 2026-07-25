/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * @Description: lookup outer join implementations
 */
#include <vector>
#include <memory>
#include "hash_builder.h"
#include "lookup_outer_join.h"

using namespace omniruntime::vec;
namespace omniruntime {
namespace op {
LookupOuterJoinOperatorFactory::LookupOuterJoinOperatorFactory(const type::DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *buildOutputCols,
    const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables)
    : buildOutputTypes(buildOutputTypes), probeTypes(probeTypes), hashTables(hashTables)
{
    this->probeOutputCols.insert(this->probeOutputCols.end(), probeOutputCols, probeOutputCols + probeOutputColsCount);
    this->buildOutputCols.insert(this->buildOutputCols.end(), buildOutputCols,
        buildOutputCols + buildOutputTypes.GetSize());
}

LookupOuterJoinOperatorFactory::LookupOuterJoinOperatorFactory(const type::DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *buildOutputCols,
    const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables, BuildSide buildSide)
    : buildOutputTypes(buildOutputTypes), probeTypes(probeTypes), hashTables(hashTables), buildSide(buildSide)
{
    this->probeOutputCols.insert(this->probeOutputCols.end(), probeOutputCols, probeOutputCols + probeOutputColsCount);
    this->buildOutputCols.insert(this->buildOutputCols.end(), buildOutputCols,
                                 buildOutputCols + buildOutputTypes.GetSize());
}

LookupOuterJoinOperatorFactory::~LookupOuterJoinOperatorFactory() = default;

LookupOuterJoinOperatorFactory *LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
    const type::DataTypes &probeTypes, int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *buildOutputCols,
    const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr)
{
    auto hashBuilderFactory = reinterpret_cast<HashBuilderOperatorFactory *>(hashBuilderFactoryAddr);
    auto pOperatorFactory = new LookupOuterJoinOperatorFactory(probeTypes, probeOutputCols, probeOutputColsCount,
        buildOutputCols, buildOutputTypes, hashBuilderFactory->GetHashTablesVariants());
    return pOperatorFactory;
}

LookupOuterJoinOperatorFactory *LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
    const type::DataTypes &probeTypes, int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *buildOutputCols,
    const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr, BuildSide buildSide)
{
    auto hashBuilderFactory = reinterpret_cast<HashBuilderOperatorFactory *>(hashBuilderFactoryAddr);
    auto pOperatorFactory = new LookupOuterJoinOperatorFactory(probeTypes, probeOutputCols, probeOutputColsCount,
        buildOutputCols, buildOutputTypes, hashBuilderFactory->GetHashTablesVariants(), buildSide);
    return pOperatorFactory;
}

LookupOuterJoinOperatorFactory *LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, HashBuilderOperatorFactory* hashBuilderOperatorFactory,
    const config::QueryConfig& queryConfig)
{
    auto buildOutputTypes = planNode->RightOutputType();
    auto buildOutputColsCount = buildOutputTypes->GetSize();
    std::vector<int32_t> buildOutputCols;
    for (int32_t index = 0; index < buildOutputColsCount; index++) {
        buildOutputCols.emplace_back(index);
    }

    auto probeOutputTypes = planNode->LeftOutputType();
    auto probeOutputColsCount = probeOutputTypes->GetSize();
    std::vector<int32_t> probeOutputCols;
    for (int32_t index = 0; index < probeOutputColsCount; index++) {
        probeOutputCols.emplace_back(index);
    }

    return new LookupOuterJoinOperatorFactory(*probeOutputTypes, probeOutputCols.data(), probeOutputColsCount,
        buildOutputCols.data(), *buildOutputTypes, hashBuilderOperatorFactory->GetHashTablesVariants());
}

Operator *LookupOuterJoinOperatorFactory::CreateOperator()
{
    auto probeOutputType = std::vector<type::DataTypePtr>();
    for (const auto& col : probeOutputCols) {
        probeOutputType.push_back(probeTypes.Get()[col]);
    }
    auto probeOutputTypes = DataTypes(probeOutputType);
    auto lookupOuterJoinOperator =
        new LookupOuterJoinOperator(probeOutputTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashTables, buildSide);
    return lookupOuterJoinOperator;
}

void LookupOuterJoinOperator::PrepareTotalVisitedCounts()
{
    std::visit(
        [&](auto &&arg) {
#ifdef OMNI_USE_TAPER_JOIN
            auto* rc = arg.GetTaperRowContainer(0);
            if (rc) {
                arg.AddTotalVisitedCounts(static_cast<uint32_t>(rc->NumRows()));
                return;
            }
#else
            size_t partitionIndex = 0;
            while (partitionIndex < arg.GetHashTableSize()) {
                if (arg.GetHashTableTypes(partitionIndex) == HashTableImplementationType::ARRAY_HASH_TABLE) {
                    auto &hashTable = arg.GetArrayTable(partitionIndex);
                    hashTable->ForEachValue(
                        [&](const auto &value, const auto &index) { arg.AddTotalVisitedCounts(value->GetRowCount()); });
                } else {
                    auto &hashTable = arg.GetHashTable(partitionIndex);
                    hashTable->hashmap.ForEachValue(
                        [&](const auto &value, const auto &index) { arg.AddTotalVisitedCounts(value->GetRowCount()); });
                }
                partitionIndex++;
            }
#endif
        },
        *hashTables);
}

LookupOuterJoinOperator::LookupOuterJoinOperator(DataTypes &probeOutputTypes, std::vector<int32_t> &probeOutputCols,
    std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables)
    : probeOutputTypes(probeOutputTypes),
      probeOutputCols(probeOutputCols),
      buildOutputCols(buildOutputCols),
      buildOutputTypes(buildOutputTypes),
      hashTables(hashTables),
      iterator(new LookupOuterPositionIterator(hashTables)),
      outputColsCount(static_cast<int32_t>(probeOutputCols.size() + buildOutputCols.size()))
{
    int32_t outputRowSize =
        OperatorUtil::GetRowSize(this->buildOutputTypes.Get()) + OperatorUtil::GetRowSize(this->probeOutputTypes.Get());
    maxRowCount = OperatorUtil::GetMaxRowCount((outputColsCount == 0) ? DEFAULT_ROW_SIZE : outputRowSize);
    SetOperatorName(opNameForLookUpJoin);
}

LookupOuterJoinOperator::LookupOuterJoinOperator(DataTypes &probeOutputTypes, std::vector<int32_t> &probeOutputCols,
    std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables, BuildSide buildSide)
    : probeOutputTypes(probeOutputTypes),
      probeOutputCols(probeOutputCols),
      buildOutputCols(buildOutputCols),
      buildOutputTypes(buildOutputTypes),
      hashTables(hashTables),
      iterator(new LookupOuterPositionIterator(hashTables)),
      outputColsCount(static_cast<int32_t>(probeOutputCols.size() + buildOutputCols.size())),
      buildSide(buildSide)
{
    int32_t outputRowSize =
            OperatorUtil::GetRowSize(this->buildOutputTypes.Get()) + OperatorUtil::GetRowSize(this->probeOutputTypes.Get());
    maxRowCount = OperatorUtil::GetMaxRowCount((outputColsCount == 0) ? DEFAULT_ROW_SIZE : outputRowSize);
    SetOperatorName(opNameForLookUpJoin);
}

LookupOuterJoinOperator::~LookupOuterJoinOperator()
{
    delete iterator;
    iterator = nullptr;
}

int32_t LookupOuterJoinOperator::AddInput(VectorBatch *vecBatch)
{
    // do nothing, lookup outer join just process matched rows in GetOutput
    return 0;
}

int32_t LookupOuterJoinOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!isPrepareTotalVisitedCounts) {
        PrepareTotalVisitedCounts();
        isPrepareTotalVisitedCounts = true;
    }

    totalRowCount =
        std::visit([&](auto &&arg) { return arg.GetTotalVisitedCounts() - arg.GetVisitedCounts(); }, *hashTables);
    if (totalRowCount <= 0) {
        SetStatus(OMNI_STATUS_FINISHED);
        iterator->Reset();
        return 0;
    }
    int32_t rowCount = std::min(maxRowCount, static_cast<int32_t>(totalRowCount) - outputtedRowCount);
    auto result = std::make_unique<VectorBatch>(rowCount);
    auto resultPtr = result.get();
    BuildVecBatch(resultPtr);
    if (buildSide == OMNI_BUILD_LEFT) {
        BaseVector **pVector = resultPtr->GetVectors();
        std::rotate(pVector, pVector + probeOutputCols.size(), pVector + resultPtr->GetVectorCount());
    }
    *outputVecBatch = result.release();
    outputtedRowCount += rowCount;
    if (!HasNext()) {
        SetStatus(OMNI_STATUS_FINISHED);
        iterator->Reset();
        totalRowCount = 0;
        outputtedRowCount = 0;
    }
    return 0;
}

void LookupOuterJoinOperator::BuildVecBatch(VectorBatch *vectorBatch)
{
    auto rowCount = vectorBatch->GetRowCount();
    for (int32_t col = 0; col < probeOutputTypes.GetSize(); col++) {
        auto typeId = probeOutputTypes.GetType(col)->GetId();
        auto vector = VectorHelper::CreateVector(OMNI_FLAT, typeId, rowCount);
        for (int32_t row = 0; row < rowCount; row++) {
            if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetNull(row);
            } else {
                vector->SetNull(row);
            }
        }
        vectorBatch->Append(vector);
    }
    for (int32_t buildCol = 0; buildCol < buildOutputTypes.GetSize(); buildCol++) {
        auto vector = VectorHelper::CreateVector(OMNI_FLAT, buildOutputTypes.GetType(buildCol)->GetId(), rowCount);
        vectorBatch->Append(vector);
    }

    if (!visited) {
        PrepareAllUnvisitedRows();
        visited = true;
    }

    AppendAllUnvisitedRows(vectorBatch, buildOutputTypes.GetIds(), buildOutputTypes.GetSize(),
        probeOutputTypes.GetSize(), rowCount);
}

template <DataTypeId typeId>
void AppendTo(VectorBatch *vectorBatch, int32_t destCol, int32_t destRowIndex, uint32_t srcRowIndex, BaseVector *src)
{
    using Type = typename omniruntime::op::NativeAndVectorType<typeId>::type;
    using Vector = typename omniruntime::op::NativeAndVectorType<typeId>::vector;
    using DictVector = typename omniruntime::op::NativeAndVectorType<typeId>::dictVector;

    auto destVector = static_cast<Vector *>(vectorBatch->Get(destCol));
    if (src->IsNull(srcRowIndex)) {
        destVector->SetNull(destRowIndex);
        return;
    }

    Type value;
    if (src->GetEncoding() == OMNI_ENCODING_CONST) {
        value = static_cast<vec::ConstVector<Type> *>(src)->GetConstValue();
    } else if (src->GetEncoding() == OMNI_DICTIONARY) {
        value = static_cast<DictVector *>(src)->GetValue(srcRowIndex);
    } else {
        value = static_cast<Vector *>(src)->GetValue(srcRowIndex);
    }
    destVector->SetValue(destRowIndex, value);
}

void LookupOuterJoinOperator::AppendAllUnvisitedRows(VectorBatch *vectorBatch, const int32_t *buildOutputIds,
    int32_t buildOutputColsCount, int32_t probeOutputColsCount, int rowCount)
{
#ifdef OMNI_USE_TAPER_JOIN
    if (!taperUnvisitedRowPtrs_.empty()) {
        auto* rc = std::visit([&](auto&& arg) { return arg.GetTaperRowContainer(0); }, *hashTables);
        const auto& storedCols = std::visit([&](auto&& arg) { return arg.GetTaperStoredColIndices(); }, *hashTables);
        auto* rcc = const_cast<RowContainer*>(rc);
        for (int32_t col = 0; col < buildOutputColsCount; col++) {
            uint32_t outputCol = buildOutputCols[col];
            int32_t rcColIdx = 0;
            for (size_t k = 0; k < storedCols.size(); ++k) {
                if (storedCols[k] == static_cast<int32_t>(outputCol))
                    { rcColIdx = static_cast<int32_t>(k); break; }
            }
            auto* outVec = vectorBatch->GetVectors()[col + probeOutputColsCount];
            rcc->ExtractColumn(taperUnvisitedRowPtrs_.data() + outputtedRowCount,
                               rowCount, rcColIdx, outVec);
        }
        return;
    }
#else
    std::visit(
        [&](auto &&arg) {
            if (arg.GetHashTableSize() == 1) {
                for (auto i = 0, addrIdx = outputtedRowCount; i < rowCount; i++, addrIdx++) {
                    auto vecBatchIndex = addresses[addrIdx].first;
                    auto srcRowIndex = addresses[addrIdx].second;

                    for (int32_t col = 0; col < buildOutputColsCount; col++) {
                        auto buildOutputCol = buildOutputCols[col];
                        auto destCol = col + probeOutputColsCount;
                        auto src = arg.GetColumns(0)[buildOutputCol][vecBatchIndex];
                        DYNAMIC_TYPE_DISPATCH(AppendTo, buildOutputIds[col], vectorBatch, destCol, i, srcRowIndex, src);
                    }
                }
            } else {
                for (auto i = 0, addrIdx = outputtedRowCount; i < rowCount; i++, addrIdx++) {
                    auto vecBatchIndex = addresses[addrIdx].first;
                    auto srcRowIndex = addresses[addrIdx].second;

                    for (int32_t col = 0; col < buildOutputColsCount; col++) {
                        auto buildOutputCol = buildOutputCols[col];
                        auto destCol = col + probeOutputColsCount;
                        auto src = arg.GetColumns(hashTableIndexes[addrIdx])[buildOutputCol][vecBatchIndex];
                        DYNAMIC_TYPE_DISPATCH(AppendTo, buildOutputIds[col], vectorBatch, destCol, i, srcRowIndex, src);
                    }
                }
            }
        },
        *hashTables);
#endif
}

void LookupOuterJoinOperator::PrepareAllUnvisitedRows()
{
#ifdef OMNI_USE_TAPER_JOIN
    auto* rc = std::visit([&](auto&& arg) { return arg.GetTaperRowContainer(0); }, *hashTables);
    if (rc) {
        auto payloadOff = rc->PayloadOffset();
        taperUnvisitedRowPtrs_.clear();
        RowContainerIterator rit;
        char* rows[1024];
        int32_t n;
        auto* rcc = const_cast<RowContainer*>(rc);
        while ((n = rcc->ListRows(&rit, 1024, rows)) > 0) {
            for (int32_t i = 0; i < n; ++i) {
                if (*RowContainer::VisitedPtr(rows[i], payloadOff) == 0) {
                    taperUnvisitedRowPtrs_.push_back(rows[i]);
                }
            }
        }
        return;
    }
#else
    std::visit(
        [&](auto &&arg) {
            if (arg.GetHashTableSize() == 1) {
                iterator->GetAllUnVisitedAddressFromSingleTable(hashTableIndexes, addresses);
            } else {
                iterator->GetAllUnVisitedAddressFromMultipleTables(hashTableIndexes, addresses);
            }
        },
        *hashTables);
#endif
}

LookupOuterPositionIterator::LookupOuterPositionIterator(HashTableVariants *hashTables)
    : currentHashTable(0), currentPosition(0), hashTables(hashTables)
{}

void LookupOuterPositionIterator::GetAllUnVisitedAddressFromSingleTable(std::vector<uint32_t> &hashTableIndexes,
    std::vector<std::pair<uint32_t, uint32_t>> &addresses)
{
    std::visit(
        [&](auto &&arg) {
            using VariantType = std::decay_t<decltype(arg)>;
            using Mapped = typename VariantType::Mapped;
            if constexpr (std::is_same_v<Mapped, RowRefListWithFlags>) {
#ifndef OMNI_USE_TAPER_JOIN
                if (arg.GetHashTableTypes(currentHashTable) == HashTableImplementationType::ARRAY_HASH_TABLE) {
                    auto &hashTable = arg.GetArrayTable(currentHashTable);
                    hashTable->ForEachValue([&](const auto &value, const auto &index) {
                        auto it = value->Begin();
                        while (it.IsOk()) {
                            if (!it->visited) {
                                addresses.emplace_back(std::make_pair(it->vecBatchIdx, it->rowIdx));
                            }
                            ++it;
                        }
                    });
                } else {
                    auto &hashTable = arg.GetHashTable(currentHashTable);
                    hashTable->hashmap.ForEachValue([&](const auto &value, const auto &index) {
                        auto it = value->Begin();
                        while (it.IsOk()) {
                            if (!it->visited) {
                                addresses.emplace_back(std::make_pair(it->vecBatchIdx, it->rowIdx));
                            }
                            ++it;
                        }
                    });
                }
#endif
                currentHashTable++;
            }
        },
        *hashTables);
}

void LookupOuterPositionIterator::GetAllUnVisitedAddressFromMultipleTables(std::vector<uint32_t> &hashTableIndexes,
    std::vector<std::pair<uint32_t, uint32_t>> &addresses)
{
    std::visit(
        [&](auto &&arg) {
            using VariantType = std::decay_t<decltype(arg)>;
            using Mapped = typename VariantType::Mapped;
            if constexpr (std::is_same_v<Mapped, RowRefListWithFlags>) {
                while (currentHashTable < arg.GetHashTableSize()) {
#ifndef OMNI_USE_TAPER_JOIN
                    if (arg.GetHashTableTypes(currentHashTable) == HashTableImplementationType::ARRAY_HASH_TABLE) {
                        auto &hashTable = arg.GetArrayTable(currentHashTable);
                        hashTable->ForEachValue([&](const auto &value, const auto &index) {
                            auto it = value->Begin();
                            while (it.IsOk()) {
                                if (!it->visited) {
                                    addresses.emplace_back(std::make_pair(it->vecBatchIdx, it->rowIdx));
                                    hashTableIndexes.emplace_back(currentHashTable);
                                }
                                ++it;
                            }
                        });
                    } else {
                        auto &hashTable = arg.GetHashTable(currentHashTable);
                        hashTable->hashmap.ForEachValue([&](const auto &value, const auto &index) {
                            auto it = value->Begin();
                            while (it.IsOk()) {
                                if (!it->visited) {
                                    addresses.emplace_back(std::make_pair(it->vecBatchIdx, it->rowIdx));
                                    hashTableIndexes.emplace_back(currentHashTable);
                                }
                                ++it;
                            }
                        });
                    }
#endif
                    currentHashTable++;
                }
            }
        },
        *hashTables);
}

void LookupOuterPositionIterator::Reset()
{
    currentPosition = 0;
    currentHashTable = 0;
}
} // end of op
} // end of omniruntime
