/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Lazy AND/OR (conjunct) evaluation unit tests
 *
 * Covers the lazy logical chain evaluation in ExprEval::VisitLazyLogical:
 * flattening, short-circuiting, three-valued logic, constant folding,
 * cost-based ordering, and the throw-on-error gate. A manual Kleene fold
 * reference (the eager AndFunction/OrFunction semantics) serves as the
 * bit-identical reference for every semantic comparison.
 */

#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/ConjunctCost.h"
#include "vectorization/ExprEval.h"
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

// omniruntime::op also declares a class named Operator; qualify the
// expression operator enum through this alias everywhere.
namespace exprs = omniruntime::expressions;

class ConjunctTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const conjunct_test_env =
    ::testing::AddGlobalTestEnvironment(new ConjunctTestEnvironment);

DataTypePtr BoolType()
{
    return std::make_shared<DataType>(OMNI_BOOLEAN);
}

/// Builds a boolean vector from a tri-state encoding: 0 = false, 1 = true, -1 = null.
BaseVector* CreateTriBoolVector(const std::vector<int8_t>& vals)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, vals.size());
    auto* typed = static_cast<Vector<bool>*>(vec);
    for (size_t i = 0; i < vals.size(); ++i) {
        if (vals[i] < 0) {
            typed->SetNull(i);
        } else {
            typed->SetValue(i, vals[i] != 0);
        }
    }
    return vec;
}

/// Builds a left-deep same-op chain exactly like ParseJsonMultiAndOr does.
BinaryExpr* MakeChain(const std::vector<Expr*>& operands, exprs::Operator op)
{
    if (operands.size() < 2) {
        throw std::runtime_error("MakeChain requires at least 2 operands");
    }
    auto* root = new BinaryExpr(op, operands[0], operands[1], BoolType());
    for (size_t i = 2; i < operands.size(); ++i) {
        root = new BinaryExpr(op, root, operands[i], BoolType());
    }
    return root;
}

/// Lazy conjunct evaluation (the interpreter default) over the full batch.
BaseVector* EvalConjunctExpr(const Expr* expr, VectorBatch* batch, int32_t n)
{
    std::unique_ptr<ExecutionContext> context = std::make_unique<ExecutionContext>();
    context->SetResultRowSize(n);
    context->SetThrowOnError(false);
    ExprEval e(batch, context.get());
    e.VisitExpr(*expr);
    auto* result = e.GetResult();
    return result;
}

/// Leaves over the full batch; AND/OR folded per row with Kleene semantics.
BaseVector* EvalManualConjunctRec(const Expr* expr, VectorBatch* batch, int32_t n, ExecutionContext* context)
{
    if (expr->GetType() == exprs::ExprType::BINARY_E) {
        const auto* b = static_cast<const BinaryExpr*>(expr);
        if (b->op == exprs::Operator::AND || b->op == exprs::Operator::OR) {
            const bool isAnd = b->op == exprs::Operator::AND;
            BaseVector* left = EvalManualConjunctRec(b->left, batch, n, context);
            BaseVector* right = EvalManualConjunctRec(b->right, batch, n, context);
            BaseVector* result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, n);
            auto* typed = static_cast<Vector<bool>*>(result);
            for (int32_t i = 0; i < n; ++i) {
                const bool lNull = left->IsNull(i);
                const bool rNull = right->IsNull(i);
                const bool lTrue = !lNull && VectorHelper::GetValueFromVector<bool>(left, i);
                const bool rTrue = !rNull && VectorHelper::GetValueFromVector<bool>(right, i);
                const bool decided = isAnd ? ((!lNull && !lTrue) || (!rNull && !rTrue)) : (lTrue || rTrue);
                if (decided) {
                    typed->SetValue(i, !isAnd);
                } else if (lNull || rNull) {
                    typed->SetNull(i);
                } else {
                    typed->SetValue(i, isAnd);
                }
            }
            delete left;
            delete right;
            return result;
        }
    }
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    return e.GetResult();
}

/// Manual Kleene reference — the eager AndFunction/OrFunction semantics the
/// lazy path must reproduce bit-for-bit (NULL literals included).
BaseVector* EvalManualConjunct(const Expr* expr, VectorBatch* batch, int32_t n)
{
    ExecutionContext context;
    context.SetResultRowSize(n);
    context.SetThrowOnError(false);
    return EvalManualConjunctRec(expr, batch, n, &context);
}

void ExpectBoolVecEqual(BaseVector* actual, BaseVector* expected, int32_t n)
{
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_EQ(actual->IsNull(i), expected->IsNull(i)) << "null mismatch at row " << i;
        if (!actual->IsNull(i)) {
            EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(actual, i),
                VectorHelper::GetValueFromVector<bool>(expected, i)) << "value mismatch at row " << i;
        }
    }
}

