/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: bitwise function test
 */

#include <gtest/gtest.h>
#include <iostream>
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

// ========== BitwiseAnd Tests for BYTE, SHORT, INT, LONG ==========

/// Bitwise AND Function Test
/// bitwise_and(a, b) -> a & b
/// Tests the bitwise AND operation across all supported integral types.
/// Handles NULL values correctly.

TEST(VectorizationTest, BitwiseAndByte) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitAndExpr = new FuncExpr("bitwise_and", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_BYTE, OMNI_BYTE};
    auto signature = std::make_shared<FunctionSignature>("bitwise_and", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitAndExpr->vectorFunction, nullptr) << "bitwise_and function not found for BYTE type";

    std::vector vecOfTypes = {ByteType(), ByteType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values include: basic cases, zero, -1, min/max boundary values
    int8_t col1[rowSize] = {0b1010, 0b1100, 0b0111, 0, -1, INT8_MIN, INT8_MAX, INT8_MAX};
    int8_t col2[rowSize] = {0b0110, 0b0101, 0b1010, -1, 0, INT8_MAX, INT8_MIN, -1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(2);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitAndExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseAndByte Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int8_t>(0b1010 & 0b0110));   // 0b0010 = 2
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int8_t>(0b1100 & 0b0101));   // 0b0100 = 4
    EXPECT_TRUE(resultVector->IsNull(2));                                          // NULL handling
    EXPECT_EQ(resultVector->GetValue(3), static_cast<int8_t>(0 & -1));            // 0
    EXPECT_EQ(resultVector->GetValue(4), static_cast<int8_t>(-1 & 0));            // 0
    EXPECT_EQ(resultVector->GetValue(5), static_cast<int8_t>(INT8_MIN & INT8_MAX)); // 0
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int8_t>(INT8_MAX & INT8_MIN)); // 0
    EXPECT_EQ(resultVector->GetValue(7), INT8_MAX);                                // MAX & -1 = MAX

    delete input;
    delete bitAndExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseAndShort) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitAndExpr = new FuncExpr("bitwise_and", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_SHORT, OMNI_SHORT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_and", sigArgs, OMNI_SHORT);
    ASSERT_NE(bitAndExpr->vectorFunction, nullptr) << "bitwise_and function not found for SHORT type";

    std::vector vecOfTypes = {ShortType(), ShortType()};
    DataTypes inputTypes(vecOfTypes);
    
    int16_t col1[rowSize] = {0x00FF, static_cast<int16_t>(0xFF00), 0x5555, 0, -1, INT16_MIN, INT16_MAX, INT16_MAX};
    int16_t col2[rowSize] = {0x0F0F, static_cast<int16_t>(0xF0F0), static_cast<int16_t>(0xAAAA), -1, 0, INT16_MAX, INT16_MIN, -1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitAndExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseAndShort Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int16_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int16_t>(0x00FF & 0x0F0F));   // 0x000F
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int16_t>(0xFF00 & 0xF0F0));   // 0xF000
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int16_t>(0x5555 & 0xAAAA));   // 0x0000
    EXPECT_TRUE(resultVector->IsNull(3));                                           // NULL handling
    EXPECT_EQ(resultVector->GetValue(4), static_cast<int16_t>(-1 & 0));            // 0
    EXPECT_EQ(resultVector->GetValue(5), static_cast<int16_t>(INT16_MIN & INT16_MAX)); // 0
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int16_t>(INT16_MAX & INT16_MIN)); // 0
    EXPECT_EQ(resultVector->GetValue(7), INT16_MAX);                                // MAX & -1 = MAX

    delete input;
    delete bitAndExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseAndInt) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitAndExpr = new FuncExpr("bitwise_and", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_and", sigArgs, OMNI_INT);
    ASSERT_NE(bitAndExpr->vectorFunction, nullptr) << "bitwise_and function not found for INT type";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0b1010, 0b1100, 0b0111, 0, -1, -4, 60, INT32_MIN, INT32_MAX, INT32_MAX};
    int32_t col2[rowSize] = {0b0110, 0b0101, 0b1010, -1, 0, 12, 21, INT32_MAX, INT32_MIN, -1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitAndExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseAndInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0b0010);   // 0b1010 & 0b0110 = 0b0010
    EXPECT_EQ(resultVector->GetValue(1), 0b0100);   // 0b1100 & 0b0101 = 0b0100
    EXPECT_EQ(resultVector->GetValue(2), 0b0010);   // 0b0111 & 0b1010 = 0b0010
    EXPECT_EQ(resultVector->GetValue(3), 0);        // 0 & -1 = 0
    EXPECT_TRUE(resultVector->IsNull(4));           // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), 12);       // -4 & 12 = 12
    EXPECT_EQ(resultVector->GetValue(6), 20);       // 60 & 21 = 20
    EXPECT_EQ(resultVector->GetValue(7), 0);        // MIN & MAX = 0
    EXPECT_EQ(resultVector->GetValue(8), 0);        // MAX & MIN = 0
    EXPECT_EQ(resultVector->GetValue(9), INT32_MAX);// MAX & -1 = MAX

    delete input;
    delete bitAndExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseAndLong) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitAndExpr = new FuncExpr("bitwise_and", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG, OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>("bitwise_and", sigArgs, OMNI_LONG);
    ASSERT_NE(bitAndExpr->vectorFunction, nullptr) << "bitwise_and function not found for LONG type";

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    
    int64_t col1[rowSize] = {static_cast<int64_t>(0xFFFFFFFF00000000LL), 0x00000000FFFFFFFFLL, 0x5555555555555555LL, 
                             0, -1, INT64_MIN, INT64_MAX, INT64_MAX, 0x123456789ABCDEF0LL, -1};
    int64_t col2[rowSize] = {0x0F0F0F0F0F0F0F0FLL, static_cast<int64_t>(0xF0F0F0F0F0F0F0F0LL), static_cast<int64_t>(0xAAAAAAAAAAAAAAAALL),
                             -1, 0, INT64_MAX, INT64_MIN, -1, 0x0FEDCBA987654321LL, 1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitAndExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseAndLong Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int64_t>(0xFFFFFFFF00000000LL & 0x0F0F0F0F0F0F0F0FLL));
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int64_t>(0x00000000FFFFFFFFLL & 0xF0F0F0F0F0F0F0F0LL));
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int64_t>(0x5555555555555555LL & 0xAAAAAAAAAAAAAAAALL)); // 0
    EXPECT_EQ(resultVector->GetValue(3), 0LL);         // 0 & -1 = 0
    EXPECT_EQ(resultVector->GetValue(4), 0LL);         // -1 & 0 = 0
    EXPECT_TRUE(resultVector->IsNull(5));              // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), 0LL);         // MAX & MIN = 0
    EXPECT_EQ(resultVector->GetValue(7), INT64_MAX);   // MAX & -1 = MAX
    EXPECT_EQ(resultVector->GetValue(8), static_cast<int64_t>(0x123456789ABCDEF0LL & 0x0FEDCBA987654321LL));
    EXPECT_EQ(resultVector->GetValue(9), 1LL);         // -1 & 1 = 1

    delete input;
    delete bitAndExpr;
    delete context;
}

