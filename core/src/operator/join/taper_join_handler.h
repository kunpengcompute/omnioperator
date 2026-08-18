/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: TAPER join handler — stubs for speculative probe framework
 */

#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "memory/simple_arena_allocator.h"
#include "operator/hashmap/row_container.h"
#include "operator/hashmap/taper_hashtable.h"
#include "operator/hashmap/vector_marshaller.h"
#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif
#include "type/data_types.h"
#include "type/decimal128.h"
#include "util/compiler_util.h"
#include "vector/decoded_vector.h"

namespace omniruntime {
namespace op {

namespace {
static constexpr uint32_t ROW_PTR_SIZE = 6;

static ALWAYS_INLINE void SetRowPtr(char *buf, char *ptr) {
    uint64_t val = reinterpret_cast<uint64_t>(ptr);
    memcpy(buf, &val, ROW_PTR_SIZE);
}

static ALWAYS_INLINE char *GetRowPtr(const char *buf) {
    uint64_t val = 0;
    memcpy(&val, buf, ROW_PTR_SIZE);
    return reinterpret_cast<char *>(val);
}

struct TaperColMeta {
    int32_t offset;
    int32_t nullByte;
    uint8_t nullMask;
    int32_t layout;
    const uint8_t* nulls;
    const void* flatPtr;
    alignas(16) char constBuf[16];
    const int32_t* ids;
    int32_t enc;
    const void* container;
    RowContainer::StringViewStorage constStr;
    int32_t arrayTypeId;
    const void* vecBase;
    void* arena;
    void (*writer)(char*, const TaperColMeta&, int32_t);
};

template <typename T, vec::DVecLayout L>
static ALWAYS_INLINE void TaperFixedWriter(char* row, const TaperColMeta& m, int32_t idx) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) {
        RowContainer::SetNullAt(row, m.nullByte, m.nullMask);
        return;
    }
    if constexpr (L == vec::DVecLayout::Flat) {
        RowContainer::StoreValue<T>(row, m.offset, reinterpret_cast<const T*>(m.flatPtr)[idx]);
    } else if constexpr (L == vec::DVecLayout::Constant) {
        RowContainer::StoreValue<T>(row, m.offset, *reinterpret_cast<const T*>(m.constBuf));
    } else {
        RowContainer::StoreValue<T>(row, m.offset, reinterpret_cast<const T*>(m.flatPtr)[m.ids[idx]]);
    }
}

template <int Enc>
static ALWAYS_INLINE void TaperVarcharWriter(char* row, const TaperColMeta& m, int32_t idx) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) {
        RowContainer::SetNullAt(row, m.nullByte, m.nullMask);
        return;
    }
    if constexpr (Enc == vec::OMNI_ENCODING_CONST) {
        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, m.offset, m.constStr);
    } else if constexpr (Enc == vec::OMNI_DICTIONARY) {
        auto* dictVec = static_cast<Vector<DictionaryContainer<std::string_view>>*>(const_cast<void*>(m.container));
        auto sv = dictVec->GetValue(idx);
        RowContainer::StoreValue<RowContainer::StringViewStorage>(
            row, m.offset, {sv.data(), static_cast<uint32_t>(sv.size())});
    } else {
        auto* lscVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(const_cast<void*>(m.container));
        auto sv = lscVec->GetValue(idx);
        RowContainer::StoreValue<RowContainer::StringViewStorage>(
            row, m.offset, {sv.data(), static_cast<uint32_t>(sv.size())});
    }
}

static ALWAYS_INLINE void TaperArrayWriter(char* row, const TaperColMeta& m, int32_t idx) {
    if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) {
        RowContainer::SetNullAt(row, m.nullByte, m.nullMask);
        return;
    }
    auto serializer = vectorSerializerCenter[static_cast<size_t>(m.arrayTypeId)];
    type::StringRef ref;
    serializer(m.vecBase, idx, *static_cast<mem::SimpleArenaAllocator*>(m.arena), ref);
    RowContainer::StoreValue<char*>(row, m.offset, const_cast<char*>(ref.data));
    RowContainer::StoreValue<size_t>(row, m.offset + sizeof(char*), ref.size);
}

// 批量填充一列(连续行 base,行 i = base + i*fixedRowSize)。
// 整块已 memset 清零;非 null 行写值;null 行收集到 nullFlags。
template <typename T, vec::DVecLayout L>
static void BatchFillColumn(char* base, int32_t count, int32_t fixedRowSize,
    int32_t startRow, const TaperColMeta& m, uint64_t* nullFlags)
{
    const T* flat = reinterpret_cast<const T*>(m.flatPtr);
    if (m.nulls) {
        for (int32_t i = 0; i < count; ++i) {
            int32_t idx = startRow + i;
            if (BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) {
                nullFlags[i >> 6] |= 1ull << (i & 63);
                continue;
            }
            T v;
            if constexpr (L == vec::DVecLayout::Flat) v = flat[idx];
            else if constexpr (L == vec::DVecLayout::Constant) v = *reinterpret_cast<const T*>(m.constBuf);
            else v = flat[m.ids[idx]];
            RowContainer::StoreValue<T>(base + i * fixedRowSize, m.offset, v);
        }
    } else {
        for (int32_t i = 0; i < count; ++i) {
            int32_t idx = startRow + i;
            T v;
            if constexpr (L == vec::DVecLayout::Flat) v = flat[idx];
            else if constexpr (L == vec::DVecLayout::Constant) v = *reinterpret_cast<const T*>(m.constBuf);
            else v = flat[m.ids[idx]];
            RowContainer::StoreValue<T>(base + i * fixedRowSize, m.offset, v);
        }
    }
}

