/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* Description: Unit tests for mathematical functions (acos, acosh, asin, asinh, atan, atan2, atanh, cos, cosh, cot, csc, sec, sin, tan, tanh, degrees, radians)
*/
 	 
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/MathFunctions.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "codegen/func_registry.h"
#include "type/decimal_operations.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::codegen;
using namespace omniruntime::type;

class MathFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper function to test unary mathematical operations
template<typename T, DataTypeId typeId, DataTypeId returnId>
void TestUnaryMathOperation(
        const std::string& functionName,
        const std::vector<T>& inputData,
        const std::vector<T>& expectedResults,
        double tolerance = 1e-6) {

     int32_t rowSize = static_cast<int32_t>(inputData.size());

     BaseVector* rawInput = VectorHelper::CreateFlatVector(typeId, rowSize);
     auto* inputVector = static_cast<Vector<T>*>(rawInput);
     for (int32_t i = 0; i < rowSize; ++i) {
         inputVector->SetValue(i, inputData[i]);
         inputVector->SetNotNull(i);
     }

     std::vector<DataTypeId> argTypes = {typeId};
     auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, returnId);
     auto vectorFunction = VectorFunction::Find(signature);
     ASSERT_NE(vectorFunction, nullptr);

     ExecutionContext context;
     context.SetResultRowSize(rowSize);

     std::stack<BaseVector*> args;
     args.push(rawInput); // Apply() will delete it

     BaseVector* rawResult = nullptr;
     auto resultType = std::make_shared<DataType>(typeId);
     vectorFunction->Apply(args, resultType, rawResult, &context);
     ASSERT_NE(rawResult, nullptr);
     auto* resultVector = static_cast<Vector<typename NativeType<returnId>::type>*>(rawResult);
     ASSERT_NE(resultVector, nullptr);

     for (int32_t i = 0; i < rowSize; ++i) {
         T actual = resultVector->GetValue(i);
         T expected = expectedResults[i];

         if constexpr (std::is_floating_point_v<T>) {
             if (std::isnan(expected)) {
                 EXPECT_TRUE(std::isnan(actual))
                     << "NaN mismatch at index " << i << " for " << functionName
                     << " with input=" << inputData[i];
             } else if (std::isinf(expected)) {
                 EXPECT_TRUE(std::isinf(actual) &&
                             std::signbit(actual) == std::signbit(expected))
                     << "Infinity mismatch at index " << i << " for " << functionName
                     << " with input=" << inputData[i];
             } else {
                 EXPECT_NEAR(actual, expected, tolerance)
                     << "Value mismatch at index " << i << " for " << functionName
                     << " with input=" << inputData[i]
                     << ", expected=" << expected << ", actual=" << actual;
             }
         } else {
             EXPECT_EQ(actual, expected)
                 << "Value mismatch at index " << i << " for " << functionName
                 << " with input=" << inputData[i];
         }
     }

     delete rawResult;
}


// Helper function to test binary mathematical operations
 	 template<typename T, DataTypeId typeId>
 	 void TestBinaryMathOperation(const std::string& functionName,
 	                              const std::vector<T>& leftData,
 	                              const std::vector<T>& rightData,
 	                              const std::vector<T>& expectedResults,
 	                              double tolerance = 1e-6) {
 	     int32_t rowSize = static_cast<int32_t>(leftData.size());

 	     // Create left vector
 	     BaseVector* leftVec = VectorHelper::CreateFlatVector(typeId, rowSize);
 	     auto* leftVector = static_cast<Vector<T>*>(leftVec);
 	     for (int32_t i = 0; i < rowSize; ++i) {
 	         leftVector->SetValue(i, leftData[i]);
 	         leftVector->SetNotNull(i);
 	     }

 	     // Create right vector
 	     BaseVector* rightVec = VectorHelper::CreateFlatVector(typeId, rowSize);
 	     auto* rightVector = static_cast<Vector<T>*>(rightVec);
 	     for (int32_t i = 0; i < rowSize; ++i) {
 	         rightVector->SetValue(i, rightData[i]);
 	         rightVector->SetNotNull(i);
 	     }

 	     // Create function signature
 	     std::vector<DataTypeId> argTypes = {typeId, typeId};
 	     auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, typeId);

 	     // Find vector function
 	     auto vectorFunction = VectorFunction::Find(signature);
 	     ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName << " not found for type " << static_cast<int>(typeId);

 	     // Create execution context
 	     ExecutionContext context;
 	     context.SetResultRowSize(rowSize);

 	     // Prepare arguments stack
 	     std::stack<BaseVector*> args;

 	     args.push(leftVec);
 	     args.push(rightVec);

 	     // Execute function
 	     BaseVector* result = nullptr;
 	     auto resultType = std::make_shared<DataType>(typeId);
 	     vectorFunction->Apply(args, resultType, result, &context);

 	     // Verify results
 	     auto* resultVector = static_cast<Vector<T>*>(result);
 	     ASSERT_NE(resultVector, nullptr);

 	     for (int32_t i = 0; i < rowSize; ++i) {
 	         T actualResult = resultVector->GetValue(i);
 	         T expectedResult = expectedResults[i];

 	         if (std::isnan(expectedResult)) {
 	             EXPECT_TRUE(std::isnan(actualResult))
 	                 << "NaN mismatch at index " << i << " for " << functionName;
 	         } else if (std::isinf(expectedResult)) {
 	             EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
 	                 << "Infinity mismatch at index " << i << " for " << functionName;
 	         } else {
 	             EXPECT_NEAR(actualResult, expectedResult, tolerance)
 	                 << "Value mismatch at index " << i << " for " << functionName
 	                 << " with left=" << leftData[i] << ", right=" << rightData[i]
 	                 << ", expected=" << expectedResult << ", actual=" << actualResult;
 	         }
 	     }
 	     // Cleanup
 	     delete result;
 	 }

// Helper: round(expr, scale)
// leftData = expr, rightData = scale (int32_t), argTypes = {typeId, OMNI_INT}, return typeId.
template <typename T, DataTypeId typeId>
static void TestBinaryRoundOperation(const std::string &functionName,
    const std::vector<T> &leftData, const std::vector<int32_t> &rightData,
    const std::vector<T> &expectedResults, double tolerance = 1e-6) {
    int32_t rowSize = static_cast<int32_t>(leftData.size());
    ASSERT_EQ(rightData.size(), static_cast<size_t>(rowSize));
    ASSERT_EQ(expectedResults.size(), static_cast<size_t>(rowSize));

    BaseVector *leftVec = VectorHelper::CreateFlatVector(typeId, rowSize);
    auto *leftVector = static_cast<Vector<T> *>(leftVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        leftVector->SetValue(i, leftData[i]);
        leftVector->SetNotNull(i);
    }

    BaseVector *rightVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *rightVector = static_cast<Vector<int32_t> *>(rightVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        rightVector->SetValue(i, rightData[i]);
        rightVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {typeId, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, typeId);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName << " not found for type " << static_cast<int>(typeId);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(leftVec);
    args.push(rightVec);

    BaseVector *result = nullptr;
    auto resultType = std::make_shared<DataType>(typeId);
    vectorFunction->Apply(args, resultType, result, &context);

    ASSERT_NE(result, nullptr);
    auto *resultVector = static_cast<Vector<T> *>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        T actualResult = resultVector->GetValue(i);
        T expectedResult = expectedResults[i];
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(expectedResult)) {
                EXPECT_TRUE(std::isnan(actualResult)) << "NaN mismatch at index " << i << " for " << functionName;
            } else if (std::isinf(expectedResult)) {
                EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                    << "Infinity mismatch at index " << i << " for " << functionName;
            } else {
                EXPECT_NEAR(actualResult, expectedResult, tolerance)
                    << "Value mismatch at index " << i << " for " << functionName
                    << " with left=" << leftData[i] << ", right(scale)=" << rightData[i]
                    << ", expected=" << expectedResult << ", actual=" << actualResult;
            }
        } else {
            EXPECT_EQ(actualResult, expectedResult)
                << "Value mismatch at index " << i << " for " << functionName
                << " with left=" << leftData[i] << ", right(scale)=" << rightData[i];
        }
    }
    delete result;
}

