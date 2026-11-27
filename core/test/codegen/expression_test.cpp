/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * Description: ...
 */
#include <string>
#include <vector>
#include <chrono>
#include "gtest/gtest.h"
#include "codegen/filter_codegen.h"
#include "operator/filter/filter_and_project.h"
#include "expression/jsonparser/jsonparser.cpp"
#include "operator/projection/projection.h"
#include "util/test_util.h"
#include "type/base_operations.h"
#include "expression/expr_verifier.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace ExpressionTest {
static constexpr int TEST_EXPR_PERF_TIME = 1;
template <typename... Ts> struct Arguments {
    int64_t *vals;
    int rowCount;
    int colCount = 3;
    int32_t *selected;
    uint8_t **bitmap;
    int32_t **offsets;
    int64_t context;
    int64_t dictionaries[5] = {};
    omniruntime::op::ExecutionContext *ctx;

    Arguments(int rowCount, int initSize, Ts *... initArgs) : rowCount(rowCount)
    {
        int columnCount = sizeof...(Ts);
        vals = new int64_t[columnCount]();

        int colIndex = 0;
        (Create<Ts>(vals, colIndex++, rowCount), ...);
        colIndex = 0;
        (Init(vals[colIndex++], initSize, initArgs), ...);

        selected = new int32_t[rowCount];

        bitmap = new uint8_t *[colCount];
        for (int i = 0; i < colCount; i++) {
            bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(rowCount)] { 0 };
        }

        offsets = new int32_t *[colCount];
        for (int col = 0; col < colCount; col++) {
            offsets[col] = new int32_t[1];
        }

        ctx = new omniruntime::op::ExecutionContext(4096, nullptr);
        context = reinterpret_cast<int64_t>(ctx);
    }

    template <typename T> void Create(int64_t *table, int colIndex, int rowNum)
    {
        table[colIndex] = reinterpret_cast<int64_t>(new T[rowNum]());
    }

    template <typename T> void Init(int64_t target, int size, T *arg)
    {
        T *col = reinterpret_cast<T *>(target);
        for (int i = 0; i < size; i++) {
            col[i] = arg[i];
        }
    }
};

template <typename T> void DeleteArgument(T &args)
{
    for (int i = 0; i < args.colCount; i++) {
        delete[] args.bitmap[i];
        delete[] args.offsets[i];
        auto column = reinterpret_cast<int32_t *>(args.vals[i]);
        delete[] column;
    }

    delete[] args.vals;
    delete[] args.bitmap;
    delete[] args.offsets;
    delete[] args.selected;
    delete args.ctx;
}

void PrintValueLine(const double *wallTimeArray, const double *cpuTimeArray, const int32_t arrLength)
{
    std::cout << "Wall Time: [";
    for (int i = 0; i < arrLength; ++i) {
        if (i == arrLength - 1) {
            std::cout << wallTimeArray[i];
        } else {
            std::cout << wallTimeArray[i];
            std::cout << ",";
        }
    }
    std::cout << "]" << std::endl;

    std::cout << "Cpu Time: [";
    for (int i = 0; i < arrLength; ++i) {
        if (i == arrLength - 1) {
            std::cout << cpuTimeArray[i];
        } else {
            std::cout << cpuTimeArray[i];
            std::cout << ",";
        }
    }
    std::cout << "]" << std::endl;
}

Expr *PrepareLongExpr()
{
    LiteralExpr *subLeft = new LiteralExpr(100L, LongType());
    FieldExpr *subRight = new FieldExpr(1, LongType());
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, LongType());
    LiteralExpr *addLeft = new LiteralExpr(100L, LongType());
    FieldExpr *addRight = new FieldExpr(2, LongType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, LongType());
    FieldExpr *mulLeft = new FieldExpr(0, LongType());
    BinaryExpr *mulExpr1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, LongType());
    BinaryExpr *mulExpr2 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, LongType());
    return mulExpr2;
}

