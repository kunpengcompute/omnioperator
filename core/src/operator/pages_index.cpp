/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: pages index implementations
 */
#include <algorithm>
#include <cstring>
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "type/data_type.h"
#include "radix_sort.h"
#include "pages_index.h"
#include "simd/func/quick_sort_simd.h"
#include "operator/util/operator_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace omniruntime::op {
using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
using DictionaryVarcharVector = Vector<DictionaryContainer<std::string_view>>;

static constexpr int32_t QUICK_SORT_SMALL_LEN = 16;
static constexpr int32_t QUICK_SORT_BIG_LEN = 64;
static constexpr int32_t QUICK_SORT_STEP_SIZE = 8;
static constexpr int32_t SWAP_STEP_SIZE = 8;
static constexpr int32_t QUICK_SORT_MIDDLE = 2;

template <type::DataTypeId dataTypeId>
void ConstructVector(uint64_t *vaStart, int32_t length, BaseVector **inputVecBatch, bool hasNull, bool hasDictionary,
    BaseVector *outputVector, int32_t outputIndex);

template <type::DataTypeId dataTypeId>
void ConstructVectorRadixSort(const uint8_t *vaStart, int32_t length, BaseVector **inputVectors, bool hasNull,
    bool hasDictionary, uint32_t radixRowWidth, BaseVector *outputVector);

// function implements for class PagesIndex
PagesIndex::PagesIndex(const DataTypes &types)
    : typesCount(types.GetSize()), hasDictionaries(typesCount, false),
      hasNulls(typesCount, false), dataTypes(types)
{}

void ALWAYS_INLINE Swap(int64_t *values, uint64_t *addresses, int32_t a, int32_t b)
{
    auto tmpValue = values[a];
    auto tmpAddr = addresses[a];
    values[a] = values[b];
    addresses[a] = addresses[b];
    values[b] = tmpValue;
    addresses[b] = tmpAddr;
}

__attribute__((noinline)) void VectorSwap(int64_t *values, uint64_t *addresses, int32_t from, int32_t l, int32_t s)
{
    int32_t i = 0;
    for (; i < s - SWAP_STEP_SIZE; i += SWAP_STEP_SIZE, from += SWAP_STEP_SIZE, l += SWAP_STEP_SIZE) {
        Swap(values, addresses, from, l);
        Swap(values, addresses, from + 1, l + 1);
        Swap(values, addresses, from + 2, l + 2);
        Swap(values, addresses, from + 3, l + 3);
        Swap(values, addresses, from + 4, l + 4);
        Swap(values, addresses, from + 5, l + 5);
        Swap(values, addresses, from + 6, l + 6);
        Swap(values, addresses, from + 7, l + 7);
    }
    for (; i < s; i++, from++, l++) {
        Swap(values, addresses, from, l);
    }
}

void PagesIndex::AddVecBatch(omniruntime::vec::VectorBatch *vecBatch)
{
    inputVecBatches.emplace_back(vecBatch);
    rowCount += vecBatch->GetRowCount();
}

void PagesIndex::Prepare()
{
    const size_t vecBatchCount = inputVecBatches.size();
    const uint32_t columnCount = this->typesCount;

    this->valueAddresses = new uint64_t[this->rowCount];
    std::fill(hasDictionaries.begin(), hasDictionaries.end(), false);
    std::fill(hasNulls.begin(), hasNulls.end(), false);
    this->columns = new BaseVector **[columnCount];
    for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
        this->columns[colIdx] = new BaseVector *[vecBatchCount];
    }

    int32_t valueIndex = 0;
    for (size_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = inputVecBatches[vecBatchIdx];
        const auto vecBatchRowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
        // generate value address.
        for (uint32_t rowIdx = 0; rowIdx < vecBatchRowCount; rowIdx++) {
            const uint64_t valueAddress = EncodeSyntheticAddress(vecBatchIdx, rowIdx);
            this->valueAddresses[valueIndex++] = valueAddress;
        }

        // put vectors to a collector.
        for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
            auto* vector = vecBatch->Get(static_cast<int32_t>(colIdx));
            if (vector->HasNull()) {
                hasNulls[colIdx] = true;
            }
            if (vector->GetEncoding() == OMNI_DICTIONARY) {
                hasDictionaries[colIdx] = true;
            }
            this->columns[colIdx][vecBatchIdx] = vector;
        }
    }
}

template void PagesIndex::PrepareRadixSort<type::OMNI_LONG>(const bool ascending, const bool nullsFirst,
    const uint32_t sortCol);
template void PagesIndex::PrepareRadixSort<type::OMNI_INT>(const bool ascending, const bool nullsFirst,
    const uint32_t sortCol);
template void PagesIndex::PrepareRadixSort<type::OMNI_SHORT>(const bool ascending, const bool nullsFirst,
    const uint32_t sortCol);
template void PagesIndex::PrepareRadixSort<type::OMNI_BOOLEAN>(const bool ascending, const bool nullsFirst,
    const uint32_t sortCol);
template void PagesIndex::PrepareRadixSort<type::OMNI_DECIMAL64>(const bool ascending, const bool nullsFirst,
    const uint32_t sortCol);
template void PagesIndex::PrepareRadixSort<type::OMNI_BYTE>(const bool ascending, const bool nullsFirst,
    const uint32_t sortCol);

template <DataTypeId typeId>
void PagesIndex::PrepareRadixSort(const bool ascending, const bool nullsFirst, const uint32_t sortCol)
{
    using T = typename NativeType<typeId>::type;

    const size_t vecBatchCount = inputVecBatches.size();
    const uint32_t columnCount = this->typesCount;
    std::fill(hasDictionaries.begin(), hasDictionaries.end(), false);
    std::fill(hasNulls.begin(), hasNulls.end(), false);
    this->columns = new BaseVector **[columnCount];
    for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
        this->columns[colIdx] = new BaseVector *[vecBatchCount];
    }
    for (size_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = inputVecBatches[vecBatchIdx];
        // put vectors to a collector.
        for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
            auto vector = vecBatch->Get(colIdx);
            this->columns[colIdx][vecBatchIdx] = vector;
            if (vector->GetEncoding() == OMNI_DICTIONARY) {
                hasDictionaries[colIdx] = true;
            }
            if (colIdx == sortCol) {
                totalNullCount += vector->GetNullCount();
            }
            if (vector->HasNull()) {
                hasNulls[colIdx] = true;
            }
        }
    }
    radixValueWidth = sizeof(T);
    bool hasNegative = true;
    if constexpr (typeId == OMNI_LONG) {
        int64_t tmp = 0;
        for (size_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
            auto *col = static_cast<Vector<T> *>(columns[sortCol][vecBatchIdx]); // only one column
            const uint32_t vecBatchRowCount = inputVecBatches[vecBatchIdx]->GetRowCount();
            for (uint32_t rowIdx = 0; rowIdx < vecBatchRowCount; ++rowIdx) {
                tmp |= col->GetValue(rowIdx);
            }
        }
        // __builtin_clzl(static_cast<uint64_t>(tmp)) gets number of leading zeros in tmp
        radixValueWidth = LONG_NBYTES - __builtin_clzl(static_cast<uint64_t>(tmp)) / CHAR_BIT;
        hasNegative = tmp < 0;
    }
    radixRowWidth = radixValueWidth + INT_NBYTES + SHORT_NBYTES;
    const bool hasNull = (totalNullCount != 0);
    if (!hasNull && !hasNegative) {
        FillRadixDataChunk<typeId, false, false>(sortCol, nullsFirst);
    } else if (!hasNull && hasNegative) {
        FillRadixDataChunk<typeId, false, true>(sortCol, nullsFirst);
    } else if (hasNull && !hasNegative) {
        FillRadixDataChunk<typeId, true, false>(sortCol, nullsFirst);
    } else {
        FillRadixDataChunk<typeId, true, true>(sortCol, nullsFirst);
    }
}

template <DataTypeId typeId, bool hasNull, bool hasNegative>
ALWAYS_INLINE void PagesIndex::FillRadixDataChunk(const int32_t sortCol, const bool nullsFirst)
{
    using T = typename NativeType<typeId>::type;
    static constexpr uint8_t signFlipMask = 128;
    this->radixComboRow.resize((this->rowCount + 1) * this->radixRowWidth, 0);
    static constexpr uint32_t signByte = sizeof(T) - 1;
    uint8_t *rowPtr =
        (hasNull && nullsFirst) ? radixComboRow.data() + totalNullCount * radixRowWidth : radixComboRow.data();
    uint8_t *nullPtr = (hasNull && nullsFirst) ?
        radixComboRow.data() :
        radixComboRow.data() + (rowCount - totalNullCount) * radixRowWidth;

    const size_t vecBatchCount = inputVecBatches.size();
    for (size_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = inputVecBatches[vecBatchIdx];
        const auto vecBatchRowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
        auto *col = static_cast<Vector<T> *>(columns[sortCol][vecBatchIdx]); // only one column
        for (uint32_t rowIdx = 0; rowIdx < vecBatchRowCount; rowIdx++) {
            if constexpr (hasNull) {
                if (col->IsNull(rowIdx)) {
                    auto tmpNullPtr = nullPtr + radixValueWidth;
                    *reinterpret_cast<uint16_t *>(tmpNullPtr) = static_cast<uint16_t>(vecBatchIdx);
                    *reinterpret_cast<uint32_t *>(tmpNullPtr + SHORT_NBYTES) = rowIdx;
                    nullPtr = tmpNullPtr + SHORT_NBYTES + INT_NBYTES;
                    continue;
                }
            }
            const auto value = col->GetValue(rowIdx);
            *reinterpret_cast<T *>(rowPtr) = value;
            auto tmpRowPtr = rowPtr + radixValueWidth;
            *reinterpret_cast<uint16_t *>(tmpRowPtr) = static_cast<uint16_t>(vecBatchIdx);
            *reinterpret_cast<uint32_t *>(tmpRowPtr + SHORT_NBYTES) = rowIdx;
            if constexpr (hasNegative) {
                rowPtr[signByte] ^= signFlipMask;
            }
            rowPtr = tmpRowPtr + SHORT_NBYTES + INT_NBYTES;
        }
    }
}

static bool ALWAYS_INLINE OnlyEqualVarChar(int64_t leftValue, uint32_t leftLength, int64_t rightValue,
    uint32_t rightLength)
{
    return (leftLength != rightLength) ? false : (memcmp((void *)rightValue, (void *)leftValue, leftLength) == 0);
}

template <int32_t sortAscending>
static int32_t ALWAYS_INLINE OnlyCompareVarChar(int64_t leftValue, uint32_t leftLength, int64_t rightValue,
    uint32_t rightLength)
{
    if constexpr (sortAscending == 1) {
        int32_t result = memcmp((void *)leftValue, (void *)rightValue, std::min(leftLength, rightLength));
        if (result != 0) {
            return result;
        }
        return leftLength - rightLength;
    } else {
        int32_t result = memcmp((void *)rightValue, (void *)leftValue, std::min(rightLength, leftLength));
        if (result != 0) {
            return result;
        }
        return rightLength - leftLength;
    }
}

template <int32_t sortAscending>
void QuickSortVarCharSmall(int64_t *values, std::vector<uint32_t> &varcharLength, uint64_t *addresses, int32_t from,
    int32_t to)
{
    for (int32_t i = from + 1; i < to; ++i) {
        const int64_t iPtr = values[i];
        const uint32_t iLength = varcharLength[i];
        const uint64_t iAddr = addresses[i];
        int32_t j = i - 1;
        while (j >= from && (OnlyCompareVarChar<sortAscending>(values[j], varcharLength[j], iPtr, iLength) > 0)) {
            values[j + 1] = values[j];
            varcharLength[j + 1] = varcharLength[j];
            addresses[j + 1] = addresses[j];
            --j;
        }
        values[j + 1] = iPtr;
        varcharLength[j + 1] = iLength;
        addresses[j + 1] = iAddr;
    }
}

template <int32_t sortAscending>
int32_t Median3VarChar(int64_t *values, std::vector<uint32_t> &varcharLength, int32_t a, int32_t b, int32_t c)
{
    const int32_t ab = OnlyCompareVarChar<sortAscending>(values[a], varcharLength[a], values[b], varcharLength[b]);
    const int32_t ac = OnlyCompareVarChar<sortAscending>(values[a], varcharLength[a], values[c], varcharLength[c]);
    const int32_t bc = OnlyCompareVarChar<sortAscending>(values[b], varcharLength[b], values[c], varcharLength[c]);
    return ((ab < 0) ? (bc < 0 ? b : ac < 0 ? c : a) : (bc > 0 ? b : ac > 0 ? c : a));
}