/// Lazy path vs manual Kleene reference, bit-identical values and nulls.
void ExpectLazyMatchesEager(const char* name, Expr* expr, VectorBatch* batch, int32_t n)
{
    BaseVector* lazy = EvalConjunctExpr(expr, batch, n);
    BaseVector* eager = EvalManualConjunct(expr, batch, n);
    ExpectBoolVecEqual(lazy, eager, n);
    delete lazy;
    delete eager;
}

} // namespace

// ---- three-valued logic (Kleene) -------------------------------------------

TEST(ConjunctTest, KleeneAndTruthTable)
{
    // Columns take the three values in a fixed 3x3 pattern.
    std::vector<int8_t> left = {0, 0, 0, 1, 1, 1, -1, -1, -1};
    std::vector<int8_t> right = {0, 1, -1, 0, 1, -1, 0, 1, -1};
    // Expected Kleene AND results for the pairs above.
    std::vector<int8_t> expected = {0, 0, 0, 0, 1, -1, 0, -1, -1};

    auto* batch = new VectorBatch(9);
    batch->Append(CreateTriBoolVector(left));
    batch->Append(CreateTriBoolVector(right));
    auto* expr = MakeChain({new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType())}, exprs::Operator::AND);

    BaseVector* result = EvalConjunctExpr(expr, batch, 9);
    auto* resVec = static_cast<Vector<bool>*>(result);
    for (int32_t i = 0; i < 9; ++i) {
        if (expected[i] < 0) {
            EXPECT_TRUE(result->IsNull(i)) << "row " << i << " should be NULL";
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "row " << i << " should not be NULL";
            EXPECT_EQ(resVec->GetValue(i), expected[i] != 0) << "row " << i;
        }
    }
    delete result;
    delete batch;
    delete expr;
}

TEST(ConjunctTest, KleeneOrTruthTable)
{
    std::vector<int8_t> left = {0, 0, 0, 1, 1, 1, -1, -1, -1};
    std::vector<int8_t> right = {0, 1, -1, 0, 1, -1, 0, 1, -1};
    // Expected Kleene OR results.
    std::vector<int8_t> expected = {0, 1, -1, 1, 1, 1, -1, 1, -1};

    auto* batch = new VectorBatch(9);
    batch->Append(CreateTriBoolVector(left));
    batch->Append(CreateTriBoolVector(right));
    auto* expr = MakeChain({new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType())}, exprs::Operator::OR);

    BaseVector* result = EvalConjunctExpr(expr, batch, 9);
    auto* resVec = static_cast<Vector<bool>*>(result);
    for (int32_t i = 0; i < 9; ++i) {
        if (expected[i] < 0) {
            EXPECT_TRUE(result->IsNull(i)) << "row " << i << " should be NULL";
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "row " << i << " should not be NULL";
            EXPECT_EQ(resVec->GetValue(i), expected[i] != 0) << "row " << i;
        }
    }
    delete result;
    delete batch;
    delete expr;
}

TEST(ConjunctTest, KleeneNullPropagationThroughChain)
{
    // TRUE AND NULL AND FALSE = FALSE; TRUE AND NULL AND TRUE = NULL.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 1, 1, 1}));
    batch->Append(CreateTriBoolVector({-1, -1, -1, -1}));
    batch->Append(CreateTriBoolVector({0, 1, 0, 1}));
    auto* expr = MakeChain(
        {new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType()), new FieldExpr(2, BoolType())}, exprs::Operator::AND);

    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    auto* resVec = static_cast<Vector<bool>*>(result);
    EXPECT_FALSE(result->IsNull(0)) << "NULL then FALSE must decide FALSE";
    EXPECT_EQ(resVec->GetValue(0), false);
    EXPECT_TRUE(result->IsNull(1)) << "NULL then TRUE stays NULL";
    EXPECT_FALSE(result->IsNull(2));
    EXPECT_EQ(resVec->GetValue(2), false);
    EXPECT_TRUE(result->IsNull(3));

    delete result;
    delete batch;
    delete expr;
}

TEST(ConjunctTest, KleeneNullPropagationThroughOrChain)
{
    // FALSE OR NULL OR TRUE = TRUE; FALSE OR NULL OR FALSE = NULL.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({0, 0, 0, 0}));
    batch->Append(CreateTriBoolVector({-1, -1, -1, -1}));
    batch->Append(CreateTriBoolVector({1, 0, 1, 0}));
    auto* expr = MakeChain(
        {new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType()), new FieldExpr(2, BoolType())}, exprs::Operator::OR);

    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    auto* resVec = static_cast<Vector<bool>*>(result);
    EXPECT_FALSE(result->IsNull(0)) << "NULL then TRUE must decide TRUE";
    EXPECT_EQ(resVec->GetValue(0), true);
    EXPECT_TRUE(result->IsNull(1)) << "NULL then FALSE stays NULL";
    EXPECT_FALSE(result->IsNull(2));
    EXPECT_EQ(resVec->GetValue(2), true);
    EXPECT_TRUE(result->IsNull(3));

    delete result;
    delete batch;
    delete expr;
}

