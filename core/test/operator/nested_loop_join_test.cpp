/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * @Description: sort operator test implementations
 */

#include "gtest/gtest.h"
#include "operator/join/hash_builder_expr.h"
#include "operator/join/lookup_join_expr.h"
#include "operator/join/lookup_outer_join_expr.h"
#include "operator/join/nest_loop_join_builder.h"
#include "operator/join/nest_loop_join_lookup.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "expression/jsonparser/jsonparser.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace NestedLoopJoinTest {
const int32_t DISTINCT_VALUE_COUNT = 4;
const int32_t REPEAT_COUNT = 200;
const int32_t COLUMN_COUNT_4 = 4;
const int32_t VEC_BATCH_COUNT_10 = 10;
const int32_t VEC_BATCH_COUNT_1 = 1;
const int32_t BUILD_POSITION_COUNT = 1000;
const int32_t PROBE_POSITION_COUNT = 1000;
const int32_t TIME_TO_SLEEP = 100;

void DeleteNestedLoopJoinOperatorFactory(NestedLoopJoinBuildOperatorFactory *nestedLoopJoinBuildOperatorFactory,
    NestLoopJoinLookupOperatorFactory *nestLoopJoinLookupOperatorFactory)
{
    delete nestedLoopJoinBuildOperatorFactory;
    delete nestLoopJoinLookupOperatorFactory;
}

VectorBatch *CreateBuildInputForAllTypes(DataTypes &buildTypes, void **buildDatas, int32_t dataSize, bool isDictionary)
{
    int32_t buildTypesSize = buildTypes.GetSize();
    auto *buildTypeIds = const_cast<int32_t *>(buildTypes.GetIds());
    BaseVector *buildVectors[buildTypesSize];
    for (int32_t i = 0; i < buildTypesSize; i++) {
        buildVectors[i] = VectorHelper::CreateVector(OMNI_FLAT, buildTypeIds[i], dataSize);
        SetValue(buildVectors[i], 0, buildDatas[i]);
    }
    for (int32_t i = 1; i < dataSize; i++) {
        for (int32_t j = 0; j < buildTypesSize; j++) {
            if (i == j + 1) {
                buildVectors[j]->SetNull(i);
            } else {
                SetValue(buildVectors[j], i, buildDatas[j]);
            }
        }
    }
    if (isDictionary) {
        int32_t ids[dataSize];
        for (int32_t i = 0; i < dataSize; i++) {
            ids[i] = i;
        }
        for (int32_t i = 0; i < buildTypesSize; i++) {
            auto buildVector = buildVectors[i];
            buildVectors[i] = DYNAMIC_TYPE_DISPATCH(CreateDictionary, buildTypeIds[i], buildVector, ids, dataSize);
            delete buildVector;
        }
    }
    auto buildVecBatch = new VectorBatch(dataSize);
    for (int32_t i = 0; i < buildTypesSize; i++) {
        buildVecBatch->Append(buildVectors[i]);
    }
    return buildVecBatch;
}

VectorBatch *CreateProbeInputForAllTypes(DataTypes &probeTypes, void **probeDatas, int32_t dataSize, bool isDictionary)
{
    int32_t probeTypesSize = probeTypes.GetSize();
    auto *probeTypeIds = const_cast<int32_t *>(probeTypes.GetIds());
    BaseVector *probeVectors[probeTypesSize];
    for (int32_t i = 0; i < probeTypesSize; i++) {
        probeVectors[i] = VectorHelper::CreateVector(OMNI_FLAT, probeTypeIds[i], dataSize);
    }
    for (int32_t i = 0; i < dataSize - 1; i++) {
        for (int32_t j = 0; j < probeTypesSize; j++) {
            if (i == j) {
                probeVectors[j]->SetNull(i);
            } else {
                SetValue(probeVectors[j], i, probeDatas[j]);
            }
        }
    }
    for (int32_t j = 0; j < probeTypesSize; j++) {
        SetValue(probeVectors[j], probeTypesSize, probeDatas[j]);
    }
    if (isDictionary) {
        int32_t ids[dataSize];
        for (int32_t i = 0; i < dataSize; i++) {
            ids[i] = i;
        }
        for (int32_t i = 0; i < probeTypesSize; i++) {
            auto probeVector = probeVectors[i];
            probeVectors[i] = DYNAMIC_TYPE_DISPATCH(CreateDictionary, probeTypeIds[i], probeVector, ids, dataSize);
            delete probeVector;
        }
    }
    auto probeVecBatch = new VectorBatch(dataSize);
    for (int32_t j = 0; j < probeTypesSize; j++) {
        probeVecBatch->Append(probeVectors[j]);
    }
    return probeVecBatch;
}

