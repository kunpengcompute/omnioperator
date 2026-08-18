/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: TAPER join hash table variants — skeleton for speculative probe framework
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "common_join.h"
#include "memory/simple_arena_allocator.h"
#include "operator/execution_context.h"
#include "operator/hash_util.h"
#include "operator/hashmap/array_map.h"
#include "operator/hashmap/column_marshaller.h"
#include "operator/hashmap/vector_marshaller.h"
#include "operator/join/taper_join_handler.h"
#include "operator/status.h"
#include "type/data_types.h"
#include "vector/array_vector.h"
#include "vector/decoded_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace vec {
class VectorBatch;
class BaseVector;
} // namespace vec

namespace op {

// TaperJoinHashTableVariants is referenced via std::visit in the taper
// branches; only the Mapped alias type is needed for is_same_v dispatch.
// The 6-arg Find path (which consumes Mapped values) is not used by taper
// join - FindBatch/ProbeBatch is the real path.
struct TaperMapped {};

enum class HashTableImplementationType {
    NORMAL_HASH_TABLE,
    ARRAY_HASH_TABLE
};

// --- Pre-decoded hash column access (FindBatch ser branch) -----------------
// Layout/encoding dispatch hoisted out of the per-row loop: each column is
// unwrapped once into SerHashColMeta + a typed hasher; the row loop just
// calls hasher(colMeta[k], pos) with zero layout dispatch.
struct SerHashColMeta {
    const uint8_t* nulls;
    const void* flatPtr;
    alignas(16) char constBuf[16];
    const int32_t* ids;
    RowContainer::StringViewStorage constStr;
    void* container;
    mem::SimpleArenaAllocator* arena;
    int64_t (*hasher)(const SerHashColMeta&, int32_t);
};

template <typename T, vec::DVecLayout L>
static inline int64_t SerHashFixedCol(const SerHashColMeta& m, int32_t pos) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), pos)) return kNullHash;
    if constexpr (L == vec::DVecLayout::Flat) {
        return HashUtil::HashValue(reinterpret_cast<const T*>(m.flatPtr)[pos]);
    } else if constexpr (L == vec::DVecLayout::Constant) {
        return HashUtil::HashValue(*reinterpret_cast<const T*>(m.constBuf));
    } else {
        return HashUtil::HashValue(reinterpret_cast<const T*>(m.flatPtr)[m.ids[pos]]);
    }
}

template <vec::DVecLayout L>
static inline int64_t SerHashDecimal128Col(const SerHashColMeta& m, int32_t pos) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), pos)) return kNullHash;
    Decimal128 d128;
    if constexpr (L == vec::DVecLayout::Flat) d128 = reinterpret_cast<const Decimal128*>(m.flatPtr)[pos];
    else if constexpr (L == vec::DVecLayout::Constant) d128 = *reinterpret_cast<const Decimal128*>(m.constBuf);
    else d128 = reinterpret_cast<const Decimal128*>(m.flatPtr)[m.ids[pos]];
    return HashUtil::HashValue(d128.LowBits(), d128.HighBits());
}

template <int Enc>
static inline int64_t SerHashVarcharCol(const SerHashColMeta& m, int32_t pos) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), pos)) return kNullHash;
    std::string_view sv;
    if constexpr (Enc == OMNI_ENCODING_CONST) {
        sv = std::string_view(m.constStr.data, m.constStr.size);
    } else if constexpr (Enc == OMNI_DICTIONARY) {
        sv = static_cast<Vector<DictionaryContainer<std::string_view>>*>(m.container)->GetValue(pos);
    } else {
        sv = static_cast<Vector<LargeStringContainer<std::string_view>>*>(m.container)->GetValue(pos);
    }
    return HashUtil::HashValue(reinterpret_cast<int8_t*>(const_cast<char*>(sv.data())),
        static_cast<int32_t>(sv.size()));
}

template <int Kind>  // 1=array 2=map 3=row
static inline int64_t SerHashComplexCol(const SerHashColMeta& m, int32_t pos) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), pos)) return kNullHash;
    type::StringRef ref;
    SerializerFunc serializer;
    if constexpr (Kind == 1) {
        serializer = complexVectorSerializerCenter[type::OMNI_ARRAY];
    } else if constexpr (Kind == 2) {
        serializer = complexVectorSerializerCenter[type::OMNI_MAP];
    } else {
        serializer = complexVectorSerializerCenter[type::OMNI_ROW];
    }
    serializer(static_cast<BaseVector*>(m.container), pos, *m.arena, ref);
    return HashUtil::HashValue(reinterpret_cast<int8_t*>(const_cast<char*>(ref.data)),
        static_cast<int32_t>(ref.size));
}

// Fixed-width reader for FindBatch fixed branch: layout fully resolved at
// pre-decoding time via template instantiation; the row loop only calls
// reader(flatPtr, constVal, ids, pos) with zero layout dispatch.
template <typename T, vec::DVecLayout L>
static ALWAYS_INLINE int64_t TaperReadFixedVal(const void* flatPtr, int64_t constVal, const int32_t* ids, int32_t pos) {
    if constexpr (L == vec::DVecLayout::Flat) return static_cast<int64_t>(reinterpret_cast<const T*>(flatPtr)[pos]);
    else if constexpr (L == vec::DVecLayout::Constant) return constVal;
    else return static_cast<int64_t>(reinterpret_cast<const T*>(flatPtr)[ids[pos]]);
}

