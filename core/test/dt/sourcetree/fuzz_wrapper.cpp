/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "fuzz_wrapper.h"
#include <iostream>
#include <vector>
#include "type/decimal128.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/limit/distinct_limit.h"
#include "operator/sort/sort.h"
#include "operator/union/union.h"
#include "operator/topn/topn.h"
#include "operator/filter/filter_and_project.h"
#include "operator/window/window.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "operator/join/sortmergejoin/sort_merge_join.h"
#include "operator/topnsort/topn_sort_expr.h"
#include "operator/join/sortmergejoin/sort_merge_join_expr_v3.h"
#include "expression/expressions.h"
#include "test/util/dt_fuzz_factory_create_util.h"
#include "test/util/test_util.h"

// for fuzz mode
// type1 numbers
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace DtFuzzFactoryCreateUtil;

const int32_t SHORT_DECIMAL_SIZE = 18;
const int32_t LONG_DECIMAL_SIZE = 38;
uint16_t CHAR_SIZE = 10;
std::vector<DataTypePtr> allSupportedBaseTypes = {ShortType(),
                                                  IntType(),
                                                  LongType(),
                                                  BooleanType(),
                                                  DoubleType(),
                                                  Date32Type(DAY),
                                                  Decimal64Type(SHORT_DECIMAL_SIZE, 0),
                                                  Decimal128Type(LONG_DECIMAL_SIZE, 0),
                                                  VarcharType(CHAR_SIZE),
                                                  CharType(CHAR_SIZE)};

VectorBatch *CreateInputForAllTypes(DataTypes &sourceTypes, void **sortDatas, int32_t dataSize, uint16_t loopCount,
                                    bool isDictionary, bool hasNull)
{
    int32_t sourceTypesSize = sourceTypes.GetSize();
    std::vector<DataTypePtr> sourceTypesVec = sourceTypes.Get();
    int32_t *sourceTypeIds = const_cast<int32_t *>(sourceTypes.GetIds());
    int32_t totalDataSize = dataSize * loopCount + dataSize;
    const int32_t modValue = 4;

    BaseVector *sourceVectors[sourceTypesSize];
    for (int32_t i = 0; i < sourceTypesSize; i++) {
        int32_t capacityInBytes =
            (sourceTypeIds[i] == OMNI_CHAR || sourceTypeIds[i] == OMNI_VARCHAR)
                ? static_cast<int32_t>(static_cast<VarcharDataType *>(sourceTypesVec[i].get())->GetWidth()) *
                      totalDataSize
                : 0;
        sourceVectors[i] = VectorHelper::CreateVector(OMNI_FLAT, sourceTypeIds[i], totalDataSize, capacityInBytes);
        VectorHelper::SetValue(sourceVectors[i], 0, sortDatas[i]);
    }
    for (int32_t i = 1; i < totalDataSize; i++) {
        for (int32_t j = 0; j < sourceTypesSize; j++) {
            if ((i % modValue == 0) && hasNull && (sourceTypeIds[j] == OMNI_VARCHAR || sourceTypeIds[j] == OMNI_CHAR)) {
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(sourceVectors[j])->SetNull(i);
            } else if ((i % modValue == 0) && hasNull) {
                sourceVectors[j]->SetNull(i);
            } else {
                VectorHelper::SetValue(sourceVectors[j], i, sortDatas[j]);
            }
        }
    }

    if (isDictionary) {
        int32_t ids[totalDataSize];
        for (int32_t i = 0; i < totalDataSize; i++) {
            ids[i] = i;
        }
        for (int32_t i = 0; i < sourceTypesSize; i++) {
            auto sortVector = sourceVectors[i];
            sourceVectors[i] = VectorHelper::CreateDictionaryVector(ids, totalDataSize, sortVector, sourceTypeIds[i]);
            delete sortVector;
        }
    }

    auto sortVecBatch = new VectorBatch(totalDataSize);
    for (int32_t i = 0; i < sourceTypesSize; i++) {
        sortVecBatch->Append(sourceVectors[i]);
    }
    return sortVecBatch;
}

void DeleteOperatorFactory(omniruntime::op::OperatorFactory *operatorFactory) { delete operatorFactory; }