VectorBatch *CreateExpectVecBatchForAllTypesWithNoFilter(VectorBatch *probeVecBatch, VectorBatch *buildVecBatch,
    int32_t *probeRows, int32_t *buildRows, int32_t rowSize)
{
    int32_t probeVecCount = probeVecBatch->GetVectorCount();
    int32_t buildVecCount = buildVecBatch->GetVectorCount();
    int32_t probeVecRowCount = probeVecBatch->GetRowCount();
    int32_t buildVecRowCount = buildVecBatch->GetRowCount();
    auto expectVecBatch = new VectorBatch(rowSize);
    for (int32_t i = 0; i < probeVecCount; i++) {
        expectVecBatch->Append(VectorHelper::CopyPositionsVector(probeVecBatch->Get(i), probeRows, 0, rowSize));
    }

    for (int32_t i = 0; i < buildVecCount; i++) {
        expectVecBatch->Append(VectorHelper::CopyPositionsVector(buildVecBatch->Get(i), buildRows, 0, rowSize));
    }
    return expectVecBatch;
}

VectorBatch *CreateExpectVecBatchForAllTypesWithLeftJoin(VectorBatch *probeVecBatch, VectorBatch *buildVecBatch,
    int32_t *probeRows, int32_t *buildRows, int32_t rowSize, int32_t *nullPosition, int32_t nullPositionSize)
{
    int32_t probeVecCount = probeVecBatch->GetVectorCount();
    int32_t buildVecCount = buildVecBatch->GetVectorCount();
    int32_t probeVecRowCount = probeVecBatch->GetRowCount();
    int32_t buildVecRowCount = buildVecBatch->GetRowCount();
    auto expectVecBatch = new VectorBatch(rowSize);
    for (int32_t i = 0; i < probeVecCount; i++) {
        expectVecBatch->Append(VectorHelper::CopyPositionsVector(probeVecBatch->Get(i), probeRows, 0, rowSize));
    }
    if (buildVecRowCount == 0) {
        for (int32_t j = 0; j < buildVecCount; j++) {
            BaseVector *vector = buildVecBatch->Get(j);
            DataTypeId vectorDateTypeId = vector->GetTypeId();
            BaseVector *outputVector = VectorHelper::CreateVector(OMNI_FLAT, vectorDateTypeId, rowSize);
            for (int32_t i = 0; i < nullPositionSize; ++i) {
                outputVector->SetNull(nullPosition[i]);
            }
            expectVecBatch->Append(outputVector);
        }
    } else {
        for (int32_t i = 0; i < buildVecCount; i++) {
            BaseVector *buildVector = VectorHelper::CopyPositionsVector(buildVecBatch->Get(i), buildRows, 0, rowSize);
            for (int32_t j = 0; j < nullPositionSize; ++j) {
                buildVector->SetNull(nullPosition[j]);
            }
            expectVecBatch->Append(buildVector);
        }
    }

    return expectVecBatch;
}

VectorBatch *CreateExpectVecBatchForAllTypesWithRightJoin(VectorBatch *probeVecBatch, VectorBatch *buildVecBatch,
    int32_t probeOutputColsSize, int32_t buildOutputColsSize, int32_t *buildNotMatchRows, int32_t rowSize)
{
    auto expectVecBatch = new VectorBatch(rowSize);
    // probe side set null
    for (int32_t j = 0; j < probeOutputColsSize; j++) {
        BaseVector *vector = probeVecBatch->Get(j);
        DataTypeId vectorDateTypeId = vector->GetTypeId();
        BaseVector *outputVector = VectorHelper::CreateVector(OMNI_FLAT, vectorDateTypeId, rowSize);
        for (int32_t i = 0; i < rowSize; ++i) {
            outputVector->SetNull(i);
        }
        expectVecBatch->Append(outputVector);
    }

    // build side copy origin rows
    for (int32_t j = 0; j < buildOutputColsSize; j++) {
        BaseVector *outputVector = VectorHelper::CopyPositionsVector(buildVecBatch->Get(j), buildNotMatchRows, 0, rowSize);
        expectVecBatch->Append(outputVector);
    }

    return expectVecBatch;
}