// Column-major packer for FindBatch fixed multi-column: one column at a time,
// layout/width fully resolved via template instantiation; the inner row loop
// is pure bit-packing with zero layout dispatch.
template <typename KeyT, typename T, vec::DVecLayout L>
static ALWAYS_INLINE void TaperPackColumn(KeyT* packedBuf, int32_t probeStart, int32_t probeEnd,
    const uint8_t* nulls, const void* flatPtr, int64_t constVal, const int32_t* ids, uint8_t width, KeyT mask) {
    for (int32_t pos = probeStart; pos < probeEnd; ++pos) {
        int32_t idx = pos - probeStart;
        KeyT val = 0;
        if (!(nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(nulls), pos))) {
            if constexpr (L == vec::DVecLayout::Flat) val = static_cast<KeyT>(reinterpret_cast<const T*>(flatPtr)[pos]);
            else if constexpr (L == vec::DVecLayout::Constant) val = static_cast<KeyT>(constVal);
            else val = static_cast<KeyT>(reinterpret_cast<const T*>(flatPtr)[ids[pos]]);
            val &= mask;
        }
        packedBuf[idx] = (packedBuf[idx] << width) | val;
    }
}

// Fixed-width column metadata setup for FindBatch ser branch: layout and width
// resolved via template instantiation, sets flatPtr, constBuf, ids, and hasher.
template <typename T>
static ALWAYS_INLINE void SetupSerHashFixedCol(SerHashColMeta& m, const vec::DecodedVector& dv, vec::DVecLayout layout) {
    m.flatPtr = dv.template FlatValues<T>();
    if (layout == vec::DVecLayout::Constant) {
        *reinterpret_cast<T*>(m.constBuf) = dv.template GetConstValue<T>();
    }
    m.ids = dv.Ids();
    m.hasher = layout == vec::DVecLayout::Flat
        ? &SerHashFixedCol<T, vec::DVecLayout::Flat>
        : layout == vec::DVecLayout::Constant
        ? &SerHashFixedCol<T, vec::DVecLayout::Constant>
        : &SerHashFixedCol<T, vec::DVecLayout::Dictionary>;
}

// Column-major pack dispatch for FindBatch fixed multi-column: resolves layout
// via template instantiation and calls TaperPackColumn with correct layout.
template <typename KeyT, typename T>
static ALWAYS_INLINE void DispatchTaperPackColumn(KeyT* packedBuf, int32_t probeStart, int32_t probeEnd,
    const uint8_t* nulls, const vec::DecodedVector& dv, uint8_t width, KeyT mask) {
    auto layout = dv.GetLayout();
    if (layout == vec::DVecLayout::Flat) {
        TaperPackColumn<KeyT, T, vec::DVecLayout::Flat>(packedBuf, probeStart, probeEnd, nulls,
            dv.template FlatValues<T>(), 0, nullptr, width, mask);
    } else if (layout == vec::DVecLayout::Constant) {
        TaperPackColumn<KeyT, T, vec::DVecLayout::Constant>(packedBuf, probeStart, probeEnd, nulls,
            nullptr, dv.template GetConstValue<T>(), nullptr, width, mask);
    } else {
        TaperPackColumn<KeyT, T, vec::DVecLayout::Dictionary>(packedBuf, probeStart, probeEnd, nulls,
            dv.template FlatValues<T>(), 0, dv.Ids(), width, mask);
    }
}

// Single fixed-width key filler for FindBatch fixed single-column: reads the
// pre-resolved layout and writes rowHashes + positions with zero dispatch.
template <typename KeyT, vec::DVecLayout L>
static ALWAYS_INLINE void TaperFillSingleKey(KeyT* out, int32_t* positions,
    const char* isNulls, int32_t probeStart, int32_t probeEnd,
    const void* flatPtr, int64_t constVal, const int32_t* ids) {
    for (int32_t pos = probeStart; pos < probeEnd; ++pos) {
        int32_t idx = pos - probeStart;
        if (isNulls[idx]) continue;
        KeyT key;
        if constexpr (L == vec::DVecLayout::Flat) key = static_cast<KeyT>(reinterpret_cast<const KeyT*>(flatPtr)[pos]);
        else if constexpr (L == vec::DVecLayout::Constant) key = static_cast<KeyT>(constVal);
        else key = static_cast<KeyT>(reinterpret_cast<const KeyT*>(flatPtr)[ids[pos]]);
        out[idx] = key;
        positions[idx] = pos;
    }
}

template <typename KeyType, bool NeedVisited>
class TaperJoinHashTableVariants {
public:
    using Key = KeyType;
    using Mapped = TaperMapped;
    static constexpr bool IS_SIMPLE_KEY = false;

    TaperJoinHashTableVariants(int32_t operatorCount, DataTypes* buildTypes,
        const std::vector<int32_t>& buildHashCols, JoinType joinType, BuildSide buildSide)
        : buildTypes_(buildTypes), buildHashCols_(buildHashCols),
          tableCount_(static_cast<uint32_t>(operatorCount)),
          joinType_(joinType), buildSide_(buildSide),
          handlers_(tableCount_),
          executionContexts_(tableCount_),
          inputVecBatches_(std::vector<std::vector<vec::VectorBatch*>>(tableCount_)),
          totalRowCount_(std::vector<size_t>(tableCount_)) {
        for (uint32_t i = 0; i < tableCount_; ++i) {
            executionContexts_[i] = std::make_unique<ExecutionContext>();
        }
        // Store all build columns (keys + output + filter are subsets)
        int numBuildCols = buildTypes_->GetSize();
        taperStoredColIndices_.reserve(numBuildCols);
        for (int i = 0; i < numBuildCols; ++i) {
            taperStoredColIndices_.push_back(i);
        }
        // Multi-column key: compute pack plan (bit-widths + masks per key column)
        if (buildHashCols_.size() > 1) {
            isMultiColumn_ = true;
            bitWidths_.reserve(buildHashCols_.size());
            masks_.reserve(buildHashCols_.size());
            for (size_t i = 0; i < buildHashCols_.size(); ++i) {
                auto typeId = buildTypes_->GetIds()[buildHashCols_[i]];
                auto w = PackedBitWidth(typeId);
                bitWidths_.push_back(w);
                if (w >= static_cast<decltype(w)>(sizeof(KeyType) * 8)) {
                    masks_.push_back(static_cast<KeyType>(~static_cast<KeyType>(0)));
                } else {
                    masks_.push_back((static_cast<KeyType>(1) << w) - 1);
                }
            }
        }
    }
    ~TaperJoinHashTableVariants() = default;