// Test projection on int64_t data type
// Formula: l_extendprice * (100 - l_discount) * (100 + l_tax)
// Purpose: just test expression on int64_t data type
TEST(ExpressionTest, q1LongType)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int64_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int64_t[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 100000 + 20;
        col2[i] = rand() % 50;
        col3[i] = rand() % 8 + 5;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ LongType(), LongType(), LongType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    ExprVerifier verifier;
    LiteralExpr *subLeft = new LiteralExpr(100L, LongType());
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    FieldExpr *subRight = new FieldExpr(1, LongType());
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, LongType());
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    LiteralExpr *addLeft = new LiteralExpr(100L, LongType());
    FieldExpr *addRight = new FieldExpr(2, LongType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, LongType());
    FieldExpr *mulLeft = new FieldExpr(0, LongType());
    BinaryExpr *mulExpr1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, LongType());
    BinaryExpr *mulExpr2 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, LongType());
    std::vector<Expr *> exprs = { mulExpr2 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    std::cout << "prepare projection operator : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *outputVecBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            int64_t result = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            int64_t actualLong = ((reinterpret_cast<Vector<int64_t> *>(t->Get(0)))->GetValue(i)) *
                (100L - (reinterpret_cast<Vector<int64_t> *>(t->Get(1)))->GetValue(i)) *
                (100L + (reinterpret_cast<Vector<int64_t> *>(t->Get(2)))->GetValue(i));
            EXPECT_EQ(result, actualLong);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

// Test projection on int8_t data type
// Formula: l_extendprice * (10 - l_discount) * (10 + l_tax)
// Purpose: just test expression on int8_t data type
TEST(ExpressionTest, q1ByteType)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int8_t[numRows];
    auto *col2 = new int8_t[numRows];
    auto *col3 = new int8_t[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 2 + 1;
        col2[i] = rand() % 5;
        col3[i] = rand() % 3;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ ByteType(), ByteType(), ByteType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    ExprVerifier verifier;
    LiteralExpr *subLeft = new LiteralExpr(static_cast<int8_t>(10), ByteType(), false);
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    FieldExpr *subRight = new FieldExpr(1, ByteType());
    EXPECT_TRUE(verifier.VisitExpr(*subRight));
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, ByteType());
    EXPECT_TRUE(verifier.VisitExpr(*subExpr));
    LiteralExpr *addLeft = new LiteralExpr(static_cast<int8_t>(10), ByteType());
    FieldExpr *addRight = new FieldExpr(2, ByteType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, ByteType());
    FieldExpr *mulLeft = new FieldExpr(0, ByteType());
    BinaryExpr *mulExpr1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, ByteType());
    BinaryExpr *mulExpr2 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, ByteType());
    std::vector<Expr *> exprs = { mulExpr2 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    std::cout << "prepare projection operator : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *outputVecBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            int8_t result = (reinterpret_cast<Vector<int8_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            int8_t actualByte = ((reinterpret_cast<Vector<int8_t> *>(t->Get(0)))->GetValue(i)) *
                (10 - (reinterpret_cast<Vector<int8_t> *>(t->Get(1)))->GetValue(i)) *
                (10 + (reinterpret_cast<Vector<int8_t> *>(t->Get(2)))->GetValue(i));
            EXPECT_EQ(result, actualByte);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

// Test projection on int16_t data type
// Formula: l_extendprice * (100 - l_discount) * (100 + l_tax)
// Purpose: just test expression on int16_t data type
TEST(ExpressionTest, q1ShortType)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int16_t[numRows];
    auto *col2 = new int16_t[numRows];
    auto *col3 = new int16_t[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 5;
        col2[i] = rand() % 50 + 20;
        col3[i] = rand() % 50 + 3;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ ShortType(), ShortType(), ShortType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    ExprVerifier verifier;
    LiteralExpr *subLeft = new LiteralExpr(static_cast<int16_t>(100), ShortType(), false);
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    FieldExpr *subRight = new FieldExpr(1, ShortType());
    EXPECT_TRUE(verifier.VisitExpr(*subRight));
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, ShortType());
    EXPECT_TRUE(verifier.VisitExpr(*subExpr));
    LiteralExpr *addLeft = new LiteralExpr(static_cast<int16_t>(100), ShortType());
    FieldExpr *addRight = new FieldExpr(2, ShortType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, ShortType());
    FieldExpr *mulLeft = new FieldExpr(0, ShortType());
    BinaryExpr *mulExpr1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, ShortType());
    BinaryExpr *mulExpr2 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, ShortType());
    std::vector<Expr *> exprs = { mulExpr2 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    std::cout << "prepare projection operator : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *outputVecBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            int16_t result = (reinterpret_cast<Vector<int16_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            int16_t actualShort = ((reinterpret_cast<Vector<int16_t> *>(t->Get(0)))->GetValue(i)) *
                (100 - (reinterpret_cast<Vector<int16_t> *>(t->Get(1)))->GetValue(i)) *
                (100 + (reinterpret_cast<Vector<int16_t> *>(t->Get(2)))->GetValue(i));
            EXPECT_EQ(result, actualShort);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

// Test projection on double data type
// Formula: l_extendprice * (1 - l_discount) * (1 + l_tax)
// Purpose: just test expression on double data type
TEST(ExpressionTest, q1DoubleType)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new double[numRows];
    auto *col2 = new double[numRows];
    auto *col3 = new double[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 100000 + 20;
        col2[i] = rand() % 50;
        col3[i] = rand() % 8 + 5;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ DoubleType(), DoubleType(), DoubleType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    ExprVerifier verifier;
    LiteralExpr *subLeft = new LiteralExpr(1.0, DoubleType());
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    FieldExpr *subRight = new FieldExpr(1, DoubleType());
    EXPECT_TRUE(verifier.VisitExpr(*subRight));
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, DoubleType());
    EXPECT_TRUE(verifier.VisitExpr(*subExpr));
    LiteralExpr *addLeft = new LiteralExpr(1.0, DoubleType());
    FieldExpr *addRight = new FieldExpr(2, DoubleType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, DoubleType());
    FieldExpr *mulLeft = new FieldExpr(0, DoubleType());
    BinaryExpr *mulExpr1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, DoubleType());
    BinaryExpr *mulExpr2 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, DoubleType());
    std::vector<Expr *> exprs = { mulExpr2 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *outputVecBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            double result = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
            double actualDouble = ((reinterpret_cast<Vector<double> *>(t->Get(0)))->GetValue(i)) *
                (1.0 - (reinterpret_cast<Vector<double> *>(t->Get(1)))->GetValue(i)) *
                (1.0 + (reinterpret_cast<Vector<double> *>(t->Get(2)))->GetValue(i));
            EXPECT_EQ(result, actualDouble);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

// Test filter on double data type
// Formula: l_extendprice * (1 - l_discount) * (1 + l_tax) < 1000.00
// Purpose: just test expression on double data type
TEST(ExpressionTest, q1DoubleFilter)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new double[numRows];
    auto *col2 = new double[numRows];
    auto *col3 = new double[numRows];
    for (int64_t i = 0; i < numRows; i++) {
        auto d = rand() % 100000 + 20;
        col1[i] = d / 100.0;
    }
    for (int64_t i = 0; i < numRows; i++) {
        auto d = rand() % 50;
        col2[i] = d / 100.0;
    }
    for (int64_t i = 0; i < numRows; i++) {
        auto d = rand() % 8 + 5;
        col3[i] = d / 100.0;
    }

    Timer timer;
    timer.SetStart();

    // prepare expression
    LiteralExpr *subLeft = new LiteralExpr(1.0, DoubleType());
    FieldExpr *subRight = new FieldExpr(1, DoubleType());
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, DoubleType());
    LiteralExpr *addLeft = new LiteralExpr(1.0, DoubleType());
    FieldExpr *addRight = new FieldExpr(2, DoubleType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, DoubleType());
    FieldExpr *mulLeft = new FieldExpr(0, DoubleType());
    BinaryExpr *mulExpr1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, DoubleType());
    BinaryExpr *mulExpr2 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, DoubleType());

    LiteralExpr *minValue = new LiteralExpr(1000.00, DoubleType());
    BinaryExpr *ltExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, mulExpr2, minValue, BooleanType());

    const string defaultTestFunctionName = "double-comparison-function";
    Arguments<double, double, double> args { numRows, numRows, col1, col2, col3 };

    DataTypes inputTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType(), DoubleType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    auto overflowConfig = new OverflowConfig();
    auto codegen = FilterCodeGen(defaultTestFunctionName, *ltExpr, overflowConfig);
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    timer.CalculateElapse();
    std::cout << "compile expression: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;


    int32_t result = 0;
    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        timer.Reset();
        timer.SetStart();
        result = func(args.vals, args.rowCount, args.selected, (int64_t *)args.bitmap, (int64_t *)args.offsets,
            args.context, args.dictionaries);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;
        // verify result
        int32_t actualCount = 0;
        for (int j = 0; j < numRows; j++) {
            double actualDouble = col1[j] * (1.0 - col2[j]) * (1.0 + col3[j]);
            if (actualDouble < 1000.0) {
                actualCount++;
            }
        }
        EXPECT_EQ(result, actualCount);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    DeleteArgument(args);
    delete ltExpr;
    delete[] col1;
    delete[] col2;
    delete[] col3;
    delete overflowConfig;
    VectorHelper::FreeVecBatch(t);
}

