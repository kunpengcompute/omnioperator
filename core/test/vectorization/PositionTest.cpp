/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Position function unit tests
 *   position(substring, string) -> int32
 *   Returns 1-based position of first occurrence; 0 if not found; 1 if substring is empty.
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

class PositionTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const position_test_env =
    ::testing::AddGlobalTestEnvironment(new PositionTestEnvironment);

class PositionFunctionTestHelper {
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

    static void ExecutePosition(BaseVector* subStringVec, BaseVector* stringVec,
                                DataTypeId outputTypeId, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {
            subStringVec->GetTypeId(),
            stringVec->GetTypeId()
        };
        auto sig = std::make_shared<FunctionSignature>("position", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Position function not found";
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(subStringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(subStringVec);
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(PositionTest, BasicPosition) {
    std::vector<std::string> subStrings = {"aa", "aa", "xyz"};
    std::vector<std::string> strings = {"aaads", "aaads", "aaads"};
    std::vector<int32_t> expected = {1, 1, 0};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, EmptySubstringReturnsOne) {
    std::vector<std::string> subStrings = {"", "", ""};
    std::vector<std::string> strings = {"aaads", "test", "hello"};
    std::vector<int32_t> expected = {1, 1, 1};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, EmptyStringReturnsZero) {
    std::vector<std::string> subStrings = {"aa", "x"};
    std::vector<std::string> strings = {"", ""};
    std::vector<int32_t> expected = {0, 0};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 2);
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, SubstringLongerThanStringReturnsZero) {
    std::vector<std::string> subStrings = {"aaadsxyz", "longer"};
    std::vector<std::string> strings = {"aaads", "hi"};
    std::vector<int32_t> expected = {0, 0};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 2);
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, NullHandling) {
    std::vector<std::string> subStrings = {"aa", "aa", "aa"};
    std::vector<std::string> strings = {"aaads", "aaads", "aaads"};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    subVec->SetNull(0);
    strVec->SetNull(1);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    EXPECT_TRUE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    EXPECT_FALSE(result->IsNull(2));
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, UnicodeSupport) {
    std::vector<std::string> subStrings = {"万丈", "万丈", "xyz"};
    std::vector<std::string> strings = {"一丁丂七丄丅丆万丈三上下", "一丁丂七丄丅丆万丈三上下", "一丁丂七丄丅丆万丈三上下"};
    std::vector<int32_t> expected = {8, 8, 0};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, SingleRow) {
    std::vector<std::string> subStrings = {"o"};
    std::vector<std::string> strings = {"hello"};
    std::vector<int32_t> expected = {5};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete subVec;
    delete strVec;
    delete result;
}

TEST(PositionTest, EquivalentToLocateWithStartOne) {
    std::vector<std::string> subStrings = {"ab", "cd", "ef"};
    std::vector<std::string> strings = {"xabxx", "cdcd", "nope"};
    std::vector<int32_t> expected = {2, 1, 0};
    BaseVector* subVec = PositionFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = PositionFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    PositionFunctionTestHelper::ExecutePosition(subVec, strVec, OMNI_INT, result);
    PositionFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete result;
}