// ---- equivalence with the eager reference -----------------------------------

TEST(ConjunctTest, PropertyRandomChainsMatchEager)
{
    std::mt19937 rng(20261023);
    std::uniform_int_distribution<int32_t> sizeDist(1, 64);
    std::uniform_int_distribution<int32_t> valDist(0, 9);   // 0-6 true/false, 7-9 null

    for (int32_t iter = 0; iter < 200; ++iter) {
        int32_t n = sizeDist(rng);
        int32_t k = 2 + valDist(rng) % 5;   // 2..6 conjuncts
        std::vector<std::vector<int8_t>> cols(k, std::vector<int8_t>(n));
        for (auto& col : cols) {
            for (int32_t i = 0; i < n; ++i) {
                int32_t v = valDist(rng);
                col[i] = v >= 7 ? -1 : (v % 2);
            }
        }

        auto* batch = new VectorBatch(n);
        std::vector<Expr*> operands;
        for (int32_t c = 0; c < k; ++c) {
            batch->Append(CreateTriBoolVector(cols[c]));
            operands.push_back(new FieldExpr(c, BoolType()));
        }
        auto* andExpr = MakeChain(operands, exprs::Operator::AND);
        ExpectLazyMatchesEager("and", andExpr, batch, n);
        delete andExpr;

        // Same columns re-referenced by fresh field expressions for the OR chain.
        std::vector<Expr*> orOperands;
        for (int32_t c = 0; c < k; ++c) {
            orOperands.push_back(new FieldExpr(c, BoolType()));
        }
        auto* orExpr = MakeChain(orOperands, exprs::Operator::OR);
        ExpectLazyMatchesEager("or", orExpr, batch, n);
        delete orExpr;

        delete batch;
    }
}

TEST(ConjunctTest, PropertyMixedNestingMatchesEager)
{
    std::mt19937 rng(20261024);
    std::uniform_int_distribution<int32_t> valDist(0, 9);

    for (int32_t iter = 0; iter < 100; ++iter) {
        const int32_t n = 32;
        auto* batch = new VectorBatch(n);
        batch->Append(CreateTriBoolVector([&]() {
            std::vector<int8_t> col(n);
            for (int32_t i = 0; i < n; ++i) {
                int32_t v = valDist(rng);
                col[i] = v >= 7 ? -1 : (v % 2);
            }
            return col;
        }()));
        batch->Append(CreateTriBoolVector([&]() {
            std::vector<int8_t> col(n);
            for (int32_t i = 0; i < n; ++i) {
                int32_t v = valDist(rng);
                col[i] = v >= 7 ? -1 : (v % 2);
            }
            return col;
        }()));
        batch->Append(CreateTriBoolVector([&]() {
            std::vector<int8_t> col(n);
            for (int32_t i = 0; i < n; ++i) {
                int32_t v = valDist(rng);
                col[i] = v >= 7 ? -1 : (v % 2);
            }
            return col;
        }()));

        // (a AND b) OR c  and  (a OR b) AND c
        auto* orInner = new BinaryExpr(exprs::Operator::OR, new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType()),
            BoolType());
        auto* andInner = new BinaryExpr(exprs::Operator::AND, new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType()),
            BoolType());
        auto* expr1 = new BinaryExpr(exprs::Operator::AND, orInner, new FieldExpr(2, BoolType()), BoolType());
        auto* expr2 = new BinaryExpr(exprs::Operator::OR, andInner, new FieldExpr(2, BoolType()), BoolType());

        ExpectLazyMatchesEager("expr1", expr1, batch, n);
        ExpectLazyMatchesEager("expr2", expr2, batch, n);

        delete expr1;
        delete expr2;
        delete batch;
    }
}

TEST(ConjunctTest, PropertyIntComparisonChainsMatchEager)
{
    // Expensive (non-leaf) conjuncts take the compacted-evaluation path; the
    // reference stays bit-identical across mixed selectivities.
    std::mt19937 rng(20261025);
    std::uniform_int_distribution<int32_t> valDist(-1, 20);

    for (int32_t iter = 0; iter < 100; ++iter) {
        const int32_t n = 64;
        std::vector<int32_t> a(n);
        std::vector<int32_t> b(n);
        for (int32_t i = 0; i < n; ++i) {
            a[i] = valDist(rng);
            b[i] = valDist(rng);
        }
        auto intType = std::make_shared<DataType>(OMNI_INT);
        auto* batch = new VectorBatch(n);
        auto* aVec = static_cast<Vector<int32_t>*>(VectorHelper::CreateFlatVector(OMNI_INT, n));
        auto* bVec = static_cast<Vector<int32_t>*>(VectorHelper::CreateFlatVector(OMNI_INT, n));
        for (int32_t i = 0; i < n; ++i) {
            aVec->SetValue(i, a[i]);
            bVec->SetValue(i, b[i]);
        }
        batch->Append(aVec);
        batch->Append(bVec);

        auto* expr = MakeChain(
            {new BinaryExpr(exprs::Operator::GT, new FieldExpr(0, intType), new LiteralExpr(5, intType), BoolType()),
                new BinaryExpr(exprs::Operator::LT, new FieldExpr(1, intType), new LiteralExpr(10, intType), BoolType()),
                new BinaryExpr(exprs::Operator::NEQ, new FieldExpr(0, intType), new FieldExpr(1, intType), BoolType())},
            exprs::Operator::AND);

        ExpectLazyMatchesEager("intchain", expr, batch, n);
        delete expr;
        delete batch;
    }
}

