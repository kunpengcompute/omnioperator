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

package nova.hetu.omniruntime.tensql;

import static nova.hetu.omniruntime.util.TestUtils.filterOperatorMatch;
import static nova.hetu.omniruntime.util.TestUtils.filterOperatorMatchWithJson;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonAndExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonLessThanOrEqualExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;

import org.testng.annotations.Test;

import java.util.List;

/**
 * sql7 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql7ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#7) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#7 , 1202:1)) , $operator$LESS_THAN_OR_EQUAL:4(#7 , 1213:1)) , "
            + "IS_NOT_NULL:4(#8))";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 7)),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(1, 7),
                                    getOmniJsonLiteral(1, false, 1202))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 1213))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8)));

    DataType[] dataSourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, LongDataType.LONG};

    Object[][] dataSourceValue = {{1L, 2L, 3L, 4L, 5L}, // cs_warehouse_sk #0 long
            {1L, 2L, 3L, 4L, 5L}, // cs_ship_mode_sk #1 long
            {1L, 2L, 3L, 4L, 5L}, // cs_call_center_sk #2 long
            {1L, 2L, 3L, 4L, 5L}, // cs_ship_date_sk #3 long
            {1L, 2L, 3L, 4L, 5L}, // w_warehouse_sk #4 long
            {1L, 2L, 3L, 4L, 5L}, // sm_ship_mode_sk #5 long
            {1L, 2L, 3L, 4L, 5L}, // cc_call_center_sk #6 long
            {-3, 1202, 1205, 1213, 1213}, // d_month_seq #7 int
            {1L, 2L, 3L, 4L, 5L} // d_date_sk #8 long
    };

    Object[][] dataSourceValueWithNull = {{null, 2L, 3L, 4L, 5L}, // cs_warehouse_sk #0 long
            {1L, null, 3L, 4L, 5L}, // cs_ship_mode_sk #1 long
            {1L, 2L, null, 4L, 5L}, // cs_call_center_sk #2 long
            {1L, 2L, 3L, null, 5L}, // cs_ship_date_sk #3 long
            {null, null, 3L, 4L, 5L}, // w_warehouse_sk #4 long
            {null, 2L, 3L, 4L, null}, // sm_ship_mode_sk #5 long
            {1L, 2L, null, null, 5L}, // cc_call_center_sk #6 long
            {-3, null, 1205, 1213, 1213}, // d_month_seq #7 int
            {1L, 2L, 3L, null, 5L} // d_date_sk #8 long
    };

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":0}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":1}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":2}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":8}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {2, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);
    }

    @Test
    public void testForCatalogSalesTable() {
        testForCatalogSalesTableWithNull();
    }

    @Test
    public void testForWarehouseTable() {
        testForWarehouseTableWithNull();
    }

    @Test
    public void testForShipModeTable() {
        testForShipModeTableWithNull();
    }

    @Test
    public void testForCallCenterTable() {
        testForCallCenterTableWithNull();
    }

    @Test
    public void testForDateDimTable() {
        testForDateDimTableWithNull();
    }

    private void testForCatalogSalesTableWithNull() {
        String expNotNull = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#0), IS_NOT_NULL:4(#1)), IS_NOT_NULL:4(#2)), "
                + "IS_NOT_NULL:4(#3))";
        int[] resultKeepRowIdxForNotNull1 = {4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull);
    }

    private void testForWarehouseTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#4)";
        int[] resultKeepRowIdxForNotNull = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }

    private void testForShipModeTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#5)";
        int[] resultKeepRowIdxForNotNull = {1, 2, 3};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }

    private void testForCallCenterTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull = {0, 1, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }

    private void testForDateDimTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#7)";
        int[] resultKeepRowIdxForNotNull1 = {0, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#8)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expMixed = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#7), $operator$GREATER_THAN_OR_EQUAL:4(#7 , 1202:1)), "
                + "$operator$LESS_THAN_OR_EQUAL:4(#7 , 1213:1)), IS_NOT_NULL:4(#8))";
        int[] resultKeepRowIdxForMixed = {2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixed,
                expMixed);
    }
}
