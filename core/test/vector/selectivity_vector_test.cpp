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

#include "vector/selectivity_vector.h"
#include "gtest/gtest.h"
#include "type/data_types.h"
#include "util/type_util.h"
#include "util/test_util.h"
namespace omniruntime::vec::test {
TEST(SelectivityVector, test_set_all_bits)
{
    constexpr size_t length = 10;
    SelectivityVector selectivityVector(length, true);
    EXPECT_EQ(true, selectivityVector.IsAllSelected());
}

TEST(SelectivityVector, test_init)
{
    constexpr size_t srcLength = 3000;
    constexpr size_t bitNum = srcLength * 8;
    uint8_t src[srcLength] = {0};
    for (size_t i = 0; i < srcLength; i++) {
        src[i] = 0b00001000;
    }
    SelectivityVector selectivityVector1(src, srcLength, bitNum);
    for (size_t i = 0; i < srcLength; i++) {
        if (i % 8 == 3) {
            EXPECT_EQ(selectivityVector1.IsValid(i), true);
        } else {
            EXPECT_EQ(selectivityVector1.IsValid(i), false);
        }
    }
    EXPECT_EQ(selectivityVector1.IsAllSelected(), false);
}

TEST(SelectivityVector, test_init1)
{
    constexpr size_t srcLength = 30000;
    constexpr size_t bitNum = srcLength * 8;
    uint8_t src[srcLength] = {0};
    for (size_t i = 0; i < srcLength; i++) {
        src[i] = 0b11111111;
    }
    SelectivityVector selectivityVector1(src, srcLength, bitNum);
    for (size_t i = 0; i < srcLength; i++) {
        EXPECT_EQ(selectivityVector1.IsValid(i), true);
    }
    EXPECT_EQ(selectivityVector1.IsAllSelected(), true);

    for (size_t i = 0; i < srcLength; i++) {
        src[i] = 0b01010101;
    }
    SelectivityVector selectivityVector2(src, srcLength, bitNum);
    for (size_t i = 0; i < srcLength; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(selectivityVector2.IsValid(i), true);
        } else {
            EXPECT_EQ(selectivityVector2.IsValid(i), false);
        }
    }
    EXPECT_EQ(selectivityVector2.IsAllSelected(), false);
}

TEST(SelectivityVector, test_set_all_bits1)
{
    constexpr size_t length = 10;
    SelectivityVector selectivityVector(length, false);
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
    selectivityVector.SetAll();
    EXPECT_EQ(true, selectivityVector.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(true, selectivityVector.IsValid(i));
    }
}
TEST(SelectivityVector, test_clear_all1)
{
    constexpr size_t length1 = 10;
    SelectivityVector selectivityVector1(length1);
    selectivityVector1.ClearAll();
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
    EXPECT_EQ(0, selectivityVector1.BeginPos());
    EXPECT_EQ(0, selectivityVector1.EndPos());
}

TEST(SelectivityVector, test_clear_all2)
{
    constexpr size_t length = 567;
    SelectivityVector selectivityVector(length, true);
    selectivityVector.ClearAll();
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(false, selectivityVector.IsValid(i));
    }
}

TEST(SelectivityVector, test_resize)
{
    constexpr size_t length1 = 10;
    constexpr size_t length2 = 30;
    constexpr size_t length3 = 50;
    SelectivityVector selectivityVector1;
    selectivityVector1.Resize(length1);
    EXPECT_EQ(true, selectivityVector1.IsAllSelected());
    EXPECT_EQ(length1, selectivityVector1.Size());
    selectivityVector1.Resize(length2);
    EXPECT_EQ(true, selectivityVector1.IsAllSelected());
    EXPECT_EQ(length2, selectivityVector1.Size());
    selectivityVector1.SetBit(3, false);
    selectivityVector1.Resize(length3);
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
    EXPECT_EQ(length3, selectivityVector1.Size());
}

TEST(SelectivityVector, test_set_bit)
{
    constexpr size_t length1 = 10;
    SelectivityVector selectivityVector1(length1);
    selectivityVector1.SetBit(2, false);
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
    EXPECT_EQ(false, selectivityVector1.IsValid(2));
}

