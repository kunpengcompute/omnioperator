/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: NaNvl function unit tests
*/

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Nanvl.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class NanvlTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const nanvl_test_env = ::testing::AddGlobalTestEnvironment(new NanvlTestEnvironment);

class NanvlFunctionTestHelper {
public:
    template<typename T>
    static void ValidateNumericResult(BaseVector* result, const std::vector<T>& expected,
                                    const std::vector<bool>& expectedNulls, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<T>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (expectedNulls[i]) {
                EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
                continue;
            }
            EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
            T actualValue = resultVec->GetValue(i);
            T expectedValue = expected[i];
            if (std::isnan(expectedValue)) {
                EXPECT_TRUE(std::isnan(actualValue)) << "Row " << i << " should be NaN";
            } else if (std::isinf(expectedValue)) {
                EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " Infinity value mismatch";
            } else if constexpr (std::is_floating_point_v<T>) {
                EXPECT_NEAR(actualValue, expectedValue, 1e-6)
                    << "Row " << i << " value mismatch";
            } else {
                EXPECT_EQ(actualValue, expectedValue)
                    << "Row " << i << " value mismatch";
            }
        }
    }

    template<typename T>
    static BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, values.size());
        auto* typedVec = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    template<typename T>
    static void ExecuteNanvl(BaseVector* expr1Vec, BaseVector* expr2Vec,
                            DataTypeId outputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("nanvl",
            std::vector<DataTypeId>{outputTypeId, outputTypeId}, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Nanvl function not found for type "
                                    << static_cast<int>(outputTypeId);

        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(expr1Vec->GetSize());
        std::stack<BaseVector*> args;

        // Push arguments: expr1 first, then expr2 (Apply pops expr2 first, then expr1)
        args.push(expr1Vec);
        args.push(expr2Vec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Nanvl function threw an exception";
    }
};

// =====================================================
// Double type tests
// =====================================================

// Test 1: nanvl with normal (non-NaN) double value - should return expr1
TEST(NanvlTest, DoubleNormalValue) {
    std::vector<double> expr1Values = {5.0, 3.14, -2.5};
    std::vector<double> expr2Values = {10.0, 20.0, 30.0};
    std::vector<double> expected = {5.0, 3.14, -2.5};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(expr1Values.size()));

    delete resultVec;
}

// Test 2: nanvl with NaN expr1 - should return expr2
TEST(NanvlTest, DoubleNanExpr1) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {nan, nan, nan};
    std::vector<double> expr2Values = {5.0, 10.0, 20.0};
    std::vector<double> expected = {5.0, 10.0, 20.0};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(expr1Values.size()));

    delete resultVec;
}