// ========== Additional BitwiseAnd boundary tests ==========

TEST(VectorizationTest, BitwiseAndAllNulls) {
    // Test case where all inputs have NULL values
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitAndExpr = new FuncExpr("bitwise_and", args, type);
    ASSERT_NE(bitAndExpr->vectorFunction, nullptr) << "bitwise_and function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {1, 2, 3, 4};
    int32_t col2[rowSize] = {5, 6, 7, 8};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    
    // Set all values in first column to NULL
    for (int i = 0; i < rowSize; i++) {
        input->Get(0)->SetNull(i);
    }

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitAndExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseAndAllNulls Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // All results should be NULL since first operand is always NULL
    for (int i = 0; i < rowSize; i++) {
        EXPECT_TRUE(resultVector->IsNull(i));
    }

    delete input;
    delete bitAndExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseAndSameValue) {
    // Test a & a = a (identity property)
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitAndExpr = new FuncExpr("bitwise_and", args, type);
    ASSERT_NE(bitAndExpr->vectorFunction, nullptr) << "bitwise_and function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, 42, -1, INT32_MAX};
    int32_t col2[rowSize] = {0, 42, -1, INT32_MAX};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitAndExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseAndSameValue Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);
    EXPECT_EQ(resultVector->GetValue(1), 42);
    EXPECT_EQ(resultVector->GetValue(2), -1);
    EXPECT_EQ(resultVector->GetValue(3), INT32_MAX);

    delete input;
    delete bitAndExpr;
    delete context;
}

// ========== BitwiseXor Tests for BYTE, SHORT, INT, LONG ==========

/// Bitwise XOR Function Test
/// bitwise_xor(a, b) -> a ^ b
/// Tests the bitwise XOR operation across all supported integral types.
/// Handles NULL values correctly.

TEST(VectorizationTest, BitwiseXorByte) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitXorExpr = new FuncExpr("bitwise_xor", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_BYTE, OMNI_BYTE};
    auto signature = std::make_shared<FunctionSignature>("bitwise_xor", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitXorExpr->vectorFunction, nullptr) << "bitwise_xor function not found for BYTE type";

    std::vector vecOfTypes = {ByteType(), ByteType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values include: basic cases, zero, -1, min/max boundary values
    int8_t col1[rowSize] = {0b1010, 0b1100, 0b0111, 0, -1, INT8_MIN, INT8_MAX, INT8_MAX};
    int8_t col2[rowSize] = {0b0110, 0b0101, 0b1010, -1, 0, INT8_MAX, INT8_MIN, -1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(2);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitXorExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseXorByte Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int8_t>(0b1010 ^ 0b0110));   // 0b1100 = 12
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int8_t>(0b1100 ^ 0b0101));   // 0b1001 = 9
    EXPECT_TRUE(resultVector->IsNull(2));                                          // NULL handling
    EXPECT_EQ(resultVector->GetValue(3), static_cast<int8_t>(0 ^ -1));            // -1
    EXPECT_EQ(resultVector->GetValue(4), static_cast<int8_t>(-1 ^ 0));            // -1
    EXPECT_EQ(resultVector->GetValue(5), static_cast<int8_t>(INT8_MIN ^ INT8_MAX)); // -1
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int8_t>(INT8_MAX ^ INT8_MIN)); // -1
    EXPECT_EQ(resultVector->GetValue(7), INT8_MIN);                                // MAX ^ -1 = MIN

    delete input;
    delete bitXorExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseXorShort) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitXorExpr = new FuncExpr("bitwise_xor", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_SHORT, OMNI_SHORT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_xor", sigArgs, OMNI_SHORT);
    ASSERT_NE(bitXorExpr->vectorFunction, nullptr) << "bitwise_xor function not found for SHORT type";

    std::vector vecOfTypes = {ShortType(), ShortType()};
    DataTypes inputTypes(vecOfTypes);
    
    int16_t col1[rowSize] = {0x00FF, static_cast<int16_t>(0xFF00), 0x5555, 0, -1, INT16_MIN, INT16_MAX, INT16_MAX};
    int16_t col2[rowSize] = {0x0F0F, static_cast<int16_t>(0xF0F0), static_cast<int16_t>(0xAAAA), -1, 0, INT16_MAX, INT16_MIN, -1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitXorExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseXorShort Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int16_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int16_t>(0x00FF ^ 0x0F0F));   // 0x0FF0
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int16_t>(0xFF00 ^ 0xF0F0));   // 0x0FF0
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int16_t>(0x5555 ^ 0xAAAA));   // 0xFFFF = -1
    EXPECT_TRUE(resultVector->IsNull(3));                                           // NULL handling
    EXPECT_EQ(resultVector->GetValue(4), static_cast<int16_t>(-1 ^ 0));            // -1
    EXPECT_EQ(resultVector->GetValue(5), static_cast<int16_t>(INT16_MIN ^ INT16_MAX)); // -1
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int16_t>(INT16_MAX ^ INT16_MIN)); // -1
    EXPECT_EQ(resultVector->GetValue(7), INT16_MIN);                                // MAX ^ -1 = MIN

    delete input;
    delete bitXorExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseXorInt) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitXorExpr = new FuncExpr("bitwise_xor", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_xor", sigArgs, OMNI_INT);
    ASSERT_NE(bitXorExpr->vectorFunction, nullptr) << "bitwise_xor function not found for INT type";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0b1010, 0b1100, 0b0111, 0, -1, -4, 60, INT32_MIN, INT32_MAX, INT32_MAX};
    int32_t col2[rowSize] = {0b0110, 0b0101, 0b1010, -1, 0, 12, 21, INT32_MAX, INT32_MIN, -1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitXorExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseXorInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0b1100);   // 0b1010 ^ 0b0110 = 0b1100
    EXPECT_EQ(resultVector->GetValue(1), 0b1001);   // 0b1100 ^ 0b0101 = 0b1001
    EXPECT_EQ(resultVector->GetValue(2), 0b1101);   // 0b0111 ^ 0b1010 = 0b1101
    EXPECT_EQ(resultVector->GetValue(3), -1);       // 0 ^ -1 = -1
    EXPECT_TRUE(resultVector->IsNull(4));           // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), -16);      // -4 ^ 12 = -16
    EXPECT_EQ(resultVector->GetValue(6), 41);       // 60 ^ 21 = 41
    EXPECT_EQ(resultVector->GetValue(7), -1);       // MIN ^ MAX = -1
    EXPECT_EQ(resultVector->GetValue(8), -1);       // MAX ^ MIN = -1
    EXPECT_EQ(resultVector->GetValue(9), INT32_MIN);// MAX ^ -1 = MIN

    delete input;
    delete bitXorExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseXorLong) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitXorExpr = new FuncExpr("bitwise_xor", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG, OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>("bitwise_xor", sigArgs, OMNI_LONG);
    ASSERT_NE(bitXorExpr->vectorFunction, nullptr) << "bitwise_xor function not found for LONG type";

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    
    int64_t col1[rowSize] = {static_cast<int64_t>(0xFFFFFFFF00000000LL), 0x00000000FFFFFFFFLL, 0x5555555555555555LL, 
                             0, -1, INT64_MIN, INT64_MAX, INT64_MAX, 0x123456789ABCDEF0LL, -1};
    int64_t col2[rowSize] = {0x0F0F0F0F0F0F0F0FLL, static_cast<int64_t>(0xF0F0F0F0F0F0F0F0LL), static_cast<int64_t>(0xAAAAAAAAAAAAAAAALL),
                             -1, 0, INT64_MAX, INT64_MIN, -1, 0x0FEDCBA987654321LL, 1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitXorExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseXorLong Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int64_t>(0xFFFFFFFF00000000LL ^ 0x0F0F0F0F0F0F0F0FLL));
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int64_t>(0x00000000FFFFFFFFLL ^ 0xF0F0F0F0F0F0F0F0LL));
    EXPECT_EQ(resultVector->GetValue(2), -1LL);        // 0x5555... ^ 0xAAAA... = -1
    EXPECT_EQ(resultVector->GetValue(3), -1LL);        // 0 ^ -1 = -1
    EXPECT_EQ(resultVector->GetValue(4), -1LL);        // -1 ^ 0 = -1
    EXPECT_TRUE(resultVector->IsNull(5));              // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), -1LL);        // MAX ^ MIN = -1
    EXPECT_EQ(resultVector->GetValue(7), INT64_MIN);   // MAX ^ -1 = MIN
    EXPECT_EQ(resultVector->GetValue(8), static_cast<int64_t>(0x123456789ABCDEF0LL ^ 0x0FEDCBA987654321LL));
    EXPECT_EQ(resultVector->GetValue(9), -2LL);        // -1 ^ 1 = -2

    delete input;
    delete bitXorExpr;
    delete context;
}