template <int32_t sortAscending>
int32_t NO_INLINE GetMedianPositionVarChar(int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from,
    int32_t to, int32_t len)
{
    int32_t l = from;
    int32_t n = to - 1;
    int32_t m = from + len / QUICK_SORT_MIDDLE;
    if (len > QUICK_SORT_BIG_LEN) {
        const int32_t s = len / QUICK_SORT_STEP_SIZE;
        l = Median3VarChar<sortAscending>(values, varcharLength, l, l + s, l + QUICK_SORT_MIDDLE * s);
        m = Median3VarChar<sortAscending>(values, varcharLength, m - s, m, m + s);
        n = Median3VarChar<sortAscending>(values, varcharLength, n - QUICK_SORT_MIDDLE * s, n - s, n);
    }
    return Median3VarChar<sortAscending>(values, varcharLength, l, m, n);
}

// bigger NCHUNK leads to ROB stall and wasted comparison due to b > c
static constexpr int32_t NCHUNK = 16;
// bigger NSTEP leads to reg spilling and hence increased inst count
static constexpr int32_t NSTEP = 12;
// Currently the comparison using subtraction includes 9 insts. ROB-size=96, 96/9=10
static constexpr int32_t NMAX_SIZE = NCHUNK * NSTEP;

template <int32_t sortAscending>
int32_t ALWAYS_INLINE GetNextCompareLeftVarChar(int32_t *compareTmp, int32_t &k, int32_t &limit, int32_t b,
    int32_t c, int64_t *values, const std::vector<uint32_t> &varcharLength, int64_t pivotValue, uint32_t pivotLength)
{
    if (LIKELY(k < limit)) {
        return compareTmp[k++];
    }
    k = 1;
    // use up
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = b;
    int32_t j = 0;
    for (; j < limit - NSTEP; i += NSTEP, j += NSTEP) {
        compareTmp[j] = OnlyCompareVarChar<sortAscending>(values[i], varcharLength[i], pivotValue, pivotLength);
        compareTmp[j + 1] =
            OnlyCompareVarChar<sortAscending>(values[i + 1], varcharLength[i + 1], pivotValue, pivotLength);
        compareTmp[j + 2] =
            OnlyCompareVarChar<sortAscending>(values[i + 2], varcharLength[i + 2], pivotValue, pivotLength);
        compareTmp[j + 3] =
            OnlyCompareVarChar<sortAscending>(values[i + 3], varcharLength[i + 3], pivotValue, pivotLength);
        compareTmp[j + 4] =
            OnlyCompareVarChar<sortAscending>(values[i + 4], varcharLength[i + 4], pivotValue, pivotLength);
        compareTmp[j + 5] =
            OnlyCompareVarChar<sortAscending>(values[i + 5], varcharLength[i + 5], pivotValue, pivotLength);
        compareTmp[j + 6] =
            OnlyCompareVarChar<sortAscending>(values[i + 6], varcharLength[i + 6], pivotValue, pivotLength);
        compareTmp[j + 7] =
            OnlyCompareVarChar<sortAscending>(values[i + 7], varcharLength[i + 7], pivotValue, pivotLength);
        compareTmp[j + 8] =
            OnlyCompareVarChar<sortAscending>(values[i + 8], varcharLength[i + 8], pivotValue, pivotLength);
        compareTmp[j + 9] =
            OnlyCompareVarChar<sortAscending>(values[i + 9], varcharLength[i + 9], pivotValue, pivotLength);
        compareTmp[j + 10] =
            OnlyCompareVarChar<sortAscending>(values[i + 10], varcharLength[i + 10], pivotValue, pivotLength);
        compareTmp[j + 11] =
            OnlyCompareVarChar<sortAscending>(values[i + 11], varcharLength[i + 11], pivotValue, pivotLength);
    }
    for (; j < limit; ++i, ++j) {
        compareTmp[j] = OnlyCompareVarChar<sortAscending>(values[i], varcharLength[i], pivotValue, pivotLength);
    }
    return compareTmp[0];
}

template <int32_t sortAscending>
int32_t ALWAYS_INLINE GetNextCompareRightVarChar(int32_t *compareTmp, int32_t &k, int32_t &limit, int32_t b,
    int32_t c, int64_t *values, std::vector<uint32_t> &varcharLength, int64_t pivotValue, uint32_t pivotLength)
{
    if (LIKELY(k < limit)) {
        return compareTmp[k++];
    }
    k = 1;
    // use up
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = c;
    int32_t j = 0;
    for (; j < limit - NSTEP; i -= NSTEP, j += NSTEP) {
        compareTmp[j] = OnlyCompareVarChar<sortAscending>(values[i], varcharLength[i], pivotValue, pivotLength);
        compareTmp[j + 1] =
            OnlyCompareVarChar<sortAscending>(values[i - 1], varcharLength[i - 1], pivotValue, pivotLength);
        compareTmp[j + 2] =
            OnlyCompareVarChar<sortAscending>(values[i - 2], varcharLength[i - 2], pivotValue, pivotLength);
        compareTmp[j + 3] =
            OnlyCompareVarChar<sortAscending>(values[i - 3], varcharLength[i - 3], pivotValue, pivotLength);
        compareTmp[j + 4] =
            OnlyCompareVarChar<sortAscending>(values[i - 4], varcharLength[i - 4], pivotValue, pivotLength);
        compareTmp[j + 5] =
            OnlyCompareVarChar<sortAscending>(values[i - 5], varcharLength[i - 5], pivotValue, pivotLength);
        compareTmp[j + 6] =
            OnlyCompareVarChar<sortAscending>(values[i - 6], varcharLength[i - 6], pivotValue, pivotLength);
        compareTmp[j + 7] =
            OnlyCompareVarChar<sortAscending>(values[i - 7], varcharLength[i - 7], pivotValue, pivotLength);
        compareTmp[j + 8] =
            OnlyCompareVarChar<sortAscending>(values[i - 8], varcharLength[i - 8], pivotValue, pivotLength);
        compareTmp[j + 9] =
            OnlyCompareVarChar<sortAscending>(values[i - 9], varcharLength[i - 9], pivotValue, pivotLength);
        compareTmp[j + 10] =
            OnlyCompareVarChar<sortAscending>(values[i - 10], varcharLength[i - 10], pivotValue, pivotLength);
        compareTmp[j + 11] =
            OnlyCompareVarChar<sortAscending>(values[i - 11], varcharLength[i - 11], pivotValue, pivotLength);
    }
    for (; j < limit; --i, ++j) {
        compareTmp[j] = OnlyCompareVarChar<sortAscending>(values[i], varcharLength[i], pivotValue, pivotLength);
    }
    return compareTmp[0];
}

void ALWAYS_INLINE SwapVarchar(int64_t *values, std::vector<uint32_t> &varcharLength, uint64_t *addresses, int32_t a,
    int32_t b)
{
    auto tmpValuePtr = values[a];
    auto tmpLength = varcharLength[a];
    auto tmpAddress = addresses[a];
    values[a] = values[b];
    varcharLength[a] = varcharLength[b];
    addresses[a] = addresses[b];
    values[b] = tmpValuePtr;
    varcharLength[b] = tmpLength;
    addresses[b] = tmpAddress;
}

void NO_INLINE VectorSwapVarChar(int64_t *values, std::vector<uint32_t> &varcharLength, uint64_t *addresses,
    int32_t from, int32_t l, int32_t s)
{
    int32_t i = 0;
    for (; i < s - SWAP_STEP_SIZE; i += SWAP_STEP_SIZE, from += SWAP_STEP_SIZE, l += SWAP_STEP_SIZE) {
        SwapVarchar(values, varcharLength, addresses, from, l);
        SwapVarchar(values, varcharLength, addresses, from + 1, l + 1);
        SwapVarchar(values, varcharLength, addresses, from + 2, l + 2);
        SwapVarchar(values, varcharLength, addresses, from + 3, l + 3);
        SwapVarchar(values, varcharLength, addresses, from + 4, l + 4);
        SwapVarchar(values, varcharLength, addresses, from + 5, l + 5);
        SwapVarchar(values, varcharLength, addresses, from + 6, l + 6);
        SwapVarchar(values, varcharLength, addresses, from + 7, l + 7);
    }
    for (; i < s; i++, from++, l++) {
        SwapVarchar(values, varcharLength, addresses, from, l);
    }
}

template <int32_t sortAscending>
void QuickSortVarcharInternal(int64_t *values, std::vector<uint32_t> &varcharLength, uint64_t *addresses, int32_t from,
    int32_t to, int32_t *compareTmp)
{
    const int32_t len = to - from;
    if (len <= QUICK_SORT_SMALL_LEN) {
        QuickSortVarCharSmall<sortAscending>(values, varcharLength, addresses, from, to);
        return;
    }

    const int32_t m = GetMedianPositionVarChar<sortAscending>(values, varcharLength, from, to, len);

    const int64_t pivotValue = values[m];
    const uint32_t pivotLength = varcharLength[m];

    int32_t a = from;
    int32_t b = a;
    int32_t c = to - 1;
    int32_t d = c;

    int32_t bk = 0;
    int32_t blim = 0;
    int32_t ck = 0;
    int32_t clim = 0;
    int32_t *leftCompareTmp = compareTmp;
    int32_t *rightCompareTmp = compareTmp + NMAX_SIZE;
    while (true) {
        int32_t comparison;
        while (b <= c && (comparison = GetNextCompareLeftVarChar<sortAscending>(leftCompareTmp, bk, blim, b, c,
            values, varcharLength, pivotValue, pivotLength)) <= 0) {
            if (UNLIKELY(comparison == 0)) {
                SwapVarchar(values, varcharLength, addresses, a++, b);
            }
            b++;
        }
        while (c >= b && (comparison = GetNextCompareRightVarChar<sortAscending>(rightCompareTmp, ck, clim, b, c,
            values, varcharLength, pivotValue, pivotLength)) >= 0) {
            if (UNLIKELY(comparison == 0)) {
                SwapVarchar(values, varcharLength, addresses, c, d--);
            }
            c--;
        }
        if (b > c) {
            break;
        }
        SwapVarchar(values, varcharLength, addresses, b++, c--);
    }

    // Swap partition elements back to middle
    int32_t s;
    const int32_t n = to;
    s = std::min(a - from, b - a);
    VectorSwapVarChar(values, varcharLength, addresses, from, b - s, s);
    s = std::min(d - c, n - d - 1);
    VectorSwapVarChar(values, varcharLength, addresses, b, n - s, s);

    // Recursively sort non-partition-elements
    if ((s = b - a) > 1) {
        QuickSortVarcharInternal<sortAscending>(values, varcharLength, addresses, from, from + s, compareTmp);
    }
    if ((s = d - c) > 1) {
        QuickSortVarcharInternal<sortAscending>(values, varcharLength, addresses, n - s, n, compareTmp);
    }
}

template <int32_t sortAscending>
void QuickSortVarChar(int64_t *values, std::vector<uint32_t> &varcharLength, uint64_t *addresses, int32_t from,
    int32_t to)
{
    std::array<int32_t, NMAX_SIZE + NMAX_SIZE> compareResult = {};
    QuickSortVarcharInternal<sortAscending>(values, varcharLength, addresses, from, to, compareResult.data());
}

template <typename RawType> static bool ALWAYS_INLINE OnlyEqual(RawType &left, RawType &right)
{
    if constexpr (std::is_same_v<RawType, double>) {
        const double diff = left - right;
        return (std::abs(diff) < __DBL_EPSILON__);
    } else if constexpr (std::is_same_v<RawType, float>) {
        const float diff = left - right;
        return (std::abs(diff) < __FLT_EPSILON__);
    } else {
        return left == right;
    }
}

template <int32_t sortAscending> static int8_t ALWAYS_INLINE Decimal128Compare(Decimal128 &left, Decimal128 &right)
{
    if constexpr (sortAscending == 1) {
        return left > right ?
            OperatorUtil::COMPARE_STATUS_GREATER_THAN :
            left < right ? OperatorUtil::COMPARE_STATUS_LESS_THAN : OperatorUtil::COMPARE_STATUS_EQUAL;
    } else {
        return right > left ?
            OperatorUtil::COMPARE_STATUS_GREATER_THAN :
            right < left ? OperatorUtil::COMPARE_STATUS_LESS_THAN : OperatorUtil::COMPARE_STATUS_EQUAL;
    }
}


// Exact comparison only: Spark and Velox order doubles by exact value.
// The previous epsilon comparison (both absolute diff < DBL_EPSILON and
// relative diff < max*DBL_EPSILON) treated near-equal doubles (1 ULP apart)
// as "equal", causing arbitrary ordering in single-key sorts and incorrect
// tie-breaking in multi-key sorts. For example 3.0 and 3.0000000000000004
// (1 ULP apart) were considered "equal" by the relative epsilon because
// 4.44e-16 < 3.0 * 2.22e-16 = 6.66e-16, so the quicksort did not swap them
// and the output order depended on the input order rather than the values.
static bool ALWAYS_INLINE DoubleEqual(double left, double right)
{
    return left == right;
}

