/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: If function unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/If.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "expression/expressions.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
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

class IfTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const if_test_env = ::testing::AddGlobalTestEnvironment(new IfTestEnvironment);

class IfFunctionTestHelper {
public:
    template<typename T>
    static void ValidateNumericResult(BaseVector* result, const std::vector<T>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<T>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            T actualValue = resultVec->GetValue(i);
            T expectedValue = expected[i];

            if constexpr (std::is_floating_point_v<T>) {
                EXPECT_NEAR(actualValue, expectedValue, 1e-6)
                    << "Row " << i << " value mismatch";
            } else {
                EXPECT_EQ(actualValue, expectedValue)
                    << "Row " << i << " value mismatch";
            }
        }
    }

    static void ValidateStringResult(BaseVector* result, const std::vector<std::string>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not string type";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            std::string_view actualValue = resultVec->GetValue(i);
            std::string expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static void ValidateBooleanResult(BaseVector* result, const std::vector<bool>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            bool actualValue = resultVec->GetValue(i);
            bool expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static void ExecuteIf(BaseVector* condVec, BaseVector* trueVec, BaseVector* falseVec,
                          DataTypeId outputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("if",
            std::vector<DataTypeId>{OMNI_BOOLEAN, outputTypeId, outputTypeId}, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "If function not found for type " << outputTypeId;

        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context;
        context.SetResultRowSize(condVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(condVec);
        args.push(trueVec);
        args.push(falseVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "If function threw an exception";
    }
};

TEST(IfTest, BooleanIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<bool> trueValues = {true, true, false};
    std::vector<bool> falseValues = {false, false, true};
    std::vector<bool> expected = {true, false, false};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateBooleanVector(trueValues);
    BaseVector* falseVec = CreateBooleanVector(falseValues);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_BOOLEAN, resultVec);
    IfFunctionTestHelper::ValidateBooleanResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, BooleanIfNullCondition) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<bool> trueValues = {true, true, false};
    std::vector<bool> falseValues = {false, false, true};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateBooleanVector(trueValues);
    BaseVector* falseVec = CreateBooleanVector(falseValues);

    condVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_BOOLEAN, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL (NULL cond -> false branch)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* boolResult = dynamic_cast<Vector<bool>*>(resultVec);
    EXPECT_EQ(boolResult->GetValue(0), true);
    EXPECT_EQ(boolResult->GetValue(1), false);  
    EXPECT_EQ(boolResult->GetValue(2), false);

    delete resultVec;
}

TEST(IfTest, ByteIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<int8_t> trueValues = {10, 20, 30};
    std::vector<int8_t> falseValues = {40, 50, 60};
    std::vector<int8_t> expected = {10, 50, 30};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_BYTE);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_BYTE);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_BYTE, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, ShortIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<int16_t> trueValues = {1000, 2000, 3000};
    std::vector<int16_t> falseValues = {4000, 5000, 6000};
    std::vector<int16_t> expected = {1000, 5000, 3000};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_SHORT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_SHORT);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_SHORT, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, IntIfTrue) {

    std::vector<bool> condValues = {true, true, true};
    std::vector<int32_t> trueValues = {100, 200, 300};
    std::vector<int32_t> falseValues = {400, 500, 600};
    std::vector<int32_t> expected = {100, 200, 300};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, IntIfFalse) {

    std::vector<bool> condValues = {false, false, false};
    std::vector<int32_t> trueValues = {100, 200, 300};
    std::vector<int32_t> falseValues = {400, 500, 600};
    std::vector<int32_t> expected = {400, 500, 600};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, IntIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<int32_t> trueValues = {100, 200, 300};
    std::vector<int32_t> falseValues = {400, 500, 600};
    std::vector<int32_t> expected = {100, 500, 300};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, IntIfNullCondition) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<int32_t> trueValues = {100, 200, 300};
    std::vector<int32_t> falseValues = {400, 500, 600};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    condVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_FALSE(resultVec->IsNull(1));  
    EXPECT_FALSE(resultVec->IsNull(2));

    auto* intResult = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(intResult->GetValue(0), 100);
    EXPECT_EQ(intResult->GetValue(1), 500);  
    EXPECT_EQ(intResult->GetValue(2), 300);

    delete resultVec;
}

