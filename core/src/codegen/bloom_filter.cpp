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
 * Description: BloomFilter operator source file
 */

#include "bloom_filter.h"
#include "util/type_util.h"
#include "codegen/functions/murmur3_hash.h"

namespace omniruntime {
namespace op {
using namespace std;
using namespace omniruntime::codegen::function;
using namespace omniruntime::type;

BloomFilter::BloomFilter(int8_t *in, int32_t versionJava) : version(versionJava)
{
    int32_t inputVersion = (reinterpret_cast<int32_t *>(in))[0]; // version 4 Bytes
    if (version != inputVersion) {
        throw omniruntime::exception::OmniException("ILLEGAL_INPUT", "wrong version for bloom filter");
    }

    numHashFunctions = (reinterpret_cast<int32_t *>(in))[1]; // numHashFunctions 4 Bytes
    bits = new BitArray(in + 8);                             // offset is 8 Bytes
}

BloomFilter::~BloomFilter() { delete bits; }

/*
 * @Func : put long data into BloomFilter struct
 * @param item : long type data
 * @return : Returns true if the bit slot is reversed after insertion, Otherwise, false is returned.
 */
bool BloomFilter::PutLong(int64_t item)
{
    int32_t h1 = Mm3Int64(item, false, 0, false);
    int32_t h2 = Mm3Int64(item, false, h1, false);

    uint64_t bitSize = bits->GetBitSize();
    bool bitsChanged = false;
    for (int32_t i = 1; i <= numHashFunctions; i++) {
        int32_t combinedHash = h1 + (i * h2);
        // Flip all the bits if it's negative (guaranteed positive number)
        if (combinedHash < 0) {
            combinedHash = ~combinedHash;
        }
        bitsChanged |= bits->Set(combinedHash % bitSize);
    }
    return bitsChanged;
}

/*
 * @Func : Check whether a item is in the filter.
 * @param item : long type data
 * @return : Returns true if the item in the filter, Otherwise, false is returned.
 */
bool BloomFilter::MightContainLong(int64_t item)
{
    int32_t h1 = Mm3Int64(item, false, 0, false);
    int32_t h2 = Mm3Int64(item, false, h1, false);

    uint64_t bitSize = bits->GetBitSize();
    for (int32_t i = 1; i <= numHashFunctions; i++) {
        int32_t combinedHash = h1 + (i * h2);
        // Flip all the bits if it's negative (guaranteed positive number)
        if (combinedHash < 0) {
            combinedHash = ~combinedHash;
        }
        if (!bits->Get(combinedHash % bitSize)) {
            return false;
        }
    }
    return true;
}

int32_t BloomFilter::GetNumHashFunctions() { return numHashFunctions; }

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

OmniStatus BloomFilterOperator::Close() { return OMNI_STATUS_NORMAL; }
} // namespace op
} // namespace omniruntime