#ifdef __ARM_FEATURE_SVE
// SVE 加速:8 字节固定列(int64/double/Decimal64 位模式)批量 scatter store。
template <vec::DVecLayout L>
static void BatchFillColumnSVE8(char* base, int32_t count, int32_t fixedRowSize,
    int32_t startRow, const TaperColMeta& m, uint64_t* nullFlags)
{
    const int64_t* flat = reinterpret_cast<const int64_t*>(m.flatPtr);
    if (m.nulls) {
        for (int32_t i = 0; i < count; ++i)
            if (BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), startRow + i))
                nullFlags[i >> 6] |= 1ull << (i & 63);
    }
    int32_t VL = svcntd();
    uint64_t addrBase = reinterpret_cast<uint64_t>(base) + static_cast<uint64_t>(m.offset);
    int32_t i = 0;
    for (; i + VL <= count; i += VL) {
        svbool_t pg = svptrue_b64();
        svint64_t vVal;
        if constexpr (L == vec::DVecLayout::Flat) {
            vVal = svld1_s64(pg, flat + startRow + i);
        } else if constexpr (L == vec::DVecLayout::Constant) {
            vVal = svdup_n_s64(*reinterpret_cast<const int64_t*>(m.constBuf));
        } else {
            svuint64_t vIds = svld1_u64(pg, reinterpret_cast<const uint64_t*>(m.ids + startRow + i));
            vVal = svld1_gather_u64base_s64(pg,
                svadd_n_u64_x(pg, svlsl_n_u64_x(pg, vIds, 3), reinterpret_cast<uint64_t>(flat)));
        }
        svuint64_t vIdx = svindex_u64(i, 1);
        svuint64_t vAddr = svadd_n_u64_x(
            pg, svmul_n_u64_x(pg, vIdx, static_cast<uint64_t>(fixedRowSize)), addrBase);
        svst1_scatter_u64base_s64(pg, vAddr, vVal);
    }
    for (; i < count; ++i) {
        int32_t idx = startRow + i;
        if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) continue;
        int64_t v;
        if constexpr (L == vec::DVecLayout::Flat) v = flat[idx];
        else if constexpr (L == vec::DVecLayout::Constant) v = *reinterpret_cast<const int64_t*>(m.constBuf);
        else v = flat[m.ids[idx]];
        RowContainer::StoreValue<int64_t>(base + i * fixedRowSize, m.offset, v);
    }
}

// SVE 加速:4 字节固定列(int32/float)批量 scatter store。
template <vec::DVecLayout L>
static void BatchFillColumnSVE4(char* base, int32_t count, int32_t fixedRowSize,
    int32_t startRow, const TaperColMeta& m, uint64_t* nullFlags)
{
    const int32_t* flat = reinterpret_cast<const int32_t*>(m.flatPtr);
    if (m.nulls) {
        for (int32_t i = 0; i < count; ++i)
            if (BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), startRow + i))
                nullFlags[i >> 6] |= 1ull << (i & 63);
    }
    int32_t VL = svcntd();
    uint64_t addrBase = reinterpret_cast<uint64_t>(base) + static_cast<uint64_t>(m.offset);
    int32_t i = 0;
    for (; i + VL <= count; i += VL) {
        svbool_t pg = svptrue_b64();
        svint32_t vVal;
        if constexpr (L == vec::DVecLayout::Flat) {
            vVal = svld1w_s32(pg, flat + startRow + i);
        } else if constexpr (L == vec::DVecLayout::Constant) {
            vVal = svdup_n_s32(*reinterpret_cast<const int32_t*>(m.constBuf));
        } else {
            svuint64_t vIds = svld1_u64(pg, reinterpret_cast<const uint64_t*>(m.ids + startRow + i));
            vVal = svld1w_gather_u64base_s32(pg,
                svadd_n_u64_x(pg, svlsl_n_u64_x(pg, vIds, 2), reinterpret_cast<uint64_t>(flat)));
        }
        svuint64_t vIdx = svindex_u64(i, 1);
        svuint64_t vAddr = svadd_n_u64_x(
            pg, svmul_n_u64_x(pg, vIdx, static_cast<uint64_t>(fixedRowSize)), addrBase);
        svst1w_scatter_u64base_s32(pg, vAddr, vVal);
    }
    for (; i < count; ++i) {
        int32_t idx = startRow + i;
        if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) continue;
        int32_t v;
        if constexpr (L == vec::DVecLayout::Flat) v = flat[idx];
        else if constexpr (L == vec::DVecLayout::Constant) v = *reinterpret_cast<const int32_t*>(m.constBuf);
        else v = flat[m.ids[idx]];
        RowContainer::StoreValue<int32_t>(base + i * fixedRowSize, m.offset, v);
    }
}
#endif

// 按 layout 外层分派(每列一次,编译期 L 模板展开,内层零运行时分支)。
template <typename T>
static void BatchFillColumnDispatch(char* base, int32_t count, int32_t fixedRowSize,
    int32_t startRow, const TaperColMeta& m, uint64_t* nullFlags)
{
    constexpr int32_t kFlat = static_cast<int32_t>(vec::DVecLayout::Flat);
    constexpr int32_t kConst = static_cast<int32_t>(vec::DVecLayout::Constant);
#if 0  // SVE 批量填列临时禁用(越界问题待定位),恢复时改回 #ifdef __ARM_FEATURE_SVE
    if constexpr (sizeof(T) == 8) {
        if (m.layout == kFlat) {
            BatchFillColumnSVE8<vec::DVecLayout::Flat>(base, count, fixedRowSize, startRow, m, nullFlags);
        } else if (m.layout == kConst) {
            BatchFillColumnSVE8<vec::DVecLayout::Constant>(base, count, fixedRowSize, startRow, m, nullFlags);
        } else {
            BatchFillColumnSVE8<vec::DVecLayout::Dictionary>(base, count, fixedRowSize, startRow, m, nullFlags);
        }
        return;
    }
    if constexpr (sizeof(T) == 4) {
        if (m.layout == kFlat) {
            BatchFillColumnSVE4<vec::DVecLayout::Flat>(base, count, fixedRowSize, startRow, m, nullFlags);
        } else if (m.layout == kConst) {
            BatchFillColumnSVE4<vec::DVecLayout::Constant>(base, count, fixedRowSize, startRow, m, nullFlags);
        } else {
            BatchFillColumnSVE4<vec::DVecLayout::Dictionary>(base, count, fixedRowSize, startRow, m, nullFlags);
        }
        return;
    }
#endif
    if (m.layout == kFlat) {
        BatchFillColumn<T, vec::DVecLayout::Flat>(base, count, fixedRowSize, startRow, m, nullFlags);
    } else if (m.layout == kConst) {
        BatchFillColumn<T, vec::DVecLayout::Constant>(base, count, fixedRowSize, startRow, m, nullFlags);
    } else {
        BatchFillColumn<T, vec::DVecLayout::Dictionary>(base, count, fixedRowSize, startRow, m, nullFlags);
    }
}

// null 块统一置位(批量填列后一轮)。
template <typename T>
static ALWAYS_INLINE void SetupFixedColMeta(TaperColMeta& m, const vec::DecodedVector& decodedCol, vec::DVecLayout layout) {
    m.flatPtr = decodedCol.template FlatValues<T>();
    if (layout == vec::DVecLayout::Constant) {
        *reinterpret_cast<T*>(m.constBuf) = decodedCol.template GetConstValue<T>();
    }
    m.ids = decodedCol.Ids();
    m.writer = layout == vec::DVecLayout::Flat
        ? &TaperFixedWriter<T, vec::DVecLayout::Flat>
        : layout == vec::DVecLayout::Constant
        ? &TaperFixedWriter<T, vec::DVecLayout::Constant>
        : &TaperFixedWriter<T, vec::DVecLayout::Dictionary>;
}