TEST(SelectivityVector, test_set_all_bits_true_then_set_false1)
{
    constexpr size_t length = 10;
    SelectivityVector selectivityVector(length, true);
    selectivityVector.SetBit(2, false);
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
    EXPECT_EQ(false, selectivityVector.IsValid(2));
    EXPECT_EQ(true, selectivityVector.IsValid(3));
}

TEST(SelectivityVector, test_set_all_bits_false_then_set_true1)
{
    constexpr size_t length = 10;
    SelectivityVector selectivityVector(length, false);
    selectivityVector.SetBit(2, true);
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
    EXPECT_EQ(true, selectivityVector.IsValid(2));
    EXPECT_EQ(false, selectivityVector.IsValid(3));
}

TEST(SelectivityVector, test_set_all_bits_false_then_set_true2)
{
    constexpr size_t length = 10000;
    SelectivityVector selectivityVector(length, false);
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
    for (size_t i = 0; i < length; i += 2) {
        selectivityVector.SetBit(i, true);
    }
    for (size_t i = 0; i < length; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(true, selectivityVector.IsValid(i));
        } else {
            EXPECT_EQ(false, selectivityVector.IsValid(i));
        }
    }
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
}

TEST(SelectivityVector, test_set_all_bits_true_then_set_false2)
{
    constexpr size_t length = 10000;
    SelectivityVector selectivityVector(length, true);
    EXPECT_EQ(true, selectivityVector.IsAllSelected());
    for (size_t i = 0; i < length; i += 2) {
        selectivityVector.SetBit(i, false);
    }
    for (size_t i = 0; i < length; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(false, selectivityVector.IsValid(i));
        } else {
            EXPECT_EQ(true, selectivityVector.IsValid(i));
        }
    }
    EXPECT_EQ(false, selectivityVector.IsAllSelected());
}

TEST(SelectivityVector, test_and1)
{
    constexpr size_t length = 10;
    SelectivityVector selectivityVector1(length, true);
    SelectivityVector selectivityVector2(length, true);
    for (size_t i = 0; i < length; i += 2) {
        if (i % 2 == 0) {
            selectivityVector1.SetBit(i, false);
        }
    }
    selectivityVector2.And(selectivityVector1);
    for (size_t i = 0; i < length; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(false, selectivityVector2.IsValid(i));
        } else {
            EXPECT_EQ(true, selectivityVector2.IsValid(i));
        }
    }
    EXPECT_EQ(false, selectivityVector2.IsAllSelected());
}

TEST(SelectivityVector, test_and2)
{
    constexpr size_t length1 = 10;
    constexpr size_t length2 = 20;
    SelectivityVector selectivityVector1(length1, true);
    SelectivityVector selectivityVector2(length2, true);
    for (size_t i = 0; i < length1; i += 2) {
        if (i % 2 == 0) {
            selectivityVector1.SetBit(i, false);
        }
    }
    selectivityVector2.And(selectivityVector1);
    for (size_t i = 0; i < length1; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(false, selectivityVector2.IsValid(i));
        } else {
            EXPECT_EQ(true, selectivityVector2.IsValid(i));
        }
    }
    EXPECT_EQ(false, selectivityVector2.IsAllSelected());
    EXPECT_EQ(20, selectivityVector2.Size());
    EXPECT_EQ(1, selectivityVector2.BeginPos());
    EXPECT_EQ(20, selectivityVector2.EndPos());
}

TEST(SelectivityVector, test_and3)
{
    constexpr size_t length = 10;
    SelectivityVector selectivityVector1(length, true);
    SelectivityVector selectivityVector2(length, true);
    for (size_t i = 0; i < length; i += 2) {
        if (i % 2 == 0) {
            selectivityVector1.SetBit(i, false);
        }
    }
    SelectivityVector selectivityVector3;
    selectivityVector3.And(selectivityVector1, selectivityVector2);
    for (size_t i = 0; i < length; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(false, selectivityVector3.IsValid(i));
        } else {
            EXPECT_EQ(true, selectivityVector3.IsValid(i));
        }
    }
    EXPECT_EQ(false, selectivityVector3.IsAllSelected());
}