// ========== Additional BitwiseXor boundary tests ==========

TEST(VectorizationTest, BitwiseXorSameValue) {
    // Test a ^ a = 0 (XOR identity property)
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitXorExpr = new FuncExpr("bitwise_xor", args, type);
    ASSERT_NE(bitXorExpr->vectorFunction, nullptr) << "bitwise_xor function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, 42, -1, INT32_MAX};
    int32_t col2[rowSize] = {0, 42, -1, INT32_MAX};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitXorExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseXorSameValue Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // a ^ a = 0 for all values
    EXPECT_EQ(resultVector->GetValue(0), 0);
    EXPECT_EQ(resultVector->GetValue(1), 0);
    EXPECT_EQ(resultVector->GetValue(2), 0);
    EXPECT_EQ(resultVector->GetValue(3), 0);

    delete input;
    delete bitXorExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseXorAllNulls) {
    // Test case where all inputs have NULL values
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitXorExpr = new FuncExpr("bitwise_xor", args, type);
    ASSERT_NE(bitXorExpr->vectorFunction, nullptr) << "bitwise_xor function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {1, 2, 3, 4};
    int32_t col2[rowSize] = {5, 6, 7, 8};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    
    // Set all values in first column to NULL
    for (int i = 0; i < rowSize; i++) {
        input->Get(0)->SetNull(i);
    }

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitXorExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseXorAllNulls Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // All results should be NULL since first operand is always NULL
    for (int i = 0; i < rowSize; i++) {
        EXPECT_TRUE(resultVector->IsNull(i));
    }

    delete input;
    delete bitXorExpr;
    delete context;
}

// ========== BitwiseOr Tests for BYTE, SHORT, INT, LONG ==========

/// Bitwise OR Function Test
/// bitwise_or(a, b) -> a | b
/// Tests the bitwise OR operation across all supported integral types.
/// Handles NULL values correctly.

