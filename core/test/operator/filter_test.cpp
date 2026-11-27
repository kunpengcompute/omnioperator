/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * Description: filter operator test
 */
#include <iostream>
#include <vector>
#include <chrono>

#include "gtest/gtest.h"
#include "operator/filter/filter_and_project.h"
#include "util/test_util.h"
#include "util/config_util.h"
#include "vector/unsafe_vector.h"
#include "vector/string_view.h"
#include "expression/jsonparser/jsonparser.h"
#include "type/data_type_serializer.h"

namespace FilterTest {
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;


const std::string strCompareExpr = "{"
    "  \"exprType\": \"BINARY\","
    "  \"returnType\": 4,"
    "  \"operator\": \"AND\","
    "  \"left\": {"
    "    \"exprType\": \"BINARY\","
    "    \"returnType\": 4,"
    "    \"operator\": \"AND\","
    "    \"left\": {"
    "      \"exprType\": \"UNARY\","
    "      \"returnType\": 4,"
    "      \"operator\": \"not\","
    "      \"expr\": {"
    "        \"exprType\": \"IS_NULL\","
    "        \"returnType\": 4,"
    "        \"arguments\": ["
    "          {"
    "            \"exprType\": \"FIELD_REFERENCE\","
    "            \"dataType\": 15,"
    "            \"colVal\": 0,"
    "            \"width\": 10"
    "          }"
    "        ]"
    "      }"
    "    },"
    "    \"right\": {"
    "      \"exprType\": \"UNARY\","
    "      \"returnType\": 4,"
    "      \"operator\": \"not\","
    "      \"expr\": {"
    "        \"exprType\": \"IS_NULL\","
    "        \"returnType\": 4,"
    "        \"arguments\": ["
    "          {"
    "            \"exprType\": \"FIELD_REFERENCE\","
    "            \"dataType\": 15,"
    "            \"colVal\": 1,"
    "            \"width\": 10"
    "          }"
    "        ]"
    "      }"
    "    }"
    "  },"
    "  \"right\": {"
    "    \"exprType\": \"BINARY\","
    "    \"returnType\": 4,"
    "    \"operator\": \"EQUAL\","
    "    \"left\": {"
    "      \"exprType\": \"FIELD_REFERENCE\","
    "      \"dataType\": 15,"
    "      \"colVal\": 0,"
    "      \"width\": 10"
    "    },"
    "    \"right\": {"
    "      \"exprType\": \"FIELD_REFERENCE\","
    "      \"dataType\": 15,"
    "      \"colVal\": 1,"
    "      \"width\": 10"
    "    }"
    "  }"
    "}";
void PrintValueLine(const double *valueArray, const int32_t arrLength)
{
    std::cout << "[";
    for (int i = 0; i < arrLength; ++i) {
        if (i == arrLength - 1) {
            std::cout << valueArray[i];
        } else {
            std::cout << valueArray[i];
            std::cout << ",";
        }
    }
    std::cout << "]" << std::endl;
}

bool CheckOutput(VectorBatch *t, const int32_t numRows, bool (*filter)(VectorBatch *, int32_t))
{
    for (int32_t i = 0; i < numRows; i++) {
        if (!filter(t, i)) {
            return false;
        }
    }
    return true;
}

// Expects 1 column of type int32
bool Filter1(VectorBatch *t, int32_t index)
{
    int n = 4;
    return (reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(index) <= n;
}

// Expects 2 columns of type int32, int64
bool Filter2(VectorBatch *t, int32_t index)
{
    int32_t val1 = reinterpret_cast<Vector<int32_t> *>(t->Get(0))->GetValue(index);
    int64_t val2 = reinterpret_cast<Vector<int64_t> *>(t->Get(1))->GetValue(index);
    // true if both values are negative
    return val1 < 0 && val2 < 0;
}

// Expects 3 columns of type int32, int64, double
bool Filter3(VectorBatch *t, int32_t index)
{
    int32_t val1 = reinterpret_cast<Vector<int32_t> *>(t->Get(0))->GetValue(index);
    int64_t val2 = reinterpret_cast<Vector<int64_t> *>(t->Get(1))->GetValue(index);
    double val3 = reinterpret_cast<Vector<double> *>(t->Get(2))->GetValue(index);
    // first val is multiple of 3, second val = 3 billion, third val >= 0.4.
    return val1 % 3 == 0 && val2 == static_cast<int64_t>(3e9) && val3 >= 0.4;
}

bool Filter4(VectorBatch *t, int32_t index)
{
    int v0 = 1;
    int v2 = 4800;
    double v4 = 50.8;
    int v5 = 52;
    int32_t val0 = reinterpret_cast<Vector<int32_t> *>(t->Get(0))->GetValue(index);
    int32_t val2 = reinterpret_cast<Vector<int32_t> *>(t->Get(1))->GetValue(index);
    double val4 = reinterpret_cast<Vector<double> *>(t->Get(2))->GetValue(index);
    int64_t val5 = reinterpret_cast<Vector<int64_t> *>(t->Get(3))->GetValue(index);
    return (val0 != v0 && val2 > v2 && val4 < v4) || val5 >= v5;
}

bool Filter5(VectorBatch *t, int32_t index)
{
    int64_t v0 = 0;
    int64_t v1 = -3e9;
    int32_t v2 = -12;
    int32_t v3 = 50;
    // Project order reversedExpressionEvaluator
    int64_t val0 = reinterpret_cast<Vector<int64_t> *>(t->Get(0))->GetValue(index);
    int64_t val1 = reinterpret_cast<Vector<int64_t> *>(t->Get(1))->GetValue(index);
    int32_t val2 = reinterpret_cast<Vector<int32_t> *>(t->Get(2))->GetValue(index);
    int32_t val3 = reinterpret_cast<Vector<int32_t> *>(t->Get(3))->GetValue(index);
    return (val0 >= v0 || val1 <= v1) && (val2 == v2 || val3 < v3);
}

// Expects 1 column of type Decimal128
bool Filter6(VectorBatch *t, int32_t index)
{
    int32_t n = 500000;
    Decimal128 val = reinterpret_cast<Vector<Decimal128> *>(t->Get(0))->GetValue(index);
    return Decimal128(val.HighBits(), val.LowBits()) <= n;
}

TEST(FilterTest, LessThanProcessRow)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numCols = 1;
    const int32_t numRows = 1;
    int32_t dataCol[numRows] = {0};

    int64_t valueAddrs[numCols] = {reinterpret_cast<int64_t>(dataCol)};
    int32_t inputLens[numCols]= {0};

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    const int32_t projectCount = 1;
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()) };
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(2000, IntType()), BooleanType());
    auto *overflowConfig = new OverflowConfig();

    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    exprEvaluator->SetSupportCodegen(true);
    OperatorFactory *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = dynamic_cast<FilterAndProjectOperator *>(factory->CreateOperator(nullptr));

    int64_t outValueAddrs[projectCount];
    int32_t outLens[projectCount];
    bool filterPass = op->ProcessRow(valueAddrs, inputLens, outValueAddrs, outLens);
    EXPECT_TRUE(filterPass);
    EXPECT_EQ(outValueAddrs[0], valueAddrs[0]);
    EXPECT_EQ(outLens[0], 0);

    dataCol[0] = 2000;
    filterPass = op->ProcessRow(valueAddrs, inputLens, outValueAddrs, outLens);
    EXPECT_FALSE(filterPass);

    dataCol[0] = 1000;
    inputLens[0] = -1;
    filterPass = op->ProcessRow(valueAddrs, inputLens, outValueAddrs, outLens);
    EXPECT_FALSE(filterPass);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, ExprCodegenStringCompare)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    ConfigUtil::SetIsComplexTypeWithCodegen(IsComplexTypeWithCodegen::TRUE);
    const int32_t numRows = 10000000;

    // prepare expression isnotnull(a) and isnotnull(b) and (a = b)
    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() }));
    auto field0 = new FieldExpr(0, VarcharType());
    auto field1 = new FieldExpr(1, VarcharType());
    Expr *filterExpr = JSONParser::ParseJSON(nlohmann::json::parse(strCompareExpr));
    std::vector<Expr *> projections = { field0, field1 };

    // prepare test data
    auto vec1 = new Vector<LargeStringContainer<std::string_view>>(numRows);
    auto vec2 = new Vector<LargeStringContainer<std::string_view>>(numRows);
    auto *expectedData1 = new std::string[numRows];
    auto *expectedData2 = new std::string[numRows];
    auto expectSelectedRows = 0;

    string_view str0 = "31";
    string_view str1 = "2413";

    for (int i = 0; i < numRows; ++i) {
        if (i % 2 == 0) {
            vec1->SetValue(i, str0);
        } else {
            vec1->SetValue(i, str1);
        }
        vec2->SetValue(i, str1);
        if ((vec1->GetValue(i) == vec2->GetValue(i)) && (!vec1->IsNull(i) && !vec2->IsNull(i))) {
            expectedData1[expectSelectedRows] = vec1->GetValue(i);
            expectedData2[expectSelectedRows] = vec2->GetValue(i);
            expectSelectedRows++;
        }
    }

    auto vecBatch = new VectorBatch(numRows);
    vecBatch->Append(vec1);
    vecBatch->Append(vec2);

    // prepare filter factory and operator
    auto createStart = std::chrono::high_resolution_clock::now();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(std::move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    auto createEnd = std::chrono::high_resolution_clock::now();
    std::cout << "The time of creating operator and factory is " <<
        std::chrono::duration_cast<std::chrono::microseconds>(createEnd - createStart).count() << std::endl;

    // evaluate expression
    auto evaluateStart = std::chrono::high_resolution_clock::now();
    op->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    auto evaluateEnd = std::chrono::high_resolution_clock::now();
    std::cout << "The time of evaluate expression is " <<
        std::chrono::duration_cast<std::chrono::microseconds>(evaluateEnd - evaluateStart).count() << std::endl;

    // verify correctness
    std::cout << "The numReturned is " << numReturned << std::endl;
    EXPECT_EQ(numReturned, expectSelectedRows);
    AssertVecBatchEquals(outputVecBatch, 2, expectSelectedRows, expectedData1, expectedData2);

    // delete
    if (outputVecBatch != nullptr) {
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
    delete[] expectedData1;
    delete[] expectedData2;
}

TEST(FilterTest, LessThan)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()) };
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(2000, IntType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val < 2000);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Shorts_LessThan)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 32768;
    int16_t *col1 = new int16_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = static_cast<int16_t>(i);
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ ShortType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<Expr *> projections = { new FieldExpr(0, ShortType()) };
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, ShortType()),
        new LiteralExpr(static_cast<int16_t>(2000), ShortType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);
    for (int32_t i = 0; i < numReturned; i++) {
        int16_t val = (reinterpret_cast<Vector<int16_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val < 2000);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Bytes_LessThan)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 256;
    int8_t *col1 = new int8_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = static_cast<int8_t>(i);
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ ByteType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<Expr *> projections = { new FieldExpr(0, ByteType()) };
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, ByteType()),
        new LiteralExpr(static_cast<int8_t>(20), ByteType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 148);
    for (int32_t i = 0; i < numReturned; i++) {
        int8_t val = (reinterpret_cast<Vector<int8_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val < 20);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, LessThanWihtoutParsing)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *column = new FieldExpr(0, IntType());
    auto *left = new FieldExpr(0, IntType());
    auto *right = new LiteralExpr(2000, IntType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, left, right, BooleanType());
    std::vector<Expr *> projections = { column };
    auto *overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val < 2000);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, GreaterThan)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 25;
        col2[i] = 3e9;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1, col2);

    auto *col0Expr = new FieldExpr(0, IntType());
    auto *col1Expr = new FieldExpr(1, LongType());
    std::vector<Expr *> projections = { col0Expr, col1Expr };

    auto *gtLeft = new FieldExpr(0, IntType());
    auto *gtRight = new LiteralExpr(20, IntType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::GT, gtLeft, gtRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 800);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_TRUE(val0 > 20);
        EXPECT_EQ(val1, 3e9L);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Shorts_GreaterThan)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 32768;
    int16_t *col1 = new int16_t[numRows];
    int64_t *col2 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = static_cast<int16_t>(i % 25);
        col2[i] = 3e9;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({ ShortType(), LongType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1, col2);

    auto *col0Expr = new FieldExpr(0, ShortType());
    auto *col1Expr = new FieldExpr(1, LongType());
    std::vector<Expr *> projections = { col0Expr, col1Expr };

    auto *gtLeft = new FieldExpr(0, ShortType());
    auto *gtRight = new LiteralExpr(static_cast<int16_t>(20), ShortType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::GT, gtLeft, gtRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 5240);
    for (int32_t i = 0; i < numReturned; i++) {
        int16_t val0 = (reinterpret_cast<Vector<int16_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_TRUE(val0 > 20);
        EXPECT_EQ(val1, 3e9L);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Bytes_GreaterThan)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 256;
    int8_t *col1 = new int8_t[numRows];
    int64_t *col2 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = static_cast<int8_t>(i % 25);
        col2[i] = 3e9;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({ ByteType(), LongType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1, col2);

    auto *col0Expr = new FieldExpr(0, ByteType());
    auto *col1Expr = new FieldExpr(1, LongType());
    std::vector<Expr *> projections = { col0Expr, col1Expr };

    auto *gtLeft = new FieldExpr(0, ByteType());
    auto *gtRight = new LiteralExpr(static_cast<int8_t>(20), ByteType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::GT, gtLeft, gtRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 40);
    for (int32_t i = 0; i < numReturned; i++) {
        int8_t val0 = (reinterpret_cast<Vector<int8_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_TRUE(val0 > 20);
        EXPECT_EQ(val1, 3e9L);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, EqualTo)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new double[numRows];
    auto *col3 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col2[i] = i % 100;
        col3[i] = i % 100;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType(), DoubleType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1, col3, col2);

    auto *col1Expr = new FieldExpr(1, LongType());
    auto *col2Expr = new FieldExpr(2, DoubleType());
    std::vector<Expr *> projections = { col2Expr, col1Expr };

    auto *eqLeft = new FieldExpr(2, DoubleType());
    auto *eqRight = new LiteralExpr(50.0, DoubleType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, eqRight, BooleanType());

    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 50);
    for (int32_t i = 0; i < numReturned; i++) {
        double val0 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val0, 50);
        EXPECT_EQ(val0, val1);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, GreaterThanOrEqualTo)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col2[i] = (i * (i + 2)) % 40;
        col1[i] = i;
        if (i % 45 == 0) {
            col2[i] = 30;
        }
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1, col2);

    auto *col1Expr = new FieldExpr(1, IntType());
    std::vector<Expr *> projections = { col1Expr };

    auto *gteLeft = new FieldExpr(1, IntType());
    auto *gteRight = new LiteralExpr(30, IntType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft, gteRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 834);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 >= 30);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, NotEqualTo)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto *col1 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ DoubleType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *col0Expr = new FieldExpr(0, DoubleType());
    std::vector<Expr *> projections = { col0Expr };
    auto *neqLeft = new FieldExpr(0, DoubleType());
    auto *neqRight = new LiteralExpr(0, DoubleType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::NEQ, neqLeft, neqRight, BooleanType());

    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 4999);
    double cnt = 1;
    for (int32_t i = 0; i < numReturned; i++) {
        double val0 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, cnt++);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Shorts_NotEqualTo)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 32768;
    int16_t *col1 = new int16_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = static_cast<int16_t>(i);
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ ShortType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *col0Expr = new FieldExpr(0, ShortType());
    std::vector<Expr *> projections = { col0Expr };
    auto *neqLeft = new FieldExpr(0, ShortType());
    auto *neqRight = new LiteralExpr(0, ShortType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::NEQ, neqLeft, neqRight, BooleanType());

    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 32767);
    int16_t cnt = 1;
    for (int32_t i = 0; i < numReturned; i++) {
        int16_t val0 = (reinterpret_cast<Vector<int16_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_NE(val0, 0);
        EXPECT_EQ(val0, cnt++);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Bytes_NotEqualTo)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 256;
    int8_t *col1 = new int8_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = static_cast<int8_t>(i);
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ ByteType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *col0Expr = new FieldExpr(0, ByteType());
    std::vector<Expr *> projections = { col0Expr };
    auto *neqLeft = new FieldExpr(0, ByteType());
    auto *neqRight = new LiteralExpr(0, ByteType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::NEQ, neqLeft, neqRight, BooleanType());

    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 255);
    int8_t cnt = 1;
    for (int32_t i = 0; i < numReturned; i++) {
        int8_t val0 = (reinterpret_cast<Vector<int8_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_NE(val0, 0);
        EXPECT_EQ(val0, cnt++);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, AllPass)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 20000;
    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 9348;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *col0Expr = new FieldExpr(0, IntType());
    std::vector<Expr *> projections = { col0Expr };
    auto *eqLeft = new FieldExpr(0, IntType());
    auto *eqRight = new LiteralExpr(9348, IntType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, eqRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 20000);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, MultipleInputs)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *data1 = new int32_t[numRows];
    auto *data2 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        data1[i] = i % 10;
        data2[i] = i % 6 + 1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, data1);

    auto *col0Expr = new FieldExpr(0, IntType());
    std::vector<Expr *> projections = { col0Expr };

    auto *lteLeft = new FieldExpr(0, IntType());
    auto *lteRight = new LiteralExpr(4, IntType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LTE, lteLeft, lteRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch1 = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch1);
    EXPECT_TRUE(CheckOutput(outputVecBatch1, numReturned, Filter1));
    EXPECT_EQ(numReturned, 500);

    VectorBatch *in2 = CreateVectorBatch(inputTypes, numRows, data2);
    op->AddInput(in2);
    VectorBatch *outputVecBatch2 = nullptr;
    numReturned = op->GetOutput(&outputVecBatch2);
    EXPECT_TRUE(CheckOutput(outputVecBatch2, numReturned, Filter1));
    EXPECT_EQ(numReturned, 668);

    VectorHelper::FreeVecBatch(outputVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch2);
    delete[] data1;
    delete[] data2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, NegativeValues)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *data1 = new int32_t[numRows];
    auto *data2 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        data1[i] = i * i % 100 + 1;
        if (i % 5 == 0) {
            data1[i] = -data1[i];
        }
        data2[i] = i % 100 + 3e9;
        if (i % 7 == 0) {
            data2[i] = -data2[i];
        }
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, data1, data2);

    auto *col0Expr = new FieldExpr(0, IntType());
    auto *col1Expr = new FieldExpr(1, LongType());
    std::vector<Expr *> projections = { col0Expr, col1Expr };

    auto *lte1Left = new FieldExpr(0, IntType());
    auto *lte1Right = new LiteralExpr(-1, IntType());
    auto *lte1Expr = new BinaryExpr(omniruntime::expressions::Operator::LTE, lte1Left, lte1Right, BooleanType());

    auto *lte2Left = new FieldExpr(1, LongType());
    auto *lte2Right = new LiteralExpr(-1L, LongType());
    auto *lte2Expr = new BinaryExpr(omniruntime::expressions::Operator::LTE, lte2Left, lte2Right, BooleanType());

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, lte1Expr, lte2Expr, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_TRUE(CheckOutput(outputVecBatch, numReturned, Filter2));
    // Both values are negative for every multiple of 35.
    EXPECT_EQ(numReturned, 286);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] data1;
    delete[] data2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, AllTypes)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *data1 = new int32_t[numRows];
    auto *data2 = new int64_t[numRows];
    auto *data3 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        data1[i] = i % 3;
        data2[i] = (i % 2 != 0) ? 3e9 : 0;
        data3[i] = i % 10 / 10.0;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType(), DoubleType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, data1, data2, data3);

    auto *col0Expr = new FieldExpr(0, IntType());
    auto *col1Expr = new FieldExpr(1, LongType());
    auto *col2Expr = new FieldExpr(2, DoubleType());
    std::vector<Expr *> projections = { col0Expr, col1Expr, col2Expr };

    // create the filter expression object
    auto *eq2Left = new FieldExpr(1, LongType());
    auto *eq2Right = new LiteralExpr(3000000000L, LongType());
    auto *eq2Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eq2Left, eq2Right, BooleanType());

    auto *gteLeft = new FieldExpr(2, DoubleType());
    auto *gteRight = new LiteralExpr(0.4, DoubleType());
    auto *gteExpr = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft, gteRight, BooleanType());

    auto *innerAndExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq2Expr, gteExpr, BooleanType());

    auto *eq1Left = new FieldExpr(0, IntType());
    auto *eq1Right = new LiteralExpr(0, IntType());
    auto *eq1Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eq1Left, eq1Right, BooleanType());

    Expr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq1Expr, innerAndExpr, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_TRUE(CheckOutput(outputVecBatch, numReturned, Filter3));
    EXPECT_EQ(numReturned, 100);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] data1;
    delete[] data2;
    delete[] data3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Compile)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t dataSize = 10000;
    auto *data1 = new double[dataSize];
    auto *data2 = new int32_t[dataSize];
    auto *data3 = new double[dataSize];
    auto *data4 = new double[dataSize];
    for (int32_t i = 0; i < dataSize; ++i) {
        data4[i] = i;
        data3[i] = i % 10 / 100.0;
        data1[i] = i % 26;
        data2[i] = 6;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ DoubleType(), IntType(), DoubleType(), DoubleType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, dataSize, data1, data2, data3, data4);

    // TPCH 6
    auto *col0Expr = new FieldExpr(0, DoubleType());
    std::vector<Expr *> projections = { col0Expr };

    auto *gtRight = new LiteralExpr(8766.0, DoubleType());
    auto *gtExpr =
        new BinaryExpr(omniruntime::expressions::Operator::GT, new FieldExpr(3, DoubleType()), gtRight, BooleanType());

    auto *lt1Right = new LiteralExpr(9131.0, DoubleType());
    auto *lt1Expr =
        new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(3, DoubleType()), lt1Right, BooleanType());
    auto *and1Expression = new BinaryExpr(omniruntime::expressions::Operator::AND, gtExpr, lt1Expr, BooleanType());

    auto *lt2Right = new LiteralExpr(24.0, DoubleType());
    auto *lt2expr =
        new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, DoubleType()), lt2Right, BooleanType());

    auto *data = new FieldExpr(2, DoubleType());
    auto *lower = new LiteralExpr(0.05, DoubleType());
    auto *upper = new LiteralExpr(0.07, DoubleType());
    std::vector<Expr *> args;
    auto *betweenExpr = new BetweenExpr(data, lower, upper);
    auto *and2Expression = new BinaryExpr(omniruntime::expressions::Operator::AND, betweenExpr, lt2expr, BooleanType());

    auto *filterExpr =
        new BinaryExpr(omniruntime::expressions::Operator::AND, and1Expression, and2Expression, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numSelectedRows = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numSelectedRows, 100);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] data1;
    delete[] data2;
    delete[] data3;
    delete[] data4;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, LogicalOperators1)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];
    auto *col4 = new int64_t[numRows];
    auto *col5 = new double[numRows];
    auto *col6 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 3 != 0 ? 1 : 0;
        col2[i] = col3[i] = i;
        col4[i] = i % 2 != 0 ? 2999999999 : 3e9;
        col5[i] = 50 + i / 10.0;
        col6[i] = i % 55;
    }

    // int int int long double long
    DataTypes inputTypes(
        std::vector<DataTypePtr>({ IntType(), IntType(), IntType(), LongType(), DoubleType(), LongType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3, col4, col5, col6);

    // projection objects:
    auto *col0Expr = new FieldExpr(0, IntType());
    auto *col2Expr = new FieldExpr(2, IntType());
    auto *col4Expr = new FieldExpr(4, DoubleType());
    auto *col5Expr = new FieldExpr(5, LongType());
    std::vector<Expr *> projections = { col0Expr, col2Expr, col4Expr, col5Expr };

    auto *eqRight = new LiteralExpr(3000000000L, LongType());
    auto *eqExpr =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(3, LongType()), eqRight, BooleanType());
    auto *neqExpr = new BinaryExpr(omniruntime::expressions::Operator::NEQ, new FieldExpr(0, IntType()),
        new LiteralExpr(1, IntType()), BooleanType());
    auto *andExpr1 = new BinaryExpr(omniruntime::expressions::Operator::AND, neqExpr, eqExpr, BooleanType());

    auto *gtExpr = new BinaryExpr(omniruntime::expressions::Operator::GT, new FieldExpr(2, IntType()),
        new LiteralExpr(4800, IntType()), BooleanType());
    auto *lteExpr = new BinaryExpr(omniruntime::expressions::Operator::LTE, new FieldExpr(1, IntType()),
        new LiteralExpr(9990, IntType()), BooleanType());
    auto *andExpr2 = new BinaryExpr(omniruntime::expressions::Operator::AND, gtExpr, lteExpr, BooleanType());

    auto *andExpr3 = new BinaryExpr(omniruntime::expressions::Operator::AND, andExpr2, andExpr1, BooleanType());

    auto *ltRight = new LiteralExpr(50.8, DoubleType());
    auto *ltExpr =
        new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(4, DoubleType()), ltRight, BooleanType());
    auto *andExpr4 = new BinaryExpr(omniruntime::expressions::Operator::AND, ltExpr, andExpr3, BooleanType());

    auto *gteRight = new LiteralExpr(52L, LongType());
    auto *gteExpr =
        new BinaryExpr(omniruntime::expressions::Operator::GTE, new FieldExpr(5, LongType()), gteRight, BooleanType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::OR, gteExpr, andExpr4, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 543);
    EXPECT_TRUE(CheckOutput(outputVecBatch, numReturned, Filter4));

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    delete[] col4;
    delete[] col5;
    delete[] col6;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, LogicalOperators2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int64_t[numRows];
    auto *col4 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 100;
        col2[i] = i % 7 == 0 ? -12 : i;
        col3[i] = i % 8 == 0 ? -i - 3e9 : i + 3e9;
        col4[i] = i % 9 - 4;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), LongType(), LongType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3, col4);

    // projections
    auto *col0Expr = new FieldExpr(0, IntType());
    auto *col1Expr = new FieldExpr(1, IntType());
    auto *col2Expr = new FieldExpr(2, LongType());
    auto *col3Expr = new FieldExpr(3, LongType());

    std::vector<Expr *> projections = { col3Expr, col2Expr, col1Expr, col0Expr };

    auto *lteRight = new LiteralExpr(-3000000000L, LongType());
    auto *lteExpr =
        new BinaryExpr(omniruntime::expressions::Operator::LTE, new FieldExpr(2, LongType()), lteRight, BooleanType());
    auto *gteRight = new LiteralExpr(0L, LongType());
    auto *gteExpr =
        new BinaryExpr(omniruntime::expressions::Operator::GTE, new FieldExpr(3, LongType()), gteRight, BooleanType());
    auto *orExpr1 = new BinaryExpr(omniruntime::expressions::Operator::OR, lteExpr, gteExpr, BooleanType());

    auto *ltExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(50, IntType()), BooleanType());
    auto *eqExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(1, IntType()),
        new LiteralExpr(-12, IntType()), BooleanType());
    auto *orExpr2 = new BinaryExpr(omniruntime::expressions::Operator::OR, ltExpr, eqExpr, BooleanType());

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, orExpr1, orExpr2, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 3498);
    EXPECT_TRUE(CheckOutput(outputVecBatch, numReturned, Filter5));

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    delete[] col4;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, LogicalOperators3)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 0;
        col2[i] = 1;
    }
    col1[0] = 0;
    col1[1] = 1;
    col1[2] = 1;
    col1[3] = 2;
    col1[4] = 3;
    col1[5] = 5;
    col1[6] = 8;
    col1[7] = 13;
    col2[2] = 0;

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    // projections
    auto *col0Expr = new FieldExpr(0, IntType());
    auto *col1Expr = new FieldExpr(1, IntType());
    std::vector<Expr *> projections = { col0Expr, col1Expr };

    auto *eq1Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(55, IntType()),
        new FieldExpr(0, IntType()), BooleanType());
    auto *eq2Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(5, IntType()),
        new FieldExpr(0, IntType()), BooleanType());
    auto *or1Expr = new BinaryExpr(omniruntime::expressions::Operator::OR, eq1Expr, eq2Expr, BooleanType());

    auto *eq3Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(8, IntType()), BooleanType());
    auto *or2Expr = new BinaryExpr(omniruntime::expressions::Operator::OR, or1Expr, eq3Expr, BooleanType());

    auto *eq4Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(13, IntType()), BooleanType());
    auto *or3Expr = new BinaryExpr(omniruntime::expressions::Operator::OR, or2Expr, eq4Expr, BooleanType());


    auto *eq5Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(1, IntType()), BooleanType());
    auto *eq6Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(2, IntType()), BooleanType());
    auto *or4Expr = new BinaryExpr(omniruntime::expressions::Operator::OR, eq5Expr, eq6Expr, BooleanType());

    auto *eq7Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(3, IntType()), BooleanType());
    auto *or5Expr = new BinaryExpr(omniruntime::expressions::Operator::OR, or4Expr, eq7Expr, BooleanType());

    auto *or6Expr = new BinaryExpr(omniruntime::expressions::Operator::OR, or5Expr, or3Expr, BooleanType());

    auto *neqRight = new LiteralExpr(0, IntType());
    auto *neqExpr =
        new BinaryExpr(omniruntime::expressions::Operator::NEQ, new FieldExpr(1, IntType()), neqRight, BooleanType());

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, neqExpr, or6Expr, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 6);
    for (int32_t i = 0; i < 6; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_TRUE(val0 != 0);
        EXPECT_TRUE(val1 != 0);
        EXPECT_TRUE(val1 == 1);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, ArithmeticAdd)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    auto t = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()) };

    // filter
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, new FieldExpr(0, IntType()),
        new LiteralExpr(1, IntType()), IntType());
    auto *filterExpr =
        new BinaryExpr(omniruntime::expressions::Operator::GT, addExpr, new LiteralExpr(4, IntType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 + 1 > 4);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, ArithmeticSubtract)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, LongType()) };

    auto *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, new FieldExpr(0, IntType()),
        new LiteralExpr(5, IntType()), IntType());
    auto *filterExpr =
        new BinaryExpr(omniruntime::expressions::Operator::LT, new LiteralExpr(0, IntType()), subExpr, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 4000);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 - 5 > 0);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, ArithmeticMultiply)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 2;
        col2[i] = i % 10 + 1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    auto *mul1Expr = new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(0, IntType()),
        new FieldExpr(0, IntType()), IntType());
    auto *eqExpr =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(0, IntType()), mul1Expr, BooleanType());

    auto *mulLeft = new LiteralExpr(2L, LongType());
    auto *mul2Expr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, new FieldExpr(1, LongType()), LongType());
    auto *gtLeft = new LiteralExpr(7L, LongType());
    auto *gtExpr = new BinaryExpr(omniruntime::expressions::Operator::GT, gtLeft, mul2Expr, BooleanType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eqExpr, gtExpr, BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, LongType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val0, 0);
        EXPECT_TRUE(val1 * 2 < 7);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Conditional)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 2;
        col2[i] = 50;
        col3[i] = 100;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
        new FieldExpr(2, IntType()) };

    // filters
    auto *condition = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(0, IntType()), BooleanType());
    auto *texp = new BinaryExpr(omniruntime::expressions::Operator::ADD, new FieldExpr(1, IntType()),
        new LiteralExpr(5, IntType()), IntType());
    auto *fexp = new FieldExpr(2, IntType());

    auto *eqLeft = new IfExpr(condition, texp, fexp);

    auto *filterExpr =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, new LiteralExpr(55, IntType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 5000);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Conditional2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 2;
        col2[i] = i % 5;
        col3[i] = i % 10;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    // filters
    auto *condition = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new LiteralExpr(0, IntType()), BooleanType());
    auto *texp = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(1, IntType()),
        new LiteralExpr(3, IntType()), BooleanType());
    auto *fexp = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(1, IntType()),
        new LiteralExpr(4, IntType()), BooleanType());
    auto *ifExpr = new IfExpr(condition, texp, fexp);

    auto *gtExpr = new BinaryExpr(omniruntime::expressions::Operator::GT, new FieldExpr(2, IntType()),
        new LiteralExpr(3, IntType()), BooleanType());

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, ifExpr, gtExpr, BooleanType());

    // filters
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, In)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i % 5;
        col3[i] = i % 6 + 12;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    // filter
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, IntType()));
    args.push_back(new LiteralExpr(1, IntType()));
    args.push_back(new LiteralExpr(3, IntType()));
    args.push_back(new LiteralExpr(5, IntType()));

    auto *filterExpr = new InExpr(args);

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 3000);
    for (int i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 == 1 || val0 == 3 || val0 == 5);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, testLongIn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 100;
    auto *col1 = new int64_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i % 5;
        col3[i] = i % 6 + 12;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<Expr *> args;
    int64_t target1 = 1;
    int64_t target2 = 3;
    int64_t target3 = 5;
    args.push_back(new FieldExpr(0, LongType()));
    args.push_back(new LiteralExpr(target1, LongType()));
    args.push_back(new LiteralExpr(target2, LongType()));
    args.push_back(new LiteralExpr(target3, LongType()));

    auto *filterExpr = new InExpr(args);

    std::vector<Expr *> projections = { new FieldExpr(0, LongType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, LongType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 30);
    for (int i = 0; i < numReturned; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 == target1 || val0 == target2 || val0 == target3);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, testDoubleIn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new double[numRows];
    auto *col2 = new double[numRows];
    auto *col3 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i % 5;
        col3[i] = i % 6 + 12;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType(), DoubleType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    // filter
    std::vector<Expr *> args;
    double target1 = 1.0;
    double target2 = 3.0;
    double target3 = 5.0;
    args.push_back(new FieldExpr(0, DoubleType()));
    args.push_back(new LiteralExpr(target1, DoubleType()));
    args.push_back(new LiteralExpr(target2, DoubleType()));
    args.push_back(new LiteralExpr(target3, DoubleType()));

    auto *filterExpr = new InExpr(args);

    std::vector<Expr *> projections = { new FieldExpr(0, DoubleType()), new FieldExpr(1, DoubleType()),
        new FieldExpr(2, DoubleType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 3000);
    for (int i = 0; i < numReturned; i++) {
        double val0 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 == target1 || val0 == target2 || val0 == target3);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, testStringIn1)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(10) }));

    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        if (i % 3 == 0) {
            value = "hello";
        } else {
            value = "hi";
        }
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    // filter
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, VarcharType()));
    args.push_back(new LiteralExpr(new std::string("hello"), VarcharType()));
    args.push_back(new LiteralExpr(new std::string("bye"), VarcharType()));
    args.push_back(new LiteralExpr(new std::string("okay"), VarcharType()));

    auto *filterExpr = new InExpr(args);

    std::vector<Expr *> projections = { new FieldExpr(0, VarcharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 4);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, testStringIn2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    DataTypes inputTypes(std::vector<DataTypePtr>({ CharType(10) }));
    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            value = "hello";
        } else {
            value = "hi";
        }
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    // filter
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, CharType()));
    args.push_back(new LiteralExpr(new std::string("hello"), CharType()));
    args.push_back(new LiteralExpr(new std::string("bye"), CharType()));
    args.push_back(new LiteralExpr(new std::string("okay"), CharType()));

    auto *filterExpr = new InExpr(args);

    std::vector<Expr *> projections = { new FieldExpr(0, CharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 5);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, testDecimal128In)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *data1 = new int64_t[numRows * 2];
    for (int64_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1000;
        data1[2 * i + 1] = 0;
    }

    std::vector<DataTypePtr> vecOfTypes = { Decimal128Type() };
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, data1);

    // filter
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, Decimal128Type(38, 0)));
    args.push_back(new LiteralExpr(new std::string("1000"), Decimal128Type(38, 0)));
    args.push_back(new LiteralExpr(new std::string("2000"), Decimal128Type(38, 0)));
    args.push_back(new LiteralExpr(new std::string("555555"), Decimal128Type(38, 0)));

    auto *filterExpr = new InExpr(args);

    std::vector<Expr *> projections = { new FieldExpr(0, Decimal128Type(38, 0)) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] data1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Between)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
        col2[i] = i % 11;
        col3[i] = (i % 21) - 3;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    auto *filterExpr =
        new BetweenExpr(new FieldExpr(1, IntType()), new FieldExpr(0, IntType()), new FieldExpr(2, IntType()));
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 4705);
    for (int i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        int32_t val2 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(2)))->GetValue(i);
        EXPECT_TRUE((val0 <= val1) && (val1 <= val2));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, NotEqualToAbs)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 100000;
    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i - 32435;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    // filter
    DataTypePtr retType = IntType();
    std::string funcStr = "abs";
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, IntType()));
    auto absExpr = GetFuncExpr(funcStr, args, IntType());

    auto filterExpr =
        new BinaryExpr(omniruntime::expressions::Operator::NEQ, absExpr, new LiteralExpr(4, IntType()), BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 99998);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

