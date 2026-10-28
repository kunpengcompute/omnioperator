/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Switch (CASE) expression unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/If.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "expression/expressions.h"
#include "VectorTestUtil.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::VectorTestUtil;

class SwitchTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const switch_test_env = ::testing::AddGlobalTestEnvironment(new SwitchTestEnvironment);

namespace {
BaseVector* RunSwitchExpr(SwitchExpr* switchExpr, VectorBatch* batch, int32_t rowSize)
{
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    ExprEval e(batch, &context);
    e.VisitExpr(*switchExpr);
    return e.GetResult();
}
}

TEST(SwitchTest, LazySwitchExprIntMixed)
{
    // case when c0 then a when c0 == false then b else c end: exercises the
    // mixed classification and the shrinking remainingRows cascade.
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new FieldExpr(0, boolType), new FieldExpr(1, intType));
    auto* secondCond = new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, boolType),
        new LiteralExpr(false, boolType), boolType);
    whenClause.emplace_back(secondCond, new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    int32_t a[rowSize] = {1, 2, 3, 4};
    int32_t b[rowSize] = {10, 20, 30, 40};
    int32_t c[rowSize] = {100, 200, 300, 400};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);
    EXPECT_EQ(resVec->GetValue(1), 20);
    EXPECT_EQ(resVec->GetValue(2), 3);
    EXPECT_EQ(resVec->GetValue(3), 40);

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprAllMatchFirstCase)
{
    // All rows match the first case; later cases and else must not run (values
    // from them would differ).
    const int32_t rowSize = 4;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* cond = new BinaryExpr(expressions::Operator::GT, new FieldExpr(0, intType), new LiteralExpr(0, intType),
        std::make_shared<DataType>(OMNI_BOOLEAN));

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(cond, new FieldExpr(1, intType));
    whenClause.emplace_back(new LiteralExpr(true, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {5, 6, 7, 8};
    int32_t a[rowSize] = {1, 2, 3, 4};
    int32_t b[rowSize] = {10, 20, 30, 40};
    int32_t c[rowSize] = {100, 200, 300, 400};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_EQ(resVec->GetValue(i), a[i]);
    }

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprNoneMatchElse)
{
    // No case matches any row: everything falls through to the else clause.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GT, new FieldExpr(0, intType),
        new LiteralExpr(100, intType), boolType), new FieldExpr(1, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::LT, new FieldExpr(0, intType),
        new LiteralExpr(-100, intType), boolType), new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {1, 2, 3};
    int32_t a[rowSize] = {10, 20, 30};
    int32_t b[rowSize] = {40, 50, 60};
    int32_t c[rowSize] = {70, 80, 90};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_EQ(resVec->GetValue(i), c[i]);
    }

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprNullConditionFallsThrough)
{
    // A NULL condition does not match (SQL semantics): the row falls through
    // to later cases and finally the else clause.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GT, new FieldExpr(0, intType),
        new LiteralExpr(5, intType), boolType), new FieldExpr(1, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GTE, new FieldExpr(0, intType),
        new LiteralExpr(0, intType), boolType), new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {7, 0, 3};
    int32_t a[rowSize] = {1, 2, 3};
    int32_t b[rowSize] = {10, 20, 30};
    int32_t c[rowSize] = {100, 200, 300};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b, c);
    input->Get(0)->SetNull(1);   // NULL condition at row 1

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);    // x > 5
    EXPECT_EQ(resVec->GetValue(1), 200);  // NULL condition -> else
    EXPECT_EQ(resVec->GetValue(2), 30);   // x >= 0

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprConstConditions)
{
    // Constant conditions take the all-true / all-false paths without per-row
    // work: the first case (const true) covers every row.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new LiteralExpr(true, boolType), new FieldExpr(1, intType));
    whenClause.emplace_back(new LiteralExpr(false, boolType), new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {1, 2, 3};
    int32_t a[rowSize] = {10, 20, 30};
    int32_t b[rowSize] = {40, 50, 60};
    int32_t c[rowSize] = {70, 80, 90};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_EQ(resVec->GetValue(i), a[i]);
    }

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprComputedResults)
{
    // Computed results run compacted over their selected rows only.
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* then1 = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(1, intType),
        new LiteralExpr(static_cast<int32_t>(1), intType), intType);
    auto* then2 = new BinaryExpr(expressions::Operator::SUB, new FieldExpr(2, intType),
        new LiteralExpr(static_cast<int32_t>(1), intType), intType);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new FieldExpr(0, boolType), then1);
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, boolType),
        new LiteralExpr(false, boolType), boolType), then2);
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    int32_t a[rowSize] = {1, 2, 3, 4};
    int32_t b[rowSize] = {10, 20, 30, 40};
    int32_t c[rowSize] = {100, 200, 300, 400};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 2);    // a[0] + 1
    EXPECT_EQ(resVec->GetValue(1), 19);   // b[1] - 1
    EXPECT_EQ(resVec->GetValue(2), 4);    // a[2] + 1
    EXPECT_EQ(resVec->GetValue(3), 39);   // b[3] - 1

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprStringResults)
{
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new FieldExpr(0, boolType), new FieldExpr(1, strType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, boolType),
        new LiteralExpr(false, boolType), boolType), new FieldExpr(2, strType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, strType));

    std::vector vecOfTypes = {BooleanType(), VarcharType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    std::string a[rowSize] = {"aa", "bb", "cc", "dd"};
    std::string b[rowSize] = {"ee", "ff", "gg", "hh"};
    std::string c[rowSize] = {"ii", "jj", "kk", "ll"};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), "aa");
    EXPECT_EQ(resVec->GetValue(1), "ff");
    EXPECT_EQ(resVec->GetValue(2), "cc");
    EXPECT_EQ(resVec->GetValue(3), "hh");

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprArrayResults)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(std::make_shared<DataType>(OMNI_INT));

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new FieldExpr(0, boolType), new FieldExpr(1, arrayType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, boolType),
        new LiteralExpr(false, boolType), boolType), new FieldExpr(2, arrayType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, arrayType));

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateBooleanVector({true, false, true}));
    input->Append(CreateArrayVector({{1, 2}, {3}, {4, 5, 6}}));
    input->Append(CreateArrayVector({{9}, {8, 7}, {6}}));
    input->Append(CreateArrayVector({{-1}, {-2}, {-3}}));

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resArr = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(resArr, nullptr);
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(0)), 2);  // case 1: {1, 2}
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(1)), 2);  // case 2: {8, 7}
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(2)), 3);  // case 1: {4, 5, 6}

    BaseVector* row1 = resArr->GetValue(1);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row1)->GetValue(0), 8);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row1)->GetValue(1), 7);
    delete row1;

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprUnderFilter)
{
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* then1 = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(1, intType),
        new LiteralExpr(static_cast<int32_t>(1), intType), intType);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new FieldExpr(0, boolType), then1);
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, boolType),
        new LiteralExpr(false, boolType), boolType), new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    int32_t a[rowSize] = {1, 2, 3, 4};
    int32_t b[rowSize] = {10, 20, 30, 40};
    int32_t c[rowSize] = {100, 200, 300, 400};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b, c);

    bool mask[rowSize] = {true, true, false, false};
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    context.hasFilter = true;
    context.SetIsSelectRow(mask);

    ExprEval e(input, &context);
    e.VisitExpr(*switchExpr);
    auto* result = e.GetResult();

    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetSize(), 2);
    EXPECT_EQ(resVec->GetValue(0), 2);    // row 0 selected: cond true -> a[0] + 1
    EXPECT_EQ(resVec->GetValue(1), 20);   // row 1 selected: cond false -> b[1]
    EXPECT_TRUE(context.hasFilter);
    EXPECT_EQ(context.GetIsSelectRow(), static_cast<bool*>(mask));

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprNoElseNullFill)
{
    // CASE without ELSE: unmatched rows are NULL.
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new FieldExpr(0, boolType), new FieldExpr(1, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(1, intType));
    delete switchExpr->falseExpr;
    switchExpr->falseExpr = nullptr;   // strip the else clause; dataType stays int

    std::vector vecOfTypes = {BooleanType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true};
    int32_t a[rowSize] = {1, 2, 3};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);
    EXPECT_TRUE(resVec->IsNull(1));
    EXPECT_EQ(resVec->GetValue(2), 3);

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprEmptyCaseList)
{
    // CASE with no when clauses: every row takes the else clause.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(1, intType));

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t a[rowSize] = {1, 2, 3};
    int32_t b[rowSize] = {10, 20, 30};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, a, b);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_EQ(resVec->GetValue(i), b[i]);
    }

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprNullPropagation)
{
    // case when x > 5 then x when x > 2 then x else x end: every result is the
    // shared column x, so the null-propagation fast path applies. Rows where x
    // is null must be null in the result and conditions must not run on them.
    const int32_t rowSize = 4;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GT, new FieldExpr(0, intType),
        new LiteralExpr(5, intType), boolType), new FieldExpr(0, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GT, new FieldExpr(0, intType),
        new LiteralExpr(2, intType), boolType), new FieldExpr(0, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(0, intType));

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {7, 3, 0, 10};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x);
    input->Get(0)->SetNull(2);   // NULL at row 2

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 7);    // x > 5
    EXPECT_EQ(resVec->GetValue(1), 3);    // x > 2
    EXPECT_TRUE(resVec->IsNull(2));       // NULL x -> NULL result
    EXPECT_EQ(resVec->GetValue(3), 10);   // x > 5

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprCrossColumnConditionsSlowPath)
{
    // The null-propagation analysis rejects conditions referencing columns
    // other than the shared result column; the slow path must still be
    // correct, including for NULL conditions.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GT, new FieldExpr(1, intType),
        new LiteralExpr(0, intType), boolType), new FieldExpr(0, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(0, intType));

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {1, 2, 3};
    int32_t y[rowSize] = {5, 0, -1};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, y);
    input->Get(1)->SetNull(1);   // NULL condition at row 1

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);    // y > 0 -> x
    EXPECT_EQ(resVec->GetValue(1), 2);    // NULL condition -> else -> x
    EXPECT_EQ(resVec->GetValue(2), 3);    // y <= 0 -> else -> x

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprNested)
{
    // outer: case when c0 then (case when c1 then a else b end) else c end
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);

    std::vector<std::pair<Expr *, Expr *>> innerWhens;
    innerWhens.emplace_back(new FieldExpr(1, boolType), new FieldExpr(2, intType));
    auto* inner = new SwitchExpr(innerWhens, new FieldExpr(3, intType));

    std::vector<std::pair<Expr *, Expr *>> outerWhens;
    outerWhens.emplace_back(new FieldExpr(0, boolType), inner);
    auto* outer = new SwitchExpr(outerWhens, new FieldExpr(4, intType));

    std::vector vecOfTypes = {BooleanType(), BooleanType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool c0[rowSize] = {true, true, false};
    bool c1[rowSize] = {true, false, true};
    int32_t a[rowSize] = {1, 2, 3};
    int32_t b[rowSize] = {10, 20, 30};
    int32_t c[rowSize] = {100, 200, 300};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, c0, c1, a, b, c);

    auto* result = RunSwitchExpr(outer, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);    // c0 -> inner -> a
    EXPECT_EQ(resVec->GetValue(1), 20);   // c0 -> inner -> b
    EXPECT_EQ(resVec->GetValue(2), 300);  // else -> c

    delete result;
    delete input;
    delete outer;
}

