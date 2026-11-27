/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: overlay function unit tests
 *   overlay(input, replace, pos, len) -> varchar
 *   Replaces len chars of input starting at 1-based pos with replace.
 *   pos: 1-based; 0 = first char; negative = from end. len: -1 = use replace length.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
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

class OverlayTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const overlay_test_env =
    ::testing::AddGlobalTestEnvironment(new OverlayTestEnvironment);

class OverlayFunctionTestHelper {
public:
    static void ValidateStringResult(BaseVector* result,
                                      const std::vector<std::string>& expected,
                                      int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            std::string_view actualSv = resultVec->GetValue(i);
            std::string actual(actualSv);
            const std::string& exp = expected[static_cast<size_t>(i)];
            EXPECT_EQ(actual, exp) << "Row " << i << " expected=\"" << exp << "\" actual=\"" << actual << "\"";
        }
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        vec->SetIsField(true);
        auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        EXPECT_NE(typed, nullptr);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typed->SetValue(i, sv);
        }
        return vec;
    }

    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteOverlay(BaseVector* inputVec, BaseVector* replaceVec,
                               BaseVector* posVec, BaseVector* lenVec,
                               BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {
            OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, OMNI_INT
        };
        auto sig = std::make_shared<FunctionSignature>("overlay", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "overlay(input, replace, pos, len) not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(replaceVec);
        args.push(posVec);
        args.push(lenVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(OverlayTest, BasicOverlayLenMinusOne) {
    std::vector<std::string> inputs = { "Spark SQL", "Spark SQL", "Spark SQL" };
    std::vector<std::string> replaces = { "_", "CORE", "tructured" };
    std::vector<int32_t> positions = { 6, 7, 2 };
    std::vector<int32_t> lengths = { -1, -1, 4 };
    std::vector<std::string> expected = { "Spark_SQL", "Spark CORE", "Structured SQL" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(OverlayTest, OverlayPosZeroAndNegative) {
    std::vector<std::string> inputs = { "Spark SQL", "Spark SQL", "Spark SQL" };
    std::vector<std::string> replaces = { "##", "##", "##" };
    std::vector<int32_t> positions = { 0, -10, 10 };
    std::vector<int32_t> lengths = { -1, -1, -1 };
    // pos=0: part3 start (0-based) = (0+2)-1 = 1 -> "##park SQL"
    // pos=-10: posPlusLen=-8 <= 0, part3Start = 9+(-8)=1 -> "##park SQL"
    // pos=-10 len=4: part3Start = 9+(-6)=3 -> "##rk SQL"
    // pos=10: part3 start > len -> "Spark SQL##"
    std::vector<std::string> expected = { "##park SQL", "##park SQL", "Spark SQL##" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(OverlayTest, OverlayExplicitLength) {
    std::vector<std::string> inputs = { "Spark SQL", "Spark SQL" };
    std::vector<std::string> replaces = { "ANSI ", "##" };
    std::vector<int32_t> positions = { 7, 10 };
    std::vector<int32_t> lengths = { 0, 4 };
    std::vector<std::string> expected = { "Spark ANSI SQL", "Spark SQL##" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(OverlayTest, OverlayUnicode) {
    std::vector<std::string> inputs = { "Spark\u6570\u636ESQL" };
    std::vector<std::string> replaces = { "_" };
    std::vector<int32_t> positions = { 6 };
    std::vector<int32_t> lengths = { -1 };
    std::vector<std::string> expected = { "Spark_\u636ESQL" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 1);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(OverlayTest, OverlayNegativePosLenMinusOne) {
    std::vector<std::string> inputs = { "Spark\u6570\u636ESQL" };
    std::vector<std::string> replaces = { "_" };
    std::vector<int32_t> positions = { -6 };
    std::vector<int32_t> lengths = { 2 };
    std::vector<std::string> expected = { "_\u636ESQL" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 1);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(OverlayTest, OverlayNegativePosExplicitLen) {
    std::vector<std::string> inputs = { "Spark SQL" };
    std::vector<std::string> replaces = { "##" };
    std::vector<int32_t> positions = { -10 };
    std::vector<int32_t> lengths = { 4 };
    std::vector<std::string> expected = { "##rk SQL" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 1);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(OverlayTest, OverlayEmptyReplace) {
    std::vector<std::string> inputs = { "hello" };
    std::vector<std::string> replaces = { "" };
    std::vector<int32_t> positions = { 3 };
    std::vector<int32_t> lengths = { 2 };
    std::vector<std::string> expected = { "heo" };
    BaseVector* inputVec = OverlayFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* replaceVec = OverlayFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* posVec = OverlayFunctionTestHelper::CreateInt32Vector(positions);
    BaseVector* lenVec = OverlayFunctionTestHelper::CreateInt32Vector(lengths);
    BaseVector* result = nullptr;
    OverlayFunctionTestHelper::ExecuteOverlay(inputVec, replaceVec, posVec, lenVec, result);
    OverlayFunctionTestHelper::ValidateStringResult(result, expected, 1);
    delete inputVec;
    delete replaceVec;
    delete posVec;
    delete lenVec;
    delete result;
}
