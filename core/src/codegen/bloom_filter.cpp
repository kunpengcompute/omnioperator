/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: BloomFilter operator source file
 */

#include "bloom_filter.h"
#include "util/type_util.h"
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__aarch64__) && defined(__ARM_FEATURE_SVE)
#if defined(__has_include)
#if __has_include(<arm_sve.h>)
#include <arm_sve.h>
#define OMNI_BLOOM_ENABLE_SVE 1
#endif
#else
#include <arm_sve.h>
#define OMNI_BLOOM_ENABLE_SVE 1
#endif
#endif
#ifndef OMNI_BLOOM_ENABLE_SVE
#define OMNI_BLOOM_ENABLE_SVE 0
#endif

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace omniruntime {
namespace op {
using namespace std;
using namespace omniruntime::type;

namespace {
// Spark BloomFilterImpl wire: version(int32 BE) + numHashFunctions(int32 BE) + numWords(int32 BE) + words(long BE[]).
// Omni SIMD algorithm does not use numHashFunctions; write 0 so Driver-side Java mightContain
// is a no-op (always true) and cannot false-negative partition pruning.
constexpr int32_t SPARK_WIRE_HEADER_SIZE = 12;
constexpr int32_t LEGACY_NATIVE_HEADER_SIZE = 8;
constexpr int32_t SPARK_WIRE_NUM_HASH_FUNCTIONS = 0;

int32_t ReadNativeInt32(const char *data, int32_t offset)
{
    int32_t value = 0;
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

int32_t ReadInt32BE(const char *data, int32_t offset)
{
    const auto *buf = reinterpret_cast<const uint8_t *>(data + offset);
    return (static_cast<int32_t>(buf[0]) << 24) | (static_cast<int32_t>(buf[1]) << 16) |
        (static_cast<int32_t>(buf[2]) << 8) | static_cast<int32_t>(buf[3]);
}

void WriteInt32BE(char *data, int32_t offset, int32_t value)
{
    auto *buf = reinterpret_cast<uint8_t *>(data + offset);
    buf[0] = static_cast<uint8_t>((value >> 24) & 0xff);
    buf[1] = static_cast<uint8_t>((value >> 16) & 0xff);
    buf[2] = static_cast<uint8_t>((value >> 8) & 0xff);
    buf[3] = static_cast<uint8_t>(value & 0xff);
}

uint64_t ReadInt64BE(const char *data, int32_t offset)
{
    const auto *buf = reinterpret_cast<const uint8_t *>(data + offset);
    uint64_t value = 0;
    for (int32_t i = 0; i < 8; i++) {
        value = (value << 8) | buf[i];
    }
    return value;
}

void WriteInt64BE(char *data, int32_t offset, uint64_t value)
{
    auto *buf = reinterpret_cast<uint8_t *>(data + offset);
    buf[0] = static_cast<uint8_t>((value >> 56) & 0xff);
    buf[1] = static_cast<uint8_t>((value >> 48) & 0xff);
    buf[2] = static_cast<uint8_t>((value >> 40) & 0xff);
    buf[3] = static_cast<uint8_t>((value >> 32) & 0xff);
    buf[4] = static_cast<uint8_t>((value >> 24) & 0xff);
    buf[5] = static_cast<uint8_t>((value >> 16) & 0xff);
    buf[6] = static_cast<uint8_t>((value >> 8) & 0xff);
    buf[7] = static_cast<uint8_t>(value & 0xff);
}

bool IsValidWordsNum(int32_t wordsNum)
{
    return wordsNum >= 4 && (wordsNum & (wordsNum - 1)) == 0;
}

struct SerializedLayout {
    int32_t version;
    int32_t wordsNum;
    int32_t bitsOffset;
    bool bigEndian;
};

SerializedLayout ParseSerializedHeader(const char *serialized)
{
    SerializedLayout layout{};
    int32_t versionBE = ReadInt32BE(serialized, 0);
    int32_t versionNative = ReadNativeInt32(serialized, 0);
    if (versionBE == BloomFilter::VERSION) {
        // Spark DataInputStream / old Omni WriteInt layout.
        layout.version = versionBE;
        layout.wordsNum = ReadInt32BE(serialized, 8);
        layout.bitsOffset = SPARK_WIRE_HEADER_SIZE;
        layout.bigEndian = true;
    } else if (versionNative == BloomFilter::VERSION) {
        // Legacy Omni native-endian 8-byte header (pre-endian restore).
        layout.version = versionNative;
        layout.wordsNum = ReadNativeInt32(serialized, 4);
        layout.bitsOffset = LEGACY_NATIVE_HEADER_SIZE;
        layout.bigEndian = false;
    } else {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "BloomFilter version is invalid.");
    }
    if (!IsValidWordsNum(layout.wordsNum)) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT",
            "BloomFilter requires wordsNum to be a power of two and at least 4.");
    }
    return layout;
}

void CopyWordsFromSerialized(BitArray *bits, const char *serialized, const SerializedLayout &layout)
{
    auto *dst = reinterpret_cast<uint64_t *>(bits->GetData());
    if (layout.bigEndian) {
        for (int32_t i = 0; i < layout.wordsNum; i++) {
            dst[i] = ReadInt64BE(serialized, layout.bitsOffset + i * static_cast<int32_t>(sizeof(uint64_t)));
        }
    } else {
        std::memcpy(dst, serialized + layout.bitsOffset, sizeof(uint64_t) * layout.wordsNum);
    }
}
}