    /// Bit-width for packable types; returns 0 for unsupported (varchar etc.).
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

    // --- Metadata / accessors -------------------------------------------------

    uint32_t GetHashTableCount() const { return tableCount_; }
    uint32_t GetHashTableSize() const {
        uint32_t total = 0;
        for (const auto& h : handlers_) {
            if (h) total += static_cast<uint32_t>(h->Size());
        }
        for (const auto& h : serHandlers_) {
            if (h) total += static_cast<uint32_t>(h->Size());
        }
        return total;
    }

    DataTypes* GetBuildDataTypes() { return buildTypes_; }
    void SetProbeTypes(DataTypes* probeDataTypes) { probeTypes_ = probeDataTypes; }

    HashTableImplementationType GetHashTableTypes(int /*partitionIndex*/) const {
        return HashTableImplementationType::NORMAL_HASH_TABLE;
    }
    bool GetIsMultiCols() const { return isMultiColumn_; }
    JoinType GetJoinType() const { return joinType_; }
    BuildSide GetBuildSide() const { return buildSide_; }
    OmniStatus GetStatus() const { return status_; }
    void SetStatus(OmniStatus s) { status_ = s; }

    void SetTaperStoredColumns(const std::vector<int32_t>& indices) {
        taperStoredColIndices_ = indices;
    }

    void AddVecBatch(int partitionIndex, omniruntime::vec::VectorBatch* vecBatch) {
        inputVecBatches_[partitionIndex].push_back(vecBatch);
        totalRowCount_[partitionIndex] += vecBatch->GetRowCount();
    }
    void Prepare(int partitionIndex) {
        auto& vecBatches = inputVecBatches_[partitionIndex];
        for (size_t i = 0; i < vecBatches.size(); ++i) {
            CollectTaperRows(partitionIndex, vecBatches[i], static_cast<uint32_t>(i));
        }
        // Free build-side VectorBatches -- data now stored in RowContainer
        for (auto* vb : vecBatches) {
            vec::VectorHelper::FreeVecBatch(vb);
        }
        vecBatches.clear();
    }
    void BuildHashTable(int partitionIndex) {
        if ((isSer_ && !serHandlers_[partitionIndex]) ||
            (!isSer_ && !handlers_[partitionIndex])) {
            return;
        }
        if(isSer_){
            BuildTaperHashTableSerialized(partitionIndex);
        } else {
            BuildTaperHashTableFixed(partitionIndex);
        }
    }

    void CollectTaperRows(int32_t partitionIndex, vec::VectorBatch* vecBatch, uint32_t batchIdx);
    void BuildTaperHashTableSerialized(int32_t partitionIndex);
    void BuildTaperHashTableFixed(int32_t partitionIndex);

