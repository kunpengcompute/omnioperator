/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Least and Greatest function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cstdint>
#include <cmath>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/LeastGreatest.h"
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

// Force function registration via static initialization in Register.h
// (RegisterFunctions::state_ = Register() is called automatically)

// ==================== Helper Functions ====================

// Create a numeric vector with values and null flags
template<typename T>
BaseVector* CreateNumericVector(DataTypeId typeId, const std::vector<T>& values, const std::vector<bool>& nulls) {
    int32_t rowSize = static_cast<int32_t>(values.size());
    BaseVector* vec = VectorHelper::CreateFlatVector(typeId, rowSize);
    auto* typedVec = static_cast<Vector<T>*>(vec);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (nulls[i]) {
            vec->SetNull(i);
        } else {
            typedVec->SetValue(i, values[i]);
            vec->SetNotNull(i);
        }
    }
    return vec;
}

// Create a string vector with values
BaseVector* CreateStringVectorWithValues(const std::vector<std::string>& values, const std::vector<bool>& nulls) {
    int32_t rowSize = static_cast<int32_t>(values.size());
    BaseVector* vec = VectorHelper::CreateStringVector(rowSize);
    auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (nulls[i]) {
            vec->SetNull(i);
        } else {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
            vec->SetNotNull(i);
        }
    }
    return vec;
}

// Execute a least/greatest function
void ExecuteLeastGreatest(const std::string& funcName,
                           const std::vector<BaseVector*>& inputs,
                           DataTypeId outputTypeId,
                           BaseVector*& result) {
    // Ensure vector functions are registered for unit tests.
    link_register_functions();

    // Build signature for lookup
    std::vector<DataTypeId> inputTypes;
    inputTypes.reserve(inputs.size());
    for (const auto* input : inputs) {
        inputTypes.push_back(input->GetTypeId());
    }

    auto signature = std::make_shared<FunctionSignature>(funcName, inputTypes, outputTypeId);
    auto function = VectorFunction::Find(signature);
    
    ASSERT_NE(function, nullptr) << funcName << " function not found for signature";
    
    int32_t rowSize = inputs[0]->GetSize();
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    
    // Push args to stack (reverse order for stack)
    std::stack<BaseVector*> argStack;
    for (int i = static_cast<int>(inputs.size()) - 1; i >= 0; --i) {
        argStack.push(inputs[i]);
    }
    auto outputType = std::make_shared<DataType>(outputTypeId);
    BaseVector* temp = nullptr;
    while (!argStack.empty()) {
        function->Apply(argStack, outputType, result, &context);
        if (temp != nullptr) {
            delete temp;
        }
        if (argStack.size() > 0) {
            argStack.push(result);
            temp = result;
        }
    }
    ASSERT_NE(result, nullptr) << "Result vector is null";
}

// Validate numeric result
template<typename T>
void ValidateNumericResult(BaseVector* result, const std::vector<T>& expected, const std::vector<bool>& expectedNulls) {
    ASSERT_EQ(result->GetSize(), static_cast<int32_t>(expected.size()));
    auto* flatVec = static_cast<Vector<T>*>(result);
    
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expectedNulls[i]) {
            EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be null";
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should not be null";
            T actualValue = flatVec->GetValue(i);
            T expectedValue = expected[i];
            
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(expectedValue)) {
                    EXPECT_TRUE(std::isnan(actualValue)) << "Row " << i << " should be NaN";
                } else if (std::isinf(expectedValue)) {
                    EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " infinity mismatch";
                } else {
                    EXPECT_NEAR(actualValue, expectedValue, 1e-6) << "Row " << i << " value mismatch";
                }
            } else {
                EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
            }
        }
    }
}

// Validate string result
void ValidateStringResult(BaseVector* result, const std::vector<std::string>& expected, const std::vector<bool>& expectedNulls) {
    ASSERT_NE(result, nullptr) << "Result vector is null";
    ASSERT_EQ(result->GetSize(), static_cast<int32_t>(expected.size()));
    auto* flatVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expectedNulls[i]) {
            EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be null";
        } else {
            EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should not be null";
            std::string_view actualValue = flatVec->GetValue(i);
            EXPECT_EQ(actualValue, expected[i]) << "Row " << i << " value mismatch";
        }
    }
}