TEST(SelectivityVector, test_or1)
{
    constexpr size_t length = 1000;
    SelectivityVector selectivityVector1(length, true);
    SelectivityVector selectivityVector2(length, false);
    selectivityVector2.Or(selectivityVector1);
    EXPECT_EQ(true, selectivityVector2.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(true, selectivityVector2.IsValid(i));
        EXPECT_EQ(true, selectivityVector1.IsValid(i));
    }
}

TEST(SelectivityVector, test_or2)
{
    constexpr size_t length = 1000;
    SelectivityVector selectivityVector1(length, false);
    SelectivityVector selectivityVector2(length, false);
    selectivityVector2.Or(selectivityVector1);
    EXPECT_EQ(false, selectivityVector2.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(false, selectivityVector2.IsValid(i));
        EXPECT_EQ(false, selectivityVector1.IsValid(i));
    }
}

TEST(SelectivityVector, test_or3)
{
    constexpr size_t length = 1000;
    SelectivityVector selectivityVector1(length, true);
    SelectivityVector selectivityVector2(length, true);
    for (size_t i = 0; i < length; i++) {
        if (i % 3 == 0) {
            selectivityVector1.SetBit(i, false);
        }
    }
    selectivityVector1.Or(selectivityVector2);
    EXPECT_EQ(true, selectivityVector1.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(true, selectivityVector2.IsValid(i));
        EXPECT_EQ(true, selectivityVector1.IsValid(i));
    }
}

TEST(SelectivityVector, test_or4)
{
    constexpr size_t length = 10;
    bool setFlag1[length] = {false, true, false, false, true, false, false, true, true, false};
    bool setFlag2[length] = {true, true, false, false, false, false, false, false, false, false};
    bool resule[length] = {true, true, false, false, true, false, false, true, true, false};
    SelectivityVector selectivityVector1(length);
    SelectivityVector selectivityVector2(length);
    for (size_t i = 0; i < length; i++) {
        selectivityVector1.SetBit(i, setFlag1[i]);
        selectivityVector2.SetBit(i, setFlag2[i]);
    }
    selectivityVector1.Or(selectivityVector2);
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(resule[i], selectivityVector1.IsValid(i));
    }
}

TEST(SelectivityVector, test_or5)
{
    constexpr size_t length1 = 10;
    constexpr size_t length2 = 20;
    bool setFlag1[length1] = {false, true, false, false, true, false, false, true, true, false};
    bool setFlag2[length2] = {true, true, false, false, false, false, false, false, false, false,
                              true, true, false, false, false, false, false, false, false, false};
    bool resule[length2] = {true, true, false, false, true,  false, false, true,  true,  false,
                            true, true, false, false, false, false, false, false, false, false};
    SelectivityVector selectivityVector1(length1);
    SelectivityVector selectivityVector2(length2);
    for (size_t i = 0; i < length1; i++) {
        selectivityVector1.SetBit(i, setFlag1[i]);
    }
    for (size_t i = 0; i < length2; i++) {
        selectivityVector2.SetBit(i, setFlag2[i]);
    }
    selectivityVector1.Or(selectivityVector2);
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
    EXPECT_EQ(length2, selectivityVector1.Size());
    for (size_t i = 0; i < length2; i++) {
        EXPECT_EQ(resule[i], selectivityVector1.IsValid(i));
    }
}

TEST(SelectivityVector, test_or6)
{
    constexpr size_t length1 = 10;
    constexpr size_t length2 = 20;
    bool setFlag1[length1] = {false, true, false, false, true, false, false, true, true, false};
    bool setFlag2[length2] = {true, true, false, false, false, false, false, false, false, false,
                              true, true, false, false, false, false, false, true,  false, false};
    bool resule[length2] = {true, true, false, false, true,  false, false, true, true,  false,
                            true, true, false, false, false, false, false, true, false, false};
    SelectivityVector selectivityVector1(length1);
    SelectivityVector selectivityVector2(length2);
    for (size_t i = 0; i < length1; i++) {
        selectivityVector1.SetBit(i, setFlag1[i]);
    }
    for (size_t i = 0; i < length2; i++) {
        selectivityVector2.SetBit(i, setFlag2[i]);
    }
    selectivityVector2.Or(selectivityVector1);
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
    EXPECT_EQ(length2, selectivityVector2.Size());
    for (size_t i = 0; i < length2; i++) {
        EXPECT_EQ(resule[i], selectivityVector2.IsValid(i));
    }
}

