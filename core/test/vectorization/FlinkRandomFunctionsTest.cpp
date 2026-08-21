/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for Flink-semantics random functions
 *              (rand_integer(bound), rand_integer(seed, bound), uuid()).
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cctype>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/FlinkRandomFunctions.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "codegen/func_registry.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::codegen;
using namespace omniruntime::type;

class RandomFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// ============================================================================
// JavaUtilRandom: validate the java.util.Random replication directly.
// ============================================================================

// Anchor test: new java.util.Random(0).nextDouble() == 0.730967787376657 (well-known
// Java constant). This proves the 48-bit LCG (next(26)/next(27)) is correct, which in
// turn guarantees nextInt() parity since it uses the same next().
TEST_F(RandomFunctionsTest, JavaUtilRandomNextDoubleAnchor) {
    JavaUtilRandom rng(0);
    EXPECT_NEAR(rng.NextDouble(), 0.730967787376657, 1e-15);
}

// Two generators with the same seed must produce identical sequences (Flink guarantee).
TEST_F(RandomFunctionsTest, JavaUtilRandomReproducible) {
    JavaUtilRandom a(12345);
    JavaUtilRandom b(12345);
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(a.NextInt(100), b.NextInt(100)) << "mismatch at draw " << i;
    }
    JavaUtilRandom da(12345);
    JavaUtilRandom db(67890);
    bool anyDiff = false;
    for (int i = 0; i < 32; ++i) {
        if (da.NextInt(1000) != db.NextInt(1000)) { anyDiff = true; break; }
    }
    EXPECT_TRUE(anyDiff) << "different seeds should (very likely) yield different sequences";
}

// NextInt range for both the rejection-sampling path and the power-of-two fast path.
TEST_F(RandomFunctionsTest, JavaUtilRandomNextIntRange) {
    JavaUtilRandom rng(7);
    for (int i = 0; i < 100; ++i) {
        int v = rng.NextInt(100);   // not a power of two -> rejection sampling
        EXPECT_GE(v, 0);
        EXPECT_LT(v, 100);
    }
    JavaUtilRandom rng2(7);
    for (int i = 0; i < 100; ++i) {
        int v = rng2.NextInt(16);   // power of two -> fast path
        EXPECT_GE(v, 0);
        EXPECT_LT(v, 16);
    }
}

// ============================================================================
// flink_rand() — 0-arg, no seed, DOUBLE in [0,1), non-deterministic.
// ============================================================================

TEST_F(RandomFunctionsTest, FlinkRandNoArgRange) {
    const int32_t rowSize = 16;
    std::vector<DataTypeId> argTypes = {};
    auto signature = std::make_shared<FunctionSignature>("flink_rand", argTypes, OMNI_DOUBLE);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_DOUBLE);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto *resultVector = static_cast<Vector<double> *>(rawResult);
    bool allSame = true;
    double first = resultVector->GetValue(0);
    for (int32_t i = 0; i < rowSize; ++i) {
        double v = resultVector->GetValue(i);
        EXPECT_GE(v, 0.0) << "row " << i;
        EXPECT_LT(v, 1.0) << "row " << i;
        if (i > 0 && v != first) {
            allSame = false;
        }
    }
    EXPECT_FALSE(allSame) << "flink_rand() should produce a varying sequence within a batch";
    delete rawResult;
}

// ============================================================================
// flink_rand(seed) — 1-arg seeded, deterministic and Flink-parity.
// ============================================================================

TEST_F(RandomFunctionsTest, FlinkRandSeedMatchesJavaRandom) {
    const int32_t rowSize = 8;
    const int32_t seed = 42;

    BaseVector *seedArg = new ConstVector<int32_t>(seed, OMNI_INT, rowSize);

    std::vector<DataTypeId> argTypes = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("flink_rand", argTypes, OMNI_DOUBLE);
    auto *seedConst = new ConstVector<int32_t>(seed, OMNI_INT);
    std::vector<BaseVector *> constantInputs = { seedConst };
    auto vectorFunction = VectorFunction::Find(signature, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(seedArg);

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_DOUBLE);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);
    delete seedConst;

    // Expected: exactly the sequence java.util.Random(42).nextDouble() would produce.
    JavaUtilRandom expected(seed);
    auto *resultVector = static_cast<Vector<double> *>(rawResult);
    for (int32_t i = 0; i < rowSize; ++i) {
        ASSERT_FALSE(resultVector->IsNull(i)) << "row " << i;
        EXPECT_NEAR(resultVector->GetValue(i), expected.NextDouble(), 1e-15) << "row " << i;
    }
    delete rawResult;
}

