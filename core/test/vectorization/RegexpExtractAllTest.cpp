/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: RegexpExtractAll test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/RegexpExtractAll.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/array_vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class RegexpExtractAllTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const regexp_extract_all_test_env =
    ::testing::AddGlobalTestEnvironment(new RegexpExtractAllTestEnvironment);

class RegexpExtractAllTestHelper {
public:
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, std::string_view(values[i]));
        }
        return vec;
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

    static void ValidateArrayResult(BaseVector* result, const std::vector<std::vector<std::string>>& expected,
                                    int rowSize) {
        auto* arrayResult = dynamic_cast<ArrayVector*>(result);
        ASSERT_NE(arrayResult, nullptr) << "Result vector is not ArrayVector";
        auto elementVector = arrayResult->GetElementVector();
        ASSERT_NE(elementVector.get(), nullptr);
        auto* strElemVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(elementVector.get());
        ASSERT_NE(strElemVec, nullptr) << "Element vector is not string type";

        for (int row = 0; row < rowSize; ++row) {
            if (arrayResult->IsNull(row)) {
                continue;
            }
            int64_t startOffset = arrayResult->GetOffset(row);
            int64_t arraySize = arrayResult->GetSize(row);
            const auto& expRow = expected[row];
            EXPECT_EQ(static_cast<size_t>(arraySize), expRow.size()) << "Row " << row << " array size mismatch";
            for (int64_t k = 0; k < arraySize && k < static_cast<int64_t>(expRow.size()); ++k) {
                int64_t globalIdx = startOffset + k;
                std::string_view actual = strElemVec->GetValue(globalIdx);
                EXPECT_EQ(actual, expRow[k]) << "Row " << row << " element " << k << " mismatch";
            }
        }
    }

    static void ExecuteRegexpExtractAllTwoArgs(BaseVector* strVec, BaseVector* patternVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR, OMNI_VARCHAR};
        auto signature = std::make_shared<FunctionSignature>("regexp_extract_all", inputTypeIds, OMNI_ARRAY);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "regexp_extract_all (2 args) not found";
        auto outputType = std::make_shared<DataType>(OMNI_ARRAY);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        strVec->SetIsField(true);  // caller-owned, do not let Apply delete
        patternVec->SetIsField(true);
        args.push(strVec);
        args.push(patternVec);
        function->Apply(args, outputType, result, &context);
    }

    static void ExecuteRegexpExtractAllThreeArgs(BaseVector* strVec, BaseVector* patternVec, BaseVector* groupVec,
                                                 BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT};
        auto signature = std::make_shared<FunctionSignature>("regexp_extract_all", inputTypeIds, OMNI_ARRAY);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "regexp_extract_all (3 args) not found";
        auto outputType = std::make_shared<DataType>(OMNI_ARRAY);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        strVec->SetIsField(true);  // caller-owned, do not let Apply delete
        patternVec->SetIsField(true);
        args.push(strVec);
        args.push(patternVec);
        groupVec->SetIsField(true);
        args.push(groupVec);
        function->Apply(args, outputType, result, &context);
    }

    static void ExecuteRegexpExtractAllThreeArgsLong(BaseVector* strVec, BaseVector* patternVec, BaseVector* groupVec,
                                                     BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_LONG};
        auto signature = std::make_shared<FunctionSignature>("regexp_extract_all", inputTypeIds, OMNI_ARRAY);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "regexp_extract_all (3 args LONG) not found";
        auto outputType = std::make_shared<DataType>(OMNI_ARRAY);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        strVec->SetIsField(true);  // caller-owned, do not let Apply delete
        patternVec->SetIsField(true);
        args.push(strVec);
        args.push(patternVec);
        groupVec->SetIsField(true);
        args.push(groupVec);
        function->Apply(args, outputType, result, &context);
    }
};

TEST(RegexpExtractAllTest, SimplePatternTwoArgs) {
    std::vector<std::string> strValues = {"1a 2b 14m", "x1y2z3"};
    std::vector<std::string> patterns = {"\\d+", "\\d+"};
    std::vector<std::vector<std::string>> expected = {{"1", "2", "14"}, {"1", "2", "3"}};

    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllTwoArgs(strVec, patternVec, result);
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 2);

    delete strVec;
    delete patternVec;
    delete result;
}

TEST(RegexpExtractAllTest, WithGroupIndexThreeArgs) {
    std::vector<std::string> strValues = {"1a 2b 14m", "1a 2b 14m"};
    std::vector<std::string> patterns = {"(\\d+)([a-z]+)", "(\\d+)([a-z]+)"};
    std::vector<int32_t> groupIndices = {1, 2};
    std::vector<std::vector<std::string>> expected = {{"1", "2", "14"}, {"a", "b", "m"}};

    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* groupVec = RegexpExtractAllTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllThreeArgs(strVec, patternVec, groupVec, result);
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 2);

    delete strVec;
    delete patternVec;
    delete groupVec;
    delete result;
}

