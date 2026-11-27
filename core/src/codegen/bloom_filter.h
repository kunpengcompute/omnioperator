/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: BloomFilter operator header
 */
#ifndef OMNI_RUNTIME_BLOOM_FILTER_H
#define OMNI_RUNTIME_BLOOM_FILTER_H

#include <cstddef>
#include <memory>
#include "util/bit_array.h"
#include "operator/operator_factory.h"

namespace omniruntime {
namespace op {
class BloomFilter {
public:
    static constexpr int32_t VERSION = 1;

    explicit BloomFilter(int8_t *in, int32_t versionJava);

    explicit BloomFilter(int32_t size, int32_t version);

    explicit BloomFilter(char *serialized, bool isRelease = false);

    /**
     * Returns the process-wide filter for these serialized bytes, creating it on first use.
     * might_contain codegen embeds the filter address in the compiled function and in the
     * filter cache key, so tasks probing the same runtime filter must share one object to
     * reuse the compiled code. Entries are weak: a filter lives as long as the plans using
     * it. The returned filter is shared and must not be modified.
     */
    static std::shared_ptr<BloomFilter> GetShared(const char *serialized, size_t length);

    void Serialize(char *serialized);

    void Merge(char *in);

    uint64_t GetSerializedSize();

    ~BloomFilter();

    bool PutLong(int64_t item);

    bool MightContainLong(int64_t item);

    void MightContainLongBatch(const int64_t *items, bool *results, int32_t count);

    int32_t GetVersion() const
    {
        return version;
    }

    BitArray *GetBits()
    {
        return bits;
    }

private:
    // SIMD
    static bool IsPowerOfTwo(int32_t value);
    static int32_t GetSimdLaneCount();
    static void BloomMaskBatch(const int64_t *hashCodes, uint64_t *masks, int32_t laneCount);
    static void BloomIndexBatch(uint32_t bloomSize, const int64_t *hashCodes, uint64_t *indexes, int32_t laneCount);

    static uint64_t BloomMask(uint64_t hashCode);
    static uint32_t BloomIndex(uint32_t bloomSize, uint64_t hashCode);

    void ValidateVersion();

    BitArray *bits;
    int32_t version;
};

class BloomFilterOperatorFactory : public OperatorFactory {
public:
    explicit BloomFilterOperatorFactory(int32_t version);

    ~BloomFilterOperatorFactory() override;

    static BloomFilterOperatorFactory *CreateBloomFilterOperatorFactory(int32_t filterVersion);

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    int32_t version;
};

/*
 * BloomFilterOperator is only used by spark runtimeFilter Feature, It is used independently and cannot cooperate with
 * other operators. AddInput must be IntVector GetOutput must be LongVector
 */
class BloomFilterOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    BloomFilterOperator(omniruntime::mem::OmniTaskMemoryPool *pool, int32_t version);

    ~BloomFilterOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **blOutPut) override;

    OmniStatus Close() override;

private:
    int32_t version;
    BloomFilter *bloomFilterAddress;
    VectorBatch *inputVecBatch;
};
} // end of op
} // end of omniruntime
#endif // OMNI_RUNTIME_BLOOM_FILTER_H