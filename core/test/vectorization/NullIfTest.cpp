/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: NullIf function unit tests
*/

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/NullIf.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/data_type.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class NullIfTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const nullif_test_env = ::testing::AddGlobalTestEnvironment(new NullIfTestEnvironment);

class NullIfFunctionTestHelper {
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
            ASSERT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
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

    static void ValidateStringResult(BaseVector* result, const std::vector<std::string>& expected,
                                     const std::vector<bool>& expectedNulls, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not string type";

        for (int i = 0; i < rowSize; ++i) {
            if (expectedNulls[i]) {
                EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
                continue;
            }
            ASSERT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
            std::string_view actualValue = resultVec->GetValue(i);
            std::string expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static void ValidateBooleanResult(BaseVector* result, const std::vector<bool>& expected,
                                      const std::vector<bool>& expectedNulls, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";

        for (int i = 0; i < rowSize; ++i) {
            if (expectedNulls[i]) {
                EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
                continue;
            }
            ASSERT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
            bool actualValue = resultVec->GetValue(i);
            bool expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    template<typename T>
    static BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId) {
        BaseVector* vec = VectorHelper::CreateFlatVector(typeId, values.size());
        auto* typedVec = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values,
                                          DataTypeId typeId = OMNI_VARCHAR) {
        BaseVector* vec = VectorHelper::CreateFlatVector(typeId, values.size());
        auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }

    static void ExecuteNullIf(BaseVector* expr1Vec, BaseVector* expr2Vec,
                              DataTypeId outputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("nullif",
            std::vector<DataTypeId>{outputTypeId, outputTypeId}, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "NullIf function not found for type "
                                    << static_cast<int>(outputTypeId);

        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context;
        context.SetResultRowSize(expr1Vec->GetSize());
        std::stack<BaseVector*> args;

        // Push arguments: expr1 first, then expr2 (Apply pops expr2 first, then expr1)
        args.push(expr1Vec);
        args.push(expr2Vec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "NullIf function threw an exception";
    }
};

// =====================================================
// INT type tests
// =====================================================

TEST(NullIfTest, IntEqualValues) {
    std::vector<int32_t> expr1Values = {5, 10, 15};
    std::vector<int32_t> expr2Values = {5, 10, 15};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int32_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, IntNotEqualValues) {
    std::vector<int32_t> expr1Values = {5, 10, 15};
    std::vector<int32_t> expr2Values = {6, 11, 16};
    std::vector<int32_t> expected = {5, 10, 15};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, IntNullExpr1) {
    std::vector<int32_t> expr1Values = {0, 0, 0};
    std::vector<int32_t> expr2Values = {5, 10, 15};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int32_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);
    for (int i = 0; i < 3; ++i) {
        expr1Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, IntNullExpr2) {
    std::vector<int32_t> expr1Values = {5, 10, 15};
    std::vector<int32_t> expr2Values = {0, 0, 0};
    std::vector<int32_t> expected = {5, 10, 15};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);
    for (int i = 0; i < 3; ++i) {
        expr2Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, IntMixedValues) {
    // Row 0: equal -> NULL
    // Row 1: not equal -> expr1
    // Row 2: null expr1 -> NULL
    // Row 3: null expr2 -> expr1
    // Row 4: both null -> NULL
    std::vector<int32_t> expr1Values = {42, 100, 0, 7, 0};
    std::vector<int32_t> expr2Values = {42, 200, 50, 0, 0};
    std::vector<int32_t> expected = {0, 100, 0, 7, 0};
    std::vector<bool> expectedNulls = {true, false, true, false, true};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);
    expr1Vec->SetNull(2);
    expr2Vec->SetNull(3);
    expr1Vec->SetNull(4);
    expr2Vec->SetNull(4);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 5);

    delete resultVec;
}

// =====================================================
// LONG type tests
// =====================================================

TEST(NullIfTest, LongEqualValues) {
    std::vector<int64_t> expr1Values = {100000LL, 200000LL, 300000LL};
    std::vector<int64_t> expr2Values = {100000LL, 200000LL, 300000LL};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int64_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_LONG);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_LONG);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_LONG, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, LongNotEqualValues) {
    std::vector<int64_t> expr1Values = {100000LL, 200000LL, 300000LL};
    std::vector<int64_t> expr2Values = {100001LL, 200001LL, 300001LL};
    std::vector<int64_t> expected = {100000LL, 200000LL, 300000LL};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_LONG);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_LONG);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_LONG, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// BYTE type tests
