/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2026. All rights reserved.
 * @Description: hash util implementations
 */
#ifndef __HASH_UTIL_H__
#define __HASH_UTIL_H__

#include <cstdint>
#include "util/compiler_util.h"
#include "type/data_utils.h"
#include "codegen/functions/murmur3_hash.h"
#include "codegen/functions/mathfunctions.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"
#include "vector/mixed_vector.h"

namespace omniruntime {
namespace op {
constexpr uint32_t ROTATE_DISTANCE_1 = 1;
constexpr uint32_t ROTATE_DISTANCE_2 = 2;
constexpr uint32_t ROTATE_DISTANCE_4 = 4;
constexpr uint32_t ROTATE_DISTANCE_7 = 7;
constexpr uint32_t ROTATE_DISTANCE_8 = 8;
constexpr uint32_t ROTATE_DISTANCE_11 = 11;
constexpr uint32_t ROTATE_DISTANCE_12 = 12;
constexpr uint32_t ROTATE_DISTANCE_16 = 16;
constexpr uint32_t ROTATE_DISTANCE_18 = 18;
constexpr uint32_t ROTATE_DISTANCE_23 = 23;
constexpr uint32_t ROTATE_DISTANCE_27 = 27;
constexpr uint32_t ROTATE_DISTANCE_29 = 29;
constexpr uint32_t ROTATE_DISTANCE_31 = 31;
constexpr uint32_t ROTATE_DISTANCE_32 = 32;
constexpr uint32_t ROTATE_DISTANCE_33 = 33;
constexpr uint32_t ROTATE_DISTANCE_48 = 48;
constexpr uint32_t MAX_ROTATE_DISTANCE = 64;
constexpr int64_t DEFAULT_SEED = 0;
constexpr int32_t SIZE_OF_LONG = 8;
constexpr int64_t SIGN_LONG_MASK = 1L << 63;
constexpr int64_t HASH_OF_TRUE = 1231;
constexpr int64_t HASH_OF_FALSE = 1237;

constexpr uint64_t PRIME64_1 = 0x9E3779B185EBCA87L;
constexpr uint64_t PRIME64_2 = 0xC2B2AE3D27D4EB4FL;
constexpr int64_t PRIME64_3 = 0x165667B19E3779F9L;
constexpr uint64_t PRIME64_4 = 0x85EBCA77C2b2AE63L;
constexpr int64_t PRIME64_5 = 0x27D4EB2F165667C5L;
constexpr int32_t MAX_ARRAY_SIZE = 1073741824;
constexpr int32_t UPDATE_BODY_LENGTH = 32;
constexpr int32_t UINT8_STEP_4 = 4;
constexpr int32_t UINT8_STEP_8 = 8;
constexpr int32_t UINT8_STEP_32 = 32;
constexpr int32_t PTR_STEP_1 = 1;
constexpr int32_t PTR_STEP_2 = 2;
constexpr int32_t PTR_STEP_3 = 3;
constexpr int32_t PTR_STEP_4 = 4;
constexpr uint32_t MM3HASH_SEED = 42;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    constexpr bool IS_BIG_ENDIAN = true;
#else
    constexpr bool IS_BIG_ENDIAN = false;
#endif


class HashUtil {
public:
    static std::unique_ptr<omniruntime::vec::Vector<int32_t>> ComputePartitionIds(
        std::vector<omniruntime::vec::BaseVector *> &vecs, int32_t partitionNum, int32_t rowCount);

    static std::unique_ptr<omniruntime::vec::Vector<int32_t>> ComputePartitionIdsMixed(
            omniruntime::vec::MixedVectorBatch &mixedBatch, int32_t partitionNum, int32_t rowCount);

    static uint64_t NextPowerOfTwo(uint64_t x);

    static uint32_t HashArraySize(uint32_t expected, float f);

    static ALWAYS_INLINE int64_t HashValue(int32_t value)
    {
        return RotateLeft(value * PRIME64_2, ROTATE_DISTANCE_31) * PRIME64_1;
    }

    static ALWAYS_INLINE int64_t HashValue(int64_t value)
    {
        return RotateLeft(value * PRIME64_2, ROTATE_DISTANCE_31) * PRIME64_1;
    }

    static ALWAYS_INLINE int64_t HashValue(double value)
    {
        return HashValue(DoubleToLongBits(value));
    }

    static ALWAYS_INLINE int64_t HashValue(bool value)
    {
        return value ? HASH_OF_TRUE : HASH_OF_FALSE;
    }

    static ALWAYS_INLINE int64_t HashDecimal64Value(int64_t value)
    {
        return value;
    }

    static ALWAYS_INLINE int64_t HashValue(int64_t low, int64_t high)
    {
        return static_cast<uint64_t>(XxHash64HashValue(low)) ^
            static_cast<uint64_t>(XxHash64HashValue(UnpackUnsignedLong(high)));
    }

