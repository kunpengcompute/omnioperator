/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */

#ifndef OMNI_RUNTIME_COLUMN_MARSHALLER_H
#define OMNI_RUNTIME_COLUMN_MARSHALLER_H

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>
#include "vector/vector_helper.h"
#include "type/string_ref.h"

#include "type/data_type.h"
#include "operator/hashmap/base_hash_map.h"
#include "operator/hashmap/taper_hashtable.h"
#include "operator/omni_id_type_vector_traits.h"
#include "operator/execution_context.h"
#include "vector_marshaller.h"
#include "vector/vector.h"
#include "vector/decoded_vector.h"
#include "row_container.h"

#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif

namespace omniruntime {
namespace op {
using namespace vec;
enum class HandleType {
    serialize,
    fixedInt16,
    fixedInt32,
    fixedInt64,
    packedInt32,
    packedInt64,
    packedInt128,
    NormalizeKey,
    fixed256Bytes,
    onlyOneKey
};
static constexpr uint64_t kNullHash = 1;

template <typename Hashmap> class ColumnSerializeHandler {
public:
    Hashmap hashmap;
    static constexpr bool HasSpecialNullFunc = false;
    using KeyType = typename Hashmap::Keys;
    using ValueType = typename Hashmap::Values;
    using Result = typename Hashmap::ResultType;
    ColumnSerializeHandler(uint8_t initDegree = 16) : hashmap(initDegree) {}

    Result InsertValueToHashmap(BaseVector **groupVectors, int32_t groupColNum, int32_t rowIdx,
        mem::SimpleArenaAllocator &arenaAllocator)
    {
        type::StringRef key;
        for (int32_t groupColIdx = 0; groupColIdx < groupColNum; groupColIdx++) {
            auto curVector = groupVectors[groupColIdx];
            auto &curFunc = serializers[groupColIdx];
            curFunc(curVector, rowIdx, arenaAllocator, key);
        }
        return hashmap.Emplace(key);
    }

    Result InsertDictValueToHashmap(BaseVector **groupVectors, int32_t groupColNum, int32_t rowIdx,
                                mem::SimpleArenaAllocator &arenaAllocator)
    {
        type::StringRef key;
        for (int32_t groupColIdx = 0; groupColIdx < groupColNum; groupColIdx++) {
            auto curVector = groupVectors[groupColIdx];
            auto &curFunc = serializers[groupColIdx];
            curFunc(curVector, rowIdx, arenaAllocator, key);
        }
        return hashmap.Emplace(key);
    }

    Result InsertConstValueToHashmap(BaseVector **groupVectors, int32_t groupColNum, int32_t rowIdx,
                                mem::SimpleArenaAllocator &arenaAllocator)
    {
        type::StringRef key;
        for (int32_t groupColIdx = 0; groupColIdx < groupColNum; groupColIdx++) {
            auto curVector = groupVectors[groupColIdx];
            auto &curFunc = serializers[groupColIdx];
            curFunc(curVector, rowIdx, arenaAllocator, key);
        }
        return hashmap.Emplace(key);
    }

    ALWAYS_INLINE void TryToInsertJoinKeysToHashmap(BaseVector **joinVectors, int32_t joinColNum, int32_t rowIdx,
        int32_t i, mem::SimpleArenaAllocator &arenaAllocator, std::vector<type::StringRef> &keys,
        std::vector<int8_t> &isNotNullKeys)
    {
        keys[i].size = 0;
        keys[i].data = nullptr;
        for (int32_t joinColIdx = 0; joinColIdx < joinColNum; joinColIdx++) {
            auto curVector = joinVectors[joinColIdx];
            auto &curFunc = ignoreNullSerializers[joinColIdx];
            if (UNLIKELY(!curFunc(curVector, rowIdx, arenaAllocator, keys[i]))) {
                isNotNullKeys[i] = false;
                return;
            }
        }
        isNotNullKeys[i] = true;
    }

    ALWAYS_INLINE void TryToInsertFixedJoinKeysToHashmap(BaseVector **joinVectors, int32_t joinColNum, int32_t rowIdx,
        type::StringRef &key, bool &isNotNullKey)
    {
        size_t pos = 0;
        for (int32_t groupColIdx = 0; groupColIdx < joinColNum; groupColIdx++) {
            auto curVector = joinVectors[groupColIdx];
            auto &curFunc = fixedKeysIgnoreNullSerializers[groupColIdx];
            if (UNLIKELY(!curFunc(curVector, rowIdx, key, pos))) {
                isNotNullKey = false;
                return;
            }
        }
        isNotNullKey = true;
    }

    ALWAYS_INLINE void TryToInsertFixedJoinKeysToHashmapSimd(BaseVector **joinVectors, int32_t joinRowNum,
        int32_t colIdx, std::vector<type::StringRef> &keys, std::vector<bool> &isNotNullKey, size_t &pos)
    {
        auto &curFunc = fixedKeysIgnoreNullSerializersSimd[colIdx];
        auto curVector = joinVectors[colIdx];
        for (int32_t rowid = 0; rowid < joinRowNum; rowid++) {
            if (UNLIKELY(!curFunc(curVector, rowid, keys, pos, joinRowNum))) {
                isNotNullKey[rowid] = false;
            }
            isNotNullKey[rowid] = isNotNullKey[rowid] & true;
        }
    }

    ALWAYS_INLINE void BatchCalculateHash(std::vector<KeyType> &keys, std::vector<int8_t> &isNotNullKeys,
        std::vector<size_t> &hashes, int32_t maxStep)
    {
        for (int i = 0; i < maxStep; ++i) {
            if (LIKELY(isNotNullKeys[i])) {
                hashes[i] = hashmap.CalculateHash(keys[i]);
            }
        }
    }

    ALWAYS_INLINE Result InsertJoinKeysToHashmap(KeyType &key)
    {
        return hashmap.EmplaceNotNullKey(key);
    }

    ALWAYS_INLINE Result InsertNullKeysToHashmap(KeyType &key)
    {
        return hashmap.EmplaceNullValue(key);
    }

    ALWAYS_INLINE Result InsertJoinKeysToHashmap(KeyType &key, size_t &hashValue)
    {
        return hashmap.EmplaceNotNullKey(key, hashValue);
    }

    void ParseKeyToCols(const KeyType &key, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int32_t rowIdx)
    {
        auto *pos = key.data;
        for (int32_t i = 0; i < groupColNum; ++i) {
            auto curVectorPtr = groupOutputVectors[i];
            auto deserializeFunc = deserializers[i];
            pos = deserializeFunc(curVectorPtr, rowIdx, pos);
        }
    }

    ALWAYS_INLINE Result FindValueFromHashmap(KeyType &key)
    {
        return hashmap.FindMatchPosition(key);
    }

    void InitSize(int groupBySize)
    {
        serializers.reserve(groupBySize);
        deserializers.reserve(groupBySize);
    }

    void ResetSerializer()
    {
        serializers.clear();
        deserializers.clear();
    }

    void ResetIgnoreNullSerializer()
    {
        ignoreNullSerializers.clear();
    }

    void ResetFixedKeysIgnoreNullSerializer()
    {
        fixedKeysIgnoreNullSerializers.clear();
    }

    void ResetFixedKeysIgnoreNullSerializerSimd()
    {
        fixedKeysIgnoreNullSerializersSimd.clear();
    }

    void PushBackSerializer(VectorSerializer &serializer)
    {
        serializers.push_back(serializer);
    }

    void PushBackIgnoreNullSerializer(VectorSerializerIgnoreNull &serializer)
    {
        ignoreNullSerializers.push_back(serializer);
    }

    void PushBackFixedKeysIgnoreNullSerializer(FixedKeyVectorSerializerIgnoreNull &serializer)
    {
        fixedKeysIgnoreNullSerializers.push_back(serializer);
    }

    void PushBackFixedKeysIgnoreNullSerializerSimd(FixedKeyVectorSerializerIgnoreNullSimd &serializer)
    {
        fixedKeysIgnoreNullSerializersSimd.push_back(serializer);
    }

    void PushBackDeSerializer(VectorDeSerializer &deserializer)
    {
        deserializers.push_back(deserializer);
    }

    size_t GetElementsSize() const
    {
        return hashmap.GetElementsSize();
    }

    void ResetHashmap()
    {
        hashmap.Reset();
    };

private:
    std::vector<VectorSerializer> serializers;
    std::vector<VectorDeSerializer> deserializers;

    std::vector<VectorSerializerIgnoreNull> ignoreNullSerializers;
    std::vector<FixedKeyVectorSerializerIgnoreNull> fixedKeysIgnoreNullSerializers;

    std::vector<FixedKeyVectorSerializerIgnoreNullSimd> fixedKeysIgnoreNullSerializersSimd;
};

class TaperColumnSerializeHandler {
public:
    static constexpr bool HasSpecialNullFunc = false;
    using HashTable = TaperFlatHashTable<int64_t, true>;
    int32_t totalAggValueSize = 0;
    int32_t totalAggStatesSize = 0;
    std::unique_ptr<HashTable> table;
    std::unique_ptr<RowContainer> aggRows;
    std::vector<int64_t> workingHashVals;
    std::vector<int32_t> workingUpdateIndices;
    int32_t workingUpdateCount = 0;
    std::vector<int32_t> keyTypeSizes;
    std::vector<int32_t> keyTypeIds;
    std::vector<bool> isVariableLenType;
    std::vector<int32_t> varcharColIndices;
    int32_t varcharSlotColIdx = -1;
    std::vector<const char*> mergedVarcharCache_;
    int32_t mergedVarcharCacheCount_ = 0;
    std::vector<int32_t> colToVarcharPos_;
    RowContainerIterator rowContainerIter;
    std::vector<char*> rowPtrs;
    std::vector<uint8_t*> groups;

    // Decoded vectors cache: populated once per batch to eliminate encoding branches in hot path
    std::vector<DecodedVector> decodedCols;

    struct ColInfo {
        BaseVector* vector;
        int32_t typeId;
        int32_t offset;
        uint32_t nullByte;
        uint8_t nullMask;
        uint8_t isDictVarchar;
        uint8_t isConstVarchar;
        void* typedVector;
    };
    std::vector<ColInfo> colInfos;

    uint8_t*& RowFromData(char* data)
    {
        return *reinterpret_cast<uint8_t**>(data);
    }

    static constexpr uint32_t ROW_PTR_SIZE = 6;

    /// Store pointer as ROW_PTR_SIZE bytes (48-bit) in the hash table value buffer.
    static ALWAYS_INLINE void SetRowPtr(char* buf, uint8_t* ptr)
    {
        uint64_t val = reinterpret_cast<uint64_t>(ptr);
        memcpy(buf, &val, ROW_PTR_SIZE);
    }

    /// Read pointer from ROW_PTR_SIZE-byte value buffer (zero-extend lower 48 bits).
    static ALWAYS_INLINE uint8_t* GetRowPtr(const char* buf)
    {
        uint64_t val = 0;
        memcpy(&val, buf, ROW_PTR_SIZE);
        return reinterpret_cast<uint8_t*>(val);
    }

    TaperColumnSerializeHandler(mem::SimpleArenaAllocator &pool, int32_t size)
    {
        table = std::make_unique<HashTable>(pool, sizeof(uint64_t), ROW_PTR_SIZE);
        totalAggStatesSize = size;
        totalAggValueSize = size + sizeof(size_t);
    }

    /// Initialize the RowContainer with key type information.
    /// Called after InitSize to set up the row layout with fixed-width key columns.
    /// @param keySizes Fixed row sizes for each key column (sizeof(char*) for VARCHAR, sizeof(char*)+sizeof(size_t) for complex types)
    /// @param isVariableLen True for each column that stores variable-length data (VARCHAR, ARRAY, etc.)
    /// @param typeIds DataTypeId for each key column (used by SpillExtract to distinguish VARCHAR from complex types)
    /// @param varcharCols Indices of VARCHAR/CHAR/VARBINARY columns to merge into a single slot
    /// @param pool Memory pool for row allocation
    void InitRowContainer(const std::vector<int32_t>& keySizes,
                          const std::vector<bool>& isVariableLen,
                          const std::vector<int32_t>& typeIds,
                          const std::vector<int32_t>& varcharCols,
                          mem::SimpleArenaAllocator& pool)
    {
        keyTypeSizes = keySizes;
        isVariableLenType = isVariableLen;
        keyTypeIds = typeIds;
        varcharColIndices = varcharCols;
        if (varcharCols.size() > 1) {
            varcharSlotColIdx = varcharCols[0];
        }
        aggRows = std::make_unique<RowContainer>(
            keySizes, static_cast<int32_t>(keySizes.size()),
            totalAggStatesSize, pool);
    }

    /// Get the offset where AggState data begins within a row.
    int32_t AggStateOffset() const
    {
        return aggRows ? aggRows->AggStateOffset() : totalAggValueSize;
    }

    void EmplaceTable(BaseVector **groupVectors, int32_t groupColNum, int32_t rowsNum,
        std::vector<uint8_t*>& groups, std::vector<uint8_t*>& newGroups, Encoding encoding)
    {
        // Caller must call DecodeGroupByColumns() first. This delegates to the
        // decoded path which eliminates all encoding branches in the hot loop.
        (void)encoding;
        EmplaceTableWithDecode(groupColNum, rowsNum, groups, newGroups);
    }

    /// Decode all group-by columns upfront. Call once per batch before EmplaceTable.
    void DecodeGroupByColumns(BaseVector** groupVectors, int32_t groupColNum, int32_t rowsNum)
    {
        if (static_cast<int32_t>(decodedCols.size()) != groupColNum) {
            decodedCols.resize(groupColNum);
        }
        for (int32_t i = 0; i < groupColNum; ++i) {
            decodedCols[i].Decode(groupVectors[i], rowsNum);
        }
    }

    /// Templated dispatch for GetUnequalsNum using DecodedVector.
    /// TypeId and HasNull are compile-time constants → zero runtime branches in the hot loop.
    template <DataTypeId Kind, bool HasNull>
    int32_t GetUnequalsNumTyped(int32_t colIdx, int32_t count, int32_t offset,
                                uint32_t nullByte, uint8_t nullMask,
                                int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
        using T = typename NativeType<Kind>::type;
        const DecodedVector& decoded = decodedCols[colIdx];
        auto layout = decoded.GetLayout();

        if (layout == DVecLayout::Constant) {
            return BatchCompareDecodedConst<T, HasNull>(decoded, count, offset, nullByte, nullMask, indices, idxFrom, groups);
        } else if (layout == DVecLayout::Dictionary) {
            return BatchCompareDecoded<T, HasNull, true>(decoded, count, offset, nullByte, nullMask, indices, idxFrom, groups);
        }
        return BatchCompareDecoded<T, HasNull>(decoded, count, offset, nullByte, nullMask, indices, idxFrom, groups);
    }

    /// Varchar specialization for GetUnequalsNumTyped.
    template <bool HasNull>
    int32_t GetUnequalsNumVarcharTyped(int32_t colIdx, int32_t count, int32_t offset,
                                       uint32_t nullByte, uint8_t nullMask,
                                       int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
        return BatchCompareVarcharDecoded<HasNull>(decodedCols[colIdx], colIdx, count, offset, nullByte, nullMask, indices, idxFrom, groups);
    }

    /// Templated dispatch for BatchStoreKeyColumn using DecodedVector.
    template <DataTypeId Kind, bool HasNull>
    void BatchStoreKeyColumnTyped(int32_t colIdx, int32_t offset, uint32_t nullByte, uint8_t nullMask,
                                  uint8_t** rows, uint32_t* rowIndices, int32_t rowCount)
    {
        using T = typename NativeType<Kind>::type;
        const DecodedVector& decoded = decodedCols[colIdx];
        auto layout = decoded.GetLayout();

        if (layout == DVecLayout::Constant) {
            BatchStoreDecodedConst<T, HasNull>(decoded, offset, nullByte, nullMask, rows, rowIndices, rowCount);
        } else if (layout == DVecLayout::Dictionary) {
            BatchStoreDecoded<T, HasNull, true>(decoded, offset, nullByte, nullMask, rows, rowIndices, rowCount);
        } else {
            BatchStoreDecoded<T, HasNull>(decoded, offset, nullByte, nullMask, rows, rowIndices, rowCount);
        }
    }

    /// Varchar specialization for BatchStoreKeyColumnTyped.
    template <bool HasNull>
    void BatchStoreKeyColumnVarcharTyped(int32_t colIdx, int32_t offset, uint32_t nullByte, uint8_t nullMask,
                                         uint8_t** rows, uint32_t* rowIndices, int32_t rowCount)
    {
        BatchStoreVarcharDecoded<HasNull>(decodedCols[colIdx], colIdx, offset, nullByte, nullMask, rows, rowIndices, rowCount);
    }

    /// EmplaceTable variant that uses DecodedVector for compare/store — zero encoding branches in hot path.
    /// Caller must call DecodeGroupByColumns() first.
    void EmplaceTableWithDecode(int32_t groupColNum, int32_t rowsNum,
        std::vector<uint8_t*>& groups, std::vector<uint8_t*>& newGroups)
    {
        groups.resize(rowsNum);
        std::vector<uint32_t> newGroupRowIndices(rowsNum);
        int32_t newGroupCount = 0;
        size_t newGroupsStartIdx = newGroups.size();
        auto initRow = [&](uint32_t rowIdx, char* data) -> char* {
            auto* row = aggRows->NewRow();
            SetRowPtr(data, reinterpret_cast<uint8_t*>(row));
            newGroups.push_back(GetRowPtr(data));
            newGroupRowIndices[newGroupCount++] = rowIdx;
            return row;
        };
        workingUpdateIndices.resize(rowsNum);
        workingUpdateCount = 0;
        workingHashVals.resize(rowsNum);

        // Hash using decoded columns — zero runtime encoding branches
        for (int32_t i = 0; i < groupColNum; ++i) {
            auto type = decodedCols[i].GetTypeId();
            if (type == type::OMNI_ARRAY) {
                DoArrayHashWithDecode(decodedCols[i], rowsNum, i > 0);
            } else if (type == type::OMNI_ROW) {
                DoRowHash(decodedCols[i].Base(), rowsNum, workingHashVals);
            } else {
                #define HASH_DECODE_DISPATCH(TID) \
                    case TID: \
                        DoHashWithDecode<TID>(decodedCols[i], rowsNum, i > 0); \
                        break;

                switch (type) {
                    HASH_DECODE_DISPATCH(type::OMNI_BYTE)
                    HASH_DECODE_DISPATCH(type::OMNI_SHORT)
                    HASH_DECODE_DISPATCH(type::OMNI_INT)
                    HASH_DECODE_DISPATCH(type::OMNI_DATE32)
                    HASH_DECODE_DISPATCH(type::OMNI_TIME32)
                    HASH_DECODE_DISPATCH(type::OMNI_LONG)
                    HASH_DECODE_DISPATCH(type::OMNI_TIMESTAMP)
                    HASH_DECODE_DISPATCH(type::OMNI_DECIMAL64)
                    HASH_DECODE_DISPATCH(type::OMNI_DATE64)
                    HASH_DECODE_DISPATCH(type::OMNI_TIME64)
                    HASH_DECODE_DISPATCH(type::OMNI_DOUBLE)
                    HASH_DECODE_DISPATCH(type::OMNI_FLOAT)
                    HASH_DECODE_DISPATCH(type::OMNI_DECIMAL128)
                    HASH_DECODE_DISPATCH(type::OMNI_BOOLEAN)
                    HASH_DECODE_DISPATCH(type::OMNI_VARCHAR)
                    HASH_DECODE_DISPATCH(type::OMNI_CHAR)
                    HASH_DECODE_DISPATCH(type::OMNI_VARBINARY)
                    default: break;
                }
                #undef HASH_DECODE_DISPATCH
            }
        }

        table->EmplaceBatch(
            workingHashVals.data(),
            rowsNum,
            [&](uint32_t) { return false; },
            [&](uint32_t rowIdx, char* data) { initRow(rowIdx, data); },
            [&](uint32_t rowIdx, char* data, bool initFlag) {
                groups[rowIdx] = GetRowPtr(data);
                if (!initFlag) {
                    workingUpdateIndices[workingUpdateCount++] = rowIdx;
                }
            });

        if (newGroupCount > 0) {
            // Store keys for new groups using decoded vectors
            // Store merged VARCHAR columns first (all in one contiguous block)
            if (varcharColIndices.size() > 1) {
                BatchStoreMergedVarcharColumns(groupColNum,
                    newGroups.data() + newGroupsStartIdx, newGroupRowIndices.data(), newGroupCount);
            }
            for (int32_t colIdx = 0; colIdx < groupColNum; ++colIdx) {
                auto col = aggRows->ColumnAt(colIdx);
                auto offset = col.Offset();
                auto nullByte = col.NullByte();
                auto nullMask = col.NullMask();
                bool hasNull = decodedCols[colIdx].HasNull();
                auto typeId = decodedCols[colIdx].GetTypeId();

                // Skip merged VARCHAR columns (already handled by BatchStoreMergedVarcharColumns)
                if (keyTypeSizes[colIdx] == 0 && colIdx != varcharSlotColIdx) {
                    continue;
                }

                if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
                    if (varcharColIndices.size() > 1 && colIdx == varcharSlotColIdx) {
                        continue; // merged block pointer already stored
                    } else {
                        if (hasNull) {
                            BatchStoreKeyColumnVarcharTyped<true>(colIdx, offset, nullByte, nullMask,
                                newGroups.data() + newGroupsStartIdx, newGroupRowIndices.data(), newGroupCount);
                        } else {
                            BatchStoreKeyColumnVarcharTyped<false>(colIdx, offset, nullByte, nullMask,
                                newGroups.data() + newGroupsStartIdx, newGroupRowIndices.data(), newGroupCount);
                        }
                    }
                } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_ROW) {
                    // Complex types: fall back to original BatchStoreComplex
                    for (int32_t i = 0; i < newGroupCount; ++i) {
                        char* row = reinterpret_cast<char*>(newGroups[newGroupsStartIdx + i]);
                        auto rowIdx = newGroupRowIndices[i];
                        if (hasNull && decodedCols[colIdx].IsNull(rowIdx)) {
                            RowContainer::SetNullAt(row, nullByte, nullMask);
                        } else {
                            RowContainer::ClearNullAt(row, nullByte, nullMask);
                            type::StringRef key; key.data = nullptr; key.size = 0;
                            auto& curFunc = serializers[colIdx];
                            curFunc(decodedCols[colIdx].Base(), rowIdx, table->Pool(), key);
                            *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                            *reinterpret_cast<size_t*>(row + offset + sizeof(char*)) = key.size;
                        }
                    }
                } else {
                    // Fixed-width types: dispatch on TypeId + HasNull
                    #define STORE_DISPATCH(TID, CPP_TYPE) \
                        case TID: \
                            if (hasNull) { \
                                BatchStoreKeyColumnTyped<TID, true>(colIdx, offset, nullByte, nullMask, \
                                    newGroups.data() + newGroupsStartIdx, newGroupRowIndices.data(), newGroupCount); \
                            } else { \
                                BatchStoreKeyColumnTyped<TID, false>(colIdx, offset, nullByte, nullMask, \
                                    newGroups.data() + newGroupsStartIdx, newGroupRowIndices.data(), newGroupCount); \
                            } \
                            break;

                    switch (typeId) {
                        STORE_DISPATCH(type::OMNI_BYTE, int8_t)
                        STORE_DISPATCH(type::OMNI_SHORT, int16_t)
                        STORE_DISPATCH(type::OMNI_INT, int32_t)
                        STORE_DISPATCH(type::OMNI_DATE32, int32_t)
                        STORE_DISPATCH(type::OMNI_TIME32, int32_t)
                        STORE_DISPATCH(type::OMNI_LONG, int64_t)
                        STORE_DISPATCH(type::OMNI_TIMESTAMP, int64_t)
                        STORE_DISPATCH(type::OMNI_DECIMAL64, int64_t)
                        STORE_DISPATCH(type::OMNI_DATE64, int64_t)
                        STORE_DISPATCH(type::OMNI_TIME64, int64_t)
                        STORE_DISPATCH(type::OMNI_DOUBLE, double)
                        STORE_DISPATCH(type::OMNI_FLOAT, float)
                        STORE_DISPATCH(type::OMNI_DECIMAL128, Decimal128)
                        STORE_DISPATCH(type::OMNI_BOOLEAN, bool)
                        default: break;
                    }
                    #undef STORE_DISPATCH
                }
            }
        }

