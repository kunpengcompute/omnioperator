/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: TAPER join handler — stubs for speculative probe framework
 */

#pragma once

#include <memory>
#include <cstdint>
#include <vector>

#include "operator/hashmap/taper_hashtable.h"
#include "operator/hashmap/row_container.h"
#include "operator/hashmap/vector_marshaller.h"
#include "memory/simple_arena_allocator.h"
#include "vector/decoded_vector.h"
#include "type/data_types.h"
#include "type/decimal128.h"

namespace omniruntime {
namespace op {

namespace {
static constexpr uint32_t ROW_PTR_SIZE = 6;

static void SetRowPtr(char *buf, char *ptr) {
    uint64_t val = reinterpret_cast<uint64_t>(ptr);
    memcpy(buf, &val, ROW_PTR_SIZE);
}

static char *GetRowPtr(const char *buf) {
    uint64_t val = 0;
    memcpy(&val, buf, ROW_PTR_SIZE);
    return reinterpret_cast<char *>(val);
}

} // namespace

// Fixed-width-key TAPER join handler.
// Maintains a TaperFlatHashTable + RowContainer pair for build data.
template <typename KeyType, bool NeedVisited>
class TaperJoinFixedHandler {
public:
    using Key = KeyType;
    using HashTable = TaperFlatHashTable<KeyType, false>;

    // Size in bytes of the per-row payload stored past key + null data.
    // Layout: [next: char*][visited: uint8_t]
    static constexpr int32_t kPayloadSize = sizeof(char*) + 1;

    TaperJoinFixedHandler() = default;
    TaperJoinFixedHandler(mem::SimpleArenaAllocator& pool, uint8_t /*initDegree*/)
        : arena_(&pool),
          table_(std::make_unique<HashTable>(pool, sizeof(KeyType), ROW_PTR_SIZE)) {}
    ~TaperJoinFixedHandler() = default;

    void InitRowContainer(const std::vector<int32_t>& keyTypeSizes,
                          const std::vector<bool>& /*isVariableLen*/,
                          const std::vector<int32_t>& typeIds,
                          const std::vector<int32_t>& /*varcharCols*/) {
        numCols_ = static_cast<int32_t>(keyTypeSizes.size());
        typeIds_ = typeIds;
        rows_ = std::make_unique<RowContainer>(keyTypeSizes, numCols_, kPayloadSize, *arena_);
    }

    void ReserveTable(size_t numRows) {
        if (!table_) {
            throw omniruntime::exception::OmniException("RUNTIME_ERROR", "TaperJoinFixedHandler HashTable is null!");
        }
        table_->Reserve(numRows);
    }

