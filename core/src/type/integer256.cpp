/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * @Description: Integer256 implementations
 */

#include "integer256.h"

namespace omniruntime::type {
void Integer256::Multiply(const Integer256 &x, const Integer256 &y, Integer256 &result)
{
    std::size_t xSize = x.size;
    std::size_t ySize = y.size;
    const uint64_t *px = x.data;
    const uint64_t *py = y.data;
    if (xSize == 1) {
        bool s = y.sign != x.sign;
        if (ySize == 1) {
            result = static_cast<uint128_t>(*px) * static_cast<uint128_t>(*py);
        } else {
            uint64_t l = *px;
            Multiply(y, l, result);
        }
        result.sign = s;
        return;
    }
    if (ySize == 1) {
        bool s = y.sign != x.sign;
        uint64_t l = *py;
        Multiply(x, l, result);
        result.sign = s;
        return;
    }

    if ((void *) &result == (void *) &x) {
        Integer256 t(x);
        Multiply(t, y, result);
        return;
    }
    if ((void *) &result == (void *) &y) {
        Integer256 t(y);
        Multiply(x, t, result);
        return;
    }

    result.size = x.size + y.size;
    uint64_t *pr = result.data;
    uint128_t carry = 0;
    for (std::size_t i = 0; i < xSize; ++i) {
        std::size_t j = 0;
        for (; j < ySize; ++j) {
            carry += static_cast<uint128_t>(px[i]) * static_cast<uint128_t>(py[j]);
            carry += pr[i + j];
            pr[i + j] = static_cast<uint64_t>(carry);
            carry >>= BITS_64;
        }
        if (carry) {
            if (result.size < i + j + 1) {
                result.size = i + j + 1;
            }
            pr[i + j] = static_cast<uint64_t>(carry);
        }
        carry = 0;
    }
    result.Normalize();
    result.sign = x.sign != y.sign;
}

void Integer256::DivideUnsigned(const Integer256 &x, const Integer256 &y, Integer256 &result, Integer256 &r)
{
    std::size_t xOrder = x.size - 1;
    std::size_t yOrder = y.size - 1;
    const uint64_t *px = x.data;
    const uint64_t *py = y.data;
    r = x;
    r.sign = false;
    Integer256 t;
    bool rNeg = false;

    if (xOrder <= yOrder) {
        if ((xOrder < yOrder) || (r.CompareUnsigned(y) < 0)) {
            return;
        }
    }
    if (xOrder == 0) {
        result = px[0] / py[0];
        r = px[0] % py[0];
        return;
    } else if (xOrder == 1) {
        uint128_t a = (static_cast<uint128_t>(px[1]) << BITS_64) | px[0];
        uint128_t b = yOrder ? (static_cast<uint128_t>(py[1]) << BITS_64) | py[0] : py[0];
        result = a / b;
        r = a % b;
        return;
    }
    result.size = 1 + xOrder - yOrder;
    const uint64_t *prem = r.data;
    uint64_t *pr = nullptr;
    pr = result.data;
    for (std::size_t i = 1; i < 1 + xOrder - yOrder; ++i) {
        pr[i] = 0;
    }

    bool firstPass = true;
    do {
        uint64_t guess = 1;
        if ((prem[xOrder] <= py[yOrder]) && (xOrder > 0)) {
            uint128_t a = (static_cast<uint128_t>(prem[xOrder]) << BITS_64) | prem[xOrder - 1];
            uint128_t b = py[yOrder];
            uint128_t v = a / b;
            if (v <= UINT64_MAX) {
                guess = static_cast<uint64_t>(v);
                --xOrder;
            }
        } else if (xOrder == 0) {
            guess = prem[0] / py[yOrder];
        } else {
            uint128_t a = (static_cast<uint128_t>(prem[xOrder]) << BITS_64) | prem[xOrder - 1];
            uint128_t b = (yOrder > 0) ? (static_cast<uint128_t>(py[yOrder]) << BITS_64) | py[yOrder - 1] : (
                static_cast<uint128_t>(py[yOrder]) << BITS_64);
            uint128_t v = a / b;
            guess = static_cast<uint64_t>(v);
        }
        std::size_t shift = xOrder - yOrder;
        if (rNeg) {
            if (pr[shift] > guess) {
                pr[shift] -= guess;
            } else {
                t.size = shift + 1;
                t.data[shift] = guess;
                for (std::size_t i = 0; i < shift; ++i) {
                    t.data[i] = 0;
                }
                Subtract(result, t);
            }
        } else if (UINT64_MAX - pr[shift] > guess) {
            pr[shift] += guess;
        } else {
            t.size = shift + 1;
            t.data[shift] = guess;
            for (std::size_t i = 0; i < shift; ++i) {
                t.data[i] = 0;
            }
            Add(result, t);
        }

        uint128_t carry = 0;
        t.size = y.size + shift + 1;
        bool truncated = (t.size != y.size + shift + 1);
        uint64_t *pt = t.data;
        for (std::size_t i = 0; i < shift; ++i) {
            pt[i] = 0;
        }
        for (std::size_t i = 0; i < y.size; ++i) {
            carry += static_cast<uint128_t>(py[i]) * static_cast<uint128_t>(guess);
            pt[i + shift] = static_cast<uint64_t>(carry);
            carry >>= BITS_64;
        }
        if (carry && !truncated) {
            pt[t.size - 1] = static_cast<uint64_t>(carry);
        } else if (!truncated) {
            t.size = t.size - 1;
        }
        if (truncated && carry) {
            for (std::size_t i = 0; i <= xOrder; ++i) {
                r.data[i] = ~prem[i];
            }
            Increment(r);
            r.Normalize();
            Add(r, t);
            rNeg = !rNeg;
        } else if (r.Compare(t) > 0) {
            Subtract(r, t);
        } else {
            r.Swap(t);
            Subtract(r, t);
            prem = r.data;
            rNeg = !rNeg;
        }
        if (firstPass) {
            firstPass = false;
            while (pr[result.size - 1] == 0) {
                result.size = result.size - 1;
            }
        }
        xOrder = r.size - 1;
        if (xOrder < yOrder) {
            break;
        }
    } while ((xOrder > yOrder) || (r.CompareUnsigned(y) >= 0));

    if (rNeg && ComputeSign(r)) {
        Decrement(result);
        if (y.sign) {
            r.Negate();
            Subtract(r, y);
        } else {
            Subtract(y, r, r);
        }
    }
}

Integer256 operator+(const Integer256 &x, const Integer256 &y)
{
    Integer256 r(0);
    Integer256::Add(x, y, r);
    return r;
}

Integer256 operator-(const Integer256 &x, const Integer256 &y)
{
    Integer256 r(0);
    Integer256::Subtract(x, y, r);
    return r;
}

Integer256 operator*(const Integer256 &x, const Integer256 &y)
{
    Integer256 r(0);
    Integer256::Multiply(x, y, r);
    return r;
}

Integer256 operator/(const Integer256 &x, const Integer256 &y)
{
    Integer256 r(0);
    Integer256::Divide(x, y, r);
    return r;
}

Integer256 operator%(const Integer256 &x, const Integer256 &y)
{
    Integer256 r(0);
    Integer256::Modulus(x, y, r);
    return r;
}

bool operator>(const Integer256 &x, const Integer256 &y)
{
    return x.Compare(y) == 1;
}

bool operator<(const Integer256 &x, const Integer256 &y)
{
    return x.Compare(y) == -1;
}

bool operator==(const Integer256 &x, const Integer256 &y)
{
    return x.Compare(y) == 0;
}

bool operator>=(const Integer256 &x, const Integer256 &y)
{
    return x.Compare(y) == 0 || x.Compare(y) == 1;
}

bool operator<=(const Integer256 &x, const Integer256 &y)
{
    return x.Compare(y) == 0 || x.Compare(y) == -1;
}
}