// Test acosh function
TEST(MathFunctionsTest, AcoshDouble) {
    double tolerance = 1e-6;
    std::vector<double> inputData = {1.0, 2.0, 5.0, 10.0};
    std::vector<double> expectedResults;
    std::string functionName = "acosh";
    for (double x : inputData) {
        expectedResults.push_back(std::acosh(x));
    }
    int rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, type)};
    auto funcExpr = new FuncExpr("acosh", args, returnType);
    std::vector<DataTypeId> sigArgs = {OMNI_DOUBLE};
    double col1[4] = {1.0, 2.0, 5.0, 10.0};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    std::cout << "=== acosh input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== acosh Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<double> *>(result);


    for (int32_t i = 0; i < rowSize; ++i) {
        double actualResult = resultVector->GetValue(i);
        double expectedResult = expectedResults[i];

        if (std::isnan(expectedResult)) {
            EXPECT_TRUE(std::isnan(actualResult))
                << "NaN mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else if (std::isinf(expectedResult)) {
            EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                << "Infinity mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else {
            EXPECT_NEAR(actualResult, expectedResult, tolerance)
                << "Value mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }
    delete input;
    delete funcExpr;
    delete context;
}

// Test abs function
TEST(MathFunctionsTest, AbsByte) {
    std::vector<int8_t> inputData = {1, 0, 0, -5, -1};
    std::vector<int8_t> expectedResults;
    for (int8_t x : inputData) {
        expectedResults.push_back(std::abs(x));
    }
    TestUnaryMathOperation<int8_t, OMNI_BYTE, OMNI_BYTE>("abs", inputData, expectedResults);
}
TEST(MathFunctionsTest, AbsShort) {
    std::vector<int16_t> inputData = {1, 5, 0, -5, -1};
    std::vector<int16_t> expectedResults;
    for (int16_t x : inputData) {
        expectedResults.push_back(std::abs(x));
    }
    TestUnaryMathOperation<int16_t, OMNI_SHORT, OMNI_SHORT>("abs", inputData, expectedResults);
}
TEST(MathFunctionsTest, AbsInt) {
    std::vector<int32_t> inputData = {1, 5, 0, -5, -1};
    std::vector<int32_t> expectedResults;
    for (int32_t x : inputData) {
        expectedResults.push_back(std::abs(x));
    }
    TestUnaryMathOperation<int32_t, OMNI_INT, OMNI_INT>("abs", inputData, expectedResults);
}
TEST(MathFunctionsTest, AbsLong) {
    std::vector<int64_t> inputData = {1, 5, 0, -5, -1};
    std::vector<int64_t> expectedResults;
    for (int64_t x : inputData) {
        expectedResults.push_back(std::abs(x));
    }
    TestUnaryMathOperation<int64_t, OMNI_LONG, OMNI_LONG>("abs", inputData, expectedResults);
}
TEST(MathFunctionsTest, AbsFloat) {
    std::vector<float> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<float> expectedResults;
    for (float x : inputData) {
        expectedResults.push_back(std::abs(x));
    }
    TestUnaryMathOperation<float, OMNI_FLOAT, OMNI_FLOAT>("abs", inputData, expectedResults);
}

TEST(MathFunctionsTest, AbsDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::abs(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("abs", inputData, expectedResults);
}

// Test acos function
TEST(MathFunctionsTest, AcosDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::acos(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("acos", inputData, expectedResults);
}

// Test negative function for double
TEST(MathFunctionsTest, NegativeDouble) {
    std::vector<double> inputData = {1.0, -1.0, 0.0, 3.14, -3.14, 100.5, -100.5};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(-x);
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("negative", inputData, expectedResults, 1e-9);
}

// Test negative function for float
TEST(MathFunctionsTest, NegativeFloat) {
    std::vector<float> inputData = {1.0f, -1.0f, 0.0f, 3.14f, -3.14f, 100.5f, -100.5f};
    std::vector<float> expectedResults;
    for (float x : inputData) {
        expectedResults.push_back(-x);
    }
    TestUnaryMathOperation<float, OMNI_FLOAT, OMNI_FLOAT>("negative", inputData, expectedResults, 1e-6);
}

// Test negative function for int32
TEST(MathFunctionsTest, NegativeInt) {
    std::vector<int32_t> inputData = {1, -1, 0, 100, -100, 2147483647, -2147483648};
    std::vector<int32_t> expectedResults;
    for (int32_t x : inputData) {
        expectedResults.push_back(-x);
    }
    TestUnaryMathOperation<int32_t, OMNI_INT, OMNI_INT>("negative", inputData, expectedResults, 0);
}

// Test negative function for int64
TEST(MathFunctionsTest, NegativeLong) {
    std::vector<int64_t> inputData = {1LL, -1LL, 0LL, 100LL, -100LL, 9223372036854775807LL, -9223372036854775807LL};
    std::vector<int64_t> expectedResults;
    for (int64_t x : inputData) {
        expectedResults.push_back(-x);
    }
    TestUnaryMathOperation<int64_t, OMNI_LONG, OMNI_LONG>("negative", inputData, expectedResults, 0);
}

// Test negative function for int16
TEST(MathFunctionsTest, NegativeShort) {
    std::vector<int16_t> inputData = {1, -1, 0, 100, -100, 32767, -32768};
    std::vector<int16_t> expectedResults;
    for (int16_t x : inputData) {
        expectedResults.push_back(-x);
    }
    TestUnaryMathOperation<int16_t, OMNI_SHORT, OMNI_SHORT>("negative", inputData, expectedResults, 0);
}

// Test negative function for int8
TEST(MathFunctionsTest, NegativeByte) {
    std::vector<int8_t> inputData = {1, -1, 0, 100, -100, 127, -128};
    std::vector<int8_t> expectedResults;
    for (int8_t x : inputData) {
        expectedResults.push_back(-x);
    }
    TestUnaryMathOperation<int8_t, OMNI_BYTE, OMNI_BYTE>("negative", inputData, expectedResults, 0);
}

// Test negative function with edge cases
TEST(MathFunctionsTest, NegativeEdgeCases) {
    // Test with zero
    std::vector<double> zeroInput = {0.0};
    std::vector<double> zeroExpected = {0.0};
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("negative", zeroInput, zeroExpected, 1e-9);

    // Test with very small numbers
    std::vector<double> smallInput = {1e-10, -1e-10};
    std::vector<double> smallExpected = {-1e-10, 1e-10};
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("negative", smallInput, smallExpected, 1e-15);

    // Test with very large numbers
    std::vector<double> largeInput = {1e10, -1e10};
    std::vector<double> largeExpected = {-1e10, 1e10};
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("negative", largeInput, largeExpected, 1e-9);
}

// Test negative function for OMNI_DECIMAL64
TEST(MathFunctionsTest, NegativeDecimal64) {
    std::vector<int64_t> inputData = {123LL, -456LL, 0L, 999999999999999999LL};
    std::vector<int64_t> expectedResults = {-123LL, 456LL, 0L, -999999999999999999LL};
    TestUnaryMathOperation<int64_t, OMNI_DECIMAL64, OMNI_DECIMAL64>("negative", inputData, expectedResults, 0);
}

// Test negative function for OMNI_DECIMAL128
// Decimal128's MAX_LONG_PRECISION is 38, use 99999999999999999999999999999999999999 as Max
TEST(MathFunctionsTest, NegativeDecimal128) {
    std::vector<Decimal128> inputData = {Decimal128("1234567890"), Decimal128("-1234567890"), Decimal128("0"), Decimal128("99999999999999999999999999999999999999")};
    std::vector<Decimal128> expectedResults = {Decimal128("-1234567890"), Decimal128("1234567890"), Decimal128("0"), Decimal128("-99999999999999999999999999999999999999")};
    TestUnaryMathOperation<Decimal128, OMNI_DECIMAL128, OMNI_DECIMAL128>("negative", inputData, expectedResults, 0);
}

// Test asin function
TEST(MathFunctionsTest, AsinDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::asin(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("asin", inputData, expectedResults);
}


// Test asinh function
TEST(MathFunctionsTest, AsinhDouble) {
    std::vector<double> inputData = {1.0, 1.5, 0.0, -0.5, -1.5};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::asinh(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("asinh", inputData, expectedResults);
}

// Test atan function
TEST(MathFunctionsTest, AtanDouble) {
    std::vector<double> inputData = {1.0, 1.5, 0.0, -0.5, -1.5};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::atan(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("atan", inputData, expectedResults);
}

// Test atan2 function
TEST(MathFunctionsTest, Atan2Double) {
    std::vector<double> inputData_x = {1.0, 0.5, 0.0, -0.5, -1.0};
	std::vector<double> inputData_y = {2.0, 1.0, 0.0, -1, -2};
    std::vector<double> expectedResults;
	for (int i = 0; i < inputData_x.size(); i++) {
		expectedResults.push_back(std::atan2(inputData_x[i] + 0.0, inputData_y[i] + 0.0));
	}
    TestBinaryMathOperation<double, OMNI_DOUBLE>("atan2", inputData_x, inputData_y, expectedResults);
}


// Test cos function
TEST(MathFunctionsTest, CosDouble) {
    std::vector<double> inputData = {1.0, 5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::cos(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("cos", inputData, expectedResults);
}


// Test cosh function
TEST(MathFunctionsTest, CoshDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::cosh(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("cosh", inputData, expectedResults);
}

// Test sin function
TEST(MathFunctionsTest, SinDouble) {
    std::vector<double> inputData = {1.0, 5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::sin(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sin", inputData, expectedResults);
}

// Test tan function
TEST(MathFunctionsTest, TanDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::tan(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("tan", inputData, expectedResults);
}

// Test tanh function
TEST(MathFunctionsTest, TanhDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0, 10.0, -10.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::tanh(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("tanh", inputData, expectedResults);
}

// Test cbrt function
TEST(MathFunctionsTest, CbrtDouble) {
    std::vector<double> inputData = {64, 8.1, 0.0, 132, -119};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::cbrt(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("cbrt", inputData, expectedResults);
}


// Test ceil double function
TEST(MathFunctionsTest, ceilDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::ceil(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_LONG>("ceil", inputData, expectedResults);
}


// Test ceil long function
TEST(MathFunctionsTest, CeilLong) {
    std::vector<int64_t> inputData = {std::numeric_limits<int64_t>::max(), 25, 11, 41, 1};
    std::vector<int64_t> expectedResults;
    for (int64_t x : inputData) {
        expectedResults.push_back(std::ceil(x));
    }
    TestUnaryMathOperation<int64_t, OMNI_LONG, OMNI_LONG>("ceil", inputData, expectedResults);
}

// Test ceil double function with DOUBLE return type (Flink semantics)
TEST(MathFunctionsTest, CeilDoubleReturnDouble) {
    std::vector<double> inputData = {1.0, 0.5, 0.0, -0.5, -1.0, 2.878, -2.878};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::ceil(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("ceil", inputData, expectedResults);
}

// Test sign function
TEST(MathFunctionsTest, SignDouble) {
    std::vector<double> inputData = {0.0, -0.0, 10.1, -10.1, 0.5, -0.5};
    std::vector<double> expectedResults = {0.0, 0.0, 1.0, -1.0, 1.0, -1.0};
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sign", inputData, expectedResults);
}

// Test sign function with edge cases (NULL, NaN, Infinity, zero)
TEST(MathFunctionsTest, SignDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    // Test cases: 0, NaN, infinity, -infinity, positive, negative
    std::vector<double> inputData = {0.0, kNanVal, kInfVal, -kInfVal, 100.0, -100.0};
    // sign(0) = 0, sign(NaN) = NaN, sign(inf) = 1, sign(-inf) = -1, sign(100) = 1, sign(-100) = -1
    std::vector<double> expectedResults = {0.0, kNanVal, 1.0, -1.0, 1.0, -1.0};
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sign", inputData, expectedResults);
}

// Test sinh function
TEST(MathFunctionsTest, SinhDouble) {
    std::vector<double> inputData = {0.0, 1.0, -1.0, 2.0, -2.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::sinh(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sinh", inputData, expectedResults);
}

// Test sinh function with edge cases (NULL, NaN, Infinity)
TEST(MathFunctionsTest, SinhDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    // Test cases: 0, NaN, infinity, -infinity
    std::vector<double> inputData = {0.0, kNanVal, kInfVal, -kInfVal};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::sinh(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sinh", inputData, expectedResults);
}

// Test rand() - no args, returns double in [0, 1)
TEST(MathFunctionsTest, RandNoArg) {
    const int32_t rowSize = 16;
    std::vector<DataTypeId> argTypes = {};
    auto signature = std::make_shared<FunctionSignature>("rand", argTypes, OMNI_DOUBLE);
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
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        double v = resultVector->GetValue(i);
        EXPECT_GE(v, 0.0) << "rand() at index " << i;
        EXPECT_LT(v, 1.0) << "rand() at index " << i;
    }
    delete rawResult;
}

// Test rand(seed) - Velox semantics: one generator per batch, each row gets next value (sequence, not constant); Spark seed is int only
TEST(MathFunctionsTest, RandWithSeed) {
    const int32_t rowSize = 4;
    std::vector<int32_t> seedData = {42, 42, 42, 42};
    BaseVector *seedVec = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    auto *seedVector = static_cast<Vector<int32_t> *>(seedVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        seedVector->SetValue(i, seedData[i]);
        seedVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("rand", argTypes, OMNI_DOUBLE);
    // Seed is column (non-constant), so pass { nullptr } so UnpackInitialize gets packed.size() == 1
    std::vector<BaseVector *> constantInputs = { nullptr };
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
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        double v = resultVector->GetValue(i);
        EXPECT_GE(v, 0.0) << "row " << i;
        EXPECT_LT(v, 1.0) << "row " << i;
    }
    // Velox: same batch with same seed produces a sequence (not constant encoding)
    double first = resultVector->GetValue(0);
    bool allSame = true;
    for (int32_t i = 1; i < rowSize && allSame; ++i) {
        if (resultVector->GetValue(i) != first) allSame = false;
    }
    EXPECT_FALSE(allSame) << "rand(seed) batch should produce a sequence, not all same (Velox-aligned)";
    delete rawResult;
}

// Test sqrt function
TEST(MathFunctionsTest, SqrtDouble) {
    std::vector<double> inputData = {0.0, 4.0, 9.0, 16.0, 25.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::sqrt(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sqrt", inputData, expectedResults);
}

// Test sqrt function with edge cases (NULL, NaN, Infinity, negative numbers)
TEST(MathFunctionsTest, SqrtDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    // Test cases: 0, 4, NaN, infinity, -1 (should be NaN)
    std::vector<double> inputData = {0.0, 4.0, kNanVal, kInfVal, -1.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::sqrt(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sqrt", inputData, expectedResults);
}

// Test hypot function - basic cases
// hypot(x, y) = sqrt(x² + y²)
TEST(MathFunctionsTest, HypotDoubleBasic) {
    // Test basic Pythagorean triples and common cases
    std::vector<double> leftData = {3.0, -3.0, 3.0, 3.5, 3.5, 0.0, 5.0, 12.0};
    std::vector<double> rightData = {4.0, -4.0, -4.0, 4.5, -4.5, 0.0, 12.0, 5.0};
    std::vector<double> expectedResults;
    for (size_t i = 0; i < leftData.size(); ++i) {
        expectedResults.push_back(std::hypot(leftData[i], rightData[i]));
    }
    // Expected results:
    // hypot(3, 4) = 5
    // hypot(-3, -4) = 5
    // hypot(3, -4) = 5
    // hypot(3.5, 4.5) ≈ 5.70087712549569
    // hypot(3.5, -4.5) ≈ 5.70087712549569
    // hypot(0, 0) = 0
    // hypot(5, 12) = 13
    // hypot(12, 5) = 13
    TestBinaryMathOperation<double, OMNI_DOUBLE>("hypot", leftData, rightData, expectedResults);
}

// Test hypot function with edge cases (NaN, Infinity, zero)
TEST(MathFunctionsTest, HypotDoubleEdgeCases) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

    // Test edge cases:
    // - hypot(0, 0) = 0
    // - hypot(inf, x) = inf (for any finite x)
    // - hypot(x, inf) = inf (for any finite x)
    // - hypot(-inf, x) = inf
    // - hypot(nan, x) = nan
    // - hypot(x, nan) = nan
    // - hypot(inf, nan) = inf (IEEE 754 special case)
    std::vector<double> leftData = {0.0, kInf, 1.0, -kInf, kNan, 1.0, kInf};
    std::vector<double> rightData = {0.0, 1.0, kInf, 1.0, 1.0, kNan, kNan};
    std::vector<double> expectedResults;
    for (size_t i = 0; i < leftData.size(); ++i) {
        expectedResults.push_back(std::hypot(leftData[i], rightData[i]));
    }
    TestBinaryMathOperation<double, OMNI_DOUBLE>("hypot", leftData, rightData, expectedResults);
}

// Test hypot function with very small and very large numbers
TEST(MathFunctionsTest, HypotDoubleExtremeValues) {
    // Test with very small numbers (underflow prevention)
    std::vector<double> leftData = {1e-200, 1e200, 1e-300, 1.0};
    std::vector<double> rightData = {1e-200, 1e200, 1e-300, 1e-15};
    std::vector<double> expectedResults;
    for (size_t i = 0; i < leftData.size(); ++i) {
        expectedResults.push_back(std::hypot(leftData[i], rightData[i]));
    }
    // std::hypot handles overflow/underflow better than sqrt(x*x + y*y)
    TestBinaryMathOperation<double, OMNI_DOUBLE>("hypot", leftData, rightData, expectedResults, 1e-10);
}

// Test hypot function with ExprEval integration
TEST(MathFunctionsTest, HypotDoubleExprEval) {
    double tolerance = 1e-6;
    std::vector<double> leftData = {3.0, 5.0, 8.0, 0.0};
    std::vector<double> rightData = {4.0, 12.0, 15.0, 0.0};
    std::vector<double> expectedResults;
    std::string functionName = "hypot";
    for (size_t i = 0; i < leftData.size(); ++i) {
        expectedResults.push_back(std::hypot(leftData[i], rightData[i]));
    }
    int rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};
    auto funcExpr = new FuncExpr("hypot", args, returnType);

    double col1[4] = {3.0, 5.0, 8.0, 0.0};
    double col2[4] = {4.0, 12.0, 15.0, 0.0};
    std::vector vecOfTypes = {DoubleType(), DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    std::cout << "=== hypot input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== hypot Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<double> *>(result);

    for (int32_t i = 0; i < rowSize; ++i) {
        double actualResult = resultVector->GetValue(i);
        double expectedResult = expectedResults[i];

        if (std::isnan(expectedResult)) {
            EXPECT_TRUE(std::isnan(actualResult))
                << "NaN mismatch at index " << i << " for " << functionName
                << " with left=" << leftData[i] << ", right=" << rightData[i];
        } else if (std::isinf(expectedResult)) {
            EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                << "Infinity mismatch at index " << i << " for " << functionName
                << " with left=" << leftData[i] << ", right=" << rightData[i];
        } else {
            EXPECT_NEAR(actualResult, expectedResult, tolerance)
                << "Value mismatch at index " << i << " for " << functionName
                << " with left=" << leftData[i] << ", right=" << rightData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }
    delete input;
    delete funcExpr;
    delete context;
}

// Test sec function
TEST(MathFunctionsTest, SecDouble) {
    // Test normal values
    std::vector<double> inputData = {0.0, M_PI / 4.0, M_PI / 3.0, M_PI / 6.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::cos(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("sec", inputData, expectedResults);
}

TEST(MathFunctionsTest, Log10Double) {
    std::vector<double> inputData = {1.0, 10.0, 100.0, 0.1, 0.01, 2.5, 7.389};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::log10(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("log10", inputData, expectedResults);
}


// Test log2 function
TEST(MathFunctionsTest, Log2Double) {
    std::vector<double> inputData = {1.0, 2.0, 4.0, 8.0, 0.5, 0.25, 16.0, 3.14};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::log2(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("log2", inputData, expectedResults);
}


// Test ln function
TEST(MathFunctionsTest, LnDouble) {
    std::vector<double> inputData = {1.0, 2.718281828459045, 7.38905609893, 0.5, 0.1, 2.0, 10.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::log(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("ln", inputData, expectedResults);
}


// Test log1p function
TEST(MathFunctionsTest, Log1pDouble) {
    std::vector<double> inputData = {0.0, 1.0, 9.0, -0.5, -0.9, 0.001, 100.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::log1p(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("log1p", inputData, expectedResults);
}


// Test generic log(base, x) function
TEST(MathFunctionsTest, LogBaseXDouble) {
    std::vector<double> bases   = {2.0, 10.0, 3.0, 5.0, 2.718281828459045}; // e
    std::vector<double> xs      = {4.0, 100.0, 9.0, 25.0, 7.38905609893};

    std::vector<double> leftData;   // base
    std::vector<double> rightData;  // x
    std::vector<double> expectedResults;

    for (size_t i = 0; i < bases.size(); ++i) {
        double base = bases[i];
        double x = xs[i];
        leftData.push_back(base);
        rightData.push_back(x);
        expectedResults.push_back(std::log(x) / std::log(base)); // change of base formula
    }

    TestBinaryMathOperation<double, OMNI_DOUBLE>("log", leftData, rightData, expectedResults);
}

// Test pmod function for int32
TEST(MathFunctionsExtendedTest, PmodInt32) {
    std::vector<int32_t> leftData = {1, -1, 3, -1, 391819};
    std::vector<int32_t> rightData = {3, 3, -2, -3, 8292};
	// pmod(1,3)=1, pmod(-1,3)=2, pmod(3,-2)=-1 (same sign as divisor), pmod(-1,-3)=-1, pmod(391819,8292)=2095
    std::vector<int32_t> expected = {1, 2, -1, -1, 2095};
	TestBinaryMathOperation<int32_t, OMNI_INT>("pmod", leftData, rightData, expected);
}

// Test pmod function for int64
// pmod(a,n): result has same sign as n. pmod(4293096798,-925): r=4293096798%(-925)=673, keepR=false, result=(673-925)%(-925)=-252 (not 673).
TEST(MathFunctionsExtendedTest, PmodInt64) {
    std::vector<int64_t> leftData = {4611791058295013614LL, -3828032596LL, 4293096798LL, -15181561541535LL};
    std::vector<int64_t> rightData = {2147532562LL, 48163LL, -925LL, -23LL};
    std::vector<int64_t> expected = {0LL, 10807LL, -252LL, -5LL};
	TestBinaryMathOperation<int64_t, OMNI_LONG>("pmod", leftData, rightData, expected);
}

// Test pmod function for double
TEST(MathFunctionsExtendedTest, PmodDouble) {
    std::vector<double> leftData = {0.5, -1.1, 0.7};
    std::vector<double> rightData = {0.3, 2.0, -0.3};
    // pmod(0.5,0.3)=0.2, pmod(-1.1,2.0)=0.9, pmod(0.7,-0.3)=-0.2 (same sign as divisor), row 3: div by zero -> NULL
    std::vector<double> expected = {0.2, 0.9, -0.2}; // Last one should be NULL due to division by zero
	TestBinaryMathOperation<double, OMNI_DOUBLE>("pmod", leftData, rightData, expected);
}

// Test pmod function with NULL values
TEST(MathFunctionsExtendedTest, PmodWithNull) {
    int rowSize = 3;
    auto returnType = std::make_shared<DataType>(OMNI_INT);
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};
    auto funcExpr = new FuncExpr("pmod", args, returnType);

    int32_t col1[3] = {10, 20, 30};
    int32_t col2[3] = {3, 5, 7};

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    // Set first value of col1 to NULL
    input->Get(0)->SetNull(0);

    std::cout << "=== pmod with NULL input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== pmod with NULL Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    // First row should be NULL
    EXPECT_TRUE(result->IsNull(0)) << "Result should be NULL when input is NULL";
    EXPECT_FALSE(result->IsNull(1)) << "Result should not be NULL for valid inputs";
    EXPECT_FALSE(result->IsNull(2)) << "Result should not be NULL for valid inputs";

    delete input;
    delete funcExpr;
    delete context;
}


TEST(MathFunctionsExtendedTest, PositiveDouble) {

    std::vector<double> inputData = {1.5, -1.5, 0.0, std::numeric_limits<double>::max(),
                      std::numeric_limits<double>::lowest(), std::numeric_limits<double>::infinity()};
    std::vector<double> expected = {1.5, -1.5, 0.0, std::numeric_limits<double>::max(),
                                    std::numeric_limits<double>::lowest(), std::numeric_limits<double>::infinity()};
	TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("positive", inputData, expected, 1e-6);
}

TEST(MathFunctionsExtendedTest, PositiveDecimal128) {
    std::vector<Decimal128> inputData = {Decimal128("1234567890"), Decimal128("-1234567890"), Decimal128("0"), Decimal128("99999999999999999999999999999999999999")};
    std::vector<Decimal128> expectedResults = {Decimal128("1234567890"), Decimal128("-1234567890"), Decimal128("0"), Decimal128("99999999999999999999999999999999999999")};
    TestUnaryMathOperation<Decimal128, OMNI_DECIMAL128, OMNI_DECIMAL128>("positive", inputData, expectedResults, 0);
}

// Test power function
TEST(MathFunctionsExtendedTest, PowerDouble) {
    std::vector<double> base = {2.0, 3.0, 4.0, 0.5, 10.0, 0.0, 1.0, -2.0};
    std::vector<double> exponent = {3.0, 2.0, 0.5, 2.0, -1.0, 5.0, 100.0, 3.0};
    std::vector<double> expected;
    for (int i = 0; i < base.size(); ++i) {
        expected.push_back(std::pow(base[i], exponent[i]));
    }
	TestBinaryMathOperation<double, OMNI_DOUBLE>("power", base, exponent, expected);
}

// Test power function with edge cases
TEST(MathFunctionsExtendedTest, PowerDoubleEdgeCases) {
    std::vector<double> base = {0.0, 1.0, std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity(), 0.0, 2.0};
    std::vector<double> exponent = {0.0, std::numeric_limits<double>::infinity(), 1.0,
                      2.0, -1.0, std::numeric_limits<double>::max()};
    std::vector<double> expected;
    for (int i = 0; i < base.size(); ++i) {
        expected.push_back(std::pow(base[i], exponent[i]));
    }
	TestBinaryMathOperation<double, OMNI_DOUBLE>("power", base, exponent, expected);
}

// Test rint function
TEST(MathFunctionsExtendedTest, RintDouble) {
    std::vector<double> inputData = {1.5, 2.5, -1.5, -2.5, 0.5, -0.5, 0.0, 3.7};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::rint(x));
    }
	TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("rint", inputData, expectedResults, 1e-6);
}


// Test rint function with edge cases
TEST(MathFunctionsExtendedTest, RintDoubleEdgeCases) {

    std::vector<double> inputData = {std::numeric_limits<double>::max(),
                      std::numeric_limits<double>::lowest(),
                      std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN(),
                      1e308};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::rint(x));
    }
	TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("rint", inputData, expectedResults, 1e-6);
}

// Test round function for double
TEST(MathFunctionsExtendedTest, RoundDouble) {
    std::vector<double> inputData = {1.5, 2.5, -1.5, -2.5, 0.5, -0.5, 0.0, 3.7};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::round(x));
    }
	TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("round", inputData, expectedResults, 1e-6);
}

// Test round function with edge cases
TEST(MathFunctionsExtendedTest, RoundDoubleEdgeCases) {
    std::vector<double> inputData = {std::numeric_limits<double>::max(),
                       std::numeric_limits<double>::lowest(),
                       std::numeric_limits<double>::infinity(),
                       -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN(),
                       1e308};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::round(x));
    }
	TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("round", inputData, expectedResults, 1e-6);
}

// round(expr, scale): explicit scale per row
TEST(MathFunctionsExtendedTest, RoundDoubleWithScale) {
    std::vector<double> leftData = {3.8636365, 1.45, -0.60265756, 11111.0, 1.0 / 3, 0.0};
    std::vector<int32_t> rightData = {2, 1, 2, -1, 2, 0};
    std::vector<double> expectedResults = {
        std::round(3.8636365 * 100) / 100,
        std::round(1.45 * 10) / 10,
        std::round(-0.60265756 * 100) / 100,
        std::round(11111.0 * 0.1) / 0.1,
        std::round((1.0 / 3) * 100) / 100,
        0.0
    };
    TestBinaryRoundOperation<double, OMNI_DOUBLE>("round", leftData, rightData, expectedResults, 1e-10);
}

// Test csc function
TEST(MathFunctionsTest, CscDouble) {
    std::vector<double> inputData = {1.0, -1.0, M_PI / 2.0, M_PI / 4.0, M_PI / 3.0, M_PI / 6.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::sin(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("csc", inputData, expectedResults);
}

// Test csc function
TEST(MathFunctionsTest, CscDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    // csc(0) = 1/sin(0) = +Infinity, csc(NaN) = NaN, csc(Inf) = NaN, csc(-Inf) = NaN
    std::vector<double> inputData = {0.0, kNanVal, kInfVal, -kInfVal};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::sin(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("csc", inputData, expectedResults);
}

// Test csc function via ExprEval path
TEST(MathFunctionsTest, CscDoubleExprEval) {
    double tolerance = 1e-6;
    std::vector<double> inputData = {1.0, -1.0, M_PI / 2.0, M_PI / 4.0, M_PI / 6.0};
    std::vector<double> expectedResults;
    std::string functionName = "csc";
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::sin(x));
    }
    int rowSize = static_cast<int>(inputData.size());
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, type)};
    auto funcExpr = new FuncExpr("csc", args, returnType);
    double col1[5] = {1.0, -1.0, M_PI / 2.0, M_PI / 4.0, M_PI / 6.0};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<double> *>(result);

    for (int32_t i = 0; i < rowSize; ++i) {
        double actualResult = resultVector->GetValue(i);
        double expectedResult = expectedResults[i];

        if (std::isnan(expectedResult)) {
            EXPECT_TRUE(std::isnan(actualResult))
                << "NaN mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else if (std::isinf(expectedResult)) {
            EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                << "Infinity mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else {
            EXPECT_NEAR(actualResult, expectedResult, tolerance)
                << "Value mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }
    delete input;
    delete funcExpr;
    delete context;
}

// Test degrees function
TEST(MathFunctionsTest, DegreesDouble) {
    std::vector<double> inputData = {0.0, 1.0, -1.0, 3.14, -3.14, M_PI, -M_PI, M_PI / 2.0, M_PI / 4.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(x * (180.0 / M_PI));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("degrees", inputData, expectedResults);
}

// Test radians function (degrees -> radians, inverse of degrees)
TEST(MathFunctionsTest, RadiansDouble) {
    std::vector<double> inputData = {0.0, 180.0, -180.0, 90.0, 360.0, 45.0, 1.0, -90.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(x * (M_PI / 180.0));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("radians", inputData, expectedResults);
}

// Test degrees function
TEST(MathFunctionsTest, DegreesDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    std::vector<double> inputData = {0.0, kNanVal, kInfVal, -kInfVal,
                                     std::numeric_limits<double>::min(),
                                     std::numeric_limits<double>::max()};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(x * (180.0 / M_PI));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("degrees", inputData, expectedResults);
}

// Test degrees function via ExprEval path
TEST(MathFunctionsTest, DegreesDoubleExprEval) {
    double tolerance = 1e-6;
    std::vector<double> inputData = {0.0, 1.0, -1.0, 3.14, -3.14, M_PI, -M_PI};
    std::vector<double> expectedResults;
    std::string functionName = "degrees";
    for (double x : inputData) {
        expectedResults.push_back(x * (180.0 / M_PI));
    }
    int rowSize = static_cast<int>(inputData.size());
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, type)};
    auto funcExpr = new FuncExpr("degrees", args, returnType);
    double col1[7] = {0.0, 1.0, -1.0, 3.14, -3.14, M_PI, -M_PI};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<double> *>(result);

    for (int32_t i = 0; i < rowSize; ++i) {
        double actualResult = resultVector->GetValue(i);
        double expectedResult = expectedResults[i];

        if (std::isnan(expectedResult)) {
            EXPECT_TRUE(std::isnan(actualResult))
                << "NaN mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else if (std::isinf(expectedResult)) {
            EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                << "Infinity mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else {
            EXPECT_NEAR(actualResult, expectedResult, tolerance)
                << "Value mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }
    delete input;
    delete funcExpr;
    delete context;
}

// Test cot function
TEST(MathFunctionsTest, CotDouble) {
    std::vector<double> inputData = {1.0, -1.0, M_PI / 4.0, M_PI / 3.0, M_PI / 6.0};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::tan(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("cot", inputData, expectedResults);
}

// Test cot function
TEST(MathFunctionsTest, CotDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    std::vector<double> inputData = {0.0, kNanVal, kInfVal, -kInfVal};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::tan(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("cot", inputData, expectedResults);
}

// Test cot function via ExprEval path
TEST(MathFunctionsTest, CotDoubleExprEval) {
    double tolerance = 1e-6;
    std::vector<double> inputData = {1.0, -1.0, M_PI / 4.0, M_PI / 3.0, M_PI / 6.0};
    std::vector<double> expectedResults;
    std::string functionName = "cot";
    for (double x : inputData) {
        expectedResults.push_back(1.0 / std::tan(x));
    }
    int rowSize = static_cast<int>(inputData.size());
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, type)};
    auto funcExpr = new FuncExpr("cot", args, returnType);
    double col1[5] = {1.0, -1.0, M_PI / 4.0, M_PI / 3.0, M_PI / 6.0};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<double> *>(result);

    for (int32_t i = 0; i < rowSize; ++i) {
        double actualResult = resultVector->GetValue(i);
        double expectedResult = expectedResults[i];

        if (std::isnan(expectedResult)) {
            EXPECT_TRUE(std::isnan(actualResult))
                << "NaN mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else if (std::isinf(expectedResult)) {
            EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                << "Infinity mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else {
            EXPECT_NEAR(actualResult, expectedResult, tolerance)
                << "Value mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }
    delete input;
    delete funcExpr;
    delete context;
}

// Test atanh function
TEST(MathFunctionsTest, AtanhDouble) {
    std::vector<double> inputData = {0.0, 0.5, -0.5, 0.9, -0.9};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::atanh(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("atanh", inputData, expectedResults);
}

// Test atanh function
TEST(MathFunctionsTest, AtanhDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    std::vector<double> inputData = {0.0, 1.0, -1.0, kNanVal, kInfVal, -kInfVal};
    std::vector<double> expectedResults = {
        0.0,
        kInfVal,
        -kInfVal,
        kNanVal,
        kNanVal,
        kNanVal
    };
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("atanh", inputData, expectedResults);
}

// Test atanh function via ExprEval path
TEST(MathFunctionsTest, AtanhDoubleExprEval) {
    double tolerance = 1e-6;
    std::vector<double> inputData = {0.0, 0.5, -0.5, 0.9, -0.9};
    std::vector<double> expectedResults;
    std::string functionName = "atanh";
    for (double x : inputData) {
        expectedResults.push_back(std::atanh(x));
    }
    int rowSize = static_cast<int>(inputData.size());
    auto returnType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, type)};
    auto funcExpr = new FuncExpr("atanh", args, returnType);
    double col1[5] = {0.0, 0.5, -0.5, 0.9, -0.9};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<double> *>(result);

    for (int32_t i = 0; i < rowSize; ++i) {
        double actualResult = resultVector->GetValue(i);
        double expectedResult = expectedResults[i];

        if (std::isnan(expectedResult)) {
            EXPECT_TRUE(std::isnan(actualResult))
                << "NaN mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else if (std::isinf(expectedResult)) {
            EXPECT_TRUE(std::isinf(actualResult) && std::signbit(actualResult) == std::signbit(expectedResult))
                << "Infinity mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i];
        } else {
            EXPECT_NEAR(actualResult, expectedResult, tolerance)
                << "Value mismatch at index " << i << " for " << functionName
                << " with input=" << inputData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }
    delete input;
    delete funcExpr;
    delete context;
}

// Test exp function - Returns e raised to the power of the input
TEST(MathFunctionsExtendedTest, ExpDouble) {
    const double kE = std::exp(1.0);  // Euler's number e ≈ 2.71828
    std::vector<double> inputData = {0.0, 1.0, -1.0, 2.0, -2.0, 0.5};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::exp(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("exp", inputData, expectedResults, 1e-10);
}

// Test exp function with edge cases (NaN, Infinity, large values)
TEST(MathFunctionsExtendedTest, ExpDoubleEdgeCases) {
    constexpr double kInfVal = std::numeric_limits<double>::infinity();
    constexpr double kNanVal = std::numeric_limits<double>::quiet_NaN();

    // Test cases: 0, NaN, infinity, -infinity, large positive (may overflow), large negative
    std::vector<double> inputData = {0.0, kNanVal, kInfVal, -kInfVal, 100.0, -100.0};
    // exp(0) = 1, exp(NaN) = NaN, exp(inf) = inf, exp(-inf) = 0, exp(100) ≈ 2.688e43, exp(-100) ≈ 0
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::exp(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("exp", inputData, expectedResults, 1e-6);
}


// Test expm1 function - returns e^x - 1
TEST(MathFunctionsTest, Expm1Double) {
    const double kE = std::exp(1.0);
    std::vector<double> inputData = {0.0, 1.0, -1.0, 0.5, 2.0, -0.5};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::expm1(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("expm1", inputData, expectedResults);
}

// Test expm1 function with edge cases (NaN, Infinity, small numbers for precision)
TEST(MathFunctionsTest, Expm1DoubleEdgeCases) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
    const double kE = std::exp(1.0);

    // Test cases covering edge cases:
    // - NaN input should return NaN
    // - Positive infinity should return positive infinity
    // - Negative infinity should return -1
    // - Zero should return 0
    // - 1 should return e-1
    // - Small numbers (1e-12) to test precision advantage over exp(x)-1
    std::vector<double> inputData = {0.0, 1.0, kNan, kInf, -kInf, 1e-12};
    std::vector<double> expectedResults;
    for (double x : inputData) {
        expectedResults.push_back(std::expm1(x));
    }
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("expm1", inputData, expectedResults);
}

// ============================================================================
// div (Integral Division) Tests
// ============================================================================

// Helper function to test binary operations with int64_t return type for integral division
template <typename TInput, DataTypeId inputTypeId>
void TestIntegralDivideOperation(
        const std::string& functionName,
        const std::vector<TInput>& leftData,
        const std::vector<TInput>& rightData,
        const std::vector<int64_t>& expectedResults,
        const std::vector<bool>& expectedNulls = {}) {

    int32_t rowSize = static_cast<int32_t>(leftData.size());

    // Create left vector
    BaseVector* leftVec = VectorHelper::CreateFlatVector(inputTypeId, rowSize);
    auto* leftVector = static_cast<Vector<TInput>*>(leftVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        leftVector->SetValue(i, leftData[i]);
        leftVector->SetNotNull(i);
    }

    // Create right vector
    BaseVector* rightVec = VectorHelper::CreateFlatVector(inputTypeId, rowSize);
    auto* rightVector = static_cast<Vector<TInput>*>(rightVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        rightVector->SetValue(i, rightData[i]);
        rightVector->SetNotNull(i);
    }

    // Create function signature: div(inputType, inputType) -> LONG
    std::vector<DataTypeId> argTypes = {inputTypeId, inputTypeId};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_LONG);

    // Find vector function
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName << " not found for type " << static_cast<int>(inputTypeId);

    // Create execution context
    ExecutionContext context;
    context.SetResultRowSize(rowSize);

    // Prepare arguments stack
    std::stack<BaseVector*> args;
    args.push(leftVec);
    args.push(rightVec);

    // Execute function
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, result, &context);

    // Verify results
    auto* resultVector = static_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        if (!expectedNulls.empty() && expectedNulls[i]) {
            EXPECT_TRUE(result->IsNull(i))
                << "Expected NULL at index " << i << " for " << functionName
                << " with left=" << leftData[i] << ", right=" << rightData[i];
        } else {
            int64_t actualResult = resultVector->GetValue(i);
            int64_t expectedResult = expectedResults[i];
            EXPECT_EQ(actualResult, expectedResult)
                << "Value mismatch at index " << i << " for " << functionName
                << " with left=" << leftData[i] << ", right=" << rightData[i]
                << ", expected=" << expectedResult << ", actual=" << actualResult;
        }
    }

    // Cleanup
    delete result;
}

// Test div function for LONG type - basic cases
TEST(MathFunctionsTest, DivLongBasic) {
    std::vector<int64_t> leftData = {10, 20, 15, -10, 100, 7};
    std::vector<int64_t> rightData = {3, 4, 5, 3, 7, 2};
    std::vector<int64_t> expected = {3, 5, 3, -3, 14, 3};  // Integer division truncates toward zero
    TestIntegralDivideOperation<int64_t, OMNI_LONG>("div", leftData, rightData, expected);
}

// Test div function for LONG type - negative numbers
TEST(MathFunctionsTest, DivLongNegative) {
    std::vector<int64_t> leftData = {-10, 10, -10, 10};
    std::vector<int64_t> rightData = {3, -3, -3, 3};
    // Integer division truncates toward zero:
    // -10 / 3 = -3 (not -4)
    // 10 / -3 = -3 (not -4)
    // -10 / -3 = 3
    // 10 / 3 = 3
    std::vector<int64_t> expected = {-3, -3, 3, 3};
    TestIntegralDivideOperation<int64_t, OMNI_LONG>("div", leftData, rightData, expected);
}

// Test div function for LONG type - edge case: Long.MIN_VALUE / -1
TEST(MathFunctionsTest, DivLongMinValueEdgeCase) {
    constexpr int64_t longMin = std::numeric_limits<int64_t>::min();
    constexpr int64_t longMax = std::numeric_limits<int64_t>::max();

    std::vector<int64_t> leftData = {longMin, longMax, longMin};
    std::vector<int64_t> rightData = {-1, 1, 1};
    // Long.MIN_VALUE / -1 returns Long.MIN_VALUE (Java semantics - overflow wraps around)
    // Long.MAX_VALUE / 1 = Long.MAX_VALUE
    // Long.MIN_VALUE / 1 = Long.MIN_VALUE
    std::vector<int64_t> expected = {longMin, longMax, longMin};
    TestIntegralDivideOperation<int64_t, OMNI_LONG>("div", leftData, rightData, expected);
}

// Test div function for LONG type - division by zero returns NULL
TEST(MathFunctionsTest, DivLongDivisionByZero) {
    int rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};
    auto funcExpr = new FuncExpr("div", args, returnType);

    int64_t col1[4] = {10, 20, 30, 40};
    int64_t col2[4] = {2, 0, 5, 0};  // Contains zeros

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    std::cout << "=== div with zero divisor input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== div with zero divisor Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);

    // Row 0: 10 / 2 = 5
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(resultVector->GetValue(0), 5);

    // Row 1: 20 / 0 = NULL
    EXPECT_TRUE(result->IsNull(1)) << "Division by zero should return NULL";

    // Row 2: 30 / 5 = 6
    EXPECT_FALSE(result->IsNull(2));
    EXPECT_EQ(resultVector->GetValue(2), 6);

    // Row 3: 40 / 0 = NULL
    EXPECT_TRUE(result->IsNull(3)) << "Division by zero should return NULL";

    delete input;
    delete funcExpr;
    delete context;
}

// Test div function for DECIMAL64 type - basic cases
TEST(MathFunctionsTest, DivDecimal64Basic) {
    // DECIMAL64 is represented as int64_t internally
    std::vector<int64_t> leftData = {1000, 2500, 1500, -1000, 10000};
    std::vector<int64_t> rightData = {300, 400, 500, 300, 700};
    // Integer division: 1000/300=3, 2500/400=6, 1500/500=3, -1000/300=-3, 10000/700=14
    std::vector<int64_t> expected = {3, 6, 3, -3, 14};
    TestIntegralDivideOperation<int64_t, OMNI_DECIMAL64>("div", leftData, rightData, expected);
}

// Test div function for DECIMAL64 type - large values
TEST(MathFunctionsTest, DivDecimal64LargeValues) {
    std::vector<int64_t> leftData = {999999999999999999LL, 123456789012345678LL, 100000000000000000LL};
    std::vector<int64_t> rightData = {1000000000LL, 1000000000LL, 100000000LL};
    std::vector<int64_t> expected = {999999999LL, 123456789LL, 1000000000LL};
    TestIntegralDivideOperation<int64_t, OMNI_DECIMAL64>("div", leftData, rightData, expected);
}

// Test div function for DECIMAL128 type - basic cases
TEST(MathFunctionsTest, DivDecimal128Basic) {
    std::vector<Decimal128> leftData = {
        Decimal128("1000"),
        Decimal128("2500"),
        Decimal128("1500"),
        Decimal128("-1000"),
        Decimal128("10000")
    };
    std::vector<Decimal128> rightData = {
        Decimal128("300"),
        Decimal128("400"),
        Decimal128("500"),
        Decimal128("300"),
        Decimal128("700")
    };
    // Integer division
    std::vector<int64_t> expected = {3, 6, 3, -3, 14};
    TestIntegralDivideOperation<Decimal128, OMNI_DECIMAL128>("div", leftData, rightData, expected);
}

// Test div function for DECIMAL128 type - negative numbers
TEST(MathFunctionsTest, DivDecimal128Negative) {
    std::vector<Decimal128> leftData = {
        Decimal128("-100"),
        Decimal128("100"),
        Decimal128("-100"),
        Decimal128("100")
    };
    std::vector<Decimal128> rightData = {
        Decimal128("30"),
        Decimal128("-30"),
        Decimal128("-30"),
        Decimal128("30")
    };
    // Integer division truncates toward zero
    std::vector<int64_t> expected = {-3, -3, 3, 3};
    TestIntegralDivideOperation<Decimal128, OMNI_DECIMAL128>("div", leftData, rightData, expected);
}

// Test div function for DECIMAL128 type - large values (within int64_t range)
TEST(MathFunctionsTest, DivDecimal128LargeValues) {
    std::vector<Decimal128> leftData = {
        Decimal128("9223372036854775807"),  // Long.MAX_VALUE
        Decimal128("12345678901234567890"),
        Decimal128("10000000000000000000")
    };
    std::vector<Decimal128> rightData = {
        Decimal128("10"),
        Decimal128("10"),
        Decimal128("10")
    };
    // Integer division results that fit in int64_t
    // 9223372036854775807 / 10 = 922337203685477580
    // 12345678901234567890 / 10 = 1234567890123456789
    // 10000000000000000000 / 10 = 1000000000000000000
    std::vector<int64_t> expected = {
        922337203685477580LL,
        1234567890123456789LL,
        1000000000000000000LL
    };
    TestIntegralDivideOperation<Decimal128, OMNI_DECIMAL128>("div", leftData, rightData, expected);
}

// Test div function with NULL values
TEST(MathFunctionsTest, DivWithNullValues) {
    int rowSize = 3;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};
    auto funcExpr = new FuncExpr("div", args, returnType);

    int64_t col1[3] = {100, 200, 300};
    int64_t col2[3] = {10, 20, 30};

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    // Set first value of col1 to NULL
    input->Get(0)->SetNull(0);

    std::cout << "=== div with NULL input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== div with NULL Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    // First row should be NULL (input is NULL)
    EXPECT_TRUE(result->IsNull(0)) << "Result should be NULL when input is NULL";
    // Second and third rows should have valid results
    EXPECT_FALSE(result->IsNull(1)) << "Result should not be NULL for valid inputs";
    EXPECT_FALSE(result->IsNull(2)) << "Result should not be NULL for valid inputs";

    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    EXPECT_EQ(resultVector->GetValue(1), 10);  // 200 / 20 = 10
    EXPECT_EQ(resultVector->GetValue(2), 10);  // 300 / 30 = 10

    delete input;
    delete funcExpr;
    delete context;
}

// ============================================================================
// truncate(expr) / truncate(expr, scale) — Flink TRUNCATE(numeric[, integer]).
// Truncates toward zero (RoundingMode.DOWN). Single-arg uses scale = 0.
// Registered for byte/short/int/long/float/double.
// Integral scale >= 0: unchanged; negative scale zeros digits left of decimal point
// (e.g. truncate(12345, -1) -> 12340).
// ============================================================================

// --- Single argument (scale defaults to 0) ---

// truncate(double): drop the fractional part toward zero.
TEST(MathFunctionsTest, TruncateDoubleNoScale) {
    std::vector<double> input =    {3.78, -3.78, 3.2, -3.9, 0.0, 5.0, -0.5};
    std::vector<double> expected = {3.0,  -3.0,  3.0, -3.0, 0.0, 5.0,  0.0};
    TestUnaryMathOperation<double, OMNI_DOUBLE, OMNI_DOUBLE>("truncate", input, expected);
}

// truncate(float): drop the fractional part toward zero.
TEST(MathFunctionsTest, TruncateFloatNoScale) {
    std::vector<float> input =    {2.9f, -2.9f, 7.99f, -0.5f, 0.0f};
    std::vector<float> expected = {2.0f, -2.0f, 7.0f,   0.0f, 0.0f};
    TestUnaryMathOperation<float, OMNI_FLOAT, OMNI_FLOAT>("truncate", input, expected);
}

// truncate(int): integral values are returned unchanged.
TEST(MathFunctionsTest, TruncateIntNoScale) {
    std::vector<int32_t> input =    {5, -7, 0, 100, -2147483648};
    std::vector<int32_t> expected = {5, -7, 0, 100, -2147483648};
    TestUnaryMathOperation<int32_t, OMNI_INT, OMNI_INT>("truncate", input, expected);
}

// truncate(bigint): integral values are returned unchanged.
TEST(MathFunctionsTest, TruncateLongNoScale) {
    std::vector<int64_t> input =    {123456789012LL, -99LL, 0LL};
    std::vector<int64_t> expected = {123456789012LL, -99LL, 0LL};
    TestUnaryMathOperation<int64_t, OMNI_LONG, OMNI_LONG>("truncate", input, expected);
}

// truncate(tinyint): integral values are returned unchanged.
TEST(MathFunctionsTest, TruncateByteNoScale) {
    std::vector<int8_t> input =    {static_cast<int8_t>(5), static_cast<int8_t>(-7), static_cast<int8_t>(127)};
    std::vector<int8_t> expected = {static_cast<int8_t>(5), static_cast<int8_t>(-7), static_cast<int8_t>(127)};
    TestUnaryMathOperation<int8_t, OMNI_BYTE, OMNI_BYTE>("truncate", input, expected);
}

// truncate(smallint): integral values are returned unchanged.
TEST(MathFunctionsTest, TruncateShortNoScale) {
    std::vector<int16_t> input =    {static_cast<int16_t>(300), static_cast<int16_t>(-42), static_cast<int16_t>(0)};
    std::vector<int16_t> expected = {static_cast<int16_t>(300), static_cast<int16_t>(-42), static_cast<int16_t>(0)};
    TestUnaryMathOperation<int16_t, OMNI_SHORT, OMNI_SHORT>("truncate", input, expected);
}

// --- Two arguments (expr, scale) ---

// truncate(double, scale): positive, zero and negative scales.
TEST(MathFunctionsTest, TruncateDoubleWithScale) {
    std::vector<double> left =     {3.14159, 3.14159, 3.14159, -3.14159, 1234.56, 987.65};
    std::vector<int32_t> scale =   {2,       1,       0,       2,        -2,      -1};
    std::vector<double> expected = {3.14,    3.1,     3.0,     -3.14,    1200.0,  980.0};
    TestBinaryRoundOperation<double, OMNI_DOUBLE>("truncate", left, scale, expected);
}

// truncate(float, scale): positive scales (tolerance loosened for float precision).
TEST(MathFunctionsTest, TruncateFloatWithScale) {
    std::vector<float> left =     {3.14159f, -3.14159f, 12.345f};
    std::vector<int32_t> scale =  {2,        2,         1};
    std::vector<float> expected = {3.14f,    -3.14f,    12.3f};
    TestBinaryRoundOperation<float, OMNI_FLOAT>("truncate", left, scale, expected, 1e-4);
}

// truncate(int, scale): scale>=0 unchanged; negative scale zeros left digits.
TEST(MathFunctionsTest, TruncateIntWithScale) {
    std::vector<int32_t> left =     {12345, 12345, -678,  -12345, 42};
    std::vector<int32_t> scale =    {2,     0,     3,     -1,     -1};
    std::vector<int32_t> expected = {12345, 12345, -678,  -12340, 40};
    TestBinaryRoundOperation<int32_t, OMNI_INT>("truncate", left, scale, expected);
}

// truncate(bigint, scale): scale>=0 unchanged; negative scale zeros left digits.
TEST(MathFunctionsTest, TruncateLongWithScale) {
    std::vector<int64_t> left =     {98765LL, -1LL, 0LL, 123456789012LL, -12345LL};
    std::vector<int32_t> scale =    {1,       0,    5,   -2,             -1};
    std::vector<int64_t> expected = {98765LL, -1LL, 0LL, 123456789000LL, -12340LL};
    TestBinaryRoundOperation<int64_t, OMNI_LONG>("truncate", left, scale, expected);
}

// truncate(tinyint, negative scale): zeros digits left of decimal point.
TEST(MathFunctionsTest, TruncateByteWithNegativeScale) {
    std::vector<int8_t> left =     {static_cast<int8_t>(127), static_cast<int8_t>(-57)};
    std::vector<int32_t> scale =   {-1, -1};
    std::vector<int8_t> expected = {static_cast<int8_t>(120), static_cast<int8_t>(-50)};
    TestBinaryRoundOperation<int8_t, OMNI_BYTE>("truncate", left, scale, expected);
}

// truncate(smallint, negative scale): zeros digits left of decimal point.
TEST(MathFunctionsTest, TruncateShortWithNegativeScale) {
    std::vector<int16_t> left =     {static_cast<int16_t>(1234), static_cast<int16_t>(-1234)};
    std::vector<int32_t> scale =    {-2, -2};
    std::vector<int16_t> expected = {static_cast<int16_t>(1200), static_cast<int16_t>(-1200)};
    TestBinaryRoundOperation<int16_t, OMNI_SHORT>("truncate", left, scale, expected);
}