omniruntime::expressions::Expr *CreateJoinFilterExprWithChar()
{
    // create the filter expression
    std::string funcStr = "substr";
    DataTypePtr retType1 = VarcharType();

    auto leftSubstrColumn = new FieldExpr(7, retType1);
    auto leftSubstrIndex = new LiteralExpr(0, IntType());
    auto leftSubstrLen = new LiteralExpr(1, IntType());
    std::vector<Expr *> leftSubstrArgs;
    leftSubstrArgs.push_back(leftSubstrColumn);
    leftSubstrArgs.push_back(leftSubstrIndex);
    leftSubstrArgs.push_back(leftSubstrLen);
    auto leftSubstrExpr = GetFuncExpr(funcStr, leftSubstrArgs, retType1);

    auto rightSubstrColumn = new FieldExpr(8, retType1);
    auto rightSubstrIndex = new LiteralExpr(0, IntType());
    auto rightSubstrLen = new LiteralExpr(1, IntType());
    std::vector<Expr *> rightSubstrArgs;
    rightSubstrArgs.push_back(rightSubstrColumn);
    rightSubstrArgs.push_back(rightSubstrIndex);
    rightSubstrArgs.push_back(rightSubstrLen);
    auto rightSubstrExpr = GetFuncExpr(funcStr, rightSubstrArgs, retType1);

    auto *notEqualExpr =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, leftSubstrExpr, rightSubstrExpr, BooleanType());
    return notEqualExpr;
}

omniruntime::expressions::Expr *CreateJoinFilterExprWithInt()
{
    // create the filter expression
    std::string funcStr = "abs";
    DataTypePtr retType1 = IntType();

    auto leftSubstrColumn = new FieldExpr(0, retType1);
    std::vector<Expr *> leftSubstrArgs;
    leftSubstrArgs.push_back(leftSubstrColumn);
    auto leftSubstrExpr = GetFuncExpr(funcStr, leftSubstrArgs, retType1);

    auto rightSubstrColumn = new FieldExpr(2, retType1);
    std::vector<Expr *> rightSubstrArgs;
    rightSubstrArgs.push_back(rightSubstrColumn);
    auto rightSubstrExpr = GetFuncExpr(funcStr, rightSubstrArgs, retType1);

    auto *notEqualExpr =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, leftSubstrExpr, rightSubstrExpr, BooleanType());
    return notEqualExpr;
}

VectorBatch *CreateVectorBatchWithDict(const DataTypes &types, int32_t rowCount, ...)
{
    int32_t typesCount = types.GetSize();
    BaseVector *baseVectors[rowCount];
    auto *vectorBatch = new VectorBatch(rowCount);
    auto *typeIds = const_cast<int32_t *>(types.GetIds());
    va_list args;
    va_start(args, typesCount);
    for (int32_t i = 0; i < typesCount; i++) {
        auto &type = types.GetType(i);
        baseVectors[i] = CreateVector(*type, rowCount, args);
    }
    va_end(args);
    int32_t ids[rowCount];
    for (int32_t i = 0; i < rowCount; i++) {
        ids[i] = i;
    }
    for (int32_t i = 0; i < typesCount; i++) {
        auto baseVector = baseVectors[i];
        vectorBatch->Append(DYNAMIC_TYPE_DISPATCH(CreateDictionary, typeIds[i], baseVector, ids, rowCount));
        delete baseVector;
    }
    return vectorBatch;
}


TEST(NestedLoopJoinTest, TestComparePerf) {}

