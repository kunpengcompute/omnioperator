/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Existing third-party copyright and license terms continue to apply.
 *
 * Copyright (C) 2018 The C++ Component Model(COMO) Open Source Project
 *
 *
 *
 * Third-party license and source notices
 *
 * Description: registry  function  implementation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libboundscheck/include/securec.h>
#include "dtoa.h"

namespace omniruntime::codegen::function {
template <class Dest, class Source> static ALWAYS_INLINE Dest BitCast(const Source &source)
{
    Dest dest;
    memmove_s(&dest, sizeof(dest), &source, sizeof(dest));
    return dest;
}

static ALWAYS_INLINE int64_t Unsigned64RightShift(int64_t input, int32_t shift)
{
    return static_cast<int64_t>(static_cast<uint64_t>(input) >> shift);
}

static ALWAYS_INLINE int32_t Unsigned32RightShift(int32_t input, int32_t shift)
{
    return static_cast<int32_t>(static_cast<uint32_t>(input) >> shift);
}

template <class T> static ALWAYS_INLINE int32_t NumberOfLeadingZeros(T input)
{
    if constexpr (std::is_same_v<T, int64_t>) {
        return __builtin_clzl(input);
    } else {
        return __builtin_clz(input);
    }
}

template <class T> static ALWAYS_INLINE int32_t NumberOfTrailingZeros(T input)
{
    if constexpr (std::is_same_v<T, int64_t>) {
        return __builtin_ctzl(input);
    } else {
        return __builtin_ctz(input);
    }
}

static FDBigInteger GetZero() noexcept
{
    int temp[] = {0};
    auto zero = FDBigInteger(temp, 0, 1);
    zero.MakeImmutable();
    return zero;
}

static std::vector<FDBigInteger> GetPow5Cache() noexcept
{
    static std::vector<FDBigInteger> pow5Cache(FDBigInteger::MAX_FIVE_POW);
    int i = 0;
    while (i < FDBigInteger::SMALL_5_POW_SIZE) {
        int temp[] = {FDBigInteger::SMALL_5_POW[i]};
        FDBigInteger pow5 = FDBigInteger(temp, 0, 1);
        pow5.MakeImmutable();
        pow5Cache[i] = pow5;
        i++;
    }
    FDBigInteger prev = pow5Cache[i - 1];
    while (i < FDBigInteger::MAX_FIVE_POW) {
        prev = prev.Mul(5);
        prev.MakeImmutable();
        pow5Cache[i] = prev;
        i++;
    }
    return pow5Cache;
}

FDBigInteger::FDBigInteger(long lValue, const char *digits, int kDigits, int nDigits)
{
    data[0] = static_cast<int>(lValue); // starting value
    data[1] = static_cast<int>(Unsigned64RightShift(lValue, 32));
    offset = 0;
    nWords = 2;
    int i = kDigits;
    int limit = nDigits - 5; // slurp digits 5 at a time.
    int v;
    while (i < limit) {
        int iLim = i + 5;
        v = static_cast<int>(digits[i++]) - static_cast<int>('0');
        while (i < iLim) {
            v = 10 * v + static_cast<int>(digits[i++]) - static_cast<int>('0');
        }
        MulAddMe(100000, v); // ... where 100000 is 10^5.
    }
    int factor = 1;
    v = 0;
    while (i < nDigits) {
        v = 10 * v + static_cast<int>(digits[i++]) - static_cast<int>('0');
        factor *= 10;
    }
    if (factor != 1) {
        MulAddMe(factor, v);
    }
    TrimLeadingZeros();
}

FDBigInteger FDBigInteger::ZERO = GetZero();
std::vector<FDBigInteger> FDBigInteger::POW_5_CACHE = GetPow5Cache();

void FDBigInteger::TrimLeadingZeros()
{
    int i = nWords;
    if (i > 0 && (data[--i] == 0)) {
        while (i > 0 && data[i - 1] == 0) {
            i--;
        }
        nWords = i;
        if (i == 0) { // all words are zero
            offset = 0;
        }
    }
}

void FDBigInteger::MakeImmutable() { isImmutable = true; }

int FDBigInteger::Cmp(const FDBigInteger &other) const
{
    int aSize = nWords + offset;
    int bSize = other.nWords + other.offset;
    if (aSize > bSize) {
        return 1;
    } else if (aSize < bSize) {
        return -1;
    }
    int aLen = nWords;
    int bLen = other.nWords;
    while (aLen > 0 && bLen > 0) {
        int a = data[--aLen];
        int b = other.data[--bLen];
        if (a != b) {
            return ((a & LONG_MASK) < (b & LONG_MASK)) ? -1 : 1;
        }
    }
    if (aLen > 0) {
        return CheckZeroTail(data, aLen);
    }
    if (bLen > 0) {
        return -CheckZeroTail(other.data, bLen);
    }
    return 0;
}

FDBigInteger FDBigInteger::Mul(int i)
{
    if (nWords == 0) {
        return *this;
    }
    int r[MAX_DATA_LENGTH]{0};
    Mul(data, nWords, i, r);
    return {r, offset, nWords + 1};
}

FDBigInteger FDBigInteger::LeftShift(int shift)
{
    if (shift == 0 || nWords == 0) {
        return *this;
    }
    int wordCount = shift >> 5;
    int bitCount = shift & 0x1f;
    if (isImmutable) {
        if (bitCount == 0) {
            int newData[MAX_DATA_LENGTH]{0};
            int count = nWords * static_cast<int>(sizeof(int));
            memcpy_s(newData, count, data, count);
            return {newData, offset + wordCount, nWords};
        } else {
            int antiCount = 32 - bitCount;
            int idx = nWords - 1;
            int prev = data[idx];
            int hi = Unsigned32RightShift(prev, antiCount);
            int result[MAX_DATA_LENGTH]{0};
            int tmpLen = 0;
            if (hi != 0) {
                tmpLen = nWords + 1;
                result[nWords] = hi;
            } else {
                tmpLen = nWords;
            }
            LeftShift(data, idx, result, bitCount, antiCount, prev);
            return {result, offset + wordCount, tmpLen};
        }
    } else {
        if (bitCount != 0) {
            int antiCount = 32 - bitCount;
            if ((data[0] << bitCount) == 0) {
                int idx = 0;
                int prev = data[idx];
                for (; idx < nWords - 1; idx++) {
                    int v = Unsigned32RightShift(prev, antiCount);
                    prev = data[idx + 1];
                    v |= (prev << bitCount);
                    data[idx] = v;
                }
                int v = Unsigned32RightShift(prev, antiCount);
                data[idx] = v;
                if (v == 0) {
                    nWords--;
                }
                offset++;
            } else {
                int idx = nWords - 1;
                int prev = data[idx];
                int hi = Unsigned32RightShift(prev, antiCount);
                int src[MAX_DATA_LENGTH]{0};
                memcpy_s(src, nWords * sizeof(int), data, nWords * sizeof(int));
                if (hi != 0) {
                    if (nWords == DataSize()) {
                        Resize(nWords + 1);
                    }
                    data[nWords++] = hi;
                }
                LeftShift(src, idx, data, bitCount, antiCount, prev);
            }
        }
        offset += wordCount;
        return *this;
    }
}

int FDBigInteger::AddAndCmp(const FDBigInteger &x, const FDBigInteger &y)
{
    FDBigInteger big;
    FDBigInteger small;
    int xSize = x.Size();
    int ySize = y.Size();
    int bSize;
    int sSize;
    if (xSize >= ySize) {
        big = x;
        small = y;
        bSize = xSize;
        sSize = ySize;
    } else {
        big = y;
        small = x;
        bSize = ySize;
        sSize = xSize;
    }
    int thSize = Size();
    if (bSize == 0) {
        return thSize == 0 ? 0 : 1;
    }
    if (sSize == 0) {
        return Cmp(big);
    }
    if (bSize > thSize) {
        return -1;
    }
    if (bSize + 1 < thSize) {
        return 1;
    }
    long top = (big.data[big.nWords - 1] & LONG_MASK);
    if (sSize == bSize) {
        top += (small.data[small.nWords - 1] & LONG_MASK);
    }
    if (Unsigned64RightShift(top, 32) == 0) {
        if (Unsigned64RightShift(top + 1, 32) == 0) {
            // good case - no carry extension
            if (bSize < thSize) {
                return 1;
            }
            // here sum.nWords == nWords
            long v = (data[nWords - 1] & LONG_MASK);
            if (v < top) {
                return -1;
            }
            if (v > top + 1) {
                return 1;
            }
        }
    } else { // (top>>>32)!=0 guaranteed carry extension
        if (bSize + 1 > thSize) {
            return -1;
        }
        // here sum.nWords == nWords
        Unsigned64RightShift(top, 32);
        long v = (data[nWords - 1] & LONG_MASK);
        if (v < top) {
            return -1;
        }
        if (v > top + 1) {
            return 1;
        }
    }
    return Cmp(big.Add(small));
}

FDBigInteger FDBigInteger::MulBy10()
{
    if (nWords == 0) {
        return *this;
    }
    if (isImmutable) {
        int res[MAX_DATA_LENGTH]{0};
        res[nWords] = MulAndCarryBy10(data, nWords, res);
        return {res, offset, nWords + 1};
    } else {
        int p = MulAndCarryBy10(data, nWords, data);
        if (p != 0) {
            if (nWords == DataSize()) {
                if (data[0] == 0) {
                    auto l = --nWords;
                    memcpy_s(data, l * sizeof(int), data + 1, l * sizeof(int));
                    offset++;
                } else {
                    Resize(DataSize() + 1);
                }
            }
            data[nWords++] = p;
        } else {
            TrimLeadingZeros();
        }
        return *this;
    }
}

void FDBigInteger::Mul(const int *src, int srcLen, int value, int *dst)
{
    long val = value & LONG_MASK;
    long carry = 0;
    for (int i = 0; i < srcLen; i++) {
        long product = (src[i] & LONG_MASK) * val + carry;
        dst[i] = static_cast<int>(product);
        carry = Unsigned64RightShift(product, 32);
    }
    dst[srcLen] = static_cast<int>(carry);
}

FDBigInteger FDBigInteger::Big5PowRec(int p)
{
    if (p < MAX_FIVE_POW) {
        return POW_5_CACHE[p];
    }
    // construct the value.
    // recursively.
    int q, r;
    // in order to compute 5^p,
    // compute its square root, 5^(p/2) and square.
    // or, let q = p / 2, r = p -q, then
    // 5^p = 5^(q+r) = 5^q * 5^r
    q = p >> 1;
    r = p - q;
    FDBigInteger bigQ = Big5PowRec(q);
    if (r < SMALL_5_POW_SIZE) {
        return bigQ.Mul(SMALL_5_POW[r]);
    } else {
        return bigQ.Mul(Big5PowRec(r));
    }
}

FDBigInteger FDBigInteger::Big5Pow(int p)
{
    if (p < MAX_FIVE_POW) {
        return POW_5_CACHE[p];
    }
    return Big5PowRec(p);
}

FDBigInteger FDBigInteger::ValueOfPow52(int p5, int p2)
{
    if (p5 != 0) {
        if (p2 == 0) {
            return Big5Pow(p5);
        } else if (p5 < SMALL_5_POW_SIZE) {
            int pow5 = SMALL_5_POW[p5];
            int wordcount = p2 >> 5;
            int bitCount = p2 & 0x1f;
            if (bitCount == 0) {
                int temp[] = {pow5};
                return {temp, wordcount, 1};
            } else {
                int temp[] = {pow5 << bitCount, Unsigned32RightShift(pow5, 32 - bitCount)};
                return {temp, wordcount, 2};
            }
        } else {
            return Big5Pow(p5).LeftShift(p2);
        }
    } else {
        return ValueOfPow2(p2);
    }
}

FDBigInteger FDBigInteger::ValueOfMulPow52(long value, int p5, int p2)
{
    int v0 = static_cast<int>(value);
    int v1 = static_cast<int>(Unsigned64RightShift(value, 32));
    int wordcount = p2 >> 5;
    int bitCount = p2 & 0x1f;
    if (p5 != 0) {
        if (p5 < SMALL_5_POW_SIZE) {
            long pow5 = SMALL_5_POW[p5] & LONG_MASK;
            long carry = (v0 & LONG_MASK) * pow5;
            v0 = static_cast<int>(carry);
            carry = Unsigned64RightShift(carry, 32);
            carry = (v1 & LONG_MASK) * pow5 + carry;
            v1 = static_cast<int>(carry);
            int v2 = static_cast<int>(Unsigned64RightShift(carry, 32));
            if (bitCount == 0) {
                int temp[] = {v0, v1, v2};
                return {temp, wordcount, 3};
            } else {
                int temp[] = {v0 << bitCount, (v1 << bitCount) | Unsigned32RightShift(v0, 32 - bitCount),
                              (v2 << bitCount) | Unsigned32RightShift(v1, 32 - bitCount),
                              Unsigned32RightShift(v2, 32 - bitCount)};
                return {temp, wordcount, 4};
            }
        } else {
            FDBigInteger pow5 = Big5Pow(p5);
            int r[MAX_DATA_LENGTH]{0};
            int len = 0;
            if (v1 == 0) {
                len = pow5.nWords + 1 + ((p2 != 0) ? 1 : 0);
                Mul(pow5.data, pow5.nWords, v0, r);
            } else {
                len = pow5.nWords + 2 + ((p2 != 0) ? 1 : 0);
                Mul(pow5.data, pow5.nWords, v0, v1, r);
            }
            return (FDBigInteger(r, pow5.offset, len)).LeftShift(p2);
        }
    } else if (p2 != 0) {
        if (bitCount == 0) {
            int temp[] = {v0, v1};
            return {temp, wordcount, 2};
        } else {
            int temp[] = {v0 << bitCount, (v1 << bitCount) | Unsigned32RightShift(v0, 32 - bitCount),
                          Unsigned32RightShift(v1, 32 - bitCount)};
            return {temp, wordcount, 3};
        }
    }
    int temp[] = {v0, v1};
    return {temp, 0, 2};
}

int FDBigInteger::MulAndCarryBy10(const int *src, int srcLen, int *dst)
{
    long carry = 0;
    for (int i = 0; i < srcLen; i++) {
        long product = (src[i] & LONG_MASK) * 10L + carry;
        dst[i] = static_cast<int>(product);
        carry = Unsigned64RightShift(product, 32);
    }
    return static_cast<int>(carry);
}

void FDBigInteger::Mul(const int *src, int srcLen, int v0, int v1, int *dst)
{
    long v = v0 & LONG_MASK;
    long carry = 0;
    for (int j = 0; j < srcLen; j++) {
        long product = v * (src[j] & LONG_MASK) + carry;
        dst[j] = static_cast<int>(product);
        carry = Unsigned64RightShift(product, 32);
    }
    dst[srcLen] = static_cast<int>(carry);
    v = v1 & LONG_MASK;
    carry = 0;
    for (int j = 0; j < srcLen; j++) {
        long product = (dst[j + 1] & LONG_MASK) + v * (src[j] & LONG_MASK) + carry;
        dst[j + 1] = static_cast<int>(product);
        carry = Unsigned64RightShift(product, 32);
    }
    dst[srcLen + 1] = static_cast<int>(carry);
}

void FDBigInteger::Mul(const int *s1, int s1Len, const int *s2, int s2Len, int *dst)
{
    for (int i = 0; i < s1Len; i++) {
        long v = s1[i] & LONG_MASK;
        long p = 0L;
        for (int j = 0; j < s2Len; j++) {
            p += (dst[i + j] & LONG_MASK) + v * (s2[j] & LONG_MASK);
            dst[i + j] = static_cast<int>(p);
            p = Unsigned64RightShift(p, 32);
        }
        dst[i + s2Len] = static_cast<int>(p);
    }
}

int FDBigInteger::CheckZeroTail(const int *a, int from)
{
    while (from > 0) {
        if (a[--from] != 0) {
            return 1;
        }
    }
    return 0;
}

void FDBigInteger::LeftShift(const int *src, int idx, int *result, int bitCount, int antiCount, int prev)
{
    for (; idx > 0; idx--) {
        int v = (prev << bitCount);
        prev = src[idx - 1];
        v |= Unsigned32RightShift(prev, antiCount);
        result[idx] = v;
    }
    int v = prev << bitCount;
    result[0] = v;
}

int FDBigInteger::GetNormalizationBias() const
{
    if (nWords == 0) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "Zero value cannot be normalized");
    }
    int zeros = NumberOfLeadingZeros(data[nWords - 1]);
    return (zeros < 4) ? 28 + zeros : zeros - 4;
}

