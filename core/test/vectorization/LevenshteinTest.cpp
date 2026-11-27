/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for levenshtein distance function
 */

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "codegen/func_signature.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

class LevenshteinTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }

    void TestLevenshtein2Args(const std::vector<std::string> &lefts,
        const std::vector<std::string> &rights,
        const std::vector<int32_t> &expected)
    {
        int rowSize = static_cast<int>(lefts.size());
        BaseVector *leftVec = VectorHelper::CreateStringVector(rowSize);
        leftVec->SetIsField(true);
        auto *leftStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(leftVec);
        ASSERT_NE(leftStrVec, nullptr);

        BaseVector *rightVec = VectorHelper::CreateStringVector(rowSize);
        rightVec->SetIsField(true);
        auto *rightStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(rightVec);
        ASSERT_NE(rightStrVec, nullptr);

        std::vector<std::string_view> leftSvs(rowSize);
        std::vector<std::string_view> rightSvs(rowSize);
        for (int i = 0; i < rowSize; i++) {
            leftSvs[i] = std::string_view(lefts[i]);
            rightSvs[i] = std::string_view(rights[i]);
            leftStrVec->SetValue(i, leftSvs[i]);
            rightStrVec->SetValue(i, rightSvs[i]);
        }

        auto signature = std::make_shared<FunctionSignature>("levenshtein",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr);

        BaseVector *resultVector = nullptr;
        auto intType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        args.push(rightVec);
        args.push(leftVec);

        ASSERT_NO_THROW(function->Apply(args, intType, resultVector, &context));

        auto *intVec = dynamic_cast<Vector<int32_t> *>(resultVector);
        ASSERT_NE(intVec, nullptr);
        for (int i = 0; i < rowSize; i++) {
            EXPECT_EQ(intVec->GetValue(i), expected[i])
                << "Mismatch at row " << i << ": levenshtein(\"" << lefts[i] << "\", \"" << rights[i] << "\")";
        }

        delete resultVector;
        delete leftVec;
        delete rightVec;
    }

    void TestLevenshtein3Args(const std::vector<std::string> &lefts,
        const std::vector<std::string> &rights,
        const std::vector<int32_t> &thresholds,
        const std::vector<int32_t> &expected)
    {
        int rowSize = static_cast<int>(lefts.size());
        BaseVector *leftVec = VectorHelper::CreateStringVector(rowSize);
        leftVec->SetIsField(true);
        auto *leftStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(leftVec);
        ASSERT_NE(leftStrVec, nullptr);

        BaseVector *rightVec = VectorHelper::CreateStringVector(rowSize);
        rightVec->SetIsField(true);
        auto *rightStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(rightVec);
        ASSERT_NE(rightStrVec, nullptr);

        BaseVector *thresholdVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
        thresholdVec->SetIsField(true);
        auto *threshIntVec = dynamic_cast<Vector<int32_t> *>(thresholdVec);
        ASSERT_NE(threshIntVec, nullptr);

        std::vector<std::string_view> leftSvs(rowSize);
        std::vector<std::string_view> rightSvs(rowSize);
        for (int i = 0; i < rowSize; i++) {
            leftSvs[i] = std::string_view(lefts[i]);
            rightSvs[i] = std::string_view(rights[i]);
            leftStrVec->SetValue(i, leftSvs[i]);
            rightStrVec->SetValue(i, rightSvs[i]);
            threshIntVec->SetValue(i, thresholds[i]);
        }

        auto signature = std::make_shared<FunctionSignature>("levenshtein",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr);

        BaseVector *resultVector = nullptr;
        auto intType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        args.push(leftVec);
        args.push(rightVec);
        args.push(thresholdVec);

        ASSERT_NO_THROW(function->Apply(args, intType, resultVector, &context));

        auto *intVec = dynamic_cast<Vector<int32_t> *>(resultVector);
        ASSERT_NE(intVec, nullptr);
        for (int i = 0; i < rowSize; i++) {
            EXPECT_EQ(intVec->GetValue(i), expected[i])
                << "Mismatch at row " << i << ": levenshtein(\"" << lefts[i]
                << "\", \"" << rights[i] << "\", " << thresholds[i] << ")";
        }

        delete resultVector;
        delete leftVec;
        delete rightVec;
        delete thresholdVec;
    }
};

TEST_F(LevenshteinTest, IdenticalStrings) {
    TestLevenshtein2Args(
        {"abc", "hello", ""},
        {"abc", "hello", ""},
        {0, 0, 0});
}

TEST_F(LevenshteinTest, BasicEdits) {
    TestLevenshtein2Args(
        {"kitten", "frog", "hello", "hello"},
        {"sitting", "fog", "olleh", "hello world"},
        {3, 1, 4, 6});
}