TEST(SelectivityVector, test_or7)
{
    constexpr size_t length = 10;
    bool setFlag1[length] = {false, true, false, false, true, false, false, true, true, false};
    bool setFlag2[length] = {true, true, false, false, false, false, false, false, false, false};
    bool resule[length] = {true, true, false, false, true, false, false, true, true, false};
    SelectivityVector selectivityVector1(length);
    SelectivityVector selectivityVector2(length);
    for (size_t i = 0; i < length; i++) {
        selectivityVector1.SetBit(i, setFlag1[i]);
        selectivityVector2.SetBit(i, setFlag2[i]);
    }
    SelectivityVector selectivityVector3;
    selectivityVector3.Or(selectivityVector1, selectivityVector2);
    EXPECT_EQ(false, selectivityVector3.IsAllSelected());
    for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(resule[i], selectivityVector3.IsValid(i));
    }
}

TEST(SelectivityVector, test_or8)
{
    constexpr size_t length1 = 10;
    constexpr size_t length2 = 20;
    SelectivityVector selectivityVector1(length1);
    SelectivityVector selectivityVector2(length2);
    for (size_t i = 0; i < length1; i++) {
        if (i % 2 == 0) {
            selectivityVector1.SetBit(i, false);
        }
    }
    for (size_t i = 0; i < length1; i++) {
        if (i % 2 != 0) {
            selectivityVector2.SetBit(i, false);
        }
    }
    selectivityVector2.Or(selectivityVector1);
    EXPECT_EQ(true, selectivityVector2.IsAllSelected());
}

TEST(SelectivityVector, test_not1)
{
    constexpr size_t length1 = 1000;
    SelectivityVector selectivityVector1(length1);
    for (size_t i = 0; i < length1; i++) {
        if (i % 2 == 0) {
            selectivityVector1.SetBit(i, false);
        }
    }
    selectivityVector1.Not();
    for (size_t i = 0; i < length1; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(selectivityVector1.IsValid(i), true);
        } else {
            EXPECT_EQ(selectivityVector1.IsValid(i), false);
        }
    }
    EXPECT_EQ(false, selectivityVector1.IsAllSelected());
}

TEST(SelectivityVector, test_not2)
{
    constexpr size_t length1 = 1000;
    SelectivityVector selectivityVector1(length1);
    for (size_t i = 0; i < length1; i++) {
        selectivityVector1.SetBit(i, false);
    }
    selectivityVector1.Not();
    for (size_t i = 0; i < length1; i++) {
        EXPECT_EQ(selectivityVector1.IsValid(i), true);
    }
    EXPECT_EQ(true, selectivityVector1.IsAllSelected());
}

TEST(SelectivityVector, test_convert_from_vectorbatch_and1)
{
    constexpr int32_t dataSize = 5;
    int32_t data1[dataSize] = {4, 3, 2, 1, 0};
    int64_t data2[dataSize] = {0, 1, 2, 3, 4};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType()};
    VectorBatch *vecBatch = omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    vecBatch->Get(0)->SetNull(1);
    vecBatch->Get(0)->SetNull(3);
    SelectivityVector result(dataSize, true);
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::AND>(vecBatch, result);
    EXPECT_EQ(false, result.IsAllSelected());
    EXPECT_EQ(true, result.IsValid(0));
    EXPECT_EQ(false, result.IsValid(1));
    EXPECT_EQ(true, result.IsValid(2));
    EXPECT_EQ(false, result.IsValid(3));
    EXPECT_EQ(true, result.IsValid(4));
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(SelectivityVector, test_convert_from_vectorbatch_and2)
{
    constexpr int32_t dataSize = 10000;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int64_t[dataSize];
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType()};
    VectorBatch *vecBatch = omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            vecBatch->Get(0)->SetNull(i);
        }
    }
    SelectivityVector result(dataSize, true);
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::AND>(vecBatch, result);
    EXPECT_EQ(false, result.IsAllSelected());
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(false, result.IsValid(i));
        } else {
            EXPECT_EQ(true, result.IsValid(i));
        }
    }
    VectorHelper::FreeVecBatch(vecBatch);
    delete[] data1;
    delete[] data2;
}

