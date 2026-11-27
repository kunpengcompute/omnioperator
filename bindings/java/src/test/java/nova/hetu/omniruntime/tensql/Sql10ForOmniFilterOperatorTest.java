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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonLessThanOrEqualExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;

import org.testng.annotations.Test;

import java.util.List;

/**
 * sql10 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql10ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#0) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#0 , 2451484:2)) , $operator$LESS_THAN_OR_EQUAL:4(#0 , 2451513:2)) , "
            + "IS_NOT_NULL:4(#1)) , IS_NOT_NULL:4(#2)) , IS_NOT_NULL:4(#3))";
    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#4) , "
            + "IS_NOT_NULL:4(#5)) , $operator$EQUAL:4(#4 , 11:1)) , $operator$EQUAL:4(#5 , 1999:1)) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#6 , 2451484:2)) , $operator$LESS_THAN_OR_EQUAL:4(#6 , 2451513:2)) , "
            + "IS_NOT_NULL:4(#6))";
    private static final String MUST_TEST_EXP3 = "AND:4(AND:4(IS_NOT_NULL:4(#7) , $operator$EQUAL:4(#7 , 7:1)) , "
            + "IS_NOT_NULL:4(#8))";
    private static final String MUST_TEST_EXP4 = "AND:4(IS_NOT_NULL:4(#9), IS_NOT_NULL:4(#10))";
    private static final String MUST_TEST_EXP5 = "AND:4(IS_NOT_NULL:4(#11), IS_NOT_NULL:4(#12))";
    private static final String MUST_TEST_EXP6 = "AND:4(IS_NOT_NULL:4(#13), IS_NOT_NULL:4(#14))";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(
                                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 0)),
                                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 0),
                                                    getOmniJsonLiteral(2, false, 2451484))),
                                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 0),
                                            getOmniJsonLiteral(2, false, 2451513))),
                            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 1))),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 2))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 3)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(
                                    omniJsonAndExpr(
                                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 4)),
                                                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 5))),
                                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 4),
                                                    getOmniJsonLiteral(1, false, 11))),
                                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 5),
                                            getOmniJsonLiteral(1, false, 1999))),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 6),
                                    getOmniJsonLiteral(2, false, 2451484))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 6),
                            getOmniJsonLiteral(2, false, 2451513))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6)));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 7)),
                    omniJsonEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 7))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonAndExpr(
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 9)),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 10)));
    private static final String MUST_TEST_EXP5_JSON = omniJsonAndExpr(
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 11)),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 12)));
    private static final String MUST_TEST_EXP6_JSON = omniJsonAndExpr(
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 13)),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 14)));

    Object[][] dataSourceValue = {{1214L, 2451484L, 2451484L, 2451489L, 2451513L, 2452000L}, // #0 ss_sold_date_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #1 ss_item_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #2 ss_customer_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #3 ss_store_sk #long
            {-1, 11, 11, 11, 11, 15}, // #4 d_moy #int
            {1000, 1999, 1999, 1999, 2000, 2001}, // d_year #5 #int
            {1214L, 2451484L, 2451484L, 2451489L, 2451513L, 2452000L}, // d_date_sk #6 #long
            {-1, 7, 7, 7, 7, 10}, // #7 i_manager_id #int
            {1L, 2L, 3L, 4L, 5L, 6L}, // #8 i_item_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #9 c_customer_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #10 c_current_addr_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #11 ca_address_sk #long
            {"a", "ab", " ", "", "  ", " ab "}, // #12 ca_zip #char(10)
            {"a", " ab", "ab ", "abc", "   ", ""}, // s_zip #13 char(10)
            {1L, 2L, 3L, 4L, 5L, 6L} // s_store_sk #14 #long
    };

    Object[][] dataSourceValueWithNull = {{1214L, null, 2451484L, 2451489L, 2451513L, 2452000L},
            // #0 ss_sold_date_sk #long
            {null, 2L, 3L, 4L, 5L, null}, // #1 ss_item_sk #long
            {null, null, 3L, 4L, 5L, 6L}, // #2 ss_customer_sk #long
            {1L, 2L, 3L, null, 5L, 6L}, // #3 ss_store_sk #long
            {null, 11, 11, 11, 11, 15}, // #4 d_moy #int
            {1000, null, 1999, 1999, 2000, 2001}, // d_year #5 #int
            {1214L, 2451484L, null, 2451489L, 2451513L, 2452000L}, // d_date_sk #6 #long
            {-1, null, 7, 7, 7, 10}, // #7 i_manager_id #int
            {1L, 2L, null, 4L, 5L, 6L}, // #8 i_item_sk #long
            {null, null, 3L, 4L, 5L, 6L}, // #9 c_customer_sk #long
            {null, 2L, 3L, 4L, 5L, null}, // #10 c_current_addr_sk #long
            {1L, null, 3L, 4L, 5L, 6L}, // #11 ca_address_sk #long
            {null, "ab", null, null, "  ", " ab "}, // #12 ca_zip #char(10)
            {null, " ab", "ab ", "abc", " ", null}, // s_zip #13 char(10)
            {1L, null, null, 4L, 5L, 6L} // s_store_sk #14 #long
    };

    DataType[] dataSourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
            IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, IntDataType.INTEGER, LongDataType.LONG,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, VarcharDataType.VARCHAR, VarcharDataType.VARCHAR,
            LongDataType.LONG};

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10", "#11", "#12", "#13", "#14");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":0}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":1}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":2}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":3}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":8}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":10}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":11}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":12,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":13,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":14}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {2, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2WithNull = {3};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {2, 3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {4, 5};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);

        int[] resultKeepRowIdxForEXP6 = {3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP6, MUST_TEST_EXP6_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2WithNull = {3};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);

        int[] resultKeepRowIdxForEXP5 = {4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP5,
                MUST_TEST_EXP5);

        int[] resultKeepRowIdxForEXP6 = {3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP6,
                MUST_TEST_EXP6);
    }

    @Test
    public void testForStoreSalesTable() {
        testForStoreSalesTableWithNull();
        testForStoreSalesTableWithNotNull();
    }

    private void testForStoreSalesTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#0)";
        int[] resultKeepRowIdxForNotNull1 = {0, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#1)";
        int[] resultKeepRowIdxForNotNull2 = {1, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expNotNull3 = "IS_NOT_NULL:4(#2)";
        int[] resultKeepRowIdxForNotNull3 = {2, 3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull3,
                expNotNull3);

        String expNotNull4 = "IS_NOT_NULL:4(#3)";
        int[] resultKeepRowIdxForNotNull4 = {0, 1, 2, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull4,
                expNotNull4);
    }

    private void testForStoreSalesTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#0 , 2451484:2)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#0 , 2451513:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }
}