        if (workingUpdateCount == 0) {
            return;
        }

        // Compare keys for existing groups using decoded vectors
        PrepareColInfosForDecode(groupColNum);
        int32_t unequalsNum = GetUnequalsNumWithDecode(workingUpdateCount, groupColNum, groups.data());

        for (int32_t i = 0; i < unequalsNum; i++) {
            auto rowIdx = workingUpdateIndices[i];
            table->Emplace(
                workingHashVals[rowIdx],
                [&](auto, TaperHashTableChunk& chunk, uint8_t slot) {
                    auto* row = GetRowPtr(table->GetChunkValue(chunk, slot).buf);
                    return CompareKeysWithDecode(row, groupColNum, rowIdx);
                },
                [&](char* data) {
                    auto* row = initRow(rowIdx, data);
                    for (int32_t colIdx = 0; colIdx < groupColNum; ++colIdx) {
                        StoreKeyOneRowFromDecode(colIdx, row, rowIdx);
                    }
                },
                [&](char* data, bool) { groups[rowIdx] = GetRowPtr(data); });
        }
        workingUpdateCount = 0;
    }

    /// Cache column info for CompareKeysWithDecode (avoids repeated RowContainer::ColumnAt lookups).
    void PrepareColInfosForDecode(int32_t groupColNum)
    {
        if (static_cast<int32_t>(colInfos.size()) != groupColNum) {
            colInfos.resize(groupColNum);
        }
        for (int32_t i = 0; i < groupColNum; ++i) {
            auto col = aggRows->ColumnAt(i);
            colInfos[i].vector = decodedCols[i].Base();
            colInfos[i].typeId = decodedCols[i].GetTypeId();
            colInfos[i].offset = col.Offset();
            colInfos[i].nullByte = col.NullByte();
            colInfos[i].nullMask = col.NullMask();
            auto tid = colInfos[i].typeId;
            if (tid == type::OMNI_VARCHAR || tid == type::OMNI_CHAR || tid == type::OMNI_VARBINARY) {
                auto enc = decodedCols[i].Base()->GetEncoding();
                colInfos[i].isDictVarchar = (enc == vec::OMNI_DICTIONARY) ? 1 : 0;
                colInfos[i].isConstVarchar = (enc == vec::OMNI_ENCODING_CONST) ? 1 : 0;
            } else {
                colInfos[i].isDictVarchar = 0;
                colInfos[i].isConstVarchar = 0;
            }
            colInfos[i].typedVector = nullptr;
        }
    }

    /// GetUnequalsNum using DecodedVector — dispatch once per column, then zero branches in hot loop.
    int32_t GetUnequalsNumWithDecode(int32_t count, int32_t groupColNum, uint8_t* const* groups)
    {
        // Pre-compute merged VARCHAR pointers for all rows to avoid re-walking in each column's compare
        mergedVarcharCache_.clear();
        colToVarcharPos_.assign(groupColNum, -1);
        mergedVarcharCacheCount_ = 0;
        if (varcharColIndices.size() > 1) {
            mergedVarcharCacheCount_ = varcharColIndices.size();
            // Index cache by absolute row position (idx) to survive workingUpdateIndices swaps
            int32_t maxIdx = 0;
            for (int32_t wi = 0; wi < count; ++wi) {
                maxIdx = std::max(maxIdx, workingUpdateIndices[wi]);
            }
            mergedVarcharCache_.resize((maxIdx + 1) * mergedVarcharCacheCount_);
            for (int32_t i = 0; i < mergedVarcharCacheCount_; ++i) {
                colToVarcharPos_[varcharColIndices[i]] = i;
            }
            for (int32_t wi = 0; wi < count; ++wi) {
                int32_t idx = workingUpdateIndices[wi];
                GetAllMergedVarcharPtrs(reinterpret_cast<const char*>(groups[idx]),
                    &mergedVarcharCache_[idx * mergedVarcharCacheCount_], mergedVarcharCacheCount_);
            }
        }

        int32_t idxFrom = 0;
        for (int32_t groupColIdx = 0; groupColIdx < groupColNum; ++groupColIdx) {
            if (idxFrom >= count) break;
            auto col = aggRows->ColumnAt(groupColIdx);
            auto offset = col.Offset();
            auto nullByte = col.NullByte();
            auto nullMask = col.NullMask();
            bool hasNull = decodedCols[groupColIdx].HasNull();
            auto typeId = decodedCols[groupColIdx].GetTypeId();

            #define COMPARE_DISPATCH(TID) \
                case TID: \
                    if (hasNull) { \
                        GetUnequalsNumTyped<TID, true>(groupColIdx, count, offset, nullByte, nullMask, workingUpdateIndices.data(), idxFrom, groups); \
                    } else { \
                        GetUnequalsNumTyped<TID, false>(groupColIdx, count, offset, nullByte, nullMask, workingUpdateIndices.data(), idxFrom, groups); \
                    } \
                    break;

            if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
                if (hasNull) {
                    GetUnequalsNumVarcharTyped<true>(groupColIdx, count, offset, nullByte, nullMask, workingUpdateIndices.data(), idxFrom, groups);
                } else {
                    GetUnequalsNumVarcharTyped<false>(groupColIdx, count, offset, nullByte, nullMask, workingUpdateIndices.data(), idxFrom, groups);
                }
            } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_ROW) {
                auto &curFunc = comparators[groupColIdx];
                auto &decoded = decodedCols[groupColIdx];
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = workingUpdateIndices[i];
                    uint8_t *row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char *>(row), nullByte, nullMask);
                    if (rowIsNull != decoded.IsNull(idx)) {
                        std::swap(workingUpdateIndices[i], workingUpdateIndices[idxFrom]); idxFrom++;
                        continue;
                    }
                    if (rowIsNull)
                        continue;
                    uint8_t *addr = reinterpret_cast<uint8_t *>(
                        *reinterpret_cast<char **>(reinterpret_cast<char *>(row) + offset));
                    if (!curFunc(*decoded.Base(), idx, addr)) {
                        std::swap(workingUpdateIndices[i], workingUpdateIndices[idxFrom]); idxFrom++;
                    }
                }
            } else {
                switch (typeId) {
                    COMPARE_DISPATCH(type::OMNI_BYTE)
                    COMPARE_DISPATCH(type::OMNI_SHORT)
                    COMPARE_DISPATCH(type::OMNI_INT)
                    COMPARE_DISPATCH(type::OMNI_DATE32)
                    COMPARE_DISPATCH(type::OMNI_TIME32)
                    COMPARE_DISPATCH(type::OMNI_LONG)
                    COMPARE_DISPATCH(type::OMNI_TIMESTAMP)
                    COMPARE_DISPATCH(type::OMNI_DECIMAL64)
                    COMPARE_DISPATCH(type::OMNI_DATE64)
                    COMPARE_DISPATCH(type::OMNI_TIME64)
                    COMPARE_DISPATCH(type::OMNI_DOUBLE)
                    COMPARE_DISPATCH(type::OMNI_FLOAT)
                    COMPARE_DISPATCH(type::OMNI_DECIMAL128)
                    COMPARE_DISPATCH(type::OMNI_BOOLEAN)
                    default: break;
                }
            }
            #undef COMPARE_DISPATCH
        }
        return idxFrom;
    }

    /// CompareKeys using DecodedVector — zero encoding branches per row.
    bool CompareKeysWithDecode(uint8_t* row, int32_t groupColNum, int32_t rowIdx)
    {
        std::vector<const char*> varcharPtrsCache(groupColNum, nullptr);
        bool hasMergedVarchar = (varcharColIndices.size() > 1);
        if (hasMergedVarchar) {
            GetAllMergedVarcharPtrs(reinterpret_cast<const char*>(row), varcharPtrsCache.data(), groupColNum);
        }

        for (int32_t groupColIdx = 0; groupColIdx < groupColNum; groupColIdx++) {
            auto& ci = colInfos[groupColIdx];
            auto offset = ci.offset;
            bool rowIsNull = RowContainer::IsNullAt(
                reinterpret_cast<char*>(row), ci.nullByte, ci.nullMask);
            bool vecIsNull = decodedCols[groupColIdx].IsNull(rowIdx);
            if (rowIsNull != vecIsNull) return false;
            if (rowIsNull) continue;

            auto typeId = ci.typeId;
            #define COMPARE_KEY_DISPATCH(TID, CPP_TYPE) \
                case TID: \
                    if (RowContainer::ReadValue<CPP_TYPE>(reinterpret_cast<char*>(row), offset) != \
                        decodedCols[groupColIdx].GetValue<CPP_TYPE>(rowIdx)) return false; \
                    break;

            if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
                const char* varcharData;
                if (hasMergedVarchar) {
                    varcharData = varcharPtrsCache[colToVarcharPos_[groupColIdx]];
                } else {
                    auto col = aggRows->ColumnAt(groupColIdx);
                    varcharData = *reinterpret_cast<char**>(reinterpret_cast<char*>(row) + col.Offset());
                }
                if (varcharData == nullptr || varcharData[0] == 0) return false;
                auto* rowData = reinterpret_cast<const uint8_t*>(varcharData);
                std::string_view val;
                auto layout = decodedCols[groupColIdx].GetLayout();
                if (layout == DVecLayout::Constant) {
                    val = static_cast<ConstVector<std::string_view>*>(decodedCols[groupColIdx].Base())->GetConstValue();
                } else if (layout == DVecLayout::Dictionary) {
                    val = static_cast<Vector<DictionaryContainer<std::string_view>>*>(decodedCols[groupColIdx].Base())->GetValue(rowIdx);
                } else {
                    val = static_cast<Vector<LargeStringContainer<std::string_view>>*>(decodedCols[groupColIdx].Base())->GetValue(rowIdx);
                }
                if (!CompareVarcharFromRow(rowData, val)) return false;
            } else {
                switch (typeId) {
                    COMPARE_KEY_DISPATCH(type::OMNI_BYTE, int8_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_SHORT, int16_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_INT, int32_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_DATE32, int32_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_TIME32, int32_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_LONG, int64_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_TIMESTAMP, int64_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_DECIMAL64, int64_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_DATE64, int64_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_TIME64, int64_t)
                    COMPARE_KEY_DISPATCH(type::OMNI_DOUBLE, double)
                    COMPARE_KEY_DISPATCH(type::OMNI_FLOAT, float)
                    COMPARE_KEY_DISPATCH(type::OMNI_DECIMAL128, Decimal128)
                    COMPARE_KEY_DISPATCH(type::OMNI_BOOLEAN, bool)
                    default: {
                        auto& curFunc = comparators[groupColIdx];
                        uint8_t* addr = reinterpret_cast<uint8_t*>(
                            *reinterpret_cast<char**>(reinterpret_cast<char*>(row) + offset));
                        if (!curFunc(*decodedCols[groupColIdx].Base(), rowIdx, addr)) return false;
                        break;
                    }
                }
            }
            #undef COMPARE_KEY_DISPATCH
        }
        return true;
    }

    /// Get VARCHAR value string_view from decoded vector, handling all encodings.
    static std::string_view GetVarcharFromDecoded(const DecodedVector& decoded, int32_t rowIdx) {
        auto layout = decoded.GetLayout();
        if (layout == DVecLayout::Constant) {
            return static_cast<ConstVector<std::string_view>*>(decoded.Base())->GetConstValue();
        } else if (layout == DVecLayout::Dictionary) {
            return static_cast<Vector<DictionaryContainer<std::string_view>>*>(decoded.Base())->GetValue(rowIdx);
        } else {
            return static_cast<Vector<LargeStringContainer<std::string_view>>*>(decoded.Base())->GetValue(rowIdx);
        }
    }

    /// Compute the serialized size of a non-null VARCHAR value (matches VariableTypeSerializer format).
    static int32_t ComputeVarcharSerializedSize(const DecodedVector& decoded, int32_t rowIdx) {
        auto value = GetVarcharFromDecoded(decoded, rowIdx);
        auto stringLen = static_cast<int32_t>(value.size());
        uint8_t rowLenSize = (stringLen <= 0xFF) ? 1 : (stringLen <= 0xFFFF) ? 2 : 4;
        return sizeof(uint8_t) + rowLenSize + stringLen;
    }

    /// Serialize a non-null VARCHAR value directly into a buffer.
    /// Returns the number of bytes written.
    static int32_t SerializeVarcharToBuffer(char* writePos, const DecodedVector& decoded, int32_t rowIdx) {
        auto value = GetVarcharFromDecoded(decoded, rowIdx);
        auto stringLen = static_cast<int32_t>(value.size());
        uint8_t rowLenSize = (stringLen <= 0xFF) ? 1 : (stringLen <= 0xFFFF) ? 2 : 4;
        *reinterpret_cast<uint8_t*>(writePos) = rowLenSize;
        memcpy(writePos + sizeof(uint8_t), &stringLen, rowLenSize);
        memcpy(writePos + sizeof(uint8_t) + rowLenSize, value.data(), stringLen);
        return sizeof(uint8_t) + rowLenSize + stringLen;
    }

    /// Store one key column value from decoded vector for a single row.
    void StoreKeyOneRowFromDecode(int32_t colIdx, char* row, int32_t rowIdx)
    {
        auto col = aggRows->ColumnAt(colIdx);
        auto offset = col.Offset();
        auto nullByte = col.NullByte();
        auto nullMask = col.NullMask();
        auto& decoded = decodedCols[colIdx];
        auto typeId = decoded.GetTypeId();

        // Skip merged VARCHAR columns (handled by the first/slot column)
        if (varcharColIndices.size() > 1 &&
            (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) &&
            colIdx != varcharSlotColIdx) {
            return;
        }

        // For the merged VARCHAR slot column, store all VARCHAR columns in one block
        if (varcharColIndices.size() > 1 && colIdx == varcharSlotColIdx) {
            int32_t totalSize = 0;
            for (int32_t vcIdx : varcharColIndices) {
                if (decodedCols[vcIdx].IsNull(rowIdx)) {
                    totalSize += 1;
                } else {
                    totalSize += ComputeVarcharSerializedSize(decodedCols[vcIdx], rowIdx);
                }
            }
            char* blockStart = nullptr;
            if (totalSize > 0) {
                const uint8_t* tmp = nullptr;
                table->Pool().AllocateContinue(totalSize, tmp);
                blockStart = const_cast<char*>(reinterpret_cast<const char*>(tmp));
            }
            char* writePos = blockStart;
            for (int32_t vcIdx : varcharColIndices) {
                auto vcCol = aggRows->ColumnAt(vcIdx);
                auto vcNullByte = vcCol.NullByte();
                auto vcNullMask = vcCol.NullMask();
                if (decodedCols[vcIdx].IsNull(rowIdx)) {
                    RowContainer::SetNullAt(row, vcNullByte, vcNullMask);
                    if (writePos) { *writePos = 0; writePos += 1; }
                } else {
                    RowContainer::ClearNullAt(row, vcNullByte, vcNullMask);
                    writePos += SerializeVarcharToBuffer(writePos, decodedCols[vcIdx], rowIdx);
                }
            }
            *reinterpret_cast<char**>(row + offset) = blockStart;
            return;
        }

        if (decoded.IsNull(rowIdx)) {
            RowContainer::SetNullAt(row, nullByte, nullMask);
            return;
        }
        RowContainer::ClearNullAt(row, nullByte, nullMask);

        #define STORE_ONE_DISPATCH(TID, CPP_TYPE) \
            case TID: \
                RowContainer::StoreValue(row, offset, decoded.GetValue<CPP_TYPE>(rowIdx)); \
                break;

        if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
            type::StringRef key; key.data = nullptr; key.size = 0;
            auto& curFunc = serializers[colIdx];
            curFunc(decoded.Base(), rowIdx, table->Pool(), key);
            *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
        } else {
            switch (typeId) {
                STORE_ONE_DISPATCH(type::OMNI_BYTE, int8_t)
                STORE_ONE_DISPATCH(type::OMNI_SHORT, int16_t)
                STORE_ONE_DISPATCH(type::OMNI_INT, int32_t)
                STORE_ONE_DISPATCH(type::OMNI_DATE32, int32_t)
                STORE_ONE_DISPATCH(type::OMNI_TIME32, int32_t)
                STORE_ONE_DISPATCH(type::OMNI_LONG, int64_t)
                STORE_ONE_DISPATCH(type::OMNI_TIMESTAMP, int64_t)
                STORE_ONE_DISPATCH(type::OMNI_DECIMAL64, int64_t)
                STORE_ONE_DISPATCH(type::OMNI_DATE64, int64_t)
                STORE_ONE_DISPATCH(type::OMNI_TIME64, int64_t)
                STORE_ONE_DISPATCH(type::OMNI_DOUBLE, double)
                STORE_ONE_DISPATCH(type::OMNI_FLOAT, float)
                STORE_ONE_DISPATCH(type::OMNI_DECIMAL128, Decimal128)
                STORE_ONE_DISPATCH(type::OMNI_BOOLEAN, bool)
                default: {
                    type::StringRef key; key.data = nullptr; key.size = 0;
                    auto& curFunc = serializers[colIdx];
                    curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                    *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                    *reinterpret_cast<size_t*>(row + offset + sizeof(char*)) = key.size;
                    break;
                }
            }
        }
        #undef STORE_ONE_DISPATCH
    }

    /// Compare VARCHAR data in serialized row format directly with std::string_view.
    /// Row format: [rowLenSize(1B)][length(rowLenSize bytes)][data]
    /// Callers MUST guarantee rowData[0] != 0 (null case handled before calling).
    static ALWAYS_INLINE bool CompareVarcharFromRow(const uint8_t* rowData, std::string_view sv) {
        uint8_t rowLenSize = rowData[0];
        size_t stringLen;
        switch (rowLenSize) {
        case 1: stringLen = *reinterpret_cast<const uint8_t*>(rowData + 1); break;
        case 2: stringLen = *reinterpret_cast<const uint16_t*>(rowData + 1); break;
        case 4: stringLen = *reinterpret_cast<const uint32_t*>(rowData + 1); break;
        default: __builtin_unreachable();
        }

        if (stringLen != sv.size()) return false;
        if (stringLen == 0) return true;

        const char* rowDataPtr = reinterpret_cast<const char*>(rowData + 1 + rowLenSize);
        return memcmp(rowDataPtr, sv.data(), stringLen) == 0;
    }

    /// Fill all merged VARCHAR data pointers for a row in one pass.
    /// Returns the number of varchar columns filled (0 for single/non-merged).
    int32_t GetAllMergedVarcharPtrs(const char* row, const char** outPtrs, int32_t maxCount) const {
        if (varcharColIndices.size() <= 1) return 0;
        auto varcharSlotCol = aggRows->ColumnAt(varcharSlotColIdx);
        const char* pos = *reinterpret_cast<char**>(const_cast<char*>(row) + varcharSlotCol.Offset());
        int32_t count = 0;
        for (int32_t vcIdx : varcharColIndices) {
            if (count >= maxCount) break;
            auto col = aggRows->ColumnAt(vcIdx);
            auto nullByte = col.NullByte();
            auto nullMask = col.NullMask();
            if (RowContainer::IsNullAt(row, nullByte, nullMask)) {
                outPtrs[count++] = nullptr;
                if (pos != nullptr) pos += 1;
            } else {
                outPtrs[count++] = (pos != nullptr) ? pos : nullptr;
                if (pos != nullptr) pos += ComputeVarCharSerializedSize(pos);
            }
        }
        return count;
    }

    /// Get the serialized data pointer for a specific VARCHAR column in a row.
    /// For single VARCHAR, reads the pointer directly from the row slot.
    /// Prefer GetAllMergedVarcharPtrs for hot paths with multiple VARCHAR columns.
    const char* GetMergedVarcharData(const char* row, int32_t targetVarcharColIdx) const {
        if (varcharColIndices.size() <= 1) {
            auto col = aggRows->ColumnAt(targetVarcharColIdx);
            return *reinterpret_cast<char**>(const_cast<char*>(row) + col.Offset());
        }
        auto varcharSlotCol = aggRows->ColumnAt(varcharSlotColIdx);
        const char* pos = *reinterpret_cast<char**>(const_cast<char*>(row) + varcharSlotCol.Offset());
        if (pos == nullptr) return nullptr;
        for (int32_t vcIdx : varcharColIndices) {
            auto col = aggRows->ColumnAt(vcIdx);
            auto nullByte = col.NullByte();
            auto nullMask = col.NullMask();
            if (RowContainer::IsNullAt(row, nullByte, nullMask)) {
                if (vcIdx == targetVarcharColIdx) return nullptr;
                pos += 1;
            } else {
                if (vcIdx == targetVarcharColIdx) return pos;
                pos += ComputeVarCharSerializedSize(pos);
            }
        }
        return nullptr;
    }

    /// Compute the total serialized byte size of a VARCHAR/CHAR/VARBINARY value
    /// from its serialized data pointer.
    /// Format: [uint8_t rowLenSize][rowLenSize bytes: length][length bytes: data]
    /// Null: [uint8_t: 0] → 1 byte.
    static ALWAYS_INLINE size_t ComputeVarCharSerializedSize(const char* data) {
        uint8_t rowLenSize = *reinterpret_cast<const uint8_t*>(data);
        if (rowLenSize == 0) return sizeof(uint8_t);
        size_t stringLen;
        switch (rowLenSize) {
            case 1: stringLen = *reinterpret_cast<const uint8_t*>(data + sizeof(uint8_t)); break;
            case 2: stringLen = *reinterpret_cast<const uint16_t*>(data + sizeof(uint8_t)); break;
            case 4: stringLen = *reinterpret_cast<const uint32_t*>(data + sizeof(uint8_t)); break;
            default: return sizeof(uint8_t);
        }
        return sizeof(uint8_t) + rowLenSize + stringLen;
    }

    // ========== DecodedVector-based templated helpers (zero runtime encoding branches) ==========