TEST(IfTest, IntIfNullTrueValue) {

    std::vector<bool> condValues = {true, true, false};
    std::vector<int32_t> trueValues = {100, 200, 300};
    std::vector<int32_t> falseValues = {400, 500, 600};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    trueVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (true value is NULL)";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(IfTest, LongIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<int64_t> trueValues = {100LL, 200LL, 300LL};
    std::vector<int64_t> falseValues = {400LL, 500LL, 600LL};
    std::vector<int64_t> expected = {100LL, 500LL, 300LL};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_LONG);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_LONG);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_LONG, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, FloatIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<float> trueValues = {100.5f, 200.7f, 300.9f};
    std::vector<float> falseValues = {400.1f, 500.3f, 600.5f};
    std::vector<float> expected = {100.5f, 500.3f, 300.9f};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_FLOAT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_FLOAT, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, DoubleIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<double> trueValues = {100.5, 200.7, 300.9};
    std::vector<double> falseValues = {400.1, 500.3, 600.5};
    std::vector<double> expected = {100.5, 500.3, 300.9};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_DOUBLE);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_DOUBLE, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, StringIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<std::string> trueValues = {"hello", "world", "test"};
    std::vector<std::string> falseValues = {"foo", "bar", "baz"};
    std::vector<std::string> expected = {"hello", "bar", "test"};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateStringVector(trueValues);
    BaseVector* falseVec = CreateStringVector(falseValues);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_VARCHAR, resultVec);
    IfFunctionTestHelper::ValidateStringResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, StringIfNullCondition) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<std::string> trueValues = {"hello", "world", "test"};
    std::vector<std::string> falseValues = {"foo", "bar", "baz"};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateStringVector(trueValues);
    BaseVector* falseVec = CreateStringVector(falseValues);

    condVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_VARCHAR, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_FALSE(resultVec->IsNull(1));  
    EXPECT_FALSE(resultVec->IsNull(2));

    delete resultVec;
}

TEST(IfTest, TimestampIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<int64_t> trueValues = {1706522730000000LL, 1706609130000000LL, 1706695530000000LL};
    std::vector<int64_t> falseValues = {0LL, 86400000000LL, 172800000000LL};
    std::vector<int64_t> expected = {1706522730000000LL, 86400000000LL, 1706695530000000LL};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_TIMESTAMP);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_TIMESTAMP);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_TIMESTAMP, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, TimestampIfNullCondition) {

    std::vector<bool> condValues = {true, false, false};
    std::vector<int64_t> trueValues = {1000000LL, 2000000LL, 3000000LL};
    std::vector<int64_t> falseValues = {4000000LL, 5000000LL, 6000000LL};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_TIMESTAMP);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_TIMESTAMP);

    condVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    auto* tsResult = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(tsResult->GetValue(0), 4000000LL);

    delete resultVec;
}

TEST(IfTest, BinaryIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<std::string> trueValues = {"binary_a", "binary_b", "binary_c"};
    std::vector<std::string> falseValues = {"bin_x", "bin_y", "bin_z"};
    std::vector<std::string> expected = {"binary_a", "bin_y", "binary_c"};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateStringVector(trueValues, OMNI_VARBINARY);
    BaseVector* falseVec = CreateStringVector(falseValues, OMNI_VARBINARY);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_VARBINARY, resultVec);
    IfFunctionTestHelper::ValidateStringResult(resultVec, expected, condValues.size());

    delete resultVec;
}

TEST(IfTest, BinaryIfNullCondition) {

    std::vector<bool> condValues = {true, true, false};
    std::vector<std::string> trueValues = {"aaa", "bbb", "ccc"};
    std::vector<std::string> falseValues = {"xxx", "yyy", "zzz"};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateStringVector(trueValues, OMNI_VARBINARY);
    BaseVector* falseVec = CreateStringVector(falseValues, OMNI_VARBINARY);

    condVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_VARBINARY, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));  

    auto* binResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVec);
    EXPECT_EQ(binResult->GetValue(0), "aaa");
    EXPECT_EQ(binResult->GetValue(1), "bbb");
    EXPECT_EQ(binResult->GetValue(2), "zzz");  

    delete resultVec;
}

TEST(IfTest, BinaryIfNullValue) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<std::string> trueValues = {"data1", "data2", "data3"};
    std::vector<std::string> falseValues = {"alt1", "alt2", "alt3"};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateStringVector(trueValues, OMNI_VARBINARY);
    BaseVector* falseVec = CreateStringVector(falseValues, OMNI_VARBINARY);

    trueVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_VARBINARY, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (true value is NULL and cond is true)";
    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));

    delete resultVec;
}