TEST_F(LevenshteinTest, EmptyStrings) {
    TestLevenshtein2Args(
        {"", "hello", "hello"},
        {"hello", "", ""},
        {5, 5, 5});
}

TEST_F(LevenshteinTest, SubstitutionAndDeletion) {
    TestLevenshtein2Args(
        {"hello world", "hello world", "hello word"},
        {"hel wold", "hellq wodld", "dello world"},
        {3, 2, 2});
}

TEST_F(LevenshteinTest, WhitespacePreserved) {
    TestLevenshtein2Args(
        {"  facebook  "},
        {"  facebook  "},
        {0});
}

TEST_F(LevenshteinTest, UnicodeStrings) {
    TestLevenshtein2Args(
        {"\xE5\x8D\x83\xE4\xB8\x96",
         "\xE4\xB8\x96\xE7\x95\x8C\xE5\x8D\x83\xE4\xB8\x96"},
        {"fog",
         "\xE5\xA4\xA7\x61\xE7\x95\x8C\x62"},
        {3, 4});
}

TEST_F(LevenshteinTest, ThresholdExceeded) {
    TestLevenshtein3Args(
        {"kitten"},
        {"sitting"},
        {2},
        {-1});
}

TEST_F(LevenshteinTest, ThresholdBothEmpty) {
    TestLevenshtein3Args(
        {""},
        {""},
        {0},
        {0});
}

TEST_F(LevenshteinTest, ThresholdEmptyVsNonEmpty) {
    TestLevenshtein3Args(
        {"aaapppp", "aaapppp"},
        {"", ""},
        {7, 6},
        {7, -1});
}

TEST_F(LevenshteinTest, ThresholdElephantHippo) {
    TestLevenshtein3Args(
        {"elephant", "elephant", "hippo", "hippo"},
        {"hippo", "hippo", "elephant", "elephant"},
        {7, 6, 7, 6},
        {7, -1, 7, -1});
}

TEST_F(LevenshteinTest, ThresholdSingleChar) {
    TestLevenshtein3Args(
        {"b", "a", "aa", "aa"},
        {"a", "b", "aa", "aa"},
        {0, 0, 0, 2},
        {-1, -1, 0, 0});
}

TEST_F(LevenshteinTest, ThresholdAllDifferent) {
    TestLevenshtein3Args(
        {"aaa", "aaa"},
        {"bbb", "bbb"},
        {2, 3},
        {-1, 3});
}

TEST_F(LevenshteinTest, ThresholdLengthDifference) {
    TestLevenshtein3Args(
        {"aaaaaa", "aaapppp", "a", "aaapppp", "a"},
        {"b", "b", "bbb", "b", "bbb"},
        {10, 8, 4, 7, 3},
        {6, 7, 3, 7, 3});
}

TEST_F(LevenshteinTest, ThresholdSmallValues) {
    TestLevenshtein3Args(
        {"a", "bbb", "aaapppp", "a", "bbb"},
        {"bbb", "a", "b", "bbb", "a"},
        {2, 2, 6, 1, 1},
        {-1, -1, -1, -1, -1});
}

TEST_F(LevenshteinTest, ThresholdNumericStrings) {
    TestLevenshtein3Args(
        {"12345", "1234567"},
        {"1234567", "12345"},
        {1, 1},
        {-1, -1});
}

TEST_F(LevenshteinTest, ThresholdUnicode) {
    TestLevenshtein3Args(
        {"\xE5\x8D\x83\xE4\xB8\x96",
         "\xE5\x8D\x83\xE4\xB8\x96",
         "\xE4\xB8\x96\xE7\x95\x8C\xE5\x8D\x83\xE4\xB8\x96",
         "\xE4\xB8\x96\xE7\x95\x8C\xE5\x8D\x83\xE4\xB8\x96"},
        {"fog", "fog",
         "\xE5\xA4\xA7\x61\xE7\x95\x8C\x62",
         "\xE5\xA4\xA7\x61\xE7\x95\x8C\x62"},
        {3, 2, 4, 3},
        {3, -1, 4, -1});
}

TEST_F(LevenshteinTest, FunctionRegistered) {
    auto signature2 = std::make_shared<FunctionSignature>("levenshtein",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);
    EXPECT_NE(VectorFunction::Find(signature2), nullptr);

    auto signature3 = std::make_shared<FunctionSignature>("levenshtein",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
    EXPECT_NE(VectorFunction::Find(signature3), nullptr);

    auto signatureLong = std::make_shared<FunctionSignature>("levenshtein",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR, OMNI_LONG}, OMNI_INT);
    EXPECT_NE(VectorFunction::Find(signatureLong), nullptr);
}
