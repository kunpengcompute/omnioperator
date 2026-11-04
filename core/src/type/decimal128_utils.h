/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: decimal128 utils
 */

#ifndef OMNI_RUNTIME_DECIMAL128_UTILS_H
#define OMNI_RUNTIME_DECIMAL128_UTILS_H
#include "operator/hash_util.h"
#include "data_utils.h"

namespace omniruntime {
namespace type {
class Decimal128Utils {
public:
    static inline __uint128_t StrToUint128_t(const char *s)
    {
        const char *p = s;
        __uint128_t val = 0;

        if (*p == '-' || *p == '+') {
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            val = (10 * val) + (*p - '0');
            p++;
        }
        if (*s == '-') {
            val = -val;
        }
        return val;
    }

    static inline std::string Uint128_tToStr(__uint128_t num)
    {
        std::string str;
        do {
            int digit = num % 10;
            str = std::to_string(digit) + str;
            num = (num - digit) / 10;
        } while (num != 0);
        return str;
    }

    // Emulate java.math.BigInteger.toByteArray() (minimal two's-complement), same as
    // omni_commercial_1230. The old code fed raw two's-complement bits into the magnitude
    // path (sign bit stripped, 0xFF padding kept), giving 17 bytes for -1 and wrong
    // negative hashes. Steps: |value| -> byteLen = bitLength()/8 + 1 (negative powers of
    // two: one bit less, -128 -> 1 byte) -> big-endian bytes -> negate for negatives.
    static int8_t *Decimal128ToBytes(int64_t highBits, uint64_t lowBits, int32_t &byteLen)
    {
        const __uint128_t value =
            (static_cast<__uint128_t>(static_cast<uint64_t>(highBits)) << 64) | static_cast<__uint128_t>(lowBits);
        const bool isNegative = (static_cast<uint64_t>(highBits) >> 63) != 0;
        __uint128_t mag = isNegative ? (static_cast<__uint128_t>(0) - value) : value;

        int32_t magBitLength = 0;
        for (int32_t b = 127; b >= 0; --b) {
            if (((mag >> b) & 1) != 0) {
                magBitLength = b + 1;
                break;
            }
        }
        if (magBitLength == 0) {
            byteLen = 1;
            auto *bytes = new int8_t[1];
            bytes[0] = 0;
            return bytes;
        }

        int32_t bitLength = magBitLength;
        if (isNegative && (mag & (mag - 1)) == 0) {
            bitLength = magBitLength - 1;
        }
        byteLen = bitLength / 8 + 1;
        auto *bytes = new int8_t[byteLen];
        for (int32_t i = byteLen - 1; i >= 0; --i) {
            bytes[i] = static_cast<int8_t>(mag & 0xFF);
            mag >>= 8;
        }
        if (isNegative) {
            int32_t carry = 1;
            for (int32_t i = byteLen - 1; i >= 0; --i) {
                const int32_t v = static_cast<int32_t>(static_cast<uint8_t>(~bytes[i])) + carry;
                bytes[i] = static_cast<int8_t>(v & 0xFF);
                carry = v >> 8;
            }
        }
        return bytes;
    }
};
}
}

#endif // OMNI_RUNTIME_DECIMAL128_UTILS_H
