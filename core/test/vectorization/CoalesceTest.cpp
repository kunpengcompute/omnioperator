/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Coalesce function unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Coalesce.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "type/data_type.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class CoalesceTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const coalesce_test_env = ::testing::AddGlobalTestEnvironment(new CoalesceTestEnvironment);

class CoalesceFunctionTestHelper {
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
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, values.size());
        auto* typedVec = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values, DataTypeId typeId = OMNI_VARCHAR) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, values.size());
        auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }

    static void ExecuteCoalesce(const std::vector<BaseVector*>& inputs, DataTypeId outputTypeId,
                                BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds(inputs.size(), outputTypeId);

        auto signature = std::make_shared<FunctionSignature>("coalesce", inputTypeIds, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Coalesce function not found for signature";

        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputs[0]->GetSize());
        std::stack<BaseVector*> args;
        for (auto* input : inputs) {
            args.push(input);
        }

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Coalesce function threw an exception";
    }

    static void ExecuteCoalesceWithComplexType(const std::vector<BaseVector*>& inputs,
                                                const DataTypePtr& outputType,
                                                BaseVector*& result) {
        DataTypeId outId = outputType->GetId();
        std::vector<DataTypeId> inputTypeIds(inputs.size(), outId);

        auto signature = std::make_shared<FunctionSignature>("coalesce", inputTypeIds, outId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Coalesce function not found for complex type signature";

        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputs[0]->GetSize());
        std::stack<BaseVector*> args;
        for (auto* input : inputs) {
            args.push(input);
        }

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Coalesce function threw an exception for complex type";
    }
};


TEST(CoalesceTest, BooleanCoalesceTwoArgs) {

    std::vector<bool> arg1Values = {true, false, true};
    std::vector<bool> arg2Values = {false, true, false};
    std::vector<bool> expected = {true, false, true};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_BOOLEAN);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_BOOLEAN);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_BOOLEAN, resultVec);
    CoalesceFunctionTestHelper::ValidateBooleanResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, BooleanCoalesceWithNullFirst) {

    std::vector<bool> arg1Values = {true, false, true};
    std::vector<bool> arg2Values = {false, true, false};
    std::vector<bool> expected = {false, false, true};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_BOOLEAN);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_BOOLEAN);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_BOOLEAN, resultVec);
    CoalesceFunctionTestHelper::ValidateBooleanResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, ByteCoalesce) {

    std::vector<int8_t> arg1Values = {10, 20, 30};
    std::vector<int8_t> arg2Values = {40, 50, 60};
    std::vector<int8_t> expected = {40, 20, 30};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_BYTE);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_BYTE);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_BYTE, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, ShortCoalesce) {

    std::vector<int16_t> arg1Values = {100, 200, 300};
    std::vector<int16_t> arg2Values = {400, 500, 600};
    std::vector<int16_t> expected = {400, 200, 300};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_SHORT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_SHORT);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_SHORT, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, IntCoalesceTwoArgs) {

    std::vector<int32_t> arg1Values = {100, 200, 300};
    std::vector<int32_t> arg2Values = {400, 500, 600};
    std::vector<int32_t> expected = {100, 200, 300};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_INT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, IntCoalesceWithNullFirst) {

    std::vector<int32_t> arg1Values = {100, 200, 300};
    std::vector<int32_t> arg2Values = {400, 500, 600};
    std::vector<int32_t> expected = {400, 200, 300};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_INT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_INT);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, IntCoalesceAllNull) {

    std::vector<int32_t> arg1Values = {100, 200, 300};
    std::vector<int32_t> arg2Values = {400, 500, 600};
    std::vector<bool> expectedNulls = {true, true, true};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_INT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_INT);
    for (int i = 0; i < 3; ++i) {
        arg1Vec->SetNull(i);
        arg2Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_INT, resultVec);

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(resultVec->IsNull(i)) << "Row " << i << " should be NULL";
    }

    delete resultVec;
}

