/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "gtest/gtest.h"
#include "codegen/bloom_filter.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace omniruntime::TestUtil;

namespace BloomFilterTest {
TEST(BloomFilterTest, TestBloomFilterInit)
{
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    // construct data
    const int32_t rowNum = 12;
    int32_t intVecData[rowNum] = {1, 6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    VectorBatch *inputVectorBatch = CreateVectorBatch(inputTypes, rowNum, intVecData);

    auto *factory = BloomFilterOperatorFactory::CreateBloomFilterOperatorFactory(1);
    omniruntime::op::Operator *op = factory->CreateOperator();
    op->AddInput(inputVectorBatch);

    VectorBatch *result = nullptr;
    op->GetOutput(&result);
    long longValue = (static_cast<Vector<int64_t> *>(result->Get(0)))->GetValue(0);
    BloomFilter *bfResult = (BloomFilter *)longValue;
    EXPECT_EQ(bfResult->GetNumHashFunctions(), 6);
    EXPECT_EQ(bfResult->GetBits()->GetWordsNum(), 4);
    VectorHelper::FreeVecBatch(result);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
}

TEST(BloomFilterTest, TestBloomFilterPutLong)
{
    int32_t versionJava = 1;
    int32_t hashFuncNum = 6;
    int32_t numWords = 1048576; // 1MBytes

    int32_t byteLength = numWords * sizeof(uint64_t) + sizeof(versionJava) + sizeof(hashFuncNum) + sizeof(numWords);
    byte *in = new byte[byteLength]{(byte)0};
    (reinterpret_cast<int32_t *>(in))[0] = 1;
    (reinterpret_cast<int32_t *>(in))[1] = hashFuncNum;
    (reinterpret_cast<int32_t *>(in))[2] = numWords;
    BloomFilter *bf = new BloomFilter(reinterpret_cast<int8_t *>(in), versionJava);
    EXPECT_TRUE(bf->PutLong(LONG_MIN));
    EXPECT_TRUE(bf->PutLong(LONG_MAX));
    delete bf;
    delete[] in;
}

TEST(BloomFilterTest, TestBloomFilterMightContain)
{
    int32_t versionJava = 1;
    int32_t hashFuncNum = 6;
    int32_t numWords = 1048576; // 1MBytes

    int32_t byteLength = numWords * sizeof(uint64_t) + sizeof(versionJava) + sizeof(hashFuncNum) + sizeof(numWords);
    byte *in = new byte[byteLength]{(byte)0};
    (reinterpret_cast<int32_t *>(in))[0] = 1;
    (reinterpret_cast<int32_t *>(in))[1] = hashFuncNum;
    (reinterpret_cast<int32_t *>(in))[2] = numWords;
    BloomFilter *bf = new BloomFilter(reinterpret_cast<int8_t *>(in), versionJava);
    for (uint64_t i = 1; i < 100; i += 2) {
        EXPECT_TRUE(bf->PutLong(i));
    }

    for (int j = 1; j < 100; j += 2) {
        EXPECT_TRUE(bf->MightContainLong(j));
        EXPECT_FALSE(bf->MightContainLong(j - 1));
    }
    delete bf;
    delete[] in;
}
} // namespace BloomFilterTest