    static ALWAYS_INLINE int64_t HashValue(int8_t *value, int32_t length)
    {
        return XxHash64Hash(DEFAULT_SEED, value, 0, length);
    }

    static ALWAYS_INLINE int64_t XxHash64HashValue(int64_t value)
    {
        int64_t hash = DEFAULT_SEED + PRIME64_5 + SIZE_OF_LONG;
        hash = XxHash64UpdateTail(hash, value);
        hash = XxHash64FinalShuffle(hash);
        return hash;
    }

    ALWAYS_INLINE uint64_t operator () (uint64_t combinedHash) const
    {
        return combinedHash;
    }

    static ALWAYS_INLINE int64_t CombineHash(int64_t previousHashValue, int64_t value)
    {
        return (ROTATE_DISTANCE_31 * previousHashValue + value);
    }

    /*
     * it is used to get position for rawHash when reading or writing join hash vecBatch
     */
    static ALWAYS_INLINE uint32_t GetRawHashPosition(int64_t rawHash, uint64_t mask)
    {
        uint64_t hash = static_cast<uint64_t>(rawHash);
        hash ^= hash >> ROTATE_DISTANCE_33;
        hash *= 0xff51afd7ed558ccdUL;
        hash ^= hash >> ROTATE_DISTANCE_33;
        hash *= 0xc4ceb9fe1a85ec53UL;
        hash ^= hash >> ROTATE_DISTANCE_33;

        return static_cast<uint32_t>(hash & mask);
    }

    /*
     * it is used to get partition for rawHash when getting partition for probe of join and local exchange
     */
    static ALWAYS_INLINE uint32_t GetRawHashPartition(int64_t rawHash, uint32_t mask)
    {
        int64_t value = Reverse(rawHash);
        int64_t hash = DEFAULT_SEED + PRIME64_5 + SIZE_OF_LONG;
        hash = XxHash64UpdateTail(hash, value);
        hash = XxHash64FinalShuffle(hash);

        return static_cast<uint32_t>(hash) & mask;
    }

    static ALWAYS_INLINE int64_t RotateLeft(uint64_t i, uint32_t distance)
    {
        return (i << distance) | (i >> (MAX_ROTATE_DISTANCE - distance));
    }

    static ALWAYS_INLINE int64_t XxHash64Mix(int64_t current, int64_t value)
    {
        return HashUtil::RotateLeft(static_cast<uint64_t>(current + value * PRIME64_2), ROTATE_DISTANCE_31) * PRIME64_1;
    }

    static ALWAYS_INLINE int64_t XxHash64UpdateTail(int64_t hash, int64_t value)
    {
        uint64_t temp = static_cast<uint64_t>(hash) ^ static_cast<uint64_t>(XxHash64Mix(0, value));
        return HashUtil::RotateLeft(temp, ROTATE_DISTANCE_27) * PRIME64_1 + PRIME64_4;
    }

    static ALWAYS_INLINE int64_t XxHash64UpdateTail(int64_t hash, int8_t value)
    {
        uint32_t unsignedValue = static_cast<uint32_t>(value) & static_cast<uint32_t>(0xFF);
        uint64_t temp = static_cast<uint64_t>(hash) ^ static_cast<uint64_t>(unsignedValue * PRIME64_5);
        return RotateLeft(temp, ROTATE_DISTANCE_11) * PRIME64_1;
    }

    static ALWAYS_INLINE int64_t XxHash64UpdateTail(int64_t hash, int32_t value)
    {
        uint64_t unsignedValue = static_cast<uint64_t>(value) & static_cast<uint64_t>(0xFFFFFFFFL);
        uint64_t temp = static_cast<uint64_t>(hash) ^ (unsignedValue * PRIME64_1);
        return HashUtil::RotateLeft(temp, ROTATE_DISTANCE_23) * PRIME64_2 + PRIME64_3;
    }

    static ALWAYS_INLINE int64_t XxHash64FinalShuffle(uint64_t hash)
    {
        hash ^= hash >> ROTATE_DISTANCE_33;
        hash *= PRIME64_2;
        hash ^= hash >> ROTATE_DISTANCE_29;
        hash *= PRIME64_3;
        hash ^= hash >> ROTATE_DISTANCE_32;
        return hash;
    }

    static ALWAYS_INLINE int64_t XxHash64UpdateTail(int64_t hash, int8_t *address, int index, int length)
    {
        auto ptr64 = reinterpret_cast<int64_t *>(address + index);
        while (index <= length - UINT8_STEP_8) {
            hash = XxHash64UpdateTail(hash, *ptr64);
            index += UINT8_STEP_8;
            ptr64++;
        }

        auto ptr32 = reinterpret_cast<int32_t *>(address + index);
        if (index <= length - UINT8_STEP_4) {
            hash = XxHash64UpdateTail(hash, *ptr32);
            index += UINT8_STEP_4;
        }

        while (index < length) {
            hash = XxHash64UpdateTail(hash, *(address + index));
            index++;
        }
        hash = XxHash64FinalShuffle(hash);
        return hash;
    }