int FDBigInteger::QuoRemIteration(FDBigInteger &s)
{
    // ensure that this and S have the same number of
    // digits. If S is properly normalized and q < 10 then
    // this must be so.
    int thSize = Size();
    int sSize = s.Size();
    if (thSize < sSize) {
        // this value is significantly less than S, result of division is zero.
        // just mul this by 10.
        int p = MulAndCarryBy10(data, nWords, data);
        if (p != 0) {
            data[nWords++] = p;
        } else {
            TrimLeadingZeros();
        }
        return 0;
    } else if (thSize > sSize) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "disparate values");
    }
    // estimate q the obvious way. We will usually be
    // right. If not, then we're only off by a little and
    // will re-add.
    long q = (data[nWords - 1] & LONG_MASK) / (s.data[s.nWords - 1] & LONG_MASK);
    long diff = MulDiffMe(q, s);
    if (diff != 0L) {
        //@ assert q != 0;
        //@ assert offset == \old(Math.min(offset, S.offset));
        //@ assert offset <= S.offset;

        // q is too big.
        // add S back in until this turns +. This should
        // not be very many times!
        long sum = 0L;
        int tStart = s.offset - offset;
        //@ assert tStart >= 0;
        int *sd = s.data;
        int *td = data;
        while (sum == 0L) {
            for (int sIndex = 0, tIndex = tStart; tIndex < nWords; sIndex++, tIndex++) {
                sum += (td[tIndex] & LONG_MASK) + (sd[sIndex] & LONG_MASK);
                td[tIndex] = static_cast<int>(sum);
                sum = Unsigned64RightShift(sum, 32); // Signed or unsigned, answer is 0 or 1
            }
            //
            // Originally the following line read
            // "if ( sum !=0 && sum != -1 )"
            // but that would be wrong, because of the
            // treatment of the two values as entirely unsigned,
            // it would be impossible for a carry-out to be interpreted
            // as -1 -- it would have to be a single-bit carry-out, or +1.
            //
            q -= 1;
        }
    }
    // finally, we can multiply this by 10.
    // it cannot overflow, right, as the high-order word has
    // at least 4 high-order zeros!
    MulAndCarryBy10(data, nWords, data);
    TrimLeadingZeros();
    return static_cast<int>(q);
}

