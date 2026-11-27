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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonInExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonLessThanOrEqualExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Date32DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;

import org.testng.annotations.Test;

import java.util.Arrays;
import java.util.List;

/**
 * sql1 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql1ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#3) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#3 , 10406:8)) , $operator$LESS_THAN_OR_EQUAL:4(#3 , 10467:8)) , "
            + "IS_NOT_NULL:4(#4))";

    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#5) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 100:1)) , $operator$LESS_THAN_OR_EQUAL:4(#5 , 500:1)) , "
            + "IS_NOT_NULL:4(#6)) , IS_NOT_NULL:4(#7))";

    private static final String MUST_TEST_EXP3 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#0) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#0 , 76:1)) , $operator$LESS_THAN_OR_EQUAL:4(#0 , 106:1)) , "
            + "IN:4(#1,512:1,409:1,677:1,16:1)) , IS_NOT_NULL:4(#1))";

    private static final String MUST_TEST_EXP4 = "IS_NOT_NULL:4(#8)";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(8, 3)),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(8, 3),
                                    getOmniJsonLiteral(8, false, 10406))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(8, 3), getOmniJsonLiteral(8, false, 10467))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 4)));

    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 5)),
                                    omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(1, 5),
                                            getOmniJsonLiteral(1, false, 100))),
                            omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(1, 5),
                                    getOmniJsonLiteral(1, false, 500))),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 7)));

    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 0)),
                                    omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(1, 0),
                                            getOmniJsonLiteral(1, false, 76))),
                            omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(1, 0),
                                    getOmniJsonLiteral(1, false, 106))),
                    omniJsonInExpr(1, 1, Arrays.asList(512, 409, 677, 16))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 1)));

    private static final String MUST_TEST_EXP4_JSON = omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8));

    private static final String MUST_TEST_EXP5_JSON = omniJsonIsNullExpr(getOmniJsonFieldReference(2, 2));

    /*
     * 涉及表：item,date_dim,inventory,store_sales
     */
    Object[][] dataSource = {{50, 76, 80, 106, 200}, // #0 i_current_price
            {16, 30, 409, 512, 677}, // #1 i_manufact_id #int
            {0L, 5L, 10L, 15L, 20L}, // #2 i_item_sk #long
            {10405, 10406, 10450, 10467, 10500}, // #3 d_date #date
            {1L, 2L, 3L, 4L, 5L}, // #4 d_date_sk #long
            {10, 100, 200, 500, 1000}, // #5 inv_quantity_on_hand #int
            {0L, 5L, 20L, 30L, 50L}, // #6 inv_item_sk #long
            {0L, 1L, 20L, 40L, 60L}, // #7 inv_date_sk #long
            {2L, 4L, 6L, 8L, 10L} // #8 ss_item_sk #long
    };

    Object[][] dataSourceWithNull = {{50, 76, null, 106, 200}, // #0 i_current_price
            {16, 30, 409, 512, null}, // #1 i_manufact_id #int
            {0L, null, 10L, null, 20L}, // #2 i_item_sk #long
            {10405, 10406, 10450, null, 10500}, // #3 d_date #date
            {1L, 2L, null, 4L, 5L}, // #4 d_date_sk #long
            {10, 100, 200, null, 1000}, // #5 inv_quantity_on_hand #int
            {0L, 5L, 20L, null, 50L}, // #6 inv_item_sk #long
            {0L, 1L, null, null, 60L}, // #7 inv_date_sk #long
            {2L, null, null, 8L, 10L} // #8 ss_item_sk #long
    };

    // 数据源类型
    DataType[] dataSourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, Date32DataType.DATE32,
            LongDataType.LONG, IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};

    List<String> dataSourceProjections = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":1}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":2}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":8,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":8}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {1};
        filterOperatorMatchWithJson(dataSourceWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2 = {1};
        filterOperatorMatchWithJson(dataSourceWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {3};
        filterOperatorMatchWithJson(dataSourceWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {0, 3, 4};
        filterOperatorMatchWithJson(dataSourceWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {1, 3};
        filterOperatorMatchWithJson(dataSourceWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {1};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2 = {1};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForEXP2,
                MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {3};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {0, 3, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);
    }

    @Test
    public void testForItemTableFilter() {
        testForItemTableWithNotNull();
        testForItemTableWithNull();
    }

    @Test
    public void testForDatedimTableFilter() {
        testForDateDimTableWithNotNull();
        testForDateDimTableWithNull();
    }

    @Test
    public void testForInventoryTableFilter() {
        testForInventoryTableWithNotNull();
        testForInventoryTableWithNull();
    }

    private void testForInventoryTableWithNotNull() {
        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#5 , 500:1)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLe, expLe);

        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 100:1)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForGe, expGe);

        String expLeAndGe = "AND:4($operator$LESS_THAN_OR_EQUAL:4(#5 , 500:1), "
                + "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 100:1))";
        int[] resultKeepRowIdxForLeAndGe = {1, 2, 3};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLeAndGe, expLeAndGe);
    }

    private void testForInventoryTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#5)";
        int[] resultKeepRowIdxForNotNull1 = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expNotNull3 = "IS_NOT_NULL:4(#7)";
        int[] resultKeepRowIdxForNotNull3 = {0, 1, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForNotNull3,
                expNotNull3);

        String expGe = "AND:4(IS_NOT_NULL:4(#5) , $operator$GREATER_THAN_OR_EQUAL:4(#5 , 100:1))";
        int[] resultKeepRowIdxForGe = {1, 2, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForGe, expGe);

        String expLe = "AND:4(IS_NOT_NULL:4(#5) , $operator$LESS_THAN_OR_EQUAL:4(#5 , 500:1))";
        int[] resultKeepRowIdxForLe = {0, 1, 2};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLe, expLe);

        String expLeAndGe1 = "AND:4(AND:4(IS_NOT_NULL:4(#5) , $operator$GREATER_THAN_OR_EQUAL:4(#5 , 100:1)) ,"
                + " $operator$LESS_THAN_OR_EQUAL:4(#5 , 500:1))";
        int[] resultKeepRowIdxForLeAndGe1 = {1, 2};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLeAndGe1,
                expLeAndGe1);

        String expLeAndGe2 = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#5) , $operator$GREATER_THAN_OR_EQUAL:4(#5 , 100:1)) ,"
                + " $operator$LESS_THAN_OR_EQUAL:4(#5 , 500:1)) , IS_NOT_NULL:4(#6))";
        int[] resultKeepRowIdxForLeAndGe2 = {1, 2};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLeAndGe2,
                expLeAndGe2);
    }

    private void testForDateDimTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#3 , 10406:8)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#3 , 10467:8)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLe, expLe);

        String expLeAndGe = "AND:4($operator$GREATER_THAN_OR_EQUAL:4(#3 , 10406:8) , "
                + "$operator$LESS_THAN_OR_EQUAL:4(#3 , 10467:8))";
        int[] resultKeepRowIdxForLeAndGe = {1, 2, 3};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLeAndGe, expLeAndGe);
    }

    private void testForDateDimTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#3)";
        int[] resultKeepRowIdxForNotNull1 = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#4)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 3, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expGe = "AND:4(IS_NOT_NULL:4(#3) , $operator$GREATER_THAN_OR_EQUAL:4(#3 , 10406:8))";
        int[] resultKeepRowIdxForGe = {1, 2, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForGe, expGe);

        String expLe = "AND:4(IS_NOT_NULL:4(#3) , $operator$LESS_THAN_OR_EQUAL:4(#3 , 10467:8))";
        int[] resultKeepRowIdxForLe = {0, 1, 2};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLe, expLe);

        String expLeAndGe1 = "AND:4(AND:4(IS_NOT_NULL:4(#3) , "
                + "$operator$GREATER_THAN_OR_EQUAL:4(#3 , 10406:8)) , $operator$LESS_THAN_OR_EQUAL:4(#3 , 10467:8))";
        int[] resultKeepRowIdxForLeAndGe1 = {1, 2};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLeAndGe1,
                expLeAndGe1);
    }

    private void testForItemTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#0 , 76:1)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#0 , 106:1)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLe, expLe);

        String expIn = "IN:4(#1,512:1,409:1,677:1,16:1)";
        int[] resultKeepRowIdxForIn = {0, 2, 3, 4};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForIn, expIn);

        String expLeAndGe = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#0) , $operator$GREATER_THAN_OR_EQUAL:4(#0 , 76:1)) , "
                + "$operator$LESS_THAN_OR_EQUAL:4(#0 , 106:1)) , IN:4(#1,512:1,409:1,677:1,16:1))";
        int[] resultKeepRowIdxForLeAndGe = {2, 3};
        filterOperatorMatch(dataSource, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForLeAndGe, expLeAndGe);
    }

    private void testForItemTableWithNull() {
        String expIn1 = "IS_NOT_NULL:4(#0)";
        int[] resultKeepRowIdxForIn1 = {0, 1, 3, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForIn1, expIn1);

        String expIn2 = "IS_NOT_NULL:4(#2)";
        int[] resultKeepRowIdxForIn2 = {0, 2, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForIn2, expIn2);

        String expIn3 = "AND:4(IS_NOT_NULL:4(#0) , $operator$GREATER_THAN_OR_EQUAL:4(#0 , 76:1))";
        int[] resultKeepRowIdxForIn3 = {1, 3, 4};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForIn3, expIn3);

        String expIn4 = "AND:4(AND:4(IS_NOT_NULL:4(#0) , $operator$GREATER_THAN_OR_EQUAL:4(#0 , 76:1)) , "
                + "$operator$LESS_THAN_OR_EQUAL:4(#0 , 106:1))";
        int[] resultKeepRowIdxForIn4 = {1, 3};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForIn4, expIn4);

        String expIn5 = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#0) , $operator$GREATER_THAN_OR_EQUAL:4(#0 , 76:1)),"
                + "$operator$LESS_THAN_OR_EQUAL:4(#0 , 106:1)) , IN:4(#1,512:1,409:1,677:1,16:1))";
        int[] resultKeepRowIdxForIn5 = {3};
        filterOperatorMatch(dataSourceWithNull, dataSourceTypes, dataSourceProjections, resultKeepRowIdxForIn5, expIn5);
    }
}
