/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* Description: Unit tests for DECIMAL-input unary math functions.
*
* Covers the unary functions in MathDecimalFunctions.h#DecimalMathFunctionTable:
*   asin, acos, atan, sin, sinh, cos, cosh, tan, tanh, cot, degrees, radians
* and the binary function in DecimalBinaryMathFunctionTable: atan2(y, x).
*
* These functions are stateful VectorFunctions (DecimalToDoubleMathFunction) that descale
* the DECIMAL unscaled integer to double (unscaled / 10^scale) then apply the op, mirroring
* Flink's per-function doubleValue overloads. The scale is carried by the operand DataType
* (Decimal64DataType/Decimal128DataType::GetScale), NOT by the per-row value, so the tests
* MUST go through the FuncExpr/ExprEval path (which builds the scale-aware function from
* arguments[0]->dataType). A direct VectorFunction::Find would only resolve the gate
* placeholder registered with DataTypeId-only signature (nullptr DataType), whose GetScale
* would be invalid.
*
* Test structure mirrors MathFunctionTest.cpp (EXPECT_NEAR for finite, NaN/Inf checks for
* out-of-domain), but input vectors are DECIMAL64 (int64_t unscaled) / DECIMAL128 (Decimal128
* unscaled) and the expected value is op(unscaled / 10^scale).
*/

#include <gtest/gtest.h>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "type/decimal128.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime::TestUtil;

// Register all functions once for the whole test binary.
class MathDecimalFunctionTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};
::testing::Environment *const math_decimal_test_env =
    ::testing::AddGlobalTestEnvironment(new MathDecimalFunctionTestEnvironment);

namespace {
// Tolerance for comparing doubles produced from descaled decimals. The descale itself
// (unscaled / 10^scale) introduces rounding for non-terminating decimals, so use a
// tolerance proportional to the scale rather than an over-tight 1e-6.
constexpr double kTolerance = 1e-9;

/// Compare a double result against an expected double, handling NaN and ±Inf.
void ExpectDoubleEq(const std::string &funcName, int32_t row, double actual, double expected) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual))
            << "NaN mismatch at row " << row << " for " << funcName;
    } else if (std::isinf(expected)) {
        EXPECT_TRUE(std::isinf(actual) && std::signbit(actual) == std::signbit(expected))
            << "Infinity mismatch at row " << row << " for " << funcName
            << " expected=" << expected << " actual=" << actual;
    } else {
        EXPECT_NEAR(actual, expected, kTolerance)
            << "Value mismatch at row " << row << " for " << funcName
            << " expected=" << expected << " actual=" << actual;
    }
}

/// Run a unary math function on a DECIMAL64 column with the given scale.
/// `unscaled` holds the raw int64_t values; the descaled input is unscaled[i] / 10^scale.
/// `expectedFn` computes the expected double from the descaled double input.
/// Rows where unscaled[i] == kNullSentinel are set NULL on the input vector.
void TestDecimal64MathOp(const std::string &funcName, int32_t scale,
    const std::vector<int64_t> &unscaled,
    const std::function<double(double)> &expectedFn,
    const std::vector<bool> &nullRows = {}) {
    int32_t rowSize = static_cast<int32_t>(unscaled.size());
    // Operand DataType carries precision+scale; DecimalToDoubleMathFunction reads scale from it.
    auto decType = std::make_shared<Decimal64DataType>(18, scale);
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);

    BaseVector *col = VectorHelper::CreateFlatVector(OMNI_DECIMAL64, rowSize);
    auto *vec = static_cast<Vector<int64_t> *>(col);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!nullRows.empty() && nullRows[i]) {
            col->SetNull(i);
        } else {
            vec->SetValue(i, unscaled[i]);
            col->SetNotNull(i);
        }
    }
    auto *batch = new VectorBatch(rowSize);
    batch->Append(col);

    std::vector<Expr *> args = {new FieldExpr(0, decType)};
    auto *funcExpr = new FuncExpr(funcName, args, returnType);

    auto *context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.Visit(*funcExpr);
    auto *result = e.GetResult();
    ASSERT_NE(result, nullptr);
    auto *resultVec = dynamic_cast<Vector<double> *>(result);
    ASSERT_NE(resultVec, nullptr);

    double factor = std::pow(10.0, static_cast<double>(scale));
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!nullRows.empty() && nullRows[i]) {
            EXPECT_TRUE(result->IsNull(i))
                << "Expected NULL at row " << i << " for " << funcName;
            continue;
        }
        ASSERT_FALSE(result->IsNull(i))
            << "Unexpected NULL at row " << i << " for " << funcName;
        double descaled = static_cast<double>(unscaled[i]) / factor;
        ExpectDoubleEq(funcName, i, resultVec->GetValue(i), expectedFn(descaled));
    }

    delete result;
    delete batch;
    delete funcExpr;
    delete context;
}

