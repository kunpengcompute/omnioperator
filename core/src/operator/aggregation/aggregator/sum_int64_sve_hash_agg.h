/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: AArch64 SVE batch kernel for Spark sum(bigint) HashAgg group updates
 */
#ifndef OMNI_RUNTIME_SUM_INT64_SVE_HASH_AGG_H
#define OMNI_RUNTIME_SUM_INT64_SVE_HASH_AGG_H

#include <cstddef>
#include <cstdint>

#include "aggregator.h"
#include "state_flag_operation.h"

namespace omniruntime {
namespace op {

/// indicesMode for SveHashAggSumInt64Update (aligned with Bolt BatchReadView).
enum class SumInt64SveIndicesMode : int32_t {
    FLAT = 1,      // values[row]
    CONSTANT = 2,  // constantValue for every active row
    DICTIONARY = 3 // values[dictIds[row]]
};

/// nullsMode for SveHashAggSumInt64Update (aligned with Bolt BatchReadView::nullsMode).
/// Omni null polarity: bit=1 means NULL (inverted vs Velox when loading predicates).
enum class SumInt64SveNullsMode : int32_t {
    NONE = 0,       // no nulls → all lanes valid
    FLAT = 1,       // row-level null bitmap; nullBits points at byte-aligned row0
    CONSTANT = 2,   // constant null from bit0 of nullBits
    DICTIONARY = 3  // nulls on dictionary entries; gather via dictIds
};

/// True when Linux aarch64 runtime has SVE and vector length is 256-bit (svcntb()==32).
bool CanUseSveHashAggSumInt64();

/// Pack `#pragma pack(1)` layout matching SumFlatIMAggregator::SumFlatState for int64.
#pragma pack(push, 1)
struct SumInt64FlatState {
    int64_t value;
    AggValueState valueState;
};
#pragma pack(pop)

/**
 * Wrapping int64 HashAgg group sum update (Spark sum semantics).
 *
 * Null bitmaps use Omni polarity (bit=1 = null). Kernel inverts when forming the
 * accumulate predicate (aligned with 563.diff sveInputNullMaskForMode).
 *
 * @param rowStates     per-row group AggregateState* (offset applied inside)
 * @param aggStateOffset byte offset to SumInt64FlatState within each group row
 * @param values        flat/dict dictionary data, or pointer to one constant when CONSTANT
 * @param dictIds       dictionary ids when DICTIONARY / DICTIONARY nulls; otherwise unused
 * @param nullBits      Omni null bitmap; nullptr when nullsMode==NONE.
 *                      For FLAT: byte-aligned pointer to row0 bit.
 *                      For CONSTANT: bitmap containing the const null bit.
 *                      For DICTIONARY: dictionary-entry null bitmap.
 * @param rowCount      number of rows (== rowStates count for the slice)
 * @param indicesMode   flat / constant / dictionary value access
 * @param nullsMode     how to interpret nullBits
 */
void SveHashAggSumInt64Update(AggregateState **rowStates, size_t aggStateOffset, const int64_t *values,
    const int32_t *dictIds, const uint8_t *nullBits, int32_t rowCount, SumInt64SveIndicesMode indicesMode,
    SumInt64SveNullsMode nullsMode);

} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_SUM_INT64_SVE_HASH_AGG_H