FDBigInteger FDBigInteger::Add(const FDBigInteger &other)
{
    FDBigInteger big, small;
    int bigLen, smallLen;
    int tSize = Size();
    int oSize = other.Size();
    if (tSize >= oSize) {
        big = *this;
        bigLen = tSize;
        small = other;
        smallLen = oSize;
    } else {
        big = other;
        bigLen = oSize;
        small = *this;
        smallLen = tSize;
    }
    int r[MAX_DATA_LENGTH]{0};
    int i = 0;
    long carry = 0L;
    for (; i < smallLen; i++) {
        carry += (i < big.offset ? 0L : (big.data[i - big.offset] & LONG_MASK)) +
                 ((i < small.offset ? 0L : (small.data[i - small.offset] & LONG_MASK)));
        r[i] = static_cast<int>(carry);
        carry >>= 32; // signed shift.
    }
    for (; i < bigLen; i++) {
        carry += (i < big.offset ? 0L : (big.data[i - big.offset] & LONG_MASK));
        r[i] = static_cast<int>(carry);
        carry >>= 32; // signed shift.
    }
    r[bigLen] = static_cast<int>(carry);
    return {r, 0, bigLen + 1};
}

long FDBigInteger::MulDiffMe(long q, FDBigInteger &s)
{
    long diff = 0L;
    if (q != 0) {
        int deltaSize = s.offset - offset;
        if (deltaSize >= 0) {
            int *sd = s.data;
            int *td = data;
            for (int sIndex = 0, tIndex = deltaSize; sIndex < s.nWords; sIndex++, tIndex++) {
                diff += (td[tIndex] & LONG_MASK) - q * (sd[sIndex] & LONG_MASK);
                td[tIndex] = static_cast<int>(diff);
                diff >>= 32; // N.B. SIGNED shift.
            }
        } else {
            deltaSize = -deltaSize;
            int rd[MAX_DATA_LENGTH]{0};
            int sIndex = 0;
            int rIndex = 0;
            int *sd = s.data;
            for (; rIndex < deltaSize && sIndex < s.nWords; sIndex++, rIndex++) {
                diff -= q * (sd[sIndex] & LONG_MASK);
                rd[rIndex] = static_cast<int>(diff);
                diff >>= 32; // N.B. SIGNED shift.
            }
            int tIndex = 0;
            int *td = data;
            for (; sIndex < s.nWords; sIndex++, tIndex++, rIndex++) {
                diff += (td[tIndex] & LONG_MASK) - q * (sd[sIndex] & LONG_MASK);
                rd[rIndex] = static_cast<int>(diff);
                diff >>= 32; // N.B. SIGNED shift.
            }
            nWords += deltaSize;
            offset -= deltaSize;
            UpdateDataVector(rd);
        }
    }
    return diff;
}

