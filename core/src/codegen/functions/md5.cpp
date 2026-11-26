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
 * Third-party license and source notices
 *
 * @Description: md5 function implementations
 *  This code is taken from duckdb/common/crypto/md5.hpp with following changes:
 *  1. removed other duckdb dependencies.
 *  2. Add the DigestToBase10 method which produce the md5 in decimal
 *
 * This code taken from the SQLite test library.  Originally found on
 * the internet.  The original header comment follows this comment.
 * The code is largerly unchanged, but there have been some modifications.
 *
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#include "md5.h"

namespace omniruntime::codegen::function {
using Fun = unsigned int (*)(unsigned int, unsigned int, unsigned int);

static inline unsigned int Fun1(unsigned int x, unsigned int y, unsigned int z) { return z ^ (x & (y ^ z)); }

static inline unsigned int Fun2(unsigned int x, unsigned int y, unsigned int z) { return Fun1(z, x, y); }

static inline unsigned int Fun3(unsigned int x, unsigned int y, unsigned int z) { return x ^ y ^ z; }

static inline unsigned int Fun4(unsigned int x, unsigned int y, unsigned int z) { return y ^ (x | ~z); }

static inline void Md5Fun(Fun f, unsigned int &w, unsigned int x, unsigned int y, unsigned int z, unsigned int data,
                          unsigned int s)
{
    w += f(x, y, z) + data;
    w = w << s | w >> (32 - s);
    w += x;
}

/**
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 long words of new data.  MD5Update blocks
 * the data and converts bytes into long words for this routine.
 */
static void MD5Transform(unsigned int buf[4], const unsigned int in[16])
{
    unsigned int b1 = buf[0];
    unsigned int b2 = buf[1];
    unsigned int b3 = buf[2];
    unsigned int b4 = buf[3];

    Md5Fun(Fun1, b1, b2, b3, b4, in[0] + 0xd76aa478, 7);
    Md5Fun(Fun1, b4, b1, b2, b3, in[1] + 0xe8c7b756, 12);
    Md5Fun(Fun1, b3, b4, b1, b2, in[2] + 0x242070db, 17);
    Md5Fun(Fun1, b2, b3, b4, b1, in[3] + 0xc1bdceee, 22);
    Md5Fun(Fun1, b1, b2, b3, b4, in[4] + 0xf57c0faf, 7);
    Md5Fun(Fun1, b4, b1, b2, b3, in[5] + 0x4787c62a, 12);
    Md5Fun(Fun1, b3, b4, b1, b2, in[6] + 0xa8304613, 17);
    Md5Fun(Fun1, b2, b3, b4, b1, in[7] + 0xfd469501, 22);
    Md5Fun(Fun1, b1, b2, b3, b4, in[8] + 0x698098d8, 7);
    Md5Fun(Fun1, b4, b1, b2, b3, in[9] + 0x8b44f7af, 12);
    Md5Fun(Fun1, b3, b4, b1, b2, in[10] + 0xffff5bb1, 17);
    Md5Fun(Fun1, b2, b3, b4, b1, in[11] + 0x895cd7be, 22);
    Md5Fun(Fun1, b1, b2, b3, b4, in[12] + 0x6b901122, 7);
    Md5Fun(Fun1, b4, b1, b2, b3, in[13] + 0xfd987193, 12);
    Md5Fun(Fun1, b3, b4, b1, b2, in[14] + 0xa679438e, 17);
    Md5Fun(Fun1, b2, b3, b4, b1, in[15] + 0x49b40821, 22);

    Md5Fun(Fun2, b1, b2, b3, b4, in[1] + 0xf61e2562, 5);
    Md5Fun(Fun2, b4, b1, b2, b3, in[6] + 0xc040b340, 9);
    Md5Fun(Fun2, b3, b4, b1, b2, in[11] + 0x265e5a51, 14);
    Md5Fun(Fun2, b2, b3, b4, b1, in[0] + 0xe9b6c7aa, 20);
    Md5Fun(Fun2, b1, b2, b3, b4, in[5] + 0xd62f105d, 5);
    Md5Fun(Fun2, b4, b1, b2, b3, in[10] + 0x02441453, 9);
    Md5Fun(Fun2, b3, b4, b1, b2, in[15] + 0xd8a1e681, 14);
    Md5Fun(Fun2, b2, b3, b4, b1, in[4] + 0xe7d3fbc8, 20);
    Md5Fun(Fun2, b1, b2, b3, b4, in[9] + 0x21e1cde6, 5);
    Md5Fun(Fun2, b4, b1, b2, b3, in[14] + 0xc33707d6, 9);
    Md5Fun(Fun2, b3, b4, b1, b2, in[3] + 0xf4d50d87, 14);
    Md5Fun(Fun2, b2, b3, b4, b1, in[8] + 0x455a14ed, 20);
    Md5Fun(Fun2, b1, b2, b3, b4, in[13] + 0xa9e3e905, 5);
    Md5Fun(Fun2, b4, b1, b2, b3, in[2] + 0xfcefa3f8, 9);
    Md5Fun(Fun2, b3, b4, b1, b2, in[7] + 0x676f02d9, 14);
    Md5Fun(Fun2, b2, b3, b4, b1, in[12] + 0x8d2a4c8a, 20);

    Md5Fun(Fun3, b1, b2, b3, b4, in[5] + 0xfffa3942, 4);
    Md5Fun(Fun3, b4, b1, b2, b3, in[8] + 0x8771f681, 11);
    Md5Fun(Fun3, b3, b4, b1, b2, in[11] + 0x6d9d6122, 16);
    Md5Fun(Fun3, b2, b3, b4, b1, in[14] + 0xfde5380c, 23);
    Md5Fun(Fun3, b1, b2, b3, b4, in[1] + 0xa4beea44, 4);
    Md5Fun(Fun3, b4, b1, b2, b3, in[4] + 0x4bdecfa9, 11);
    Md5Fun(Fun3, b3, b4, b1, b2, in[7] + 0xf6bb4b60, 16);
    Md5Fun(Fun3, b2, b3, b4, b1, in[10] + 0xbebfbc70, 23);
    Md5Fun(Fun3, b1, b2, b3, b4, in[13] + 0x289b7ec6, 4);
    Md5Fun(Fun3, b4, b1, b2, b3, in[0] + 0xeaa127fa, 11);
    Md5Fun(Fun3, b3, b4, b1, b2, in[3] + 0xd4ef3085, 16);
    Md5Fun(Fun3, b2, b3, b4, b1, in[6] + 0x04881d05, 23);
    Md5Fun(Fun3, b1, b2, b3, b4, in[9] + 0xd9d4d039, 4);
    Md5Fun(Fun3, b4, b1, b2, b3, in[12] + 0xe6db99e5, 11);
    Md5Fun(Fun3, b3, b4, b1, b2, in[15] + 0x1fa27cf8, 16);
    Md5Fun(Fun3, b2, b3, b4, b1, in[2] + 0xc4ac5665, 23);

    Md5Fun(Fun4, b1, b2, b3, b4, in[0] + 0xf4292244, 6);
    Md5Fun(Fun4, b4, b1, b2, b3, in[7] + 0x432aff97, 10);
    Md5Fun(Fun4, b3, b4, b1, b2, in[14] + 0xab9423a7, 15);
    Md5Fun(Fun4, b2, b3, b4, b1, in[5] + 0xfc93a039, 21);
    Md5Fun(Fun4, b1, b2, b3, b4, in[12] + 0x655b59c3, 6);
    Md5Fun(Fun4, b4, b1, b2, b3, in[3] + 0x8f0ccc92, 10);
    Md5Fun(Fun4, b3, b4, b1, b2, in[10] + 0xffeff47d, 15);
    Md5Fun(Fun4, b2, b3, b4, b1, in[1] + 0x85845dd1, 21);
    Md5Fun(Fun4, b1, b2, b3, b4, in[8] + 0x6fa87e4f, 6);
    Md5Fun(Fun4, b4, b1, b2, b3, in[15] + 0xfe2ce6e0, 10);
    Md5Fun(Fun4, b3, b4, b1, b2, in[6] + 0xa3014314, 15);
    Md5Fun(Fun4, b2, b3, b4, b1, in[13] + 0x4e0811a1, 21);
    Md5Fun(Fun4, b1, b2, b3, b4, in[4] + 0xf7537e82, 6);
    Md5Fun(Fun4, b4, b1, b2, b3, in[11] + 0xbd3af235, 10);
    Md5Fun(Fun4, b3, b4, b1, b2, in[2] + 0x2ad7d2bb, 15);
    Md5Fun(Fun4, b2, b3, b4, b1, in[9] + 0xeb86d391, 21);

    buf[0] += b1;
    buf[1] += b2;
    buf[2] += b3;
    buf[3] += b4;
}

static inline void ByteReverse(unsigned char *buf, unsigned longs)
{
    unsigned int t;
    do {
        t = (static_cast<unsigned int>(static_cast<unsigned>(buf[3]) << 8 | buf[2]) << 16) |
            (static_cast<unsigned>(buf[1]) << 8 | buf[0]);
        *reinterpret_cast<unsigned int *>(buf) = t;
        buf += 4;
    } while (--longs);
}

void Md5Function::Finish(unsigned char *outDigest)
{
    unsigned char *ptr;
    unsigned bitsCount;

    // Compute number of bytes mod 64
    bitsCount = (bits[0] >> 3) & 0x3F;

    // Set the first char of padding to 0x80.  This is safe since there is
    // always at least one byte free
    ptr = in + bitsCount;
    *ptr++ = 0x80;

    // Bytes of padding needed to make 64 bytes
    bitsCount = 64 - 1 - bitsCount;

    // Pad out to 56 mod 64
    if (bitsCount < 8) {
        // Two lots of padding:  Pad the first block to 64 bytes
        memset_s(ptr, bitsCount, 0, bitsCount);
        ByteReverse(in, 16);
        MD5Transform(buf, reinterpret_cast<unsigned int *>(in));

        // Now fill the next block with 56 bytes
        memset_s(in, 56, 0, 56);
    } else {
        // Pad block to 56 bytes
        memset_s(ptr, bitsCount - 8, 0, bitsCount - 8);
    }
    ByteReverse(in, 14);

    // Append length in bits and transform
    (reinterpret_cast<unsigned int *>(in))[14] = bits[0];
    (reinterpret_cast<unsigned int *>(in))[15] = bits[1];
    MD5Transform(buf, reinterpret_cast<unsigned int *>(in));
    ByteReverse(reinterpret_cast<unsigned char *>(buf), 4);
    memcpy_s(outDigest, 16, buf, 16);
}

void Md5Function::FinishHex(char *outDigest)
{
    unsigned char digest[MD5_HASH_LENGTH_BINARY];
    Finish(digest);
    DigestToBase16(digest, outDigest);
}

void Md5Function::MD5Update(const char *data, uint64_t len)
{
    unsigned int temp = bits[0];
    if ((bits[0] = temp + (static_cast<unsigned int>(len) << 3)) < temp) {
        bits[1]++; // Carry from low to high
    }
    bits[1] += len >> 29;
    temp = (temp >> 3) & 0x3f; // Bytes already in shsInfo->data

    // Handle any leading odd-sized chunks
    if (temp) {
        unsigned char *p = in + temp;

        temp = 64 - temp;
        if (len < temp) {
            memcpy_s(p, len, data, len);
            return;
        }
        memcpy_s(p, temp, data, temp);
        ByteReverse(in, 16);
        MD5Transform(buf, reinterpret_cast<unsigned int *>(in));
        data += temp;
        len -= temp;
    }

    // Process data in 64-byte chunks
    while (len >= 64) {
        memcpy_s(in, 64, data, 64);
        ByteReverse(in, 16);
        MD5Transform(buf, reinterpret_cast<unsigned int *>(in));
        data += 64;
        len -= 64;
    }

    // Handle any remaining bytes of data.
    memcpy_s(in, len, data, len);
}

void Md5Function::DigestToBase16(const unsigned char *digest, char *zBuf)
{
    static char const HEX_CODES[] = "0123456789abcdef";
    int i, j;
    for (j = i = 0; i < MD5_HASH_LENGTH_BINARY; i++) {
        int a = digest[i];
        zBuf[j++] = HEX_CODES[(a >> 4) & 0xf];
        zBuf[j++] = HEX_CODES[a & 0xf];
    }
}
} // namespace omniruntime::codegen::function