// =====================================================

TEST(NullIfTest, ByteEqualValues) {
    std::vector<int8_t> expr1Values = {1, 2, 3};
    std::vector<int8_t> expr2Values = {1, 2, 3};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int8_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_BYTE);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_BYTE);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_BYTE, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, ByteNotEqualValues) {
    std::vector<int8_t> expr1Values = {1, 2, 3};
    std::vector<int8_t> expr2Values = {4, 5, 6};
    std::vector<int8_t> expected = {1, 2, 3};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_BYTE);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_BYTE);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_BYTE, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// SHORT type tests
// =====================================================

TEST(NullIfTest, ShortEqualValues) {
    std::vector<int16_t> expr1Values = {100, 200, 300};
    std::vector<int16_t> expr2Values = {100, 200, 300};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int16_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_SHORT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_SHORT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_SHORT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, ShortNotEqualValues) {
    std::vector<int16_t> expr1Values = {100, 200, 300};
    std::vector<int16_t> expr2Values = {101, 201, 301};
    std::vector<int16_t> expected = {100, 200, 300};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_SHORT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_SHORT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_SHORT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// FLOAT type tests
// =====================================================

TEST(NullIfTest, FloatEqualValues) {
    std::vector<float> expr1Values = {1.5f, 2.5f, 3.5f};
    std::vector<float> expr2Values = {1.5f, 2.5f, 3.5f};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<float> expected = {0.0f, 0.0f, 0.0f};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, FloatNotEqualValues) {
    std::vector<float> expr1Values = {1.5f, 2.5f, 3.5f};
    std::vector<float> expr2Values = {1.6f, 2.6f, 3.6f};
    std::vector<float> expected = {1.5f, 2.5f, 3.5f};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_FLOAT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_FLOAT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_FLOAT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// DOUBLE type tests
// =====================================================

TEST(NullIfTest, DoubleEqualValues) {
    std::vector<double> expr1Values = {3.14159, 2.71828, 1.41421};
    std::vector<double> expr2Values = {3.14159, 2.71828, 1.41421};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<double> expected = {0.0, 0.0, 0.0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, DoubleNotEqualValues) {
    std::vector<double> expr1Values = {3.14159, 2.71828, 1.41421};
    std::vector<double> expr2Values = {3.14160, 2.71829, 1.41422};
    std::vector<double> expected = {3.14159, 2.71828, 1.41421};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, DoubleNullExpr1) {
    std::vector<double> expr1Values = {0.0, 0.0};
    std::vector<double> expr2Values = {5.0, 10.0};
    std::vector<bool> expectedNulls = {true, true};
    std::vector<double> expected = {0.0, 0.0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);
    expr1Vec->SetNull(0);
    expr1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

TEST(NullIfTest, DoubleNullExpr2) {
    std::vector<double> expr1Values = {5.0, 10.0};
    std::vector<double> expr2Values = {0.0, 0.0};
    std::vector<double> expected = {5.0, 10.0};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DOUBLE);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DOUBLE);
    expr2Vec->SetNull(0);
    expr2Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DOUBLE, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// =====================================================
// BOOLEAN type tests
// =====================================================

TEST(NullIfTest, BooleanEqualValues) {
    std::vector<bool> expr1Values = {true, false, true};
    std::vector<bool> expr2Values = {true, false, true};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<bool> expected = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_BOOLEAN);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_BOOLEAN);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_BOOLEAN, resultVec);
    NullIfFunctionTestHelper::ValidateBooleanResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, BooleanNotEqualValues) {
    std::vector<bool> expr1Values = {true, false, true};
    std::vector<bool> expr2Values = {false, true, false};
    std::vector<bool> expected = {true, false, true};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_BOOLEAN);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_BOOLEAN);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_BOOLEAN, resultVec);
    NullIfFunctionTestHelper::ValidateBooleanResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, BooleanNullExpr1) {
    std::vector<bool> expr1Values = {true, false};
    std::vector<bool> expr2Values = {true, false};
    std::vector<bool> expectedNulls = {true, true};
    std::vector<bool> expected = {false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_BOOLEAN);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_BOOLEAN);
    expr1Vec->SetNull(0);
    expr1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_BOOLEAN, resultVec);
    NullIfFunctionTestHelper::ValidateBooleanResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