TEST(SelectivityVector, test_convert_from_vectorbatch_and3)
{
    constexpr int32_t dataSize = 10000;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int64_t[dataSize];
    auto *data3 = new double[dataSize];
    auto *data4 = new int64_t[dataSize];
    auto *data5 = new int64_t[dataSize];
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
        data3[i] = i;
        data4[i] = i;
        data5[i] = i;
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType(), DoubleType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType(), LongType(), DoubleType(), LongType()};
    VectorBatch *vecBatch =
        omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            vecBatch->Get(0)->SetNull(i);
        }
        if (i % 3 == 0) {
            vecBatch->Get(2)->SetNull(i);
        }
        if (i % 7 == 0) {
            vecBatch->Get(4)->SetNull(i);
        }
    }
    SelectivityVector result(dataSize, true);
    std::vector<size_t> selectColums = {0, 2, 4};
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::AND>(vecBatch, selectColums,
                                                                                            result);
    EXPECT_EQ(false, result.IsAllSelected());
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0 || i % 3 == 0 || i % 7 == 0) {
            EXPECT_EQ(false, result.IsValid(i));
        } else {
            EXPECT_EQ(true, result.IsValid(i));
        }
    }
    VectorHelper::FreeVecBatch(vecBatch);
    delete[] data1;
    delete[] data2;
    delete[] data3;
    delete[] data4;
    delete[] data5;
}

TEST(SelectivityVector, test_convert_from_vectorbatch_or1)
{
    constexpr int32_t dataSize = 5;
    int32_t data1[dataSize] = {4, 3, 2, 1, 0};
    int64_t data2[dataSize] = {0, 1, 2, 3, 4};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType()};
    VectorBatch *vecBatch = omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    vecBatch->Get(0)->SetNull(1);
    vecBatch->Get(0)->SetNull(3);
    SelectivityVector result(dataSize, true);
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::OR>(vecBatch, result);
    EXPECT_EQ(true, result.IsAllSelected());
    EXPECT_EQ(true, result.IsValid(0));
    EXPECT_EQ(true, result.IsValid(1));
    EXPECT_EQ(true, result.IsValid(2));
    EXPECT_EQ(true, result.IsValid(3));
    EXPECT_EQ(true, result.IsValid(4));
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(SelectivityVector, test_convert_from_vectorbatch_or2)
{
    constexpr int32_t dataSize = 5;
    int32_t data1[dataSize] = {4, 3, 2, 1, 0};
    int64_t data2[dataSize] = {0, 1, 2, 3, 4};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType()};
    VectorBatch *vecBatch = omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    vecBatch->Get(0)->SetNull(1);
    vecBatch->Get(1)->SetNull(1);
    vecBatch->Get(0)->SetNull(2);
    vecBatch->Get(0)->SetNull(3);
    vecBatch->Get(1)->SetNull(3);
    SelectivityVector result(dataSize, false);
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::OR>(vecBatch, result);
    EXPECT_EQ(false, result.IsAllSelected());
    EXPECT_EQ(true, result.IsValid(0));
    EXPECT_EQ(false, result.IsValid(1));
    EXPECT_EQ(true, result.IsValid(2));
    EXPECT_EQ(false, result.IsValid(3));
    EXPECT_EQ(true, result.IsValid(4));
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(SelectivityVector, test_convert_from_vectorbatch_or3)
{
    constexpr int32_t dataSize = 10000;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int64_t[dataSize];
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType()};
    VectorBatch *vecBatch = omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            vecBatch->Get(0)->SetNull(i);
        }
    }
    SelectivityVector result(dataSize, false);
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::OR>(vecBatch, result);
    EXPECT_EQ(true, result.IsAllSelected());
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(true, result.IsValid(i));
        } else {
            EXPECT_EQ(true, result.IsValid(i));
        }
    }
    VectorHelper::FreeVecBatch(vecBatch);
    delete[] data1;
    delete[] data2;
}