/// Run a unary math function on a DECIMAL128 column with the given scale.
/// `unscaled` holds Decimal128 values; the descaled input is unscaled[i].ToInt128() / 10^scale.
void TestDecimal128MathOp(const std::string &funcName, int32_t scale,
    const std::vector<Decimal128> &unscaled,
    const std::function<double(double)> &expectedFn,
    const std::vector<bool> &nullRows = {}) {
    int32_t rowSize = static_cast<int32_t>(unscaled.size());
    auto decType = std::make_shared<Decimal128DataType>(38, scale);
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);

    BaseVector *col = VectorHelper::CreateFlatVector(OMNI_DECIMAL128, rowSize);
    auto *vec = static_cast<Vector<Decimal128> *>(col);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!nullRows.empty() && nullRows[i]) {
            col->SetNull(i);
        } else {
            vec->SetValue(i, unscaled[i]);
            col->SetNotNull(i);
        }
    }
    auto *batch = new VectorBatch(rowSize);
    batch->Append(col);

    std::vector<Expr *> args = {new FieldExpr(0, decType)};
    auto *funcExpr = new FuncExpr(funcName, args, returnType);

    auto *context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.Visit(*funcExpr);
    auto *result = e.GetResult();
    ASSERT_NE(result, nullptr);
    auto *resultVec = dynamic_cast<Vector<double> *>(result);
    ASSERT_NE(resultVec, nullptr);

    double factor = std::pow(10.0, static_cast<double>(scale));
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!nullRows.empty() && nullRows[i]) {
            EXPECT_TRUE(result->IsNull(i))
                << "Expected NULL at row " << i << " for " << funcName;
            continue;
        }
        ASSERT_FALSE(result->IsNull(i))
            << "Unexpected NULL at row " << i << " for " << funcName;
        double descaled = static_cast<double>(unscaled[i].ToInt128()) / factor;
        ExpectDoubleEq(funcName, i, resultVec->GetValue(i), expectedFn(descaled));
    }

    delete result;
    delete batch;
    delete funcExpr;
    delete context;
}

// Shorthand lambdas for the 10 ops. cot mirrors the C++ impl (1/tan, not cos/sin).
const auto kAsin = [](double x) { return std::asin(x); };
const auto kAcos = [](double x) { return std::acos(x); };
const auto kAtan = [](double x) { return std::atan(x); };
const auto kSin  = [](double x) { return std::sin(x); };
const auto kSinh = [](double x) { return std::sinh(x); };
const auto kCos  = [](double x) { return std::cos(x); };
const auto kCosh = [](double x) { return std::cosh(x); };
const auto kTan  = [](double x) { return std::tan(x); };
const auto kTanh = [](double x) { return std::tanh(x); };
const auto kCot  = [](double x) { return 1.0 / std::tan(x); };
// degrees/radians mirror Degrees/RadiansFunction (x*(180/π) / x*(π/180)).
const auto kDegrees = [](double x) { return x * (180.0 / M_PI); };
const auto kRadians = [](double x) { return x * (M_PI / 180.0); };

