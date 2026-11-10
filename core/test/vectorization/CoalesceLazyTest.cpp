/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Lazy CoalesceExpr evaluation unit tests
 *
 * Covers ExprEval::Visit(CoalesceExpr) lazy evaluation: each argument runs
 * only on the rows where every earlier argument was NULL, first non-null
 * wins, and rows still NULL after all arguments stay NULL. The manual
 * first-non-null merge over individually evaluated arguments serves as the
 * reference (the pre-lazy eager behavior).
 */

#include <gtest/gtest.h>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/Coalesce.h"
#include "vectorization/functions/If.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "expression/expressions.h"
#include "operator/execution_context.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::expressions;

namespace {

class CoalesceLazyTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const coalesce_lazy_test_env =
    ::testing::AddGlobalTestEnvironment(new CoalesceLazyTestEnvironment);

DataTypePtr TestIntType()
{
    return std::make_shared<DataType>(OMNI_INT);
}

DataTypePtr TestStrType()
{
    return std::make_shared<DataType>(OMNI_VARCHAR);
}

/// int vector with a null sentinel: INT32_MIN = null.
BaseVector* CreateTriIntVector(const std::vector<int32_t>& vals)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, vals.size());
    auto* typed = static_cast<Vector<int32_t>*>(vec);
    for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] == INT32_MIN) {
            typed->SetNull(i);
        } else {
            typed->SetValue(i, vals[i]);
        }
    }
    return vec;
}

/// string vector; empty string marker "-" = null.
BaseVector* CreateTriStrVector(const std::vector<std::string>& vals)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, vals.size());
    auto* typed = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
    for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] == "-") {
            typed->SetNull(i);
        } else {
            typed->SetValue(i, std::string_view(vals[i]));
        }
    }
    return vec;
}

/// DATE64 vector; INT64_MIN = null.
BaseVector* CreateTriDate64Vector(const std::vector<int64_t>& vals)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_DATE64, vals.size());
    auto* typed = static_cast<Vector<int64_t>*>(vec);
    for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] == INT64_MIN) {
            typed->SetNull(i);
        } else {
            typed->SetValue(i, vals[i]);
        }
    }
    return vec;
}

DataTypePtr TestDate64Type()
{
    return std::make_shared<DataType>(OMNI_DATE64);
}

/// Evaluates the coalesce expression through ExprEval (lazy path).
BaseVector* EvalLazyCoalesce(const CoalesceExpr* expr, VectorBatch* batch, int32_t n)
{
    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(false);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    auto* result = e.GetResult();
    delete context;
    return result;
}

/// Reference: evaluates each argument individually and merges first non-null
/// per row — exactly the eager semantics the lazy path replaces. Int-typed.
BaseVector* EvalManualCoalesce(CoalesceExpr* expr, VectorBatch* batch, int32_t n)
{
    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(false);

    // Flatten the (possibly nested) CoalesceExpr chain into its leaf arguments.
    std::vector<Expr*> leaves;
    std::function<void(Expr*)> flatten = [&](Expr* node) {
        if (node->GetType() == ExprType::COALESCE_E) {
            for (Expr* arg : static_cast<CoalesceExpr*>(node)->arguments) {
                flatten(arg);
            }
        } else {
            leaves.push_back(node);
        }
    };
    flatten(expr);

    BaseVector* result = VectorHelper::CreateFlatVector(OMNI_INT, n);
    auto* typed = static_cast<Vector<int32_t>*>(result);
    std::vector<uint8_t> decided(n, 0);
    for (Expr* leaf : leaves) {
        ExprEval e(batch, context);
        e.VisitExpr(*leaf);
        BaseVector* argVec = e.GetResult();
        for (int32_t i = 0; i < n; ++i) {
            if (!decided[i] && !argVec->IsNull(i)) {
                decided[i] = 1;
                typed->SetValue(i, VectorHelper::GetValueFromVector<int32_t>(argVec, i));
            }
        }
        delete argVec;
    }
    for (int32_t i = 0; i < n; ++i) {
        if (!decided[i]) {
            result->SetNull(i);
        }
    }
    delete context;
    return result;
}

