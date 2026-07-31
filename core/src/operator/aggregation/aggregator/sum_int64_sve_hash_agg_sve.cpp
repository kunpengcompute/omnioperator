/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: AArch64 SVE batch kernel for Spark sum(bigint) HashAgg group updates.
 * Aligned with Bolt SumAggregateSparkInt64SubOpSve (563.diff): null predicate via
 * sveInputNullMaskForMode + scalar lane accumulate (value += and valueState=NORMAL).
 * Omni has no Velox nullByte/numNulls_, so Bolt distinct-group clearNull is omitted.
 * Compiled only on Linux aarch64 with -march=armv8-a+sve (see operator/CMakeLists.txt).
 */
#include "sum_int64_sve_hash_agg.h"

#include <arm_sve.h>

#if defined(__linux__)
#include <sys/auxv.h>
#endif

namespace omniruntime {
namespace op {
namespace {

constexpr uint64_t kSupportedSveVectorBytes = 32;

template <typename T, typename U> constexpr T RoundUp(T value, U factor)
{
    return (value + (factor - 1)) / factor * factor;
}

template <typename T> bool IsBitSet(const T *bits, uint64_t idx)
{
    return bits[idx / (sizeof(bits[0]) * 8)] & (static_cast<T>(1) << (idx & ((sizeof(bits[0]) * 8) - 1)));
}

/// Omni: bit=1 means NULL. Returns accumulate predicate (1 = non-null / valid).
inline svbool_t SveInputValidMaskForMode(const uint8_t *nulls, int32_t byteIndex, SumInt64SveNullsMode nullsMode,
    const int32_t *dictIds, int32_t length)
{
    svbool_t pg;
    if (nullsMode == SumInt64SveNullsMode::NONE) {
        return svptrue_b8();
    }
    if (nullsMode == SumInt64SveNullsMode::FLAT) {
        // Load Omni null bits (1=null), then invert → valid mask.
        __asm__ __volatile__("ldr %0, [%1]" : "=Upl"(pg) : "r"(&(nulls[byteIndex])) : "memory");
        pg = svnot_b_z(svptrue_b8(), pg);
        return pg;
    }
    if (nullsMode == SumInt64SveNullsMode::CONSTANT) {
        // Omni bit0 set → all null → no accumulate.
        if (!IsBitSet(reinterpret_cast<const uint64_t *>(nulls), 0)) {
            return svptrue_b8();
        }
        return svpfalse();
    }
    if (nullsMode == SumInt64SveNullsMode::DICTIONARY) {
        // Gather dictionary null bits (Omni: 1=null); pack valid (bit==0) into predicate.
        // Structure mirrors Bolt nullsMode==3 (563.diff); polarity inverted for Omni.
        svuint32_t onc = svdup_u32(1);
        svuint32_t inv = svindex_u32(0, 1);
        svuint32_t pow = svlsl_m(svptrue_b32(), onc, inv);
        uint8_t tmpValid[4] = {0};
        const uint32_t *null32ptr = reinterpret_cast<const uint32_t *>(nulls);
        const uint32_t *dictU32 = reinterpret_cast<const uint32_t *>(dictIds);

        svuint32_t posv, idxbufv, bufv, offsetv;
        svbool_t validvec, pg1;

        pg1 = svwhilelt_b32(byteIndex * 8, length);
        posv = svld1(pg1, dictU32 + byteIndex * 8);
        idxbufv = svlsr_x(pg1, posv, 5);
        bufv = svld1_gather_index(pg1, null32ptr, idxbufv);
        offsetv = svand_m(pg1, posv, 0b11111);
        bufv = svlsr_m(pg1, bufv, offsetv);
        bufv = svand_m(pg1, bufv, 0x1);
        validvec = svcmpeq(pg1, bufv, 0);
        tmpValid[0] = __builtin_expect(svptest_any(pg1, validvec), 1) ? static_cast<uint8_t>(svaddv(validvec, pow)) : 0;

        pg1 = svwhilelt_b32(byteIndex * 8 + 8, length);
        posv = svld1(pg1, dictU32 + byteIndex * 8 + 8);
        idxbufv = svlsr_x(pg1, posv, 5);
        bufv = svld1_gather_index(pg1, null32ptr, idxbufv);
        offsetv = svand_m(pg1, posv, 0b11111);
        bufv = svlsr_m(pg1, bufv, offsetv);
        bufv = svand_m(pg1, bufv, 0x1);
        validvec = svcmpeq(pg1, bufv, 0);
        tmpValid[1] = __builtin_expect(svptest_any(pg1, validvec), 1) ? static_cast<uint8_t>(svaddv(validvec, pow)) : 0;

        pg1 = svwhilelt_b32(byteIndex * 8 + 16, length);
        posv = svld1(pg1, dictU32 + byteIndex * 8 + 16);
        idxbufv = svlsr_x(pg1, posv, 5);
        bufv = svld1_gather_index(pg1, null32ptr, idxbufv);
        offsetv = svand_m(pg1, posv, 0b11111);
        bufv = svlsr_m(pg1, bufv, offsetv);
        bufv = svand_m(pg1, bufv, 0x1);
        validvec = svcmpeq(pg1, bufv, 0);
        tmpValid[2] = __builtin_expect(svptest_any(pg1, validvec), 1) ? static_cast<uint8_t>(svaddv(validvec, pow)) : 0;

        pg1 = svwhilelt_b32(byteIndex * 8 + 24, length);
        posv = svld1(pg1, dictU32 + byteIndex * 8 + 24);
        idxbufv = svlsr_x(pg1, posv, 5);
        bufv = svld1_gather_index(pg1, null32ptr, idxbufv);
        offsetv = svand_m(pg1, posv, 0b11111);
        bufv = svlsr_m(pg1, bufv, offsetv);
        bufv = svand_m(pg1, bufv, 0x1);
        validvec = svcmpeq(pg1, bufv, 0);
        tmpValid[3] = __builtin_expect(svptest_any(pg1, validvec), 1) ? static_cast<uint8_t>(svaddv(validvec, pow)) : 0;

        __asm__ __volatile__("ldr %0, [%1]" : "=Upl"(pg) : "r"(tmpValid) : "memory");
        return pg;
    }
    return svpfalse();
}

/// Scalar accumulate for one active row: value += and EMPTY→NORMAL (Omni state model).
/// indicesMode is fixed for the whole batch — specialize so the hot loop has no mode branch.
template <SumInt64SveIndicesMode Mode>
inline void AccumulateGroupSum(int64_t constantValue, const int64_t *values, const int32_t *dictIds, int32_t row,
    AggregateState **rowStates, size_t aggStateOffset)
{
    int64_t rowValue;
    if constexpr (Mode == SumInt64SveIndicesMode::DICTIONARY) {
        rowValue = values[dictIds[row]];
    } else if constexpr (Mode == SumInt64SveIndicesMode::CONSTANT) {
        rowValue = constantValue;
    } else {
        rowValue = values[row];
    }
    auto *st = reinterpret_cast<SumInt64FlatState *>(rowStates[row] + aggStateOffset);
    st->value += rowValue;
    st->valueState = AggValueState::NORMAL;
}

template <SumInt64SveIndicesMode Mode>
inline void AccumulateFullBlock(int64_t constantValue, const int64_t *values, const int32_t *dictIds, int32_t rowBase,
    AggregateState **rowStates, size_t aggStateOffset)
{
    constexpr int32_t kRowsPerBlock = 32;
    for (int32_t row = rowBase; row < rowBase + kRowsPerBlock; ++row) {
        AccumulateGroupSum<Mode>(constantValue, values, dictIds, row, rowStates, aggStateOffset);
    }
}

template <SumInt64SveIndicesMode Mode>
inline void AccumulateActiveRows(uint32_t activeBits, int64_t constantValue, const int64_t *values,
    const int32_t *dictIds, int32_t rowBase, AggregateState **rowStates, size_t aggStateOffset)
{
    while (activeBits != 0) {
        const int32_t lane = __builtin_ctz(activeBits);
        AccumulateGroupSum<Mode>(constantValue, values, dictIds, rowBase + lane, rowStates, aggStateOffset);
        activeBits &= activeBits - 1;
    }
}

template <SumInt64SveIndicesMode Mode>
inline void SveHashAggBatchUpdateGroupSumsImpl(AggregateState **rowStates, size_t aggStateOffset, const int64_t *values,
    const int32_t *dictIds, const uint8_t *nullBits, int32_t rowCount, SumInt64SveNullsMode nullsMode)
{
    const int32_t begin = 0;
    const int32_t end = rowCount;
    const int32_t firstWord = RoundUp(begin, 32) == begin ? begin : RoundUp(begin, 32) - 32;
    const int32_t lastWord = RoundUp(end, 32);
    const int64_t constantValue = Mode == SumInt64SveIndicesMode::CONSTANT ? values[0] : 0;

    for (int32_t count = firstWord; count + 32 <= lastWord; count += 32) {
        const int32_t arr8Index = count / 8;
        svbool_t validMask = SveInputValidMaskForMode(nullBits, arr8Index, nullsMode, dictIds, end);
        svbool_t mask = svand_b_z(svptrue_b8(), validMask, svwhilelt_b8(count, end));
        // VL is runtime-gated to 256 bits, so a b8 predicate occupies exactly 32 bits in memory.
        uint32_t activeBits = 0;
        __asm__ __volatile__("str %1, [%0]" : : "r"(&activeBits), "Upl"(mask) : "memory");
        if (activeBits == 0) {
            continue;
        } else if (activeBits == UINT32_MAX) {
            AccumulateFullBlock<Mode>(constantValue, values, dictIds, count, rowStates, aggStateOffset);
        } else {
            AccumulateActiveRows<Mode>(activeBits, constantValue, values, dictIds, count, rowStates, aggStateOffset);
        }
    }
}

inline void SveHashAggBatchUpdateGroupSums(AggregateState **rowStates, size_t aggStateOffset, const int64_t *values,
    const int32_t *dictIds, const uint8_t *nullBits, int32_t rowCount, SumInt64SveIndicesMode indicesMode,
    SumInt64SveNullsMode nullsMode)
{
    // Dispatch once per batch; hot accumulate path is mode-specialized.
    switch (indicesMode) {
        case SumInt64SveIndicesMode::CONSTANT:
            SveHashAggBatchUpdateGroupSumsImpl<SumInt64SveIndicesMode::CONSTANT>(rowStates, aggStateOffset, values,
                dictIds, nullBits, rowCount, nullsMode);
            break;
        case SumInt64SveIndicesMode::DICTIONARY:
            SveHashAggBatchUpdateGroupSumsImpl<SumInt64SveIndicesMode::DICTIONARY>(rowStates, aggStateOffset, values,
                dictIds, nullBits, rowCount, nullsMode);
            break;
        case SumInt64SveIndicesMode::FLAT:
        default:
            SveHashAggBatchUpdateGroupSumsImpl<SumInt64SveIndicesMode::FLAT>(rowStates, aggStateOffset, values, dictIds,
                nullBits, rowCount, nullsMode);
            break;
    }
}

} // namespace

bool CanUseSveHashAggSumInt64()
{
    static const bool kCanUse = []() {
#if defined(__linux__)
#ifndef HWCAP_SVE
        constexpr unsigned long kHwcapSve = 1UL << 22;
#else
        constexpr unsigned long kHwcapSve = HWCAP_SVE;
#endif
        const unsigned long hwcap = getauxval(AT_HWCAP);
        if ((hwcap & kHwcapSve) == 0) {
            return false;
        }
#endif
        return svcntb() == kSupportedSveVectorBytes;
    }();
    return kCanUse;
}

void SveHashAggSumInt64Update(AggregateState **rowStates, size_t aggStateOffset, const int64_t *values,
    const int32_t *dictIds, const uint8_t *nullBits, int32_t rowCount, SumInt64SveIndicesMode indicesMode,
    SumInt64SveNullsMode nullsMode)
{
    if (rowCount <= 0 || rowStates == nullptr || values == nullptr) {
        return;
    }
    if (indicesMode == SumInt64SveIndicesMode::DICTIONARY && dictIds == nullptr) {
        return;
    }
    if (nullsMode == SumInt64SveNullsMode::DICTIONARY && (nullBits == nullptr || dictIds == nullptr)) {
        return;
    }
    if (nullsMode != SumInt64SveNullsMode::NONE && nullBits == nullptr) {
        return;
    }
    SveHashAggBatchUpdateGroupSums(rowStates, aggStateOffset, values, dictIds, nullBits, rowCount, indicesMode,
        nullsMode);
}

} // namespace op
} // namespace omniruntime
