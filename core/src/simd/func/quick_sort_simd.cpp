/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include "small_case_sort.h"
#include "simd/simd.h"
#include "libboundscheck/include/securec.h"
#include "quick_sort_simd.h"
constexpr int32_t SMALL_CASE_LENGTH = 16;
constexpr int32_t CHUNK_SIZE = 8;
constexpr int32_t UNROLL_SIZE = 4;
constexpr int32_t BUFFER_SIZE = 50;
template <class D, class Traits, class RawType>
static int32_t inline DrawSamples(D d, Traits st, RawType *values, int32_t from, int32_t to, RawType *valueBuf)
{
    int32_t num = to - from;
    RawType *valueStart = values + from;
    RawType *valueMid = values + from + num / 2;
    RawType *valueLast = values + to - OMNI_LANES(RawType);

    int32_t oneStep = num / CHUNK_SIZE;
    int32_t twoStep = oneStep + oneStep;
    using V = Vec<decltype(d)>;
    V valueVec1 = LoadU(d, valueStart);
    V valueVec2 = LoadU(d, valueStart + oneStep);
    V valueVec3 = LoadU(d, valueStart + twoStep);
    V valueVec4 = LoadU(d, valueMid - oneStep);
    V valueVec5 = LoadU(d, valueMid);
    V valueVec6 = LoadU(d, valueMid + oneStep);
    V valueVec7 = LoadU(d, valueLast);
    V valueVec8 = LoadU(d, valueLast - oneStep);
    V valueVec9 = LoadU(d, valueLast - twoStep);

    V medianVal1 = MedianOf3(st, valueVec1, valueVec2, valueVec3);
    V medianVal2 = MedianOf3(st, valueVec4, valueVec5, valueVec6);
    V medianVal3 = MedianOf3(st, valueVec7, valueVec8, valueVec9);
    StoreU(medianVal1, d, valueBuf);
    StoreU(medianVal2, d, valueBuf + OMNI_LANES(RawType));
    StoreU(medianVal3, d, valueBuf + 2 * OMNI_LANES(RawType));
    return OMNI_LANES(RawType) * 3;
}

template <class D, typename RawType> static VFromD<D> inline ChoosePivot(D d, RawType *buf, int32_t from, int32_t to)
{
    constexpr int32_t N1 = 1;
    int32_t mid = from + (to - from) / 2;
    int32_t prev = mid - N1;

    RawType midValue = buf[mid];
    for (; std::fabs(buf[prev] - midValue) < DBL_EPSILON; prev -= N1) {
        if (prev == from) {
            return GetVec(d, buf[prev]);
        }
    }
    int32_t next = prev + N1;
    int32_t last = to - 1;
    for (; std::fabs(buf[next] - midValue) < DBL_EPSILON; next += N1) {
        if (next == last) {
            return GetVec(d, buf[prev]);
        }
    }
    int32_t excessIfMedian = next - mid;
    int32_t excessIfPrev = mid - prev;
    RawType pivot = excessIfMedian < excessIfPrev ? midValue : buf[prev];
    return GetVec(d, pivot);
}

template <class D, class Traits, class RawType> static VFromD<D> inline GetSmallestVec(D d, Traits st)
{
    return st.GetExtremumVector();
}

