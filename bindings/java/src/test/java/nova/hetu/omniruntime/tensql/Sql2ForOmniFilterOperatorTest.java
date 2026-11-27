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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonAbsExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonAndExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonCastExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIfExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonOrExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;

import org.testng.annotations.Test;

import java.util.List;

/**
 * sql2 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql2ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(IS_NOT_NULL:4(#0) , IS_NOT_NULL:4(#2)) , "
            + "IS_NOT_NULL:4(#1))";
    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(IS_NOT_NULL:4(#3) , IS_NOT_NULL:4(#5)) , "
            + "IS_NOT_NULL:4(#4))";
    private static final String MUST_TEST_EXP3 = "AND:4(OR:4(OR:4($operator$EQUAL:4(#7 , 2000:1) , "
            + "AND:4($operator$EQUAL:4(#7 , 1999:1) , $operator$EQUAL:4(#8 , 12:1))) , "
            + "AND:4($operator$EQUAL:4(#7 , 2001:1) , $operator$EQUAL:4(#8 , 1:1))) , IS_NOT_NULL:4(#6))";
    private static final String MUST_TEST_EXP4 = "AND:4(AND:4(IS_NOT_NULL:4(#9) , IS_NOT_NULL:4(#10)) , "
            + "IS_NOT_NULL:4(#11))";
    private static final String MUST_TEST_EXP5 = "IS_NOT_NULL:4(#14)";
    private static final String MUST_TEST_EXP6 = "AND:4(AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#7) , "
            + "IS_NOT_NULL:4(#13)) , $operator$EQUAL:4(#7 , 2000:1)) , $operator$GREATER_THAN:4(#13 , 0.0:3)) , "
            + "$operator$GREATER_THAN:4(IF:3($operator$GREATER_THAN:4(#13 , 0.0:3), "
            + "$operator$DIVIDE:3(abs:3($operator$SUBTRACT:3(CAST:3(#12) , #13)) , #13), null:3) , 0.1:3)) , "
            + "IS_NOT_NULL:4(#14))";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 0)),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 2))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 1)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 3)),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 5))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 4)));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonOrExpr(omniJsonOrExpr(
                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 2000)),
                    omniJsonAndExpr(
                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 1999)),
                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 8), getOmniJsonLiteral(1, false, 12)))),
                    omniJsonAndExpr(
                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 2001)),
                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 8), getOmniJsonLiteral(1, false, 1)))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 9)),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 10))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 11)));
    private static final String MUST_TEST_EXP5_JSON = omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 14));
    private static final String MUST_TEST_EXP6_JSON = omniJsonAndExpr(omniJsonAndExpr(
            omniJsonAndExpr(omniJsonAndExpr(
                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 7)),
                            omniJsonIsNotNullExpr(getOmniJsonFieldReference(3, 13))),
                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 2000))),
                    omniJsonGreaterThanExpr(getOmniJsonFieldReference(3, 13), getOmniJsonLiteral(3, false, 0.0))),
            omniJsonGreaterThanExpr(omniJsonIfExpr(
                    omniJsonGreaterThanExpr(getOmniJsonFieldReference(3, 13), getOmniJsonLiteral(3, false, 0.0)), 3,
                    omniJsonFourArithmeticExpr("DIVIDE", 3, omniJsonAbsExpr(3, omniJsonFourArithmeticExpr("SUBTRACT", 3,
                            omniJsonCastExpr(3, getOmniJsonFieldReference(2, 12)), getOmniJsonFieldReference(3, 13))),
                            getOmniJsonFieldReference(3, 13)),
                    getOmniJsonFieldReference(3, 13)), getOmniJsonLiteral(3, false, 0.1))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 14)));

    Object[][] dataSourceValue = {{1L, 2L, 3L, 4L, 5L, 6L}, // i_item_sk #0 #long
            {"ab", "", "  ", " ab", "ab  ", "a b c"}, // i_brand #1 char(50)
            {"a", " ", "abccscss", " ab ", "ab  ab", "abc"}, // i_brand #2 char(50)
            {1L, 2L, 3L, 4L, 5L, 6L}, // ss_item_sk #3 long
            {1L, 2L, 3L, 4L, 5L, 6L}, // ss_store_sk #4 #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // ss_sold_date_sk #5 #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // d_date_sk #6 #long
            {-10, 1999, 2000, 2000, 1999, 2001}, // d_year #7 #int
            {-2, 0, 1, 1, 12, 1}, // d_moy #8 #int
            {1L, 2L, 3L, 4L, 5L, 6L}, // s_store_sk #9 long
            {"abab", "", "  ", " ab", "ab  ", "a b c"}, // s_store_name #10 char(50)
            {"ab", "", "  ", " ab", "ab  ", "a b c"}, // s_company_name #11 char(50)
            {1L, 2L, 3L, 4L, 5L, 6L}, // sum_sales #12 long
            {-2.0, 0.0, 1.0, 1.5, 2.0, 3.0}, // avg_monthly_sales #13 double
            {-3, 12, 1215, 1223, 1217, 1214} // rn #14 #int
    };

    Object[][] dataSourceValueWithNull = {{null, 2L, 3L, 4L, 5L, 6L}, // i_item_sk #0 #long
            {"ab", null, "  ", " ab", "ab  ", "a b c"}, // i_brand #1 char(50)
            {"a", " ", null, " ab ", "ab  ab", "abc"}, // i_brand #2 char(50)
            {null, 2L, 3L, 4L, 5L, null}, // ss_item_sk #3 long
            {1L, null, 3L, 4L, 5L, 6L}, // ss_store_sk #4 #long
            {1L, 2L, null, null, 5L, 6L}, // ss_sold_date_sk #5 #long
            {1L, null, 3L, 4L, 5L, 6L}, // d_date_sk #6 #long
            {null, 1999, 2000, 2000, 1999, 2001}, // d_year #7 #int
            {-2, 0, 1, 1, 12, 1}, // d_moy #8 #int
            {null, 2L, 3L, 4L, 5L, 6L}, // s_store_sk #9 long
            {"abab", "", "  ", " ab", "ab  ", null}, // s_store_name #10 char(50)
            {"ab", "", "  ", " ab", null, "a b c"}, // s_company_name #11 char(50)
            {1L, 2L, 3L, 4L, 5L, 6L}, // sum_sales #12 long
            {-2.0, 0.0, 1.0, 1.5, null, 3.0}, // avg_monthly_sales #13 double
            {12, null, 1223, 1217, 1214, 1224} // rn #14 #int
    };

    DataType[] dataSourceTypes = {LongDataType.LONG, VarcharDataType.VARCHAR, VarcharDataType.VARCHAR,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER,
            IntDataType.INTEGER, LongDataType.LONG, VarcharDataType.VARCHAR, VarcharDataType.VARCHAR, LongDataType.LONG,
            DoubleDataType.DOUBLE, IntDataType.INTEGER};

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10", "#11", "#12", "#13", "#14");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":0}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":1,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":2,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":8}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":10,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":11,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":12}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":3,\"colVal\":13}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":14}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {3, 4, 5};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2 = {4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {2, 3, 4, 5};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {1, 2, 3};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {0, 2, 3, 4, 5};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);

        int[] resultKeepRowIdxForEXP6 = {2, 3};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP6, MUST_TEST_EXP6_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2 = {4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP2,
                MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {2, 3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {1, 2, 3};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);

        int[] resultKeepRowIdxForEXP5 = {0, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP5,
                MUST_TEST_EXP5);

        int[] resultKeepRowIdxForEXP6 = {2, 3};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP6,
                MUST_TEST_EXP6);
    }

    @Test
    public void testForDateDimTable() {
        testForDateDimTableWithNotNull();
        testForDateDimTableWithNull();
    }

    private void testForDateDimTableWithNotNull() {
        String expEq1 = "$operator$EQUAL:4(#7 , 2000:1)";
        int[] resultKeepRowIdxForEq = {2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq, expEq1);

        String expEqAnd1 = "AND:4($operator$EQUAL:4(#7 , 1999:1) , $operator$EQUAL:4(#8 , 12:1))";
        int[] resultKeepRowIdxForEqAnd1 = {4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEqAnd1, expEqAnd1);

        String expEq2 = "$operator$EQUAL:4(#7 , 2001:1)";
        int[] resultKeepRowIdxForEq2 = {5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq2, expEq2);

        String expEqAnd2 = "AND:4($operator$EQUAL:4(#7 , 2001:1) , $operator$EQUAL:4(#8 , 1:1))";
        int[] resultKeepRowIdxForEqAnd2 = {5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEqAnd2, expEqAnd2);

        // expEq1 or expEqAnd1 or expEqAnd2
        String expOr = "OR:4(OR:4($operator$EQUAL:4(#7 , 2000:1) , AND:4($operator$EQUAL:4(#7 , 1999:1) , "
                + "$operator$EQUAL:4(#8 , 12:1))) , AND:4($operator$EQUAL:4(#7 , 2001:1) , "
                + "$operator$EQUAL:4(#8 , 1:1)))";
        int[] resultKeepRowIdxForOr = {2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForOr, expOr);
    }

    private void testForDateDimTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull = {0, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }
}
