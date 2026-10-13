/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Char_length / character_length / length function unit tests
 *   length(string) -> int32
 *   Returns the number of characters (Unicode code points). Empty string returns 0.
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

class CharLengthTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const char_length_test_env =
    ::testing::AddGlobalTestEnvironment(new CharLengthTestEnvironment);

class CharLengthFunctionTestHelper {
public:
    static void ValidateNumericResult(BaseVector* result,
                                     const std::vector<int32_t>& expected,
                                     int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actual = resultVec->GetValue(i);
            int32_t exp = expected[i];
            EXPECT_EQ(actual, exp) << "Row " << i << " expected=" << exp << " actual=" << actual;
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

    static void ExecuteLength(BaseVector* stringVec, DataTypeId outputTypeId, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { stringVec->GetTypeId() };
        auto sig = std::make_shared<FunctionSignature>("length", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Length function not found";
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(CharLengthTest, EmptyStringReturnsZero) {
    std::vector<std::string> strings = {"", "", ""};
    std::vector<int32_t> expected = {0, 0, 0};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    CharLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(CharLengthTest, AsciiSingleChar) {
    std::vector<std::string> strings = {"a", "1", " "};
    std::vector<int32_t> expected = {1, 1, 1};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    CharLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(CharLengthTest, AsciiMultiChar) {
    std::vector<std::string> strings = {"hello", "12345", "abc"};
    std::vector<int32_t> expected = {5, 5, 3};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    CharLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(CharLengthTest, UnicodeCharacters) {
    std::vector<std::string> strings = {"中", "中文", "a中b"};
    std::vector<int32_t> expected = {1, 2, 3};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    CharLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(CharLengthTest, NullHandling) {
    std::vector<std::string> strings = {"a", "bb", "ccc"};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    strVec->SetNull(1);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    EXPECT_FALSE(result->IsNull(2));
    delete strVec;
    delete result;
}

TEST(CharLengthTest, SingleRow) {
    std::vector<std::string> strings = {"hello"};
    std::vector<int32_t> expected = {5};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    CharLengthFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete result;
}

TEST(CharLengthTest, NullByteCountedAsOneChar) {
    std::vector<std::string> strings = {std::string("\0", 1)};
    std::vector<int32_t> expected = {1};
    BaseVector* strVec = CharLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    CharLengthFunctionTestHelper::ExecuteLength(strVec, OMNI_INT, result);
    CharLengthFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete result;
}
