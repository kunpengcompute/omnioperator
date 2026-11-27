/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for width_bucket function
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <optional>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/MathFunctions.h"
#include "expression/expressions.h"
#include "type/data_type.h"
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

class WidthBucketTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper function to test width_bucket function
// width_bucket(value, bound1, bound2, numBuckets) -> bigint
void TestWidthBucketOperation(
        const std::string& functionName,
        const std::vector<double>& valueData,
        const std::vector<double>& bound1Data,
        const std::vector<double>& bound2Data,
        const std::vector<int64_t>& numBucketsData,
        const std::vector<std::optional<int64_t>>& expectedResults) {
    
    int32_t rowSize = static_cast<int32_t>(valueData.size());
    
    // Create value vector (double)
    BaseVector* valueVec = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    auto* valueVector = static_cast<Vector<double>*>(valueVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        valueVector->SetValue(i, valueData[i]);
        valueVector->SetNotNull(i);
    }
    
    // Create bound1 vector (double)
    BaseVector* bound1Vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    auto* bound1Vector = static_cast<Vector<double>*>(bound1Vec);
    for (int32_t i = 0; i < rowSize; ++i) {
        bound1Vector->SetValue(i, bound1Data[i]);
        bound1Vector->SetNotNull(i);
    }
    
    // Create bound2 vector (double)
    BaseVector* bound2Vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    auto* bound2Vector = static_cast<Vector<double>*>(bound2Vec);
    for (int32_t i = 0; i < rowSize; ++i) {
        bound2Vector->SetValue(i, bound2Data[i]);
        bound2Vector->SetNotNull(i);
    }
    
    // Create numBuckets vector (int64_t)
    BaseVector* numBucketsVec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    auto* numBucketsVector = static_cast<Vector<int64_t>*>(numBucketsVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        numBucketsVector->SetValue(i, numBucketsData[i]);
        numBucketsVector->SetNotNull(i);
    }
    
    // Create function signature: width_bucket(double, double, double, long) -> long
    std::vector<DataTypeId> argTypes = {OMNI_DOUBLE, OMNI_DOUBLE, OMNI_DOUBLE, OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_LONG);
    
    // Find vector function
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName << " not found";
    
    // Create execution context
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    
    // Prepare arguments stack (pushed in reverse order for stack)
    std::stack<BaseVector*> args;
    args.push(valueVec);
    args.push(bound1Vec);
    args.push(bound2Vec);
    args.push(numBucketsVec);
    
    // Execute function
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, result, &context);
    
    // Verify results
    ASSERT_NE(result, nullptr);
    auto* resultVector = static_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVector, nullptr);
    
    for (int32_t i = 0; i < rowSize; ++i) {
        if (!expectedResults[i].has_value()) {
            // Expect NULL result
            EXPECT_TRUE(resultVector->IsNull(i))
                << "Expected NULL at index " << i << " for " << functionName
                << " with value=" << valueData[i] << ", bound1=" << bound1Data[i]
                << ", bound2=" << bound2Data[i] << ", numBuckets=" << numBucketsData[i];
        } else {
            // Expect non-NULL result
            EXPECT_FALSE(resultVector->IsNull(i))
                << "Unexpected NULL at index " << i << " for " << functionName;
            if (!resultVector->IsNull(i)) {
                int64_t actual = resultVector->GetValue(i);
                int64_t expected = expectedResults[i].value();
                EXPECT_EQ(actual, expected)
                    << "Value mismatch at index " << i << " for " << functionName
                    << " with value=" << valueData[i] << ", bound1=" << bound1Data[i]
                    << ", bound2=" << bound2Data[i] << ", numBuckets=" << numBucketsData[i]
                    << ", expected=" << expected << ", actual=" << actual;
            }
        }
    }
    
    // Cleanup
    delete result;
}

// Test width_bucket function - basic cases with bound1 < bound2 (ascending range)
TEST_F(WidthBucketTest, BasicAscendingRange) {
    std::cout << "=== Testing width_bucket basic ascending range ===" << std::endl;
    
    // Test cases from Velox: min < max
    // width_bucket(3.14, 0, 4, 3) = 3
    // width_bucket(2, 0, 4, 3) = 2
    // width_bucket(-1, 0, 3.2, 4) = 0  (value < bound1)
    std::vector<double> valueData = {3.14, 2.0, -1.0, 0.0, 4.0, 2.0};
    std::vector<double> bound1Data = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::vector<double> bound2Data = {4.0, 4.0, 3.2, 4.0, 4.0, 4.0};
    std::vector<int64_t> numBucketsData = {3, 3, 4, 3, 3, 3};
    std::vector<std::optional<int64_t>> expectedResults = {3, 2, 0, 1, 4, 2};
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}