void ExpectIntVecEqual(BaseVector* actual, const std::vector<int32_t>& expected,
    const std::vector<bool>& expectedNull, int32_t n)
{
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_EQ(actual->IsNull(i), expectedNull[i]) << "row " << i;
        if (!expectedNull[i]) {
            EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(actual, i), expected[i]) << "row " << i;
        }
    }
}

void ExpectLazyMatchesManual(CoalesceExpr* expr, VectorBatch* batch, int32_t n)
{
    BaseVector* lazy = EvalLazyCoalesce(expr, batch, n);
    BaseVector* manual = EvalManualCoalesce(expr, batch, n);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_EQ(lazy->IsNull(i), manual->IsNull(i)) << "null mismatch at row " << i;
        if (!lazy->IsNull(i)) {
            EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(lazy, i),
                VectorHelper::GetValueFromVector<int32_t>(manual, i)) << "value mismatch at row " << i;
        }
    }
    delete lazy;
    delete manual;
}

} // namespace

TEST(CoalesceLazyTest, TwoArgIntWithNulls)
{
    const int32_t n = 6;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({1, INT32_MIN, 3, INT32_MIN, INT32_MIN, 6}));
    batch->Append(CreateTriIntVector({INT32_MIN, 2, INT32_MIN, 4, 5, INT32_MIN}));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestIntType()), new FieldExpr(1, TestIntType()));
    ASSERT_NE(expr->vectorFunction, nullptr);

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    ExpectIntVecEqual(result, {1, 2, 3, 4, 5, 6},
        {false, false, false, false, false, false}, n);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, AllNullArguments)
{
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN}));
    batch->Append(CreateTriIntVector({INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN}));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestIntType()), new FieldExpr(1, TestIntType()));

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_TRUE(result->IsNull(i)) << "row " << i << " should stay NULL";
    }
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, ThreeArgNestedChain)
{
    // coalesce(a, coalesce(b, c)) — the inner lazy coalesce runs inside the
    // outer one's compacted space; must compose.
    const int32_t n = 6;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({1, INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN}));
    batch->Append(CreateTriIntVector({9, 2, INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN}));
    batch->Append(CreateTriIntVector({8, 7, 3, INT32_MIN, 5, INT32_MIN}));
    auto* inner = new CoalesceExpr(new FieldExpr(1, TestIntType()), new FieldExpr(2, TestIntType()));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestIntType()), inner);

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    // row0: 1; row1: a null -> b=2; row2: a,b null -> c=3; row3: all null; row4: a,b null -> c=5; row5: all null
    ExpectIntVecEqual(result, {1, 2, 3, 0, 5, 0},
        {false, false, false, true, false, true}, n);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, ConstantNonNullArgumentDecides)
{
    // coalesce(a, 42): null rows of a fall through to the constant.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({1, INT32_MIN, 3, INT32_MIN}));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestIntType()), new LiteralExpr(42, TestIntType()));

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    ExpectIntVecEqual(result, {1, 42, 3, 42}, {false, false, false, false}, n);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, ConstantNullArgumentIsNoop)
{
    // coalesce(NULL, a) = a.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({1, INT32_MIN, 3, 4}));
    auto* expr = new CoalesceExpr(new LiteralExpr(0, TestIntType(), true), new FieldExpr(0, TestIntType()));

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    ExpectIntVecEqual(result, {1, 0, 3, 4}, {false, true, false, false}, n);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, StringCoalesceWithNulls)
{
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriStrVector({"aa", "-", "cc", "-"}));
    batch->Append(CreateTriStrVector({"-", "bb", "-", "dd"}));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestStrType()), new FieldExpr(1, TestStrType()));
    ASSERT_NE(expr->vectorFunction, nullptr);

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    auto* typed = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    const std::string expected[4] = {"aa", "bb", "cc", "dd"};
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "row " << i;
        EXPECT_EQ(std::string(typed->GetValue(i)), expected[i]) << "row " << i;
    }
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, Date64PartialDecideProjectsBranchRows)
{
    // ProjectRows must handle OMNI_DATE64: a non-cheap IF branch deciding a
    // proper subset of the remaining rows hits it; before the fix it threw
    // "Unsupported coalesce type Id" here.
    const int32_t n = 6;
    auto* batch = new VectorBatch(n);
    // Rows 1, 3, 5 decided by the cheap first argument; rows 0, 2, 4 remain.
    batch->Append(CreateTriDate64Vector({INT64_MIN, 11, INT64_MIN, 13, INT64_MIN, 15}));
    // On the remaining rows the IF branch is non-null at 0 and 4, null at 2 —
    // a proper subset of the evaluated rows, so ProjectRows must compact it.
    batch->Append(CreateTriDate64Vector({21, 0, INT64_MIN, 0, 24, 0}));
    auto* branch = new IfExpr(new LiteralExpr(true, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(1, TestDate64Type()), new FieldExpr(1, TestDate64Type()));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestDate64Type()), branch);

    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->GetSize(), n);
    auto* typed = static_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(typed->GetValue(0), 21);
    EXPECT_EQ(typed->GetValue(1), 11);
    EXPECT_TRUE(result->IsNull(2));
    EXPECT_EQ(typed->GetValue(3), 13);
    EXPECT_EQ(typed->GetValue(4), 24);
    EXPECT_EQ(typed->GetValue(5), 15);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, EmptyBatch)
{
    // An empty batch has no columns, so use literal arguments (no field reads).
    const int32_t n = 0;
    auto* batch = new VectorBatch(n);
    auto* expr = new CoalesceExpr(new LiteralExpr(1, TestIntType()), new LiteralExpr(2, TestIntType()));
    BaseVector* result = EvalLazyCoalesce(expr, batch, n);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->GetSize(), 0);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, UnderOuterFilterProjectsSelectedRows)
{
    // Projection-style coalesce under an operator filter: Act 0 normalizes the
    // selected rows into a compacted space; the result is sized to them.
    const int32_t n = 6;
    bool mask[n] = {true, false, true, true, false, true};
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({1, 0, INT32_MIN, 4, 0, INT32_MIN}));
    batch->Append(CreateTriIntVector({9, 0, 3, 9, 0, 6}));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestIntType()), new FieldExpr(1, TestIntType()));

    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(false);
    context->hasFilter = true;
    context->SetIsSelectRow(mask);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    delete context;

    // Selected rows in order: 0, 2, 3, 5 -> coalesce = 1, 3, 4, 6.
    EXPECT_EQ(result->GetSize(), 4);
    ExpectIntVecEqual(result, {1, 3, 4, 6}, {false, false, false, false}, 4);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, ThrowOnErrorGateStillCorrect)
{
    // With throw-on-error enabled the lazy path is gated off and the eager
    // path runs; the result must be identical.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriIntVector({1, INT32_MIN, 3, INT32_MIN}));
    batch->Append(CreateTriIntVector({9, 2, 9, 4}));
    auto* expr = new CoalesceExpr(new FieldExpr(0, TestIntType()), new FieldExpr(1, TestIntType()));

    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(true);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    delete context;
    ExpectIntVecEqual(result, {1, 2, 3, 4}, {false, false, false, false}, n);
    delete result;
    delete expr;
    delete batch;
}