TEST(NestedLoopJoinTest, TestCrossNoEqualityJoinOnChar)
{
    int32_t intValue = 20;
    int64_t longValue = 40;
    bool boolValue = true;
    double doubleValue = 30.0;
    Decimal128 decimal128(20, 0);
    std::string stringValue("string");
    int64_t shortValue = 14;
    const int32_t dataSize = 11;
    const int32_t builddataSize = 5;
    const int32_t probeDatasize = 7;
    void *joinDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue, &decimal128,
                                 &stringValue, &stringValue, &shortValue};
    void *probeDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue};
    void *buildDatas[dataSize] = {&decimal128, &stringValue, &stringValue, &shortValue};
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
        Decimal64Type(2, 0), Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>(
        { IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY), Decimal64Type(2, 0) }));
    DataTypes buildDataTypes(
        std::vector<DataTypePtr>({ Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[4] = {0, 1, 2, 3};
    int32_t probeColumns[6] = {0, 1, 2, 3, 4, 5};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    auto buildVecBatch = CreateBuildInputForAllTypes(buildDataTypes, buildDatas, builddataSize, false);
    auto probeVecBatch = CreateProbeInputForAllTypes(probeDataTypes, probeDatas, probeDatasize, false);
    int32_t probeRows[35] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
                             6, 6, 6, 6, 6};
    int32_t buildRows[35] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4,
                             0, 1, 2, 3, 4};
    auto expectVecBatch =
        CreateExpectVecBatchForAllTypesWithNoFilter(probeVecBatch, buildVecBatch, probeRows, buildRows, 35);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, nullptr, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestCrossNoEqualityJoinOnCharWithDictionary)
{
    int32_t intValue = 20;
    int64_t longValue = 40;
    bool boolValue = true;
    double doubleValue = 30.0;
    Decimal128 decimal128(20, 0);
    std::string stringValue("string");
    int64_t shortValue = 14;
    const int32_t dataSize = 11;
    const int32_t builddataSize = 5;
    const int32_t probeDatasize = 7;
    void *joinDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue, &decimal128,
                                 &stringValue, &stringValue, &shortValue};
    void *probeDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue};
    void *buildDatas[dataSize] = {&decimal128, &stringValue, &stringValue, &shortValue};
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
        Decimal64Type(2, 0), Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>(
        { IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY), Decimal64Type(2, 0) }));
    DataTypes buildDataTypes(
        std::vector<DataTypePtr>({ Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[4] = {0, 1, 2, 3};
    int32_t probeColumns[6] = {0, 1, 2, 3, 4, 5};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    auto buildVecBatch = CreateBuildInputForAllTypes(buildDataTypes, buildDatas, builddataSize, true);
    auto probeVecBatch = CreateProbeInputForAllTypes(probeDataTypes, probeDatas, probeDatasize, true);
    int32_t probeRows[35] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
                             6, 6, 6, 6, 6};
    int32_t buildRows[35] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4,
                             0, 1, 2, 3, 4};
    auto expectVecBatch =
        CreateExpectVecBatchForAllTypesWithNoFilter(probeVecBatch, buildVecBatch, probeRows, buildRows, 35);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, nullptr, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestInnerNoEqualityJoinOnChar)
{
    int32_t intValue = 20;
    int64_t longValue = 40;
    bool boolValue = true;
    double doubleValue = 30.0;
    Decimal128 decimal128(20, 0);
    std::string stringValue("string");
    int64_t shortValue = 14;
    const int32_t dataSize = 11;
    const int32_t builddataSize = 5;
    const int32_t probeDatasize = 7;
    void *joinDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue, &decimal128,
                                 &stringValue, &stringValue, &shortValue};
    void *probeDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue};
    void *buildDatas[dataSize] = {&decimal128, &stringValue, &stringValue, &shortValue};
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
        Decimal64Type(2, 0), Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>(
        { IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY), Decimal64Type(2, 0) }));
    DataTypes buildDataTypes(
        std::vector<DataTypePtr>({ Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[4] = {0, 1, 2, 3};
    int32_t probeColumns[6] = {0, 1, 2, 3, 4, 5};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto buildVecBatch = CreateBuildInputForAllTypes(buildDataTypes, buildDatas, builddataSize, false);
    auto probeVecBatch = CreateProbeInputForAllTypes(probeDataTypes, probeDatas, probeDatasize, false);
    int32_t probeRows[21] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6};
    int32_t buildRows[21] = {0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4};
    auto expectVecBatch =
        CreateExpectVecBatchForAllTypesWithNoFilter(probeVecBatch, buildVecBatch, probeRows, buildRows, 21);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestInnerNoEqualityJoinOnCharWithDictionary)
{
    int32_t intValue = 20;
    int64_t longValue = 40;
    bool boolValue = true;
    double doubleValue = 30.0;
    Decimal128 decimal128(20, 0);
    std::string stringValue("string");
    int64_t shortValue = 14;
    const int32_t dataSize = 11;
    const int32_t builddataSize = 5;
    const int32_t probeDatasize = 7;
    void *joinDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue, &decimal128,
                                 &stringValue, &stringValue, &shortValue};
    void *probeDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue};
    void *buildDatas[dataSize] = {&decimal128, &stringValue, &stringValue, &shortValue};
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
        Decimal64Type(2, 0), Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>(
        { IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY), Decimal64Type(2, 0) }));
    DataTypes buildDataTypes(
        std::vector<DataTypePtr>({ Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[4] = {0, 1, 2, 3};
    int32_t probeColumns[6] = {0, 1, 2, 3, 4, 5};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto buildVecBatch = CreateBuildInputForAllTypes(buildDataTypes, buildDatas, builddataSize, true);
    auto probeVecBatch = CreateProbeInputForAllTypes(probeDataTypes, probeDatas, probeDatasize, true);
    int32_t probeRows[21] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6};
    int32_t buildRows[21] = {0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4, 0, 1, 4};
    auto expectVecBatch =
        CreateExpectVecBatchForAllTypesWithNoFilter(probeVecBatch, buildVecBatch, probeRows, buildRows, 21);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestLeftNoEqualityJoinOnChar)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t buildDataSize = 4;
    int32_t buildData0[buildDataSize] = {20, 16, 13, 5};
    std::string buildData1[buildDataSize] = {"1000", "2000", "3000", "4000"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5), IntType(), VarcharType(5) }));

    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    int32_t probeRows[5] = {0, 1, 2, 3, 4};
    int32_t buildRows[5] = {0, 1, 2, 3, 1};
    int32_t nullPosition[2] = {3, 4};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 5, nullPosition, 2);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestLeftNoEqualityJoinOnCharWithDictionary)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t buildDataSize = 4;
    int32_t buildData0[buildDataSize] = {20, 16, 13, 5};
    std::string buildData1[buildDataSize] = {"1000", "2000", "3000", "4000"};
    auto buildVecBatch = CreateVectorBatchWithDict(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatchWithDict(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10), IntType(), VarcharType(10) }));


    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    int32_t probeRows[5] = {0, 1, 2, 3, 4};
    int32_t buildRows[5] = {0, 1, 2, 3, 1};
    int32_t nullPosition[2] = {3, 4};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 5, nullPosition, 2);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestRightNoEqualityJoinOnChar)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t buildDataSize = 4;
    int32_t buildData0[buildDataSize] = {20, 16, 13, 5};
    std::string buildData1[buildDataSize] = {"1000", "2000", "3000", "4000"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5), IntType(), VarcharType(5) }));

    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    int32_t probeRows[5] = {0, 1, 2, 3, 4};
    int32_t buildRows[5] = {0, 1, 2, 3, 1};
    int32_t nullPosition[2] = {3, 4};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 5, nullPosition, 2);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);
    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestRightNoEqualityJoinOnCharWithDictionary)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t buildDataSize = 4;
    int32_t buildData0[buildDataSize] = {20, 16, 13, 5};
    std::string buildData1[buildDataSize] = {"1000", "2000", "3000", "4000"};
    auto buildVecBatch = CreateVectorBatchWithDict(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatchWithDict(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10), IntType(), VarcharType(10) }));


    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    int32_t probeRows[5] = {0, 1, 2, 3, 4};
    int32_t buildRows[5] = {0, 1, 2, 3, 1};
    int32_t nullPosition[2] = {3, 4};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 5, nullPosition, 2);

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    nestedLoopJoinBuildOperator->AddInput(buildVecBatch);
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);

    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestOperatorWithoutInput)
{
    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));

    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};

    int32_t probeRows[5] = {0, 1, 2, 3, 4};
    int32_t buildRows[5] = {0, 1, 2, 3, 1};

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();

    // create the operator
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;

    // Simulate no output
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);

    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));

    Expr::DeleteExprs({ joinFilter });
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestLeftJoinOperatorWithoutBuildInput)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t buildDataSize = 0;
    int32_t buildData0[buildDataSize] = {};
    std::string buildData1[buildDataSize] = {};
    auto buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5), IntType(), VarcharType(5) }));

    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);

    int32_t probeRows[5] = {0, 1, 2, 3, 4};
    int32_t buildRows[5] = {0, 0, 0, 0, 0};
    int32_t nullPosition[5] = {0, 1, 2, 3, 4};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 5, nullPosition, 5);


    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(buildVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestCrossJoinOperatorWithoutBuildInput)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t buildDataSize = 0;
    int32_t buildData0[buildDataSize] = {};
    std::string buildData1[buildDataSize] = {};
    auto buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5), IntType(), VarcharType(5) }));

    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = nullptr;
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);

    int32_t probeRows[0] = {};
    int32_t buildRows[0] = {};
    int32_t nullPosition[0] = {};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 0, nullPosition, 0);

    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_LEFT;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(buildVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

TEST(NestedLoopJoinTest, TestInnerJoinOperatorWithoutBuildInput)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(10) }));
    const int32_t buildDataSize = 0;
    int32_t buildData0[buildDataSize] = {};
    std::string buildData1[buildDataSize] = {};
    auto buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5), IntType(), VarcharType(5) }));

    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }

    auto nestedLoopJoinBuildOperatorFactory =
        new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, buildDataTypes.GetSize());
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithInt();
    auto nestedLoopJoinBuildOperator =
        static_cast<NestedLoopJoinBuildOperator *>(nestedLoopJoinBuildOperatorFactory->CreateOperator(nullptr));
    VectorBatch *nestedLoopBuildOutput = nullptr;
    nestedLoopJoinBuildOperator->GetOutput(&nestedLoopBuildOutput);

    int32_t probeRows[0] = {};
    int32_t buildRows[0] = {};
    int32_t nullPosition[0] = {};
    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(probeVecBatch, buildVecBatch, probeRows,
        buildRows, 0, nullPosition, 0);

    auto nestedLoopJoinBuildOperatorFactoryAddr = (int64_t)nestedLoopJoinBuildOperatorFactory;
    int32_t probeOutputColsCount = probeDataTypes.GetSize();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto nestLoopJoinLookupOperatorFactory =
        NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(joinTypePtr, probeDataTypes,
        probeColumns, probeOutputColsCount, joinFilter, nestedLoopJoinBuildOperatorFactoryAddr, nullptr, config::QueryConfig());
    auto nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator(nullptr));
    nestLoopJoinLookupOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    nestLoopJoinLookupOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(buildVecBatch);
    omniruntime::op::Operator::DeleteOperator(nestedLoopJoinBuildOperator);
    omniruntime::op::Operator::DeleteOperator(nestLoopJoinLookupOperator);
    DeleteNestedLoopJoinOperatorFactory(nestedLoopJoinBuildOperatorFactory, nestLoopJoinLookupOperatorFactory);
}