static ALWAYS_INLINE void SetupVarcharColMeta(TaperColMeta& m, const vec::DecodedVector& decodedCol,
    std::vector<RowContainer::StringViewStorage>& constStrCache, int32_t c, mem::SimpleArenaAllocator* arena) {
    auto* vec = decodedCol.Base();
    m.enc = vec->GetEncoding();
    if (m.enc == OMNI_ENCODING_CONST) {
        if (constStrCache[c].data == nullptr) {
            auto sv0 = static_cast<ConstVector<std::string_view>*>(vec)->GetConstValue();
            auto* buf = reinterpret_cast<char*>(arena->Allocate(sv0.size()));
            memcpy(buf, sv0.data(), sv0.size());
            constStrCache[c] = {buf, static_cast<uint32_t>(sv0.size())};
        }
        m.constStr = constStrCache[c];
        m.writer = &TaperVarcharWriter<OMNI_ENCODING_CONST>;
    } else if (m.enc == OMNI_DICTIONARY) {
        m.container = static_cast<Vector<DictionaryContainer<std::string_view>>*>(vec);
        m.writer = &TaperVarcharWriter<OMNI_DICTIONARY>;
    } else {
        m.container = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        m.writer = &TaperVarcharWriter<OMNI_FLAT>;
    }
}

template <typename T>
static ALWAYS_INLINE void AnalyzeRangeMinMax(const TaperColMeta& m, int32_t n, int32_t startRow,
    const uint8_t* an, int64_t* analyzeMin, int64_t* analyzeMax) {
    const T* fp = reinterpret_cast<const T*>(m.flatPtr);
    for (int32_t i = 0; i < n; ++i) {
        int32_t idx = startRow + i;
        if (an && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(an), idx)) continue;
        int64_t v = fp[idx];
        if (v < *analyzeMin) *analyzeMin = v;
        if (v > *analyzeMax) *analyzeMax = v;
    }
}

template <typename T>
static ALWAYS_INLINE void AnalyzeSingleMinMax(const TaperColMeta& m, int32_t idx,
    int64_t* analyzeMin, int64_t* analyzeMax) {
    int64_t v = reinterpret_cast<const T*>(m.flatPtr)[idx];
    if (v < *analyzeMin) *analyzeMin = v;
    if (v > *analyzeMax) *analyzeMax = v;
}