template <class D, class Traits, class V, typename RawType>
int32_t PartitionToMultipleOfUnroll(D d, Traits st, RawType *values, AddrType *addresses, int32_t &num, V pivotVec,
                                    RawType *valueBuf, AddrType *addrBuf, V &smallestVec, V &largestVec)
{
    const int32_t numRem =
        (num < 2 * UNROLL_SIZE * OMNI_LANES(TFromD<D>)) ? num : (num & (UNROLL_SIZE * OMNI_LANES(TFromD<D>) - 1));
    if (numRem <= 0) {
        return 0;
    }

    int32_t readL = 0;
    RawType *valuePosL = values;
    AddrType *addrPosL = addresses;
    int32_t bufR = 0;
    MFromD<D> compareResult;
    MFromD<D> blendedMask;
    int32_t i = 0;
    for (; i + OMNI_LANES(TFromD<D>) <= numRem; i += OMNI_LANES(TFromD<D>)) {
        V valueVec = LoadU(d, values + readL);
        st.UpdateMinMax(d, valueVec, smallestVec, largestVec);

        auto addrVec = LoadU(ADDR_TAG, addresses + readL);
        readL += OMNI_LANES(TFromD<D>);

        compareResult = st.QuickSortCompare(d, pivotVec, valueVec);
        blendedMask = Not(compareResult);
        auto sizeL = CompressBlendedStore(d, valueVec, addrVec, blendedMask, valuePosL, addrPosL);
        valuePosL += sizeL;
        addrPosL += sizeL;

        bufR += CompressStore(d, valueVec, addrVec, compareResult, valueBuf + bufR, addrBuf + bufR);
    }

    if (i != numRem) {
        auto firstMask = FirstN(d, numRem - i);
        auto lastValueVec = LoadU(d, values + readL);
        st.UpdateMinMax(d, lastValueVec, smallestVec, largestVec);
        auto lastAddrVec = LoadU(ADDR_TAG, addresses + readL);
        compareResult = st.QuickSortCompare(d, pivotVec, lastValueVec);
        blendedMask = GetBlendedMask(d, firstMask, compareResult);
        auto sizeL = CompressBlendedStore(d, lastValueVec, lastAddrVec, blendedMask, valuePosL, addrPosL);
        valuePosL += sizeL;
        addrPosL += sizeL;
        MFromD<D> mask = GetAndMask(d, compareResult, firstMask);
        bufR += CompressStore(d, lastValueVec, lastAddrVec, mask, valueBuf + bufR, addrBuf + bufR);
    }
    num -= static_cast<int>(bufR);
    auto copyByteSize = bufR * sizeof(int64_t);
    memcpy_s(valuePosL, copyByteSize, values + num, copyByteSize);
    memcpy_s(addrPosL, copyByteSize, addresses + num, copyByteSize);
    memcpy_s(values + num, copyByteSize, valueBuf, copyByteSize);
    memcpy_s(addresses + num, copyByteSize, addrBuf, copyByteSize);
    return static_cast<int32_t>(valuePosL - values);
}
template <class D, class Traits, typename T>
inline void StoreLeftRight(D d, Traits st, const Vec<D> v, AddrVec a, const Vec<D> pivot, T *__restrict__ keys,
                           AddrType *addresses, int32_t &writeL, int32_t &remaining)
{
    const size_t N = Lanes(d);
    const Mask<D> comp = st.QuickSortCompare(d, pivot, v);
    remaining -= N;
    if (CompressIsPartition<T>::value) {
        const Vec<D> lr = st.CompressKeys(v, comp);
        const AddrVec addLr = st.CompressKeys(a, comp);
        const size_t numLeft = N - CountTrue(d, comp);
        StoreU(lr, d, keys + writeL);
        StoreU(addLr, ADDR_TAG, addresses + writeL);
        StoreU(lr, d, keys + remaining + writeL);
        StoreU(addLr, ADDR_TAG, addresses + remaining + writeL);
        writeL += numLeft;
    } else {
        const size_t numLeft = CompressStore(v, Not(comp), d, keys + writeL);
        writeL += numLeft;
        (void)CompressBlendedStore(v, comp, d, keys + remaining + writeL);
        (void)CompressBlendedStore(a, comp, ADDR_TAG, addresses + remaining + writeL);
    }
}
template <class D, class Traits, typename RawType, class V>
static void inline StoreLeftRight4(D d, Traits st, RawType *keys, V pivot, V v0, V v1, V v2, V v3, AddrType *addresses,
                                   AddrVec a0, AddrVec a1, AddrVec a2, AddrVec a3, int32_t &writeL, int32_t &remaining,
                                   V &smallestVec, V &largestVec)
{
    StoreLeftRight(d, st, v0, a0, pivot, keys, addresses, writeL, remaining);
    StoreLeftRight(d, st, v1, a1, pivot, keys, addresses, writeL, remaining);
    StoreLeftRight(d, st, v2, a2, pivot, keys, addresses, writeL, remaining);
    StoreLeftRight(d, st, v3, a3, pivot, keys, addresses, writeL, remaining);
    st.UpdateMinMax(d, v0, smallestVec, largestVec);
    st.UpdateMinMax(d, v1, smallestVec, largestVec);
    st.UpdateMinMax(d, v2, smallestVec, largestVec);
    st.UpdateMinMax(d, v3, smallestVec, largestVec);
}

