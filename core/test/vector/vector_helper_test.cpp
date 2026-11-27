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

namespace VectorHelperTest {
TEST(VectorHelper, setAndGetValue)
{
    // int32 test
    auto vector1 = std::make_unique<Vector<int32_t>>(10);
    int32_t value1 = 100;
    VectorHelper::SetValue(vector1.get(), 5, &value1);
    EXPECT_EQ(vector1->GetValue(5), value1);

    // int64 test
    auto vector2 = std::make_unique<Vector<int64_t>>(10);
    int64_t value2 = 1000;
    VectorHelper::SetValue(vector2.get(), 5, &value2);
    EXPECT_EQ(vector2->GetValue(5), value2);

    // int16 test
    auto vector3 = std::make_unique<Vector<int16_t>>(10);
    int16_t value3 = 100;
    VectorHelper::SetValue(vector3.get(), 5, &value3);
    EXPECT_EQ(vector3->GetValue(5), value3);

    // double test
    auto vector4 = std::make_unique<Vector<double>>(10);
    double value4 = 33.333;
    VectorHelper::SetValue(vector4.get(), 5, &value4);
    EXPECT_EQ(vector4->GetValue(5), value4);

    // boolean test
    auto vector5 = std::make_unique<Vector<bool>>(10);
    bool value5 = true;
    VectorHelper::SetValue(vector5.get(), 5, &value5);
    EXPECT_EQ(vector5->GetValue(5), value5);

    // varchar test
    auto vector6 = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(10);
    std::string value6 = "testvectorhelper";
    VectorHelper::SetValue(vector6.get(), 5, &value6);
    EXPECT_EQ(vector6->GetValue(5), value6);

    // decimal test
    auto vector7 = std::make_unique<Vector<Decimal128>>(10);
    Decimal128 value7(111, 222);
    VectorHelper::SetValue(vector7.get(), 5, &value7);
    EXPECT_EQ(vector7->GetValue(5), value7);
}

TEST(VectorHelper, printVectorValue)
{
    int32_t value_size = 10;
    auto varcharVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(value_size);
    std::string value = "testvectorhelper";
    VectorHelper::SetValue(varcharVec.get(), 5, &value);
    for (int32_t rowIndex = 0; rowIndex < value_size; ++rowIndex) {
        VectorHelper::PrintVectorValue(varcharVec.get(), rowIndex);
    }
}
} // namespace VectorHelperTest