#ifdef __ARM_FEATURE_SVE
    /// SVE-optimized batch compare for primitive types with null handling.
    /// Uses ARM SVE instructions for vectorized comparison on aarch64.
    template <typename T, bool HasNull>
    int32_t SveBatchCompareDecoded(const DecodedVector& decoded, int32_t count, int32_t offset,
                                    uint32_t nullByte, uint8_t nullMask,
                                    int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
        const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
        const T* flatVals = decoded.FlatValues<T>();

        if (!rawNulls || !flatVals) {
            return BatchCompareDecoded<T, HasNull>(decoded, count, offset, nullByte, nullMask, indices, idxFrom, groups);
        }

        const int32_t n = count;
        svbool_t pgAll = svptrue_b64();

        for (int32_t i = idxFrom; i < n;) {
            svbool_t pg = svwhilelt_b64_s64((int64_t)i, (int64_t)n);
            int32_t activeCount = svcntp_b64(pgAll, pg);

            svuint64_t vIdx = svld1uw_u64(pg, (uint32_t*)&indices[i]);

            svuint64_t vWordIdx = svlsr_n_u64_x(pg, vIdx, 6);
            svuint64_t vNullByteOff = svlsl_n_u64_x(pg, vWordIdx, 3);
            svuint64_t vNullWords = svld1_gather_offset_u64(pg, vNullByteOff, (uint64_t)rawNulls);

            svuint64_t vPtrOffsets = svlsl_n_u64_x(pg, vIdx, 3);
            svuint64_t vRowPtrs = svld1_gather_offset_u64(pg, vPtrOffsets, (uint64_t)groups);

            svuint64_t vBitIdx = svand_n_u64_x(pg, vIdx, 63);
            svuint64_t vNullMasks = svlsl_u64_x(pg, svdup_n_u64(1), vBitIdx);
            svuint64_t vDecodedNotNull = svand_u64_x(pg, vNullWords, vNullMasks);
            svbool_t vDecodedIsNull = svcmpeq_n_u64(pg, vDecodedNotNull, 0);

            svuint64_t vNullAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)nullByte);
            svuint64_t vRowNullByte = svld1ub_gather_u64(pg, vNullAddr);
            svbool_t vRowIsNull = svcmpne_n_u64(pg, svand_n_u64_x(pg, vRowNullByte, (uint64_t)nullMask), 0);

            svbool_t vNullXor = sveor_b_z(pg, vRowIsNull, vDecodedIsNull);
            svbool_t vNullMatch = svnot_b_z(pg, vNullXor);
            svbool_t vBothNull = svand_b_z(pg, vRowIsNull, vDecodedIsNull);

            svbool_t vValueMatch;

            if constexpr (std::is_same_v<T, int8_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1sb_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1sb_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, int16_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1sh_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1sh_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1sw_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1sw_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, double>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svfloat64_t vRowValues = svld1_gather_f64(pg, vValueAddr);
                svfloat64_t vDecodedValues = svld1_f64(pg, flatVals + i);
                vValueMatch = svcmpeq_f64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svuint64_t vRowValues = svld1ub_gather_u64(pg, vValueAddr);
                svuint64_t vDecodedValues = svld1ub_u64(pg, flatVals + i);
                vValueMatch = svcmpeq_u64(pg, vRowValues, vDecodedValues);
            } else {
                return BatchCompareDecoded<T, HasNull>(decoded, count, offset, nullByte, nullMask, indices, idxFrom, groups);
            }

            svbool_t vMatch = svand_b_z(pg, vNullMatch, svorr_b_z(pg, vBothNull, vValueMatch));

            if (!svptest_any(pg, svnot_b_z(pg, vMatch))) {
                i += activeCount;
                continue;
            }

            svuint64_t vMatchFlag = svsel_u64(vMatch, svdup_n_u64(1), svdup_n_u64(0));
            uint64_t matchFlags[32];
            svst1_u64(pg, matchFlags, vMatchFlag);

            for (int32_t j = 0; j < activeCount; j++) {
                if (matchFlags[j] == 0) {
                    std::swap(indices[i + j], indices[idxFrom]);
                    idxFrom++;
                }
            }

            i += activeCount;
        }
        return idxFrom;
    }

    /// SVE-optimized batch compare for no-null case.
    template <typename T>
    int32_t SveBatchCompareNoNullDecoded(const DecodedVector& decoded, int32_t count, int32_t offset,
                                          int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
        const T* flatVals = decoded.FlatValues<T>();
        if (!flatVals) {
            return BatchCompareDecoded<T, false>(decoded, count, offset, 0, 0, indices, idxFrom, groups);
        }

        const int32_t n = count;
        svbool_t pgAll = svptrue_b64();

        for (int32_t i = idxFrom; i < n;) {
            svbool_t pg = svwhilelt_b64_s64((int64_t)i, (int64_t)n);
            int32_t activeCount = svcntp_b64(pgAll, pg);

            svuint64_t vIdx = svld1uw_u64(pg, (uint32_t*)&indices[i]);
            svuint64_t vPtrOffsets = svlsl_n_u64_x(pg, vIdx, 3);
            svuint64_t vRowPtrs = svld1_gather_offset_u64(pg, vPtrOffsets, (uint64_t)groups);

            svbool_t vValueMatch;

            if constexpr (std::is_same_v<T, int8_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1sb_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1sb_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, int16_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1sh_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1sh_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1sw_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1sw_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svint64_t vRowValues = svld1_gather_s64(pg, vValueAddr);
                svint64_t vDecodedValues = svld1_s64(pg, flatVals + i);
                vValueMatch = svcmpeq_s64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, double>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svfloat64_t vRowValues = svld1_gather_f64(pg, vValueAddr);
                svfloat64_t vDecodedValues = svld1_f64(pg, flatVals + i);
                vValueMatch = svcmpeq_f64(pg, vRowValues, vDecodedValues);
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
                svuint64_t vRowValues = svld1ub_gather_u64(pg, vValueAddr);
                svuint64_t vDecodedValues = svld1ub_u64(pg, flatVals + i);
                vValueMatch = svcmpeq_u64(pg, vRowValues, vDecodedValues);
            } else {
                return BatchCompareDecoded<T, false>(decoded, count, offset, 0, 0, indices, idxFrom, groups);
            }

            if (!svptest_any(pg, svnot_b_z(pg, vValueMatch))) {
                i += activeCount;
                continue;
            }

            svuint64_t vMatchFlag = svsel_u64(vValueMatch, svdup_n_u64(1), svdup_n_u64(0));
            uint64_t matchFlags[32];
            svst1_u64(pg, matchFlags, vMatchFlag);

            for (int32_t j = 0; j < activeCount; j++) {
                if (matchFlags[j] == 0) {
                    std::swap(indices[i + j], indices[idxFrom]);
                    idxFrom++;
                }
            }

            i += activeCount;
        }
        return idxFrom;
    }
#endif

    /// Batch compare using DecodedVector — flat values accessed directly, no encoding switch.
    template <typename T, bool HasNull, bool isDic = false>
    int32_t BatchCompareDecoded(const DecodedVector& decoded, int32_t count, int32_t offset,
                                uint32_t nullByte, uint8_t nullMask,
                                int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
#ifdef __ARM_FEATURE_SVE
        if constexpr (!isDic) {
            if constexpr (HasNull) {
                if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                              std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                              std::is_same_v<T, double> || std::is_same_v<T, uint8_t>) {
                    return SveBatchCompareDecoded<T, HasNull>(decoded, count, offset, nullByte, nullMask, indices, idxFrom, groups);
                }
            } else {
                if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                              std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                              std::is_same_v<T, double> || std::is_same_v<T, uint8_t>) {
                    return SveBatchCompareNoNullDecoded<T>(decoded, count, offset, indices, idxFrom, groups);
                }
            }
        }
#endif

        const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
        const T* flatVals = decoded.FlatValues<T>();
        const int32_t* ids = decoded.Ids();

        auto getValue = [&](int32_t idx) {
            if constexpr (isDic) { return flatVals[ids[idx]]; }
            else { return flatVals[idx]; }
        };

        if constexpr (HasNull) {
            for (int32_t i = idxFrom; i < count; ++i) {
                int32_t idx = indices[i];
                uint8_t* row = groups[idx];
                bool rowIsNull = RowContainer::IsNullAt(
                    reinterpret_cast<char*>(row), nullByte, nullMask);
                bool vecIsNull = BitUtil::IsBitSet(rawNulls, idx);
                if (rowIsNull != vecIsNull) {
                    std::swap(indices[i], indices[idxFrom]);
                    idxFrom++;
                    continue;
                }
                if (rowIsNull) continue;
                if (RowContainer::ReadValue<T>(reinterpret_cast<char*>(row), offset) != getValue(idx)) {
                    std::swap(indices[i], indices[idxFrom]);
                    idxFrom++;
                }
            }
        } else {
            for (int32_t i = idxFrom; i < count; ++i) {
                int32_t idx = indices[i];
                uint8_t* row = groups[idx];
                if (RowContainer::ReadValue<T>(reinterpret_cast<char*>(row), offset) != getValue(idx)) {
                    std::swap(indices[i], indices[idxFrom]);
                    idxFrom++;
                }
            }
        }
        return idxFrom;
    }

    /// Specialization for Constant layout — compare against single broadcast value.
    template <typename T, bool HasNull>
    int32_t BatchCompareDecodedConst(const DecodedVector& decoded, int32_t count, int32_t offset,
                                     uint32_t nullByte, uint8_t nullMask,
                                     int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
        T constVal = decoded.GetConstValue<T>();

#ifdef __ARM_FEATURE_SVE
        if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                      std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
                      std::is_same_v<T, double> || std::is_same_v<T, uint8_t>) {
            const uint64_t* rawNulls = nullptr;
            if constexpr (HasNull) {
                rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
            }

            const int32_t n = count;
            svbool_t pgAll = svptrue_b64();

            for (int32_t i = idxFrom; i < n;) {
                svbool_t pg = svwhilelt_b64_s64((int64_t)i, (int64_t)n);
                int32_t activeCount = svcntp_b64(pgAll, pg);

                svuint64_t vIdx = svld1uw_u64(pg, (uint32_t*)&indices[i]);
                svuint64_t vPtrOffsets = svlsl_n_u64_x(pg, vIdx, 3);
                svuint64_t vRowPtrs = svld1_gather_offset_u64(pg, vPtrOffsets, (uint64_t)groups);

                svbool_t vRowIsNull;
                svbool_t vDecodedIsNull;

                if constexpr (HasNull) {
                    svuint64_t vNullAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)nullByte);
                    svuint64_t vRowNullByte = svld1ub_gather_u64(pg, vNullAddr);
                    vRowIsNull = svcmpne_n_u64(pg, svand_n_u64_x(pg, vRowNullByte, (uint64_t)nullMask), 0);

                    svuint64_t vWordIdx = svlsr_n_u64_x(pg, vIdx, 6);
                    svuint64_t vNullByteOff = svlsl_n_u64_x(pg, vWordIdx, 3);
                    svuint64_t vNullWords = svld1_gather_offset_u64(pg, vNullByteOff, (uint64_t)rawNulls);
                    svuint64_t vBitIdx = svand_n_u64_x(pg, vIdx, 63);
                    svuint64_t vNullMasks = svlsl_u64_x(pg, svdup_n_u64(1), vBitIdx);
                    svuint64_t vDecodedNotNull = svand_u64_x(pg, vNullWords, vNullMasks);
                    vDecodedIsNull = svcmpeq_n_u64(pg, vDecodedNotNull, 0);

                    svbool_t vNullXor = sveor_b_z(pg, vRowIsNull, vDecodedIsNull);
                    svbool_t vNullMatch = svnot_b_z(pg, vNullXor);
                    svbool_t vBothNull = svand_b_z(pg, vRowIsNull, vDecodedIsNull);

                    svbool_t vValueMatch;
                    svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);

                    if constexpr (std::is_same_v<T, int8_t>) {
                        svint64_t vRowValues = svld1sb_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(static_cast<int64_t>(constVal));
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, int16_t>) {
                        svint64_t vRowValues = svld1sh_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(static_cast<int64_t>(constVal));
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, int32_t>) {
                        svint64_t vRowValues = svld1sw_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(static_cast<int64_t>(constVal));
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, int64_t>) {
                        svint64_t vRowValues = svld1_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(constVal);
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, double>) {
                        svfloat64_t vRowValues = svld1_gather_f64(pg, vValueAddr);
                        svfloat64_t vConst = svdup_n_f64(constVal);
                        vValueMatch = svcmpeq_f64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, uint8_t>) {
                        svuint64_t vRowValues = svld1ub_gather_u64(pg, vValueAddr);
                        svuint64_t vConst = svdup_n_u64(static_cast<uint64_t>(constVal));
                        vValueMatch = svcmpeq_u64(pg, vRowValues, vConst);
                    }

                    svbool_t vMatch = svand_b_z(pg, vNullMatch, svorr_b_z(pg, vBothNull, vValueMatch));

                    if (!svptest_any(pg, svnot_b_z(pg, vMatch))) {
                        i += activeCount;
                        continue;
                    }

                    svuint64_t vMatchFlag = svsel_u64(vMatch, svdup_n_u64(1), svdup_n_u64(0));
                    uint64_t matchFlags[32];
                    svst1_u64(pg, matchFlags, vMatchFlag);

                    for (int32_t j = 0; j < activeCount; j++) {
                        if (matchFlags[j] == 0) {
                            std::swap(indices[i + j], indices[idxFrom]);
                            idxFrom++;
                        }
                    }
                } else {
                    svbool_t vValueMatch;
                    svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);

                    if constexpr (std::is_same_v<T, int8_t>) {
                        svint64_t vRowValues = svld1sb_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(static_cast<int64_t>(constVal));
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, int16_t>) {
                        svint64_t vRowValues = svld1sh_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(static_cast<int64_t>(constVal));
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, int32_t>) {
                        svint64_t vRowValues = svld1sw_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(static_cast<int64_t>(constVal));
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, int64_t>) {
                        svint64_t vRowValues = svld1_gather_s64(pg, vValueAddr);
                        svint64_t vConst = svdup_n_s64(constVal);
                        vValueMatch = svcmpeq_s64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, double>) {
                        svfloat64_t vRowValues = svld1_gather_f64(pg, vValueAddr);
                        svfloat64_t vConst = svdup_n_f64(constVal);
                        vValueMatch = svcmpeq_f64(pg, vRowValues, vConst);
                    } else if constexpr (std::is_same_v<T, uint8_t>) {
                        svuint64_t vRowValues = svld1ub_gather_u64(pg, vValueAddr);
                        svuint64_t vConst = svdup_n_u64(static_cast<uint64_t>(constVal));
                        vValueMatch = svcmpeq_u64(pg, vRowValues, vConst);
                    }

                    if (!svptest_any(pg, svnot_b_z(pg, vValueMatch))) {
                        i += activeCount;
                        continue;
                    }

                    svuint64_t vMatchFlag = svsel_u64(vValueMatch, svdup_n_u64(1), svdup_n_u64(0));
                    uint64_t matchFlags[32];
                    svst1_u64(pg, matchFlags, vMatchFlag);

                    for (int32_t j = 0; j < activeCount; j++) {
                        if (matchFlags[j] == 0) {
                            std::swap(indices[i + j], indices[idxFrom]);
                            idxFrom++;
                        }
                    }
                }

                i += activeCount;
            }
            return idxFrom;
        }
