/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: vectorization expression 'in' test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/decimal_operations.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "expression/parserhelper.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

TEST(HashFunctionTest, hash) {
    std::vector<std::string> values = {
        "s00",
        "s10",
        "s20",
        "s30",
        "s40"
    };
    std::vector<bool> nulls = {false, false, false, false, false};
    std::vector<int32_t> expectedResults = {
        831442313,
        -1630222375,
        945460688,
        -1889759045,
        1856475565
    };
    std::vector<bool> expectedNulls = {false, false, false, false, false};
    std::string functionName = "mm3hash";
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_VARCHAR, values.size());
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < values.size(); ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        nulls[i] ? inputVector->SetNull(i) : inputVector->SetNotNull(i);
    }

    BaseVector* inputVec2 = new ConstVector<int32_t>(42, OMNI_INT);

    std::vector<DataTypeId> argTypes = {OMNI_VARCHAR, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_INT);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName <<" not found ";

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(values.size());

    std::stack<BaseVector*> args;
    args.push(inputVector);
    args.push(inputVec2);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    vectorFunction->Apply(args, resultType, result, &context);
    auto* resultVector = dynamic_cast<Vector<int32_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    for (int32_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(resultVector->IsNull(i), expectedNulls[i]);
        if (!expectedNulls[i]) {
            int32_t actual = resultVector->GetValue(i);
            EXPECT_EQ(actual, expectedResults[i])
                << "Mismatch at index " << i << " for " << functionName
                << " input=" << values[i]
                << " expected=" << expectedResults[i];
        }
    }
    delete result;
}

TEST(HashFunctionTest, xxhash64) {
    std::vector<std::string> values = {
        "s00",
        "s10",
        "s20",
        "s30",
        "s40"
    };
    std::vector<bool> nulls = {false, false, false, false, false};
    std::vector<int64_t> expectedResults = {
        837240679467877550,
        -1285367105249354636,
        -2124644269910718099,
        -3985832445560234674,
        -53604285750853315
    };
    std::vector<bool> expectedNulls = {false, false, false, false, false};
    std::string functionName = "xxhash64";
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_VARCHAR, values.size());
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < values.size(); ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        nulls[i] ? inputVector->SetNull(i) : inputVector->SetNotNull(i);
    }

    BaseVector* inputVec2 = new ConstVector<int64_t>(42, OMNI_LONG);

    std::vector<DataTypeId> argTypes = {OMNI_VARCHAR, OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_LONG);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName <<" not found ";

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(values.size());

    std::stack<BaseVector*> args;
    args.push(inputVector);
    args.push(inputVec2);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, result, &context);
    auto* resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    for (int32_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(resultVector->IsNull(i), expectedNulls[i]);
        if (!expectedNulls[i]) {
            int64_t actual = resultVector->GetValue(i);
            EXPECT_EQ(actual, expectedResults[i])
                << "Mismatch at index " << i << " for " << functionName
                << " input=" << values[i]
                << " expected=" << expectedResults[i];
        }
    }
    delete result;
}