    void FindBatch(
        int32_t probeStart,
        int32_t probeEnd,
        const std::vector<int8_t>& probeNulls,
        omniruntime::vec::BaseVector** probeHashColumns,
        int32_t probeHashColCount,
        bool singleHT,
        uint32_t partitionMask,
        const std::vector<int64_t>& probeHashes,
        std::vector<char*>& chainHeads) {
        int32_t numRows = probeEnd - probeStart;
        if (isSer_) {
            std::vector<int64_t> rowHashes(numRows);
            std::vector<int32_t> positions(numRows);
            std::vector<char> isNulls(numRows, 0);
            std::vector<DecodedVector> dv(probeHashColCount);
            {
                for (int32_t k = 0; k < probeHashColCount; ++k) {
                        dv[k].Decode(probeHashColumns[k], probeHashColumns[k]->GetSize());
                }
            }
            {
            std::vector<SerHashColMeta> colMeta(probeHashColCount);
            mem::SimpleArenaAllocator hashArena;
            for (int32_t k = 0; k < probeHashColCount; ++k) {
                auto& m = colMeta[k];
                m.nulls = dv[k].Nulls();
                m.hasher = nullptr;
                m.arena = nullptr;
                auto layout = dv[k].GetLayout();
                auto typeId = probeHashColumns[k]->GetTypeId();
                auto w = PackedBitWidth(static_cast<int32_t>(typeId));
                if (w == 0) {
                    if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR ||
                        typeId == type::OMNI_VARBINARY) {
                        auto enc = dv[k].Base()->GetEncoding();
                        if (enc == OMNI_ENCODING_CONST) {
                            auto sv = static_cast<ConstVector<std::string_view>*>(dv[k].Base())->GetConstValue();
                            m.constStr = {sv.data(), static_cast<uint32_t>(sv.size())};
                            m.hasher = &SerHashVarcharCol<OMNI_ENCODING_CONST>;
                        } else if (enc == OMNI_DICTIONARY) {
                            m.container = static_cast<Vector<DictionaryContainer<std::string_view>>*>(dv[k].Base());
                            m.hasher = &SerHashVarcharCol<OMNI_DICTIONARY>;
                        } else {
                            m.container = static_cast<Vector<LargeStringContainer<std::string_view>>*>(dv[k].Base());
                            m.hasher = &SerHashVarcharCol<OMNI_FLAT>;
                        }
                    } else if (typeId == type::OMNI_ARRAY) {
                        m.container = dv[k].Base();
                        m.arena = &hashArena;
                        m.hasher = &SerHashComplexCol<1>;
                    } else if (typeId == type::OMNI_MAP) {
                        m.container = dv[k].Base();
                        m.arena = &hashArena;
                        m.hasher = &SerHashComplexCol<2>;
                    } else if (typeId == type::OMNI_ROW) {
                        m.container = dv[k].Base();
                        m.arena = &hashArena;
                        m.hasher = &SerHashComplexCol<3>;
                    }
                } else {
                    switch (w) {
                        case 8:
                            SetupSerHashFixedCol<int8_t>(m, dv[k], layout);
                            break;
                        case 16:
                            SetupSerHashFixedCol<int16_t>(m, dv[k], layout);
                            break;
                        case 32:
                            SetupSerHashFixedCol<int32_t>(m, dv[k], layout);
                            break;
                        case 64:
                            SetupSerHashFixedCol<int64_t>(m, dv[k], layout);
                            break;
                        case 128:
                            m.flatPtr = dv[k].template FlatValues<Decimal128>();
                            if (layout == vec::DVecLayout::Constant) {
                                *reinterpret_cast<Decimal128*>(m.constBuf) =
                                    dv[k].template GetConstValue<Decimal128>();
                            }
                            m.ids = dv[k].Ids();
                            m.hasher = layout == vec::DVecLayout::Flat
                                ? &SerHashDecimal128Col<vec::DVecLayout::Flat>
                                : layout == vec::DVecLayout::Constant
                                ? &SerHashDecimal128Col<vec::DVecLayout::Constant>
                                : &SerHashDecimal128Col<vec::DVecLayout::Dictionary>;
                            break;
                        default: break;
                    }
                }
            }
            for (int32_t pos = probeStart; pos < probeEnd; ++pos) {
                int32_t idx = pos - probeStart;
                chainHeads[idx] = nullptr;
                if (probeNulls[pos]) {
                    isNulls[idx] = 1;
                    continue;
                }
                int64_t hash = 0;
                for (int32_t k = 0; k < probeHashColCount; ++k) {
                    const auto& m = colMeta[k];
                    if (m.hasher) hash = SerFastHashMix(hash, m.hasher(m, pos));
                }
                rowHashes[idx] = hash;
                positions[idx] = pos;
            }
            }
            if (numRows > 0 && serHandlers_[partitionMask]) {
                serHandlers_[partitionMask]->SetProbeDecodedColumns(dv,
                    probeHashColCount > 0 ? probeHashColumns[0]->GetSize() : 0);
                serHandlers_[partitionMask]->ProbeBatch(
                    rowHashes.data(), numRows,
                    probeHashColumns, probeHashColCount,
                    positions.data(), chainHeads.data(), isNulls.data());
            }
        } else {
            std::vector<Key> rowHashes(numRows);
            std::vector<int32_t> positions(numRows);
            std::vector<char> isNulls(numRows, 0);
            if (isMultiColumn_) {
                std::vector<vec::DecodedVector> dv(probeHashColCount);
                {
                for (int32_t k = 0; k < probeHashColCount; ++k) {
                    dv[k].Decode(probeHashColumns[k], probeHashColumns[k]->GetSize());
                }
                }
                {
                std::fill_n(chainHeads.begin(), numRows, nullptr);
                for (int32_t pos = probeStart; pos < probeEnd; ++pos) {
                    int32_t idx = pos - probeStart;
                    isNulls[idx] = probeNulls[pos] ? 1 : 0;
                }
                // Multi-column: column-major pack — per column resolve layout once
                // (template instantiation), inner row loop is pure bit-packing.
                for (int32_t k = 0; k < probeHashColCount; ++k) {
                    auto layout = dv[k].GetLayout();
                    const uint8_t* nulls = dv[k].Nulls();
                    switch (bitWidths_[k]) {
                        case 8:
                            DispatchTaperPackColumn<Key, int8_t>(rowHashes.data(), probeStart, probeEnd, nulls, dv[k], 8, masks_[k]);
                            break;
                        case 16:
                            DispatchTaperPackColumn<Key, int16_t>(rowHashes.data(), probeStart, probeEnd, nulls, dv[k], 16, masks_[k]);
                            break;
                        case 32:
                            DispatchTaperPackColumn<Key, int32_t>(rowHashes.data(), probeStart, probeEnd, nulls, dv[k], 32, masks_[k]);
                            break;
                        case 64:
                            DispatchTaperPackColumn<Key, int64_t>(rowHashes.data(), probeStart, probeEnd, nulls, dv[k], 64, masks_[k]);
                            break;
                        default: break;
                    }
                }
                for (int32_t pos = probeStart; pos < probeEnd; ++pos) {
                    int32_t idx = pos - probeStart;
                    positions[idx] = pos;
                }
                }
            } else {
                auto* vec = probeHashColumns[0];
                vec::DecodedVector dv;
                {
                    dv.Decode(vec, vec->GetSize());
                }
                {
                    std::fill_n(chainHeads.begin(), numRows, nullptr);
                    for (int32_t pos = probeStart; pos < probeEnd; ++pos) {
                        int32_t idx = pos - probeStart;
                        isNulls[idx] = probeNulls[pos] ? 1 : 0;
                    }
                    auto singleLayout = dv.GetLayout();
                    const void* singleFlat = dv.template FlatValues<Key>();
                    int64_t singleConst = (singleLayout == vec::DVecLayout::Constant)
                        ? dv.template GetConstValue<Key>() : 0;
                    const int32_t* singleIds = dv.Ids();
                    if (singleLayout == vec::DVecLayout::Flat) {
                        TaperFillSingleKey<Key, vec::DVecLayout::Flat>(rowHashes.data(), positions.data(),
                            isNulls.data(), probeStart, probeEnd, singleFlat, singleConst, singleIds);
                    } else if (singleLayout == vec::DVecLayout::Constant) {
                        TaperFillSingleKey<Key, vec::DVecLayout::Constant>(rowHashes.data(), positions.data(),
                            isNulls.data(), probeStart, probeEnd, singleFlat, singleConst, singleIds);
                    } else {
                        TaperFillSingleKey<Key, vec::DVecLayout::Dictionary>(rowHashes.data(), positions.data(),
                            isNulls.data(), probeStart, probeEnd, singleFlat, singleConst, singleIds);
                    }
                }
            }
            if (numRows > 0 && handlers_[partitionMask]) {
                if (arrayMode_ && partitionMask == 0 && !isMultiColumn_) {
                    // Non-SIMD fallback: query arrayTable directly (single numeric key col).
                    bool* assigned = arrayTable_->GetAssigned();
                    char** slots = arrayTable_->GetSlots();
                    int64_t amin = arrayMin_;
                    int64_t amax = arrayMax_;
                    for (int32_t idx = 0; idx < numRows; ++idx) {
                        if (isNulls[idx]) { chainHeads[idx] = nullptr; continue; }
                        Key key = rowHashes[idx];
                        if (key >= amin && key <= amax) {
                            int64_t pos = static_cast<int64_t>(key) - amin;
                            chainHeads[idx] = assigned[pos] ? slots[pos] : nullptr;
                        } else {
                            chainHeads[idx] = nullptr;
                        }
                    }
                } else {
                    handlers_[partitionMask]->ProbeBatch(
                        rowHashes.data(), numRows,
                        probeHashColumns, probeHashColCount,
                        positions.data(), chainHeads.data(), isNulls.data());
                }
            }
        }
    }

    // --- SIMD helpers ---------------------------------------------------------

    bool CanProbeSIMD(omniruntime::vec::BaseVector** cols, int32_t cnt, uint32_t partition) {
        return arrayMode_ && !isSer_ && partition == 0 && tableCount_ == 1 && cnt == 1 && cols && cols[0] &&
            cols[0]->GetEncoding() != OMNI_DICTIONARY && cols[0]->GetEncoding() != OMNI_ENCODING_CONST;
    }
    KeyType* GetSingleProbeHashKeyBase(omniruntime::vec::BaseVector** cols) {
        return reinterpret_cast<KeyType*>(VectorHelper::UnsafeGetValues(cols[0]));
    }

    // --- Array-map helpers ----------------------------------------------------

    DefaultArrayMap<char>* GetArrayTable(int idx) {
        return (idx == 0 && arrayMode_) ? arrayTable_.get() : nullptr;
    }
    char** GetArraySlots(int idx) {
        return (idx == 0 && arrayMode_) ? arrayTable_->GetSlots() : nullptr;
    }
    bool* GetArrayAssigned(int idx) {
        return (idx == 0 && arrayMode_) ? arrayTable_->GetAssigned() : nullptr;
    }
    std::pair<int64_t, int64_t> GetmaxMinValue(int /*partitionIndex*/) { return {arrayMax_, arrayMin_}; }

    // --- Build filter support -------------------------------------------------

    void InitBuildFilterCols(std::vector<int32_t>& /*buildFilterCols*/,
                             int /*probeColCount*/,
                             std::vector<std::vector<omniruntime::vec::BaseVector**>>& /*tableBfColPtrs*/) {}

    // --- Visited tracking -----------------------------------------------------

    ALWAYS_INLINE void IncrementVisited() { visitedCounts++; }
    uint32_t GetVisitedCounts() const { return visitedCounts; }
    uint32_t GetTotalVisitedCounts() const { return totalVisitedCounts; }
    void AddTotalVisitedCounts(int cnt) { totalVisitedCounts += cnt; }

    // --- TAPER-specific accessors ---------------------------------------------

    RowContainer* GetTaperRowContainer(int idx) {
        if (isSer_ && idx >= 0 && static_cast<size_t>(idx) < serHandlers_.size() && serHandlers_[idx])
            return serHandlers_[idx]->Rows();
        return idx >= 0 && static_cast<size_t>(idx) < handlers_.size() && handlers_[idx]
            ? handlers_[idx]->Rows() : nullptr;
    }
    const std::vector<int32_t>& GetTaperStoredColIndices() const { return taperStoredColIndices_; }
    const std::vector<int32_t>& GetBuildHashCols() const { return buildHashCols_; }

    void SetSerMode() { isSer_ = true; serHandlers_.resize(tableCount_); }
    bool IsSerMode() const { return isSer_; }

    bool HasDuplicates() const { return hasDuplicates_; }

    // --- Key type for template dispatch ---------------------------------------
    KeyType keyType{};

