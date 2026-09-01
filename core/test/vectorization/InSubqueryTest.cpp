/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: vectorization expression 'in subquery' test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "codegen/batch_functions/batch_utilfunctions.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace omniruntime::expressions;
using namespace omniruntime::codegen::function;
using namespace omniruntime::TestUtil;

// ============================================================================
// Test InSubqueryExpr batch function for integer types
// ============================================================================

TEST(VectorizationTest, InSubqueryExprInt32BasicTest)
{
    // Probe values: [1, 2, 3, 4, 5]
    // Subquery result: [2, 4, 6]
    // Expected: [false, true, false, true, false]

    int32_t rowCnt = 5;
    int32_t subqueryRowCount = 3;

    int32_t probeValues[5] = {1, 2, 3, 4, 5};
    bool probeNulls[5] = {false, false, false, false, false};

    int32_t subqueryValues[3] = {2, 4, 6};
    bool subqueryNulls[3] = {false, false, false};

    bool finalResult[5] = {false};
    bool finalNull[5] = {false};

    codegen::function::InSubqueryExpr<int32_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    std::vector<bool> expected = {false, true, false, true, false};
    for (int i = 0; i < rowCnt; i++) {
        EXPECT_EQ(finalResult[i], expected[i]) << "Mismatch at index " << i;
        EXPECT_FALSE(finalNull[i]) << "Should not be NULL at index " << i;
    }
}

TEST(VectorizationTest, InSubqueryExprInt32NullProbeTest)
{
    // Probe value is NULL -> result should be NULL
    int32_t rowCnt = 3;
    int32_t subqueryRowCount = 2;

    int32_t probeValues[3] = {1, 2, 3};
    bool probeNulls[3] = {false, true, false};  // middle value is NULL

    int32_t subqueryValues[2] = {2, 4};
    bool subqueryNulls[2] = {false, false};

    bool finalResult[3] = {false};
    bool finalNull[3] = {false};

    codegen::function::InSubqueryExpr<int32_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    // First value: 1 not in [2, 4] -> false
    EXPECT_FALSE(finalResult[0]);
    EXPECT_FALSE(finalNull[0]);

    // Second value: NULL -> NULL
    EXPECT_FALSE(finalResult[1]);
    EXPECT_TRUE(finalNull[1]);

    // Third value: 3 not in [2, 4] -> false
    EXPECT_FALSE(finalResult[2]);
    EXPECT_FALSE(finalNull[2]);
}

TEST(VectorizationTest, InSubqueryExprInt32NullInSubqueryTest)
{
    // Subquery contains NULL, no match -> result should be NULL
    int32_t rowCnt = 3;
    int32_t subqueryRowCount = 3;

    int32_t probeValues[3] = {1, 2, 5};
    bool probeNulls[3] = {false, false, false};

    int32_t subqueryValues[3] = {2, 4, 0};
    bool subqueryNulls[3] = {false, false, true};  // last value is NULL

    bool finalResult[3] = {false};
    bool finalNull[3] = {false};

    codegen::function::InSubqueryExpr<int32_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    // First value: 1 not in [2, 4, NULL] -> NULL (because subquery has NULL)
    EXPECT_FALSE(finalResult[0]);
    EXPECT_TRUE(finalNull[0]);

    // Second value: 2 in [2, 4, NULL] -> true
    EXPECT_TRUE(finalResult[1]);
    EXPECT_FALSE(finalNull[1]);

    // Third value: 5 not in [2, 4, NULL] -> NULL (because subquery has NULL)
    EXPECT_FALSE(finalResult[2]);
    EXPECT_TRUE(finalNull[2]);
}

TEST(VectorizationTest, InSubqueryExprInt32EmptySubqueryTest)
{
    // Empty subquery result -> all results should be false
    int32_t rowCnt = 3;
    int32_t subqueryRowCount = 0;

    int32_t probeValues[3] = {1, 2, 3};
    bool probeNulls[3] = {false, false, false};

    int32_t *subqueryValues = nullptr;
    bool *subqueryNulls = nullptr;

    bool finalResult[3] = {false};
    bool finalNull[3] = {false};

    codegen::function::InSubqueryExpr<int32_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    for (int i = 0; i < rowCnt; i++) {
        EXPECT_FALSE(finalResult[i]) << "Should be false at index " << i;
        EXPECT_FALSE(finalNull[i]) << "Should not be NULL at index " << i;
    }
}