template <int32_t sortAscending> static int8_t ALWAYS_INLINE DoubleCompare(double left, double right)
{
    // NaN is the maximum value (Spark/Velox semantics):
    // ASC: NaN sorts after everything; DESC: NaN sorts before everything.
    const bool leftNan = std::isnan(left);
    const bool rightNan = std::isnan(right);
    if (leftNan || rightNan) {
        if (leftNan && rightNan) {
            return omniruntime::op::OperatorUtil::COMPARE_STATUS_EQUAL;
        }
        if constexpr (sortAscending == 1) {
            return leftNan ? OperatorUtil::COMPARE_STATUS_GREATER_THAN
                           : OperatorUtil::COMPARE_STATUS_LESS_THAN;
        } else {
            return leftNan ? OperatorUtil::COMPARE_STATUS_LESS_THAN
                           : OperatorUtil::COMPARE_STATUS_GREATER_THAN;
        }
    }

    if (DoubleEqual(left, right)) {
        return omniruntime::op::OperatorUtil::COMPARE_STATUS_EQUAL;
    }

    if constexpr (sortAscending == 1) {
        return left > right?
            OperatorUtil::COMPARE_STATUS_GREATER_THAN : OperatorUtil::COMPARE_STATUS_LESS_THAN;
    } else {
        return right > left ?
            OperatorUtil::COMPARE_STATUS_GREATER_THAN : OperatorUtil::COMPARE_STATUS_LESS_THAN;
    }
}

// Exact comparison only: same rationale as DoubleEqual above.
static bool ALWAYS_INLINE FloatEqual(float left, float right)
{
    return left == right;
}

template <int32_t sortAscending> static int8_t ALWAYS_INLINE FloatCompare(float left, float right)
{
    // NaN is the maximum value (Spark/Velox semantics).
    const bool leftNan = std::isnan(left);
    const bool rightNan = std::isnan(right);
    if (leftNan || rightNan) {
        if (leftNan && rightNan) {
            return omniruntime::op::OperatorUtil::COMPARE_STATUS_EQUAL;
        }
        if constexpr (sortAscending == 1) {
            return leftNan ? OperatorUtil::COMPARE_STATUS_GREATER_THAN
                           : OperatorUtil::COMPARE_STATUS_LESS_THAN;
        } else {
            return leftNan ? OperatorUtil::COMPARE_STATUS_LESS_THAN
                           : OperatorUtil::COMPARE_STATUS_GREATER_THAN;
        }
    }

    if (FloatEqual(left, right)) {
        return omniruntime::op::OperatorUtil::COMPARE_STATUS_EQUAL;
    }

    if constexpr (sortAscending == 1) {
        return left > right ?
            OperatorUtil::COMPARE_STATUS_GREATER_THAN : OperatorUtil::COMPARE_STATUS_LESS_THAN;
    } else {
        return right > left ?
            OperatorUtil::COMPARE_STATUS_GREATER_THAN : OperatorUtil::COMPARE_STATUS_LESS_THAN;
    }
}

template <int32_t sortAscending>
static void ALWAYS_INLINE QuickSortFloatSmall(int64_t *values, uint64_t *addresses, int32_t from, int32_t to)
{
    for (int32_t i = from + 1; i < to; ++i) {
        auto iValuePtr = values[i];
        auto iValue = *(reinterpret_cast<float *>(&iValuePtr));
        auto iAddr = addresses[i];
        int32_t j = i - 1;
        while (j >= from) {
            auto jValuePtr = values[j];
            auto jValue = *(reinterpret_cast<float *>(&jValuePtr));
            if (FloatCompare<sortAscending>(jValue, iValue) <= 0) {
                break;
            }
            values[j + 1] = values[j];
            addresses[j + 1] = addresses[j];
            --j;
        }
        values[j + 1] = iValuePtr;
        addresses[j + 1] = iAddr;
    }
}

template <int32_t sortAscending>
static float ALWAYS_INLINE GetMedianFloat(float va, float vb, float vc)
{
    int8_t ab = FloatCompare<sortAscending>(va, vb);
    int8_t ac = FloatCompare<sortAscending>(va, vc);
    int8_t bc = FloatCompare<sortAscending>(vb, vc);
    return ((ab < 0) ? (bc < 0 ? vb : ac < 0 ? vc : va) : (bc > 0 ? vb : ac > 0 ? vc : va));
}

template <int32_t sortAscending>
static float ALWAYS_INLINE GetMedianFloat(int64_t *values, int32_t a, int32_t b, int32_t c)
{
    float va = *(reinterpret_cast<float *>(&values[a]));
    float vb = *(reinterpret_cast<float *>(&values[b]));
    float vc = *(reinterpret_cast<float *>(&values[c]));
    int8_t ab = FloatCompare<sortAscending>(va, vb);
    int8_t ac = FloatCompare<sortAscending>(va, vc);
    int8_t bc = FloatCompare<sortAscending>(vb, vc);
    return ((ab < 0) ? (bc < 0 ? vb : ac < 0 ? vc : va) : (bc > 0 ? vb : ac > 0 ? vc : va));
}

template <int32_t sortAscending>
static float ALWAYS_INLINE GetMedianFloatValue(int64_t *values, int32_t from, int32_t to, int32_t len)
{
    int32_t l = from;
    int32_t n = to - 1;
    int32_t m = from + len / QUICK_SORT_MIDDLE;

    if (len > QUICK_SORT_BIG_LEN) {
        int32_t s = len / QUICK_SORT_STEP_SIZE;
        float vl = GetMedianFloat<sortAscending>(values, l, l + s, l + QUICK_SORT_MIDDLE * s);
        float vm = GetMedianFloat<sortAscending>(values, m - s, m, m + s);
        float vn = GetMedianFloat<sortAscending>(values, n - QUICK_SORT_MIDDLE * s, n - s, n);
        return GetMedianFloat<sortAscending>(vl, vm, vn);
    }
    return GetMedianFloat<sortAscending>(values, l, m, n);
}

template <int32_t sortAscending>
int8_t ALWAYS_INLINE GetNextCompareFloatLeft(int8_t *compareTmp, int32_t &k, int32_t &limit, int32_t b, int32_t c,
    int64_t *values, float &pivotValue)
{
    if (k < limit) {
        return compareTmp[k++];
    }
    k = 1;

    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = b;
    int32_t j = 0;
    for (; j < limit - NSTEP; i += NSTEP, j += NSTEP) {
        auto v0 = *(reinterpret_cast<float *>(&values[i]));
        auto v1 = *(reinterpret_cast<float *>(&values[i + 1]));
        auto v2 = *(reinterpret_cast<float *>(&values[i + 2]));
        auto v3 = *(reinterpret_cast<float *>(&values[i + 3]));
        compareTmp[j] = FloatCompare<sortAscending>(v0, pivotValue);
        compareTmp[j + 1] = FloatCompare<sortAscending>(v1, pivotValue);
        compareTmp[j + 2] = FloatCompare<sortAscending>(v2, pivotValue);
        compareTmp[j + 3] = FloatCompare<sortAscending>(v3, pivotValue);
        auto v4 = *(reinterpret_cast<float *>(&values[i + 4]));
        auto v5 = *(reinterpret_cast<float *>(&values[i + 5]));
        auto v6 = *(reinterpret_cast<float *>(&values[i + 6]));
        auto v7 = *(reinterpret_cast<float *>(&values[i + 7]));
        compareTmp[j + 4] = FloatCompare<sortAscending>(v4, pivotValue);
        compareTmp[j + 5] = FloatCompare<sortAscending>(v5, pivotValue);
        compareTmp[j + 6] = FloatCompare<sortAscending>(v6, pivotValue);
        compareTmp[j + 7] = FloatCompare<sortAscending>(v7, pivotValue);
        auto v8 = *(reinterpret_cast<float *>(&values[i + 8]));
        auto v9 = *(reinterpret_cast<float *>(&values[i + 9]));
        auto v10 = *(reinterpret_cast<float *>(&values[i + 10]));
        auto v11 = *(reinterpret_cast<float *>(&values[i + 11]));
        compareTmp[j + 8] = FloatCompare<sortAscending>(v8, pivotValue);
        compareTmp[j + 9] = FloatCompare<sortAscending>(v9, pivotValue);
        compareTmp[j + 10] = FloatCompare<sortAscending>(v10, pivotValue);
        compareTmp[j + 11] = FloatCompare<sortAscending>(v11, pivotValue);
    }
    for (; j < limit; ++i, ++j) {
        auto v = *(reinterpret_cast<float *>(&values[i]));
        compareTmp[j] = FloatCompare<sortAscending>(v, pivotValue);
    }
    return compareTmp[0];
}

template <int32_t sortAscending>
inline int8_t GetNextCompareFloatRight(int8_t *compareTmp, int32_t &k, int32_t &limit, int32_t b, int32_t c,
    int64_t *values, float &pivotValue)
{
    if (k < limit) {
        return compareTmp[k++];
    }
    k = 1;
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = c;
    int32_t j = 0;
    for (; j < limit - NSTEP; i -= NSTEP, j += NSTEP) {
        auto v0 = *(reinterpret_cast<float *>(&values[i]));
        auto v1 = *(reinterpret_cast<float *>(&values[i - 1]));
        auto v2 = *(reinterpret_cast<float *>(&values[i - 2]));
        auto v3 = *(reinterpret_cast<float *>(&values[i - 3]));
        compareTmp[j] = FloatCompare<sortAscending>(v0, pivotValue);
        compareTmp[j + 1] = FloatCompare<sortAscending>(v1, pivotValue);
        compareTmp[j + 2] = FloatCompare<sortAscending>(v2, pivotValue);
        compareTmp[j + 3] = FloatCompare<sortAscending>(v3, pivotValue);
        auto v4 = *(reinterpret_cast<float *>(&values[i - 4]));
        auto v5 = *(reinterpret_cast<float *>(&values[i - 5]));
        auto v6 = *(reinterpret_cast<float *>(&values[i - 6]));
        auto v7 = *(reinterpret_cast<float *>(&values[i - 7]));
        compareTmp[j + 4] = FloatCompare<sortAscending>(v4, pivotValue);
        compareTmp[j + 5] = FloatCompare<sortAscending>(v5, pivotValue);
        compareTmp[j + 6] = FloatCompare<sortAscending>(v6, pivotValue);
        compareTmp[j + 7] = FloatCompare<sortAscending>(v7, pivotValue);
        auto v8 = *(reinterpret_cast<float *>(&values[i - 8]));
        auto v9 = *(reinterpret_cast<float *>(&values[i - 9]));
        auto v10 = *(reinterpret_cast<float *>(&values[i - 10]));
        auto v11 = *(reinterpret_cast<float *>(&values[i - 11]));
        compareTmp[j + 8] = FloatCompare<sortAscending>(v8, pivotValue);
        compareTmp[j + 9] = FloatCompare<sortAscending>(v9, pivotValue);
        compareTmp[j + 10] = FloatCompare<sortAscending>(v10, pivotValue);
        compareTmp[j + 11] = FloatCompare<sortAscending>(v11, pivotValue);
    }
    for (; j < limit; --i, ++j) {
        auto v = *(reinterpret_cast<float *>(&values[i]));
        compareTmp[j] = FloatCompare<sortAscending>(v, pivotValue);
    }
    return compareTmp[0];
}

template <int32_t sortAscending>
void QuickSortFloatInternal(int64_t *values, uint64_t *addresses, int32_t from, int32_t to, int8_t *compareTmp)
{
    int32_t len = to - from;
    if (len <= QUICK_SORT_SMALL_LEN) {
        QuickSortFloatSmall<sortAscending>(values, addresses, from, to);
        return;
    }

    auto pivotValue = GetMedianFloatValue<sortAscending>(values, from, to, len);

    int32_t a = from;
    int32_t b = a;
    int32_t c = to - 1;
    int32_t d = c;

    int32_t bk = 0;
    int32_t blim = 0;
    int32_t ck = 0;
    int32_t clim = 0;
    int8_t *leftCompareTmp = compareTmp;
    int8_t *rightCompareTmp = compareTmp + NMAX_SIZE;

    while (true) {
        int8_t comparison;
        while (b <= c && (comparison = GetNextCompareFloatLeft<sortAscending>(leftCompareTmp, bk, blim, b, c,
            values, pivotValue)) <= 0) {
            if (UNLIKELY(comparison == 0)) {
                Swap(values, addresses, a++, b);
            }
            b++;
        }

        while (c >= b && (comparison = GetNextCompareFloatRight<sortAscending>(rightCompareTmp, ck, clim, b, c,
            values, pivotValue)) >= 0) {
            if (UNLIKELY(comparison == 0)) {
                Swap(values, addresses, c, d--);
            }
            c--;
        }
        if (b > c) {
            break;
        }
        Swap(values, addresses, b++, c--);
    }

    int32_t s;
    int32_t n = to;
    s = std::min(a - from, b - a);
    VectorSwap(values, addresses, from, b - s, s);
    s = std::min(d - c, n - d - 1);
    VectorSwap(values, addresses, b, n - s, s);

    if ((s = b - a) > 1) {
        QuickSortFloatInternal<sortAscending>(values, addresses, from, from + s, compareTmp);
    }
    if ((s = d - c) > 1) {
        QuickSortFloatInternal<sortAscending>(values, addresses, n - s, n, compareTmp);
    }
}