private:
    DataTypes* buildTypes_ = nullptr;
    DataTypes* probeTypes_ = nullptr;
    std::vector<int32_t> buildHashCols_;
    std::vector<int32_t> buildOutputCols;
    std::vector<int32_t> buildFilterCols;
    std::vector<int32_t> taperStoredColIndices_;
    std::vector<omniruntime::vec::BaseVector***> columns_;
    uint32_t tableCount_ = 0;
    JoinType joinType_ = OMNI_JOIN_TYPE_INNER;
    BuildSide buildSide_ = OMNI_BUILD_UNKNOWN;
    OmniStatus status_ = OmniStatus::OMNI_STATUS_NORMAL;
    std::vector<std::unique_ptr<TaperJoinFixedHandler<KeyType, NeedVisited>>> handlers_;
    std::vector<std::unique_ptr<TaperJoinSerializedHandler>> serHandlers_;
    uint32_t visitedCounts = 0;
    uint32_t totalVisitedCounts = 0;

    std::vector<std::unique_ptr<ExecutionContext>> executionContexts_;
    std::vector<std::vector<omniruntime::vec::VectorBatch*>> inputVecBatches_;
    std::vector<size_t> totalRowCount_;

    // Multi-column key bit-packing support (same as agg packed mode)
    bool isMultiColumn_ = false;
    bool isSer_ = false;
    bool hasDuplicates_ = false;
    std::vector<uint8_t> bitWidths_;
    std::vector<KeyType> masks_;

    // Array-join (direct-mapped) mode support
    struct TaperArrayAnalyzer {
        bool active = true;    // analyzing (AppendRows<true>)
        bool feasible = true;  // single numeric flat key col
        int64_t min = INT64_MAX;
        int64_t max = INT64_MIN;
        bool hasValue = false;
    } arrayAnalyzer_;
    bool arrayMode_ = false;
    std::unique_ptr<DefaultArrayMap<char>> arrayTable_;
    int64_t arrayMin_ = 0;
    int64_t arrayMax_ = 0;

    void InitHandlerRowContainer(int partitionIndex);
};