TEST(IfTest, ArrayIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<std::vector<int32_t>> trueArrays = {{1, 2, 3}, {4, 5}, {6}};
    std::vector<std::vector<int32_t>> falseArrays = {{10, 20}, {30, 40, 50}, {60, 70}};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateArrayVector(trueArrays);
    BaseVector* falseVec = CreateArrayVector(falseArrays);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_ARRAY, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultArray = dynamic_cast<ArrayVector*>(resultVec);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(static_cast<int64_t>(0)), 3);
    EXPECT_EQ(resultArray->GetSize(static_cast<int64_t>(1)), 3);
    EXPECT_EQ(resultArray->GetSize(static_cast<int64_t>(2)), 1);

    BaseVector* row0Slice = resultArray->GetValue(0);
    auto* row0Vec = static_cast<Vector<int32_t>*>(row0Slice);
    EXPECT_EQ(row0Vec->GetValue(0), 1);
    EXPECT_EQ(row0Vec->GetValue(1), 2);
    EXPECT_EQ(row0Vec->GetValue(2), 3);
    delete row0Slice;

    BaseVector* row1Slice = resultArray->GetValue(1);
    auto* row1Vec = static_cast<Vector<int32_t>*>(row1Slice);
    EXPECT_EQ(row1Vec->GetValue(0), 30);
    EXPECT_EQ(row1Vec->GetValue(1), 40);
    EXPECT_EQ(row1Vec->GetValue(2), 50);
    delete row1Slice;

    delete resultVec;
}

TEST(IfTest, ArrayIfNullCondition) {

    std::vector<bool> condValues = {true, true, false};
    std::vector<std::vector<int32_t>> trueArrays = {{1, 2}, {3, 4}, {5, 6}};
    std::vector<std::vector<int32_t>> falseArrays = {{10, 20}, {30, 40}, {50, 60}};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateArrayVector(trueArrays);
    BaseVector* falseVec = CreateArrayVector(falseArrays);

    condVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_ARRAY, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultArray = dynamic_cast<ArrayVector*>(resultVec);

    EXPECT_EQ(resultArray->GetSize(static_cast<int64_t>(0)), 2);
    BaseVector* row0Slice = resultArray->GetValue(0);
    auto* row0Vec = static_cast<Vector<int32_t>*>(row0Slice);
    EXPECT_EQ(row0Vec->GetValue(0), 10);
    EXPECT_EQ(row0Vec->GetValue(1), 20);
    delete row0Slice;

    delete resultVec;
}

TEST(IfTest, ArrayIfNullValue) {

    std::vector<bool> condValues = {true, false};
    std::vector<std::vector<int32_t>> trueArrays = {{1, 2}, {3, 4}};
    std::vector<std::vector<int32_t>> falseArrays = {{10, 20}, {30, 40}};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateArrayVector(trueArrays);
    BaseVector* falseVec = CreateArrayVector(falseArrays);

    trueVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_ARRAY, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (true array is NULL and cond is true)";
    EXPECT_FALSE(resultVec->IsNull(1));

    delete resultVec;
}


TEST(IfTest, MapIfMixed) {

    std::vector<bool> condValues = {true, false};
    std::vector<std::vector<std::pair<std::string, int32_t>>> trueMaps = {
        {{"k1", 1}, {"k2", 2}},
        {{"k3", 3}}
    };
    std::vector<std::vector<std::pair<std::string, int32_t>>> falseMaps = {
        {{"a1", 10}},
        {{"a2", 20}, {"a3", 30}}
    };

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateMapVector(trueMaps);
    BaseVector* falseVec = CreateMapVector(falseMaps);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_MAP, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultMap = dynamic_cast<MapVector*>(resultVec);
    ASSERT_NE(resultMap, nullptr);

    EXPECT_EQ(resultMap->GetSize(static_cast<int64_t>(0)), 2);
    EXPECT_EQ(resultMap->GetSize(static_cast<int64_t>(1)), 2);

    delete resultVec;
}

