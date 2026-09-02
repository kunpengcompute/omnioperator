/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: omni_row_test
 */
#include <iostream>
#include "gtest/gtest.h"
#include "vector/omni_row.h"
#include "vector/vector.h"
#include "test/util/test_util.h"
#include "operator/hash_util.h"
#include "util/type_util.h"

namespace omniruntime::vec::test {
using namespace omniruntime::vec;
using namespace omniruntime::TestUtil;
using omniruntime::type::ByteType;
using OmniArrayType = omniruntime::type::ArrayType;
using OmniMapType = omniruntime::type::MapType;
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
    EXPECT_EQ(buffer[0], 0b00100000);
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
    EXPECT_EQ(buffer[0], 0b00010010);
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

TEST(omni_row, byte_zero_compact_and_roundtrip)
{
    SerializedValue<int8_t> ser;
    ser.SetValue(0);
    EXPECT_EQ(ser.CompactLength(), 2);
    uint8_t buffer[8] = {};
    uint8_t *end = ser.WriteBuffer(buffer);
    EXPECT_EQ(end, buffer + 2);
    EXPECT_EQ(buffer[0] & 0x0F, 1);

    Vector<int8_t> vec(1);
    uint8_t *rest = RowToVec<type::OMNI_BYTE>(buffer, &vec, 0);
    EXPECT_EQ(rest, buffer + 2);
    EXPECT_EQ(vec.GetValue(0), static_cast<int8_t>(0));
}

TEST(omni_row, byte_oversize_prefix_does_not_overflow_stack)
{
    // Corrupt length=15 (max 4-bit prefix) must not memcpy past a 1-byte int8_t.
    uint8_t buffer[16] = {};
    buffer[0] = 0x0F;
    buffer[1] = 0x7F;
    Vector<int8_t> vec(1);
    uint8_t *rest = RowToVec<type::OMNI_BYTE>(buffer, &vec, 0);
    EXPECT_EQ(rest, buffer + 1 + 15);
    EXPECT_EQ(vec.GetValue(0), static_cast<int8_t>(0x7F));
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
    type::Decimal128 value{ 123, 123 };
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
    std::string_view value{ "hello zy", 8 };
    SerializedValue<std::string_view> serializedValue;
    serializedValue.SetValue(value);
    auto len = serializedValue.CompactLength();
    uint8_t buffer[len];
    uint8_t *end = serializedValue.WriteBuffer(buffer);
    EXPECT_EQ(buffer + len, end);
    EXPECT_EQ(buffer[0], 0b00000001);
    EXPECT_EQ(buffer[1], 8);
    EXPECT_TRUE(memcmp(value.data(), buffer + 2, value.size()) == 0);
}

TEST(omni_row, fill_buffer_no_null)
{
    std::vector<DataTypePtr> types(
        { IntDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance() });
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
    EXPECT_EQ(buf[12], 0b00000001);
    EXPECT_EQ(buf[13], 11);
    EXPECT_TRUE(memcmp(reinterpret_cast<void *>(buf + 14), testStr.data(), testStr.length()) == 0);
    mem::Allocator::GetAllocator()->Free(buf, len);
}

TEST(omni_row, fill_buffer_and_deserial_to_vector)
{
    std::vector<DataTypePtr> types({ LongDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance(),
        BooleanDataType::Instance()});
    int32_t rowNumber = 5;
    int64_t data1[] = {-111, 222, 333, 444, -555};
    double data2[] = {999.99f, 999.0f, 999.999f, 99.96f, 999.99999f};
    std::string data3[] = {"Asleep, high machines shall no", "Asleep, indian sciences may in",
                               "As junior schools love simply.", "A", "Ab"};
    bool data4[] = {true, false, false, true, true};

    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1, data2, data3, data4);