/**
 * Build a BloomFilter object and store the input data into the BloomFilter structure after reversing the byte order.
 *
 * Only UT use this constructor now, so we do not need to filp the endian
 *
 * @param in: Pointer to input data.
 * @param versionJava: The version of the BloomFilter.
 */
BloomFilter::BloomFilter(int8_t *in, int32_t versionJava) : version(versionJava)
{
    int32_t versionIn = (reinterpret_cast<int32_t *>(in))[0];
    if (version != versionIn) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "wrong version for bloom filter");
    }

    // version is int32, 4bytes len
    bits = new BitArray(in + sizeof(int32_t));
    ValidateVersion();
}

/**
 * Build a BloomFilter object and initialize its internal BitArray based on the input size.
 *
 * @param size: Size of input data bits.
 * @param version: The version of the BloomFilter.
 */
BloomFilter::BloomFilter(int32_t size, int32_t version)
{
    bits = new BitArray(size);
    this->version = version;
    ValidateVersion();
}

/**
 * Build a BloomFilter object from serialized bytes.
 * Preferred wire format matches Spark BloomFilterImpl / DataOutputStream (big-endian):
 *   version(4) + numHashFunctions(4) + numWords(4) + words(long[]).
 * Also accepts the legacy Omni native-endian 8-byte header for compatibility.
 */
BloomFilter::BloomFilter(char *serialized, bool isRelease) : bits(nullptr), version(0)
{
    auto releaseSerialized = [&]() {
        if (isRelease && serialized != nullptr) {
            delete[] serialized;
            serialized = nullptr;
        }
    };

    try {
        SerializedLayout layout = ParseSerializedHeader(serialized);
        version = layout.version;
        bits = new BitArray(layout.wordsNum);
        CopyWordsFromSerialized(bits, serialized, layout);
    } catch (...) {
        releaseSerialized();
        throw;
    }
    releaseSerialized();
    ValidateVersion();
}

/**
 * Serialize for Spark Java BloomFilterImpl.readFrom (big-endian WriteInt/WriteLong).
 * numHashFunctions is unused by Omni SIMD MightContain; written as 0 so Driver Java
 * mightContain cannot drop partitions with a mismatched hash algorithm.
 */
void BloomFilter::Serialize(char *serialized)
{
    WriteInt32BE(serialized, 0, version);
    WriteInt32BE(serialized, sizeof(int32_t), SPARK_WIRE_NUM_HASH_FUNCTIONS);
    WriteInt32BE(serialized, 2 * static_cast<int32_t>(sizeof(int32_t)), bits->GetWordsNum());
    auto *src = reinterpret_cast<const uint64_t *>(bits->GetData());
    int32_t wordsNum = bits->GetWordsNum();
    for (int32_t i = 0; i < wordsNum; i++) {
        WriteInt64BE(serialized, SPARK_WIRE_HEADER_SIZE + i * static_cast<int32_t>(sizeof(uint64_t)), src[i]);
    }
}