template <int32_t sortAscending>
void QuickSortFloat(int64_t *values, uint64_t *addresses, int32_t from, int32_t to)
{
    std::array<int8_t, NMAX_SIZE + NMAX_SIZE> compareResult = {};
    QuickSortFloatInternal<sortAscending>(values, addresses, from, to, compareResult.data());
}

template <int32_t sortAscending>
static void ALWAYS_INLINE QuickSortDecimal128Small(int64_t *values, uint64_t *addresses, int32_t from, int32_t to)
{
    for (int32_t i = from + 1; i < to; ++i) {
        auto iValuePtr = values[i];
        auto iValue = *((Decimal128 *)iValuePtr);
        auto iAddr = addresses[i];
        int32_t j = i - 1;
        while (j >= from) {
            auto jValuePtr = values[j];
            auto jValue = *((Decimal128 *)jValuePtr);
            if constexpr (sortAscending == 0) {
                if (jValue >= iValue) {
                    break;
                }
            } else {
                if (jValue <= iValue) {
                    break;
                }
            }
            values[j + 1] = values[j];
            addresses[j + 1] = addresses[j];
            --j;
        }
        values[j + 1] = iValuePtr;
        addresses[j + 1] = iAddr;
    }
}


template <int32_t sortAscending>
static void ALWAYS_INLINE QuickSortDoubleSmall(int64_t *values, uint64_t *addresses, int32_t from, int32_t to)
{
    for (int32_t i = from + 1; i < to; ++i) {
        auto iValuePtr = values[i];
        auto iValue = *(reinterpret_cast<double *>(&iValuePtr));
        auto iAddr = addresses[i];
        int32_t j = i - 1;
        while (j >= from) {
            auto jValuePtr = values[j];
            auto jValue = *(reinterpret_cast<double *>(&jValuePtr));
            if (DoubleCompare<sortAscending>(jValue, iValue) <= 0) {
                break;
            }
            values[j + 1] = values[j];
            addresses[j + 1] = addresses[j];
            --j;
        }
        values[j + 1] = iValuePtr;
        addresses[j + 1] = iAddr;
    }
}


static Decimal128 ALWAYS_INLINE GetMedianDecimal128(Decimal128 &va, Decimal128 &vb, Decimal128 &vc)
{
    if (va <= vb) {
        if (vb <= vc) {
            return vb;
        }
        if (va <= vc) {
            return vc;
        } else {
            return va;
        }
    } else {
        if (vb > vc) {
            return vb;
        }
        if (va > vc) {
            return vc;
        } else {
            return va;
        }
    }
}

static Decimal128 ALWAYS_INLINE GetMedianDecimal128(int64_t *values, int32_t a, int32_t b, int32_t c)
{
    Decimal128 va = *((Decimal128 *)(values[a]));
    Decimal128 vb = *((Decimal128 *)(values[b]));
    Decimal128 vc = *((Decimal128 *)(values[c]));
    return GetMedianDecimal128(va, vb, vc);
}

static Decimal128 ALWAYS_INLINE GetMedianDecimal128Value(int64_t *values, int32_t from, int32_t to, int32_t len)
{
    int32_t l = from;
    int32_t n = to - 1;
    int32_t m = from + len / QUICK_SORT_MIDDLE;
    if (len > QUICK_SORT_BIG_LEN) {
        int32_t s = len / QUICK_SORT_STEP_SIZE;
        Decimal128 vl = GetMedianDecimal128(values, l, l + s, l + QUICK_SORT_MIDDLE * s);
        Decimal128 vm = GetMedianDecimal128(values, m - s, m, m + s);
        Decimal128 vn = GetMedianDecimal128(values, n - QUICK_SORT_MIDDLE * s, n - s, n);
        return GetMedianDecimal128(vl, vm, vn);
    } else {
        return GetMedianDecimal128(values, l, m, n);
    }
}


template <int32_t sortAscending>
static double ALWAYS_INLINE GetMedianDouble(double va, double vb, double vc)
{
    int8_t ab = DoubleCompare<sortAscending>(va, vb);
    int8_t ac = DoubleCompare<sortAscending>(va, vc);
    int8_t bc = DoubleCompare<sortAscending>(vb, vc);
    return ((ab < 0) ? (bc < 0 ? vb : ac < 0 ? vc : va) : (bc > 0 ? vb : ac > 0 ? vc : va));
}


template <int32_t sortAscending>
static double ALWAYS_INLINE GetMedianDouble(int64_t *values, int32_t a, int32_t b, int32_t c)
{
    double va = *(reinterpret_cast<double *>(&values[a]));
    double vb = *(reinterpret_cast<double *>(&values[b]));
    double vc = *(reinterpret_cast<double *>(&values[c]));
    int8_t ab = DoubleCompare<sortAscending>(va, vb);
    int8_t ac = DoubleCompare<sortAscending>(va, vc);
    int8_t bc = DoubleCompare<sortAscending>(vb, vc);
    return ((ab < 0) ? (bc < 0 ? vb : ac < 0 ? vc : va) : (bc > 0 ? vb : ac > 0 ? vc : va));
}

template <int32_t sortAscending>
static double ALWAYS_INLINE GetMedianDoubleValue(int64_t *values, int32_t from, int32_t to, int32_t len)
{
    int32_t l = from;
    int32_t n = to - 1;
    int32_t m = from + len / QUICK_SORT_MIDDLE;

    if (len > QUICK_SORT_BIG_LEN) {
        int32_t s = len / QUICK_SORT_STEP_SIZE;
        double vl = GetMedianDouble<sortAscending>(values, l, l + s, l + QUICK_SORT_MIDDLE * s);
        double vm = GetMedianDouble<sortAscending>(values, m - s, m, m + s);
        double vn = GetMedianDouble<sortAscending>(values, n - QUICK_SORT_MIDDLE * s, n - s, n);
        return GetMedianDouble<sortAscending>(vl, vm, vn);
    }
    return GetMedianDouble<sortAscending>(values, l, m, n);
}

template <int32_t sortAscending>
int8_t ALWAYS_INLINE GetNextCompareDecimal128Left(int8_t *compareTmp, int32_t &k, int32_t &limit, int32_t b, int32_t c,
    int64_t *values, Decimal128 &pivotValue)
{
    if (k < limit) {
        return compareTmp[k++];
    }
    k = 1;
    // use up
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = b;
    int32_t j = 0;
    for (; j < limit - NSTEP; i += NSTEP, j += NSTEP) {
        auto v0 = *((Decimal128 *)(values[i]));
        auto v1 = *((Decimal128 *)(values[i + 1]));
        auto v2 = *((Decimal128 *)(values[i + 2]));
        auto v3 = *((Decimal128 *)(values[i + 3]));
        compareTmp[j] = Decimal128Compare<sortAscending>(v0, pivotValue);
        compareTmp[j + 1] = Decimal128Compare<sortAscending>(v1, pivotValue);
        compareTmp[j + 2] = Decimal128Compare<sortAscending>(v2, pivotValue);
        compareTmp[j + 3] = Decimal128Compare<sortAscending>(v3, pivotValue);
        auto v4 = *((Decimal128 *)(values[i + 4]));
        auto v5 = *((Decimal128 *)(values[i + 5]));
        auto v6 = *((Decimal128 *)(values[i + 6]));
        auto v7 = *((Decimal128 *)(values[i + 7]));
        compareTmp[j + 4] = Decimal128Compare<sortAscending>(v4, pivotValue);
        compareTmp[j + 5] = Decimal128Compare<sortAscending>(v5, pivotValue);
        compareTmp[j + 6] = Decimal128Compare<sortAscending>(v6, pivotValue);
        compareTmp[j + 7] = Decimal128Compare<sortAscending>(v7, pivotValue);
        auto v8 = *((Decimal128 *)(values[i + 8]));
        auto v9 = *((Decimal128 *)(values[i + 9]));
        auto v10 = *((Decimal128 *)(values[i + 10]));
        auto v11 = *((Decimal128 *)(values[i + 11]));
        compareTmp[j + 8] = Decimal128Compare<sortAscending>(v8, pivotValue);
        compareTmp[j + 9] = Decimal128Compare<sortAscending>(v9, pivotValue);
        compareTmp[j + 10] = Decimal128Compare<sortAscending>(v10, pivotValue);
        compareTmp[j + 11] = Decimal128Compare<sortAscending>(v11, pivotValue);
    }
    for (; j < limit; ++i, ++j) {
        auto v = *((Decimal128 *)(values[i]));
        compareTmp[j] = Decimal128Compare<sortAscending>(v, pivotValue);
    }
    return compareTmp[0];
}

template <int32_t sortAscending>
inline int8_t GetNextCompareDecimal128Right(int8_t *compareTmp, int32_t &k, int32_t &limit, int32_t b, int32_t c,
    int64_t *values, Decimal128 &pivotValue)
{
    if (k < limit) {
        return compareTmp[k++];
    }
    k = 1;
    // use up
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = c;
    int32_t j = 0;
    for (; j < limit - NSTEP; i -= NSTEP, j += NSTEP) {
        auto v0 = *((Decimal128 *)(values[i]));
        auto v1 = *((Decimal128 *)(values[i - 1]));
        auto v2 = *((Decimal128 *)(values[i - 2]));
        auto v3 = *((Decimal128 *)(values[i - 3]));
        compareTmp[j] = Decimal128Compare<sortAscending>(v0, pivotValue);
        compareTmp[j + 1] = Decimal128Compare<sortAscending>(v1, pivotValue);
        compareTmp[j + 2] = Decimal128Compare<sortAscending>(v2, pivotValue);
        compareTmp[j + 3] = Decimal128Compare<sortAscending>(v3, pivotValue);
        auto v4 = *((Decimal128 *)(values[i - 4]));
        auto v5 = *((Decimal128 *)(values[i - 5]));
        auto v6 = *((Decimal128 *)(values[i - 6]));
        auto v7 = *((Decimal128 *)(values[i - 7]));
        compareTmp[j + 4] = Decimal128Compare<sortAscending>(v4, pivotValue);
        compareTmp[j + 5] = Decimal128Compare<sortAscending>(v5, pivotValue);
        compareTmp[j + 6] = Decimal128Compare<sortAscending>(v6, pivotValue);
        compareTmp[j + 7] = Decimal128Compare<sortAscending>(v7, pivotValue);
        auto v8 = *((Decimal128 *)(values[i - 8]));
        auto v9 = *((Decimal128 *)(values[i - 9]));
        auto v10 = *((Decimal128 *)(values[i - 10]));
        auto v11 = *((Decimal128 *)(values[i - 11]));
        compareTmp[j + 8] = Decimal128Compare<sortAscending>(v8, pivotValue);
        compareTmp[j + 9] = Decimal128Compare<sortAscending>(v9, pivotValue);
        compareTmp[j + 10] = Decimal128Compare<sortAscending>(v10, pivotValue);
        compareTmp[j + 11] = Decimal128Compare<sortAscending>(v11, pivotValue);
    }
    for (; j < limit; --i, ++j) {
        auto v = *((Decimal128 *)(values[i]));
        compareTmp[j] = Decimal128Compare<sortAscending>(v, pivotValue);
    }
    return compareTmp[0];
}


template <int32_t sortAscending>
int8_t ALWAYS_INLINE GetNextCompareDoubleLeft(int8_t *compareTmp, int32_t &k, int32_t &limit, int32_t b, int32_t c,
    int64_t *values, double &pivotValue)
{
    if (k < limit) {
        return compareTmp[k++];
    }
    k = 1;

    // use up
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = b;
    int32_t j = 0;
    for (; j < limit - NSTEP; i += NSTEP, j += NSTEP) {
        auto v0 = *(reinterpret_cast<double *>(&values[i]));
        auto v1 = *(reinterpret_cast<double *>(&values[i + 1]));
        auto v2 = *(reinterpret_cast<double *>(&values[i + 2]));
        auto v3 = *(reinterpret_cast<double *>(&values[i + 3]));
        compareTmp[j] = DoubleCompare<sortAscending>(v0, pivotValue);
        compareTmp[j + 1] = DoubleCompare<sortAscending>(v1, pivotValue);
        compareTmp[j + 2] = DoubleCompare<sortAscending>(v2, pivotValue);
        compareTmp[j + 3] = DoubleCompare<sortAscending>(v3, pivotValue);
        auto v4 = *(reinterpret_cast<double *>(&values[i + 4]));
        auto v5 = *(reinterpret_cast<double *>(&values[i + 5]));
        auto v6 = *(reinterpret_cast<double *>(&values[i + 6]));
        auto v7 = *(reinterpret_cast<double *>(&values[i + 7]));
        compareTmp[j + 4] = DoubleCompare<sortAscending>(v4, pivotValue);
        compareTmp[j + 5] = DoubleCompare<sortAscending>(v5, pivotValue);
        compareTmp[j + 6] = DoubleCompare<sortAscending>(v6, pivotValue);
        compareTmp[j + 7] = DoubleCompare<sortAscending>(v7, pivotValue);
        auto v8 = *(reinterpret_cast<double *>(&values[i + 8]));
        auto v9 = *(reinterpret_cast<double *>(&values[i + 9]));
        auto v10 = *(reinterpret_cast<double *>(&values[i + 10]));
        auto v11 = *(reinterpret_cast<double *>(&values[i + 11]));
        compareTmp[j + 8] = DoubleCompare<sortAscending>(v8, pivotValue);
        compareTmp[j + 9] = DoubleCompare<sortAscending>(v9, pivotValue);
        compareTmp[j + 10] = DoubleCompare<sortAscending>(v10, pivotValue);
        compareTmp[j + 11] = DoubleCompare<sortAscending>(v11, pivotValue);
    }
    for (; j < limit; ++i, ++j) {
        auto v = *(reinterpret_cast<double *>(&values[i]));
        compareTmp[j] = DoubleCompare<sortAscending>(v, pivotValue);
    }
    return compareTmp[0];
}