TEST(NullIfTest, BooleanNullExpr2) {
    std::vector<bool> expr1Values = {true, false};
    std::vector<bool> expr2Values = {false, true};
    std::vector<bool> expected = {true, false};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_BOOLEAN);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_BOOLEAN);
    expr2Vec->SetNull(0);
    expr2Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_BOOLEAN, resultVec);
    NullIfFunctionTestHelper::ValidateBooleanResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// =====================================================
// VARCHAR (string) type tests
// =====================================================

TEST(NullIfTest, VarcharEqualValues) {
    std::vector<std::string> expr1Values = {"hello", "world", "test"};
    std::vector<std::string> expr2Values = {"hello", "world", "test"};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<std::string> expected = {"", "", ""};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARCHAR);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARCHAR);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARCHAR, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, VarcharNotEqualValues) {
    std::vector<std::string> expr1Values = {"hello", "world", "test"};
    std::vector<std::string> expr2Values = {"HELLO", "WORLD", "TEST"};
    std::vector<std::string> expected = {"hello", "world", "test"};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARCHAR);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARCHAR);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARCHAR, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, VarcharNullExpr1) {
    std::vector<std::string> expr1Values = {"hello", "world"};
    std::vector<std::string> expr2Values = {"hello", "world"};
    std::vector<bool> expectedNulls = {true, true};
    std::vector<std::string> expected = {"", ""};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARCHAR);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARCHAR);
    expr1Vec->SetNull(0);
    expr1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARCHAR, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

TEST(NullIfTest, VarcharNullExpr2) {
    std::vector<std::string> expr1Values = {"hello", "world"};
    std::vector<std::string> expr2Values = {"world", "hello"};
    std::vector<std::string> expected = {"hello", "world"};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARCHAR);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARCHAR);
    expr2Vec->SetNull(0);
    expr2Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARCHAR, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

TEST(NullIfTest, VarcharEmptyStrings) {
    // Empty string equal to empty string -> NULL
    // Empty string not equal to non-empty -> expr1
    std::vector<std::string> expr1Values = {"", "", "abc"};
    std::vector<std::string> expr2Values = {"", "abc", ""};
    std::vector<std::string> expected = {"", "", "abc"};
    std::vector<bool> expectedNulls = {true, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARCHAR);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARCHAR);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARCHAR, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// DATE32 type tests (stored as int32_t)
// =====================================================

TEST(NullIfTest, Date32EqualValues) {
    std::vector<int32_t> expr1Values = {19358, 19359, 19360};
    std::vector<int32_t> expr2Values = {19358, 19359, 19360};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int32_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DATE32);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DATE32);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DATE32, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, Date32NotEqualValues) {
    std::vector<int32_t> expr1Values = {19358, 19359, 19360};
    std::vector<int32_t> expr2Values = {19359, 19360, 19361};
    std::vector<int32_t> expected = {19358, 19359, 19360};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DATE32);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DATE32);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DATE32, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// TIMESTAMP type tests (stored as int64_t)
