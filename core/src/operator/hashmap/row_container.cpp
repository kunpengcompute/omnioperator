/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: Row Container for Aggregation Implementation
 */

#include "row_container.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "vector/unsafe_vector.h"
#include "type/data_type.h"
#include "type/decimal128.h"
#include "util/bit_util.h"
#include "util/debug.h"
#include "operator/hashmap/vector_marshaller.h"

#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif

namespace omniruntime::op {

RowContainer::RowContainer(const std::vector<int32_t>& keyTypeSizes,
                           int32_t numKeys,
                           int32_t aggStateSize,
                           mem::SimpleArenaAllocator& pool)
    : numKeys(numKeys),
      aggStateSize(aggStateSize),
      pool(pool)
{
    // Calculate row layout: [key columns] [null bits] [AggState] [padding]
    int32_t offset = 0;

    // Step 1: Key column offsets
    offsets.resize(numKeys);
    nullOffsets.resize(numKeys);

    for (int32_t i = 0; i < numKeys; ++i) {
        offsets[i] = offset;
        offset += keyTypeSizes[i];
    }

    // Ensure minimum sizeof(void*) for the free-list next pointer at offset 0
    offset = std::max<int32_t>(offset, static_cast<int32_t>(sizeof(void*)));

    // Step 2: Null bits block
    // Null bits use absolute bit positions from the start of the null block.
    // The null block starts right after key data at the current offset.
    // Only key column null bits are stored (no agg null bit, no free flag).

    int32_t nullBitPos = 0;
    for (int32_t i = 0; i < numKeys; ++i) {
        nullOffsets[i] = nullBitPos;
        nullBitPos++;
    }

    // Calculate null bytes (no padding to 8-bit boundary)
    nullBytes = BitUtil::Nbytes(nullBitPos);
    nullBlockStart = offset;
    offset += nullBytes;

    // Step 3: AggState offset (no alignment padding)
    aggStateOffset = offset;
    offset += aggStateSize;

    fixedRowSize = offset;

    // Build RowColumn descriptors
    rowColumns.reserve(numKeys);
    for (int32_t i = 0; i < numKeys; ++i) {
        int32_t absNullBitPos = nullBlockStart * 8 + nullOffsets[i];
        rowColumns.emplace_back(offsets[i], absNullBitPos);
    }
}

char* RowContainer::NewRow()
{
    ++numRows;
    char* row = nullptr;

    if (firstFreeRow != nullptr) {
        row = firstFreeRow;
        firstFreeRow = *reinterpret_cast<char**>(row);
        --numFreeRows;
    } else {
        if (batchRemaining == 0) {
            batchPtr = reinterpret_cast<char*>(pool.Allocate(kBatchSize * fixedRowSize));
            batchRemaining = kBatchSize;
        }
        row = batchPtr + (kBatchSize - batchRemaining) * fixedRowSize;
        --batchRemaining;
        allocations.emplace_back(row, 1);
    }

    return InitializeRow(row);
}

char* RowContainer::NewRowBatch(int32_t count, int32_t* outCount)
{
    if (firstFreeRow != nullptr) {
        *outCount = 0;
        return nullptr;
    }
    char* base = reinterpret_cast<char*>(pool.Allocate(count * fixedRowSize));
    memset(base, 0, count * fixedRowSize);
    numRows += count;
    allocations.emplace_back(base, count);
    *outCount = count;
    return base;
}

char* RowContainer::InitializeRow(char* row)
{
    memset(row, 0, fixedRowSize);
    return row;
}

int32_t RowContainer::ListRows(RowContainerIterator* iter, int32_t maxRows, char** rows)
{
    int32_t count = 0;
    int32_t numAllocations = static_cast<int32_t>(allocations.size());

    while (count < maxRows && iter->allocationIndex < numAllocations) {
        auto [base, n] = allocations[iter->allocationIndex];
        int32_t take = std::min(n - iter->rowOffset, maxRows - count);
        for (int32_t i = 0; i < take; ++i) {
            rows[count++] = base + (iter->rowOffset + i) * fixedRowSize;
        }
        iter->rowOffset += take;
        if (iter->rowOffset >= n) {
            iter->allocationIndex++;
            iter->rowOffset = 0;
        }
    }

    if (iter->allocationIndex >= numAllocations) {
        iter->allocationIndex = std::numeric_limits<int32_t>::max();
    }

    return count;
}

#ifdef __ARM_FEATURE_SVE
template <typename T>
static void SveExtractColumnImpl(char** rows, int32_t totalRows, int32_t offset,
                                  int32_t nullByte, uint8_t nullMask,
                                  Vector<T>* vec)
{
    T* outValues = vec::unsafe::UnsafeVector::GetRawValues(vec);
    uint64_t* outNulls = reinterpret_cast<uint64_t*>(vec::unsafe::UnsafeBaseVector::GetNulls(vec));
    bool hasNull = false;

    svbool_t pgAll = svptrue_b64();

    for (int32_t i = 0; i < totalRows;) {
        svbool_t pg = svwhilelt_b64_s64((int64_t)i, (int64_t)totalRows);
        int32_t activeCount = svcntp_b64(pgAll, pg);

        // Row pointer gather + nullptr detection (LEFT/ANTI unmatch probe rows).
        svuint64_t vIdx = svindex_u64(i, 1);
        svuint64_t vPtrOffsets = svlsl_n_u64_x(pg, vIdx, 3);
        svuint64_t vRowPtrs = svld1_gather_offset_u64(pg, vPtrOffsets, (uint64_t)rows);
        svbool_t vNullPtr = svcmpeq_n_u64(pg, vRowPtrs, 0);
        svbool_t vNonNullPtr = svnot_b_z(pg, vNullPtr);

        // Build-null read: predicated gather so null-pointer rows are not
        // dereferenced (avoids gather at 0+nullByte garbage address).
        svuint64_t vNullAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)nullByte);
        svuint64_t vRowNullByte = svld1ub_gather_u64(vNonNullPtr, vNullAddr);
        svbool_t vBuildNull = svcmpne_n_u64(pg, svand_n_u64_x(pg, vRowNullByte, (uint64_t)nullMask), 0);