// Function tests
TEST(FilterTest, MathFunctionFilter1)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];

    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 2;
        col2[i] = i % 5;
        col3[i] = -1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    // filters
    DataTypePtr retType = IntType();
    std::string funcStr = "abs";
    std::vector<Expr *> args1;
    args1.push_back(new FieldExpr(0, IntType()));
    auto abs1Expr = GetFuncExpr(funcStr, args1, IntType());

    std::vector<Expr *> args2;
    args2.push_back(new FieldExpr(2, IntType()));
    auto abs2Expr = GetFuncExpr(funcStr, args2, IntType());
    auto eq1Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, abs1Expr, abs2Expr, BooleanType());

    std::vector<Expr *> args3;
    args3.push_back(new FieldExpr(0, IntType()));
    auto abs3Expr = GetFuncExpr(funcStr, args3, IntType());

    std::vector<Expr *> args4;
    args4.push_back(new FieldExpr(1, IntType()));
    auto abs4Expr = GetFuncExpr(funcStr, args4, IntType());
    auto eq2Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, abs3Expr, abs4Expr, BooleanType());

    auto filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq1Expr, eq2Expr, BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);

    EXPECT_EQ(numReturned, 1000);
    for (int i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        int32_t val2 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(2)))->GetValue(i);
        EXPECT_TRUE((std::abs(val0) == std::abs(val1)) && (std::abs(val1) == std::abs(val2)));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}