/**
 * Merge another serialized BloomFilter into this one.
 */
void BloomFilter::Merge(char *serialized)
{
    SerializedLayout layout = ParseSerializedHeader(serialized);
    if (layout.version != version) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "BloomFilter version must be the same");
    }
    if (layout.wordsNum != bits->GetWordsNum()) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "BloomFilter wordsNum must be the same");
    }
    if (layout.bigEndian) {
        std::vector<uint64_t> words(static_cast<size_t>(layout.wordsNum));
        for (int32_t i = 0; i < layout.wordsNum; i++) {
            words[static_cast<size_t>(i)] =
                ReadInt64BE(serialized, layout.bitsOffset + i * static_cast<int32_t>(sizeof(uint64_t)));
        }
        bits->Merge(words.data());
    } else {
        bits->Merge(reinterpret_cast<const uint64_t *>(serialized + layout.bitsOffset));
    }
}

/**
 * Spark wire size: 12-byte header + 8 bytes per word.
 */
uint64_t BloomFilter::GetSerializedSize()
{
    return SPARK_WIRE_HEADER_SIZE + bits->GetWordsNum() * 8;
}

BloomFilter::~BloomFilter()
{
    delete bits;
}

bool BloomFilter::IsPowerOfTwo(int32_t value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

uint64_t BloomFilter::BloomMask(uint64_t hashCode)
{
    return (1ULL << (hashCode & 63)) | (1ULL << ((hashCode >> 6) & 63)) |
        (1ULL << ((hashCode >> 12) & 63)) | (1ULL << ((hashCode >> 18) & 63));
}

uint32_t BloomFilter::BloomIndex(uint32_t bloomSize, uint64_t hashCode)
{
    return (hashCode >> 24) & (bloomSize - 1);
}

int32_t BloomFilter::GetSimdLaneCount()
{
#if OMNI_BLOOM_ENABLE_SVE
    return static_cast<int32_t>(svcntb() / sizeof(uint64_t));
#elif defined(__aarch64__) && defined(__ARM_NEON)
    return 2;
#else
    return 1;
#endif
}

void BloomFilter::BloomMaskBatch(const int64_t *hashCodes, uint64_t *masks, int32_t laneCount)
{
#if OMNI_BLOOM_ENABLE_SVE
    (void)laneCount;
    svbool_t pg = svptrue_b64();
    svuint64_t hashVec = svld1_u64(pg, reinterpret_cast<const uint64_t *>(hashCodes));
    svuint64_t bitMask = svdup_n_u64(63);
    svuint64_t one = svdup_n_u64(1);

    svuint64_t bit0 = svand_u64_x(pg, hashVec, bitMask);
    svuint64_t bit1 = svand_u64_x(pg, svlsr_n_u64_x(pg, hashVec, 6), bitMask);
    svuint64_t bit2 = svand_u64_x(pg, svlsr_n_u64_x(pg, hashVec, 12), bitMask);
    svuint64_t bit3 = svand_u64_x(pg, svlsr_n_u64_x(pg, hashVec, 18), bitMask);

    svuint64_t mask0 = svlsl_u64_x(pg, one, bit0);
    svuint64_t mask1 = svlsl_u64_x(pg, one, bit1);
    svuint64_t mask2 = svlsl_u64_x(pg, one, bit2);
    svuint64_t mask3 = svlsl_u64_x(pg, one, bit3);
    svuint64_t mask = svorr_u64_x(pg, svorr_u64_x(pg, mask0, mask1), svorr_u64_x(pg, mask2, mask3));
    svst1_u64(pg, masks, mask);
#elif defined(__aarch64__) && defined(__ARM_NEON)
    auto buildMask2 = [](uint64x2_t hashVec) {
        const uint64x2_t bitMask = vdupq_n_u64(63);
        const uint64x2_t one = vdupq_n_u64(1);
        const int64x2_t shift6 = vdupq_n_s64(-6);
        const int64x2_t shift12 = vdupq_n_s64(-12);
        const int64x2_t shift18 = vdupq_n_s64(-18);

        uint64x2_t bit0 = vandq_u64(hashVec, bitMask);
        uint64x2_t bit1 = vandq_u64(vshlq_u64(hashVec, shift6), bitMask);
        uint64x2_t bit2 = vandq_u64(vshlq_u64(hashVec, shift12), bitMask);
        uint64x2_t bit3 = vandq_u64(vshlq_u64(hashVec, shift18), bitMask);

        uint64x2_t mask0 = vshlq_u64(one, vreinterpretq_s64_u64(bit0));
        uint64x2_t mask1 = vshlq_u64(one, vreinterpretq_s64_u64(bit1));
        uint64x2_t mask2 = vshlq_u64(one, vreinterpretq_s64_u64(bit2));
        uint64x2_t mask3 = vshlq_u64(one, vreinterpretq_s64_u64(bit3));
        return vorrq_u64(vorrq_u64(mask0, mask1), vorrq_u64(mask2, mask3));
    };

    int32_t i = 0;
    for (; i + 2 <= laneCount; i += 2) {
        uint64x2_t hashVec = vld1q_u64(reinterpret_cast<const uint64_t *>(hashCodes + i));
        vst1q_u64(masks + i, buildMask2(hashVec));
    }
    for (; i < laneCount; i++) {
        masks[i] = BloomMask(static_cast<uint64_t>(hashCodes[i]));
    }
#else
    for (int32_t i = 0; i < laneCount; i++) {
        masks[i] = BloomMask(static_cast<uint64_t>(hashCodes[i]));
    }
#endif
}

void BloomFilter::BloomIndexBatch(uint32_t bloomSize, const int64_t *hashCodes, uint64_t *indexes, int32_t laneCount)
{
#if OMNI_BLOOM_ENABLE_SVE
    (void)laneCount;
    svbool_t pg = svptrue_b64();
    svuint64_t hashVec = svld1_u64(pg, reinterpret_cast<const uint64_t *>(hashCodes));
    svuint64_t indexMask = svdup_n_u64(static_cast<uint64_t>(bloomSize - 1));
    svuint64_t index = svand_u64_x(pg, svlsr_n_u64_x(pg, hashVec, 24), indexMask);
    svst1_u64(pg, indexes, index);
#elif defined(__aarch64__) && defined(__ARM_NEON)
    const int64x2_t shift24 = vdupq_n_s64(-24);
    const uint64x2_t indexMask = vdupq_n_u64(static_cast<uint64_t>(bloomSize - 1));

    int32_t i = 0;
    for (; i + 2 <= laneCount; i += 2) {
        uint64x2_t hashVec = vld1q_u64(reinterpret_cast<const uint64_t *>(hashCodes + i));
        uint64x2_t index = vandq_u64(vshlq_u64(hashVec, shift24), indexMask);
        vst1q_u64(indexes + i, index);
    }
    for (; i < laneCount; i++) {
        indexes[i] = BloomIndex(bloomSize, static_cast<uint64_t>(hashCodes[i]));
    }
#else
    for (int32_t i = 0; i < laneCount; i++) {
        indexes[i] = BloomIndex(bloomSize, static_cast<uint64_t>(hashCodes[i]));
    }
#endif
}

void BloomFilter::ValidateVersion()
{
    if (version != VERSION) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "BloomFilter only supports version1.");
    }

    int32_t wordsNum = bits->GetWordsNum();
    if (wordsNum < 4 || !IsPowerOfTwo(wordsNum)) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT",
            "BloomFilter requires wordsNum to be a power of two and at least 4.");
    }
}