        svbool_t vNullRow = svorr_b_z(pg, vNullPtr, vBuildNull);
        if (svptest_any(pgAll, vNullRow)) {
            hasNull = true;
        }

        // Value gather (predicated on non-null pointers) + null select + store.
        svuint64_t vValueAddr = svadd_n_u64_x(pg, vRowPtrs, (uint64_t)offset);
        if constexpr (std::is_same_v<T, int64_t>) {
            svint64_t v = svld1_gather_s64(vNonNullPtr, vValueAddr);
            svst1_s64(pg, outValues + i, svsel_s64(vNullRow, svdup_n_s64(0), v));
        } else if constexpr (std::is_same_v<T, double>) {
            svfloat64_t v = svld1_gather_f64(vNonNullPtr, vValueAddr);
            svst1_f64(pg, outValues + i, svsel_f64(vNullRow, svdup_n_f64(0.0), v));
        } else if constexpr (std::is_same_v<T, int32_t>) {
            svint64_t v = svld1sw_gather_s64(vNonNullPtr, vValueAddr);
            int64_t tmp[32];
            svst1_s64(pg, tmp, svsel_s64(vNullRow, svdup_n_s64(0), v));
            for (int32_t j = 0; j < activeCount; j++) outValues[i + j] = static_cast<T>(tmp[j]);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            svint64_t v = svld1sh_gather_s64(vNonNullPtr, vValueAddr);
            int64_t tmp[32];
            svst1_s64(pg, tmp, svsel_s64(vNullRow, svdup_n_s64(0), v));
            for (int32_t j = 0; j < activeCount; j++) outValues[i + j] = static_cast<T>(tmp[j]);
        } else if constexpr (std::is_same_v<T, int8_t>) {
            svint64_t v = svld1sb_gather_s64(vNonNullPtr, vValueAddr);
            int64_t tmp[32];
            svst1_s64(pg, tmp, svsel_s64(vNullRow, svdup_n_s64(0), v));
            for (int32_t j = 0; j < activeCount; j++) outValues[i + j] = static_cast<T>(tmp[j]);
        } else {
            // bool and other exotic types: scalar fallback (handles null bitmap).
            for (int32_t j = 0; j < activeCount; j++) {
                if (rows[i + j] == nullptr || RowContainer::IsNullAt(rows[i + j], nullByte, nullMask)) {
                    outValues[i + j] = T{};
                    BitUtil::SetBit(outNulls, i + j);
                    hasNull = true;
                } else {
                    outValues[i + j] = RowContainer::ReadValue<T>(rows[i + j], offset);
                }
            }
            i += activeCount;
            continue;
        }