struct JoinTestSetup {
    VectorBatch *buildVecBatch;
    VectorBatch *probeVecBatch;
    DataTypes buildDataTypes;
    DataTypes probeDataTypes;
    std::vector<int32_t> buildColumns;
    std::vector<int32_t> probeColumns;
    std::vector<int32_t> probeRows;
    std::vector<int32_t> buildRows;
    std::vector<int32_t> buildNotMatchRows;
    std::vector<int32_t> nullPosition;
};

static JoinTestSetup CreateJoinTestData()
{
    // Build side
    DataTypes buildTypes({IntType(), VarcharType(5)});
    const int32_t buildDataSize = 4;
    int32_t buildData0[buildDataSize] = {20, 16, 13, 5};
    std::string buildData1[buildDataSize] = {"1000", "2000", "3000", "4000"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    // Probe side
    DataTypes probeTypes({IntType(), VarcharType(5)});
    const int32_t probeDataSize = 5;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 22};
    std::string probeData1[probeDataSize] = {"35709", "65709", "31904", "12477", "90419"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    return {
        buildVecBatch,
        probeVecBatch,
        buildTypes,
        probeTypes,
        {0, 1},
        {0, 1},
        {0, 1, 2, 3, 4},
        {0, 1, 2, 3, 1},
        {3},
        {3, 4}
    };
}

TEST(NestedLoopJoinTest, TestRightBuildRightNoEqualityJoinOnChar)
{
    auto data = CreateJoinTestData();

    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithNoFilter(
        data.probeVecBatch, data.buildVecBatch,
        data.probeRows.data(), data.buildRows.data(), 3);

    auto expectVecBatch2 = CreateExpectVecBatchForAllTypesWithRightJoin(
        data.probeVecBatch, data.buildVecBatch, 2, 2,
        data.buildNotMatchRows.data(), 1);

    auto factory = new NestedLoopJoinBuildOperatorFactory(data.buildDataTypes, data.buildColumns.data(), 2);
    auto buildOp = static_cast<NestedLoopJoinBuildOperator *>(factory->CreateOperator(nullptr));
    buildOp->AddInput(data.buildVecBatch);
    VectorBatch *buildOutput = nullptr;
    buildOp->GetOutput(&buildOutput);

    int64_t factoryAddr = (int64_t) factory;
    auto filter = CreateJoinFilterExprWithInt();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_RIGHT;
    auto lookupFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
        joinTypePtr, data.probeDataTypes, data.probeColumns.data(), 2, filter, factoryAddr, nullptr, config::QueryConfig());
    auto lookupOp = dynamic_cast<NestLoopJoinLookupOperator *>(lookupFactory->CreateOperator(nullptr));
    lookupOp->AddInput(data.probeVecBatch);

    VectorBatch *output1 = nullptr;
    lookupOp->GetOutput(&output1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(output1, expectVecBatch));
    VectorHelper::FreeVecBatch(output1);
    VectorHelper::FreeVecBatch(expectVecBatch);

    VectorBatch *output2 = nullptr;
    lookupOp->GetOutput(&output2);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(output2, expectVecBatch2));
    VectorHelper::FreeVecBatch(output2);
    VectorHelper::FreeVecBatch(expectVecBatch2);

    Expr::DeleteExprs({filter});
    omniruntime::op::Operator::DeleteOperator(buildOp);
    omniruntime::op::Operator::DeleteOperator(lookupOp);
    DeleteNestedLoopJoinOperatorFactory(factory, lookupFactory);
}