/// Run the binary atan2(y, x) on two DECIMAL64 columns with (possibly different) scales.
/// yUnscaled/xUnscaled hold raw int64_t values; descaled operands are unscaled[i] / 10^scale.
/// A row is NULL (and expected NULL) if yNull[i] or xNull[i] is true.
void TestAtan2Decimal64(int32_t scaleY, int32_t scaleX,
    const std::vector<int64_t> &yUnscaled, const std::vector<int64_t> &xUnscaled,
    const std::vector<bool> &yNull = {}, const std::vector<bool> &xNull = {}) {
    int32_t rowSize = static_cast<int32_t>(yUnscaled.size());
    auto yType = std::make_shared<Decimal64DataType>(18, scaleY);
    auto xType = std::make_shared<Decimal64DataType>(18, scaleX);
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);

    BaseVector *yCol = VectorHelper::CreateFlatVector(OMNI_DECIMAL64, rowSize);
    BaseVector *xCol = VectorHelper::CreateFlatVector(OMNI_DECIMAL64, rowSize);
    auto *yVec = static_cast<Vector<int64_t> *>(yCol);
    auto *xVec = static_cast<Vector<int64_t> *>(xCol);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!yNull.empty() && yNull[i]) { yCol->SetNull(i); } else { yVec->SetValue(i, yUnscaled[i]); yCol->SetNotNull(i); }
        if (!xNull.empty() && xNull[i]) { xCol->SetNull(i); } else { xVec->SetValue(i, xUnscaled[i]); xCol->SetNotNull(i); }
    }
    auto *batch = new VectorBatch(rowSize);
    batch->Append(yCol);
    batch->Append(xCol);

    std::vector<Expr *> args = {new FieldExpr(0, yType), new FieldExpr(1, xType)};
    auto *funcExpr = new FuncExpr("atan2", args, returnType);

    auto *context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.Visit(*funcExpr);
    auto *result = e.GetResult();
    ASSERT_NE(result, nullptr);
    auto *resultVec = dynamic_cast<Vector<double> *>(result);
    ASSERT_NE(resultVec, nullptr);

    double fy = std::pow(10.0, static_cast<double>(scaleY));
    double fx = std::pow(10.0, static_cast<double>(scaleX));
    for (int32_t i = 0; i < rowSize; ++i) {
        bool isNull = (!yNull.empty() && yNull[i]) || (!xNull.empty() && xNull[i]);
        if (isNull) {
            EXPECT_TRUE(result->IsNull(i)) << "Expected NULL at row " << i << " for atan2";
            continue;
        }
        ASSERT_FALSE(result->IsNull(i)) << "Unexpected NULL at row " << i << " for atan2";
        double expected = std::atan2(yUnscaled[i] / fy, xUnscaled[i] / fx);
        ExpectDoubleEq("atan2", i, resultVec->GetValue(i), expected);
    }

    delete result;
    delete batch;
    delete funcExpr;
    delete context;
}