// Test filter on decimal64 data type
// Formula: l_extendprice * (1 - l_discount) * (1 + l_tax) < 1000.00
// Purpose: just test expression on decimal64 data type
TEST(ExpressionTest, q1Decimal64Type)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int64_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int64_t[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 100000 + 20;
        col2[i] = rand() % 50;
        col3[i] = rand() % 8 + 5;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ LongType(), LongType(), LongType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    ExprVerifier verifier;
    LiteralExpr *subLeft = new LiteralExpr(100L, Decimal64Type(12, 2));
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    FieldExpr *subRight = new FieldExpr(1, Decimal64Type(12, 2));
    EXPECT_TRUE(verifier.VisitExpr(*subRight));
    BinaryExpr *subExpr =
        new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, Decimal64Type(12, 2));
    EXPECT_TRUE(verifier.VisitExpr(*subExpr));
    LiteralExpr *addLeft = new LiteralExpr(100L, Decimal64Type(12, 2));
    FieldExpr *addRight = new FieldExpr(2, Decimal64Type(12, 2));
    BinaryExpr *addExpr =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal64Type(12, 2));
    FieldExpr *mulLeft = new FieldExpr(0, Decimal64Type(12, 2));
    BinaryExpr *mulExpr1 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, Decimal64Type(12, 4));
    BinaryExpr *mulExpr2 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, Decimal64Type(12, 6));
    std::vector<Expr *> exprs = { mulExpr2 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(std::move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *vectorBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int j = 0; j < numRows; j++) {
            int64_t result = (reinterpret_cast<Vector<int64_t> *>(vectorBatch->Get(0)))->GetValue(j);
            auto val1 = reinterpret_cast<Vector<int64_t> *>(t->Get(0))->GetValue(j);
            auto val2 = reinterpret_cast<Vector<int64_t> *>(t->Get(1))->GetValue(j);
            auto val3 = reinterpret_cast<Vector<int64_t> *>(t->Get(2))->GetValue(j);
            int64_t actualDecimal64 = val1 * (100 - val2) * (100 + val3);
            EXPECT_EQ(result, actualDecimal64);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

// Test filter on decimal128 data type
// Formula: l_extendprice * (1 - l_discount) * (1 + l_tax) < 1000.00
// Purpose: just test expression on decimal128 data type
TEST(ExpressionTest, q1Decimal128Type)
{
    const int32_t numRows = 200;

    // prepare data
    const int32_t rounds = TEST_EXPR_PERF_TIME;
    auto *data1 = new int64_t[numRows * 2];
    auto *data2 = new int64_t[numRows * 2];
    auto *data3 = new int64_t[numRows * 2];

    for (int64_t i = 0; i < numRows; i++) {
        data1[2 * i] = rand() % 100000 + 20;
        data1[2 * i + 1] = 0;
        data2[2 * i] = rand() % 50;
        data2[2 * i + 1] = 0;
        data3[2 * i] = rand() % 8 + 5;
        data3[2 * i + 1] = 0;
    }

    std::vector<DataTypePtr> vecOfTypes = { Decimal128Type(), Decimal128Type(), Decimal128Type() };
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    ExprVerifier verifier;
    LiteralExpr *subLeft = new LiteralExpr(new std::string("100"), Decimal128Type(32, 2));
    EXPECT_TRUE(verifier.VisitExpr(*subLeft));
    FieldExpr *subRight = new FieldExpr(1, Decimal128Type(32, 2));
    EXPECT_TRUE(verifier.VisitExpr(*subRight));
    BinaryExpr *subExpr =
        new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, Decimal128Type(32, 2));
    EXPECT_TRUE(verifier.VisitExpr(*subExpr));
    LiteralExpr *addLeft = new LiteralExpr(new std::string("100"), Decimal128Type(32, 2));
    FieldExpr *addRight = new FieldExpr(2, Decimal128Type(32, 2));
    BinaryExpr *addExpr =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(32, 2));
    FieldExpr *mulLeft = new FieldExpr(0, Decimal128Type(32, 2));
    BinaryExpr *mulExpr1 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, subExpr, Decimal128Type(32, 4));
    BinaryExpr *mulExpr2 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExpr1, addExpr, Decimal128Type(32, 6));
    std::vector<Expr *> exprs = { mulExpr2 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, data1, data2, data3);

    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(std::move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *vectorBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            Decimal128 result = reinterpret_cast<Vector<Decimal128> *>(vectorBatch->Get(0))->GetValue(i);
            EXPECT_EQ(result.HighBits(), 0);

            Decimal128 val1 = reinterpret_cast<Vector<Decimal128> *>(t->Get(0))->GetValue(i);
            Decimal128 val2 = reinterpret_cast<Vector<Decimal128> *>(t->Get(1))->GetValue(i);
            Decimal128 val3 = reinterpret_cast<Vector<Decimal128> *>(t->Get(2))->GetValue(i);

            int64_t actualDecimal128 = val1.LowBits() * (100 - val2.LowBits()) * (100 + val3.LowBits());
            EXPECT_EQ(result.LowBits(), actualDecimal128);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] data1;
    delete[] data2;
    delete[] data3;
}