void TestFilter(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "Filter Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    FieldExpr *col1Expr = new FieldExpr(2, LongType());
    FieldExpr *col2Expr = new FieldExpr(4, DoubleType());
    std::vector<Expr *> projections = {col2Expr, col1Expr};
    FieldExpr *eqLeft = new FieldExpr(4, DoubleType());
    LiteralExpr *eqRight = new LiteralExpr(50.0, DoubleType());
    BinaryExpr *eqExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, eqRight, BooleanType());
    auto overflowConfig = new OverflowConfig();

    auto factory = dynamic_cast<FilterAndProjectOperatorFactory *>(
        CreateFilterFactory(sourceTypes, eqExpr, projections, overflowConfig));
    omniruntime::op::Operator *op = factory->CreateOperator();
    op->AddInput(sourceVecBatch);
    VectorBatch *resultVecBatch = nullptr;
    op->GetOutput(&resultVecBatch);

    if (resultVecBatch) {
        VectorHelper::FreeVecBatch(resultVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(op);
    DeleteOperatorFactory(factory);
    delete overflowConfig;
}

void TestSort(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "Sort Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    auto operatorFactory = dynamic_cast<SortOperatorFactory *>(CreateSortFactory(sourceTypes));
    auto sortOperator = dynamic_cast<SortOperator *>(operatorFactory->CreateOperator());
    sortOperator->AddInput(sourceVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    sortOperator->GetOutput(&outputVecBatch);

    if (outputVecBatch) {
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(sortOperator);
    DeleteOperatorFactory(operatorFactory);
}

void TestAggregation(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "Aggregation Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    AggregationOperatorFactory *nativeOperatorFactory =
        dynamic_cast<AggregationOperatorFactory *>(CreateAggregationFactory(sourceTypes));
    nativeOperatorFactory->Init();
    auto groupBy = nativeOperatorFactory->CreateOperator();
    groupBy->AddInput(sourceVecBatch);
    VectorBatch *resultVecBatch = nullptr;
    groupBy->GetOutput(&resultVecBatch);

    if (resultVecBatch) {
        VectorHelper::FreeVecBatch(resultVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(groupBy);
    DeleteOperatorFactory(nativeOperatorFactory);
}

void TestHashAggregation(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize,
                         uint16_t loopCount)
{
    std::cout << "HashAggregation Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    HashAggregationOperatorFactory *nativeOperatorFactory =
        dynamic_cast<HashAggregationOperatorFactory *>(CreateHashAggregationFactory(sourceTypes));
    nativeOperatorFactory->Init();
    auto groupBy = nativeOperatorFactory->CreateOperator();
    groupBy->AddInput(sourceVecBatch);
    VectorBatch *resultVecBatch = nullptr;
    groupBy->GetOutput(&resultVecBatch);

    if (resultVecBatch) {
        VectorHelper::FreeVecBatch(resultVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(groupBy);
    DeleteOperatorFactory(nativeOperatorFactory);
}

void TestHashJoin(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount,
                  std::string filterExpr, int32_t optCount)
{
    std::cout << "HashJoin Fuzz" << std::endl;
    loopCount = loopCount <= 100 ? loopCount : 100;
    auto vecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, (loopCount / 10) + 1, true, true);

    auto overflowConfig = new OverflowConfig();
    auto operatorFactories = CreateHashJoinFactory(sourceTypes, overflowConfig, filterExpr, optCount);
    auto hashBuilderFactory = dynamic_cast<HashBuilderOperatorFactory *>(operatorFactories[0]);
    auto hashBuilderOperator = hashBuilderFactory->CreateOperator();
    hashBuilderOperator->AddInput(vecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorBatch *probeVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    auto lookupJoinFactory = dynamic_cast<LookupJoinOperatorFactory *>(operatorFactories[1]);
    auto lookupJoinOperator = lookupJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);

        if (outputVecBatch) {
            VectorHelper::FreeVecBatch(outputVecBatch);
        }
    }
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    for (auto operatorFactory : operatorFactories) {
        DeleteOperatorFactory(operatorFactory);
    }
    delete overflowConfig;
}

void TestLookupOuterJoin(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize,
                         uint16_t loopCount, std::string filterExpr, int32_t optCount)
{
    std::cout << "LookupOuterJoin Fuzz" << std::endl;
    loopCount = loopCount <= 100 ? loopCount : 100;
    auto vecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, (loopCount / 10) + 1, true, true);

    auto overflowConfig = new OverflowConfig();
    auto operatorFactories = CreateLookupOuterFactory(sourceTypes, overflowConfig, filterExpr, optCount);
    auto hashBuilderFactory = dynamic_cast<HashBuilderOperatorFactory *>(operatorFactories[0]);
    auto hashBuilderOperator = hashBuilderFactory->CreateOperator();
    hashBuilderOperator->AddInput(vecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorBatch *probeVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    auto lookupJoinFactory = dynamic_cast<LookupOuterJoinOperatorFactory *>(operatorFactories[1]);
    auto lookupJoinOperator = lookupJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    VectorHelper::FreeVecBatch(probeVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    for (auto operatorFactory : operatorFactories) {
        DeleteOperatorFactory(operatorFactory);
    }
    delete overflowConfig;
}

void TestDistinctLimit(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "DistinctLimit Fuzz" << std::endl;
    VectorBatch *vecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    auto operatorFactory =
        dynamic_cast<DistinctLimitOperatorFactory *>(CreateDistinctLimitFactory(sourceTypes, loopCount));
    auto distinctLimitOperator = operatorFactory->CreateOperator();
    distinctLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    distinctLimitOperator->GetOutput(&outputVecBatch);

    if (outputVecBatch) {
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(distinctLimitOperator);
    DeleteOperatorFactory(operatorFactory);
}

void TestProject(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "Project Fuzz" << std::endl;
    VectorBatch *vecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    FieldExpr *addLeft = new FieldExpr(1, IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());

    std::vector<Expr *> exprs = {addExpr};
    auto overflowConfig = new OverflowConfig();

    auto operatorFactory =
        dynamic_cast<ProjectionOperatorFactory *>(CreateProjectFactory(sourceTypes, exprs, overflowConfig));
    auto projectOperator = operatorFactory->CreateOperator();
    projectOperator->AddInput(vecBatch);
    VectorBatch *resultVecBatch = nullptr;
    projectOperator->GetOutput(&resultVecBatch);

    if (resultVecBatch) {
        VectorHelper::FreeVecBatch(resultVecBatch);
    }
    omniruntime::op::Operator::DeleteOperator(projectOperator);
    DeleteOperatorFactory(operatorFactory);
    delete overflowConfig;
}

void TestUnion(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "Union Fuzz" << std::endl;
    VectorBatch *vecBatch1 = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);
    VectorBatch *vecBatch2 = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    UnionOperatorFactory *operatorFactory = dynamic_cast<UnionOperatorFactory *>(CreateUnionFactory(sourceTypes));
    auto unionOperator = operatorFactory->CreateOperator();
    unionOperator->AddInput(vecBatch1);
    unionOperator->AddInput(vecBatch2);
    std::vector<VectorBatch *> outputVecBatches;
    while (unionOperator->GetStatus() == OMNI_STATUS_NORMAL) {
        VectorBatch *outputVecBatch = nullptr;
        unionOperator->GetOutput(&outputVecBatch);
        outputVecBatches.push_back(outputVecBatch);
    }

    VectorHelper::FreeVecBatches(outputVecBatches);
    omniruntime::op::Operator::DeleteOperator(unionOperator);
    DeleteOperatorFactory(operatorFactory);
}

void TestSortMergeJoin(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "SortMergeJoin Fuzz" << std::endl;
    loopCount = loopCount <= 100 ? loopCount : 100;
    auto overflowConfig = new OverflowConfig();
    auto smjOp = dynamic_cast<SortMergeJoinOperator *>(CreateSortMergeJoinOperator(sourceTypes, overflowConfig));

    // construct data
    VectorBatch *streamedTblVecBatch1 = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, false, true);

    VectorBatch *bufferedTblVecBatch1 = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, false, true);

    // need add buffered table data
    smjOp->AddStreamedTableInput(streamedTblVecBatch1);

    // need add buffered table data
    smjOp->AddBufferedTableInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = omniruntime::TestUtil::CreateEmptyVectorBatch(sourceTypes);
    smjOp->AddBufferedTableInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = omniruntime::TestUtil::CreateEmptyVectorBatch(sourceTypes);
    smjOp->AddStreamedTableInput(streamedTblVecBatchEof);

    VectorBatch *result = nullptr;
    smjOp->GetOutput(&result);
    // getOutput should be called cyclically due to vecBatch iterator output function.
    if (smjOp->GetStatus() != OMNI_STATUS_FINISHED) {
        while (smjOp->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorHelper::FreeVecBatch(result);
            smjOp->GetOutput(&result);
        }
    }

    // construct exception input
    auto streamedTblVecBatchEof1 = omniruntime::TestUtil::CreateEmptyVectorBatch(sourceTypes);
    smjOp->AddStreamedTableInput(streamedTblVecBatchEof1);

    // check the join result
    VectorHelper::FreeVecBatch(result);

    omniruntime::op::Operator::DeleteOperator(smjOp);
    delete overflowConfig;
}

void TestTopN(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "TopN Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);

    TopNOperatorFactory *topNOperatorFactory = dynamic_cast<TopNOperatorFactory *>(CreateTopNFactory(sourceTypes));
    auto topNOperator = topNOperatorFactory->CreateOperator();
    topNOperator->AddInput(sourceVecBatch);
    VectorBatch *outputVectorBatch = nullptr;
    topNOperator->GetOutput(&outputVectorBatch);

    if (outputVectorBatch) {
        VectorHelper::FreeVecBatch(outputVectorBatch);
    }
    omniruntime::op::Operator::DeleteOperator(topNOperator);
    DeleteOperatorFactory(topNOperatorFactory);
}

void TestTopNSort(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "TopNSort Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, false, true);
    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(8, VarcharType(CHAR_SIZE))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(1, IntType())};
    TopNSortWithExprOperatorFactory *topNSortOperatorFactory = dynamic_cast<TopNSortWithExprOperatorFactory *>(
        CreateTopNSortFactory(sourceTypes, partitionKeys, sortKeys, dataSize));
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(topNSortOperatorFactory->CreateOperator());
    topNSortOperator->AddInput(sourceVecBatch);
    VectorBatch *outputVectorBatch = nullptr;
    topNSortOperator->GetOutput(&outputVectorBatch);

    if (outputVectorBatch) {
        VectorHelper::FreeVecBatch(outputVectorBatch);
    }
    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    DeleteOperatorFactory(topNSortOperatorFactory);
}

void TestSortMergeJoinV3(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize,
                         uint16_t loopCount)
{
    std::cout << "SortMergeJoinV3 Fuzz" << std::endl;

    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, false, true);
    StreamedTableWithExprOperatorFactoryV3 *streamedTableWithExprOperatorFactoryV3 =
        dynamic_cast<StreamedTableWithExprOperatorFactoryV3 *>(CreateStreamedWithExprOperatorFactory());
    BufferedTableWithExprOperatorFactoryV3 *sortMergeJoinV3OperatorFactory =
        dynamic_cast<BufferedTableWithExprOperatorFactoryV3 *>(
            CreateSortMergeJoinV3Factory(streamedTableWithExprOperatorFactoryV3));
    auto sortMergeJoinV3Operator =
        static_cast<BufferedTableWithExprOperatorV3 *>(sortMergeJoinV3OperatorFactory->CreateOperator());
    sortMergeJoinV3Operator->AddInput(sourceVecBatch);
    VectorBatch *outputVectorBatch = nullptr;
    sortMergeJoinV3Operator->GetOutput(&outputVectorBatch);

    if (outputVectorBatch) {
        VectorHelper::FreeVecBatch(outputVectorBatch);
    }
    omniruntime::op::Operator::DeleteOperator(sortMergeJoinV3Operator);
    DeleteOperatorFactory(streamedTableWithExprOperatorFactoryV3);
    DeleteOperatorFactory(sortMergeJoinV3OperatorFactory);
}

void TestWindow(void **testData, omniruntime::type::DataTypes &sourceTypes, int32_t dataSize, uint16_t loopCount)
{
    std::cout << "Window Fuzz" << std::endl;
    auto sourceVecBatch = CreateInputForAllTypes(sourceTypes, testData, dataSize, loopCount, true, true);
    DataTypes allTypes(
        std::vector<DataTypePtr>({ShortType(), IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
                                  Decimal64Type(SHORT_DECIMAL_SIZE, 0), Decimal128Type(LONG_DECIMAL_SIZE, 0),
                                  VarcharType(CHAR_SIZE), CharType(CHAR_SIZE), DoubleType()}));

    // dealing data with the operator
    WindowOperatorFactory *operatorFactory = dynamic_cast<WindowOperatorFactory *>(CreateWindowFactory(sourceTypes));
    auto windowOperator = operatorFactory->CreateOperator();
    windowOperator->AddInput(sourceVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);
    if (windowOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        while (windowOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorHelper::FreeVecBatch(outputVecBatch);
            windowOperator->GetOutput(&outputVecBatch);
        }
    }

    omniruntime::op::Operator::DeleteOperator(windowOperator);
    if (outputVecBatch) {
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    DeleteOperatorFactory(operatorFactory);
}

int GlobalFuzz(struct FuzzData fzd, uint16_t loopCount, std::string filterExpr, int32_t opCnt, uint16_t chooseFunc)
{
    std::cout << "chooseFunc is [" << chooseFunc << "]" << std::endl;
    std::cout << "input paras: shortValue = " << fzd.shortValue << ", intValue = " << fzd.intValue
              << ", longValue = " << fzd.longValue << ", boolValue = " << fzd.boolValue
              << ", doubleValue = " << fzd.doubleValue << ", highBits = " << fzd.highBits
              << ", lowBits = " << fzd.lowBits << ", strValue = " << fzd.strValue << ", loopCount = " << loopCount
              << std::endl;

    Decimal128 decimal128(fzd.highBits, fzd.lowBits);
    const int32_t dataSize = 10;
    CHAR_SIZE = fzd.strValue.size() == 0 ? 10 : fzd.strValue.size();

    // all types: short, int, long, boolean, double, date32, decimal, decimal128, varchar, char
    void *inputDatas[dataSize] = {&(fzd.shortValue),  &(fzd.intValue), &(fzd.longValue), &(fzd.boolValue),
                                  &(fzd.doubleValue), &(fzd.intValue), &(fzd.longValue), &decimal128,
                                  &(fzd.strValue),    &(fzd.strValue)};
    DataTypes sourceTypes(allSupportedBaseTypes);

    switch (chooseFunc) {
        case 1: TestAggregation(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 2: TestFilter(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 3: TestHashAggregation(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 4: {
            std::cout << "filterExpr = " << filterExpr << ", operatorCount = " << opCnt << std::endl;
            TestHashJoin(inputDatas, sourceTypes, dataSize, loopCount, filterExpr, opCnt);
            break;
        }
        case 5: TestDistinctLimit(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 6: TestProject(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 7: TestSort(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 8: TestSortMergeJoin(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 9: TestTopN(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 10: TestUnion(inputDatas, sourceTypes, dataSize, loopCount); break;
        case 11: TestWindow(inputDatas, sourceTypes, dataSize, loopCount); break;
        default: {
            TestAggregation(inputDatas, sourceTypes, dataSize, loopCount);
            TestFilter(inputDatas, sourceTypes, dataSize, loopCount);
            TestHashAggregation(inputDatas, sourceTypes, dataSize, loopCount);
            std::cout << "filterExpr = " << filterExpr << ", operatorCount = " << opCnt << std::endl;
            TestHashJoin(inputDatas, sourceTypes, dataSize, loopCount, filterExpr, opCnt);
            TestLookupOuterJoin(inputDatas, sourceTypes, dataSize, loopCount, filterExpr, opCnt);
            TestDistinctLimit(inputDatas, sourceTypes, dataSize, loopCount);
            TestProject(inputDatas, sourceTypes, dataSize, loopCount);
            TestSort(inputDatas, sourceTypes, dataSize, loopCount);
            TestSortMergeJoin(inputDatas, sourceTypes, dataSize, loopCount);
            TestTopN(inputDatas, sourceTypes, dataSize, loopCount);
            TestUnion(inputDatas, sourceTypes, dataSize, loopCount);
            TestWindow(inputDatas, sourceTypes, dataSize, loopCount);
            TestTopNSort(inputDatas, sourceTypes, dataSize, loopCount);
            TestSortMergeJoinV3(inputDatas, sourceTypes, dataSize, loopCount);
            break;
        }
    }
    return 0;
}