/// atan2(y, x) on two DECIMAL128 columns (same scale for simplicity).
void TestAtan2Decimal128(int32_t scale,
    const std::vector<Decimal128> &yUnscaled, const std::vector<Decimal128> &xUnscaled) {
    int32_t rowSize = static_cast<int32_t>(yUnscaled.size());
    auto decType = std::make_shared<Decimal128DataType>(38, scale);
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);

    BaseVector *yCol = VectorHelper::CreateFlatVector(OMNI_DECIMAL128, rowSize);
    BaseVector *xCol = VectorHelper::CreateFlatVector(OMNI_DECIMAL128, rowSize);
    auto *yVec = static_cast<Vector<Decimal128> *>(yCol);
    auto *xVec = static_cast<Vector<Decimal128> *>(xCol);
    for (int32_t i = 0; i < rowSize; ++i) {
        yVec->SetValue(i, yUnscaled[i]); yCol->SetNotNull(i);
        xVec->SetValue(i, xUnscaled[i]); xCol->SetNotNull(i);
    }
    auto *batch = new VectorBatch(rowSize);
    batch->Append(yCol);
    batch->Append(xCol);

    std::vector<Expr *> args = {new FieldExpr(0, decType), new FieldExpr(1, decType)};
    auto *funcExpr = new FuncExpr("atan2", args, returnType);

    auto *context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.Visit(*funcExpr);
    auto *result = e.GetResult();
    ASSERT_NE(result, nullptr);
    auto *resultVec = dynamic_cast<Vector<double> *>(result);
    ASSERT_NE(resultVec, nullptr);

    double f = std::pow(10.0, static_cast<double>(scale));
    for (int32_t i = 0; i < rowSize; ++i) {
        ASSERT_FALSE(result->IsNull(i)) << "Unexpected NULL at row " << i << " for atan2";
        double expected = std::atan2(
            static_cast<double>(yUnscaled[i].ToInt128()) / f,
            static_cast<double>(xUnscaled[i].ToInt128()) / f);
        ExpectDoubleEq("atan2", i, resultVec->GetValue(i), expected);
    }

    delete result;
    delete batch;
    delete funcExpr;
    delete context;
}

/// sign(DECIMAL64) -> DECIMAL64. Verifies the output is scale-preserving: unscaled result equals
/// signum(input) * 10^scale (i.e. ±1.0..0 / 0 at the same scale), and NULL propagates.
void TestSignDecimal64(int32_t scale, const std::vector<int64_t> &unscaled,
    const std::vector<bool> &nullRows = {}) {
    int32_t rowSize = static_cast<int32_t>(unscaled.size());
    auto decType = std::make_shared<Decimal64DataType>(18, scale);
    auto returnType = std::make_shared<Decimal64DataType>(18, scale);  // sign preserves p,s

    BaseVector *col = VectorHelper::CreateFlatVector(OMNI_DECIMAL64, rowSize);
    auto *vec = static_cast<Vector<int64_t> *>(col);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!nullRows.empty() && nullRows[i]) { col->SetNull(i); } else { vec->SetValue(i, unscaled[i]); col->SetNotNull(i); }
    }
    auto *batch = new VectorBatch(rowSize);
    batch->Append(col);

    std::vector<Expr *> args = {new FieldExpr(0, decType)};
    auto *funcExpr = new FuncExpr("sign", args, returnType);

    auto *context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.Visit(*funcExpr);
    auto *result = e.GetResult();
    ASSERT_NE(result, nullptr);
    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr) << "sign(DECIMAL64) must return a DECIMAL64 (int64 unscaled) vector";

    int64_t one = static_cast<int64_t>(std::llround(std::pow(10.0, static_cast<double>(scale))));
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!nullRows.empty() && nullRows[i]) {
            EXPECT_TRUE(result->IsNull(i)) << "Expected NULL at row " << i << " for sign";
            continue;
        }
        ASSERT_FALSE(result->IsNull(i)) << "Unexpected NULL at row " << i << " for sign";
        int64_t signum = (unscaled[i] > 0) - (unscaled[i] < 0);
        EXPECT_EQ(resultVec->GetValue(i), signum * one)
            << "sign mismatch at row " << i << " (scale=" << scale << ")";
    }

    delete result;
    delete batch;
    delete funcExpr;
    delete context;
}