TEST(CoalesceTest, LongCoalesce) {

    std::vector<int64_t> arg1Values = {100LL, 200LL, 300LL};
    std::vector<int64_t> arg2Values = {400LL, 500LL, 600LL};
    std::vector<int64_t> expected = {100LL, 500LL, 300LL};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_LONG);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_LONG);
    arg1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_LONG, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, FloatCoalesce) {

    std::vector<float> arg1Values = {1.1f, 2.2f, 3.3f};
    std::vector<float> arg2Values = {4.4f, 5.5f, 6.6f};
    std::vector<float> expected = {4.4f, 2.2f, 3.3f};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_FLOAT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_FLOAT);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_FLOAT, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, DoubleCoalesce) {

    std::vector<double> arg1Values = {100.5, 200.7, 300.9};
    std::vector<double> arg2Values = {400.1, 500.3, 600.5};
    std::vector<double> expected = {100.5, 500.3, 300.9};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_DOUBLE);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_DOUBLE);
    arg1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_DOUBLE, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, TimestampCoalesce) {

    std::vector<int64_t> arg1Values = {1706518530000000LL, 0LL, 1706518530000000LL};
    std::vector<int64_t> arg2Values = {0LL, 1706518530000000LL, 0LL};
    std::vector<int64_t> expected = {0LL, 1706518530000000LL, 1706518530000000LL};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_TIMESTAMP);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_TIMESTAMP);
    arg1Vec->SetNull(0);
    arg1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_TIMESTAMP, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, BinaryCoalesce) {

    std::vector<std::string> arg1Values = {"binary1", "binary2", "binary3"};
    std::vector<std::string> arg2Values = {"alt1", "alt2", "alt3"};
    std::vector<std::string> expected = {"alt1", "binary2", "binary3"};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateStringVector(arg1Values, OMNI_VARBINARY);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateStringVector(arg2Values, OMNI_VARBINARY);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_VARBINARY, resultVec);
    CoalesceFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, BinaryCoalesceAllNull) {

    std::vector<std::string> arg1Values = {"a", "b", "c"};
    std::vector<std::string> arg2Values = {"d", "e", "f"};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateStringVector(arg1Values, OMNI_VARBINARY);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateStringVector(arg2Values, OMNI_VARBINARY);
    for (int i = 0; i < 3; ++i) {
        arg1Vec->SetNull(i);
        arg2Vec->SetNull(i);
    }

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_VARBINARY, resultVec);

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(resultVec->IsNull(i)) << "Row " << i << " should be NULL";
    }

    delete resultVec;
}