TEST(ExpressionTest, Decimal64BigIntToDecimal128)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // Prepare data
    auto *bigintCol = new int64_t[numRows];
    for (int64_t i = 0; i < numRows; i++) {
        bigintCol[i] = rand() % 1000; // Random bigint values
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ LongType() });
    DataTypes inputTypes(vecOfTypes);

    std::string unparsed = R"(
    {
        "exprType": "BINARY",
        "left": {
                "dataType" : 6,
                "exprType": "LITERAL",
                "isNull": false,
                "precision": 4,
                "scale": 3,
                "value": 998
            },
            "operator": "MULTIPLY",
            "precision": 24,
            "returnType": 7,
            "right": {
                "colVal": 0,
                "dataType": 2,
                "exprType": "FIELD_REFERENCE"
            },
            "scale": 3
    }
    )";
    nlohmann::json unparsedJSON = nlohmann::json::parse(unparsed);
    std::cout << "parsed into JSON" << std::endl;

    Expr *expr = JSONParser::ParseJSON(unparsedJSON);
    ExprPrinter printer;
    expr->Accept(printer);
    cout << endl;

    std::vector<Expr *> exprs = { expr };

    Timer timer;
    timer.SetStart();

    // Create row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, bigintCol);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // Prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(std::move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
            << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for(int i = 0; i < rounds; i++) {
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *vectorBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // Verify result
        for (int j = 0; j < numRows; j++) {
            Decimal128 result = reinterpret_cast<Vector<Decimal128> *>(vectorBatch->Get(0))->GetValue(j);
            int64_t bigintValue = reinterpret_cast<Vector<int64_t> *>(t->Get(0))->GetValue(j);
            int64_t expectedValue = (998 * bigintValue); // Apply decimal scale 3
            EXPECT_EQ(result.LowBits(), expectedValue);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] bigintCol;
}

