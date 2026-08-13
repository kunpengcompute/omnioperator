/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Tests for vectorized MD5 and concat_ws fusion
 */

#include <gtest/gtest.h>

#include <stack>
#include <string>
#include <vector>

#include "codegen/func_signature.h"
#include "codegen/functions/md5.h"
#include "expression/expressions.h"
#include "test/util/test_util.h"
#include "type/data_type.h"
#include "type/string_Impl.h"
#include "util/config/QueryConfig.h"
#include "vector/vector_helper.h"
#include "vectorization/ExprEval.h"
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/FusedMd5ConcatWsFunction.h"
#include "vectorization/functions/Md5ConcatWsFusion.h"
#include "vectorization/functions/StreamingMd5Context.h"
#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5
#include "vectorization/functions/Md5VectorFunction.h"
#endif
#include "vectorization/registration/Register.h"

using namespace omniruntime;
using namespace omniruntime::codegen;
using namespace omniruntime::expressions;
using namespace omniruntime::op;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;

namespace {

using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

class Md5FusionTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment *const md5FusionTestEnvironment =
    ::testing::AddGlobalTestEnvironment(new Md5FusionTestEnvironment);

BaseVector *CreateStringInput(const std::vector<std::string> &values, const std::vector<bool> &nulls = {},
    DataTypeId typeId = OMNI_VARCHAR)
{
    auto *raw = VectorHelper::CreateFlatVector(typeId, static_cast<int32_t>(values.size()));
    auto *vector = static_cast<VarcharVector *>(raw);
    for (size_t row = 0; row < values.size(); ++row) {
        const auto rowIndex = static_cast<int32_t>(row);
        vector->SetValue(rowIndex, std::string_view(values[row]));
        if (!nulls.empty() && nulls[row]) {
            vector->SetNull(rowIndex);
        } else {
            vector->SetNotNull(rowIndex);
        }
    }
    return raw;
}

class CountingConcatWsFunction final : public VectorFunction {
public:
    explicit CountingConcatWsFunction(int32_t &applyCount) : applyCount_(applyCount) {}

    void Apply(std::stack<BaseVector *> &, const DataTypePtr &,
        BaseVector *&, ExecutionContext *) const override
    {
        ++applyCount_;
        FAIL() << "concat_ws must not be evaluated on the fused path";
    }

private:
    int32_t &applyCount_;
};

#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5
std::string ScalarMd5(const std::string &value)
{
    codegen::function::Md5Function scalar(value.data(), value.size());
    char digest[StreamingMd5Context::HEX_DIGEST_SIZE];
    scalar.FinishHex(digest);
    return std::string(digest, sizeof(digest));
}

void ExpectVectorMd5(const std::vector<std::string> &values, const std::vector<bool> &nulls)
{
    auto signature = std::make_shared<FunctionSignature>(
        "Md5", std::vector<DataTypeId> {OMNI_VARBINARY}, OMNI_VARCHAR);
    config::QueryConfig queryConfig;
    auto function = VectorFunction::Find(signature, queryConfig);
    ASSERT_NE(function, nullptr);
    ASSERT_NE(dynamic_cast<Md5VectorFunction *>(function.get()), nullptr);

    std::stack<BaseVector *> args;
    args.push(CreateStringInput(values, nulls, OMNI_VARBINARY));
    ExecutionContext executionContext;
    executionContext.SetResultRowSize(static_cast<int32_t>(values.size()));
    BaseVector *result = nullptr;
    function->Apply(args, std::make_shared<DataType>(OMNI_VARCHAR), result, &executionContext);

    auto *output = dynamic_cast<VarcharVector *>(result);
    ASSERT_NE(output, nullptr);
    for (size_t row = 0; row < values.size(); ++row) {
        if (!nulls.empty() && nulls[row]) {
            EXPECT_TRUE(output->IsNull(static_cast<int32_t>(row)));
        } else {
            EXPECT_FALSE(output->IsNull(static_cast<int32_t>(row)));
            EXPECT_EQ(std::string(output->GetValue(static_cast<int32_t>(row))), ScalarMd5(values[row]));
        }
    }
    delete result;
}
#endif

} // namespace

TEST(Md5FusionTest, StreamingContextUsesCommonHexEncoding)
{
    StreamingMd5Context context;
    context.Update("a", 1);
    context.Update("b", 1);
    context.Update("c", 1);
    uint8_t binaryDigest[StreamingMd5Context::BINARY_DIGEST_SIZE];
    context.Finish(binaryDigest);

    char hexDigest[StreamingMd5Context::HEX_DIGEST_SIZE];
    StreamingMd5Context::DigestToHex(binaryDigest, hexDigest);
    EXPECT_EQ(std::string(hexDigest, sizeof(hexDigest)), "900150983cd24fb0d6963f7d28e17f72");

    context.Reset();
    context.Update("", 0);
    context.FinishHex(hexDigest);
    EXPECT_EQ(std::string(hexDigest, sizeof(hexDigest)), "d41d8cd98f00b204e9800998ecf8427e");
}