// --- Out-of-line implementations ----------------------------------------------

inline uint64_t SerFastHashMix(uint64_t a, uint64_t b)
{
    return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
}

template <typename KeyType, bool NeedVisited>
void TaperJoinHashTableVariants<KeyType, NeedVisited>::CollectTaperRows(
    int32_t partitionIndex, vec::VectorBatch* vecBatch, uint32_t batchIdx) {
    (void)batchIdx;
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) return;

    auto& storedCols = taperStoredColIndices_.empty() ? buildHashCols_ : taperStoredColIndices_;
    int storedColNum = static_cast<int>(storedCols.size());

    // Lazy-init handler
    if ((isSer_ && !serHandlers_[partitionIndex]) || (!isSer_ && !handlers_[partitionIndex])) {
        std::vector<bool> isVar(storedColNum, false);
        std::vector<int32_t> typeIds(storedColNum);
        std::vector<int32_t> varcharCols;
        for (int c = 0; c < storedColNum; ++c) {
            typeIds[c] = buildTypes_->GetIds()[storedCols[c]];
        }
        std::vector<int32_t> keySizes(storedColNum);
        for (int c = 0; c < storedColNum; ++c) {
            switch (typeIds[c]) {
                case type::OMNI_BYTE: case type::OMNI_BOOLEAN: keySizes[c] = 1; break;
                case type::OMNI_SHORT: keySizes[c] = 2; break;
                case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_FLOAT:
                case type::OMNI_TIME32: keySizes[c] = 4; break;
                case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE: case type::OMNI_DATE64: case type::OMNI_TIME64:
                    keySizes[c] = 8; break;
                case type::OMNI_VARCHAR: case type::OMNI_CHAR: case type::OMNI_VARBINARY:
                    keySizes[c] = sizeof(char*) + sizeof(uint32_t); break;
                case type::OMNI_ARRAY: case type::OMNI_MAP: case type::OMNI_ROW:
                    keySizes[c] = sizeof(char*) + sizeof(size_t); isVar[c] = true; break;
                case type::OMNI_DECIMAL128: keySizes[c] = sizeof(Decimal128); break;
                default: throw omniruntime::exception::OmniException("TAPER_NOT_SUPPORTED",
                    "Join key column type not supported");
            }
        }
        std::vector<int32_t> keyColIndices;
        keyColIndices.reserve(buildHashCols_.size());
        for (int32_t buildCol : buildHashCols_) {
            auto it = std::find(storedCols.begin(), storedCols.end(), buildCol);
            if (it == storedCols.end()) {
                throw omniruntime::exception::OmniException("TAPER_NOT_SUPPORTED",
                    "Join key column not found in stored columns");
            }
            keyColIndices.push_back(static_cast<int32_t>(std::distance(storedCols.begin(), it)));
        }
        auto* ctx = executionContexts_[partitionIndex].get();
        if (isSer_) {
            serHandlers_[partitionIndex] = std::make_unique<TaperJoinSerializedHandler>(*ctx->GetArena());
            serHandlers_[partitionIndex]->InitRowContainer(keySizes, isVar, typeIds, varcharCols, keyColIndices);
        } else {
            handlers_[partitionIndex] = std::make_unique<TaperJoinFixedHandler<KeyType, NeedVisited>>(
                *ctx->GetArena(), 0);
            handlers_[partitionIndex]->InitRowContainer(keySizes, isVar, typeIds, varcharCols);
        }
    }

    // Decode and append each row
    std::vector<vec::DecodedVector> decoded(storedColNum);
    for (int c = 0; c < storedColNum; ++c) {
        int colIdx = storedCols[c];
        auto* colVec = vecBatch->Get(colIdx);
        decoded[c].Decode(colVec, rowCount);
    }
    std::vector<char*> rows(rowCount);
    if (isSer_) {
        serHandlers_[partitionIndex]->AppendRows(decoded.data(), 0, rowCount, rows.data());
    } else if (partitionIndex == 0 && !isMultiColumn_ && arrayAnalyzer_.active) {
        auto& sc = taperStoredColIndices_.empty() ? buildHashCols_ : taperStoredColIndices_;
        auto itf = std::find(sc.begin(), sc.end(), buildHashCols_[0]);
        int32_t keyIdx = (itf != sc.end()) ? static_cast<int32_t>(std::distance(sc.begin(), itf)) : 0;
        handlers_[partitionIndex]->template AppendRows<true>(decoded.data(), 0, rowCount, rows.data(),
            keyIdx, &arrayAnalyzer_.min, &arrayAnalyzer_.max, &arrayAnalyzer_.feasible);
        if (arrayAnalyzer_.min != INT64_MAX) arrayAnalyzer_.hasValue = true;
        if (!arrayAnalyzer_.feasible) arrayAnalyzer_.active = false;
    } else {
        handlers_[partitionIndex]->template AppendRows<false>(decoded.data(), 0, rowCount, rows.data());
    }
    // 转移字符串容器所有权,防止 VectorBatch 释放后悬垂 use-after-free
    if (isSer_) {
        serHandlers_[partitionIndex]->HoldStringContainers(decoded.data());
    } else {
        handlers_[partitionIndex]->HoldStringContainers(decoded.data());
    }
}