    /// Append a decoded row to the RowContainer, storing key column data and payload.
    char* AppendRow(omniruntime::vec::DecodedVector* decodedCols, int32_t rowIdx, uint32_t vecBatchIdx) {
        char* row = rows_->NewRow();

        // Store all columns (keys + output + filter)
        for (int32_t c = 0; c < numCols_; ++c) {
            int32_t typeId = typeIds_[c];
            auto col = rows_->ColumnAt(c);
            auto offset = col.Offset();
            bool isNull = decodedCols[c].IsNull(rowIdx);
            if (isNull) {
                RowContainer::SetNullAt(row, col.NullByte(), col.NullMask());
                continue;
            }
            switch (typeId) {
                case type::OMNI_BYTE:
                case type::OMNI_BOOLEAN:
                    RowContainer::StoreValue<int8_t>(row, offset, decodedCols[c].GetValue<int8_t>(rowIdx));
                    break;
                case type::OMNI_SHORT:
                    RowContainer::StoreValue<int16_t>(row, offset, decodedCols[c].GetValue<int16_t>(rowIdx));
                    break;
                case type::OMNI_INT:
                case type::OMNI_DATE32:
                case type::OMNI_TIME32:
                case type::OMNI_FLOAT:
                    RowContainer::StoreValue<int32_t>(row, offset, decodedCols[c].GetValue<int32_t>(rowIdx));
                    break;
                case type::OMNI_LONG:
                case type::OMNI_TIMESTAMP:
                case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE:
                case type::OMNI_TIME64:
                case type::OMNI_DATE64:
                    RowContainer::StoreValue<int64_t>(row, offset, decodedCols[c].GetValue<int64_t>(rowIdx));
                    break;
                case type::OMNI_DECIMAL128:
                    RowContainer::StoreValue<Decimal128>(row, offset, decodedCols[c].GetValue<Decimal128>(rowIdx));
                    break;
                case type::OMNI_VARCHAR:
                case type::OMNI_CHAR:
                case type::OMNI_VARBINARY: {
                    auto* vec = decodedCols[c].Base();
                    auto enc = vec->GetEncoding();
                    std::string_view sv;
                    if (enc == OMNI_ENCODING_CONST) {
                        sv = static_cast<ConstVector<std::string_view>*>(vec)->GetConstValue();
                    } else if (enc == OMNI_DICTIONARY) {
                        sv = decodedCols[c].GetValue<std::string_view>(rowIdx);
                    } else {
                        sv = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec)
                                 ->GetValue(rowIdx);
                    }
                    // Serialize: [1B lenSize] [1/2/4B length] [data]
                    uint32_t slen = static_cast<uint32_t>(sv.size());
                    uint8_t lenSize = 1;
                    if (slen > 255) lenSize = 2;
                    if (slen > 65535) lenSize = 4;
                    int32_t totalSize = 1 + lenSize + static_cast<int32_t>(slen);
                    auto* buf = reinterpret_cast<char*>(arena_->Allocate(totalSize));
                    buf[0] = static_cast<char>(lenSize);
                    memcpy(buf + 1, &slen, lenSize);
                    memcpy(buf + 1 + lenSize, sv.data(), slen);
                    RowContainer::StoreValue<char*>(row, offset, buf);
                    break;
                }
                case type::OMNI_ARRAY:
                case type::OMNI_MAP:
                case type::OMNI_ROW: {
                    auto serializer = vectorSerializerCenter[static_cast<size_t>(typeId)];
                    type::StringRef ref;
                    serializer(decodedCols[c].Base(), rowIdx, *arena_, ref);
                    RowContainer::StoreValue<char*>(row, offset, const_cast<char*>(ref.data));
                    RowContainer::StoreValue<size_t>(row, offset + sizeof(char*), ref.size);
                    break;
                }
            }
        }

        // Set payload: next=null, visited=0
        auto payloadOff = rows_->PayloadOffset();
        *reinterpret_cast<char**>(row + payloadOff) = nullptr;
        *reinterpret_cast<uint8_t*>(row + payloadOff + sizeof(char*)) = 0;

        return row;
    }

    RowContainer* Rows() { return rows_.get(); }
    const RowContainer* Rows() const { return rows_.get(); }

    void ProbeBatch(
        const Key* keys,
        int32_t numKeys,
        omniruntime::vec::BaseVector** /*probeHashColumns*/,
        int32_t /*probeHashColCount*/,
        const int32_t* /*probePositions*/,
        char** chainHeads,
        const char* isNulls) {
        if (UNLIKELY(!table_)) return;

        table_->ProbeBatch(keys, static_cast<uint32_t>(numKeys),
            [isNulls](uint32_t ki) { return isNulls[ki] != 0; },
            [](uint32_t, char*) {},
            [&](uint32_t ki, char* data, bool initFlag) {
                if (!initFlag) { chainHeads[ki] = GetRowPtr(data); }
            });
    }

    void EmplaceBatch(const Key* keys, char** rows, int32_t numRows, const bool* isNulls) {
        auto payloadOff = rows_->PayloadOffset();
        table_->EmplaceBatch(
            keys,
            static_cast<uint32_t>(numRows),
            [isNulls](uint32_t rowIdx) {
                return isNulls[rowIdx];
            },
            [rows](uint32_t rowIdx, char* buf) { SetRowPtr(buf, rows[rowIdx]); },
            [rows, payloadOff](uint32_t rowIdx, char* buf, bool initFlag) {
                if (!initFlag) {
                    char* oldHead = GetRowPtr(buf);
                    SetRowPtr(buf, rows[rowIdx]);
                    *reinterpret_cast<char**>(rows[rowIdx] + payloadOff) = oldHead;
                }
            });
    }