TEST(SelectivityVector, test_convert_from_vectorbatch_or4)
{
    constexpr int32_t dataSize = 10000;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int64_t[dataSize];
    auto *data3 = new double[dataSize];
    auto *data4 = new int64_t[dataSize];
    auto *data5 = new int64_t[dataSize];
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
        data3[i] = i;
        data4[i] = i;
        data5[i] = i;
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType(), DoubleType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType(), LongType(), DoubleType(), LongType()};
    VectorBatch *vecBatch =
        omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            vecBatch->Get(0)->SetNull(i);
        }
        if (i % 3 == 0) {
            vecBatch->Get(2)->SetNull(i);
        }
        if (i % 7 == 0) {
            vecBatch->Get(4)->SetNull(i);
        }
    }
    SelectivityVector result(dataSize, false);
    std::vector<size_t> selectColums = {0, 2, 4};
    SelectivityVector::GetSelectivityVectorFromVectorBatch<GetSelectivityVectorMethod::OR>(vecBatch, selectColums,
                                                                                           result);
    EXPECT_EQ(false, result.IsAllSelected());
    for (size_t i = 0; i < dataSize; i++) {
        if (i % 2 == 0 && i % 3 == 0 && i % 7 == 0) {
            EXPECT_EQ(false, result.IsValid(i));
        } else {
            EXPECT_EQ(true, result.IsValid(i));
        }
    }
    VectorHelper::FreeVecBatch(vecBatch);
    delete[] data1;
    delete[] data2;
    delete[] data3;
    delete[] data4;
    delete[] data5;
}

TEST(SelectivityVector, test_gather_result_1)
{
    constexpr int32_t dataSize = 20;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int64_t[dataSize];
    auto *data3 = new double[dataSize];
    auto *data4 = new int64_t[dataSize];
    auto *data5 = new int64_t[dataSize];
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
        data3[i] = i;
        data4[i] = i;
        data5[i] = i;
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType(), DoubleType(), LongType()}));
    std::vector<DataTypePtr> typess = {IntType(), LongType(), LongType(), DoubleType(), LongType()};
    VectorBatch *vecBatch =
        omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);
    std::vector<BaseVector *> src(5, nullptr);
    for (int i = 0; i < 5; i++) {
        src[i] = vecBatch->Get(i);
    }
    std::vector<BaseVector *> result;
    SelectivityVector selectivityVector(dataSize, true);
    for (int i = 0; i < dataSize; i++) {
        if (i % 2 != 0) {
            selectivityVector.SetBit(i, false);
        }
    }
    bool hasFiltered = SelectivityVector::GetFlatBaseVectorsFromSelectivityVector(src, selectivityVector, result);
    EXPECT_TRUE(hasFiltered);
    constexpr int32_t resulrSize = 10;
    auto *result1 = new int32_t[resulrSize];
    auto *result2 = new int64_t[resulrSize];
    auto *result3 = new double[resulrSize];
    auto *result4 = new int64_t[resulrSize];
    auto *result5 = new int64_t[resulrSize];
    for (int i = 0; i < dataSize; i++) {
        if (i % 2 == 0) {
            result1[i / 2] = data1[i];
            result2[i / 2] = data2[i];
            result3[i / 2] = data3[i];
            result4[i / 2] = data4[i];
            result5[i / 2] = data5[i];
        }
    }
    VectorBatch *expectedVectorBatch =
        omniruntime::TestUtil::CreateVectorBatch(sourceTypes, resulrSize, result1, result2, result3, result4, result5);
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(TestUtil::ColumnMatch(result[i], expectedVectorBatch->Get(i)));
    }
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(expectedVectorBatch);
    for (int i = 0; i < result.size(); i++) {
        delete (result[i]);
    }
    delete[] data1;
    delete[] data2;
    delete[] data3;
    delete[] data4;
    delete[] data5;
    delete[] result1;
    delete[] result2;
    delete[] result3;
    delete[] result4;
    delete[] result5;
}