// =====================================================

TEST(NullIfTest, TimestampEqualValues) {
    std::vector<int64_t> expr1Values = {1672531200000LL, 1672617600000LL, 1672704000000LL};
    std::vector<int64_t> expr2Values = {1672531200000LL, 1672617600000LL, 1672704000000LL};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int64_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_TIMESTAMP);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_TIMESTAMP);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_TIMESTAMP, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, TimestampNotEqualValues) {
    std::vector<int64_t> expr1Values = {1672531200000LL, 1672617600000LL};
    std::vector<int64_t> expr2Values = {1672531200001LL, 1672617600001LL};
    std::vector<int64_t> expected = {1672531200000LL, 1672617600000LL};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_TIMESTAMP);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_TIMESTAMP);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_TIMESTAMP, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

// =====================================================
// VARBINARY type tests (same storage as VARCHAR)
// =====================================================

TEST(NullIfTest, VarbinaryEqualValues) {
    std::vector<std::string> expr1Values = {"\x01\x02", "\x03\x04", "\x05\x06"};
    std::vector<std::string> expr2Values = {"\x01\x02", "\x03\x04", "\x05\x06"};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<std::string> expected = {"", "", ""};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARBINARY);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARBINARY);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARBINARY, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, VarbinaryNotEqualValues) {
    std::vector<std::string> expr1Values = {"\x01\x02", "\x03\x04", "\x05\x06"};
    std::vector<std::string> expr2Values = {"\x01\x03", "\x03\x05", "\x05\x07"};
    std::vector<std::string> expected = {"\x01\x02", "\x03\x04", "\x05\x06"};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARBINARY);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARBINARY);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARBINARY, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// DATE64 type tests (stored as int64_t)
// =====================================================

TEST(NullIfTest, Date64EqualValues) {
    std::vector<int64_t> expr1Values = {19358LL, 19359LL, 19360LL};
    std::vector<int64_t> expr2Values = {19358LL, 19359LL, 19360LL};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int64_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DATE64);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DATE64);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DATE64, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, Date64NotEqualValues) {
    std::vector<int64_t> expr1Values = {19358LL, 19359LL, 19360LL};
    std::vector<int64_t> expr2Values = {19359LL, 19360LL, 19361LL};
    std::vector<int64_t> expected = {19358LL, 19359LL, 19360LL};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DATE64);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DATE64);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DATE64, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// DECIMAL64 type tests (stored as int64_t)
// =====================================================

TEST(NullIfTest, Decimal64EqualValues) {
    std::vector<int64_t> expr1Values = {1000LL, 2000LL, 3000LL};
    std::vector<int64_t> expr2Values = {1000LL, 2000LL, 3000LL};
    std::vector<bool> expectedNulls = {true, true, true};
    std::vector<int64_t> expected = {0, 0, 0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DECIMAL64);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DECIMAL64);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DECIMAL64, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(NullIfTest, Decimal64NotEqualValues) {
    std::vector<int64_t> expr1Values = {1000LL, 2000LL, 3000LL};
    std::vector<int64_t> expr2Values = {1001LL, 2001LL, 3001LL};
    std::vector<int64_t> expected = {1000LL, 2000LL, 3000LL};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DECIMAL64);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DECIMAL64);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DECIMAL64, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

// =====================================================
// DECIMAL128 type tests
// =====================================================

TEST(NullIfTest, Decimal128EqualValues) {
    std::vector<Decimal128> expr1Values = {
        Decimal128(1000),
        Decimal128(2000),
        Decimal128(3000)
    };
    std::vector<Decimal128> expr2Values = {
        Decimal128(1000),
        Decimal128(2000),
        Decimal128(3000)
    };
    std::vector<bool> expectedNulls = {true, true, true};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DECIMAL128);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DECIMAL128);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DECIMAL128, resultVec);

    auto* resultTypedVec = dynamic_cast<Vector<Decimal128>*>(resultVec);
    ASSERT_NE(resultTypedVec, nullptr);
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(resultVec->IsNull(i)) << "Row " << i << " should be NULL";
    }

    delete resultVec;
}