TEST(VectorizationTest, BitwiseOrByte) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_BYTE, OMNI_BYTE};
    auto signature = std::make_shared<FunctionSignature>("bitwise_or", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found for BYTE type";

    std::vector vecOfTypes = {ByteType(), ByteType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values include: basic cases, zero, -1, min/max boundary values
    int8_t col1[rowSize] = {0b1010, 0b1100, 0b0111, 0, -1, INT8_MIN, INT8_MAX, INT8_MAX};
    int8_t col2[rowSize] = {0b0110, 0b0101, 0b1010, -1, 0, INT8_MAX, INT8_MIN, 1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(2);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrByte Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int8_t>(0b1010 | 0b0110));   // 0b1110 = 14
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int8_t>(0b1100 | 0b0101));   // 0b1101 = 13
    EXPECT_TRUE(resultVector->IsNull(2));                                          // NULL handling
    EXPECT_EQ(resultVector->GetValue(3), static_cast<int8_t>(0 | -1));            // -1
    EXPECT_EQ(resultVector->GetValue(4), static_cast<int8_t>(-1 | 0));            // -1
    EXPECT_EQ(resultVector->GetValue(5), static_cast<int8_t>(INT8_MIN | INT8_MAX)); // -1
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int8_t>(INT8_MAX | INT8_MIN)); // -1
    EXPECT_EQ(resultVector->GetValue(7), INT8_MAX);                                // MAX | 1 = MAX

    delete input;
    delete bitOrExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseOrShort) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_SHORT, OMNI_SHORT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_or", sigArgs, OMNI_SHORT);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found for SHORT type";

    std::vector vecOfTypes = {ShortType(), ShortType()};
    DataTypes inputTypes(vecOfTypes);
    
    int16_t col1[rowSize] = {0x00FF, static_cast<int16_t>(0xFF00), 0x5555, 0, -1, INT16_MIN, INT16_MAX, INT16_MAX};
    int16_t col2[rowSize] = {0x0F0F, static_cast<int16_t>(0xF0F0), static_cast<int16_t>(0xAAAA), -1, 0, INT16_MAX, INT16_MIN, 1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrShort Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int16_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int16_t>(0x00FF | 0x0F0F));   // 0x0FFF
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int16_t>(0xFF00 | 0xF0F0));   // 0xFFF0
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int16_t>(0x5555 | 0xAAAA));   // 0xFFFF = -1
    EXPECT_TRUE(resultVector->IsNull(3));                                           // NULL handling
    EXPECT_EQ(resultVector->GetValue(4), static_cast<int16_t>(-1 | 0));            // -1
    EXPECT_EQ(resultVector->GetValue(5), static_cast<int16_t>(INT16_MIN | INT16_MAX)); // -1
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int16_t>(INT16_MAX | INT16_MIN)); // -1
    EXPECT_EQ(resultVector->GetValue(7), INT16_MAX);                                // MAX | 1 = MAX

    delete input;
    delete bitOrExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseOrInt) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_or", sigArgs, OMNI_INT);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found for INT type";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0b1010, 0b1100, 0b0111, 0, -1, -4, 60, INT32_MIN, INT32_MAX, INT32_MAX};
    int32_t col2[rowSize] = {0b0110, 0b0101, 0b1010, -1, 0, 12, 21, INT32_MAX, INT32_MIN, 1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0b1110);   // 0b1010 | 0b0110 = 0b1110
    EXPECT_EQ(resultVector->GetValue(1), 0b1101);   // 0b1100 | 0b0101 = 0b1101
    EXPECT_EQ(resultVector->GetValue(2), 0b1111);   // 0b0111 | 0b1010 = 0b1111
    EXPECT_EQ(resultVector->GetValue(3), -1);       // 0 | -1 = -1
    EXPECT_TRUE(resultVector->IsNull(4));           // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), -4);       // -4 | 12 = -4
    EXPECT_EQ(resultVector->GetValue(6), 61);       // 60 | 21 = 61
    EXPECT_EQ(resultVector->GetValue(7), -1);       // MIN | MAX = -1
    EXPECT_EQ(resultVector->GetValue(8), -1);       // MAX | MIN = -1
    EXPECT_EQ(resultVector->GetValue(9), INT32_MAX);// MAX | 1 = MAX

    delete input;
    delete bitOrExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseOrLong) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG, OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>("bitwise_or", sigArgs, OMNI_LONG);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found for LONG type";

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    
    int64_t col1[rowSize] = {static_cast<int64_t>(0xFFFFFFFF00000000LL), 0x00000000FFFFFFFFLL, 0x5555555555555555LL, 
                             0, -1, INT64_MIN, INT64_MAX, INT64_MAX, 0x123456789ABCDEF0LL, INT64_MIN};
    int64_t col2[rowSize] = {0x0F0F0F0F0F0F0F0FLL, static_cast<int64_t>(0xF0F0F0F0F0F0F0F0LL), static_cast<int64_t>(0xAAAAAAAAAAAAAAAALL),
                             -1, 0, INT64_MAX, INT64_MIN, 1, 0x0FEDCBA987654321LL, 1};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrLong Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int64_t>(0xFFFFFFFF00000000LL | 0x0F0F0F0F0F0F0F0FLL));
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int64_t>(0x00000000FFFFFFFFLL | 0xF0F0F0F0F0F0F0F0LL));
    EXPECT_EQ(resultVector->GetValue(2), -1LL);        // 0x5555... | 0xAAAA... = -1
    EXPECT_EQ(resultVector->GetValue(3), -1LL);        // 0 | -1 = -1
    EXPECT_EQ(resultVector->GetValue(4), -1LL);        // -1 | 0 = -1
    EXPECT_TRUE(resultVector->IsNull(5));              // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), -1LL);        // MAX | MIN = -1
    EXPECT_EQ(resultVector->GetValue(7), INT64_MAX);   // MAX | 1 = MAX
    EXPECT_EQ(resultVector->GetValue(8), static_cast<int64_t>(0x123456789ABCDEF0LL | 0x0FEDCBA987654321LL));
    EXPECT_EQ(resultVector->GetValue(9), static_cast<int64_t>(INT64_MIN | 1LL));  // MIN | 1

    delete input;
    delete bitOrExpr;
    delete context;
}

// ========== Additional BitwiseOr boundary tests ==========

TEST(VectorizationTest, BitwiseOrSameValue) {
    // Test a | a = a (OR identity property)
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, 42, -1, INT32_MAX};
    int32_t col2[rowSize] = {0, 42, -1, INT32_MAX};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrSameValue Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // a | a = a for all values
    EXPECT_EQ(resultVector->GetValue(0), 0);
    EXPECT_EQ(resultVector->GetValue(1), 42);
    EXPECT_EQ(resultVector->GetValue(2), -1);
    EXPECT_EQ(resultVector->GetValue(3), INT32_MAX);

    delete input;
    delete bitOrExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseOrAllNulls) {
    // Test case where all inputs have NULL values
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {1, 2, 3, 4};
    int32_t col2[rowSize] = {5, 6, 7, 8};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    
    // Set all values in first column to NULL
    for (int i = 0; i < rowSize; i++) {
        input->Get(0)->SetNull(i);
    }

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrAllNulls Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // All results should be NULL since first operand is always NULL
    for (int i = 0; i < rowSize; i++) {
        EXPECT_TRUE(resultVector->IsNull(i));
    }

    delete input;
    delete bitOrExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseOrWithZero) {
    // Test a | 0 = a (zero identity property)
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};

    auto bitOrExpr = new FuncExpr("bitwise_or", args, type);
    ASSERT_NE(bitOrExpr->vectorFunction, nullptr) << "bitwise_or function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, 42, -1, INT32_MAX};
    int32_t col2[rowSize] = {0, 0, 0, 0};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitOrExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseOrWithZero Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // a | 0 = a for all values
    EXPECT_EQ(resultVector->GetValue(0), 0);
    EXPECT_EQ(resultVector->GetValue(1), 42);
    EXPECT_EQ(resultVector->GetValue(2), -1);
    EXPECT_EQ(resultVector->GetValue(3), INT32_MAX);

    delete input;
    delete bitOrExpr;
    delete context;
}

// ========== BitwiseNot Tests for BYTE, SHORT, INT, LONG ==========

/// Bitwise NOT Function Test
/// bitwise_not(a) -> ~a
/// Tests the bitwise NOT (complement) operation across all supported integral types.
/// Handles NULL values correctly.