TEST(IfTest, MapIfNullCondition) {

    std::vector<bool> condValues = {true, false};
    std::vector<std::vector<std::pair<std::string, int32_t>>> trueMaps = {
        {{"k1", 1}},
        {{"k2", 2}}
    };
    std::vector<std::vector<std::pair<std::string, int32_t>>> falseMaps = {
        {{"a1", 10}, {"a2", 20}},
        {{"a3", 30}}
    };

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateMapVector(trueMaps);
    BaseVector* falseVec = CreateMapVector(falseMaps);

    condVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_MAP, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultMap = dynamic_cast<MapVector*>(resultVec);

    EXPECT_EQ(resultMap->GetSize(static_cast<int64_t>(0)), 2);

    delete resultVec;
}

TEST(IfTest, RowIfMixed) {

    std::vector<bool> condValues = {true, false, true};
    std::vector<std::pair<std::string, int32_t>> trueRows = {
        {"alice", 25}, {"bob", 30}, {"charlie", 35}
    };
    std::vector<std::pair<std::string, int32_t>> falseRows = {
        {"dave", 40}, {"eve", 45}, {"frank", 50}
    };

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateRowVector(trueRows);
    BaseVector* falseVec = CreateRowVector(falseRows);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_ROW, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultRow = dynamic_cast<RowVector*>(resultVec);
    ASSERT_NE(resultRow, nullptr);
    ASSERT_EQ(resultRow->ChildSize(), 2);

    auto* strChild = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultRow->ChildAt(0).get());
    ASSERT_NE(strChild, nullptr);
    EXPECT_EQ(strChild->GetValue(0), "alice");   
    EXPECT_EQ(strChild->GetValue(1), "eve");      
    EXPECT_EQ(strChild->GetValue(2), "charlie"); 

    auto* intChild = dynamic_cast<Vector<int32_t>*>(resultRow->ChildAt(1).get());
    ASSERT_NE(intChild, nullptr);
    EXPECT_EQ(intChild->GetValue(0), 25);   
    EXPECT_EQ(intChild->GetValue(1), 45);   
    EXPECT_EQ(intChild->GetValue(2), 35);   

    delete resultVec;
}

TEST(IfTest, RowIfNullCondition) {

    std::vector<bool> condValues = {true, false};
    std::vector<std::pair<std::string, int32_t>> trueRows = {
        {"alice", 25}, {"bob", 30}
    };
    std::vector<std::pair<std::string, int32_t>> falseRows = {
        {"dave", 40}, {"eve", 45}
    };

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateRowVector(trueRows);
    BaseVector* falseVec = CreateRowVector(falseRows);

    condVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_ROW, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultRow = dynamic_cast<RowVector*>(resultVec);
    auto* strChild = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultRow->ChildAt(0).get());
    auto* intChild = dynamic_cast<Vector<int32_t>*>(resultRow->ChildAt(1).get());
    EXPECT_EQ(strChild->GetValue(0), "dave");
    EXPECT_EQ(intChild->GetValue(0), 40);

    delete resultVec;
}

TEST(IfTest, RowIfNullValue) {

    std::vector<bool> condValues = {true, false};
    std::vector<std::pair<std::string, int32_t>> trueRows = {
        {"alice", 25}, {"bob", 30}
    };
    std::vector<std::pair<std::string, int32_t>> falseRows = {
        {"dave", 40}, {"eve", 45}
    };

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateRowVector(trueRows);
    BaseVector* falseVec = CreateRowVector(falseRows);

    trueVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_ROW, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (true row is NULL)";
    EXPECT_FALSE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(IfTest, AllNullConditions) {

    std::vector<bool> condValues = {false, false, false};
    std::vector<int32_t> trueValues = {100, 200, 300};
    std::vector<int32_t> falseValues = {400, 500, 600};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    condVec->SetNull(0);
    condVec->SetNull(1);
    condVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);

    auto* intResult = dynamic_cast<Vector<int32_t>*>(resultVec);
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(intResult->GetValue(i), falseValues[i]);
    }

    delete resultVec;
}

TEST(IfTest, AllNullValues) {

    std::vector<bool> condValues = {true, false};
    std::vector<int32_t> trueValues = {100, 200};
    std::vector<int32_t> falseValues = {300, 400};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    trueVec->SetNull(0);
    trueVec->SetNull(1);
    falseVec->SetNull(0);
    falseVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    delete resultVec;
}

