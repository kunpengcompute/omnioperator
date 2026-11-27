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
#include "vector/vector_batch.h"
#include "vector_test_util.h"
#include "vector/dictionary_container.h"

namespace omniruntime::vec::test {
TEST(vector2, vec_batch)
{
    int32_t rowCnt = 32;
    auto intVec = std::make_unique<Vector<int32_t>>(rowCnt);
    auto stringVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        intVec->SetValue(i, i * 2);
        std::string str("hello" + std::to_string(i));
        std::string_view value(str.data(), str.size());
        stringVec->SetValue(i, value);
    }

    int32_t dictSize = 4;
    int32_t *values = new int32_t[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        values[i] = i % dictSize;
    }
    using DICTIONARY_DATA_TYPE = typename TYPE_UTIL<int32_t>::DICTIONARY_TYPE;
    auto dictionary = CreateDictionary<DICTIONARY_DATA_TYPE>(dictSize);
    auto container = std::make_shared<DictionaryContainer<int32_t>>(values, rowCnt, dictionary, dictSize, 0);
    std::unique_ptr<NullsBuffer> nullsBuffer = std::make_unique<NullsBuffer>(rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        nullsBuffer->SetNull(i, false);
    }
    auto intDictVec = std::make_unique<Vector<DictionaryContainer<int32_t>>>(rowCnt, container, nullsBuffer.get());

    VectorBatch vectorBatch(rowCnt);
    vectorBatch.Append(intVec.release());
    vectorBatch.Append(stringVec.release());
    vectorBatch.Append(intDictVec.release());

    auto intCol0 = reinterpret_cast<Vector<int32_t> *>(vectorBatch.Get(0));
    auto stringCol1 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vectorBatch.Get(1));
    auto intDictCol2 = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vectorBatch.Get(2));
    for (int32_t i = 0; i < rowCnt; i++) {
        EXPECT_EQ(intCol0->GetValue(i), i * 2);
        EXPECT_EQ(stringCol1->GetValue(i), "hello" + std::to_string(i));
        EXPECT_EQ(intDictCol2->GetValue(i), (i % dictSize) * 2 / 3);
    }
    delete[] values;
    vectorBatch.FreeAllVectors();
}

TEST(vector2, vec_batch_resize)
{
    int rowCount = 100;
    int resizeCount = 50;
    auto *pVec1 = new Vector<NativeType<OMNI_LONG>::type>(rowCount, OMNI_LONG);
    auto *pVec2 = new Vector<NativeType<OMNI_DOUBLE>::type>(rowCount, OMNI_DOUBLE);
    auto *pVec3 = new Vector<NativeType<OMNI_DOUBLE>::type>(rowCount, OMNI_DOUBLE);

    auto *pVb = new VectorBatch(rowCount);
    pVb->Append(pVec1);
    pVb->Append(pVec2);
    pVb->Append(pVec3);

    ASSERT_EQ(pVb->GetRowCount(), rowCount);
    ASSERT_EQ(pVb->GetRowCount(), pVec1->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec2->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec3->GetSize());

    pVb->Resize(resizeCount);
    ASSERT_EQ(pVb->GetRowCount(), resizeCount);
    ASSERT_EQ(pVb->GetRowCount(), pVec1->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec2->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec3->GetSize());

    VectorHelper::PrintVecBatch(pVb);
    VectorHelper::FreeVecBatch(pVb);
}
} // namespace omniruntime::vec::test