// For testing different types
TEST(FilterTest, MathFunctionFilter2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int32_t[numRows];

    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 2;
        col2[i] = i % 5;
        col3[i] = -1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    // filters
    std::string castStr = "CAST";
    DataTypePtr retType = DoubleType();
    std::vector<Expr *> args1;
    args1.push_back(new FieldExpr(0, IntType()));
    auto cast1 = GetFuncExpr(castStr, args1, DoubleType());

    std::vector<Expr *> args2;
    args2.push_back(new FieldExpr(1, LongType()));
    auto cast2 = GetFuncExpr(castStr, args2, DoubleType());

    auto filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, cast1, cast2, BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2000);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, FilterString1)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(30) }));
    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        if (i % 40 == 0) {
            value = "hello";
        } else {
            value = "abcdefghijklmnopqrstuvwxyz";
        }
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, VarcharType()),
        new LiteralExpr(new std::string("hello"), VarcharType()), BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, VarcharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);

    EXPECT_EQ(numReturned, 25);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Coalesce1)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];

    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 100;
        col2[i] = 21;
        col3[i] = -1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            t->Get(1)->SetNull(i);
        }
    }

    auto *coalesceExpr = new CoalesceExpr(new FieldExpr(1, IntType()), new FieldExpr(0, IntType()));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(21, IntType()),
        coalesceExpr, BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, IntType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 500);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Coalesce2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;

    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(30) }));
    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            vector->SetNull(i);
        }
        value = "hello";
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    auto *coalesceExpr =
        new CoalesceExpr(new FieldExpr(0, VarcharType()), new LiteralExpr(new std::string("bye"), VarcharType()));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, coalesceExpr,
        new LiteralExpr(new std::string("hello"), VarcharType()), BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, VarcharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 500);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Coalesce3)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *data1 = new int64_t[numRows * 2];
    auto *data2 = new int64_t[numRows * 2];
    for (int64_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1000;
        data1[2 * i + 1] = 0;
        data2[2 * i] = (i + 1) * 1;
        data2[2 * i + 1] = 0;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ Decimal128Type() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, data1);

    std::vector<Expr *> projections = { new FieldExpr(0, Decimal128Type(38, 0)) };
    auto v1 = new LiteralExpr(new std::string("500000"), Decimal128Type(38, 0));
    v1->isNull = false;
    auto v2 = new LiteralExpr(new std::string("1234"), Decimal128Type(38, 0));
    auto coalesce = new CoalesceExpr(v1, v2);
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LTE, new FieldExpr(0, Decimal128Type(38, 0)),
        coalesce, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch1 = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch1);
    EXPECT_TRUE(CheckOutput(outputVecBatch1, numReturned, Filter6));
    EXPECT_EQ(numReturned, 500);

    VectorBatch *in2 = CreateVectorBatch(inputTypes, numRows, data2);
    op->AddInput(in2);
    VectorBatch *outputVecBatch2 = nullptr;
    numReturned = op->GetOutput(&outputVecBatch2);
    EXPECT_TRUE(CheckOutput(outputVecBatch2, numReturned, Filter6));
    EXPECT_EQ(numReturned, 1000);

    VectorHelper::FreeVecBatch(outputVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch2);
    delete[] data1;
    delete[] data2;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Coalesce4)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;

    DataTypes inputTypes(std::vector<DataTypePtr>({ CharType(5) }));
    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            vector->SetNull(i);
        }
        value = "hello";
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    auto *coalesceExpr =
        new CoalesceExpr(new FieldExpr(0, CharType()), new LiteralExpr(new std::string("world"), CharType()));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, coalesceExpr,
        new LiteralExpr(new std::string("hello"), CharType()), BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, CharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 500);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Coalesce5)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *col1 = new int64_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 100;
        col2[i] = 21;
        col3[i] = -1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            t->Get(1)->SetNull(i);
        }
    }

    int64_t targetValue = 21;
    auto *coalesceExpr = new CoalesceExpr(new FieldExpr(1, LongType()), new FieldExpr(0, LongType()));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(targetValue, LongType()),
        coalesceExpr, BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, LongType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, LongType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 500);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, Coalesce6)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *col1 = new double[numRows];
    auto *col2 = new double[numRows];
    auto *col3 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 100.0;
        col2[i] = 21.0;
        col3[i] = -1.0;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType(), DoubleType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            t->Get(1)->SetNull(i);
        }
    }
    auto *coalesceExpr = new CoalesceExpr(new FieldExpr(1, DoubleType()), new FieldExpr(0, DoubleType()));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(21.0, DoubleType()),
        coalesceExpr, BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, DoubleType()), new FieldExpr(1, DoubleType()),
        new FieldExpr(2, DoubleType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 500);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

// Testing multithreading
// Two operators running at once
void process(omniruntime::op::FilterAndProjectOperatorFactory *factory, int32_t *numReturned)
{
    const int32_t numRows = 100000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 2;
        col2[i] = i % 5;
        col3[i] = -1;
    }
    VectorBatch *outputVecBatch = nullptr;
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    op->AddInput(t);
    *numReturned = op->GetOutput(&outputVecBatch);
    std::cout << "numSelectedRows: " << *numReturned << std::endl;
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