/*
 * @Func : put long data into BloomFilter struct
 * @param item : long type data
 * @return : Returns true if the bit slot is reversed after insertion, Otherwise, false is returned.
 */
bool BloomFilter::PutLong(int64_t item)
{
    auto data = reinterpret_cast<uint64_t *>(bits->GetData());
    uint32_t wordsNum = static_cast<uint32_t>(bits->GetWordsNum());
    uint64_t hash = item;
    uint64_t mask = BloomMask(hash);
    uint32_t index = BloomIndex(wordsNum, hash);
    bool bitsChanged = (data[index] & mask) != mask;
    data[index] |= mask;
    return bitsChanged;
}

/*
 * @Func : Check whether a item is in the filter.
 * @param item : long type data
 * @return : Returns true if the item in the filter, Otherwise, false is returned.
 */
bool BloomFilter::MightContainLong(int64_t item)
{
    auto data = reinterpret_cast<uint64_t *>(bits->GetData());
    uint32_t wordsNum = static_cast<uint32_t>(bits->GetWordsNum());
    uint64_t hash = item;
    uint64_t mask = BloomMask(hash);
    uint32_t index = BloomIndex(wordsNum, hash);
    return mask == (data[index] & mask);
}

void BloomFilter::MightContainLongBatch(const int64_t *items, bool *results, int32_t count)
{
    if (count <= 0) {
        return;
    }

    auto data = reinterpret_cast<uint64_t *>(bits->GetData());
    uint32_t wordsNum = static_cast<uint32_t>(bits->GetWordsNum());
    int32_t simdLaneCount = GetSimdLaneCount();
    std::vector<uint64_t> masks(simdLaneCount);
    std::vector<uint64_t> indexes(simdLaneCount);
    int32_t i = 0;
    for (; i + simdLaneCount <= count; i += simdLaneCount) {
        BloomMaskBatch(items + i, masks.data(), simdLaneCount);
        BloomIndexBatch(wordsNum, items + i, indexes.data(), simdLaneCount);
        for (int32_t lane = 0; lane < simdLaneCount; lane++) {
            uint64_t mask = masks[lane];
            results[i + lane] = mask == (data[static_cast<uint32_t>(indexes[lane])] & mask);
        }
    }

    for (; i < count; i++) {
        uint64_t hash = static_cast<uint64_t>(items[i]);
        uint64_t mask = BloomMask(hash);
        uint32_t index = BloomIndex(wordsNum, hash);
        results[i] = mask == (data[index] & mask);
    }
}