// Test function Cast(decimal64 as double)
// test cost of Cast function
TEST(ExpressionTest, q1Decimal64Cast)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int64_t[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 100000 + 20;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ LongType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    FieldExpr *col0Expr = new FieldExpr(0, Decimal64Type(12, 2));

    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(col0Expr);
    auto cast0 = GetFuncExpr(castStr, args, DoubleType());

    std::vector<Expr *> exprs = { cast0 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        auto copy = DuplicateVectorBatch(t);
        VectorBatch *vectorBatch = nullptr;

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;
        for (int i = 0; i < numRows; i++) {
            double result = (reinterpret_cast<Vector<double> *>(vectorBatch->Get(0)))->GetValue(i);
            double actual = ((reinterpret_cast<Vector<int64_t> *>(t->Get(0)))->GetValue(i)) / 100.00;
            EXPECT_EQ(result, actual);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
}

// Test projection on date data type
// Formula: l_shipdate < '2022-10-01'
TEST(ExpressionTest, q1DateType)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int32_t[numRows];
    for (int64_t i = 0; i < numRows; i++) {
        // 2020-01-01 18262
        col1[i] = rand() % 1000 + 18262;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ IntType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    FieldExpr *ltLeft = new FieldExpr(0, Date32Type());
    // 2022-10-01 19266
    LiteralExpr *ltRight = new LiteralExpr(19266, Date32Type());
    BinaryExpr *ltExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, ltLeft, ltRight, BooleanType());
    std::vector<Expr *> exprs = { ltExpr };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        VectorBatch *vectorBatch = nullptr;
        auto copy = DuplicateVectorBatch(t);

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            bool result = (reinterpret_cast<Vector<bool> *>(vectorBatch->Get(0)))->GetValue(i);
            bool actualBool = ((reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(i)) < 19266;
            EXPECT_EQ(result, actualBool);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
}

// Test expression if (l_shipdate >= '2022-01-01' and l_shipdate < '2023-01-01')
//             then revenue/10000 else 0
TEST(ExpressionTest, q1Case1)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int32_t[numRows];
    auto *col2 = new double[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        // 2020-01-01 18262
        col1[i] = rand() % 1000 + 18262;
        col2[i] = rand() % 10000000 + 200;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ IntType(), DoubleType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    // 2022-01-01 18993 2023-01-01 19358
    ExprVerifier verifier;
    BinaryExpr *conditionLeft = new BinaryExpr(omniruntime::expressions::Operator::GTE, new FieldExpr(0, Date32Type()),
        new LiteralExpr(18993, Date32Type()), BooleanType());
    EXPECT_TRUE(verifier.VisitExpr(*conditionLeft));
    BinaryExpr *conditionRight = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, Date32Type()),
        new LiteralExpr(19358, Date32Type()), BooleanType());
    EXPECT_TRUE(verifier.VisitExpr(*conditionRight));
    BinaryExpr *condition =
        new BinaryExpr(omniruntime::expressions::Operator::AND, conditionLeft, conditionRight, BooleanType());
    EXPECT_TRUE(verifier.VisitExpr(*condition));
    FieldExpr *divLeft = new FieldExpr(1, DoubleType());
    LiteralExpr *divRight = new LiteralExpr(10000.0, DoubleType());
    BinaryExpr *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, divLeft, divRight, DoubleType());
    LiteralExpr *falseExpr = new LiteralExpr(0.0, DoubleType());
    IfExpr *ifExpr = new IfExpr(condition, divExpr, falseExpr);
    EXPECT_TRUE(verifier.VisitExpr(*ifExpr));
    std::vector<Expr *> exprs = { ifExpr };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();

    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        VectorBatch *vectorBatch = nullptr;
        auto copy = DuplicateVectorBatch(t);

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            double result = (reinterpret_cast<Vector<double> *>(vectorBatch->Get(0)))->GetValue(i);
            double actualDouble = (((reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(i)) >= 18993 &&
                ((reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(i)) < 19358) ?
                ((reinterpret_cast<Vector<double> *>(t->Get(1)))->GetValue(i)) / 10000.0 :
                0.0;
            EXPECT_EQ(result, actualDouble);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
}

// Use switch case to implement 12 GPA conversion
// case when mark >= 90 then 12
//      when mark >= 85 then 11
//      when mark >= 80 then 10
//      when mark >= 77 then 9
//      when mark >= 74 then 8
//      when mark >= 70 then 7
//      when mark >= 67 then 6
//      when mark >= 64 then 5
//      when mark >= 60 then 4
//      when mark >= 57 then 3
//      when mark >= 54 then 2
//      when mark >= 50 then 1
//      else 0 end
TEST(ExpressionTest, q1SwitchCase)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int32_t[numRows];
    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 60 + 40;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ IntType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    FieldExpr *gteLeft0 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight0 = new LiteralExpr(90, IntType());
    BinaryExpr *gteExpr0 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft0, gteRight0, BooleanType());

    FieldExpr *gteLeft1 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight1 = new LiteralExpr(85, IntType());
    BinaryExpr *gteExpr1 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft1, gteRight1, BooleanType());

    FieldExpr *gteLeft2 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight2 = new LiteralExpr(80, IntType());
    BinaryExpr *gteExpr2 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft2, gteRight2, BooleanType());

    FieldExpr *gteLeft3 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight3 = new LiteralExpr(77, IntType());
    BinaryExpr *gteExpr3 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft3, gteRight3, BooleanType());

    FieldExpr *gteLeft4 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight4 = new LiteralExpr(74, IntType());
    BinaryExpr *gteExpr4 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft4, gteRight4, BooleanType());

    FieldExpr *gteLeft5 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight5 = new LiteralExpr(70, IntType());
    BinaryExpr *gteExpr5 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft5, gteRight5, BooleanType());

    FieldExpr *gteLeft6 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight6 = new LiteralExpr(67, IntType());
    BinaryExpr *gteExpr6 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft6, gteRight6, BooleanType());

    FieldExpr *gteLeft7 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight7 = new LiteralExpr(64, IntType());
    BinaryExpr *gteExpr7 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft7, gteRight7, BooleanType());

    FieldExpr *gteLeft8 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight8 = new LiteralExpr(60, IntType());
    BinaryExpr *gteExpr8 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft8, gteRight8, BooleanType());

    FieldExpr *gteLeft9 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight9 = new LiteralExpr(57, IntType());
    BinaryExpr *gteExpr9 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft9, gteRight9, BooleanType());

    FieldExpr *gteLeft10 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight10 = new LiteralExpr(54, IntType());
    BinaryExpr *gteExpr10 =
        new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft10, gteRight10, BooleanType());

    FieldExpr *gteLeft11 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight11 = new LiteralExpr(50, IntType());
    BinaryExpr *gteExpr11 =
        new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft11, gteRight11, BooleanType());

    LiteralExpr *literalResult0 = new LiteralExpr(12, IntType());
    LiteralExpr *literalResult1 = new LiteralExpr(11, IntType());
    LiteralExpr *literalResult2 = new LiteralExpr(10, IntType());
    LiteralExpr *literalResult3 = new LiteralExpr(9, IntType());
    LiteralExpr *literalResult4 = new LiteralExpr(8, IntType());
    LiteralExpr *literalResult5 = new LiteralExpr(7, IntType());
    LiteralExpr *literalResult6 = new LiteralExpr(6, IntType());
    LiteralExpr *literalResult7 = new LiteralExpr(5, IntType());
    LiteralExpr *literalResult8 = new LiteralExpr(4, IntType());
    LiteralExpr *literalResult9 = new LiteralExpr(3, IntType());
    LiteralExpr *literalResult10 = new LiteralExpr(2, IntType());
    LiteralExpr *literalResult11 = new LiteralExpr(1, IntType());
    LiteralExpr *literalResult12 = new LiteralExpr(0, IntType());

    std::vector<std::pair<Expr *, Expr *>> whenClause;
    std::pair<Expr *, Expr *> when0;
    std::pair<Expr *, Expr *> when1;
    std::pair<Expr *, Expr *> when2;
    std::pair<Expr *, Expr *> when3;
    std::pair<Expr *, Expr *> when4;
    std::pair<Expr *, Expr *> when5;
    std::pair<Expr *, Expr *> when6;
    std::pair<Expr *, Expr *> when7;
    std::pair<Expr *, Expr *> when8;
    std::pair<Expr *, Expr *> when9;
    std::pair<Expr *, Expr *> when10;
    std::pair<Expr *, Expr *> when11;

    when0.first = gteExpr0;
    when0.second = literalResult0;
    when1.first = gteExpr1;
    when1.second = literalResult1;
    when2.first = gteExpr2;
    when2.second = literalResult2;
    when3.first = gteExpr3;
    when3.second = literalResult3;
    when4.first = gteExpr4;
    when4.second = literalResult4;
    when5.first = gteExpr5;
    when5.second = literalResult5;
    when6.first = gteExpr6;
    when6.second = literalResult6;
    when7.first = gteExpr7;
    when7.second = literalResult7;
    when8.first = gteExpr8;
    when8.second = literalResult8;
    when9.first = gteExpr9;
    when9.second = literalResult9;
    when10.first = gteExpr10;
    when10.second = literalResult10;
    when11.first = gteExpr11;
    when11.second = literalResult11;

    whenClause.push_back(when0);
    whenClause.push_back(when1);
    whenClause.push_back(when2);
    whenClause.push_back(when3);
    whenClause.push_back(when4);
    whenClause.push_back(when5);
    whenClause.push_back(when6);
    whenClause.push_back(when7);
    whenClause.push_back(when8);
    whenClause.push_back(when9);
    whenClause.push_back(when10);
    whenClause.push_back(when11);

    SwitchExpr *switchExpr = new SwitchExpr(whenClause, literalResult12);
    std::vector<Expr *> exprs = { switchExpr };


    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        VectorBatch *vectorBatch = nullptr;
        auto copy = DuplicateVectorBatch(t);

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            int32_t result = (reinterpret_cast<Vector<int32_t> *>(vectorBatch->Get(0)))->GetValue(i);
            int32_t actualInt;
            auto col1Valuei = (reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(i);
            if (col1Valuei >= 90) {
                actualInt = 12;
            } else if (col1Valuei >= 85) {
                actualInt = 11;
            } else if (col1Valuei >= 80) {
                actualInt = 10;
            } else if (col1Valuei >= 77) {
                actualInt = 9;
            } else if (col1Valuei >= 74) {
                actualInt = 8;
            } else if (col1Valuei >= 70) {
                actualInt = 7;
            } else if (col1Valuei >= 67) {
                actualInt = 6;
            } else if (col1Valuei >= 64) {
                actualInt = 5;
            } else if (col1Valuei >= 60) {
                actualInt = 4;
            } else if (col1Valuei >= 57) {
                actualInt = 3;
            } else if (col1Valuei >= 54) {
                actualInt = 2;
            } else if (col1Valuei >= 50) {
                actualInt = 1;
            } else {
                actualInt = 0;
            }
            EXPECT_EQ(result, actualInt);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
}