#include <thread>
#include <ratio>
TEST(FilterTest, Multithreading)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType(), IntType() }));

    int32_t *numReturned = new int32_t;
    int32_t *numReturned2 = new int32_t;

    // find wall clock time
    auto start = std::chrono::high_resolution_clock::now();

    // filters
    std::string castStr = "CAST";
    std::string absStr = "abs";
    DataTypePtr retType = DoubleType();
    std::vector<Expr *> args1;
    args1.push_back(new FieldExpr(0, IntType()));
    auto cast1Expr = GetFuncExpr(castStr, args1, DoubleType());
    std::vector<Expr *> args2;
    args2.push_back(cast1Expr);
    auto eqLeft = GetFuncExpr(absStr, args2, DoubleType());

    std::vector<Expr *> args3;
    args3.push_back(new FieldExpr(1, LongType()));
    auto cast2Expr = GetFuncExpr(castStr, args3, DoubleType());
    std::vector<Expr *> args4;
    args4.push_back(cast2Expr);
    auto eqRight = GetFuncExpr(absStr, args4, DoubleType());

    auto filterExpr1 = new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, eqRight, BooleanType());

    std::vector<Expr *> projections1 = { new FieldExpr(0, IntType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr1, projections1, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    std::thread thread1(process, factory, numReturned);

    // filter2
    auto eqRight2 = new LiteralExpr(4L, LongType());
    auto filterExpr2 =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(1, LongType()), eqRight2, BooleanType());
    std::vector<Expr *> projections2 = { new FieldExpr(0, IntType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, IntType()) };

    auto exprEvaluator2 = std::make_shared<ExpressionEvaluator>(filterExpr2, projections2, inputTypes, overflowConfig);
    auto *factory2 = new FilterAndProjectOperatorFactory(move(exprEvaluator2));
    std::thread thread2(process, factory2, numReturned2);

    thread2.join();
    thread1.join();
    EXPECT_EQ(*numReturned, 20000);
    EXPECT_EQ(*numReturned2, 20000);

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Total time for multithreading test: ";
    std::cout << std::chrono::duration<double, std::milli>(end - start).count() << std::endl;

    delete factory2;
    delete numReturned;
    delete numReturned2;
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, TestFilterDictionaryVec)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
        col2[i] = i % 11;
    }
    DataTypes inputTypes1(std::vector<DataTypePtr>({ IntType(), IntType() }));
    auto *t = CreateVectorBatch(inputTypes1, numRows, col1, col2);

    int32_t ids[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto dictionary = std::make_shared<Vector<int32_t>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        dictionary->SetValue(i, (i % 21) - 3);
    }
    auto dicVec = VectorHelper::CreateDictionary(ids, numRows, dictionary.get());
    t->Append(dicVec);

    std::vector<DataTypePtr> vecOfTypes({ IntType(), IntType(), IntType() });
    DataTypes inputTypes2(vecOfTypes);

    auto *filterExpr =
        new BetweenExpr(new FieldExpr(1, IntType()), new FieldExpr(0, IntType()), new FieldExpr(2, IntType()));
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, IntType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes2, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    VectorBatch *copiedBatch = DuplicateVectorBatch(t);
    op->AddInput(copiedBatch);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 7);
    for (int i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, col1[i]);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, col2[i]);
        int32_t val2 = (reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(outputVecBatch->Get(2)))->GetValue(i);
        EXPECT_EQ(val2, (reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(t->Get(2)))->GetValue(i));
    }

    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] col1;
    delete[] col2;
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, TestFilterDictionaryVarchar)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 3;
    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i * 3;
    }
    DataTypes inputTypes1(std::vector<DataTypePtr>({ IntType() }));
    auto *t = CreateVectorBatch(inputTypes1, numRows, col1);
    std::vector<DataTypePtr> vecOfTypes({ IntType(), IntType(), IntType() });
    DataTypes inputTypes(vecOfTypes);

    int32_t ids[] = {0, 1, 2};
    std::string_view strView = "test";
    auto dictionary = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        dictionary->SetValue(i, strView);
    }
    auto dicVec = VectorHelper::CreateStringDictionary(ids, numRows, dictionary.get());
    t->Append(dicVec);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(6, IntType()), BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, IntType()), new FieldExpr(1, VarcharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    auto *copiedBatch = DuplicateVectorBatch(t);
    op->AddInput(copiedBatch);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);
    EXPECT_EQ(numReturned, 2);
    for (int i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, col1[i]);
        std::string_view val1 =
            (reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_TRUE(val1 ==
            (reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(t->Get(1)))->GetValue(i));
    }

    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, DecimalFilterBinaryTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *data1 = new int64_t[numRows * 2];
    auto *data2 = new int64_t[numRows * 2];
    for (int64_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1000;
        data1[2 * i + 1] = 0;
        data2[2 * i] = (i + 1) * 1;
        data2[2 * i + 1] = 0;
    }

    std::vector<DataTypePtr> vecOfTypes = { Decimal128Type() };
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, data1);

    std::vector<Expr *> projections = { new FieldExpr(0, Decimal128Type(38, 0)) };
    auto *lteRight = new LiteralExpr(new std::string("500000"), Decimal128Type(38, 0));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LTE, new FieldExpr(0, Decimal128Type(38, 0)),
        lteRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    op->AddInput(in1);
    VectorBatch *outputVecBatch1 = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch1);
    EXPECT_TRUE(CheckOutput(outputVecBatch1, numReturned, Filter6));
    EXPECT_EQ(numReturned, 500);

    VectorBatch *in2 = CreateVectorBatch(inputTypes, numRows, data2);
    op->AddInput(in2);
    VectorBatch *outputVecBatch2 = nullptr;
    numReturned = op->GetOutput(&outputVecBatch2);
    EXPECT_TRUE(CheckOutput(outputVecBatch2, numReturned, Filter6));
    EXPECT_EQ(numReturned, 1000);

    VectorHelper::FreeVecBatch(outputVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch2);
    delete[] data1;
    delete[] data2;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, DecimalFilterAbsTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    auto *data1 = new int64_t[numRows * 2];
    auto *data2 = new int64_t[numRows * 2];
    auto *data3 = new int64_t[numRows * 2];
    for (int64_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1;
        data1[2 * i + 1] = -1000;
        data2[2 * i] = (i + 1) * 1;
        data2[2 * i + 1] = -1000;
        data3[2 * i] = (i + 1) * 1;
        data3[2 * i + 1] = -1000;
    }

    std::vector<DataTypePtr> vecOfTypes = { Decimal128Type(), Decimal128Type(), Decimal128Type() };
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, data1, data2, data3);

    std::vector<Expr *> projections = { new FieldExpr(0, Decimal128Type(38, 0)),
        new FieldExpr(1, Decimal128Type(38, 0)), new FieldExpr(2, Decimal128Type(38, 0)) };

    // filters
    std::string absStr = "abs";
    DataTypePtr retType = Decimal128Type(38, 0);
    std::vector<Expr *> args1;
    args1.push_back(new FieldExpr(0, Decimal128Type(38, 0)));
    auto absExpr1 = GetFuncExpr(absStr, args1, Decimal128Type(38, 0));

    std::vector<Expr *> args2;
    args2.push_back(new FieldExpr(2, Decimal128Type(38, 0)));
    auto absExpr2 = GetFuncExpr(absStr, args2, Decimal128Type(38, 0));

    auto *eqExpr1 = new BinaryExpr(omniruntime::expressions::Operator::EQ, absExpr1, absExpr2, BooleanType());

    std::vector<Expr *> args3;
    args3.push_back(new FieldExpr(1, Decimal128Type(38, 0)));
    auto absExpr3 = GetFuncExpr(absStr, args3, Decimal128Type(38, 0));

    std::vector<Expr *> args4;
    args4.push_back(new FieldExpr(2, Decimal128Type(38, 0)));
    auto absExpr4 = GetFuncExpr(absStr, args4, Decimal128Type(38, 0));

    auto *eqExpr2 = new BinaryExpr(omniruntime::expressions::Operator::EQ, absExpr3, absExpr4, BooleanType());
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eqExpr1, eqExpr2, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    auto start = std::chrono::system_clock::now();
    op->AddInput(in1);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "BenchmarkDecimalColumn round - elapsed: " << elapsed.count() << " ms\n";
    EXPECT_EQ(numReturned, 1000);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] data1;
    delete[] data2;
    delete[] data3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, FilterStringWithNull)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 2;
    auto strings = new std::string[numRows];
    strings[0] = "hello";

    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(1000) }));
    auto *batch = CreateVectorBatch(inputTypes, numRows, strings);
    batch->Get(0)->SetNull(1);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, VarcharType()),
        new LiteralExpr(new std::string("hello"), VarcharType()), BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, VarcharType()) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(batch);
    VectorBatch *outputVecBatch = nullptr;
    int32_t numReturned = op->GetOutput(&outputVecBatch);

    EXPECT_EQ(numReturned, 1);
    BaseVector *vec = outputVecBatch->Get(0);
    if (vec->GetEncoding() == omniruntime::vec::OMNI_DICTIONARY) {
        auto *vcVec = reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(vec);
        for (int32_t i = 0; i < numReturned; i++) {
            EXPECT_TRUE(vcVec->GetValue(i) == "hello");
        }
    } else {
        auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        for (int32_t i = 0; i < numReturned; i++) {
            EXPECT_TRUE(vcVec->GetValue(i) == "hello");
        }
    }


    VectorHelper::FreeVecBatch(outputVecBatch);
    delete factory;
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] strings;
    delete overflowConfig;
}

TEST(FilterTest, StartsWith)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 9;
    std::string col0[numRows] = {"", "", "abc", "abc", "abc", "abc", "", "abc", ""};
    std::string col1[numRows] = {"", "abc", "", "abcd", "bd", "ab", "abc", "", ""};

    auto vec0 = new Vector<vec::LargeStringContainer<std::string_view>>(numRows);
    auto vec1 = new Vector<vec::LargeStringContainer<std::string_view>>(numRows);
    for (int32_t i = 0; i < 6; i++) {
        if (col0[i].empty()) {
            vec0->SetNull(i);
        } else {
            std::string_view val0(col0[i].c_str(), col0[i].size());
            vec0->SetValue(i, val0);
        }
        if (col1[i].empty()) {
            vec1->SetNull(i);
        } else {
            std::string_view val1(col1[i].c_str(), col1[i].size());
            vec1->SetValue(i, val1);
        }
    }
    for (int32_t i = 6; i < numRows; i++) {
        std::string_view val0(col0[i].c_str(), col0[i].size());
        vec0->SetValue(i, val0);
        std::string_view val1(col1[i].c_str(), col1[i].size());
        vec1->SetValue(i, val1);
    }

    auto vecBatch = new VectorBatch(numRows);
    vecBatch->Append(vec0);
    vecBatch->Append(vec1);

    // filter
    DataTypePtr retType = BooleanType();
    std::string funcStr = "StartsWith";
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, VarcharType(5)));
    args.push_back(new FieldExpr(1, VarcharType(5)));
    auto funcExpr = GetFuncExpr(funcStr, args, retType);

    std::vector<Expr *> projections = { new FieldExpr(0, VarcharType(5)), new FieldExpr(1, VarcharType(5)) };
    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(5), VarcharType(5) }));
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(funcExpr, projections, inputTypes, overflowConfig);

    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    const int32_t expectNumRows = 3;
    std::string expect0[expectNumRows] = {"abc", "abc", ""};
    std::string expect1[expectNumRows] = {"ab", "", ""};
    auto expectVecBatch = CreateVectorBatch(inputTypes, expectNumRows, expect0, expect1);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, EndsWith)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 9;
    std::string col0[numRows] = {"", "", "abc", "abc", "abc", "abc", "", "abc", ""};
    std::string col1[numRows] = {"", "abc", "", "abcd", "bd", "bc", "abc", "", ""};

    auto vec0 = new Vector<vec::LargeStringContainer<std::string_view>>(numRows);
    auto vec1 = new Vector<vec::LargeStringContainer<std::string_view>>(numRows);
    for (int32_t i = 0; i < 6; i++) {
        if (col0[i].empty()) {
            vec0->SetNull(i);
        } else {
            std::string_view val0(col0[i].c_str(), col0[i].size());
            vec0->SetValue(i, val0);
        }
        if (col1[i].empty()) {
            vec1->SetNull(i);
        } else {
            std::string_view val1(col1[i].c_str(), col1[i].size());
            vec1->SetValue(i, val1);
        }
    }
    for (int32_t i = 6; i < numRows; i++) {
        std::string_view val0(col0[i].c_str(), col0[i].size());
        vec0->SetValue(i, val0);
        std::string_view val1(col1[i].c_str(), col1[i].size());
        vec1->SetValue(i, val1);
    }

    auto vecBatch = new VectorBatch(numRows);
    vecBatch->Append(vec0);
    vecBatch->Append(vec1);

    // filter
    DataTypePtr retType = BooleanType();
    std::string funcStr = "EndsWith";
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, VarcharType(5)));
    args.push_back(new FieldExpr(1, VarcharType(5)));
    auto funcExpr = GetFuncExpr(funcStr, args, retType);

    std::vector<Expr *> projections = { new FieldExpr(0, VarcharType(5)), new FieldExpr(1, VarcharType(5)) };
    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(5), VarcharType(5) }));
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(funcExpr, projections, inputTypes, overflowConfig);

    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    op->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    const int32_t expectNumRows = 3;
    std::string expect0[expectNumRows] = {"abc", "abc", ""};
    std::string expect1[expectNumRows] = {"bc", "", ""};
    auto expectVecBatch = CreateVectorBatch(inputTypes, expectNumRows, expect0, expect1);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, SimpleFilter)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(2000, IntType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto filter = new SimpleFilter(*filterExpr);
    bool initialized = filter->Initialize(overflowConfig);
    EXPECT_TRUE(initialized);

    ExecutionContext context(4096, nullptr);
    auto vector = reinterpret_cast<Vector<int32_t> *>(in1->Get(0));
    int64_t values[1];
    bool isNulls[1];
    for (int i = 0; i < numRows; i++) {
        values[0] = reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(vector)) + i * sizeof(int32_t);
        isNulls[0] = vector->IsNull(i);
        bool result = filter->Evaluate(values, isNulls, nullptr, (int64_t)(&context));
        if (i < 2000) {
            EXPECT_TRUE(result);
        } else {
            EXPECT_FALSE(result);
        }
    }
    Expr::DeleteExprs({ filterExpr });
    VectorHelper::FreeVecBatch(in1);
    delete filter;
    delete[] col1;
    delete overflowConfig;
}

TEST(FilterTest, SimpleFilterWithNulls)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 5000;
    auto col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *in1 = CreateVectorBatch(inputTypes, numRows, col1);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(2000, IntType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto filter = new SimpleFilter(*filterExpr);
    bool initialized = filter->Initialize(overflowConfig);
    EXPECT_TRUE(initialized);

    // set first 500 elements to null
    auto vector = reinterpret_cast<Vector<int32_t> *>(in1->Get(0));
    for (int i = 0; i < 500; i++) {
        vector->SetNull(i);
    }

    ExecutionContext context(4096, nullptr);
    int64_t values[1];
    bool isNulls[1];
    for (int i = 0; i < numRows; i++) {
        values[0] = reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(vector)) + i * sizeof(int32_t);
        isNulls[0] = vector->IsNull(i);
        bool result = filter->Evaluate(values, isNulls, nullptr, (int64_t)(&context));
        if (i >= 500 && i < 2000) {
            EXPECT_TRUE(result);
        } else {
            EXPECT_FALSE(result);
        }
    }
    Expr::DeleteExprs({ filterExpr });
    VectorHelper::FreeVecBatch(in1);
    ;
    delete filter;
    delete[] col1;
    delete overflowConfig;
}

TEST(FilterTest, SimpleFilterIntWithNulls)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int32_t data0[numRows] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    int32_t data1[numRows] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7};

    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType() }));
    auto vecBatch = CreateVectorBatch(inputTypes, numRows, data0, data1);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new FieldExpr(0, IntType()),
        new FieldExpr(1, IntType()), BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto filter = new SimpleFilter(*filterExpr);
    bool initialized = filter->Initialize(overflowConfig);
    EXPECT_TRUE(initialized);

    for (int i = 0; i < numRows; i++) {
        if (i % 5 == 4) {
            vecBatch->Get(0)->SetNull(i);
            vecBatch->Get(1)->SetNull(i);
        }
    }

    ExecutionContext context(4096, nullptr);
    int64_t values[2];
    bool isNulls[2];
    auto vector0 = reinterpret_cast<Vector<int32_t> *>(vecBatch->Get(0));
    auto vector1 = reinterpret_cast<Vector<int32_t> *>(vecBatch->Get(1));
    for (int i = 0; i < numRows; i++) {
        values[0] = reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(vector0) + i);
        isNulls[0] = vector0->IsNull(i);
        values[1] = reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(vector1) + i);
        isNulls[1] = vector1->IsNull(i);
        bool result = filter->Evaluate(values, isNulls, nullptr, (int64_t)(&context));
        EXPECT_FALSE(result);
    }
    Expr::DeleteExprs({ filterExpr });
    VectorHelper::FreeVecBatch(vecBatch);
    delete filter;
    delete overflowConfig;
}