/// sign(DECIMAL128) -> DECIMAL128. Same invariant as DECIMAL64 but on 128-bit unscaled values.
void TestSignDecimal128(int32_t scale, const std::vector<Decimal128> &unscaled) {
    int32_t rowSize = static_cast<int32_t>(unscaled.size());
    auto decType = std::make_shared<Decimal128DataType>(38, scale);
    auto returnType = std::make_shared<Decimal128DataType>(38, scale);

    BaseVector *col = VectorHelper::CreateFlatVector(OMNI_DECIMAL128, rowSize);
    auto *vec = static_cast<Vector<Decimal128> *>(col);
    for (int32_t i = 0; i < rowSize; ++i) {
        vec->SetValue(i, unscaled[i]);
        col->SetNotNull(i);
    }
    auto *batch = new VectorBatch(rowSize);
    batch->Append(col);

    std::vector<Expr *> args = {new FieldExpr(0, decType)};
    auto *funcExpr = new FuncExpr("sign", args, returnType);

    auto *context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.Visit(*funcExpr);
    auto *result = e.GetResult();
    ASSERT_NE(result, nullptr);
    auto *resultVec = dynamic_cast<Vector<Decimal128> *>(result);
    ASSERT_NE(resultVec, nullptr) << "sign(DECIMAL128) must return a DECIMAL128 vector";

    __int128 one = 1;
    for (int32_t s = 0; s < scale; ++s) { one *= 10; }
    for (int32_t i = 0; i < rowSize; ++i) {
        ASSERT_FALSE(result->IsNull(i)) << "Unexpected NULL at row " << i << " for sign";
        __int128 v = unscaled[i].ToInt128();
        __int128 signum = (v > 0) - (v < 0);
        EXPECT_TRUE(resultVec->GetValue(i).ToInt128() == signum * one)
            << "sign mismatch at row " << i << " (scale=" << scale << ")";
    }

    delete result;
    delete batch;
    delete funcExpr;
    delete context;
}
} // namespace

// ============================================================================
// ASIN (domain [-1, 1], out-of-domain -> NaN)
// ============================================================================

TEST(MathDecimalFunctionTest, AsinDecimal64) {
    // scale=4: 0.0, 0.5, 1.0, -0.5, -1.0, 0.8660(√3/2), 2.0(越界)
    TestDecimal64MathOp("asin", 4,
        {0LL, 5000LL, 10000LL, -5000LL, -10000LL, 8660LL, 20000LL},
        kAsin);
}

TEST(MathDecimalFunctionTest, AsinDecimal128) {
    // scale=10: 0.0, 0.5, 1.0, -0.5, -1.0, 0.8660254038(√3/2), 2.0(越界)
    TestDecimal128MathOp("asin", 10,
        {Decimal128("0"), Decimal128("5000000000"), Decimal128("10000000000"),
         Decimal128("-5000000000"), Decimal128("-10000000000"),
         Decimal128("8660254038"), Decimal128("20000000000")},
        kAsin);
}

TEST(MathDecimalFunctionTest, AsinDecimal64Null) {
    TestDecimal64MathOp("asin", 4,
        {5000LL, 0LL, 10000LL},
        kAsin,
        {false, true, false});  // row 1 NULL -> NULL output
}

// ============================================================================
// ACOS (domain [-1, 1], out-of-domain -> NaN)
// ============================================================================

TEST(MathDecimalFunctionTest, AcosDecimal64) {
    TestDecimal64MathOp("acos", 4,
        {0LL, 5000LL, 10000LL, -5000LL, -10000LL, 8660LL, 20000LL, -20000LL},
        kAcos);
}

TEST(MathDecimalFunctionTest, AcosDecimal128) {
    TestDecimal128MathOp("acos", 10,
        {Decimal128("0"), Decimal128("5000000000"), Decimal128("10000000000"),
         Decimal128("-5000000000"), Decimal128("-10000000000"),
         Decimal128("8660254038"), Decimal128("20000000000"), Decimal128("-20000000000")},
        kAcos);
}

// ============================================================================
// ATAN (full domain, odd function)
// ============================================================================

TEST(MathDecimalFunctionTest, AtanDecimal64) {
    TestDecimal64MathOp("atan", 4,
        {0LL, 10000LL, -10000LL, 5000LL, -5000LL, 20000LL, -20000LL},
        kAtan);
}

