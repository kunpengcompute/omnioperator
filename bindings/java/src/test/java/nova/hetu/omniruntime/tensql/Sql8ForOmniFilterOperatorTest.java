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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIfExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonInExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonLessThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonNotEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonOrExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;

import org.testng.annotations.Test;

import java.util.Arrays;
import java.util.List;

/**
 * sql8 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql8ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(OR:4(AND:4(IN:4(#0,"
            + "'Home                                              ':15,"
            + "'Books                                             ':15,"
            + "'Electronics                                       ':15) , "
            + "IN:4(#1,'wallpaper                                         ':15,"
            + "'parenting                                         ':15,"
            + "'musical                                           ':15)) , "
            + "AND:4(IN:4(#0,'Shoes                                             ':15,"
            + "'Jewelry                                           ':15,"
            + "'Men                                               ':15) , "
            + "IN:4(#1,'womens                                            ':15,"
            + "'birdal                                            ':15,"
            + "'pants                                             ':15))) , IS_NOT_NULL:4(#2))";
    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#3) , "
            + "$operator$EQUAL:4(#3 , 2000:1)) , $operator$GREATER_THAN_OR_EQUAL:4(#4 , 2451545:2)) , "
            + "$operator$LESS_THAN_OR_EQUAL:4(#4 , 2451910:2)) , IS_NOT_NULL:4(#4))";
    private static final String MUST_TEST_EXP3 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#5) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 2451545:2)) , $operator$LESS_THAN_OR_EQUAL:4(#5 , 2451910:2)) , "
            + "IS_NOT_NULL:4(#6)) , IS_NOT_NULL:4(#7))";
    private static final String MUST_TEST_EXP4 = "IS_NOT_NULL:4(#8)";
    private static final String MUST_TEST_EXP5 = "$operator$GREATER_THAN:4(IF:3(not:4($operator$EQUAL:4(#10 , 0.0:3)), "
            + "$operator$DIVIDE:3(abs:3($operator$SUBTRACT:3(CAST:3(#9) , #10)) , #10), null:3) , 0.1:3)";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonOrExpr(
                    omniJsonAndExpr(
                            omniJsonInExpr(15, 0,
                                    Arrays.asList("Home                                              ",
                                            "Books                                             ",
                                            "Electronics                                       ")),
                            omniJsonInExpr(15, 1,
                                    Arrays.asList("wallpaper                                         ",
                                            "parenting                                         ",
                                            "musical                                           "))),
                    omniJsonAndExpr(
                            omniJsonInExpr(15, 0,
                                    Arrays.asList("Shoes                                             ",
                                            "Jewelry                                           ",
                                            "Men                                               ")),
                            omniJsonInExpr(15, 1,
                                    Arrays.asList("womens                                            ",
                                            "birdal                                            ",
                                            "pants                                             ")))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 2)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 3)),
                                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 3),
                                            getOmniJsonLiteral(1, false, 2000))),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 4),
                                    getOmniJsonLiteral(2, false, 2451545))),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 4)));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 5)),
                                    omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 5),
                                            getOmniJsonLiteral(2, false, 2451545))),
                            omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 5),
                                    getOmniJsonLiteral(2, false, 2451910))),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 7)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8));
    private static final String MUST_TEST_EXP5_JSON = omniJsonGreaterThanExpr(
            omniJsonIfExpr(omniJsonNotEqualExpr(getOmniJsonFieldReference(3, 10), getOmniJsonLiteral(3, false, 0.0)), 3,
                    omniJsonFourArithmeticExpr("DIVIDE", 3, omniJsonAbsExpr(3, omniJsonFourArithmeticExpr("SUBTRACT", 3,
                            omniJsonCastExpr(3, getOmniJsonFieldReference(2, 9)), getOmniJsonFieldReference(3, 10))),
                            getOmniJsonFieldReference(3, 10)),
                    getOmniJsonFieldReference(3, 10)),
            getOmniJsonLiteral(3, false, 0.1));

    Object[][] dataSourceValue = {{"Home                                              ",
            "Books                                             ", "Books                                             ",
            "Shoes                                             ", "Men                                               ",
            "Jewelry                                           ", " ", ""}, // i_category #0 char(50)
            {"wallpaper                                         ", "parenting                                         ",
                    "musical                                           ",
                    "self-help                                         ",
                    "womens                                            ",
                    "birdal                                            ",
                    "pants                                             ",
                    "other                                             "}, // i_class #1 char(50)
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // i_item_sk #2 long
            {1000, 2000, 2000, 2000, 2001, 2010, 2020, 2021}, // d_year #3 int
            {2246L, 2451545L, 2451545L, 2451560L, 2451910L, 2451910L, 2452000L, 2452000L}, // d_date_sk #4 long
            {2246L, 2451545L, 2451545L, 2451560L, 2451910L, 2451910L, 2452000L, 2452000L}, // ss_sold_date_sk #5 #long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // ss_item_sk #6 #long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // ss_store_sk #7 #long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // s_store_sk #8 long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // sum_sales #9 long
            {-2.0, -1.0, 0.0, 1.5, 2.0, 3.0, 4.0, 5.0} // avg_monthly_sales #10 double
    };

    Object[][] dataSourceValueWithNull = {{"Home                                              ",
            "Books                                             ", "Books                                             ",
            "Shoes                                             ", "Men                                               ",
            "Jewelry                                           ", null, ""}, // i_category #0 char(50)
            {"wallpaper                                         ", null,
                    "musical                                           ",
                    "self-help                                         ",
                    "womens                                            ",
                    "birdal                                            ",
                    "pants                                             ",
                    "pants                                             "}, // i_class #1 char(50)
            {null, 2L, null, 4L, 5L, 6L, 7L, 8L}, // i_item_sk #2 long
            {1000, null, 2000, 2000, 2001, 2010, 2020, 2021}, // d_year #3 int
            {2246L, 2451545L, null, 2451560L, 2451910L, 2451910L, 2452000L, 2452000L}, // d_date_sk #4 long
            {2246L, null, null, 2451560L, 2451910L, 2451910L, 2452000L, 2452000L}, // ss_sold_date_sk #5 #long
            {1L, 2L, 3L, 4L, null, 6L, 7L, 8L}, // ss_item_sk #6 #long
            {1L, 2L, 3L, 4L, 5L, null, 7L, 8L}, // ss_store_sk #7 #long
            {null, 2L, 3L, 4L, null, 6L, 7L, null}, // s_store_sk #8 long
            {1L, 2L, 3L, 4L, null, 6L, 7L, 8L}, // sum_sales #9 long
            {-2.0, -1.0, 0.0, null, 2.0, 3.0, 4.0, 5.0} // avg_monthly_sales #10 double
    };

    DataType[] dataSourceTypes = {VarcharDataType.VARCHAR, VarcharDataType.VARCHAR, LongDataType.LONG,
            IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
            LongDataType.LONG, LongDataType.LONG, DoubleDataType.DOUBLE};

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":0,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":1,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":2}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":8}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":3,\"colVal\":10}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {4, 5};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2WithNull = {3};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {3};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {1, 2, 3, 5, 6};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {5, 6, 7};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2WithNull = {3};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {3};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {1, 2, 3, 5, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);

        int[] resultKeepRowIdxForEXP5 = {5, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP5,
                MUST_TEST_EXP5);
    }

    @Test
    public void testForItemTable() {
        testForItemTableWithNotNull();
    }

    @Test
    public void testForDateDimTable() {
        testForDateDimTableWithNotNull();
        testForDateDimTableWithNull();
    }

    @Test
    public void testForStoreSalesTable() {
        testForStoreSalesTableWithNull();
        testForStoreSalesTableWithNotNull();
    }

    @Test
    public void testForStoreTable() {
        testForStoreTableWithNull();
    }

    private void testForItemTableWithNotNull() {
        String expIn1 = "IN:4(#0,'Home                                              ':15,"
                + "'Books                                             ':15,"
                + "'Electronics                                       ':15)";
        int[] resultKeepRowIdxForIn1 = {0, 1, 2};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn1, expIn1);

        String expIn2 = "IN:4(#1,'wallpaper                                         ':15,"
                + "'parenting                                         ':15,"
                + "'musical                                           ':15)";
        int[] resultKeepRowIdxForIn2 = {0, 1, 2};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn2, expIn2);

        String expIn3 = "IN:4(#0,'Shoes                                             ':15,"
                + "'Jewelry                                           ':15,"
                + "'Men                                               ':15)";
        int[] resultKeepRowIdxForIn3 = {3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn3, expIn3);

        String expIn4 = "IN:4(#1,'womens                                            ':15,"
                + "'birdal                                            ':15,"
                + "'pants                                             ':15)";
        int[] resultKeepRowIdxForIn4 = {4, 5, 6};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn4, expIn4);

        // (In1 and In2 ) Or (In3 and Int4) = {0, 1, 2, 4, 5}
        String expMixedOr = "OR:4(AND:4(IN:4(#0,'Home                                              ':15,"
                + "'Books                                             ':15,"
                + "'Electronics                                       ':15) , "
                + "IN:4(#1,'wallpaper                                         ':15,"
                + "'parenting                                         ':15,"
                + "'musical                                           ':15)) , "
                + "AND:4(IN:4(#0,'Shoes                                             ':15,"
                + "'Jewelry                                           ':15,"
                + "'Men                                               ':15) , "
                + "IN:4(#1,'womens                                            ':15,"
                + "'birdal                                            ':15,"
                + "'pants                                             ':15)))";
        int[] resultKeepRowIdxForMixedOr = {0, 1, 2, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixedOr,
                expMixedOr);
    }

    private void testForStoreSalesTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 2451545:2)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#5 , 2451910:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForStoreSalesTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#5)";
        int[] resultKeepRowIdxForNotNull1 = {0, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 2, 3, 5, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expNotNull3 = "IS_NOT_NULL:4(#7)";
        int[] resultKeepRowIdxForNotNull3 = {0, 1, 2, 3, 4, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull3,
                expNotNull3);
    }

    private void testForDateDimTableWithNotNull() {
        String expEq = "$operator$EQUAL:4(#3, 2000:1)";
        int[] resultKeepRowIdxForEq = {1, 2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq, expEq);

        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#4 , 2451545:2)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#4 , 2451910:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForDateDimTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#3)";
        int[] resultKeepRowIdxForNotNull = {0, 2, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#4)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);
    }

    private void testForStoreTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#8)";
        int[] resultKeepRowIdxForNotNull = {1, 2, 3, 5, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }
}