TEST(ConjunctTest, DictionaryAndConstEncodingsCorrect)
{
    // Three operands so the chain takes the lazy loop, with a dictionary
    // column and a constant column among the conjuncts. No eager reference:
    // the eager AndFunction merge throws on dictionary encodings (its value
    // reader supports flat/const only) — the lazy loop resolves them
    // per-row through GetValueFromVector instead.
    const int32_t n = 8;
    auto* base = CreateTriBoolVector({1, 0, -1, 1, 0, -1, 1, 0});
    int32_t indices[n] = {7, 0, 1, 2, 3, 4, 5, 6};
    BaseVector* dict = VectorHelper::CreateDictionary(indices, n, static_cast<Vector<bool>*>(base), false);

    auto* batch = new VectorBatch(n);
    batch->Append(dict);                                        // {0, 1, 0, N, 1, 0, N, 1}
    batch->Append(new ConstVector<bool>(true, OMNI_BOOLEAN, n));   // all TRUE
    batch->Append(CreateTriBoolVector({0, 1, -1, 1, 0, 0, -1, 1}));
    auto* expr = MakeChain(
        {new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType()), new FieldExpr(2, BoolType())}, exprs::Operator::AND);

    // Expected AND of the three columns, row by row (N = null):
    std::vector<int8_t> expected = {0, 1, 0, -1, 0, 0, -1, 1};
    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    for (int32_t i = 0; i < n; ++i) {
        if (expected[i] < 0) {
            EXPECT_TRUE(result->IsNull(i)) << "row " << i << " should be NULL";
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "row " << i << " should not be NULL";
            EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(result, i), expected[i] != 0) << "row " << i;
        }
    }
    delete result;
    delete expr;
    delete batch;
    delete base;
}

// ---- constant folding and observable ordering ---------------------------------

TEST(ConjunctTest, ConstantFoldingDecidingConstRunsNothingElse)
{
    // FALSE AND <function that would throw> : the deciding constant is ordered
    // first, so the function is never evaluated. The eager path would evaluate
    // the function and throw.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 0, 1, 0}));
    auto* throwingFunc = new FuncExpr("no_such_conjunct_function",
        std::vector<Expr*> {new FieldExpr(0, BoolType())}, BoolType());
    auto* expr = MakeChain({throwingFunc, new LiteralExpr(false, BoolType())}, exprs::Operator::AND);

    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    auto* resVec = static_cast<Vector<bool>*>(result);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_FALSE(result->IsNull(i));
        EXPECT_EQ(resVec->GetValue(i), false);
    }
    delete result;
    delete batch;
    delete expr;
}

TEST(ConjunctTest, ThrowOnErrorGateDisablesLazyPath)
{
    // Throw-on-error gates the lazy path off; the eager path throws.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 0, 1, 0}));
    auto* throwingFunc = new FuncExpr("no_such_conjunct_function",
        std::vector<Expr*> {new FieldExpr(0, BoolType())}, BoolType());
    auto* expr = MakeChain({new LiteralExpr(false, BoolType()), throwingFunc}, exprs::Operator::AND);

    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(true);
    // try/catch so the cleanup below always runs.
    bool threw = false;
    try {
        ExprEval e(batch, context);
        e.VisitExpr(*expr);
    } catch (const omniruntime::exception::OmniException &) {
        threw = true;
    }
    EXPECT_TRUE(threw);
    delete context;
    delete batch;
    delete expr;
}

TEST(ConjunctTest, IdentityConstantsAreDropped)
{
    // col AND TRUE = col;  col OR FALSE = col.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 0, -1, 1}));
    auto* andExpr = MakeChain({new FieldExpr(0, BoolType()), new LiteralExpr(true, BoolType())}, exprs::Operator::AND);
    auto* orExpr = MakeChain({new FieldExpr(0, BoolType()), new LiteralExpr(false, BoolType())}, exprs::Operator::OR);

    BaseVector* andRes = EvalConjunctExpr(andExpr, batch, n);
    BaseVector* orRes = EvalConjunctExpr(orExpr, batch, n);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_EQ(andRes->IsNull(i), orRes->IsNull(i));
        if (!andRes->IsNull(i)) {
            EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(andRes, i),
                VectorHelper::GetValueFromVector<bool>(orRes, i));
        }
        EXPECT_EQ(andRes->IsNull(i), batch->Get(0)->IsNull(i));
        if (!andRes->IsNull(i)) {
            EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(andRes, i),
                VectorHelper::GetValueFromVector<bool>(batch->Get(0), i));
        }
    }
    delete andRes;
    delete orRes;
    delete andExpr;
    delete orExpr;
    delete batch;
}