TEST(MathDecimalFunctionTest, AtanDecimal128) {
    TestDecimal128MathOp("atan", 10,
        {Decimal128("0"), Decimal128("10000000000"), Decimal128("-10000000000"),
         Decimal128("5000000000"), Decimal128("-5000000000"),
         Decimal128("20000000000"), Decimal128("-20000000000")},
        kAtan);
}

// ============================================================================
// SIN (full domain)
// ============================================================================

TEST(MathDecimalFunctionTest, SinDecimal64) {
    // scale=4: 0, π/6≈0.5236, π/2≈1.5708, π≈3.1416, -π/6, 2.0
    TestDecimal64MathOp("sin", 4,
        {0LL, 5236LL, 15708LL, 31416LL, -5236LL, 20000LL, -20000LL},
        kSin);
}

TEST(MathDecimalFunctionTest, SinDecimal128) {
    TestDecimal128MathOp("sin", 10,
        {Decimal128("0"), Decimal128("5235987756"), Decimal128("15707963268"),
         Decimal128("31415926536"), Decimal128("-5235987756"),
         Decimal128("20000000000"), Decimal128("-20000000000")},
        kSin);
}

// ============================================================================
// SINH (full domain, odd function)
// ============================================================================

TEST(MathDecimalFunctionTest, SinhDecimal64) {
    TestDecimal64MathOp("sinh", 4,
        {0LL, 10000LL, -10000LL, 20000LL, -20000LL, 5000LL},
        kSinh);
}

TEST(MathDecimalFunctionTest, SinhDecimal128) {
    TestDecimal128MathOp("sinh", 10,
        {Decimal128("0"), Decimal128("10000000000"), Decimal128("-10000000000"),
         Decimal128("20000000000"), Decimal128("-20000000000"), Decimal128("5000000000")},
        kSinh);
}

// ============================================================================
// COS (full domain, even function)
// ============================================================================

TEST(MathDecimalFunctionTest, CosDecimal64) {
    // 0, π/3≈1.0472, π/2≈1.5708, π≈3.1416, -1.0, 2.0
    TestDecimal64MathOp("cos", 4,
        {0LL, 10472LL, 15708LL, 31416LL, -10000LL, 20000LL},
        kCos);
}

TEST(MathDecimalFunctionTest, CosDecimal128) {
    TestDecimal128MathOp("cos", 10,
        {Decimal128("0"), Decimal128("10471975512"), Decimal128("15707963268"),
         Decimal128("31415926536"), Decimal128("-10000000000"), Decimal128("20000000000")},
        kCos);
}

// ============================================================================
// COSH (full domain, even function, cosh(0)=1)
// ============================================================================

TEST(MathDecimalFunctionTest, CoshDecimal64) {
    TestDecimal64MathOp("cosh", 4,
        {0LL, 10000LL, -10000LL, 20000LL, -20000LL, 5000LL},
        kCosh);
}

TEST(MathDecimalFunctionTest, CoshDecimal128) {
    TestDecimal128MathOp("cosh", 10,
        {Decimal128("0"), Decimal128("10000000000"), Decimal128("-10000000000"),
         Decimal128("20000000000"), Decimal128("-20000000000"), Decimal128("5000000000")},
        kCosh);
}

// ============================================================================
// TAN (full domain; tan(π/2) is a large finite number under fp rounding)
// ============================================================================

TEST(MathDecimalFunctionTest, TanDecimal64) {
    // 0, π/4≈0.7854, π/6≈0.5236, 1.0, -1.0, 2.0
    TestDecimal64MathOp("tan", 4,
        {0LL, 7854LL, 5236LL, 10000LL, -10000LL, 20000LL},
        kTan);
}

TEST(MathDecimalFunctionTest, TanDecimal128) {
    TestDecimal128MathOp("tan", 10,
        {Decimal128("0"), Decimal128("7853981634"), Decimal128("5235987756"),
         Decimal128("10000000000"), Decimal128("-10000000000"), Decimal128("20000000000")},
        kTan);
}

// ============================================================================
// TANH (full domain; large |x| -> ±1)
// ============================================================================