TEST(SelectivityVector, test_gather_result_2)
{
    constexpr int32_t dataSize = 20;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int16_t[dataSize];
    auto *data3 = new double[dataSize];
    auto *data4 = new int64_t[dataSize];
    std::string data5[dataSize] = {""};
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
        data3[i] = i / 1.1;
        data4[i] = i;
        data5[i] = std::to_string(i) + "test_gather_result_2";
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), ShortType(), DoubleType(), LongType(), CharType(20)}));
    VectorBatch *vecBatch =
        omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);
    std::vector<BaseVector *> src(5, nullptr);
    for (int i = 0; i < 5; i++) {
        src[i] = vecBatch->Get(i);
    }
    std::vector<BaseVector *> result;
    int countSelected = 0;
    SelectivityVector selectivityVector(dataSize, true);
    for (int i = 0; i < dataSize; i++) {
        if (i % 3 == 0) {
            selectivityVector.SetBit(i, false);
        } else {
            countSelected++;
        }
    }
    bool hasFiltered = SelectivityVector::GetFlatBaseVectorsFromSelectivityVector(src, selectivityVector, result);
    EXPECT_TRUE(hasFiltered);
    auto *result1 = new int32_t[countSelected];
    auto *result2 = new int16_t[countSelected];
    auto *result3 = new double[countSelected];
    auto *result4 = new int64_t[countSelected];
    std::string result5[dataSize];
    for (int i = 0, k = 0; i < dataSize; i++) {
        if (i % 3 != 0) {
            result1[k] = data1[i];
            result2[k] = data2[i];
            result3[k] = data3[i];
            result4[k] = data4[i];
            result5[k] = data5[i];
            k++;
        }
    }
    VectorBatch *expectedVectorBatch = omniruntime::TestUtil::CreateVectorBatch(sourceTypes, countSelected, result1,
                                                                                result2, result3, result4, result5);
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(TestUtil::ColumnMatch(result[i], expectedVectorBatch->Get(i)));
    }
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(expectedVectorBatch);
    for (int i = 0; i < result.size(); i++) {
        delete (result[i]);
    }
    delete[] data1;
    delete[] data2;
    delete[] data3;
    delete[] data4;
    delete[] result1;
    delete[] result2;
    delete[] result3;
    delete[] result4;
}

TEST(SelectivityVector, test_gather_result_3)
{
    constexpr int32_t dataSize = 20;
    auto *data1 = new int32_t[dataSize];
    auto *data2 = new int16_t[dataSize];
    auto *data3 = new double[dataSize];
    auto *data4 = new int64_t[dataSize];
    std::string data5[dataSize] = {""};
    for (size_t i = 0; i < dataSize; i++) {
        data1[i] = i;
        data2[i] = i;
        data3[i] = i / 1.1;
        data4[i] = i;
        data5[i] = std::to_string(i) + "test_gather_result_2";
    }

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), ShortType(), DoubleType(), LongType(), CharType(20)}));
    VectorBatch *vecBatch =
        omniruntime::TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);
    std::vector<BaseVector *> src(5, nullptr);
    for (int i = 0; i < 5; i++) {
        src[i] = vecBatch->Get(i);
    }
    std::vector<BaseVector *> result;
    int countSelected = 0;
    SelectivityVector selectivityVector(dataSize, true);
    bool hasFiltered = SelectivityVector::GetFlatBaseVectorsFromSelectivityVector(src, selectivityVector, result);
    EXPECT_FALSE(hasFiltered);
    EXPECT_EQ(result.size(), 0);
    VectorHelper::FreeVecBatch(vecBatch);
    for (int i = 0; i < result.size(); i++) {
        delete (result[i]);
    }
    delete[] data1;
    delete[] data2;
    delete[] data3;
    delete[] data4;
}
} // namespace omniruntime::vec::test