TEST(VectorizationTest, InSubqueryExprInt64Test)
{
    int32_t rowCnt = 4;
    int32_t subqueryRowCount = 3;

    int64_t probeValues[4] = {100, 200, 300, 400};
    bool probeNulls[4] = {false, false, false, false};

    int64_t subqueryValues[3] = {200, 400, 600};
    bool subqueryNulls[3] = {false, false, false};

    bool finalResult[4] = {false};
    bool finalNull[4] = {false};

    codegen::function::InSubqueryExpr<int64_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    std::vector<bool> expected = {false, true, false, true};
    for (int i = 0; i < rowCnt; i++) {
        EXPECT_EQ(finalResult[i], expected[i]) << "Mismatch at index " << i;
        EXPECT_FALSE(finalNull[i]) << "Should not be NULL at index " << i;
    }
}

TEST(VectorizationTest, InSubqueryExprDoubleTest)
{
    int32_t rowCnt = 4;
    int32_t subqueryRowCount = 3;

    double probeValues[4] = {1.5, 2.5, 3.5, 4.5};
    bool probeNulls[4] = {false, false, false, false};

    double subqueryValues[3] = {2.5, 4.5, 6.5};
    bool subqueryNulls[3] = {false, false, false};

    bool finalResult[4] = {false};
    bool finalNull[4] = {false};

    codegen::function::InSubqueryExpr<double>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    std::vector<bool> expected = {false, true, false, true};
    for (int i = 0; i < rowCnt; i++) {
        EXPECT_EQ(finalResult[i], expected[i]) << "Mismatch at index " << i;
        EXPECT_FALSE(finalNull[i]) << "Should not be NULL at index " << i;
    }
}

// ============================================================================
// Test InSubqueryExpr AST node
// ============================================================================

TEST(VectorizationTest, InSubqueryExprASTNodeTest)
{
    auto intType = std::make_shared<DataType>(OMNI_INT);

    // Create probe value expression
    auto probeExpr = new FieldExpr(0, intType);

    // Create subquery result expression
    auto subqueryExpr = new FieldExpr(1, intType);

    // Create InSubqueryExpr
    expressions::InSubqueryExpr* inSubExpr = new expressions::InSubqueryExpr(probeExpr, subqueryExpr, 1);

    // Verify type
    EXPECT_EQ(inSubExpr->GetType(), ExprType::IN_SUBQUERY_E);

    // Verify return type is boolean
    EXPECT_EQ(inSubExpr->GetReturnTypeId(), OMNI_BOOLEAN);

    // Verify toString
    std::string str = inSubExpr->toString();
    EXPECT_TRUE(str.find("InSubquery") != std::string::npos);

    delete inSubExpr;
}

// ============================================================================
// Test three-valued NULL logic edge cases
// ============================================================================

TEST(VectorizationTest, InSubqueryExprAllNullSubqueryTest)
{
    // All subquery values are NULL -> no match, result should be NULL
    int32_t rowCnt = 2;
    int32_t subqueryRowCount = 3;

    int32_t probeValues[2] = {1, 2};
    bool probeNulls[2] = {false, false};

    int32_t subqueryValues[3] = {0, 0, 0};
    bool subqueryNulls[3] = {true, true, true};  // all NULL

    bool finalResult[2] = {false};
    bool finalNull[2] = {false};

    codegen::function::InSubqueryExpr<int32_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    for (int i = 0; i < rowCnt; i++) {
        EXPECT_FALSE(finalResult[i]) << "Should be false at index " << i;
        EXPECT_TRUE(finalNull[i]) << "Should be NULL at index " << i;
    }
}

TEST(VectorizationTest, InSubqueryExprMatchWithNullInSubqueryTest)
{
    // Match found even though subquery contains NULL -> result should be true
    int32_t rowCnt = 2;
    int32_t subqueryRowCount = 3;

    int32_t probeValues[2] = {2, 5};
    bool probeNulls[2] = {false, false};

    int32_t subqueryValues[3] = {2, 0, 4};
    bool subqueryNulls[3] = {false, true, false};  // middle is NULL

    bool finalResult[2] = {false};
    bool finalNull[2] = {false};

    codegen::function::InSubqueryExpr<int32_t>(subqueryValues, subqueryNulls, subqueryRowCount,
        probeValues, probeNulls, finalResult, finalNull, rowCnt);

    // First value: 2 in [2, NULL, 4] -> true (match takes precedence)
    EXPECT_TRUE(finalResult[0]);
    EXPECT_FALSE(finalNull[0]);

    // Second value: 5 not in [2, NULL, 4] -> NULL (because subquery has NULL)
    EXPECT_FALSE(finalResult[1]);
    EXPECT_TRUE(finalNull[1]);
}
