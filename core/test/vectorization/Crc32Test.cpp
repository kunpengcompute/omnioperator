/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
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

class Crc32TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const crc32_test_env =
    ::testing::AddGlobalTestEnvironment(new Crc32TestEnvironment);

class Crc32FunctionTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int64_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr);
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            EXPECT_EQ(resultVec->GetValue(i), expected[i]) << "Row " << i;
        }
    }

    static BaseVector* CreateBinaryVector(const std::vector<std::string>& values) {
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

    static void ExecuteCrc32(BaseVector* inputVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARBINARY };
        auto sig = std::make_shared<FunctionSignature>("crc32", inputTypeIds, OMNI_LONG);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(Crc32Test, BasicValues) {
    std::vector<std::string> binaries = {"DEAD_BEEF", "CRC32"};
    std::vector<int64_t> expected = {2634114297L, 4128576900L};
    BaseVector* binVec = Crc32FunctionTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    Crc32FunctionTestHelper::ExecuteCrc32(binVec, result);
    Crc32FunctionTestHelper::ValidateResult(result, expected, 2);
    delete binVec;
    delete result;
}

TEST(Crc32Test, LongString) {
    std::vector<std::string> binaries = {"velox is an open source unified execution engine."};
    std::vector<int64_t> expected = {2173230066L};
    BaseVector* binVec = Crc32FunctionTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    Crc32FunctionTestHelper::ExecuteCrc32(binVec, result);
    Crc32FunctionTestHelper::ValidateResult(result, expected, 1);
    delete binVec;
    delete result;
}

TEST(Crc32Test, EmptyInput) {
    std::vector<std::string> binaries = {""};
    std::vector<int64_t> expected = {0L};
    BaseVector* binVec = Crc32FunctionTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    Crc32FunctionTestHelper::ExecuteCrc32(binVec, result);
    Crc32FunctionTestHelper::ValidateResult(result, expected, 1);
    delete binVec;
    delete result;
}

TEST(Crc32Test, NullHandling) {
    std::vector<std::string> binaries = {"DEAD_BEEF", "CRC32"};
    BaseVector* binVec = Crc32FunctionTestHelper::CreateBinaryVector(binaries);
    binVec->SetNull(1);
    BaseVector* result = nullptr;
    Crc32FunctionTestHelper::ExecuteCrc32(binVec, result);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 2634114297L);
    delete binVec;
    delete result;
}

TEST(Crc32Test, MultipleRows) {
    std::vector<std::string> binaries = {"DEAD_BEEF", "CRC32", "velox is an open source unified execution engine.", ""};
    std::vector<int64_t> expected = {2634114297L, 4128576900L, 2173230066L, 0L};
    BaseVector* binVec = Crc32FunctionTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    Crc32FunctionTestHelper::ExecuteCrc32(binVec, result);
    Crc32FunctionTestHelper::ValidateResult(result, expected, 4);
    delete binVec;
    delete result;
}