template <class D, class Traits, class V, typename RawType>
int32_t PartitionWithSIMD(D d, Traits st, RawType *values, AddrType *addresses, int32_t from, int32_t to, V pivotVec,
                          RawType *valueBuf, AddrType *addrBuf, V &smallestVec, V &largestVec)
{
    int32_t num = to - from;
    RawType *valueStart = values + from;
    AddrType *addressStart = addresses + from;

    num -= OMNI_LANES(TFromD<D>);
    int32_t last = num;

    V valueLast = LoadU(d, valueStart + last);
    st.UpdateMinMax(d, valueLast, smallestVec, largestVec);
    auto addrLast = LoadU(ADDR_TAG, addressStart + last);

    int32_t consumedL = PartitionToMultipleOfUnroll(d, st, valueStart, addressStart, num, pivotVec, valueBuf, addrBuf,
                                                    smallestVec, largestVec);
    valueStart += consumedL;
    addressStart += consumedL;
    last -= consumedL;
    num -= consumedL;

    RawType *valueReadL = valueStart;
    RawType *valueReadR = valueStart + num;
    AddrType *addrReadL = addressStart;
    AddrType *addrReadR = addressStart + num;
    int32_t writeL = 0;
    int32_t remaining = num;
    if (num != 0) {
        constexpr int32_t pos0 = 0;
        constexpr int32_t pos1 = OMNI_LANES(TFromD<D>);
        constexpr int32_t pos2 = 2 * OMNI_LANES(TFromD<D>);
        constexpr int32_t pos3 = 3 * OMNI_LANES(TFromD<D>);

        V leftValueVec0 = LoadU(d, valueReadL + pos0);
        V leftValueVec1 = LoadU(d, valueReadL + pos1);
        V leftValueVec2 = LoadU(d, valueReadL + pos2);
        V leftValueVec3 = LoadU(d, valueReadL + pos3);
        valueReadL += UNROLL_SIZE * OMNI_LANES(TFromD<D>);
        valueReadR -= UNROLL_SIZE * OMNI_LANES(TFromD<D>);
        V rightValueVec0 = LoadU(d, valueReadR + pos0);
        V rightValueVec1 = LoadU(d, valueReadR + pos1);
        V rightValueVec2 = LoadU(d, valueReadR + pos2);
        V rightValueVec3 = LoadU(d, valueReadR + pos3);

        AddrVec leftAddrVec0 = LoadU(ADDR_TAG, addrReadL + pos0);
        AddrVec leftAddrVec1 = LoadU(ADDR_TAG, addrReadL + pos1);
        AddrVec leftAddrVec2 = LoadU(ADDR_TAG, addrReadL + pos2);
        AddrVec leftAddrVec3 = LoadU(ADDR_TAG, addrReadL + pos3);
        addrReadL += UNROLL_SIZE * OMNI_LANES(TFromD<D>);
        addrReadR -= UNROLL_SIZE * OMNI_LANES(TFromD<D>);
        AddrVec rightAddrVec0 = LoadU(ADDR_TAG, addrReadR + pos0);
        AddrVec rightAddrVec1 = LoadU(ADDR_TAG, addrReadR + pos1);
        AddrVec rightAddrVec2 = LoadU(ADDR_TAG, addrReadR + pos2);
        AddrVec rightAddrVec3 = LoadU(ADDR_TAG, addrReadR + pos3);

        while (valueReadL != valueReadR) {
            V curValueVec0;
            V curValueVec1;
            V curValueVec2;
            V curValueVec3;
            AddrVec curAddrVec0;
            AddrVec curAddrVec1;
            AddrVec curAddrVec2;
            AddrVec curAddrVec3;
            constexpr int32_t prefetchStep = 3 * UNROLL_SIZE * OMNI_LANES(TFromD<D>);

            int32_t capacityL = static_cast<int32_t>(valueReadL - valueStart) - writeL;
            if (capacityL > UNROLL_SIZE * OMNI_LANES(TFromD<D>)) {
                valueReadR -= UNROLL_SIZE * OMNI_LANES(TFromD<D>);
                curValueVec0 = LoadU(d, valueReadR + pos0);
                curValueVec1 = LoadU(d, valueReadR + pos1);
                curValueVec2 = LoadU(d, valueReadR + pos2);
                curValueVec3 = LoadU(d, valueReadR + pos3);
                __builtin_prefetch(valueReadR - prefetchStep, 0, 3);

                addrReadR -= UNROLL_SIZE * OMNI_LANES(TFromD<D>);
                curAddrVec0 = LoadU(ADDR_TAG, addrReadR + pos0);
                curAddrVec1 = LoadU(ADDR_TAG, addrReadR + pos1);
                curAddrVec2 = LoadU(ADDR_TAG, addrReadR + pos2);
                curAddrVec3 = LoadU(ADDR_TAG, addrReadR + pos3);
                __builtin_prefetch(addrReadR - prefetchStep, 0, 3);
            } else {
                curValueVec0 = LoadU(d, valueReadL + pos0);
                curValueVec1 = LoadU(d, valueReadL + pos1);
                curValueVec2 = LoadU(d, valueReadL + pos2);
                curValueVec3 = LoadU(d, valueReadL + pos3);
                valueReadL += UNROLL_SIZE * OMNI_LANES(TFromD<D>);
                __builtin_prefetch(valueReadL + prefetchStep, 0, 3);

                curAddrVec0 = LoadU(ADDR_TAG, addrReadL + pos0);
                curAddrVec1 = LoadU(ADDR_TAG, addrReadL + pos1);
                curAddrVec2 = LoadU(ADDR_TAG, addrReadL + pos2);
                curAddrVec3 = LoadU(ADDR_TAG, addrReadL + pos3);
                addrReadL += UNROLL_SIZE * OMNI_LANES(TFromD<D>);
                __builtin_prefetch(addrReadL + prefetchStep, 0, 3);
            }

            StoreLeftRight4(d, st, valueStart, pivotVec, curValueVec0, curValueVec1, curValueVec2, curValueVec3,
                            addressStart, curAddrVec0, curAddrVec1, curAddrVec2, curAddrVec3, writeL, remaining,
                            smallestVec, largestVec);
        }

        StoreLeftRight4(d, st, valueStart, pivotVec, leftValueVec0, leftValueVec1, leftValueVec2, leftValueVec3,
                        addressStart, leftAddrVec0, leftAddrVec1, leftAddrVec2, leftAddrVec3, writeL, remaining,
                        smallestVec, largestVec);
        StoreLeftRight4(d, st, valueStart, pivotVec, rightValueVec0, rightValueVec1, rightValueVec2, rightValueVec3,
                        addressStart, rightAddrVec0, rightAddrVec1, rightAddrVec2, rightAddrVec3, writeL, remaining,
                        smallestVec, largestVec);
    }

    int32_t totalR = last - writeL;
    MFromD<D> compareResult = st.QuickSortCompare(d, pivotVec, valueLast);
    MFromD<D> blendedMask = Not(compareResult);
    if (totalR == 0) {
        writeL += CompressStore(d, valueLast, addrLast, blendedMask, valueStart + writeL, addressStart + writeL);
    } else {
        int32_t startR = totalR < OMNI_LANES(TFromD<D>) ? writeL + totalR - OMNI_LANES(TFromD<D>) : writeL;
        auto valueRightVec = LoadU(d, valueStart + startR);
        st.UpdateMinMax(d, valueRightVec, smallestVec, largestVec);
        StoreU(valueRightVec, d, valueStart + last);
        auto addrRightVec = LoadU(ADDR_TAG, addressStart + startR);
        StoreU(addrRightVec, ADDR_TAG, addressStart + last);

        writeL += CompressBlendedStore(d, valueLast, addrLast, blendedMask, valueStart + writeL, addressStart + writeL);
        CompressBlendedStore(d, valueLast, addrLast, compareResult, valueStart + writeL, addressStart + writeL);
    }
    return consumedL + writeL + from;
}