    // Create array vector
    // [[1], [2], [3, 3], [4], [5, 5, 5]]
    int32_t elementSize = 8;
    auto elementVector = std::make_shared<vec::Vector<int32_t>>(elementSize);
    elementVector->SetValue(0, 1);
    elementVector->SetValue(1, 2);
    elementVector->SetValue(2, 3);
    elementVector->SetValue(3, 3);
    elementVector->SetValue(4, 4);
    elementVector->SetValue(5, 5);
    elementVector->SetValue(6, 5);
    elementVector->SetValue(7, 5);
    vec::ArrayVector* arrayVector = new vec::ArrayVector(rowNumber, elementVector);
    arrayVector->SetOffset(0, 0);
    arrayVector->SetOffset(1, 1);
    arrayVector->SetOffset(2, 2);
    arrayVector->SetOffset(3, 4);
    arrayVector->SetOffset(4, 5);
    arrayVector->SetOffset(5, 8);
    vecBatch->Append(arrayVector);

    // fill fake data into result vector batch before parse function
    int64_t fakeData1[] = {0, 0, 0, 0, 0};
    double fakeData2[] = {0.1f, 0.1f, 0.1f, 0.2f, 0.3f};
    std::string fakeData3[] = {"a", "b", "c", "d", "e"};
    bool fakeData4[] = {false, false, false, false, false};
    // Create fakeArray vector
    // [[0], [0], [0, 0], [0], [0, 0, 0]]
    auto fakeElementVector = std::make_shared<vec::Vector<int32_t>>(elementSize);
    fakeElementVector->SetValue(0, 0);
    fakeElementVector->SetValue(1, 0);
    fakeElementVector->SetValue(2, 0);
    fakeElementVector->SetValue(3, 0);
    fakeElementVector->SetValue(4, 0);
    fakeElementVector->SetValue(5, 0);
    fakeElementVector->SetValue(6, 0);
    fakeElementVector->SetValue(7, 0);
    vec::ArrayVector* fakeArrayVector = new vec::ArrayVector(rowNumber, fakeElementVector);
    fakeArrayVector->SetOffset(0, 0);
    fakeArrayVector->SetOffset(1, 1);
    fakeArrayVector->SetOffset(2, 2);
    fakeArrayVector->SetOffset(3, 4);
    fakeArrayVector->SetOffset(4, 5);
    fakeArrayVector->SetOffset(5, 8);

    VectorBatch *result = CreateVectorBatch(dataTypes, rowNumber, fakeData1, fakeData2, fakeData3, fakeData4);
    result->Append(fakeArrayVector);

    types.push_back(std::make_shared<OmniArrayType>(IntType()));
    std::vector<Encoding> encodings({OMNI_FLAT, OMNI_FLAT, OMNI_FLAT, OMNI_FLAT, OMNI_ENCODING_ARRAY});
    std::vector<type::DataTypeId> typeIds({
        type::DataTypeId::OMNI_LONG,
        type::DataTypeId::OMNI_DOUBLE,
        type::DataTypeId::OMNI_VARCHAR,
        type::DataTypeId::OMNI_BOOLEAN,
        type::DataTypeId::OMNI_ARRAY
    });

    RowBuffer rowBuffer(typeIds, encodings);
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        rowBuffer.TransValueFromVectorBatch(vecBatch, i);
        auto len = rowBuffer.FillBuffer();
        rows.emplace_back(rowBuffer.TakeRowBuffer(), len);
    }

    auto parser = std::make_unique<RowParser>(types);
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
    std::vector<DataTypePtr> types(
        { LongDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance() });
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
    std::vector<DataTypePtr> types(
        { LongDataType::Instance(), DoubleDataType::Instance(), VarcharDataType::Instance() });
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
    std::vector<DataTypePtr> types({ BooleanDataType::Instance() });
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

vec::MapVector* createMapVector()
{
    int mapSize = 4;
    int keySize = 11;

    auto* keys = new vec::Vector<double>(keySize);
    auto* values = new vec::Vector<int32_t>(keySize);

    for (int i = 0; i < keySize; i++) {
        keys->SetValue(i, 0.1 * i);
        values->SetValue(i, i);
    }

    auto* mapVec = new MapVector(mapSize);

    mapVec->SetOffset(0, 0);
    mapVec->SetOffset(1, 3);
    mapVec->SetOffset(2, 5);
    mapVec->SetOffset(3, 9);
    mapVec->SetOffset(4, 11);

    mapVec->AddKeys(keys);
    mapVec->AddValues(values);
    return mapVec;
}