static ALWAYS_INLINE void BatchSetNullBits(char* base, int32_t count, int32_t fixedRowSize,
    const TaperColMeta& m, const uint64_t* nullFlags)
{
    for (int32_t i = 0; i < count; ++i) {
        if (nullFlags[i >> 6] & (1ull << (i & 63))) {
            base[i * fixedRowSize + m.nullByte] |= m.nullMask;
        }
    }
}

}  // anonymous namespace

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
        constStrCache_.assign(numCols_, {nullptr, 0});
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
                        if (constStrCache_[c].data == nullptr) {
                            sv = static_cast<ConstVector<std::string_view>*>(vec)->GetConstValue();
                            auto* buf = reinterpret_cast<char*>(arena_->Allocate(sv.size()));
                            memcpy(buf, sv.data(), sv.size());
                            constStrCache_[c] = {buf, static_cast<uint32_t>(sv.size())};
                        }
                        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, offset,
                            constStrCache_[c]);
                    } else if (enc == OMNI_DICTIONARY) {
                        sv = decodedCols[c].GetValue<std::string_view>(rowIdx);
                        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, offset,
                            {sv.data(), static_cast<uint32_t>(sv.size())});
                    } else {
                        sv = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec)
                                 ->GetValue(rowIdx);
                        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, offset,
                            {sv.data(), static_cast<uint32_t>(sv.size())});
                    }
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

    template <bool Analyze = false>
    void AppendRows(vec::DecodedVector* decodedCols, int32_t startRow, int32_t count, char** rows,
        int32_t analyzeCol = -1, int64_t* analyzeMin = nullptr, int64_t* analyzeMax = nullptr,
        bool* feasible = nullptr) {
        std::vector<TaperColMeta> colMeta(numCols_);
        for (int32_t c = 0; c < numCols_; ++c) {
            auto& m = colMeta[c];
            auto col = rows_->ColumnAt(c);
            m.offset = col.Offset();
            m.nullByte = col.NullByte();
            m.nullMask = col.NullMask();
            m.nulls = decodedCols[c].Nulls();
            auto layout = decodedCols[c].GetLayout();
            m.layout = static_cast<int32_t>(layout);
            switch (typeIds_[c]) {
                case type::OMNI_BYTE:
                case type::OMNI_BOOLEAN:
                    SetupFixedColMeta<int8_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_SHORT:
                    SetupFixedColMeta<int16_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_INT:
                case type::OMNI_DATE32:
                case type::OMNI_TIME32:
                case type::OMNI_FLOAT:
                    SetupFixedColMeta<int32_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_LONG:
                case type::OMNI_TIMESTAMP:
                case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE:
                case type::OMNI_TIME64:
                case type::OMNI_DATE64:
                    SetupFixedColMeta<int64_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_DECIMAL128:
                    SetupFixedColMeta<Decimal128>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_VARCHAR:
                case type::OMNI_CHAR:
                case type::OMNI_VARBINARY:
                    SetupVarcharColMeta(m, decodedCols[c], constStrCache_, c, arena_);
                    break;
                case type::OMNI_ARRAY:
                case type::OMNI_MAP:
                case type::OMNI_ROW:
                    m.arrayTypeId = typeIds_[c];
                    m.vecBase = decodedCols[c].Base();
                    m.arena = arena_;
                    m.writer = &TaperArrayWriter;
                    break;
            }
        }
        if constexpr (Analyze) {
            if (feasible && *feasible && analyzeCol >= 0 && analyzeCol < numCols_) {
                bool ok = true;
                switch (typeIds_[analyzeCol]) {
                    case type::OMNI_BYTE:
                    case type::OMNI_BOOLEAN:
                    case type::OMNI_SHORT:
                    case type::OMNI_INT:
                    case type::OMNI_DATE32:
                    case type::OMNI_TIME32:
                    case type::OMNI_FLOAT:
                    case type::OMNI_LONG:
                    case type::OMNI_TIMESTAMP:
                    case type::OMNI_DECIMAL64:
                    case type::OMNI_DOUBLE:
                    case type::OMNI_TIME64:
                    case type::OMNI_DATE64:
                        ok = decodedCols[analyzeCol].GetLayout() == vec::DVecLayout::Flat;
                        break;
                    default:
                        ok = false;
                        break;
                }
                *feasible = ok;
            }
        }
        // 批量路径:无 free 行时一次分配 count 行 + 批量填列(SVE/标量)+ null 统一置位
        if (!rows_->HasFreeRows()) {
            int32_t n = 0;
            char* base = rows_->NewRowBatch(count, &n);
            if (base != nullptr) {
                int32_t frs = rows_->FixedRowSize();
                int32_t w = (n + 63) / 64;
                std::vector<uint64_t> nullFlags(numCols_ * w, 0);
                uint64_t* flags = nullFlags.data();
                for (int32_t c = 0; c < numCols_; ++c) {
                    const auto& m = colMeta[c];
                    uint64_t* f = flags + c * w;
                    switch (typeIds_[c]) {
                        case type::OMNI_BYTE:
                        case type::OMNI_BOOLEAN:
                            BatchFillColumnDispatch<int8_t>(base, n, frs, startRow, m, f); break;
                        case type::OMNI_SHORT:
                            BatchFillColumnDispatch<int16_t>(base, n, frs, startRow, m, f); break;
                        case type::OMNI_INT:
                        case type::OMNI_DATE32:
                        case type::OMNI_TIME32:
                        case type::OMNI_FLOAT:
                            BatchFillColumnDispatch<int32_t>(base, n, frs, startRow, m, f); break;
                        case type::OMNI_LONG:
                        case type::OMNI_TIMESTAMP:
                        case type::OMNI_DECIMAL64:
                        case type::OMNI_DOUBLE:
                        case type::OMNI_TIME64:
                        case type::OMNI_DATE64:
                            BatchFillColumnDispatch<int64_t>(base, n, frs, startRow, m, f); break;
                        case type::OMNI_DECIMAL128:
                            BatchFillColumnDispatch<Decimal128>(base, n, frs, startRow, m, f); break;
                        default:
                            // 字符串/复杂:逐行 writer(标量)
                            for (int32_t i = 0; i < n; ++i) {
                                int32_t idx = startRow + i;
                                if (m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx)) {
                                    f[i >> 6] |= 1ull << (i & 63);
                                    continue;
                                }
                                m.writer(base + i * frs, m, idx);
                            }
                            break;
                    }
                }
                for (int32_t c = 0; c < numCols_; ++c) {
                    BatchSetNullBits(base, n, frs, colMeta[c], flags + c * w);
                }
                if constexpr (Analyze) {
                    if (feasible && *feasible && analyzeCol >= 0 && analyzeCol < numCols_ && analyzeMin && analyzeMax) {
                        const auto& m = colMeta[analyzeCol];
                        const uint8_t* an = m.nulls;
                        switch (typeIds_[analyzeCol]) {
                            case type::OMNI_BYTE:
                            case type::OMNI_BOOLEAN:
                                AnalyzeRangeMinMax<int8_t>(m, n, startRow, an, analyzeMin, analyzeMax);
                                break;
                            case type::OMNI_SHORT:
                                AnalyzeRangeMinMax<int16_t>(m, n, startRow, an, analyzeMin, analyzeMax);
                                break;
                            case type::OMNI_INT:
                            case type::OMNI_DATE32:
                            case type::OMNI_TIME32:
                            case type::OMNI_FLOAT:
                                AnalyzeRangeMinMax<int32_t>(m, n, startRow, an, analyzeMin, analyzeMax);
                                break;
                            case type::OMNI_LONG:
                            case type::OMNI_TIMESTAMP:
                            case type::OMNI_DECIMAL64:
                            case type::OMNI_DOUBLE:
                            case type::OMNI_TIME64:
                            case type::OMNI_DATE64:
                                AnalyzeRangeMinMax<int64_t>(m, n, startRow, an, analyzeMin, analyzeMax);
                                break;
                            default:
                                break;
                        }
                    }
                }
                return;
            }
        }
        for (int32_t i = 0; i < count; ++i) {
            char* row = rows_->NewRow();
            int32_t idx = startRow + i;
            for (int32_t c = 0; c < numCols_; ++c) {
                const TaperColMeta& m = colMeta[c];
                m.writer(row, m, idx);
                if constexpr (Analyze) {
                    if (c == analyzeCol && feasible && *feasible && analyzeMin && analyzeMax) {
                        if (!(m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx))) {
                            switch (typeIds_[c]) {
                                case type::OMNI_BYTE:
                                case type::OMNI_BOOLEAN:
                                    AnalyzeSingleMinMax<int8_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                case type::OMNI_SHORT:
                                    AnalyzeSingleMinMax<int16_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                case type::OMNI_INT:
                                case type::OMNI_DATE32:
                                case type::OMNI_TIME32:
                                case type::OMNI_FLOAT:
                                    AnalyzeSingleMinMax<int32_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                case type::OMNI_LONG:
                                case type::OMNI_TIMESTAMP:
                                case type::OMNI_DECIMAL64:
                                case type::OMNI_DOUBLE:
                                case type::OMNI_TIME64:
                                case type::OMNI_DATE64:
                                    AnalyzeSingleMinMax<int64_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                }
            }
        }
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

        {
        table_->ProbeBatch(keys, static_cast<uint32_t>(numKeys),
            [isNulls](uint32_t ki) { return isNulls[ki] != 0; },
            [&](uint32_t ki, char*) { chainHeads[ki] = nullptr; },
            [&](uint32_t ki, char* data, bool initFlag) {
                if (!initFlag) { chainHeads[ki] = GetRowPtr(data); }
            });
        }
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

    /// Transfer string container shared_ptrs from decoded vectors to keep backing data alive
    /// after the source VectorBatch is freed. Must be called once per vecBatch before FreeVecBatch.
    void HoldStringContainers(omniruntime::vec::DecodedVector* decodedCols) {
        constStrCache_.assign(numCols_, {nullptr, 0});
        for (int32_t c = 0; c < numCols_; ++c) {
            if (typeIds_[c] == type::OMNI_VARCHAR || typeIds_[c] == type::OMNI_CHAR ||
                typeIds_[c] == type::OMNI_VARBINARY) {
                auto* vec = decodedCols[c].Base();
                if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
                    continue; // Const data copied into arena, no container to hold
                }
                if (vec->GetEncoding() == OMNI_DICTIONARY) {
                    stringContainers_.push_back(
                        unsafe::UnsafeDictionaryVector::GetDictionaryOriginal<std::string_view>(
                            static_cast<Vector<DictionaryContainer<std::string_view>>*>(vec)));
                } else {
                    stringContainers_.push_back(
                        std::static_pointer_cast<void>(
                            unsafe::UnsafeStringVector::GetContainer(
                                static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec))));
                }
            }
        }
    }

    bool HasDuplicates() const { return rows_ && rows_->NumRows() > static_cast<int64_t>(table_->Size()); }
    uint32_t Size() const { return static_cast<uint32_t>(table_->Size()); }

