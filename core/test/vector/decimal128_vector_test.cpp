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

#include <gtest/gtest.h>
#include "vector/vector_common.h"
#include "test/util/test_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::TestUtil;

namespace Decimal128VectorTest {
TEST(Decimal128Vector, SliceVector)
{
    auto originalVector = std::make_unique<Vector<Decimal128>>(10);
    for (int32_t i = 0; i < originalVector->GetSize(); i++) {
        std::string str = "12345" + std::to_string(i);
        Decimal128 value = Decimal128(str);
        originalVector->SetValue(i, value);
    }

    int32_t offset = 3;
    auto slice1 = (Vector<Decimal128> *)(originalVector->Slice(offset, 4));
    EXPECT_EQ(slice1->GetOffset(), offset);
    EXPECT_EQ(slice1->GetSize(), 4);
    for (int32_t i = 0; i < slice1->GetSize(); i++) {
        EXPECT_TRUE(slice1->GetValue(i) == originalVector->GetValue(i + offset));
    }

    auto slice2 = (Vector<Decimal128> *)(slice1->Slice(1, 2));
    for (int32_t i = 0; i < slice2->GetSize(); i++) {
        EXPECT_TRUE(slice2->GetValue(i) == originalVector->GetValue(i + offset + 1));
    }
    delete slice1;
    delete slice2;
}

// Test set/get
TEST(Decimal128Vector, SetAndGetValue)
{
    auto vector = std::make_unique<Vector<Decimal128>>(256);
    for (int32_t i = 0; i < 256; i++) {
        std::string str = "12345" + std::to_string(i);
        Decimal128 value = Decimal128(str);
        vector->SetValue(i, value);
        EXPECT_EQ(vector->GetValue(i), value);
    }
}

// Test is null
TEST(Decimal128Vector, SetValueNull)
{
    auto vector = std::make_unique<Vector<Decimal128>>(256);
    for (int32_t i = 0; i < 256; i++) {
        if (i % 5 == 0) {
            vector->SetNull(i);
        } else {
            Decimal128 value = Decimal128(i);
            vector->SetValue(i, value);
        }
    }
    for (int32_t i = 0; i < 256; i++) {
        if (i % 5 == 0) {
            EXPECT_TRUE(vector->IsNull(i));
        } else {
            EXPECT_FALSE(vector->IsNull(i));
        }
    }
}

// Test is copyPosition
TEST(Decimal128Vector, CopyPositions)
{
    auto originalVector = std::make_unique<Vector<Decimal128>>(4);
    for (int32_t i = 0; i < originalVector->GetSize(); i++) {
        Decimal128 value = Decimal128(i);
        originalVector->SetValue(i, value);
    }

    auto *possions = new int32_t[2];
    possions[0] = 1;
    possions[1] = 3;
    auto copyPostionVector = (Vector<Decimal128> *)(originalVector->CopyPositions(possions, 0, 2));

    for (int32_t i = 0; i < copyPostionVector->GetSize(); i++) {
        EXPECT_TRUE(copyPostionVector->GetValue(i) == originalVector->GetValue(possions[i]));
    }

    delete[] possions;
    delete copyPostionVector;
}

class Decimal128VectorTest {
public:
    Decimal128VectorTest(int32_t size) : values(new int64_t[size]) {}

    ~Decimal128VectorTest() { delete[] values; }

    void SetValue(int32_t index, int64_t value) { values[index] = value; }

    int64_t GetValue(int32_t index) { return values[index]; }

private:
    int64_t *values;
};

// Performance test
TEST(Decimal128Vector, PerformanceCompare)
{
    int32_t rowCount = 1000;
    Timer timer;

    // test long vector set value
    auto *vectorTest2 = new Decimal128VectorTest(rowCount);
    timer.Start("point test vector set value");
    for (int32_t i = 0; i < rowCount; ++i) {
        vectorTest2->SetValue(i, i);
    }
    timer.End();

    // test long vector set value
    Decimal128VectorTest vectorTest1(rowCount);
    timer.Start("stack test vector set value");
    for (int32_t i = 0; i < rowCount; ++i) {
        vectorTest1.SetValue(i, i);
    }
    timer.End();

    // test long vector get value
    timer.Start("point test vector get value");
    for (int32_t i = 0; i < rowCount; ++i) {
        vectorTest2->GetValue(i);
    }
    timer.End();

    // test long vector get value
    timer.Start("stack test vector get value");
    for (int32_t i = 0; i < rowCount; ++i) {
        vectorTest1.GetValue(i);
    }
    timer.End();

    // vector set value
    auto decimal128Vector = std::make_unique<Vector<Decimal128>>(rowCount);
    timer.Start("vector set value");
    for (int32_t i = 0; i < rowCount; ++i) {
        Decimal128 value = Decimal128(i);
        decimal128Vector->SetValue(i, value);
    }
    timer.End();

    // original set value
    int64_t *decimal128Vector2 = new int64_t[rowCount];
    timer.Start("original set value");
    for (int32_t i = 0; i < rowCount; ++i) {
        decimal128Vector2[i] = i;
    }
    timer.End();

    // vector get value
    Decimal128 *result = new Decimal128[rowCount];
    timer.Start("vector get value");
    for (int32_t i = 0; i < rowCount; ++i) {
        result[i] = decimal128Vector->GetValue(i);
    }
    timer.End();
    for (int32_t i = 0; i < rowCount; ++i) {
        Decimal128 expect(i);
        ASSERT_TRUE(result[i] == expect);
    }

    // original get value
    long value = 0;
    timer.Start("original get value");
    for (int32_t i = 0; i < rowCount; ++i) {
        value = *(decimal128Vector2 + i);
    }
    timer.End();
    value = value + 1;

    delete[] decimal128Vector2;
    delete vectorTest2;
    delete[] result;
}
} // namespace Decimal128VectorTest
