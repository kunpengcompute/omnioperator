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

package nova.hetu.omniruntime.operator;

import static org.testng.Assert.assertEquals;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;
import nova.hetu.omniruntime.type.Decimal128DataType;

import org.testng.annotations.Test;

import java.util.List;

/**
 * OmniExprVerify test.
 *
 * @since 2022-05-16
 */
public class OmniExprVerifyTest {
    /**
     * Test for Spark check error.
     */
    @Test
    public void exprVerifierForSpark() {
        DataType[] inputTypes = {new Decimal128DataType(21, 5)};
        List<String> projectionsJSON = ImmutableList
                .of("{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"precision\":21,\"scale\":5,\"colVal\":0}");
        String filterJSON = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"LESS_THAN_THAN\","
                + "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"colVal\":0,"
                + "\"precision\":21,\"scale\":5},\"right\":{\"exprType\":\"LITERAL\",\"dataType\":6,"
                + "\"precision\":10,\"scale\":5,\"isNull\":false,\"value\":2000}}";

        long isSupported = new OmniExprVerify().exprVerifyNative(DataTypeSerializer.serialize(inputTypes), 0,
                filterJSON, projectionsJSON.toArray(new Object[0]), projectionsJSON.size(), 1);

        assertEquals(isSupported, 0);
    }

    /**
     * Test for Spark check success.
     */
    @Test
    public void exprVerifierForSpark2() {
        DataType[] inputTypes = {new Decimal128DataType(21, 5)};
        List<String> projectionsJSON = ImmutableList
                .of("{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"precision\":21,\"scale\":5,\"colVal\":0}");
        String filterJSON = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"AND\","
                + "\"left\":{\"exprType\":\"UNARY\",\"returnType\":4,\"operator\":\"not\","
                + "\"expr\":{\"exprType\":\"IS_NULL\",\"returnType\":4,"
                + "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":0,\"precision\":17,"
                + "\"scale\":2}]}},\"right\":{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"LESS_THAN\","
                + "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":0,\"precision\":17,"
                + "\"scale\":2},\"right\":{\"exprType\":\"LITERAL\",\"dataType\":6,\"isNull\":false,\"value\":2000,"
                + "\"precision\":17,\"scale\":2}}}";

        long isSupported = new OmniExprVerify().exprVerifyNative(DataTypeSerializer.serialize(inputTypes), 0,
                filterJSON, projectionsJSON.toArray(new Object[0]), projectionsJSON.size(), 1);

        assertEquals(isSupported, 1);
    }

    /**
     * Test for Spark check success.
     */
    @Test
    public void exprVerifierForSpark3() {
        DataType[] inputTypes = {new Decimal128DataType(21, 5)};
        List<String> projectionsJSON = ImmutableList
                .of("{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"precision\":21,\"scale\":5,\"colVal\":0}");
        String filterJSON = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"AND\","
                + "\"left\":{\"exprType\":\"UNARY\",\"returnType\":4,\"operator\":\"not\","
                + "\"expr\":{\"exprType\":\"IS_NULL\",\"returnType\":4,"
                + "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"colVal\":0,\"precision\":22,"
                + "\"scale\":6}]}},\"right\":{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"LESS_THAN\","
                + "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"colVal\":0,\"precision\":22,"
                + "\"scale\":6},\"right\":{\"exprType\":\"LITERAL\",\"dataType\":7,\"isNull\":false,"
                + "\"value\":\"20000000\",\"precision\":22,\"scale\":6}}}";

        long isSupported = new OmniExprVerify().exprVerifyNative(DataTypeSerializer.serialize(inputTypes), 0,
                filterJSON, projectionsJSON.toArray(new Object[0]), projectionsJSON.size(), 1);

        assertEquals(isSupported, 1);
    }
}