// Column seed: per row (new Random(rowSeed)).nextDouble() — same seed value => same output.
TEST_F(RandomFunctionsTest, FlinkRandSeedColumnUsesPerRowSeed) {
    const int32_t rowSize = 5;
    const int32_t seeds[rowSize] = {1, 42, 42, 100, 42};

    BaseVector *seedVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *seedVector = static_cast<Vector<int32_t> *>(seedVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        seedVector->SetValue(i, seeds[i]);
        seedVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("flink_rand", argTypes, OMNI_DOUBLE);
    std::vector<BaseVector *> constantInputs = { nullptr };  // column seed
    auto vectorFunction = VectorFunction::Find(signature, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(seedVec);

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_DOUBLE);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto *resultVector = static_cast<Vector<double> *>(rawResult);
    for (int32_t i = 0; i < rowSize; ++i) {
        JavaUtilRandom expected(seeds[i]);
        double exp = expected.NextDouble();
        EXPECT_NEAR(resultVector->GetValue(i), exp, 1e-15) << "row " << i;
    }
    // Rows 1, 2, 4 all have seed 42 -> identical first nextDouble().
    EXPECT_NEAR(resultVector->GetValue(1), resultVector->GetValue(2), 1e-15);
    EXPECT_NEAR(resultVector->GetValue(1), resultVector->GetValue(4), 1e-15);
    // Different seeds -> different first draws (1 vs 42 vs 100).
    EXPECT_NE(resultVector->GetValue(0), resultVector->GetValue(1));
    EXPECT_NE(resultVector->GetValue(1), resultVector->GetValue(3));
    delete rawResult;
}

// Literal vs column: mirrors FuncExpr — separate Find(constantInputs) per path.
// Column (verify_expr_rand.sql RAND(seed), same seed => same output):
//   Find(sig, {nullptr}) + FLAT seed column.
// Literal (verify_expr_rand2.sql RAND(42), sequence advances):
//   Find(sig, {ConstVector}) + runtime ConstVector seed arg.
TEST_F(RandomFunctionsTest, FlinkRandLiteralVsColumnSeedSemantics) {
    const int32_t rowSize = 3;
    const int32_t seed = 42;
    auto signature = std::make_shared<FunctionSignature>(
        "flink_rand", std::vector<DataTypeId>{OMNI_INT}, OMNI_DOUBLE);

    // Column path — same as verify_expr_rand.sql when multiple rows share seed=42.
    BaseVector *columnSeedVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *columnSeedVector = static_cast<Vector<int32_t> *>(columnSeedVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        columnSeedVector->SetValue(i, seed);
        columnSeedVector->SetNotNull(i);
    }
    auto columnFn = VectorFunction::Find(signature, { nullptr });
    ASSERT_NE(columnFn, nullptr);
    ExecutionContext ctxCol;
    ctxCol.SetResultRowSize(rowSize);
    std::stack<BaseVector *> argsCol;
    argsCol.push(columnSeedVec);
    BaseVector *columnResult = nullptr;
    columnFn->Apply(argsCol, std::make_shared<DataType>(OMNI_DOUBLE), columnResult, &ctxCol);

    // Literal path — same as verify_expr_rand2.sql RAND(42).
    auto *seedConst = new ConstVector<int32_t>(seed, OMNI_INT);
    auto literalFn = VectorFunction::Find(signature, { seedConst });
    ASSERT_NE(literalFn, nullptr);
    BaseVector *literalSeedArg = new ConstVector<int32_t>(seed, OMNI_INT, rowSize);
    ExecutionContext ctxLit;
    ctxLit.SetResultRowSize(rowSize);
    std::stack<BaseVector *> argsLit;
    argsLit.push(literalSeedArg);
    BaseVector *literalResult = nullptr;
    literalFn->Apply(argsLit, std::make_shared<DataType>(OMNI_DOUBLE), literalResult, &ctxLit);
    delete seedConst;

    auto *litVec = static_cast<Vector<double> *>(literalResult);
    auto *colVec = static_cast<Vector<double> *>(columnResult);
    JavaUtilRandom firstDraw(seed);
    double colExpected = firstDraw.NextDouble();
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_NEAR(colVec->GetValue(i), colExpected, 1e-15) << "column row " << i;
    }
    EXPECT_NEAR(litVec->GetValue(0), colExpected, 1e-15);
    EXPECT_NE(litVec->GetValue(1), litVec->GetValue(0));
    EXPECT_NE(litVec->GetValue(2), litVec->GetValue(1));
    delete literalResult;
    delete columnResult;
}