template <int32_t sortAscending>
inline int8_t GetNextCompareDoubleRight(int8_t *compareTmp, int32_t &k, int32_t &limit, int32_t b, int32_t c,
    int64_t *values, double &pivotValue)
{
    if (k < limit) {
        return compareTmp[k++];
    }
    k = 1;
    // use up
    limit = std::min(c - b + 1, NMAX_SIZE);

    int32_t i = c;
    int32_t j = 0;
    for (; j < limit - NSTEP; i -= NSTEP, j += NSTEP) {
        auto v0 = *(reinterpret_cast<double *>(&values[i]));
        auto v1 = *(reinterpret_cast<double *>(&values[i - 1]));
        auto v2 = *(reinterpret_cast<double *>(&values[i - 2]));
        auto v3 = *(reinterpret_cast<double *>(&values[i - 3]));
        compareTmp[j] = DoubleCompare<sortAscending>(v0, pivotValue);
        compareTmp[j + 1] = DoubleCompare<sortAscending>(v1, pivotValue);
        compareTmp[j + 2] = DoubleCompare<sortAscending>(v2, pivotValue);
        compareTmp[j + 3] = DoubleCompare<sortAscending>(v3, pivotValue);
        auto v4 = *(reinterpret_cast<double *>(&values[i - 4]));
        auto v5 = *(reinterpret_cast<double *>(&values[i - 5]));
        auto v6 = *(reinterpret_cast<double *>(&values[i - 6]));
        auto v7 = *(reinterpret_cast<double *>(&values[i - 7]));
        compareTmp[j + 4] = DoubleCompare<sortAscending>(v4, pivotValue);
        compareTmp[j + 5] = DoubleCompare<sortAscending>(v5, pivotValue);
        compareTmp[j + 6] = DoubleCompare<sortAscending>(v6, pivotValue);
        compareTmp[j + 7] = DoubleCompare<sortAscending>(v7, pivotValue);
        auto v8 = *(reinterpret_cast<double *>(&values[i - 8]));
        auto v9 = *(reinterpret_cast<double *>(&values[i - 9]));
        auto v10 = *(reinterpret_cast<double *>(&values[i - 10]));
        auto v11 = *(reinterpret_cast<double *>(&values[i - 11]));
        compareTmp[j + 8] = DoubleCompare<sortAscending>(v8, pivotValue);
        compareTmp[j + 9] = DoubleCompare<sortAscending>(v9, pivotValue);
        compareTmp[j + 10] = DoubleCompare<sortAscending>(v10, pivotValue);
        compareTmp[j + 11] = DoubleCompare<sortAscending>(v11, pivotValue);
    }
    for (; j < limit; --i, ++j) {
        auto v = *(reinterpret_cast<double *>(&values[i]));
        compareTmp[j] = DoubleCompare<sortAscending>(v, pivotValue);
    }
    return compareTmp[0];
}

template <int32_t sortAscending>
void QuickSortDecimal128Internal(int64_t *values, uint64_t *addresses, int32_t from, int32_t to, int8_t *compareTmp)
{
    int32_t len = to - from;
    if (len <= QUICK_SORT_SMALL_LEN) {
        QuickSortDecimal128Small<sortAscending>(values, addresses, from, to);
        return;
    }

    auto pivotValue = GetMedianDecimal128Value(values, from, to, len);

    int32_t a = from;
    int32_t b = a;
    int32_t c = to - 1;
    int32_t d = c;

    int32_t bk = 0;
    int32_t blim = 0;
    int32_t ck = 0;
    int32_t clim = 0;
    int8_t *leftCompareTmp = compareTmp;
    int8_t *rightCompareTmp = compareTmp + NMAX_SIZE;
    while (true) {
        int8_t comparison;
        while (b <= c && (comparison = GetNextCompareDecimal128Left<sortAscending>(leftCompareTmp, bk, blim, b, c,
            values, pivotValue)) <= 0) {
            if (UNLIKELY(comparison == 0)) {
                Swap(values, addresses, a++, b);
            }
            b++;
        }
        while (c >= b && (comparison = GetNextCompareDecimal128Right<sortAscending>(rightCompareTmp, ck, clim, b, c,
            values, pivotValue)) >= 0) {
            if (UNLIKELY(comparison == 0)) {
                Swap(values, addresses, c, d--);
            }
            c--;
        }
        if (b > c) {
            break;
        }
        Swap(values, addresses, b++, c--);
    }

    // Swap partition elements back to middle
    int32_t s;
    int32_t n = to;
    s = std::min(a - from, b - a);
    VectorSwap(values, addresses, from, b - s, s);
    s = std::min(d - c, n - d - 1);
    VectorSwap(values, addresses, b, n - s, s);

    // Recursively sort non-partition-elements
    if ((s = b - a) > 1) {
        QuickSortDecimal128Internal<sortAscending>(values, addresses, from, from + s, compareTmp);
    }
    if ((s = d - c) > 1) {
        QuickSortDecimal128Internal<sortAscending>(values, addresses, n - s, n, compareTmp);
    }
}


template <int32_t sortAscending>
void QuickSortDoubleInternal(int64_t *values, uint64_t *addresses, int32_t from, int32_t to, int8_t *compareTmp)
{
    int32_t len = to - from;
    if (len <= QUICK_SORT_SMALL_LEN) {
        QuickSortDoubleSmall<sortAscending>(values, addresses, from, to);
        return;
    }

    auto pivotValue = GetMedianDoubleValue<sortAscending>(values, from, to, len);

    int32_t a = from;
    int32_t b = a;
    int32_t c = to - 1;
    int32_t d = c;

    int32_t bk = 0;
    int32_t blim = 0;
    int32_t ck = 0;
    int32_t clim = 0;
    int8_t *leftCompareTmp = compareTmp;
    int8_t *rightCompareTmp = compareTmp + NMAX_SIZE;

    while (true) {
        int8_t comparison;
        while (b <= c && (comparison = GetNextCompareDoubleLeft<sortAscending>(leftCompareTmp, bk, blim, b, c,
            values, pivotValue)) <= 0) {
            if (UNLIKELY(comparison == 0)) {
                Swap(values, addresses, a++, b);
            }
            b++;
        }

        while (c >= b && (comparison = GetNextCompareDoubleRight<sortAscending>(rightCompareTmp, ck, clim, b, c,
            values, pivotValue)) >= 0) {
            if (UNLIKELY(comparison == 0)) {
                Swap(values, addresses, c, d--);
            }
            c--;
        }
        if (b > c) {
            break;
        }
        Swap(values, addresses, b++, c--);
    }

    // Swap partition elements back to middle
    int32_t s;
    int32_t n = to;
    s = std::min(a - from, b - a);
    VectorSwap(values, addresses, from, b - s, s);
    s = std::min(d - c, n - d - 1);
    VectorSwap(values, addresses, b, n - s, s);

    // Recursively sort non-partition-elements
    if ((s = b - a) > 1) {
        QuickSortDoubleInternal<sortAscending>(values, addresses, from, from + s, compareTmp);
    }
    if ((s = d - c) > 1) {
        QuickSortDoubleInternal<sortAscending>(values, addresses, n - s, n, compareTmp);
    }

}

template <int32_t sortAscending>
void QuickSortDecimal128(int64_t *values, uint64_t *addresses, int32_t from, int32_t to)
{
    std::array<int8_t, NMAX_SIZE + NMAX_SIZE> compareResult = {};
    QuickSortDecimal128Internal<sortAscending>(values, addresses, from, to, compareResult.data());
}

template <int32_t sortAscending>
void QuickSortDouble(int64_t *values, uint64_t *addresses, int32_t from, int32_t to)
{
    std::array<int8_t, NMAX_SIZE + NMAX_SIZE> compareResult = {};
    QuickSortDoubleInternal<sortAscending>(values, addresses, from, to, compareResult.data());
}

template <typename RawType, bool hasNull, bool hasDictionary, bool sortNullFirst>
void SortNullAndGetValue(BaseVector **sortColumn, int64_t *values, std::vector<uint32_t> &varcharLength,
    uint64_t *addresses, int32_t &from, int32_t &to)
{
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to;
    int32_t i = from;
    while (i < nonNullTo) {
        uint64_t encodedIndex = addresses[i];
        uint32_t vecBatchIdx = DecodeSliceIndex(encodedIndex);
        uint32_t rowIdx = DecodePosition(encodedIndex);
        auto column = sortColumn[vecBatchIdx];
        // Handle ConstVector first: const value is never null, avoid IsNull on ConstVector's small NullsBuffer
        if (UNLIKELY(column->GetEncoding() == OMNI_ENCODING_CONST)) {
            if constexpr (std::is_same_v<RawType, Decimal128>) {
                values[i] = reinterpret_cast<int64_t>(
                    &(static_cast<ConstVector<Decimal128> *>(column)->GetConstValueRef()));
            } else if constexpr (std::is_same_v<RawType, double>) {
                double value = static_cast<ConstVector<double> *>(column)->GetConstValue();
                memcpy(values + i, &value, sizeof(double));
            } else if constexpr (std::is_same_v<RawType, float>) {
                values[i] = 0;
                float value = static_cast<ConstVector<float> *>(column)->GetConstValue();
                memcpy(values + i, &value, sizeof(float));
            } else {
                values[i] = static_cast<ConstVector<RawType> *>(column)->GetConstValue();
            }
            ++i;
            continue;
        }
        if constexpr (hasNull) {
            if (UNLIKELY(column->IsNull(rowIdx))) {
                // first, put all nulls at the first or at the last according sortNullFirst
                if constexpr (sortNullFirst) {
                    Swap(values, addresses, i++, nonNullFrom++); // [0, nonNullFrom)
                } else { // swap the last nonNull element back to i--,need to CHECK the new i-th element again!
                    Swap(values, addresses, i, --nonNullTo); // [i, nonNullTo)
                }
                continue;
            }
        }

        if constexpr (hasDictionary) {
            if (column->GetEncoding() == OMNI_DICTIONARY) {
                using DictionaryFlatVector = vec::Vector<DictionaryContainer<RawType>>;
                auto dictionaryVector = static_cast<DictionaryFlatVector *>(column);
                if constexpr (std::is_same_v<RawType, Decimal128>) {
                    RawType *valuePtr = unsafe::UnsafeDictionaryVector::GetDictionary(dictionaryVector);
                    int32_t originalRowIndex = unsafe::UnsafeDictionaryVector::GetIds(dictionaryVector)[rowIdx];
                    values[i] = reinterpret_cast<int64_t>(valuePtr + originalRowIndex);
                } else if constexpr (std::is_same_v<RawType, double>) {
                    double value = dictionaryVector->GetValue(rowIdx);
                    memcpy(values + i, &value, sizeof(double));
                } else if constexpr (std::is_same_v<RawType, float>) {
                    values[i] = 0;
                    float value = dictionaryVector->GetValue(rowIdx);
                    memcpy(values + i, &value, sizeof(float));
                } else {
                    values[i] = dictionaryVector->GetValue(rowIdx);
                }
            } else {
                using FlatVector = Vector<RawType>;
                if constexpr (std::is_same_v<RawType, Decimal128>) {
                    values[i] = reinterpret_cast<int64_t>(
                        unsafe::UnsafeVector::GetRawValues(static_cast<FlatVector *>(column)) + rowIdx);
                } else if constexpr (std::is_same_v<RawType, double>) {
                    double value = static_cast<FlatVector *>(column)->GetValue(rowIdx);
                    memcpy(values + i, &value, sizeof(double));
                } else if constexpr (std::is_same_v<RawType, float>) {
                    values[i] = 0;
                    float value = static_cast<FlatVector *>(column)->GetValue(rowIdx);
                    memcpy(values + i, &value, sizeof(float));
                } else {
                    values[i] = static_cast<FlatVector *>(column)->GetValue(rowIdx);
                }
            }
        } else {
            using FlatVector = Vector<RawType>;
            if constexpr (std::is_same_v<RawType, Decimal128>) {
                values[i] = reinterpret_cast<int64_t>(
                    unsafe::UnsafeVector::GetRawValues(static_cast<FlatVector *>(column)) + rowIdx);
            } else if constexpr (std::is_same_v<RawType, double>) {
                double value = static_cast<FlatVector *>(column)->GetValue(rowIdx);
                memcpy(values + i, &value, sizeof(double));
            } else if constexpr (std::is_same_v<RawType, float>) {
                values[i] = 0;
                float value = static_cast<FlatVector *>(column)->GetValue(rowIdx);
                memcpy(values + i, &value, sizeof(float));
            } else {
                values[i] = static_cast<FlatVector *>(column)->GetValue(rowIdx);
            }
        }
        ++i;
    }
    from = nonNullFrom;
    to = nonNullTo;
}