private:
    int32_t numCols_ = 0;
    mem::SimpleArenaAllocator* arena_ = nullptr;
    std::vector<int32_t> typeIds_;
    std::unique_ptr<HashTable> table_;
    std::unique_ptr<RowContainer> rows_;
    std::vector<std::shared_ptr<void>> stringContainers_;
    std::vector<RowContainer::StringViewStorage> constStrCache_;
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
        constStrCache_.assign(numCols_, {nullptr, 0});
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
                    RowContainer::StoreValue<Decimal128>(row, offset,
                        decodedCols[c].GetValue<Decimal128>(rowIdx));
                    break;
                case type::OMNI_VARCHAR:
                case type::OMNI_CHAR:
                case type::OMNI_VARBINARY: {
                    auto* vec = decodedCols[c].Base();
                    auto enc = vec->GetEncoding();
                    std::string_view sv;
                    if (enc == OMNI_ENCODING_CONST) {
                        if (constStrCache_[c].data == nullptr) {
                            sv = static_cast<ConstVector<std::string_view>*>(vec)->GetConstValue();
                            auto* buf = reinterpret_cast<char*>(arena_->Allocate(sv.size()));
                            memcpy(buf, sv.data(), sv.size());
                            constStrCache_[c] = {buf, static_cast<uint32_t>(sv.size())};
                        }
                        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, offset,
                            constStrCache_[c]);
                    } else if (enc == OMNI_DICTIONARY) {
                        sv = decodedCols[c].GetValue<std::string_view>(rowIdx);
                        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, offset,
                            {sv.data(), static_cast<uint32_t>(sv.size())});
                    } else {
                        sv = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec)
                                 ->GetValue(rowIdx);
                        RowContainer::StoreValue<RowContainer::StringViewStorage>(row, offset,
                            {sv.data(), static_cast<uint32_t>(sv.size())});
                    }
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

    template <bool Analyze = false>
    void AppendRows(vec::DecodedVector* decodedCols, int32_t startRow, int32_t count, char** rows,
        int32_t analyzeCol = -1, int64_t* analyzeMin = nullptr, int64_t* analyzeMax = nullptr,
        bool* feasible = nullptr) {
        std::vector<TaperColMeta> colMeta(numCols_);
        for (int32_t c = 0; c < numCols_; ++c) {
            auto& m = colMeta[c];
            auto col = rows_->ColumnAt(c);
            m.offset = col.Offset();
            m.nullByte = col.NullByte();
            m.nullMask = col.NullMask();
            m.nulls = decodedCols[c].Nulls();
            auto layout = decodedCols[c].GetLayout();
            m.layout = static_cast<int32_t>(layout);
            switch (typeIds_[c]) {
                case type::OMNI_BYTE:
                case type::OMNI_BOOLEAN:
                    SetupFixedColMeta<int8_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_SHORT:
                    SetupFixedColMeta<int16_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_INT:
                case type::OMNI_DATE32:
                case type::OMNI_TIME32:
                case type::OMNI_FLOAT:
                    SetupFixedColMeta<int32_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_LONG:
                case type::OMNI_TIMESTAMP:
                case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE:
                case type::OMNI_TIME64:
                case type::OMNI_DATE64:
                    SetupFixedColMeta<int64_t>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_DECIMAL128:
                    SetupFixedColMeta<Decimal128>(m, decodedCols[c], layout);
                    break;
                case type::OMNI_VARCHAR:
                case type::OMNI_CHAR:
                case type::OMNI_VARBINARY:
                    SetupVarcharColMeta(m, decodedCols[c], constStrCache_, c, arena_);
                    break;
                case type::OMNI_ARRAY:
                case type::OMNI_MAP:
                case type::OMNI_ROW:
                    m.arrayTypeId = typeIds_[c];
                    m.vecBase = decodedCols[c].Base();
                    m.arena = arena_;
                    m.writer = &TaperArrayWriter;
                    break;
            }
        }
        if constexpr (Analyze) {
            if (feasible && *feasible && analyzeCol >= 0 && analyzeCol < numCols_) {
                bool ok = true;
                switch (typeIds_[analyzeCol]) {
                    case type::OMNI_BYTE:
                    case type::OMNI_BOOLEAN:
                    case type::OMNI_SHORT:
                    case type::OMNI_INT:
                    case type::OMNI_DATE32:
                    case type::OMNI_TIME32:
                    case type::OMNI_FLOAT:
                    case type::OMNI_LONG:
                    case type::OMNI_TIMESTAMP:
                    case type::OMNI_DECIMAL64:
                    case type::OMNI_DOUBLE:
                    case type::OMNI_TIME64:
                    case type::OMNI_DATE64:
                        ok = decodedCols[analyzeCol].GetLayout() == vec::DVecLayout::Flat;
                        break;
                    default:
                        ok = false;
                        break;
                }
                *feasible = ok;
            }
        }
        for (int32_t i = 0; i < count; ++i) {
            char* row = rows_->NewRow();
            int32_t idx = startRow + i;
            for (int32_t c = 0; c < numCols_; ++c) {
                const TaperColMeta& m = colMeta[c];
                m.writer(row, m, idx);
                if constexpr (Analyze) {
                    if (c == analyzeCol && feasible && *feasible && analyzeMin && analyzeMax) {
                        if (!(m.nulls && BitUtil::IsBitSet(reinterpret_cast<const uint64_t*>(m.nulls), idx))) {
                            switch (typeIds_[c]) {
                                case type::OMNI_BYTE:
                                case type::OMNI_BOOLEAN:
                                    AnalyzeSingleMinMax<int8_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                case type::OMNI_SHORT:
                                    AnalyzeSingleMinMax<int16_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                case type::OMNI_INT:
                                case type::OMNI_DATE32:
                                case type::OMNI_TIME32:
                                case type::OMNI_FLOAT:
                                    AnalyzeSingleMinMax<int32_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                case type::OMNI_LONG:
                                case type::OMNI_TIMESTAMP:
                                case type::OMNI_DECIMAL64:
                                case type::OMNI_DOUBLE:
                                case type::OMNI_TIME64:
                                case type::OMNI_DATE64:
                                    AnalyzeSingleMinMax<int64_t>(m, idx, analyzeMin, analyzeMax);
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                }
            }
        }
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

    /// Compare two build rows by key. Callers guarantee both rows have non-null
    /// keys (null key rows are filtered by EmplaceBatch's isNulls filter).
    bool CompareKeys(const char* row1, const char* row2) const {
        for (int32_t colIdx : keyColIndices_) {
            auto col = rows_->ColumnAt(colIdx);
            auto off = col.Offset();
            auto typeId = typeIds_[colIdx];
            switch (typeId) {
                case type::OMNI_BYTE: case type::OMNI_BOOLEAN:
                    if (RowContainer::ReadValue<int8_t>(row1, off) !=
                        RowContainer::ReadValue<int8_t>(row2, off)) return false;
                    break;
                case type::OMNI_SHORT:
                    if (RowContainer::ReadValue<int16_t>(row1, off) !=
                        RowContainer::ReadValue<int16_t>(row2, off)) return false;
                    break;
                case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_TIME32: case type::OMNI_FLOAT:
                    if (RowContainer::ReadValue<int32_t>(row1, off) !=
                        RowContainer::ReadValue<int32_t>(row2, off)) return false;
                    break;
                case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                case type::OMNI_DOUBLE: case type::OMNI_DATE64: case type::OMNI_TIME64:
                    if (RowContainer::ReadValue<int64_t>(row1, off) !=
                        RowContainer::ReadValue<int64_t>(row2, off)) return false;
                    break;
                case type::OMNI_DECIMAL128:
                    if (RowContainer::ReadValue<Decimal128>(row1, off) !=
                        RowContainer::ReadValue<Decimal128>(row2, off)) return false;
                    break;
                case type::OMNI_VARCHAR: case type::OMNI_CHAR: case type::OMNI_VARBINARY: {
                    auto sv1 = RowContainer::ReadValue<RowContainer::StringViewStorage>(const_cast<char*>(row1), off);
                    auto sv2 = RowContainer::ReadValue<RowContainer::StringViewStorage>(const_cast<char*>(row2), off);
                    if (sv1.data == sv2.data && sv1.size == sv2.size) break;
                    if (sv1.data == nullptr || sv2.data == nullptr) return false;
                    if (sv1.size != sv2.size) return false;
                    if (memcmp(sv1.data, sv2.data, sv1.size) != 0) return false;
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

    // Join 阶段key列数据永远不会为Null
    template <typename T>
    int32_t BatchCompareProbeColumn(int32_t colIdx, int32_t count, int32_t offset,
                                    uint32_t nullByte, uint8_t nullMask, int32_t *indices, int32_t &idxFrom,
                                    const int32_t *positions)
    {
        const auto& decoded = probeDecodedCols_[colIdx];
        if (decoded.GetLayout() == vec::DVecLayout::Flat) {
            const T* flatVals = unsafe::UnsafeVector::GetRawValues(
                static_cast<Vector<T> *>(decoded.Base()));
            for (int32_t i = idxFrom; i < count; ++i) {
                if (i + PrefetchHelper::kPrefetchDistance < count) {
                    auto* prefetchRow =
                        workingUpdateRows_[i + PrefetchHelper::kPrefetchDistance];
                    __builtin_prefetch(prefetchRow + offset);
                }
                int32_t ki = indices[i];
                char* row = workingUpdateRows_[i];
                if (RowContainer::ReadValue<T>(row, offset) != flatVals[positions[ki]]) {
                    std::swap(indices[i], indices[idxFrom]);
                    std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                    idxFrom++;
                }
            }
            return idxFrom;
        }
        auto constVal = static_cast<ConstVector<T> *>(decoded.Base())->GetConstValue();
        for (int32_t i = idxFrom; i < count; ++i) {
            if (i + PrefetchHelper::kPrefetchDistance < count) {
                auto* prefetchRow =
                    workingUpdateRows_[i + PrefetchHelper::kPrefetchDistance];
                __builtin_prefetch(prefetchRow + offset);
            }
            int32_t ki = indices[i];
            char* row = workingUpdateRows_[i];
            if (RowContainer::ReadValue<T>(row, offset) != constVal) {
                std::swap(indices[i], indices[idxFrom]);
                std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                idxFrom++;
            }
        }
        return idxFrom;
    }

    template <typename T>
    int32_t BatchCompareProbeColumnDict(int32_t colIdx, int32_t count, int32_t offset,
                                        uint32_t nullByte, uint8_t nullMask, int32_t *indices, int32_t &idxFrom,
                                        const int32_t *positions)
    {
        const auto& decoded = probeDecodedCols_[colIdx];
        const T* flatVals = decoded.template FlatValues<T>();
        const int32_t* ids = decoded.Ids();
        for (int32_t i = idxFrom; i < count; ++i) {
            if (i + PrefetchHelper::kPrefetchDistance < count) {
                auto* prefetchRow =
                    workingUpdateRows_[i + PrefetchHelper::kPrefetchDistance];
                __builtin_prefetch(prefetchRow + offset);
            }
            int32_t ki = indices[i];
            char* row = workingUpdateRows_[i];
            if (RowContainer::ReadValue<T>(row, offset) != flatVals[ids[positions[ki]]]) {
                std::swap(indices[i], indices[idxFrom]);
                std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                idxFrom++;
            }
        }
        return idxFrom;
    }

    int32_t BatchCompareVarcharColumn(int32_t colIdx, int32_t count, int32_t offset,
                                      uint32_t nullByte, uint8_t nullMask, int32_t *indices, int32_t &idxFrom,
                                      const int32_t *positions)
    {
        const auto& decoded = probeDecodedCols_[colIdx];
        if (decoded.GetLayout() == vec::DVecLayout::Flat) {
            for (int32_t i = idxFrom; i < count; ++i) {
                PrefetchHelper::PrefetchRowString(workingUpdateRows_.data(), offset, nullByte, nullMask, count, i);
                int32_t ki = indices[i];
                char* row = workingUpdateRows_[i];
                auto storage = RowContainer::ReadValue<RowContainer::StringViewStorage>(row, offset);
                std::string_view sv =
                    static_cast<Vector<LargeStringContainer<std::string_view>>*>(decoded.Base())
                        ->GetValue(positions[ki]);
                if (storage.size != sv.size() || memcmp(storage.data, sv.data(), storage.size) != 0) {
                    std::swap(indices[i], indices[idxFrom]);
                    std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                    idxFrom++;
                }
            }
            return idxFrom;
        } else if (decoded.GetLayout() == vec::DVecLayout::Constant) {
            std::string_view sv = static_cast<ConstVector<std::string_view> *>(decoded.Base())->GetConstValue();
            for (int32_t i = idxFrom; i < count; ++i) {
                PrefetchHelper::PrefetchRowString(workingUpdateRows_.data(), offset, nullByte, nullMask, count, i);
                int32_t ki = indices[i];
                char* row = workingUpdateRows_[i];
                auto storage = RowContainer::ReadValue<RowContainer::StringViewStorage>(row, offset);
                if (storage.size != sv.size() || memcmp(storage.data, sv.data(), storage.size) != 0) {
                    std::swap(indices[i], indices[idxFrom]);
                    std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                    idxFrom++;
                }
            }
            return idxFrom;
        }
        for (int32_t i = idxFrom; i < count; ++i) {
            PrefetchHelper::PrefetchRowString(workingUpdateRows_.data(), offset, nullByte, nullMask, count, i);
            int32_t ki = indices[i];
            char* row = workingUpdateRows_[i];
            auto storage = RowContainer::ReadValue<RowContainer::StringViewStorage>(row, offset);
            std::string_view sv =
                static_cast<Vector<DictionaryContainer<std::string_view>>*>(decoded.Base())
                    ->GetValue(positions[ki]);
            if (storage.size != sv.size() || memcmp(storage.data, sv.data(), storage.size) != 0) {
                std::swap(indices[i], indices[idxFrom]);
                std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                idxFrom++;
            }
        }
        return idxFrom;
    }

    int32_t BatchCompareComplexColumn(int32_t typeId, int32_t colIdx, int32_t count, int32_t offset,
        uint32_t nullByte, uint8_t nullMask, int32_t* indices, int32_t& idxFrom, const int32_t* positions)
    {
        const auto& decoded = probeDecodedCols_[colIdx];
        auto comparator = vectorComparatorCenter[static_cast<size_t>(typeId)];
        for (int32_t i = idxFrom; i < count; ++i) {
            int32_t ki = indices[i];
            char* row = workingUpdateRows_[i];
            bool rowNull = RowContainer::IsNullAt(row, nullByte, nullMask);
            bool probeNull = decoded.IsNull(positions[ki]);
            if (rowNull != probeNull) {
                std::swap(indices[i], indices[idxFrom]);
                std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                idxFrom++;
                continue;
            }
            if (rowNull) continue;
            auto* storedPtr = RowContainer::ReadValue<char*>(row, offset);
            if (storedPtr == nullptr) {
                std::swap(indices[i], indices[idxFrom]);
                std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                idxFrom++;
                continue;
            }
            uint8_t* bytePtr = reinterpret_cast<uint8_t*>(const_cast<char*>(storedPtr));
            if (!comparator(*decoded.Base(), positions[ki], bytePtr)) {
                std::swap(indices[i], indices[idxFrom]);
                std::swap(workingUpdateRows_[i], workingUpdateRows_[idxFrom]);
                idxFrom++;
            }
        }
        return idxFrom;
    }

    /// Per-row probe comparison using the pre-decoded probeDecodedCols_.
    /// Used by pass3 linear probing where rows arrive one at a time.
    bool CompareRowWithProbeDecoded(const char* row, int32_t probeHashColCount, int32_t probePosition) const {
        for (int32_t k = 0; k < probeHashColCount; ++k) {
            int32_t colIdx = keyColIndices_[k];
            auto col = rows_->ColumnAt(colIdx);
            bool rowNull = RowContainer::IsNullAt(const_cast<char*>(row), col.NullByte(), col.NullMask());
            const auto& decoded = probeDecodedCols_[k];
            bool probeNull = decoded.IsNull(probePosition);
            if (rowNull != probeNull) return false;
            if (rowNull) continue;
            auto typeId = decoded.GetTypeId();
            auto w = PackedBitWidth(static_cast<int32_t>(typeId));
            int32_t colOff = col.Offset();
            switch (w) {
                case 8:
                    if (RowContainer::ReadValue<int8_t>(const_cast<char*>(row), colOff) !=
                        decoded.GetValue<int8_t>(probePosition)) return false;
                    break;
                case 16:
                    if (RowContainer::ReadValue<int16_t>(const_cast<char*>(row), colOff) !=
                        decoded.GetValue<int16_t>(probePosition)) return false;
                    break;
                case 32:
                    if (RowContainer::ReadValue<int32_t>(const_cast<char*>(row), colOff) !=
                        decoded.GetValue<int32_t>(probePosition)) return false;
                    break;
                case 64:
                    if (RowContainer::ReadValue<int64_t>(const_cast<char*>(row), colOff) !=
                        decoded.GetValue<int64_t>(probePosition)) return false;
                    break;
                case 128:
                    if (RowContainer::ReadValue<Decimal128>(const_cast<char*>(row), colOff)
                        != decoded.GetValue<Decimal128>(probePosition)) return false;
                    break;
                default:
                    if (w == 0) {
                        if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR ||
                            typeId == type::OMNI_VARBINARY) {
                            auto rowSv = RowContainer::ReadValue<RowContainer::StringViewStorage>(
                                const_cast<char*>(row), colOff);
                            if (rowSv.data == nullptr) return false;
                            std::string_view sv =
                                static_cast<Vector<LargeStringContainer<std::string_view>>*>(decoded.Base())
                                    ->GetValue(probePosition);
                            if (rowSv.size != sv.size()) return false;
                            if (memcmp(rowSv.data, sv.data(), rowSv.size) != 0) return false;
                        } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_MAP || typeId == type::OMNI_ROW) {
                            auto* storedPtr = RowContainer::ReadValue<char*>(const_cast<char*>(row), colOff);
                            if (storedPtr == nullptr) return false;
                            uint8_t* bytePtr = reinterpret_cast<uint8_t*>(const_cast<char*>(storedPtr));
                            auto comparator = vectorComparatorCenter[static_cast<size_t>(typeId)];
                            auto* probeVec = decoded.Base();
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

    /// Move decoded probe columns into the handler so ProbeBatch skips re-decoding. Call before ProbeBatch.
    void SetProbeDecodedColumns(std::vector<vec::DecodedVector>& decodedCols, int32_t rowCount) {
        probeDecodedCols_ = std::move(decodedCols);
        probeDecodedRowCount_ = rowCount;
    }

    // --- ProbeBatch --------------------------------------
    // 第 1 轮: ProbeBatch(hash-only) → 记录 workingUpdateIndices_/ChunkData_
    // 第 2 轮: CompareRowWithProbe → 匹配填 chainHead / 不匹配紧缩等待第 3 轮
    // 第 3 轮: Probe
    void ProbeBatch(
        const int64_t *keys,
        int32_t numKeys,
        omniruntime::vec::BaseVector **probeHashColumns,
        int32_t probeHashColCount,
        const int32_t *probePositions,
        char **chainHeads,
        const char *isNulls)
    {
        if (UNLIKELY(!table_)) return;

        workingUpdateIndices_.resize(numKeys);
        workingUpdateRows_.resize(numKeys);
        workingUpdateCount_ = 0;

        // Decoded probe columns are set externally via SetProbeDecodedColumns before ProbeBatch.
        // 第 1 轮：ProbeBatch，只按 hash tag 匹配，不做 key 比较
        {
        table_->ProbeBatch(keys, static_cast<uint32_t>(numKeys),
            [isNulls](uint32_t ki) { return isNulls[ki] != 0; },
            [&](uint32_t ki, char*) { chainHeads[ki] = nullptr; },
            [this](uint32_t ki, char* data, bool initFlag) {
                if(!initFlag){
                    workingUpdateIndices_[workingUpdateCount_] = ki;
                    workingUpdateRows_[workingUpdateCount_] = GetRowPtr(data);
                    ++workingUpdateCount_;
                }
            });
        }

        // 第 2 轮：批量列比较 — 逐列对所有命中行一次性比较，mismatch 紧缩到前部
        {
        int32_t idxFrom = 0;
        int32_t cmpCount = workingUpdateCount_;
        for (int32_t k = 0; k < probeHashColCount; ++k) {
            if (idxFrom >= cmpCount) break;
            int32_t colIdx = keyColIndices_[k];
            auto col = rows_->ColumnAt(colIdx);
            auto& decoded = probeDecodedCols_[k];
            auto typeId = decoded.GetTypeId();
            auto w = PackedBitWidth(static_cast<int32_t>(typeId));
            auto layout = decoded.GetLayout();

            if (w == 0) {
                if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
                    idxFrom = BatchCompareVarcharColumn(k, cmpCount, col.Offset(), col.NullByte(), col.NullMask(),
                        workingUpdateIndices_.data(), idxFrom, probePositions);
                } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_MAP || typeId == type::OMNI_ROW) {
                    idxFrom = BatchCompareComplexColumn(typeId, k, cmpCount, col.Offset(), col.NullByte(),
                        col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                }
            } else {
                if (layout == vec::DVecLayout::Dictionary) {
                    switch (w) {
                    case 8:
                        idxFrom = BatchCompareProbeColumnDict<int8_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 16:
                        idxFrom = BatchCompareProbeColumnDict<int16_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 32:
                        idxFrom = BatchCompareProbeColumnDict<int32_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 64:
                        idxFrom = BatchCompareProbeColumnDict<int64_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 128:
                        idxFrom = BatchCompareProbeColumnDict<Decimal128>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    default:
                        throw std::runtime_error("Unsupported column type in dictionary layout");
                        break;
                    }
                } else {
                    switch (w) {
                    case 8:
                        idxFrom = BatchCompareProbeColumn<int8_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 16:
                        idxFrom = BatchCompareProbeColumn<int16_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 32:
                        idxFrom = BatchCompareProbeColumn<int32_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 64:
                        idxFrom = BatchCompareProbeColumn<int64_t>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    case 128:
                        idxFrom = BatchCompareProbeColumn<Decimal128>(k, cmpCount, col.Offset(), col.NullByte(),
                            col.NullMask(), workingUpdateIndices_.data(), idxFrom, probePositions);
                        break;
                    default:
                        throw std::runtime_error("Unsupported column type in flat layout");
                        break;
                    }
                }
            }
        }

        // 匹配的行 (idxFrom..cmpCount-1) → chainHeads
        for (int32_t i = idxFrom; i < cmpCount; ++i) {
            uint32_t ki = workingUpdateIndices_[i];
            chainHeads[ki] = workingUpdateRows_[i];
        }
        workingUpdateCount_ = idxFrom;  // 不匹配的去 pass3
        }

        // 第 3 轮：对剩余 mismatch 行线性探测同 hash 的其他 slot
        {
        for (int32_t i = 0; i < workingUpdateCount_; ++i) {
            uint32_t ki = workingUpdateIndices_[i];
            table_->Probe(keys[ki],
                [&](const int64_t&, TaperHashTableChunk& chunk, uint8_t slot) {
                    auto* row = GetRowPtr(table_->GetChunkValue(chunk, slot).buf);
                    return CompareRowWithProbeDecoded(row, probeHashColCount, probePositions[ki]);
                },
                [&](char*) { chainHeads[ki] = nullptr; },
                [&](char* data, bool initFlag) {
                    if (!initFlag) chainHeads[ki] = GetRowPtr(data);
                });
        }
        }
        workingUpdateCount_ = 0;
    }

    void EmplaceBatch(const int64_t* keys, char** rows, int32_t numRows, const bool* isNulls = nullptr) {
        auto payloadOff = rows_->PayloadOffset();

        workingUpdateIndices_.resize(numRows);
        workingUpdateChunkData_.resize(numRows);
        workingUpdateRows_.resize(numRows);
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
            if (i + PrefetchHelper::kPrefetchDistance < workingUpdateCount_) {
                __builtin_prefetch(
                    GetRowPtr(workingUpdateChunkData_[i + PrefetchHelper::kPrefetchDistance]));
            }
            int32_t rowIdx = workingUpdateIndices_[i];
            // 用时解算:二轮中 SetRowPtr 会覆盖 slot,必须读"当前"slot 值
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

    /// Transfer string container shared_ptrs from decoded vectors to keep backing data alive
    /// after the source VectorBatch is freed. Must be called once per vecBatch before FreeVecBatch.
    void HoldStringContainers(omniruntime::vec::DecodedVector* decodedCols) {
        constStrCache_.assign(numCols_, {nullptr, 0});
        for (int32_t c = 0; c < numCols_; ++c) {
            if (typeIds_[c] == type::OMNI_VARCHAR || typeIds_[c] == type::OMNI_CHAR ||
                typeIds_[c] == type::OMNI_VARBINARY) {
                auto* vec = decodedCols[c].Base();
                if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
                    continue; // Const data copied into arena, no container to hold
                }
                if (vec->GetEncoding() == OMNI_DICTIONARY) {
                    stringContainers_.push_back(
                        unsafe::UnsafeDictionaryVector::GetDictionaryOriginal<std::string_view>(
                            static_cast<Vector<DictionaryContainer<std::string_view>>*>(vec)));
                } else {
                    stringContainers_.push_back(
                        std::static_pointer_cast<void>(
                            unsafe::UnsafeStringVector::GetContainer(
                                static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec))));
                }
            }
        }
    }

    bool HasDuplicates() const { return rows_ && rows_->NumRows() > static_cast<int64_t>(table_->Size()); }
    uint32_t Size() const { return static_cast<uint32_t>(table_->Size()); }

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
    std::vector<char*> workingUpdateRows_;
    int32_t workingUpdateCount_ = 0;
    std::vector<std::shared_ptr<void>> stringContainers_;
    std::vector<RowContainer::StringViewStorage> constStrCache_;
    std::vector<vec::DecodedVector> probeDecodedCols_;
    int32_t probeDecodedRowCount_ = 0;
};

} // namespace op
} // namespace omniruntime
