/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * @Description: join hash table
 */
#include "join_hash_table_variants.h"
#include "operator/util/operator_util.h"

#include <algorithm>
#include <cmath>

namespace omniruntime {
namespace op {
static constexpr uint32_t ARRAY_THRESHOLD = 8;
static constexpr double LOAD_FACTOR = 0.75;
static constexpr uint32_t BLOCK_SIZE = 1024;
static constexpr uint8_t MIN_DEGREE = 5;

template <typename KeyType, typename RowRefListType>
JoinHashTableVariants<KeyType, RowRefListType>::JoinHashTableVariants(uint32_t hashTableCount,
    DataTypes *buildDataTypes, std::vector<int32_t> &buildHashCols,
    JoinType joinType, BuildSide buildSide, bool isMultiCols)
    : hashTableCount(hashTableCount),
      hashTableSize(0),
      probeTypes(nullptr),
      buildTypes(buildDataTypes),
      buildHashCols(buildHashCols),
      totalRowCount(std::vector<uint32_t>(hashTableCount, 0)),
      inputVecBatches(std::vector<std::vector<omniruntime::vec::VectorBatch *>>(hashTableCount)),
      columns(std::vector<omniruntime::vec::BaseVector ***>(hashTableCount)),
      joinType(joinType),
      buildSide(buildSide),
      isMultiCols(isMultiCols)
{
    hashTables = std::vector<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>>(hashTableCount);
    arrayTables = std::vector<std::unique_ptr<DefaultArrayMap<RowRefListType>>>(hashTableCount);
    hashTableTypes = std::vector<HashTableImplementationType>(hashTableCount);
    maxMins = std::vector<std::pair<int64_t, int64_t>>(hashTableCount);
    sizeOfRowRefList = sizeof(RowRefListType);
    for (uint32_t i = 0; i < hashTableCount; ++i) {
        executionContexts.emplace_back(std::make_unique<ExecutionContext>());
    }

    for (const int32_t buildHashCol : buildHashCols) {
        auto type = buildTypes->GetIds()[buildHashCol];
        if (type == OMNI_VARCHAR || type == OMNI_CHAR || type == OMNI_VARBINARY || type == OMNI_ARRAY ||
            type == OMNI_ROW) {
            isFixedKeys = false;
            break;
        }
    }

    if (isFixedKeys) {
        for (const int32_t buildHashCol : buildHashCols) {
            auto type = buildTypes->GetIds()[buildHashCol];
            switch (type) {
                case OMNI_BYTE:
                    fixedKeysSize += OperatorUtil::SIZE_OF_BYTE;
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                    fixedKeysSize += OperatorUtil::SIZE_OF_INT;
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                case OMNI_DOUBLE:
                case OMNI_DATE64:
                    fixedKeysSize += OperatorUtil::SIZE_OF_LONG;
                    break;
                case OMNI_BOOLEAN:
                    fixedKeysSize += OperatorUtil::SIZE_OF_BOOL;
                    break;
                case OMNI_SHORT:
                    fixedKeysSize += OperatorUtil::SIZE_OF_SHORT;
                    break;
                case OMNI_DECIMAL128:
                    fixedKeysSize += OperatorUtil::SIZE_OF_DECIMAL128;
                    break;
                default:
                    break;
            }
        }
    }
    isNeedNullKeyTable = joinType == OMNI_JOIN_TYPE_FULL
            || (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT)
            || (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT);
}

template <typename KeyType, typename RowRefListType>
JoinHashTableVariants<KeyType, RowRefListType>::~JoinHashTableVariants()
{
    ReleaseBuildState();
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::ReleaseBuildState()
{
    if (buildStateReleased_) {
        return;
    }
    for (auto &hashTable : hashTables) {
        hashTable.reset();
    }
    for (auto &arrayTable : arrayTables) {
        arrayTable.reset();
    }
    for (auto &column : columns) {
        if (column != nullptr) {
            for (auto colIdx = 0; colIdx < buildTypes->GetSize(); ++colIdx) {
                delete[] column[colIdx];
            }
            delete[] column;
            column = nullptr;
        }
    }
    for (auto &inputVecBatch : inputVecBatches) {
        VectorHelper::FreeVecBatches(inputVecBatch);
        inputVecBatch.clear();
    }
    for (auto &ctx : executionContexts) {
        ctx.reset();
    }
    for (auto &rows : totalRowCount) {
        rows = 0;
    }
    hashTableSize.store(0);
    buildStateReleased_ = true;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::ResetPartition(int32_t partitionIndex)
{
    if (buildStateReleased_ || partitionIndex < 0 || static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
        return;
    }
    if (columns[partitionIndex] != nullptr) {
        for (auto colIdx = 0; colIdx < buildTypes->GetSize(); ++colIdx) {
            delete[] columns[partitionIndex][colIdx];
        }
        delete[] columns[partitionIndex];
        columns[partitionIndex] = nullptr;
    }
    VectorHelper::FreeVecBatches(inputVecBatches[partitionIndex]);
    inputVecBatches[partitionIndex].clear();
    totalRowCount[partitionIndex] = 0;
    if (hashTables[partitionIndex] != nullptr || arrayTables[partitionIndex] != nullptr) {
        if (hashTableSize > 0) {
            hashTableSize--;
        }
    }
    hashTables[partitionIndex].reset();
    arrayTables[partitionIndex].reset();
    hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
    executionContexts[partitionIndex] = std::make_unique<ExecutionContext>();
    visitedCounts = 0;
}

template <typename T>
static inline T GetVectorValue(BaseVector *vec, int32_t index)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<T> *>(vec)->GetConstValue();
    } else if (vec->GetEncoding() == OMNI_DICTIONARY) {
        return reinterpret_cast<Vector<DictionaryContainer<T>> *>(vec)->GetValue(index);
    } else {
        return reinterpret_cast<Vector<T> *>(vec)->GetValue(index);
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::ComputeMultiColKey(
    BaseVector **hashColumns, int32_t hashColCount, int32_t index, KeyType& key)
{
    if constexpr (std::is_same_v<KeyType, int32_t> ||
                  std::is_same_v<KeyType, int64_t> || std::is_same_v<KeyType, int128_t>) {
        for (int i = 0; i < hashColCount; ++i) {
            switch (hashColumns[i]->GetTypeId()) {
                case OMNI_BOOLEAN:
                case OMNI_BYTE:
                    key = static_cast<KeyType>(key) << BITS_OF_BYTE | static_cast<KeyType>(
                        GetVectorValue<int8_t>(hashColumns[i], index));
                    break;
                case OMNI_SHORT:
                    key = static_cast<KeyType>(key) << BITS_OF_SHORT | static_cast<KeyType>(
                        GetVectorValue<int16_t>(hashColumns[i], index));
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                case OMNI_FLOAT:
                    key = static_cast<KeyType>(key) << BITS_OF_INT | static_cast<KeyType>(
                        GetVectorValue<int32_t>(hashColumns[i], index));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                case OMNI_DOUBLE:
                case OMNI_DATE64:
                    key = static_cast<KeyType>(key) << BITS_OF_LONG | static_cast<KeyType>(
                        GetVectorValue<int64_t>(hashColumns[i], index));
                    break;
                default:
                    std::string omniExceptionInfo =
                        "Error in computing multiCol key, not support type: " +
                        std::to_string(hashColumns[i]->GetTypeId());
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
InsertResult<RowRefListType *> JoinHashTableVariants<KeyType, RowRefListType>::Find(
    std::vector<VectorSerializerIgnoreNull> &probeSerializers, ExecutionContext *probeArena,
    BaseVector **probeHashColumns, int32_t probeHashColCount, int32_t probePosition, uint32_t partition)
{
    KeyType key;
    if (isMultiCols) {
        if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                      std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t> ||
                      std::is_same_v<KeyType, int128_t>) {
            key = 0;
        }
        ComputeMultiColKey(probeHashColumns, probeHashColCount, probePosition, key);
        return hashTables[partition]->FindValueFromHashmap(key);
    }

    if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                  std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t>) {
        if (probeHashColumns[0]->GetEncoding() == OMNI_ENCODING_CONST) {
            key = static_cast<ConstVector<KeyType> *>(probeHashColumns[0])->GetConstValue();
        } else if (probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY) {
            auto curVector = reinterpret_cast<Vector<KeyType> *>(probeHashColumns[0]);
            key = curVector->GetValue(probePosition);
        } else {
            auto curVector = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(probeHashColumns[0]);
            key = curVector->GetValue(probePosition);
        }
        if (hashTableTypes[partition] == HashTableImplementationType::ARRAY_HASH_TABLE) {
            if (key >= maxMins[partition].second && key <= maxMins[partition].first) {
                return arrayTables[partition]->FindValueFromHashmap(
                    static_cast<int64_t>(key - maxMins[partition].second));
            } else {
                return arrayTables[partition]->EmptyResult();
            }
        } else {
            return hashTables[partition]->FindValueFromHashmap(key);
        }
    } else {
        if constexpr (!std::is_same_v<KeyType, type::StringRef>) {
            if (probeHashColumns[0]->GetEncoding() == OMNI_ENCODING_CONST) {
                key = static_cast<ConstVector<KeyType> *>(probeHashColumns[0])->GetConstValue();
            } else if (probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY) {
                auto curVector = reinterpret_cast<Vector<KeyType> *>(probeHashColumns[0]);
                key = curVector->GetValue(probePosition);
            } else {
                auto curVector = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(probeHashColumns[0]);
                key = curVector->GetValue(probePosition);
            }
        } else {
            auto &arenaAllocator = *(probeArena->GetArena());
            for (int32_t colIdx = 0; colIdx < probeHashColCount; colIdx++) {
                auto curVector = probeHashColumns[colIdx];
                auto &curFunc = probeSerializers[colIdx];
                curFunc(curVector, probePosition, arenaAllocator, key);
            }
        }
        return hashTables[partition]->FindValueFromHashmap(key);
    }
}

template<typename KeyType, typename RowRefListType>
KeyType JoinHashTableVariants<KeyType, RowRefListType>::GetKeyValue(BaseVector **probeHashColumns,
    int32_t probePosition)
{
    KeyType key;
    if (probeHashColumns[0]->GetEncoding() == OMNI_ENCODING_CONST) {
        key = static_cast<ConstVector<KeyType> *>(probeHashColumns[0])->GetConstValue();
    } else if (probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY) {
        auto curVector = reinterpret_cast<Vector<KeyType> *>(probeHashColumns[0]);
        key = curVector->GetValue(probePosition);
    } else {
        auto curVector = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(probeHashColumns[0]);
        key = curVector->GetValue(probePosition);
    }
    return key;
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceFixedKey(HashTableType &hashTable, int32_t partitionIndex,
    VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum)
{
    if (isNeedNullKeyTable) {
        EmplaceFixedKeyToNormalHashTable(hashTable, partitionIndex, vecBatch, vecBatchIdx, buildVectors, buildColNum);
    } else {
        EmplaceFixedNotNullKeyToNormalHashTable(hashTable, partitionIndex, vecBatch, vecBatchIdx, buildVectors,
            buildColNum);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceSingleKey(HashTableType &hashTable, int32_t partitionIndex,
    VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    if (isNeedNullKeyTable) {
        if (buildVectors[0]->GetEncoding() != OMNI_DICTIONARY) {
            EmplaceSingleKeyToNormalHashTable<false>(hashTable, partitionIndex, vecBatch, vecBatchIdx, buildVectors);
        } else {
            EmplaceSingleKeyToNormalHashTable<true>(hashTable, partitionIndex, vecBatch, vecBatchIdx, buildVectors);
        }
    } else {
        if (buildVectors[0]->GetEncoding() != OMNI_DICTIONARY) {
            EmplaceSingleNotNullKeyToNormalHashTable<false>(hashTable, partitionIndex, vecBatch, vecBatchIdx,
                buildVectors);
        } else {
            EmplaceSingleNotNullKeyToNormalHashTable<true>(hashTable, partitionIndex, vecBatch, vecBatchIdx,
                buildVectors);
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <bool isDic, typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceSingleNotNullKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    KeyType key;

    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = (!buildVectors[0]->IsNull(offset));
        if (LIKELY(unNullKey)) {
            if constexpr (isDic) {
                key = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(buildVectors[0])->GetValue(offset);
            } else {
                if (buildVectors[0]->GetEncoding() == OMNI_ENCODING_CONST) {
                    key = static_cast<ConstVector<KeyType> *>(buildVectors[0])->GetConstValue();
                } else {
                    key = reinterpret_cast<Vector<KeyType> *>(buildVectors[0])->GetValue(offset);
                }
            }
            auto ret = hashTable->InsertJoinKeysToHashmap(key);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx) }, arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <bool isDic, typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceSingleKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    KeyType key;
    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = (!buildVectors[0]->IsNull(offset));
        if constexpr (isDic) {
            key = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(buildVectors[0])->GetValue(offset);
        } else {
            if (buildVectors[0]->GetEncoding() == OMNI_ENCODING_CONST) {
                key = static_cast<ConstVector<KeyType> *>(buildVectors[0])->GetConstValue();
            } else {
                key = reinterpret_cast<Vector<KeyType> *>(buildVectors[0])->GetValue(offset);
            }
        }
        if (LIKELY(unNullKey)) {
            auto ret = hashTable->InsertJoinKeysToHashmap(key);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx) }, arenaAllocator);
            }
        } else {
            auto ret = hashTable->InsertNullKeysToHashmap(key);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx) }, arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceMultiKey(HashTableType &hashTable, int32_t partitionIndex,
    VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    if (joinType == OMNI_JOIN_TYPE_FULL) {
        EmplaceMultiKeyToNormalHashTable(hashTable, partitionIndex,
            vecBatch, vecBatchIdx, buildVectors);
    } else {
        EmplaceMultiNotNullKeyToNormalHashTable(hashTable, partitionIndex,
            vecBatch, vecBatchIdx, buildVectors);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceMultiNotNullKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    KeyType key;

    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = true;
        for (int i = 0; i < this->buildHashCols.size(); ++i) {
            unNullKey = unNullKey && (!buildVectors[i]->IsNull(offset));
        }
        if (LIKELY(unNullKey)) {
            if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                          std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t> ||
                          std::is_same_v<KeyType, int128_t>) {
                key = 0;
            }
            ComputeMultiColKey(buildVectors, buildHashCols.size(), offset, key);
            auto ret = hashTable->InsertJoinKeysToHashmap(key);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx) }, arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceMultiKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    KeyType key;

    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = true;
        for (int i = 0; i < this->buildHashCols.size(); ++i) {
            unNullKey = unNullKey && (!buildVectors[i]->IsNull(offset));
        }
        if (LIKELY(unNullKey)) {
            if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                          std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t> ||
                          std::is_same_v<KeyType, int128_t>) {
                key = 0;
            }
            ComputeMultiColKey(buildVectors, buildHashCols.size(), offset, key);
            auto ret = hashTable->InsertJoinKeysToHashmap(key);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx) }, arenaAllocator);
            }
        } else {
            auto ret = hashTable->InsertNullKeysToHashmap(key);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx) }, arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceVariableKey(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum,
    std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes, std::vector<KeyType> &tryRes)
{
    if (isNeedNullKeyTable) {
        EmplaceVariableKeyToNormalHashTable(hashTable, partitionIndex, vecBatch, vecBatchIdx, buildVectors, buildColNum,
            isNotNullKeys, hashes, tryRes);
    } else {
        EmplaceVariableNotNullKeyToNormalHashTable(hashTable, partitionIndex, vecBatch, vecBatchIdx, buildVectors,
            buildColNum, isNotNullKeys, hashes, tryRes);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceVariableNotNullKeyToNormalHashTable(
    HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
    BaseVector **buildVectors, int32_t buildColNum, std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes,
    std::vector<KeyType> &tryRes)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    for (uint32_t offset = 0; offset < rowCount; offset += BLOCK_SIZE) {
        uint32_t maxStep = std::min(rowCount - offset, BLOCK_SIZE);
        for (uint32_t rowIdx = offset, i = 0; rowIdx < offset + maxStep; rowIdx++, i++) {
            hashTable->TryToInsertJoinKeysToHashmap(buildVectors, buildColNum, rowIdx, i, arenaAllocator, tryRes,
                isNotNullKeys);
        }

        hashTable->BatchCalculateHash(tryRes, isNotNullKeys, hashes, maxStep);
        for (uint32_t rowIdx = 0; rowIdx < maxStep; rowIdx++) {
            if (LIKELY(isNotNullKeys[rowIdx])) {
                auto ret = hashTable->InsertJoinKeysToHashmap(tryRes[rowIdx], hashes[rowIdx]);
                if (ret.IsInsert()) {
                    rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                    *rowRef =
                        RowRefListType(static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx));
                    ret.SetValue(rowRef);
                } else {
                    rowRef = ret.GetValue();
                    rowRef->Insert({ static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx) },
                        arenaAllocator);
                }
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceVariableKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum,
    std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes, std::vector<KeyType> &tryRes)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    for (uint32_t offset = 0; offset < rowCount; offset += BLOCK_SIZE) {
        uint32_t maxStep = std::min(rowCount - offset, BLOCK_SIZE);
        for (uint32_t rowIdx = offset, i = 0; rowIdx < offset + maxStep; rowIdx++, i++) {
            hashTable->TryToInsertJoinKeysToHashmap(buildVectors, buildColNum, rowIdx, i, arenaAllocator, tryRes,
                isNotNullKeys);
        }

        hashTable->BatchCalculateHash(tryRes, isNotNullKeys, hashes, maxStep);
        for (uint32_t rowIdx = 0; rowIdx < maxStep; rowIdx++) {
            if (LIKELY(isNotNullKeys[rowIdx])) {
                auto ret = hashTable->InsertJoinKeysToHashmap(tryRes[rowIdx], hashes[rowIdx]);
                if (ret.IsInsert()) {
                    rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                    *rowRef =
                        RowRefListType(static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx));
                    ret.SetValue(rowRef);
                } else {
                    rowRef = ret.GetValue();
                    rowRef->Insert({ static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx) },
                        arenaAllocator);
                }
            } else {
                auto ret = hashTable->InsertNullKeysToHashmap(tryRes[rowIdx]);
                if (ret.IsInsert()) {
                    rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                    *rowRef =
                        RowRefListType(static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx));
                    ret.SetValue(rowRef);
                } else {
                    rowRef = ret.GetValue();
                    rowRef->Insert({ static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx) },
                        arenaAllocator);
                }
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildNormalHashTableWithFixedKey(int32_t partitionIndex,
    uint8_t initDegree)
{
    auto hashTable = std::make_unique<JoinHashTableVariant<KeyType, RowRefListType>>(std::max(initDegree, MIN_DEGREE));
    auto buildColNum = static_cast<int32_t>(this->buildHashCols.size());
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    auto vecBatchesNum = static_cast<int32_t>(vecBatchesOnePartition.size());

    if constexpr (!std::is_same_v<KeyType, StringRef>) {
        BaseVector *buildVectors[buildColNum];
        for (auto j = 0; j < vecBatchesNum; j++) {
            auto vecBatch = vecBatchesOnePartition[j];
            for (int32_t i = 0; i < buildColNum; ++i) {
                buildVectors[i] = vecBatch->Get(this->buildHashCols[i]);
            }
            if (isMultiCols) {
                EmplaceMultiKey<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>>(hashTable,
                    partitionIndex, vecBatch, j, buildVectors);
            } else {
                EmplaceSingleKey<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>>(hashTable,
                    partitionIndex, vecBatch, j, buildVectors);
            }
        }
    } else if constexpr (std::is_same_v<KeyType, StringRef>) {
        BaseVector *buildVectors[buildColNum];
        for (auto j = 0; j < vecBatchesNum; j++) {
            auto vecBatch = vecBatchesOnePartition[j];
            hashTable->ResetFixedKeysIgnoreNullSerializerSimd();
            for (int32_t i = 0; i < buildColNum; ++i) {
                auto curVector = vecBatch->Get(this->buildHashCols[i]);
                if (curVector->GetEncoding() == Encoding::OMNI_DICTIONARY) {
                    hashTable->PushBackFixedKeysIgnoreNullSerializer(
                        dicVectorSerializerFixedKeysIgnoreNullCenter[curVector->GetTypeId()]);
                    hashTable->PushBackFixedKeysIgnoreNullSerializerSimd(
                        dicVectorSerializerFixedKeysIgnoreNullCenterSimd[curVector->GetTypeId()]);
                } else if (curVector->GetEncoding() == Encoding::OMNI_ENCODING_CONST) {
                    hashTable->PushBackFixedKeysIgnoreNullSerializer(
                        constVectorSerializerFixedKeysIgnoreNullCenter[curVector->GetTypeId()]);
                    hashTable->PushBackFixedKeysIgnoreNullSerializerSimd(
                        constVectorSerializerFixedKeysIgnoreNullCenterSimd[curVector->GetTypeId()]);
                } else {
                    hashTable->PushBackFixedKeysIgnoreNullSerializer(
                        vectorSerializerFixedKeysIgnoreNullCenter[curVector->GetTypeId()]);
                    hashTable->PushBackFixedKeysIgnoreNullSerializerSimd(
                        vectorSerializerFixedKeysIgnoreNullCenterSimd[curVector->GetTypeId()]);
                }

                buildVectors[i] = curVector;
            }
            EmplaceFixedKey<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>>(hashTable, partitionIndex,
                vecBatch, j, buildVectors, buildColNum);
        }
    }

    hashTables[partitionIndex] = std::move(hashTable);
    hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
    hashTableSize++;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildNormalHashTableWithVariableKey(int32_t partitionIndex,
    uint8_t initDegree)
{
    if constexpr (std::is_same_v<KeyType, StringRef>) {
        auto hashTable =
            std::make_unique<JoinHashTableVariant<KeyType, RowRefListType>>(std::max(initDegree, MIN_DEGREE));
        auto buildColNum = static_cast<int32_t>(this->buildHashCols.size());
        auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
        auto vecBatchesNum = static_cast<int32_t>(vecBatchesOnePartition.size());

        BaseVector *buildVectors[buildColNum];
        std::vector<int8_t> isNotNullKeys(BLOCK_SIZE);
        std::vector<size_t> hashes(BLOCK_SIZE);
        std::vector<StringRef> tryRes(BLOCK_SIZE);
        for (auto j = 0; j < vecBatchesNum; j++) {
            auto vecBatch = vecBatchesOnePartition[j];
            hashTable->ResetIgnoreNullSerializer();
            for (int32_t i = 0; i < buildColNum; ++i) {
                auto curVector = vecBatch->Get(this->buildHashCols[i]);
                auto typeId = static_cast<size_t>(curVector->GetTypeId());
                VectorSerializerIgnoreNull serializer = nullptr;
                if (curVector->GetEncoding() == Encoding::OMNI_DICTIONARY && typeId < dicVectorSerializerIgnoreNullCenter.size()) {
                    serializer = dicVectorSerializerIgnoreNullCenter[typeId];
                } else if (curVector->GetEncoding() == Encoding::OMNI_ENCODING_CONST &&
                    typeId < constVectorSerializerIgnoreNullCenter.size()) {
                    serializer = constVectorSerializerIgnoreNullCenter[typeId];
                }

                // Complex join keys (e.g. struct) may have only flat serializer registered.
                if (serializer == nullptr && typeId < vectorSerializerIgnoreNullCenter.size()) {
                    serializer = vectorSerializerIgnoreNullCenter[typeId];
                }
                if (serializer == nullptr) {
                    throw OmniException("UNSUPPORTED_ERROR", "No build serializer for join key type");
                }
                hashTable->PushBackIgnoreNullSerializer(serializer);
                buildVectors[i] = curVector;
            }

            EmplaceVariableKey<std::unique_ptr<JoinHashTableVariant<StringRef, RowRefListType>>>(hashTable,
                partitionIndex, vecBatch, j, buildVectors, buildColNum, isNotNullKeys, hashes, tryRes);
        }

        hashTables[partitionIndex] = std::move(hashTable);
        hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
        hashTableSize++;
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceFixedNotNullKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    KeyType key;
    bool unNullKey = false;
    char *keysPool = reinterpret_cast<char *>(arenaAllocator.Allocate(fixedKeysSize * rowCount));
    std::vector<KeyType> keys;
    std::vector<bool> unNullKeys(rowCount, true);
    for (uint32_t i = 0;i < rowCount; i++) {
        keys.emplace_back(StringRef(keysPool, fixedKeysSize));
        keysPool += fixedKeysSize;
    }
    size_t pos = 0;
    for (uint32_t col = 0; col < buildColNum; col++) {
        hashTable->TryToInsertFixedJoinKeysToHashmapSimd(buildVectors, rowCount, col, keys, unNullKeys, pos);
    }
    for (uint32_t offset = 0; offset < rowCount; offset++) {
        if (LIKELY(unNullKeys[offset])) {
            auto ret = hashTable->InsertJoinKeysToHashmap(keys[offset]);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx)}, arenaAllocator);
            }
        }
    }
}
template <typename KeyType, typename RowRefListType>
template <typename HashTableType>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceFixedKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    KeyType key;
    bool unNullKey = false;
    char *keysPool = reinterpret_cast<char *>(arenaAllocator.Allocate(fixedKeysSize * rowCount));
    std::vector<KeyType> keys;
    std::vector<bool> unNullKeys;
    for (size_t i = 0; i < rowCount; i++) {
        unNullKeys.emplace_back(true);
    }
    for (uint32_t i = 0; i < rowCount; i++) {
        keys.emplace_back(StringRef(keysPool, fixedKeysSize));
        keysPool += fixedKeysSize;
    }
    size_t pos = 0;
    for (uint32_t col = 0; col < buildColNum; col++) {
        hashTable->TryToInsertFixedJoinKeysToHashmapSimd(buildVectors, rowCount, col, keys, unNullKeys, pos);
    }
    for (uint32_t offset = 0; offset < rowCount; offset++) {
        if (LIKELY(unNullKeys[offset])) {
            auto ret = hashTable->InsertJoinKeysToHashmap(keys[offset]);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx)}, arenaAllocator);
            }
        } else {
            auto ret = hashTable->InsertNullKeysToHashmap(keys[offset]);
            if (ret.IsInsert()) {
                rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx));
                ret.SetValue(rowRef);
            } else {
                rowRef = ret.GetValue();
                rowRef->Insert({static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx)}, arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildHashTable(int32_t partitionIndex)
{
    if (buildStateReleased_) {
        return;
    }
    /// if build size partition is an empty partition, we need to provide a empty HashTable for probe size.
    if (totalRowCount[partitionIndex] == 0) {
        if (isFixedKeys) {
            BuildNormalHashTableWithFixedKey(partitionIndex, MIN_DEGREE);
        } else {
            BuildNormalHashTableWithVariableKey(partitionIndex, MIN_DEGREE);
        }
        return;
    }
    auto initDegree = static_cast<uint8_t>(std::ceil(log2(totalRowCount[partitionIndex] / LOAD_FACTOR)));
    auto lengthOfArrayHT = static_cast<int64_t>(std::pow(2, initDegree));
    bool shouldBuildArrayTable = false;
    if (buildHashCols.size() == 1) {
        switch (buildTypes->GetIds()[buildHashCols[0]]) {
            case omniruntime::type::OMNI_INT:
            case omniruntime::type::OMNI_DATE32: {
                int32_t min;
                int32_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            case omniruntime::type::OMNI_SHORT: {
                int16_t min;
                int16_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            case omniruntime::type::OMNI_TIMESTAMP:
            case omniruntime::type::OMNI_LONG: {
                int64_t min;
                int64_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            case omniruntime::type::OMNI_BYTE: {
                int8_t min;
                int8_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            default: {
                break;
            }
        }
    }

    if (!shouldBuildArrayTable) {
        if (isFixedKeys) {
            BuildNormalHashTableWithFixedKey(partitionIndex, initDegree);
        } else {
            BuildNormalHashTableWithVariableKey(partitionIndex, initDegree);
        }
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildHashTableAdaptive(int32_t partitionIndex)
{
    if (buildStateReleased_) {
        return;
    }
    // Group probe leaves the sentinel tail unwrapped. Keep one entire group empty
    // at the 0.9 growth threshold so a tiny adaptive seed cannot overwrite a live
    // slot before NeedRehash. With 16-slot groups this starts at 256 slots.
    uint8_t initDegree = MIN_DEGREE;
    while (true) {
        const uint64_t capacity = 1ULL << initDegree;
        const auto threshold = static_cast<uint64_t>(std::ceil(static_cast<double>(capacity) * MAX_LOAD_FACTOR));
        if (capacity - threshold >= Group::kWidth) {
            break;
        }
        ++initDegree;
    }
    if (isFixedKeys) {
        BuildNormalHashTableWithFixedKey(partitionIndex, initDegree);
    } else {
        BuildNormalHashTableWithVariableKey(partitionIndex, initDegree);
    }
}

template <typename KeyType, typename RowRefListType>
int64_t JoinHashTableVariants<KeyType, RowRefListType>::EstimateHashTableInitBytes(int32_t partitionIndex) const
{
    const uint32_t rows = totalRowCount[partitionIndex];
    if (rows == 0) {
        return 0;
    }
    using Map = JoinHashMap<KeyType, RowRefListType *>;
    if (partitionIndex >= 0 && static_cast<uint32_t>(partitionIndex) < hashTables.size() &&
        hashTables[partitionIndex] != nullptr) {
        return static_cast<int64_t>(hashTables[partitionIndex]->EstimateAllocatedBytes() +
            hashTables[partitionIndex]->EstimateRehashAllocation(rows));
    }
    auto initDegree = static_cast<uint8_t>(std::ceil(log2(rows / LOAD_FACTOR)));
    const uint8_t ctorDegree = std::max(initDegree, MIN_DEGREE);
    return static_cast<int64_t>(Map::EstimateRehashAllocationFromEmpty(rows, ctorDegree));
}

template <typename KeyType, typename RowRefListType>
int64_t JoinHashTableVariants<KeyType, RowRefListType>::EstimateRowRefArenaBytes(int32_t partitionIndex) const
{
    return static_cast<int64_t>(totalRowCount[partitionIndex]) * static_cast<int64_t>(sizeOfRowRefList);
}

template <typename KeyType, typename RowRefListType>
template <typename T>
bool JoinHashTableVariants<KeyType, RowRefListType>::TryToBuildArrayTable(uint32_t colIndex, T &min, T &max,
    int64_t rangeUpperBound, int32_t partitionIndex)
{
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    max = std::numeric_limits<T>::min();
    min = std::numeric_limits<T>::max();
    int64_t uint32Max = UINT32_MAX;
    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = vecBatchesOnePartition[vecBatchIdx];
        auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
        auto vector = vecBatch->Get(colIndex);
        if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
            if (!vector->IsNull(0)) {
                auto value = static_cast<ConstVector<T> *>(vector)->GetConstValue();
                max = std::max(max, value);
                min = std::min(min, value);
            }
        } else if (vector->GetEncoding() != OMNI_DICTIONARY) {
            // Caveat: null data might be a random number
            auto valuePtr = unsafe::UnsafeVector::GetRawValues(static_cast<Vector<T> *>(vector));
            if (vector->HasNull()) {
                for (int32_t i = 0; i < rowCount; i++) {
                    if (vector->IsNull(i)) {
                        continue;
                    }
                    auto value = *(valuePtr + i);
                    max = std::max(max, value);
                    min = std::min(min, value);
                }
            } else {
                const auto [minPtr, maxPtr] = std::minmax_element(valuePtr, valuePtr + rowCount);
                max = std::max(max, *maxPtr);
                min = std::min(min, *minPtr);
            }
            // to prevent max - min overflow
            if (max > 0 && min < 0 && min + ARRAY_THRESHOLD * rangeUpperBound < max) {
                return false;
            }
            if (max - min > uint32Max) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (max < min) { // vecBatchCount might be 0, so max and min stay unchanged.
        return false;
    }
    if (min < 0 && std::numeric_limits<T>::max() + min < max) {
        return false;
    }
    if (max - min > uint32Max || max - min > ARRAY_THRESHOLD * rangeUpperBound) {
        return false;
    }

    rangeUpperBound = max - min + 1;
    if (isNeedNullKeyTable) {
        EmplaceKeyToArrayTable(min, max, rangeUpperBound, partitionIndex);
    } else {
        EmplaceNotNullKeyToArrayTable(min, max, rangeUpperBound, partitionIndex);
    }
    return true;
}

template <typename KeyType, typename RowRefListType>
template <typename T>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceNotNullKeyToArrayTable(T &min, T &max,
    int64_t rangeUpperBound, int32_t partitionIndex)
{
    auto hashTable = std::make_unique<DefaultArrayMap<RowRefListType>>(rangeUpperBound);
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    T key;

    auto buildHashCol = buildHashCols[0];
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    for (auto j = 0; j < vecBatchCount; j++) {
        auto vecBatch = vecBatchesOnePartition[j];
        auto baseVector = vecBatch->Get(buildHashCol);
        auto rowCount = vecBatch->GetRowCount();

        if (baseVector->GetEncoding() == OMNI_ENCODING_CONST) {
            if (baseVector->IsNull(0)) {
                continue;
            }
            key = static_cast<ConstVector<T> *>(baseVector)->GetConstValue();
            auto pos = static_cast<size_t>(key - min);
            for (auto offset = 0; offset < rowCount; offset++) {
                auto ret = hashTable->InsertJoinKeysToHashmap(pos);
                if (ret.IsInsert()) {
                    rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                    *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                    ret.SetValue(rowRef);
                } else {
                    rowRef = ret.GetValue();
                    rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                }
            }
            continue;
        }

        auto curVector = reinterpret_cast<Vector<T> *>(baseVector);
        if (!curVector->HasNull()) {
            for (auto offset = 0; offset < rowCount; offset++) {
                key = curVector->GetValue(offset);
                auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                if (ret.IsInsert()) {
                    rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                    *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                    ret.SetValue(rowRef);
                } else {
                    rowRef = ret.GetValue();
                    rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                }
            }
        } else {
            for (auto offset = 0; offset < rowCount; offset++) {
                bool unNullKey = (!curVector->IsNull(offset));
                if (LIKELY(unNullKey)) {
                    key = curVector->GetValue(offset);
                    auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                    if (ret.IsInsert()) {
                        rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                        *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                        ret.SetValue(rowRef);
                    } else {
                        rowRef = ret.GetValue();
                        rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                    }
                }
            }
        }
    }

    arrayTables[partitionIndex] = std::move(hashTable);
    hashTableTypes[partitionIndex] = HashTableImplementationType::ARRAY_HASH_TABLE;
    maxMins[partitionIndex] = std::make_pair(max, min);
    hashTableSize++;
}

template <typename KeyType, typename RowRefListType>
template <typename T>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceKeyToArrayTable(T &min, T &max, int64_t rangeUpperBound,
    int32_t partitionIndex)
{
    auto hashTable = std::make_unique<DefaultArrayMap<RowRefListType>>(rangeUpperBound);
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    RowRefListType *rowRef = nullptr;
    T key;

    auto buildHashCol = buildHashCols[0];
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    for (auto j = 0; j < vecBatchCount; j++) {
        auto vecBatch = vecBatchesOnePartition[j];
        auto baseVector = vecBatch->Get(buildHashCol);
        auto rowCount = vecBatch->GetRowCount();

        if (baseVector->GetEncoding() == OMNI_ENCODING_CONST) {
            if (!baseVector->IsNull(0)) {
                key = static_cast<ConstVector<T> *>(baseVector)->GetConstValue();
                for (auto offset = 0; offset < rowCount; offset++) {
                    auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                    if (ret.IsInsert()) {
                        rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                        *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                        ret.SetValue(rowRef);
                    } else {
                        rowRef = ret.GetValue();
                        rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                    }
                }
            } else {
                for (auto offset = 0; offset < rowCount; offset++) {
                    auto ret = hashTable->InsertNullKeysToHashmap();
                    if (ret.IsInsert()) {
                        rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                        *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                        ret.SetValue(rowRef);
                    } else {
                        rowRef = ret.GetValue();
                        rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                    }
                }
            }
            continue;
        }

        auto curVector = reinterpret_cast<Vector<T> *>(baseVector);
        if (!curVector->HasNull()) {
            for (auto offset = 0; offset < rowCount; offset++) {
                key = curVector->GetValue(offset);
                auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                if (ret.IsInsert()) {
                    rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                    *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                    ret.SetValue(rowRef);
                } else {
                    rowRef = ret.GetValue();
                    rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                }
            }
        } else {
            for (auto offset = 0; offset < rowCount; offset++) {
                bool unNullKey = (!curVector->IsNull(offset));
                if (LIKELY(unNullKey)) {
                    key = curVector->GetValue(offset);
                    auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                    if (ret.IsInsert()) {
                        rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                        *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                        ret.SetValue(rowRef);
                    } else {
                        rowRef = ret.GetValue();
                        rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                    }
                } else {
                    auto ret = hashTable->InsertNullKeysToHashmap();
                    if (ret.IsInsert()) {
                        rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
                        *rowRef = RowRefListType(static_cast<uint32_t>(offset), static_cast<uint32_t>(j));
                        ret.SetValue(rowRef);
                    } else {
                        rowRef = ret.GetValue();
                        rowRef->Insert({ static_cast<uint32_t>(offset), static_cast<uint32_t>(j) }, arenaAllocator);
                    }
                }
            }
        }
    }

    arrayTables[partitionIndex] = std::move(hashTable);
    hashTableTypes[partitionIndex] = HashTableImplementationType::ARRAY_HASH_TABLE;
    maxMins[partitionIndex] = std::make_pair(max, min);
    hashTableSize++;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::Prepare(int32_t partitionIndex)
{
    if (buildStateReleased_) {
        return;
    }
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    uint32_t columnCount = buildTypes->GetSize();

    auto columnsOnePartition = new BaseVector **[columnCount];
    this->columns[partitionIndex] = columnsOnePartition;
    for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
        columnsOnePartition[colIdx] = new BaseVector *[vecBatchCount];
    }

    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = inputVecBatches[partitionIndex][vecBatchIdx];
        for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
            auto vector = vecBatch->Get(static_cast<int32_t>(colIdx));
            columnsOnePartition[colIdx][vecBatchIdx] = vector;
        }
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::InitBuildFilterCols(std::vector<int32_t> &buildFilterCols,
    int32_t originalProbeColsCount, std::vector<std::vector<BaseVector **>> &tableBuildFilterColPtrs)
{
    // Index by fixed slot count (hashTableCount), not hashTableSize. After Path A / restore
    // many slots have columns==nullptr; hashTableSize is a cumulative counter and
    // ResetPartition does not decrement it. assign() drops stale pointers from a previous
    // restore round so BuildJoinPosition does not dereference freed build columns.
    tableBuildFilterColPtrs.assign(hashTableCount, {});
    auto buildFilterColsCount = buildFilterCols.size();
    for (uint32_t hashTableIdx = 0; hashTableIdx < hashTableCount; ++hashTableIdx) {
        auto *partitionColumns = GetColumns(static_cast<int32_t>(hashTableIdx));
        if (partitionColumns == nullptr) {
            continue;
        }
        for (uint32_t i = 0; i < buildFilterColsCount; i++) {
            auto buildFilterCol = buildFilterCols[i] - originalProbeColsCount;
            tableBuildFilterColPtrs[hashTableIdx].emplace_back(partitionColumns[buildFilterCol]);
        }
    }
}

// Forward declaration is not encouraged. Only for LCOV.
template class JoinHashTableVariants<int8_t, RowRefList>;
template class JoinHashTableVariants<int16_t, RowRefList>;
template class JoinHashTableVariants<int32_t, RowRefList>;
template class JoinHashTableVariants<int64_t, RowRefList>;
template class JoinHashTableVariants<Decimal128, RowRefList>;
template class JoinHashTableVariants<StringRef, RowRefList>;
template class JoinHashTableVariants<int8_t, RowRefListWithFlags>;
template class JoinHashTableVariants<int16_t, RowRefListWithFlags>;
template class JoinHashTableVariants<int32_t, RowRefListWithFlags>;
template class JoinHashTableVariants<int64_t, RowRefListWithFlags>;
template class JoinHashTableVariants<Decimal128, RowRefListWithFlags>;
template class JoinHashTableVariants<StringRef, RowRefListWithFlags>;
template class JoinHashTableVariants<int128_t, RowRefList>;
template class JoinHashTableVariants<int128_t, RowRefListWithFlags>;
}
}
