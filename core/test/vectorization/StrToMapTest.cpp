/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class StrToMapTestHelper {
public:
    static void ExecuteAndValidate(
        const std::vector<std::string> &inputStrs,
        const std::string &entryDelimiter,
        const std::string &kvDelimiter,
        const std::vector<std::vector<std::pair<std::string, std::optional<std::string>>>> &expected)
    {
        int32_t rowSize = static_cast<int32_t>(inputStrs.size());

        std::unique_ptr<BaseVector> inputVecGuard(VectorHelper::CreateStringVector(rowSize));
        auto *inputVec = inputVecGuard.get();
        auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(inputVec);
        for (int32_t i = 0; i < rowSize; i++) {
            std::string_view sv(inputStrs[i]);
            inputVector->SetValue(i, sv);
        }

        std::unique_ptr<ConstVector<std::string_view>> entryDelimVec(
            new ConstVector<std::string_view>(std::string_view(entryDelimiter), OMNI_VARCHAR, rowSize));
        std::unique_ptr<ConstVector<std::string_view>> kvDelimVec(
            new ConstVector<std::string_view>(std::string_view(kvDelimiter), OMNI_VARCHAR, rowSize));

        auto signature = std::make_shared<codegen::FunctionSignature>("str_to_map",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR},
            OMNI_MAP);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr);

        BaseVector *resultVector = nullptr;
        auto mapType = std::make_shared<DataType>(OMNI_MAP);

        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        // caller-owned, do not let Apply delete
        inputVec->SetIsField(true);
        entryDelimVec->SetIsField(true);
        kvDelimVec->SetIsField(true);
        args.push(inputVec);
        args.push(entryDelimVec.get());
        args.push(kvDelimVec.get());

        function->Apply(args, mapType, resultVector, &context);
        ASSERT_NE(resultVector, nullptr);

        auto *mapVec = dynamic_cast<MapVector *>(resultVector);
        ASSERT_NE(mapVec, nullptr);

        auto *keyVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(
            mapVec->GetKeyVector().get());
        auto *valueVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(
            mapVec->GetValueVector().get());
        ASSERT_NE(keyVec, nullptr);
        ASSERT_NE(valueVec, nullptr);

        for (int32_t row = 0; row < rowSize; ++row) {
            int64_t startIdx = mapVec->GetOffset(row);
            int64_t mapSize = mapVec->GetOffset(row + 1) - startIdx;
            ASSERT_EQ(mapSize, static_cast<int64_t>(expected[row].size()));

            for (int64_t j = 0; j < mapSize; ++j) {
                int64_t idx = startIdx + j;
                std::string_view actualKey = keyVec->GetValue(idx);
                ASSERT_EQ(std::string(actualKey), expected[row][j].first);

                if (expected[row][j].second.has_value()) {
                    ASSERT_FALSE(valueVec->IsNull(idx));
                    std::string_view actualValue = valueVec->GetValue(idx);
                    ASSERT_EQ(std::string(actualValue), expected[row][j].second.value());
                } else {
                    ASSERT_TRUE(valueVec->IsNull(idx));
                }
            }
        }

        delete resultVector;
    }
};

TEST(StrToMapTest, BasicTest)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a:1,b:2,c:3"},
        ",", ":",
        {{{"a", "1"}, {"b", "2"}, {"c", "3"}}});
}

TEST(StrToMapTest, ValueWithSpace)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a: ,b:2"},
        ",", ":",
        {{{"a", " "}, {"b", "2"}}});
}

TEST(StrToMapTest, EmptyValue)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a:,b:2"},
        ",", ":",
        {{{"a", ""}, {"b", "2"}}});
}

TEST(StrToMapTest, EmptyInput)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {""},
        ",", ":",
        {{{"", std::nullopt}}});
}

TEST(StrToMapTest, NoKeyValueDelimiter)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a"},
        ",", ":",
        {{{"a", std::nullopt}}});
}

TEST(StrToMapTest, CustomDelimiters)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a=1,b=2,c=3"},
        ",", "=",
        {{{"a", "1"}, {"b", "2"}, {"c", "3"}}});
}

TEST(StrToMapTest, UnderscoreEntryDelimiter)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a:1_b:2_c:3"},
        "_", ":",
        {{{"a", "1"}, {"b", "2"}, {"c", "3"}}});
}

TEST(StrToMapTest, SameDelimiters)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a:1,b:2,c:3"},
        ",", ",",
        {{{"a:1", std::nullopt}, {"b:2", std::nullopt}, {"c:3", std::nullopt}}});
}

TEST(StrToMapTest, MultipleRows)
{
    StrToMapTestHelper::ExecuteAndValidate(
        {"a:1,b:2", "x:10,y:20,z:30"},
        ",", ":",
        {{{"a", "1"}, {"b", "2"}},
         {{"x", "10"}, {"y", "20"}, {"z", "30"}}});
}

TEST(StrToMapTest, DuplicateKeysThrows)
{
    ASSERT_THROW(
        StrToMapTestHelper::ExecuteAndValidate(
            {"a:1,b:2,a:3"},
            ",", ":",
            {{{"a", "1"}, {"b", "2"}, {"a", "3"}}}),
        omniruntime::exception::OmniException);
}

TEST(StrToMapTest, EmptyEntryDelimiterThrows)
{
    ASSERT_THROW(
        StrToMapTestHelper::ExecuteAndValidate(
            {"a:1,b:2"},
            "", ":",
            {{{"a", "1"}, {"b", "2"}}}),
        omniruntime::exception::OmniException);
}

TEST(StrToMapTest, EmptyKvDelimiterThrows)
{
    ASSERT_THROW(
        StrToMapTestHelper::ExecuteAndValidate(
            {"a:1,b:2"},
            ",", "",
            {{{"a", "1"}, {"b", "2"}}}),
        omniruntime::exception::OmniException);
}

TEST(StrToMapTest, LongEntryDelimiterThrows)
{
    ASSERT_THROW(
        StrToMapTestHelper::ExecuteAndValidate(
            {"a:1,b:2"},
            ";;", ":",
            {{{"a", "1"}, {"b", "2"}}}),
        omniruntime::exception::OmniException);
}

TEST(StrToMapTest, LongKvDelimiterThrows)
{
    ASSERT_THROW(
        StrToMapTestHelper::ExecuteAndValidate(
            {"a:1,b:2"},
            ",", "::",
            {{{"a", "1"}, {"b", "2"}}}),
        omniruntime::exception::OmniException);
}