// ============================================================================
// rand_integer(bound) — 1-arg, no seed, non-deterministic.
// ============================================================================

TEST_F(RandomFunctionsTest, RandIntegerBoundRange) {
    const int32_t rowSize = 32;
    const int32_t bound = 100;

    BaseVector *boundVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *boundVector = static_cast<Vector<int32_t> *>(boundVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        boundVector->SetValue(i, bound);
        boundVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("rand_integer", argTypes, OMNI_INT);
    // bound is a column (non-constant): pass { nullptr } so UnpackInitialize matches num_args == 1.
    std::vector<BaseVector *> constantInputs = { nullptr };
    auto vectorFunction = VectorFunction::Find(signature, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(boundVec);

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto *resultVector = static_cast<Vector<int32_t> *>(rawResult);
    for (int32_t i = 0; i < rowSize; ++i) {
        ASSERT_FALSE(resultVector->IsNull(i)) << "row " << i;
        int32_t v = resultVector->GetValue(i);
        EXPECT_GE(v, 0) << "row " << i;
        EXPECT_LT(v, bound) << "row " << i;
    }
    delete rawResult;
}

// bound == NULL and bound <= 0 both produce a NULL result.
TEST_F(RandomFunctionsTest, RandIntegerBoundNullAndNonPositive) {
    const int32_t rowSize = 4;
    BaseVector *boundVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *boundVector = static_cast<Vector<int32_t> *>(boundVec);
    int32_t bounds[rowSize] = {100, 0, -5, 50};
    for (int32_t i = 0; i < rowSize; ++i) {
        boundVector->SetValue(i, bounds[i]);
        boundVector->SetNotNull(i);
    }
    boundVector->SetNull(0);  // row 0: NULL bound -> NULL result

    std::vector<DataTypeId> argTypes = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("rand_integer", argTypes, OMNI_INT);
    std::vector<BaseVector *> constantInputs = { nullptr };
    auto vectorFunction = VectorFunction::Find(signature, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(boundVec);

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto *resultVector = static_cast<Vector<int32_t> *>(rawResult);
    EXPECT_TRUE(resultVector->IsNull(0)) << "NULL bound -> NULL";
    EXPECT_TRUE(resultVector->IsNull(1)) << "bound == 0 -> NULL";
    EXPECT_TRUE(resultVector->IsNull(2)) << "bound < 0 -> NULL";
    EXPECT_FALSE(resultVector->IsNull(3)) << "valid bound -> not NULL";
    EXPECT_GE(resultVector->GetValue(3), 0);
    EXPECT_LT(resultVector->GetValue(3), 50);
    delete rawResult;
}

// ============================================================================
// rand_integer(seed, bound) — 2-arg, seeded, deterministic and Flink-parity.
// ============================================================================

TEST_F(RandomFunctionsTest, RandIntegerSeedBoundMatchesJavaRandom) {
    const int32_t rowSize = 8;
    const int32_t seed = 42;
    const int32_t bound = 100;

    BaseVector *seedArg = new ConstVector<int32_t>(seed, OMNI_INT, rowSize);
    BaseVector *boundVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *boundVector = static_cast<Vector<int32_t> *>(boundVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        boundVector->SetValue(i, bound);
        boundVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("rand_integer", argTypes, OMNI_INT);
    auto *seedConst = new ConstVector<int32_t>(seed, OMNI_INT);
    std::vector<BaseVector *> constantInputs = { seedConst, nullptr };
    auto vectorFunction = VectorFunction::Find(signature, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(seedArg);
    args.push(boundVec);

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);
    delete seedConst;

    // Expected: exactly the sequence java.util.Random(42).nextInt(100) would produce.
    JavaUtilRandom expected(seed);
    auto *resultVector = static_cast<Vector<int32_t> *>(rawResult);
    for (int32_t i = 0; i < rowSize; ++i) {
        ASSERT_FALSE(resultVector->IsNull(i)) << "row " << i;
        int32_t exp = expected.NextInt(bound);
        EXPECT_EQ(resultVector->GetValue(i), exp) << "row " << i;
    }
    delete rawResult;
}

// Column seed: per row (new Random(rowSeed)).nextInt(bound).
TEST_F(RandomFunctionsTest, RandIntegerSeedBoundColumnUsesPerRowSeed) {
    const int32_t rowSize = 4;
    const int32_t seeds[rowSize] = {1, 42, 42, 100};
    const int32_t bound = 100;

    BaseVector *seedVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *seedVector = static_cast<Vector<int32_t> *>(seedVec);
    BaseVector *boundVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *boundVector = static_cast<Vector<int32_t> *>(boundVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        seedVector->SetValue(i, seeds[i]);
        seedVector->SetNotNull(i);
        boundVector->SetValue(i, bound);
        boundVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("rand_integer", argTypes, OMNI_INT);
    std::vector<BaseVector *> constantInputs = { nullptr, nullptr };
    auto vectorFunction = VectorFunction::Find(signature, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(seedVec);
    args.push(boundVec);

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto *resultVector = static_cast<Vector<int32_t> *>(rawResult);
    for (int32_t i = 0; i < rowSize; ++i) {
        JavaUtilRandom expected(seeds[i]);
        EXPECT_EQ(resultVector->GetValue(i), expected.NextInt(bound)) << "row " << i;
    }
    EXPECT_EQ(resultVector->GetValue(1), resultVector->GetValue(2));
    delete rawResult;
}

// ============================================================================
// uuid() — 0-arg, RFC 4122 v4 string, non-deterministic.
// ============================================================================

static void ExpectValidUuidV4(const std::string &s) {
    ASSERT_EQ(s.size(), 36u);
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            EXPECT_EQ(s[i], '-') << "expected hyphen at " << i;
        } else if (i == 14) {
            EXPECT_EQ(s[i], '4') << "version nibble must be 4";
        } else if (i == 19) {
            char c = s[i];
            EXPECT_TRUE(c == '8' || c == '9' || c == 'a' || c == 'b')
                << "variant nibble must be one of 8/9/a/b, got " << c;
        } else {
            char c = s[i];
            EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(c))) << "hex digit expected at " << i;
        }
    }
}

TEST_F(RandomFunctionsTest, UuidNoArgFormatAndUniqueness) {
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    const int32_t rowSize = 16;

    std::vector<DataTypeId> argTypes = {};
    auto signature = std::make_shared<FunctionSignature>("uuid", argTypes, OMNI_VARCHAR);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;

    BaseVector *rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_VARCHAR);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto *resultVector = static_cast<VarcharVector *>(rawResult);
    std::set<std::string> seen;
    for (int32_t i = 0; i < rowSize; ++i) {
        ASSERT_FALSE(resultVector->IsNull(i)) << "row " << i;
        std::string s(resultVector->GetValue(i));
        ExpectValidUuidV4(s);
        seen.insert(s);
    }
    EXPECT_EQ(seen.size(), static_cast<size_t>(rowSize)) << "UUIDs should be unique across rows";
    delete rawResult;
}
