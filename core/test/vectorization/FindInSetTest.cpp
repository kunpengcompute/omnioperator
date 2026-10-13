/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: FindInSet function unit tests
 *   find_in_set(str, strArray) -> int32
 *   Returns 1-based index of str in comma-delimited strArray, 0 if not found.
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

class FindInSetTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const find_in_set_test_env =
    ::testing::AddGlobalTestEnvironment(new FindInSetTestEnvironment);

class FindInSetTestHelper {
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

    static void ExecuteFindInSet(BaseVector* strVec, BaseVector* strArrayVec,
                                 DataTypeId strTypeId, DataTypeId arrayTypeId,
                                 DataTypeId outputTypeId, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {strTypeId, arrayTypeId};
        auto sig = std::make_shared<FunctionSignature>("find_in_set", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "find_in_set function not found for given type signature";
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(strVec);
        args.push(strArrayVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(FindInSetTest, BasicMatch) {
    std::vector<std::string> strs = {"ab", "abc", "c", "def"};
    std::vector<std::string> arrays = {"abc,b,ab,c,def", "abc,b,ab,c,def",
                                       "abc,b,ab,c,def", "abc,b,ab,c,def"};
    std::vector<int32_t> expected = {3, 1, 4, 5};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 4);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, NotFound) {
    std::vector<std::string> strs = {"dfg", "xyz", "zzz"};
    std::vector<std::string> arrays = {"abc,b,ab,c,def", "abc,b,ab,c,def", "a,b,c"};
    std::vector<int32_t> expected = {0, 0, 0};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, NeedleContainsComma) {
    std::vector<std::string> strs = {"ab,", "a,b", ",c"};
    std::vector<std::string> arrays = {"abc,b,ab,c,def", "a,b,c", "a,b,c"};
    std::vector<int32_t> expected = {0, 0, 0};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, DuplicateMatches) {
    std::vector<std::string> strs = {"ab", "abc"};
    std::vector<std::string> arrays = {"abc,b,ab,ab,ab", "abc,abc,abc,abc,abc"};
    std::vector<int32_t> expected = {3, 1};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 2);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, PartialMatchNotFound) {
    std::vector<std::string> strs = {"dfg", "dfg"};
    std::vector<std::string> arrays = {"dfgdsiaq", "dfgdsiaq, dshadad"};
    std::vector<int32_t> expected = {0, 0};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 2);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, EmptyStrings) {
    std::vector<std::string> strs = {"", "", "", ""};
    std::vector<std::string> arrays = {"", "123", "123,", ",123"};
    std::vector<int32_t> expected = {1, 0, 2, 1};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 4);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, EmptyNeedleInNonEmptyArray) {
    std::vector<std::string> strs = {"123"};
    std::vector<std::string> arrays = {""};
    std::vector<int32_t> expected = {0};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, NullHandling) {
    std::vector<std::string> strs = {"ab", "ab"};
    std::vector<std::string> arrays = {"abc,b,ab,c", "abc,b,ab,c"};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    strVec->SetNull(0);
    arrayVec->SetNull(1);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    EXPECT_TRUE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, UnicodeSupport) {
    std::vector<std::string> strs = {
        "\u0061\u0062",
        "\u0063",
        "\xF0\x9F\x98\x8A",
        "\xF0\x9F\x98\x8A",
        "ab\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8",
        "ab\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8"
    };
    std::vector<std::string> arrays = {
        "abc,b,ab,c,def",
        "abc,b,ab,c,def",
        "\xF0\x9F\x8C\x8D,\xF0\x9F\x98\x8A",
        "\xF0\x9F\x98\x8A,123",
        ",ab\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8",
        "ab\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8,"
    };
    std::vector<int32_t> expected = {3, 4, 2, 1, 2, 1};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 6);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, SingleRow) {
    std::vector<std::string> strs = {"hello"};
    std::vector<std::string> arrays = {"world,hello,foo"};
    std::vector<int32_t> expected = {2};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, SingleElementArray) {
    std::vector<std::string> strs = {"abc", "xyz"};
    std::vector<std::string> arrays = {"abc", "abc"};
    std::vector<int32_t> expected = {1, 0};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 2);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, LastElement) {
    std::vector<std::string> strs = {"def"};
    std::vector<std::string> arrays = {"abc,b,ab,c,def"};
    std::vector<int32_t> expected = {5};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, FirstElement) {
    std::vector<std::string> strs = {"abc"};
    std::vector<std::string> arrays = {"abc,b,ab,c,def"};
    std::vector<int32_t> expected = {1};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, TrailingComma) {
    std::vector<std::string> strs = {"", "abc"};
    std::vector<std::string> arrays = {"abc,", "abc,"};
    std::vector<int32_t> expected = {2, 1};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 2);
    delete strVec;
    delete arrayVec;
    delete result;
}

TEST(FindInSetTest, LeadingComma) {
    std::vector<std::string> strs = {"", "abc"};
    std::vector<std::string> arrays = {",abc", ",abc"};
    std::vector<int32_t> expected = {1, 2};
    BaseVector* strVec = FindInSetTestHelper::CreateStringVector(strs);
    BaseVector* arrayVec = FindInSetTestHelper::CreateStringVector(arrays);
    BaseVector* result = nullptr;
    FindInSetTestHelper::ExecuteFindInSet(strVec, arrayVec, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, result);
    FindInSetTestHelper::ValidateNumericResult(result, expected, 2);
    delete strVec;
    delete arrayVec;
    delete result;
}
