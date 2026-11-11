/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2026. All rights reserved.
 * Description: Murmur3 Hash function
 */
#include "util/compiler_util.h"
#include "type/decimal128_utils.h"
#include "murmur3_hash.h"

namespace omniruntime::codegen::function {
static const int COMBINE_HASH_VALUE = 31;
static const uint32_t MM3_C1 = 0xcc9e2d51;
static const uint32_t MM3_C2 = 0x1b873593;

static const uint32_t MM3_BITS_INT = 32;

static const uint32_t MIXK1_ROTATE_LEFT_NUM = 15;

static const uint32_t MIXH1_ROTATE_LEFT_NUM = 13;
static const uint32_t MIXH1_MULTIPLY_M = 5;
static const uint32_t MIXH1_ADD_N = 0xe6546b64;

static const uint32_t FMIX_RIGHT_SHIFT_M = 16;
static const uint32_t FMIX_RIGHT_SHIFT_N = 13;
static const uint32_t FMIX_MULTIPLY_M = 0x85ebca6b;
static const uint32_t FMIX_MULTIPLY_N = 0xc2b2ae35;

static const uint32_t HASH_LONG_RIGHT_SHIFT = 32;

static const uint32_t MM3_SIZE_BYTE = 1;
static const uint32_t MM3_SIZE_SHORT = 2;
static const uint32_t MM3_SIZE_INT = 4;
static const uint32_t MM3_SIZE_LONG = 8;

static const uint32_t MM3_INT_ONE = 1;

static const uint32_t REVERSE_SHIFT_M = 24;
static const uint32_t REVERSE_SHIFT_N = 8;
static const uint32_t REVERSE_AND_A = 0xff;
static const uint32_t REVERSE_AND_B = 0xff0000;
static const uint32_t REVERSE_AND_C = 0xff00;
static const uint32_t REVERSE_AND_D = 0xff000000;

uint32_t ALWAYS_INLINE RotateLeft(uint32_t i, uint32_t distance)
{
    return (i << distance) | (i >> (MM3_BITS_INT - distance));
}

uint32_t ALWAYS_INLINE MixK1(uint32_t k1)
{
    k1 *= MM3_C1;
    k1 = RotateLeft(k1, MIXK1_ROTATE_LEFT_NUM);
    k1 *= MM3_C2;
    return k1;
}

uint32_t ALWAYS_INLINE MixH1(uint32_t h1, uint32_t k1)
{
    h1 ^= k1;
    h1 = RotateLeft(h1, MIXH1_ROTATE_LEFT_NUM);
    h1 = h1 * MIXH1_MULTIPLY_M + MIXH1_ADD_N;
    return h1;
}

uint32_t ALWAYS_INLINE Fmix(uint32_t h1, uint32_t length)
{
    h1 ^= length;
    h1 ^= h1 >> FMIX_RIGHT_SHIFT_M;
    h1 *= FMIX_MULTIPLY_M;
    h1 ^= h1 >> FMIX_RIGHT_SHIFT_N;
    h1 *= FMIX_MULTIPLY_N;
    h1 ^= h1 >> FMIX_RIGHT_SHIFT_M;
    return h1;
}

bool ALWAYS_INLINE IsBigEndian()
{
    union {
        uint32_t m;
        char n;
    } uval = { 0 };
    uval.m = MM3_INT_ONE;
    if (uval.n == MM3_INT_ONE) {
        return false;
    } else {
        return true;
    }
}

uint32_t ALWAYS_INLINE ReverseBytes(uint32_t x)
{
    return ((x >> REVERSE_SHIFT_M) & REVERSE_AND_A) | ((x << REVERSE_SHIFT_N) & REVERSE_AND_B) |
        ((x >> REVERSE_SHIFT_N) & REVERSE_AND_C) | ((x << REVERSE_SHIFT_M) & REVERSE_AND_D);
}

uint32_t ALWAYS_INLINE HashBytesByInt(char *base, uint32_t lengthInBytes, uint32_t seed)
{
    uint32_t h1 = seed;
    for (uint i = 0; i < lengthInBytes; i += MM3_SIZE_INT) {
        uint32_t halfWord = *reinterpret_cast<uint32_t *>(base + i);
        if (IsBigEndian()) {
            halfWord = ReverseBytes(halfWord);
        }
        h1 = MixH1(h1, MixK1(halfWord));
    }
    return h1;
}

uint32_t HashShort(uint16_t input, uint32_t seed)
{
    uint32_t k1 = static_cast<uint32_t>(input);
    k1 = MixK1(k1);
    uint32_t h1 = MixH1(seed, k1);

    return Fmix(h1, MM3_SIZE_SHORT);
}

uint32_t HashByte(uint8_t input, uint32_t seed)
{
    uint32_t k1 = static_cast<uint32_t>(input);
    k1 = MixK1(k1);
    uint32_t h1 = MixH1(seed, k1);

    return Fmix(h1, MM3_SIZE_BYTE);
}

uint32_t HashInt(uint32_t input, uint32_t seed)
{
    uint32_t k1 = MixK1(input);
    uint32_t h1 = MixH1(seed, k1);

    return Fmix(h1, MM3_SIZE_INT);
}

uint32_t HashLong(uint64_t input, uint32_t seed)
{
    auto low = static_cast<uint32_t>(input);
    auto high = static_cast<uint32_t>(input >> HASH_LONG_RIGHT_SHIFT);

    uint32_t k1 = MixK1(low);
    uint32_t h1 = MixH1(seed, k1);

    k1 = MixK1(high);
    h1 = MixH1(h1, k1);

    return Fmix(h1, MM3_SIZE_LONG);
}

uint32_t HashUnsafeBytes(char *base, uint32_t lengthInBytes, uint32_t seed)
{
    uint32_t lengthAligned = lengthInBytes - lengthInBytes % MM3_SIZE_INT;
    uint32_t h1 = HashBytesByInt(base, lengthAligned, seed);
    for (uint i = lengthAligned; i < lengthInBytes; i++) {
        int32_t halfWord = static_cast<int32_t>(static_cast<int8_t>(base[i]));
        uint32_t k1 = MixK1(static_cast<uint32_t>(halfWord));
        h1 = MixH1(h1, k1);
    }
    return Fmix(h1, lengthInBytes);
}

extern "C" DLLEXPORT int32_t Mm3Int8(int8_t val, bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(
       HashInt(static_cast<uint32_t>(static_cast<int32_t>(val)), static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3Int16(int16_t val, bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(
       HashInt(static_cast<uint32_t>(static_cast<int32_t>(val)), static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3Int32(int32_t val, bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(HashInt(static_cast<uint32_t>(val * !isValNull), static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3Int64(int64_t val, bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(HashLong(static_cast<uint64_t>(val * !isValNull), static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3String1(std::string_view val, bool isValNull, int32_t seed, bool isSeedNull)
{
    std::string str(val);
    return Mm3String(str.data(), str.length(), isValNull, seed, isSeedNull);
}

extern "C" DLLEXPORT int32_t Mm3String(char *val, int32_t valLen, bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    valLen = valLen * !isValNull;
    return static_cast<int32_t>(HashUnsafeBytes(val, static_cast<uint32_t>(valLen), static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3Float(float val, bool isValNull, int32_t seed, bool isSeedNull)
{
    union {
        uint32_t lVal;
        float dVal;
    } uVal = { 0 };
    uVal.dVal = val * !isValNull;
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(HashInt(uVal.lVal, static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3Double(double val, bool isValNull, int32_t seed, bool isSeedNull)
{
    union {
        uint64_t lVal;
        double dVal;
    } uVal = { 0 };
    uVal.dVal = val * !isValNull;
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(HashLong(uVal.lVal, static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int32_t Mm3Decimal64(int64_t val, int32_t precision, int32_t scale, bool isValNull, int32_t seed,
    bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return static_cast<int32_t>(HashLong(val * !isValNull, seed));
}

extern "C" DLLEXPORT int32_t Mm3Decimal128(int64_t xHigh, uint64_t xLow, int32_t precision, int32_t scale,
    bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    int32_t byteLen = 0;
    auto bytes = omniruntime::type::Decimal128Utils::Decimal128ToBytes(xHigh, xLow, byteLen);
    auto result = static_cast<int32_t>(HashUnsafeBytes(reinterpret_cast<char *>(bytes), byteLen, seed));
    delete[] bytes;
    return result;
}

extern "C" DLLEXPORT int32_t Mm3Boolean(bool val, bool isValNull, int32_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    uint32_t intVal = val ? 1 : 0;
    return static_cast<int32_t>(HashInt(static_cast<uint32_t>(intVal * !isValNull), static_cast<uint32_t>(seed)));
}

extern "C" DLLEXPORT int64_t CombineHash(int64_t prevHashVal, bool isPrevHashValNull, int64_t val, bool isValNull)
{
    if (isPrevHashValNull) {
        prevHashVal = 0;
    }
    if (isValNull) {
        val = 0;
    }
    return COMBINE_HASH_VALUE * prevHashVal + val;
}
}