#endif

        if constexpr (HasNull) {
            const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
            for (int32_t i = idxFrom; i < count; ++i) {
                int32_t idx = indices[i];
                uint8_t* row = groups[idx];
                bool rowIsNull = RowContainer::IsNullAt(
                    reinterpret_cast<char*>(row), nullByte, nullMask);
                bool vecIsNull = BitUtil::IsBitSet(rawNulls, idx);
                if (rowIsNull != vecIsNull) {
                    std::swap(indices[i], indices[idxFrom]);
                    idxFrom++;
                    continue;
                }
                if (rowIsNull) continue;
                if (RowContainer::ReadValue<T>(reinterpret_cast<char*>(row), offset) != constVal) {
                    std::swap(indices[i], indices[idxFrom]);
                    idxFrom++;
                }
            }
        } else {
            for (int32_t i = idxFrom; i < count; ++i) {
                int32_t idx = indices[i];
                uint8_t* row = groups[idx];
                if (RowContainer::ReadValue<T>(reinterpret_cast<char*>(row), offset) != constVal) {
                    std::swap(indices[i], indices[idxFrom]);
                    idxFrom++;
                }
            }
        }
        return idxFrom;
    }

    /// Batch compare for VARCHAR using DecodedVector — encoding resolved at decode time.
    template <bool HasNull>
    int32_t BatchCompareVarcharDecoded(const DecodedVector& decoded, int32_t colIdx, int32_t count, int32_t offset,
                                       uint32_t nullByte, uint8_t nullMask,
                                       int32_t* indices, int32_t& idxFrom, uint8_t* const* groups)
    {
        auto layout = decoded.GetLayout();

        auto getVarcharData = [&](uint8_t* row, int32_t groupIdx) -> const uint8_t* {
            if (!mergedVarcharCache_.empty()) {
                int32_t vcPos = colToVarcharPos_[colIdx];
                return reinterpret_cast<const uint8_t*>(
                    mergedVarcharCache_[groupIdx * mergedVarcharCacheCount_ + vcPos]);
            }
            return reinterpret_cast<const uint8_t*>(
                *reinterpret_cast<char**>(reinterpret_cast<char*>(row) + offset));
        };

        if (layout == DVecLayout::Constant) {
            std::string_view constVal = static_cast<ConstVector<std::string_view>*>(decoded.Base())->GetConstValue();
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = indices[i];
                    uint8_t* row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask);
                    bool vecIsNull = BitUtil::IsBitSet(rawNulls, idx);
                    if (rowIsNull != vecIsNull) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    if (rowIsNull) continue;
                    const uint8_t* rowData = getVarcharData(row, idx);
                    if (rowData == nullptr || rowData[0] == 0 || !CompareVarcharFromRow(rowData, constVal)) {
                        std::swap(indices[i], indices[idxFrom]); idxFrom++;
                    }
                }
            } else {
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = indices[i];
                    uint8_t* row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask);
                    if (rowIsNull) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    const uint8_t* rowData = getVarcharData(row, idx);
                    if (rowData == nullptr || rowData[0] == 0 || !CompareVarcharFromRow(rowData, constVal)) {
                        std::swap(indices[i], indices[idxFrom]); idxFrom++;
                    }
                }
            }
        } else if (layout == DVecLayout::Dictionary) {
            auto* dicVec = static_cast<Vector<DictionaryContainer<std::string_view>>*>(decoded.Base());
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = indices[i];
                    uint8_t* row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask);
                    bool vecIsNull = BitUtil::IsBitSet(rawNulls, idx);
                    if (rowIsNull != vecIsNull) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    if (rowIsNull) continue;
                    const uint8_t* rowData = getVarcharData(row, idx);
                    if (rowData == nullptr || rowData[0] == 0) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    std::string_view val = dicVec->GetValue(idx);
                    if (!CompareVarcharFromRow(rowData, val)) { std::swap(indices[i], indices[idxFrom]); idxFrom++; }
                }
            } else {
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = indices[i];
                    uint8_t* row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask);
                    if (rowIsNull) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    const uint8_t* rowData = getVarcharData(row, idx);
                    if (rowData == nullptr || rowData[0] == 0) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    std::string_view val = dicVec->GetValue(idx);
                    if (!CompareVarcharFromRow(rowData, val)) { std::swap(indices[i], indices[idxFrom]); idxFrom++; }
                }
            }
        } else {
            auto* strVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(decoded.Base());
            int32_t* strOffsets = unsafe::UnsafeStringVector::GetOffsets(strVec);
            char* strData = unsafe::UnsafeStringVector::GetValues(strVec);
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = indices[i];
                    uint8_t* row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask);
                    bool vecIsNull = BitUtil::IsBitSet(rawNulls, idx);
                    if (rowIsNull != vecIsNull) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    if (rowIsNull) continue;
                    const uint8_t* rowData = getVarcharData(row, idx);
                    if (rowData == nullptr || rowData[0] == 0) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    auto len = strOffsets[idx + 1] - strOffsets[idx];
                    std::string_view val(strData + strOffsets[idx], len);
                    if (!CompareVarcharFromRow(rowData, val)) { std::swap(indices[i], indices[idxFrom]); idxFrom++; }
                }
            } else {
                for (int32_t i = idxFrom; i < count; ++i) {
                    int32_t idx = indices[i];
                    uint8_t* row = groups[idx];
                    bool rowIsNull = RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask);
                    if (rowIsNull) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    const uint8_t* rowData = getVarcharData(row, idx);
                    if (rowData == nullptr || rowData[0] == 0) { std::swap(indices[i], indices[idxFrom]); idxFrom++; continue; }
                    auto len = strOffsets[idx + 1] - strOffsets[idx];
                    std::string_view val(strData + strOffsets[idx], len);
                    if (!CompareVarcharFromRow(rowData, val)) { std::swap(indices[i], indices[idxFrom]); idxFrom++; }
                }
            }
        }
        return idxFrom;
    }

    /// Batch store using DecodedVector — flat values accessed directly, no encoding switch.
    template <typename T, bool HasNull, bool isDic = false>
    void BatchStoreDecoded(const DecodedVector& decoded, int32_t offset, uint32_t nullByte, uint8_t nullMask,
                           uint8_t** rows, uint32_t* rowIndices, int32_t rowCount)
    {
        const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
        const T* flatVals = decoded.FlatValues<T>();
        const int32_t* ids = decoded.Ids();

        auto getValue = [&](int32_t idx) {
            if constexpr (isDic) { return flatVals[ids[idx]]; }
            else { return flatVals[idx]; }
        };

        if constexpr (HasNull) {
            for (int32_t i = 0; i < rowCount; ++i) {
                char* row = reinterpret_cast<char*>(rows[i]);
                auto rowIdx = rowIndices[i];
                if (BitUtil::IsBitSet(rawNulls, rowIdx)) {
                    RowContainer::SetNullAt(row, nullByte, nullMask);
                } else {
                    RowContainer::ClearNullAt(row, nullByte, nullMask);
                    RowContainer::StoreValue(row, offset, getValue(rowIdx));
                }
            }
        } else {
            for (int32_t i = 0; i < rowCount; ++i) {
                char* row = reinterpret_cast<char*>(rows[i]);
                auto rowIdx = rowIndices[i];
                RowContainer::ClearNullAt(row, nullByte, nullMask);
                RowContainer::StoreValue(row, offset, getValue(rowIdx));
            }
        }
    }

    /// Specialization for Constant layout — store single broadcast value.
    template <typename T, bool HasNull>
    void BatchStoreDecodedConst(const DecodedVector& decoded, int32_t offset, uint32_t nullByte, uint8_t nullMask,
                                uint8_t** rows, uint32_t* rowIndices, int32_t rowCount)
    {
        T constVal = decoded.GetConstValue<T>();
        const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());

        if constexpr (HasNull) {
            for (int32_t i = 0; i < rowCount; ++i) {
                char* row = reinterpret_cast<char*>(rows[i]);
                auto rowIdx = rowIndices[i];
                if (BitUtil::IsBitSet(rawNulls, rowIdx)) {
                    RowContainer::SetNullAt(row, nullByte, nullMask);
                } else {
                    RowContainer::ClearNullAt(row, nullByte, nullMask);
                    RowContainer::StoreValue(row, offset, constVal);
                }
            }
        } else {
            for (int32_t i = 0; i < rowCount; ++i) {
                char* row = reinterpret_cast<char*>(rows[i]);
                RowContainer::ClearNullAt(row, nullByte, nullMask);
                RowContainer::StoreValue(row, offset, constVal);
            }
        }
    }

    /// Batch store for VARCHAR using DecodedVector.
    template <bool HasNull>
    void BatchStoreVarcharDecoded(const DecodedVector& decoded, int32_t colIdx, int32_t offset,
                                  uint32_t nullByte, uint8_t nullMask,
                                  uint8_t** rows, uint32_t* rowIndices, int32_t rowCount)
    {
        auto& curFunc = serializers[colIdx];
        auto layout = decoded.GetLayout();

        if (layout == DVecLayout::Constant) {
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t i = 0; i < rowCount; ++i) {
                    char* row = reinterpret_cast<char*>(rows[i]);
                    auto rowIdx = rowIndices[i];
                    if (BitUtil::IsBitSet(rawNulls, rowIdx)) {
                        RowContainer::SetNullAt(row, nullByte, nullMask);
                    } else {
                        RowContainer::ClearNullAt(row, nullByte, nullMask);
                        type::StringRef key; key.data = nullptr; key.size = 0;
                        curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                        *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                    }
                }
            } else {
                for (int32_t i = 0; i < rowCount; ++i) {
                    char* row = reinterpret_cast<char*>(rows[i]);
                    auto rowIdx = rowIndices[i];
                    RowContainer::ClearNullAt(row, nullByte, nullMask);
                    type::StringRef key; key.data = nullptr; key.size = 0;
                    curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                    *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                }
            }
        } else if (layout == DVecLayout::Dictionary) {
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t i = 0; i < rowCount; ++i) {
                    char* row = reinterpret_cast<char*>(rows[i]);
                    auto rowIdx = rowIndices[i];
                    if (BitUtil::IsBitSet(rawNulls, rowIdx)) {
                        RowContainer::SetNullAt(row, nullByte, nullMask);
                    } else {
                        RowContainer::ClearNullAt(row, nullByte, nullMask);
                        type::StringRef key; key.data = nullptr; key.size = 0;
                        curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                        *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                    }
                }
            } else {
                for (int32_t i = 0; i < rowCount; ++i) {
                    char* row = reinterpret_cast<char*>(rows[i]);
                    auto rowIdx = rowIndices[i];
                    RowContainer::ClearNullAt(row, nullByte, nullMask);
                    type::StringRef key; key.data = nullptr; key.size = 0;
                    curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                    *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                }
            }
        } else {
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t i = 0; i < rowCount; ++i) {
                    char* row = reinterpret_cast<char*>(rows[i]);
                    auto rowIdx = rowIndices[i];
                    if (BitUtil::IsBitSet(rawNulls, rowIdx)) {
                        RowContainer::SetNullAt(row, nullByte, nullMask);
                    } else {
                        RowContainer::ClearNullAt(row, nullByte, nullMask);
                        type::StringRef key; key.data = nullptr; key.size = 0;
                        curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                        *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                    }
                }
            } else {
                for (int32_t i = 0; i < rowCount; ++i) {
                    char* row = reinterpret_cast<char*>(rows[i]);
                    auto rowIdx = rowIndices[i];
                    RowContainer::ClearNullAt(row, nullByte, nullMask);
                    type::StringRef key; key.data = nullptr; key.size = 0;
                    curFunc(decoded.Base(), rowIdx, table->Pool(), key);
                    *reinterpret_cast<char**>(row + offset) = const_cast<char*>(key.data);
                }
            }
        }
    }

    /// Batch store for merged VARCHAR columns - stores all VARCHAR data in one contiguous block.
    /// Memory layout: [col0_data][col1_data]...[colN_data]
    /// Called once per batch before per-column stores, handles all VARCHAR columns together.
    void BatchStoreMergedVarcharColumns(int32_t groupColNum,
                                        uint8_t** rows, uint32_t* rowIndices, int32_t rowCount)
    {
        auto varcharSlotCol = aggRows->ColumnAt(varcharSlotColIdx);
        auto varcharSlotOffset = varcharSlotCol.Offset();
        auto varcharSlotNullByte = varcharSlotCol.NullByte();
        auto varcharSlotNullMask = varcharSlotCol.NullMask();

        for (int32_t i = 0; i < rowCount; ++i) {
            char* row = reinterpret_cast<char*>(rows[i]);
            auto rowIdx = rowIndices[i];

            // First pass: calculate total size needed for all VARCHAR columns
            int32_t totalSize = 0;
            for (int32_t vcIdx : varcharColIndices) {
                auto col = aggRows->ColumnAt(vcIdx);
                auto nullByte = col.NullByte();
                auto nullMask = col.NullMask();
                if (decodedCols[vcIdx].HasNull() && decodedCols[vcIdx].IsNull(rowIdx)) {
                    totalSize += 1; // null marker: 1 byte
                } else {
                    totalSize += ComputeVarcharSerializedSize(decodedCols[vcIdx], rowIdx);
                }
            }

            // Allocate contiguous block
            char* blockStart = nullptr;
            if (totalSize > 0) {
                const uint8_t* tmp = nullptr;
                table->Pool().AllocateContinue(totalSize, tmp);
                blockStart = const_cast<char*>(reinterpret_cast<const char*>(tmp));
            }

            // Second pass: serialize each VARCHAR column directly into the block
            char* writePos = blockStart;
            for (int32_t vcIdx : varcharColIndices) {
                auto col = aggRows->ColumnAt(vcIdx);
                auto nullByte = col.NullByte();
                auto nullMask = col.NullMask();

                if (decodedCols[vcIdx].HasNull() && decodedCols[vcIdx].IsNull(rowIdx)) {
                    RowContainer::SetNullAt(row, nullByte, nullMask);
                    if (writePos) {
                        *writePos = 0; // null marker
                        writePos += 1;
                    }
                } else {
                    RowContainer::ClearNullAt(row, nullByte, nullMask);
                    writePos += SerializeVarcharToBuffer(writePos, decodedCols[vcIdx], rowIdx);
                }
            }

            // Store the block pointer in the first VARCHAR slot
            *reinterpret_cast<char**>(row + varcharSlotOffset) = blockStart;
        }
    }

    // ========== End of DecodedVector helpers ==========

    /// Use RowContainer::ListRows to iterate through rows for output.
    /// Returns row pointers where each row has:
    ///   [key data at column offsets] [null bits] [AggState at aggStateOffset]
    template <bool withHashVal = false, class Func, class NullFunc>
    void Extract(int32_t rowsNum, OutputState &outputState, Func func, NullFunc nullFunc)
    {
        rowPtrs.resize(rowsNum);
        int32_t numExtracted = aggRows->ListRows(&rowContainerIter, rowsNum, rowPtrs.data());

        for (int32_t idx = 0; idx < numExtracted; ++idx) {
            auto* row = rowPtrs[idx];
            // Build a StringRef key for complex type columns from the row
            // For the new RowContainer layout, keys are stored in-place
            // We pass the row pointer directly so ParseKeyToCols can read from it
            func(reinterpret_cast<uint8_t*>(row), reinterpret_cast<uint8_t*>(row), idx);
        }
        outputState.hasBeenOutputNum += numExtracted;
    }

    /// Parse key columns from a RowContainer row into output vectors.
    /// For fixed-width types, read directly from row offsets.
    /// For complex types, read from the serialized StringRef stored in the row.
    /// For merged VARCHAR columns, read from the contiguous block.
    void ParseKeyToCols(uint8_t* rowPtr, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int32_t rowIdx)
    {
        auto* row = reinterpret_cast<char*>(rowPtr);
        // For merged VARCHAR: get the block pointer and parse all VARCHAR columns from it
        const char* mergedVarcharPos = nullptr;
        if (varcharColIndices.size() > 1) {
            auto varcharSlotCol = aggRows->ColumnAt(varcharSlotColIdx);
            auto varcharSlotOffset = varcharSlotCol.Offset();
            mergedVarcharPos = *reinterpret_cast<char**>(row + varcharSlotOffset);
        }
        for (int32_t i = 0; i < groupColNum; ++i) {
            auto col = aggRows->ColumnAt(i);
            auto offset = col.Offset();
            auto* outVector = groupOutputVectors[i];
            auto typeId = outVector->GetTypeId();
            auto nullByte = col.NullByte();
            auto nullMask = col.NullMask();
            if (RowContainer::IsNullAt(row, nullByte, nullMask)) {
                vec::VectorHelper::SetNull(outVector, rowIdx);
                if (varcharColIndices.size() > 1 && mergedVarcharPos != nullptr) {
                    if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR ||
                        typeId == type::OMNI_VARBINARY) {
                        mergedVarcharPos += 1;
                    }
                }
                continue;
            }

            switch (typeId) {
                case type::OMNI_BYTE:
                    static_cast<Vector<int8_t>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<int8_t>(row, offset));
                    break;
                case type::OMNI_SHORT:
                    static_cast<Vector<int16_t>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<int16_t>(row, offset));
                    break;
                case type::OMNI_INT:
                case type::OMNI_DATE32:
                case type::OMNI_TIME32:
                    static_cast<Vector<int32_t>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<int32_t>(row, offset));
                    break;
                case type::OMNI_LONG:
                case type::OMNI_TIMESTAMP:
                case type::OMNI_DECIMAL64:
                case type::OMNI_DATE64:
                case type::OMNI_TIME64:
                    static_cast<Vector<int64_t>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<int64_t>(row, offset));
                    break;
                case type::OMNI_DOUBLE:
                    static_cast<Vector<double>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<double>(row, offset));
                    break;
                case type::OMNI_FLOAT:
                    static_cast<Vector<float>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<float>(row, offset));
                    break;
                case type::OMNI_BOOLEAN:
                    static_cast<Vector<bool>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<int8_t>(row, offset) != 0);
                    break;
                case type::OMNI_DECIMAL128:
                    static_cast<Vector<Decimal128>*>(outVector)->SetValue(rowIdx,
                        RowContainer::ReadValue<Decimal128>(row, offset));
                    break;
                case type::OMNI_VARCHAR:
                case type::OMNI_CHAR:
                case type::OMNI_VARBINARY: {
                    if (varcharColIndices.size() > 1 && mergedVarcharPos != nullptr) {
                        // Read from merged block
                        const char* pos = mergedVarcharPos;
                        pos = deserializers[i](outVector, rowIdx, pos);
                        mergedVarcharPos = pos; // Advance cursor for next VARCHAR
                    } else {
                        // Single VARCHAR: read pointer directly from row
                        char* dataPtr = *reinterpret_cast<char**>(row + offset);
                        const char* pos = dataPtr;
                        deserializers[i](outVector, rowIdx, pos);
                    }
                    break;
                }
                default: {
                    char* dataPtr = *reinterpret_cast<char**>(row + offset);
                    auto deserializeFunc = deserializers[i];
                    const char* pos = dataPtr;
                    deserializeFunc(outVector, rowIdx, pos);
                    break;
                }
            }
        }
    }

    void ParseNull(const char *key, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int rowIdx)
    {
        for (int32_t i = 0; i < groupColNum; ++i) {
            vec::VectorHelper::SetNull(groupOutputVectors[i], rowIdx);
        }
    }

    /// SpillExtract traverses the hash table for spill operations.
    /// It uses the hash table Visitor and reconstructs serialized key data
    /// for writing to disk. The key is reconstructed from the RowContainer
    /// row by re-serializing each key column.
    template <class Func, class NullFunc>
    void SpillExtract(int32_t rowsNum, OutputState &outputState, Func func, NullFunc nullFunc)
    {
        auto tblVisitor = [&] {
            if (outputState.rowBegin) {
                return table->GetResultVisitor(
                    outputState.rowBegin, static_cast<uint16_t>(outputState.rowOffset));
            }
            return table->GetResultVisitor();
        }();
        uint32_t idx = 0;
        while (idx < rowsNum && !tblVisitor.Finished()) {
            auto* row = GetRowPtr(tblVisitor.CurVal().buf);
            // Reconstruct serialized key data from RowContainer row
            // For fixed-width types, re-serialize each column into a contiguous buffer
            // For complex types, the serialized data pointer is stored in the row
            // For merged VARCHAR columns, read from the contiguous block
            type::StringRef key;

            // For merged VARCHAR: get the block pointer
            const char* mergedVarcharPos = nullptr;
            if (varcharColIndices.size() > 1) {
                auto varcharSlotCol = aggRows->ColumnAt(varcharSlotColIdx);
                auto varcharSlotOffset = varcharSlotCol.Offset();
                mergedVarcharPos = *reinterpret_cast<char**>(reinterpret_cast<char*>(row) + varcharSlotOffset);
            }

            for (int32_t colIdx = 0; colIdx < static_cast<int32_t>(serializers.size()); ++colIdx) {
                auto col = aggRows->ColumnAt(colIdx);
                auto offset = col.Offset();
                auto nullByte = col.NullByte();
                auto nullMask = col.NullMask();
                auto colTypeId = keyTypeIds[colIdx];
                bool isMergedVarcharNonSlot = (varcharColIndices.size() > 1 &&
                    (colTypeId == type::OMNI_VARCHAR || colTypeId == type::OMNI_CHAR ||
                     colTypeId == type::OMNI_VARBINARY) &&
                    colIdx != varcharSlotColIdx);

                if (keyTypeSizes[colIdx] == 0 && !isMergedVarcharNonSlot) {
                    continue;
                }

                if (RowContainer::IsNullAt(reinterpret_cast<char*>(row), nullByte, nullMask)) {
                    auto* pos = table->Pool().AllocateContinue(sizeof(uint8_t), reinterpret_cast<const uint8_t*&>(key.data));
                    *pos = 0;
                    key.size += sizeof(uint8_t);
                    if (varcharColIndices.size() > 1 && mergedVarcharPos != nullptr &&
                        (colTypeId == type::OMNI_VARCHAR || colTypeId == type::OMNI_CHAR || colTypeId == type::OMNI_VARBINARY)) {
                        mergedVarcharPos += 1;
                    }
                    continue;
                }

                if (isVariableLenType[colIdx]) {
                    if (colTypeId == type::OMNI_VARCHAR || colTypeId == type::OMNI_CHAR ||
                        colTypeId == type::OMNI_VARBINARY) {
                        const char* dataPtr = nullptr;
                        size_t dataSize = 0;

                        if (varcharColIndices.size() > 1 && mergedVarcharPos != nullptr) {
                            dataPtr = mergedVarcharPos;
                            dataSize = ComputeVarCharSerializedSize(dataPtr);
                            mergedVarcharPos += dataSize;
                        } else {
                            dataPtr = *reinterpret_cast<char**>(reinterpret_cast<char*>(row) + offset);
                            if (dataPtr != nullptr) {
                                dataSize = ComputeVarCharSerializedSize(dataPtr);
                            }
                        }
                        if (dataPtr != nullptr && dataSize > 0) {
                            auto* dest = table->Pool().AllocateContinue(dataSize, reinterpret_cast<const uint8_t*&>(key.data));
                            memcpy(dest, dataPtr, dataSize);
                            key.size += dataSize;
                        }
                    } else {
                        char* dataPtr = *reinterpret_cast<char**>(reinterpret_cast<char*>(row) + offset);
                        size_t dataSize = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(row) + offset + sizeof(char*));
                        if (dataPtr != nullptr && dataSize > 0) {
                            auto* dest = table->Pool().AllocateContinue(dataSize, reinterpret_cast<const uint8_t*&>(key.data));
                            memcpy(dest, dataPtr, dataSize);
                            key.size += dataSize;
                        }
                    }
                } else {
                    int32_t colSize = keyTypeSizes[colIdx];
                    auto resSize = sizeof(uint8_t) + colSize;
                    auto* pos = table->Pool().AllocateContinue(resSize, reinterpret_cast<const uint8_t*&>(key.data));
                    *pos = static_cast<uint8_t>(colSize);
                    memcpy(pos + sizeof(uint8_t), reinterpret_cast<char*>(row) + offset, colSize);
                    key.size += resSize;
                }
            }

            // Ensure space for the key size trailer
            table->Pool().AllocateContinue(sizeof(size_t), reinterpret_cast<const uint8_t*&>(key.data));

            // Store key size trailer
            *reinterpret_cast<size_t*>(const_cast<char*>(key.data) + key.size) = key.size;

            // Adjust value pointer to AggState offset
            uint8_t* valuePtr = reinterpret_cast<uint8_t*>(row) + aggRows->AggStateOffset();

            func(key, tblVisitor.CurKey(), valuePtr, idx);
            tblVisitor.Next();
            idx++;
        }
        tblVisitor.SavePos([&](auto ptr, auto tagPos) {
            outputState.rowBegin = reinterpret_cast<char*>(ptr);
            outputState.rowOffset = tagPos;
            outputState.hasBeenOutputNum += idx;
        });
    }

    template <DataTypeId id>
    void Hash(BaseVector *vector, int32_t rowsNum, std::vector<int64_t> &workingHashVals, bool isMix)
    {
        bool isDictEncoded = (vector->GetEncoding() == vec::OMNI_DICTIONARY);
        bool isConstEncoded = (vector->GetEncoding() == vec::OMNI_ENCODING_CONST);
        if (isDictEncoded) {
            DoHash<id, true>(vector, rowsNum, workingHashVals, isMix);
        } else if (isConstEncoded) {
            DoHash<id, false, true>(vector, rowsNum, workingHashVals, isMix);
        } else {
            DoHash<id>(vector, rowsNum, workingHashVals, isMix);
        }
    }

    static inline uint64_t FastHashMix(uint64_t a, uint64_t b)
    {
        return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    }

    /// Hash using DecodedVector — dispatch on layout to eliminate runtime encoding branches.
    template<DataTypeId id, bool HasNull, bool isMix>
    void DoHashWithDecodeFlat(const DecodedVector& decoded, int32_t rowsNum)
    {
        using RealVector = typename NativeAndVectorType<id>::vector;
        using Type = typename NativeAndVectorType<id>::type;
        GroupbyHashCalculator<Type> calculator {};
        auto* realVector = static_cast<RealVector*>(decoded.Base());
        if constexpr (HasNull) {
            const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
            for (int32_t row = 0; row < rowsNum; row++) {
                int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(realVector->GetValue(row));
                if constexpr (isMix) {
                    workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal);
                } else {
                    workingHashVals[row] = hashVal;
                }
            }
        } else {
            for (int32_t row = 0; row < rowsNum; row++) {
                int64_t hashVal = calculator(realVector->GetValue(row));
                if constexpr (isMix) {
                    workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal);
                } else {
                    workingHashVals[row] = hashVal;
                }
            }
        }
    }

    template<DataTypeId id, bool HasNull, bool isMix>
    void DoHashWithDecodeDict(const DecodedVector& decoded, int32_t rowsNum)
    {
        using Type = typename NativeAndVectorType<id>::type;
        GroupbyHashCalculator<Type> calculator {};
        if constexpr (std::is_same_v<Type, std::string_view>) {
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t row = 0; row < rowsNum; row++) {
                    int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(decoded.GetValue<Type>(row));
                    if constexpr (isMix) { workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal); }
                    else { workingHashVals[row] = hashVal; }
                }
            } else {
                for (int32_t row = 0; row < rowsNum; row++) {
                    int64_t hashVal = calculator(decoded.GetValue<Type>(row));
                    if constexpr (isMix) { workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal); }
                    else { workingHashVals[row] = hashVal; }
                }
            }
        } else {
            const Type* dictVals = decoded.FlatValues<Type>();
            const int32_t* ids = decoded.Ids();
            if constexpr (HasNull) {
                const uint64_t* rawNulls = reinterpret_cast<const uint64_t*>(decoded.Nulls());
                for (int32_t row = 0; row < rowsNum; row++) {
                    int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(dictVals[ids[row]]);
                    if constexpr (isMix) { workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal); }
                    else { workingHashVals[row] = hashVal; }
                }
            } else {
                for (int32_t row = 0; row < rowsNum; row++) {
                    int64_t hashVal = calculator(dictVals[ids[row]]);
                    if constexpr (isMix) { workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal); }
                    else { workingHashVals[row] = hashVal; }
                }
            }
        }
    }

    template<DataTypeId id, bool HasNull, bool isMix>
    void DoHashWithDecodeConst(const DecodedVector& decoded, int32_t rowsNum)
    {
        using Type = typename NativeAndVectorType<id>::type;
        GroupbyHashCalculator<Type> calculator {};
        auto hashVal = decoded.IsNull(0) ? kNullHash : calculator(static_cast<ConstVector<Type>*>(decoded.Base())->GetConstValue());
        for (int32_t row = 0; row < rowsNum; row++) {
            if constexpr (isMix) {
                workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal);
            } else {
                workingHashVals[row] = hashVal;
            }
        }
    }

    template<DataTypeId id>
    void DoHashWithDecode(const DecodedVector& decoded, int32_t rowsNum, bool isMix)
    {
        auto layout = decoded.GetLayout();
        bool hasNull = decoded.HasNull();
        if (layout == DVecLayout::Constant) {
            if (hasNull && isMix) { DoHashWithDecodeConst<id, true, true>(decoded, rowsNum); }
            else if (hasNull && !isMix) { DoHashWithDecodeConst<id, true, false>(decoded, rowsNum); }
            else if (isMix) { DoHashWithDecodeConst<id, false, true>(decoded, rowsNum); }
            else { DoHashWithDecodeConst<id, false, false>(decoded, rowsNum); }
        } else if (layout == DVecLayout::Dictionary) {
            if (hasNull && isMix) { DoHashWithDecodeDict<id, true, true>(decoded, rowsNum); }
            else if (hasNull && !isMix) { DoHashWithDecodeDict<id, true, false>(decoded, rowsNum); }
            else if (isMix) { DoHashWithDecodeDict<id, false, true>(decoded, rowsNum); }
            else { DoHashWithDecodeDict<id, false, false>(decoded, rowsNum); }
        } else {
            if (hasNull && isMix) { DoHashWithDecodeFlat<id, true, true>(decoded, rowsNum); }
            else if (hasNull && !isMix) { DoHashWithDecodeFlat<id, true, false>(decoded, rowsNum); }
            else if (isMix) { DoHashWithDecodeFlat<id, false, true>(decoded, rowsNum); }
            else { DoHashWithDecodeFlat<id, false, false>(decoded, rowsNum); }
        }
    }

    template<DataTypeId id, bool isDic = false, bool isConst = false>
    void DoHash(BaseVector *vector, int32_t rowsNum, std::vector<int64_t> &workingHashVals, bool isMix)
    {
        using RealVector = typename NativeAndVectorType<id>::vector;
        using Type = typename NativeAndVectorType<id>::type;
        GroupbyHashCalculator<Type> calculator {};
        if constexpr (isConst) {
            auto hashVal = vector->IsNull(0) ? kNullHash : calculator(static_cast<ConstVector<Type> *>(vector)->GetConstValue());
            if (isMix) {
                for (int32_t row = 0; row < rowsNum; row++) {
                    workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal);
                }
            } else {
                for (int32_t row = 0; row < rowsNum; row++) {
                    workingHashVals[row] = hashVal;
                }
            }
        } else if constexpr (isDic) {
            auto* dicVec = static_cast<Vector<DictionaryContainer<Type>> *>(vector);
            if (vector->HasNull()) {
                auto* rawNulls = reinterpret_cast<const uint64_t*>(unsafe::UnsafeBaseVector::GetNulls(vector));
                if (isMix) {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(dicVec->GetValue(row));
                        workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal);
                    }
                } else {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(dicVec->GetValue(row));
                        workingHashVals[row] = hashVal;
                    }
                }
            } else {
                if (isMix) {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        workingHashVals[row] = FastHashMix(workingHashVals[row], calculator(dicVec->GetValue(row)));
                    }
                } else {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        workingHashVals[row] = calculator(dicVec->GetValue(row));
                    }
                }
            }
        } else {
            auto realVector = static_cast<RealVector *>(vector);
            if (vector->HasNull()) {
                auto* rawNulls = reinterpret_cast<const uint64_t*>(unsafe::UnsafeBaseVector::GetNulls(vector));
                if (isMix) {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(realVector->GetValue(row));
                        workingHashVals[row] = FastHashMix(workingHashVals[row], hashVal);
                    }
                } else {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        int64_t hashVal = BitUtil::IsBitSet(rawNulls, row) ? kNullHash : calculator(realVector->GetValue(row));
                        workingHashVals[row] = hashVal;
                    }
                }
            } else {
                if (isMix) {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        workingHashVals[row] = FastHashMix(workingHashVals[row], calculator(realVector->GetValue(row)));
                    }
                } else {
                    for (int32_t row = 0; row < rowsNum; row++) {
                        workingHashVals[row] = calculator(realVector->GetValue(row));
                    }
                }
            }
        }
    }

    void DoRowHash(BaseVector *vector, int32_t rowsNum, std::vector<int64_t> &workingHashVals)
    {
        auto rowVector = dynamic_cast<RowVector *>(vector);
        int32_t childCount = rowVector->ChildSize();
        for (int32_t i = 0; i < childCount; i++) {
            auto &childVec = rowVector->ChildAt(i);
            auto childTypeId = childVec->GetTypeId();
            if (childTypeId == type::OMNI_ARRAY) {
                DecodedVector decoded;
                decoded.Decode(childVec.get(), rowsNum);
                DoArrayHashWithDecode(decoded, rowsNum, i > 0);
            } else if (childTypeId == type::OMNI_ROW) {
                DoRowHash(childVec.get(), rowsNum, workingHashVals);
            } else {
                DYNAMIC_TYPE_DISPATCH(Hash, childTypeId, childVec.get(), rowsNum, workingHashVals, i > 0);
            }
        }
    }

    /// Hash array elements using DecodedVector layout — eliminates runtime encoding branches.
    void DoArrayHashWithDecode(const DecodedVector& decoded, int32_t rowsNum, bool isMix)
    {
        auto layout = decoded.GetLayout();
        if (layout == DVecLayout::Flat) {
            auto* arrayVector = static_cast<ArrayVector*>(decoded.Base());
            DoArrayHashElementDispatch(arrayVector, rowsNum, isMix);
        } else if (layout == DVecLayout::Dictionary) {
            auto* dictVec = static_cast<Vector<DictionaryContainer<ArrayVector*>>*>(decoded.Base());
            DoArrayHashDictDispatch(dictVec, rowsNum, isMix);
        } else {
            auto* constVec = static_cast<ConstVector<ArrayVector*>*>(decoded.Base());
            DoArrayHashConstDispatch(constVec, rowsNum, isMix);
        }
    }

    template <DataTypeId id>
    void DoArrayHashElementFlat(ArrayVector* arrayVector, int32_t rowsNum, bool isMix)
    {
        using RealVector = typename NativeAndVectorType<id>::vector;
        using Type = typename NativeAndVectorType<id>::type;
        auto* elementVec = arrayVector->GetElementVector().get();
        auto* realVector = static_cast<RealVector*>(elementVec);
        GroupbyHashCalculator<Type> calculator {};
        bool hasNull = elementVec->HasNull();
        for (int32_t i = 0; i < rowsNum; i++) {
            int64_t start = arrayVector->GetOffset(i);
            int64_t end = arrayVector->GetOffset(i + 1);
            int64_t finalHash = 0;
            bool first = true;
            if (hasNull) {
                for (int64_t row = start; row < end; row++) {
                    auto hash = elementVec->IsNull(row) ? kNullHash : calculator(realVector->GetValue(row));
                    if (first) { finalHash = hash; first = false; }
                    else { finalHash = FastHashMix(finalHash, hash); }
                }
            } else {
                for (int64_t row = start; row < end; row++) {
                    auto hash = calculator(realVector->GetValue(row));
                    if (first) { finalHash = hash; first = false; }
                    else { finalHash = FastHashMix(finalHash, hash); }
                }
            }
            if (isMix) {
                workingHashVals[i] = FastHashMix(workingHashVals[i], finalHash);
            } else {
                workingHashVals[i] = finalHash;
            }
        }
    }

    /// Hash a single value from a decoded vector (basic types only).
    template <DataTypeId id>
    static int64_t HashOneDecodedValue(const DecodedVector& decoded, int32_t rowIdx)
    {
        using T = typename NativeType<id>::type;
        if (decoded.IsNull(rowIdx)) return kNullHash;
        if constexpr (std::is_same_v<T, std::string_view>) {
            if (decoded.GetLayout() == DVecLayout::Flat) {
                using FlatVec = Vector<LargeStringContainer<T>>;
                return GroupbyHashCalculator<T>{}(static_cast<FlatVec*>(decoded.Base())->GetValue(rowIdx));
            }
            if (decoded.GetLayout() == DVecLayout::Dictionary) {
                using DicVec = Vector<DictionaryContainer<T>>;
                return GroupbyHashCalculator<T>{}(static_cast<DicVec*>(decoded.Base())->GetValue(rowIdx));
            }
            return GroupbyHashCalculator<T>{}(static_cast<ConstVector<T>*>(decoded.Base())->GetConstValue());
        }
        return GroupbyHashCalculator<T>{}(decoded.GetValue<T>(rowIdx));
    }

    /// Hash a single struct element (OMNI_ROW) from a decoded row vector.
    int64_t HashNestedRow(const DecodedVector& decoded, int32_t rowIdx)
    {
        auto* rowVec = static_cast<RowVector*>(decoded.Base());
        int64_t hash = 0;
        bool first = true;
        for (int32_t c = 0; c < rowVec->ChildSize(); c++) {
            DecodedVector childDecoded;
            childDecoded.Decode(rowVec->ChildAt(c).get(), decoded.RowCount());
            int64_t fieldHash = HashNestedField(childDecoded, rowIdx);
            if (first) { hash = fieldHash; first = false; }
            else { hash = FastHashMix(hash, fieldHash); }
        }
        return hash;
    }

    /// Hash a single array element (OMNI_ARRAY) from a decoded array vector.
    int64_t HashNestedArray(const DecodedVector& decoded, int32_t rowIdx)
    {
        auto* arrayVec = static_cast<ArrayVector*>(decoded.Base());
        int64_t start = arrayVec->GetOffset(rowIdx);
        int64_t end = arrayVec->GetOffset(rowIdx + 1);
        int64_t hash = 0;
        bool first = true;
        for (int64_t r = start; r < end; r++) {
            int64_t elemHash = HashArrayElementAt(arrayVec, r);
            if (first) { hash = elemHash; first = false; }
            else { hash = FastHashMix(hash, elemHash); }
        }
        return hash;
    }

    /// Hash a single element at position 'rowIdx' within an ArrayVector.
    /// Dispatches on the element type of the array.
    int64_t HashArrayElementAt(ArrayVector* arrayVec, int64_t rowIdx)
    {
        auto* elementVec = arrayVec->GetElementVector().get();
        auto elementTypeId = elementVec->GetTypeId();
        if (elementTypeId == type::OMNI_ROW) {
            DecodedVector elemDecoded;
            elemDecoded.Decode(elementVec, arrayVec->GetSize());
            return HashNestedRow(elemDecoded, static_cast<int32_t>(rowIdx));
        }
        if (elementTypeId == type::OMNI_ARRAY) {
            DecodedVector elemDecoded;
            elemDecoded.Decode(elementVec, arrayVec->GetSize());
            return HashNestedArray(elemDecoded, static_cast<int32_t>(rowIdx));
        }
        DecodedVector elemDecoded;
        elemDecoded.Decode(elementVec, arrayVec->GetSize());
        return DYNAMIC_TYPE_DISPATCH(HashOneDecodedValue, elementTypeId, elemDecoded, static_cast<int32_t>(rowIdx));
    }

    /// Hash a single field from a decoded vector, handling complex types.
    int64_t HashNestedField(const DecodedVector& decoded, int32_t rowIdx)
    {
        auto typeId = decoded.GetTypeId();
        if (typeId == type::OMNI_ROW) {
            return HashNestedRow(decoded, rowIdx);
        }
        if (typeId == type::OMNI_ARRAY) {
            return HashNestedArray(decoded, rowIdx);
        }
        return DYNAMIC_TYPE_DISPATCH(HashOneDecodedValue, typeId, decoded, rowIdx);
    }

    /// Hash arrays whose element type is OMNI_ROW (array of structs).
    void DoArrayHashElementRow(ArrayVector* arrayVector, int32_t rowsNum, bool isMix)
    {
        auto* elementRowVec = static_cast<RowVector*>(arrayVector->GetElementVector().get());
        int32_t childCount = elementRowVec->ChildSize();
        int64_t totalElements = arrayVector->GetOffset(rowsNum);
        if (UNLIKELY(totalElements > INT32_MAX)) {
            throw OmniException("DoArrayHashElementRow failed: total elements exceeds int32_t range");
        }

        std::vector<DecodedVector> decodedChildren(childCount);
        for (int32_t c = 0; c < childCount; c++) {
            decodedChildren[c].Decode(elementRowVec->ChildAt(c).get(), static_cast<int32_t>(totalElements));
        }

        bool hasNull = elementRowVec->HasNull();

        for (int32_t i = 0; i < rowsNum; i++) {
            int64_t start = arrayVector->GetOffset(i);
            int64_t end = arrayVector->GetOffset(i + 1);
            int64_t finalHash = 0;
            bool first = true;
            if (UNLIKELY(end > INT32_MAX)) {
                throw OmniException("DoArrayHashElementRow failed: end exceeds int32_t range");
            }
            for (int64_t row = start; row < end; row++) {
                if (hasNull && elementRowVec->IsNull(static_cast<int32_t>(row))) {
                    if (first) { finalHash = kNullHash; first = false; }
                    else { finalHash = FastHashMix(finalHash, kNullHash); }
                    continue;
                }
                int64_t elementHash = 0;
                for (int32_t c = 0; c < childCount; c++) {
                    int64_t fieldHash = HashNestedField(decodedChildren[c], static_cast<int32_t>(row));
                    if (c == 0) elementHash = fieldHash;
                    else elementHash = FastHashMix(elementHash, fieldHash);
                }
                if (first) { finalHash = elementHash; first = false; }
                else { finalHash = FastHashMix(finalHash, elementHash); }
            }
            if (isMix) {
                workingHashVals[i] = FastHashMix(workingHashVals[i], finalHash);
            } else {
                workingHashVals[i] = finalHash;
            }
        }
    }

    /// Hash arrays whose element type is OMNI_ARRAY (nested arrays).
    void DoArrayHashElementNestedArray(ArrayVector* arrayVector, int32_t rowsNum, bool isMix)
    {
        auto* innerArray = static_cast<ArrayVector*>(arrayVector->GetElementVector().get());
        int32_t innerSize = innerArray->GetSize();

        DecodedVector innerDecoded;
        innerDecoded.Decode(innerArray->GetElementVector().get(), innerSize);

        for (int32_t i = 0; i < rowsNum; i++) {
            int64_t outerStart = arrayVector->GetOffset(i);
            int64_t outerEnd = arrayVector->GetOffset(i + 1);
            int64_t finalHash = 0;
            bool first = true;
            for (int64_t outerRow = outerStart; outerRow < outerEnd; outerRow++) {
                int64_t innerStart = innerArray->GetOffset(static_cast<int64_t>(outerRow));
                int64_t innerEnd = innerArray->GetOffset(static_cast<int64_t>(outerRow) + 1);
                int64_t subArrayHash = 0;
                bool subFirst = true;
                for (int64_t innerRow = innerStart; innerRow < innerEnd; innerRow++) {
                    int64_t elemHash = HashNestedField(innerDecoded, static_cast<int32_t>(innerRow));
                    if (subFirst) { subArrayHash = elemHash; subFirst = false; }
                    else { subArrayHash = FastHashMix(subArrayHash, elemHash); }
                }
                if (first) { finalHash = subArrayHash; first = false; }
                else { finalHash = FastHashMix(finalHash, subArrayHash); }
            }
            if (isMix) {
                workingHashVals[i] = FastHashMix(workingHashVals[i], finalHash);
            } else {
                workingHashVals[i] = finalHash;
            }
        }
    }

    void DoArrayHashElementDispatch(ArrayVector* arrayVector, int32_t rowsNum, bool isMix)
    {
        auto elementVec = arrayVector->GetElementVector().get();
        auto elementTypeId = elementVec->GetTypeId();
        if (elementTypeId == type::OMNI_ROW) {
            DoArrayHashElementRow(arrayVector, rowsNum, isMix);
            return;
        }
        if (elementTypeId == type::OMNI_ARRAY) {
            DoArrayHashElementNestedArray(arrayVector, rowsNum, isMix);
            return;
        }
        #define ARRAY_HASH_DISPATCH(TID) \
            case TID: \
                DoArrayHashElementFlat<TID>(arrayVector, rowsNum, isMix); \
                break;
        switch (elementTypeId) {
            ARRAY_HASH_DISPATCH(type::OMNI_BYTE)
            ARRAY_HASH_DISPATCH(type::OMNI_SHORT)
            ARRAY_HASH_DISPATCH(type::OMNI_INT)
            ARRAY_HASH_DISPATCH(type::OMNI_DATE32)
            ARRAY_HASH_DISPATCH(type::OMNI_TIME32)
            ARRAY_HASH_DISPATCH(type::OMNI_LONG)
            ARRAY_HASH_DISPATCH(type::OMNI_TIMESTAMP)
            ARRAY_HASH_DISPATCH(type::OMNI_DECIMAL64)
            ARRAY_HASH_DISPATCH(type::OMNI_DATE64)
            ARRAY_HASH_DISPATCH(type::OMNI_TIME64)
            ARRAY_HASH_DISPATCH(type::OMNI_DOUBLE)
            ARRAY_HASH_DISPATCH(type::OMNI_FLOAT)
            ARRAY_HASH_DISPATCH(type::OMNI_DECIMAL128)
            ARRAY_HASH_DISPATCH(type::OMNI_BOOLEAN)
            ARRAY_HASH_DISPATCH(type::OMNI_VARCHAR)
            ARRAY_HASH_DISPATCH(type::OMNI_CHAR)
            ARRAY_HASH_DISPATCH(type::OMNI_VARBINARY)
            default: break;
        }
        #undef ARRAY_HASH_DISPATCH
    }

    void DoArrayHashDictDispatch(Vector<DictionaryContainer<ArrayVector*>>* dictVec, int32_t rowsNum, bool isMix)
    {
        for (int32_t i = 0; i < rowsNum; i++) {
            auto* arrayVector = dictVec->GetValue(i);
            DoArrayHashElementDispatch(arrayVector, 1, false);
            if (isMix) {
                workingHashVals[i] = FastHashMix(workingHashVals[i], workingHashVals[0]);
            } else {
                workingHashVals[i] = workingHashVals[0];
            }
        }
    }

    void DoArrayHashConstDispatch(ConstVector<ArrayVector*>* constVec, int32_t rowsNum, bool isMix)
    {
        auto* arrayVector = constVec->GetConstValue();
        DoArrayHashElementDispatch(arrayVector, 1, false);
        int64_t constHash = workingHashVals[0];
        for (int32_t i = 0; i < rowsNum; i++) {
            if (isMix) {
                workingHashVals[i] = FastHashMix(workingHashVals[i], constHash);
            } else {
                workingHashVals[i] = constHash;
            }
        }
    }

    void ParseKeyToCols(const type::StringRef &key, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int32_t rowIdx)
    {
        auto *pos = key.data;
        for (int32_t i = 0; i < groupColNum; ++i) {
            auto curVectorPtr = groupOutputVectors[i];
            auto deserializeFunc = deserializers[i];
            pos = deserializeFunc(curVectorPtr, rowIdx, pos);
        }
    }

    void InitSize(int groupBySize)
    {
        serializers.reserve(groupBySize);
        deserializers.reserve(groupBySize);
        comparators.reserve(groupBySize);
    }

    void ResetSerializer()
    {
        serializers.clear();
        deserializers.clear();
        comparators.clear();
    }

    void ResetIgnoreNullSerializer()
    {
        ignoreNullSerializers.clear();
    }

    void ResetFixedKeysIgnoreNullSerializer()
    {
        fixedKeysIgnoreNullSerializers.clear();
    }

    void ResetFixedKeysIgnoreNullSerializerSimd()
    {
        fixedKeysIgnoreNullSerializersSimd.clear();
    }

    void PushBackSerializer(VectorSerializer &serializer)
    {
        serializers.push_back(serializer);
    }

    void PushBackIgnoreNullSerializer(VectorSerializerIgnoreNull &serializer)
    {
        ignoreNullSerializers.push_back(serializer);
    }

    void PushBackFixedKeysIgnoreNullSerializer(FixedKeyVectorSerializerIgnoreNull &serializer)
    {
        fixedKeysIgnoreNullSerializers.push_back(serializer);
    }

    void PushBackFixedKeysIgnoreNullSerializerSimd(FixedKeyVectorSerializerIgnoreNullSimd &serializer)
    {
        fixedKeysIgnoreNullSerializersSimd.push_back(serializer);
    }

    void PushBackDeSerializer(VectorDeSerializer &deserializer)
    {
        deserializers.push_back(deserializer);
    }

    void PushBackComparator(VectorComparator &comparator)
    {
        comparators.push_back(comparator);
    }

    size_t GetElementsSize() const
    {
        return table->Size();
    }

    void ResetHashmap()
    {
        table->Clear();
        if (aggRows) {
            aggRows->Reset();
        }
        rowContainerIter = RowContainerIterator{};
        workingUpdateCount = 0;
        mergedVarcharCacheCount_ = 0;
    };