FDBigInteger FDBigInteger::Mul(FDBigInteger other)
{
    if (nWords == 0) {
        return *this;
    }
    if (Size() == 1) {
        return other.Mul(data[0]);
    }
    if (other.nWords == 0) {
        return other;
    }
    if (other.Size() == 1) {
        return Mul(other.data[0]);
    }
    int r[MAX_DATA_LENGTH]{0};
    Mul(data, nWords, other.data, other.nWords, r);
    return {r, offset + other.offset, nWords + other.nWords};
}

void FDBigInteger::MulAddMe(int iv, int addend)
{
    long v = iv & LONG_MASK;
    // unroll 0th iteration, doing addition.
    long p = v * (data[0] & LONG_MASK) + (addend & LONG_MASK);
    data[0] = static_cast<int>(p);
    p = Unsigned64RightShift(p, 32);
    for (int i = 1; i < nWords; i++) {
        p += v * (data[i] & LONG_MASK);
        data[i] = static_cast<int>(p);
        p = Unsigned64RightShift(p, 32);
    }
    if (p != 0L) {
        data[nWords++] = static_cast<int>(p); // will fail noisily if illegal!
    }
}

void DoubleToString::DevelopLongDigits(int exponent, long leftValue, int insignificantDigits)
{
    if (insignificantDigits != 0) {
        // Discard non-significant low-order bits, while rounding,
        // up to insignificant value.
        long pow10 = FDBigInteger::LONG_5_POW[insignificantDigits] << insignificantDigits; // 10^i == 5^i * 2^i;
        long residue = leftValue % pow10;
        leftValue /= pow10;
        exponent += insignificantDigits;
        if (residue >= (pow10 >> 1)) {
            // round up based on the low-order bits we're discarding
            leftValue++;
        }
    }
    int digitNo = 20 - 1;
    int c;
    if (leftValue <= INT32_MAX) {
        // even easier subcase!
        // can do int arithmetic rather than long!
        int iValue = static_cast<int>(leftValue);
        c = iValue % 10;
        iValue /= 10;
        while (c == 0) {
            exponent++;
            c = iValue % 10;
            iValue /= 10;
        }
        while (iValue != 0) {
            digits[digitNo--] = static_cast<char>(c + '0');
            exponent++;
            c = iValue % 10;
            iValue /= 10;
        }
        digits[digitNo] = static_cast<char>(c + '0');
    } else {
        // same algorithm as above (same bugs, too )
        // but using long arithmetic.
        c = static_cast<int>(leftValue % 10L);
        leftValue /= 10L;
        while (c == 0) {
            exponent++;
            c = static_cast<char>(leftValue % 10L);
            leftValue /= 10L;
        }
        while (leftValue != 0L) {
            digits[digitNo--] = static_cast<char>(c + '0');
            exponent++;
            c = static_cast<char>(leftValue % 10L);
            leftValue /= 10;
        }
        digits[digitNo] = static_cast<char>(c + '0');
    }
    this->decExponent = exponent + 1;
    this->firstDigitIndex = digitNo;
    this->nDigits = 20 - digitNo;
}

