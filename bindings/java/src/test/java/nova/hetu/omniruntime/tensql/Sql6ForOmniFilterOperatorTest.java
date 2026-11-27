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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIfExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonInExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonLessThanOrEqualExpr;
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
 * sql6 for OmniFilter operator test.
 *
 * @since 2022-03-21
 */
public class Sql6ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(OR:4(AND:4(AND:4(IN:4(#0,"
            + "'Books                                             ':15,"
            + "'Children                                          ':15,"
            + "'Electronics                                       ':15) , "
            + "IN:4(#1,'personal                                          ':15,"
            + "'portable                                          ':15,"
            + "'reference                                         ':15,"
            + "'self-help                                         ':15)) , "
            + "IN:4(#2,'scholaramalgamalg #14                             ':15,"
            + "'scholaramalgamalg #7                              ':15,"
            + "'exportiunivamalg #9                               ':15,"
            + "'scholaramalgamalg #9                              ':15)) , "
            + "AND:4(AND:4(IN:4(#0,'Women                                             ':15,"
            + "'Music                                             ':15,"
            + "'Men                                               ':15) , "
            + "IN:4(#1,'accessories                                       ':15,"
            + "'classical                                         ':15,"
            + "'fragrances                                        ':15,"
            + "'pants                                             ':15)) , "
            + "IN:4(#2,'amalgimporto #1                                   ':15,"
            + "'edu packscholar #1                                ':15,"
            + "'exportiimporto #1                                 ':15,"
            + "'importoamalg #1                                   ':15))) , IS_NOT_NULL:4(#3))";
    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#4) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 2452123:2)) , $operator$LESS_THAN_OR_EQUAL:4(#5 , 2452487:2)) , "
            + "IS_NOT_NULL:4(#5)) , IS_NOT_NULL:4(#6))";
    private static final String MUST_TEST_EXP3 = "AND:4(AND:4(AND:4("
            + "IN:4(#7,1227:1,1224:1,1219:1,1221:1,1226:1,1223:1,1229:1,1230:1,1222:1,1228:1,1225:1,1220:1) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#8 , 2452123:2)) , $operator$LESS_THAN_OR_EQUAL:4(#8 , 2452487:2)) , "
            + "IS_NOT_NULL:4(#8))";
    private static final String MUST_TEST_EXP4 = "IS_NOT_NULL:4(#9)";
    private static final String MUST_TEST_EXP5 = "$operator$GREATER_THAN:4(IF:3($operator$GREATER_THAN:4(#11 , 0.0:3), "
            + "$operator$DIVIDE:3(abs:3($operator$SUBTRACT:3(CAST:3(#10) , #11)) , #11), null:3) , 0.1:3)";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonOrExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(
                                    omniJsonInExpr(15, 0,
                                            Arrays.asList("Books                                             ",
                                                    "Children                                          ",
                                                    "Electronics                                       ")),
                                    omniJsonInExpr(15, 1,
                                            Arrays.asList("personal                                          ",
                                                    "portable                                          ",
                                                    "reference                                         ",
                                                    "self-help                                         "))),
                            omniJsonInExpr(15, 2,
                                    Arrays.asList("scholaramalgamalg #14                             ",
                                            "scholaramalgamalg #7                              ",
                                            "exportiunivamalg #9                               ",
                                            "scholaramalgamalg #9                              "))),
                    omniJsonAndExpr(
                            omniJsonAndExpr(
                                    omniJsonInExpr(15, 0,
                                            Arrays.asList("Women                                             ",
                                                    "Music                                             ",
                                                    "Men                                               ")),
                                    omniJsonInExpr(15, 1,
                                            Arrays.asList("accessories                                       ",
                                                    "classical                                         ",
                                                    "fragrances                                        ",
                                                    "pants                                             "))),
                            omniJsonInExpr(15, 2,
                                    Arrays.asList("amalgimporto #1                                   ",
                                            "edu packscholar #1                                ",
                                            "exportiimporto #1                                 ",
                                            "importoamalg #1                                   ")))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 3)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 4)),
                                    omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 5),
                                            getOmniJsonLiteral(2, false, 2452123))),
                            omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 5),
                                    getOmniJsonLiteral(2, false, 2452487))),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 5))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6)));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonInExpr(1, 7,
                                    Arrays.asList(1227, 1224, 1219, 1221, 1226, 1223, 1229, 1230, 1222, 1228, 1225,
                                            1220)),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 8),
                                    getOmniJsonLiteral(2, false, 2452123))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 8),
                            getOmniJsonLiteral(2, false, 2452487))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 9));
    private static final String MUST_TEST_EXP5_JSON = omniJsonGreaterThanExpr(
            omniJsonIfExpr(omniJsonGreaterThanExpr(getOmniJsonFieldReference(3, 11), getOmniJsonLiteral(3, false, 0.0)),
                    3,
                    omniJsonFourArithmeticExpr("DIVIDE", 3, omniJsonAbsExpr(3, omniJsonFourArithmeticExpr("SUBTRACT", 3,
                            omniJsonCastExpr(3, getOmniJsonFieldReference(2, 10)), getOmniJsonFieldReference(3, 11))),
                            getOmniJsonFieldReference(3, 11)),
                    getOmniJsonFieldReference(3, 11)),
            getOmniJsonLiteral(3, false, 0.1));

    Object[][] dataSourceValue = {
            {"Books                                             ", "Children                                          ",
                    "Books                                             ",
                    "Women                                             ",
                    "Men                                               ", "    ", " ", ""}, // i_category #0 char(50)
            {"reference                                         ", "reference                                         ",
                    "reference                                         ",
                    "self-help                                         ",
                    "accessories                                       ",
                    "classical                                         ",
                    "fragrances                                        ",
                    "pants                                             "}, // i_class #1 char(50)
            {"scholaramalgamalg #14                             ", "scholaramalgamalg #7                              ",
                    "exportiunivamalg #9                               ",
                    "scholaramalgamalg #9                              ",
                    "amalgimporto #1                                   ",
                    "edu packscholar #1                                ",
                    "exportiimporto #1                                 ",
                    "importoamalg #1                                   "}, // i_brand #2 char(50)
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // i_item_sk #3 long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // ss_item_sk #4 #long
            {-12234L, 2452121L, 2452123L, 2452200L, 2452486L, 2452487L, 2452487L, 2455000L}, // ss_sold_date_sk #5 #long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // ss_store_sk #6 #long
            {-12, 12, 1228, 1223, 1227, 1219, 1226, 22144}, // d_month_seq #7 #int
            {-12234L, 2452121L, 2452123L, 2452200L, 2452486L, 2452487L, 2452487L, 2455000L}, // d_date_sk #8 #long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // s_store_sk #9 long
            {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // sum_sales #10 long
            {-2.0, -1.0, 0.0, 1.5, 2.0, 3.0, 4.0, 5.0} // avg_monthly_sales #11 double
    };

    Object[][] dataSourceValueWithNull = {
            {"Books                                             ", "Children                                          ",
                    null, "Women                                             ",
                    "Men                                               ", "    ", " ", ""}, // i_category #0 char(50)
            {"personal                                          ", "personal                                          ",
                    "reference                                         ",
                    "self-help                                         ",
                    "accessories                                       ",
                    "classical                                         ",
                    "fragrances                                        ",
                    "pants                                             "}, // i_class #1 char(50)
            {"scholaramalgamalg #14                             ", "scholaramalgamalg #7                              ",
                    "exportiunivamalg #9                               ",
                    "scholaramalgamalg #9                              ",
                    "amalgimporto #1                                   ",
                    "edu packscholar #1                                ",
                    "exportiimporto #1                                 ",
                    "importoamalg #1                                   "}, // i_brand #2 char(50)
            {null, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // i_item_sk #3 long
            {null, 2L, 3L, 4L, 5L, 6L, 7L, 8L}, // ss_item_sk #4 #long
            {-12234L, null, 2452123L, 2452200L, 2452486L, 2452487L, 2452487L, 2455000L}, // ss_sold_date_sk #5 #long
            {1L, 2L, null, 4L, 5L, 6L, 7L, 8L}, // ss_store_sk #6 #long
            {-12, 12, 1228, 1223, 1227, 1219, 1226, 22144}, // d_month_seq #7 #int
            {-12234L, null, null, 2452200L, 2452486L, 2452487L, 2452487L, null}, // d_date_sk #8 #long
            {null, 2L, 3L, 4L, 5L, null, 7L, null}, // s_store_sk #9 long
            {1L, 2L, 3L, 4L, null, 6L, 7L, 8L}, // sum_sales #10 long
            {-2.0, -1.0, 0.0, null, 2.0, 3.0, 4.0, 5.0} // avg_monthly_sales #11 double
    };

    DataType[] dataSourceTypes = {VarcharDataType.VARCHAR, VarcharDataType.VARCHAR, VarcharDataType.VARCHAR,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, DoubleDataType.DOUBLE};

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10", "#11");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":0,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":1,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":2,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":8}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":10}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":3,\"colVal\":11}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {1, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2 = {3, 4, 5, 6};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {3, 4, 5, 6};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {1, 2, 3, 4, 6};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {5, 6, 7};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {1, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2WithNull = {3, 4, 5, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {3, 4, 5, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {1, 2, 3, 4, 6};
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
    public void testForStoreSalesTable() {
        testForStoreSalesTableWithNull();
        testForStoreSalesTableWithNotNull();
    }

    @Test
    public void testForDateDimTable() {
        testForDateDimTableWithNotNull();
        testForDateDimTableWithNull();
    }

    @Test
    public void testForStoreTable() {
        testForStoreTableWithNull();
    }

    private void testForItemTableWithNotNull() {
        String expIn1 = "IN:4(#0,'Books                                             ':15,"
                + "'Children                                          ':15,"
                + "'Electronics                                       ':15)";
        int[] resultKeepRowIdxForIn1 = {0, 1, 2};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn1, expIn1);

        String expIn2 = "IN:4 (#1,'personal                                          ':15,"
                + "'portable                                          ':15,"
                + "'reference                                         ':15,"
                + "'self-help                                         ':15)";
        int[] resultKeepRowIdxForIn2 = {0, 1, 2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn2, expIn2);

        String expIn3 = "IN:4(#2,'scholaramalgamalg #14                             ':15,"
                + "'scholaramalgamalg #7                              ':15,"
                + "'exportiunivamalg #9                               ':15,"
                + "'scholaramalgamalg #9                              ':15)";
        int[] resultKeepRowIdxForIn3 = {0, 1, 2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn3, expIn3);

        String expIn4 = "IN:4 (#0,'Women                                             ':15," + ""
                + "'Music                                             ':15,"
                + "'Men                                               ':15)";
        int[] resultKeepRowIdxForIn4 = {3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn4, expIn4);

        String expIn5 = "IN:4(#1,'accessories                                       ':15,"
                + "'classical                                         ':15,"
                + "'fragrances                                        ':15,"
                + "'pants                                             ':15)";
        int[] resultKeepRowIdxForIn5 = {4, 5, 6, 7};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn5, expIn5);

        String expIn6 = "IN:4(#2,'amalgimporto #1                                   ':15,"
                + "'edu packscholar #1                                ':15,"
                + "'exportiimporto #1                                 ':15,"
                + "'importoamalg #1                                   ':15)";
        int[] resultKeepRowIdxForIn6 = {4, 5, 6, 7};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn6, expIn6);

        String expAnd1 = "AND:4(AND:4(IN:4(#0,'Books                                             ':15,"
                + "'Children                                          ':15,"
                + "'Electronics                                       ':15),"
                + "IN:4(#1,'personal                                          ':15,"
                + "'portable                                          ':15,"
                + "'reference                                         ':15,"
                + "'self-help                                         ':15)),"
                + "IN:4(#2,'scholaramalgamalg #14                             ':15,"
                + "'scholaramalgamalg #7                              ':15,"
                + "'exportiunivamalg #9                               ':15,"
                + "'scholaramalgamalg #9                              ':15))";
        int[] resultKeepRowIdxForAnd1 = {0, 1, 2};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForAnd1, expAnd1);

        String expAnd2 = "AND:4(AND:4(IN:4(#0,'Women                                             ':15,"
                + "'Music                                             ':15,"
                + "'Men                                               ':15),"
                + "IN(#1,'accessories                                       ':15,"
                + "'classical                                         ':15,"
                + "'fragrances                                        ':15,"
                + "'pants                                             ':15)),"
                + "IN(#2,'amalgimporto #1                                   ':15,"
                + "'edu packscholar #1                                ':15,"
                + "'exportiimporto #1                                 ':15,"
                + "'importoamalg #1                                   ':15))";
        int[] resultKeepRowIdxForAnd2 = {4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForAnd2, expAnd2);

        // (In1 and In2 and In3) Or (In4 and Int5 and Int6) = {0, 1, 2, 4}
        String expMixedOr = "OR:4(AND:4(AND:4(IN:4(#0,'Books                                             ':15,"
                + "'Children                                          ':15,"
                + "'Electronics                                       ':15),"
                + "IN:4(#1,'personal                                          ':15,"
                + "'portable                                          ':15,"
                + "'reference                                         ':15,"
                + "'self-help                                         ':15)),  "
                + "IN:4(#2,'scholaramalgamalg #14                             ':15,"
                + "'scholaramalgamalg #7                              ':15,"
                + "'exportiunivamalg #9                               ':15,"
                + "'scholaramalgamalg #9                              ':15)), "
                + "AND:4(AND:4(IN:4 (#0, 'Women                                             ':15,"
                + "'Music                                             ':15,"
                + "'Men                                               ':15), "
                + "IN:4(#1,'accessories                                       ':15,"
                + "'classical                                         ':15,"
                + "'fragrances                                        ':15,"
                + "'pants                                             ':15)), "
                + "IN:4(#2,'amalgimporto #1                                   ':15,"
                + "'edu packscholar #1                                ':15,"
                + "'exportiimporto #1                                 ':15,"
                + "'importoamalg #1                                   ':15)))";
        int[] resultKeepRowIdxForMixedOr = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixedOr,
                expMixedOr);
    }

    private void testForStoreSalesTableWithNull() {
        String expNotNull = "AND:4(AND:4(IS_NOT_NULL:4(#4) , IS_NOT_NULL:4(#5)) , IS_NOT_NULL:4(#6))";
        int[] resultKeepRowIdxForNotNull = {3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }

    private void testForStoreSalesTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 2452123:2)";
        int[] resultKeepRowIdxForGe = {2, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#5 , 2452487:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4, 5, 6};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForDateDimTableWithNotNull() {
        String expIn = "IN:4(#7,1227:1,1224:1,1219:1,1221:1,1226:1,1223:1,1229:1,1230:1,1222:1,1228:1,1225:1,1220:1)";
        int[] resultKeepRowIdxForIn = {2, 3, 4, 5, 6};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForIn, expIn);

        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#5 , 2452123:2)";
        int[] resultKeepRowIdxForGe = {2, 3, 4, 5, 6, 7};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#5 , 2452487:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4, 5, 6};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForDateDimTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#8)";
        int[] resultKeepRowIdxForNotNull = {0, 3, 4, 5, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);

        String expMixed = "AND:4(IN:4(#7,1227:1,1224:1,1219:1,1221:1,1226:1,1223:1,1229:1,1230:1,1222:1,1228:1,1225:1,"
                + "1220:1), IS_NOT_NULL:4(#8))";
        int[] resultKeepRowIdxForMixed = {3, 4, 5, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixed,
                expMixed);
    }

    private void testForStoreTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#9)";
        int[] resultKeepRowIdxForNotNull = {1, 2, 3, 4, 6};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }
}