TEST(FilterTest, SimpleFilterCharWithNulls)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 9;
    std::string_view data0[numRows] = {"35709", "35709", "35709", "31904", "", "", "35709", "35709", ""};
    std::string_view data1[numRows] = {"31904", "35709", "31904", "31904", "31904", "35709", "35709", "31904", "35709"};

    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType(5), VarcharType(5) }));
    auto vec0 = VectorHelper::CreateStringVector(numRows);
    auto vec1 = VectorHelper::CreateStringVector(numRows);
    auto *vector0 = (Vector<LargeStringContainer<std::string_view>> *)vec0;
    auto *vector1 = (Vector<LargeStringContainer<std::string_view>> *)vec1;

    for (int i = 0; i < numRows; i++) {
        if (data0[i].compare("") == 0) {
            vector0->SetNull(i);
        } else {
            vector0->SetValue(i, data0[i]);
        }
        vector1->SetValue(i, data1[i]);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(vec0);
    t->Append(vec1);

    // filter expression object
    std::string funcStr = "substr";
    DataTypePtr retType = VarcharType();
    std::vector<Expr *> args1;
    args1.push_back(new FieldExpr(0, VarcharType()));
    args1.push_back(new LiteralExpr(1, IntType()));
    args1.push_back(new LiteralExpr(5, IntType()));
    auto substrExpr1 = GetFuncExpr(funcStr, args1, VarcharType());

    std::vector<Expr *> args2;
    args2.push_back(new FieldExpr(1, VarcharType()));
    args2.push_back(new LiteralExpr(1, IntType()));
    args2.push_back(new LiteralExpr(5, IntType()));
    auto substrExpr2 = GetFuncExpr(funcStr, args2, VarcharType());
    auto filterExpr = new BinaryExpr(omniruntime::expressions::Operator::NEQ, substrExpr1, substrExpr2, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto filter = new SimpleFilter(*filterExpr);
    bool initialized = filter->Initialize(overflowConfig);
    EXPECT_TRUE(initialized);

    ExecutionContext context(4096, nullptr);
    int64_t values[2];
    bool isNulls[2];
    int32_t lengths[2];

    for (int i = 0; i < numRows; i++) {
        isNulls[0] = vector0->IsNull(i);
        isNulls[1] = vector1->IsNull(i);

        values[0] = reinterpret_cast<int64_t>(vector0->GetValue(i).data());
        values[1] = reinterpret_cast<int64_t>(vector1->GetValue(i).data());

        lengths[0] = vector0->GetValue(i).length();
        lengths[1] = vector1->GetValue(i).length();

        bool result = filter->Evaluate(values, isNulls, lengths, (int64_t)(&context));
        if (i == 0 || i == 2 || i == 7) {
            EXPECT_TRUE(result);
        } else {
            EXPECT_FALSE(result);
        }
    }

    Expr::DeleteExprs({ filterExpr });
    delete filter;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
}

VectorBatch *CreateSequenceVectorBatch(const std::vector<DataTypePtr> &types, int length)
{
    auto *vectorBatch = new VectorBatch((int)types.size());
    for (int i = 0; i < (int32_t)types.size(); ++i) {
        auto vector = VectorHelper::CreateVector(OMNI_FLAT, types[i]->GetId(), length);
        for (int index = 0; index < length; ++index) {
            switch (types[i]->GetId()) {
                case OMNI_INT:
                case OMNI_DATE32:
                    static_cast<Vector<int32_t> *>(vector)->SetValue(index, index);
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    static_cast<Vector<int64_t> *>(vector)->SetValue(index, index);
                    break;
                case OMNI_DOUBLE:
                    static_cast<Vector<double> *>(vector)->SetValue(index, index);
                    break;
                case OMNI_BOOLEAN:
                    static_cast<Vector<bool> *>(vector)->SetValue(index, index);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR: {
                    std::string_view input(std::to_string(index));
                    static_cast<Vector<LargeStringContainer<string_view>> *>(vector)->SetValue(index, input);
                    break;
                }
                case OMNI_DECIMAL128:
                    static_cast<Vector<Decimal128> *>(vector)->SetValue(index, Decimal128(0, index));
                    break;
                default:
                    LogError("No such data type %d", types[i]->GetId());
                    break;
            }
        }
        vectorBatch->Append(vector);
    }
    return vectorBatch;
}

VectorBatch *CreateSequenceVectorBatchWithDictionaryVector(const std::vector<DataTypePtr> &types, int length)
{
    auto *vectorBatch = new VectorBatch((int)types.size());
    int ratio = 5;
    for (int i = 0; i < (int32_t)types.size(); ++i) {
        auto inner = VectorHelper::CreateVector(OMNI_FLAT, types[i]->GetId(), length / ratio);
        for (int index = 0; index < length / ratio; ++index) {
            switch (types[i]->GetId()) {
                case OMNI_INT:
                case OMNI_DATE32:
                    static_cast<Vector<int32_t> *>(inner)->SetValue(index, index);
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    static_cast<Vector<int64_t> *>(inner)->SetValue(index, index);
                    break;
                case OMNI_DOUBLE:
                    static_cast<Vector<double> *>(inner)->SetValue(index, index);
                    break;
                case OMNI_BOOLEAN:
                    static_cast<Vector<bool> *>(inner)->SetValue(index, index);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    static_cast<Vector<string_view> *>(inner)->SetValue(index, std::to_string(index).c_str());
                    break;
                case OMNI_DECIMAL128:
                    static_cast<Vector<Decimal128> *>(inner)->SetValue(index, Decimal128(0, index));
                    break;
                default:
                    LogError("No such data type %d", types[i]->GetId());
                    break;
            }
        }
        std::vector<int32_t> ids(length);
        for (int k = 0; k < length; ++k) {
            ids[k] = (k % ratio);
        }
        auto vector = VectorHelper::CreateDictionaryVector(ids.data(), ids.size(), inner, types[i]->GetId());
        delete inner;
        vectorBatch->Append(vector);
    }
    return vectorBatch;
}

struct MultiThreadArgs {
    FilterAndProjectOperatorFactory *operatorFactory;
    int vecCount;
    std::vector<DataTypePtr> *types;
    int index;
    std::vector<double> wallTime;
    std::vector<double> cpuTime;
    int threadNum;
    int rounds = 15;
};

void SetMultiThreadArgs(struct MultiThreadArgs *multiThreadArgs, FilterAndProjectOperatorFactory *operatorFactory,
    int vecCount, std::vector<DataTypePtr> *types)
{
    multiThreadArgs->operatorFactory = operatorFactory;
    multiThreadArgs->vecCount = vecCount;
    multiThreadArgs->types = types;
}

void OperatorAddInput(struct MultiThreadArgs *multiThreadArgs)
{
    // create operator
    omniruntime::op::Operator *op = multiThreadArgs->operatorFactory->CreateOperator(nullptr);

    // allocate vectors to operator and assemble them to vecBatch
    int vectorBatchNum;
    if (multiThreadArgs->index == 0) {
        vectorBatchNum = (multiThreadArgs->vecCount / multiThreadArgs->threadNum) +
            (multiThreadArgs->vecCount % multiThreadArgs->threadNum);
    } else {
        vectorBatchNum = multiThreadArgs->vecCount / multiThreadArgs->threadNum;
    }
    std::vector<VectorBatch *> vvb(vectorBatchNum);
    int64_t positionPage = 1024;
    for (int i = 0; i < vectorBatchNum; ++i) {
        int randomNumber = rand() % multiThreadArgs->vecCount;
        if (randomNumber % 2 == 0) {
            vvb[i] = CreateSequenceVectorBatch(*(multiThreadArgs->types), positionPage);
        } else {
            vvb[i] = CreateSequenceVectorBatchWithDictionaryVector(*(multiThreadArgs->types), positionPage);
        }
    }

    // test 15 times and collect the time of AddInput() and GetOutput()
    double wallTimeSum = 0;
    double cpuTimeSum = 0;
    for (int i = 0; i < multiThreadArgs->rounds; ++i) {
        for (const auto &vb : vvb) {
            VectorBatch *outputVecBatch = nullptr;
            VectorBatch *copiedBatch = DuplicateVectorBatch(vb);
            Timer timer;
            timer.SetStart();
            op->AddInput(copiedBatch);
            op->GetOutput(&outputVecBatch);
            timer.CalculateElapse();
            wallTimeSum += timer.GetWallElapse();
            cpuTimeSum += timer.GetCpuElapse();
            VectorHelper::FreeVecBatch(outputVecBatch);
        }
    }

    // record the average wallTime and cpuTime
    multiThreadArgs->wallTime[multiThreadArgs->index] = wallTimeSum / multiThreadArgs->rounds;
    multiThreadArgs->cpuTime[multiThreadArgs->index] = cpuTimeSum / multiThreadArgs->rounds;

    VectorHelper::FreeVecBatches(vvb);
    omniruntime::op::Operator::DeleteOperator(op);
}

void TestAddInputMultiThreads(struct MultiThreadArgs *multiThreadArgs, int32_t threadNum, double &wallElapsed,
    double &cpuElapsed)
{
    std::vector<std::thread> vecOfThreads;
    multiThreadArgs->wallTime.resize(threadNum);
    multiThreadArgs->cpuTime.resize(threadNum);
    multiThreadArgs->threadNum = threadNum;

    // generate multithreading
    for (int32_t i = 0; i < threadNum; ++i) {
        multiThreadArgs->index = i;
        std::thread th(OperatorAddInput, multiThreadArgs);
        vecOfThreads.push_back(std::move(th));
    }

    for (auto &th : vecOfThreads) {
        if (th.joinable()) {
            th.join();
        }
    }

    // Calculate wallTime and cpuTime of all threads.
    for (int i = 0; i < threadNum; ++i) {
        wallElapsed += multiThreadArgs->wallTime[i];
        cpuElapsed += multiThreadArgs->cpuTime[i];
    }
}

TEST(FilterTest, Test10SQLMutilThread)
{
    std::map<std::string, std::vector<DataTypePtr>> INPUT_TYPES = {
        { "q1", { IntType(), IntType(), IntType(), Date32Type() } },
        { "q2", { LongType(), LongType(), IntType(), IntType() } },
        { "q3", { VarcharType(200), VarcharType(200), IntType() } },
        { "q4", { VarcharType(200), IntType(), IntType() } },
        { "q5", { CharType(200), IntType(), IntType() } },
        { "q6", { VarcharType(200), LongType(), IntType() } },
        { "q7", { VarcharType(200), IntType() } },
        { "q8", { VarcharType(200), VarcharType(200), LongType(), IntType() } },
        { "q9", { LongType(), IntType(), IntType(), VarcharType(200) } },
        { "q10", { LongType(), IntType(), IntType(), VarcharType(200) } },
    };

    std::map<std::string, std::vector<std::string>> PROJECTIONS = {
        { "q1",
                { R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":0})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":8,"colVal":3})" } },
        { "q2",
                { R"({"exprType":"BINARY","returnType":2,"operator":"SUBTRACT",
        "left":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},
        "right":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}})",
                        R"({"exprType":"BINARY","returnType":2,"operator":"ADD",
        "left":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1},
        "right":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3}]})" } },
        { "q3",
                { R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":200})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2}]})" } },
        { "q4",
                { R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})" } },
        { "q5",
                { R"({"exprType":"FUNCTION","returnType":16,"function_name":"concat",
        "arguments":[{"exprType":"FUNCTION","returnType":16,"function_name":"concat",
        "arguments":[{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"foo","width":3},
        {"exprType": "FIELD_REFERENCE","dataType":16,"colVal":0,"width":200}],"width":203},
        {"exprType":"LITERAL","dataType":16,"isNull":false,"value":"lish","width":4}],"width":207})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})" } },
        { "q6",
                { R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2}]})" } },
        { "q7",
                { R"({"exprType":"FUNCTION","returnType":15,"function_name":"substr",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},
        {"exprType": "LITERAL" ,"dataType":2,"isNull":false,"value":0},
        {"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}],"width":200})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})" } },
        { "q8",
                { R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":200})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":2})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3}]})" } },
        { "q9",
                { R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
       "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1}]})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
       "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2}]})",
                        R"({"exprType":"FUNCTION","returnType":15,"function_name":"substr",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":200},
        {"exprType": "LITERAL" ,"dataType":2,"isNull":false,"value":0},
        {"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}],"width":200})" } },
        { "q10",
                { R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
                        R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1}]})",
                        R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})",
                        R"({"exprType":"FUNCTION","returnType":15,"function_name":"substr",
        "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":200},
        {"exprType": "LITERAL" ,"dataType":2,"isNull":false,"value":0},
        {"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}],"width":200})" } },
    };

    std::map<std::string, std::string> FILTERS = {
        { "q1",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":0},
            {"exprType": "LITERAL" ,"dataType":1,"isNull":false,"value":1},
            {"exprType": "LITERAL" ,"dataType":1,"isNull":false,"value":2}]},
            "right":{"exprType":"BETWEEN","returnType":4,
            "value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},
            "lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},
            "upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":10}}},
            "right":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":0},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}]}})" },
        { "q2",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN",
            "left":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},
            "right":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":0}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":10}}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":2}]},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}}})" },
        { "q3",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"4","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"5","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"6","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"7","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"8","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"9","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"10","width":200}]},
            "right":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200}]}},
            "right":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":3},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":4},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":5}]}})" },
        { "q4",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200}]},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}})" },
        { "q5",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":200},
            "right":{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"3","width":200}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN_OR_EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":10}}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"LESS_THAN_OR_EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":20}}})" },
        { "q6",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"4","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"5","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"6","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"7","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"8","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"9","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"10","width":200}]},
            "right":{"exprType":"BETWEEN","returnType":4,
            "value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1},
            "lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1},
            "upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":10}}},
            "right":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":3},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":4},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":5}]}})" },
        { "q7",
            R"({"exprType":"BETWEEN","returnType":4,
            "value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},
            "lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3},
            "upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":5}})" },
        { "q8",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"4","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"5","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"6","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"7","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"8","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"9","width":200},
            {"exprType":"LITERAL","dataType":15,"isNull":false,"value":"10","width":200}]},
            "right":{"exprType":"BETWEEN","returnType":4,
            "value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":2},
            "lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":3},
            "upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":5}}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}})" },
        { "q9",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BETWEEN","returnType":4,
            "value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},
            "lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":3},
            "upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":5}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}},
            "right":{"exprType":"IN","returnType":4,
            "arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},
            {"exprType":"LITERAL","dataType":1,"isNull":false,"value":2}]}})" },
        { "q10",
            R"({"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BINARY","returnType":4,"operator":"OR",
            "left":{"exprType":"BETWEEN","returnType":4,
            "value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},
            "lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":3},
            "upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":5}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}},
            "right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL",
            "left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},
            "right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}})" },
    };

    const static std::map<int64_t, std::string> SQL_NAME = { { 0, "q1" }, { 1, "q2" }, { 2, "q3" }, { 3, "q4" },
                                                             { 4, "q5" }, { 5, "q6" }, { 6, "q7" }, { 7, "q8" },
                                                             { 8, "q9" }, { 9, "q10" } };
    int64_t totalPositions = 1000000;
    int64_t positionsPerPage = 1024;
    static constexpr int FILTER_UNIT_TEST_TIME = 1;

    double singleThreadWallTime[FILTER_UNIT_TEST_TIME];
    double singleThreadCpuTime[FILTER_UNIT_TEST_TIME];
    double fourThreadsWallTime[FILTER_UNIT_TEST_TIME];
    double fourThreadsCpuTime[FILTER_UNIT_TEST_TIME];
    double eightThreadsWallTime[FILTER_UNIT_TEST_TIME];
    double eightThreadsCpuTime[FILTER_UNIT_TEST_TIME];
    double sixteenThreadsWallTime[FILTER_UNIT_TEST_TIME];
    double sixteenThreadsCpuTime[FILTER_UNIT_TEST_TIME];

    for (int i = 0; i < FILTER_UNIT_TEST_TIME; i++) {
        std::string sqlName = SQL_NAME.at(i);
        auto overflowConfig = new OverflowConfig();
        DataTypes dataTypes(INPUT_TYPES.at(sqlName));

        auto filterJsonExpr = nlohmann::json::parse(FILTERS.at(sqlName));
        auto filterExpr = JSONParser::ParseJSON(filterJsonExpr);
        nlohmann::json jsonProjectExprs[PROJECTIONS.at(sqlName).size()];
        for (size_t i = 0; i < PROJECTIONS.at(sqlName).size(); i++) {
            jsonProjectExprs[i] = nlohmann::json::parse(PROJECTIONS.at(sqlName).data()[i]);
        }
        auto projExprs = JSONParser::ParseJSON(jsonProjectExprs, PROJECTIONS.at(sqlName).size());

        auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projExprs, dataTypes, overflowConfig);
        auto *operatorFactory = new FilterAndProjectOperatorFactory(move(exprEvaluator));

        // single-thread test
        struct MultiThreadArgs singleThreadArgs;
        SetMultiThreadArgs(&singleThreadArgs, operatorFactory, totalPositions / positionsPerPage,
                           &(INPUT_TYPES.at(sqlName)));
        double singleThreadWallElapsed = 0;
        double singleThreadCpuElapsed = 0;
        int singleThreadNum = 1;
        TestAddInputMultiThreads(&singleThreadArgs, singleThreadNum, singleThreadWallElapsed, singleThreadCpuElapsed);
        singleThreadWallTime[i] = singleThreadWallElapsed;
        singleThreadCpuTime[i] = singleThreadCpuElapsed;

        double fourThreadsWallElapsed = 0;
        double fourThreadsCpuElapsed = 0;
        int fourThreadsNum = 4;
        struct MultiThreadArgs fourThreadsArgs;
        SetMultiThreadArgs(&fourThreadsArgs, operatorFactory, totalPositions / positionsPerPage,
                           &(INPUT_TYPES.at(sqlName)));
        TestAddInputMultiThreads(&fourThreadsArgs, fourThreadsNum, fourThreadsWallElapsed, fourThreadsCpuElapsed);
        fourThreadsWallTime[i] = fourThreadsWallElapsed;
        fourThreadsCpuTime[i] = fourThreadsCpuElapsed;

        double eightThreadsWallElapsed = 0;
        double eightThreadsCpuElapsed = 0;
        int eightThreadsNum = 8;
        struct MultiThreadArgs eightThreadsArgs;
        SetMultiThreadArgs(&eightThreadsArgs, operatorFactory, totalPositions / positionsPerPage,
                           &(INPUT_TYPES.at(sqlName)));
        TestAddInputMultiThreads(&eightThreadsArgs, eightThreadsNum, eightThreadsWallElapsed, eightThreadsCpuElapsed);
        eightThreadsWallTime[i] = eightThreadsWallElapsed;
        eightThreadsCpuTime[i] = eightThreadsCpuElapsed;

        double sixteenThreadsWallElapsed = 0;
        double sixteenThreadsCpuElapsed = 0;
        int sixteenThreadsNum = 16;
        struct MultiThreadArgs sixteenThreadsArgs;
        SetMultiThreadArgs(&sixteenThreadsArgs, operatorFactory, totalPositions / positionsPerPage,
                           &(INPUT_TYPES.at(sqlName)));
        TestAddInputMultiThreads(&sixteenThreadsArgs, sixteenThreadsNum, sixteenThreadsWallElapsed,
                                 sixteenThreadsCpuElapsed);
        sixteenThreadsWallTime[i] = sixteenThreadsWallElapsed;
        sixteenThreadsCpuTime[i] = sixteenThreadsCpuElapsed;

        delete operatorFactory;
        delete overflowConfig;
    }

    std::cout << "singleThreadWallTime";
    PrintValueLine(singleThreadWallTime, FILTER_UNIT_TEST_TIME);
    std::cout << "fourThreadsWallTime";
    PrintValueLine(fourThreadsWallTime, FILTER_UNIT_TEST_TIME);
    std::cout << "eightThreadsWallTime";
    PrintValueLine(eightThreadsWallTime, FILTER_UNIT_TEST_TIME);
    std::cout << "sixteenThreadsWallTime";
    PrintValueLine(sixteenThreadsWallTime, FILTER_UNIT_TEST_TIME);

    std::cout << "singleThreadCpuTime";
    PrintValueLine(singleThreadCpuTime, FILTER_UNIT_TEST_TIME);
    std::cout << "fourThreadsCpuTime";
    PrintValueLine(fourThreadsCpuTime, FILTER_UNIT_TEST_TIME);
    std::cout << "eightThreadsCpuTime";
    PrintValueLine(eightThreadsCpuTime, FILTER_UNIT_TEST_TIME);
    std::cout << "sixteenThreadsCpuTime";
    PrintValueLine(sixteenThreadsCpuTime, FILTER_UNIT_TEST_TIME);
}
 
