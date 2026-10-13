 /*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: bit_count function test
*/

#include <gtest/gtest.h>
#include <vector>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

// ========== BitCount Tests for BOOL, BYTE, SHORT, INT, LONG ==========

/// Bit Count Function Test
/// bit_count(x) -> int32_t
/// Returns the number of bits that are set (1) in the binary representation of x.
/// Examples:
///   bit_count(0) = 0
///   bit_count(1) = 1
///   bit_count(255) = 8 (0xFF has 8 bits set)
///   bit_count(-1) = 8 for int8_t (0xFF), 32 for int32_t (0xFFFFFFFF)

// ========== BitCount Boolean Tests ==========

TEST(VectorizationTest, BitCountBoolean) {
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};

    auto bitCountExpr = new FuncExpr("bit_count", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_BOOLEAN};
    auto signature = std::make_shared<FunctionSignature>("bit_count", sigArgs, OMNI_INT);
    ASSERT_NE(bitCountExpr->vectorFunction, nullptr) << "bit_count function not found for BOOLEAN type";

    std::vector vecOfTypes = {BooleanType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values: true=1, false=0
    bool col1[rowSize] = {true, false, true, false};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitCountExpr);
    auto result = e.GetResult();

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 1);   // true has 1 bit set
    EXPECT_EQ(resultVector->GetValue(1), 0);   // false has 0 bits set
    EXPECT_EQ(resultVector->GetValue(2), 1);   // true has 1 bit set
    EXPECT_TRUE(resultVector->IsNull(3));      // NULL handling

    delete result;
    delete input;
    delete bitCountExpr;
    delete context;
}

// ========== BitCount Byte Tests ==========

TEST(VectorizationTest, BitCountByte) {
    int rowSize = 10;
    auto inputType = std::make_shared<DataType>(OMNI_BYTE);
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};

    auto bitCountExpr = new FuncExpr("bit_count", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_BYTE};
    auto signature = std::make_shared<FunctionSignature>("bit_count", sigArgs, OMNI_INT);
    ASSERT_NE(bitCountExpr->vectorFunction, nullptr) << "bit_count function not found for BYTE type";

    std::vector vecOfTypes = {ByteType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values: 0, 1, -1 (all bits set), 127, -128, various patterns
    int8_t col1[rowSize] = {
        0,                              // 0 bits
        1,                              // 1 bit
        -1,                             // 8 bits (0xFF)
        INT8_MAX,                       // 127 = 0b01111111, 7 bits
        INT8_MIN,                       // -128 = 0b10000000, 1 bit
        static_cast<int8_t>(0b10101010),// 4 bits
        static_cast<int8_t>(0b01010101),// 4 bits
        static_cast<int8_t>(0b11110000),// 4 bits
        static_cast<int8_t>(0b00001111),// 4 bits
        42                              // 0b00101010, 3 bits
    };
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitCountExpr);
    auto result = e.GetResult();

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);   // 0 has 0 bits
    EXPECT_EQ(resultVector->GetValue(1), 1);   // 1 has 1 bit
    EXPECT_EQ(resultVector->GetValue(2), 8);   // -1 has 8 bits
    EXPECT_EQ(resultVector->GetValue(3), 7);   // 127 has 7 bits
    EXPECT_EQ(resultVector->GetValue(4), 1);   // -128 has 1 bit
    EXPECT_TRUE(resultVector->IsNull(5));      // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), 4);   // 0b01010101 has 4 bits
    EXPECT_EQ(resultVector->GetValue(7), 4);   // 0b11110000 has 4 bits
    EXPECT_EQ(resultVector->GetValue(8), 4);   // 0b00001111 has 4 bits
    EXPECT_EQ(resultVector->GetValue(9), 3);   // 42 has 3 bits

    delete result;
    delete input;
    delete bitCountExpr;
    delete context;
}

// ========== BitCount Short Tests ==========