// Test width_bucket function - cases with bound1 > bound2 (descending range)
TEST_F(WidthBucketTest, BasicDescendingRange) {
    std::cout << "=== Testing width_bucket basic descending range ===" << std::endl;
    
    // Test cases from Velox: min > max
    // width_bucket(3.14, 4, 0, 3) = 1
    // width_bucket(2, 4, 0, 3) = 2
    // width_bucket(-1, 3.2, 0, 4) = 5  (value <= bound2)
    std::vector<double> valueData = {3.14, 2.0, -1.0};
    std::vector<double> bound1Data = {4.0, 4.0, 3.2};
    std::vector<double> bound2Data = {0.0, 0.0, 0.0};
    std::vector<int64_t> numBucketsData = {3, 3, 4};
    std::vector<std::optional<int64_t>> expectedResults = {1, 2, 5};
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}

// Test width_bucket function - infinity values
TEST_F(WidthBucketTest, InfinityValues) {
    std::cout << "=== Testing width_bucket with infinity values ===" << std::endl;
    
    constexpr double kInf = std::numeric_limits<double>::infinity();
    
    // width_bucket(kInf, 0, 4, 3) = 4 (numBuckets + 1, since value >= bound2)
    // width_bucket(-kInf, 0, 4, 3) = 0 (since value < bound1)
    // width_bucket(kInf, 4, 0, 3) = 0 (since value > bound1 when bound1 > bound2)
    std::vector<double> valueData = {kInf, -kInf, kInf};
    std::vector<double> bound1Data = {0.0, 0.0, 4.0};
    std::vector<double> bound2Data = {4.0, 4.0, 0.0};
    std::vector<int64_t> numBucketsData = {3, 3, 3};
    std::vector<std::optional<int64_t>> expectedResults = {4, 0, 0};
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}

// Test width_bucket function - NULL results for invalid inputs
TEST_F(WidthBucketTest, NullResultsForInvalidInputs) {
    std::cout << "=== Testing width_bucket with invalid inputs (should return NULL) ===" << std::endl;
    
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr int64_t kMaxInt64 = std::numeric_limits<int64_t>::max();
    
    // Test cases that should return NULL:
    // - numBuckets = 0
    // - value is NaN
    // - bound1 is NaN
    // - bound1 is infinite
    // - bound2 is NaN
    // - bound2 is infinite
    // - bound1 == bound2
    // - numBuckets == Long.MaxValue
    std::vector<double> valueData = {3.14, kNan, 3.14, 3.14, 3.14, 3.14, 3.14, 3.14};
    std::vector<double> bound1Data = {0.0, 0.0, kNan, kInf, 0.0, 0.0, 0.0, 0.0};
    std::vector<double> bound2Data = {4.0, 4.0, 0.0, 0.0, kNan, kInf, 0.0, 10.0};
    std::vector<int64_t> numBucketsData = {0, 10, 10, 10, 10, 10, 10, kMaxInt64};
    std::vector<std::optional<int64_t>> expectedResults = {
        std::nullopt,  // numBuckets = 0
        std::nullopt,  // value is NaN
        std::nullopt,  // bound1 is NaN
        std::nullopt,  // bound1 is infinite
        std::nullopt,  // bound2 is NaN
        std::nullopt,  // bound2 is infinite
        std::nullopt,  // bound1 == bound2
        std::nullopt   // numBuckets == Long.MaxValue
    };
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}

// Test width_bucket function - large range values
TEST_F(WidthBucketTest, LargeRangeValues) {
    std::cout << "=== Testing width_bucket with large range values ===" << std::endl;
    
    // max - min + 1 > Long.MaxValue case
    // width_bucket(5.3, 0, 9223372036854775807, 10) = 1
    std::vector<double> valueData = {5.3};
    std::vector<double> bound1Data = {0.0};
    std::vector<double> bound2Data = {9223372036854775807.0};
    std::vector<int64_t> numBucketsData = {10};
    std::vector<std::optional<int64_t>> expectedResults = {1};
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}