void DoubleToString::Dtoa(int binExp, long fractBits, int nSignificantBits, bool isCompatibleFormat)
{
    // Examine number. Determine if it is an easy case,
    // which we can do pretty trivially using float/long conversion,
    // or whether we must do real work.
    int tailZeros = NumberOfTrailingZeros(fractBits);

    // number of significant bits of fractBits;
    int nFractBits = EXP_SHIFT + 1 - tailZeros;

    // number of significant bits to the right of the point.
    int nTinyBits = std::max(0, nFractBits - binExp - 1);
    if (binExp <= MAX_SMALL_BIN_EXP && binExp >= MIN_SMALL_BIN_EXP) {
        // Look more closely at the number to decide if,
        // with scaling by 10^nTinyBits, the result will fit in
        // a long.
        if ((nTinyBits < FDBigInteger::LONG_5_POW_SIZE) && ((nFractBits + N_5_BITS[nTinyBits]) < 64)) {
            //
            // We can do this:
            // take the fraction bits, which are normalized.
            // (1) nTinyBits == 0: Shift left or right appropriately to align the binary point
            //     at the extreme right, i.e.where a long int point is expected to be.
            //     The integer result is easily converted to a string.
            // (2) nTinyBits > 0: Shift right by EXP_SHIFT-n FractBits, which effectively converts to
            //     long and scales by 2^nTinyBits. Then multiply by 5^nTinyBits to complete the scaling.
            //     We know this won't overflow because we just counted the number of bits necessary in the result.
            //     The integer you get from this can then be converted to a string pretty easily.
            //
            if (nTinyBits == 0) {
                int insignificant;
                if (binExp > nSignificantBits) {
                    insignificant = InsignificantDigitsForPow2(binExp - nSignificantBits - 1);
                } else {
                    insignificant = 0;
                }
                if (binExp >= EXP_SHIFT) {
                    fractBits <<= (binExp - EXP_SHIFT);
                } else {
                    fractBits = Unsigned64RightShift(fractBits, EXP_SHIFT - binExp);
                }
                DevelopLongDigits(0, fractBits, insignificant);
                return;
            }
        }
    }
    //
    // This is the hard case. We are going to compute large positive integers B and S and integer decExp, s.t.
    //     d = ( B / S )// 10^decExp
    //     1 <= B / S < 10
    // Obvious choices are:
    //     decExp = floor( log10(d) )
    //     B = d// 2^nTinyBits// 10^max( 0, -decExp )
    //     S = 10^max( 0, decExp)// 2^nTinyBits
    // (noting that nTinyBits has already been forced to non-negative)
    // I am also going to compute a large positive integer
    //     M = (1/2^nSignificantBits)// 2^nTinyBits// 10^max( 0, -decExp )
    // i.e. M is (1/2) of the ULP of d, scaled like B.
    // When we iterate through dividing B/S and picking off the quotient bits,
    // we will know when to stop when the remainder
    // is <= M.
    //
    // We keep track of powers of 2 and powers of 5.
    int decExp = EstimateDecExp(fractBits, binExp);
    int d2, d5; // powers of 2 and powers of 5, respectively, in D
    int s2, s5; // powers of 2 and powers of 5, respectively, in S
    int b2, b5; // powers of 2 and powers of 5, respectively, in B

    d5 = std::max(0, -decExp);
    d2 = d5 + nTinyBits + binExp;

    s5 = std::max(0, decExp);
    s2 = s5 + nTinyBits;

    b5 = d5;
    b2 = d2 - nSignificantBits;

    // the long integer fractBits contains the (nFractBits) interesting bits from the
    // mantissa of d ( hidden 1 added if necessary) followed by (EXP_SHIFT+1-nFractBits) zeros.
    // In the interest of compactness, I will shift out those zeros before turning fractBits
    // into a FDBigInteger.
    // The resulting whole number will be
    //     d * 2^(nFractBits-1-binExp).
    fractBits = Unsigned64RightShift(fractBits, tailZeros);
    d2 -= nFractBits - 1;
    int common2factor = std::min(d2, s2);
    d2 -= common2factor;
    s2 -= common2factor;
    b2 -= common2factor;

    // HACK!! For exact powers of two, the next smallest number is only half as far away
    // as we think (because the meaning of ULP changes at power-of-two bounds)
    // for this reason, we hack M2. Hope this works.
    if (nFractBits == 1) {
        b2 -= 1;
    }

    if (b2 < 0) {
        // oops. since we cannot scale M down far enough, we must scale the other values up.
        d2 -= b2;
        s2 -= b2;
        b2 = 0;
    }

    // Construct, Scale, iterate. Some day, we'll write a stopping test that
    // takes account of the asymmetry of the spacing of floating-point numbers
    // below perfect powers of 2 26 Sept 96 is not that day. So we use a symmetric test.
    int nDigit = 0;
    bool low, high;
    long lowDigitDifference;
    int q;

    // Detect the special cases where all the numbers we are about to compute will fit in int or long integers.
    // In these cases, we will avoid doing FDBigInteger arithmetic. We use the same algorithms,
    // except that we "normalize" our FDBigIntegers before iterating. This is to make division easier,
    // as it makes our fist guess (quotient of high-order words) more accurate!
    //
    // Some day, we'll write a stopping test that takes account of the asymmetry of
    // the spacing of floating-point numbers below perfect powers of 2 26 Sept 96 is not that day.
    // So we use a symmetric test.
    //
    // binary digits needed to represent B, approx.
    int bBits = nFractBits + d2 + ((d5 < N_5_BITS_SIZE) ? N_5_BITS[d5] : (d5 * 3));

    // binary digits needed to represent 10*S, approx.
    int tensBits = s2 + 1 + (((s5 + 1) < N_5_BITS_SIZE) ? N_5_BITS[(s5 + 1)] : ((s5 + 1) * 3));
    if (bBits < 64 && tensBits < 64) {
        if (bBits < 32 && tensBits < 32) {
            // wa-hoo! They're all ints!
            int b = (static_cast<int>(fractBits) * FDBigInteger::SMALL_5_POW[d5]) << d2;
            int s = FDBigInteger::SMALL_5_POW[s5] << s2;
            int m = FDBigInteger::SMALL_5_POW[b5] << b2;
            int tens = s * 10;
            //
            // Unroll the first iteration. If our decExp estimate
            // was too high, our first quotient will be zero. In this
            // case, we discard it and decrement decExp.
            nDigit = 0;
            q = b / s;
            b = 10 * (b % s);
            m *= 10;
            low = (b < m);
            high = (b + m > tens);
            if ((q == 0) && !high) {
                // oops. Usually ignore leading zero.
                decExp--;
            } else {
                digits[nDigit++] = static_cast<char>('0' + q);
            }

            // HACK! Java spec sez that we always have at least one digit
            // after the . in either F- or E-form output.
            // Thus, we will need more than one digit if we're using E-form
            if (!isCompatibleFormat || decExp < -3 || decExp >= 8) {
                high = false;
                low = false;
            }
            while (!low && !high) {
                b = 10 * (b % s);
                q = b / s;
                bool overflow = __builtin_mul_overflow(m, 10, &m);
                if (!overflow) {
                    low = (b < m);
                    high = (b + m > tens);
                } else {
                    // hack -- m might overflow! in this case, it is certainly > b,
                    // which won't and b+m > tens, too, since that has overflowed either!
                    low = true;
                    high = true;
                }
                digits[nDigit++] = static_cast<char>('0' + q);
            }
            lowDigitDifference = (b << 1) - tens;
        } else {
            // still good! they're all longs!
            long b = (fractBits * FDBigInteger::LONG_5_POW[d5]) << d2;
            long s = FDBigInteger::LONG_5_POW[s5] << s2;
            long m = FDBigInteger::LONG_5_POW[b5] << b2;
            long tens = s * 10L;
            //
            // Unroll the first iteration. If our decExp estimate
            // was too high, our first quotient will be zero. In this
            // case, we discard it and decrement decExp.
            //
            nDigit = 0;
            q = static_cast<int>(b / s);
            b = 10L * (b % s);
            m *= 10L;
            low = (b < m);
            high = (b + m > tens);
            if ((q == 0) && !high) {
                // oops. Usually ignore leading zero.
                decExp--;
            } else {
                digits[nDigit++] = static_cast<char>('0' + q);
            }

            // HACK! Java spec sez that we always have at least one digit
            // after the . in either F- or E-form output.
            // Thus, we will need more than one digit if we're using E-form
            if (!isCompatibleFormat || decExp < -3 || decExp >= 8) {
                high = low = false;
            }
            while (!low && !high) {
                q = static_cast<int>(b / s);
                b = 10 * (b % s);
                bool overflow = __builtin_mul_overflow(m, 10, &m);
                if (!overflow) {
                    high = (b + m > tens);
                    low = (b < m);
                } else {
                    // m might overflow! in this case, it is certainly > b,
                    // which won't and b+m > tens, too, since that has overflowed either!
                    low = true;
                    high = true;
                }
                digits[nDigit++] = static_cast<char>('0' + q);
            }
            lowDigitDifference = (b << 1) - tens;
        }
    } else {
        // We really must do FDBigInteger arithmetic.
        // Fist, construct our FDBigInteger initial values.
        FDBigInteger sval = FDBigInteger::ValueOfPow52(s5, s2);
        int shiftBias = sval.GetNormalizationBias();
        sval = sval.LeftShift(shiftBias); // normalize so that division works better

        FDBigInteger bval = FDBigInteger::ValueOfMulPow52(fractBits, d5, d2 + shiftBias);
        FDBigInteger mval = FDBigInteger::ValueOfPow52(b5 + 1, b2 + shiftBias + 1);
        FDBigInteger tenSval = FDBigInteger::ValueOfPow52(s5 + 1, s2 + shiftBias + 1); // Sval.mul( 10 );

        // Unroll the first iteration. If our decExp estimate
        // was too high, our first quotient will be zero. In this
        // case, we discard it and decrement decExp.
        nDigit = 0;
        q = bval.QuoRemIteration(sval);
        low = (bval.Cmp(mval) < 0);
        high = tenSval.AddAndCmp(bval, mval) <= 0;

        if ((q == 0) && !high) {
            // oops. Usually ignore leading zero.
            decExp--;
        } else {
            digits[nDigit++] = static_cast<char>('0' + q);
        }

        // HACK! Java spec sez that we always have at least one digit
        // after the . in either F- or E-form output. Thus, we will need more than
        // one digit if we're using E-form
        if (!isCompatibleFormat || decExp < -3 || decExp >= 8) {
            high = low = false;
        }
        while (!low && !high) {
            q = bval.QuoRemIteration(sval);
            mval = mval.MulBy10(); // Mval = Mval.mul( 10 );
            low = (bval.Cmp(mval) < 0);
            high = tenSval.AddAndCmp(bval, mval) <= 0;
            digits[nDigit++] = static_cast<char>('0' + q);
        }
        if (high && low) {
            bval = bval.LeftShift(1);
            lowDigitDifference = bval.Cmp(tenSval);
        } else {
            lowDigitDifference = 0L; // this here only for flow analysis!
        }
    }
    decExponent = decExp + 1;
    firstDigitIndex = 0;
    nDigits = nDigit;

    // Last digit gets rounded based on stopping condition.
    if (high) {
        if (low) {
            if (lowDigitDifference == 0L) {
                // it's a tie! choose based on which digits we like.
                auto index = firstDigitIndex + nDigits - 1;
                if (index >= MAX_DIGIT_INDEX) {
                    throw std::runtime_error("digits index overflow! index value:" + std::to_string(index));
                }
                if ((digits[index] & 1) != 0) {
                    Roundup();
                }
            } else if (lowDigitDifference > 0) {
                Roundup();
            }
        } else {
            Roundup();
        }
    }
}