    static ALWAYS_INLINE int64_t XxHash64Update(int64_t hash, int64_t value)
    {
        uint64_t temp = static_cast<uint64_t>(hash) ^ static_cast<uint64_t>(XxHash64Mix(0, value));
        return temp * PRIME64_1 + PRIME64_4;
    }

    static int64_t XxHash64UpdateBody(int64_t seed, int8_t *address, int32_t length)
    {
        int64_t v1 = seed + PRIME64_1 + PRIME64_2;
        int64_t v2 = seed + PRIME64_2;
        int64_t v3 = seed;
        int64_t v4 = seed - PRIME64_1;

        int32_t remaining = length;
        auto ptr = reinterpret_cast<int64_t *>(address);
        while (remaining >= UINT8_STEP_32) {
            v1 = XxHash64Mix(v1, *(ptr));
            v2 = XxHash64Mix(v2, *(ptr + PTR_STEP_1));
            v3 = XxHash64Mix(v3, *(ptr + PTR_STEP_2));
            v4 = XxHash64Mix(v4, *(ptr + PTR_STEP_3));

            ptr = ptr + PTR_STEP_4;
            remaining = remaining - UINT8_STEP_32;
        }

        int64_t hash = HashUtil::RotateLeft(v1, ROTATE_DISTANCE_1) + HashUtil::RotateLeft(v2, ROTATE_DISTANCE_7) +
            HashUtil::RotateLeft(v3, ROTATE_DISTANCE_12) + HashUtil::RotateLeft(v4, ROTATE_DISTANCE_18);

        hash = XxHash64Update(hash, v1);
        hash = XxHash64Update(hash, v2);
        hash = XxHash64Update(hash, v3);
        hash = XxHash64Update(hash, v4);

        return hash;
    }

    static ALWAYS_INLINE int64_t XxHash64Hash(int64_t seed, int8_t *data, int32_t offset, int32_t length)
    {
        int8_t *address = data + offset;
        int64_t hash = 0;
        if (length >= UPDATE_BODY_LENGTH) {
            hash = XxHash64UpdateBody(seed, address, length);
        } else {
            hash = seed + PRIME64_5;
        }
        hash += length;

        int32_t index = static_cast<uint32_t>(length) & static_cast<uint32_t>(0xFFFFFFE0);
        return XxHash64UpdateTail(hash, address, index, length);
    }

    static ALWAYS_INLINE int32_t FloatToIntBits(float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(value));
        constexpr uint32_t EXP_MASK = 0x7F800000U;
        constexpr uint32_t MANTISSA_MASK = 0x007FFFFFU;
        constexpr uint32_t CANONICAL_NAN = 0x7FC00000U;
        if ((bits & EXP_MASK) == EXP_MASK && (bits & MANTISSA_MASK) != 0U) {
            bits = CANONICAL_NAN;
        }
        return static_cast<int32_t>(bits);
    }

    static ALWAYS_INLINE int64_t DoubleToLongBits(double value)
    {
        uint64_t bits;
        memcpy(&bits, &value, sizeof(value));
        if ((bits & 9218868437227405312UL) == 9218868437227405312UL && (bits & 4503599627370495UL) != 0UL) {
            bits = 9221120237041090560L;
        }

        return bits;
    }

    static ALWAYS_INLINE int64_t UnpackUnsignedLong(int64_t value)
    {
        return static_cast<uint64_t>(value) & static_cast<uint64_t>(~omniruntime::op::SIGN_LONG_MASK);
    }

    static ALWAYS_INLINE int64_t Reverse(uint64_t i)
    {
        i = (i & 0x5555555555555555UL) << ROTATE_DISTANCE_1 | ((i >> ROTATE_DISTANCE_1) & 0x5555555555555555UL);
        i = (i & 0x3333333333333333UL) << ROTATE_DISTANCE_2 | ((i >> ROTATE_DISTANCE_2) & 0x3333333333333333UL);
        i = (i & 0x0f0f0f0f0f0f0f0fUL) << ROTATE_DISTANCE_4 | ((i >> ROTATE_DISTANCE_4) & 0x0f0f0f0f0f0f0f0fUL);
        i = (i & 0x00ff00ff00ff00ffUL) << ROTATE_DISTANCE_8 | ((i >> ROTATE_DISTANCE_8) & 0x00ff00ff00ff00ffUL);
        i = (i << ROTATE_DISTANCE_48) | ((i & 0xffff0000UL) << ROTATE_DISTANCE_16) |
            ((i >> ROTATE_DISTANCE_16) & 0xffff0000UL) | (i >> ROTATE_DISTANCE_48);
        return i;
    }
};
}
}

#endif