TEST(SwitchTest, LazySwitchExprInsideLambda)
{
    // transform(arr, x -> case when x > 3 then x else 0 end): exercises lazy
    // switch evaluation over the lambda input space.
    int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({intType});
    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offset = {0, 3, 4, 5};
    auto* input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);
    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    ParamRefExpr* param = new ParamRefExpr("x", IntType());
    BinaryExpr* cond = new BinaryExpr(expressions::Operator::GT, param, new LiteralExpr(3, IntType()), boolType);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    // A distinct ParamRefExpr node per AST position (same as a real parse).
    whenClause.emplace_back(cond, new ParamRefExpr("x", IntType()));
    auto* switchBody = new SwitchExpr(whenClause, new LiteralExpr(0, IntType()));
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    auto* expr = new FuncExpr("transform", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LambdaExpr(switchBody, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    ExprEval e(input, &context);
    e.VisitExpr(*expr);
    auto* result = e.GetResult();

    auto* arrVec = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(arrVec, nullptr);
    auto* element = dynamic_cast<Vector<int32_t>*>(arrVec->GetElementVector().get());
    ASSERT_NE(element, nullptr);
    int32_t expect[] = {0, 0, 0, 4, 5};
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(element->GetValue(i), expect[i]);
    }

    delete input;
    delete result;
    delete expr;
}