void DoubleToString::Roundup()
{
    int index = (firstDigitIndex + nDigits - 1);
    int q = digits[index];
    if (q == '9') {
        while (q == '9' && index > firstDigitIndex) {
            digits[index] = '0';
            q = digits[--index];
        }
        if (q == '9') {
            // carryout! High-order 1, rest 0s, larger exp.
            decExponent += 1;
            digits[firstDigitIndex] = '1';
            return;
        }
        // else fall through.
    }
    digits[index] = static_cast<char>(q + 1);
}

int DoubleToString::GetChars(char *result) const
{
    int index = 0;
    if (isNegative) {
        result[0] = '-';
        index = 1;
    }
    if (decExponent > 0 && decExponent < 8) {
        // print digits.digits.
        int charLength = std::min(nDigits, decExponent);
        memcpy_s(result + index, charLength, digits + firstDigitIndex, charLength);
        index += charLength;
        if (charLength < decExponent) {
            charLength = decExponent - charLength;
            std::fill(result + index, result + (index + charLength), '0');
            index += charLength;
            result[index++] = '.';
            result[index++] = '0';
        } else {
            result[index++] = '.';
            if (charLength < nDigits) {
                int t = nDigits - charLength;
                memcpy_s(result + index, t, digits + (firstDigitIndex + charLength), t);
                index += t;
            } else {
                result[index++] = '0';
            }
        }
    } else if (decExponent <= 0 && decExponent > -3) {
        result[index++] = '0';
        result[index++] = '.';
        if (decExponent != 0) {
            std::fill(result + index, result + (index - decExponent), '0');
            index -= decExponent;
        }
        memcpy_s(result + index, nDigits, digits + firstDigitIndex, nDigits);
        index += nDigits;
    } else {
        result[index++] = digits[firstDigitIndex];
        result[index++] = '.';
        if (nDigits > 1) {
            memcpy_s(result + index, nDigits - 1, digits + (firstDigitIndex + 1), nDigits - 1);
            index += nDigits - 1;
        } else {
            result[index++] = '0';
        }
        result[index++] = 'E';
        int e;
        if (decExponent <= 0) {
            result[index++] = '-';
            e = -decExponent + 1;
        } else {
            e = decExponent - 1;
        }
        // decExponent has 1, 2, or 3, digits
        if (e <= 9) {
            result[index++] = static_cast<char>(e + '0');
        } else if (e <= 99) {
            result[index++] = static_cast<char>(e / 10 + '0');
            result[index++] = static_cast<char>(e % 10 + '0');
        } else {
            result[index++] = static_cast<char>(e / 100 + '0');
            e %= 100;
            result[index++] = static_cast<char>(e / 10 + '0');
            result[index++] = static_cast<char>(e % 10 + '0');
        }
    }
    return index;
}

