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
#include <iostream>
#include "gtest/gtest.h"
#include "vector/omni_row.h"
#include "test/util/test_util.h"
#include "operator/hash_util.h"

namespace omniruntime::vec::test {
using namespace omniruntime::vec;
using namespace omniruntime::TestUtil;
TEST(omni_row, compact_value_test)
{
    Vector<int16_t> shortVec(1);
    shortVec.SetValue(0, 10);
    SerializedValue<int16_t> value;
    value.TransValue(&shortVec, 0);
    EXPECT_EQ(value.CompactLength(), 1 + 1);
}

TEST(omni_row, compact_set_value)
{
    int32_t value = static_cast<int32_t>(INT16_MAX) + 1;
    SerializedValue<int32_t> serializedValue;
    serializedValue.SetValue(value);
    EXPECT_EQ(serializedValue.CompactLength(), 1 + 3);
}

TEST(omni_row, compact_set_null)
{
    SerializedValue<int32_t> serializedValue;
    serializedValue.SetNull();
    EXPECT_EQ(serializedValue.CompactLength(), 1);
}

TEST(omni_row, compact_set_string_null)
{
    SerializedValue<std::string_view> serializedValue;
    serializedValue.SetNull();
    EXPECT_EQ(serializedValue.CompactLength(), 1);
}

TEST(omni_row, compact_set_string)
{
    SerializedValue<std::string_view> serializedValue;
    std::string_view testStr("test", 4);
    serializedValue.SetValue(testStr);
    EXPECT_EQ(serializedValue.CompactLength(), 1 + 1 + 4);
}

TEST(omni_row, compact_set_negative_value)
{
    int32_t value = -1024;
    SerializedValue<int32_t> serializedValue;
    serializedValue.SetValue(value);
    EXPECT_EQ(serializedValue.CompactLength(), 1 + 2);
}

TEST(omni_row, null_write_buffer)
{
    SerializedValue<int32_t> serializedValue;
    serializedValue.SetNull();
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b01000000);
}

TEST(omni_row, int_write_buffer)
{
    int32_t value = -1024;
    SerializedValue<int32_t> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b00100010);
    // truncate value
    int16_t ret = ~value;
    EXPECT_EQ(*(reinterpret_cast<int16_t *>(buffer + 1)), ret);
}

TEST(omni_row, short_write_buffer)
{
    int16_t value = 125;
    SerializedValue<int16_t> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b00000001);
    // truncate value
    int8_t ret = value;
    EXPECT_EQ(*(reinterpret_cast<int8_t *>(buffer + 1)), ret);
}

TEST(omni_row, double_write_buffer)
{
    double value = -3.1415926f;
    SerializedValue<double> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b00001000);
    // truncate value
    EXPECT_EQ(*(reinterpret_cast<double *>(buffer + 1)), value);
}

TEST(omni_row, bool_write_buffer)
{
    bool value = true;
    SerializedValue<bool> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b00000001);
}

TEST(omni_row, decimal128_write_buffer)
{
    type::Decimal128 value{123, 123};
    SerializedValue<type::Decimal128> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b00010000);
    EXPECT_EQ(*(reinterpret_cast<Decimal128 *>(buffer + 1)), value);
}

TEST(omni_row, str_write_buffer)
{
    std::string_view value{"hello zy", 8};
    SerializedValue<std::string_view> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b10000001);
    EXPECT_EQ(buffer[1], 8);
    EXPECT_TRUE(memcmp(value.data(), buffer + 2, value.size()) == 0);
}

TEST(omni_row, fill_buffer_no_null)
{
    std::vector<DataTypePtr> types({IntDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance()});
    RowBuffer rowBuffer(types);
    std::string_view testStr("hello world", 11);
    auto *buffer = rowBuffer.GetOneOfRow(0);
    auto *intValue = reinterpret_cast<SerializedValue<int32_t> *>(buffer);
    intValue->SetValue(129);
    buffer = rowBuffer.GetOneOfRow(1);
    auto *doubleValue = reinterpret_cast<SerializedValue<double> *>(buffer);
    double ori = 3.1415926;
    doubleValue->SetValue(ori);
    buffer = rowBuffer.GetOneOfRow(2);
    auto *strValue = reinterpret_cast<SerializedValue<std::string_view> *>(buffer);
    strValue->SetValue(testStr);
    auto len = rowBuffer.FillBuffer();
    auto *buf = rowBuffer.GetRowBuffer();

    EXPECT_EQ(buf[0], 0b00000010);
    int16_t data = (int32_t)buf[1];
    EXPECT_EQ(data, 129);

    EXPECT_EQ(buf[3], 0b00001000);
    double d = *(double *)(buf + 4);

    EXPECT_TRUE(memcmp(reinterpret_cast<void *>(&d), reinterpret_cast<void *>(&ori), sizeof(double)) == 0);
    EXPECT_EQ(buf[12], 0b10000001);
    EXPECT_EQ(buf[13], 11);
    EXPECT_TRUE(memcmp(reinterpret_cast<void *>(buf + 14), testStr.data(), testStr.length()) == 0);
    mem::Allocator::GetAllocator()->Free(buf, len);
}