TEST(IfTest, SingleRow) {

    std::vector<bool> condValues = {true};
    std::vector<int32_t> trueValues = {42};
    std::vector<int32_t> falseValues = {99};
    std::vector<int32_t> expected = {42};

    BaseVector* condVec = CreateBooleanVector(condValues);
    BaseVector* trueVec = CreateNumericVector(trueValues, OMNI_INT);
    BaseVector* falseVec = CreateNumericVector(falseValues, OMNI_INT);

    BaseVector* resultVec = nullptr;
    IfFunctionTestHelper::ExecuteIf(condVec, trueVec, falseVec, OMNI_INT, resultVec);
    IfFunctionTestHelper::ValidateNumericResult(resultVec, expected, condValues.size());

    delete resultVec;
}

namespace {
BaseVector* RunIfExpr(IfExpr* ifExpr, VectorBatch* batch, int32_t rowSize)
{
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    ExprEval e(batch, &context);
    e.VisitExpr(*ifExpr);
    return e.GetResult();
}
}

TEST(IfTest, LazyIfExprIntMixed)
{
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), new FieldExpr(1, intType), new FieldExpr(2, intType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    int32_t trueVals[rowSize] = {1, 2, 3, 4};
    int32_t falseVals[rowSize] = {10, 20, 30, 40};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, trueVals, falseVals);

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);
    EXPECT_EQ(resVec->GetValue(1), 20);
    EXPECT_EQ(resVec->GetValue(2), 3);
    EXPECT_EQ(resVec->GetValue(3), 40);

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprComputedBranches)
{
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* trueBranch = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(1, intType),
        new LiteralExpr(static_cast<int32_t>(1), intType), intType);
    auto* falseBranch = new BinaryExpr(expressions::Operator::SUB, new FieldExpr(2, intType),
        new LiteralExpr(static_cast<int32_t>(1), intType), intType);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), trueBranch, falseBranch);
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    int32_t a[rowSize] = {1, 2, 3, 4};
    int32_t b[rowSize] = {10, 20, 30, 40};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b);

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 2);    // a[0] + 1
    EXPECT_EQ(resVec->GetValue(1), 19);   // b[1] - 1
    EXPECT_EQ(resVec->GetValue(2), 4);    // a[2] + 1
    EXPECT_EQ(resVec->GetValue(3), 39);   // b[3] - 1

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprNullCondition)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), new FieldExpr(1, intType), new FieldExpr(2, intType));

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true};
    int32_t trueVals[rowSize] = {1, 2, 3};
    int32_t falseVals[rowSize] = {10, 20, 30};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, trueVals, falseVals);
    input->Get(0)->SetNull(1);
    input->Get(1)->SetNull(2);   // true branch value is NULL at row 2

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);
    EXPECT_EQ(resVec->GetValue(1), 20);    // NULL condition takes the false branch
    EXPECT_TRUE(resVec->IsNull(2));        // NULL true value is preserved

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprStringBranches)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), new FieldExpr(1, strType), new FieldExpr(2, strType));

    std::vector vecOfTypes = {BooleanType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true};
    std::string trueVals[rowSize] = {"hello", "world", "test"};
    std::string falseVals[rowSize] = {"foo", "bar", "baz"};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, trueVals, falseVals);

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), "hello");
    EXPECT_EQ(resVec->GetValue(1), "bar");
    EXPECT_EQ(resVec->GetValue(2), "test");

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprConstConditionShortCircuitsFalseBranch)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    // The false branch references a function that is not registered: with lazy
    // evaluation and a constant true condition it must never be visited.
    auto* falseBranch = new FuncExpr("not_a_registered_function",
        std::vector<Expr*>{new FieldExpr(0, intType)}, intType);
    auto* ifExpr = new IfExpr(new LiteralExpr(true, boolType), new FieldExpr(0, intType), falseBranch);
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col[rowSize] = {1, 2, 3};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, col);

    BaseVector* result = nullptr;
    ASSERT_NO_THROW(result = RunIfExpr(ifExpr, input, rowSize));
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);
    EXPECT_EQ(resVec->GetValue(1), 2);
    EXPECT_EQ(resVec->GetValue(2), 3);

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprNested)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* innerIf = new IfExpr(new FieldExpr(1, boolType), new FieldExpr(2, intType), new FieldExpr(3, intType));
    auto* outerIf = new IfExpr(new FieldExpr(0, boolType), innerIf, new FieldExpr(4, intType));
    ASSERT_NE(outerIf->vectorFunction, nullptr);

    std::vector vecOfTypes = {BooleanType(), BooleanType(), IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond1[rowSize] = {true, false, true};
    bool cond2[rowSize] = {true, true, false};
    int32_t a[rowSize] = {1, 2, 3};
    int32_t b[rowSize] = {10, 20, 30};
    int32_t c[rowSize] = {100, 200, 300};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond1, cond2, a, b, c);

    auto* result = RunIfExpr(outerIf, input, rowSize);
    auto* resVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), 1);    // cond1 true, cond2 true -> a
    EXPECT_EQ(resVec->GetValue(1), 200);  // cond1 false -> c
    EXPECT_EQ(resVec->GetValue(2), 30);   // cond1 true, cond2 false -> b

    delete result;
    delete input;
    delete outerIf;
}