template <typename KeyType, bool NeedVisited>
void TaperJoinHashTableVariants<KeyType, NeedVisited>::BuildTaperHashTableSerialized(int32_t partitionIndex) {
    auto* serHandler = isSer_ ? serHandlers_[partitionIndex].get() : nullptr;
    if (!serHandler) return;
    auto* rc = serHandler->Rows();
    if (!rc) {
        return;
    }

    serHandler->ReserveTable(static_cast<size_t>(rc->NumRows()));
    constexpr uint32_t BLOCK_SIZE = 1024;
    int64_t keys[BLOCK_SIZE];
    char* rows[BLOCK_SIZE];
    bool isNulls[BLOCK_SIZE];
    RowContainerIterator it;
    int32_t n;
    while ((n = rc->ListRows(&it, BLOCK_SIZE, rows)) > 0) {
        memset(isNulls, false, sizeof(isNulls));
        for (int32_t i = 0; i < n; ++i) {
            int64_t hash = 0;
            for (size_t k = 0; k < buildHashCols_.size(); ++k) {
                int32_t colIdx = buildHashCols_[k];
                auto col = rc->ColumnAt(colIdx);
                bool isNull = RowContainer::IsNullAt(rows[i], col.NullByte(), col.NullMask());
                int64_t colHash;
                if (isNull) {
                    isNulls[i] = true;
                    colHash = kNullHash;
                    break;
                } else {
                    auto typeId = buildTypes_->GetIds()[colIdx];
                    int32_t colOff = col.Offset();
                    auto w = PackedBitWidth(typeId);
                    if (w == 0) {
                        if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR ||
                            typeId == type::OMNI_VARBINARY) {
                            auto storage = RowContainer::ReadValue<RowContainer::StringViewStorage>(rows[i], colOff);
                            if (storage.data != nullptr) {
                                colHash = HashUtil::HashValue(
                                    reinterpret_cast<int8_t*>(const_cast<char*>(storage.data)),
                                    static_cast<int32_t>(storage.size));
                            } else {
                                isNulls[i] = true;
                                colHash = kNullHash;
                            }
                        } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_MAP || typeId == type::OMNI_ROW) {
                            auto* dataPtr = RowContainer::ReadValue<char*>(rows[i], colOff);
                            auto dataLen = RowContainer::ReadValue<size_t>(rows[i], colOff + sizeof(char*));
                            if (dataPtr != nullptr && dataLen > 0) {
                                colHash = HashUtil::HashValue(
                                    reinterpret_cast<int8_t*>(dataPtr),
                                    static_cast<int32_t>(dataLen));
                            } else {
                                isNulls[i] = true;
                                colHash = kNullHash;
                            }
                        } else {
                            throw omniruntime::exception::OmniException("TAPER_NOT_SUPPORTED",
                                "Join key column type not supported");
                        }
                    } else {
                        switch (w) {
                            case 8:
                                colHash = HashUtil::HashValue(
                                    RowContainer::ReadValue<int8_t>(rows[i], colOff)); break;
                            case 16:
                                colHash = HashUtil::HashValue(
                                    RowContainer::ReadValue<int16_t>(rows[i], colOff)); break;
                            case 32:
                                colHash = HashUtil::HashValue(
                                    RowContainer::ReadValue<int32_t>(rows[i], colOff)); break;
                            case 64:
                                colHash = HashUtil::HashValue(
                                    RowContainer::ReadValue<int64_t>(rows[i], colOff)); break;
                            case 128: {
                                auto d128 = RowContainer::ReadValue<Decimal128>(rows[i], colOff);
                                colHash = HashUtil::HashValue(
                                    d128.LowBits(), d128.HighBits());
                                break;
                            }
                        }
                    }
                }
                hash = SerFastHashMix(hash, colHash);
            }
            keys[i] = hash;
        }
        serHandler->EmplaceBatch(reinterpret_cast<const int64_t*>(keys), rows, n, isNulls);
    }
    hasDuplicates_ = hasDuplicates_ || serHandler->HasDuplicates();
}