// use if statement to implement 12 GPA conversion
TEST(ExpressionTest, q1If)
{
    const int32_t numRows = 200;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    auto *col1 = new int32_t[numRows];

    for (int64_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 60 + 40;
    }

    auto vecOfTypes = std::vector<DataTypePtr>({ IntType() });
    DataTypes inputTypes(vecOfTypes);

    // prepare expression
    FieldExpr *gteLeft0 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight0 = new LiteralExpr(90, IntType());
    BinaryExpr *gteExpr0 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft0, gteRight0, BooleanType());

    FieldExpr *gteLeft1 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight1 = new LiteralExpr(85, IntType());
    BinaryExpr *gteExpr1 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft1, gteRight1, BooleanType());

    FieldExpr *gteLeft2 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight2 = new LiteralExpr(80, IntType());
    BinaryExpr *gteExpr2 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft2, gteRight2, BooleanType());

    FieldExpr *gteLeft3 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight3 = new LiteralExpr(77, IntType());
    BinaryExpr *gteExpr3 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft3, gteRight3, BooleanType());

    FieldExpr *gteLeft4 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight4 = new LiteralExpr(74, IntType());
    BinaryExpr *gteExpr4 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft4, gteRight4, BooleanType());

    FieldExpr *gteLeft5 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight5 = new LiteralExpr(70, IntType());
    BinaryExpr *gteExpr5 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft5, gteRight5, BooleanType());

    FieldExpr *gteLeft6 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight6 = new LiteralExpr(67, IntType());
    BinaryExpr *gteExpr6 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft6, gteRight6, BooleanType());

    FieldExpr *gteLeft7 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight7 = new LiteralExpr(64, IntType());
    BinaryExpr *gteExpr7 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft7, gteRight7, BooleanType());

    FieldExpr *gteLeft8 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight8 = new LiteralExpr(60, IntType());
    BinaryExpr *gteExpr8 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft8, gteRight8, BooleanType());

    FieldExpr *gteLeft9 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight9 = new LiteralExpr(57, IntType());
    BinaryExpr *gteExpr9 = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft9, gteRight9, BooleanType());

    FieldExpr *gteLeft10 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight10 = new LiteralExpr(54, IntType());
    BinaryExpr *gteExpr10 =
        new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft10, gteRight10, BooleanType());

    FieldExpr *gteLeft11 = new FieldExpr(0, IntType());
    LiteralExpr *gteRight11 = new LiteralExpr(50, IntType());
    BinaryExpr *gteExpr11 =
        new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft11, gteRight11, BooleanType());

    LiteralExpr *literalResult0 = new LiteralExpr(12, IntType());
    LiteralExpr *literalResult1 = new LiteralExpr(11, IntType());
    LiteralExpr *literalResult2 = new LiteralExpr(10, IntType());
    LiteralExpr *literalResult3 = new LiteralExpr(9, IntType());
    LiteralExpr *literalResult4 = new LiteralExpr(8, IntType());
    LiteralExpr *literalResult5 = new LiteralExpr(7, IntType());
    LiteralExpr *literalResult6 = new LiteralExpr(6, IntType());
    LiteralExpr *literalResult7 = new LiteralExpr(5, IntType());
    LiteralExpr *literalResult8 = new LiteralExpr(4, IntType());
    LiteralExpr *literalResult9 = new LiteralExpr(3, IntType());
    LiteralExpr *literalResult10 = new LiteralExpr(2, IntType());
    LiteralExpr *literalResult11 = new LiteralExpr(1, IntType());
    LiteralExpr *literalResult12 = new LiteralExpr(0, IntType());

    IfExpr *ifExpr12 = new IfExpr(gteExpr11, literalResult11, literalResult12);
    IfExpr *ifExpr11 = new IfExpr(gteExpr10, literalResult10, ifExpr12);
    IfExpr *ifExpr10 = new IfExpr(gteExpr9, literalResult9, ifExpr11);
    IfExpr *ifExpr9 = new IfExpr(gteExpr8, literalResult8, ifExpr10);
    IfExpr *ifExpr8 = new IfExpr(gteExpr7, literalResult7, ifExpr9);
    IfExpr *ifExpr7 = new IfExpr(gteExpr6, literalResult6, ifExpr8);
    IfExpr *ifExpr6 = new IfExpr(gteExpr5, literalResult5, ifExpr7);
    IfExpr *ifExpr5 = new IfExpr(gteExpr4, literalResult4, ifExpr6);
    IfExpr *ifExpr4 = new IfExpr(gteExpr3, literalResult3, ifExpr5);
    IfExpr *ifExpr3 = new IfExpr(gteExpr2, literalResult2, ifExpr4);
    IfExpr *ifExpr2 = new IfExpr(gteExpr1, literalResult1, ifExpr3);
    IfExpr *ifExpr1 = new IfExpr(gteExpr0, literalResult0, ifExpr2);

    std::vector<Expr *> exprs = { ifExpr1 };

    Timer timer;
    timer.SetStart();

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);
    timer.CalculateElapse();
    std::cout << "make row vector: "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);

    timer.CalculateElapse();
    std::cout << "compile expression : "
              << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        VectorBatch *vectorBatch = nullptr;
        auto copy = DuplicateVectorBatch(t);

        timer.Reset();
        op->AddInput(copy);
        op->GetOutput(&vectorBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;

        // verify result
        for (int i = 0; i < numRows; i++) {
            int32_t result = (reinterpret_cast<Vector<int32_t> *>(vectorBatch->Get(0)))->GetValue(i);
            int32_t actualInt;
            auto col1Value = ((reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(i));
            if (col1Value >= 90) {
                actualInt = 12;
            } else if (col1Value >= 85) {
                actualInt = 11;
            } else if (col1Value >= 80) {
                actualInt = 10;
            } else if (col1Value >= 77) {
                actualInt = 9;
            } else if (col1Value >= 74) {
                actualInt = 8;
            } else if (col1Value >= 70) {
                actualInt = 7;
            } else if (col1Value >= 67) {
                actualInt = 6;
            } else if (col1Value >= 64) {
                actualInt = 5;
            } else if (col1Value >= 60) {
                actualInt = 4;
            } else if (col1Value >= 57) {
                actualInt = 3;
            } else if (col1Value >= 54) {
                actualInt = 2;
            } else if (col1Value >= 50) {
                actualInt = 1;
            } else {
                actualInt = 0;
            }
            EXPECT_EQ(result, actualInt);
        }
        VectorHelper::FreeVecBatch(vectorBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
}

Expr *GenerateCastExpr(DataTypePtr input, DataTypePtr output)
{
    std::vector<Expr *> args;
    FieldExpr *col0Expr = new FieldExpr(0, input);
    args.push_back(col0Expr);
    auto cast0 = GetFuncExpr("CAST", args, output);
    return cast0;
}

void PrintRunTime(std::string title, double wallTime, double cpuTime)
{
    std::cout << title << " wall " << wallTime << " cpu " << cpuTime << std::endl;
}

TEST(ExpressionTest, q1CastDoubleLong)
{
    const int32_t numRows = 20000000;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    double *col0 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col0[i] = (rand() % 100000 + 20) / 100.0;
    }

    // prepare expression
    auto cast0 = GenerateCastExpr(DoubleType(), LongType());

    std::vector<Expr *> exprs = { cast0 };
    auto vecOfTypes = std::vector<DataTypePtr>({ DoubleType() });
    DataTypes inputTypes(vecOfTypes);

    Timer timer;

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0);
    timer.CalculateElapse();
    PrintRunTime("make row vector: ", timer.GetWallElapse(), timer.GetCpuElapse());

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    PrintRunTime("compile expression: ", timer.GetWallElapse(), timer.GetCpuElapse());

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        timer.Reset();
        auto copy = DuplicateVectorBatch(t);
        op->AddInput(copy);
        VectorBatch *outputVecBatch = nullptr;
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;
        for (int i = 0; i < numRows; i++) {
            int64_t result = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            int64_t actual = static_cast<int64_t>(col0[i]);
            EXPECT_EQ(result, actual);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col0;
}

TEST(ExpressionTest, q1CastDecimal64Double)
{
    const int32_t numRows = 20000000;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    int64_t *col0 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col0[i] = rand() % 100000 + 20;
    }


    // prepare expression
    auto cast0 = GenerateCastExpr(Decimal64Type(12, 2), DoubleType());

    std::vector<Expr *> exprs = { cast0 };
    auto vecOfTypes = std::vector<DataTypePtr>({ LongType() });
    DataTypes inputTypes(vecOfTypes);

    Timer timer;

    // make row vector
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0);
    timer.CalculateElapse();
    PrintRunTime("make row vector: ", timer.GetWallElapse(), timer.GetCpuElapse());

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    PrintRunTime("compile expression: ", timer.GetWallElapse(), timer.GetCpuElapse());

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        timer.Reset();
        auto copy = DuplicateVectorBatch(t);
        op->AddInput(copy);
        VectorBatch *outputVecBatch = nullptr;
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;
        for (int i = 0; i < numRows; i++) {
            double result = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
            double actual = col0[i] / 100.00;
            EXPECT_TRUE(abs(result - actual) < 0.1);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col0;
}