TEST(MathDecimalFunctionTest, TanhDecimal64) {
    TestDecimal64MathOp("tanh", 4,
        {0LL, 10000LL, -10000LL, 20000LL, -20000LL, 100000LL, -100000LL},
        kTanh);
}

TEST(MathDecimalFunctionTest, TanhDecimal128) {
    TestDecimal128MathOp("tanh", 10,
        {Decimal128("0"), Decimal128("10000000000"), Decimal128("-10000000000"),
         Decimal128("20000000000"), Decimal128("-20000000000"),
         Decimal128("100000000000"), Decimal128("-100000000000")},
        kTanh);
}

// ============================================================================
// COT (1/tan; near kπ -> large finite; cot(π/4)=1, cot(π/6)=√3)
// ============================================================================

TEST(MathDecimalFunctionTest, CotDecimal64) {
    // π/6≈0.5236(->√3), π/4≈0.7854(->1), 1.0, 0.01(->~100), 2.0, -π/4(->-1)
    TestDecimal64MathOp("cot", 4,
        {5236LL, 7854LL, 10000LL, 100LL, 20000LL, -7854LL},
        kCot);
}

TEST(MathDecimalFunctionTest, CotDecimal128) {
    TestDecimal128MathOp("cot", 10,
        {Decimal128("5235987756"), Decimal128("7853981634"), Decimal128("10000000000"),
         Decimal128("100000000"), Decimal128("20000000000"), Decimal128("-7853981634")},
        kCot);
}

// ============================================================================
// Cross-check: DECIMAL result equals DOUBLE result for the same logical input.
// Verifies the descale path produces byte-identical results to the DOUBLE overload
// (the core invariant of DecimalToDoubleMathFunction).
// ============================================================================

TEST(MathDecimalFunctionTest, DecimalMatchesDoubleSin) {
    // sin(0.5): DOUBLE path vs DECIMAL64(scale=4) descale path
    double doubleResult = std::sin(0.5);
    // DECIMAL64 scale=4: 0.5 -> unscaled 5000, descale 5000/1e4 = 0.5 exactly
    TestDecimal64MathOp("sin", 4, {5000LL}, [&](double) { return doubleResult; });
}

TEST(MathDecimalFunctionTest, DecimalMatchesDoubleCot) {
    // cot(π/4≈0.7854): descale 7854/1e4 = 0.7854, expected = 1/tan(0.7854)
    double descaled = 7854 / 10000.0;
    TestDecimal64MathOp("cot", 4, {7854LL}, [&](double) { return 1.0 / std::tan(descaled); });
}

// ============================================================================
// DEGREES (radians -> degrees, x*(180/π); odd function)
// ============================================================================

TEST(MathDecimalFunctionTest, DegreesDecimal64) {
    // scale=4: 0, π, π/2, 2π, -π, π/4, 1 rad
    TestDecimal64MathOp("degrees", 4,
        {0LL, 31416LL, 15708LL, 62832LL, -31416LL, 7854LL, 10000LL},
        kDegrees);
}

TEST(MathDecimalFunctionTest, DegreesDecimal128) {
    TestDecimal128MathOp("degrees", 10,
        {Decimal128("0"), Decimal128("31415926536"), Decimal128("15707963268"),
         Decimal128("62831853072"), Decimal128("-31415926536"),
         Decimal128("7853981634"), Decimal128("10000000000")},
        kDegrees);
}

TEST(MathDecimalFunctionTest, DegreesDecimal64Null) {
    TestDecimal64MathOp("degrees", 4, {31416LL, 0LL, 15708LL}, kDegrees, {false, true, false});
}

// ============================================================================
// RADIANS (degrees -> radians, x*(π/180); inverse of DEGREES; odd function)
// ============================================================================

