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

namespace ContainerVectorTest {
const int32_t POSITION_COUNT = 100;
const int32_t VECTOR_COUNT = 2;
TEST(ContainerVector, setAndGetValue)
{
    auto doubleVector = std::make_unique<Vector<double>>(POSITION_COUNT);
    auto longVector = std::make_unique<Vector<int64_t>>(POSITION_COUNT);
    std::vector<int64_t> vectorAddresses(2);
    vectorAddresses[0] = reinterpret_cast<int64_t>(doubleVector.get());
    vectorAddresses[1] = reinterpret_cast<int64_t>(longVector.get());
    std::vector<DataTypePtr> VECTOR_TYPES = {DoubleType(), LongType()};
    auto *vector = new ContainerVector(POSITION_COUNT, vectorAddresses, VECTOR_TYPES);
    BaseVector *values[] = {new Vector<double>(POSITION_COUNT), new Vector<int64_t>(POSITION_COUNT)};
    for (int i = 0; i < VECTOR_COUNT; ++i) {
        vector->SetValue(i, reinterpret_cast<int64_t>(values[i]));
    }

    for (int i = 0; i < VECTOR_COUNT; ++i) {
        EXPECT_EQ(vector->GetValue(i), reinterpret_cast<int64_t>(values[i]));
    }

    delete vector;
}

TEST(ContainerVector, testNullFlagWithSet)
{
    int rows = 10;
    auto intVector = new Vector<int32_t>(rows);
    auto longVector = new Vector<int64_t>(rows);

    std::vector<int64_t> subAddrs = {reinterpret_cast<int64_t>(intVector), reinterpret_cast<int64_t>(longVector)};
    std::vector<DataTypePtr> subTypes = {IntType(), LongType()};
    auto *hasNulls = new ContainerVector(rows, subAddrs, subTypes);

    std::vector<bool> nulls = {true, false, true, false, true, false, true, false, true, false};
    for (int32_t i = 0; i < rows; ++i) {
        if (nulls[i]) {
            hasNulls->SetNull(i);
        }
    }
    EXPECT_TRUE(hasNulls->HasNull());

    delete hasNulls;
}

TEST(ContainerVector, jniFreeVector)
{
    auto doubleVector = new Vector<double>(POSITION_COUNT);
    auto longVector = new Vector<int64_t>(POSITION_COUNT);

    std::vector<int64_t> vectorAddresses(2);
    vectorAddresses[0] = reinterpret_cast<int64_t>(doubleVector);
    vectorAddresses[1] = reinterpret_cast<int64_t>(longVector);
    std::vector<DataTypePtr> VECTOR_TYPES = {DoubleType(), LongType()};
    auto *vector = new ContainerVector(POSITION_COUNT, vectorAddresses, VECTOR_TYPES);
    auto *vec = (BaseVector *)vector;

    delete vec;
}

TEST(ContainerVector, appendVector)
{
    int32_t rows = 5;
    int64_t data[5] = {1, 2, 3, 4, 5};
    auto *src1 = new Vector<int64_t>(5);
    src1->SetValues(0, data, 5);

    double data1[5] = {1.1, 2.2, 3.3, 4.4, 5.5};
    auto *doubleVector = new Vector<double>(rows);
    doubleVector->SetValues(0, data1, 5);

    std::vector<int64_t> vectorAddresses = {reinterpret_cast<int64_t>(doubleVector), reinterpret_cast<int64_t>(src1)};
    std::vector<DataTypePtr> vectorTypes = {DoubleType(), LongType()};
    auto *vector = new ContainerVector(rows, vectorAddresses, vectorTypes);

    auto *src2 = new Vector<int64_t>(rows);
    int64_t data2[5] = {6, 7, 8, 9, 10};
    src2->SetValues(0, data2, rows);
    double data22[5] = {6.6, 7.7, 8.8, 9.9, 10.1};
    auto *doubleVector1 = new Vector<double>(rows);
    doubleVector1->SetValues(0, data22, rows);

    std::vector<int64_t> vecAddr2 = {reinterpret_cast<int64_t>(doubleVector1), reinterpret_cast<int64_t>(src2)};
    std::vector<DataTypePtr> dataTypes2 = {DoubleType(), LongType()};
    auto *vector2 = new ContainerVector(rows, vecAddr2, dataTypes2);

    auto *appendedDouble = new Vector<double>(10);
    auto *appendedLong = new Vector<int64_t>(10);
    std::vector<int64_t> appendedAddr = {reinterpret_cast<int64_t>(appendedDouble),
                                         reinterpret_cast<int64_t>(appendedLong)};
    std::vector<DataTypePtr> appendedDataType = {DoubleType(), LongType()};
    auto *appended = new ContainerVector(rows * 2, appendedAddr, appendedDataType);
    appended->Append(vector, 0, 5);
    appended->Append(vector2, 5, 5);

    double expected[] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.1};
    AssertDoubleVectorEquals(reinterpret_cast<Vector<double> *>(appended->GetValue(0)), expected);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(reinterpret_cast<Vector<int64_t> *>(appended->GetValue(1))->GetValue(i), i + 1);
    }

    delete vector;
    delete vector2;
    delete appended;
}

TEST(ContainerVector, copyPositions)
{
    // TODO: add UT after container vector supports this function.
}

TEST(ContainerVector, sliceVector)
{
    // TODO: add UT after container vector supports this function.
}
} // namespace ContainerVectorTest
