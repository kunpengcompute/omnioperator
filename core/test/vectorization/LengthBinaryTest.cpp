/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: length(binary) function unit tests
 *   length(binary) -> int32
 *   Returns the byte length of the binary input. Empty binary returns 0.
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

class LengthBinaryTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const length_binary_test_env =
    ::testing::AddGlobalTestEnvironment(new LengthBinaryTestEnvironment);

class LengthBinaryTestHelper {
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

    static void ExecuteLength(BaseVector* binaryVec, DataTypeId outputTypeId, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARBINARY };
        auto sig = std::make_shared<FunctionSignature>("length", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "length function not found for VARBINARY input";
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(binaryVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(binaryVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

// ========== length(binary) Tests ==========

TEST(LengthBinaryTest, EmptyBinaryReturnsZero) {
    std::vector<std::string> binaries = {""};
    std::vector<int32_t> expected = {0};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 1);
    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, SingleByte) {
    std::vector<std::string> binaries = {
        std::string("\x00", 1),
        std::string("\x41", 1),
        std::string("\xFF", 1)
    };
    std::vector<int32_t> expected = {1, 1, 1};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 3);
    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, MultipleBytes) {
    std::vector<std::string> binaries = {
        std::string("\x01\x02", 2),
        std::string("\xFF\xFF\xFF\xFF", 4),
        std::string("\x00\x01\x02\x03\x04\x05\x06\x07", 8)
    };
    std::vector<int32_t> expected = {2, 4, 8};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 3);
    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, NullHandling) {
    std::vector<std::string> binaries = {
        std::string("\x01", 1),
        std::string("\x02\x03", 2),
        std::string("\x04\x05\x06", 3)
    };
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    binVec->SetNull(1);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    EXPECT_FALSE(result->IsNull(2));

    auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 1);
    EXPECT_EQ(resultVec->GetValue(2), 3);

    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, BinaryVsStringSemanticsDiffer) {
    // UTF-8 encoded "中" is 3 bytes; length(string) returns 1 (char count),
    // but length(binary) must return 3 (byte count).
    std::string utf8Chinese = "中";  // 3 bytes in UTF-8
    std::vector<std::string> binaries = {utf8Chinese};
    std::vector<int32_t> expected = {3};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 1);
    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, MixedLengths) {
    std::vector<std::string> binaries = {
        "",
        std::string("\x00", 1),
        std::string("\x01\x02", 2),
        std::string("\x01\x02\x03", 3),
        std::string("\x01\x02\x03\x04", 4),
        std::string("\x01\x02\x03\x04\x05", 5)
    };
    std::vector<int32_t> expected = {0, 1, 2, 3, 4, 5};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 6);
    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, LongBinary) {
    std::string longBin(1000, '\xAB');
    std::vector<std::string> binaries = {longBin};
    std::vector<int32_t> expected = {1000};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 1);
    delete binVec;
    delete result;
}

TEST(LengthBinaryTest, SingleRow) {
    std::vector<std::string> binaries = {std::string("\xDE\xAD\xBE\xEF", 4)};
    std::vector<int32_t> expected = {4};
    BaseVector* binVec = LengthBinaryTestHelper::CreateBinaryVector(binaries);
    BaseVector* result = nullptr;
    LengthBinaryTestHelper::ExecuteLength(binVec, OMNI_INT, result);
    LengthBinaryTestHelper::ValidateNumericResult(result, expected, 1);
    delete binVec;
    delete result;
}