    template <typename F>
    void ForEachValue(F&& /*fn*/) {} //TODO

    uint32_t Size() const { return 0; } //TODO
    uint64_t AllocatedBytes() const { return 0; } //TODO

private:
    int32_t numCols_ = 0;
    mem::SimpleArenaAllocator* arena_ = nullptr;
    std::vector<int32_t> typeIds_;
    std::unique_ptr<HashTable> table_;
    std::unique_ptr<RowContainer> rows_;

};

// Serialized-key TAPER join handler for multi-column or non-integer keys.
// Uses int64_t hash as the hash-table key; row pointers are packed into
// 6 bytes (lower 48 bits) for space efficiency.
class TaperJoinSerializedHandler {
public:
    using HashTable = TaperFlatHashTable<int64_t, true>;

    TaperJoinSerializedHandler() = default;
    TaperJoinSerializedHandler(mem::SimpleArenaAllocator& pool)
        : arena_(&pool),
          table_(std::make_unique<HashTable>(pool, sizeof(int64_t), ROW_PTR_SIZE)) {}
    ~TaperJoinSerializedHandler() = default;

    // --- RowContainer ------------------------------------------------------

    void InitRowContainer(const std::vector<int32_t>& keyTypeSizes,
                          const std::vector<bool>& /*isVariableLen*/,
                          const std::vector<int32_t>& typeIds,
                          const std::vector<int32_t>& /*varcharCols*/,
                          const std::vector<int32_t>& keyColIndices = {}) {
        numCols_ = static_cast<int32_t>(keyTypeSizes.size());
        typeIds_ = typeIds;
        keyColIndices_ = keyColIndices;
        rows_ = std::make_unique<RowContainer>(keyTypeSizes, numCols_, kPayloadSize, *arena_);
    }

    void ReserveTable(size_t numRows) {
        if (table_) table_->Reserve(numRows);
    }

    char* AppendRow(omniruntime::vec::DecodedVector* decodedCols, int32_t rowIdx, uint32_t /*vecBatchIdx*/) {
        char* row = rows_->NewRow();

        for (int32_t c = 0; c < numCols_; ++c) {
            int32_t typeId = typeIds_[c];
            auto col = rows_->ColumnAt(c);
            auto offset = col.Offset();
            bool isNull = decodedCols[c].IsNull(rowIdx);
            if (isNull) {
                RowContainer::SetNullAt(row, col.NullByte(), col.NullMask());
                continue;
            }
            switch (typeId) {
                case type::OMNI_BYTE:   case type::OMNI_BOOLEAN:
                    RowContainer::StoreValue<int8_t>(row, offset, decodedCols[c].GetValue<int8_t>(rowIdx)); break;
                case type::OMNI_SHORT:
                    RowContainer::StoreValue<int16_t>(row, offset, decodedCols[c].GetValue<int16_t>(rowIdx)); break;
                case type::OMNI_INT:    case type::OMNI_DATE32: case type::OMNI_TIME32: case type::OMNI_FLOAT:
                    RowContainer::StoreValue<int32_t>(row, offset, decodedCols[c].GetValue<int32_t>(rowIdx)); break;
                case type::OMNI_LONG:   case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE: case type::OMNI_TIME64: case type::OMNI_DATE64:
                    RowContainer::StoreValue<int64_t>(row, offset, decodedCols[c].GetValue<int64_t>(rowIdx)); break;
                case type::OMNI_DECIMAL128: 
                    RowContainer::StoreValue<Decimal128>(row, offset, decodedCols[c].GetValue<Decimal128>(rowIdx)); break;
                case type::OMNI_VARCHAR:
                case type::OMNI_CHAR:
                case type::OMNI_VARBINARY: {
                    auto* vec = decodedCols[c].Base();
                    auto enc = vec->GetEncoding();
                    std::string_view sv;
                    if (enc == OMNI_ENCODING_CONST) {
                        sv = static_cast<ConstVector<std::string_view>*>(vec)->GetConstValue();
                    } else if (enc == OMNI_DICTIONARY) {
                        sv = decodedCols[c].GetValue<std::string_view>(rowIdx);
                    } else {
                        sv = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec)
                                 ->GetValue(rowIdx);
                    }
                    uint32_t slen = static_cast<uint32_t>(sv.size());
                    uint8_t lenSize = 1;
                    if (slen > 255) lenSize = 2;
                    if (slen > 65535) lenSize = 4;
                    int32_t totalSize = 1 + lenSize + static_cast<int32_t>(slen);
                    auto* buf = reinterpret_cast<char*>(arena_->Allocate(totalSize));
                    buf[0] = static_cast<char>(lenSize);
                    memcpy(buf + 1, &slen, lenSize);
                    memcpy(buf + 1 + lenSize, sv.data(), slen);
                    RowContainer::StoreValue<char*>(row, offset, buf);
                    break;
                }
                case type::OMNI_ARRAY:
                case type::OMNI_MAP:
                case type::OMNI_ROW: {
                    auto serializer = vectorSerializerCenter[static_cast<size_t>(typeId)];
                    type::StringRef ref;
                    serializer(decodedCols[c].Base(), rowIdx, *arena_, ref);
                    RowContainer::StoreValue<char*>(row, offset, const_cast<char*>(ref.data));
                    RowContainer::StoreValue<size_t>(row, offset + sizeof(char*), ref.size);
                    break;
                }
            }
        }