int DoubleToString::InsignificantDigitsForPow2(int p2)
{
    if (p2 > 1 && p2 < INSIGNIFICANT_DIGITS_NUMBER_SIZE) {
        return INSIGNIFICANT_DIGITS_NUMBER[p2];
    }
    return 0;
}

int DoubleToString::EstimateDecExp(long fractBits, int binExp)
{
    auto d2 = BitCast<double>(EXP_ONE | (fractBits & DoubleConsts::SIGNIF_BIT_MASK));
    double d = (d2 - 1.5) * 0.289529654 + 0.176091259 + static_cast<double>(binExp) * 0.301029995663981;
    long dBits = BitCast<long>(d); // can't be NaN here so use raw
    int exponent = static_cast<int>((dBits & DoubleConsts::EXP_BIT_MASK) >> EXP_SHIFT) - DoubleConsts::EXP_BIAS;
    bool isNeg = (dBits & DoubleConsts::SIGN_BIT_MASK) != 0; // discover sign
    if (exponent >= 0 && exponent < 52) {                    // hot path
        long mask = DoubleConsts::SIGNIF_BIT_MASK >> exponent;
        int r = static_cast<int>(((dBits & DoubleConsts::SIGNIF_BIT_MASK) | FRACT_HOB) >> (EXP_SHIFT - exponent));
        return isNeg ? (((mask & dBits) == 0L) ? -r : -r - 1) : r;
    } else if (exponent < 0) {
        return (((dBits & ~DoubleConsts::SIGN_BIT_MASK) == 0) ? 0 : ((isNeg) ? -1 : 0));
    } else {
        return static_cast<int>(d);
    }
}