TEST(RegexpExtractAllTest, WithGroupIndexLong) {
    std::vector<std::string> strValues = {"1a 2b 14m"};
    std::vector<std::string> patterns = {"(\\d+)([a-z]+)"};
    std::vector<int64_t> groupIndices = {2};
    std::vector<std::vector<std::string>> expected = {{"a", "b", "m"}};

    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* groupVec = RegexpExtractAllTestHelper::CreateNumericVector(groupIndices, OMNI_LONG);
    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllThreeArgsLong(strVec, patternVec, groupVec, result);
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 1);

    delete strVec;
    delete patternVec;
    delete groupVec;
    delete result;
}

TEST(RegexpExtractAllTest, NoMatchReturnsEmptyArray) {
    std::vector<std::string> strValues = {"hello", "world"};
    std::vector<std::string> patterns = {"\\d+", "\\d+"};
    std::vector<std::vector<std::string>> expected = {{}, {}};

    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllTwoArgs(strVec, patternVec, result);
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 2);

    delete strVec;
    delete patternVec;
    delete result;
}

TEST(RegexpExtractAllTest, NullStringInput) {
    std::vector<std::string> strValues = {"1a 2b", "x", "3z"};
    std::vector<std::string> patterns = {"\\d+", "\\d+", "\\d+"};
    std::vector<std::vector<std::string>> expected = {{"1", "2"}, {}, {"3"}};
    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    strVec->SetNull(1);

    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllTwoArgs(strVec, patternVec, result);
    auto* arr = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(arr, nullptr);
    EXPECT_FALSE(arr->IsNull(0));
    EXPECT_TRUE(arr->IsNull(1));
    EXPECT_FALSE(arr->IsNull(2));
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 3);

    delete strVec;
    delete patternVec;
    delete result;
}

TEST(RegexpExtractAllTest, NullPatternInput) {
    std::vector<std::string> strValues = {"1a 2b", "x", "3z"};
    std::vector<std::string> patterns = {"\\d+", "\\d+", "\\d+"};
    std::vector<std::vector<std::string>> expected = {{"1", "2"}, {}, {"3"}};
    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    patternVec->SetNull(1);

    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllTwoArgs(strVec, patternVec, result);
    auto* arr = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(arr, nullptr);
    EXPECT_FALSE(arr->IsNull(0));
    EXPECT_TRUE(arr->IsNull(1));
    EXPECT_FALSE(arr->IsNull(2));
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 3);

    delete strVec;
    delete patternVec;
    delete result;
}

TEST(RegexpExtractAllTest, NullGroupIndex) {
    std::vector<std::string> strValues = {"1a 2b", "3c"};
    std::vector<std::string> patterns = {"\\d+", "\\d+"};
    std::vector<int32_t> groupIndices = {0, 0};
    std::vector<std::vector<std::string>> expected = {{"1", "2"}, {}};
    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* groupVec = RegexpExtractAllTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    groupVec->SetNull(1);

    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllThreeArgs(strVec, patternVec, groupVec, result);
    auto* arr = dynamic_cast<ArrayVector*>(result);
    ASSERT_NE(arr, nullptr);
    EXPECT_FALSE(arr->IsNull(0));
    EXPECT_TRUE(arr->IsNull(1));
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 2);

    delete strVec;
    delete patternVec;
    delete groupVec;
    delete result;
}

TEST(RegexpExtractAllTest, GroupZeroFullMatch) {
    std::vector<std::string> strValues = {"1a 2b 14m"};
    std::vector<std::string> patterns = {"\\d+"};
    std::vector<int32_t> groupIndices = {0};
    std::vector<std::vector<std::string>> expected = {{"1", "2", "14"}};

    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* groupVec = RegexpExtractAllTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllThreeArgs(strVec, patternVec, groupVec, result);
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 1);

    delete strVec;
    delete patternVec;
    delete groupVec;
    delete result;
}

TEST(RegexpExtractAllTest, InvalidPatternThrows) {
    std::vector<std::string> strValues = {"hello"};
    std::vector<std::string> patterns = {"["};
    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* result = nullptr;

    EXPECT_THROW(
        RegexpExtractAllTestHelper::ExecuteRegexpExtractAllTwoArgs(strVec, patternVec, result),
        omniruntime::exception::OmniException);

    delete strVec;
    delete patternVec;
    delete result;
}

TEST(RegexpExtractAllTest, SingleMatch) {
    std::vector<std::string> strValues = {"abc123def"};
    std::vector<std::string> patterns = {"\\d+"};
    std::vector<std::vector<std::string>> expected = {{"123"}};

    BaseVector* strVec = RegexpExtractAllTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractAllTestHelper::CreateStringVector(patterns);
    BaseVector* result = nullptr;
    RegexpExtractAllTestHelper::ExecuteRegexpExtractAllTwoArgs(strVec, patternVec, result);
    RegexpExtractAllTestHelper::ValidateArrayResult(result, expected, 1);

    delete strVec;
    delete patternVec;
    delete result;
}