TEST(CoalesceLazyTest, PropertyRandomMatchesManual)
{
    std::mt19937 rng(20261027);
    std::uniform_int_distribution<int32_t> valDist(0, 19);

    for (int32_t iter = 0; iter < 150; ++iter) {
        const int32_t n = 1 + valDist(rng) % 48;
        const int32_t k = 2 + valDist(rng) % 3;   // 2..4 arguments
        std::vector<std::vector<int32_t>> cols(k, std::vector<int32_t>(n));
        for (auto& col : cols) {
            for (int32_t i = 0; i < n; ++i) {
                int32_t v = valDist(rng);
                col[i] = v >= 14 ? INT32_MIN : v;   // ~30% nulls
            }
        }
        auto* batch = new VectorBatch(n);
        for (int32_t c = 0; c < k; ++c) {
            batch->Append(CreateTriIntVector(cols[c]));
        }
        // Build a left-deep chain like the parser: coalesce(a0, coalesce(a1, ...)).
        CoalesceExpr* expr = new CoalesceExpr(new FieldExpr(k - 2, TestIntType()), new FieldExpr(k - 1, TestIntType()));
        for (int32_t c = k - 3; c >= 0; --c) {
            expr = new CoalesceExpr(new FieldExpr(c, TestIntType()), expr);
        }
        ExpectLazyMatchesManual(expr, batch, n);
        delete expr;
        delete batch;
    }
}