private:
    std::vector<VectorSerializer> serializers;
    std::vector<VectorDeSerializer> deserializers;
    std::vector<VectorComparator> comparators;

    std::vector<VectorSerializerIgnoreNull> ignoreNullSerializers;
    std::vector<FixedKeyVectorSerializerIgnoreNull> fixedKeysIgnoreNullSerializers;

    std::vector<FixedKeyVectorSerializerIgnoreNullSimd> fixedKeysIgnoreNullSerializersSimd;
};

template <typename T, bool isPacked = false>
class TaperGroupbySingleFixHandler {
public:
    static constexpr bool HasSpecialNullFunc = true;
    using HashTable = TaperFlatHashTable<T, true>;
    int32_t totalAggValueSize = 0;
    std::unique_ptr<HashTable> table;
    bool shouldExtractNull = false;
    int nullValueSize = 0;
    std::vector<T> keys;
    std::string nullValue;

    /// DecodedVector cache — resolves encoding once per batch so hot path
    /// runs with zero encoding branches (follows bolt TaperGroupFlatAgg pattern).
    std::vector<DecodedVector> decodedCols;

    /// Decode all group-by columns upfront. Call once per batch before EmplaceTable.
    void DecodeGroupByColumns(BaseVector** groupVectors, int32_t groupColNum, int32_t rowsNum)
    {
        if (static_cast<int32_t>(decodedCols.size()) != groupColNum) {
            decodedCols.resize(groupColNum);
        }
        for (int32_t i = 0; i < groupColNum; ++i) {
            decodedCols[i].Decode(groupVectors[i], rowsNum);
        }
    }