// Test 3: nanvl with NULL expr1 - should return NULL
TEST(NanvlTest, DoubleNullExpr1) {
    std::vector<double> expr1Values = {0.0, 0.0, 0.0};
    std::vector<double> expr2Values = {5.0, 10.0, 20.0};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<double> expected = {0.0, 0.0, 0.0}; // placeholder, won't be checked

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    // Set all expr1 values to NULL
    for (int i = 0; i < 3; ++i) {
        expr1Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// Test 4: nanvl with NaN expr1 and NULL expr2 - should return NULL
TEST(NanvlTest, DoubleNanExpr1NullExpr2) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {nan, nan};
    std::vector<double> expr2Values = {0.0, 0.0}; // placeholder
    std::vector<bool> expectedNulls = {true, true};
    std::vector<double> expected = {0.0, 0.0}; // placeholder

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    // Set all expr2 values to NULL
    for (int i = 0; i < 2; ++i) {
        expr2Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// Test 5: nanvl with NaN expr1 and NaN expr2 - should return NaN (expr2)
TEST(NanvlTest, DoubleNanBothArgs) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {nan};
    std::vector<double> expr2Values = {nan};
    std::vector<double> expected = {nan};
    std::vector<bool> expectedNulls = {false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 6: nanvl with Infinity expr1 - Infinity is NOT NaN, should return expr1
TEST(NanvlTest, DoubleInfinityExpr1) {
    double posInf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();
    std::vector<double> expr1Values = {posInf, negInf};
    std::vector<double> expr2Values = {100.0, 200.0};
    std::vector<double> expected = {posInf, negInf};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// Test 7: nanvl with zero and negative zero - should return expr1
TEST(NanvlTest, DoubleZeroValues) {
    std::vector<double> expr1Values = {0.0, -0.0};
    std::vector<double> expr2Values = {100.0, 200.0};
    std::vector<double> expected = {0.0, -0.0};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// Test 8: nanvl with mixed values (comprehensive double test)
TEST(NanvlTest, DoubleMixedValues) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double posInf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();

    // Row 0: normal value -> return expr1 (3.14)
    // Row 1: NaN expr1 -> return expr2 (100.0)
    // Row 2: NULL expr1 -> return NULL
    // Row 3: Infinity -> return expr1 (Infinity)
    // Row 4: -Infinity -> return expr1 (-Infinity)
    // Row 5: 0.0 -> return expr1 (0.0)
    // Row 6: NaN expr1 with NULL expr2 -> return NULL
    std::vector<double> expr1Values = {3.14, nan, 0.0, posInf, negInf, 0.0, nan};
    std::vector<double> expr2Values = {10.0, 100.0, 200.0, 50.0, 80.0, 90.0, 0.0};
    std::vector<double> expected = {3.14, 100.0, 0.0, posInf, negInf, 0.0, 0.0};
    std::vector<bool> expectedNulls = {false, false, true, false, false, false, true};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    // Set row 2 expr1 to NULL
    expr1Vec->SetNull(2);
    // Set row 6 expr2 to NULL
    expr2Vec->SetNull(6);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 7);

    delete resultVec;
}

// =====================================================
// Float type tests
// =====================================================

// Test 9: nanvl with normal (non-NaN) float value - should return expr1
TEST(NanvlTest, FloatNormalValue) {
    std::vector<float> expr1Values = {5.0f, 3.14f, -2.5f};
    std::vector<float> expr2Values = {10.0f, 20.0f, 30.0f};
    std::vector<float> expected = {5.0f, 3.14f, -2.5f};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(expr1Values.size()));

    delete resultVec;
}

// Test 10: nanvl with NaN float expr1 - should return expr2
TEST(NanvlTest, FloatNanExpr1) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> expr1Values = {nan, nan, nan};
    std::vector<float> expr2Values = {5.0f, 10.0f, 20.0f};
    std::vector<float> expected = {5.0f, 10.0f, 20.0f};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(expr1Values.size()));

    delete resultVec;
}

// Test 11: nanvl with NULL float expr1 - should return NULL
TEST(NanvlTest, FloatNullExpr1) {
    std::vector<float> expr1Values = {0.0f, 0.0f, 0.0f};
    std::vector<float> expr2Values = {5.0f, 10.0f, 20.0f};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<float> expected = {0.0f, 0.0f, 0.0f}; // placeholder

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    // Set all expr1 values to NULL
    for (int i = 0; i < 3; ++i) {
        expr1Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// Test 12: nanvl with NaN float expr1 and NULL expr2 - should return NULL
TEST(NanvlTest, FloatNanExpr1NullExpr2) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> expr1Values = {nan, nan};
    std::vector<float> expr2Values = {0.0f, 0.0f}; // placeholder
    std::vector<bool> expectedNulls = {true, true};
    std::vector<float> expected = {0.0f, 0.0f}; // placeholder

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    // Set all expr2 values to NULL
    for (int i = 0; i < 2; ++i) {
        expr2Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// Test 13: nanvl with NaN float both args - should return NaN
TEST(NanvlTest, FloatNanBothArgs) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> expr1Values = {nan};
    std::vector<float> expr2Values = {nan};
    std::vector<float> expected = {nan};
    std::vector<bool> expectedNulls = {false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 14: nanvl with Infinity float expr1 - should return expr1
TEST(NanvlTest, FloatInfinityExpr1) {
    float posInf = std::numeric_limits<float>::infinity();
    float negInf = -std::numeric_limits<float>::infinity();
    std::vector<float> expr1Values = {posInf, negInf};
    std::vector<float> expr2Values = {100.0f, 200.0f};
    std::vector<float> expected = {posInf, negInf};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// Test 15: nanvl with mixed float values (comprehensive float test)
TEST(NanvlTest, FloatMixedValues) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    float posInf = std::numeric_limits<float>::infinity();
    float negInf = -std::numeric_limits<float>::infinity();

    // Row 0: normal value -> return expr1 (3.14f)
    // Row 1: NaN expr1 -> return expr2 (100.0f)
    // Row 2: NULL expr1 -> return NULL
    // Row 3: Infinity -> return expr1 (Infinity)
    // Row 4: -Infinity -> return expr1 (-Infinity)
    // Row 5: 0.0f -> return expr1 (0.0f)
    // Row 6: NaN expr1 with NULL expr2 -> return NULL
    std::vector<float> expr1Values = {3.14f, nan, 0.0f, posInf, negInf, 0.0f, nan};
    std::vector<float> expr2Values = {10.0f, 100.0f, 200.0f, 50.0f, 80.0f, 90.0f, 0.0f};
    std::vector<float> expected = {3.14f, 100.0f, 0.0f, posInf, negInf, 0.0f, 0.0f};
    std::vector<bool> expectedNulls = {false, false, true, false, false, false, true};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    // Set row 2 expr1 to NULL
    expr1Vec->SetNull(2);
    // Set row 6 expr2 to NULL
    expr2Vec->SetNull(6);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 7);

    delete resultVec;
}

// =====================================================
// Spark reference test cases (from NullExpressionsSuite.scala)
// =====================================================

// Test 16: Spark reference - nanvl(5.0, null) = 5.0
TEST(NanvlTest, SparkRef_NormalValueWithNullExpr2) {
    std::vector<double> expr1Values = {5.0};
    std::vector<double> expr2Values = {0.0}; // placeholder
    std::vector<double> expected = {5.0};
    std::vector<bool> expectedNulls = {false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);
    expr2Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 17: Spark reference - nanvl(null, 5.0) = null
TEST(NanvlTest, SparkRef_NullWithNormalExpr2) {
    std::vector<double> expr1Values = {0.0}; // placeholder
    std::vector<double> expr2Values = {5.0};
    std::vector<double> expected = {0.0}; // placeholder
    std::vector<bool> expectedNulls = {true};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);
    expr1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 18: Spark reference - nanvl(null, NaN) = null
TEST(NanvlTest, SparkRef_NullWithNanExpr2) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {0.0}; // placeholder
    std::vector<double> expr2Values = {nan};
    std::vector<double> expected = {0.0}; // placeholder
    std::vector<bool> expectedNulls = {true};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);
    expr1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 19: Spark reference - nanvl(NaN, 5.0) = 5.0
TEST(NanvlTest, SparkRef_NanWithNormalExpr2) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {nan};
    std::vector<double> expr2Values = {5.0};
    std::vector<double> expected = {5.0};
    std::vector<bool> expectedNulls = {false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 20: Spark reference - nanvl(NaN, null) = null
TEST(NanvlTest, SparkRef_NanWithNullExpr2) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {nan};
    std::vector<double> expr2Values = {0.0}; // placeholder
    std::vector<double> expected = {0.0}; // placeholder
    std::vector<bool> expectedNulls = {true};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);
    expr2Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

// Test 21: Spark reference - nanvl(NaN, NaN) = NaN
TEST(NanvlTest, SparkRef_NanWithNanExpr2) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> expr1Values = {nan};
    std::vector<double> expr2Values = {nan};
    std::vector<double> expected = {nan};
    std::vector<bool> expectedNulls = {false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0)) << "Result should not be NULL";
    auto* resultVector = dynamic_cast<Vector<double>*>(resultVec);
    ASSERT_NE(resultVector, nullptr);
    EXPECT_TRUE(std::isnan(resultVector->GetValue(0))) << "Result should be NaN";
    delete resultVec;
}

// Test 22: nanvl with max/min double values - boundary test
TEST(NanvlTest, DoubleBoundaryValues) {
    std::vector<double> expr1Values = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::epsilon()
    };
    std::vector<double> expr2Values = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> expected = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::epsilon()
    };
    std::vector<bool> expectedNulls = {false, false, false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<double>(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 4);

    delete resultVec;
}

// Test 23: nanvl with max/min float values - boundary test
TEST(NanvlTest, FloatBoundaryValues) {
    std::vector<float> expr1Values = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::epsilon()
    };
    std::vector<float> expr2Values = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> expected = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::epsilon()
    };
    std::vector<bool> expectedNulls = {false, false, false, false};

    BaseVector* expr1Vec = NanvlFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NanvlFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NanvlFunctionTestHelper::ExecuteNanvl<float>(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NanvlFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 4);

    delete resultVec;
}