template void QuickSortAscSIMD<long>(long *, unsigned long *, int, int);
template void QuickSortDescSIMD<double>(double *, unsigned long *, int, int);
template void QuickSortDescSIMD<long>(long *, unsigned long *, int, int);
template void QuickSortAscSIMD<double>(double *, unsigned long *, int, int);
template <class D, class Traits, class RawType>
void QuickSortInternalSIMD(D d, Traits st, RawType *values, AddrType *addresses, int32_t from, int32_t to,
                           RawType *valueBuf, AddrType *addrBuf, bool chooseAvg, RawType avg)
{
    int32_t num = to - from;
    if (num <= SMALL_CASE_LENGTH) {
        if (st.currentOrder == SortOrder::DESCENDING) {
            simd::SmallCaseSortDesc<RawType>(reinterpret_cast<int64_t *>(values), addresses, from, to);
        } else {
            simd::SmallCaseSortAsec<RawType>(reinterpret_cast<int64_t *>(values), addresses, from, to);
        }
        return;
    }
    using V = Vec<decltype(d)>;
    V pivotVal;
    if (chooseAvg) {
        RawType avgVec[OMNI_LANES(RawType)] = {0};
        for (int64_t i = 0; i < OMNI_LANES(RawType); i++) {
            avgVec[i] = avg;
        }
        pivotVal = LoadU(d, avgVec);
    } else {
        int32_t count = DrawSamples(d, st, values, from, to, valueBuf);
        simd::Sort3Rows(st, valueBuf, count);
        pivotVal = ChoosePivot(d, valueBuf, 0, count);
    }
    V smallestVec = st.GetSmallestVec(d);

    V largestVec = st.GetLargestVec(d);
    int32_t partitionIdx =
        PartitionWithSIMD(d, st, values, addresses, from, to, pivotVal, valueBuf, addrBuf, smallestVec, largestVec);
    double ratio = std::min(to - partitionIdx, partitionIdx - from) / double(to - from);
    if (ratio < 0.2) {
        chooseAvg = !chooseAvg;
    }
    auto smallest = st.GetSmallest(d, smallestVec);
    auto largest = st.GetLargest(d, largestVec);
    RawType pivot = ExtractLane(pivotVal, 0);
    if ((partitionIdx - from > 1) && (NotEqual(pivot, smallest))) {
        RawType newAvg = GetAvg(pivot, smallest);
        QuickSortInternalSIMD(d, st, values, addresses, from, partitionIdx, valueBuf, addrBuf, chooseAvg, newAvg);
    }
    if ((to - partitionIdx > 1) && (NotEqual(pivot, largest))) {
        RawType newAvg = GetAvg(pivot, largest);
        QuickSortInternalSIMD(d, st, values, addresses, partitionIdx, to, valueBuf, addrBuf, chooseAvg, newAvg);
    }
}
// only for ut
#ifdef COVERAGE
template void
QuickSortInternalSIMD<simd::Simd<long, 2ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderAscending<long>>>, long>(
    simd::Simd<long, 2ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderAscending<long>>>, long *, unsigned long *,
    int, int, long *, unsigned long *, bool, long);