        auto payloadOff = rows_->PayloadOffset();
        *reinterpret_cast<char**>(row + payloadOff) = nullptr;
        *reinterpret_cast<uint8_t*>(row + payloadOff + sizeof(char*)) = 0;
        return row;
    }

    RowContainer* Rows() { return rows_.get(); }
    const RowContainer* Rows() const { return rows_.get(); }

    static uint8_t PackedBitWidth(int32_t typeId) {
        switch (typeId) {
            case type::OMNI_BYTE:   case type::OMNI_BOOLEAN: return 8;
            case type::OMNI_SHORT:  return 16;
            case type::OMNI_INT:    case type::OMNI_DATE32: case type::OMNI_TIME32:
            case type::OMNI_FLOAT:  return 32;
            case type::OMNI_LONG:   case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
            case type::OMNI_DOUBLE: case type::OMNI_DATE64: case type::OMNI_TIME64:
                return 64;
            case type::OMNI_DECIMAL128: return 128;
            default: return 0;
        }
    }

    bool CompareKeys(const char* row1, const char* row2) const {
        for (int32_t colIdx : keyColIndices_) {
            auto col = rows_->ColumnAt(colIdx);
            auto off = col.Offset();
            auto nullByte = col.NullByte();
            auto nullMask = col.NullMask();
            bool null1 = RowContainer::IsNullAt(row1, nullByte, nullMask);
            bool null2 = RowContainer::IsNullAt(row2, nullByte, nullMask);
            if (null1 != null2) return false;
            if (null1) continue;
            auto typeId = typeIds_[colIdx];
            switch (typeId) {
                case type::OMNI_BYTE: case type::OMNI_BOOLEAN:
                    if (RowContainer::ReadValue<int8_t>(row1, off) != RowContainer::ReadValue<int8_t>(row2, off)) return false;
                    break;
                case type::OMNI_SHORT:
                    if (RowContainer::ReadValue<int16_t>(row1, off) != RowContainer::ReadValue<int16_t>(row2, off)) return false;
                    break;
                case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_TIME32: case type::OMNI_FLOAT:
                    if (RowContainer::ReadValue<int32_t>(row1, off) != RowContainer::ReadValue<int32_t>(row2, off)) return false;
                    break;
                case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE: case type::OMNI_DATE64: case type::OMNI_TIME64:
                    if (RowContainer::ReadValue<int64_t>(row1, off) != RowContainer::ReadValue<int64_t>(row2, off)) return false;
                    break;
                case type::OMNI_DECIMAL128:
                    if (RowContainer::ReadValue<Decimal128>(row1, off) != RowContainer::ReadValue<Decimal128>(row2, off)) return false;
                    break;
                case type::OMNI_VARCHAR: case type::OMNI_CHAR: case type::OMNI_VARBINARY: {
                    auto* p1 = RowContainer::ReadValue<char*>(const_cast<char*>(row1), off);
                    auto* p2 = RowContainer::ReadValue<char*>(const_cast<char*>(row2), off);
                    if (p1 == p2) break;
                    if (p1 == nullptr || p2 == nullptr) return false;
                    uint8_t ls1 = static_cast<uint8_t>(p1[0]);
                    uint8_t ls2 = static_cast<uint8_t>(p2[0]);
                    uint32_t len1 = 0, len2 = 0;
                    memcpy(&len1, p1 + 1, ls1);
                    memcpy(&len2, p2 + 1, ls2);
                    if (len1 != len2) return false;
                    if (memcmp(p1 + 1 + ls1, p2 + 1 + ls2, len1) != 0) return false;
                    break;
                }
                case type::OMNI_ARRAY:
                case type::OMNI_MAP:
                case type::OMNI_ROW: {
                    auto* p1 = RowContainer::ReadValue<char*>(const_cast<char*>(row1), off);
                    auto* p2 = RowContainer::ReadValue<char*>(const_cast<char*>(row2), off);
                    if (p1 == p2) break;
                    if (p1 == nullptr || p2 == nullptr) return false;
                    size_t len1 = RowContainer::ReadValue<size_t>(const_cast<char*>(row1), off + sizeof(char*));
                    size_t len2 = RowContainer::ReadValue<size_t>(const_cast<char*>(row2), off + sizeof(char*));
                    if (len1 != len2) return false;
                    if (memcmp(p1, p2, len1) != 0) return false;
                    break;
                }
                default:
                    return false;
            }
        }
        return true;
    }

