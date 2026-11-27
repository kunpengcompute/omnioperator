/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for spark_partition_id function
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/Misc.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "codegen/func_registry.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::codegen;
using namespace omniruntime::type;

class SparkPartitionIdTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper function to test spark_partition_id function with a specific partition ID
void TestSparkPartitionIdOperation(
        const std::string& functionName,
        int32_t partitionId,
        int32_t rowSize) {
    
    std::cout << "[DEBUG] Testing spark_partition_id with partitionId=" << partitionId 
              << ", rowSize=" << rowSize << std::endl;
    
    // Create function signature: spark_partition_id() -> int32_t
    std::vector<DataTypeId> argTypes = {};  // No arguments
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_INT);
    
    // Find simple function factory
    auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
    ASSERT_NE(factoryIt, VectorFunction::simpleFunctionFactoryMap_.end()) 
        << "Function factory " << functionName << " not found";
    
    // Create QueryConfig with partition ID
    std::unordered_map<std::string, std::string> configValues;
    configValues["spark.partition_id"] = std::to_string(partitionId);
    config::QueryConfig queryConfig(configValues);
    
    // Create vector function with config
    std::vector<BaseVector*> constantInputs;  // No constant inputs
    auto factory = factoryIt->second();
    auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName << " creation failed";
    
    // Create execution context
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    
    // Prepare empty arguments stack (no input arguments)
    std::stack<BaseVector*> args;
    
    // Execute function
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, result, &context);
    
    // Verify results
    ASSERT_NE(result, nullptr) << "Result is null";
    auto* resultVector = static_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "Unexpected NULL at index " << i << " for " << functionName;
        if (!resultVector->IsNull(i)) {
            int32_t actual = resultVector->GetValue(i);
            EXPECT_EQ(actual, partitionId)
                << "Value mismatch at index " << i << " for " << functionName
                << ", expected partitionId=" << partitionId << ", actual=" << actual;
        }
    }
    
    // Cleanup
    delete result;
}

// Test spark_partition_id function - basic cases
TEST_F(SparkPartitionIdTest, BasicPartitionId) {
    std::cout << "=== Testing spark_partition_id basic cases ===" << std::endl;
    
    // Test with partition ID = 0
    TestSparkPartitionIdOperation("spark_partition_id", 0, 1);
    
    // Test with partition ID = 100
    TestSparkPartitionIdOperation("spark_partition_id", 100, 1);
    
    // Test with partition ID = 0, larger vector
    TestSparkPartitionIdOperation("spark_partition_id", 0, 100);
    
    // Test with partition ID = 100, larger vector
    TestSparkPartitionIdOperation("spark_partition_id", 100, 100);
}

// Test spark_partition_id function - various partition IDs
TEST_F(SparkPartitionIdTest, VariousPartitionIds) {
    std::cout << "=== Testing spark_partition_id with various partition IDs ===" << std::endl;
    
    // Test with small partition IDs
    TestSparkPartitionIdOperation("spark_partition_id", 1, 10);
    TestSparkPartitionIdOperation("spark_partition_id", 5, 10);
    TestSparkPartitionIdOperation("spark_partition_id", 10, 10);
    
    // Test with larger partition IDs
    TestSparkPartitionIdOperation("spark_partition_id", 1000, 10);
    TestSparkPartitionIdOperation("spark_partition_id", 9999, 10);
}

// Test spark_partition_id function - default partition ID (0)
TEST_F(SparkPartitionIdTest, DefaultPartitionId) {
    std::cout << "=== Testing spark_partition_id with default partition ID ===" << std::endl;
    
    // When no partition ID is set in config, default is 0
    // Create function signature: spark_partition_id() -> int32_t
    std::vector<DataTypeId> argTypes = {};
    auto signature = std::make_shared<FunctionSignature>("spark_partition_id", argTypes, OMNI_INT);
    
    // Find simple function factory
    auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
    ASSERT_NE(factoryIt, VectorFunction::simpleFunctionFactoryMap_.end()) 
        << "Function factory spark_partition_id not found";
    
    // Create QueryConfig without partition ID (use default)
    std::unordered_map<std::string, std::string> emptyConfigValues;
    config::QueryConfig queryConfig(emptyConfigValues);
    
    // Create vector function with config
    std::vector<BaseVector*> constantInputs;
    auto factory = factoryIt->second();
    auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);
    
    // Create execution context
    int32_t rowSize = 5;
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    
    // Execute function
    std::stack<BaseVector*> args;
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, result, &context);
    
    // Verify results - default partition ID should be 0
    ASSERT_NE(result, nullptr);
    auto* resultVector = static_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVector, nullptr);
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i));
        if (!resultVector->IsNull(i)) {
            int32_t actual = resultVector->GetValue(i);
            EXPECT_EQ(actual, 0) << "Default partition ID should be 0";
        }
    }
    
    delete result;
}

// Test spark_partition_id function - constant result across all rows
TEST_F(SparkPartitionIdTest, ConstantResultAcrossRows) {
    std::cout << "=== Testing spark_partition_id constant result across all rows ===" << std::endl;
    
    int32_t partitionId = 42;
    int32_t rowSize = 1000;
    
    // Create function signature: spark_partition_id() -> int32_t
    std::vector<DataTypeId> argTypes = {};
    auto signature = std::make_shared<FunctionSignature>("spark_partition_id", argTypes, OMNI_INT);
    
    // Find simple function factory
    auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
    ASSERT_NE(factoryIt, VectorFunction::simpleFunctionFactoryMap_.end());
    
    // Create QueryConfig with partition ID
    std::unordered_map<std::string, std::string> configValues;
    configValues["spark.partition_id"] = std::to_string(partitionId);
    config::QueryConfig queryConfig(configValues);
    
    // Create vector function with config
    std::vector<BaseVector*> constantInputs;
    auto factory = factoryIt->second();
    auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
    ASSERT_NE(vectorFunction, nullptr);
    
    // Create execution context
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    
    // Execute function
    std::stack<BaseVector*> args;
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, result, &context);
    
    // Verify ALL rows have the same partition ID
    ASSERT_NE(result, nullptr);
    auto* resultVector = static_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVector, nullptr);
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i));
        if (!resultVector->IsNull(i)) {
            int32_t actual = resultVector->GetValue(i);
            EXPECT_EQ(actual, partitionId) 
                << "All rows should have the same partition ID, but row " << i 
                << " has " << actual << " instead of " << partitionId;
        }
    }
    
    delete result;
}

// Test spark_partition_id function - single row
TEST_F(SparkPartitionIdTest, SingleRow) {
    std::cout << "=== Testing spark_partition_id with single row ===" << std::endl;
    
    TestSparkPartitionIdOperation("spark_partition_id", 7, 1);
}