TEST(MathDecimalFunctionTest, RadiansDecimal64) {
    // scale=2: 0, 180, 90, 360, -180, 45, 1 deg
    TestDecimal64MathOp("radians", 2,
        {0LL, 18000LL, 9000LL, 36000LL, -18000LL, 4500LL, 100LL},
        kRadians);
}

TEST(MathDecimalFunctionTest, RadiansDecimal128) {
    TestDecimal128MathOp("radians", 6,
        {Decimal128("0"), Decimal128("180000000"), Decimal128("90000000"),
         Decimal128("360000000"), Decimal128("-180000000"),
         Decimal128("45000000"), Decimal128("1000000")},
        kRadians);
}

// ============================================================================
// ATAN2(y, x) (binary; value range (-π, π]; quadrant from both signs)
// Verifies argument order op(descale(arg0=y), descale(arg1=x)) and null propagation.
// ============================================================================

TEST(MathDecimalFunctionTest, Atan2Decimal64) {
    // Four quadrants + axes + origin. scale=4 for both operands.
    //        y:  1     1    -1    -1     0     1     0    -1     0
    //        x:  1    -1    -1     1     1     0    -1     0     0
    TestAtan2Decimal64(4, 4,
        {10000LL, 10000LL, -10000LL, -10000LL, 0LL, 10000LL, 0LL, -10000LL, 0LL},
        {10000LL, -10000LL, -10000LL, 10000LL, 10000LL, 0LL, -10000LL, 0LL, 0LL});
}

TEST(MathDecimalFunctionTest, Atan2Decimal64DifferentScales) {
    // y scale=4 (1.0 -> 10000), x scale=2 (1.0 -> 100): exercises per-operand descale.
    TestAtan2Decimal64(4, 2,
        {10000LL, 10000LL, -10000LL},
        {100LL, -100LL, 100LL});
}

TEST(MathDecimalFunctionTest, Atan2Decimal64Null) {
    // row0 normal; row1 y NULL; row2 x NULL -> both NULL outputs.
    TestAtan2Decimal64(4, 4,
        {10000LL, 0LL, 10000LL},
        {10000LL, 10000LL, 0LL},
        {false, true, false},
        {false, false, true});
}

TEST(MathDecimalFunctionTest, Atan2Decimal128) {
    // scale=10; same four quadrants.
    TestAtan2Decimal128(10,
        {Decimal128("10000000000"), Decimal128("10000000000"),
         Decimal128("-10000000000"), Decimal128("-10000000000")},
        {Decimal128("10000000000"), Decimal128("-10000000000"),
         Decimal128("-10000000000"), Decimal128("10000000000")});
}

// ============================================================================
// SIGN (DECIMAL -> DECIMAL, scale-preserving: result = signum * 10^scale)
// e.g. scale=4: 12.3400 -> 1.0000 (10000), -0.5000 -> -1.0000 (-10000), 0 -> 0.0000 (0)
// ============================================================================

TEST(MathDecimalFunctionTest, SignDecimal64) {
    // scale=4: positive, negative, zero, small positive, large negative
    TestSignDecimal64(4, {123400LL, -5000LL, 0LL, 1LL, -9999999LL});
}

TEST(MathDecimalFunctionTest, SignDecimal64Scale0) {
    // scale=0: 10^0 = 1, so result is exactly signum (1 / -1 / 0)
    TestSignDecimal64(0, {7LL, -3LL, 0LL});
}

TEST(MathDecimalFunctionTest, SignDecimal64Null) {
    TestSignDecimal64(2, {250LL, 0LL, -250LL}, {false, true, false});
}

TEST(MathDecimalFunctionTest, SignDecimal128) {
    // scale=10: positive, negative, zero, large magnitude
    TestSignDecimal128(10,
        {Decimal128("12340000000"), Decimal128("-5000000000"), Decimal128("0"),
         Decimal128("99999999999999999999")});
}

TEST(MathDecimalFunctionTest, SignDecimal128Scale0) {
    TestSignDecimal128(0, {Decimal128("42"), Decimal128("-17"), Decimal128("0")});
}