    TaperGroupbySingleFixHandler(mem::SimpleArenaAllocator &pool, int32_t size)
    {
        table = std::make_unique<HashTable>(pool, sizeof(T), sizeof(char*));
        totalAggValueSize = size;
    }

    TaperGroupbySingleFixHandler(mem::SimpleArenaAllocator &pool, int32_t size,
        std::vector<int32_t> typeIds, std::vector<uint8_t> bitWidths)
    {
        table = std::make_unique<HashTable>(pool, sizeof(T), sizeof(char*));
        totalAggValueSize = size;
        if constexpr (isPacked) {
            InitPlan(std::move(typeIds), std::move(bitWidths));
        }
    }

    uint8_t*& RowFromData(char* data)
    {
        return *reinterpret_cast<uint8_t**>(data);
    }

    void EmplaceTable(BaseVector **groupVectors, int32_t groupColNum, int32_t rowsNum,
        std::vector<uint8_t*>& groups, std::vector<uint8_t*>& newGroups, Encoding encoding)
    {
        (void)encoding;

        auto initRow = [&](uint32_t rowIdx, char* data) {
            if (totalAggValueSize > 0) {
                auto* row = table->Pool().Allocate(totalAggValueSize);
                RowFromData(data) = reinterpret_cast<uint8_t*>(row);
                newGroups.push_back(RowFromData(data));
            }
        };

        DecodeGroupByColumns(groupVectors, groupColNum, rowsNum);

        if constexpr (isPacked) {
            InitKeysPackedFromDecode(rowsNum, groups, newGroups);
        } else {
            auto& decoded = decodedCols[0];
            auto layout = decoded.GetLayout();
            switch (layout) {
                case DVecLayout::Flat:
                    InitKeysFromDecode<false, false>(rowsNum, groups, newGroups);
                    break;
                case DVecLayout::Dictionary:
                    InitKeysFromDecode<true, false>(rowsNum, groups, newGroups);
                    break;
                case DVecLayout::Constant:
                    InitKeysFromDecode<false, true>(rowsNum, groups, newGroups);
                    break;
            }
        }

        table->EmplaceBatch(
            keys.data(),
            rowsNum,
            [&](uint32_t idx) { return !isPacked && decodedCols[0].IsNull(idx); },
            [&](uint32_t rowIdx, char* data) { initRow(rowIdx, data); },
            [&](uint32_t rowIdx, char* data, bool initFlag) {
                groups[rowIdx] = RowFromData(data);
            });
    }

    /// Zero-branch key initialization using DecodedVector.
    /// Template parameters isDic/isConst are compile-time constants → no runtime encoding checks in hot loop.
    template<bool isDic = false, bool isConst = false>
    void InitKeysFromDecode(int32_t rowsNum, std::vector<uint8_t*>& groups, std::vector<uint8_t*>& newGroups)
    {
        auto& decoded = decodedCols[0];
        keys.resize(rowsNum);
        const uint64_t* rawNulls = decoded.HasNull() ?
            reinterpret_cast<const uint64_t*>(decoded.Nulls()) : nullptr;

        auto handleNull = [&](int32_t i) {
            if (!shouldExtractNull) {
                if (totalAggValueSize > 0) {
                    nullValue.resize(totalAggValueSize);
                }
                newGroups.push_back(reinterpret_cast<uint8_t*>(nullValue.data()));
                shouldExtractNull = true;
                nullValueSize = 1;
            }
            groups[i] = reinterpret_cast<uint8_t*>(nullValue.data());
        };

        if constexpr (isConst) {
            T constVal = decoded.GetConstValue<T>();
            for (int32_t i = 0; i < rowsNum; i++) {
                if (rawNulls && BitUtil::IsBitSet(rawNulls, i)) {
                    handleNull(i);
                } else {
                    keys[i] = constVal;
                }
            }
        } else if constexpr (isDic) {
            const T* dictVals = decoded.FlatValues<T>();
            const int32_t* ids = decoded.Ids();
            for (int32_t i = 0; i < rowsNum; i++) {
                if (rawNulls && BitUtil::IsBitSet(rawNulls, i)) {
                    handleNull(i);
                } else {
                    keys[i] = dictVals[ids[i]];
                }
            }
        } else {
            const T* flatVals = decoded.FlatValues<T>();
            for (int32_t i = 0; i < rowsNum; i++) {
                if (rawNulls && BitUtil::IsBitSet(rawNulls, i)) {
                    handleNull(i);
                } else {
                    keys[i] = flatVals[i];
                }
            }
        }
    }

    /// Packed mode: initialize keys using DecodedVector (multi-column).
    void InitKeysPackedFromDecode(int32_t rowsNum, std::vector<uint8_t*>& groups, std::vector<uint8_t*>& newGroups)
    {
        keys.resize(rowsNum);
        PrepareFromDecode();
        for (int32_t i = 0; i < rowsNum; i++) {
            keys[i] = PackKeyFromDecode(i);
        }
    }

    template<bool isNull>
    void InsertOneValueToHashmap(T key, uint8_t *value)
    {
        if constexpr (isNull) {
            if (!shouldExtractNull) {
                if (totalAggValueSize > 0) {
                    nullValue.resize(totalAggValueSize);
                    std::memcpy(nullValue.data(), value, totalAggValueSize);
                }
                shouldExtractNull = true;
                nullValueSize = 1;
            }
            return;
        }

        table->Emplace(
          key,
          [&](char* data) {
             RowFromData(data) = value;
          },
          [&](char* data, bool) { });
    }

   template <class Func, class NullFunc>
   void Extract(int32_t rowsNum, OutputState &outputState, Func func, NullFunc nullFunc)
    {
        auto tblVisitor = [&] {
            if (outputState.rowBegin) {
                return table->GetResultVisitor(
                    outputState.rowBegin, static_cast<uint16_t>(outputState.rowOffset));
            }
            return table->GetResultVisitor();
        }();
        uint32_t idx = 0;
        if (shouldExtractNull) {
            auto key = static_cast<T>(0);
            nullFunc(key, reinterpret_cast<uint8_t*>(nullValue.data()), idx);
            shouldExtractNull = false;
            idx++;
        }
        while (idx < rowsNum && !tblVisitor.Finished()) {
            auto *row = RowFromData(tblVisitor.CurVal().buf);
            func(tblVisitor.CurKey(), row, idx);
            tblVisitor.Next();
            idx++;
        }
        tblVisitor.SavePos([&](auto ptr, auto tagPos) {
            outputState.rowBegin = reinterpret_cast<char*>(ptr);
            outputState.rowOffset = tagPos;
            outputState.hasBeenOutputNum += rowsNum;
        });
    }

    template<bool isDict = false>
    void ParseKeyToColsSingle(const T &key, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int rowIdx)
    {
        auto curVectorPtr = groupOutputVectors[0];
        if constexpr (isDict) {
            auto dictionaryVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(curVectorPtr);
            dictionaryVector->SetValue(rowIdx, key);
        } else {
            reinterpret_cast<Vector<T>*>(curVectorPtr)->SetValue(rowIdx, key);
        }
    }

    void ParseKeyToCols(const T &key, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int rowIdx)
    {
        if constexpr (isPacked) {
            UnpackKey(key, groupColNum);
            for (int32_t col = 0; col < groupColNum; ++col) {
                auto *outVector = groupOutputVectors[col];
                if (unpackIsNull[col]) {
                    vec::VectorHelper::SetNull(outVector, rowIdx);
                    continue;
                }
                SetValueByType(outVector, rowIdx, plan[col].typeId, unpackValues[col]);
            }
        } else {
            auto curVectorPtr = groupOutputVectors[0];
            bool isDictEncoded = (curVectorPtr->GetEncoding() == Encoding::OMNI_DICTIONARY);
            if (isDictEncoded) {
                ParseKeyToColsSingle<true>(key, groupOutputVectors, groupColNum, rowIdx);
            } else {
                ParseKeyToColsSingle<false>(key, groupOutputVectors, groupColNum, rowIdx);
            }
        }
    }

    void ParseNull(const T &key, std::vector<vec::BaseVector *> &groupOutputVectors, int32_t groupColNum,
        const int rowIdx)
    {
        auto curVectorPtr = groupOutputVectors[0];
        vec::VectorHelper::SetNull(curVectorPtr, rowIdx);
    }

    size_t GetElementsSize() const
    {
        return table->Size() + nullValueSize;
    }

    void ResetHashmap()
    {
        table->Clear();
        nullValue.clear();
    }
    private:
    using UnsignedKey = std::conditional_t<std::is_same_v<T, omniruntime::type::int128_t>,
        __uint128_t, std::make_unsigned_t<T>>;

    using LoaderFn = UnsignedKey (*)(BaseVector *vector, int32_t rowIdx, UnsignedKey mask);

    struct PlanEntry {
        int32_t typeId = OMNI_INVALID;
        uint8_t bitWidth = 0;
        UnsignedKey mask = 0;
        LoaderFn flatLoader = nullptr;
        LoaderFn dictLoader = nullptr;
        LoaderFn constLoader = nullptr;
        LoaderFn activeLoader = nullptr;
    };

    std::vector<PlanEntry> plan;
    mutable std::vector<uint8_t> unpackIsNull;
    mutable std::vector<UnsignedKey> unpackValues;

    /// Set activeLoaders based on decoded layout (zero runtime encoding branches).
    ALWAYS_INLINE void PrepareFromDecode()
    {
        for (size_t col = 0; col < decodedCols.size(); ++col) {
            const auto layout = decodedCols[col].GetLayout();
            if (layout == DVecLayout::Dictionary) {
                plan[col].activeLoader = plan[col].dictLoader;
            } else if (layout == DVecLayout::Constant) {
                plan[col].activeLoader = plan[col].constLoader;
            } else {
                plan[col].activeLoader = plan[col].flatLoader;
            }
        }
    }

    static ALWAYS_INLINE UnsignedKey MaskForWidth(uint8_t width)
    {
        if (width == 0) {
            return 0;
        }
        constexpr uint8_t kBits = static_cast<uint8_t>(sizeof(UnsignedKey) * 8);
        if (width >= kBits) {
            return static_cast<UnsignedKey>(~static_cast<UnsignedKey>(0));
        }
        return (static_cast<UnsignedKey>(1) << width) - 1;
    }

    template<typename K, bool isDict>
    static ALWAYS_INLINE UnsignedKey LoadBits(BaseVector *vector, int32_t rowIdx, UnsignedKey mask)
    {
        if constexpr (isDict) {
            auto v = reinterpret_cast<Vector<DictionaryContainer<K>> *>(vector)->GetValue(rowIdx);
            return static_cast<UnsignedKey>(static_cast<std::make_unsigned_t<K>>(v)) & mask;
        } else {
            auto v = reinterpret_cast<Vector<K> *>(vector)->GetValue(rowIdx);
            return static_cast<UnsignedKey>(static_cast<std::make_unsigned_t<K>>(v)) & mask;
        }
    }

    template<typename K>
    static ALWAYS_INLINE UnsignedKey LoadBitsConst(BaseVector *vector, int32_t rowIdx, UnsignedKey mask)
    {
        (void)rowIdx;
        auto v = reinterpret_cast<ConstVector<K> *>(vector)->GetConstValue();
        return static_cast<UnsignedKey>(static_cast<std::make_unsigned_t<K>>(v)) & mask;
    }

