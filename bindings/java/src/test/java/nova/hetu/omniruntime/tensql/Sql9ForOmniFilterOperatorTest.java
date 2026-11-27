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
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonInExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonIsNotNullExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonLessThanOrEqualExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonOrExpr;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;

import org.testng.annotations.Test;

import java.util.Arrays;
import java.util.List;

/**
 * sql9 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql9ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#0), "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#0 , 2450819:2)), $operator$LESS_THAN_OR_EQUAL:4(#0 , 2451904:2)), "
            + "IS_NOT_NULL:4(#1)), IS_NOT_NULL:4(#2)), IS_NOT_NULL:4(#3))";
    private static final String MUST_TEST_EXP2 = "AND:4(AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#4) , "
            + "$operator$EQUAL:4(#4 , 1:1)) , IN:4(#5,1998:1,1999:1,2000:1)) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#6 , 2450819:2)) , "
            + "$operator$LESS_THAN_OR_EQUAL:4(#6 , 2451904:2)) , IS_NOT_NULL:4(#6))";
    private static final String MUST_TEST_EXP3 = "AND:4(AND:4(AND:4(IS_NOT_NULL:4(#7) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#7 , 200:1)) , "
            + "$operator$LESS_THAN_OR_EQUAL:4(#7 , 295:1)) , IS_NOT_NULL:4(#8))";
    private static final String MUST_TEST_EXP4 = "AND:4(OR:4($operator$EQUAL:4(#9 , 8:1) , "
            + "$operator$GREATER_THAN:4(#10 , 0:1)) , IS_NOT_NULL:4(#11))";
    private static final String MUST_TEST_EXP5 = "IS_NOT_NULL:4(#12)";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(
                                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 4)),
                                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 0),
                                                    getOmniJsonLiteral(2, false, 2450819))),
                                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 0),
                                            getOmniJsonLiteral(2, false, 2451904))),
                            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 1))),
                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 2))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 3)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(
                                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 4)),
                                            omniJsonEqualExpr(getOmniJsonFieldReference(1, 4),
                                                    getOmniJsonLiteral(1, false, 1))),
                                    omniJsonInExpr(1, 5, Arrays.asList(1998, 1999, 2000))),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(2, 6),
                                    getOmniJsonLiteral(2, false, 2450819))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(2, 6),
                            getOmniJsonLiteral(2, false, 2451904))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6)));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 7)),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(1, 7),
                                    getOmniJsonLiteral(1, false, 200))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(1, 7), getOmniJsonLiteral(1, false, 295))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 8)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonAndExpr(
            omniJsonOrExpr(omniJsonEqualExpr(getOmniJsonFieldReference(1, 9), getOmniJsonLiteral(1, false, 8)),
                    omniJsonGreaterThanExpr(getOmniJsonFieldReference(1, 10), getOmniJsonLiteral(1, false, 0))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 11)));
    private static final String MUST_TEST_EXP5_JSON = omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 12));

    Object[][] dataSourceValue = {{1214L, 2450819L, 2450820L, 2451904L, 2451904L, 2455000L}, // #0 ss_sold_date_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #1 ss_store_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #2 ss_hdemo_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #3 ss_customer_sk #long
            {-1, 0, 1, 1, 1, 4}, // #4 d_dow #int
            {1000, 1998, 1999, 2000, 2000, 2001}, // #5 d_year #int
            {1214L, 2450819L, 2450820L, 2451904L, 2451904L, 2455000L}, // #6 d_date_sk #long
            {-10, 100, 200, 210, 295, 300}, // #7 s_number_employees #int
            {1L, 2L, 3L, 4L, 5L, 6L}, // #8 s_store_sk #long
            {-2, 4, 8, 8, 8, 20}, // #9 hd_dep_count #int
            {-3, 0, 1, 2, 4, 10}, // #10 hd_vehicle_count #int
            {1L, 2L, 3L, 4L, 5L, 6L}, // #11 hd_demo_sk #long
            {1L, 2L, 3L, 4L, 5L, 6L}, // #12 c_customer_sk #long
    };

    Object[][] dataSourceValueWithNull = {{1214L, null, 2450820L, 2451904L, 2451904L, 2455000L},
            // #0 ss_sold_date_sk #long
            {null, 2L, 3L, 4L, 5L, null}, // #1 ss_store_sk #long
            {null, null, 3L, 4L, 5L, 6L}, // #2 ss_hdemo_sk #long
            {1L, 2L, 3L, null, 5L, 6L}, // #3 ss_customer_sk #long
            {-1, null, 1, 1, 1, 4}, // #4 d_dow #int
            {1000, 1998, 1999, 2000, 2000, 2001}, // #5 d_year #int
            {1214L, null, 2450820L, 2451904L, 2451904L, null}, // #6 d_date_sk #long
            {-10, 100, null, 210, 295, 300}, // #7 s_number_employees #int
            {1L, 2L, 3L, null, 5L, 6L}, // #8 s_store_sk #long
            {-2, null, 8, 8, 8, 20}, // #9 hd_dep_count #int
            {-3, 0, 1, 2, 4, 10}, // #10 hd_vehicle_count #int
            {1L, 2L, null, 4L, 5L, null}, // #11 hd_demo_sk #long
            {null, null, 3L, 4L, 5L, null}, // #12 c_customer_sk #long
    };

    DataType[] dataSourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
            IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, IntDataType.INTEGER, LongDataType.LONG,
            IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10", "#11", "#12");

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
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":10}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":11}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":12}");

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {2, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2WithNull = {2, 3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);

        int[] resultKeepRowIdxForEXP5 = {2, 3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP5, MUST_TEST_EXP5_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2WithNull = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects,
                resultKeepRowIdxForEXP2WithNull, MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);

        int[] resultKeepRowIdxForEXP5 = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP5,
                MUST_TEST_EXP5);
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
        testForStoreTableWithNotNull();
    }

    @Test
    public void testForHouseholdDemographicsTable() {
        testForHouseholdDemographicsTableWithNotNull();
        testForHouseholdDemographicsTableWithNull();
    }

    private void testForHouseholdDemographicsTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#11)";
        int[] resultKeepRowIdxForNotNull1 = {0, 1, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);
    }

    private void testForHouseholdDemographicsTableWithNotNull() {
        String expEq = "$operator$EQUAL:4(#9 , 8:1)";
        int[] resultKeepRowIdxForEq = {2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq, expEq);

        String expGt = "$operator$GREATER_THAN:4(#10, 0:1)";
        int[] resultKeepRowIdxForGt = {2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGt, expGt);
    }

    private void testForStoreTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#7)";
        int[] resultKeepRowIdxForNotNull1 = {0, 1, 3, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#8)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 2, 4, 5};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);
    }

    private void testForStoreTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#7 , 200:1)";
        int[] resultKeepRowIdxForGe = {2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#7 , 295:1)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
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
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#0 , 2450819:2)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#0 , 2451904:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForDateDimTableWithNotNull() {
        String expEq = "$operator$EQUAL:4(#4 , 1:1)";
        int[] resultKeepRowIdxForEq = {2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq, expEq);

        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#6 , 2450819:2)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4, 5};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#6 , 2451904:2)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForDateDimTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull = {0, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }
}