TEST(IfTest, LazyIfExprUnderFilter)
{
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto* trueBranch = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(1, intType),
        new LiteralExpr(static_cast<int32_t>(1), intType), intType);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), trueBranch, new FieldExpr(2, intType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    std::vector vecOfTypes = {BooleanType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    int32_t a[rowSize] = {1, 2, 3, 4};
    int32_t b[rowSize] = {10, 20, 30, 40};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b);

    bool mask[rowSize] = {true, true, false, false};
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    context.hasFilter = true;
    context.SetIsSelectRow(mask);

    ExprEval e(input, &context);
    e.VisitExpr(*ifExpr);
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
    delete ifExpr;
}

TEST(IfTest, LazyIfExprArrayBranches)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(std::make_shared<DataType>(OMNI_INT));
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), new FieldExpr(1, arrayType), new FieldExpr(2, arrayType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateBooleanVector({true, false, true}));
    input->Append(CreateArrayVector({{1, 2}, {3}, {4, 5, 6}}));
    input->Append(CreateArrayVector({{9}, {8, 7}, {6}}));

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resArr = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(resArr, nullptr);
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(0)), 2);
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(1)), 2);
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(2)), 3);

    BaseVector* row0 = resArr->GetValue(0);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row0)->GetValue(0), 1);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row0)->GetValue(1), 2);
    delete row0;

    BaseVector* row1 = resArr->GetValue(1);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row1)->GetValue(0), 8);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row1)->GetValue(1), 7);
    delete row1;

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprMapBranches)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto mapType = std::make_shared<MapType>(std::make_shared<DataType>(OMNI_VARCHAR),
        std::make_shared<DataType>(OMNI_INT));
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), new FieldExpr(1, mapType), new FieldExpr(2, mapType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateBooleanVector({true, false, true}));
    input->Append(CreateMapVector({{{"k1", 1}}, {{"k2", 2}}, {{"k3", 3}}}));
    input->Append(CreateMapVector({{{"a1", 10}}, {{"a2", 20}, {"a3", 30}}, {{"a4", 40}}}));

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resMap = dynamic_cast<MapVector*>(result);
    ASSERT_NE(resMap, nullptr);
    EXPECT_EQ(resMap->GetSize(static_cast<int64_t>(0)), 1);  // true branch {"k1": 1}
    EXPECT_EQ(resMap->GetSize(static_cast<int64_t>(1)), 2);  // false branch {"a2": 20, "a3": 30}
    EXPECT_EQ(resMap->GetSize(static_cast<int64_t>(2)), 1);  // true branch {"k3": 3}

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprRowBranches)
{
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    std::vector<std::shared_ptr<DataType>> rowFields = {std::make_shared<DataType>(OMNI_VARCHAR),
        std::make_shared<DataType>(OMNI_INT)};
    auto rowType = std::make_shared<RowType>(rowFields);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), new FieldExpr(1, rowType), new FieldExpr(2, rowType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateBooleanVector({true, false, true}));
    input->Append(CreateRowVector({{"alice", 25}, {"bob", 30}, {"charlie", 35}}));
    input->Append(CreateRowVector({{"dave", 40}, {"eve", 45}, {"frank", 50}}));

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resRow = dynamic_cast<RowVector*>(result);
    ASSERT_NE(resRow, nullptr);
    auto* strChild = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resRow->ChildAt(0).get());
    auto* intChild = dynamic_cast<Vector<int32_t>*>(resRow->ChildAt(1).get());
    ASSERT_NE(strChild, nullptr);
    ASSERT_NE(intChild, nullptr);
    EXPECT_EQ(strChild->GetValue(0), "alice");
    EXPECT_EQ(intChild->GetValue(0), 25);
    EXPECT_EQ(strChild->GetValue(1), "eve");
    EXPECT_EQ(intChild->GetValue(1), 45);
    EXPECT_EQ(strChild->GetValue(2), "charlie");
    EXPECT_EQ(intChild->GetValue(2), 35);

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprInsideLambda)
{
    // transform(arr, x -> if(x > 3, x, 0)): exercises lazy IF evaluation over the
    // lambda input space, where branches are lambda params instead of batch columns.
    int rowSize = 3;
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
    auto* ifBody = new IfExpr(cond, new ParamRefExpr("x", IntType()), new LiteralExpr(0, IntType()));
    ASSERT_NE(ifBody->vectorFunction, nullptr);
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    auto* expr = new FuncExpr("transform", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LambdaExpr(ifBody, paramTypes, paramNameToIdxMap, IntType())
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

TEST(IfTest, LazyIfExprInsideLambdaUnderFilter)
{
    // Filtered batch + IF inside a lambda: the lambda space is the flattened
    // elements (6 > 3 batch rows), so the batch filter mask must not be applied
    // to it — it is defined over batch rows only. Regresses the mask over-read
    // and the out-of-bounds branch selection of the lazy-IF filtered path.
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
    auto* ifBody = new IfExpr(cond, new ParamRefExpr("x", IntType()), new LiteralExpr(0, IntType()));
    ASSERT_NE(ifBody->vectorFunction, nullptr);
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    auto* expr = new FuncExpr("transform", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LambdaExpr(ifBody, paramTypes, paramNameToIdxMap, IntType())
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

TEST(IfTest, LazyIfExprComputedStringBranches)
{
    // IF(cond, lower(a), b): expensive string branch merged through the ordered,
    // type-specialized merge path.
    const int32_t rowSize = 4;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto* trueBranch = new FuncExpr("lower", std::vector<Expr*>{new FieldExpr(1, strType)}, strType);
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), trueBranch, new FieldExpr(2, strType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    std::vector vecOfTypes = {BooleanType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    bool cond[rowSize] = {true, false, true, false};
    std::string a[rowSize] = {"HELLO", "WORLD", "TEST", "ABC"};
    std::string b[rowSize] = {"foo", "bar", "baz", "qux"};
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, cond, a, b);

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resVec, nullptr);
    EXPECT_EQ(resVec->GetValue(0), "hello");
    EXPECT_EQ(resVec->GetValue(1), "bar");
    EXPECT_EQ(resVec->GetValue(2), "test");
    EXPECT_EQ(resVec->GetValue(3), "qux");

    delete result;
    delete input;
    delete ifExpr;
}

TEST(IfTest, LazyIfExprComputedArrayBranches)
{
    // IF(cond, array(a), b): expensive array branch merged through the ordered,
    // complex merge path.
    const int32_t rowSize = 3;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(std::make_shared<DataType>(OMNI_INT));
    auto* trueBranch = new FuncExpr("array", std::vector<Expr*>{new FieldExpr(1, intType)},
        std::make_shared<DataType>(OMNI_ARRAY));
    auto* ifExpr = new IfExpr(new FieldExpr(0, boolType), trueBranch, new FieldExpr(2, arrayType));
    ASSERT_NE(ifExpr->vectorFunction, nullptr);

    auto* input = new VectorBatch(rowSize);
    input->Append(CreateBooleanVector({true, false, true}));
    input->Append(CreateNumericVector<int32_t>({1, 2, 3}, OMNI_INT));
    input->Append(CreateArrayVector({{9}, {8, 7}, {6}}));

    auto* result = RunIfExpr(ifExpr, input, rowSize);
    auto* resArr = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(resArr, nullptr);
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(0)), 1);  // [a[0]]
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(1)), 2);  // false branch {8, 7}
    EXPECT_EQ(resArr->GetSize(static_cast<int64_t>(2)), 1);  // [a[2]]

    BaseVector* row0 = resArr->GetValue(0);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row0)->GetValue(0), 1);
    delete row0;

    BaseVector* row1 = resArr->GetValue(1);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row1)->GetValue(0), 8);
    EXPECT_EQ(static_cast<Vector<int32_t>*>(row1)->GetValue(1), 7);
    delete row1;

    delete result;
    delete input;
    delete ifExpr;
}