vec::MapVector* createFakeMapVector()
{
    int mapSize = 4;
    int keySize = 11;

    auto* keys = new vec::Vector<double>(keySize);
    auto* values = new vec::Vector<int32_t>(keySize);

    for (int i = 0; i < keySize; i++) {
        keys->SetValue(i, 0.0);
        values->SetValue(i, 0);
    }

    auto* mapVec = new MapVector(mapSize);

    mapVec->SetOffset(0, 0);
    mapVec->SetOffset(1, 3);
    mapVec->SetOffset(2, 5);
    mapVec->SetOffset(3, 9);
    mapVec->SetOffset(4, 11);

    mapVec->AddKeys(keys);
    mapVec->AddValues(values);
    return mapVec;
}

TEST(omni_row, fill_buffer_and_deserial_to_map_vector)
{
    std::vector<DataTypePtr> types({ LongDataType::Instance() });
    int32_t rowNumber = 4;
    int64_t data1[] = {-111, 222, 333, -444};

    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1);
    vec::MapVector* mapVector = createMapVector();
    vecBatch->Append(mapVector);

    int64_t fakeData1[] = {0, 0, 0, 0};
    VectorBatch *result = CreateVectorBatch(dataTypes, rowNumber, fakeData1);
    vec::MapVector* fakeMapVector = createFakeMapVector();
    result->Append(fakeMapVector);

    types.push_back(std::make_shared<OmniMapType>(DoubleType(), IntType()));
    std::vector<Encoding> encodings({OMNI_FLAT, OMNI_ENCODING_MAP});
    std::vector<type::DataTypeId> typeIds({
        type::DataTypeId::OMNI_LONG,
        type::DataTypeId::OMNI_MAP
    });

    RowBuffer rowBuffer(typeIds, encodings);
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        rowBuffer.TransValueFromVectorBatch(vecBatch, i);
        auto len = rowBuffer.FillBuffer();
        rows.emplace_back(rowBuffer.TakeRowBuffer(), len);
    }

    auto parser = std::make_unique<RowParser>(types);
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

TEST(omni_row, array_byte_compact_length_not_uint8_truncated)
{
    // 200 TINYINT encodings * 2 bytes = 400 > 255. Returning uint8_t from CalElementSize
    // wrapped the payload length, so FillBuffer under-allocated shuffle rows.
    // ARRAY wire: 1-byte header + CompactNonNegLen(nElem) count bytes + element payload.
    // nElem=200 needs 2 count bytes (not 1): PrefixLen + 2 + 400 = 403.
    const int32_t nElem = 200;
    auto elementVector = std::make_shared<vec::Vector<int8_t>>(nElem);
    for (int32_t i = 0; i < nElem; i++) {
        elementVector->SetValue(i, static_cast<int8_t>(i - 100));
    }
    auto *arrayVector = new vec::ArrayVector(1, elementVector);
    arrayVector->SetOffset(0, 0);
    arrayVector->SetOffset(1, nElem);

    SerializedValue<BaseVector *, OMNI_ENCODING_ARRAY> ser;
    ser.TransValue(arrayVector, 0);
    const int32_t expected =
        BaseSerialize::PrefixLen + BaseSerialize::CompactNonNegLen(nElem) + nElem * 2;
    EXPECT_EQ(ser.CompactLength(), expected);

    uint8_t buffer[1024];
    uint8_t *end = ser.WriteBuffer(buffer);
    EXPECT_EQ(static_cast<int32_t>(end - buffer), expected);

    delete arrayVector;
}