    static ALWAYS_INLINE void SetValueByType(BaseVector *vector, int32_t rowIdx, int32_t typeId, UnsignedKey value)
    {
        switch (typeId) {
            case OMNI_BYTE:
                reinterpret_cast<Vector<int8_t> *>(vector)->SetValue(rowIdx, static_cast<int8_t>(value));
                break;
            case OMNI_SHORT:
                reinterpret_cast<Vector<int16_t> *>(vector)->SetValue(rowIdx, static_cast<int16_t>(value));
                break;
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_TIME32:
                reinterpret_cast<Vector<int32_t> *>(vector)->SetValue(rowIdx, static_cast<int32_t>(value));
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
            case OMNI_DATE64:
            case OMNI_TIME64:
                reinterpret_cast<Vector<int64_t> *>(vector)->SetValue(rowIdx, static_cast<int64_t>(value));
                break;
            case OMNI_BOOLEAN:
                reinterpret_cast<Vector<bool> *>(vector)->SetValue(rowIdx, static_cast<bool>(value));
                break;
            default:
                break;
        }
    }

    /// Pack key from DecodedVector — uses pre-decoded null bitmap and value pointers.
    ALWAYS_INLINE T PackKeyFromDecode(int32_t rowIdx) const
    {
        UnsignedKey packed = 0;
        for (size_t col = 0; col < decodedCols.size(); ++col) {
            auto &entry = plan[col];
            bool isNull = decodedCols[col].IsNull(rowIdx);
            packed = (packed << 1) | static_cast<UnsignedKey>(isNull ? 1 : 0);
            UnsignedKey valueBits = 0;
            if (!isNull) {
                valueBits = entry.activeLoader(decodedCols[col].Base(), rowIdx, entry.mask);
            }
            packed = (packed << entry.bitWidth) | valueBits;
        }
        return static_cast<T>(packed);
    }

    ALWAYS_INLINE void UnpackKey(const T &key, int32_t groupColNum) const
    {
        UnsignedKey packed = static_cast<UnsignedKey>(key);
        if (UNLIKELY(static_cast<size_t>(groupColNum) != plan.size())) {
            unpackIsNull.resize(groupColNum);
            unpackValues.resize(groupColNum);
        }
        for (int32_t col = groupColNum - 1; col >= 0; --col) {
            auto width = plan[col].bitWidth;
            auto mask = plan[col].mask;
            unpackValues[col] = packed & mask;
            packed >>= width;
            unpackIsNull[col] = static_cast<uint8_t>(packed & 1);
            packed >>= 1;
        }
    }

    void InitPlan(std::vector<int32_t> typeIds, std::vector<uint8_t> bitWidths)
    {
        plan.resize(typeIds.size());
        unpackIsNull.resize(typeIds.size());
        unpackValues.resize(typeIds.size());
        for (size_t i = 0; i < typeIds.size(); ++i) {
            plan[i].typeId = typeIds[i];
            plan[i].bitWidth = bitWidths[i];
            plan[i].mask = MaskForWidth(bitWidths[i]);
            switch (typeIds[i]) {
                case OMNI_BYTE:
                    plan[i].flatLoader = &LoadBits<int8_t, false>;
                    plan[i].dictLoader = &LoadBits<int8_t, true>;
                    plan[i].constLoader = &LoadBitsConst<int8_t>;
                    break;
                case OMNI_SHORT:
                    plan[i].flatLoader = &LoadBits<int16_t, false>;
                    plan[i].dictLoader = &LoadBits<int16_t, true>;
                    plan[i].constLoader = &LoadBitsConst<int16_t>;
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                case OMNI_TIME32:
                    plan[i].flatLoader = &LoadBits<int32_t, false>;
                    plan[i].dictLoader = &LoadBits<int32_t, true>;
                    plan[i].constLoader = &LoadBitsConst<int32_t>;
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                case OMNI_DATE64:
                case OMNI_TIME64:
                    plan[i].flatLoader = &LoadBits<int64_t, false>;
                    plan[i].dictLoader = &LoadBits<int64_t, true>;
                    plan[i].constLoader = &LoadBitsConst<int64_t>;
                    break;
                case OMNI_BOOLEAN:
                    plan[i].flatLoader = &LoadBits<int8_t, false>;
                    plan[i].dictLoader = &LoadBits<int8_t, true>;
                    plan[i].constLoader = &LoadBitsConst<int8_t>;
                    break;
                default:
                    plan[i].flatLoader = nullptr;
                    plan[i].dictLoader = nullptr;
                    plan[i].constLoader = nullptr;
                    break;
            }
            plan[i].activeLoader = plan[i].flatLoader;
        }
    }
};

template <typename Key, typename Value>
class TaperFixedKeyMap {
public:
    using Keys = Key;
    using Values = Value;
    using HashTable = TaperFlatHashTable<Key, false>;
    static constexpr bool StoresValues = true;
    static constexpr uint32_t ROW_PTR_SIZE = 6;

    static_assert(std::is_pointer_v<Value>, "TaperFixedKeyMap value must be a row pointer");

    void Init(mem::SimpleArenaAllocator &pool)
    {
        table = std::make_unique<HashTable>(pool, sizeof(Key), ROW_PTR_SIZE);
    }

    template <typename InitValue>
    void EmplaceBatch(const Key *keys, int32_t rowsNum, InitValue &&initValue,
        std::vector<Value> &groups)
    {
        groups.resize(rowsNum);
        table->EmplaceBatch(
            keys,
            rowsNum,
            [](uint32_t) { return false; },
            [&](uint32_t rowIdx, char *data) {
                SetValue(data, initValue(rowIdx));
            },
            [&](uint32_t rowIdx, char *data, bool) {
                groups[rowIdx] = GetValue(data);
            });
    }

    void InsertExisting(const Key &key, Value value)
    {
        table->Emplace(
            key,
            [&](char *data) { SetValue(data, value); },
            [&](char *, bool) {});
    }

    size_t GetElementsSize() const
    {
        return table == nullptr ? 0 : table->Size();
    }

    void Reset()
    {
        if (table != nullptr) {
            table->Clear();
        }
    }

    HashTable *GetTable()
    {
        return table.get();
    }

    static ALWAYS_INLINE void SetValue(char *buf, Value value)
    {
        uint64_t raw = reinterpret_cast<uint64_t>(value);
        std::memcpy(buf, &raw, ROW_PTR_SIZE);
    }

    static ALWAYS_INLINE Value GetValue(const char *buf)
    {
        uint64_t raw = 0;
        std::memcpy(&raw, buf, ROW_PTR_SIZE);
        return reinterpret_cast<Value>(raw);
    }

private:
    std::unique_ptr<HashTable> table;
};

template <typename Key>
class TaperFixedKeySet {
public:
    using Keys = Key;
    using Values = uint8_t *;
    using HashTable = TaperFlatHashTable<Key, false>;
    static constexpr bool StoresValues = false;

    void Init(mem::SimpleArenaAllocator &pool)
    {
        table = std::make_unique<HashTable>(pool, sizeof(Key), 0);
    }

    void EmplaceBatch(const Key *keys, int32_t rowsNum)
    {
        table->EmplaceKeyBatch(keys, static_cast<uint32_t>(rowsNum), [](uint32_t) { return false; });
    }

    template <typename Func>
    void ForEachKey(Func &&func)
    {
        auto visitor = table->GetResultVisitor();
        while (!visitor.Finished()) {
            func(visitor.CurKey());
            visitor.Next();
        }
    }

    size_t GetElementsSize() const
    {
        return table == nullptr ? 0 : table->Size();
    }

    void Reset()
    {
        if (table != nullptr) {
            table->Clear();
        }
    }

    HashTable *GetTable()
    {
        return table.get();
    }

private:
    std::unique_ptr<HashTable> table;
};

template <typename Hashmap>
class NormalizeKeyHandler {
public:
    static constexpr bool HasSpecialNullFunc = false;
    using KeyType = typename Hashmap::Keys;
    using ValueType = typename Hashmap::Values;
    using SignedKey = omniruntime::type::int128_t;
    using EncodedKey = omniruntime::type::uint128_t;

    static bool CanUse(const std::vector<type::DataTypeId> &groupByTypeIds)
    {
        return BuildBitLayout(groupByTypeIds, nullptr, nullptr, nullptr, nullptr);
    }

    bool Init(const std::vector<type::DataTypeId> &groupByTypeIds,
        const std::vector<int32_t> &keyTypeSizes, int32_t aggStateSize,
        mem::SimpleArenaAllocator &pool)
    {
        colCount_ = static_cast<int32_t>(groupByTypeIds.size());
        if (colCount_ <= 1 ||
            (Hashmap::StoresValues && keyTypeSizes.size() != groupByTypeIds.size())) {
            return false;
        }
        for (const auto typeId : groupByTypeIds) {
            if (!IsSupportedType(typeId)) {
                return false;
            }
        }
        typeIds_ = groupByTypeIds;
        if (!BuildBitLayout(typeIds_, &fullBits_, &idBits_, &bitOffsets_, &idMasks_)) {
            return false;
        }

        if constexpr (!Hashmap::StoresValues) {
            if (aggStateSize != 0) {
                return false;
            }
        }
        hashmap.Init(pool);
        if constexpr (Hashmap::StoresValues) {
            aggRows_ = std::make_unique<RowContainer>(keyTypeSizes, colCount_, aggStateSize, pool);
            aggStateSize_ = aggStateSize;
        }
        hasRange_.assign(colCount_, false);
        min_.assign(colCount_, 0);
        max_.assign(colCount_, 0);
        activeReadPlans_.resize(colCount_);
        InitializeRanges();
        return true;
    }

    bool TryEncodeBatch(BaseVector **groupVectors, int32_t groupColNum, int32_t rowCount)
    {
        if (UNLIKELY(groupColNum != colCount_ || rowCount < 0)) {
            return false;
        }
        if constexpr (Hashmap::StoresValues) {
            if (aggRows_ == nullptr) {
                return false;
            }
        }
        if (!PrepareReaders(groupVectors)) {
            return false;
        }

        if (!rangeInitialized_) {
            ResetStats();
            AnalyzeBatch(groupVectors, rowCount);
            if (!ConfigureRanges()) {
                return false;
            }
            rangeInitialized_ = true;
            return EncodeBatch(groupVectors, rowCount);
        }

        if (EncodeBatch(groupVectors, rowCount)) {
            return true;
        }

        // Like Velox/Bolt, reconsider the range only after a value no longer
        // maps. Existing distinct keys are the authoritative history.
        ResetStats();
        const auto oldMin = min_;
        const auto oldMax = max_;
        const auto oldHasRange = hasRange_;
        if constexpr (Hashmap::StoresValues) {
            AnalyzeStoredRows();
        } else {
            AnalyzeStoredKeys(oldMin);
        }
        AnalyzeBatch(groupVectors, rowCount);
        if (!ConfigureRanges()) {
            if constexpr (!Hashmap::StoresValues) {
                min_ = oldMin;
                max_ = oldMax;
                hasRange_ = oldHasRange;
            }
            return false;
        }
        if (!RebuildHashmap(oldMin)) {
            min_ = oldMin;
            max_ = oldMax;
            hasRange_ = oldHasRange;
            return false;
        }
        return EncodeBatch(groupVectors, rowCount);
    }

    void EmplaceStates(BaseVector **groupVectors, int32_t rowCount,
        std::vector<ValueType> &groups, std::vector<ValueType> &newGroups)
    {
        static_assert(Hashmap::StoresValues, "EmplaceStates requires a value map");
        hashmap.EmplaceBatch(
            encodedKeysBuffer_.data(), rowCount,
            [&](uint32_t rowIdx) {
                auto *row = aggRows_->NewRow();
                StoreOriginalKeys(groupVectors, static_cast<int32_t>(rowIdx), row);
                auto value = reinterpret_cast<ValueType>(row + aggRows_->AggStateOffset());
                if (aggStateSize_ > 0) {
                    newGroups.emplace_back(value);
                }
                return value;
            },
            groups);
    }

    void EmplaceKeys(int32_t rowCount)
    {
        static_assert(!Hashmap::StoresValues, "EmplaceKeys requires a key set");
        hashmap.EmplaceBatch(encodedKeysBuffer_.data(), rowCount);
    }

    void ParseKeyToCols(const KeyType &key, std::vector<BaseVector *> &groupOutputVectors,
        int32_t groupColNum, int32_t rowIdx)
    {
        const auto encoded = static_cast<EncodedKey>(key);
        for (int32_t col = 0; col < groupColNum; ++col) {
            const auto id = (encoded >> bitOffsets_[col]) & idMasks_[col];
            if (id == 0) {
                groupOutputVectors[col]->SetNull(rowIdx);
                continue;
            }
            const auto value = static_cast<int64_t>(
                static_cast<SignedKey>(min_[col]) + static_cast<SignedKey>(id) - 1);
            SetNormalizedFixedValue(groupOutputVectors[col], rowIdx, typeIds_[col], value);
        }
    }

    void ParseRowToCols(ValueType value, std::vector<BaseVector *> &groupOutputVectors,
        int32_t groupColNum, int32_t rowIdx)
    {
        static_assert(Hashmap::StoresValues, "ParseRowToCols requires a value map");
        const auto *row = reinterpret_cast<const char *>(value) - aggRows_->AggStateOffset();
        for (int32_t col = 0; col < groupColNum; ++col) {
            const auto rowColumn = aggRows_->ColumnAt(col);
            if (RowContainer::IsNullAt(row, rowColumn.NullByte(), rowColumn.NullMask())) {
                groupOutputVectors[col]->SetNull(rowIdx);
                continue;
            }
            SetNormalizedFixedValue(groupOutputVectors[col], rowIdx, typeIds_[col],
                ReadStoredValue(row, col));
        }
    }

    void ParseNull(const KeyType &, std::vector<BaseVector *> &groupOutputVectors,
        int32_t groupColNum, int32_t rowIdx)
    {
        for (int32_t col = 0; col < groupColNum; ++col) {
            groupOutputVectors[col]->SetNull(rowIdx);
        }
    }

    template <class Func, class NullFunc>
    void Extract(int32_t rowsNum, OutputState &outputState, Func func, NullFunc nullFunc)
    {
        (void)nullFunc;
        auto *table = hashmap.GetTable();
        auto visitor = outputState.rowBegin == nullptr
            ? table->GetResultVisitor()
            : table->GetResultVisitor(outputState.rowBegin, static_cast<uint16_t>(outputState.rowOffset));
        uint32_t idx = 0;
        while (idx < static_cast<uint32_t>(rowsNum) && !visitor.Finished()) {
            if constexpr (Hashmap::StoresValues) {
                auto value = Hashmap::GetValue(visitor.CurVal().buf);
                func(visitor.CurKey(), reinterpret_cast<uint8_t *>(value), idx);
            } else {
                func(visitor.CurKey(), nullptr, idx);
            }
            visitor.Next();
            ++idx;
        }
        visitor.SavePos([&](auto ptr, auto tagPos) {
            outputState.rowBegin = reinterpret_cast<char *>(ptr);
            outputState.rowOffset = tagPos;
            outputState.hasBeenOutputNum += idx;
        });
    }

    template <class Func>
    void ForEachRow(Func &&func)
    {
        static_assert(Hashmap::StoresValues, "ForEachRow requires a value map");
        RowContainerIterator iterator;
        char *rows[kRehashBatchSize];
        int32_t rowCount = 0;
        do {
            rowCount = aggRows_->ListRows(&iterator, kRehashBatchSize, rows);
            for (int32_t row = 0; row < rowCount; ++row) {
                func(reinterpret_cast<ValueType>(rows[row] + aggRows_->AggStateOffset()));
            }
        } while (rowCount > 0);
    }

    template <class Func>
    void ForEachKey(Func &&func)
    {
        static_assert(!Hashmap::StoresValues, "ForEachKey requires a key set");
        hashmap.ForEachKey(std::forward<Func>(func));
    }

    size_t GetElementsSize() const
    {
        return hashmap.GetElementsSize();
    }

    void ResetHashmap()
    {
        hashmap.Reset();
        if constexpr (Hashmap::StoresValues) {
            if (aggRows_ != nullptr) {
                aggRows_->Reset();
            }
        }
        InitializeRanges();
    }

private:
    static constexpr uint8_t kTotalKeyBits = 127;
    static constexpr int32_t kRehashBatchSize = 1024;
    using ReaderFn = int64_t (*)(BaseVector *, int32_t);
    using AnalyzeColumnFn = void (*)(BaseVector *, int32_t, bool &, int64_t &, int64_t &);
    using EncodeColumnFn = bool (*)(BaseVector *, int32_t, int64_t, int64_t,
        EncodedKey, uint8_t, bool, KeyType *);

    struct ColumnReadPlan {
        ReaderFn read = nullptr;
        AnalyzeColumnFn analyze = nullptr;
        EncodeColumnFn encode = nullptr;
    };

    bool HasFullRange(int32_t col) const
    {
        return idBits_[col] == fullBits_[col];
    }

    void InitializeRanges()
    {
        rangeInitialized_ = true;
        for (int32_t col = 0; col < colCount_; ++col) {
            if (HasFullRange(col)) {
                hasRange_[col] = true;
                min_[col] = TypeMin(typeIds_[col]);
                max_[col] = TypeMax(typeIds_[col]);
            } else {
                hasRange_[col] = false;
                rangeInitialized_ = false;
            }
        }
    }

    static bool IsSupportedType(type::DataTypeId typeId)
    {
        return typeId == type::OMNI_BYTE || typeId == type::OMNI_SHORT ||
            typeId == type::OMNI_INT || typeId == type::OMNI_DATE32 ||
            typeId == type::OMNI_TIME32 || typeId == type::OMNI_LONG ||
            typeId == type::OMNI_DATE64 || typeId == type::OMNI_TIME64 ||
            typeId == type::OMNI_TIMESTAMP || typeId == type::OMNI_DECIMAL64;
    }

    template <type::DataTypeId TypeId, Encoding VectorEncoding>
    static int64_t ReadNormalizedFixedValueTyped(BaseVector *vector, int32_t rowIdx)
    {
        using NativeType = typename NativeAndVectorType<TypeId>::type;
        if constexpr (VectorEncoding == vec::OMNI_DICTIONARY) {
            return static_cast<int64_t>(
                static_cast<Vector<DictionaryContainer<NativeType>> *>(vector)->GetValue(rowIdx));
        } else if constexpr (VectorEncoding == vec::OMNI_ENCODING_CONST) {
            return static_cast<int64_t>(static_cast<ConstVector<NativeType> *>(vector)->GetConstValue());
        } else {
            return static_cast<int64_t>(static_cast<Vector<NativeType> *>(vector)->GetValue(rowIdx));
        }
    }

    template <type::DataTypeId TypeId, Encoding VectorEncoding, bool HasNull>
    static void AnalyzeColumnTyped(BaseVector *vector, int32_t rowCount,
        bool &hasValue, int64_t &minValue, int64_t &maxValue)
    {
        for (int32_t row = 0; row < rowCount; ++row) {
            if constexpr (HasNull) {
                if (vector->IsNull(row)) {
                    continue;
                }
            }
            const int64_t value = ReadNormalizedFixedValueTyped<TypeId, VectorEncoding>(vector, row);
            if (!hasValue) {
                hasValue = true;
                minValue = value;
                maxValue = value;
            } else {
                minValue = std::min(minValue, value);
                maxValue = std::max(maxValue, value);
            }
        }
    }

    template <type::DataTypeId TypeId, Encoding VectorEncoding, bool HasNull>
    static bool EncodeColumnTyped(BaseVector *vector, int32_t rowCount,
        int64_t minValue, int64_t maxValue, EncodedKey mask, uint8_t bitOffset,
        bool initialize, KeyType *encodedKeys)
    {
        bool allMapped = true;
        for (int32_t row = 0; row < rowCount; ++row) {
            if constexpr (HasNull) {
                if (vector->IsNull(row)) {
                    if (initialize) {
                        encodedKeys[row] = KeyType(0);
                    }
                    continue;
                }
            }
            const int64_t value = ReadNormalizedFixedValueTyped<TypeId, VectorEncoding>(vector, row);
            if (value < minValue || value > maxValue) {
                allMapped = false;
                if (initialize) {
                    encodedKeys[row] = KeyType(0);
                }
                continue;
            }
            const auto id = static_cast<EncodedKey>(
                static_cast<SignedKey>(value) - static_cast<SignedKey>(minValue) + 1);
            if (id > mask) {
                allMapped = false;
                if (initialize) {
                    encodedKeys[row] = KeyType(0);
                }
                continue;
            }
            const auto shifted = id << bitOffset;
            encodedKeys[row] = initialize ? static_cast<KeyType>(shifted) :
                static_cast<KeyType>(static_cast<EncodedKey>(encodedKeys[row]) | shifted);
        }
        return allMapped;
    }

    template <type::DataTypeId TypeId, Encoding VectorEncoding, bool HasNull>
    static ColumnReadPlan MakeReadPlan()
    {
        return {
            &ReadNormalizedFixedValueTyped<TypeId, VectorEncoding>,
            &AnalyzeColumnTyped<TypeId, VectorEncoding, HasNull>,
            &EncodeColumnTyped<TypeId, VectorEncoding, HasNull>};
    }