template <typename KeyType, bool NeedVisited>
void TaperJoinHashTableVariants<KeyType, NeedVisited>::BuildTaperHashTableFixed(int32_t partitionIndex) {
    auto* handler = handlers_[partitionIndex].get();
    if (!handler) return;
    auto* rc = handler->Rows();
    if (!rc) {
            return;
    }

    // Array-join mode: direct-mapped table (single numeric flat key col, partition 0).
    if (partitionIndex == 0 && !isMultiColumn_ && arrayAnalyzer_.feasible && arrayAnalyzer_.hasValue) {
        int64_t totalRows = rc->NumRows();
        if (totalRows > 0) {
            double capD = std::ceil(std::log2(std::max<double>(static_cast<double>(totalRows) / 0.75, 1.0)));
            int64_t cap = 1LL << static_cast<int32_t>(capD);
            int64_t range = arrayAnalyzer_.max - arrayAnalyzer_.min + 1;
            if (range > 0 && range <= 8 * cap && range <= static_cast<int64_t>(UINT32_MAX)) {
                arrayMode_ = true;
                arrayMin_ = arrayAnalyzer_.min;
                arrayMax_ = arrayAnalyzer_.max;
                arrayTable_ = std::make_unique<DefaultArrayMap<char>>(range);
                auto* table = arrayTable_.get();
                auto payloadOff = rc->PayloadOffset();
                auto& sc = taperStoredColIndices_.empty() ? buildHashCols_ : taperStoredColIndices_;
                auto itf = std::find(sc.begin(), sc.end(), buildHashCols_[0]);
                int32_t keyColIdx = (itf != sc.end()) ? static_cast<int32_t>(std::distance(sc.begin(), itf)) : 0;
                auto keyCol = rc->ColumnAt(keyColIdx);
                constexpr uint32_t BLOCK_SIZE = 1024;
                char* rows[BLOCK_SIZE];
                RowContainerIterator it;
                int32_t n;
                while ((n = rc->ListRows(&it, BLOCK_SIZE, rows)) > 0) {
                    for (int32_t i = 0; i < n; ++i) {
                        if (RowContainer::IsNullAt(rows[i], keyCol.NullByte(), keyCol.NullMask())) continue;
                        KeyType key = RowContainer::ReadValue<KeyType>(rows[i], keyCol.Offset());
                        size_t pos = static_cast<size_t>(key - arrayMin_);
                        auto ret = table->InsertJoinKeysToHashmap(pos);
                        if (ret.IsInsert()) {
                            ret.SetValue(rows[i]);
                        } else {
                            *RowContainer::NextPtr(rows[i], payloadOff) = ret.GetValue();
                            ret.SetValue(rows[i]);
                        }
                    }
                }
                hasDuplicates_ = hasDuplicates_ ||
                    (rc->NumRows() > static_cast<int64_t>(table->GetElementsSize()));
                return;
            }
        }
    }

    handler->ReserveTable(static_cast<size_t>(rc->NumRows()));
    constexpr uint32_t BLOCK_SIZE = 1024;
    KeyType keys[BLOCK_SIZE];
    char* rows[BLOCK_SIZE];
    bool isNulls[BLOCK_SIZE];
    RowContainerIterator it;
    int32_t n;
    while ((n = rc->ListRows(&it, BLOCK_SIZE, rows)) > 0) {
        memset(isNulls, false, sizeof(isNulls));
        if (isMultiColumn_) {
            for (int32_t i = 0; i < n; ++i) {
                KeyType packed = 0;
                for (size_t k = 0; k < buildHashCols_.size(); ++k) {
                    int32_t colIdx = buildHashCols_[k];
                    auto col = rc->ColumnAt(colIdx);
                    isNulls[i] = RowContainer::IsNullAt(rows[i], col.NullByte(), col.NullMask());
                    if (isNulls[i]) break; // 只要有一个 null 就跳出。
                    KeyType val = 0;
                    switch (bitWidths_[k]) {
                        case 8:
                            val = static_cast<KeyType>(
                                RowContainer::ReadValue<int8_t>(rows[i], col.Offset())); break;
                        case 16:
                            val = static_cast<KeyType>(
                                RowContainer::ReadValue<int16_t>(rows[i], col.Offset())); break;
                        case 32:
                            val = static_cast<KeyType>(
                                RowContainer::ReadValue<int32_t>(rows[i], col.Offset())); break;
                        case 64:
                            val = static_cast<KeyType>(
                                RowContainer::ReadValue<int64_t>(rows[i], col.Offset())); break;
                    }
                    val &= masks_[k];
                    packed = (packed << bitWidths_[k]) | val;
                }
                keys[i] = packed;
            }
        } else {
            // 单列 key：从 RowContainer 取 key 列数据（key 不一定是第一列）
            auto& storedCols = taperStoredColIndices_.empty() ? buildHashCols_ : taperStoredColIndices_;
            auto it = std::find(storedCols.begin(), storedCols.end(), buildHashCols_[0]);
            int32_t keyColIdx = (it != storedCols.end())
                ? static_cast<int32_t>(std::distance(storedCols.begin(), it)) : 0;
            auto keyCol= rc->ColumnAt(keyColIdx);
            for (int32_t i = 0; i < n; ++i) {
                isNulls[i] = RowContainer::IsNullAt(rows[i], keyCol.NullByte(), keyCol.NullMask());
                keys[i] = RowContainer::ReadValue<KeyType>(rows[i], keyCol.Offset());
            }
        }
        handler->EmplaceBatch(keys, rows, n, isNulls);
    }
    hasDuplicates_ = hasDuplicates_ || handler->HasDuplicates();
}

} // namespace op
} // namespace omniruntime