TEST(ConjunctTest, ConstantNullConjunctTaints)
{
    // Kleene with a NULL literal: col AND NULL = (col = TRUE → NULL, col = FALSE
    // → FALSE); col OR NULL = (col = TRUE → TRUE, col = FALSE → NULL).
    // Note: the eager AndFunction path cannot represent this shape (its value
    // reader ignores nulls on constants), so the lazy loop's constant folding
    // is what makes these correct — no eager reference here.
    const int32_t n = 4;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 0, -1, 1}));
    auto nullLit = new LiteralExpr(0, BoolType(), true);
    auto* andExpr = MakeChain({new FieldExpr(0, BoolType()), nullLit}, exprs::Operator::AND);
    auto* orExpr = MakeChain({new FieldExpr(0, BoolType()), new LiteralExpr(0, BoolType(), true)}, exprs::Operator::OR);

    BaseVector* andRes = EvalConjunctExpr(andExpr, batch, n);
    EXPECT_TRUE(andRes->IsNull(0)) << "TRUE AND NULL = NULL";
    EXPECT_FALSE(andRes->IsNull(1));
    EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(andRes, 1), false) << "FALSE AND NULL = FALSE";
    EXPECT_TRUE(andRes->IsNull(2)) << "NULL AND NULL = NULL";
    EXPECT_TRUE(andRes->IsNull(3)) << "TRUE AND NULL = NULL";

    BaseVector* orRes = EvalConjunctExpr(orExpr, batch, n);
    EXPECT_FALSE(orRes->IsNull(0));
    EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(orRes, 0), true) << "TRUE OR NULL = TRUE";
    EXPECT_TRUE(orRes->IsNull(1)) << "FALSE OR NULL = NULL";
    EXPECT_TRUE(orRes->IsNull(2)) << "NULL OR NULL = NULL";
    EXPECT_FALSE(orRes->IsNull(3));
    EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(orRes, 3), true) << "TRUE OR NULL = TRUE";

    delete andRes;
    delete orRes;
    delete andExpr;
    delete orExpr;
    delete batch;
}

TEST(ConjunctTest, AllIdentityConstants)
{
    const int32_t n = 3;
    auto* batch = new VectorBatch(n);
    auto* expr = MakeChain(
        {new LiteralExpr(true, BoolType()), new LiteralExpr(true, BoolType())}, exprs::Operator::AND);
    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    auto* resVec = static_cast<Vector<bool>*>(result);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_FALSE(result->IsNull(i));
        EXPECT_EQ(resVec->GetValue(i), true);
    }
    delete result;
    delete expr;
    delete batch;
}

// ---- error semantics (default overflow-as-null configuration) ---------------

TEST(ConjunctTest, DivisionByZeroMatchesEager)
{
    // (1/a > 10) AND (a > 5) with a = 0 rows: the division errors degrade to
    // NULLs in the default configuration, and the lazy short-circuit must
    // produce exactly the eager NULL placement — in both plan orders.
    const int32_t n = 6;
    double aVals[n] = {0.0, 100.0, 0.0, 1.0, 20.0, 0.5};
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);

    auto* batch = new VectorBatch(n);
    auto* aVec = static_cast<Vector<double>*>(VectorHelper::CreateFlatVector(OMNI_DOUBLE, n));
    for (int32_t i = 0; i < n; ++i) {
        aVec->SetValue(i, aVals[i]);
    }
    batch->Append(aVec);

    auto makeDivGt10 = [&]() {
        return new BinaryExpr(exprs::Operator::GT,
            new BinaryExpr(exprs::Operator::DIV, new LiteralExpr(1.0, doubleType), new FieldExpr(0, doubleType), doubleType),
            new LiteralExpr(10.0, doubleType), BoolType());
    };
    auto makeAGt5 = [&]() {
        return new BinaryExpr(exprs::Operator::GT, new FieldExpr(0, doubleType), new LiteralExpr(5.0, doubleType), BoolType());
    };

    // (1/a > 10) AND (a > 5) — the plan order.
    auto* expr1 = MakeChain({makeDivGt10(), makeAGt5()}, exprs::Operator::AND);
    ExpectLazyMatchesEager("expr1", expr1, batch, n);
    delete expr1;

    // (a > 5) AND (1/a > 10) — the cheap-conjunct-first order.
    auto* expr2 = MakeChain({makeAGt5(), makeDivGt10()}, exprs::Operator::AND);
    ExpectLazyMatchesEager("expr2", expr2, batch, n);
    delete expr2;

    delete batch;
}

