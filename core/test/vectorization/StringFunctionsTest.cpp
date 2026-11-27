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

TEST(StringFunctionTest, sha1) {
    std::vector<std::string> values = {
        "s00",
        "s01",
        "s02",
        "s03",
        "s04"
    };
    std::vector<bool> nulls = {false, false, false, false, false};
    std::vector<std::string> expectedResults = {
        "797b374c572a7b7819f2cee6b083825d30f55eb0",
        "df8539e2ad6a30763981a500a41daca7097ee02a",
        "d6b06d4c52e679a37a514b837cbc4df670a7b5a0",
        "1d347e57f7b68200a44be69b1f7f98df26baaeaf",
        "bb7833be129a2aed3af30e21177e1671df956c66"
    };
    std::vector<bool> expectedNulls = {false, false, false, false, false};
    std::string functionName = "sha1";
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_VARBINARY, values.size());
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < values.size(); ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        nulls[i] ? inputVector->SetNull(i) : inputVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_VARBINARY};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_VARCHAR);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName <<" not found ";

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(values.size());

    std::stack<BaseVector*> args;
    args.push(inputVector);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_VARCHAR);
    vectorFunction->Apply(args, resultType, result, &context);
    auto* resultVector = dynamic_cast<VarcharVector*>(result);
    ASSERT_NE(resultVector, nullptr);
    for (int32_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(resultVector->IsNull(i), expectedNulls[i]);
        if (!expectedNulls[i]) {
            std::string_view actual = resultVector->GetValue(i);
            std::string actualStr(actual);
            EXPECT_EQ(actualStr, expectedResults[i])
                << "Mismatch at index " << i << " for " << functionName
                << " input=" << values[i]
                << " expected=" << expectedResults[i];
        }
    }
    delete result;
}

TEST(StringFunctionTest, sha2) {
    std::vector<std::string> values = {
        "s00",
        "s01",
        "s02",
        "s03",
        "s04"
    };
    std::vector<bool> nulls = {false, false, false, false, false};
    std::vector<int32_t> values2 = {224, 224, 224, 224, 224};
    std::vector<bool> nulls2 = {false, false, false, false, false};
    std::vector<std::string> expectedResults = {
        "08b27d2bd594244ea01d154efe5ad424fbec892c40400efcddd0fc13",
        "ceb54d697a15e0f76456c1c3192492bf04b87f92608dd5e8c4fa1c50",
        "f747539f7faa78e55d99f4953b675cbd64162afe563229c3b92c895e",
        "1045272e5bb6128b0346e1c173fd1aaf908cd15625ceecd85a57f408",
        "787390611de01a2eba4a6514b633ac83c86288494de17ccc37d2582a"
    };
    std::vector<bool> expectedNulls = {false, false, false, false, false};
    std::string functionName = "sha2";
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_VARBINARY, values.size());
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < values.size(); ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        nulls[i] ? inputVector->SetNull(i) : inputVector->SetNotNull(i);
    }

    BaseVector* inputVec2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
    auto* inputVector2 = dynamic_cast<Vector<int32_t>*>(inputVec2);
    for (int32_t i = 0; i < values2.size(); ++i) {
        inputVector2->SetValue(i, values2[i]);
        nulls2[i] ? inputVector2->SetNull(i) : inputVector2->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_VARBINARY, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_VARCHAR);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName <<" not found ";

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(values.size());

    std::stack<BaseVector*> args;
    args.push(inputVector);
    args.push(inputVector2);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_VARCHAR);
    vectorFunction->Apply(args, resultType, result, &context);
    auto* resultVector = dynamic_cast<VarcharVector*>(result);
    ASSERT_NE(resultVector, nullptr);
    for (int32_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(resultVector->IsNull(i), expectedNulls[i]);
        if (!expectedNulls[i]) {
            std::string_view actual = resultVector->GetValue(i);
            std::string actualStr(actual);
            EXPECT_EQ(actualStr, expectedResults[i])
                << "Mismatch at index " << i << " for " << functionName
                << " input=" << values[i]
                << " expected=" << expectedResults[i];
        }
    }
    delete result;
}

TEST(StringFunctionTest, md5) {
    std::vector<std::string> values = {
        "s00",
        "s01",
        "s02",
        "s03",
        "s04"
    };
    std::vector<bool> nulls = {false, false, false, false, false};
    std::vector<std::string> expectedResults = {
        "27efe58ddcf13e333d1565e67d6f8cd5",
        "ca192fd06f1e612e7345929f1eefdd7e",
        "58916685a8f0d527523270aa6f05823f",
        "91d9cb1511f8ae08a151aca65bbe146f",
        "75c445789061555c21d0f0416f02e4d4"
    };
    std::vector<bool> expectedNulls = {false, false, false, false, false};
    std::string functionName = "Md5";
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_VARBINARY, values.size());
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < values.size(); ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        nulls[i] ? inputVector->SetNull(i) : inputVector->SetNotNull(i);
    }

    std::vector<DataTypeId> argTypes = {OMNI_VARBINARY};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, OMNI_VARCHAR);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName <<" not found ";

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(values.size());

    std::stack<BaseVector*> args;
    args.push(inputVector);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_VARCHAR);
    vectorFunction->Apply(args, resultType, result, &context);
    auto* resultVector = dynamic_cast<VarcharVector*>(result);
    ASSERT_NE(resultVector, nullptr);
    for (int32_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(resultVector->IsNull(i), expectedNulls[i]);
        if (!expectedNulls[i]) {
            std::string_view actual = resultVector->GetValue(i);
            std::string actualStr(actual);
            EXPECT_EQ(actualStr, expectedResults[i])
                << "Mismatch at index " << i << " for " << functionName
                << " input=" << values[i]
                << " expected=" << expectedResults[i];
        }
    }
    delete result;
}