        // Null bitmap set: scalar SetBit (vectorized scatter of multiple lanes
        // targeting the same word would overwrite each other, losing null bits).
        if (hasNull) {
            for (int32_t j = 0; j < activeCount; j++) {
                char* r = rows[i + j];
                if (r == nullptr || RowContainer::IsNullAt(r, nullByte, nullMask)) {
                    BitUtil::SetBit(outNulls, i + j);
                }
            }
        }

        i += activeCount;
    }

    if (hasNull) {
        vec->SetNullFlag(true);
    }
}
#endif


// 提取固定宽度列到输出向量（非SVE路径）
template <typename T>
static inline void ExtractFixedWidthColumnImpl(char** rows, int32_t totalRows, int32_t offset,
    int32_t nullByte, int32_t nullMask, Vector<T>* vec)
{
    for (int32_t i = 0; i < totalRows; ++i) {
        if (LIKELY(i + PrefetchHelper::kPrefetchDistance < totalRows &&
                   rows[i + PrefetchHelper::kPrefetchDistance] != nullptr)) {
            PrefetchHelper::PrefetchRow(rows[i + PrefetchHelper::kPrefetchDistance], offset, nullByte);
        }
        if (rows[i] == nullptr || RowContainer::IsNullAt(rows[i], nullByte, nullMask)) {
            vec->SetNull(i);
        } else {
            vec->SetValue(i, RowContainer::ReadValue<T>(rows[i], offset));
        }
    }
}


