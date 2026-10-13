/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/IsNanFunction.h"
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

class IsNanTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const isnan_test_env = ::testing::AddGlobalTestEnvironment(new IsNanTestEnvironment);

class IsNanFunctionTestHelper {
public:
    template<typename T>
    static BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, values.size());
        auto* typedVec = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteIsNan(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("isnan",
            std::vector<DataTypeId>{inputTypeId}, OMNI_BOOLEAN);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "IsNan function not found for type "
                                    << static_cast<int>(inputTypeId);

        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context));
    }

    static void ValidateBoolResult(BaseVector* result, const std::vector<bool>& expected,
                                   const std::vector<bool>& expectedNulls, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(resultVec, nullptr);

        for (int i = 0; i < rowSize; ++i) {
            if (expectedNulls[i]) {
                EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
                continue;
            }
            EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
            EXPECT_EQ(resultVec->GetValue(i), expected[i]) << "Row " << i << " value mismatch";
        }
    }
};

TEST(IsNanTest, DoubleNormalValue) {
    std::vector<double> inputValues = {0.0, 1.0, -1.0, 3.14};
    std::vector<bool> expected = {false, false, false, false};
    std::vector<bool> expectedNulls = {false, false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, DoubleNanValue) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> inputValues = {nan, nan, nan};
    std::vector<bool> expected = {true, true, true};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, DoubleDivisionByZero) {
    std::vector<double> inputValues = {0.0 / 0.0};
    std::vector<bool> expected = {true};
    std::vector<bool> expectedNulls = {false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

TEST(IsNanTest, DoubleNullInput) {
    std::vector<double> inputValues = {0.0, 0.0, 0.0};
    std::vector<bool> expected = {false, false, false};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    for (int i = 0; i < 3; ++i) {
        inputVec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(IsNanTest, DoubleInfinityValue) {
    double posInf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();
    std::vector<double> inputValues = {posInf, negInf};
    std::vector<bool> expected = {false, false};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

TEST(IsNanTest, DoubleMixedValues) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double posInf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();

    std::vector<double> inputValues = {0.0, nan, 3.14, posInf, negInf, 0.0, nan};
    std::vector<bool> expected = {false, true, false, false, false, false, true};
    std::vector<bool> expectedNulls = {false, false, false, false, false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    inputVec->SetNull(5);
    expected[5] = false;

    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, DoubleBoundaryValues) {
    std::vector<double> inputValues = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::epsilon(),
        -0.0
    };
    std::vector<bool> expected = {false, false, false, false, false};
    std::vector<bool> expectedNulls = {false, false, false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_DOUBLE, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, FloatNormalValue) {
    std::vector<float> inputValues = {0.0f, 1.0f, -1.0f, 3.14f};
    std::vector<bool> expected = {false, false, false, false};
    std::vector<bool> expectedNulls = {false, false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, FloatNanValue) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> inputValues = {nan, nan, nan};
    std::vector<bool> expected = {true, true, true};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, FloatDivisionByZero) {
    std::vector<float> inputValues = {0.0f / 0.0f};
    std::vector<bool> expected = {true};
    std::vector<bool> expectedNulls = {false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls, 1);

    delete resultVec;
}

TEST(IsNanTest, FloatNullInput) {
    std::vector<float> inputValues = {0.0f, 0.0f, 0.0f};
    std::vector<bool> expected = {false, false, false};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    for (int i = 0; i < 3; ++i) {
        inputVec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls, 3);

    delete resultVec;
}

TEST(IsNanTest, FloatInfinityValue) {
    float posInf = std::numeric_limits<float>::infinity();
    float negInf = -std::numeric_limits<float>::infinity();
    std::vector<float> inputValues = {posInf, negInf};
    std::vector<bool> expected = {false, false};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls, 2);

    delete resultVec;
}

TEST(IsNanTest, FloatMixedValues) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    float posInf = std::numeric_limits<float>::infinity();
    float negInf = -std::numeric_limits<float>::infinity();

    std::vector<float> inputValues = {0.0f, nan, 3.14f, posInf, negInf, 0.0f, nan};
    std::vector<bool> expected = {false, true, false, false, false, false, true};
    std::vector<bool> expectedNulls = {false, false, false, false, false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    inputVec->SetNull(5);
    expected[5] = false;

    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}

TEST(IsNanTest, FloatBoundaryValues) {
    std::vector<float> inputValues = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::epsilon(),
        -0.0f
    };
    std::vector<bool> expected = {false, false, false, false, false};
    std::vector<bool> expectedNulls = {false, false, false, false, false};

    BaseVector* inputVec = IsNanFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    BaseVector* resultVec = nullptr;
    IsNanFunctionTestHelper::ExecuteIsNan(inputVec, OMNI_FLOAT, resultVec);
    IsNanFunctionTestHelper::ValidateBoolResult(resultVec, expected, expectedNulls,
                                                static_cast<int>(inputValues.size()));

    delete resultVec;
}