TEST(FilterTest, DISABLED_FilterStruct)
{
    std::string inputTypesCharPtr =
        R"([{"id":32,"fieldTypes":[{"id":2,"idValue":2},{"id":2,"idValue":2}],"idValue":32}])";
    auto inputDataTypes = Deserialize(inputTypesCharPtr);
    auto overflowConfig = new OverflowConfig();
    std::string exprs[1];
    exprs[0] = R"({
        "exprType" : "FIELD_REFERENCE",
        "dataType" : 2,
        "ordinal" : 1,
        "input" : {
            "exprType" : "FIELD_REFERENCE",
            "dataType" : 32,
            "colVal" : 0
        }
        })";
            std::string filterExpr = R"({
        "exprType": "BINARY",
        "returnType": 4,
        "operator": "AND",
        "left": {
            "exprType": "UNARY",
            "returnType": 4,
            "operator": "not",
            "expr": {
            "exprType": "IS_NULL",
            "returnType": 4,
            "arguments": [
                {
                "exprType": "FIELD_REFERENCE",
                "dataType": 32,
                "colVal": 0
                }
            ]
            }
        },
        "right": {
            "exprType": "BINARY",
            "returnType": 4,
            "operator": "EQUAL",
            "left": {
            "exprType": "FIELD_REFERENCE",
            "dataType": 2,
            "ordinal": 0,
            "input": {
                "exprType": "FIELD_REFERENCE",
                "dataType": 32,
                "colVal": 0
            }
            },
            "right": {
            "exprType": "LITERAL",
            "dataType": 2,
            "isNull": false,
            "value": 1
            }
        }
        })";

    int64_t col1[3] = {1, 23, 4};
    int64_t col2[3] = {4, 23, 4};
    std::vector<DataTypePtr> vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    auto  children = CreateVectors(inputTypes, 3, col1, col2);

    auto *vectorBatch2 = new VectorBatch(3);
    auto rowVector = new RowVector(3, children);
    vectorBatch2->Append(rowVector);

    std::vector<omniruntime::expressions::Expr *> expressions;
    nlohmann::json jsonExprs[1];
    for (int32_t i = 0; i < 1; i++) {
        jsonExprs[i] = nlohmann::json::parse(exprs[i]);
    }
    expressions = JSONParser::ParseJSON(jsonExprs, 1);
    auto filterExpression = JSONParser::ParseJSON(filterExpr);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpression, expressions, inputDataTypes,
        overflowConfig);
    auto factory = new FilterAndProjectOperatorFactory(std::move(exprEvaluator));
    auto filterOperator = factory->CreateOperator(nullptr);
    filterOperator->AddInput(vectorBatch2);
    VectorBatch *outputVecBatch = nullptr;
    filterOperator->GetOutput(&outputVecBatch);
    VectorHelper::PrintVecBatch(outputVecBatch);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete filterOperator;
    delete overflowConfig;
    delete factory;
}

class ScopedBatchExprEvaluateConfig {
public:
    explicit ScopedBatchExprEvaluateConfig(bool enabled)
        : previous_(ConfigUtil::IsEnableBatchExprEvaluate())
    {
        ConfigUtil::SetEnableBatchExprEvaluate(enabled);
    }

    ~ScopedBatchExprEvaluateConfig()
    {
        ConfigUtil::SetEnableBatchExprEvaluate(previous_);
    }

    ScopedBatchExprEvaluateConfig(const ScopedBatchExprEvaluateConfig&) = delete;
    ScopedBatchExprEvaluateConfig& operator=(const ScopedBatchExprEvaluateConfig&) = delete;

private:
    bool previous_;
};

// Filter a Vector<StringView> using an equality expression (column == filterValue).
// Tests both the single-row (non-batch) and batch LLVM codegen paths for OMNI_STRING_VIEW.
void RunStringViewFilterTest(bool batchMode, const std::string& filterValue, int32_t expectedMatch)
{
    ScopedBatchExprEvaluateConfig batchExprConfig(batchMode);

    const int32_t numRows = 11;
    // Alternating inline (≤12 bytes) and non-inline (>12 bytes) strings.
    std::vector<std::string> strs = {
        "hello", "world_is_longer", "hello", "foo_bar_baz_qux", "hello",
        "bar_baz_qux_quux", "hello", "baz_qux_quux_corge", "hello", "world_is_longer",
        "xyz_longer_str"
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string(filterValue), svType),
        BooleanType());

        std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            EXPECT_EQ(std::string(outVec->GetValue(i)), filterValue);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

#ifdef STRINGVIEW_ENABLE
TEST(FilterTest, StringViewShortEqualityFilterRowByRow)
{
    RunStringViewFilterTest(false, "hello", 5);
}

TEST(FilterTest, StringViewShortEqualityFilterBatch)
{
    RunStringViewFilterTest(true, "hello", 5);
}

TEST(FilterTest, StringViewLongEqualityFilterRowByRow)
{
    RunStringViewFilterTest(false, "world_is_longer", 2);
}

TEST(FilterTest, StringViewLongEqualityFilterBatch)
{
    RunStringViewFilterTest(true, "world_is_longer", 2);
}

void RunStringViewFilterNETest(bool batchMode, const std::string& filterValue, int32_t expectedMatch)
{
    ScopedBatchExprEvaluateConfig batchExprConfig(batchMode);

    const int32_t numRows = 11;
    // Alternating inline (≤12 bytes) and non-inline (>12 bytes) strings.
    std::vector<std::string> strs = {
        "hello", "world_is_longer", "hello", "foo_bar_baz_qux", "hello",
        "bar_baz_qux_quux", "hello", "baz_qux_quux_corge", "hello", "world_is_longer",
        "xyz_longer_str"
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::NEQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string(filterValue), svType),
        BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            EXPECT_NE(std::string(outVec->GetValue(i)), filterValue);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewShortNotEqualityFilterRowByRow)
{
    RunStringViewFilterNETest(false, "hello", 6);
}
TEST(FilterTest, StringViewShortNotEqualityFilterRowByBatch)
{
    RunStringViewFilterNETest(true, "hello", 6);
}
TEST(FilterTest, StringViewLongNotEqualityFilterRowByRow)
{
    RunStringViewFilterNETest(false, "world_is_longer", 9);
}
TEST(FilterTest, StringViewLongNotEqualityFilterBatch)
{
    RunStringViewFilterNETest(true, "world_is_longer", 9);
}