TEST(ConjunctTest, SingleConjunctAfterFolding)
{
    // AND(col, TRUE) folds to a single operand; the result must equal col.
    const int32_t n = 5;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 0, -1, 1, 0}));
    auto* expr = MakeChain({new FieldExpr(0, BoolType()), new LiteralExpr(true, BoolType())}, exprs::Operator::AND);
    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    for (int32_t i = 0; i < n; ++i) {
        EXPECT_EQ(result->IsNull(i), batch->Get(0)->IsNull(i));
        if (!result->IsNull(i)) {
            EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(result, i),
                VectorHelper::GetValueFromVector<bool>(batch->Get(0), i));
        }
    }
    delete result;
    delete expr;
    delete batch;
}

// ---- projection-style evaluation under an outer filter -----------------------

TEST(ConjunctTest, AndUnderOuterFilterProjectsSelectedRows)
{
    // A projection containing AND/OR after a filter: the evaluator runs with
    // hasFilter set and a row mask. The lazy path normalizes this into an
    // explicit compacted space (Act 0), so the result is sized to the
    // selected rows and computed only over them.
    const int32_t n = 6;
    bool mask[n] = {true, false, true, true, false, true};
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({1, 0, -1, 1, 0, -1}));
    batch->Append(CreateTriBoolVector({1, 1, 1, 0, 0, -1}));
    auto* expr = MakeChain({new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType())}, exprs::Operator::AND);

    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(false);
    context->hasFilter = true;
    context->SetIsSelectRow(mask);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    delete context;

    // Selected rows in order: 0, 2, 3, 5.
    int8_t expected[4] = {1, -1, 0, -1};   // AND of the two columns
    EXPECT_EQ(result->GetSize(), 4);
    auto* resVec = static_cast<Vector<bool>*>(result);
    for (int32_t i = 0; i < 4; ++i) {
        if (expected[i] < 0) {
            EXPECT_TRUE(result->IsNull(i)) << "row " << i;
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "row " << i;
            EXPECT_EQ(resVec->GetValue(i), expected[i] != 0) << "row " << i;
        }
    }
    delete result;
    delete batch;
    delete expr;
}

// ---- cheap-chain eager bail-out (tpcds_dip_investigation.md) -----------------
//
// q74 in the weekend1 TPCDS run regressed +15.3s vs the switch build: its
// final WHERE is a 9-way AND of join-key equalities — every operand costs
// 1-5, nothing the ordering can defer, so the lazy machinery only added
// per-conjunct scans and selectivity churn. The fix: all-cheap chains fold
// eagerly with a single Kleene merge (ExprEval::VisitLazyLogical bail-out).
// These tests pin the bail-out against the manual eager reference.

BaseVector* CreateIntVector(const std::vector<int32_t>& vals, const std::vector<bool>& nulls = {})
{
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, vals.size());
    auto* typed = static_cast<Vector<int32_t>*>(vec);
    for (size_t i = 0; i < vals.size(); ++i) {
        if (!nulls.empty() && nulls[i]) {
            typed->SetNull(i);
        } else {
            typed->SetValue(i, vals[i]);
        }
    }
    return vec;
}

/// A 9-way AND of cheap comparisons — q74's WHERE shape (join-key equalities
/// plus constant bounds), over deterministic data with NULLs mixed in.
std::pair<Expr*, VectorBatch*> MakeQ74ShapeChain(int32_t n, exprs::Operator op)
{
    auto* batch = new VectorBatch(n);
    for (int32_t c = 0; c < 9; ++c) {
        batch->Append(VectorHelper::CreateFlatVector(OMNI_INT, n));
    }
    for (int32_t i = 0; i < n; ++i) {
        auto set = [&](int32_t col, int32_t v) { static_cast<Vector<int32_t>*>(batch->Get(col))->SetValue(i, v); };
        set(0, i);
        set(1, i % 3);
        set(2, i % 7);
        set(3, 3);
        if (i % 5 == 3) {
            static_cast<Vector<int32_t>*>(batch->Get(3))->SetNull(i);
        }
        set(4, i % 2);
        set(5, i % 4);
        set(6, (i * 7) % 11);
        set(7, 5);
        set(8, i % 6);
    }
    auto intT = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> ops = {
        new BinaryExpr(exprs::Operator::EQ, new FieldExpr(0, intT), new FieldExpr(1, intT), BoolType()),
        new BinaryExpr(exprs::Operator::LT, new FieldExpr(2, intT), new LiteralExpr(5, intT), BoolType()),
        new BinaryExpr(exprs::Operator::EQ, new FieldExpr(3, intT), new LiteralExpr(3, intT), BoolType()),
        new BinaryExpr(exprs::Operator::GT, new FieldExpr(4, intT), new LiteralExpr(0, intT), BoolType()),
        new BinaryExpr(exprs::Operator::LTE, new FieldExpr(5, intT), new LiteralExpr(2, intT), BoolType()),
        new BinaryExpr(exprs::Operator::EQ, new FieldExpr(6, intT), new FieldExpr(8, intT), BoolType()),
        new BinaryExpr(exprs::Operator::GTE, new FieldExpr(7, intT), new LiteralExpr(5, intT), BoolType()),
        new BinaryExpr(exprs::Operator::NEQ, new FieldExpr(8, intT), new LiteralExpr(4, intT), BoolType()),
        new BinaryExpr(exprs::Operator::LT, new FieldExpr(0, intT), new LiteralExpr(30, intT), BoolType()),
    };
    return {MakeChain(ops, op), batch};
}

