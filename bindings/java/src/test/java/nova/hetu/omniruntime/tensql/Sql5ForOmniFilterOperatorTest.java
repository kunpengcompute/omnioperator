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
 * sql5 for OmniFilter operator test.
 *
 * @since 2022-03-31
 */
public class Sql5ForOmniFilterOperatorTest {
    private static final String MUST_TEST_EXP1 = "AND:4(AND:4(IS_NOT_NULL:4(#3) , "
            + "$operator$EQUAL:4(#3 , 'Hopewell':15)) , IS_NOT_NULL:4(#4))";
    private static final String MUST_TEST_EXP2 = "IS_NOT_NULL:4(#10)";
    private static final String MUST_TEST_EXP3 = "AND:4(IS_NOT_NULL:4(#5) , IS_NOT_NULL:4(#6))";
    private static final String MUST_TEST_EXP4 = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#7) , IS_NOT_NULL:4(#8)) , "
            + "$operator$GREATER_THAN_OR_EQUAL:4(#7 , 32287:1)) , $operator$LESS_THAN_OR_EQUAL:4(#8 , 82287:1)) , "
            + "IS_NOT_NULL:4(#9))";

    private static final String MUST_TEST_EXP1_JSON = omniJsonAndExpr(
            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(15, 3)),
                    omniJsonEqualExpr(getOmniJsonFieldReference(15, 3), getOmniJsonLiteral(15, false, "Hopewell"))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 4)));
    private static final String MUST_TEST_EXP2_JSON = omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 10));
    private static final String MUST_TEST_EXP3_JSON = omniJsonAndExpr(
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 5)),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 6)));
    private static final String MUST_TEST_EXP4_JSON = omniJsonAndExpr(
            omniJsonAndExpr(
                    omniJsonAndExpr(
                            omniJsonAndExpr(omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 7)),
                                    omniJsonIsNotNullExpr(getOmniJsonFieldReference(1, 8))),
                            omniJsonGreaterThanOrEqualExpr(getOmniJsonFieldReference(1, 7),
                                    getOmniJsonLiteral(1, false, 32287))),
                    omniJsonLessThanOrEqualExpr(getOmniJsonFieldReference(1, 8), getOmniJsonLiteral(1, false, 82287))),
            omniJsonIsNotNullExpr(getOmniJsonFieldReference(2, 9)));

    List<String> dataSourceProjects = ImmutableList.of("#0", "#1", "#2", "#3", "#4", "#5", "#6", "#7", "#8", "#9",
            "#10");

    List<String> dataSourceProjectionJson = ImmutableList.of(
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":0}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":1}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":2}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":3,\"width\":50}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":4}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":5}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":6}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":7}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":8}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":9}",
            "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":10}");

    DataType[] dataSourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, VarcharDataType.VARCHAR,
            LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER,
            LongDataType.LONG, LongDataType.LONG};

    Object[][] dataSourceValue = {{1L, 2L, 3L, 4L, 5L}, // c_current_addr_sk #0 #long
            {1L, 2L, 3L, 4L, 5L}, // c_current_cdemo_sk #1 #long
            {1L, 2L, 3L, 4L, 5L}, // c_current_hdemo_sk #2 #long
            {"Hopewell", "Hopewell", "Hopewell  ", "  ", "  Hopewell"}, // ca_city #3 #char(10)
            {1L, 2L, 3L, 4L, 5L}, // ca_address_sk #4 #long
            {1L, 2L, 3L, 4L, 5L}, // hd_demo_sk #5 #long
            {1L, 2L, 3L, 4L, 5L}, // hd_income_band_sk #6 #long
            {0, 32287, 33300, 82287, 90000}, // ib_lower_bound #7 #int
            {210, 50000, 82287, 82287, 95000}, // ib_upper_bound #8 #int
            {1L, 2L, 3L, 4L, 5L}, // ib_income_band_sk #9 #long
            {1L, 2L, 3L, 4L, 5L} // sr_cdemo_sk #10 #long
    };

    Object[][] dataSourceValueWithNull = {{null, 2L, 3L, 4L, 5L}, // c_current_addr_sk #0 #long
            {1L, null, 3L, 4L, 5L}, // c_current_cdemo_sk #1 #long
            {1L, 2L, null, 4L, 5L}, // c_current_hdemo_sk #2 #long
            {"Hopewell", "Hopewell", "Hopewell  ", null, "  Hopewell"}, // ca_city #3 #char(10)
            {null, 2L, 3L, 4L, 5L}, // ca_address_sk #4 #long
            {null, 2L, 3L, 4L, 5L}, // hd_demo_sk #5 #long
            {1L, null, 3L, 4L, 5L}, // hd_income_band_sk #6 #long
            {0, null, 33300, 82200, 82287}, // ib_lower_bound #7 #int
            {210, 50000, 82287, null, 82287}, // ib_upper_bound #8 #int
            {1L, 2L, 3L, 4L, null}, // ib_income_band_sk #9 #long
            {null, null, 3L, 4L, 5L} // sr_cdemo_sk #10 #long
    };

    @Test
    public void testMustExpJson() {
        int[] resultKeepRowIdxForEXP1 = {1};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP1, MUST_TEST_EXP1_JSON, 1);

        int[] resultKeepRowIdxForEXP2 = {2, 3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP2, MUST_TEST_EXP2_JSON, 1);

        int[] resultKeepRowIdxForEXP3 = {2, 3, 4};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP3, MUST_TEST_EXP3_JSON, 1);

        int[] resultKeepRowIdxForEXP4 = {2};
        filterOperatorMatchWithJson(dataSourceValueWithNull, dataSourceTypes, dataSourceProjectionJson,
                resultKeepRowIdxForEXP4, MUST_TEST_EXP4_JSON, 1);
    }

    @Test
    public void testMustExp() {
        int[] resultKeepRowIdxForEXP1 = {1};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP1,
                MUST_TEST_EXP1);

        int[] resultKeepRowIdxForEXP2 = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP2,
                MUST_TEST_EXP2);

        int[] resultKeepRowIdxForEXP3 = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP3,
                MUST_TEST_EXP3);

        int[] resultKeepRowIdxForEXP4 = {2};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEXP4,
                MUST_TEST_EXP4);
    }

    @Test
    public void testForCustomerTable() {
        testForCustomerTableWithNull();
    }

    @Test
    public void testForCustomerAddressTable() {
        testForCustomeraddressTableWithNotNull();
        testForCustomeraddressTableWithNull();
    }

    @Test
    public void testForHouseholdDemographicsTable() {
        testForHouseholdDemographicsTableWithNull();
    }

    @Test
    public void testForIncomeBandTable() {
        testForIncomeBandTableWithNotNull();
        testForIncomeBandTableWithNull();
    }

    @Test
    public void testForStoreReturnsTable() {
        testForStoreReturnsTableWithNull();
    }

    private void testForCustomerTableWithNull() {
        String expNotNull = "AND:4(AND:4(IS_NOT_NULL:4(#0),IS_NOT_NULL:4(#1)), IS_NOT_NULL:4(#2))";
        int[] resultKeepRowIdxForNotNull = {3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }

    private void testForCustomeraddressTableWithNotNull() {
        String expEq = "$operator$EQUAL:4(#3 ,'Hopewell':15)";
        int[] resultKeepRowIdxForEq = {0, 1};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForEq, expEq);
    }

    private void testForCustomeraddressTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#3)";
        int[] resultKeepRowIdxForNotNull1 = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#4)";
        int[] resultKeepRowIdxForNotNull2 = {1, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expMixed = "AND:4(AND:4(IS_NOT_NULL:4(#3), $operator$EQUAL:4(#3 ,'Hopewell':15)), IS_NOT_NULL:4(#4))";
        int[] resultKeepRowIdxForMixed = {1};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixed,
                expMixed);
    }

    private void testForHouseholdDemographicsTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#5)";
        int[] resultKeepRowIdxForNotNull1 = {1, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#6)";
        int[] resultKeepRowIdxForNotNull2 = {0, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expMixed = "AND:4(IS_NOT_NULL:4(#5), IS_NOT_NULL:4(#6))";
        int[] resultKeepRowIdxForMixed = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixed,
                expMixed);
    }

    private void testForIncomeBandTableWithNotNull() {
        String expGe = "$operator$GREATER_THAN_OR_EQUAL:4(#7,32287:1)";
        int[] resultKeepRowIdxForGe = {1, 2, 3, 4};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForGe, expGe);

        String expLe = "$operator$LESS_THAN_OR_EQUAL:4(#8, 82287:1)";
        int[] resultKeepRowIdxForLe = {0, 1, 2, 3};
        filterOperatorMatch(dataSourceValue, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForLe, expLe);
    }

    private void testForIncomeBandTableWithNull() {
        String expNotNull1 = "IS_NOT_NULL:4(#7)";
        int[] resultKeepRowIdxForNotNull1 = {0, 2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull1,
                expNotNull1);

        String expNotNull2 = "IS_NOT_NULL:4(#8)";
        int[] resultKeepRowIdxForNotNull2 = {0, 1, 2, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull2,
                expNotNull2);

        String expMixed = "AND:4(AND:4(AND:4(AND:4(IS_NOT_NULL:4(#7), IS_NOT_NULL:4(#8)), "
                + "$operator$GREATER_THAN_OR_EQUAL:4(#7,32287:1)), $operator$LESS_THAN_OR_EQUAL:4(#8, 82287:1)), "
                + "IS_NOT_NULL:4(#9))";
        int[] resultKeepRowIdxForMixed = {2};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForMixed,
                expMixed);
    }

    private void testForStoreReturnsTableWithNull() {
        String expNotNull = "IS_NOT_NULL:4(#10)";
        int[] resultKeepRowIdxForNotNull = {2, 3, 4};
        filterOperatorMatch(dataSourceValueWithNull, dataSourceTypes, dataSourceProjects, resultKeepRowIdxForNotNull,
                expNotNull);
    }
}