TEST(SwitchTest, LazySwitchExprInsideLambdaUnderFilter)
{
    // Filtered batch + CASE inside a lambda: the lambda space is the flattened
    // elements (6 > 3 batch rows), so the batch filter mask must not be applied
    // to it — it is defined over batch rows only. Regresses the mask over-read
    // and the out-of-bounds branch selection of the lazy-switch filtered path.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({intType});
    int32_t col[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int32_t> offset = {0, 6, 7, 8};
    auto* input = CreateArrayVectorBatch(types, offset, rowSize, 8, col);
    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    ParamRefExpr* param = new ParamRefExpr("x", IntType());
    BinaryExpr* cond = new BinaryExpr(expressions::Operator::GT, param, new LiteralExpr(3, IntType()), boolType);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    // A distinct ParamRefExpr node per AST position (same as a real parse).
    whenClause.emplace_back(cond, new ParamRefExpr("x", IntType()));
    auto* switchBody = new SwitchExpr(whenClause, new LiteralExpr(0, IntType()));
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    auto* expr = new FuncExpr("transform", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LambdaExpr(switchBody, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    bool mask[rowSize] = {true, false, false};
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    context.hasFilter = true;
    context.SetIsSelectRow(mask);

    ExprEval e(input, &context);
    e.VisitExpr(*expr);
    auto* result = e.GetResult();

    auto* arrVec = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(arrVec, nullptr);
    EXPECT_EQ(arrVec->GetSize(), 1);   // only row 0 passes the batch filter
    auto* element = dynamic_cast<Vector<int32_t>*>(arrVec->GetElementVector().get());
    ASSERT_NE(element, nullptr);
    int32_t expect[] = {0, 0, 0, 4, 5, 6};
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(element->GetValue(i), expect[i]);
    }

    delete input;
    delete result;
    delete expr;
}

namespace {
// Struct column with three int fields (x, a, b). Per-field nulls are set on
// the children directly; the struct rows themselves are non-null.
RowVector* CreateStructCol(const std::vector<int32_t>& x, const std::vector<int32_t>& a,
    const std::vector<int32_t>& b)
{
    int32_t size = x.size();
    auto* rowVec = new RowVector(size);
    auto* xChild = VectorHelper::CreateFlatVector(OMNI_INT, size);
    auto* aChild = VectorHelper::CreateFlatVector(OMNI_INT, size);
    auto* bChild = VectorHelper::CreateFlatVector(OMNI_INT, size);
    for (int32_t i = 0; i < size; ++i) {
        static_cast<Vector<int32_t>*>(xChild)->SetValue(i, x[i]);
        static_cast<Vector<int32_t>*>(aChild)->SetValue(i, a[i]);
        static_cast<Vector<int32_t>*>(bChild)->SetValue(i, b[i]);
    }
    rowVec->AddChild(xChild);
    rowVec->AddChild(aChild);
    rowVec->AddChild(bChild);
    return rowVec;
}
}

TEST(SwitchTest, LazySwitchExprStructFieldsSlowPath)
{
    // case when s.x > 0 then s.a else s.b end: the results are different fields
    // of the same struct, so the null-propagation analysis must reject the fast
    // path (their per-field nulls differ per row). Row 2 has s.a null while
    // s.b is set; the result must come from s.b, not from a sealed null.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    std::vector<std::shared_ptr<DataType>> rowFields = {std::make_shared<DataType>(OMNI_INT),
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    auto rowType = std::make_shared<RowType>(rowFields);

    Expr* cond = new BinaryExpr(expressions::Operator::GT,
        new FieldExpr(0, intType, -1, new FieldExpr(0, rowType)),
        new LiteralExpr(0, intType), boolType);
    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(cond, new FieldExpr(1, intType, -1, new FieldExpr(0, rowType)));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(2, intType, -1, new FieldExpr(0, rowType)));

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateStructCol({5, -1, -1}, {10, 20, 0}, {100, 200, 300}));
    static_cast<RowVector*>(input->Get(0))->ChildAt(1).get()->SetNull(2);   // s.a is NULL at row 2

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 10);    // s.x > 0 -> s.a
    EXPECT_EQ(resVec->GetValue(1), 200);   // else -> s.b
    EXPECT_EQ(resVec->GetValue(2), 300);   // s.a null but s.x <= 0 -> s.b

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprStructFieldsNullProp)
{
    // case when s.x > 0 then s.a else s.a end: every result is the same field
    // path, so the fast path applies and rows where s.a is null are sealed as
    // NULL without evaluating the condition.
    const int32_t rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    std::vector<std::shared_ptr<DataType>> rowFields = {std::make_shared<DataType>(OMNI_INT),
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    auto rowType = std::make_shared<RowType>(rowFields);

    Expr* cond = new BinaryExpr(expressions::Operator::GT,
        new FieldExpr(0, intType, -1, new FieldExpr(0, rowType)),
        new LiteralExpr(0, intType), boolType);
    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(cond, new FieldExpr(1, intType, -1, new FieldExpr(0, rowType)));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(1, intType, -1, new FieldExpr(0, rowType)));

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateStructCol({5, -1, -1}, {10, 20, 0}, {100, 200, 300}));
    static_cast<RowVector*>(input->Get(0))->ChildAt(1).get()->SetNull(2);   // s.a is NULL at row 2

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 10);    // s.x > 0 -> s.a
    EXPECT_EQ(resVec->GetValue(1), 20);    // else -> s.a
    EXPECT_TRUE(resVec->IsNull(2));        // s.a null -> sealed NULL

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprEqFieldConstString)
{
    // case when s = 'a' then x when s = 'b' then y else z end: every condition
    // is `field = literal` over one varchar field, so the allocation-free
    // scalar fast path runs. A NULL field value never matches and falls to the
    // else clause.
    const int32_t rowSize = 4;
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, strType),
        new LiteralExpr(new std::string("a"), strType), boolType), new FieldExpr(1, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, strType),
        new LiteralExpr(new std::string("b"), strType), boolType), new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {VarcharType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    std::string s[rowSize] = {"a", "b", "c", ""};
    int32_t x[rowSize] = {1, 2, 3, 4};
    int32_t y[rowSize] = {10, 20, 30, 40};
    int32_t z[rowSize] = {100, 200, 300, 400};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, s, x, y, z);
    input->Get(0)->SetNull(3);   // NULL at row 3

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);    // 'a' -> x
    EXPECT_EQ(resVec->GetValue(1), 20);   // 'b' -> y
    EXPECT_EQ(resVec->GetValue(2), 300);  // else -> z
    EXPECT_EQ(resVec->GetValue(3), 400);  // NULL -> else -> z

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprEqFieldConstInt)
{
    // case when x = 1 then a when x = 2 then b when x = 3 then c else d end:
    // int field, scalar fast path.
    const int32_t rowSize = 5;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, intType),
        new LiteralExpr(1, intType), boolType), new FieldExpr(1, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, intType),
        new LiteralExpr(2, intType), boolType), new FieldExpr(2, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, intType),
        new LiteralExpr(3, intType), boolType), new FieldExpr(3, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(4, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {3, 1, 9, 2, 1};
    int32_t a[rowSize] = {10, 20, 30, 40, 50};
    int32_t b[rowSize] = {110, 120, 130, 140, 150};
    int32_t c[rowSize] = {210, 220, 230, 240, 250};
    int32_t d[rowSize] = {310, 320, 330, 340, 350};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b, c, d);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 210);   // 3 -> c
    EXPECT_EQ(resVec->GetValue(1), 20);    // 1 -> a
    EXPECT_EQ(resVec->GetValue(2), 330);   // 9 -> else -> d
    EXPECT_EQ(resVec->GetValue(3), 140);   // 2 -> b
    EXPECT_EQ(resVec->GetValue(4), 50);    // 1 -> a

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprEqFieldConstNullLiteral)
{
    // `WHEN x = NULL` never matches (SQL semantics); the row falls through.
    const int32_t rowSize = 2;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, intType),
        new LiteralExpr(0, intType, true), boolType), new FieldExpr(1, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(2, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {5, 0};
    int32_t a[rowSize] = {1, 2};
    int32_t b[rowSize] = {10, 20};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 10);   // x = NULL does not match -> else
    EXPECT_EQ(resVec->GetValue(1), 20);   // x = NULL does not match -> else

    delete result;
    delete input;
    delete switchExpr;
}