    bool CompareRowWithProbe(const char* row,
                             omniruntime::vec::BaseVector** probeHashColumns,
                             int32_t probeHashColCount,
                             int32_t probePosition) const {
        for (int32_t k = 0; k < probeHashColCount; ++k) {
            int32_t colIdx = keyColIndices_[k];
            auto col = rows_->ColumnAt(colIdx);
            bool rowNull = RowContainer::IsNullAt(const_cast<char*>(row), col.NullByte(), col.NullMask());
            auto* vec = probeHashColumns[k];
            vec::DecodedVector dv;
            dv.Decode(vec, vec->GetSize());
            bool probeNull = dv.IsNull(probePosition);
            if (rowNull != probeNull) return false;
            if (rowNull) continue;
            auto typeId = vec->GetTypeId();
            auto w = PackedBitWidth(static_cast<int32_t>(typeId));
            int32_t colOff = col.Offset();
            switch (w) {
                case 8:
                    if (RowContainer::ReadValue<int8_t>(const_cast<char*>(row), colOff) != dv.GetValue<int8_t>(probePosition)) return false;
                    break;
                case 16:
                    if (RowContainer::ReadValue<int16_t>(const_cast<char*>(row), colOff) != dv.GetValue<int16_t>(probePosition)) return false;
                    break;
                case 32:
                    if (RowContainer::ReadValue<int32_t>(const_cast<char*>(row), colOff) != dv.GetValue<int32_t>(probePosition)) return false;
                    break;
                case 64:
                    if (RowContainer::ReadValue<int64_t>(const_cast<char*>(row), colOff) != dv.GetValue<int64_t>(probePosition)) return false;
                    break;
                case 128:
                    if (RowContainer::ReadValue<Decimal128>(const_cast<char*>(row), colOff)
                        != dv.GetValue<Decimal128>(probePosition)) return false;
                    break;
                default:
                    if (w == 0) {
                        if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR ||
                            typeId == type::OMNI_VARBINARY) {
                            auto* rowDataPtr = RowContainer::ReadValue<char*>(const_cast<char*>(row), colOff);
                            if (rowDataPtr == nullptr) return false;
                            auto* baseVec = dv.Base();
                            auto enc = baseVec->GetEncoding();
                            std::string_view sv;
                            if (enc == OMNI_ENCODING_CONST) {
                                sv = static_cast<ConstVector<std::string_view>*>(baseVec)->GetConstValue();
                            } else if (enc == OMNI_DICTIONARY) {
                                sv = dv.GetValue<std::string_view>(probePosition);
                            } else {
                                sv = static_cast<Vector<LargeStringContainer<std::string_view>>*>(baseVec)
                                         ->GetValue(probePosition);
                            }
                            uint8_t lenSize = static_cast<uint8_t>(rowDataPtr[0]);
                            uint32_t strLen = 0;
                            memcpy(&strLen, rowDataPtr + 1, lenSize);
                            if (strLen != sv.size()) return false;
                            if (memcmp(rowDataPtr + 1 + lenSize, sv.data(), strLen) != 0) return false;
                        } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_MAP || typeId == type::OMNI_ROW) {
                            auto* storedPtr = RowContainer::ReadValue<char*>(const_cast<char*>(row), colOff);
                            if (storedPtr == nullptr) return false;
                            uint8_t* bytePtr = reinterpret_cast<uint8_t*>(const_cast<char*>(storedPtr));
                            auto comparator = vectorComparatorCenter[static_cast<size_t>(typeId)];
                            auto* probeVec = dv.Base();
                            if (!comparator(*probeVec, probePosition, bytePtr)) return false;
                        } else {
                            return false;
                        }
                    } else {
                        return false;
                    }
                    break;
            }
        }
        return true;
    }

    // --- ProbeBatch --------------------------------------
    // 第 1 轮: ProbeBatch(hash-only) → 记录 workingUpdateIndices_/ChunkData_
    // 第 2 轮: CompareRowWithProbe → 匹配填 chainHead / 不匹配紧缩等待第 3 轮
    // 第 3 轮: Probe
    void ProbeBatch(
        const int64_t* keys,
        int32_t numKeys,
        omniruntime::vec::BaseVector** probeHashColumns,
        int32_t probeHashColCount,
        const int32_t* probePositions,
        char** chainHeads,
        const char* isNulls) {
        if (UNLIKELY(!table_)) return;

        workingUpdateIndices_.resize(numKeys);
        workingUpdateChunkData_.resize(numKeys);
        workingUpdateCount_ = 0;

        // 第 1 轮：ProbeBatch，只按 hash tag 匹配，不做 key 比较
        // fInit:  空 slot → key 不在表中 → chainHeads 已为 nullptr，无操作
        // fUpdate: 命中 → 记录 index + buf 等待第 2 轮比较
        table_->ProbeBatch(keys, static_cast<uint32_t>(numKeys),
            [isNulls](uint32_t ki) { return isNulls[ki] != 0; },
            [](uint32_t, char*) { /* empty slot → miss */ },
            [this](uint32_t ki, char* data, bool initFlag) {
                if(!initFlag){
                    workingUpdateIndices_[workingUpdateCount_] = ki;
                    workingUpdateChunkData_[workingUpdateCount_] = data;
                    ++workingUpdateCount_;
                }
            });

        // 第 2 轮：对命中行比较实际 key
        //   key 匹配 → 填 chainHead
        //   key 不匹配 → 紧缩到 workingUpdateIndices_ 前部，留给第 3 轮
        int32_t remainCount = 0;
        for (int32_t i = 0; i < workingUpdateCount_; ++i) {
            uint32_t ki = workingUpdateIndices_[i];
            int32_t pos = probePositions[ki];
            auto* existingRow = GetRowPtr(workingUpdateChunkData_[i]);
            if (CompareRowWithProbe(existingRow, probeHashColumns, probeHashColCount, pos)) {
                chainHeads[ki] = existingRow;
            } else {
                workingUpdateIndices_[remainCount] = ki;
                ++remainCount;
            }
        }
        workingUpdateCount_ = remainCount;

        // 第 3 轮：对应 Emplace — Find(key, fKeyCmp, fInit, fUpdate)
        // 线性探测同 hash 的其他 slot
        for (int32_t i = 0; i < workingUpdateCount_; ++i) {
            uint32_t ki = workingUpdateIndices_[i];
            table_->Probe(keys[ki],
                [&](const int64_t&, TaperHashTableChunk& chunk, uint8_t slot) {
                    auto* row = GetRowPtr(table_->GetChunkValue(chunk, slot).buf);
                    return CompareRowWithProbe(row, probeHashColumns, probeHashColCount, probePositions[ki]);
                },
                [&](char*) { chainHeads[ki] = nullptr; },
                [&](char* data, bool initFlag) {
                    if (!initFlag) chainHeads[ki] = GetRowPtr(data);
                });
        }
        workingUpdateCount_ = 0;
    }

    void EmplaceBatch(const int64_t* keys, char** rows, int32_t numRows, const bool* isNulls = nullptr) {
        auto payloadOff = rows_->PayloadOffset();

        workingUpdateIndices_.resize(numRows);
        workingUpdateChunkData_.resize(numRows);
        workingUpdateCount_ = 0;

        table_->EmplaceBatch(
            keys,
            static_cast<uint32_t>(numRows),
            [isNulls](uint32_t rowIdx) {
                return isNulls != nullptr && isNulls[rowIdx];
            },
            [rows](uint32_t rowIdx, char* buf) { SetRowPtr(buf, rows[rowIdx]); },
            [this](uint32_t rowIdx, char* buf, bool initFlag) {
                if (!initFlag) {
                    workingUpdateIndices_[workingUpdateCount_] = rowIdx;
                    workingUpdateChunkData_[workingUpdateCount_] = buf;
                    ++workingUpdateCount_;
                }
            });

        // 第二轮：比较实际 key，同 key 当场链入冲突链
        int32_t remainCount = 0;
        for (int32_t i = 0; i < workingUpdateCount_; ++i) {
            int32_t rowIdx = workingUpdateIndices_[i];
            auto* existingRow = GetRowPtr(workingUpdateChunkData_[i]);
            if (CompareKeys(existingRow, rows[rowIdx])) {
                // key 相等 → prepend 到冲突链
                char* oldHead = existingRow;
                SetRowPtr(workingUpdateChunkData_[i], rows[rowIdx]);
                *reinterpret_cast<char**>(rows[rowIdx] + payloadOff) = oldHead;
            } else {
                // key 不等 → 留给第三轮
                workingUpdateIndices_[remainCount] = rowIdx;
                ++remainCount;
            }
        }
        workingUpdateCount_ = remainCount;

        // 第三轮：对剩余的真 hash 冲突行重新安置
        for (int32_t i = 0; i < workingUpdateCount_; ++i) {
            int32_t rowIdx = workingUpdateIndices_[i];
            table_->Emplace(
                keys[rowIdx],
                [&](const int64_t&, TaperHashTableChunk& chunk, uint8_t slot) {
                    auto* existingRow = GetRowPtr(table_->GetChunkValue(chunk, slot).buf);
                    return CompareKeys(existingRow, rows[rowIdx]);
                },
                [&](char* data) { SetRowPtr(data, rows[rowIdx]); },
                [&](char* data, bool initFlag) {
                    if (!initFlag) {
                        char* oldHead = GetRowPtr(data);
                        SetRowPtr(data, rows[rowIdx]);
                        *reinterpret_cast<char**>(rows[rowIdx] + payloadOff) = oldHead;
                    }
                });
        }
        workingUpdateCount_ = 0;
    }

    template <typename F>
    void ForEachValue(F&& /*fn*/) {} //TODO

    uint32_t Size() const { return 0; } //TODO
    uint64_t AllocatedBytes() const { return 0; } //TODO

private:
    static constexpr int32_t kPayloadSize = sizeof(char*) + 1;

    int32_t numCols_ = 0;
    mem::SimpleArenaAllocator* arena_ = nullptr;
    std::vector<int32_t> typeIds_;
    std::vector<int32_t> keyColIndices_;
    std::unique_ptr<HashTable> table_;
    std::unique_ptr<RowContainer> rows_;
    std::vector<int32_t> workingUpdateIndices_;
    std::vector<char*> workingUpdateChunkData_;
    int32_t workingUpdateCount_ = 0;
};

}  // namespace op
}  // namespace omniruntime