TEST(ExpressionTest, q1CastDecimal64Int64)
{
    const int32_t numRows = 20000000;
    const int32_t rounds = TEST_EXPR_PERF_TIME;

    // prepare data
    int64_t *col0 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col0[i] = rand() % 100000 + 20;
    }

    // prepare expression
    auto cast0 = GenerateCastExpr(Decimal64Type(12, 2), LongType());

    std::vector<Expr *> exprs = { cast0 };
    auto vecOfTypes = std::vector<DataTypePtr>({ LongType() });
    DataTypes inputTypes(vecOfTypes);

    // make row vector
    Timer timer;
    timer.Reset();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0);
    timer.CalculateElapse();
    PrintRunTime("make row vector: ", timer.GetWallElapse(), timer.GetCpuElapse());

    // prepare projection operator
    timer.Reset();
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    timer.CalculateElapse();
    PrintRunTime("compile expression: ", timer.GetWallElapse(), timer.GetCpuElapse());

    double wallTime[rounds];
    double cpuTime[rounds];
    for (int i = 0; i < rounds; i++) {
        // evaluate
        timer.Reset();
        auto copy = DuplicateVectorBatch(t);
        op->AddInput(copy);
        VectorBatch *outputVecBatch = nullptr;
        op->GetOutput(&outputVecBatch);
        timer.CalculateElapse();
        wallTime[i] = timer.GetWallElapse();
        cpuTime[i] = timer.GetCpuElapse();
        std::cout << "evaluate round: " << i + 1 << " wall " << wallTime[i] << " cpu " << cpuTime[i] << std::endl;
        for (int i = 0; i < numRows; i++) {
            int64_t result = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            EXPECT_EQ(result, col0[i] / 100);
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    PrintValueLine(wallTime, cpuTime, rounds);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col0;
}

TEST(ExpressionTest, q1TestNull)
{
    const int32_t numRows = 10;

    // prepare data
    int64_t *col1 = new int64_t[numRows];
    int64_t *col2 = new int64_t[numRows];
    int64_t *col3 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = rand() % 100000 + 20;
        col2[i] = rand() % 50;
        col3[i] = rand() % 8 + 5;
    }

    // prepare expression
    Expr *expr = PrepareLongExpr();
    std::vector<Expr *> exprs = { expr };
    auto vecOfTypes = std::vector<DataTypePtr>({ LongType(), LongType(), LongType() });
    DataTypes inputTypes(vecOfTypes);

    // make row vector
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    t->Get(0)->SetNull(2);
    t->Get(1)->SetNull(4);
    t->Get(2)->SetNull(6);

    // prepare projection operator
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);


    // evaluate
    VectorBatch *outputVecBatch = nullptr;
    auto copy = DuplicateVectorBatch(t);

    op->AddInput(copy);
    op->GetOutput(&outputVecBatch);

    // verify result
    auto vector = reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0));
    EXPECT_TRUE(vector->HasNull());
    auto retnullptr = unsafe::UnsafeBaseVector::GetNullsHelper(vector);
    for (int i = 0; i < numRows; i++) {
        if (i == 2 || i == 4 || i == 6) {
            EXPECT_TRUE((*retnullptr)[i]);
        } else {
            int64_t result = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            int64_t actualLong = col1[i] * (100L - col2[i]) * (100L + col3[i]);
            EXPECT_EQ(result, actualLong);
            EXPECT_FALSE((*retnullptr)[i]);
        }
    }
    VectorHelper::FreeVecBatch(outputVecBatch);

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    VectorHelper::FreeVecBatch(t);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    delete[] col3;
}

