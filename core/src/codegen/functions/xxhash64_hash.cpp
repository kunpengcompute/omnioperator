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
 */

/*
 * Description: registry  function  implementation
 */

#include "xxhash64_hash.h"
#include "operator/hash_util.h"
#include "type/decimal128_utils.h"

using namespace std;
using namespace omniruntime::op;

namespace omniruntime::codegen::function {
static const int64_t PRIME64_5 = 0x27D4EB2F165667C5L;
static const int64_t SIZE_OF_INT = 4L;
static const int64_t SIZE_OF_LONG = 8L;

int64_t ALWAYS_INLINE HashInt(int32_t val, int64_t seed)
{
    auto hash = seed + PRIME64_5 + SIZE_OF_INT;
    hash = HashUtil::XxHash64UpdateTail(hash, val);
    hash = static_cast<int64_t>(HashUtil::XxHash64FinalShuffle(static_cast<uint64_t>(hash)));
    return hash;
}

int64_t ALWAYS_INLINE HashLong(int64_t val, int64_t seed)
{
    auto hash = seed + PRIME64_5 + SIZE_OF_LONG;
    hash = HashUtil::XxHash64UpdateTail(hash, val);
    hash = static_cast<int64_t>(HashUtil::XxHash64FinalShuffle(static_cast<uint64_t>(hash)));
    return hash;
}

extern "C" DLLEXPORT int64_t XxH64Int8(int8_t val, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    return isValNull ? seed : HashInt(static_cast<int32_t>(val), seed);
}

extern "C" DLLEXPORT int64_t XxH64Int16(int16_t val, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    return isValNull ? seed : HashInt(static_cast<int32_t>(val), seed);
}

extern "C" DLLEXPORT int64_t XxH64Int32(int32_t val, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    return isValNull ? seed : HashInt(val, seed);
}

extern "C" DLLEXPORT int64_t XxH64Int64(int64_t val, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    return isValNull ? seed : HashLong(val, seed);
}

extern "C" DLLEXPORT int64_t XxH64String(const char *val, int32_t valLen, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    auto data = reinterpret_cast<int8_t *>(const_cast<char *>(val));
    return HashUtil::XxHash64Hash(seed, data, 0, valLen);
}

extern "C" DLLEXPORT int64_t XxH64Double(double val, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    return HashLong(HashUtil::DoubleToLongBits(val), seed);
}

extern "C" DLLEXPORT int64_t XxH64Decimal64(int64_t val, int32_t precision, int32_t scale, bool isValNull, int64_t seed,
                                            bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    return isValNull ? seed : HashLong(val, seed);
}

extern "C" DLLEXPORT int64_t XxH64Decimal128(int64_t xHigh, uint64_t xLow, int32_t precision, int32_t scale,
                                             bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    int32_t byteLen = 0;
    auto bytes = omniruntime::type::Decimal128Utils::Decimal128ToBytes(xHigh, xLow, byteLen);
    auto result = op::HashUtil::XxHash64Hash(seed, bytes, 0, byteLen);
    delete[] bytes;
    bytes = nullptr;
    return result;
}

extern "C" DLLEXPORT int64_t XxH64Boolean(bool val, bool isValNull, int64_t seed, bool isSeedNull)
{
    if (isSeedNull) {
        seed = 0;
    }
    if (isValNull) {
        return seed;
    }

    int32_t intVal = val ? 1 : 0;
    return HashInt(intVal, seed);
}
} // namespace omniruntime::codegen::function