TEST(CoalesceTest, StringCoalesce) {

    std::vector<std::string> arg1Values = {"hello", "world", "test"};
    std::vector<std::string> arg2Values = {"foo", "bar", "baz"};
    std::vector<std::string> expected = {"hello", "world", "test"};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateStringVector(arg1Values);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateStringVector(arg2Values);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_VARCHAR, resultVec);
    CoalesceFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, StringCoalesceWithNullFirst) {

    std::vector<std::string> arg1Values = {"hello", "world", "test"};
    std::vector<std::string> arg2Values = {"foo", "bar", "baz"};
    std::vector<std::string> expected = {"foo", "world", "test"};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateStringVector(arg1Values);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateStringVector(arg2Values);
    arg1Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_VARCHAR, resultVec);
    CoalesceFunctionTestHelper::ValidateStringResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, ArrayCoalesce) {

    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto arg1 = static_cast<ArrayVector*>(VectorHelper::CreateComplexVector(arrayType.get(), 3));
    
    auto elem0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 3);
    static_cast<Vector<int32_t>*>(elem0)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(elem0)->SetValue(1, 2);
    static_cast<Vector<int32_t>*>(elem0)->SetValue(2, 3);
    arg1->SetValue(0, elem0);
    delete elem0;
    
    arg1->SetNull(1);
    
    auto elem2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 3);
    static_cast<Vector<int32_t>*>(elem2)->SetValue(0, 7);
    static_cast<Vector<int32_t>*>(elem2)->SetValue(1, 8);
    static_cast<Vector<int32_t>*>(elem2)->SetValue(2, 9);
    arg1->SetValue(2, elem2);
    delete elem2;

    auto arg2 = static_cast<ArrayVector*>(VectorHelper::CreateComplexVector(arrayType.get(), 3));
    auto elem3 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 2);
    static_cast<Vector<int32_t>*>(elem3)->SetValue(0, 10);
    static_cast<Vector<int32_t>*>(elem3)->SetValue(1, 20);
    arg2->SetValue(0, elem3);
    delete elem3;
    auto elem4 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 2);
    static_cast<Vector<int32_t>*>(elem4)->SetValue(0, 40);
    static_cast<Vector<int32_t>*>(elem4)->SetValue(1, 50);
    arg2->SetValue(1, elem4);
    delete elem4;
    arg2->SetNull(2);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesceWithComplexType({arg1, arg2}, arrayType, resultVec);
    ASSERT_NE(resultVec, nullptr);

    auto* resultArray = dynamic_cast<ArrayVector*>(resultVec);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_FALSE(resultArray->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_EQ(resultArray->GetSize(0), 3) << "Row 0 array size should be 3";

    EXPECT_FALSE(resultArray->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_EQ(resultArray->GetSize(1), 2) << "Row 1 array size should be 2";

    EXPECT_FALSE(resultArray->IsNull(2)) << "Row 2 should not be NULL";
    EXPECT_EQ(resultArray->GetSize(2), 3) << "Row 2 array size should be 3";

    delete resultVec;
}

TEST(CoalesceTest, ArrayCoalesceAllNull) {

    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto arg1 = static_cast<ArrayVector*>(VectorHelper::CreateComplexVector(arrayType.get(), 2));
    arg1->SetNull(0);
    arg1->SetNull(1);

    auto arg2 = static_cast<ArrayVector*>(VectorHelper::CreateComplexVector(arrayType.get(), 2));
    arg2->SetNull(0);
    arg2->SetNull(1);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesceWithComplexType({arg1, arg2}, arrayType, resultVec);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    delete resultVec;
}

TEST(CoalesceTest, MapCoalesce) {

    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto mapType = std::make_shared<MapType>(strType, intType);

    auto arg1 = static_cast<MapVector*>(VectorHelper::CreateComplexVector(mapType.get(), 3));

    auto keyVec1 = VectorHelper::CreateFlatVector(nullptr, OMNI_VARCHAR, 1);
    std::string_view k1("k1");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(keyVec1)->SetValue(0, k1);
    auto valVec1 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 1);
    static_cast<Vector<int32_t>*>(valVec1)->SetValue(0, 1);

    VectorHelper::ExpandElementVector(arg1->GetKeyVector().get(), OMNI_VARCHAR, 1);
    VectorHelper::AppendVector(arg1->GetKeyVector().get(), 0, keyVec1, 1);
    VectorHelper::ExpandElementVector(arg1->GetValueVector().get(), OMNI_INT, 1);
    VectorHelper::AppendVector(arg1->GetValueVector().get(), 0, valVec1, 1);
    arg1->SetSize(0, 1);
    delete keyVec1;
    delete valVec1;

    arg1->SetNull(1);

    auto keyVec3 = VectorHelper::CreateFlatVector(nullptr, OMNI_VARCHAR, 1);
    std::string_view k3("k3");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(keyVec3)->SetValue(0, k3);
    auto valVec3 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 1);
    static_cast<Vector<int32_t>*>(valVec3)->SetValue(0, 3);

    int64_t currentOffset = arg1->GetOffset(2);
    VectorHelper::ExpandElementVector(arg1->GetKeyVector().get(), OMNI_VARCHAR, currentOffset + 1);
    VectorHelper::AppendVector(arg1->GetKeyVector().get(), currentOffset, keyVec3, 1);
    VectorHelper::ExpandElementVector(arg1->GetValueVector().get(), OMNI_INT, currentOffset + 1);
    VectorHelper::AppendVector(arg1->GetValueVector().get(), currentOffset, valVec3, 1);
    arg1->SetSize(2, 1);
    delete keyVec3;
    delete valVec3;

    auto arg2 = static_cast<MapVector*>(VectorHelper::CreateComplexVector(mapType.get(), 3));

    arg2->SetNull(0);

    auto keyVec2 = VectorHelper::CreateFlatVector(nullptr, OMNI_VARCHAR, 1);
    std::string_view k2("k2");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(keyVec2)->SetValue(0, k2);
    auto valVec2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 1);
    static_cast<Vector<int32_t>*>(valVec2)->SetValue(0, 2);

    int64_t offset2 = arg2->GetOffset(1);
    VectorHelper::ExpandElementVector(arg2->GetKeyVector().get(), OMNI_VARCHAR, offset2 + 1);
    VectorHelper::AppendVector(arg2->GetKeyVector().get(), offset2, keyVec2, 1);
    VectorHelper::ExpandElementVector(arg2->GetValueVector().get(), OMNI_INT, offset2 + 1);
    VectorHelper::AppendVector(arg2->GetValueVector().get(), offset2, valVec2, 1);
    arg2->SetSize(1, 1);
    delete keyVec2;
    delete valVec2;

    arg2->SetNull(2);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesceWithComplexType({arg1, arg2}, mapType, resultVec);
    ASSERT_NE(resultVec, nullptr);

    auto* resultMap = dynamic_cast<MapVector*>(resultVec);
    ASSERT_NE(resultMap, nullptr);

    EXPECT_FALSE(resultMap->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_EQ(resultMap->GetSize(0), 1) << "Row 0 map size should be 1";

    EXPECT_FALSE(resultMap->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_EQ(resultMap->GetSize(1), 1) << "Row 1 map size should be 1";

    EXPECT_FALSE(resultMap->IsNull(2)) << "Row 2 should not be NULL";
    EXPECT_EQ(resultMap->GetSize(2), 1) << "Row 2 map size should be 1";

    delete resultVec;
}

TEST(CoalesceTest, RowCoalesce) {

    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_VARCHAR),
        std::make_shared<DataType>(OMNI_INT)
    };
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"name", "age"});

    auto arg1 = static_cast<RowVector*>(VectorHelper::CreateComplexVector(rowType.get(), 3));
    std::string_view alice("Alice");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(arg1->ChildAt(0).get())->SetValue(0, alice);
    static_cast<Vector<int32_t>*>(arg1->ChildAt(1).get())->SetValue(0, 30);
    arg1->SetNull(1);
    arg1->ChildAt(0)->SetNull(1);
    arg1->ChildAt(1)->SetNull(1);
    std::string_view charlie("Charlie");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(arg1->ChildAt(0).get())->SetValue(2, charlie);
    static_cast<Vector<int32_t>*>(arg1->ChildAt(1).get())->SetValue(2, 50);

    auto arg2 = static_cast<RowVector*>(VectorHelper::CreateComplexVector(rowType.get(), 3));
    arg2->SetNull(0);
    arg2->ChildAt(0)->SetNull(0);
    arg2->ChildAt(1)->SetNull(0);
    std::string_view bob("Bob");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(arg2->ChildAt(0).get())->SetValue(1, bob);
    static_cast<Vector<int32_t>*>(arg2->ChildAt(1).get())->SetValue(1, 40);
    arg2->SetNull(2);
    arg2->ChildAt(0)->SetNull(2);
    arg2->ChildAt(1)->SetNull(2);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesceWithComplexType({arg1, arg2}, rowType, resultVec);
    ASSERT_NE(resultVec, nullptr);

    auto* resultRow = dynamic_cast<RowVector*>(resultVec);
    ASSERT_NE(resultRow, nullptr);

    EXPECT_FALSE(resultRow->IsNull(0)) << "Row 0 should not be NULL";
    auto* nameVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(resultRow->ChildAt(0).get());
    auto* ageVec = static_cast<Vector<int32_t>*>(resultRow->ChildAt(1).get());
    EXPECT_EQ(nameVec->GetValue(0), "Alice");
    EXPECT_EQ(ageVec->GetValue(0), 30);

    EXPECT_FALSE(resultRow->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_EQ(nameVec->GetValue(1), "Bob");
    EXPECT_EQ(ageVec->GetValue(1), 40);

    EXPECT_FALSE(resultRow->IsNull(2)) << "Row 2 should not be NULL";
    EXPECT_EQ(nameVec->GetValue(2), "Charlie");
    EXPECT_EQ(ageVec->GetValue(2), 50);

    delete resultVec;
}