template <bool hasNull, bool hasDictionary, bool sortNullFirst>
void SortNullAndGetVarcharValue(BaseVector **sortColumn, int64_t *values, std::vector<uint32_t> &varcharLength,
    uint64_t *addresses, int32_t &from, int32_t &to)
{
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to;
    int32_t i = from;
    while (i < nonNullTo) {
        uint64_t encodedIndex = addresses[i];
        uint32_t vecBatchIdx = DecodeSliceIndex(encodedIndex);
        uint32_t rowIdx = DecodePosition(encodedIndex);
        auto column = sortColumn[vecBatchIdx];
        // Handle ConstVector first: const value is never null, avoid IsNull on ConstVector's small NullsBuffer
        if (UNLIKELY(column->GetEncoding() == OMNI_ENCODING_CONST)) {
            std::string_view value = static_cast<ConstVector<std::string_view> *>(column)->GetConstValue();
            values[i] = reinterpret_cast<int64_t>(const_cast<char *>(value.data()));
            varcharLength[i] = value.length();
            ++i;
            continue;
        }
        if constexpr (hasNull) {
            if (UNLIKELY(column->IsNull(rowIdx))) {
                // first, put all nulls at the first or at the last according sortNullFirst
                if constexpr (sortNullFirst) {
                    SwapVarchar(values, varcharLength, addresses, i++, nonNullFrom++); // [0, nonNullFrom)
                } else { // swap the last nonNull element back to i--,need to CHECK the new i-th element again!
                    SwapVarchar(values, varcharLength, addresses, i, --nonNullTo); // [i, nonNullTo)
                }
                continue;
            }
        }

        std::string_view value;
        if constexpr (hasDictionary) {
            if (column->GetEncoding() == OMNI_DICTIONARY) {
                value = static_cast<DictionaryVarcharVector *>(column)->GetValue(rowIdx);
            } else {
                value = static_cast<VarcharVector *>(column)->GetValue(rowIdx);
            }
        } else {
            value = static_cast<VarcharVector *>(column)->GetValue(rowIdx);
        }
        values[i] = reinterpret_cast<int64_t>(const_cast<char *>(value.data()));
        varcharLength[i] = value.length();
        ++i;
    }
    from = nonNullFrom;
    to = nonNullTo;
}

template <bool hasNull, bool sortNullFirst>
static void SortNullPartitionArray(BaseVector **sortColumn, int64_t *values, uint64_t *addresses, int32_t &from,
    int32_t &to)
{
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to;
    int32_t i = from;
    while (i < nonNullTo) {
        uint64_t encodedIndex = addresses[i];
        uint32_t vecBatchIdx = DecodeSliceIndex(encodedIndex);
        uint32_t rowIdx = DecodePosition(encodedIndex);
        auto column = sortColumn[vecBatchIdx];
        if constexpr (hasNull) {
            if (UNLIKELY(column->IsNull(rowIdx))) {
                if constexpr (sortNullFirst) {
                    Swap(values, addresses, i++, nonNullFrom++);
                } else {
                    Swap(values, addresses, i, --nonNullTo);
                }
                continue;
            }
        }
        ++i;
    }
    from = nonNullFrom;
    to = nonNullTo;
}

void PagesIndex::ArrayColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings,
    const int32_t *sortNullFirsts, int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength,
    int32_t from, int32_t to, int32_t currentCol)
{
    const auto sortCol = sortCols[currentCol];
    const auto sortAscending = sortAscendings[currentCol];
    const auto sortColumn = columns[sortCol];
    const auto hasNull = hasNulls[sortCol];
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to;

    DataTypePtr colType = dataTypes.GetType(sortCol);
    if (colType->GetId() != OMNI_ARRAY) {
        return;
    }

    if (sortNullFirsts[currentCol] == 0) {
        if (hasNull) {
            SortNullPartitionArray<true, false>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        } else {
            SortNullPartitionArray<false, false>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        }
    } else {
        if (hasNull) {
            SortNullPartitionArray<true, true>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        } else {
            SortNullPartitionArray<false, true>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        }
    }

    if (nonNullFrom + 1 < nonNullTo) {
        auto *addrBase = valueAddresses;
        const int32_t asc = sortAscending;
        const bool nullsFirst = sortNullFirsts[currentCol] != 0;
        std::sort(addrBase + nonNullFrom, addrBase + nonNullTo, [&](uint64_t a, uint64_t b) {
            uint32_t leftColIdx = DecodeSliceIndex(a);
            uint32_t leftRowIdx = DecodePosition(a);
            uint32_t rightColIdx = DecodeSliceIndex(b);
            uint32_t rightRowIdx = DecodePosition(b);
            vec::BaseVector *leftCol = sortColumn[leftColIdx];
            vec::BaseVector *rightCol = sortColumn[rightColIdx];
            int32_t c = OperatorUtil::CompareArrayValue(leftCol, leftRowIdx, rightCol, rightRowIdx, nullsFirst);
            return asc ? (c < 0) : (c > 0);
        });
    }

    if (currentCol == sortColCount - 1) {
        return;
    }

    const auto nextCol = currentCol + 1;
    if (nonNullFrom != from && from + 1 < nonNullFrom) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, nonNullFrom,
            nextCol);
    } else if (nonNullTo != to && nonNullTo + 1 < to) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, nonNullTo, to,
            nextCol);
    }

    if (nonNullFrom + 1 >= nonNullTo) {
        return;
    }

    int32_t start = nonNullFrom;
    const bool nullsFirst = sortNullFirsts[currentCol] != 0;
    for (int32_t i = nonNullFrom + 1; i < nonNullTo; ++i) {
        uint64_t a = valueAddresses[start];
        uint64_t b = valueAddresses[i];
        uint32_t leftColIdx = DecodeSliceIndex(a);
        uint32_t leftRowIdx = DecodePosition(a);
        uint32_t rightColIdx = DecodeSliceIndex(b);
        uint32_t rightRowIdx = DecodePosition(b);
        int32_t c = OperatorUtil::CompareArrayValue(sortColumn[leftColIdx], leftRowIdx, sortColumn[rightColIdx],
            rightRowIdx, nullsFirst);
        if (c != 0) {
            if (start + 1 != i) {
                ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, i,
                    nextCol);
            }
            start = i;
        }
    }
    if (start + 1 != nonNullTo) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, nonNullTo,
            nextCol);
    }
}

void PagesIndex::StructColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings,
    const int32_t *sortNullFirsts, int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength,
    int32_t from, int32_t to, int32_t currentCol)
{
    const auto sortCol = sortCols[currentCol];
    const auto sortAscending = sortAscendings[currentCol];
    const auto sortColumn = columns[sortCol];
    const auto hasNull = hasNulls[sortCol];
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to;

    DataTypePtr colType = dataTypes.GetType(sortCol);
    if (colType->GetId() != OMNI_ROW) {
        return;
    }

    // Partition nulls to front/back, same as ArrayColumnarSort.
    if (sortNullFirsts[currentCol] == 0) {
        // NULLS LAST
        if (hasNull) {
            SortNullPartitionArray<true, false>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        } else {
            SortNullPartitionArray<false, false>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        }
    } else {
        // NULLS FIRST
        if (hasNull) {
            SortNullPartitionArray<true, true>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        } else {
            SortNullPartitionArray<false, true>(sortColumn, values, valueAddresses, nonNullFrom, nonNullTo);
        }
    }

    // Sort non-null range [nonNullFrom, nonNullTo) by struct value.
    if (nonNullFrom + 1 < nonNullTo) {
        auto *addrBase = valueAddresses;
        const int32_t asc = sortAscending;
        const bool nullsFirst = sortNullFirsts[currentCol] != 0;
        std::sort(addrBase + nonNullFrom, addrBase + nonNullTo, [&](uint64_t a, uint64_t b) {
            uint32_t leftColIdx = DecodeSliceIndex(a);
            uint32_t leftRowIdx = DecodePosition(a);
            uint32_t rightColIdx = DecodeSliceIndex(b);
            uint32_t rightRowIdx = DecodePosition(b);
            vec::BaseVector *leftCol = sortColumn[leftColIdx];
            vec::BaseVector *rightCol = sortColumn[rightColIdx];
            int32_t c = OperatorUtil::CompareStructValue(
                leftCol, static_cast<int32_t>(leftRowIdx), rightCol, static_cast<int32_t>(rightRowIdx), nullsFirst);
            return asc ? (c < 0) : (c > 0);
        });
    }

    if (currentCol == sortColCount - 1) {
        return;
    }

    const auto nextCol = currentCol + 1;
    // For groups of equal structs, recurse on next sort column.
    if (nonNullFrom != from && from + 1 < nonNullFrom) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, nonNullFrom,
            nextCol);
    } else if (nonNullTo != to && nonNullTo + 1 < to) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, nonNullTo, to,
            nextCol);
    }

    if (nonNullFrom + 1 >= nonNullTo) {
        return;
    }

    int32_t start = nonNullFrom;
    const bool nullsFirst = sortNullFirsts[currentCol] != 0;
    for (int32_t i = nonNullFrom + 1; i < nonNullTo; ++i) {
        uint64_t a = valueAddresses[start];
        uint64_t b = valueAddresses[i];
        uint32_t leftColIdx = DecodeSliceIndex(a);
        uint32_t leftRowIdx = DecodePosition(a);
        uint32_t rightColIdx = DecodeSliceIndex(b);
        uint32_t rightRowIdx = DecodePosition(b);
        int32_t c = OperatorUtil::CompareStructValue(
            sortColumn[leftColIdx], static_cast<int32_t>(leftRowIdx),
            sortColumn[rightColIdx], static_cast<int32_t>(rightRowIdx), nullsFirst);
        if (c != 0) {
            if (start + 1 != i) {
                ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, i,
                    nextCol);
            }
            start = i;
        }
    }
    if (start + 1 != nonNullTo) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, nonNullTo,
            nextCol);
    }
}