TEST(VectorizationTest, BitwiseNotByte) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, type)};

    auto bitNotExpr = new FuncExpr("bitwise_not", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_BYTE};
    auto signature = std::make_shared<FunctionSignature>("bitwise_not", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitNotExpr->vectorFunction, nullptr) << "bitwise_not function not found for BYTE type";

    std::vector vecOfTypes = {ByteType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values include: basic cases, zero, -1, min/max boundary values
    int8_t col1[rowSize] = {0, -1, 1, -2, INT8_MIN, INT8_MAX, 0b01010101, static_cast<int8_t>(0b10101010)};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitNotExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseNotByte Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int8_t>(~0));         // -1
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int8_t>(~(-1)));      // 0
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int8_t>(~1));         // -2
    EXPECT_TRUE(resultVector->IsNull(3));                                   // NULL handling
    EXPECT_EQ(resultVector->GetValue(4), INT8_MAX);                        // ~MIN = MAX
    EXPECT_EQ(resultVector->GetValue(5), INT8_MIN);                        // ~MAX = MIN
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int8_t>(0b10101010)); // pattern flip
    EXPECT_EQ(resultVector->GetValue(7), static_cast<int8_t>(0b01010101)); // pattern flip

    delete input;
    delete bitNotExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseNotShort) {
    int rowSize = 8;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    std::vector<Expr*> args = {new FieldExpr(0, type)};

    auto bitNotExpr = new FuncExpr("bitwise_not", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_SHORT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_not", sigArgs, OMNI_SHORT);
    ASSERT_NE(bitNotExpr->vectorFunction, nullptr) << "bitwise_not function not found for SHORT type";

    std::vector vecOfTypes = {ShortType()};
    DataTypes inputTypes(vecOfTypes);
    
    int16_t col1[rowSize] = {0, -1, 1, -2, INT16_MIN, INT16_MAX, 0x5555, static_cast<int16_t>(0xAAAA)};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(3);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitNotExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseNotShort Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int16_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), static_cast<int16_t>(~0));         // -1
    EXPECT_EQ(resultVector->GetValue(1), static_cast<int16_t>(~(-1)));      // 0
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int16_t>(~1));         // -2
    EXPECT_TRUE(resultVector->IsNull(3));                                    // NULL handling
    EXPECT_EQ(resultVector->GetValue(4), INT16_MAX);                        // ~MIN = MAX
    EXPECT_EQ(resultVector->GetValue(5), INT16_MIN);                        // ~MAX = MIN
    EXPECT_EQ(resultVector->GetValue(6), static_cast<int16_t>(0xAAAA));     // ~0x5555 = 0xAAAA
    EXPECT_EQ(resultVector->GetValue(7), static_cast<int16_t>(0x5555));     // ~0xAAAA = 0x5555

    delete input;
    delete bitNotExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseNotInt) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type)};

    auto bitNotExpr = new FuncExpr("bitwise_not", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bitwise_not", sigArgs, OMNI_INT);
    ASSERT_NE(bitNotExpr->vectorFunction, nullptr) << "bitwise_not function not found for INT type";

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, -1, 1, -2, 3, -4, 60, INT32_MIN, INT32_MAX, 0x55555555};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitNotExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseNotInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), -1);          // ~0 = -1
    EXPECT_EQ(resultVector->GetValue(1), 0);           // ~(-1) = 0
    EXPECT_EQ(resultVector->GetValue(2), -2);          // ~1 = -2
    EXPECT_EQ(resultVector->GetValue(3), 1);           // ~(-2) = 1
    EXPECT_TRUE(resultVector->IsNull(4));              // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), 3);           // ~(-4) = 3
    EXPECT_EQ(resultVector->GetValue(6), -61);         // ~60 = -61
    EXPECT_EQ(resultVector->GetValue(7), INT32_MAX);   // ~MIN = MAX
    EXPECT_EQ(resultVector->GetValue(8), INT32_MIN);   // ~MAX = MIN
    EXPECT_EQ(resultVector->GetValue(9), static_cast<int32_t>(0xAAAAAAAA)); // pattern flip

    delete input;
    delete bitNotExpr;
    delete context;
}

TEST(VectorizationTest, BitwiseNotLong) {
    int rowSize = 10;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type)};

    auto bitNotExpr = new FuncExpr("bitwise_not", args, type);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>("bitwise_not", sigArgs, OMNI_LONG);
    ASSERT_NE(bitNotExpr->vectorFunction, nullptr) << "bitwise_not function not found for LONG type";

    std::vector vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);
    
    int64_t col1[rowSize] = {0, -1, 1, -2, 3, -4, 60, INT64_MIN, INT64_MAX, 0x5555555555555555LL};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    input->Get(0)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitNotExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseNotLong Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), -1LL);        // ~0 = -1
    EXPECT_EQ(resultVector->GetValue(1), 0LL);         // ~(-1) = 0
    EXPECT_EQ(resultVector->GetValue(2), -2LL);        // ~1 = -2
    EXPECT_EQ(resultVector->GetValue(3), 1LL);         // ~(-2) = 1
    EXPECT_TRUE(resultVector->IsNull(4));              // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), 3LL);         // ~(-4) = 3
    EXPECT_EQ(resultVector->GetValue(6), -61LL);       // ~60 = -61
    EXPECT_EQ(resultVector->GetValue(7), INT64_MAX);   // ~MIN = MAX
    EXPECT_EQ(resultVector->GetValue(8), INT64_MIN);   // ~MAX = MIN
    EXPECT_EQ(resultVector->GetValue(9), static_cast<int64_t>(0xAAAAAAAAAAAAAAAALL)); // pattern flip

    delete input;
    delete bitNotExpr;
    delete context;
}

// ========== Additional BitwiseNot boundary tests ==========

