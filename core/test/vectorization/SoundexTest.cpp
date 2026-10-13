/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for soundex string function
 */

#include <gtest/gtest.h>
#include <string>
#include <string_view>
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

class SoundexTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }
};

TEST_F(SoundexTest, BasicAndEdgeCases)
{
    std::vector<std::string> input = {
        "Miller", "Robert", "Rupert", "Ashcraft", "Tymczak", "!!", "", "测试", "Pfister", "ZIN"};
    constexpr int32_t rowSize = 10;

    BaseVector *inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(inputVec);
    ASSERT_NE(strVec, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        std::string_view sv(input[i]);
        strVec->SetValue(i, sv);
    }
    inputVec->SetNull(8);

    auto signature = std::make_shared<FunctionSignature>(
        "soundex", std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    BaseVector *resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto *outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultVector);
    ASSERT_NE(outStrVec, nullptr);

    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "M460");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "R163");
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), "R163");
    EXPECT_EQ(std::string(outStrVec->GetValue(3)), "A261");
    EXPECT_EQ(std::string(outStrVec->GetValue(4)), "T522");
    EXPECT_EQ(std::string(outStrVec->GetValue(5)), "!!");
    EXPECT_EQ(std::string(outStrVec->GetValue(6)), "");
    EXPECT_EQ(std::string(outStrVec->GetValue(7)), "测试");
    EXPECT_TRUE(outStrVec->IsNull(8));
    EXPECT_EQ(std::string(outStrVec->GetValue(9)), "Z500");

    delete resultVector;
    delete inputVec;
}