    template <type::DataTypeId TypeId>
    static ColumnReadPlan SelectReaderTyped(Encoding encoding, bool hasNull)
    {
        switch (encoding) {
            case vec::OMNI_FLAT:
                return hasNull ? MakeReadPlan<TypeId, vec::OMNI_FLAT, true>() :
                    MakeReadPlan<TypeId, vec::OMNI_FLAT, false>();
            case vec::OMNI_DICTIONARY:
                return hasNull ? MakeReadPlan<TypeId, vec::OMNI_DICTIONARY, true>() :
                    MakeReadPlan<TypeId, vec::OMNI_DICTIONARY, false>();
            case vec::OMNI_ENCODING_CONST:
                return hasNull ? MakeReadPlan<TypeId, vec::OMNI_ENCODING_CONST, true>() :
                    MakeReadPlan<TypeId, vec::OMNI_ENCODING_CONST, false>();
            default:
                return {};
        }
    }

    static ColumnReadPlan SelectReader(type::DataTypeId typeId, Encoding encoding, bool hasNull)
    {
        switch (typeId) {
            case type::OMNI_BYTE:
                return SelectReaderTyped<type::OMNI_BYTE>(encoding, hasNull);
            case type::OMNI_SHORT:
                return SelectReaderTyped<type::OMNI_SHORT>(encoding, hasNull);
            case type::OMNI_INT:
                return SelectReaderTyped<type::OMNI_INT>(encoding, hasNull);
            case type::OMNI_DATE32:
                return SelectReaderTyped<type::OMNI_DATE32>(encoding, hasNull);
            case type::OMNI_TIME32:
                return SelectReaderTyped<type::OMNI_TIME32>(encoding, hasNull);
            case type::OMNI_LONG:
                return SelectReaderTyped<type::OMNI_LONG>(encoding, hasNull);
            case type::OMNI_DATE64:
                return SelectReaderTyped<type::OMNI_DATE64>(encoding, hasNull);
            case type::OMNI_TIME64:
                return SelectReaderTyped<type::OMNI_TIME64>(encoding, hasNull);
            case type::OMNI_TIMESTAMP:
                return SelectReaderTyped<type::OMNI_TIMESTAMP>(encoding, hasNull);
            case type::OMNI_DECIMAL64:
                return SelectReaderTyped<type::OMNI_DECIMAL64>(encoding, hasNull);
            default:
                return {};
        }
    }

    bool PrepareReaders(BaseVector **groupVectors)
    {
        for (int32_t col = 0; col < colCount_; ++col) {
            if (groupVectors[col] == nullptr) {
                return false;
            }
            activeReadPlans_[col] = SelectReader(
                typeIds_[col], groupVectors[col]->GetEncoding(), groupVectors[col]->HasNull());
            if (activeReadPlans_[col].read == nullptr) {
                return false;
            }
        }
        return true;
    }

    void ResetStats()
    {
        statsHasValue_.assign(colCount_, false);
        statsMin_.assign(colCount_, std::numeric_limits<int64_t>::max());
        statsMax_.assign(colCount_, std::numeric_limits<int64_t>::min());
    }

    ALWAYS_INLINE void UpdateStats(int32_t col, int64_t value)
    {
        if (!statsHasValue_[col]) {
            statsHasValue_[col] = true;
            statsMin_[col] = value;
            statsMax_[col] = value;
            return;
        }
        statsMin_[col] = std::min(statsMin_[col], value);
        statsMax_[col] = std::max(statsMax_[col], value);
    }

    void AnalyzeBatch(BaseVector **groupVectors, int32_t rowCount)
    {
        for (int32_t col = 0; col < colCount_; ++col) {
            if (HasFullRange(col)) {
                continue;
            }
            bool hasValue = statsHasValue_[col];
            activeReadPlans_[col].analyze(groupVectors[col], rowCount, hasValue, statsMin_[col], statsMax_[col]);
            statsHasValue_[col] = hasValue;
        }
    }

    void AnalyzeStoredRows()
    {
        RowContainerIterator iterator;
        char *rows[kRehashBatchSize];
        int32_t rowCount = 0;
        do {
            rowCount = aggRows_->ListRows(&iterator, kRehashBatchSize, rows);
            for (int32_t row = 0; row < rowCount; ++row) {
                for (int32_t col = 0; col < colCount_; ++col) {
                    if (HasFullRange(col)) {
                        continue;
                    }
                    const auto rowColumn = aggRows_->ColumnAt(col);
                    if (!RowContainer::IsNullAt(rows[row], rowColumn.NullByte(), rowColumn.NullMask())) {
                        UpdateStats(col, ReadStoredValue(rows[row], col));
                    }
                }
            }
        } while (rowCount > 0);
    }

    bool DecodeStoredKeyValue(const KeyType &key, const std::vector<int64_t> &rangeMins,
        int32_t col, int64_t &value) const
    {
        const auto encoded = static_cast<EncodedKey>(key);
        const auto id = (encoded >> bitOffsets_[col]) & idMasks_[col];
        if (id == 0) {
            return false;
        }
        value = static_cast<int64_t>(
            static_cast<SignedKey>(rangeMins[col]) + static_cast<SignedKey>(id) - 1);
        return true;
    }

    void AnalyzeStoredKeys(const std::vector<int64_t> &rangeMins)
    {
        hashmap.ForEachKey([&](const KeyType &key) {
            for (int32_t col = 0; col < colCount_; ++col) {
                if (HasFullRange(col)) {
                    continue;
                }
                int64_t value = 0;
                if (DecodeStoredKeyValue(key, rangeMins, col, value)) {
                    UpdateStats(col, value);
                }
            }
        });
    }

    bool ConfigureRanges()
    {
        for (int32_t col = 0; col < colCount_; ++col) {
            if (HasFullRange(col)) {
                continue;
            }
            if (!statsHasValue_[col]) {
                hasRange_[col] = false;
                continue;
            }
            const SignedKey observedMin = static_cast<SignedKey>(statsMin_[col]);
            const SignedKey observedMax = static_cast<SignedKey>(statsMax_[col]);
            const SignedKey observedWidth = observedMax - observedMin;
            const SignedKey capacity = static_cast<SignedKey>(idMasks_[col]);
            if (capacity == 0 || observedWidth >= capacity) {
                return false;
            }

            const SignedKey typeMin = static_cast<SignedKey>(TypeMin(typeIds_[col]));
            const SignedKey typeMax = static_cast<SignedKey>(TypeMax(typeIds_[col]));
            const SignedKey typeWidth = typeMax - typeMin;
            const SignedKey windowWidth = std::min(capacity - 1, typeWidth);
            const SignedKey spare = windowWidth - observedWidth;
            SignedKey newMin = observedMin - spare / 2;
            const SignedKey maxWindowMin = typeMax - windowWidth;
            newMin = std::max(typeMin, std::min(newMin, maxWindowMin));

            min_[col] = static_cast<int64_t>(newMin);
            max_[col] = static_cast<int64_t>(newMin + windowWidth);
            hasRange_[col] = true;
        }
        return true;
    }

    bool EncodeBatch(BaseVector **groupVectors, int32_t rowCount)
    {
        encodedKeysBuffer_.resize(static_cast<size_t>(rowCount));
        bool allMapped = true;
        bool initialized = false;
        for (int32_t col = 0; col < colCount_; ++col) {
            if (!hasRange_[col]) {
                if (!initialized) {
                    std::fill(encodedKeysBuffer_.begin(), encodedKeysBuffer_.end(), KeyType(0));
                    initialized = true;
                }
                for (int32_t row = 0; row < rowCount; ++row) {
                    if (!groupVectors[col]->IsNull(row)) {
                        allMapped = false;
                        break;
                    }
                }
                continue;
            }
            allMapped = activeReadPlans_[col].encode(
                groupVectors[col], rowCount, min_[col], max_[col], idMasks_[col],
                bitOffsets_[col], !initialized, encodedKeysBuffer_.data()) && allMapped;
            initialized = true;
        }
        return allMapped;
    }

    bool EncodeStoredRow(const char *row, KeyType &encoded) const
    {
        EncodedKey packed = 0;
        for (int32_t col = 0; col < colCount_; ++col) {
            const auto rowColumn = aggRows_->ColumnAt(col);
            if (RowContainer::IsNullAt(row, rowColumn.NullByte(), rowColumn.NullMask())) {
                continue;
            }
            const int64_t value = ReadStoredValue(row, col);
            if (!hasRange_[col] || value < min_[col] || value > max_[col]) {
                return false;
            }
            const auto id = static_cast<EncodedKey>(
                static_cast<SignedKey>(value) - static_cast<SignedKey>(min_[col]) + 1);
            if (id > idMasks_[col]) {
                return false;
            }
            packed |= id << bitOffsets_[col];
        }
        encoded = static_cast<KeyType>(packed);
        return true;
    }

    bool RebuildHashmap(const std::vector<int64_t> &oldMin)
    {
        if constexpr (Hashmap::StoresValues) {
            hashmap.Reset();
            RowContainerIterator iterator;
            char *rows[kRehashBatchSize];
            int32_t rowCount = 0;
            do {
                rowCount = aggRows_->ListRows(&iterator, kRehashBatchSize, rows);
                for (int32_t row = 0; row < rowCount; ++row) {
                    KeyType key;
                    if (!EncodeStoredRow(rows[row], key)) {
                        return false;
                    }
                    auto value = reinterpret_cast<ValueType>(rows[row] + aggRows_->AggStateOffset());
                    hashmap.InsertExisting(key, value);
                }
            } while (rowCount > 0);
            return true;
        } else {
            std::vector<KeyType> rebuiltKeys;
            rebuiltKeys.reserve(hashmap.GetElementsSize());
            bool valid = true;
            hashmap.ForEachKey([&](const KeyType &oldKey) {
                EncodedKey packed = 0;
                for (int32_t col = 0; col < colCount_; ++col) {
                    int64_t value = 0;
                    if (!DecodeStoredKeyValue(oldKey, oldMin, col, value)) {
                        continue;
                    }
                    if (!hasRange_[col] || value < min_[col] || value > max_[col]) {
                        valid = false;
                        return;
                    }
                    const auto id = static_cast<EncodedKey>(
                        static_cast<SignedKey>(value) - static_cast<SignedKey>(min_[col]) + 1);
                    if (id > idMasks_[col]) {
                        valid = false;
                        return;
                    }
                    packed |= id << bitOffsets_[col];
                }
                rebuiltKeys.push_back(static_cast<KeyType>(packed));
            });
            if (!valid) {
                return false;
            }
            hashmap.Reset();
            if (!rebuiltKeys.empty()) {
                hashmap.EmplaceBatch(rebuiltKeys.data(), static_cast<int32_t>(rebuiltKeys.size()));
            }
            return true;
        }
    }

    void StoreOriginalKeys(BaseVector **groupVectors, int32_t rowIdx, char *row)
    {
        for (int32_t col = 0; col < colCount_; ++col) {
            const auto rowColumn = aggRows_->ColumnAt(col);
            if (groupVectors[col]->IsNull(rowIdx)) {
                RowContainer::SetNullAt(row, rowColumn.NullByte(), rowColumn.NullMask());
                continue;
            }
            RowContainer::ClearNullAt(row, rowColumn.NullByte(), rowColumn.NullMask());
            StoreFixedValue(row, rowColumn.Offset(), typeIds_[col],
                activeReadPlans_[col].read(groupVectors[col], rowIdx));
        }
    }

    static void StoreFixedValue(char *row, int32_t offset, type::DataTypeId typeId, int64_t value)
    {
        switch (typeId) {
            case type::OMNI_BYTE:
                RowContainer::StoreValue<int8_t>(row, offset, static_cast<int8_t>(value));
                break;
            case type::OMNI_SHORT:
                RowContainer::StoreValue<int16_t>(row, offset, static_cast<int16_t>(value));
                break;
            case type::OMNI_INT:
            case type::OMNI_DATE32:
            case type::OMNI_TIME32:
                RowContainer::StoreValue<int32_t>(row, offset, static_cast<int32_t>(value));
                break;
            default:
                RowContainer::StoreValue<int64_t>(row, offset, value);
                break;
        }
    }

    int64_t ReadStoredValue(const char *row, int32_t col) const
    {
        const auto offset = aggRows_->ColumnAt(col).Offset();
        switch (typeIds_[col]) {
            case type::OMNI_BYTE:
                return RowContainer::ReadValue<int8_t>(row, offset);
            case type::OMNI_SHORT:
                return RowContainer::ReadValue<int16_t>(row, offset);
            case type::OMNI_INT:
            case type::OMNI_DATE32:
            case type::OMNI_TIME32:
                return RowContainer::ReadValue<int32_t>(row, offset);
            default:
                return RowContainer::ReadValue<int64_t>(row, offset);
        }
    }

    template <type::DataTypeId TypeId>
    static void SetNormalizedFixedValueTyped(BaseVector *vector, int32_t rowIdx, int64_t value)
    {
        using NativeType = typename NativeAndVectorType<TypeId>::type;
        static_cast<Vector<NativeType> *>(vector)->SetValue(rowIdx, static_cast<NativeType>(value));
    }

    static void SetNormalizedFixedValue(BaseVector *vector, int32_t rowIdx,
        type::DataTypeId typeId, int64_t value)
    {
        vector->SetNotNull(rowIdx);
        switch (typeId) {
            case type::OMNI_BYTE:
                SetNormalizedFixedValueTyped<type::OMNI_BYTE>(vector, rowIdx, value);
                break;
            case type::OMNI_SHORT:
                SetNormalizedFixedValueTyped<type::OMNI_SHORT>(vector, rowIdx, value);
                break;
            case type::OMNI_INT:
                SetNormalizedFixedValueTyped<type::OMNI_INT>(vector, rowIdx, value);
                break;
            case type::OMNI_DATE32:
                SetNormalizedFixedValueTyped<type::OMNI_DATE32>(vector, rowIdx, value);
                break;
            case type::OMNI_TIME32:
                SetNormalizedFixedValueTyped<type::OMNI_TIME32>(vector, rowIdx, value);
                break;
            case type::OMNI_LONG:
                SetNormalizedFixedValueTyped<type::OMNI_LONG>(vector, rowIdx, value);
                break;
            case type::OMNI_DATE64:
                SetNormalizedFixedValueTyped<type::OMNI_DATE64>(vector, rowIdx, value);
                break;
            case type::OMNI_TIME64:
                SetNormalizedFixedValueTyped<type::OMNI_TIME64>(vector, rowIdx, value);
                break;
            case type::OMNI_TIMESTAMP:
                SetNormalizedFixedValueTyped<type::OMNI_TIMESTAMP>(vector, rowIdx, value);
                break;
            case type::OMNI_DECIMAL64:
                SetNormalizedFixedValueTyped<type::OMNI_DECIMAL64>(vector, rowIdx, value);
                break;
            default:
                break;
        }
    }

    static uint8_t RequiredFullBits(type::DataTypeId typeId)
    {
        switch (typeId) {
            case type::OMNI_BYTE:
                return 9;
            case type::OMNI_SHORT:
                return 17;
            case type::OMNI_INT:
            case type::OMNI_DATE32:
            case type::OMNI_TIME32:
                return 33;
            case type::OMNI_LONG:
            case type::OMNI_DATE64:
            case type::OMNI_TIME64:
            case type::OMNI_TIMESTAMP:
            case type::OMNI_DECIMAL64:
                return 65;
            default:
                return 0;
        }
    }

    static uint8_t MinUsefulBits(type::DataTypeId typeId)
    {
        const auto fullBits = RequiredFullBits(typeId);
        if (fullBits <= 17) {
            return fullBits;
        }
        return static_cast<uint8_t>((static_cast<uint32_t>(fullBits) * 60 + 99) / 100);
    }

    static EncodedKey MaskForBits(uint8_t bits)
    {
        return bits >= 128 ? ~static_cast<EncodedKey>(0) :
            (static_cast<EncodedKey>(1) << bits) - 1;
    }

    static bool BuildBitLayout(const std::vector<type::DataTypeId> &typeIds,
        std::vector<uint8_t> *fullBits, std::vector<uint8_t> *idBits,
        std::vector<uint8_t> *bitOffsets, std::vector<EncodedKey> *idMasks)
    {
        if (typeIds.size() <= 1) {
            return false;
        }
        std::vector<uint8_t> localFullBits(typeIds.size());
        std::vector<uint8_t> localIdBits(typeIds.size());
        uint32_t fullTotal = 0;
        uint32_t usedBits = 0;
        for (size_t col = 0; col < typeIds.size(); ++col) {
            localFullBits[col] = RequiredFullBits(typeIds[col]);
            localIdBits[col] = MinUsefulBits(typeIds[col]);
            if (localFullBits[col] == 0 || localIdBits[col] == 0) {
                return false;
            }
            fullTotal += localFullBits[col];
            usedBits += localIdBits[col];
        }
        if (fullTotal <= kTotalKeyBits) {
            localIdBits = localFullBits;
            usedBits = fullTotal;
        } else if (usedBits > kTotalKeyBits) {
            return false;
        }

        while (usedBits < kTotalKeyBits) {
            bool assigned = false;
            for (size_t col = 0; col < typeIds.size() && usedBits < kTotalKeyBits; ++col) {
                if (localIdBits[col] < localFullBits[col]) {
                    ++localIdBits[col];
                    ++usedBits;
                    assigned = true;
                }
            }
            if (!assigned) {
                break;
            }
        }

        std::vector<uint8_t> localOffsets(typeIds.size());
        std::vector<EncodedKey> localMasks(typeIds.size());
        uint32_t offset = 0;
        for (size_t col = 0; col < typeIds.size(); ++col) {
            if (offset + localIdBits[col] > kTotalKeyBits) {
                return false;
            }
            localOffsets[col] = static_cast<uint8_t>(offset);
            localMasks[col] = MaskForBits(localIdBits[col]);
            offset += localIdBits[col];
        }
        if (fullBits != nullptr) {
            *fullBits = std::move(localFullBits);
        }
        if (idBits != nullptr) {
            *idBits = std::move(localIdBits);
        }
        if (bitOffsets != nullptr) {
            *bitOffsets = std::move(localOffsets);
        }
        if (idMasks != nullptr) {
            *idMasks = std::move(localMasks);
        }
        return true;
    }

    static int64_t TypeMin(type::DataTypeId typeId)
    {
        switch (typeId) {
            case type::OMNI_BYTE:
                return std::numeric_limits<int8_t>::min();
            case type::OMNI_SHORT:
                return std::numeric_limits<int16_t>::min();
            case type::OMNI_INT:
            case type::OMNI_DATE32:
            case type::OMNI_TIME32:
                return std::numeric_limits<int32_t>::min();
            default:
                return std::numeric_limits<int64_t>::min();
        }
    }

    static int64_t TypeMax(type::DataTypeId typeId)
    {
        switch (typeId) {
            case type::OMNI_BYTE:
                return std::numeric_limits<int8_t>::max();
            case type::OMNI_SHORT:
                return std::numeric_limits<int16_t>::max();
            case type::OMNI_INT:
            case type::OMNI_DATE32:
            case type::OMNI_TIME32:
                return std::numeric_limits<int32_t>::max();
            default:
                return std::numeric_limits<int64_t>::max();
        }
    }

    Hashmap hashmap;
    std::unique_ptr<RowContainer> aggRows_;
    int32_t colCount_ = 0;
    int32_t aggStateSize_ = 0;
    bool rangeInitialized_ = false;
    std::vector<type::DataTypeId> typeIds_;
    std::vector<uint8_t> fullBits_;
    std::vector<uint8_t> idBits_;
    std::vector<uint8_t> bitOffsets_;
    std::vector<EncodedKey> idMasks_;
    std::vector<bool> hasRange_;
    std::vector<int64_t> min_;
    std::vector<int64_t> max_;
    std::vector<bool> statsHasValue_;
    std::vector<int64_t> statsMin_;
    std::vector<int64_t> statsMax_;
    std::vector<ColumnReadPlan> activeReadPlans_;
    std::vector<KeyType> encodedKeysBuffer_;
};

template <typename Hashset>
class NormalizeKeyWithoutAggHandler : public NormalizeKeyHandler<Hashset> {
    static_assert(!Hashset::StoresValues,
        "NormalizeKeyWithoutAggHandler requires a key-only container");

public:
    bool Init(const std::vector<type::DataTypeId> &groupByTypeIds,
        const std::vector<int32_t> &keyTypeSizes, int32_t aggStateSize,
        mem::SimpleArenaAllocator &pool)
    {
        return aggStateSize == 0 && NormalizeKeyHandler<Hashset>::Init(
            groupByTypeIds, keyTypeSizes, aggStateSize, pool);
    }
};
}
}
#endif // OMNI_RUNTIME_COLUMN_MARSHALLER_H