TEST(VectorizationTest, BitwiseNotDoubleNot) {
    // Test ~~a = a (double NOT identity property)
    int rowSize = 5;
    auto type = std::make_shared<DataType>(OMNI_INT);
    
    // First NOT
    std::vector<Expr*> args1 = {new FieldExpr(0, type)};
    auto bitNotExpr1 = new FuncExpr("bitwise_not", args1, type);
    ASSERT_NE(bitNotExpr1->vectorFunction, nullptr) << "bitwise_not function not found";

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, 42, -1, INT32_MAX, INT32_MIN};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);

    auto context1 = new ExecutionContext(4096, nullptr);
    context1->SetResultRowSize(rowSize);
    ExprEval e1(input, context1);
    e1.Visit(*bitNotExpr1);
    auto result1 = e1.GetResult();

    // Second NOT on the result
    VectorBatch *input2 = new VectorBatch(rowSize);
    input2->Append(result1);
    
    std::vector<Expr*> args2 = {new FieldExpr(0, type)};
    auto bitNotExpr2 = new FuncExpr("bitwise_not", args2, type);
    
    auto context2 = new ExecutionContext(4096, nullptr);
    context2->SetResultRowSize(rowSize);
    ExprEval e2(input2, context2);
    e2.Visit(*bitNotExpr2);
    auto result2 = e2.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result2);
    std::cout << "=== BitwiseNotDoubleNot Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result2);
    // ~~a = a for all values
    EXPECT_EQ(resultVector->GetValue(0), 0);
    EXPECT_EQ(resultVector->GetValue(1), 42);
    EXPECT_EQ(resultVector->GetValue(2), -1);
    EXPECT_EQ(resultVector->GetValue(3), INT32_MAX);
    EXPECT_EQ(resultVector->GetValue(4), INT32_MIN);

    delete input;
    delete input2;
    delete bitNotExpr1;
    delete bitNotExpr2;
    delete context1;
    delete context2;
}

TEST(VectorizationTest, BitwiseNotAllNulls) {
    // Test case where all inputs have NULL values
    int rowSize = 4;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, type)};

    auto bitNotExpr = new FuncExpr("bitwise_not", args, type);
    ASSERT_NE(bitNotExpr->vectorFunction, nullptr) << "bitwise_not function not found";

    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {1, 2, 3, 4};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    // Set all values to NULL
    for (int i = 0; i < rowSize; i++) {
        input->Get(0)->SetNull(i);
    }

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitNotExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitwiseNotAllNulls Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // All results should be NULL since input is always NULL
    for (int i = 0; i < rowSize; i++) {
        EXPECT_TRUE(resultVector->IsNull(i));
    }

    delete input;
    delete bitNotExpr;
    delete context;
}

// ========== ShiftLeft Tests ==========

TEST(VectorizationTest, ShiftLeftInt32) {
    int rowSize = 5;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftLeftExpr = new FuncExpr("shiftleft", args, inputType);

    std::vector<DataTypeId> sigArgs = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("shiftleft", sigArgs, OMNI_INT);
    ASSERT_NE(shiftLeftExpr->vectorFunction, nullptr) << "shiftleft function not found";

    int32_t col1[rowSize] = {10, 8, 5, -4, 123};
    int32_t col2[rowSize] = {2, 32, -1, 3, 0};
    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(3);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftLeftExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftLeftInt32 Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 10 << 2);
    EXPECT_EQ(resultVector->GetValue(1), 8 << (32 % 32));
    EXPECT_EQ(resultVector->GetValue(2), static_cast<int32_t>(5U << 31));
    EXPECT_TRUE(resultVector->IsNull(3));
    EXPECT_EQ(resultVector->GetValue(4), 123 << 0);

    delete input;
    delete shiftLeftExpr;
    delete context;
}

// ========== ShiftRight Tests for INT and LONG ==========

/// ShiftRight Function Test
/// shiftright(a, b) -> a >> b
/// Arithmetic right shift with special handling for negative and overflow shift amounts.
/// For INT: shift amount is taken modulo 32
/// For LONG: shift amount is taken modulo 64

TEST(VectorizationTest, ShiftRightInt32) {
    int rowSize = 8;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftRightExpr = new FuncExpr("shiftright", args, inputType);

    std::vector<DataTypeId> sigArgs = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("shiftright", sigArgs, OMNI_INT);
    ASSERT_NE(shiftRightExpr->vectorFunction, nullptr) << "shiftright function not found for INT type";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test cases: normal shift, overflow shift (>=32), negative shift, zero shift, negative value
    int32_t col1[rowSize] = {100, 256, 1024, -16, INT32_MAX, INT32_MIN, 0, 15};
    int32_t col2[rowSize] = {2, 8, 32, 2, 1, 1, 5, 0};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(6);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftRightExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftRightInt32 Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 100 >> 2);       // 100 >> 2 = 25
    EXPECT_EQ(resultVector->GetValue(1), 256 >> 8);       // 256 >> 8 = 1
    EXPECT_EQ(resultVector->GetValue(2), 1024 >> (32 % 32)); // shift by 32 % 32 = 0
    EXPECT_EQ(resultVector->GetValue(3), -16 >> 2);       // -16 >> 2 = -4 (arithmetic shift)
    EXPECT_EQ(resultVector->GetValue(4), INT32_MAX >> 1); // INT32_MAX >> 1
    EXPECT_EQ(resultVector->GetValue(5), INT32_MIN >> 1); // INT32_MIN >> 1 (sign preserved)
    EXPECT_TRUE(resultVector->IsNull(6));                 // NULL handling
    EXPECT_EQ(resultVector->GetValue(7), 15 >> 0);        // 15 >> 0 = 15

    delete input;
    delete shiftRightExpr;
    delete context;
}

TEST(VectorizationTest, ShiftRightInt64) {
    int rowSize = 8;
    auto inputType = std::make_shared<DataType>(OMNI_LONG);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftRightExpr = new FuncExpr("shiftright", args, inputType);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("shiftright", sigArgs, OMNI_LONG);
    ASSERT_NE(shiftRightExpr->vectorFunction, nullptr) << "shiftright function not found for LONG type";

    std::vector vecOfTypes = {LongType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test cases: normal shift, overflow shift (>=64), negative shift, zero shift, negative value
    int64_t col1[rowSize] = {1000LL, 65536LL, 1LL << 40, -32LL, INT64_MAX, INT64_MIN, 0LL, 255LL};
    int32_t col2[rowSize] = {3, 16, 64, 3, 1, 1, 10, 0};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftRightExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftRightInt64 Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 1000LL >> 3);       // 1000 >> 3 = 125
    EXPECT_EQ(resultVector->GetValue(1), 65536LL >> 16);     // 65536 >> 16 = 1
    EXPECT_EQ(resultVector->GetValue(2), (1LL << 40) >> (64 % 64)); // shift by 64 % 64 = 0
    EXPECT_EQ(resultVector->GetValue(3), -32LL >> 3);        // -32 >> 3 = -4 (arithmetic shift)
    EXPECT_TRUE(resultVector->IsNull(4));                    // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), INT64_MIN >> 1);    // INT64_MIN >> 1 (sign preserved)
    EXPECT_EQ(resultVector->GetValue(6), 0LL >> 10);         // 0 >> 10 = 0
    EXPECT_EQ(resultVector->GetValue(7), 255LL >> 0);        // 255 >> 0 = 255

    delete input;
    delete shiftRightExpr;
    delete context;
}