TEST(ConjunctTest, CheapComparisonAndChainMatchesEager)
{
    const int32_t n = 37;
    auto [expr, batch] = MakeQ74ShapeChain(n, exprs::Operator::AND);
    ExpectLazyMatchesEager("9-way cheap AND (q74 shape)", expr, batch, n);
    delete expr;
    delete batch;
}

TEST(ConjunctTest, CheapComparisonOrChainMatchesEager)
{
    const int32_t n = 37;
    auto [expr, batch] = MakeQ74ShapeChain(n, exprs::Operator::OR);
    ExpectLazyMatchesEager("9-way cheap OR", expr, batch, n);
    delete expr;
    delete batch;
}

TEST(ConjunctTest, CheapChainConstOperandsMatchEager)
{
    const int32_t n = 8;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({0, 1, -1, 0, 1, -1, 0, 1}));
    batch->Append(CreateTriBoolVector({1, 1, 1, 0, 0, 0, -1, -1}));
    auto boolT = BoolType();

    // FALSE decides the whole AND chain before any operand runs.
    auto* exprFalse = MakeChain(
        {new LiteralExpr(false, boolT), new FieldExpr(0, boolT), new FieldExpr(1, boolT)}, exprs::Operator::AND);
    ExpectLazyMatchesEager("deciding-const cheap AND", exprFalse, batch, n);
    delete exprFalse;

    // TRUE decides an OR chain.
    auto* exprTrue = MakeChain(
        {new LiteralExpr(true, boolT), new FieldExpr(0, boolT), new FieldExpr(1, boolT)}, exprs::Operator::OR);
    ExpectLazyMatchesEager("deciding-const cheap OR", exprTrue, batch, n);
    delete exprTrue;

    delete batch;
}

TEST(ConjunctTest, CheapChainNullConstOperandTaints)
{
    // NULL taints without deciding. Hand-computed Kleene expectations — no
    // eager reference: the eager AndFunction path cannot represent a NULL
    // literal operand (its value reader ignores nulls on constants; the
    // bail-out fold must stay correct where the eager path is not).
    const int32_t n = 8;
    auto* batch = new VectorBatch(n);
    batch->Append(CreateTriBoolVector({0, 1, -1, 0, 1, -1, 0, 1}));
    batch->Append(CreateTriBoolVector({1, 1, 1, 0, 0, 0, -1, -1}));
    auto boolT = BoolType();
    auto* expr = MakeChain(
        {new LiteralExpr(0, boolT, true), new FieldExpr(0, boolT), new FieldExpr(1, boolT)}, exprs::Operator::AND);

    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    // (NULL, f0, f1): FALSE decides → FALSE; otherwise NULL (TRUE/NULL stay undecided+tainted).
    // f0: 0,1,-1,0,1,-1,0,1 ; f1: 1,1,1,0,0,0,-1,-1
    int8_t expected[8] = {0, -1, -1, 0, 0, 0, 0, -1};
    for (int32_t i = 0; i < n; ++i) {
        if (expected[i] < 0) {
            EXPECT_TRUE(result->IsNull(i)) << "row " << i << " should be NULL";
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "row " << i << " should not be NULL";
            EXPECT_EQ(VectorHelper::GetValueFromVector<bool>(result, i), expected[i] != 0) << "row " << i;
        }
    }
    delete result;
    delete expr;
    delete batch;
}

TEST(ConjunctTest, CheapComparisonChainUnderOuterFilterProjectsSelectedRows)
{
    // The bail-out must also handle the compacted space under an outer filter
    // (Act 0): a cheap comparison chain evaluated on scattered selected rows.
    const int32_t n = 6;
    bool mask[n] = {true, false, true, true, false, true};
    auto* batch = new VectorBatch(n);
    batch->Append(CreateIntVector({1, 9, 3, 4, 9, 2}));
    batch->Append(CreateIntVector({2, 9, 3, 5, 9, 7}));
    batch->Append(CreateIntVector({1, 9, 0, 4, 9, 2}));
    auto intT = std::make_shared<DataType>(OMNI_INT);
    auto* expr = MakeChain(
        {new BinaryExpr(exprs::Operator::EQ, new FieldExpr(0, intT), new FieldExpr(2, intT), BoolType()),
            new BinaryExpr(exprs::Operator::GT, new FieldExpr(1, intT), new LiteralExpr(4, intT), BoolType())},
        exprs::Operator::AND);

    auto* context = new ExecutionContext();
    context->SetResultRowSize(n);
    context->SetThrowOnError(false);
    context->hasFilter = true;
    context->SetIsSelectRow(mask);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    delete context;

    // Selected rows in order: 0, 2, 3, 5 → (f0 = f2) AND (f1 > 4):
    // row0: 1=1 T, 2>4 F → F; row2: 3=0 F → F; row3: 4=4 T, 5>4 T → T; row5: 2=2 T, 7>4 T → T.
    int8_t expected[4] = {0, 0, 1, 1};
    EXPECT_EQ(result->GetSize(), 4);
    auto* resVec = static_cast<Vector<bool>*>(result);
    for (int32_t i = 0; i < 4; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "row " << i;
        EXPECT_EQ(resVec->GetValue(i), expected[i] != 0) << "row " << i;
    }
    delete result;
    delete batch;
    delete expr;
}