TEST(VectorizationTest, BitCountShort) {
    int rowSize = 8;
    auto inputType = std::make_shared<DataType>(OMNI_SHORT);
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};

    auto bitCountExpr = new FuncExpr("bit_count", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_SHORT};
    auto signature = std::make_shared<FunctionSignature>("bit_count", sigArgs, OMNI_INT);
    ASSERT_NE(bitCountExpr->vectorFunction, nullptr) << "bit_count function not found for SHORT type";

    std::vector vecOfTypes = {ShortType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values: 0, 1, -1 (16 bits), max, min, patterns
    int16_t col1[rowSize] = {
        0,                              // 0 bits
        1,                              // 1 bit
        -1,                             // 16 bits
        INT16_MAX,                      // 32767 = 15 bits
        INT16_MIN,                      // -32768 = 1 bit
        255,                            // 8 bits
        256,                            // 1 bit
        1000                            // 0b1111101000 = 6 bits
    };
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitCountExpr);
    auto result = e.GetResult();

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);   // 0 has 0 bits
    EXPECT_EQ(resultVector->GetValue(1), 1);   // 1 has 1 bit
    EXPECT_EQ(resultVector->GetValue(2), 16);  // -1 has 16 bits
    EXPECT_TRUE(resultVector->IsNull(3));      // NULL handling
    EXPECT_EQ(resultVector->GetValue(4), 1);   // -32768 has 1 bit
    EXPECT_EQ(resultVector->GetValue(5), 8);   // 255 has 8 bits
    EXPECT_EQ(resultVector->GetValue(6), 1);   // 256 has 1 bit
    EXPECT_EQ(resultVector->GetValue(7), 6);   // 1000 has 6 bits

    delete result;
    delete input;
    delete bitCountExpr;
    delete context;
}

// ========== BitCount Int Tests ==========

TEST(VectorizationTest, BitCountInt) {
    int rowSize = 10;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};

    auto bitCountExpr = new FuncExpr("bit_count", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bit_count", sigArgs, OMNI_INT);
    ASSERT_NE(bitCountExpr->vectorFunction, nullptr) << "bit_count function not found for INT type";

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values: 0, 1, -1 (32 bits), max, min, powers of 2, patterns
    int32_t col1[rowSize] = {
        0,                                      // 0 bits
        1,                                      // 1 bit
        -1,                                     // 32 bits
        INT32_MAX,                              // 2147483647 = 31 bits
        INT32_MIN,                              // -2147483648 = 1 bit
        255,                                    // 8 bits
        65535,                                  // 16 bits
        1048576,                                // 2^20 = 1 bit
        0x55555555,                             // alternating bits = 16 bits
        static_cast<int32_t>(0xAAAAAAAAU)       // alternating bits = 16 bits (signed: negative)
    };
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitCountExpr);
    auto result = e.GetResult();

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);   // 0 has 0 bits
    EXPECT_EQ(resultVector->GetValue(1), 1);   // 1 has 1 bit
    EXPECT_EQ(resultVector->GetValue(2), 32);  // -1 has 32 bits
    EXPECT_EQ(resultVector->GetValue(3), 31);  // INT32_MAX has 31 bits
    EXPECT_EQ(resultVector->GetValue(4), 1);   // INT32_MIN has 1 bit
    EXPECT_TRUE(resultVector->IsNull(5));      // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), 16);  // 65535 has 16 bits
    EXPECT_EQ(resultVector->GetValue(7), 1);   // 2^20 has 1 bit
    EXPECT_EQ(resultVector->GetValue(8), 16);  // 0x55555555 has 16 bits
    EXPECT_EQ(resultVector->GetValue(9), 16);  // 0xAAAAAAAA has 16 bits

    delete result;
    delete input;
    delete bitCountExpr;
    delete context;
}

// ========== BitCount Long Tests ==========