TEST(NullIfTest, Decimal128NotEqualValues) {
    std::vector<Decimal128> expr1Values = {
        Decimal128(1000),
        Decimal128(2000),
        Decimal128(3000)
    };
    std::vector<Decimal128> expr2Values = {
        Decimal128(1001),
        Decimal128(2001),
        Decimal128(3001)
    };
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_DECIMAL128);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_DECIMAL128);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_DECIMAL128, resultVec);

    auto* resultTypedVec = dynamic_cast<Vector<Decimal128>*>(resultVec);
    ASSERT_NE(resultTypedVec, nullptr);
    for (int i = 0; i < 3; ++i) {
        ASSERT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(resultTypedVec->GetValue(i), expr1Values[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// =====================================================
// Single-row edge case tests
// =====================================================

TEST(NullIfTest, IntSingleRowEqual) {
    std::vector<int32_t> expr1Values = {42};
    std::vector<int32_t> expr2Values = {42};
    std::vector<bool> expectedNulls = {true};
    std::vector<int32_t> expected = {0};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

TEST(NullIfTest, IntSingleRowNotEqual) {
    std::vector<int32_t> expr1Values = {42};
    std::vector<int32_t> expr2Values = {43};
    std::vector<int32_t> expected = {42};
    std::vector<bool> expectedNulls = {false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

TEST(NullIfTest, IntBoundaryValues) {
    // Test with INT min/max values
    std::vector<int32_t> expr1Values = {
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max(),
        0,
        -1
    };
    std::vector<int32_t> expr2Values = {
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max() - 1,
        0,
        1
    };
    // Row 0: equal (min == min) -> NULL
    // Row 1: not equal (max != max-1) -> expr1
    // Row 2: equal (0 == 0) -> NULL
    // Row 3: not equal (-1 != 1) -> expr1
    std::vector<int32_t> expected = {
        0,
        std::numeric_limits<int32_t>::max(),
        0,
        -1
    };
    std::vector<bool> expectedNulls = {true, false, true, false};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateNumericVector(expr1Values, OMNI_INT);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateNumericVector(expr2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_INT, resultVec);
    NullIfFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, 4);

    delete resultVec;
}

TEST(NullIfTest, VarcharMixedValues) {
    // Row 0: equal -> NULL
    // Row 1: not equal -> expr1
    // Row 2: null expr1 -> NULL
    // Row 3: null expr2 -> expr1
    // Row 4: both null -> NULL
    // Row 5: equal empty strings -> NULL
    std::vector<std::string> expr1Values = {"abc", "def", "ghi", "jkl", "mno", ""};
    std::vector<std::string> expr2Values = {"abc", "DEF", "xxx", "yyy", "zzz", ""};
    std::vector<std::string> expected = {"", "def", "", "jkl", "", ""};
    std::vector<bool> expectedNulls = {true, false, true, false, true, true};

    BaseVector* expr1Vec = NullIfFunctionTestHelper::CreateStringVector(expr1Values, OMNI_VARCHAR);
    BaseVector* expr2Vec = NullIfFunctionTestHelper::CreateStringVector(expr2Values, OMNI_VARCHAR);
    expr1Vec->SetNull(2);
    expr2Vec->SetNull(3);
    expr1Vec->SetNull(4);
    expr2Vec->SetNull(4);

    BaseVector* resultVec = nullptr;
    NullIfFunctionTestHelper::ExecuteNullIf(expr1Vec, expr2Vec, OMNI_VARCHAR, resultVec);
    NullIfFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, 6);

    delete resultVec;
}