// length(string_view) -> int — vectorized StringView support for the first batch of read-only functions (task #4).
// Verifies that a length() filter on a StringView column goes through the vectorized path:
//   ExprEval (StringView column projection) -> SimpleFunction (CharLengthFunction's vec::StringView overload)
//   -> the {OMNI_STRING_VIEW}->OMNI_INT overload registered by RegisterString.
// It also acts as a discriminator: the codegen path has no StringView length, only the vectorized overload does —
// so a PASS proves the filter operator picked the vectorized (ExprEval) path for the StringView function and that
// FlatVectorReader<StringView> correctly read out the Vector<StringView>.
// Note: only row-by-row is tested (batchMode=false) — the batch codegen path has no StringView length support yet.
void RunStringViewLengthFilterTest(int32_t targetLen, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    // Pure ASCII (char count == byte count, so length is well-defined); mix of inline (<=12B) and non-inline (>12B).
    std::vector<std::string> strs = {
        "hello",            // 5
        "hi",               // 2
        "world_is_longer",  // 15  (non-inline)
        "abcde",            // 5
        "xy",               // 2
        "foo_bar_baz_qux",  // 15  (non-inline)
        "tests",            // 5
        "ab",               // 2
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView, not VARCHAR.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // filter: length(col0) == targetLen
    // Use new FuncExpr rather than GetFuncExpr: the latter uses codegen FunctionRegistry::LookupFunction (which
    // has no StringView length and returns nullptr); the FuncExpr constructor goes through VectorFunction::Find and
    // hits the vectorized SimpleFunctionRegistry, and since codegen can't find it, isSupportCodegen_=false, so the
    // operator automatically picks the vectorized (ExprEval) path.
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto *lengthExpr = new FuncExpr("length", args, IntType());
    // Runtime assertion (2): the length({OMNI_STRING_VIEW})->OMNI_INT signature resolved to a non-null vectorized function.
    // VectorFunction::Find matches the signature exactly, and only the SV overload registered here can match
    // (VARCHAR signatures / codegen don't), so non-null implies the StringView length overload was registered and
    // resolved correctly. If the SV overload were missing, this would be null and the assertion would fail.
    ASSERT_NE(lengthExpr->vectorFunction, nullptr)
        << "length(OMNI_STRING_VIEW) did not resolve to a vectorized function";
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        lengthExpr,
        new LiteralExpr(targetLen, IntType()),
        BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            EXPECT_EQ(static_cast<int32_t>(std::string(outVec->GetValue(i)).size()), targetLen);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewLengthFilterShort)
{
    RunStringViewLengthFilterTest(5, 3);   // hello, abcde, tests
}
TEST(FilterTest, StringViewLengthFilterNonInline)
{
    RunStringViewLengthFilterTest(15, 2);  // world_is_longer, foo_bar_baz_qux
}
TEST(FilterTest, StringViewLengthFilterTiny)
{
    RunStringViewLengthFilterTest(2, 3);   // hi, xy, ab
}

// startsWith(string_view, pattern) -> bool — a binary predicate from the first batch of read-only functions.
// patternIsStringView controls the literal type, exercising the literal-type behavior of binary predicates:
//   true  -> {OMNI_STRING_VIEW, OMNI_STRING_VIEW} overload (when e2e stringview.enabled, literals are also built as SV)
//   false -> {OMNI_STRING_VIEW, OMNI_VARCHAR} fallback overload (literal stays VARCHAR)
// The boolean-returning StartsWith FuncExpr serves directly as the filter root; codegen has no SV StartsWith,
// so the operator takes the vectorized (ExprEval) path.
void RunStringViewStartsWithFilterTest(const std::string &pattern, bool patternIsStringView, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    std::vector<std::string> strs = {
        "hello",                 // he ✓
        "world_is_longer",       // ✗ (non-inline)
        "help",                  // he ✓
        "xyzabc",                // ✗
        "heading_for_the_hills", // he ✓ (non-inline, >12B)
        "head",                  // he ✓
        "foobar",                // ✗
        "hi",                    // ✗ (h but not he)
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // filter: startsWith(col0, pattern)
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto patternType = patternIsStringView ? StringViewType() : VarcharType();
    args.push_back(new LiteralExpr(new std::string(pattern), patternType));
    auto *filterExpr = new FuncExpr("StartsWith", args, BooleanType());
    // Runtime assertion (2): StartsWith's SV signature resolved to a non-null vectorized function (only the SV overload registered here can match).
    ASSERT_NE(filterExpr->vectorFunction, nullptr)
        << "StartsWith(OMNI_STRING_VIEW, ...) did not resolve to a vectorized function";

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            // each row really does start with pattern
            EXPECT_EQ(std::string(outVec->GetValue(i)).rfind(pattern, 0), 0u);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewStartsWithSVLiteral)
{
    RunStringViewStartsWithFilterTest("he", true, 4);   // hello, help, heading_for_the_hills, head
}
TEST(FilterTest, StringViewStartsWithVarcharLiteral)
{
    RunStringViewStartsWithFilterTest("he", false, 4);  // same as above, via the {SV,VARCHAR} overload
}
TEST(FilterTest, StringViewStartsWithNonInlineMatch)
{
    RunStringViewStartsWithFilterTest("heading", true, 1);  // only heading_for_the_hills (non-inline)
}

// endsWith(string_view, pattern) -> bool — same template as startsWith, exercising both the {SV,SV} and {SV,VARCHAR} paths.
void RunStringViewEndsWithFilterTest(const std::string &pattern, bool patternIsStringView, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    std::vector<std::string> strs = {
        "running",              // ng ✓
        "hello",                // ✗
        "a_very_long_running",  // ng ✓ (non-inline, >12B)
        "world",                // ✗
        "song",                 // ng ✓
        "foobar",               // ✗
        "ping",                 // ng ✓
        "hi",                   // ✗
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // filter: endsWith(col0, pattern)
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto patternType = patternIsStringView ? StringViewType() : VarcharType();
    args.push_back(new LiteralExpr(new std::string(pattern), patternType));
    auto *filterExpr = new FuncExpr("EndsWith", args, BooleanType());
    // Runtime assertion (2): EndsWith's SV signature resolved to a non-null vectorized function.
    ASSERT_NE(filterExpr->vectorFunction, nullptr)
        << "EndsWith(OMNI_STRING_VIEW, ...) did not resolve to a vectorized function";

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            std::string s(outVec->GetValue(i));
            // each row really does end with pattern
            ASSERT_GE(s.size(), pattern.size());
            EXPECT_EQ(s.compare(s.size() - pattern.size(), pattern.size(), pattern), 0);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewEndsWithSVLiteral)
{
    RunStringViewEndsWithFilterTest("ng", true, 4);   // running, a_very_long_running, song, ping
}
TEST(FilterTest, StringViewEndsWithVarcharLiteral)
{
    RunStringViewEndsWithFilterTest("ng", false, 4);  // same as above, via the {SV,VARCHAR} overload
}
TEST(FilterTest, StringViewEndsWithNonInlineMatch)
{
    RunStringViewEndsWithFilterTest("long_running", true, 1);  // only a_very_long_running (non-inline)
}

// contains(string_view, pattern) -> bool — same template as startsWith, exercising both the {SV,SV} and {SV,VARCHAR} paths.
void RunStringViewContainsFilterTest(const std::string &pattern, bool patternIsStringView, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    std::vector<std::string> strs = {
        "foobar",                 // bar ✓ (suffix)
        "hello",                  // ✗
        "the_bartender_is_here",  // bar ✓ (middle, non-inline >12B)
        "world",                  // ✗
        "crowbar",                // bar ✓ (suffix)
        "xyz",                    // ✗
        "barber",                 // bar ✓ (prefix)
        "hi",                     // ✗
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // filter: contains(col0, pattern)
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto patternType = patternIsStringView ? StringViewType() : VarcharType();
    args.push_back(new LiteralExpr(new std::string(pattern), patternType));
    auto *filterExpr = new FuncExpr("Contains", args, BooleanType());
    // Runtime assertion (2): Contains's SV signature resolved to a non-null vectorized function.
    ASSERT_NE(filterExpr->vectorFunction, nullptr)
        << "Contains(OMNI_STRING_VIEW, ...) did not resolve to a vectorized function";

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            // each row really does contain pattern
            EXPECT_NE(std::string(outVec->GetValue(i)).find(pattern), std::string::npos);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewContainsSVLiteral)
{
    RunStringViewContainsFilterTest("bar", true, 4);   // foobar, the_bartender_is_here, crowbar, barber
}
TEST(FilterTest, StringViewContainsVarcharLiteral)
{
    RunStringViewContainsFilterTest("bar", false, 4);  // same as above, via the {SV,VARCHAR} overload
}
TEST(FilterTest, StringViewContainsNonInlineMatch)
{
    RunStringViewContainsFilterTest("tender", true, 1);  // only the_bartender_is_here (non-inline)
}

// ascii(string_view) -> int — a single-argument read-only function (following the length template); returns the first character's code point.
void RunStringViewAsciiFilterTest(int32_t targetCode, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    // Pure ASCII, so the first character's code point is well-defined; mix of inline and non-inline (>12B).
    std::vector<std::string> strs = {
        "hello",                 // 'h' = 104
        "world",                 // 'w' = 119
        "house_is_long_enough",  // 'h' = 104 (non-inline)
        "apple",                 // 'a' = 97
        "hi",                    // 'h' = 104
        "xyz",                   // 'x' = 120
        "hammer_time_long_str",  // 'h' = 104 (non-inline)
        "banana",                // 'b' = 98
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // filter: ascii(col0) == targetCode
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto *asciiExpr = new FuncExpr("ascii", args, IntType());
    // Runtime assertion (2): ascii's SV signature resolved to a non-null vectorized function.
    ASSERT_NE(asciiExpr->vectorFunction, nullptr)
        << "ascii(OMNI_STRING_VIEW) did not resolve to a vectorized function";
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        asciiExpr,
        new LiteralExpr(targetCode, IntType()),
        BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
        for (int32_t i = 0; i < returned; i++) {
            std::string s(outVec->GetValue(i));
            ASSERT_FALSE(s.empty());
            EXPECT_EQ(static_cast<int32_t>(static_cast<unsigned char>(s[0])), targetCode);
        }
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewAsciiFilterH)
{
    RunStringViewAsciiFilterTest(104, 4);  // 'h': hello, house_is_long_enough, hi, hammer_time_long_str
}
TEST(FilterTest, StringViewAsciiFilterA)
{
    RunStringViewAsciiFilterTest(97, 1);   // 'a': apple
}

// locate(substring, string, start) -> int — 1-based position, returns 0 if not found.
// haystack (string) is an SV column; substrIsStringView selects the {SV,SV,*} vs {VARCHAR,SV,*} overload for substring.
// Uses (locate(...) <cmpOp> cmpVal) as the filter, also verifying position semantics (==2 tests the exact position + start).
void RunStringViewLocateFilterTest(const std::string &substr, bool substrIsStringView,
    omniruntime::expressions::Operator cmpOp, int32_t cmpVal, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    std::vector<std::string> strs = {
        "banana",                  // "an" at position 2
        "hello",                   // no "an" -> 0
        "the_anaconda_is_longer",  // "an" at position 5 (non-inline)
        "world",                   // 0
        "candy",                   // "an" at position 2
        "xyz",                     // 0
        "orange",                  // "an" at position 3
        "hi",                      // 0
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // locate(substr, col0, 1) <cmpOp> cmpVal
    std::vector<Expr *> args;
    auto substrType = substrIsStringView ? StringViewType() : VarcharType();
    args.push_back(new LiteralExpr(new std::string(substr), substrType));
    args.push_back(new FieldExpr(0, svType));
    args.push_back(new LiteralExpr(1, IntType()));
    auto *locateExpr = new FuncExpr("locate", args, IntType());
    // Runtime assertion (2): locate's SV signature (string = SV column) resolved to a non-null vectorized function.
    ASSERT_NE(locateExpr->vectorFunction, nullptr)
        << "locate(..., OMNI_STRING_VIEW, ...) did not resolve to a vectorized function";
    auto *filterExpr = new BinaryExpr(cmpOp, locateExpr, new LiteralExpr(cmpVal, IntType()), BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewLocateSVLiteralFound)
{
    // locate("an", col, 1) > 0 -> banana, the_anaconda_is_longer, candy, orange
    RunStringViewLocateFilterTest("an", true, omniruntime::expressions::Operator::GT, 0, 4);
}
TEST(FilterTest, StringViewLocateVarcharLiteralFound)
{
    RunStringViewLocateFilterTest("an", false, omniruntime::expressions::Operator::GT, 0, 4);
}
TEST(FilterTest, StringViewLocateExactPosition)
{
    // locate("an", col, 1) == 2 -> banana, candy (the_anaconda=5, orange=3 excluded)
    RunStringViewLocateFilterTest("an", true, omniruntime::expressions::Operator::EQ, 2, 2);
}

// LIKE(string, pattern) -> bool — uses the VectorFunction mechanism (not SimpleFunction); % matches any string, _ matches a single character.
// LikeFunction::GetStringValueFromVector now handles OMNI_STRING_VIEW vectors (FLAT/CONST). pattern may be an SV or VARCHAR literal.
void RunStringViewLikeFilterTest(const std::string &pattern, bool patternIsStringView, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    std::vector<std::string> strs = {
        "hello",                //
        "world_is_longer",      // (non-inline)
        "help",                 //
        "xyzabc",               //
        "house_is_quite_long",  // (non-inline)
        "head",                 //
        "foobar",               //
        "hi",                   //
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // filter: col0 LIKE pattern
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto patternType = patternIsStringView ? StringViewType() : VarcharType();
    args.push_back(new LiteralExpr(new std::string(pattern), patternType));
    auto *filterExpr = new FuncExpr("LIKE", args, BooleanType());
    // Runtime assertion (2): LIKE's SV signature resolved to a non-null vectorized function (the SV LikeFunction registered here).
    ASSERT_NE(filterExpr->vectorFunction, nullptr)
        << "LIKE(OMNI_STRING_VIEW, ...) did not resolve to a vectorized function";

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewLikePrefixSVLiteral)
{
    // 'h%' -> hello, help, house_is_quite_long, head, hi
    RunStringViewLikeFilterTest("h%", true, 5);
}
TEST(FilterTest, StringViewLikePrefixVarcharLiteral)
{
    RunStringViewLikeFilterTest("h%", false, 5);
}
TEST(FilterTest, StringViewLikeUnderscoreWildcard)
{
    // '_e%' -> any 1 character + 'e' + anything: hello, help, head
    RunStringViewLikeFilterTest("_e%", true, 3);
}

// instr(string, substring) -> int — 1-based position, 0 if not found. Note the argument order is the reverse of locate: instr(str, substr).
// The vectorized version did not exist before (only codegen); added here and registered only for StringView. string (arg0) = SV column, substring selects {SV,SV} vs {SV,VARCHAR}.
void RunStringViewInStrFilterTest(const std::string &substr, bool substrIsStringView,
    omniruntime::expressions::Operator cmpOp, int32_t cmpVal, int32_t expectedMatch)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t numRows = 8;
    std::vector<std::string> strs = {
        "banana",                  // "an" at position 2
        "hello",                   // none -> 0
        "the_anaconda_is_longer",  // "an" at position 5 (non-inline)
        "world",                   // 0
        "candy",                   // "an" at position 2
        "xyz",                     // 0
        "orange",                  // "an" at position 3
        "hi",                      // 0
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    // Runtime assertion (1): the input column's physical type really is StringView.
    ASSERT_EQ(col->GetTypeId(), type::OMNI_STRING_VIEW) << "input column is not OMNI_STRING_VIEW";

    // instr(col0, substr) <cmpOp> cmpVal
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, svType));
    auto substrType = substrIsStringView ? StringViewType() : VarcharType();
    args.push_back(new LiteralExpr(new std::string(substr), substrType));
    auto *instrExpr = new FuncExpr("instr", args, IntType());
    // Runtime assertion (2): instr's SV signature (string = SV column) resolved to a non-null vectorized function.
    ASSERT_NE(instrExpr->vectorFunction, nullptr)
        << "instr(OMNI_STRING_VIEW, ...) did not resolve to a vectorized function";
    auto *filterExpr = new BinaryExpr(cmpOp, instrExpr, new LiteralExpr(cmpVal, IntType()), BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);

    EXPECT_EQ(returned, expectedMatch);

    if (out != nullptr) {
        VectorHelper::FreeVecBatch(out);
    } else {
        FAIL() << "GetOutput returned nullptr";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(FilterTest, StringViewInStrSVLiteralFound)
{
    // instr(col, "an") > 0 -> banana, the_anaconda_is_longer, candy, orange
    RunStringViewInStrFilterTest("an", true, omniruntime::expressions::Operator::GT, 0, 4);
}
TEST(FilterTest, StringViewInStrVarcharLiteralFound)
{
    RunStringViewInStrFilterTest("an", false, omniruntime::expressions::Operator::GT, 0, 4);
}
TEST(FilterTest, StringViewInStrExactPosition)
{
    // instr(col, "an") == 2 -> banana, candy (the_anaconda=5, orange=3 excluded)
    RunStringViewInStrFilterTest("an", true, omniruntime::expressions::Operator::EQ, 2, 2);
}

void RunStringViewFilterEvaluatorTest(bool batchMode, const std::string& filterValue, int32_t expectedMatch)
{
    ScopedBatchExprEvaluateConfig batchExprConfig(batchMode);

    const int32_t numRows = 11;
    // Alternating inline (≤12 bytes) and non-inline (>12 bytes) strings.
    std::vector<std::string> strs = {
        "hello", "world_is_longer", "hello", "foo_bar_baz_qux", "hello",
        "bar_baz_qux_quux", "hello", "baz_qux_quux_corge", "hello", "world_is_longer",
        "xyz_longer_str"
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string(filterValue), svType),
        BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    // this is important part as it will trigger
    // jit compilation of the expression
    exprEvaluator->FilterFuncGeneration();

    omniruntime::mem::AlignedBuffer<int32_t> selectedRowsBuffer(numRows);
    ExecutionContext execContext(4096, nullptr);

    auto out = exprEvaluator->Evaluate(vecBatch, &execContext, &selectedRowsBuffer);
    // Check for null before dereferencing: Evaluate returns nullptr when the codegen projection path
    // does not support this type, and calling GetRowCount first would cause a SIGSEGV.
    ASSERT_NE(out, nullptr) << "Evaluate returned nullptr";
    auto returned = out->GetRowCount();

    EXPECT_EQ(returned, expectedMatch);

    auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
    for (int32_t i = 0; i < returned; i++) {
        EXPECT_EQ(std::string(outVec->GetValue(i)), filterValue);
    }
    VectorHelper::FreeVecBatch(out);


    delete overflowConfig;
    delete vecBatch;
}

TEST(FilterTest, RunEvaluatorStringViewShortFilterRowByRow)
{
    RunStringViewFilterEvaluatorTest(false, "hello", 5);
}

TEST(FilterTest, RunEvaluatorStringViewShortFilterBatch)
{
    RunStringViewFilterEvaluatorTest(true, "hello", 5);
}

TEST(FilterTest, RunEvaluatorStringViewLongFilterRowByRow)
{
    RunStringViewFilterEvaluatorTest(false, "world_is_longer", 2);
}

TEST(FilterTest, RunEvaluatorStringViewLongFilterBatch)
{
    RunStringViewFilterEvaluatorTest(true, "world_is_longer", 2);
}

// ---------------------------------------------------------------------------
// Vectorization (ExprEval) path for OMNI_STRING_VIEW EQ / NEQ predicates
// (Plan 01 EXPREVAL-VECTORIZATION, blocks ②③④ + notEqual registration).
//
// StringView is not supported by LLVM codegen, so the filter is evaluated through
// ExprEval (FieldExpr/LiteralExpr StringView cases +
// ComparisonFunction<Equal|NotEqual, OMNI_STRING_VIEW>). Asserts the unsupported
// codegen capability and correctness for inline (≤12B) and non-inline (>12B)
// StringView values.
//
// Both EQ and NEQ are now registered as vector functions ("equal"/"notEqual" in
// ComparisonSignatures), so both must take the vectorization path.
// ---------------------------------------------------------------------------
void RunStringViewFilterVectorizedTest(bool batchMode, omniruntime::expressions::Operator op,
    const std::string& filterValue, int32_t expectedMatch)
{
    ScopedBatchExprEvaluateConfig batchExprConfig(batchMode);

    const int32_t numRows = 11;
    std::vector<std::string> strs = {
        "hello", "world_is_longer", "hello", "foo_bar_baz_qux", "hello",
        "bar_baz_qux_quux", "hello", "baz_qux_quux_corge", "hello", "world_is_longer",
        "xyz_longer_str"
    };

    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    auto *filterExpr = new BinaryExpr(op,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string(filterValue), svType),
        BooleanType());

    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    // 5th arg preferVectorization=true → route to ExprEval (not codegen).
    auto exprEvaluator =
        std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig, true);

    // StringView EQ/NEQ must remain outside the LLVM codegen path.
    EXPECT_FALSE(exprEvaluator->IsSupportCodegen())
        << "StringView predicate unexpectedly reports LLVM codegen support";

    exprEvaluator->FilterFuncGeneration();  // no-op when !useCodegen

    omniruntime::mem::AlignedBuffer<int32_t> selectedRowsBuffer(numRows);
    ExecutionContext execContext(4096, nullptr);

    auto out = exprEvaluator->Evaluate(vecBatch, &execContext, &selectedRowsBuffer);
    ASSERT_NE(out, nullptr) << "Evaluate returned nullptr";
    auto returned = out->GetRowCount();
    EXPECT_EQ(returned, expectedMatch);

    auto *outVec = reinterpret_cast<Vector<StringView> *>(out->Get(0));
    for (int32_t i = 0; i < returned; i++) {
        if (op == omniruntime::expressions::Operator::EQ) {
            EXPECT_EQ(std::string(outVec->GetValue(i)), filterValue);
        } else {
            EXPECT_NE(std::string(outVec->GetValue(i)), filterValue);
        }
    }
    VectorHelper::FreeVecBatch(out);

    delete overflowConfig;
    delete vecBatch;
}

TEST(FilterTest, VectorizedStringViewShortEqRowByRow)
{
    RunStringViewFilterVectorizedTest(false, omniruntime::expressions::Operator::EQ, "hello", 5);
}

TEST(FilterTest, VectorizedStringViewShortEqBatch)
{
    RunStringViewFilterVectorizedTest(true, omniruntime::expressions::Operator::EQ, "hello", 5);
}

TEST(FilterTest, VectorizedStringViewLongEqRowByRow)
{
    RunStringViewFilterVectorizedTest(false, omniruntime::expressions::Operator::EQ, "world_is_longer", 2);
}

TEST(FilterTest, VectorizedStringViewLongEqBatch)
{
    RunStringViewFilterVectorizedTest(true, omniruntime::expressions::Operator::EQ, "world_is_longer", 2);
}

TEST(FilterTest, VectorizedStringViewShortNeqRowByRow)
{
    RunStringViewFilterVectorizedTest(false, omniruntime::expressions::Operator::NEQ, "hello", 6);
}

TEST(FilterTest, VectorizedStringViewShortNeqBatch)
{
    RunStringViewFilterVectorizedTest(true, omniruntime::expressions::Operator::NEQ, "hello", 6);
}

TEST(FilterTest, VectorizedStringViewLongNeqRowByRow)
{
    RunStringViewFilterVectorizedTest(false, omniruntime::expressions::Operator::NEQ, "world_is_longer", 9);
}

TEST(FilterTest, VectorizedStringViewLongNeqBatch)
{
    RunStringViewFilterVectorizedTest(true, omniruntime::expressions::Operator::NEQ, "world_is_longer", 9);
}

// When every row passes, ProcessFilterAndProject takes its full-batch field-projection shortcut.
// Keep this covered separately from the filtered-subset cases above: the shortcut has its own
// type dispatch and must preserve Vector<StringView> rather than returning a null column.
TEST(FilterTest, VectorizedStringViewAllRowsProjectionRowByRow)
{
    RunStringViewFilterVectorizedTest(false, omniruntime::expressions::Operator::NEQ, "not_present", 11);
}

TEST(FilterTest, VectorizedStringViewAllRowsProjectionBatch)
{
    RunStringViewFilterVectorizedTest(true, omniruntime::expressions::Operator::NEQ, "not_present", 11);
}

std::shared_ptr<ExpressionEvaluator> MakeStringViewValidationEvaluator(
    Expr *filterExpr, const DataTypes &inputTypes, bool preferVectorization)
{
    std::vector<Expr *> projections = { new FieldExpr(0, inputTypes.Get()[0]) };
    auto overflowConfig = std::make_unique<OverflowConfig>();
    return std::make_shared<ExpressionEvaluator>(
        filterExpr, projections, inputTypes, overflowConfig.get(), preferVectorization);
}

TEST(FilterTest, StringViewRuntimeValidationAcceptsNestedAndReversedPredicates)
{
    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));
    auto *comparison = new BinaryExpr(omniruntime::expressions::Operator::NEQ,
        new LiteralExpr(new std::string("tiny"), svType),
        new FieldExpr(0, svType), BooleanType());
    auto *notNull = new UnaryExpr(omniruntime::expressions::Operator::NOT,
        new IsNullExpr(new FieldExpr(0, svType)), BooleanType());
    auto *filterExpr = new BinaryExpr(
        omniruntime::expressions::Operator::AND, notNull, comparison, BooleanType());
    auto evaluator = MakeStringViewValidationEvaluator(filterExpr, inputTypes, true);

    auto info = ValidateStringViewFilterForRuntime(filterExpr, inputTypes);
    ASSERT_EQ(info.fieldIndexes.size(), 1);
    EXPECT_EQ(info.fieldIndexes[0], 0);
    EXPECT_FALSE(evaluator->IsSupportCodegen());
}

TEST(FilterTest, StringViewRuntimeValidationRejectsLiteralTypeMismatch)
{
    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string("tiny"), VarcharType()), BooleanType());
    EXPECT_THROW(
        ValidateStringViewFilterForRuntime(filterExpr, inputTypes),
        omniruntime::exception::OmniException);
}

TEST(FilterTest, StringViewRuntimeValidationRejectsSourceTypeMismatch)
{
    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ VarcharType() }));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string("tiny"), svType), BooleanType());
    EXPECT_THROW(
        ValidateStringViewFilterForRuntime(filterExpr, inputTypes),
        omniruntime::exception::OmniException);
}

