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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonOrExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;

import org.testng.annotations.Test;

import java.util.List;

/**
 * sql4 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql4ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(IS_NOT_NULL:4(#0) , $operator$EQUAL:4(#0 , 1:1)) , "
            + "IS_NOT_NULL:4(#1))";
    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(IS_NOT_NULL:4(#2) , IS_NOT_NULL:4(#4)) , "
            + "IS_NOT_NULL:4(#3))";
    private static final String MUST_TEST_EXP3 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#5) , IS_NOT_NULL:4(#6)) , "
            + "$operator$EQUAL:4(#5 , 12:1)) , $operator$EQUAL:4(#6 , 2001:1)) , IS_NOT_NULL:4(#7))";
    private static final String MUST_TEST_EXP4 = "AND:4(AND:4(IS_NOT_NULL:4(#8) , IS_NOT_NULL:4(#9)) , "
            + "IS_NOT_NULL:4(#10))";
    private static final String MUST_TEST_EXP5 = "AND:4(AND:4(IS_NOT_NULL:4(#11) , IS_NOT_NULL:4(#13)) , "
            + "IS_NOT_NULL:4(#12))";
    private static final String MUST_TEST_EXP6 = "AND:4(OR:4($operator$EQUAL:4(#14 , 'breakfast           ':15) , "
            + "$operator$EQUAL:4(#14 , 'dinner              ':15)) , IS_NOT_NULL:4(#15))";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 0)),
                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 1))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 1)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 2)),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 4))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 3)));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 5)),
                                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 6))),
                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 5), getOmniJsonLiteral(1, false, 12))),
                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 6), getOmniJsonLiteral(1, false, 2001))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 7)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8)),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 9))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 10)));
    private static final String MUST_TEST_EXP5_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 11)),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 13))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 12)));
    private static final String MUST_TEST_EXP6_JSON = omniJsonAndExpr(omniJsonOrExpr(
            omniJsonEqualExpr(getOmniJsonFieldReference(15, 14), getOmniJsonLiteral(15, false, "breakfast           ")),
            omniJsonEqualExpr(getOmniJsonFieldReference(15, 14),
                    getOmniJsonLiteral(15, false, "dinner              "))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 15)));

    DataType[] dataSourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
            LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
            VarcharDataType.VARCHAR, LongDataType.LONG};

    Object[][] dataSourceValue = {{1, 1, 1, 4, 5}, // #0 i_manager_id #int
            {1L, 2L, 3L, 4L, 5L}, // #1 item_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #2 ws_sold_date_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #3 ws_item_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #4 ws_sold_time_sk #long
            {12, 12, 12, 12, 40}, // #5 d_moy #int
            {2001, 2001, 2001, 2001, 2021}, // #6 d_year #int
            {1L, 2L, 3L, 4L, 5L}, // #7 d_date_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #8 cs_sold_date_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #9 cs_item_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #10 cs_sold_time_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #11 ss_sold_date_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #12 ss_item_sk #long
            {1L, 2L, 3L, 4L, 5L}, // #13 ss_sold_time_sk #long
            // #14 t_meal_time char(20)
            {"breakfast           ", "dinner", "dinner              ", "        dinner", "  dinner   "},
            {1L, 2L, 3L, 4L, 5L} // #15 t_time_sk #long
    };

    Object[][] dataSourceValueWithNull = {{1, 1, null, 4, 5}, // #0 i_manager_id #int
            {1L, null, 3L, 4L, 5L}, // #1 item_sk #long
            {null, 2L, 3L, 4L, 5L}, // #2 ws_sold_date_sk #long
            {1L, null, 3L, 4L, 5L}, // #3 ws_item_sk #long
            {1L, 2L, null, 4L, 5L}, // #4 ws_sold_time_sk #long
            {12, null, 12, 12, 40}, // #5 d_moy #int
            {2001, 2001, null, 2001, 2021}, // #6 d_year #int
            {1L, 2L, 3L, null, 5L}, // #7 d_date_sk #long
            {1L, null, 3L, 4L, 5L}, // #8 cs_sold_date_sk #long
            {1L, 2L, null, 4L, 5L}, // #9 cs_item_sk #long
            {1L, 2L, 3L, null, 5L}, // #10 cs_sold_time_sk #long
            {1L, null, 3L, 4L, 5L}, // #11 ss_sold_date_sk #long
            {1L, 2L, null, 4L, 5L}, // #12 ss_item_sk #long
            {1L, 2L, 3L, null, 5L}, // #13 ss_sold_time_sk #long
            {"breakfast           ", null, "dinner              ", "", "  "}, // #14 t_meal_time char(20)
            {1L, 2L, null, 4L, 5L} // #15 t_time_sk #long
    };

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10", "#11", "#12", "#13", "#14", "#15");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":1}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":2}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":8}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":10}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":11}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":12}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":13}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":14,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":15}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {0};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2 = {3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {0};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {0, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {0, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);

        int[] resultKeepRowIdxForEXP6 = {0};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP6, MUST_TEST_EXP6_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {0};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2 = {3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP2,
                MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {0};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {0, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);

        int[] resultKeepRowIdxForEXP5 = {0, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP5,
                MUST_TEST_EXP5);

        int[] resultKeepRowIdxForEXP6 = {0};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP6,
                MUST_TEST_EXP6);
    }

    @Test
    public void testForItemTable() {
        testForItemTableWithNotNull();
        testForItemTableWithNull();
    }

    @Test
    public void testForDatedimTable() {
        testForDatedimTableWithNotNull();
        testForDatedimTableWithNull();
    }

    @Test
    public void testForTimedimTable() {
        testForTimedimTableWithNotNull();
        testForTimedimTableWithNull();
    }

    private void testForTimedimTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#15)";
        int[] resultKeepRowIdxForNotNull = {0, 1, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }

    private void testForTimedimTableWithNotNull() {
        String expEq1 = "$operator$EQUAL:4(#14 , 'dinner              ':15)";
        int[] resultKeepRowIdxForEq1 = {2};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq1, expEq1);

        String expEq2 = "$operator$EQUAL:4(#14 , 'breakfast           ':15)";
        int[] resultKeepRowIdxForEq2 = {0};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq2, expEq2);
    }

    private void testForDatedimTableWithNotNull() {
        String expEq1 = "$operator$EQUAL:4(#6 , 2001:1)";
        int[] resultKeepRowIdxForEq1 = {0, 1, 2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq1, expEq1);

        String expEq2 = "$operator$EQUAL:4(#5 , 12:1)";
        int[] resultKeepRowIdxForEq2 = {0, 1, 2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq2, expEq2);
    }

    private void testForDatedimTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#5)";
        int[] resultKeepRowIdxForNotNull1 = {0, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);
    }

    private void testForItemTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#0)";
        int[] resultKeepRowIdxForNotNull1 = {0, 1, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#1)";
        int[] resultKeepRowIdxForNotNull2 = {0, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);
    }

    private void testForItemTableWithNotNull() {
        String expEq = "$operator$EQUAL:4(#0 , 1:1)";
        int[] resultKeepRowIdxForEq = {0, 1, 2};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq, expEq);
    }
}