template <typename RawType>
void PagesIndex::ColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
    int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
    int32_t currentCol)
{
    const auto sortCol = sortCols[currentCol];
    const auto sortAscending = sortAscendings[currentCol];
    const auto sortColumn = columns[sortCol];
    const auto hasNull = hasNulls[sortCol];
    const auto hasDictionary = hasDictionaries[sortCol];
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to; // [nonNullFrom, nonNullTo) mark the range of non-null values
    // we are going to sort one column, so we extract all the rows of the column between from and to
    if (sortNullFirsts[currentCol] == 0) {
        if (hasNull && hasDictionary) {
            SortNullAndGetValue<RawType, true, true, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasNull) {
            SortNullAndGetValue<RawType, true, false, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasDictionary) {
            SortNullAndGetValue<RawType, false, true, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else {
            SortNullAndGetValue<RawType, false, false, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        }
    } else {
        if (hasNull && hasDictionary) {
            SortNullAndGetValue<RawType, true, true, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasNull) {
            SortNullAndGetValue<RawType, true, false, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasDictionary) {
            SortNullAndGetValue<RawType, false, true, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else {
            SortNullAndGetValue<RawType, false, false, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        }
    }

    if (nonNullFrom + 1 < nonNullTo) {
        // second, sort all non-null values
        if constexpr (std::is_same_v<RawType, Decimal128>) {
            if (sortAscending == 0) {
                QuickSortDecimal128<0>(values, valueAddresses, nonNullFrom, nonNullTo);
            } else {
                QuickSortDecimal128<1>(values, valueAddresses, nonNullFrom, nonNullTo);
            }
        } else {
            if constexpr (std::is_same_v<RawType, double>) {
                if (sortAscending == 0) {
                    QuickSortDouble<0>(values, valueAddresses, nonNullFrom, nonNullTo);
                    // QuickSortAscSIMD(reinterpret_cast<double *>(values), valueAddresses, nonNullFrom, nonNullTo);
                } else {
                    QuickSortDouble<1>(values, valueAddresses, nonNullFrom, nonNullTo);
                    // QuickSortAscSIMD(reinterpret_cast<double *>(values), valueAddresses, nonNullFrom, nonNullTo);
                }
            } else if constexpr (std::is_same_v<RawType, float>) {
                if (sortAscending == 0) {
                    QuickSortFloat<0>(values, valueAddresses, nonNullFrom, nonNullTo);
                } else {
                    QuickSortFloat<1>(values, valueAddresses, nonNullFrom, nonNullTo);
                }
            } else {
                if (sortAscending == 0) {
                    QuickSortDescSIMD(values, valueAddresses, nonNullFrom, nonNullTo);
                } else {
                    QuickSortAscSIMD(values, valueAddresses, nonNullFrom, nonNullTo);
                }
            }
        }
    }

    if (currentCol == sortColCount - 1) {
        // currently the last column has been sorted.
        return;
    }

    // third, sort next column for the null range
    const auto nextCol = currentCol + 1;
    if (nonNullFrom != from && from + 1 < nonNullFrom) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, nonNullFrom,
            nextCol);
    } else if (nonNullTo != to && nonNullTo + 1 < to) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, nonNullTo, to,
            nextCol);
    }

    if (nonNullFrom + 1 >= nonNullTo) {
        // if the non-null range has only one or zero element, the sorting is not required.
        return;
    }

    // fourth, divide ranges for non-null values first, and continue to sort the next column for each range
    int32_t start = nonNullFrom;
    if constexpr (std::is_same_v<RawType, Decimal128>) {
        auto currentValuePtr = values[nonNullFrom];
        auto currentValue = *((Decimal128 *)currentValuePtr);
        for (int32_t i = nonNullFrom + 1; i < nonNullTo; ++i) {
            auto valuePtr = values[i];
            auto value = *((Decimal128 *)valuePtr);
            if (value != currentValue) {
                currentValue = value;
                if (start + 1 != i) { // sort next column when |equivalent class| > 1
                    ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start,
                        i, nextCol);
                }
                start = i;
            }
        }
    } else {
        RawType currentValue = values[nonNullFrom];
        for (int32_t i = nonNullFrom + 1; i < nonNullTo; ++i) {
            RawType value = values[i];
            if (!OnlyEqual<RawType>(value, currentValue) != 0) {
                currentValue = value;
                if (start + 1 != i) { // sort next column when |equivalent class| > 1
                    ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start,
                        i, nextCol);
                }
                start = i;
            }
        }
    }

    if (start + 1 != nonNullTo) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, nonNullTo,
            nextCol);
    }
}

void PagesIndex::VarcharColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings,
    const int32_t *sortNullFirsts, int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength,
    int32_t from, int32_t to, int32_t currentCol)
{
    const auto sortCol = sortCols[currentCol];
    const auto sortAscending = sortAscendings[currentCol];
    const auto sortColumn = columns[sortCol];
    const auto hasNull = hasNulls[sortCol];
    const auto hasDictionary = hasDictionaries[sortCol];
    int32_t nonNullFrom = from;
    int32_t nonNullTo = to; // [nonNullFrom, nonNullTo) mark the range of non-null values
    // we are going to sort one column, so we extract all the rows of the column between from and to
    if (sortNullFirsts[currentCol] == 0) {
        if (hasNull && hasDictionary) {
            SortNullAndGetVarcharValue<true, true, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasNull) {
            SortNullAndGetVarcharValue<true, false, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasDictionary) {
            SortNullAndGetVarcharValue<false, true, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else {
            SortNullAndGetVarcharValue<false, false, false>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        }
    } else {
        if (hasNull && hasDictionary) {
            SortNullAndGetVarcharValue<true, true, true>(sortColumn, values, varcharLength, valueAddresses, nonNullFrom,
                nonNullTo);
        } else if (hasNull) {
            SortNullAndGetVarcharValue<true, false, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else if (hasDictionary) {
            SortNullAndGetVarcharValue<false, true, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        } else {
            SortNullAndGetVarcharValue<false, false, true>(sortColumn, values, varcharLength, valueAddresses,
                nonNullFrom, nonNullTo);
        }
    }

    if (nonNullFrom + 1 < nonNullTo) {
        // second, sort all non-null values
        if (sortAscending == 0) {
            QuickSortVarChar<0>(values, varcharLength, valueAddresses, nonNullFrom, nonNullTo);
        } else {
            QuickSortVarChar<1>(values, varcharLength, valueAddresses, nonNullFrom, nonNullTo);
        }
    }

    if (currentCol == sortColCount - 1) {
        // currently the last column has been sorted.
        return;
    }

    // third, sort next column for the null range
    const auto nextCol = currentCol + 1;
    if (nonNullFrom != from && from + 1 < nonNullFrom) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, nonNullFrom,
            nextCol);
    } else if (nonNullTo != to && nonNullTo + 1 < to) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, nonNullTo, to,
            nextCol);
    }

    if (nonNullFrom + 1 >= nonNullTo) {
        // if the non-null range has only one or zero element, the sorting is not required.
        return;
    }

    // fourth, divide ranges for non-null values first, and continue to sort the next column for each range
    int64_t currentValue = values[nonNullFrom];
    uint32_t currentLength = varcharLength[nonNullFrom];
    int32_t start = nonNullFrom;
    for (int32_t i = nonNullFrom + 1; i < nonNullTo; ++i) {
        int64_t value = values[i];
        uint32_t length = varcharLength[i];
        if (!OnlyEqualVarChar(value, length, currentValue, currentLength)) {
            currentValue = value;
            currentLength = length;
            if (start + 1 != i) { // sort next column when |equivalent class| > 1
                ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, i,
                    nextCol);
            }
            start = i;
        }
    }
    if (start + 1 != nonNullTo) {
        ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, start, nonNullTo,
            nextCol);
    }
}

void PagesIndex::ColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
    int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
    int32_t currentCol)
{
    switch (dataTypes.GetType(sortCols[currentCol])->GetId()) {
        case OMNI_BYTE: {
            ColumnarSort<int8_t>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from,
                to, currentCol);
            break;
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            ColumnarSort<int32_t>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from,
                to, currentCol);
            break;
        }
        case OMNI_SHORT: {
            ColumnarSort<int16_t>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from,
                to, currentCol);
            break;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            ColumnarSort<int64_t>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from,
                to, currentCol);
            break;
        }
        case OMNI_DOUBLE: {
            ColumnarSort<double>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from,
                to, currentCol);
            break;
        }
        case OMNI_FLOAT: {
            ColumnarSort<float>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from,
                to, currentCol);
            break;
        }
        case OMNI_BOOLEAN: {
            ColumnarSort<bool>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, to,
                currentCol);
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            VarcharColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, (int64_t *)values,
                varcharLength, from, to, currentCol);
            break;
        }
        case OMNI_DECIMAL128: {
            ColumnarSort<Decimal128>(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength,
                from, to, currentCol);
            break;
        }
        case OMNI_ARRAY: {
            ArrayColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, to,
                currentCol);
            break;
        }
        case OMNI_ROW: {
            StructColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values, varcharLength, from, to,
                currentCol);
            break;
        }
        default:
            break;
    }
}

void PagesIndex::SortInplace(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
    int32_t sortColCount, int32_t from, int32_t to)
{
    DYNAMIC_TYPE_DISPATCH(SortInplace, dataTypes.GetIds()[sortCols[0]], sortAscendings[0], sortNullFirsts[0], from, to);
}

template <DataTypeId typeId>
void PagesIndex::SortInplace(int32_t sortAscending, int32_t sortNullFirst, int32_t from, int32_t to)
{
    using T = typename NativeType<typeId>::type;
    auto *values = reinterpret_cast<T *>(VectorHelper::UnsafeGetValues(inplaceSortColumn));
    if (sortAscending) {
        auto comp = [](const T &left, const T &right) { return left < right; };
        if (sortNullFirst) {
            // null values have been preprocessed and can be skipped directly
            std::sort(values + totalNullCount + from, values + to, comp);
        } else {
            std::sort(values + from, values + to - totalNullCount, comp);
        }
    } else {
        auto comp = [](const T &left, const T &right) { return left > right; };
        if (sortNullFirst) {
            // null values have been preprocessed and can be skipped directly
            std::sort(values + totalNullCount + from, values + to, comp);
        } else {
            std::sort(values + from, values + to - totalNullCount, comp);
        }
    }
}

void PagesIndex::SortWithRadixSort(const int32_t *sortCols, const int32_t *sortAscendings,
    const int32_t *sortNullFirsts, int32_t sortColCount, int32_t from, int32_t to)
{
    std::vector<Data_type> tempBlock(rowCount * radixRowWidth);
    uint32_t offset = sortNullFirsts[0] ? totalNullCount * radixRowWidth : 0;
    if (sortAscendings[0]) {
        RadixSortMSD<true>(radixComboRow.data() + offset, tempBlock.data() + offset, rowCount - totalNullCount,
            radixValueWidth, false, radixRowWidth);
    } else {
        RadixSortMSD<false>(radixComboRow.data() + offset, tempBlock.data() + offset, rowCount - totalNullCount,
            radixValueWidth, false, radixRowWidth);
    }
}


void PagesIndex::Sort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
    int32_t sortColCount, int32_t from, int32_t to)
{
    // for varchar/char or decimal128, the value is ptr set, but for other types, the value is raw value set
    std::vector<int64_t> values(this->rowCount);
    bool hasVarCharCol = false;
    auto dataTypeIds = dataTypes.GetIds();
    for (int32_t i = 0; i < sortColCount; ++i) {
        auto sortColTypeId = dataTypeIds[sortCols[i]];
        if (sortColTypeId == OMNI_CHAR || sortColTypeId == OMNI_VARCHAR || sortColTypeId == OMNI_VARBINARY) {
            hasVarCharCol = true;
            break;
        }
    }

    auto varcharLength = (hasVarCharCol) ? std::vector<uint32_t>(this->rowCount) : std::vector<uint32_t>{};

    ColumnarSort(sortCols, sortAscendings, sortNullFirsts, sortColCount, values.data(), varcharLength, from, to, 0);
}

void PagesIndex::GetOutput(int32_t *outputCols, int32_t outputColsCount, VectorBatch *outputVecBatch,
    const int32_t *sourceTypes, int32_t offset, int32_t length, int32_t outputIndex) const
{
    BaseVector ***inputVecBatches = this->columns;
    uint64_t *vaStart = valueAddresses + offset;

    for (int32_t j = 0; j < outputColsCount; j++) {
        const int32_t outputCol = outputCols[j];
        const int32_t colTypeId = sourceTypes[outputCol];
        BaseVector **inputVecBatch = inputVecBatches[outputCol];
        const bool hasDictionary = hasDictionaries[outputCol];
        const bool hasNull = hasNulls[outputCol];
        auto outputVector = outputVecBatch->Get(j);
        switch (colTypeId) {
            case OMNI_BYTE:
                ConstructVector<OMNI_BYTE>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                ConstructVector<OMNI_INT>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_SHORT:
                ConstructVector<OMNI_SHORT>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP:
                ConstructVector<OMNI_LONG>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_DOUBLE:
                ConstructVector<OMNI_DOUBLE>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_FLOAT:
                ConstructVector<OMNI_FLOAT>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_BOOLEAN:
                ConstructVector<OMNI_BOOLEAN>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY: {
                ConstructVector<OMNI_VARCHAR>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            }
            case OMNI_DECIMAL128: {
                ConstructVector<OMNI_DECIMAL128>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            }
            case OMNI_ARRAY: {
                ConstructVector<OMNI_ARRAY>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            }
            case OMNI_ROW: {
                ConstructVector<OMNI_ROW>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            }
            case OMNI_MAP: {
                ConstructVector<OMNI_MAP>(vaStart, length, inputVecBatch, hasNull, hasDictionary, outputVector,
                    outputIndex);
                break;
            }
            default:
                break;
        }
    }
}

void PagesIndex::GetOutputRadixSort(int32_t *outputCols, int32_t outputColsCount,
    omniruntime::vec::VectorBatch *outputVecBatch, const int32_t *sourceTypes, int32_t offset, int32_t length) const
{
    const uint8_t *vaStart = radixComboRow.data() + offset * radixRowWidth + radixValueWidth;
    for (int32_t j = 0; j < outputColsCount; ++j) {
        int32_t outputCol = outputCols[j];
        int32_t colTypeId = sourceTypes[outputCol];
        BaseVector **inputVectors = columns[outputCol];
        bool hasDictionary = hasDictionaries[outputCol];
        bool hasNull = hasNulls[outputCol];
        auto outputVector = outputVecBatch->Get(j);
        switch (colTypeId) {
            case OMNI_BYTE:
                ConstructVectorRadixSort<OMNI_BYTE>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                ConstructVectorRadixSort<OMNI_INT>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            case OMNI_SHORT:
                ConstructVectorRadixSort<OMNI_SHORT>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                ConstructVectorRadixSort<OMNI_LONG>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            case OMNI_DOUBLE:
                ConstructVectorRadixSort<OMNI_DOUBLE>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            case OMNI_BOOLEAN:
                ConstructVectorRadixSort<OMNI_BOOLEAN>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY: {
                ConstructVectorRadixSort<OMNI_VARCHAR>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            }
            case OMNI_DECIMAL128:
                ConstructVectorRadixSort<OMNI_DECIMAL128>(vaStart, length, inputVectors, hasNull, hasDictionary,
                    radixRowWidth, outputVector);
                break;
            default:
                break;
        }
    }
}