// ---- edge cases --------------------------------------------------------------

TEST(ConjunctTest, EmptyBatch)
{
    const int32_t n = 0;
    auto* batch = new VectorBatch(n);
    auto* expr = MakeChain(
        {new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType()), new FieldExpr(2, BoolType())}, exprs::Operator::AND);
    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->GetSize(), 0);
    delete result;
    delete expr;
    delete batch;
}

TEST(ConjunctTest, EmptyBatchOr)
{
    const int32_t n = 0;
    auto* batch = new VectorBatch(n);
    auto* expr = MakeChain({new FieldExpr(0, BoolType()), new FieldExpr(1, BoolType())}, exprs::Operator::OR);
    BaseVector* result = EvalConjunctExpr(expr, batch, n);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->GetSize(), 0);
    delete result;
    delete expr;
    delete batch;
}

// ---- cost estimator ----------------------------------------------------------

TEST(ConjunctTest, CostEstimatorOrdersCheapBeforeExpensive)
{
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto* field = new FieldExpr(0, strType);
    auto* rlike = new FuncExpr("RLike",
        std::vector<Expr*> {new FieldExpr(0, strType), new LiteralExpr(new std::string("x"), strType)}, BoolType());
    auto* cmp = new BinaryExpr(exprs::Operator::GT, new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new LiteralExpr(5, std::make_shared<DataType>(OMNI_INT)), BoolType());
    auto* div = new BinaryExpr(exprs::Operator::DIV, new LiteralExpr(1.0, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_DOUBLE)), std::make_shared<DataType>(OMNI_DOUBLE));
    auto* divCmp = new BinaryExpr(exprs::Operator::GT, div, new LiteralExpr(10.0, std::make_shared<DataType>(OMNI_DOUBLE)),
        BoolType());

    EXPECT_EQ(EstimateConjunctCost(field), 0);
    EXPECT_EQ(EstimateConjunctCost(cmp), 1);
    EXPECT_EQ(EstimateConjunctCost(divCmp), 4);   // DIV(3) + comparison(1)
    EXPECT_EQ(EstimateConjunctCost(rlike), 100);

    // The relative order drives reordering: cheap comparisons first, regex last.
    EXPECT_LT(EstimateConjunctCost(cmp), EstimateConjunctCost(divCmp));
    EXPECT_LT(EstimateConjunctCost(divCmp), EstimateConjunctCost(rlike));

    delete rlike;
    delete cmp;
    delete divCmp;
    delete field;
}

TEST(ConjunctTest, CostEstimatorSumsNestedChains)
{
    auto* a = new BinaryExpr(exprs::Operator::GT, new FieldExpr(0, BoolType()), new LiteralExpr(true, BoolType()), BoolType());
    auto* b = new BinaryExpr(exprs::Operator::GT, new FieldExpr(0, BoolType()), new LiteralExpr(true, BoolType()), BoolType());
    auto* chain = MakeChain({a, b}, exprs::Operator::AND);
    EXPECT_EQ(EstimateConjunctCost(chain), EstimateConjunctCost(a) + EstimateConjunctCost(b));
    delete chain;
}

TEST(ConjunctTest, CostEstimatorCountsCastSubtreeAndLike)
{
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto* regexp = new FuncExpr("regexp_extract",
        std::vector<Expr*> {new FieldExpr(0, strType), new LiteralExpr(new std::string("x"), strType)}, strType);
    auto* cast = new FuncExpr("CAST", std::vector<Expr*> {regexp}, strType);
    auto* like = new FuncExpr("LIKE",
        std::vector<Expr*> {new FieldExpr(0, strType), new LiteralExpr(new std::string("x%"), strType)}, BoolType());

    EXPECT_EQ(EstimateConjunctCost(regexp), 100);
    EXPECT_EQ(EstimateConjunctCost(cast), 103);   // CAST(3) + regexp(100)
    EXPECT_EQ(EstimateConjunctCost(like), 100);

    delete cast;
    delete like;
}