TEST(NestedLoopJoinTest, TestRightBuildRightWithoutProbeInputNoEqualityJoinOnChar)
{
    auto data = CreateJoinTestData();
    DataTypes probeTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    const int32_t probeDataSize = 0;
    int32_t probeData0[probeDataSize] = {};
    std::string probeData1[probeDataSize] = {};
    auto emptyProbeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);
    std::vector<int32_t> buildNotMatchRows = {0, 1, 2, 3};

    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithRightJoin(
        data.probeVecBatch, data.buildVecBatch, 2, 2,
        buildNotMatchRows.data(), 4);

    auto factory = new NestedLoopJoinBuildOperatorFactory(data.buildDataTypes, data.buildColumns.data(), 2);
    auto buildOp = static_cast<NestedLoopJoinBuildOperator *>(factory->CreateOperator(nullptr));
    buildOp->AddInput(data.buildVecBatch);
    VectorBatch *buildOutput = nullptr;
    buildOp->GetOutput(&buildOutput);

    int64_t factoryAddr = (int64_t) factory;
    auto filter = CreateJoinFilterExprWithInt();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_RIGHT;
    auto lookupFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
        joinTypePtr, data.probeDataTypes, data.probeColumns.data(), 2, filter, factoryAddr, nullptr, config::QueryConfig());
    auto lookupOp = dynamic_cast<NestLoopJoinLookupOperator *>(lookupFactory->CreateOperator(nullptr));
    lookupOp->AddInput(emptyProbeVecBatch);

    VectorBatch *output = nullptr;
    lookupOp->GetOutput(&output);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(output, expectVecBatch));
    VectorHelper::FreeVecBatch(output);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(data.probeVecBatch);

    Expr::DeleteExprs({filter});
    omniruntime::op::Operator::DeleteOperator(buildOp);
    omniruntime::op::Operator::DeleteOperator(lookupOp);
    DeleteNestedLoopJoinOperatorFactory(factory, lookupFactory);
}