void PagesIndex::GetOutputInplaceSort(int32_t *outputCols, int32_t outputColsCount,
    omniruntime::vec::VectorBatch *outputVecBatch, const int32_t *sourceTypes, int32_t offset, int32_t length) const
{
    auto outputVector = outputVecBatch->Get(0);
    VectorHelper::CopyFlatVector(outputVector, inplaceSortColumn, offset, length);
}

PagesIndex::~PagesIndex()
{
    Clear();
}

// ROW path: never uses NativeType<OMNI_ROW>::type. Only instantiated for OMNI_ROW.
template <bool hasNull, bool hasDictionary>
static ALWAYS_INLINE void SetValueRow(BaseVector *inputVector, int32_t inputIndex, BaseVector *outputVector,
    int32_t outputIndex)
{
    if constexpr (hasNull) {
        if (UNLIKELY(inputVector->IsNull(inputIndex))) {
            outputVector->SetNull(outputIndex);
            return;
        }
    }
    auto *inputRow = static_cast<RowVector *>(inputVector);
    auto *outputRow = static_cast<RowVector *>(outputVector);
    auto *slice = inputRow->Slice(inputIndex, 1, false);
    outputRow->Append(slice, outputIndex, 1);
    delete slice;
}

template <bool hasNull, bool hasDictionary>
static ALWAYS_INLINE void SetValueMap(BaseVector *inputVector, int32_t inputIndex, BaseVector *outputVector,
    int32_t outputIndex)
{
    if constexpr (hasNull) {
        if (UNLIKELY(inputVector->IsNull(inputIndex))) {
            static_cast<MapVector *>(outputVector)->SetNull(outputIndex);
            return;
        }
    }
    auto *inputMap = static_cast<MapVector *>(inputVector);
    auto *outputMap = static_cast<MapVector *>(outputVector);
    std::unique_ptr<MapVector> slice(inputMap->Slice(inputIndex, 1, false));
    auto keyCount = static_cast<int32_t>(slice->GetOffset(1) - slice->GetOffset(0));
    auto dstOffset = static_cast<int32_t>(outputMap->GetOffset(outputIndex));
    outputMap->SetOffset(outputIndex + 1, dstOffset + keyCount);
    if (keyCount > 0) {
        outputMap->Append(slice.get(), dstOffset);
    }
}

// Scalar/ARRAY path: uses NativeType<dataTypeId>::type. Never instantiated for OMNI_ROW or OMNI_MAP.
template <type::DataTypeId dataTypeId, bool hasNull, bool hasDictionary>
static ALWAYS_INLINE void SetValueNonRow(BaseVector *inputVector, int32_t inputIndex, BaseVector *outputVector,
    int32_t outputIndex)
{
    if constexpr (hasNull) {
        // ConstVector is never null, skip null check for it to avoid NullsBuffer out-of-bounds access
        if (UNLIKELY(inputVector->GetEncoding() != OMNI_ENCODING_CONST && inputVector->IsNull(inputIndex))) {
            if constexpr (dataTypeId == OMNI_VARCHAR) {
                static_cast<VarcharVector *>(outputVector)->SetNull(outputIndex);
            } else if constexpr (dataTypeId == OMNI_ARRAY) {
                static_cast<ArrayVector *>(outputVector)->SetNull(outputIndex);
            } else {
                outputVector->SetNull(outputIndex);
            }
            return;
        }
    }

    using T = typename NativeType<dataTypeId>::type;
    T value;
    if constexpr (dataTypeId == OMNI_VARCHAR) {
        if (UNLIKELY(inputVector->GetEncoding() == OMNI_ENCODING_CONST)) {
            value = static_cast<ConstVector<std::string_view> *>(inputVector)->GetConstValue();
        } else if constexpr (hasDictionary) {
            if (UNLIKELY(inputVector->GetEncoding() == OMNI_DICTIONARY)) {
                value = static_cast<DictionaryVarcharVector *>(inputVector)->GetValue(inputIndex);
            } else {
                value = static_cast<VarcharVector *>(inputVector)->GetValue(inputIndex);
            }
        } else {
            value = static_cast<VarcharVector *>(inputVector)->GetValue(inputIndex);
        }
        static_cast<VarcharVector *>(outputVector)->SetValue(outputIndex, value);
    } else {
        if (UNLIKELY(inputVector->GetEncoding() == OMNI_ENCODING_CONST)) {
            value = static_cast<ConstVector<T> *>(inputVector)->GetConstValue();
            static_cast<Vector<T> *>(outputVector)->SetValue(outputIndex, value);
        } else if constexpr (hasDictionary) {
            if (UNLIKELY(inputVector->GetEncoding() == OMNI_DICTIONARY)) {
                value = static_cast<Vector<DictionaryContainer<T>> *>(inputVector)->GetValue(inputIndex);
            } else {
                value = static_cast<Vector<T> *>(inputVector)->GetValue(inputIndex);
            }
            static_cast<Vector<T> *>(outputVector)->SetValue(outputIndex, value);
        } else {
            if constexpr (dataTypeId == OMNI_ARRAY) {
                BaseVector *arrayValue = static_cast<ArrayVector *>(inputVector)->GetValue(inputIndex);
                static_cast<ArrayVector *>(outputVector)->SetValue(outputIndex, arrayValue);
                delete arrayValue;
            } else {
                value = static_cast<Vector<T> *>(inputVector)->GetValue(inputIndex);
                static_cast<Vector<T> *>(outputVector)->SetValue(outputIndex, value);
            }
        }
    }
}

template <type::DataTypeId dataTypeId, bool hasNull, bool hasDictionary>
static ALWAYS_INLINE void SetValue(BaseVector *inputVector, int32_t inputIndex, BaseVector *outputVector,
    int32_t outputIndex)
{
    if constexpr (dataTypeId == OMNI_ROW) {
        SetValueRow<hasNull, hasDictionary>(inputVector, inputIndex, outputVector, outputIndex);
    } else if constexpr (dataTypeId == OMNI_MAP) {
        SetValueMap<hasNull, hasDictionary>(inputVector, inputIndex, outputVector, outputIndex);
    } else {
        SetValueNonRow<dataTypeId, hasNull, hasDictionary>(inputVector, inputIndex, outputVector, outputIndex);
    }
}

template <type::DataTypeId dataTypeId>
NO_INLINE void ConstructVector(uint64_t *vaStart, int32_t length, BaseVector **inputVecBatch, bool hasNull,
    bool hasDictionary, BaseVector *outputVector, int32_t outputIndex)
{
    uint64_t *vaEnd = vaStart + length;
    if (hasNull && hasDictionary) {
        while (vaStart < vaEnd) { // here unroll is almost useless due to the excessive checks in SetValue
            const uint64_t valueAddress = *(vaStart++);
            const uint32_t pageIndex = DecodeSliceIndex(valueAddress);
            const uint32_t position = DecodePosition(valueAddress);
            auto* inputVector = inputVecBatch[pageIndex];
            SetValue<dataTypeId, true, true>(inputVector, static_cast<int32_t>(position), outputVector, outputIndex++);
        }
    } else if (hasNull) {
        while (vaStart < vaEnd) { // here unroll is almost useless due to the excessive checks in SetValue
            const uint64_t valueAddress = *(vaStart++);
            const uint32_t pageIndex = DecodeSliceIndex(valueAddress);
            const uint32_t position = DecodePosition(valueAddress);
            auto* inputVector = inputVecBatch[pageIndex];
            SetValue<dataTypeId, true, false>(inputVector, static_cast<int32_t>(position), outputVector, outputIndex++);
        }
    } else if (hasDictionary) {
        while (vaStart < vaEnd) { // here unroll is almost useless due to the excessive checks in SetValue
            const uint64_t valueAddress = *(vaStart++);
            const uint32_t pageIndex = DecodeSliceIndex(valueAddress);
            const uint32_t position = DecodePosition(valueAddress);
            auto* inputVector = inputVecBatch[pageIndex];
            SetValue<dataTypeId, false, true>(inputVector, static_cast<int32_t>(position), outputVector, outputIndex++);
        }
    } else {
        while (vaStart < vaEnd) { // here unroll is almost useless due to the excessive checks in SetValue
            const uint64_t valueAddress = *(vaStart++);
            const uint32_t pageIndex = DecodeSliceIndex(valueAddress);
            const uint32_t position = DecodePosition(valueAddress);
            auto* inputVector = inputVecBatch[pageIndex];
            SetValue<dataTypeId, false, false>(inputVector, static_cast<int32_t>(position), outputVector,
                outputIndex++);
        }
    }
}

template <type::DataTypeId dataTypeId>
NO_INLINE void ConstructVectorRadixSort(const uint8_t *vaStart, int32_t length, BaseVector **inputVectors,
    bool hasNull, bool hasDictionary, uint32_t radixRowWidth, BaseVector *outputVector)
{
    int32_t outputIndex = 0;
    const uint8_t *vaEnd = vaStart + length * radixRowWidth;
    if (hasNull && hasDictionary) {
        while (vaStart < vaEnd) {
            auto vecBatchIdx = *reinterpret_cast<const uint16_t *>(vaStart);
            auto position = *reinterpret_cast<const uint32_t *>(vaStart + SHORT_NBYTES);
            vaStart += radixRowWidth;
            auto inputVector = inputVectors[vecBatchIdx];
            SetValue<dataTypeId, true, true>(inputVector, static_cast<int32_t>(position), outputVector, outputIndex++);
        }
    } else if (hasNull) {
        while (vaStart < vaEnd) {
            auto vecBatchIdx = *reinterpret_cast<const uint16_t *>(vaStart);
            auto position = *reinterpret_cast<const uint32_t *>(vaStart + SHORT_NBYTES);
            vaStart += radixRowWidth;
            auto inputVector = inputVectors[vecBatchIdx];
            SetValue<dataTypeId, true, false>(inputVector, static_cast<int32_t>(position), outputVector, outputIndex++);
        }
    } else if (hasDictionary) {
        while (vaStart < vaEnd) {
            auto vecBatchIdx = *reinterpret_cast<const uint16_t *>(vaStart);
            auto position = *reinterpret_cast<const uint32_t *>(vaStart + SHORT_NBYTES);
            vaStart += radixRowWidth;
            auto inputVector = inputVectors[vecBatchIdx];
            SetValue<dataTypeId, false, true>(inputVector, static_cast<int32_t>(position), outputVector, outputIndex++);
        }
    } else {
        while (vaStart < vaEnd) {
            auto vecBatchIdx = *reinterpret_cast<const uint16_t *>(vaStart);
            auto position = *reinterpret_cast<const uint32_t *>(vaStart + SHORT_NBYTES);
            vaStart += radixRowWidth;
            auto inputVector = inputVectors[vecBatchIdx];
            SetValue<dataTypeId, false, false>(inputVector, static_cast<int32_t>(position), outputVector,
                outputIndex++);
        }
    }
}

void PagesIndex::SetSpillVecBatch(vec::VectorBatch *spillVecBatch, std::vector<int32_t> &outputCols, uint64_t rowOffset,
    bool canInplaceSort, bool canRadixSort)
{
    const int32_t outputColsCount = static_cast<int32_t>(outputCols.size());
    const int32_t offset = static_cast<int32_t>(rowOffset);
    const int32_t rowCount = spillVecBatch->GetRowCount();
    if (canInplaceSort) {
        GetOutputInplaceSort(outputCols.data(), outputColsCount, spillVecBatch, dataTypes.GetIds(), offset, rowCount);
    } else if (canRadixSort) {
        GetOutputRadixSort(outputCols.data(), outputColsCount, spillVecBatch, dataTypes.GetIds(), offset, rowCount);
    } else {
        GetOutput(outputCols.data(), outputColsCount, spillVecBatch, dataTypes.GetIds(), offset, rowCount);
    }
}

void PagesIndex::Clear()
{
    if (columns != nullptr) {
        for (uint32_t colIdx = 0; colIdx < typesCount; ++colIdx) {
            delete[] columns[colIdx];
        }
    }

    delete[] columns;
    columns = nullptr;

    delete[] valueAddresses;
    valueAddresses = nullptr;

    delete inplaceSortColumn;
    inplaceSortColumn = nullptr;

    rowCount = 0;
    totalNullCount = 0;
    VectorHelper::FreeVecBatches(inputVecBatches);
    inputVecBatches.clear();
}
}