// ==================== GREATEST Tests ====================

TEST(GreatestTest, IntBasic) {
    std::cout << "=== Test: INT greatest with 2 arguments ===" << std::endl;
    
    std::vector<int32_t> vals1 = {100, 200, 300};
    std::vector<int32_t> vals2 = {400, 100, 600};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    ValidateNumericResult<int32_t>(resultVec, {400, 200, 600}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(GreatestTest, IntWithNull) {
    std::cout << "=== Test: INT greatest with NULL ===" << std::endl;
    
    std::vector<int32_t> vals1 = {100, 200, 300};
    std::vector<int32_t> vals2 = {400, 100, 600};
    std::vector<bool> nulls1 = {true, false, false};  // first is null
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, nulls1);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    // NULL is ignored, so result is 400, 200, 600
    ValidateNumericResult<int32_t>(resultVec, {400, 200, 600}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(GreatestTest, IntAllNull) {
    std::cout << "=== Test: INT greatest with all NULL ===" << std::endl;
    
    std::vector<int32_t> vals1 = {100, 200, 300};
    std::vector<int32_t> vals2 = {400, 100, 600};
    std::vector<bool> allNulls = {true, true, true};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, allNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, allNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    // All NULL -> result is NULL
    ValidateNumericResult<int32_t>(resultVec, {0, 0, 0}, allNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(GreatestTest, LongBasic) {
    std::cout << "=== Test: LONG greatest ===" << std::endl;
    
    std::vector<int64_t> vals1 = {1000000000000LL, 2000000000000LL, 3000000000000LL};
    std::vector<int64_t> vals2 = {4000000000000LL, 1000000000000LL, 2000000000000LL};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_LONG, resultVec);
    
    ValidateNumericResult<int64_t>(resultVec, {4000000000000LL, 2000000000000LL, 3000000000000LL}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(GreatestTest, DoubleBasic) {
    std::cout << "=== Test: DOUBLE greatest ===" << std::endl;
    
    std::vector<double> vals1 = {100.5, 200.7, 300.9};
    std::vector<double> vals2 = {400.1, 100.3, 200.5};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_DOUBLE, resultVec);
    
    ValidateNumericResult<double>(resultVec, {400.1, 200.7, 300.9}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(GreatestTest, DoubleWithNaN) {
    std::cout << "=== Test: DOUBLE greatest with NaN (Spark semantics: NaN > everything) ===" << std::endl;
    
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr double negInf = -std::numeric_limits<double>::infinity();
    
    std::vector<double> vals1 = {inf, nan, negInf, 0.0};
    std::vector<double> vals2 = {nan, inf, 0.0, nan};
    std::vector<double> vals3 = {negInf, 0.0, inf, negInf};
    std::vector<bool> noNulls = {false, false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec, arg3Vec}, OMNI_DOUBLE, resultVec);
    
    // Spark: NaN > everything
    ValidateNumericResult<double>(resultVec, {nan, nan, inf, nan}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(GreatestTest, BooleanBasic) {
    std::cout << "=== Test: BOOLEAN greatest (true > false) ===" << std::endl;
    
    std::vector<bool> vals1 = {true, false, false};
    std::vector<bool> vals2 = {false, true, false};
    std::vector<bool> vals3 = {false, false, true};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<bool>(OMNI_BOOLEAN, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<bool>(OMNI_BOOLEAN, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<bool>(OMNI_BOOLEAN, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec, arg3Vec}, OMNI_BOOLEAN, resultVec);
    
    ValidateNumericResult<bool>(resultVec, {true, true, true}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(GreatestTest, StringBasic) {
    std::cout << "=== Test: STRING greatest ===" << std::endl;
    
    std::vector<std::string> vals1 = {"b", "apple", "zebra"};
    std::vector<std::string> vals2 = {"abcde", "banana", "alpha"};
    std::vector<std::string> vals3 = {"abcdefg", "cherry", "beta"};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateStringVectorWithValues(vals1, noNulls);
    BaseVector* arg2Vec = CreateStringVectorWithValues(vals2, noNulls);
    BaseVector* arg3Vec = CreateStringVectorWithValues(vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    // CreateStringVector now defaults to OMNI_VARCHAR.
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec, arg3Vec}, OMNI_VARCHAR, resultVec);
    
    ValidateStringResult(resultVec, {"b", "cherry", "zebra"}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(GreatestTest, NegativeNumbers) {
    std::cout << "=== Test: Negative numbers ===" << std::endl;
    
    std::vector<int32_t> vals1 = {-100, -500, -300};
    std::vector<int32_t> vals2 = {-200, -200, -400};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    ValidateNumericResult<int32_t>(resultVec, {-100, -200, -300}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

// ==================== LEAST Tests ====================

TEST(LeastTest, IntBasic) {
    std::cout << "=== Test: INT least with 2 arguments ===" << std::endl;
    
    std::vector<int32_t> vals1 = {100, 200, 300};
    std::vector<int32_t> vals2 = {400, 100, 600};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    ValidateNumericResult<int32_t>(resultVec, {100, 100, 300}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(LeastTest, IntWithNull) {
    std::cout << "=== Test: INT least with NULL ===" << std::endl;
    
    std::vector<int32_t> vals1 = {100, 200, 300};
    std::vector<int32_t> vals2 = {400, 100, 600};
    std::vector<bool> nulls1 = {true, false, false};  // first is null
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, nulls1);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    // NULL is ignored, so result is 400, 100, 300
    ValidateNumericResult<int32_t>(resultVec, {400, 100, 300}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(LeastTest, IntAllNull) {
    std::cout << "=== Test: INT least with all NULL ===" << std::endl;
    
    std::vector<int32_t> vals1 = {100, 200, 300};
    std::vector<int32_t> vals2 = {400, 100, 600};
    std::vector<bool> allNulls = {true, true, true};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, allNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, allNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    // All NULL -> result is NULL
    ValidateNumericResult<int32_t>(resultVec, {0, 0, 0}, allNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(LeastTest, LongBasic) {
    std::cout << "=== Test: LONG least ===" << std::endl;
    
    std::vector<int64_t> vals1 = {1000000000000LL, 2000000000000LL, 3000000000000LL};
    std::vector<int64_t> vals2 = {4000000000000LL, 1000000000000LL, 2000000000000LL};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_LONG, resultVec);
    
    ValidateNumericResult<int64_t>(resultVec, {1000000000000LL, 1000000000000LL, 2000000000000LL}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(LeastTest, DoubleBasic) {
    std::cout << "=== Test: DOUBLE least ===" << std::endl;
    
    std::vector<double> vals1 = {100.5, 200.7, 300.9};
    std::vector<double> vals2 = {400.1, 100.3, 200.5};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_DOUBLE, resultVec);
    
    ValidateNumericResult<double>(resultVec, {100.5, 100.3, 200.5}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(LeastTest, DoubleWithNaN) {
    std::cout << "=== Test: DOUBLE least with NaN (Spark semantics: NaN > everything) ===" << std::endl;
    
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr double negInf = -std::numeric_limits<double>::infinity();
    
    // From Velox tests:
    // least(inf, nan, -inf, 0.0) = -inf
    // least(nan, inf, 0.0) = 0.0
    // least(nan, nan, inf) = inf
    // least(nan, nan, nan) = nan
    std::vector<double> vals1 = {inf, nan, nan, nan};
    std::vector<double> vals2 = {nan, inf, nan, nan};
    std::vector<double> vals3 = {negInf, 0.0, inf, nan};
    std::vector<bool> noNulls = {false, false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<double>(OMNI_DOUBLE, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec, arg3Vec}, OMNI_DOUBLE, resultVec);
    
    ValidateNumericResult<double>(resultVec, {negInf, 0.0, inf, nan}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(LeastTest, BooleanBasic) {
    std::cout << "=== Test: BOOLEAN least (false < true) ===" << std::endl;
    
    // From Velox: least(true, false, false) = false
    std::vector<bool> vals1 = {true, false, true};
    std::vector<bool> vals2 = {false, true, true};
    std::vector<bool> vals3 = {false, false, true};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<bool>(OMNI_BOOLEAN, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<bool>(OMNI_BOOLEAN, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<bool>(OMNI_BOOLEAN, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec, arg3Vec}, OMNI_BOOLEAN, resultVec);
    
    ValidateNumericResult<bool>(resultVec, {false, false, true}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(LeastTest, StringBasic) {
    std::cout << "=== Test: STRING least ===" << std::endl;
    
    // From Velox: least("b", "abcde", "abcdefg") = "abcde"
    std::vector<std::string> vals1 = {"b", "apple", "zebra"};
    std::vector<std::string> vals2 = {"abcde", "banana", "alpha"};
    std::vector<std::string> vals3 = {"abcdefg", "cherry", "beta"};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateStringVectorWithValues(vals1, noNulls);
    BaseVector* arg2Vec = CreateStringVectorWithValues(vals2, noNulls);
    BaseVector* arg3Vec = CreateStringVectorWithValues(vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec, arg3Vec}, OMNI_VARCHAR, resultVec);
    
    ValidateStringResult(resultVec, {"abcde", "apple", "alpha"}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(LeastTest, NegativeNumbers) {
    std::cout << "=== Test: Negative numbers ===" << std::endl;
    
    std::vector<int32_t> vals1 = {-100, -500, -300};
    std::vector<int32_t> vals2 = {-200, -200, -400};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_INT, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_INT, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    
    ValidateNumericResult<int32_t>(resultVec, {-200, -500, -400}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

// ==================== Date and Timestamp Tests ====================

TEST(GreatestTest, DateBasic) {
    std::cout << "=== Test: DATE greatest ===" << std::endl;
    
    // From Velox: greatest(100, 1000, 10000, DATE()) = 10000
    std::vector<int32_t> vals1 = {100, 500, 1000};
    std::vector<int32_t> vals2 = {1000, 100, 500};
    std::vector<int32_t> vals3 = {10000, 10000, 10000};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_DATE32, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_DATE32, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<int32_t>(OMNI_DATE32, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec, arg3Vec}, OMNI_DATE32, resultVec);
    
    ValidateNumericResult<int32_t>(resultVec, {10000, 10000, 10000}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(LeastTest, DateBasic) {
    std::cout << "=== Test: DATE least ===" << std::endl;
    
    // From Velox: least(100, 1000, 10000, DATE()) = 100
    std::vector<int32_t> vals1 = {100, 500, 1000};
    std::vector<int32_t> vals2 = {1000, 100, 500};
    std::vector<int32_t> vals3 = {10000, 10000, 10000};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int32_t>(OMNI_DATE32, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int32_t>(OMNI_DATE32, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<int32_t>(OMNI_DATE32, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec, arg3Vec}, OMNI_DATE32, resultVec);
    
    ValidateNumericResult<int32_t>(resultVec, {100, 100, 500}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(GreatestTest, TimestampBasic) {
    std::cout << "=== Test: TIMESTAMP greatest ===" << std::endl;
    
    std::vector<int64_t> vals1 = {1569000000025LL, 4859000000482LL, 581000001651LL};
    std::vector<int64_t> vals2 = {4859000000482LL, 581000001651LL, 1569000000025LL};
    std::vector<int64_t> vals3 = {581000001651LL, 1569000000025LL, 4859000000482LL};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_TIMESTAMP, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_TIMESTAMP, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<int64_t>(OMNI_TIMESTAMP, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec, arg3Vec}, OMNI_TIMESTAMP, resultVec);
    
    ValidateNumericResult<int64_t>(resultVec, {4859000000482LL, 4859000000482LL, 4859000000482LL}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(LeastTest, TimestampBasic) {
    std::cout << "=== Test: TIMESTAMP least ===" << std::endl;
    
    std::vector<int64_t> vals1 = {1569000000025LL, 4859000000482LL, 581000001651LL};
    std::vector<int64_t> vals2 = {4859000000482LL, 581000001651LL, 1569000000025LL};
    std::vector<int64_t> vals3 = {581000001651LL, 1569000000025LL, 4859000000482LL};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_TIMESTAMP, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_TIMESTAMP, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<int64_t>(OMNI_TIMESTAMP, vals3, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec, arg3Vec}, OMNI_TIMESTAMP, resultVec);
    
    ValidateNumericResult<int64_t>(resultVec, {581000001651LL, 581000001651LL, 581000001651LL}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

// ==================== Decimal Tests ====================

TEST(GreatestTest, Decimal64Basic) {
    std::cout << "=== Test: DECIMAL64 greatest ===" << std::endl;
    
    // Decimal64 stored as int64_t with scale
    std::vector<int64_t> vals1 = {10050, 20070, 30090};  // 100.50, 200.70, 300.90
    std::vector<int64_t> vals2 = {40010, 10030, 20050};  // 400.10, 100.30, 200.50
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_DECIMAL64, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_DECIMAL64, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_DECIMAL64, resultVec);
    
    ValidateNumericResult<int64_t>(resultVec, {40010, 20070, 30090}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

TEST(LeastTest, Decimal64Basic) {
    std::cout << "=== Test: DECIMAL64 least ===" << std::endl;
    
    std::vector<int64_t> vals1 = {10050, 20070, 30090};
    std::vector<int64_t> vals2 = {40010, 10030, 20050};
    std::vector<bool> noNulls = {false, false, false};
    
    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_DECIMAL64, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_DECIMAL64, vals2, noNulls);
    
    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec}, OMNI_DECIMAL64, resultVec);
    
    ValidateNumericResult<int64_t>(resultVec, {10050, 10030, 20050}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}

// ==================== Variadic Arity Tests ====================

// A three argument call must consider all three operands in a single Apply, not just the
// two on top of the stack.
TEST(GreatestTest, ThreeArgsSingleCall) {
    std::vector<int64_t> vals1 = {100, 9999999999L, std::numeric_limits<int64_t>::max()};
    std::vector<int64_t> vals2 = {5, 1, std::numeric_limits<int64_t>::min()};
    std::vector<int64_t> vals3 = {50, 5000000000L, 0};
    std::vector<bool> noNulls = {false, false, false};

    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals3, noNulls);

    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec, arg3Vec}, OMNI_LONG, resultVec);

    ValidateNumericResult<int64_t>(resultVec,
        {100, 9999999999L, std::numeric_limits<int64_t>::max()}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

TEST(LeastTest, ThreeArgsSingleCall) {
    std::vector<int64_t> vals1 = {5, 1, -9999999999L};
    std::vector<int64_t> vals2 = {100, 9999999999L, 9999999999L};
    std::vector<int64_t> vals3 = {50, 9999999998L, 0};
    std::vector<bool> noNulls = {false, false, false};

    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals1, noNulls);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals2, noNulls);
    BaseVector* arg3Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals3, noNulls);

    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Least", {arg1Vec, arg2Vec, arg3Vec}, OMNI_LONG, resultVec);

    ValidateNumericResult<int64_t>(resultVec, {5, 1, -9999999999L}, noNulls);

    delete arg1Vec;
    delete arg2Vec;
    delete arg3Vec;
    delete resultVec;
}

// Spark's "Greatest"/"Least" must keep ignoring NULL arguments; the propagating Flink
// variants are covered in FlinkLeastGreatestTest.cpp.
TEST(GreatestTest, SparkNullStillSkipped) {
    std::vector<int64_t> vals1 = {0, 0};
    std::vector<int64_t> vals2 = {5, 0};
    std::vector<bool> nulls1 = {true, true};
    std::vector<bool> nulls2 = {false, true};

    BaseVector* arg1Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals1, nulls1);
    BaseVector* arg2Vec = CreateNumericVector<int64_t>(OMNI_LONG, vals2, nulls2);

    BaseVector* resultVec = nullptr;
    ExecuteLeastGreatest("Greatest", {arg1Vec, arg2Vec}, OMNI_LONG, resultVec);

    ValidateNumericResult<int64_t>(resultVec, {5, 0}, {false, true});

    delete arg1Vec;
    delete arg2Vec;
    delete resultVec;
}