void RowContainer::ExtractColumn(char** rows, int32_t totalRows, int32_t colIdx,
                                  vec::BaseVector* outputVector)
{
    if (colIdx >= numKeys) {
        return; // AggState columns are extracted separately
    }

    auto Col = rowColumns[colIdx];
    auto offset = Col.Offset();
    auto nullByte = Col.NullByte();
    auto nullMask = Col.NullMask();

    // Dispatch based on output vector type
    auto typeId = outputVector->GetTypeId();

    switch (typeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32:
        case type::OMNI_TIME32: {
            auto* vec = static_cast<Vector<int32_t>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<int32_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<int32_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DECIMAL64:
        case type::OMNI_DATE64:
        case type::OMNI_TIME64: {
            auto* vec = static_cast<Vector<int64_t>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<int64_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<int64_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_SHORT: {
            auto* vec = static_cast<Vector<int16_t>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<int16_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<int16_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_BYTE: {
            auto* vec = static_cast<Vector<int8_t>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<int8_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<int8_t>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_DOUBLE: {
            auto* vec = static_cast<Vector<double>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<double>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<double>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_FLOAT: {
            auto* vec = static_cast<Vector<float>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<float>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<float>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_BOOLEAN: {
            auto* vec = static_cast<Vector<bool>*>(outputVector);
#ifdef __ARM_FEATURE_SVE
            SveExtractColumnImpl<bool>(rows, totalRows, offset, nullByte, nullMask, vec);
#else
            ExtractFixedWidthColumnImpl<bool>(rows, totalRows, offset, nullByte, nullMask, vec);
#endif
            break;
        }
        case type::OMNI_DECIMAL128: {
            auto* vec = static_cast<Vector<Decimal128>*>(outputVector);
            ExtractFixedWidthColumnImpl<Decimal128>(rows, totalRows, offset, nullByte, nullMask, vec);
            break;
        }
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
        case type::OMNI_VARBINARY: {
            auto* vec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(outputVector);
            for (int32_t i = 0; i < totalRows; ++i) {
                if (LIKELY(i + PrefetchHelper::kPrefetchDistance < totalRows &&
                           rows[i + PrefetchHelper::kPrefetchDistance] != nullptr)) {
                    PrefetchHelper::PrefetchRow(rows[i + PrefetchHelper::kPrefetchDistance], offset, nullByte);
                }
                if (rows[i] == nullptr || IsNullAt(rows[i], nullByte, nullMask)) {
                    vec->SetNull(i);
                } else {
                    auto storage = RowContainer::ReadValue<RowContainer::StringViewStorage>(rows[i], offset);
                    vec->SetValue(i, std::string_view(storage.data, storage.size));
                }
            }
            break;
        }
        case type::OMNI_ARRAY:
        case type::OMNI_MAP:
        case type::OMNI_ROW: {
            auto* rowVec = (typeId == type::OMNI_ROW) ? static_cast<vec::RowVector*>(outputVector) : nullptr;
            auto* arrVec = (typeId == type::OMNI_ARRAY) ? static_cast<vec::ArrayVector*>(outputVector) : nullptr;
            auto* mapVec = (typeId == type::OMNI_MAP) ? static_cast<vec::MapVector*>(outputVector) : nullptr;
            auto deser = complexVectorDeSerializerCenter[static_cast<DataTypeId>(typeId)];
            for (int32_t i = 0; i < totalRows; ++i) {
                if (LIKELY(i + PrefetchHelper::kPrefetchDistance < totalRows &&
                   rows[i + PrefetchHelper::kPrefetchDistance] != nullptr)) {
                    PrefetchHelper::PrefetchRow(rows[i + PrefetchHelper::kPrefetchDistance], offset, nullByte);
                }
                if (rows[i] == nullptr || IsNullAt(rows[i], nullByte, nullMask)) {
                    if (rowVec) rowVec->SetNull(static_cast<int64_t>(i));
                    else if (arrVec) arrVec->SetNull(static_cast<int64_t>(i));
                    else if (mapVec) mapVec->SetNull(static_cast<int64_t>(i));
                } else {
                    auto* dataPtr = ReadValue<char*>(rows[i], offset);
                    if (UNLIKELY(dataPtr == nullptr)) {
                        if (rowVec) rowVec->SetNull(static_cast<int64_t>(i));
                        else if (arrVec) arrVec->SetNull(static_cast<int64_t>(i));
                        else if (mapVec) mapVec->SetNull(static_cast<int64_t>(i));
                        continue;
                    }
                    const char* pos = dataPtr;
                    deser(outputVector, i, pos);
                }
            }
            break;
        }
        default:
            break;
    }
}

bool RowContainer::Equals(const char* row, int32_t colIdx, vec::BaseVector* vector, int32_t rowIdx)
{
    if (colIdx >= numKeys) {
        return true; // AggState columns are not compared
    }

    auto Col = rowColumns[colIdx];
    auto offset = Col.Offset();
    auto nullByte = Col.NullByte();
    auto nullMask = Col.NullMask();

    if (IsNullAt(row, nullByte, nullMask)) {
        return vector->IsNull(rowIdx);
    }

    if (vector->IsNull(rowIdx)) {
        return false;
    }

    // Compare values by type
    auto typeId = vector->GetTypeId();
    switch (typeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32:
        case type::OMNI_TIME32:
            return ReadValue<int32_t>(row, offset) == static_cast<Vector<int32_t>*>(vector)->GetValue(rowIdx);
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DECIMAL64:
        case type::OMNI_DATE64:
        case type::OMNI_TIME64:
            return ReadValue<int64_t>(row, offset) == static_cast<Vector<int64_t>*>(vector)->GetValue(rowIdx);
        case type::OMNI_SHORT:
            return ReadValue<int16_t>(row, offset) == static_cast<Vector<int16_t>*>(vector)->GetValue(rowIdx);
        case type::OMNI_BYTE:
            return ReadValue<int8_t>(row, offset) == static_cast<Vector<int8_t>*>(vector)->GetValue(rowIdx);
        case type::OMNI_DOUBLE:
            return ReadValue<double>(row, offset) == static_cast<Vector<double>*>(vector)->GetValue(rowIdx);
        case type::OMNI_FLOAT:
            return ReadValue<float>(row, offset) == static_cast<Vector<float>*>(vector)->GetValue(rowIdx);
        case type::OMNI_BOOLEAN:
            return (ReadValue<int8_t>(row, offset) != 0) == static_cast<Vector<bool>*>(vector)->GetValue(rowIdx);
        case type::OMNI_DECIMAL128:
            return ReadValue<Decimal128>(row, offset) == static_cast<Vector<Decimal128>*>(vector)->GetValue(rowIdx);
        default:
            return false; // Complex types need serialized comparison
    }
}

} // namespace omniruntime::op