TEST(VectorizationTest, ShiftRightNegativeShiftAmount) {
    // Test negative shift amounts (should be normalized to positive)
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftRightExpr = new FuncExpr("shiftright", args, inputType);
    ASSERT_NE(shiftRightExpr->vectorFunction, nullptr) << "shiftright function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Negative shift amounts: -1 should become 31, -2 should become 30, etc.
    int32_t col1[rowSize] = {INT32_MAX, 1024, 256, 128};
    int32_t col2[rowSize] = {-1, -2, -31, -32};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftRightExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftRightNegativeShiftAmount Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // -1 % 32 + 32 = 31
    EXPECT_EQ(resultVector->GetValue(0), INT32_MAX >> 31);
    // -2 % 32 + 32 = 30
    EXPECT_EQ(resultVector->GetValue(1), 1024 >> 30);
    // -31 % 32 + 32 = 1
    EXPECT_EQ(resultVector->GetValue(2), 256 >> 1);
    // -32 % 32 + 32 = 0 (since -32 % 32 = 0, then 0 + 32 = 32, then 32 % 32 = 0)
    EXPECT_EQ(resultVector->GetValue(3), 128 >> 0);

    delete input;
    delete shiftRightExpr;
    delete context;
}

TEST(VectorizationTest, ShiftRightOverflowShiftAmount) {
    // Test shift amounts >= type bit width
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftRightExpr = new FuncExpr("shiftright", args, inputType);
    ASSERT_NE(shiftRightExpr->vectorFunction, nullptr) << "shiftright function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Shift amounts >= 32 should be taken modulo 32
    int32_t col1[rowSize] = {256, 512, 1024, 2048};
    int32_t col2[rowSize] = {32, 33, 64, 100};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftRightExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftRightOverflowShiftAmount Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 256 >> (32 % 32));  // 256 >> 0 = 256
    EXPECT_EQ(resultVector->GetValue(1), 512 >> (33 % 32));  // 512 >> 1 = 256
    EXPECT_EQ(resultVector->GetValue(2), 1024 >> (64 % 32)); // 1024 >> 0 = 1024
    EXPECT_EQ(resultVector->GetValue(3), 2048 >> (100 % 32)); // 2048 >> 4 = 128

    delete input;
    delete shiftRightExpr;
    delete context;
}

TEST(VectorizationTest, ShiftRightAllNulls) {
    // Test case where all inputs have NULL values
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftRightExpr = new FuncExpr("shiftright", args, inputType);
    ASSERT_NE(shiftRightExpr->vectorFunction, nullptr) << "shiftright function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {100, 200, 300, 400};
    int32_t col2[rowSize] = {1, 2, 3, 4};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    
    // Set all values in first column to NULL
    for (int i = 0; i < rowSize; i++) {
        input->Get(0)->SetNull(i);
    }

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftRightExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftRightAllNulls Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    // All results should be NULL since first operand is always NULL
    for (int i = 0; i < rowSize; i++) {
        EXPECT_TRUE(resultVector->IsNull(i));
    }

    delete input;
    delete shiftRightExpr;
    delete context;
}

TEST(VectorizationTest, ShiftRightArithmeticPreservesSign) {
    // Test that arithmetic right shift preserves the sign bit for negative numbers
    int rowSize = 6;
    auto inputType = std::make_shared<DataType>(OMNI_LONG);
    auto shiftType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, shiftType)};

    auto shiftRightExpr = new FuncExpr("shiftright", args, inputType);
    ASSERT_NE(shiftRightExpr->vectorFunction, nullptr) << "shiftright function not found";

    std::vector vecOfTypes = {LongType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test negative numbers - sign should be preserved
    int64_t col1[rowSize] = {-1LL, -2LL, -8LL, -128LL, INT64_MIN, -1LL};
    int32_t col2[rowSize] = {1, 1, 2, 4, 63, 63};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*shiftRightExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== ShiftRightArithmeticPreservesSign Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), -1LL >> 1);    // -1 >> 1 = -1 (all bits set)
    EXPECT_EQ(resultVector->GetValue(1), -2LL >> 1);    // -2 >> 1 = -1
    EXPECT_EQ(resultVector->GetValue(2), -8LL >> 2);    // -8 >> 2 = -2
    EXPECT_EQ(resultVector->GetValue(3), -128LL >> 4);  // -128 >> 4 = -8
    EXPECT_EQ(resultVector->GetValue(4), INT64_MIN >> 63); // INT64_MIN >> 63 = -1
    EXPECT_EQ(resultVector->GetValue(5), -1LL >> 63);   // -1 >> 63 = -1

    delete input;
    delete shiftRightExpr;
    delete context;
}

// ========== BitGet Tests for BYTE, SHORT, INT, LONG ==========

/// Bit Get Function Test
/// bit_get(num, pos) -> 0 or 1
/// Tests getting the bit value at a specified position.
/// Position 0 is the least significant bit.

TEST(VectorizationTest, BitGetByte) {
    int rowSize = 8;
    auto inputType = std::make_shared<DataType>(OMNI_BYTE);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_BYTE, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bit_get", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found for BYTE type";

    std::vector vecOfTypes = {ByteType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    // Test values: 0b10101010 = -86 (signed), various positions
    int8_t col1[rowSize] = {static_cast<int8_t>(0b10101010), static_cast<int8_t>(0b10101010), 
                            static_cast<int8_t>(0b10101010), static_cast<int8_t>(0b10101010),
                            INT8_MIN, INT8_MAX, 1, 0};
    int32_t col2[rowSize] = {0, 1, 2, 7, 7, 0, 0, 0};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetByte Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);  // bit 0 of 0b10101010 = 0
    EXPECT_EQ(resultVector->GetValue(1), 1);  // bit 1 of 0b10101010 = 1
    EXPECT_EQ(resultVector->GetValue(2), 0);  // bit 2 of 0b10101010 = 0
    EXPECT_EQ(resultVector->GetValue(3), 1);  // bit 7 of 0b10101010 = 1 (sign bit)
    EXPECT_EQ(resultVector->GetValue(4), 1);  // bit 7 of INT8_MIN = 1
    EXPECT_TRUE(resultVector->IsNull(5));     // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), 1);  // bit 0 of 1 = 1
    EXPECT_EQ(resultVector->GetValue(7), 0);  // bit 0 of 0 = 0

    delete input;
    delete bitGetExpr;
    delete context;
}