template <typename T, int Size> static void bm_codegen_between()
{
    const string defaultTestFunctionName = "test-function";
    FieldExpr *valueExpr;
    FieldExpr *lowerExpr;
    FieldExpr *upperExpr;

    T col0[] = {1001, 100, 100, 1, 5};
    T col1[] = {1001, 1245, 1245, 3, 4};
    T col2[] = {1001, -1256, 12365, 4, 6};

    std::vector<DataTypePtr> dataTypePtr;
    if (std::is_same<T, int32_t>::value) {
        valueExpr = new FieldExpr(1, IntType());
        lowerExpr = new FieldExpr(0, IntType());
        upperExpr = new FieldExpr(2, IntType());
        dataTypePtr = { IntType(), IntType(), IntType() };
    } else if (std::is_same<T, double>::value) {
        valueExpr = new FieldExpr(1, DoubleType());
        lowerExpr = new FieldExpr(0, DoubleType());
        upperExpr = new FieldExpr(2, DoubleType());
        dataTypePtr = { DoubleType(), DoubleType(), DoubleType() };
    } else {
        valueExpr = new FieldExpr(1, LongType());
        lowerExpr = new FieldExpr(0, LongType());
        upperExpr = new FieldExpr(2, LongType());
        dataTypePtr = { LongType(), LongType(), LongType() };
    }

    BetweenExpr *expr = new BetweenExpr(valueExpr, lowerExpr, upperExpr);
    Arguments<T, T, T> args { Size, 5, col0, col1, col2 };
    DataTypes inputTypes(dataTypePtr);
    VectorBatch *t = CreateVectorBatch(inputTypes, 5, col0, col1, col2);

    auto overflowConfig = new OverflowConfig();
    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, overflowConfig);

    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    Timer timer;
    timer.SetStart();
    int32_t result = func(args.vals, args.rowCount, args.selected, (int64_t *)args.bitmap, (int64_t *)args.offsets,
        args.context, args.dictionaries);
    timer.CalculateElapse();
    std::cerr << result << std::endl;
    if (std::is_same<T, int32_t>::value) {
        std::cout << "Run int32_t, Size =" << Size << ":"
                  << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;
    } else if (std::is_same<T, double>::value) {
        std::cout << "Run double, Size =" << Size << ":"
                  << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;
    } else {
        std::cout << "Run int64_t, Size =" << Size << ":"
                  << " wall " << timer.GetWallElapse() << " cpu " << timer.GetCpuElapse() << std::endl;
    }

    DeleteArgument(args);
    delete expr;
    delete overflowConfig;
    VectorHelper::FreeVecBatch(t);
}

// Following code is from Ken for between test
TEST(ExpressionTest, bm_codegen_between)
{
    bm_codegen_between<int32_t, 1000000>();
    bm_codegen_between<int32_t, 4000000>();
    bm_codegen_between<int32_t, 16000000>();

    bm_codegen_between<double, 1000000>();
    bm_codegen_between<double, 4000000>();
    bm_codegen_between<double, 16000000>();

    bm_codegen_between<long, 1000000>();
    bm_codegen_between<long, 4000000>();
    bm_codegen_between<long, 16000000>();
}

TEST(ExpressionTest, bm_codegen_between2)
{
    const string defaultTestFunctionName = "test-function";
    FieldExpr *valueExpr = new FieldExpr(1, IntType());
    FieldExpr *lowerExpr = new FieldExpr(0, IntType());
    FieldExpr *upperExpr = new FieldExpr(2, IntType());
    BetweenExpr *expr = new BetweenExpr(valueExpr, lowerExpr, upperExpr);

    int col0[] = {1001, 100, 100, 1, 5};
    int col1[] = {1001, 1245, 1245, 3, 4};
    int col2[] = {1001, -1256, 12365, 4, 6};

    Arguments<int, int, int> args { 1000, 5, col0, col1, col2 };
    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), IntType(), IntType() }));
    VectorBatch *t = CreateVectorBatch(inputTypes, 5, col0, col1, col2);

    // a > l1 && a < l2
    auto overflowConfig = new OverflowConfig();
    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, overflowConfig);

    Timer timer;
    timer.SetStart();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);
    timer.CalculateElapse();
    std::cout << "the walltime of function generate is " << timer.GetWallElapse() << std::endl;
    std::cout << "the cputime of function generate is " << timer.GetCpuElapse() << std::endl;

    int32_t result2 = func(args.vals, args.rowCount, args.selected, (int64_t *)args.bitmap, (int64_t *)args.offsets,
        args.context, args.dictionaries);
    std::cout << " result2: " << result2 << std::endl;

    delete expr;
    DeleteArgument(args);
    delete overflowConfig;
    VectorHelper::FreeVecBatch(t);
}

TEST(ExpressionTest, bm_codegen_filter)
{
    const string defaultTestFunctionName = "test-function";
    FieldExpr *col0Expr = new FieldExpr(0, IntType());
    FieldExpr *col1Expr = new FieldExpr(1, LongType());
    FieldExpr *col2Expr = new FieldExpr(2, DoubleType());

    // create the filter expression object
    FieldExpr *eq2Left = new FieldExpr(1, LongType());
    LiteralExpr *eq2Right = new LiteralExpr(3000000000L, LongType());
    BinaryExpr *eq2Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eq2Left, eq2Right, BooleanType());

    FieldExpr *gteLeft = new FieldExpr(2, DoubleType());
    LiteralExpr *gteRight = new LiteralExpr(0.4, DoubleType());
    BinaryExpr *gteExpr = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft, gteRight, BooleanType());

    BinaryExpr *innerAndExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq2Expr, gteExpr, BooleanType());

    FieldExpr *eq1Left = new FieldExpr(0, IntType());
    LiteralExpr *eq1Right = new LiteralExpr(0, IntType());
    BinaryExpr *eq1Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eq1Left, eq1Right, BooleanType());

    Expr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq1Expr, innerAndExpr, BooleanType());
    DataTypes inputTypes(std::vector<DataTypePtr>({ IntType(), LongType(), DoubleType() }));

    // a > l1 && a < l2
    auto overflowConfig = new OverflowConfig();
    auto codegen = FilterCodeGen(defaultTestFunctionName, *filterExpr, overflowConfig);

    Timer timer;
    timer.SetStart();
    codegen.GetFunction(inputTypes);
    timer.CalculateElapse();
    std::cout << "the walltime of function generate is " << timer.GetWallElapse() << std::endl;
    std::cout << "the cputime of function generate is " << timer.GetCpuElapse() << std::endl;

    delete filterExpr;
    delete col0Expr;
    delete col1Expr;
    delete col2Expr;
    delete overflowConfig;
}
}