TEST(NestedLoopJoinTest, TestFullBuildRightNoEqualityJoinOnChar)
{
    auto data = CreateJoinTestData();

    auto expectVecBatch = CreateExpectVecBatchForAllTypesWithLeftJoin(
        data.probeVecBatch, data.buildVecBatch,
        data.probeRows.data(), data.buildRows.data(), 5, data.nullPosition.data(), 2);

    auto expectVecBatch2 = CreateExpectVecBatchForAllTypesWithRightJoin(
        data.probeVecBatch, data.buildVecBatch, 2, 2,
        data.buildNotMatchRows.data(), 1);

    auto factory = new NestedLoopJoinBuildOperatorFactory(data.buildDataTypes, data.buildColumns.data(), 2);
    auto buildOp = static_cast<NestedLoopJoinBuildOperator *>(factory->CreateOperator(nullptr));
    buildOp->AddInput(data.buildVecBatch);
    VectorBatch *buildOutput = nullptr;
    buildOp->GetOutput(&buildOutput);

    int64_t factoryAddr = (int64_t) factory;
    auto filter = CreateJoinFilterExprWithInt();
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_FULL;
    auto lookupFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
        joinTypePtr, data.probeDataTypes, data.probeColumns.data(), 2, filter, factoryAddr, nullptr, config::QueryConfig());
    auto lookupOp = dynamic_cast<NestLoopJoinLookupOperator *>(lookupFactory->CreateOperator(nullptr));
    lookupOp->AddInput(data.probeVecBatch);

    VectorBatch *output1 = nullptr;
    lookupOp->GetOutput(&output1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(output1, expectVecBatch));
    VectorHelper::FreeVecBatch(output1);
    VectorHelper::FreeVecBatch(expectVecBatch);

    VectorBatch *output2 = nullptr;
    lookupOp->GetOutput(&output2);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(output2, expectVecBatch2));
    VectorHelper::FreeVecBatch(output2);
    VectorHelper::FreeVecBatch(expectVecBatch2);

    Expr::DeleteExprs({filter});
    omniruntime::op::Operator::DeleteOperator(buildOp);
    omniruntime::op::Operator::DeleteOperator(lookupOp);
    DeleteNestedLoopJoinOperatorFactory(factory, lookupFactory);
}