// Test width_bucket function using ExprEval integration
TEST_F(WidthBucketTest, ExprEvalIntegration) {
    std::cout << "=== Testing width_bucket with ExprEval integration ===" << std::endl;
    
    int rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    
    // Create function expression: width_bucket(col0, col1, col2, col3)
    std::vector<Expr*> args = {
        new FieldExpr(0, doubleType),   // value
        new FieldExpr(1, doubleType),   // bound1
        new FieldExpr(2, doubleType),   // bound2
        new FieldExpr(3, longType)      // numBuckets
    };
    auto funcExpr = new FuncExpr("width_bucket", args, returnType);
    
    // Test data
    double col0[4] = {3.14, 2.0, -1.0, 10.0};   // value
    double col1[4] = {0.0, 0.0, 0.0, 0.0};      // bound1
    double col2[4] = {4.0, 4.0, 3.2, 4.0};      // bound2
    int64_t col3[4] = {3, 3, 4, 3};             // numBuckets
    
    // Expected results:
    // width_bucket(3.14, 0, 4, 3) = 3
    // width_bucket(2.0, 0, 4, 3) = 2
    // width_bucket(-1.0, 0, 3.2, 4) = 0
    // width_bucket(10.0, 0, 4, 3) = 4 (numBuckets + 1)
    std::vector<int64_t> expectedResults = {3, 2, 0, 4};
    
    // Create input VectorBatch
    std::vector<type::DataTypePtr> vecOfTypes = {DoubleType(), DoubleType(), DoubleType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col0, col1, col2, col3);
    
    std::cout << "=== width_bucket input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);
    
    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== width_bucket Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);
    
    auto *resultVector = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVector, nullptr);
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Unexpected NULL at index " << i;
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            int64_t expected = expectedResults[i];
            EXPECT_EQ(actual, expected)
                << "Value mismatch at index " << i
                << ", expected=" << expected << ", actual=" << actual;
        }
    }
    
    delete input;
    delete funcExpr;
    delete context;
}

// Test width_bucket function - boundary cases
TEST_F(WidthBucketTest, BoundaryCases) {
    std::cout << "=== Testing width_bucket boundary cases ===" << std::endl;
    
    // Test exact boundary values
    // Formula: bucket = floor(numBuckets * (value - bound1) / (bound2 - bound1)) + 1
    // Each bucket width = (10-0)/5 = 2
    // width_bucket(0.0, 0.0, 10.0, 5) = floor(5*0/10)+1 = 1 (at lower bound, in first bucket)
    // width_bucket(10.0, 0.0, 10.0, 5) = 6 (value >= bound2, out of range)
    // width_bucket(2.0, 0.0, 10.0, 5) = floor(5*2/10)+1 = floor(1)+1 = 2 (in second bucket)
    // width_bucket(4.0, 0.0, 10.0, 5) = floor(5*4/10)+1 = floor(2)+1 = 3 (in third bucket)
    // width_bucket(6.0, 0.0, 10.0, 5) = floor(5*6/10)+1 = floor(3)+1 = 4 (in fourth bucket)
    // width_bucket(8.0, 0.0, 10.0, 5) = floor(5*8/10)+1 = floor(4)+1 = 5 (in fifth bucket)
    // width_bucket(9.9, 0.0, 10.0, 5) = floor(5*9.9/10)+1 = floor(4.95)+1 = 5 (in fifth bucket)
    std::vector<double> valueData = {0.0, 10.0, 2.0, 4.0, 6.0, 8.0, 9.9};
    std::vector<double> bound1Data = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::vector<double> bound2Data = {10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0};
    std::vector<int64_t> numBucketsData = {5, 5, 5, 5, 5, 5, 5};
    std::vector<std::optional<int64_t>> expectedResults = {1, 6, 2, 3, 4, 5, 5};
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}

// Test width_bucket function - negative bucket values (should return NULL)
TEST_F(WidthBucketTest, NegativeNumBuckets) {
    std::cout << "=== Testing width_bucket with negative numBuckets (should return NULL) ===" << std::endl;
    
    std::vector<double> valueData = {3.14, 2.0};
    std::vector<double> bound1Data = {0.0, 0.0};
    std::vector<double> bound2Data = {4.0, 4.0};
    std::vector<int64_t> numBucketsData = {-1, -100};
    std::vector<std::optional<int64_t>> expectedResults = {std::nullopt, std::nullopt};
    
    TestWidthBucketOperation("width_bucket", valueData, bound1Data, bound2Data, 
                             numBucketsData, expectedResults);
}