TEST(SwitchTest, LazySwitchExprEqFieldConstMixedFallsBack)
{
    // Mixed conditions (EQ(field, literal) and a range comparison) must fall
    // back to the general vectorized path and stay correct.
    const int32_t rowSize = 4;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::EQ, new FieldExpr(0, intType),
        new LiteralExpr(1, intType), boolType), new FieldExpr(1, intType));
    whenClause.emplace_back(new BinaryExpr(expressions::Operator::GT, new FieldExpr(0, intType),
        new LiteralExpr(5, intType), boolType), new FieldExpr(2, intType));
    auto* switchExpr = new SwitchExpr(whenClause, new FieldExpr(3, intType));

    std::vector vecOfTypes = {IntType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t x[rowSize] = {1, 7, 3, 0};
    int32_t a[rowSize] = {10, 20, 30, 40};
    int32_t b[rowSize] = {110, 120, 130, 140};
    int32_t c[rowSize] = {210, 220, 230, 240};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, x, a, b, c);

    auto* result = RunSwitchExpr(switchExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 10);    // x = 1 -> a
    EXPECT_EQ(resVec->GetValue(1), 120);   // x > 5 -> b
    EXPECT_EQ(resVec->GetValue(2), 230);   // else -> c
    EXPECT_EQ(resVec->GetValue(3), 240);   // else -> c

    delete result;
    delete input;
    delete switchExpr;
}