// NestLoopJoin with ConstVector in build side
TEST(NestedLoopJoinTest, TestCrossJoinWithBuildConstVector)
{
    // Build side: col0 (ConstVector int, all 20), col1 (regular varchar)
    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t buildDataSize = 3;
    auto *buildCol0 = new ConstVector<int32_t>(20, OMNI_INT, buildDataSize);
    auto *buildCol1 = new Vector<LargeStringContainer<std::string_view>>(buildDataSize);
    std::string buildStrings[] = {"aaa", "bbb", "ccc"};
    for (int32_t i = 0; i < buildDataSize; i++) {
        buildCol1->SetValue(i, std::string_view(buildStrings[i]));
    }
    auto *buildVecBatch = new VectorBatch(buildDataSize);
    buildVecBatch->Append(buildCol0);
    buildVecBatch->Append(buildCol1);

    // Probe side: regular vectors
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 2;
    int32_t probeData0[probeDataSize] = {10, 30};
    std::string probeData1[probeDataSize] = {"xxx", "yyy"};
    auto probeVecBatch = CreateVectorBatch(probeDataTypes, probeDataSize, probeData0, probeData1);

    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};

    auto factory = new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, 2);
    auto buildOp = static_cast<NestedLoopJoinBuildOperator *>(factory->CreateOperator(nullptr));
    buildOp->AddInput(buildVecBatch);
    VectorBatch *buildOutput = nullptr;
    buildOp->GetOutput(&buildOutput);

    int64_t factoryAddr = (int64_t)factory;
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto lookupFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
        joinTypePtr, probeDataTypes, probeColumns, 2, nullptr, factoryAddr, nullptr, config::QueryConfig());
    auto lookupOp = dynamic_cast<NestLoopJoinLookupOperator *>(lookupFactory->CreateOperator(nullptr));
    lookupOp->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupOp->GetOutput(&outputVecBatch);

    // Cross join: 2 probe rows * 3 build rows = 6 result rows
    EXPECT_EQ(outputVecBatch->GetRowCount(), 6);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(buildOp);
    omniruntime::op::Operator::DeleteOperator(lookupOp);
    DeleteNestedLoopJoinOperatorFactory(factory, lookupFactory);
}

// NestLoopJoin with ConstVector in probe side
TEST(NestedLoopJoinTest, TestCrossJoinWithProbeConstVector)
{
    // Build side: regular vectors
    DataTypes buildDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t buildDataSize = 2;
    int32_t buildData0[buildDataSize] = {10, 30};
    std::string buildData1[buildDataSize] = {"aaa", "bbb"};
    auto buildVecBatch = CreateVectorBatch(buildDataTypes, buildDataSize, buildData0, buildData1);

    // Probe side: col0 (ConstVector int, all 99), col1 (regular varchar)
    DataTypes probeDataTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 3;
    auto *probeCol0 = new ConstVector<int32_t>(99, OMNI_INT, probeDataSize);
    auto *probeCol1 = new Vector<LargeStringContainer<std::string_view>>(probeDataSize);
    std::string probeStrings[] = {"xxx", "yyy", "zzz"};
    for (int32_t i = 0; i < probeDataSize; i++) {
        probeCol1->SetValue(i, std::string_view(probeStrings[i]));
    }
    auto *probeVecBatch = new VectorBatch(probeDataSize);
    probeVecBatch->Append(probeCol0);
    probeVecBatch->Append(probeCol1);

    int32_t buildColumns[2] = {0, 1};
    int32_t probeColumns[2] = {0, 1};

    auto factory = new NestedLoopJoinBuildOperatorFactory(buildDataTypes, buildColumns, 2);
    auto buildOp = static_cast<NestedLoopJoinBuildOperator *>(factory->CreateOperator(nullptr));
    buildOp->AddInput(buildVecBatch);
    VectorBatch *buildOutput = nullptr;
    buildOp->GetOutput(&buildOutput);

    int64_t factoryAddr = (int64_t)factory;
    JoinType &&joinTypePtr = OMNI_JOIN_TYPE_INNER;
    auto lookupFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
        joinTypePtr, probeDataTypes, probeColumns, 2, nullptr, factoryAddr, nullptr, config::QueryConfig());
    auto lookupOp = dynamic_cast<NestLoopJoinLookupOperator *>(lookupFactory->CreateOperator(nullptr));
    lookupOp->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupOp->GetOutput(&outputVecBatch);

    // Cross join: 3 probe rows * 2 build rows = 6 result rows
    EXPECT_EQ(outputVecBatch->GetRowCount(), 6);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(buildOp);
    omniruntime::op::Operator::DeleteOperator(lookupOp);
    DeleteNestedLoopJoinOperatorFactory(factory, lookupFactory);
}

}