TEST(CoalesceTest, RowCoalesceAllNull) {

    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_VARCHAR),
        std::make_shared<DataType>(OMNI_INT)
    };
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"name", "age"});

    auto arg1 = static_cast<RowVector*>(VectorHelper::CreateComplexVector(rowType.get(), 2));
    arg1->SetNull(0);
    arg1->SetNull(1);

    auto arg2 = static_cast<RowVector*>(VectorHelper::CreateComplexVector(rowType.get(), 2));
    arg2->SetNull(0);
    arg2->SetNull(1);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesceWithComplexType({arg1, arg2}, rowType, resultVec);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    delete resultVec;
}

TEST(CoalesceTest, IntCoalesceMixedNullPattern) {

    std::vector<int32_t> arg1Values = {0, 200, 0, 400};
    std::vector<int32_t> arg2Values = {100, 0, 0, 0};
    std::vector<int32_t> expected = {100, 200, 0, 400};
    std::vector<bool> expectedNulls = {false, false, true, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_INT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_INT);
    arg1Vec->SetNull(0);
    arg1Vec->SetNull(2);
    arg2Vec->SetNull(1);
    arg2Vec->SetNull(2);
    arg2Vec->SetNull(3);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, IntCoalesceBoundaryValues) {

    std::vector<int32_t> arg1Values = {std::numeric_limits<int32_t>::max(),
                                        std::numeric_limits<int32_t>::min(), 0};
    std::vector<int32_t> arg2Values = {0, 0, 0};
    std::vector<int32_t> expected = {std::numeric_limits<int32_t>::max(),
                                      std::numeric_limits<int32_t>::min(), 0};
    std::vector<bool> expectedNulls = {false, false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_INT);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_INT);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_INT, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}

TEST(CoalesceTest, LongCoalesceBoundaryValues) {

    std::vector<int64_t> arg1Values = {std::numeric_limits<int64_t>::max(),
                                        std::numeric_limits<int64_t>::min()};
    std::vector<int64_t> arg2Values = {0LL, 0LL};
    std::vector<int64_t> expected = {std::numeric_limits<int64_t>::max(),
                                      std::numeric_limits<int64_t>::min()};
    std::vector<bool> expectedNulls = {false, false};

    BaseVector* arg1Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg1Values, OMNI_LONG);
    BaseVector* arg2Vec = CoalesceFunctionTestHelper::CreateNumericVector(arg2Values, OMNI_LONG);

    BaseVector* resultVec = nullptr;
    CoalesceFunctionTestHelper::ExecuteCoalesce({arg1Vec, arg2Vec}, OMNI_LONG, resultVec);
    CoalesceFunctionTestHelper::ValidateNumericResult(resultVec, expected, expectedNulls, arg1Values.size());

    delete resultVec;
}
