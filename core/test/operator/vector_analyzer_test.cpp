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

#include <vector>
#include "gtest/gtest.h"
#include "operator/hashmap/vector_analyzer.h"
#include "util/test_util.h"
#include "vector/vector_helper.h"
#include "vector/unsafe_vector.h"

using namespace std;
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::TestUtil;

namespace VectorAnalyzerTest {

const int32_t DATA_SIZE = 3;

TEST(VectorAnalyzerTest, TestHashMode)
{
    ConfigUtil::SetAggHashTableRule(AggHashTableRule::ARRAY);
    int32_t data01[DATA_SIZE] = {0, 1, 2};      // int
    double data02[DATA_SIZE] = {6.6, 5.5, 4.4}; // double
    int64_t data03[DATA_SIZE] = {1, 2, 3};      // long
    int64_t data04[DATA_SIZE] = {11, 22, 33};   // decimal64

    // check data type
    // two columnar not support
    std::vector<DataTypePtr> types1 = {IntType(), DoubleType()};
    DataTypes sourceTypes1(types1);
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes1, DATA_SIZE, data01, data02);
    std::vector<ColumnIndex> groupByIndex1(2, ColumnIndex());
    groupByIndex1[0] = {0, IntType(), IntType()};
    groupByIndex1[1] = {1, DoubleType(), DoubleType()};
    auto vector_analyzer1 = VectorAnalyzer(groupByIndex1);
    EXPECT_FALSE(vector_analyzer1.DecideHashMode(vecBatch1));
    EXPECT_FALSE(vector_analyzer1.IsArrayHashTableType());
    VectorHelper::FreeVecBatch(vecBatch1);

    // double data type not support
    std::vector<DataTypePtr> types2 = {DoubleType()};
    DataTypes sourceTypes2(types2);
    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes2, DATA_SIZE, data02);
    std::vector<ColumnIndex> groupByIndex2(1, ColumnIndex());
    groupByIndex2[0] = {0, DoubleType(), DoubleType()};
    auto vector_analyzer2 = VectorAnalyzer(groupByIndex2);
    EXPECT_FALSE(vector_analyzer2.DecideHashMode(vecBatch2));
    EXPECT_FALSE(vector_analyzer2.IsArrayHashTableType());
    VectorHelper::FreeVecBatch(vecBatch2);

    // long data type support
    std::vector<DataTypePtr> types3 = {LongType()};
    DataTypes sourceTypes3(types3);
    VectorBatch *vecBatch3 = CreateVectorBatch(sourceTypes3, DATA_SIZE, data03);
    std::vector<ColumnIndex> groupByIndex3(1, ColumnIndex());
    groupByIndex3[0] = {0, LongType(), LongType()};
    auto vector_analyzer3 = VectorAnalyzer(groupByIndex3);
    EXPECT_TRUE(vector_analyzer3.DecideHashMode(vecBatch3));
    EXPECT_TRUE(vector_analyzer3.IsArrayHashTableType());
    VectorHelper::FreeVecBatch(vecBatch3);

    // decimal64 data type support
    std::vector<DataTypePtr> types4 = {Decimal64Type(10, 2)};
    DataTypes sourceTypes4(types4);
    VectorBatch *vecBatch4 = CreateVectorBatch(sourceTypes4, DATA_SIZE, data04);
    std::vector<ColumnIndex> groupByIndex4(1, ColumnIndex());
    groupByIndex4[0] = {0, Decimal64Type(), Decimal64Type()};
    auto vector_analyzer4 = VectorAnalyzer(groupByIndex4);
    EXPECT_TRUE(vector_analyzer4.DecideHashMode(vecBatch4));
    EXPECT_TRUE(vector_analyzer4.IsArrayHashTableType());
    VectorHelper::FreeVecBatch(vecBatch4);
}

} // namespace VectorAnalyzerTest