// function implements for class BloomFilterOperatorFactory
BloomFilterOperatorFactory::BloomFilterOperatorFactory(int32_t version) : version(version) {}

BloomFilterOperatorFactory::~BloomFilterOperatorFactory() = default;

BloomFilterOperatorFactory *BloomFilterOperatorFactory::CreateBloomFilterOperatorFactory(int32_t version)
{
    auto pOperatorFactory = new BloomFilterOperatorFactory(version);
    return pOperatorFactory;
}

Operator *BloomFilterOperatorFactory::CreateOperator()
{
    auto pSortOperator = new BloomFilterOperator(version);
    return pSortOperator;
}

// function implements for class BloomFilterOperator
BloomFilterOperator::BloomFilterOperator(int32_t version) : version(version) {}

BloomFilterOperator::~BloomFilterOperator()
{
    VectorHelper::FreeVecBatch(inputVecBatch);
    delete bloomFilterAddress;
}

int32_t BloomFilterOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch == nullptr) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "BloomFilterOperator AddInput can't be nullptr!");
    }

    inputVecBatch = vecBatch;
    int32_t vectorCount = inputVecBatch->GetVectorCount();
    if (vectorCount != 1) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "vecBatch col should be 1 for bloom filter");
    }
    BaseVector *colVec = inputVecBatch->Get(0);
    auto valuesAddress = reinterpret_cast<int64_t>(VectorHelper::UnsafeGetValues(colVec));

    // init BloomFilter
    bloomFilterAddress = new BloomFilter(reinterpret_cast<int8_t *>((uintptr_t)valuesAddress), version);
    return 0;
}

int32_t BloomFilterOperator::GetOutput(VectorBatch **blOutPut)
{
    auto outPut = new VectorBatch(1);
    auto *col = new Vector<int64_t>(1);
    col->SetValue(0, reinterpret_cast<int64_t>(bloomFilterAddress));
    outPut->Append(col);
    *blOutPut = outPut;
    SetStatus(OMNI_STATUS_FINISHED);
    return 0;
}

OmniStatus BloomFilterOperator::Close()
{
    return OMNI_STATUS_NORMAL;
}
} // end of op
} // end of omniruntime