TEST(VectorizationTest, BitCountLong) {
    int rowSize = 10;
    auto inputType = std::make_shared<DataType>(OMNI_LONG);
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};

    auto bitCountExpr = new FuncExpr("bit_count", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>("bit_count", sigArgs, OMNI_INT);
    ASSERT_NE(bitCountExpr->vectorFunction, nullptr) << "bit_count function not found for LONG type";

    std::vector vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values: 0, 1, -1 (64 bits), max, min, powers of 2, patterns
    int64_t col1[rowSize] = {
        0LL,                                            // 0 bits
        1LL,                                            // 1 bit
        -1LL,                                           // 64 bits
        INT64_MAX,                                      // 63 bits
        INT64_MIN,                                      // 1 bit
        255LL,                                          // 8 bits
        0xFFFFFFFFLL,                                   // 32 bits
        0x1000000000000000LL,                           // 2^60 = 1 bit
        0x5555555555555555LL,                           // alternating bits = 32 bits
        static_cast<int64_t>(0xAAAAAAAAAAAAAAAAULL)     // alternating bits = 32 bits (signed: negative)
    };
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitCountExpr);
    auto result = e.GetResult();

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);   // 0 has 0 bits
    EXPECT_EQ(resultVector->GetValue(1), 1);   // 1 has 1 bit
    EXPECT_EQ(resultVector->GetValue(2), 64);  // -1 has 64 bits
    EXPECT_EQ(resultVector->GetValue(3), 63);  // INT64_MAX has 63 bits
    EXPECT_EQ(resultVector->GetValue(4), 1);   // INT64_MIN has 1 bit
    EXPECT_TRUE(resultVector->IsNull(5));      // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), 32);  // 0xFFFFFFFF has 32 bits
    EXPECT_EQ(resultVector->GetValue(7), 1);   // 2^60 has 1 bit
    EXPECT_EQ(resultVector->GetValue(8), 32);  // 0x5555555555555555 has 32 bits
    EXPECT_EQ(resultVector->GetValue(9), 32);  // 0xAAAAAAAAAAAAAAAA has 32 bits

    delete result;
    delete input;
    delete bitCountExpr;
    delete context;
}

// ========== BitCount Edge Cases ==========

TEST(VectorizationTest, BitCountEdgeCases) {
    // Test various edge cases for int32_t input
    int rowSize = 8;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};

    auto bitCountExpr = new FuncExpr("bit_count", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bit_count", sigArgs, OMNI_INT);
    ASSERT_NE(bitCountExpr->vectorFunction, nullptr) << "bit_count function not found for INT type";

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test edge cases: powers of 2, powers of 2 minus 1
    int32_t col1[rowSize] = {
        2,                              // 1 bit (binary: 10)
        4,                              // 1 bit (binary: 100)
        8,                              // 1 bit (binary: 1000)
        3,                              // 2 bits (binary: 11)
        7,                              // 3 bits (binary: 111)
        15,                             // 4 bits (binary: 1111)
        31,                             // 5 bits (binary: 11111)
        63                              // 6 bits (binary: 111111)
    };
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitCountExpr);
    auto result = e.GetResult();

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 1);   // 2 has 1 bit
    EXPECT_EQ(resultVector->GetValue(1), 1);   // 4 has 1 bit
    EXPECT_EQ(resultVector->GetValue(2), 1);   // 8 has 1 bit
    EXPECT_EQ(resultVector->GetValue(3), 2);   // 3 has 2 bits
    EXPECT_EQ(resultVector->GetValue(4), 3);   // 7 has 3 bits
    EXPECT_EQ(resultVector->GetValue(5), 4);   // 15 has 4 bits
    EXPECT_EQ(resultVector->GetValue(6), 5);   // 31 has 5 bits
    EXPECT_EQ(resultVector->GetValue(7), 6);   // 63 has 6 bits

    delete result;
    delete input;
    delete bitCountExpr;
    delete context;
}