template void
QuickSortInternalSIMD<simd::Simd<long, 2ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderDescending<long>>>,
                      long>(simd::Simd<long, 2ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderDescending<long>>>,
                            long *, unsigned long *, int, int, long *, unsigned long *, bool, long);
#else
template void
QuickSortInternalSIMD<simd::Simd<long, 4ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderAscending<long>>>, long>(
    simd::Simd<long, 4ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderAscending<long>>>, long *, unsigned long *,
    int, int, long *, unsigned long *, bool, long);
template void
QuickSortInternalSIMD<simd::Simd<long, 4ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderDescending<long>>>,
                      long>(simd::Simd<long, 4ul, 0>, simd::SharedTraits<simd::TraitsLane<simd::OrderDescending<long>>>,
                            long *, unsigned long *, int, int, long *, unsigned long *, bool, long);
#endif

template <class ValType> void QuickSortAscSIMD(ValType *values, AddrType *addresses, int32_t from, int32_t to)
{
    const SortTag<ValType> d;
    const simd::MakeTraits<ValType, simd::SortAscending> st;
    ValType valueBuf[BUFFER_SIZE];
    AddrType addrBuf[BUFFER_SIZE];
    QuickSortInternalSIMD(d, st, values, addresses, from, to, valueBuf, addrBuf);
}
template <class ValType> void QuickSortDescSIMD(ValType *values, AddrType *addresses, int32_t from, int32_t to)
{
    const SortTag<ValType> d;
    const simd::MakeTraits<ValType, simd::SortDescending> st;
    ValType valueBuf[BUFFER_SIZE];
    AddrType addrBuf[BUFFER_SIZE];
    QuickSortInternalSIMD(d, st, values, addresses, from, to, valueBuf, addrBuf);
}
