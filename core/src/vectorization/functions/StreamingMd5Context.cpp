/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Reusable streaming MD5 context for vectorized expressions
 *
 * The transformation logic is adapted from Omni's codegen/functions/md5.cpp,
 * which originates from the public-domain Colin Plumb MD5 implementation.
 */

#include "vectorization/functions/StreamingMd5Context.h"

#include <algorithm>
#include <cstring>

namespace omniruntime::vectorization {
namespace {

using RoundFunction = uint32_t (*)(uint32_t, uint32_t, uint32_t);

inline uint32_t Round1(uint32_t x, uint32_t y, uint32_t z)
{
    return z ^ (x & (y ^ z));
}

inline uint32_t Round2(uint32_t x, uint32_t y, uint32_t z)
{
    return Round1(z, x, y);
}

inline uint32_t Round3(uint32_t x, uint32_t y, uint32_t z)
{
    return x ^ y ^ z;
}

inline uint32_t Round4(uint32_t x, uint32_t y, uint32_t z)
{
    return y ^ (x | ~z);
}

inline void ApplyRound(RoundFunction function, uint32_t &word, uint32_t x, uint32_t y, uint32_t z,
    uint32_t data, uint32_t shift)
{
    word += function(x, y, z) + data;
    word = (word << shift) | (word >> (32U - shift));
    word += x;
}

inline uint32_t LoadLittleEndian32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8U) |
        (static_cast<uint32_t>(data[2]) << 16U) |
        (static_cast<uint32_t>(data[3]) << 24U);
}

} // namespace

StreamingMd5Context::StreamingMd5Context()
{
    Reset();
}

void StreamingMd5Context::Reset()
{
    state_[0] = 0x67452301U;
    state_[1] = 0xefcdab89U;
    state_[2] = 0x98badcfeU;
    state_[3] = 0x10325476U;
    totalBytes_ = 0;
    bufferedBytes_ = 0;
    std::memset(buffer_, 0, sizeof(buffer_));
}

void StreamingMd5Context::Update(const char *data, size_t size)
{
    if (size == 0) {
        return;
    }

    totalBytes_ += size;
    const auto *current = reinterpret_cast<const uint8_t *>(data);
    if (bufferedBytes_ != 0) {
        const size_t bytesToCopy = std::min(size, sizeof(buffer_) - bufferedBytes_);
        std::memcpy(buffer_ + bufferedBytes_, current, bytesToCopy);
        bufferedBytes_ += bytesToCopy;
        current += bytesToCopy;
        size -= bytesToCopy;
        if (bufferedBytes_ == sizeof(buffer_)) {
            Transform(buffer_);
            bufferedBytes_ = 0;
        }
    }

    while (size >= sizeof(buffer_)) {
        Transform(current);
        current += sizeof(buffer_);
        size -= sizeof(buffer_);
    }

    if (size != 0) {
        std::memcpy(buffer_, current, size);
        bufferedBytes_ = size;
    }
}

void StreamingMd5Context::Finish(uint8_t output[BINARY_DIGEST_SIZE])
{
    const uint64_t bitLength = totalBytes_ * 8U;
    uint8_t padding[64] {0x80U};
    const size_t paddingSize = bufferedBytes_ < 56 ? 56 - bufferedBytes_ : 120 - bufferedBytes_;
    Update(reinterpret_cast<const char *>(padding), paddingSize);

    uint8_t encodedLength[8];
    for (uint32_t i = 0; i < 8; ++i) {
        encodedLength[i] = static_cast<uint8_t>((bitLength >> (i * 8U)) & 0xffU);
    }
    Update(reinterpret_cast<const char *>(encodedLength), sizeof(encodedLength));

    for (size_t i = 0; i < BINARY_DIGEST_SIZE; ++i) {
        output[i] = static_cast<uint8_t>((state_[i / 4] >> ((i % 4) * 8U)) & 0xffU);
    }
}

void StreamingMd5Context::FinishHex(char output[HEX_DIGEST_SIZE])
{
    uint8_t digest[BINARY_DIGEST_SIZE];
    Finish(digest);
    DigestToHex(digest, output);
}

void StreamingMd5Context::DigestToHex(
    const uint8_t digest[BINARY_DIGEST_SIZE], char output[HEX_DIGEST_SIZE])
{
    static constexpr char HEX_CODES[] = "0123456789abcdef";
    for (size_t i = 0; i < BINARY_DIGEST_SIZE; ++i) {
        output[i * 2] = HEX_CODES[(digest[i] >> 4U) & 0x0fU];
        output[i * 2 + 1] = HEX_CODES[digest[i] & 0x0fU];
    }
}