TEST(omni_row, fill_buffer_and_deserial_to_vector)
{
    std::vector<DataTypePtr> types({LongDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance(),
                                    BooleanDataType::Instance()});
    RowBuffer rowBuffer(types);
    int32_t rowNumber = 5;
    int64_t data1[] = {-111, 222, 333, 444, -555};
    double data2[] = {999.99f, 999.0f, 999.999f, 99.96f, 999.99999f};
    std::string data3[] = {"Asleep, high machines shall no", "Asleep, indian sciences may in",
                           "As junior schools love simply.", "A", "Ab"};
    bool data4[] = {true, false, false, true, true};

    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1, data2, data3, data4);
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        rowBuffer.TransValueFromVectorBatch(vecBatch, i);
        auto len = rowBuffer.FillBuffer();
        rows.emplace_back(rowBuffer.TakeRowBuffer(), len);
    }

    auto parser = std::make_unique<RowParser>(types);

    // fill fake data into result vector batch before parse function
    int64_t fakeData1[] = {0, 0, 0, 0, 0};
    double fakeData2[] = {0.1f, 0.1f, 0.1f, 0.2f, 0.3f};
    std::string fakeData3[] = {"a", "b", "c", "d", "e"};
    bool fakeData4[] = {false, false, false, false, false};

    VectorBatch *result = CreateVectorBatch(dataTypes, rowNumber, fakeData1, fakeData2, fakeData3, fakeData4);
    BaseVector *vecs[types.size()];
    for (int32_t i = 0; i < static_cast<int32_t>(types.size()); ++i) {
        vecs[i] = result->Get(i);
    }

    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        parser->ParseOneRow(rows[i].row, vecs, i);
    }

    // after parse, result should be the same as vecbatch
    EXPECT_TRUE(VecBatchMatch(result, vecBatch));
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(result);
}

TEST(omni_row, fill_buffer_and_check_hash)
{
    std::vector<DataTypePtr> types({LongDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance()});
    RowBuffer rowBuffer(types, 2);
    int32_t rowNumber = 5;
    int64_t data1[] = {111, 222, 333, 444, 555};
    double data2[] = {999.99f, 999.0f, 999.999f, 99.96f, 999.99999f};
    std::string data3[] = {"Asleep, high machines shall no", "Asleep, indian sciences may in",
                           "As junior schools love simply.", "A", "Ab"};

    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1, data2, data3);
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        rowBuffer.TransValueFromVectorBatch(vecBatch, i);
        auto len = rowBuffer.FillBuffer();
        rows.emplace_back(rowBuffer.GetRowBuffer(), len);
        // 3.get hash position for shuffle
        int32_t hashPos = rowBuffer.CalculateHashPos();

        auto *buffer = rowBuffer.GetRowBuffer();

        auto hashVal = op::HashUtil::HashValue((int8_t *)buffer, hashPos);
        std::cout << "test calculate hash " << hashVal << std::endl;
    }
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(omni_row, fill_buffer_performance)
{
    auto t = Timer();
    t.Start("test generate vectorBatch(5 row * 3col) time:");
    std::vector<DataTypePtr> types({LongDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance()});
    RowBuffer rowBuffer(types, 2);
    int32_t rowNumber = 5;
    std::vector<int64_t> data1(rowNumber);
    std::vector<double> data2(rowNumber);
    std::vector<std::string> data3(rowNumber);
    for (int i = 0; i < rowNumber; i++) {
        data1[i] = i;
        data2[i] = (i * 0.1f);
        data3[i] = ("lala" + std::to_string(i));
    }

    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1.data(), data2.data(), data3.data());
    t.End();
    t.Start("tran vec to row time (5 row * 3 col):");
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        rowBuffer.TransValueFromVectorBatch(vecBatch, i);
        auto len = rowBuffer.FillBuffer();
        rows.emplace_back(rowBuffer.TakeRowBuffer(), len);
    }
    t.End();
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(omni_row, fill_bool_buffer_and_deserial_to_vector)
{
    std::vector<DataTypePtr> types({BooleanDataType::Instance()});
    RowBuffer rowBuffer(types);
    int32_t rowNumber = 5;
    bool data1[] = {true, false, true, true, true};

    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1);
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        rowBuffer.TransValueFromVectorBatch(vecBatch, i);
        auto len = rowBuffer.FillBuffer();
        rows.emplace_back(rowBuffer.TakeRowBuffer(), len);
    }

    auto parser = std::make_unique<RowParser>(types);

    // fill fake data into result vector batch before parse function
    bool fakedata1[] = {false, false, false, false, false};

    VectorBatch *result = CreateVectorBatch(dataTypes, rowNumber, fakedata1);
    BaseVector *vecs[types.size()];
    for (int32_t i = 0; i < static_cast<int32_t>(types.size()); ++i) {
        vecs[i] = result->Get(i);
    }

    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        parser->ParseOneRow(rows[i].row, vecs, i);
    }

    // after parse, result should be the same as vecbatch
    EXPECT_TRUE(VecBatchMatch(vecBatch, result));
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(result);
}
} // namespace omniruntime::vec::test
