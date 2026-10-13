/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: bit_length function unit tests
*   bit_length(string/binary) -> int32
*   Returns the bit length of the input (byte length * 8). Empty string returns 0.
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

class BitLengthTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const bit_length_test_env =
    ::testing::AddGlobalTestEnvironment(new BitLengthTestEnvironment);

class BitLengthFunctionTestHelper {
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

    static void ExecuteBitLength(BaseVector* stringVec, DataTypeId inputTypeId, DataTypeId outputTypeId, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { inputTypeId };
        auto sig = std::make_shared<FunctionSignature>("bit_length", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "bit_length function not found for input type " << inputTypeId;
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

// ========== bit_length String Tests ==========

TEST(BitLengthTest, EmptyStringReturnsZero) {
    std::vector<std::string> strings = {"", "", ""};
    std::vector<int32_t> expected = {0, 0, 0};  // 0 bytes * 8 = 0 bits
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, SingleAsciiChar) {
    std::vector<std::string> strings = {"a", "1", " "};
    std::vector<int32_t> expected = {8, 8, 8};  // 1 byte * 8 = 8 bits each
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, MultipleAsciiChars) {
    std::vector<std::string> strings = {"hello", "12345", "abc"};
    std::vector<int32_t> expected = {40, 40, 24};  // 5*8=40, 5*8=40, 3*8=24
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, UnicodeCharacters) {
    // UTF-8 encoding: 中 = 3 bytes, 文 = 3 bytes
    std::vector<std::string> strings = {"中", "中文", "a中b"};
    // "中" = 3 bytes = 24 bits
    // "中文" = 6 bytes = 48 bits
    // "a中b" = 1 + 3 + 1 = 5 bytes = 40 bits
    std::vector<int32_t> expected = {24, 48, 40};
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, NullHandling) {
    std::vector<std::string> strings = {"a", "bb", "ccc"};
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    strVec->SetNull(1);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    EXPECT_FALSE(result->IsNull(2));
    
    // Check non-null values
    auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 8);   // "a" = 1 byte = 8 bits
    EXPECT_EQ(resultVec->GetValue(2), 24);  // "ccc" = 3 bytes = 24 bits
    
    delete strVec;
    delete result;
}

TEST(BitLengthTest, SingleRow) {
    std::vector<std::string> strings = {"hello"};
    std::vector<int32_t> expected = {40};  // 5 bytes * 8 = 40 bits
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, NullByteCountedAsOneByte) {
    // String with null byte
    std::vector<std::string> strings = {std::string("\0", 1)};
    std::vector<int32_t> expected = {8};  // 1 byte * 8 = 8 bits
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, VeryLongString) {
    // Test with a longer string
    std::string longStr(1000, 'x');  // 1000 characters
    std::vector<std::string> strings = {longStr};
    std::vector<int32_t> expected = {8000};  // 1000 bytes * 8 = 8000 bits
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete strVec;
    delete result;
}

TEST(BitLengthTest, MixedLengthStrings) {
    std::vector<std::string> strings = {"", "1", "12", "123", "1234", "12345"};
    std::vector<int32_t> expected = {0, 8, 16, 24, 32, 40};  // 0*8, 1*8, 2*8, 3*8, 4*8, 5*8
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 6);
    delete strVec;
    delete result;
}

// ========== bit_length Binary Tests ==========

TEST(BitLengthTest, BinaryEmptyReturnsZero) {
    std::vector<std::string> binaries = {""};
    std::vector<int32_t> expected = {0};
    BaseVector* binVec = BitLengthFunctionTestHelper::CreateStringVector(binaries);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(binVec, OMNI_VARBINARY, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 1);
    delete binVec;
    delete result;
}

TEST(BitLengthTest, BinaryBasic) {
    // Binary data (treating as raw bytes)
    // Use std::string(ptr, len) constructor to handle null bytes correctly
    std::vector<std::string> binaries = {
        std::string("\x00", 1),           // 1 byte with null
        std::string("\x01\x02", 2),       // 2 bytes
        std::string("\xFF\xFF\xFF\xFF", 4) // 4 bytes
    };
    std::vector<int32_t> expected = {8, 16, 32};  // 1*8, 2*8, 4*8
    BaseVector* binVec = BitLengthFunctionTestHelper::CreateStringVector(binaries);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(binVec, OMNI_VARBINARY, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 3);
    delete binVec;
    delete result;
}

TEST(BitLengthTest, SpecialCharacters) {
    // Test with special characters
    std::vector<std::string> strings = {"\t", "\n", "\r\n", "\\n"};
    // \t = 1 byte = 8 bits
    // \n = 1 byte = 8 bits
    // \r\n = 2 bytes = 16 bits
    // \\n (literal backslash n) = 2 bytes = 16 bits
    std::vector<int32_t> expected = {8, 8, 16, 16};
    BaseVector* strVec = BitLengthFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    BitLengthFunctionTestHelper::ExecuteBitLength(strVec, OMNI_VARCHAR, OMNI_INT, result);
    BitLengthFunctionTestHelper::ValidateNumericResult(result, expected, 4);
    delete strVec;
    delete result;
}