void StreamingMd5Context::Transform(const uint8_t block[64])
{
    uint32_t input[16];
    for (uint32_t i = 0; i < 16; ++i) {
        input[i] = LoadLittleEndian32(block + i * 4);
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];

    ApplyRound(Round1, a, b, c, d, input[0] + 0xd76aa478U, 7);
    ApplyRound(Round1, d, a, b, c, input[1] + 0xe8c7b756U, 12);
    ApplyRound(Round1, c, d, a, b, input[2] + 0x242070dbU, 17);
    ApplyRound(Round1, b, c, d, a, input[3] + 0xc1bdceeeU, 22);
    ApplyRound(Round1, a, b, c, d, input[4] + 0xf57c0fafU, 7);
    ApplyRound(Round1, d, a, b, c, input[5] + 0x4787c62aU, 12);
    ApplyRound(Round1, c, d, a, b, input[6] + 0xa8304613U, 17);
    ApplyRound(Round1, b, c, d, a, input[7] + 0xfd469501U, 22);
    ApplyRound(Round1, a, b, c, d, input[8] + 0x698098d8U, 7);
    ApplyRound(Round1, d, a, b, c, input[9] + 0x8b44f7afU, 12);
    ApplyRound(Round1, c, d, a, b, input[10] + 0xffff5bb1U, 17);
    ApplyRound(Round1, b, c, d, a, input[11] + 0x895cd7beU, 22);
    ApplyRound(Round1, a, b, c, d, input[12] + 0x6b901122U, 7);
    ApplyRound(Round1, d, a, b, c, input[13] + 0xfd987193U, 12);
    ApplyRound(Round1, c, d, a, b, input[14] + 0xa679438eU, 17);
    ApplyRound(Round1, b, c, d, a, input[15] + 0x49b40821U, 22);

    ApplyRound(Round2, a, b, c, d, input[1] + 0xf61e2562U, 5);
    ApplyRound(Round2, d, a, b, c, input[6] + 0xc040b340U, 9);
    ApplyRound(Round2, c, d, a, b, input[11] + 0x265e5a51U, 14);
    ApplyRound(Round2, b, c, d, a, input[0] + 0xe9b6c7aaU, 20);
    ApplyRound(Round2, a, b, c, d, input[5] + 0xd62f105dU, 5);
    ApplyRound(Round2, d, a, b, c, input[10] + 0x02441453U, 9);
    ApplyRound(Round2, c, d, a, b, input[15] + 0xd8a1e681U, 14);
    ApplyRound(Round2, b, c, d, a, input[4] + 0xe7d3fbc8U, 20);
    ApplyRound(Round2, a, b, c, d, input[9] + 0x21e1cde6U, 5);
    ApplyRound(Round2, d, a, b, c, input[14] + 0xc33707d6U, 9);
    ApplyRound(Round2, c, d, a, b, input[3] + 0xf4d50d87U, 14);
    ApplyRound(Round2, b, c, d, a, input[8] + 0x455a14edU, 20);
    ApplyRound(Round2, a, b, c, d, input[13] + 0xa9e3e905U, 5);
    ApplyRound(Round2, d, a, b, c, input[2] + 0xfcefa3f8U, 9);
    ApplyRound(Round2, c, d, a, b, input[7] + 0x676f02d9U, 14);
    ApplyRound(Round2, b, c, d, a, input[12] + 0x8d2a4c8aU, 20);

    ApplyRound(Round3, a, b, c, d, input[5] + 0xfffa3942U, 4);
    ApplyRound(Round3, d, a, b, c, input[8] + 0x8771f681U, 11);
    ApplyRound(Round3, c, d, a, b, input[11] + 0x6d9d6122U, 16);
    ApplyRound(Round3, b, c, d, a, input[14] + 0xfde5380cU, 23);
    ApplyRound(Round3, a, b, c, d, input[1] + 0xa4beea44U, 4);
    ApplyRound(Round3, d, a, b, c, input[4] + 0x4bdecfa9U, 11);
    ApplyRound(Round3, c, d, a, b, input[7] + 0xf6bb4b60U, 16);
    ApplyRound(Round3, b, c, d, a, input[10] + 0xbebfbc70U, 23);
    ApplyRound(Round3, a, b, c, d, input[13] + 0x289b7ec6U, 4);
    ApplyRound(Round3, d, a, b, c, input[0] + 0xeaa127faU, 11);
    ApplyRound(Round3, c, d, a, b, input[3] + 0xd4ef3085U, 16);
    ApplyRound(Round3, b, c, d, a, input[6] + 0x04881d05U, 23);
    ApplyRound(Round3, a, b, c, d, input[9] + 0xd9d4d039U, 4);
    ApplyRound(Round3, d, a, b, c, input[12] + 0xe6db99e5U, 11);
    ApplyRound(Round3, c, d, a, b, input[15] + 0x1fa27cf8U, 16);
    ApplyRound(Round3, b, c, d, a, input[2] + 0xc4ac5665U, 23);

    ApplyRound(Round4, a, b, c, d, input[0] + 0xf4292244U, 6);
    ApplyRound(Round4, d, a, b, c, input[7] + 0x432aff97U, 10);
    ApplyRound(Round4, c, d, a, b, input[14] + 0xab9423a7U, 15);
    ApplyRound(Round4, b, c, d, a, input[5] + 0xfc93a039U, 21);
    ApplyRound(Round4, a, b, c, d, input[12] + 0x655b59c3U, 6);
    ApplyRound(Round4, d, a, b, c, input[3] + 0x8f0ccc92U, 10);
    ApplyRound(Round4, c, d, a, b, input[10] + 0xffeff47dU, 15);
    ApplyRound(Round4, b, c, d, a, input[1] + 0x85845dd1U, 21);
    ApplyRound(Round4, a, b, c, d, input[8] + 0x6fa87e4fU, 6);
    ApplyRound(Round4, d, a, b, c, input[15] + 0xfe2ce6e0U, 10);
    ApplyRound(Round4, c, d, a, b, input[6] + 0xa3014314U, 15);
    ApplyRound(Round4, b, c, d, a, input[13] + 0x4e0811a1U, 21);
    ApplyRound(Round4, a, b, c, d, input[4] + 0xf7537e82U, 6);
    ApplyRound(Round4, d, a, b, c, input[11] + 0xbd3af235U, 10);
    ApplyRound(Round4, c, d, a, b, input[2] + 0x2ad7d2bbU, 15);
    ApplyRound(Round4, b, c, d, a, input[9] + 0xeb86d391U, 21);

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

} // namespace omniruntime::vectorization
