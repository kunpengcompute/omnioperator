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

package nova.hetu.omniruntime.util;

import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.utils.JsonUtils;

import org.testng.annotations.Test;

/**
 * Test json serialization/deserialization
 *
 * @since 2022-9-22
 */
public class TestJsonUtils {
    private static boolean compareStringArray(String[] arr1, String[] arr2) {
        if (arr1.length != arr2.length) {
            return false;
        }
        for (int i = 0; i < arr1.length; i++) {
            if (!arr1[i].equals(arr2[i])) {
                return false;
            }
        }
        return true;
    }

    @Test
    public void testJsonStringArray() {
        String[] src = {
                omniJsonFourArithmeticExpr("MODULUS", 2, getOmniJsonFieldReference(2, 0),
                        getOmniJsonLiteral(2, false, 3)),
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 2), getOmniJsonLiteral(1, false, 5))};

        String json = JsonUtils.jsonStringArray(src);
        String[] deserializeJsons = JsonUtils.deserializeJson(json);
        assertTrue(compareStringArray(src, deserializeJsons));
    }

    @Test
    public void testJsonMultiDimStringArray() {
        String[][] strings = new String[3][];
        String[] arr1 = {
                omniJsonFourArithmeticExpr("MODULUS", 2, getOmniJsonFieldReference(2, 0),
                        getOmniJsonLiteral(2, false, 3)),
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 2), getOmniJsonLiteral(1, false, 5))};
        String[] arr2 = {getOmniJsonFieldReference(2, 3)};
        String[] arr3 = {omniJsonFourArithmeticExpr("MULTIPLY", 2, getOmniJsonFieldReference(2, 1),
                getOmniJsonLiteral(2, false, 5)), getOmniJsonFieldReference(1, 3)};
        strings[0] = arr1;
        strings[1] = arr2;
        strings[2] = arr3;

        String[] json = JsonUtils.jsonStringArray(strings);
        String[][] deserializeJsons = JsonUtils.deserializeJson(json);
        for (int i = 0; i < 3; i++) {
            assertTrue(compareStringArray(strings[i], deserializeJsons[i]));
        }
    }
}