TEST(omni_row, array_byte_yarn_missing_tinyints_roundtrip_wide_row)
{
    // Values Omni collect_set dropped on the full YARN table (COUNT DISTINCT still 47).
    const int8_t dropped[] = {-19, -15, -13, -12, -5, 5, 7, 8, 9, 11, 25};
    const int32_t nElem = static_cast<int32_t>(sizeof(dropped) / sizeof(dropped[0]));
    const int32_t rowNumber = 1;

    std::vector<DataTypePtr> types({LongDataType::Instance(), DoubleDataType::Instance()});
    int64_t data1[] = {-49996};
    double data2[] = {1.0};
    DataTypes dataTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(dataTypes, rowNumber, data1, data2);

    auto setElems = std::make_shared<vec::Vector<int8_t>>(nElem);
    auto listElems = std::make_shared<vec::Vector<int8_t>>(nElem);
    for (int32_t i = 0; i < nElem; i++) {
        setElems->SetValue(i, dropped[i]);
        listElems->SetValue(i, dropped[i]);
    }
    auto *setArr = new vec::ArrayVector(rowNumber, setElems);
    setArr->SetOffset(0, 0);
    setArr->SetOffset(1, nElem);
    auto *listArr = new vec::ArrayVector(rowNumber, listElems);
    listArr->SetOffset(0, 0);
    listArr->SetOffset(1, nElem);
    vecBatch->Append(setArr);
    vecBatch->Append(listArr);

    int64_t fake1[] = {0};
    double fake2[] = {0.0};
    VectorBatch *result = CreateVectorBatch(dataTypes, rowNumber, fake1, fake2);
    auto fakeSetElems = std::make_shared<vec::Vector<int8_t>>(nElem);
    auto fakeListElems = std::make_shared<vec::Vector<int8_t>>(nElem);
    for (int32_t i = 0; i < nElem; i++) {
        fakeSetElems->SetValue(i, 0);
        fakeListElems->SetValue(i, 0);
    }
    auto *fakeSet = new vec::ArrayVector(rowNumber, fakeSetElems);
    fakeSet->SetOffset(0, 0);
    fakeSet->SetOffset(1, nElem);
    auto *fakeList = new vec::ArrayVector(rowNumber, fakeListElems);
    fakeList->SetOffset(0, 0);
    fakeList->SetOffset(1, nElem);
    result->Append(fakeSet);
    result->Append(fakeList);

    types.push_back(std::make_shared<OmniArrayType>(ByteType()));
    types.push_back(std::make_shared<OmniArrayType>(ByteType()));
    std::vector<Encoding> encodings(
        {OMNI_FLAT, OMNI_FLAT, OMNI_ENCODING_ARRAY, OMNI_ENCODING_ARRAY});
    std::vector<type::DataTypeId> typeIds({type::DataTypeId::OMNI_LONG, type::DataTypeId::OMNI_DOUBLE,
        type::DataTypeId::OMNI_ARRAY, type::DataTypeId::OMNI_ARRAY});

    RowBuffer rowBuffer(typeIds, encodings);
    std::vector<RowInfo> rows;
    rows.reserve(rowNumber);
    rowBuffer.TransValueFromVectorBatch(vecBatch, 0);
    auto len = rowBuffer.FillBuffer();
    rows.emplace_back(rowBuffer.TakeRowBuffer(), len);

    auto parser = std::make_unique<RowParser>(types);
    BaseVector *vecs[4];
    for (int32_t i = 0; i < 4; ++i) {
        vecs[i] = result->Get(i);
    }
    parser->ParseOneRow(rows[0].row, vecs, 0);

    EXPECT_TRUE(VecBatchMatch(result, vecBatch));
    auto *outSet = static_cast<vec::ArrayVector *>(result->Get(2));
    EXPECT_EQ(outSet->GetSize(0), nElem);
    auto outElems = outSet->GetArrayAt(0, false);
    auto *outFlat = static_cast<vec::Vector<int8_t> *>(outElems.get());
    for (int32_t i = 0; i < nElem; i++) {
        EXPECT_EQ(outFlat->GetValue(i), dropped[i]) << "idx=" << i;
    }

    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(result);
}

}
