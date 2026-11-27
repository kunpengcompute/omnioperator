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

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>
#include <type/data_type_serializer.h>
#include "util/type_util.h"

using namespace omniruntime::type;
using DataType = omniruntime::type::DataType;

namespace TypeSerializationTest {
TEST(DataTypeSerializer, serialization)
{
    std::vector<DataTypePtr> fieldTypes{IntType(), Decimal128Type(20, 10), VarcharType(256)};
    std::vector<DataTypePtr> types = {IntType(),
                                      DoubleType(),
                                      BooleanType(),
                                      LongType(),
                                      Date32Type(MILLI),
                                      Date64Type(DAY),
                                      Time32Type(SEC),
                                      Time64Type(MICROSEC),
                                      VarcharType(1024),
                                      CharType(512),
                                      Decimal128Type(30, 20),
                                      Decimal64Type(20, 10),
                                      ContainerType(fieldTypes),
                                      TimestampType()};
    std::string typesJson = omniruntime::type::Serialize(types);
    DataTypes deserializedDataTypes = omniruntime::type::Deserialize(typesJson);
    auto &dataTypes = deserializedDataTypes.Get();
    EXPECT_EQ(dataTypes.size(), types.size());
    EXPECT_EQ(dataTypes[0]->GetId(), OMNI_INT);
    EXPECT_EQ(dataTypes[1]->GetId(), OMNI_DOUBLE);
    EXPECT_EQ(dataTypes[2]->GetId(), OMNI_BOOLEAN);
    EXPECT_EQ(dataTypes[3]->GetId(), OMNI_LONG);
    EXPECT_EQ(dataTypes[4]->GetId(), OMNI_DATE32);
    EXPECT_EQ(static_cast<DateDataType &>(*dataTypes[4]).GetDateUnit(), MILLI);
    EXPECT_EQ(dataTypes[5]->GetId(), OMNI_DATE64);
    EXPECT_EQ(static_cast<DateDataType &>(*dataTypes[5]).GetDateUnit(), DAY);
    EXPECT_EQ(dataTypes[6]->GetId(), OMNI_TIME32);
    EXPECT_EQ(static_cast<TimeDataType &>(*dataTypes[6]).GetTimeUnit(), SEC);
    EXPECT_EQ(dataTypes[7]->GetId(), OMNI_TIME64);
    EXPECT_EQ(static_cast<TimeDataType &>(*dataTypes[7]).GetTimeUnit(), MICROSEC);
    EXPECT_EQ(dataTypes[8]->GetId(), OMNI_VARCHAR);
    EXPECT_EQ(static_cast<VarcharDataType &>(*dataTypes[8]).GetWidth(), 1024);
    EXPECT_EQ(dataTypes[9]->GetId(), OMNI_CHAR);
    EXPECT_EQ(static_cast<CharDataType &>(*dataTypes[9]).GetWidth(), 512);
    EXPECT_EQ(dataTypes[10]->GetId(), OMNI_DECIMAL128);
    EXPECT_EQ(static_cast<Decimal128DataType &>(*dataTypes[10]).GetPrecision(), 30);
    EXPECT_EQ(static_cast<Decimal128DataType &>(*dataTypes[10]).GetScale(), 20);
    EXPECT_EQ(dataTypes[11]->GetId(), OMNI_DECIMAL64);
    EXPECT_EQ(static_cast<Decimal64DataType &>(*dataTypes[11]).GetPrecision(), 20);
    EXPECT_EQ(static_cast<Decimal64DataType &>(*dataTypes[11]).GetScale(), 10);
    EXPECT_EQ(dataTypes[12]->GetId(), OMNI_CONTAINER);
    EXPECT_EQ(dataTypes[13]->GetId(), OMNI_TIMESTAMP);
    auto containerDataType = static_cast<ContainerDataType &>(*dataTypes[12]);
    EXPECT_EQ(containerDataType.GetFieldType(0)->GetId(), OMNI_INT);
    EXPECT_EQ(containerDataType.GetFieldType(1)->GetId(), OMNI_DECIMAL128);
    EXPECT_EQ(static_cast<Decimal128DataType &>(*containerDataType.GetFieldType(1)).GetPrecision(), 20);
    EXPECT_EQ(static_cast<Decimal128DataType &>(*containerDataType.GetFieldType(1)).GetScale(), 10);
    EXPECT_EQ(containerDataType.GetFieldType(2)->GetId(), OMNI_VARCHAR);
    EXPECT_EQ(static_cast<VarcharDataType &>(*containerDataType.GetFieldType(2)).GetWidth(), 256);
}

TEST(DataTypeSerializer, DataTypeBaseOperatorTest)
{
    Date32DataType date32DataType = Date32DataType(MILLI);
    Date32DataType date32DataType2 = Date32DataType(DAY);
    ASSERT_TRUE(date32DataType != date32DataType2);
    date32DataType2 = date32DataType;
    EXPECT_EQ(date32DataType2.GetDateUnit(), MILLI);
    ASSERT_FALSE(date32DataType != date32DataType2);
    Date32DataType date32DataType3 = date32DataType2;
    EXPECT_EQ(date32DataType3.GetDateUnit(), MILLI);

    Time32DataType time32DataType = Time32DataType(MICROSEC);
    Time32DataType time32DataType2 = Time32DataType(SEC);
    ASSERT_TRUE(time32DataType != time32DataType2);
    time32DataType2 = time32DataType;
    EXPECT_EQ(time32DataType2.GetTimeUnit(), MICROSEC);
    Time32DataType time32DataType3 = time32DataType2;
    EXPECT_EQ(time32DataType3.GetTimeUnit(), MICROSEC);

    Decimal128DataType decimal128DataType = Decimal128DataType(20, 10);
    Decimal128DataType decimal128DataType2 = Decimal128DataType(30, 20);
    ASSERT_TRUE(decimal128DataType != decimal128DataType2);
    decimal128DataType2 = decimal128DataType;
    EXPECT_EQ(decimal128DataType2.GetPrecision(), decimal128DataType.GetPrecision());
    EXPECT_EQ(decimal128DataType2.GetScale(), decimal128DataType.GetScale());
    EXPECT_EQ(decimal128DataType2.GetPrecision(), 20);
    EXPECT_EQ(decimal128DataType2.GetScale(), 10);
    Decimal128DataType decimal128DataType3 = decimal128DataType2;
    EXPECT_EQ(decimal128DataType3.GetPrecision(), 20);
    EXPECT_EQ(decimal128DataType3.GetScale(), 10);

    VarcharDataType varcharDataType = VarcharDataType(1024);
    VarcharDataType varcharDataType2 = VarcharDataType(512);
    ASSERT_TRUE(varcharDataType != varcharDataType2);
    varcharDataType2 = varcharDataType;
    ASSERT_FALSE(varcharDataType != varcharDataType2);
    EXPECT_EQ(varcharDataType2.GetWidth(), varcharDataType.GetWidth());
    VarcharDataType varcharDataType3 = varcharDataType2;
    EXPECT_EQ(varcharDataType3.GetWidth(), varcharDataType.GetWidth());
    EXPECT_EQ(varcharDataType3.GetWidth(), 1024);

    std::vector<DataTypePtr> fieldTypes{IntType(), VarcharType(1024)};
    std::vector<DataTypePtr> fieldTypes2{IntType(), Date32Type(MILLI)};
    std::vector<DataTypePtr> fieldTypes3{IntType(), Date32Type(MILLI)};
    ContainerDataType containerDataType = ContainerDataType(fieldTypes);
    ContainerDataType containerDataType2 = ContainerDataType(fieldTypes2);
    ContainerDataType containerDataType3 = ContainerDataType(fieldTypes3);
    ASSERT_TRUE(containerDataType != containerDataType2);
    ASSERT_FALSE(containerDataType2 != containerDataType3);
    containerDataType2 = containerDataType;
    EXPECT_EQ(containerDataType2.GetFieldType(0)->GetId(), OMNI_INT);
    EXPECT_EQ(containerDataType2.GetFieldType(1)->GetId(), OMNI_VARCHAR);
    EXPECT_EQ(static_cast<VarcharDataType &>(*containerDataType2.GetFieldType(1)).GetWidth(), 1024);
}
} // namespace TypeSerializationTest