TEST(VectorizationTest, BitGetShort) {
    int rowSize = 8;
    auto inputType = std::make_shared<DataType>(OMNI_SHORT);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_SHORT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bit_get", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found for SHORT type";

    std::vector vecOfTypes = {ShortType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int16_t col1[rowSize] = {static_cast<int16_t>(0x5555), static_cast<int16_t>(0x5555),
                             static_cast<int16_t>(0xAAAA), static_cast<int16_t>(0xAAAA),
                             INT16_MIN, INT16_MAX, 1, 0};
    int32_t col2[rowSize] = {0, 1, 0, 1, 15, 0, 0, 15};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(4);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetShort Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 1);  // bit 0 of 0x5555 = 1
    EXPECT_EQ(resultVector->GetValue(1), 0);  // bit 1 of 0x5555 = 0
    EXPECT_EQ(resultVector->GetValue(2), 0);  // bit 0 of 0xAAAA = 0
    EXPECT_EQ(resultVector->GetValue(3), 1);  // bit 1 of 0xAAAA = 1
    EXPECT_TRUE(resultVector->IsNull(4));     // NULL handling
    EXPECT_EQ(resultVector->GetValue(5), 1);  // bit 0 of INT16_MAX = 1
    EXPECT_EQ(resultVector->GetValue(6), 1);  // bit 0 of 1 = 1
    EXPECT_EQ(resultVector->GetValue(7), 0);  // bit 15 of 0 = 0

    delete input;
    delete bitGetExpr;
    delete context;
}

TEST(VectorizationTest, BitGetInt) {
    int rowSize = 10;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_INT, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bit_get", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found for INT type";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0b1010, 0b1010, 0b1010, 0b1010, INT32_MIN, INT32_MAX, 1, 0, -1, 0x55555555};
    int32_t col2[rowSize] = {0, 1, 2, 3, 31, 0, 0, 31, 0, 16};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(6);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);  // bit 0 of 0b1010 = 0
    EXPECT_EQ(resultVector->GetValue(1), 1);  // bit 1 of 0b1010 = 1
    EXPECT_EQ(resultVector->GetValue(2), 0);  // bit 2 of 0b1010 = 0
    EXPECT_EQ(resultVector->GetValue(3), 1);  // bit 3 of 0b1010 = 1
    EXPECT_EQ(resultVector->GetValue(4), 1);  // bit 31 of INT32_MIN = 1 (sign bit)
    EXPECT_EQ(resultVector->GetValue(5), 1);  // bit 0 of INT32_MAX = 1
    EXPECT_TRUE(resultVector->IsNull(6));     // NULL handling
    EXPECT_EQ(resultVector->GetValue(7), 0);  // bit 31 of 0 = 0
    EXPECT_EQ(resultVector->GetValue(8), 1);  // bit 0 of -1 = 1
    EXPECT_EQ(resultVector->GetValue(9), 1);  // bit 16 of 0x55555555 = 1

    delete input;
    delete bitGetExpr;
    delete context;
}

TEST(VectorizationTest, BitGetLong) {
    int rowSize = 10;
    auto inputType = std::make_shared<DataType>(OMNI_LONG);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);

    std::vector<DataTypeId> sigArgs = {OMNI_LONG, OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("bit_get", sigArgs, OMNI_BYTE);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found for LONG type";

    std::vector vecOfTypes = {LongType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int64_t col1[rowSize] = {0b1010LL, 0b1010LL, 0b1010LL, 0b1010LL, INT64_MIN, INT64_MAX, 1LL, 0LL, -1LL, 0x5555555555555555LL};
    int32_t col2[rowSize] = {0, 1, 2, 3, 63, 0, 0, 63, 32, 32};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(1)->SetNull(5);  // Set one value to NULL to test NULL handling

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetLong Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 0);  // bit 0 of 0b1010 = 0
    EXPECT_EQ(resultVector->GetValue(1), 1);  // bit 1 of 0b1010 = 1
    EXPECT_EQ(resultVector->GetValue(2), 0);  // bit 2 of 0b1010 = 0
    EXPECT_EQ(resultVector->GetValue(3), 1);  // bit 3 of 0b1010 = 1
    EXPECT_EQ(resultVector->GetValue(4), 1);  // bit 63 of INT64_MIN = 1 (sign bit)
    EXPECT_TRUE(resultVector->IsNull(5));     // NULL handling
    EXPECT_EQ(resultVector->GetValue(6), 1);  // bit 0 of 1 = 1
    EXPECT_EQ(resultVector->GetValue(7), 0);  // bit 63 of 0 = 0
    EXPECT_EQ(resultVector->GetValue(8), 1);  // bit 32 of -1 = 1
    EXPECT_EQ(resultVector->GetValue(9), 1);  // bit 32 of 0x5555... = 1

    delete input;
    delete bitGetExpr;
    delete context;
}

// ========== Additional BitGet boundary tests ==========

TEST(VectorizationTest, BitGetAllZeros) {
    // Test getting bits from zero value
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {0, 0, 0, 0};
    int32_t col2[rowSize] = {0, 15, 30, 31};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetAllZeros Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    // All bits of 0 should be 0
    for (int i = 0; i < rowSize; i++) {
        EXPECT_EQ(resultVector->GetValue(i), 0);
    }

    delete input;
    delete bitGetExpr;
    delete context;
}

TEST(VectorizationTest, BitGetAllOnes) {
    // Test getting bits from -1 (all bits set)
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {-1, -1, -1, -1};
    int32_t col2[rowSize] = {0, 15, 30, 31};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetAllOnes Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    // All bits of -1 should be 1
    for (int i = 0; i < rowSize; i++) {
        EXPECT_EQ(resultVector->GetValue(i), 1);
    }

    delete input;
    delete bitGetExpr;
    delete context;
}

TEST(VectorizationTest, BitGetAllNulls) {
    // Test case where all inputs have NULL values
    int rowSize = 4;
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    auto posType = std::make_shared<DataType>(OMNI_INT);
    auto resultType = std::make_shared<DataType>(OMNI_BYTE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType), new FieldExpr(1, posType)};

    auto bitGetExpr = new FuncExpr("bit_get", args, resultType);
    ASSERT_NE(bitGetExpr->vectorFunction, nullptr) << "bit_get function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    
    int32_t col1[rowSize] = {1, 2, 3, 4};
    int32_t col2[rowSize] = {0, 1, 2, 3};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    
    // Set all values in first column to NULL
    for (int i = 0; i < rowSize; i++) {
        input->Get(0)->SetNull(i);
    }

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.Visit(*bitGetExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== BitGetAllNulls Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int8_t>*>(result);
    // All results should be NULL since first operand is always NULL
    for (int i = 0; i < rowSize; i++) {
        EXPECT_TRUE(resultVector->IsNull(i));
    }

    delete input;
    delete bitGetExpr;
    delete context;
}