TEST(Md5FusionTest, FusedFunctionPreservesConcatWsNullAndEmptySemantics)
{
    constexpr int32_t ROW_SIZE = 4;
    std::stack<BaseVector *> args;
    args.push(CreateStringInput({",", ",", ",", ","}, {false, false, false, true}));
    args.push(CreateStringInput({"a", "unused", "a", "unused"}, {false, true, false, false}));
    args.push(CreateStringInput({"unused", "unused", "", "unused"}, {true, true, false, false}));
    args.push(CreateStringInput({"b", "unused", "b", "unused"}, {false, true, false, false}));

    ExecutionContext executionContext;
    executionContext.SetResultRowSize(ROW_SIZE);
    FusedMd5ConcatWsFunction function(4);
    BaseVector *result = nullptr;
    function.Apply(args, std::make_shared<DataType>(OMNI_VARCHAR), result, &executionContext);

    auto *output = dynamic_cast<VarcharVector *>(result);
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(std::string(output->GetValue(0)), "b345e1dc09f20fdefdea469f09167892");
    EXPECT_EQ(std::string(output->GetValue(1)), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(std::string(output->GetValue(2)), "9ec2fb2c0f61fa8033289ce3710b4634");
    EXPECT_TRUE(output->IsNull(3));
    delete result;
}

TEST(Md5FusionTest, FusedFunctionHashesEmptyStringWhenOnlySeparatorIsPresent)
{
    std::stack<BaseVector *> args;
    args.push(CreateStringInput({","}));
    ExecutionContext executionContext;
    executionContext.SetResultRowSize(1);
    FusedMd5ConcatWsFunction function(1);
    BaseVector *result = nullptr;
    function.Apply(args, std::make_shared<DataType>(OMNI_VARCHAR), result, &executionContext);

    auto *output = dynamic_cast<VarcharVector *>(result);
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(std::string(output->GetValue(0)), "d41d8cd98f00b204e9800998ecf8427e");
    delete result;
}

TEST(Md5FusionTest, MatcherRejectsConcatWsWithoutVarbinaryCast)
{
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto *concatWs = new FuncExpr("concat_ws",
        {new FieldExpr(0, varcharType), new FieldExpr(1, varcharType)}, varcharType);
    auto *md5 = new FuncExpr("Md5", {concatWs}, varcharType);

    EXPECT_FALSE(Md5ConcatWsFusion::Match(*md5).has_value());
    delete md5;
}

TEST(Md5FusionTest, MatcherRejectsCastNullNode)
{
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto binaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    auto *concatWs = new FuncExpr("concat_ws",
        {new FieldExpr(0, varcharType), new FieldExpr(1, varcharType)}, varcharType);
    auto *cast = new FuncExpr("CAST_null", {concatWs}, binaryType);
    auto *md5 = new FuncExpr("Md5", {cast}, varcharType);

    EXPECT_FALSE(Md5ConcatWsFusion::Match(*md5).has_value());
    delete md5;
}

TEST(Md5FusionTest, MatcherRejectsArrayArguments)
{
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto binaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
    auto *concatWs = new FuncExpr("concat_ws",
        {new FieldExpr(0, varcharType), new FieldExpr(1, arrayType)}, varcharType);
    auto *cast = new FuncExpr("CAST", {concatWs}, binaryType);
    auto *md5 = new FuncExpr("Md5", {cast}, varcharType);

    EXPECT_FALSE(Md5ConcatWsFusion::Match(*md5).has_value());
    delete md5;
}

TEST(Md5FusionTest, ExprEvalBypassesConcatWsMaterialization)
{
    constexpr int32_t ROW_SIZE = 1;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto binaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    auto *concatWs = new FuncExpr("concat_ws", {
        new FieldExpr(0, varcharType), new FieldExpr(1, varcharType), new FieldExpr(2, varcharType)}, varcharType);
    auto *cast = new FuncExpr("CAST", {concatWs}, binaryType);
    auto *md5 = new FuncExpr("Md5", {cast}, varcharType);
    int32_t concatWsApplyCount = 0;
    concatWs->vectorFunction = std::make_shared<CountingConcatWsFunction>(concatWsApplyCount);

    std::string separator[ROW_SIZE] = {","};
    std::string value1[ROW_SIZE] = {"a"};
    std::string value2[ROW_SIZE] = {"b"};
    std::vector<DataTypePtr> inputTypeList {varcharType, varcharType, varcharType};
    DataTypes inputTypes(inputTypeList);
    VectorBatch *input = CreateVectorBatch(inputTypes, ROW_SIZE, separator, value1, value2);

    ExecutionContext executionContext;
    executionContext.SetResultRowSize(ROW_SIZE);
    ExprEval evaluator(input, &executionContext);
    evaluator.Visit(*md5);
    BaseVector *result = evaluator.GetResult();

    auto *output = dynamic_cast<VarcharVector *>(result);
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(concatWsApplyCount, 0);
    EXPECT_EQ(std::string(output->GetValue(0)), "b345e1dc09f20fdefdea469f09167892");

    delete result;
    delete input;
    delete md5;
}

#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5
TEST(Md5FusionTest, ScalarBucketFallbackCommitsRowsInInputOrder)
{
    const std::vector<std::string> values {
        std::string(80, 'a'), "unused", "short", std::string(90, 'b'), "tail"};
    const std::vector<bool> nulls {false, true, false, false, false};

    // The two long rows are below the multi-buffer threshold. Before the fix,
    // appending their bucket after short rows changed the LargeStringContainer write order.
    ExpectVectorMd5(values, nulls);
}

TEST(Md5FusionTest, MixedScalarAndMultiBufferRowsCommitInInputOrder)
{
    std::vector<std::string> values;
    std::vector<bool> nulls;
    for (int32_t row = 0; row < 20; ++row) {
        if ((row % 2) == 0) {
            values.push_back(std::string(128 + row, static_cast<char>('a' + row)));
        } else {
            values.push_back("short-" + std::to_string(row));
        }
        nulls.push_back(row == 3 || row == 17);
    }

    // Ten long rows keep one ISA-L bucket active, while short and NULL rows exercise
    // the common row-indexed digest buffer and final sequential commit.
    ExpectVectorMd5(values, nulls);
}
#endif