std::size_t DoubleToString::DoubleToStringConverter(double d, char *result)
{
    long dBits = BitCast<long>(d);
    bool isNeg = (dBits & DoubleConsts::SIGN_BIT_MASK) != 0; // discover sign
    long fractBits = dBits & DoubleConsts::SIGNIF_BIT_MASK;
    int binExp = static_cast<int>((dBits & DoubleConsts::EXP_BIT_MASK) >> EXP_SHIFT);

    auto setValueAndGetSize = [&result](const std::string &inputString) -> std::size_t {
        auto size = inputString.size();
        memcpy_s(result, size, inputString.c_str(), size);
        return size;
    };
    // Discover obvious special cases of NaN and Infinity.
    if (binExp == static_cast<int>(DoubleConsts::EXP_BIT_MASK >> EXP_SHIFT)) {
        if (fractBits == 0L) {
            if (isNeg) {
                return setValueAndGetSize("-Infinity");
            }
            return setValueAndGetSize("Infinity");
        } else {
            return setValueAndGetSize("NaN");
        }
    }
    // Finish unpacking
    // Normalize denormalized numbers. Insert assumed high-order bit
    // for normalized numbers. Subtract exponent bias.
    int nSignificantBits;
    if (binExp == 0) {
        if (fractBits == 0L) {
            if (isNeg) {
                return setValueAndGetSize("-0.0");
            }
            return setValueAndGetSize("0.0");
        }
        int leadingZeros = NumberOfLeadingZeros(fractBits);
        int shift = leadingZeros - (63 - EXP_SHIFT);
        fractBits <<= shift;
        binExp = 1 - shift;
        // recall binExp is  - shift count.
        nSignificantBits = 64 - leadingZeros;
    } else {
        fractBits |= FRACT_HOB;
        nSignificantBits = EXP_SHIFT + 1;
    }
    binExp -= DoubleConsts::EXP_BIAS;
    DoubleToString buf = DoubleToString();
    buf.setSign(isNeg);
    // call the routine that actually does all the hard work.
    buf.Dtoa(binExp, fractBits, nSignificantBits, true);
    return buf.ToString(result);
}

// just for ut test
std::string DoubleToString::DoubleToStringConverter(double d)
{
    char result[MAX_DATA_LENGTH];
    auto length = DoubleToStringConverter(d, result);
    return std::string{result, length};
}
} // namespace omniruntime::codegen::function
