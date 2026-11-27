/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Locate function unit tests
 *   locate(substring, string, start) -> int32
 *   Returns 1-based position; 0 if not found or invalid start.
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

class LocateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const locate_test_env =
    ::testing::AddGlobalTestEnvironment(new LocateTestEnvironment);

class LocateFunctionTestHelper {
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

    template <typename T>
    static BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteLocate(BaseVector* subStringVec, BaseVector* stringVec,
                              BaseVector* startVec, DataTypeId outputTypeId,
                              BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {
            subStringVec->GetTypeId(),
            stringVec->GetTypeId(),
            startVec->GetTypeId()
        };
        auto sig = std::make_shared<FunctionSignature>("locate", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Locate function not found";
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(subStringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(subStringVec);
        args.push(stringVec);
        args.push(startVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(LocateTest, BasicLocate) {
    std::vector<std::string> subStrings = {"aa", "aa", "xyz"};
    std::vector<std::string> strings = {"aaads", "aaads", "aaads"};
    std::vector<int32_t> starts = {1, 2, 1};
    std::vector<int32_t> expected = {1, 2, 0};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, EmptySubstringReturnsOne) {
    std::vector<std::string> subStrings = {"", "", ""};
    std::vector<std::string> strings = {"aaads", "test", "hello"};
    std::vector<int32_t> starts = {1, 1, 1};
    std::vector<int32_t> expected = {1, 1, 1};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, EmptyStringReturnsZero) {
    std::vector<std::string> subStrings = {"aa", "x"};
    std::vector<std::string> strings = {"", ""};
    std::vector<int32_t> starts = {1, 1};
    std::vector<int32_t> expected = {0, 0};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 2);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, InvalidStartPosition) {
    std::vector<std::string> subStrings = {"aa", "aa", "aa"};
    std::vector<std::string> strings = {"aaads", "aaads", "aaads"};
    std::vector<int32_t> starts = {0, -1, 10};
    std::vector<int32_t> expected = {0, 0, 0};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, SubstringLongerThanStringReturnsZero) {
    std::vector<std::string> subStrings = {"aaadsxyz", "longer"};
    std::vector<std::string> strings = {"aaads", "hi"};
    std::vector<int32_t> starts = {1, 1};
    std::vector<int32_t> expected = {0, 0};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 2);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

// 空值传播：在 SimpleFunction 框架下，任一参数为 NULL 的列会从计算中排除，  
// 该列结果被标记为 NULL。因此当 sub/str/start 中任意一个为 NULL 时，该列结果均为 NULL。  
// 与 Spark「start 为 NULL 时返回 0」的语义不同，受限于现有的空值传播逻辑。
TEST(LocateTest, NullHandling) {
    std::vector<std::string> subStrings = {"aa", "aa", "aa"};
    std::vector<std::string> strings = {"aaads", "aaads", "aaads"};
    std::vector<int32_t> starts = {1, 1, 1};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    subVec->SetNull(0);
    strVec->SetNull(1);
    startVec->SetNull(2);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    EXPECT_TRUE(result->IsNull(0));  // substring NULL → result NULL
    EXPECT_TRUE(result->IsNull(1));  // string NULL → result NULL
    EXPECT_TRUE(result->IsNull(2));  // start NULL → result NULL（框架预设，非 Spark 的 0）
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, UnicodeSupport) {
    std::vector<std::string> subStrings = {"万丈", "万丈", "xyz"};
    std::vector<std::string> strings = {"一丁丂七丄丅丆万丈三上下", "一丁丂七丄丅丆万丈三上下", "一丁丂七丄丅丆万丈三上下"};
    std::vector<int32_t> starts = {1, 8, 1};
    std::vector<int32_t> expected = {8, 8, 0};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, Int64StartPosition) {
    std::vector<std::string> subStrings = {"aa", "aa"};
    std::vector<std::string> strings = {"aaads", "aaads"};
    std::vector<int64_t> starts = {1LL, 2LL};
    std::vector<int32_t> expected = {1, 2};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_LONG);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 2);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}

TEST(LocateTest, SingleRow) {
    std::vector<std::string> subStrings = {"o"};
    std::vector<std::string> strings = {"hello"};
    std::vector<int32_t> starts = {1};
    std::vector<int32_t> expected = {5};
    BaseVector* subVec = LocateFunctionTestHelper::CreateStringVector(subStrings);
    BaseVector* strVec = LocateFunctionTestHelper::CreateStringVector(strings);
    BaseVector* startVec = LocateFunctionTestHelper::CreateNumericVector(starts, OMNI_INT);
    BaseVector* result = nullptr;
    LocateFunctionTestHelper::ExecuteLocate(subVec, strVec, startVec, OMNI_INT, result);
    LocateFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete subVec;
    delete strVec;
    delete startVec;
    delete result;
}
