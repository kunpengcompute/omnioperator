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

package nova.hetu.omniruntime.type;

import static org.testng.AssertJUnit.assertEquals;

import org.testng.annotations.Test;

/**
 * Data type serializer test
 *
 * @since 2022-2-17
 */
public class TestDataTypeSerializer {
    @Test
    public void testAllTypes() {
        DataType[] types = new DataType[]{BooleanDataType.BOOLEAN, ShortDataType.SHORT, IntDataType.INTEGER,
                LongDataType.LONG, DoubleDataType.DOUBLE, Decimal64DataType.DECIMAL64, Decimal128DataType.DECIMAL128,
                Date32DataType.DATE32, Date64DataType.DATE64, VarcharDataType.VARCHAR, CharDataType.CHAR,
                new ContainerDataType(new DataType[]{IntDataType.INTEGER, CharDataType.CHAR}), InvalidDataType.INVALID,
                NoneDataType.NONE, TimestampDataType.TIMESTAMP};
        String[] serializeds = new String[types.length];
        for (int i = 0; i < types.length; i++) {
            serializeds[i] = DataTypeSerializer.serializeSingle(types[i]);
        }
        String serializedAll = DataTypeSerializer.serialize(types);
        DataType[] allDeserilizedDataTypes = DataTypeSerializer.deserialize(serializedAll);
        for (int i = 0; i < types.length; i++) {
            assertEquals(DataTypeSerializer.deserializeSingle(serializeds[i]), types[i]);
            assertEquals(allDeserilizedDataTypes[i], types[i]);
        }
    }
}