TEST(FilterTest, StringViewRuntimeValidationRejectsMissingStringViewPredicate)
{
    auto varcharType = VarcharType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ varcharType }));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, varcharType),
        new LiteralExpr(new std::string("tiny"), varcharType), BooleanType());
    EXPECT_THROW(
        ValidateStringViewFilterForRuntime(filterExpr, inputTypes),
        omniruntime::exception::OmniException);
}

TEST(FilterTest, StringViewRuntimeValidationUsesVectorizedRouteWithoutPreference)
{
    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string("tiny"), svType), BooleanType());
    auto evaluator = MakeStringViewValidationEvaluator(filterExpr, inputTypes, false);
    ASSERT_FALSE(evaluator->IsSupportCodegen());

    auto info = ValidateStringViewFilterForRuntime(filterExpr, inputTypes);
    ASSERT_EQ(info.fieldIndexes.size(), 1);
    EXPECT_EQ(info.fieldIndexes[0], 0);
}

TEST(FilterTest, StringViewRuntimeValidationRejectsVarcharInputBatch)
{
    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(new Vector<LargeStringContainer<std::string_view>>(1));

    EXPECT_THROW(
        ValidateStringViewInputBatch(vecBatch, { 0 }),
        omniruntime::exception::OmniException);
    delete vecBatch;
}

TEST(FilterTest, StringViewRuntimeValidationIsDisabledByDefault)
{
    omniruntime::config::QueryConfig queryConfig;
    EXPECT_FALSE(queryConfig.StringViewRuntimeValidationEnabled());

    std::unordered_map<std::string, std::string> values = {
        { omniruntime::config::QueryConfig::KStringViewRuntimeValidationEnabled, "true" }
    };
    omniruntime::config::QueryConfig enabledConfig(values);
    EXPECT_TRUE(enabledConfig.StringViewRuntimeValidationEnabled());
}

// ---------------------------------------------------------------------------
// VectorHelper dispatch for OMNI_STRING_VIEW (Plan 01-04, audit gaps G1/G2)
//
// Before the fix, VectorHelper::SliceVector / CopyPositionsVector had no
// OMNI_STRING_VIEW case and hit `default` → OmniException. These exercise the
// helper-level dispatch that real operators (filter output, limit, shuffle)
// rely on when materializing a subset of rows from a StringView column.
// ---------------------------------------------------------------------------

namespace {
// inline (<=12B), non-inline (>12B), and a null row.
Vector<StringView> *MakeMixedStringViewVector()
{
    auto *vec = new Vector<StringView>(5);
    vec->SetValue(0, StringView("hi"));                        // inline
    vec->SetValue(1, StringView("this_is_a_long_value_123"));  // non-inline
    vec->SetNull(2);                                           // null
    vec->SetValue(3, StringView("abc"));                       // inline
    vec->SetValue(4, StringView("another_long_string_val"));   // non-inline
    return vec;
}
} // namespace

TEST(FilterTest, StringViewVectorHelperSlice)
{
    auto *vec = MakeMixedStringViewVector();
    auto *batch = new VectorBatch(5);
    batch->Append(vec);

    // Slice [1, 4): rows 1,2,3 → non-inline, null, inline
    auto *sliced = reinterpret_cast<Vector<StringView> *>(VectorHelper::SliceVector(vec, 1, 3));
    ASSERT_NE(sliced, nullptr);
    EXPECT_EQ(sliced->GetTypeId(), type::OMNI_STRING_VIEW);
    EXPECT_EQ(sliced->GetSize(), 3);
    EXPECT_EQ(std::string(sliced->GetValue(0)), "this_is_a_long_value_123");
    EXPECT_TRUE(sliced->IsNull(1));
    EXPECT_EQ(std::string(sliced->GetValue(2)), "abc");

    delete sliced;
    VectorHelper::FreeVecBatch(batch);
}

TEST(FilterTest, StringViewVectorHelperCopyPositions)
{
    auto *vec = MakeMixedStringViewVector();
    auto *batch = new VectorBatch(5);
    batch->Append(vec);

    // Reorder/select rows 4,2,1,0 → non-inline, null, non-inline, inline
    int positions[] = {4, 2, 1, 0};
    auto *copied =
        reinterpret_cast<Vector<StringView> *>(VectorHelper::CopyPositionsVector(vec, positions, 0, 4));
    ASSERT_NE(copied, nullptr);
    EXPECT_EQ(copied->GetTypeId(), type::OMNI_STRING_VIEW);
    EXPECT_EQ(copied->GetSize(), 4);
    EXPECT_EQ(std::string(copied->GetValue(0)), "another_long_string_val");
    EXPECT_TRUE(copied->IsNull(1));
    EXPECT_EQ(std::string(copied->GetValue(2)), "this_is_a_long_value_123");
    EXPECT_EQ(std::string(copied->GetValue(3)), "hi");

    delete copied;
    VectorHelper::FreeVecBatch(batch);
}

// End-to-end: real FilterAndProjectOperator output, then slice the result via
// VectorHelper — proves the operator→helper path for StringView is unblocked.
TEST(FilterTest, StringViewFilterOutputThenSlice)
{
    ScopedBatchExprEvaluateConfig batchExprConfig(true);

    const int32_t numRows = 6;
    std::vector<std::string> strs = {
        "match_value_long", "x", "match_value_long", "y", "match_value_long", "z"
    };
    auto svType = StringViewType();
    DataTypes inputTypes(std::vector<DataTypePtr>({ svType }));

    auto *col = new Vector<StringView>(numRows);
    for (int i = 0; i < numRows; i++) {
        col->SetValue(i, StringView(strs[i]));
    }
    auto *vecBatch = new VectorBatch(numRows);
    vecBatch->Append(col);

    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
        new FieldExpr(0, svType),
        new LiteralExpr(new std::string("match_value_long"), svType),
        BooleanType());
    std::vector<Expr *> projections = { new FieldExpr(0, svType) };
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, inputTypes, overflowConfig);
    auto *factory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    auto *op = factory->CreateOperator(nullptr);

    op->AddInput(vecBatch);
    VectorBatch *out = nullptr;
    int32_t returned = op->GetOutput(&out);
    EXPECT_EQ(returned, 3);  // three "match_value_long" rows
    ASSERT_NE(out, nullptr);

    // Now slice the filter OUTPUT (StringView vector) — this is what would crash
    // pre-fix when downstream operators materialize a sub-range.
    auto *outVec = out->Get(0);
    auto *sliced = reinterpret_cast<Vector<StringView> *>(VectorHelper::SliceVector(outVec, 1, 2));
    ASSERT_NE(sliced, nullptr);
    EXPECT_EQ(sliced->GetSize(), 2);
    EXPECT_EQ(std::string(sliced->GetValue(0)), "match_value_long");
    EXPECT_EQ(std::string(sliced->GetValue(1)), "match_value_long");

    delete sliced;
    VectorHelper::FreeVecBatch(out);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}
#endif

}
