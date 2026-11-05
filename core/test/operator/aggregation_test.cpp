/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
 */

#include <vector>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <mutex>
#include <cstdarg>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/all_aggregators.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/operator.h"
#include "vector/vector_helper.h"
#include "util/perf_util.h"
#include "util/config_util.h"
#include "util/test_util.h"
#include "util/test_agg_util.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/corr_aggregator.h"
#include "type/decimal128.h"
#include "util/type_util.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
const int32_t VEC_BATCH_NUM = 10;
const int32_t ROW_PER_VEC_BATCH = 200000;
const int32_t CARDINALITY = 4;
const int32_t COLUMN_NUM = 4;
const bool INPUT_MODE = true;
const bool OUTPUT_MODE = false;
const int CONST_VALUE_2 = 2;
const int CONST_VALUE_7 = 7;
const int CONST_VALUE_24 = 24;
const int CONST_VALUE_32 = 32;
const int CONST_VALUE_38 = 38;

static DataTypePtr SHORT_DECIMAL_TYPE = Decimal64Type(CONST_VALUE_7, CONST_VALUE_2);
static DataTypePtr SUM_IMMEDIATE_VARBINARY = VarcharType(CONST_VALUE_24);
static DataTypePtr AVG_IMMEDIATE_VARBINARY = VarcharType(CONST_VALUE_32);
static DataTypePtr LONG_DECIMAL_TYPE = Decimal128Type(CONST_VALUE_38, 0);

#ifdef DISABLE_TEST_NO_NEED_OCCUPY_BRANCH_TEST
long lrand()
{
    const int CONST_VALUE_8 = 8;
    if (sizeof(int) < sizeof(long)) {
        return (static_cast<long>(rand())) << (sizeof(int) * CONST_VALUE_8) | rand();
    }
    return rand();
}
#endif

using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

VectorBatch *ConstructSimpleBuildData()
{
    const int32_t dataSize = 3;
    std::vector<DataTypePtr> types{
        LongType(), LongType(), IntType(), ShortType(), DoubleType(), LongType(), LongType()
    };
    DataTypes outTypes(types);
    int64_t buildData0[dataSize] = {2, 1, 0};
    int64_t buildData1[dataSize] = {2, 1, 0};
    int32_t buildData2[dataSize] = {2, 1, 0};
    int16_t buildData3[dataSize] = {2, 1, 0};
    double buildData4[dataSize] = {2, 1, 0};
    int64_t buildData5[dataSize] = {60, 30, 0};
    int64_t buildData6[dataSize] = {60, 30, 0};
    return CreateVectorBatch(outTypes, dataSize, buildData0, buildData1, buildData2, buildData3, buildData4, buildData5,
        buildData6);
}

std::vector<VectorBatch *> ConstructSimpleBuildData(int32_t vecBatchCnt, int32_t rowPerBatch)
{
    const int32_t mod = 3;
    std::vector<VectorBatch *> input(vecBatchCnt);
    for (int32_t i = 0; i < vecBatchCnt; ++i) {
        auto *vecBatch = new VectorBatch(rowPerBatch);
        auto *col1 = new Vector<int64_t>(rowPerBatch);
        auto *col2 = new Vector<int32_t>(rowPerBatch);
        auto *col3 = new Vector<short>(rowPerBatch);
        auto *col4 = new Vector<double>(rowPerBatch);

        for (int32_t j = 0; j < rowPerBatch; ++j) {
            col1->SetValue(j, j % mod);
            col2->SetValue(j, j % mod);
            col3->SetValue(j, j % mod);
            col4->SetValue(j, j % mod);
        }

        vecBatch->Append(col1);
        vecBatch->Append(col1->Slice(0, rowPerBatch));
        vecBatch->Append(col2);
        vecBatch->Append(col3);
        vecBatch->Append(col4);

        input[i] = vecBatch;
    }
    return input;
}

BaseVector *BuildHashInput(const DataTypePtr groupType, int32_t rowPerVecBatch, int32_t cardinality)
{
    switch (groupType->GetId()) {
        case OMNI_INT:
        case OMNI_DATE32: {
            Vector<int32_t> *col = new Vector<int32_t>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                if (cardinality != 0) {
                    col->SetValue(j, j % cardinality);
                }
            }
            return col;
        }
        case OMNI_SHORT: {
            Vector<short> *col = new Vector<short>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                if (cardinality != 0) {
                    col->SetValue(j, j % cardinality);
                }
            }
            return col;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            Vector<int64_t> *col = new Vector<int64_t>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                if (cardinality != 0) {
                    col->SetValue(j, j % cardinality);
                }
            }
            return col;
        }
        case OMNI_FLOAT: {
            Vector<float> *col = new Vector<float>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                if (cardinality != 0) {
                    col->SetValue(j, j % cardinality);
                }
            }
            return col;
        }
        case OMNI_DOUBLE: {
            Vector<double> *col = new Vector<double>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, j % cardinality);
            }
            return col;
        }
        case OMNI_BOOLEAN: {
            Vector<bool> *col = new Vector<bool>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, j % cardinality);
            }
            return col;
        }
        case OMNI_DECIMAL128: {
            Vector<Decimal128> *col = new Vector<Decimal128>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                Decimal128 val = Decimal128(0, j % cardinality);
                col->SetValue(j, val);
            }
            return col;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR: {
            Vector<LargeStringContainer<std::string_view>> *col =
                new Vector<LargeStringContainer<std::string_view>>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                std::string str = std::to_string(j % cardinality);
                std::string_view strV(str.c_str(), str.size());
                col->SetValue(j, strV);
            }
            return col;
        }
        default: {
            LogError("No such %d type support", groupType->GetId());
            return nullptr;
        }
    }
}

BaseVector *BuildAggregateInput(const DataTypePtr aggType, int32_t rowPerVecBatch)
{
    switch (aggType->GetId()) {
        case OMNI_NONE: {
            Vector<int64_t> *col = new Vector<int64_t>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetNull(j);
            }
            return col;
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            Vector<int32_t> *col = new Vector<int32_t>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, 1);
            }
            return col;
        }
        case OMNI_SHORT: {
            Vector<short> *col = new Vector<short>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, 1);
            }
            return col;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            Vector<int64_t> *col = new Vector<int64_t>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, 1);
            }
            return col;
        }
        case OMNI_FLOAT: {
            Vector<float> *col = new Vector<float>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, 1);
            }
            return col;
        }
        case OMNI_DOUBLE: {
            Vector<double> *col = new Vector<double>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, 1);
            }
            return col;
        }
        case OMNI_BOOLEAN: {
            Vector<bool> *col = new Vector<bool>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                col->SetValue(j, true);
            }
            return col;
        }
        case OMNI_DECIMAL128: {
            Vector<Decimal128> *col = new Vector<Decimal128>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                Decimal128 val(0, 1);
                col->SetValue(j, val);
            }
            return col;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR: {
            Vector<LargeStringContainer<std::string_view>> *col =
                new Vector<LargeStringContainer<std::string_view>>(rowPerVecBatch);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                std::string str = std::to_string(j);
                std::string_view strV(str.c_str(), str.size());
                col->SetValue(j, strV);
            }
            return col;
        }
        default: {
            LogError("No such %d type support", aggType->GetId());
            return nullptr;
        }
    }
}

VectorBatch **BuildAggInput(int32_t vecBatchNum, int32_t rowPerVecBatch, int32_t cardinality, int32_t groupColNum,
    int32_t aggColNum, const std::vector<DataTypePtr> &groupTypes, const std::vector<DataTypePtr> &aggTypes)
{
    VectorBatch **input = new VectorBatch *[vecBatchNum];
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        VectorBatch *vecBatch = new VectorBatch(rowPerVecBatch);
        for (int32_t index = 0; index < groupColNum; ++index) {
            BaseVector *vec = BuildHashInput(groupTypes[index], rowPerVecBatch, cardinality);
            vecBatch->Append(vec);
        }
        for (int32_t index = 0; index < aggColNum; ++index) {
            BaseVector *vec = BuildAggregateInput(aggTypes[index], rowPerVecBatch);
            vecBatch->Append(vec);
        }
        input[i] = vecBatch;
    }
    return input;
}

VectorBatch **BuildVarCharInput(int32_t vecBatchNum, int32_t colNum, int32_t rowPerVecBatch, ...)
{
    va_list args;
    va_start(args, rowPerVecBatch);
    VectorBatch **input = new VectorBatch *[vecBatchNum];
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        VectorBatch *vecBatch = new VectorBatch(rowPerVecBatch);
        for (int32_t c = 0; c < colNum; ++c) {
            Vector<LargeStringContainer<std::string_view>> *col =
                new Vector<LargeStringContainer<std::string_view>>(rowPerVecBatch);
            std::string *values = va_arg(args, std::string *);
            for (int32_t j = 0; j < rowPerVecBatch; ++j) {
                std::string_view strV(values[j].c_str(), values[j].length());
                col->SetValue(j, strV);
            }
            vecBatch->Append(col);
        }
        input[i] = vecBatch;
    }
    va_end(args);
    return input;
}

#ifdef DISABLE_TEST_NO_NEED_OCCUPY_BRANCH_TEST
// create a factory and make it optimized
uintptr_t CreateHashFactoryWithJit(bool inputRaw, bool outputPartial)
{
    uint32_t groupCols[2] = {0, 1};
    std::vector<DataTypePtr> groupByTypeVec = { LongType(), LongType() };
    DataTypes groupByTypes(groupByTypeVec);
    uint32_t aggCols[2] = {2, 3};
    std::vector<DataTypePtr> aggInputTypeVec = { LongType(), LongType() };
    DataTypes aggInputTypes(aggInputTypeVec);
    std::vector<DataTypePtr> aggOutputTypeVec = { LongType(), LongType() };
    DataTypes aggOutputTypes(aggOutputTypeVec);
    uint32_t aggFunType[2] = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG};
    uint32_t maskCols[2] = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};
    std::vector<uint32_t> groupByColVector = std::vector<uint32_t>(groupCols, groupCols + 2);
    std::vector<uint32_t> aggColVector = std::vector<uint32_t>(aggCols, aggCols + 2);
    std::vector<uint32_t> aggFuncTypeVector = std::vector<uint32_t>(aggFunType, aggFunType + 2);
    std::vector<uint32_t> maskColsVector = std::vector<uint32_t>(maskCols, maskCols + 2);

    auto aggColVectorWrap = AggregatorUtil::WrapWithVector(aggColVector);
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWraps = AggregatorUtil::WrapWithVector(inputRaw, 2);
    auto outputPartialWraps = AggregatorUtil::WrapWithVector(outputPartial, 2);
    std::cout << "after jit" << std::endl;
    auto *nativeOperatorFactory =
        new omniruntime::op::HashAggregationOperatorFactory(groupByColVector, groupByTypes, aggColVectorWrap,
        aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypeVector, maskColsVector, inputRawWraps, outputPartialWraps);
    std::cout << "after create factory" << std::endl;
    nativeOperatorFactory->Init();
    return reinterpret_cast<uintptr_t>(nativeOperatorFactory);
}

uintptr_t CreateAggFactoryWithJit()
{
    const int CONST_VALUE_4 = 4;
    std::vector<DataTypePtr> dataTypeFields { LongType(), LongType(), LongType(), LongType() };
    DataTypes sourceTypes(dataTypeFields);
    uint32_t aggFuncTypes[CONST_VALUE_4] = {0, 0, 0, 0};
    std::vector<uint32_t> aggFuncTypeVector = std::vector<uint32_t>(aggFuncTypes, aggFuncTypes + CONST_VALUE_4);
    uint32_t aggInputCols[CONST_VALUE_4] = {0, 1, 2, 3};
    std::vector<uint32_t> aggInputColsVector = std::vector<uint32_t>(aggInputCols, aggInputCols + CONST_VALUE_4);
    uint32_t maskCols[CONST_VALUE_4] = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
                                        static_cast<uint32_t>(-1),
                                        static_cast<uint32_t>(-1)};
    std::vector<uint32_t> maskColsVector = std::vector<uint32_t>(maskCols, maskCols + CONST_VALUE_4);

    std::cout << "after jit" << std::endl;
    auto aggInputColsVectorWrap = AggregatorUtil::WrapWithVector(aggInputColsVector);
    auto sourceTypesWrap = AggregatorUtil::WrapWithVector(sourceTypes);
    auto trueWraps = AggregatorUtil::WrapWithVector(true, CONST_VALUE_4);
    auto falseWraps = AggregatorUtil::WrapWithVector(false, CONST_VALUE_4);
    auto nativeOperatorFactory = new AggregationOperatorFactory(sourceTypes, aggFuncTypeVector, aggInputColsVectorWrap,
        maskColsVector, sourceTypesWrap, trueWraps, falseWraps);
    nativeOperatorFactory->Init();
    std::cout << "after create factory" << std::endl;
    return reinterpret_cast<uintptr_t>(nativeOperatorFactory);
}
#endif

using HAFactoryParameters = struct HashAggregationFactoryParameters {
public:
    bool inputRaw;
    bool outputPartial;
    std::vector<uint32_t> groupCols;
    std::vector<DataTypePtr> groupByTypes;
    std::vector<uint32_t> aggCols;
    std::vector<DataTypePtr> aggInputTypes;
    std::vector<DataTypePtr> aggOutputTypes;
    std::vector<uint32_t> aggFuncTypes;
    std::vector<uint32_t> maskCols;
};

uintptr_t CreateHashFactoryWithoutJit(HAFactoryParameters &parameters)
{
    DataTypes groupByTypes(parameters.groupByTypes);
    DataTypes aggInputTypes(parameters.aggInputTypes);
    DataTypes aggOutputTypes(parameters.aggOutputTypes);

    std::vector<uint32_t> groupByColVector =
        std::vector<uint32_t>(parameters.groupCols.data(), parameters.groupCols.data() + parameters.groupCols.size());
    std::vector<uint32_t> aggColVector =
        std::vector<uint32_t>(parameters.aggCols.data(), parameters.aggCols.data() + parameters.aggCols.size());
    std::vector<uint32_t> aggFuncTypeVector = std::vector<uint32_t>(parameters.aggFuncTypes.data(),
        parameters.aggFuncTypes.data() + parameters.aggFuncTypes.size());
    std::vector<uint32_t> maskColsVector =
        std::vector<uint32_t>(parameters.maskCols.data(), parameters.maskCols.data() + parameters.maskCols.size());

    auto aggColVectorWrap = AggregatorUtil::WrapWithVector(aggColVector);
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWraps = std::vector<bool>(aggColVector.size(), parameters.inputRaw);
    auto outputPartialWraps = std::vector<bool>(aggColVector.size(), parameters.outputPartial);

    omniruntime::op::HashAggregationOperatorFactory *nativeOperatorFactory =
        new omniruntime::op::HashAggregationOperatorFactory(groupByColVector, groupByTypes, aggColVectorWrap,
        aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypeVector, maskColsVector, inputRawWraps, outputPartialWraps);
    nativeOperatorFactory->Init();
    return reinterpret_cast<uintptr_t>(nativeOperatorFactory);
}

double g_totalCpuTime;
double g_totalWallTime;
#ifdef DISABLE_TEST_NO_NEED_OCCUPY_BRANCH_TEST
void PerfTestOriginal(int64_t moduleAddr, VectorBatch **input, std::vector<DataTypePtr> allTypes)
{
    // create operator
    HashAggregationOperatorFactory *nativeOperatorFactory =
        reinterpret_cast<HashAggregationOperatorFactory *>(moduleAddr);
    auto groupBy = nativeOperatorFactory->CreateOperator();

    // execution
    for (int32_t i = 0; i < VEC_BATCH_NUM; ++i) {
        auto copiedBatch = DuplicateVectorBatch(input[i]);
        groupBy->AddInput(copiedBatch);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupBy->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 4);
    EXPECT_EQ(outputVecBatch->GetRowCount(), 4);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(groupBy);
}

void PerfTest(int64_t moduleAddr, VectorBatch **input, int32_t vecBatchNum, int32_t *rowCount,
    std::vector<DataTypePtr> allTypes)
{
    // create operatory
    HashAggregationOperatorFactory *nativeOperatorFactory =
        reinterpret_cast<HashAggregationOperatorFactory *>(moduleAddr);
    auto groupBy = nativeOperatorFactory->CreateOperator();

    // execution
    for (int pageIndex = 0; pageIndex < vecBatchNum; ++pageIndex) {
        auto copiedBatch = DuplicateVectorBatch(input[pageIndex]);
        auto errNo = groupBy->AddInput(copiedBatch);
        EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupBy->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 4);
    EXPECT_EQ(outputVecBatch->GetRowCount(), 4);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(groupBy);
}

void PerfTestNonGroup(int64_t moduleAddr, bool codegenMode, VectorBatch **input, int32_t vecBatchNum, int32_t *rowCount,
    std::vector<type::DataTypePtr> allTypes)
{
    // create operatory
    auto nativeOperatorFactory = reinterpret_cast<AggregationOperatorFactory *>(moduleAddr);
    omniruntime::op::Operator *aggregation = nullptr;
    if (codegenMode) {
        aggregation = CreateTestOperator(nativeOperatorFactory);
    } else {
        aggregation = nativeOperatorFactory->CreateOperator();
    }

    // execution
    for (int pageIndex = 0; pageIndex < vecBatchNum; ++pageIndex) {
        auto copiedBatch = DuplicateVectorBatch(input[pageIndex]);
        auto errNo = aggregation->AddInput(copiedBatch);
        EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggregation->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 4);
    EXPECT_EQ(outputVecBatch->GetRowCount(), 1);
    VectorHelper::FreeVecBatch(outputVecBatch);
}
#endif

TEST(HashAggregationOperatorTest, verify_correctness)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    // create 10 pages
    const int vecBatchNum = 10;
    const int rowSize = 2000;
    const int cardinality = 10;
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX };
    std::vector<DataTypePtr> groupTypes = { LongType(), LongType() };
    std::vector<DataTypePtr> aggTypes = { LongType(), LongType(), LongType(), LongType(), LongType() };
    VectorBatch **input1 = BuildAggInput(vecBatchNum, rowSize, cardinality, 2, 5, groupTypes, aggTypes);

    // First stage (partial)
    auto aggPartialFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1 }),
        std::vector<DataTypePtr>({ LongType(), LongType() }), aggFuncTypes, std::vector<uint32_t>({ 2, 3, 4, 5, 6 }),
        std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }),
        LongType(), LongType(), LongType() }),
        std::vector<uint32_t>(), true, true, false);

    // operator 1 (partial)
    auto aggPartial1 = aggPartialFactory->CreateOperator();
    aggPartial1->Init();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial1->AddInput(input1[i]);
    }

    VectorBatch *outputVecBatch1 = nullptr;
    int32_t vecBatchCount = aggPartial1->GetOutput(&outputVecBatch1);
    EXPECT_EQ(vecBatchCount, 1);
    op::Operator::DeleteOperator(aggPartial1);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, rowSize, cardinality, 2, 5, groupTypes, aggTypes);

    // operator 2 (partial)
    auto aggPartial2 = aggPartialFactory->CreateOperator();
    aggPartial2->Init();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial2->AddInput(input2[i]);
    }

    VectorBatch *outputVecBatch2 = nullptr;
    int32_t tableCount2 = aggPartial2->GetOutput(&outputVecBatch2);
    EXPECT_EQ(tableCount2, 1);
    op::Operator::DeleteOperator(aggPartial2);

    // Second stage (final)
    auto aggFinalFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1 }),
        std::vector<DataTypePtr>({ LongType(), LongType() }), aggFuncTypes, std::vector<uint32_t>({ 2, 3, 4, 5, 6 }),
        std::vector<DataTypePtr>({ LongType(), ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }),
        LongType(), LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), DoubleType(), LongType(), LongType(), LongType() }),
        std::vector<uint32_t>(), false, false, false);

    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch1);
    aggFinal->AddInput(outputVecBatch2);

    VectorBatch *outputVecBatch3 = nullptr;
    aggFinal->GetOutput(&outputVecBatch3);
    op::Operator::DeleteOperator(aggFinal);

    std::vector<DataTypePtr> expectFieldTypes { LongType(), LongType(), LongType(), DoubleType(),
        LongType(), LongType(), LongType() };
    // construct the output data
    DataTypes expectTypes(expectFieldTypes);
    int64_t expectData1[cardinality] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int64_t expectData2[cardinality] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int64_t expectData3[cardinality] = {4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000};
    double expectData4[cardinality] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    int64_t expectData5[cardinality] = {4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000};
    int64_t expectData6[cardinality] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int64_t expectData7[cardinality] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, cardinality, expectData1, expectData2, expectData3,
        expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch3, expectVecBatch));
    EXPECT_EQ(outputVecBatch3->GetVectorCount(), 7);

    delete[] input1;
    delete[] input2;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch3);
}

template<typename T>
void TestGroupArrayMapRegular(std::shared_ptr<DataType> groupType)
{
    ConfigUtil::SetAggHashTableRule(AggHashTableRule::ARRAY);
    const int vecBatchNum = 1;
    const int ROW_SIZE = 21;
    const int cardinality = 3;
    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
                                          OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN,
                                          OMNI_AGGREGATION_TYPE_MAX};
    std::vector<DataTypePtr> groupTypes = {groupType};
    std::vector<DataTypePtr> aggTypes = {LongType(), LongType(), LongType(), LongType(), LongType()};
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);
    input[0]->Get(0)->SetNull(0);
    input[0]->Get(0)->SetNull(11);

    input[0]->Get(2)->SetNull(0);
    input[0]->Get(2)->SetNull(1);
    input[0]->Get(2)->SetNull(2);
    input[0]->Get(2)->SetNull(3);
    input[0]->Get(2)->SetNull(4);

    input[0]->Get(3)->SetNull(0);
    input[0]->Get(4)->SetNull(0);
    input[0]->Get(5)->SetNull(0);

    input[0]->Get(3)->SetNull(1);
    input[0]->Get(4)->SetNull(1);
    input[0]->Get(5)->SetNull(1);
    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);

    // set group vector null
    input2[0]->Get(0)->SetNull(0);
    input2[0]->Get(0)->SetNull(11);
    input2[0]->Get(0)->SetNull(20);

    input2[0]->Get(2)->SetNull(0);
    input2[0]->Get(2)->SetNull(1);
    input2[0]->Get(2)->SetNull(2);
    input2[0]->Get(2)->SetNull(3);
    input2[0]->Get(2)->SetNull(4);

    input2[0]->Get(3)->SetNull(0);
    input2[0]->Get(4)->SetNull(0);
    input2[0]->Get(5)->SetNull(0);

    input2[0]->Get(3)->SetNull(1);
    input2[0]->Get(4)->SetNull(1);
    input2[0]->Get(5)->SetNull(1);

    // single stage
    auto aggFactory =
            CreateHashAggregationOperatorFactory(std::vector<uint32_t>({0}),
                                                 groupTypes, aggFuncTypes, std::vector<uint32_t>({1, 2, 3, 4, 5}),
                                                 std::vector<DataTypePtr>(
                                                         {LongType(), LongType(), LongType(), LongType(), LongType()}),
                                                 std::vector<DataTypePtr>(
                                                         {LongType(), DoubleType(), LongType(), LongType(),
                                                          LongType()}),
                                                 std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    static constexpr int64_t expectedSize = 4;

    T expectData1[expectedSize];
    if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
        expectData1[0] = std::numeric_limits<T>::quiet_NaN();  // 空值
        expectData1[1] = static_cast<T>(0);
        expectData1[2] = static_cast<T>(1);
        expectData1[3] = static_cast<T>(2);
    } else {
        // 对于整数类型，使用-1
        expectData1[0] = -1;
        expectData1[1] = 0;
        expectData1[2] = 1;
        expectData1[3] = 2;
    }

    int64_t expectData2[expectedSize] = {5, 12, 14, 11};
    double expectData3[expectedSize] = {1, 1, 1, 1};
    int64_t expectData4[expectedSize] = {3, 12, 12, 11};
    int64_t expectData5[expectedSize] = {1, 1, 1, 1};
    int64_t expectData6[expectedSize] = {1, 1, 1, 1};

    std::vector<DataTypePtr> expectedFieldTypes{groupType, LongType(), DoubleType(),
                                                LongType(), LongType(), LongType()};
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, expectedSize, expectData1, expectData2, expectData3,
                                                    expectData4, expectData5, expectData6);
    auto groupV = reinterpret_cast<Vector<T> *>(expectVecBatch->Get(0));
    groupV->SetNull(0);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    delete[] input2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verify_group_colum_array_map_regular)
{
    TestGroupArrayMapRegular<int16_t>(ShortType());
    TestGroupArrayMapRegular<int32_t>(IntType());
    TestGroupArrayMapRegular<int32_t>(Date32Type());
    TestGroupArrayMapRegular<int64_t>(LongType());
    TestGroupArrayMapRegular<int64_t>(TimestampType());
    TestGroupArrayMapRegular<int64_t>(Decimal64Type());
    TestGroupArrayMapRegular<float>(FloatType());
    TestGroupArrayMapRegular<double>(DoubleType());
}

template<typename T>
void TestGroupArrayMapWithoutAgg(std::shared_ptr<DataType> groupType)
{
    ConfigUtil::SetAggHashTableRule(AggHashTableRule::ARRAY);
    const int vecBatchNum = 1;
    const int ROW_SIZE = 21;
    const int cardinality = 3;
    std::vector<uint32_t> aggFuncTypes = {};
    std::vector<DataTypePtr> groupTypes = {groupType};
    std::vector<DataTypePtr> aggTypes = {};
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 0, groupTypes, aggTypes);
    // set group vector null
    input[0]->Get(0)->SetNull(0);
    input[0]->Get(0)->SetNull(11);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 0, groupTypes, aggTypes);

    // set group vector null
    input2[0]->Get(0)->SetNull(0);
    input2[0]->Get(0)->SetNull(11);
    input2[0]->Get(0)->SetNull(20);

    // single stage
    auto aggFactory =
            CreateHashAggregationOperatorFactory(std::vector<uint32_t>({0}),
                                                 groupTypes, aggFuncTypes, std::vector<uint32_t>({}),
                                                 aggTypes, aggTypes,
                                                 std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    static constexpr int64_t expectedSize = 4;
    T expectData1[expectedSize] = {-1, 0, 1, 2};

    std::vector<DataTypePtr> expectedFieldTypes{groupType};
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, expectedSize, expectData1);
    auto groupV = reinterpret_cast<Vector<T> *>(expectVecBatch->Get(0));
    groupV->SetNull(0);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    delete[] input2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verify_group_colum_array_map_regular_without_agg)
{
    TestGroupArrayMapWithoutAgg<int16_t>(ShortType());
    TestGroupArrayMapWithoutAgg<int32_t>(IntType());
    TestGroupArrayMapWithoutAgg<int32_t>(Date32Type());
    TestGroupArrayMapWithoutAgg<int64_t>(LongType());
    TestGroupArrayMapWithoutAgg<int64_t>(TimestampType());
    TestGroupArrayMapWithoutAgg<int64_t>(Decimal64Type());
    TestGroupArrayMapRegular<float>(FloatType());
    TestGroupArrayMapRegular<double>(DoubleType());
}

template<typename T>
void TestGroupArrayMapResize(std::shared_ptr<DataType> groupType)
{
    ConfigUtil::SetAggHashTableRule(AggHashTableRule::ARRAY);
    const int vecBatchNum = 1;
    const int ROW_SIZE = 21;
    const int cardinality = 3;
    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
                                          OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN,
                                          OMNI_AGGREGATION_TYPE_MAX};
    std::vector<DataTypePtr> groupTypes = {groupType};
    std::vector<DataTypePtr> aggTypes = {LongType(), LongType(), LongType(), LongType(), LongType()};
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);
    // set group vector null
    input[0]->Get(0)->SetNull(0);
    input[0]->Get(0)->SetNull(11);

    input[0]->Get(2)->SetNull(0);
    input[0]->Get(2)->SetNull(1);
    input[0]->Get(2)->SetNull(2);
    input[0]->Get(2)->SetNull(3);
    input[0]->Get(2)->SetNull(4);

    input[0]->Get(3)->SetNull(0);
    input[0]->Get(4)->SetNull(0);
    input[0]->Get(5)->SetNull(0);

    input[0]->Get(3)->SetNull(1);
    input[0]->Get(4)->SetNull(1);
    input[0]->Get(5)->SetNull(1);
    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);

    // set group vector null
    input2[0]->Get(0)->SetNull(0);
    input2[0]->Get(0)->SetNull(11);
    input2[0]->Get(0)->SetNull(20);

    // set group vector large number to resize map
    static_cast<Vector<T> *>(input2[0]->Get(0))->SetValue(2, 1000);

    input2[0]->Get(2)->SetNull(0);
    input2[0]->Get(2)->SetNull(1);
    input2[0]->Get(2)->SetNull(2);
    input2[0]->Get(2)->SetNull(3);
    input2[0]->Get(2)->SetNull(4);

    input2[0]->Get(3)->SetNull(0);
    input2[0]->Get(4)->SetNull(0);
    input2[0]->Get(5)->SetNull(0);

    input2[0]->Get(3)->SetNull(1);
    input2[0]->Get(4)->SetNull(1);
    input2[0]->Get(5)->SetNull(1);

    // single stage
    auto aggFactory =
            CreateHashAggregationOperatorFactory(std::vector<uint32_t>({0}),
                                                 groupTypes, aggFuncTypes, std::vector<uint32_t>({1, 2, 3, 4, 5}),
                                                 std::vector<DataTypePtr>(
                                                         {LongType(), LongType(), LongType(), LongType(), LongType()}),
                                                 std::vector<DataTypePtr>(
                                                         {LongType(), DoubleType(), LongType(), LongType(),
                                                          LongType()}),
                                                 std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    static constexpr int64_t expectedSize = 5;
    T expectData1[expectedSize] = {-1, 0, 1, 2, 1000};
    int64_t expectData2[expectedSize] = {5, 12, 14, 10, 1};
    double expectData3[expectedSize] = {1, 1, 1, 1, -1};
    int64_t expectData4[expectedSize] = {3, 12, 12, 10, 1};
    int64_t expectData5[expectedSize] = {1, 1, 1, 1, 1};
    int64_t expectData6[expectedSize] = {1, 1, 1, 1, 1};

    std::vector<DataTypePtr> expectedFieldTypes{groupType, LongType(), DoubleType(),
                                                LongType(), LongType(), LongType()};
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, expectedSize, expectData1, expectData2, expectData3,
                                                    expectData4, expectData5, expectData6);
    auto groupV = reinterpret_cast<Vector<T> *>(expectVecBatch->Get(0));
    groupV->SetNull(0);
    expectVecBatch->Get(2)->SetNull(4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    delete[] input2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verify_group_colum_array_map_resize)
{
    TestGroupArrayMapResize<int16_t>(ShortType());
    TestGroupArrayMapResize<int32_t>(IntType());
    TestGroupArrayMapResize<int32_t>(Date32Type());
    TestGroupArrayMapResize<int64_t>(LongType());
    TestGroupArrayMapResize<int64_t>(TimestampType());
    TestGroupArrayMapResize<int64_t>(Decimal64Type());
}

template<typename T>
void TestGroupArrayMapResizeWithoutAgg(std::shared_ptr<DataType> groupType)
{
    ConfigUtil::SetAggHashTableRule(AggHashTableRule::ARRAY);
    const int vecBatchNum = 1;
    const int ROW_SIZE = 21;
    const int cardinality = 3;
    std::vector<uint32_t> aggFuncTypes = {};
    std::vector<DataTypePtr> groupTypes = {groupType};
    std::vector<DataTypePtr> aggTypes = {};
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 0, groupTypes, aggTypes);
    // set group vector null
    input[0]->Get(0)->SetNull(0);
    input[0]->Get(0)->SetNull(11);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 0, groupTypes, aggTypes);

    // set group vector null
    input2[0]->Get(0)->SetNull(0);
    input2[0]->Get(0)->SetNull(11);
    input2[0]->Get(0)->SetNull(20);

    // set group vector large number to resize map
    static_cast<Vector<T> *>(input2[0]->Get(0))->SetValue(2, 1000);

    // single stage
    auto aggFactory =
            CreateHashAggregationOperatorFactory(std::vector<uint32_t>({0}),
                                                 groupTypes, aggFuncTypes, std::vector<uint32_t>({}),
                                                 aggTypes, aggTypes,
                                                 std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    static constexpr int64_t expectedSize = 5;
    T expectData1[expectedSize] = {-1, 0, 1, 2, 1000};

    std::vector<DataTypePtr> expectedFieldTypes{groupType};
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, expectedSize, expectData1);
    auto groupV = reinterpret_cast<Vector<T> *>(expectVecBatch->Get(0));
    groupV->SetNull(0);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    delete[] input2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verify_group_colum_array_map_resize_without_agg)
{
    TestGroupArrayMapResizeWithoutAgg<int16_t>(ShortType());
    TestGroupArrayMapResizeWithoutAgg<int32_t>(IntType());
    TestGroupArrayMapResizeWithoutAgg<int32_t>(Date32Type());
    TestGroupArrayMapResizeWithoutAgg<int64_t>(LongType());
    TestGroupArrayMapResizeWithoutAgg<int64_t>(TimestampType());
    TestGroupArrayMapResizeWithoutAgg<int64_t>(Decimal64Type());
    TestGroupArrayMapRegular<float>(FloatType());
    TestGroupArrayMapRegular<double>(DoubleType());
}

template<typename T>
void TestGroupArrayMapSwitch(std::shared_ptr<DataType> groupType)
{
    const int vecBatchNum = 1;
    const int ROW_SIZE = 21;
    const int cardinality = 3;
    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
                                          OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN,
                                          OMNI_AGGREGATION_TYPE_MAX};
    std::vector<DataTypePtr> groupTypes = {groupType};
    std::vector<DataTypePtr> aggTypes = {LongType(), LongType(), LongType(), LongType(), LongType()};
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);
    // set group vector null
    input[0]->Get(0)->SetNull(0);
    input[0]->Get(0)->SetNull(11);

    input[0]->Get(2)->SetNull(0);
    input[0]->Get(2)->SetNull(1);
    input[0]->Get(2)->SetNull(2);
    input[0]->Get(2)->SetNull(3);
    input[0]->Get(2)->SetNull(4);

    input[0]->Get(3)->SetNull(0);
    input[0]->Get(4)->SetNull(0);
    input[0]->Get(5)->SetNull(0);

    input[0]->Get(3)->SetNull(1);
    input[0]->Get(4)->SetNull(1);
    input[0]->Get(5)->SetNull(1);
    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);
    auto groupVector = input2[0]->Get(0);
    // set group vector large number to switch map
    static_cast<Vector<T> *>(groupVector)->SetValue(2, std::numeric_limits<T>::max());
    static_cast<Vector<T> *>(groupVector)->SetValue(5, std::numeric_limits<T>::min());
    int idx[ROW_SIZE];
    std::iota(idx, idx + ROW_SIZE, 0);
    auto groupDictVector = VectorHelper::CreateDictionaryVector(idx, ROW_SIZE, groupVector, groupType->GetId());
    input2[0]->SetVector(0, groupDictVector);
    // set group vector null
    groupDictVector->SetNull(0);
    groupDictVector->SetNull(11);
    groupDictVector->SetNull(20);

    input2[0]->Get(2)->SetNull(0);
    input2[0]->Get(2)->SetNull(1);
    input2[0]->Get(2)->SetNull(2);
    input2[0]->Get(2)->SetNull(3);
    input2[0]->Get(2)->SetNull(4);

    input2[0]->Get(3)->SetNull(0);
    input2[0]->Get(4)->SetNull(0);
    input2[0]->Get(5)->SetNull(0);

    input2[0]->Get(3)->SetNull(1);
    input2[0]->Get(4)->SetNull(1);
    input2[0]->Get(5)->SetNull(1);

    // single stage
    auto aggFactory =
            CreateHashAggregationOperatorFactory(std::vector<uint32_t>({0}),
                                                 groupTypes, aggFuncTypes, std::vector<uint32_t>({1, 2, 3, 4, 5}),
                                                 std::vector<DataTypePtr>(
                                                         {LongType(), LongType(), LongType(), LongType(), LongType()}),
                                                 std::vector<DataTypePtr>(
                                                         {LongType(), DoubleType(), LongType(), LongType(),
                                                          LongType()}),
                                                 std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    static constexpr int64_t expectedSize = 6;
    T expectData1[expectedSize] = {0, std::numeric_limits<T>::min(), 1, std::numeric_limits<T>::max(), 2, -1};
    int64_t expectData2[expectedSize] = {12, 1, 14, 1, 9, 5};
    double expectData3[expectedSize] = {1, 1, 1, -1, 1, 1};
    int64_t expectData4[expectedSize] = {12, 1, 12, 1, 9, 3};
    int64_t expectData5[expectedSize] = {1, 1, 1, 1, 1, 1};
    int64_t expectData6[expectedSize] = {1, 1, 1, 1, 1, 1};

    std::vector<DataTypePtr> expectedFieldTypes{*groupTypes.begin(), LongType(), DoubleType(),
                                                LongType(), LongType(), LongType()};
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, expectedSize, expectData1, expectData2, expectData3,
                                                    expectData4, expectData5, expectData6);
    auto groupV = reinterpret_cast<Vector<T> *>(expectVecBatch->Get(0));
    groupV->SetNull(expectedSize - 1);
    expectVecBatch->Get(2)->SetNull(3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete groupVector;
    delete[] input;
    delete[] input2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verify_group_colum_array_map_switch)
{
    TestGroupArrayMapSwitch<int64_t>(LongType());
    TestGroupArrayMapSwitch<int64_t>(TimestampType());
    TestGroupArrayMapSwitch<int64_t>(Decimal64Type());
    TestGroupArrayMapSwitch<int32_t>(IntType());
    TestGroupArrayMapSwitch<int32_t>(Date32Type());
    TestGroupArrayMapSwitch<int16_t>(ShortType());
}

template<typename T>
void TestGroupArrayMapSwitchWithoutAgg(std::shared_ptr<DataType> groupType)
{
    const int vecBatchNum = 1;
    const int ROW_SIZE = 21;
    const int cardinality = 3;
    std::vector<uint32_t> aggFuncTypes = {};
    std::vector<DataTypePtr> groupTypes = {groupType};
    std::vector<DataTypePtr> aggTypes = {};
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 0, groupTypes, aggTypes);
    // set group vector null
    input[0]->Get(0)->SetNull(0);
    input[0]->Get(0)->SetNull(11);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 0, groupTypes, aggTypes);
    auto groupVector = input2[0]->Get(0);
    // set group vector large number to switch map
    static_cast<Vector<T> *>(groupVector)->SetValue(2, std::numeric_limits<T>::max());
    static_cast<Vector<T> *>(groupVector)->SetValue(5, std::numeric_limits<T>::min());
    int idx[ROW_SIZE];
    std::iota(idx, idx + ROW_SIZE, 0);
    auto groupDictVector = VectorHelper::CreateDictionaryVector(idx, ROW_SIZE, groupVector, groupType->GetId());
    input2[0]->SetVector(0, groupDictVector);
    // set group vector null
    groupDictVector->SetNull(0);
    groupDictVector->SetNull(11);
    groupDictVector->SetNull(20);

    // single stage
    auto aggFactory =
            CreateHashAggregationOperatorFactory(std::vector<uint32_t>({0}),
                                                 groupTypes, aggFuncTypes, std::vector<uint32_t>(),
                                                 aggTypes,
                                                 aggTypes,
                                                 std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    static constexpr int64_t expectedSize = 6;
    T expectData1[expectedSize] = {0, std::numeric_limits<T>::min(), 1, std::numeric_limits<T>::max(), 2, -1};

    std::vector<DataTypePtr> expectedFieldTypes{*groupTypes.begin()};
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, expectedSize, expectData1);
    auto groupV = reinterpret_cast<Vector<T> *>(expectVecBatch->Get(0));
    groupV->SetNull(expectedSize - 1);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete groupVector;
    delete[] input;
    delete[] input2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verify_group_colum_array_map_switch_without_agg)
{
    TestGroupArrayMapSwitchWithoutAgg<int64_t>(LongType());
    TestGroupArrayMapSwitchWithoutAgg<int64_t>(TimestampType());
    TestGroupArrayMapSwitchWithoutAgg<int64_t>(Decimal64Type());
    TestGroupArrayMapSwitchWithoutAgg<int32_t>(IntType());
    TestGroupArrayMapSwitchWithoutAgg<int32_t>(Date32Type());
    TestGroupArrayMapSwitch<int16_t>(ShortType());
}

TEST(HashAggregationOperatorTest, verify_varchar_vector_correctness)
{
    // create 10 pages
    const int vecBatchNum = 1;
    const int rowSize = 8;
    const int columnCount = 4; // groupby + count + min + max
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MAX };
    std::string data0[8] = {"0", "1", "2", "0", "1", "2", "0", "1"};
    std::string data1[8] = {"0", "1", "2", "0", "1", "2", "0", "1"};
    std::string data2[8] = {"0", "1", "2", "0", "1", "2", "0", "1"};
    std::string data3[8] = {"6.6", "5.5", "4.4", "3.3", "2.2", "1.1", "1.1", "1"};
    VectorBatch **input = BuildVarCharInput(vecBatchNum, columnCount, rowSize, data0, data1, data2, data3);

    // single stage
    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ VarcharType(1) }), aggFuncTypes, std::vector<uint32_t>({ 1, 2, 3 }),
        std::vector<DataTypePtr>({ VarcharType(1), VarcharType(1), VarcharType(4) }),
        std::vector<DataTypePtr>({ LongType(), VarcharType(1), VarcharType(4) }), std::vector<uint32_t>(), true, false,
        false);
    auto resultTypes = std::vector<DataTypePtr>({ LongType(), VarcharType(1), VarcharType(4) });
    auto groupByVarChar = aggFactory->CreateOperator();
    groupByVarChar->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByVarChar->AddInput(input[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByVarChar->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    op::Operator::DeleteOperator(groupByVarChar);
    std::string expectData1[3] = {"0", "1", "2"};
    int64_t expectData2[3] = {3, 3, 2};
    std::string expectData3[3] = {"0", "1", "2"};
    std::string expectData4[3] = {"6.6", "5.5", "4.4"};
    std::vector<DataTypePtr> expectedFieldTypes { VarcharType(1), LongType(), VarcharType(1), VarcharType(3) };
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectedTypes, 3, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationOperatorTest, verify_char_vector_correctness)
{
    // create 10 pages
    const int vecBatchNum = 1;
    const int rowSize = 8;
    const int columnCount = 4; // groupby + count + min + max
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MAX };
    std::string data0[8] = {"0", "1", "2", "0", "1", "2", "0", "1"};
    std::string data1[8] = {"0", "1", "2", "0", "1", "2", "0", "1"};
    std::string data2[8] = {"0", "1", "2", "0", "1", "2", "0", "1"};
    std::string data3[8] = {"6.6", "5.5", "4.4", "3.3", "2.2", "1.1", "1.1", "1"};
    VectorBatch **input = BuildVarCharInput(vecBatchNum, columnCount, rowSize, data0, data1, data2, data3);

    // single stage
    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ CharType(1) }), aggFuncTypes, std::vector<uint32_t>({ 1, 2, 3 }),
        std::vector<DataTypePtr>({ CharType(1), CharType(1), CharType(4) }),
        std::vector<DataTypePtr>({ LongType(), CharType(1), CharType(4) }), std::vector<uint32_t>(), true, false,
        false);

    auto groupByVarChar = aggFactory->CreateOperator();
    groupByVarChar->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByVarChar->AddInput(input[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByVarChar->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    op::Operator::DeleteOperator(groupByVarChar);
    std::string expectData1[3] = {"0", "1", "2"};
    int64_t expectData2[3] = {3, 3, 2};
    std::string expectData3[3] = {"0", "1", "2"};
    std::string expectData4[3] = {"6.6", "5.5", "4.4"};

    std::vector<DataTypePtr> expectedFieldTypes { CharType(1), LongType(), CharType(1), CharType(3) };
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectedTypes, 3, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationOperatorTest, verify_null_correctness)
{
    // create 10 pages
    const int vecBatchNum = 1;
    const int ROW_SIZE = 6;
    const int cardinality = 1;
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX };
    std::vector<DataTypePtr> groupTypes = { LongType() };
    std::vector<DataTypePtr> aggTypes = { LongType(), LongType(), LongType(), LongType(), LongType() };
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_SIZE, cardinality, 1, 5, groupTypes, aggTypes);

    input[0]->Get(2)->SetNull(0);
    input[0]->Get(2)->SetNull(1);
    input[0]->Get(2)->SetNull(2);
    input[0]->Get(2)->SetNull(3);
    input[0]->Get(2)->SetNull(4);

    input[0]->Get(3)->SetNull(1);
    input[0]->Get(4)->SetNull(1);
    input[0]->Get(5)->SetNull(1);

    // single stage
    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ LongType() }), aggFuncTypes, std::vector<uint32_t>({ 1, 2, 3, 4, 5 }),
        std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), DoubleType(), LongType(), LongType(), LongType() }),
        std::vector<uint32_t>(), true, false, false);
    auto groupByNULL = aggFactory->CreateOperator();
    groupByNULL->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByNULL->AddInput(input[i]);
    }
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByNULL->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    omniruntime::op::Operator::DeleteOperator(groupByNULL);

    int64_t expectData1[1] = {0};
    int64_t expectData2[1] = {6};
    double expectData3[1] = {1};
    int64_t expectData4[1] = {5};
    int64_t expectData5[1] = {1};
    int64_t expectData6[1] = {1};

    std::vector<DataTypePtr> expectedFieldTypes { LongType(), LongType(), DoubleType(),
        LongType(), LongType(), LongType() };
    DataTypes expectedTypes(expectedFieldTypes);
    VectorBatch *expectVecBatch = CreateVectorBatch(expectedTypes, 1, expectData1, expectData2, expectData3,
        expectData4, expectData5, expectData6);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVecBatch));

    delete[] input;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, verfify_correctness_group_by_agg_same_cols)
{
    // create 10 vecBatches
    const int vecBatchNum = 10;
    const int dataSize = 10;
    std::vector<VectorBatch *> input = ConstructSimpleBuildData(vecBatchNum, dataSize);

    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1, 2, 3, 4 }),
        std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), ShortType(), DoubleType() }),
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 0, 1 }), std::vector<DataTypePtr>({ LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), LongType() }), std::vector<uint32_t>(), true, false, false);
    auto groupBy = aggFactory->CreateOperator();
    groupBy->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupBy->AddInput(input[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    groupBy->GetOutput(&outputVecBatch);
    std::vector<DataTypePtr> expectedTypes { LongType(),   LongType(), IntType(), ShortType(),
        DoubleType(), LongType(), LongType() };
    VectorBatch *expected = ConstructSimpleBuildData();
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expected));

    op::Operator::DeleteOperator(groupBy);
    VectorHelper::FreeVecBatch(outputVecBatch);

    VectorHelper::FreeVecBatch(expected);
}

TEST(HashAggregationOperatorTest, verify_distinct_correctness)
{
    // construct data
    const int32_t dataSize = 4;
    const int32_t resultDataSize = 1;

    // table1
    int64_t dataHash[dataSize] = {1L, 1L, 1L, 1L}; // all rows will be in same hash group
    int64_t data0[dataSize] = {10L, 20L, 10L, 30L};
    int64_t data1[dataSize] = {1L, 1L, 2L, 3L};
    int64_t data2[dataSize] = {3L, 5L, 5L, 7L};
    int64_t data3[dataSize] = {4L, 2L, 1L, 2L};
    int64_t data4[dataSize] = {4L, 2L, 1L, 2L};
    bool data5[dataSize] = {true, true, false, true};
    bool data6[dataSize] = {true, false, true, true};
    bool data7[dataSize] = {true, true, false, true};
    bool data8[dataSize] = {true, true, true, false};
    bool data9[dataSize] = {true, true, true, false};

    std::vector<DataTypePtr> types = { LongType(),    LongType(),    LongType(),    LongType(),
        LongType(),    LongType(),    BooleanType(), BooleanType(),
        BooleanType(), BooleanType(), BooleanType() };
    DataTypes sourceTypes(types);

    std::vector<DataTypePtr> inputTypes = { LongType(), LongType(), LongType(), LongType(), LongType() };
    DataTypes inputDataTypes(inputTypes);
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize, dataHash, data0, data1, data2, data3, data4,
        data5, data6, data7, data8, data9);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MIN };

    // STAGE1: (partial)
    auto aggPartialFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ LongType() }), aggFuncTypes, std::vector<uint32_t>({ 1, 2, 3, 4, 5 }),
        std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), LongType(),
        ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), LongType(), LongType() }),
        std::vector<uint32_t>({ 6, 7, 8, 9, 10 }), true, true, false);
    auto aggregatePartial = aggPartialFactory->CreateOperator();
    aggregatePartial->Init();
    aggregatePartial->AddInput(vecBatch1);

    VectorBatch *outputVecBatch = nullptr;
    int32_t tableCount1 = aggregatePartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(tableCount1, 1);

    // STAGE2: (final)
    auto aggFinalFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ LongType() }), aggFuncTypes, std::vector<uint32_t>({ 1, 2, 3, 4, 5 }),
        std::vector<DataTypePtr>({ LongType(), LongType(),
        ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), LongType(), DoubleType(), LongType(), LongType() }),
        std::vector<uint32_t>(), false, false, false);
    auto aggregateFinal = aggFinalFactory->CreateOperator();
    aggregateFinal->Init();
    aggregateFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    int32_t tableCount = aggregateFinal->GetOutput(&outputVecBatch1);
    EXPECT_EQ(tableCount, 1);
    EXPECT_EQ(outputVecBatch1->GetRowCount(), 1);
    EXPECT_EQ(outputVecBatch1->GetVectorCount(), 6);

    int64_t expHashData[resultDataSize] = {1L};
    int64_t expData0[resultDataSize] = {3L};
    int64_t expData1[resultDataSize] = {6L};
    double expData2[resultDataSize] = {5.0};
    int64_t expData3[resultDataSize] = {4L};
    int64_t expData4[resultDataSize] = {1L};
    std::vector<DataTypePtr> resultType = { LongType(), LongType(), LongType(), DoubleType(), LongType(), LongType() };
    DataTypes resultTypes(resultType);
    VectorBatch *expVecBatch1 =
        CreateVectorBatch(resultTypes, resultDataSize, expHashData, expData0, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch1, expVecBatch1));

    omniruntime::op::Operator::DeleteOperator(aggregatePartial);
    omniruntime::op::Operator::DeleteOperator(aggregateFinal);
    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}

TEST(HashAggregationOperatorTest, verify_first_val)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    // create 10 pages
    const int vecBatchNum = 1;
    const int rowSize = 20;
    const int cardinality = 10;
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL };
    std::vector<DataTypePtr> groupTypes = { VarcharType(5) };
    std::vector<DataTypePtr> aggTypes = { VarcharType(5), VarcharType(5) };
    VectorBatch **input1 = BuildAggInput(vecBatchNum, rowSize, cardinality, 1, 2, groupTypes, aggTypes);

    // First stage (partial)
    std::vector<uint32_t> groupByCol0({ 0 });
    DataTypes groupInputTypes0(groupTypes);
    std::vector<std::vector<uint32_t>> aggsCols0({ { 1 }, { 2 } });
    std::vector<DataTypes> aggInputTypes0({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })) });
    std::vector<DataTypes> aggOutputTypes0({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })) });
    std::vector<uint32_t> maskColsVector0({ static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) });
    std::vector<bool> inputRaws0({ true, true });
    std::vector<bool> outputPartials0({ true, true });
    auto aggPartialFactory = new HashAggregationOperatorFactory(groupByCol0, groupInputTypes0, aggsCols0,
        aggInputTypes0, aggOutputTypes0, aggFuncTypes, maskColsVector0, inputRaws0, outputPartials0);
    aggPartialFactory->Init();

    // operator 1 (partial)
    auto aggPartial1 = aggPartialFactory->CreateOperator();
    aggPartial1->Init();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial1->AddInput(input1[i]);
    }

    VectorBatch *outputVecBatch1 = nullptr;
    int32_t vecBatchCount = aggPartial1->GetOutput(&outputVecBatch1);
    EXPECT_EQ(vecBatchCount, 1);
    op::Operator::DeleteOperator(aggPartial1);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, rowSize, cardinality, 1, 2, groupTypes, aggTypes);

    // operator 2 (partial)
    auto aggPartial2 = aggPartialFactory->CreateOperator();
    aggPartial2->Init();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial2->AddInput(input2[i]);
    }

    VectorBatch *outputVecBatch2 = nullptr;
    int32_t tableCount2 = aggPartial2->GetOutput(&outputVecBatch2);
    EXPECT_EQ(tableCount2, 1);
    op::Operator::DeleteOperator(aggPartial2);

    // Second stage (final)
    std::vector<uint32_t> groupByCol1({ 0 });
    DataTypes groupInputTypes1(groupTypes);
    std::vector<std::vector<uint32_t>> aggsCols1({ { 1, 2 }, { 3, 4 } });
    std::vector<DataTypes> aggInputTypes1({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })) });
    std::vector<DataTypes> aggOutputTypes1({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })) });
    std::vector<uint32_t> maskColsVector1({ static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) });
    std::vector<bool> inputRaws1({ false, false });
    std::vector<bool> outputPartials1({ false, false });
    auto aggFinalFactory = new HashAggregationOperatorFactory(groupByCol1, groupInputTypes1, aggsCols1, aggInputTypes1,
        aggOutputTypes1, aggFuncTypes, maskColsVector1, inputRaws1, outputPartials1);
    aggFinalFactory->Init();

    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch1);
    aggFinal->AddInput(outputVecBatch2);

    VectorBatch *outputVecBatch3 = nullptr;
    aggFinal->GetOutput(&outputVecBatch3);
    op::Operator::DeleteOperator(aggFinal);

    std::vector<DataTypePtr> expectFieldTypes { VarcharType(5), VarcharType(5), VarcharType(5) };
    // construct the output data
    DataTypes expectTypes(expectFieldTypes);
    std::string expectData1[cardinality] = {"1", "2", "9", "3", "5", "4", "7", "0", "6", "8"};
    std::string expectData2[cardinality] = {"1", "2", "9", "3", "5", "4", "7", "0", "6", "8"};
    std::string expectData3[cardinality] = {"1", "2", "9", "3", "5", "4", "7", "0", "6", "8"};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, cardinality, expectData1, expectData2, expectData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch3, expectVecBatch));
    EXPECT_EQ(outputVecBatch3->GetVectorCount(), 3);

    delete[] input1;
    delete[] input2;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch3);
    delete aggPartialFactory;
    delete aggFinalFactory;
}

TEST(HashAggregationOperatorTest, verify_last_val)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    const int vecBatchNum = 1;
    const int rowSize = 20;
    const int cardinality = 10;
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_LAST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL };
    std::vector<DataTypePtr> groupTypes = { VarcharType(5) };
    std::vector<DataTypePtr> aggTypes = { VarcharType(5), VarcharType(5) };
    VectorBatch **input1 = BuildAggInput(vecBatchNum, rowSize, cardinality, 1, 2, groupTypes, aggTypes);

    std::vector<uint32_t> groupByCol0({ 0 });
    DataTypes groupInputTypes0(groupTypes);
    std::vector<std::vector<uint32_t>> aggsCols0({ { 1 }, { 2 } });
    std::vector<DataTypes> aggInputTypes0({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })) });
    std::vector<DataTypes> aggOutputTypes0({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })) });
    std::vector<uint32_t> maskColsVector0({ static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) });
    std::vector<bool> inputRaws0({ true, true });
    std::vector<bool> outputPartials0({ true, true });
    auto aggPartialFactory = new HashAggregationOperatorFactory(groupByCol0, groupInputTypes0, aggsCols0,
        aggInputTypes0, aggOutputTypes0, aggFuncTypes, maskColsVector0, inputRaws0, outputPartials0);
    aggPartialFactory->Init();

    auto aggPartial1 = aggPartialFactory->CreateOperator();
    aggPartial1->Init();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial1->AddInput(input1[i]);
    }

    VectorBatch *outputVecBatch1 = nullptr;
    int32_t vecBatchCount = aggPartial1->GetOutput(&outputVecBatch1);
    EXPECT_EQ(vecBatchCount, 1);
    op::Operator::DeleteOperator(aggPartial1);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, rowSize, cardinality, 1, 2, groupTypes, aggTypes);

    auto aggPartial2 = aggPartialFactory->CreateOperator();
    aggPartial2->Init();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial2->AddInput(input2[i]);
    }

    VectorBatch *outputVecBatch2 = nullptr;
    int32_t tableCount2 = aggPartial2->GetOutput(&outputVecBatch2);
    EXPECT_EQ(tableCount2, 1);
    op::Operator::DeleteOperator(aggPartial2);

    std::vector<uint32_t> groupByCol1({ 0 });
    DataTypes groupInputTypes1(groupTypes);
    std::vector<std::vector<uint32_t>> aggsCols1({ { 1, 2 }, { 3, 4 } });
    std::vector<DataTypes> aggInputTypes1({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5), BooleanType() })) });
    std::vector<DataTypes> aggOutputTypes1({ DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })),
        DataTypes(std::vector<DataTypePtr>({ VarcharType(5) })) });
    std::vector<uint32_t> maskColsVector1({ static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) });
    std::vector<bool> inputRaws1({ false, false });
    std::vector<bool> outputPartials1({ false, false });
    auto aggFinalFactory = new HashAggregationOperatorFactory(groupByCol1, groupInputTypes1, aggsCols1, aggInputTypes1,
        aggOutputTypes1, aggFuncTypes, maskColsVector1, inputRaws1, outputPartials1);
    aggFinalFactory->Init();

    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch1);
    aggFinal->AddInput(outputVecBatch2);

    VectorBatch *outputVecBatch3 = nullptr;
    aggFinal->GetOutput(&outputVecBatch3);
    op::Operator::DeleteOperator(aggFinal);

    std::vector<DataTypePtr> expectFieldTypes { VarcharType(5), VarcharType(5), VarcharType(5) };
    DataTypes expectTypes(expectFieldTypes);
    // BuildAggInput: group col = to_string(j % 10), agg col = to_string(j). So each group k has two rows:
    // first row agg="k", last row agg="10+k". Last(agg) must be "10","11",...,"19" (same row order as groups).
    std::string expectData1[cardinality] = {"1", "2", "9", "3", "5", "4", "7", "0", "6", "8"};
    std::string expectData2[cardinality] = {"11", "12", "19", "13", "15", "14", "17", "10", "16", "18"};
    std::string expectData3[cardinality] = {"11", "12", "19", "13", "15", "14", "17", "10", "16", "18"};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, cardinality, expectData1, expectData2, expectData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch3, expectVecBatch));
    EXPECT_EQ(outputVecBatch3->GetVectorCount(), 3);

    delete[] input1;
    delete[] input2;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch3);
    delete aggPartialFactory;
    delete aggFinalFactory;
}

#ifdef DISABLE_TEST_NO_NEED_OCCUPY_BRANCH_TEST
TEST(HashAggregationOperatorTest, DISABLED_original_multiple_threads)
{
    using namespace std;
    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;

    std::vector<DataTypePtr> groupTypes { LongType(), LongType() };
    std::vector<DataTypePtr> inputTypes { LongType(), LongType() };
    std::vector<DataTypePtr> allInputTypes { LongType(), LongType(), LongType(), LongType() };
    VectorBatch **input = buildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 2, groupTypes, inputTypes);

    auto nativeOperatorFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1 }),
        std::vector<DataTypePtr>({ LongType(), LongType() }),
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 2, 3 }), std::vector<DataTypePtr>({ LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), LongType() }), std::vector<uint32_t>(), true, false, false);
    uint64_t factoryObjAddr = reinterpret_cast<uint64_t>(nativeOperatorFactory.get());

    uint32_t threadNums[] = {1, 2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(uint32_t); ++i) {
        g_totalWallTime = 0;
        g_totalCpuTime = 0;
        auto t_ = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;

        uint32_t threadNum = threadNums[i];
        std::vector<std::thread> vecOfThreads;
        Timer timer;
        timer.SetStart();
        for (uint32_t j = 0; j < threadNum; ++j) {
            // same stage Id
            std::thread t(PerfTestOriginal, factoryObjAddr, input, std::move(allInputTypes));
            vecOfThreads.push_back(std::move(t));
        }
        for (auto &th : vecOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse();
        double cpuElapsed = timer.GetCpuElapse();
        std::cout << threadNum << " wallElapsed time: " << wallElapsed << "s" << std::endl;
        std::cout << threadNum << " cpuElapsed time: " << cpuElapsed / processorCount * t_ << "s" << std::endl;
        std::this_thread::sleep_for(100ms);
    }
    delete input;
}

TEST(HashAggregationOperatorTest, DISABLED_perf_via_API_multiple_threads)
{
    using namespace std;
    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;

    vector<DataTypePtr> groupTypes = { LongType(), LongType() };
    vector<DataTypePtr> aggTypes = { LongType(), LongType() };
    vector<DataTypePtr> allInputTypes = { LongType(), LongType(), LongType(), LongType() };
    VectorBatch **input = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 2, groupTypes, aggTypes);

    int32_t *rowCount = new int32_t[VEC_BATCH_NUM];
    for (int32_t i = 0; i < VEC_BATCH_NUM; i++) {
        rowCount[i] = ROW_PER_VEC_BATCH;
    }
    uint64_t factoryObjAddr = CreateHashFactoryWithJit(true, false);
    std::cout << "after prepare" << std::endl;
    uint32_t threadNums[] = {1, 2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(uint32_t); ++i) {
        g_totalWallTime = 0;
        g_totalCpuTime = 0;
        auto t_ = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;

        uint32_t threadNum = threadNums[i];
        std::vector<std::thread> vecOfThreads;
        Timer timer;
        timer.SetStart();
        for (uint32_t j = 0; j < threadNum; ++j) {
            // same stage Id
            std::thread t(PerfTest, factoryObjAddr, input, VEC_BATCH_NUM, rowCount, std::move(allInputTypes));
            vecOfThreads.push_back(std::move(t));
        }
        for (auto &th : vecOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse();
        double cpuElapsed = timer.GetCpuElapse();
        std::cout << threadNum << " wallElapsed time: " << wallElapsed << "s" << std::endl;
        std::cout << threadNum << " cpuElapsed time: " << cpuElapsed / processorCount * t_ << "s" << std::endl;
        std::this_thread::sleep_for(100ms);
    }
    delete reinterpret_cast<omniruntime::op::HashAggregationOperatorFactory *>(factoryObjAddr);
    delete[] rowCount;
    delete input;
}
#endif

TEST(AggregationOperatorTest, min_max_varchar)
{
    int32_t rowCount = 6;
    std::string data0[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com", "*", "Aaron.Anderson@0CQ4QUkBY2Q.edu",
                           "Zulema.Ruiz@J2XvbX7.com", "*", "Aaron.Artis@bv.org"};
    std::string data1[] = {"*", "Zulma.Carter@MfvjVN43Udd95KeZ.com", "Aaron.Anderson@0CQ4QUkBY2Q.edu", "*",
                           "Zulema.Ruiz@J2XvbX7.com", "Aaron.Artis@bv.org"};
    std::string_view dataV0[rowCount];
    std::string_view dataV1[rowCount];
    for (int i = 0; i < rowCount; ++i) {
        dataV0[i] = std::string_view(data0[i].c_str(), data0[i].size());
        dataV1[i] = std::string_view(data1[i].c_str(), data1[i].size());
    }

    std::vector<DataTypePtr> types = std::vector<DataTypePtr> { VarcharType(100), VarcharType(100) };
    auto vector1 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    auto vector2 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int32_t i = 0; i < rowCount; i++) {
        if (i == 1 || i == 4) {
            vector1->SetNull(i);
        } else {
            vector1->SetValue(i, dataV0[i]);
        }
        if (i == 0 || i == 3) {
            vector2->SetNull(i);
        } else {
            vector2->SetValue(i, dataV1[i]);
        }
    }

    auto input = new VectorBatch(6);
    input->Append(vector1);
    input->Append(vector2);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX };

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { VarcharType(100), VarcharType(100) };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1 }), types,
        partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { VarcharType(100), VarcharType(100) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    vecBatchCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);
    auto resultVec0 = static_cast<Vector<LargeStringContainer<std::string_view>> *>(finalOutputVecBatch->Get(0));
    EXPECT_EQ(resultVec0->GetSize(), 1);
    std::string_view minVal = resultVec0->GetValue(0);
    std::string minStr(minVal.data(), minVal.data() + minVal.size());
    EXPECT_EQ(minVal.size(), data0[2].size());
    EXPECT_EQ(data0[2].compare(minStr), 0);

    auto resultVec1 = static_cast<Vector<LargeStringContainer<std::string_view>> *>(finalOutputVecBatch->Get(1));
    EXPECT_EQ(resultVec1->GetSize(), 1);
    std::string_view maxVal = resultVec1->GetValue(0);
    std::string maxStr(maxVal.data(), maxVal.data() + maxVal.size());
    EXPECT_EQ(maxVal.size(), data1[1].size());
    EXPECT_EQ(data1[1].compare(maxStr), 0);

    omniruntime::op::Operator::DeleteOperator(aggFinal);
    omniruntime::op::Operator::DeleteOperator(aggPartial);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
}

TEST(AggregationOperatorTest, min_max_varchar_without_nulls)
{
    int32_t rowCount = 6;
    std::string data0[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com", "*", "Aaron.Anderson@0CQ4QUkBY2Q.edu",
                           "Zulema.Ruiz@J2XvbX7.com", "**", "Aaron.Artis@bv.org"};
    std::string data1[] = {"**", "Zulma.Carter@MfvjVN43Udd95KeZ.com", "Aaron.Anderson@0CQ4QUkBY2Q.edu", "*",
                           "Zulema.Ruiz@J2XvbX7.com", "Aaron.Artis@bv.org"};
    std::string_view dataV0[rowCount];
    std::string_view dataV1[rowCount];
    for (int i = 0; i < rowCount; ++i) {
        dataV0[i] = std::string_view(data0[i].c_str(), data0[i].size());
        dataV1[i] = std::string_view(data1[i].c_str(), data1[i].size());
    }

    std::vector<DataTypePtr> types = std::vector<DataTypePtr> { VarcharType(100), VarcharType(100) };
    auto vector1 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    auto vector2 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int32_t i = 0; i < rowCount; i++) {
        vector1->SetValue(i, dataV0[i]);
        vector2->SetValue(i, dataV1[i]);
    }

    auto input = new VectorBatch(6);
    input->Append(vector1);
    input->Append(vector2);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX };

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { VarcharType(100), VarcharType(100) };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1 }), types,
        partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { VarcharType(100), VarcharType(100) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    vecBatchCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);
    auto resultVec0 = static_cast<Vector<LargeStringContainer<std::string_view>> *>(finalOutputVecBatch->Get(0));
    EXPECT_EQ(resultVec0->GetSize(), 1);
    std::string_view minVal = resultVec0->GetValue(0);
    std::string minStr(minVal.data(), minVal.data() + minVal.size());
    EXPECT_EQ(minVal.size(), data0[1].size());
    EXPECT_EQ(data0[1].compare(minStr), 0);

    auto resultVec1 = static_cast<Vector<LargeStringContainer<std::string_view>> *>(finalOutputVecBatch->Get(1));
    EXPECT_EQ(resultVec1->GetSize(), 1);
    std::string_view maxVal = resultVec1->GetValue(0);
    std::string maxStr(maxVal.data(), maxVal.data() + maxVal.size());
    EXPECT_EQ(maxVal.size(), data1[1].size());
    EXPECT_EQ(data1[1].compare(maxStr), 0);

    omniruntime::op::Operator::DeleteOperator(aggFinal);
    omniruntime::op::Operator::DeleteOperator(aggPartial);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
}

TEST(AggregationOperatorTest, min_max_varchar_mix_withnull)
{
    int32_t rowCount = 6;
    std::string data1[] = {"", "", "", "", "", ""};
    std::string data2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com", "*", "", "Zulema.Ruiz@J2XvbX7.com", "*", ""};
    std::string data3[] = {"", "", "", "", "", ""};
    std::string data4[] = {"*", "", "Aaron.Anderson@0CQ4QUkBY2Q.edu", "", "Zulema.R", "Aaron.Artis@bv.org"};
    std::string_view dataV1[rowCount];
    std::string_view dataV2[rowCount];
    std::string_view dataV3[rowCount];
    std::string_view dataV4[rowCount];
    for (int i = 0; i < rowCount; ++i) {
        dataV1[i] = std::string_view(data1[i].c_str(), data1[i].size());
        dataV2[i] = std::string_view(data2[i].c_str(), data2[i].size());
        dataV3[i] = std::string_view(data3[i].c_str(), data3[i].size());
        dataV4[i] = std::string_view(data4[i].c_str(), data4[i].size());
    }

    std::vector<DataTypePtr> types =
        std::vector<DataTypePtr> { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto vector1 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    auto vector2 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    auto vector3 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    auto vector4 = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int32_t i = 0; i < rowCount; i++) {
        if (i == 4 || i == 5) {
            vector1->SetNull(i);
            vector2->SetNull(i);
        } else {
            vector1->SetValue(i, dataV1[i]);
            vector2->SetValue(i, dataV2[i]);
        }
        if (i == 0 || i == 1) {
            vector3->SetNull(i);
            vector4->SetNull(i);
        } else {
            vector3->SetValue(i, dataV3[i]);
            vector4->SetValue(i, dataV4[i]);
        }
    }

    auto input = new VectorBatch(6);
    input->Append(vector1);
    input->Append(vector2);
    input->Append(vector3);
    input->Append(vector4);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN };

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        types, partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    vecBatchCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    std::string expectData1[] = {""};
    std::string expectData2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com"};
    std::string expectData3[] = {""};
    std::string expectData4[] = {""};
    std::vector<DataTypePtr> outputTypes = { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    DataTypes expectTypes(outputTypes);
    auto expectVecBatch = CreateVectorBatch(expectTypes, 1, expectData1, expectData2, expectData3, expectData4);
    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expectVecBatch));

    omniruntime::op::Operator::DeleteOperator(aggPartial);
    omniruntime::op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(AggregationOperatorTest, min_max_varchar_mix_withoutnull)
{
    int32_t rowCount = 6;
    std::string data1[] = {"", "", "", "", "", ""};
    std::string data2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com", "*", "", "Zulema.Ruiz@J2XvbX7.com", "*", ""};
    std::string data3[] = {"", "", "", "", "", ""};
    std::string data4[] = {"Aaron.Anderson@0CQ4QUkBY2Q.edu", "Zulema.R", "Aaron.Artis@bv.org", "", "", "*"};
    std::vector<DataTypePtr> types =
        std::vector<DataTypePtr> { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    DataTypes inputTypes(types);
    auto input = CreateVectorBatch(inputTypes, rowCount, data1, data2, data3, data4);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN };

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        types, partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    vecBatchCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    std::string expectData1[] = {""};
    std::string expectData2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com"};
    std::string expectData3[] = {""};
    std::string expectData4[] = {""};
    std::vector<DataTypePtr> outputTypes = { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    DataTypes expectTypes(outputTypes);
    auto expectVecBatch = CreateVectorBatch(expectTypes, 1, expectData1, expectData2, expectData3, expectData4);
    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expectVecBatch));

    omniruntime::op::Operator::DeleteOperator(aggPartial);
    omniruntime::op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(AggregationOperatorTest, min_max_varchar_dict_withnull)
{
    int32_t rowCount = 6;
    std::string data1[] = {"", "", "", "", "", ""};
    std::string data2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com", "*", "", "Zulema.Ruiz@J2XvbX7.com", "*", ""};
    std::string data3[] = {"", "", "", "", "", ""};
    std::string data4[] = {"*", "", "Aaron.Anderson@0CQ4QUkBY2Q.edu", "", "Zulema.R", "Aaron.Artis@bv.org"};
    std::string_view dataV1[rowCount];
    std::string_view dataV2[rowCount];
    std::string_view dataV3[rowCount];
    std::string_view dataV4[rowCount];
    for (int i = 0; i < rowCount; ++i) {
        dataV1[i] = std::string_view(data1[i].c_str(), data1[i].size());
        dataV2[i] = std::string_view(data2[i].c_str(), data2[i].size());
        dataV3[i] = std::string_view(data3[i].c_str(), data3[i].size());
        dataV4[i] = std::string_view(data4[i].c_str(), data4[i].size());
    }

    std::vector<DataTypePtr> types =
        std::vector<DataTypePtr> { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto vector1 = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(rowCount);
    auto vector2 = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(rowCount);
    auto vector3 = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(rowCount);
    auto vector4 = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(rowCount);
    for (int32_t i = 0; i < rowCount; i++) {
        if (i == 4 || i == 5) {
            vector1->SetNull(i);
            vector2->SetNull(i);
        } else {
            vector1->SetValue(i, dataV1[i]);
            vector2->SetValue(i, dataV2[i]);
        }
        if (i == 0 || i == 1) {
            vector3->SetNull(i);
            vector4->SetNull(i);
        } else {
            vector3->SetValue(i, dataV3[i]);
            vector4->SetValue(i, dataV4[i]);
        }
    }

    std::vector<int32_t> ids = { 0, 1, 2, 3, 4, 5 };
    auto dicVec1 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(), vector1.get());
    auto dicVec2 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(), vector2.get());
    auto dicVec3 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(), vector3.get());
    auto dicVec4 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(), vector4.get());

    auto input = new VectorBatch(6);
    input->Append(dicVec1);
    input->Append(dicVec2);
    input->Append(dicVec3);
    input->Append(dicVec4);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN };

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        types, partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    vecBatchCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    std::string expectData1[] = {""};
    std::string expectData2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com"};
    std::string expectData3[] = {""};
    std::string expectData4[] = {""};
    std::vector<DataTypePtr> outputTypes = { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    DataTypes expectTypes(outputTypes);
    auto expectVecBatch = CreateVectorBatch(expectTypes, 1, expectData1, expectData2, expectData3, expectData4);
    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expectVecBatch));

    omniruntime::op::Operator::DeleteOperator(aggPartial);
    omniruntime::op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(AggregationOperatorTest, min_max_varchar_dict_withoutnull)
{
    int32_t rowCount = 6;
    std::string data1[] = {"", "", "", "", "", ""};
    std::string data2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com", "*", "", "Zulema.Ruiz@J2XvbX7.com", "*", ""};
    std::string data3[] = {"", "", "", "", "", ""};
    std::string data4[] = {"*", "", "Aaron.Anderson@0CQ4QUkBY2Q.edu", "", "Zulema.R", "Aaron.Artis@bv.org"};
    std::vector<DataTypePtr> types =
        std::vector<DataTypePtr> { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    DataTypes inputTypes(types);
    auto tmpInput = CreateVectorBatch(inputTypes, rowCount, data1, data2, data3, data4);

    std::vector<int32_t> ids = { 0, 1, 2, 3, 4, 5 };
    auto dicVec1 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(),
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(tmpInput->Get(0)));
    auto dicVec2 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(),
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(tmpInput->Get(1)));
    auto dicVec3 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(),
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(tmpInput->Get(2)));
    auto dicVec4 = VectorHelper::CreateStringDictionary(ids.data(), ids.size(),
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(tmpInput->Get(3)));
    VectorHelper::FreeVecBatch(tmpInput);

    auto input = new VectorBatch(6);
    input->Append(dicVec1);
    input->Append(dicVec2);
    input->Append(dicVec3);
    input->Append(dicVec4);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN };

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        types, partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    vecBatchCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    std::string expectData1[] = {""};
    std::string expectData2[] = {"Zulma.Carter@MfvjVN43Udd95KeZ.com"};
    std::string expectData3[] = {""};
    std::string expectData4[] = {""};
    std::vector<DataTypePtr> outputTypes = { VarcharType(30), VarcharType(30), VarcharType(30), VarcharType(30) };
    DataTypes expectTypes(outputTypes);
    auto expectVecBatch = CreateVectorBatch(expectTypes, 1, expectData1, expectData2, expectData3, expectData4);
    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expectVecBatch));

    omniruntime::op::Operator::DeleteOperator(aggPartial);
    omniruntime::op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(AggregationOperatorTest, sum_avg)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    const int32_t dataSize = 5;
    const int32_t resultDataSize = 1;

    int64_t data0[dataSize] = {3L, 10L, 2L, 7L, 3L};
    Decimal128 data1[dataSize] = {Decimal128(4000L, 0), Decimal128(2000L, 0), Decimal128(1000L, 0),
                                  Decimal128(3000L, 0), Decimal128(5000L, 0)};

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<DataTypePtr> aggTypes = { LongType(), Decimal128Type(20, 5), LongType(), Decimal128Type(20, 5) };
    DataTypes sourceTypes(aggTypes);
    VectorBatch *input = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data0, data1);
    ASSERT(!(input == nullptr));

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { LongType(), SUM_IMMEDIATE_VARBINARY,
        ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), AVG_IMMEDIATE_VARBINARY };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        aggTypes, partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);

    VectorBatch *outputVecBatch = nullptr;
    int32_t tableCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(tableCount, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { LongType(), Decimal128Type(20, 5), DoubleType(),
        Decimal128Type(20, 5) };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    tableCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(tableCount, 1);
    EXPECT_EQ(finalOutputVecBatch->GetRowCount(), 1);
    EXPECT_EQ(finalOutputVecBatch->GetVectorCount(), 4);

    int64_t expData0[resultDataSize] = {25L};
    Decimal128 expData1[resultDataSize] = {Decimal128(15000L, 0)};
    double expData2[resultDataSize] = {5.0};
    Decimal128 expData3[resultDataSize] = {Decimal128(3000L, 0)};
    std::vector<DataTypePtr> resultType = { LongType(), Decimal128Type(20, 5), DoubleType(), Decimal128Type(20, 5) };
    DataTypes resultTypes(resultType);
    VectorBatch *expVecBatch = CreateVectorBatch(resultTypes, resultDataSize, expData0, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expVecBatch));

    omniruntime::op::Operator::DeleteOperator(aggFinal);
    omniruntime::op::Operator::DeleteOperator(aggPartial);
    VectorHelper::FreeVecBatch(expVecBatch);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
}

TEST(AggregationOperatorTest, decimal128)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    const int32_t dataSize = 5;
    const int32_t resultDataSize = 1;

    std::string value0[5] = {"1234567890123456789012345678901234567", "-55", "0",
                             "9999999999999999999999999999999999999", "-9999999999999999999999999999999999999"};
    std::string value1[5] = {"1234567890123456789012", "-9223372036854775808", "0", "9999999999999999999999",
                             "-9999999999999999999999"};

    Decimal128 data0[dataSize] = {Decimal128(value0[0]), Decimal128(value0[1]), Decimal128(value0[2]),
                                  Decimal128(value0[3]), Decimal128(value0[4])};
    Decimal128 data1[dataSize] = {Decimal128(value1[0]), Decimal128(value1[1]), Decimal128(value1[2]),
                                  Decimal128(value1[3]), Decimal128(value1[4])};

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<DataTypePtr> groupTypes;
    std::vector<DataTypePtr> aggTypes = { Decimal128Type(38, 0), Decimal128Type(38, 0), Decimal128Type(38, 0),
        Decimal128Type(38, 0), Decimal128Type(38, 0), Decimal128Type(38, 0),
        Decimal128Type(38, 0), Decimal128Type(38, 0) };
    DataTypes sourceTypes(aggTypes);
    VectorBatch *input =
        CreateVectorBatch(sourceTypes, dataSize, data0, data1, data0, data1, data0, data1, data0, data1);

    ASSERT(!(input == nullptr));

    std::vector<DataTypePtr> partialOutputTypes { Decimal128Type(38, 0),   Decimal128Type(38, 0),
        Decimal128Type(38, 0),   Decimal128Type(38, 0),
        SUM_IMMEDIATE_VARBINARY, SUM_IMMEDIATE_VARBINARY,
        AVG_IMMEDIATE_VARBINARY, AVG_IMMEDIATE_VARBINARY };

    auto aggPartialFactory =
        CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3, 4, 5, 6, 7 }), aggTypes,
        partialOutputTypes, std::vector<uint32_t>(), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    int32_t tableCount = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(tableCount, 1);

    // STAGE2:
    std::vector<DataTypePtr> finalOutputTypes{ Decimal128Type(38, 0), Decimal128Type(38, 0), Decimal128Type(38, 0),
        Decimal128Type(38, 0), Decimal128Type(38, 0), Decimal128Type(38, 0),
        Decimal128Type(38, 0), Decimal128Type(38, 0) };
    auto aggFinalFactory =
        CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3, 4, 5, 6, 7 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);
    VectorBatch *finalOutputVecBatch = nullptr;
    tableCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(tableCount, 1);
    EXPECT_EQ(finalOutputVecBatch->GetRowCount(), 1);
    EXPECT_EQ(finalOutputVecBatch->GetVectorCount(), 8);

    Decimal128 expData0[resultDataSize] = {Decimal128("-9999999999999999999999999999999999999")};
    Decimal128 expData1[resultDataSize] = {Decimal128("-9999999999999999999999")};
    Decimal128 expData2[resultDataSize] = {Decimal128("9999999999999999999999999999999999999")};
    Decimal128 expData3[resultDataSize] = {Decimal128("9999999999999999999999")};
    Decimal128 expData4[resultDataSize] = {Decimal128("1234567890123456789012345678901234512")};
    Decimal128 expData5[resultDataSize] = {Decimal128("1225344518086602013204")};
    Decimal128 expData6[resultDataSize] = {Decimal128("246913578024691357802469135780246902")};
    Decimal128 expData7[resultDataSize] = {Decimal128("245068903617320402641")};
    std::vector<DataTypePtr> resultType = { Decimal128Type(38, 0), Decimal128Type(38, 0), Decimal128Type(38, 0),
        Decimal128Type(38, 0), Decimal128Type(38, 0), Decimal128Type(38, 0),
        Decimal128Type(38, 0), Decimal128Type(38, 0) };
    DataTypes resultTypes(resultType);
    VectorBatch *expVecBatch = CreateVectorBatch(resultTypes, resultDataSize, expData0, expData1, expData2, expData3,
        expData4, expData5, expData6, expData7);

    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expVecBatch));

    omniruntime::op::Operator::DeleteOperator(aggFinal);
    omniruntime::op::Operator::DeleteOperator(aggPartial);
    VectorHelper::FreeVecBatch(expVecBatch);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
}

TEST(HashAggregationOperatorTest, group_by_agg_same_cols)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    // create 10 vecBatches
    const int vecBatchNum = 10;
    const int dataSize = 10;
    std::vector<VectorBatch *> input = ConstructSimpleBuildData(vecBatchNum, dataSize);

    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1, 2, 3, 4 }),
        std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), ShortType(), DoubleType() }),
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 0, 1 }), std::vector<DataTypePtr>({ LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), LongType() }), std::vector<uint32_t>(), true, false, false);
    auto groupBy = aggFactory->CreateOperator();
    groupBy->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupBy->AddInput(input[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    groupBy->GetOutput(&outputVecBatch);
    std::vector<DataTypePtr> expectedTypes { LongType(),   LongType(), IntType(), ShortType(),
        DoubleType(), LongType(), LongType() };
    VectorBatch *expected = ConstructSimpleBuildData();
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expected));
    op::Operator::DeleteOperator(groupBy);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expected);
}

TEST(HashAggregationOperatorTest, varchar_vector_correctness)
{
    // create 10 pages
    const int vecBatchNum = 1;
    const int cardinality = 10;
    const int rowSize = 2000;
    // groupby + count + min + max
    std::vector<DataTypePtr> groupTypes = { VarcharType(10) };
    std::vector<DataTypePtr> aggTypes = { VarcharType(10), VarcharType(10), VarcharType(10) };
    VectorBatch **input = BuildAggInput(vecBatchNum, rowSize, cardinality, 1, 3, groupTypes, aggTypes);

    std::vector<DataTypePtr> outputTypes { LongType(), VarcharType(10), VarcharType(10) };
    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }), groupTypes,
        std::vector<uint32_t>(
        { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX }),
        std::vector<uint32_t>({ 1, 2, 3 }), aggTypes, outputTypes, std::vector<uint32_t>(), true, false, false);
    auto groupByVarChar = aggFactory->CreateOperator();
    groupByVarChar->Init();

    for (int32_t i = 0; i < vecBatchNum; ++i) {
        groupByVarChar->AddInput(input[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupByVarChar->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    op::Operator::DeleteOperator(groupByVarChar);

    std::vector<DataTypePtr> expectFieldTypes { VarcharType(10), LongType(), VarcharType(10), VarcharType(10) };
    // construct the output data
    DataTypes expectTypes(expectFieldTypes);
    std::string  expectData1[cardinality] = {"9", "8", "7", "6", "5", "4", "3", "2", "1", "0"};
    int64_t expectData2[cardinality] = {200, 200, 200, 200, 200, 200, 200, 200, 200, 200};
    std::string  expectData3[cardinality] = {"1009", "1008", "1007", "1006", "1005", "1004", "1003", "1002", "1", "0"};
    std::string  expectData4[cardinality] = {"999", "998", "997", "996", "995", "994", "993", "992", "991", "990"};

    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, cardinality, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(AggregationOperatorTest, verify_correctness)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    // create 10 vecBatches
    const int vecBatchNum = 10;
    const int cardinality = 4;
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX };
    std::vector<DataTypePtr> groupTypes;
    std::vector<DataTypePtr> aggTypes = { LongType(), LongType(), LongType(), LongType(), LongType() };
    VectorBatch **input1 = BuildAggInput(vecBatchNum, ROW_PER_VEC_BATCH, cardinality, 0, 5, groupTypes, aggTypes);

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes { LongType(),
        ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), LongType(), LongType(), LongType() };
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3, 4 }),
        aggTypes, partialOutputTypes, std::vector<uint32_t>(), true, true, false);

    // partial first operator
    auto aggPartial1 = aggPartialFactory->CreateOperator();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial1->AddInput(input1[i]);
    }

    VectorBatch *outputVecBatch1 = nullptr;
    int32_t tableCount1 = aggPartial1->GetOutput(&outputVecBatch1);
    EXPECT_EQ(tableCount1, 1);
    omniruntime::op::Operator::DeleteOperator(aggPartial1);

    VectorBatch **input2 = BuildAggInput(vecBatchNum, ROW_PER_VEC_BATCH, cardinality, 0, 5, groupTypes, aggTypes);
    ASSERT(!(input2 == nullptr));

    // partial second operator
    auto aggPartial2 = aggPartialFactory->CreateOperator();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggPartial2->AddInput(input2[i]);
    }
    VectorBatch *outputVecBatch2 = nullptr;
    int32_t tableCount2 = aggPartial2->GetOutput(&outputVecBatch2);
    EXPECT_EQ(tableCount2, 1);
    omniruntime::op::Operator::DeleteOperator(aggPartial2);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { LongType(), DoubleType(), LongType(), LongType(), LongType() };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3, 4 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch1);
    aggFinal->AddInput(outputVecBatch2);

    VectorBatch *finalOutputVecBatch = nullptr;
    int32_t tableCount3 = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(tableCount3, 1);
    EXPECT_EQ(finalOutputVecBatch->GetRowCount(), 1);
    EXPECT_EQ(finalOutputVecBatch->GetVectorCount(), 5);

    delete[] input1;
    delete[] input2;
    omniruntime::op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
}

TEST(AggregationOperatorTest, verify_agg_distinct)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    // construct data
    const int32_t dataSize = 4;
    const int32_t resultDataSize = 1;

    // table1
    int64_t data0[dataSize] = {10L, 20L, 10L, 30L};
    int64_t data1[dataSize] = {1L, 1L, 2L, 3L};
    int64_t data2[dataSize] = {3L, 5L, 5L, 7L};
    int64_t data3[dataSize] = {4L, 2L, 1L, 2L};
    int64_t data4[dataSize] = {4L, 2L, 1L, 2L};
    bool data5[dataSize] = {true, true, false, true};
    bool data6[dataSize] = {true, false, true, true};
    bool data7[dataSize] = {true, true, false, true};
    bool data8[dataSize] = {true, true, true, false};
    bool data9[dataSize] = {true, true, true, false};

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MIN };
    std::vector<DataTypePtr> types = { LongType(),    LongType(),    LongType(),    LongType(),    LongType(),
        BooleanType(), BooleanType(), BooleanType(), BooleanType(), BooleanType() };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch1 =
        CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2, data3, data4, data5, data6, data7, data8, data9);

    // STAGE1: (partial)
    std::vector<DataTypePtr> partialOutputTypes({ LongType(), LongType(),
        ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), LongType(), LongType() });
    auto aggPartialFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3, 4 }),
        std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType(), LongType() }), partialOutputTypes,
        std::vector<uint32_t>({ 5, 6, 7, 8, 9 }), true, true, false);
    auto aggPartial = aggPartialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch1);

    VectorBatch *outputVecBatch = nullptr;
    int32_t tableCount1 = aggPartial->GetOutput(&outputVecBatch);
    EXPECT_EQ(tableCount1, 1);

    // STAGE2: (final)
    std::vector<DataTypePtr> finalOutputTypes { LongType(), LongType(), DoubleType(), LongType(), LongType() };
    auto aggFinalFactory = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({ 0, 1, 2, 3, 4 }),
        partialOutputTypes, finalOutputTypes, std::vector<uint32_t>(), false, false, false);
    auto aggFinal = aggFinalFactory->CreateOperator();
    aggFinal->AddInput(outputVecBatch);

    VectorBatch *finalOutputVecBatch = nullptr;
    int32_t tableCount = aggFinal->GetOutput(&finalOutputVecBatch);
    EXPECT_EQ(tableCount, 1);
    EXPECT_EQ(finalOutputVecBatch->GetRowCount(), 1);
    EXPECT_EQ(finalOutputVecBatch->GetVectorCount(), 5);

    int64_t expData0[resultDataSize] = {3L};
    int64_t expData1[resultDataSize] = {6L};
    double expData2[resultDataSize] = {5.0};
    int64_t expData3[resultDataSize] = {4L};
    int64_t expData4[resultDataSize] = {1L};
    std::vector<DataTypePtr> resultType = { LongType(), LongType(), DoubleType(), LongType(), LongType() };
    DataTypes resultTypes(resultType);
    VectorBatch *expVecBatch1 =
        CreateVectorBatch(resultTypes, resultDataSize, expData0, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatch(finalOutputVecBatch, expVecBatch1));

    omniruntime::op::Operator::DeleteOperator(aggPartial);
    omniruntime::op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(finalOutputVecBatch);
}

TEST(AggregationOperatorTest, avg_correctness_test)
{
    // create 10 pages
    const int vecBatchNum = 10;
    const int cardinality = 100;
    std::vector<DataTypePtr> groupTypes;
    std::vector<DataTypePtr> aggTypes = { LongType() };
    VectorBatch **input = BuildAggInput(vecBatchNum, ROW_PER_VEC_BATCH, cardinality, 0, 1, groupTypes, aggTypes);

    // single stage
    auto aggFactory = CreateAggregationOperatorFactory(std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_AVG }),
        std::vector<uint32_t>({ 0 }), aggTypes, std::vector<DataTypePtr>({ DoubleType() }), std::vector<uint32_t>(),
        true, false, false);
    auto aggregate = aggFactory->CreateOperator();
    for (int32_t i = 0; i < vecBatchNum; ++i) {
        aggregate->AddInput(input[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    int32_t tableCount = aggregate->GetOutput(&outputVecBatch);

    EXPECT_EQ(tableCount, 1);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 1);
    EXPECT_EQ(outputVecBatch->GetRowCount(), 1);

    delete[] input;
    VectorHelper::FreeVecBatch(outputVecBatch);
    op::Operator::DeleteOperator(aggregate);
}

TEST(AggregationOperatorTest, min_max_varchar_correctness)
{
    std::string data0[] = {"operators", "operator", "operators", "helloha", "hello", "helloha"};
    std::string data1[] = {"hello", "helloha", "hello", "operator", "operators", "operator"};
    std::vector<DataTypePtr> types = std::vector<DataTypePtr> { VarcharType(10), VarcharType(10) };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, 6, data0, data1);

    // single stage
    auto aggFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX }),
        std::vector<uint32_t>({ 0, 1 }), types, std::vector<DataTypePtr>({ VarcharType(10), VarcharType(10) }),
        std::vector<uint32_t>(), true, false, false);
    auto aggOperator = aggFactory->CreateOperator();
    aggOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = aggOperator->GetOutput(&outputVecBatch);
    EXPECT_EQ(vecBatchCount, 1);

    auto resultVec0 = static_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    EXPECT_EQ(resultVec0->GetSize(), 1);
    std::string_view minVal = resultVec0->GetValue(0);
    std::string minStr(minVal.data(), minVal.data() + minVal.size());
    EXPECT_EQ(minVal.size(), data0[4].size());
    EXPECT_EQ(data0[4].compare(minStr), 0);

    auto resultVec1 = static_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(1));
    EXPECT_EQ(resultVec1->GetSize(), 1);
    std::string_view maxVal = resultVec1->GetValue(0);
    std::string maxStr(maxVal.data(), maxVal.data() + maxVal.size());
    EXPECT_EQ(maxVal.size(), data1[4].size());
    EXPECT_EQ(data1[4].compare(maxStr), 0);

    omniruntime::op::Operator::DeleteOperator(aggOperator);
    VectorHelper::FreeVecBatch(outputVecBatch);
}
#ifdef DISABLE_TEST_NO_NEED_OCCUPY_BRANCH_TEST
TEST(AggregationOperatorTest, DISABLED_perf_original)
{
    std::vector<DataTypePtr> sourceFieldTypes { LongType(), LongType(), LongType(), LongType() };
    DataTypes sourceTypes(sourceFieldTypes);
    std::vector<DataTypePtr> outputTypes { LongType(), LongType(), LongType(), LongType() };
    DataTypes aggOutputTypes(outputTypes);
    FunctionType aggFunType[] = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM};
    uint32_t aggInputCols[] = {0, 1, 2, 3};
    std::vector<uint32_t> aggFuncTypesVector =
        std::vector<uint32_t>(reinterpret_cast<uint32_t *>(aggFunType), reinterpret_cast<uint32_t *>(aggFunType) + 4);
    std::vector<uint32_t> aggInputColsVector = std::vector<uint32_t>(aggInputCols, aggInputCols + 4);
    uint32_t maskCols[4] = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
                            static_cast<uint32_t>(-1) };
    std::vector<uint32_t> maskColsVector = std::vector<uint32_t>(maskCols, maskCols + 4);

    auto aggInputColsVectorWrap = AggregatorUtil::WrapWithVector(aggInputColsVector);
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = AggregatorUtil::WrapWithVector(true, aggFuncTypesVector.size());
    auto outputPartialWrap = AggregatorUtil::WrapWithVector(false, aggFuncTypesVector.size());
    auto nativeOperatorFactory = new AggregationOperatorFactory(sourceTypes, aggFuncTypesVector, aggInputColsVectorWrap,
        maskColsVector, aggOutputTypesWrap, inputRawWrap, outputPartialWrap);
    nativeOperatorFactory->Init();
    int64_t factoryAddr = reinterpret_cast<int64_t>(nativeOperatorFactory);
    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;

    VectorBatch **input = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 0, COLUMN_NUM,
        std::vector<DataTypePtr>(), sourceTypes.Get());

    int32_t *rowCount = new int32_t[VEC_BATCH_NUM];
    for (int32_t i = 0; i < VEC_BATCH_NUM; i++) {
        rowCount[i] = ROW_PER_VEC_BATCH;
    }
    std::cout << "after prepare" << std::endl;
    uint32_t threadNums[] = {1, 2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(uint32_t); ++i) {
        g_totalWallTime = 0;
        g_totalCpuTime = 0;
        auto t_ = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;

        uint32_t threadNum = threadNums[i];
        std::vector<std::thread> vecOfThreads;
        Timer timer;
        timer.SetStart();
        for (uint32_t j = 0; j < threadNum; ++j) {
            // same stage Id
            std::thread t(PerfTestNonGroup, factoryAddr, false, input, VEC_BATCH_NUM, rowCount,
                std::vector<DataTypePtr> { LongType(), LongType(), LongType(), LongType() });
            vecOfThreads.push_back(std::move(t));
        }
        for (auto &th : vecOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse();
        double cpuElapsed = timer.GetCpuElapse();
        std::cout << threadNum << " wallElapsed time: " << wallElapsed << "s" << std::endl;
        std::cout << threadNum << " cpuElapsed time: " << cpuElapsed / processorCount * t_ << "s" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    delete[] rowCount;
    delete nativeOperatorFactory;
    delete input;
}

TEST(AggregationOperatorTest, DISABLED_perf_codegen)
{
    using namespace std;

    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;

    std::vector<DataTypePtr> groupTypes;
    std::vector<DataTypePtr> aggTypes = { LongType(), LongType(), LongType(), LongType() };
    VectorBatch **input = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 0, 4, groupTypes, aggTypes);

    int32_t *rowCount = new int32_t[VEC_BATCH_NUM];
    for (int32_t i = 0; i < VEC_BATCH_NUM; i++) {
        rowCount[i] = ROW_PER_VEC_BATCH;
    }
    uint64_t factoryObjAddr = CreateAggFactoryWithJit();
    std::cout << "after prepare" << std::endl;
    uint32_t threadNums[] = {1, 2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(uint32_t); ++i) {
        g_totalWallTime = 0;
        g_totalCpuTime = 0;
        auto t_ = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;

        uint32_t threadNum = threadNums[i];
        std::vector<std::thread> vecOfThreads;
        Timer timer;
        timer.SetStart();
        for (uint32_t j = 0; j < threadNum; ++j) {
            // same stage Id
            std::thread t(PerfTestNonGroup, factoryObjAddr, true, input, VEC_BATCH_NUM, rowCount,
                std::vector<DataTypePtr> { LongType(), LongType(), LongType(), LongType() });
            vecOfThreads.push_back(std::move(t));
        }
        for (auto &th : vecOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse();
        double cpuElapsed = timer.GetCpuElapse();
        std::cout << threadNum << " wallElapsed time: " << wallElapsed << "s" << std::endl;
        std::cout << threadNum << " cpuElapsed time: " << cpuElapsed / processorCount * t_ << "s" << std::endl;
        std::this_thread::sleep_for(100ms);
    }
    delete[] rowCount;
    delete input;
}
#endif

TEST(HashAggregationOperatorTest, compare_perf)
{
    uint32_t groupCols[] = {0, 1};
    std::vector<DataTypePtr> groupInputFieldTypes { LongType(), LongType() };
    DataTypes groupInputTypes(groupInputFieldTypes);
    uint32_t aggCols[] = {2, 3};
    std::vector<DataTypePtr> aggInputFieldTypes { LongType(), LongType() };
    DataTypes aggInputTypes(aggInputFieldTypes);
    std::vector<DataTypePtr> aggOutputFieldTypes { LongType(), LongType() };
    DataTypes aggOutputTypes(aggOutputFieldTypes);
    FunctionType aggFunType[] = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
    uint32_t maskCols[] = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};
    std::vector<uint32_t> groupByColVector = std::vector<uint32_t>(groupCols, groupCols + 2);
    std::vector<uint32_t> aggColVector = std::vector<uint32_t>(aggCols, aggCols + 2);
    std::vector<uint32_t> aggFuncTypeVector =
        std::vector<uint32_t>(reinterpret_cast<uint32_t *>(aggFunType), reinterpret_cast<uint32_t *>(aggFunType) + 2);
    std::vector<uint32_t> maskColsVector =
        std::vector<uint32_t>(reinterpret_cast<uint32_t *>(maskCols), reinterpret_cast<uint32_t *>(maskCols) + 2);

    // ------------------------------------------Create operator--------------------------------------------
    std::cout << "after JIT" << std::endl;

    auto aggColVectorWrap = AggregatorUtil::WrapWithVector(aggColVector);
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawsWrap = std::vector<bool>(aggFuncTypeVector.size(), true);
    auto outputPartialsWrap = std::vector<bool>(aggFuncTypeVector.size(), false);

    omniruntime::op::HashAggregationOperatorFactory *nativeOperatorFactory =
        new omniruntime::op::HashAggregationOperatorFactory(groupByColVector, groupInputTypes, aggColVectorWrap,
        aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypeVector, maskColsVector, inputRawsWrap, outputPartialsWrap);
    nativeOperatorFactory->Init();
    std::cout << "after create factory" << std::endl;
    // create operator
    auto jitGroupBy = CreateTestOperator(nativeOperatorFactory);

    // ------------------------------------------Process Input--------------------------------------------
    VectorBatch **input1 =
        BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 2, groupInputTypes.Get(), aggInputTypes.Get());

    Timer timer;
    timer.SetStart();

    auto *perfUtil = new PerfUtil();
    perfUtil->Init();
    perfUtil->Reset();
    perfUtil->Start();

    for (int pageIndex = 0; pageIndex < VEC_BATCH_NUM; ++pageIndex) {
        auto errNo = jitGroupBy->AddInput(input1[pageIndex]);
        EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);
    }

    perfUtil->Stop();
    long instCount = perfUtil->GetData();
    if (instCount != -1) {
        printf("HashAgg with OmniJit, used %lld instructions\n", perfUtil->GetData());
    }
    VectorBatch *jittedResult = nullptr;
    jitGroupBy->GetOutput(&jittedResult);

    timer.CalculateElapse();
    double wallElapsed = timer.GetWallElapse();
    double cpuElapsed = timer.GetCpuElapse();
    std::cout << "HashAgg with OmniJit, wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;

    auto aggColVector2Wrap = AggregatorUtil::WrapWithVector(aggColVector);
    auto aggInputTypes2Wrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    auto aggOutputTypes2Wrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRaw2Wrap = std::vector<bool>(aggFuncTypeVector.size(), true);
    auto outputPartial2Wrap = std::vector<bool>(aggFuncTypeVector.size(), false);
    HashAggregationOperatorFactory *nativeOperatorFactory2 =
        new HashAggregationOperatorFactory(groupByColVector, groupInputTypes, aggColVector2Wrap, aggInputTypes2Wrap,
        aggOutputTypes2Wrap, aggFuncTypeVector, maskColsVector, inputRaw2Wrap, outputPartial2Wrap);
    nativeOperatorFactory2->Init();
    std::cout << "after create factory" << std::endl;
    auto groupBy = nativeOperatorFactory2->CreateOperator();

    VectorBatch **input2 =
        BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 2, groupInputTypes.Get(), aggInputTypes.Get());
    if (input2 == nullptr) {
        return;
    }
    timer.Reset();

    perfUtil->Reset();
    perfUtil->Start();
    for (int pageIndex = 0; pageIndex < VEC_BATCH_NUM; ++pageIndex) {
        auto errNo = groupBy->AddInput(input2[pageIndex]);
        EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);
    }

    perfUtil->Stop();
    instCount = perfUtil->GetData();
    if (instCount != -1) {
        printf("HashAgg without OmniJit, used %lld instructions\n", perfUtil->GetData());
    }

    VectorBatch *outputVecBatch = nullptr;
    groupBy->GetOutput(&outputVecBatch);

    timer.CalculateElapse();
    wallElapsed = timer.GetWallElapse();
    cpuElapsed = timer.GetCpuElapse();
    std::cout << "HashAgg without OmniJit, wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;

    delete perfUtil;
    delete[] input1;
    delete[] input2;
    op::Operator::DeleteOperator(jitGroupBy);
    op::Operator::DeleteOperator(groupBy);
    delete nativeOperatorFactory;
    delete nativeOperatorFactory2;

    EXPECT_TRUE(VecBatchMatch(jittedResult, outputVecBatch));
    VectorHelper::FreeVecBatch(jittedResult);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationOperatorTest, multi_stage)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    std::vector<DataTypePtr> groupTypes = { LongType(), LongType() };
    std::vector<DataTypePtr> aggTypes = { LongType(), LongType(), Decimal64Type(7, 2), Decimal64Type(7, 2) };
    VectorBatch **input1 = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 4, groupTypes, aggTypes);
    HAFactoryParameters parameters1 = {
        true,
        true,
        { 0, 1 },
        { LongType(), LongType() },
        { 2, 3, 4, 5 },
        { LongType(), LongType(), SHORT_DECIMAL_TYPE, SHORT_DECIMAL_TYPE },
        { LongType(), ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), SUM_IMMEDIATE_VARBINARY,
        AVG_IMMEDIATE_VARBINARY },
        { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG },
        { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) },
    };

    uintptr_t partialFactoryAddr1 = CreateHashFactoryWithoutJit(parameters1);
    auto partialFactory1 = reinterpret_cast<omniruntime::op::HashAggregationOperatorFactory *>(partialFactoryAddr1);
    auto partialOperator1 = partialFactory1->CreateOperator();
    for (int32_t i = 0; i < VEC_BATCH_NUM; ++i) {
        partialOperator1->AddInput(input1[i]);
    }
    VectorBatch *resultFromPartial1 = nullptr;
    partialOperator1->GetOutput(&resultFromPartial1);

    VectorBatch **input2 = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 4, groupTypes, aggTypes);
    if (input2 == nullptr) {
        return;
    }

    HAFactoryParameters parameters2 = { true,
        true,
        { 0, 1 },
        { LongType(), LongType() },
        { 2, 3, 4, 5 },
        { LongType(), LongType(), SHORT_DECIMAL_TYPE, SHORT_DECIMAL_TYPE },
        { LongType(), ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), SUM_IMMEDIATE_VARBINARY,
        AVG_IMMEDIATE_VARBINARY },
        { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG },
        { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1) } };

    uintptr_t partialFactoryAddr2 = CreateHashFactoryWithoutJit(parameters2);
    auto partialFactory2 = reinterpret_cast<omniruntime::op::HashAggregationOperatorFactory *>(partialFactoryAddr2);
    auto partialOperator2 = partialFactory2->CreateOperator();
    for (int32_t i = 0; i < VEC_BATCH_NUM; ++i) {
        partialOperator2->AddInput(input2[i]);
    }
    VectorBatch *resultFromPartial2 = nullptr;
    partialOperator2->GetOutput(&resultFromPartial2);

    HAFactoryParameters parameters3 = {
        false,
        false,
        { 0, 1 },
        { LongType(), LongType() },
        { 2, 3, 4, 5 },
        { LongType(), ContainerType(std::vector<DataTypePtr> { DoubleType(), LongType() }), SUM_IMMEDIATE_VARBINARY,
        AVG_IMMEDIATE_VARBINARY },
        { LongType(), DoubleType(), LONG_DECIMAL_TYPE, SHORT_DECIMAL_TYPE },
        { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG },
        { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) },
    };

    uintptr_t finalFactoryAddr = CreateHashFactoryWithoutJit(parameters3);
    auto finalFactory = reinterpret_cast<omniruntime::op::HashAggregationOperatorFactory *>(finalFactoryAddr);
    auto operator2 = finalFactory->CreateOperator();
    operator2->AddInput(resultFromPartial1);
    operator2->AddInput(resultFromPartial2);
    VectorBatch *resultFromFinal = nullptr;
    operator2->GetOutput(&resultFromFinal);

    op::Operator::DeleteOperator(partialOperator1);
    op::Operator::DeleteOperator(partialOperator2);
    op::Operator::DeleteOperator(operator2);
    delete partialFactory1;
    delete partialFactory2;
    delete finalFactory;

    // construct the output data
    std::vector<DataTypePtr> expectFieldTypes { LongType(),   LongType(),        LongType(),
        DoubleType(), LONG_DECIMAL_TYPE, LongType() };
    DataTypes expectTypes(expectFieldTypes);
    int64_t expectData1[CARDINALITY] = {0, 1, 2, 3};
    int64_t expectData2[CARDINALITY] = {0, 1, 2, 3};
    int64_t expectData3[CARDINALITY] = {1000000, 1000000, 1000000, 1000000};
    double expectData4[CARDINALITY] = {1.0, 1.0, 1.0, 1.0};
    Decimal128 expectedDecimal(1000000L);
    Decimal128 expectData5[CARDINALITY] = {expectedDecimal, expectedDecimal, expectedDecimal, expectedDecimal};
    int64_t expectData6[CARDINALITY] = {1, 1, 1, 1};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, CARDINALITY, expectData1, expectData2, expectData3,
        expectData4, expectData5, expectData6);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFromFinal, expectVecBatch));

    delete[] input1;
    delete[] input2;
    VectorHelper::FreeVecBatch(resultFromFinal);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, supported_type_test)
{
    std::vector<DataTypePtr> groupTypes = { LongType(), LongType() };
    std::vector<DataTypePtr> aggTypes = { LongType(), LongType() };
    VectorBatch **input = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 2, groupTypes, aggTypes);

    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1 }),
        std::vector<DataTypePtr>({ LongType(), LongType() }),
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 2, 3 }), std::vector<DataTypePtr>({ LongType(), LongType() }),
        std::vector<DataTypePtr>({ LongType(), DoubleType() }), std::vector<uint32_t>(), true, false, false);
    auto groupBy = aggFactory->CreateOperator();
    groupBy->Init();

    for (int32_t i = 0; i < VEC_BATCH_NUM; ++i) {
        groupBy->AddInput(input[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    int32_t vecBatchCount = groupBy->GetOutput(&outputVecBatch);
    ASSERT_EQ(vecBatchCount, 1);
    op::Operator::DeleteOperator(groupBy);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] input;

    groupTypes[0] = Date32Type();
    groupTypes[1] = Date32Type();
    aggTypes[0] = Date32Type();
    aggTypes[1] = Date32Type();
    input = BuildAggInput(VEC_BATCH_NUM, ROW_PER_VEC_BATCH, CARDINALITY, 2, 2, groupTypes, aggTypes);
    aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0, 1 }),
        std::vector<DataTypePtr>({ Date32DataType::Instance(), Date32DataType::Instance() }),
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 2, 3 }),
        std::vector<DataTypePtr>({ Date32DataType::Instance(), Date32DataType::Instance() }),
        std::vector<DataTypePtr>({ Date32DataType::Instance(), Date32DataType::Instance() }), std::vector<uint32_t>(),
        true, false, false);
    groupBy = aggFactory->CreateOperator();
    groupBy->Init();

    for (int32_t i = 0; i < VEC_BATCH_NUM; ++i) {
        groupBy->AddInput(input[i]);
    }

    vecBatchCount = groupBy->GetOutput(&outputVecBatch);
    op::Operator::DeleteOperator(groupBy);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] input;

    // dictionary test
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, 1, 2, 0, 1, 2};
    int64_t data1[dataSize] = {0, 1, 2, 3, 4, 5};
    void *datas[2] = {data0, data1};
    std::vector<DataTypePtr> sourceFieldTypes { IntType(), LongType() };
    DataTypes sourceTypes(sourceFieldTypes);
    int32_t ids[] = {0, 1, 2, 3, 4, 5};
    VectorBatch *vectorBatch = new VectorBatch(dataSize);
    for (int32_t i = 0; i < 2; i++) {
        DataTypePtr dataType = sourceTypes.GetType(i);
        vectorBatch->Append(CreateDictionaryVector(*dataType, dataSize, ids, dataSize, datas[i]));
    }

    aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ IntType() }), std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 1 }), std::vector<DataTypePtr>({ LongType() }),
        std::vector<DataTypePtr>({ LongType() }), std::vector<uint32_t>(), true, false, false);
    groupBy = aggFactory->CreateOperator();
    groupBy->Init();

    for (int32_t i = 0; i < VEC_BATCH_NUM; ++i) {
        // create a copy of input
        VectorBatch *copied = DuplicateVectorBatch(vectorBatch);
        groupBy->AddInput(copied);
    }

    vecBatchCount = groupBy->GetOutput(&outputVecBatch);
    op::Operator::DeleteOperator(groupBy);
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationOperatorTest, TestSerializedMultiVecBatch)
{
    // construct input data
    const int32_t dataSize = 6;
    // prepare data
    int32_t data0[dataSize] = {0, -1, 127, 0, -1, 32767};
    int32_t data1[dataSize] = {-1, 127, 2147483647, 0, 32767, 2147483647};
    int64_t data2[dataSize] = {0, 1, 2, 3, 4, 5};
    int64_t data3[dataSize] = {7, 8, 9, 10, 11, 12};
    int *groupdatas[2] = {data0, data1};
    long *aggdatas[2] = {data2, data3};
    VectorBatch **input = new VectorBatch *[2];
    for (int32_t i = 0; i < 2; i++) {
        VectorBatch *vecBatch = new VectorBatch(dataSize);
        auto groupvec = new Vector<int>(dataSize);
        for (int k = 0; k < dataSize; ++k) {
            groupvec->SetValue(k, groupdatas[i][k]);
        }
        vecBatch->Append(groupvec);

        auto aggvec = new Vector<long>(dataSize);
        for (int k = 0; k < dataSize; ++k) {
            aggvec->SetValue(k, aggdatas[i][k]);
        }
        vecBatch->Append(aggvec);
        input[i] = vecBatch;
    }

    auto aggFactory = CreateHashAggregationOperatorFactory(std::vector<uint32_t>({ 0 }),
        std::vector<DataTypePtr>({ IntType() }), std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_SUM }),
        std::vector<uint32_t>({ 1 }), std::vector<DataTypePtr>({ LongType() }),
        std::vector<DataTypePtr>({ LongType() }), std::vector<uint32_t>(), true, false, false);
    auto groupBy = aggFactory->CreateOperator();
    groupBy->Init();

    for (int32_t i = 0; i < 2; ++i) {
        groupBy->AddInput(input[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    groupBy->GetOutput(&outputVecBatch);

    // construct the output data
    std::vector<DataTypePtr> expectFieldTypes { IntType(), LongType() };
    DataTypes expectTypes(expectFieldTypes);
    int32_t expectData1[5] = {0, -1, 127, 32767, 2147483647};
    int64_t expectData2[5] = {13, 12, 10, 16, 21};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, 5, expectData1, expectData2);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] input;
    op::Operator::DeleteOperator(groupBy);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
}

TEST(HashAggregationOperatorTest, test_hashagg_same_key_cross_spill_file)
{
    std::string data00[] = {"Asleep, high machines shall no", "Asleep, indian sciences may in",
        "As junior schools love simply.", "A", "Ab"};
    int64_t data01[] = {21194, 22342, 22901, 7205, 25033};
    int32_t data02[] = {11313, 10957, 11316, 11301, 11263};
    int32_t data03[] = {1, 1, 1, 1, 1};

    std::string data10[] = {"Asleep, high machines shall no", "Asleep, indian sciences may in",
        "As junior schools love simply.", "A", "Ab"};
    int64_t data11[] = {21194, 22342, 22900, 7205, 25033};
    int32_t data12[] = {11308, 11274, 10957, 11275, 11273};
    int32_t data13[] = {1, 1, 1, 1, 1};

    std::string data20[] = {"Asleep, high machines shall no", "Asleep, indian sciences may in",
        "As junior schools love simply.", "A", "Ab"};
    int64_t data21[] = {21194, 22342, 22901, 7205, 25033};
    int32_t data22[] = {11313, 11274, 11316, 11301, 11317};
    int32_t data23[] = {1, 1, 1, 1, 1};

    DataTypes dataTypes(
        std::vector<DataTypePtr>({ VarcharType(50), LongType(), Date32Type(DateUnit::DAY), IntType() }));
    VectorBatch *vecBatch0 = CreateVectorBatch(dataTypes, 5, data00, data01, data02, data03);
    VectorBatch *vecBatch1 = CreateVectorBatch(dataTypes, 5, data10, data11, data12, data13);
    VectorBatch *vecBatch2 = CreateVectorBatch(dataTypes, 5, data20, data21, data22, data23);

    std::vector<uint32_t> groupByCol({ 0, 1, 2 });
    DataTypes groupInputTypes(std::vector<DataTypePtr>({ VarcharType(50), LongType(), Date32Type(DateUnit::DAY) }));
    std::vector<uint32_t> aggInputCols({ 3 });
    auto aggInputColsWrap = AggregatorUtil::WrapWithVector(aggInputCols);
    DataTypes aggInputTypes(std::vector<DataTypePtr>({ IntType() }));
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN };
    std::vector<uint32_t> maskColsVector = { static_cast<uint32_t>(-1) };
    auto inputRaws = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartials = std::vector<bool>(aggFuncTypes.size(), true);
    SparkSpillConfig spillConfig(GenerateSpillPath(), INT32_MAX, 5);
    OperatorConfig operatorConfig(spillConfig);
    auto hashAggOperatorFactory = new HashAggregationOperatorFactory(groupByCol, groupInputTypes, aggInputColsWrap,
        aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypes, maskColsVector, inputRaws, outputPartials, operatorConfig);
    hashAggOperatorFactory->Init();
    auto *hashAggOperator = static_cast<HashAggregationOperator *>(hashAggOperatorFactory->CreateOperator());
    hashAggOperator->AddInput(vecBatch0);
    hashAggOperator->AddInput(vecBatch1);
    hashAggOperator->AddInput(vecBatch2);

    std::string expectData00[] = {"A", "A", "Ab", "Ab", "Ab", "As junior schools love simply.", "As junior schools love simply.",
        "Asleep, high machines shall no", "Asleep, high machines shall no", "Asleep, indian sciences may in", "Asleep, indian sciences may in"};
    int64_t expectData01[] = {7205, 7205, 25033, 25033, 25033, 22900, 22901, 21194, 21194, 22342, 22342};
    int32_t expectData02[] = {11275, 11301, 11273, 11317, 11263, 10957, 11316, 11308, 11313, 11274, 10957};
    int64_t expectData03[] = {1, 2, 1, 1, 1, 1, 2, 1, 2, 2, 1};

    DataTypes expectDataTypes(
        std::vector<DataTypePtr>({ VarcharType(50), LongType(), Date32Type(DateUnit::DAY), LongType() }));
    auto expectVecBatchR =
        CreateVectorBatch(expectDataTypes, 11, expectData00, expectData01, expectData02, expectData03);
    std::vector<VectorBatch *> result;
    while (hashAggOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        hashAggOperator->GetOutput(&outputVecBatch);
        result.emplace_back(outputVecBatch);
    }

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(result[0], expectVecBatchR));

    omniruntime::op::Operator::DeleteOperator(hashAggOperator);
    delete hashAggOperatorFactory;
    VectorHelper::FreeVecBatches(result);
    VectorHelper::FreeVecBatch(expectVecBatchR);
}

template <typename T> T *ExtractValueFromState(AggregateState *state)
{
    return reinterpret_cast<T *>(state);
}

// set offset 0
std::unique_ptr<AggregateState[]> NewAndInitState(Aggregator *agg, int32_t off = 0)
{
    auto state = std::make_unique<AggregateState[]>(agg->GetStateSize());
    agg->SetStateOffset(off);
    agg->InitState(state.get());
    return state;
}

TEST(AggregatorTest, try_sum_test)
{
    int32_t rowCnt=200;
    auto sumFactory=new TrySumSparkAggregatorFactory();
    std::vector<int32_t>channel0={0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumAgg = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    sumAgg->SetExecutionContext(executionContext.get());
    Vector<int64_t> *col = new Vector<int64_t>(rowCnt);
    int64_t expectVal=0;
    for (int32_t j = 0; j < rowCnt-1; ++j) {
        col->SetValue(j, j);
        expectVal+=j;
    }
    col->SetNull(rowCnt-1);
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(col);
    EXPECT_EQ(rowCnt, vecBatch->GetRowCount());

    auto state = NewAndInitState(sumAgg.get());
    sumAgg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);
    auto outputVector=VectorHelper::CreateVector(OMNI_FLAT, DataTypeId::OMNI_LONG, 1);
    std::vector<BaseVector *> extractVectors{outputVector};
    sumAgg->ExtractValues(state.get(), extractVectors, 0);
    EXPECT_EQ(expectVal, dynamic_cast<Vector<NativeType<DataTypeId::OMNI_LONG>::type> *>(extractVectors[0])->GetValue(0));
    delete outputVector;
    VectorHelper::FreeVecBatch(vecBatch);
    delete sumFactory;
}

TEST(AggregatorTest, try_sum_overflow_test)
{
    int32_t rowCnt=2;
    auto sumFactory=new TrySumSparkAggregatorFactory();
    std::vector<int32_t>channel0={0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumAgg = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
                                               *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    sumAgg->SetExecutionContext(executionContext.get());
    Vector<int64_t> *col = new Vector<int64_t>(rowCnt);
    for (int32_t j = 0; j < rowCnt; ++j) {
        col->SetValue(j, 9223372036854775807L);
    }
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(col);
    EXPECT_EQ(rowCnt, vecBatch->GetRowCount());

    auto state = NewAndInitState(sumAgg.get());
    sumAgg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);
    auto outputVector=VectorHelper::CreateVector(OMNI_FLAT, DataTypeId::OMNI_LONG, 1);
    std::vector<BaseVector *> extractVectors{outputVector};
    sumAgg->ExtractValues(state.get(), extractVectors, 0);
    EXPECT_TRUE(extractVectors[0]->IsNull(0));
    delete outputVector;
    VectorHelper::FreeVecBatch(vecBatch);
    delete sumFactory;
}

TEST(AggregatorTest, sum_test)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    int32_t rowPerVecBatch = 200;
    auto sumFactory = new SumAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    std::vector<int32_t> channal3 = { 3 };
    // sum test types : long + decimal + dictionary + null
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumLong = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, false, false);
    sumLong->SetExecutionContext(executionContext.get());
    auto sumShortDecimal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(SHORT_DECIMAL_TYPE).get()),
        *(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()), channal0, true, false, false);
    sumShortDecimal->SetExecutionContext(executionContext.get());
    auto sumNull = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal3, true, false, false);
    sumNull->SetExecutionContext(executionContext.get());

    auto longInputVec = BuildAggregateInput(LongType(), rowPerVecBatch);
    auto decimalInputVec = BuildAggregateInput(Decimal128Type(38, 2), rowPerVecBatch);
    LongDataType longDataType;
    int32_t ids[rowPerVecBatch];
    int64_t dict[rowPerVecBatch];
    for (int32_t i = 0; i < rowPerVecBatch; ++i) {
        ids[i] = 0;
        dict[i] = 1;
    }
    auto dictInputVec = CreateDictionaryVector(longDataType, rowPerVecBatch, ids, rowPerVecBatch, dict);
    auto nullInputVec = BuildAggregateInput(NoneType(), rowPerVecBatch);

    VectorBatch *vecBatch = new VectorBatch(rowPerVecBatch);
    vecBatch->Append(longInputVec);
    vecBatch->Append(decimalInputVec);
    vecBatch->Append(dictInputVec);
    vecBatch->Append(nullInputVec);
    EXPECT_EQ(rowPerVecBatch, vecBatch->GetRowCount());

    // process long
    auto state = NewAndInitState(sumLong.get());
    sumLong->ProcessGroup(state.get(), vecBatch, 0, vecBatch->GetRowCount());
    auto retLong = ExtractValueFromState<BaseState<int64_t>>(state.get());
    EXPECT_EQ(200, retLong->value);


    // process null
    state = NewAndInitState(sumNull.get());
    sumNull->ProcessGroup(state.get(), vecBatch, 0, vecBatch->GetRowCount());
    // use state.count = 0 to determine null in sum
    retLong = ExtractValueFromState<BaseState<int64_t>>(state.get());
    EXPECT_TRUE(retLong->IsEmpty());

    // process short decimal
    state = NewAndInitState(sumShortDecimal.get());
    sumShortDecimal->ProcessGroup(state.get(), vecBatch, 0, vecBatch->GetRowCount());
    auto *retDecimal = ExtractValueFromState<BaseCountState<Decimal128>>(state.get());

    Decimal128 expected = 200L;
    // use state.count < 0 to determine if sum overflow
    EXPECT_TRUE(retDecimal->count >= 0);
    EXPECT_EQ(expected, retDecimal->value);

    VectorHelper::FreeVecBatch(vecBatch);
    delete sumFactory;
}

TEST(AggregatorTest, min_test)
{
    int32_t rowPerVecBatch = 200;
    auto minFactory = new MinAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    std::vector<int32_t> channal2 = { 2 };
    std::vector<int32_t> channal3 = { 3 };
    std::vector<int32_t> channal4 = { 4 };
    std::vector<int32_t> channal5 = { 5 };
    std::vector<int32_t> channal6 = { 6 };
    // min test types : long + decimal + varchar + dictionary + null
    auto executionContext = std::make_unique<ExecutionContext>();
    auto minLong = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, false, false);
    minLong->SetExecutionContext(executionContext.get());
    auto minDecimal = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()),
        *(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()), channal3, true, false, false);
    minDecimal->SetExecutionContext(executionContext.get());
    auto minVarchar = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(VarcharType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(VarcharType()).get()), channal4, true, false, false);
    minVarchar->SetExecutionContext(executionContext.get());
    auto minNull = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal2, true, false, false);
    minNull->SetExecutionContext(executionContext.get());
    auto minIntLong = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal5, true, false, false);
    minIntLong->SetExecutionContext(executionContext.get());
    auto minLongInt = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, false, false);
    minLongInt->SetExecutionContext(executionContext.get());
    auto minBoolean = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()), channal6, true, false, false);
    minBoolean->SetExecutionContext(executionContext.get());

    auto longInputVec = BuildAggregateInput(LongType(), rowPerVecBatch);
    auto decimalInputVec = BuildAggregateInput(Decimal128Type(38, 0), rowPerVecBatch);
    std::string stringVals[rowPerVecBatch];
    for (int32_t i = 0; i < rowPerVecBatch; ++i) {
        stringVals[i] = "1";
    }
    auto varcharInputVec = CreateVarcharVector(stringVals, rowPerVecBatch);
    LongDataType longDataType;
    int32_t ids[rowPerVecBatch];
    int64_t dict[rowPerVecBatch];
    for (int32_t i = 0; i < rowPerVecBatch; ++i) {
        ids[i] = 0;
        dict[i] = 1;
    }
    auto dictInputVec = CreateDictionaryVector(longDataType, rowPerVecBatch, ids, rowPerVecBatch, dict);
    auto nullInputVec = BuildAggregateInput(NoneType(), rowPerVecBatch);
    auto intInputVec = BuildAggregateInput(IntType(), rowPerVecBatch);
    auto BoolInputVec = BuildAggregateInput(BooleanType(), rowPerVecBatch);

    VectorBatch *vectorBatch = new VectorBatch(7);
    vectorBatch->Append(longInputVec);
    vectorBatch->Append(dictInputVec);
    vectorBatch->Append(nullInputVec);
    vectorBatch->Append(decimalInputVec);
    vectorBatch->Append(varcharInputVec);
    vectorBatch->Append(intInputVec);
    vectorBatch->Append(BoolInputVec);

    std::vector<BaseVector *> result(1);

    // process long
    auto state = NewAndInitState(minLong.get());
    minLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    Vector<int64_t> longResult(1);
    result.clear();
    result.push_back(&longResult);
    minLong->ExtractValues(state.get(), result, 0);
    EXPECT_EQ(1, longResult.GetValue(0));

    // process null
    state = NewAndInitState(minNull.get());
    minNull->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    minNull->ExtractValues(state.get(), result, 0);
    EXPECT_TRUE(longResult.IsNull(0));

    // process varchar
    state = NewAndInitState(minVarchar.get());
    minVarchar->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    std::string expectedStr = "1";
    auto strPtrAddr = *ExtractValueFromState<int64_t>(state.get());
    auto len = *ExtractValueFromState<int32_t>(state.get() + sizeof(int64_t));
    EXPECT_EQ(1, len);
    EXPECT_EQ(0, std::memcmp(reinterpret_cast<const char *>(strPtrAddr), expectedStr.c_str(), 1));
    Vector<LargeStringContainer<std::string_view>> minVarcharOutput(1);
    std::vector<BaseVector *> minVarcharOutputVector;
    minVarcharOutputVector.push_back(&minVarcharOutput);
    minVarchar->ExtractValues(state.get(), minVarcharOutputVector, 0);
    auto strRes = minVarcharOutput.GetValue(0);
    EXPECT_EQ(1, strRes.size());
    EXPECT_EQ(0, std::memcmp(strRes.data(), expectedStr.c_str(), 1));

    // process int to long
    state = NewAndInitState(minIntLong.get());
    minIntLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    minIntLong->ExtractValues(state.get(), result, 0);
    EXPECT_EQ(1, longResult.GetValue(0));

    // process long to int
    state = NewAndInitState(minLongInt.get());
    minLongInt->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    Vector<int32_t> intResult(1);
    result.clear();
    result.push_back(&intResult);
    minLongInt->ExtractValues(state.get(), result, 0);
    EXPECT_EQ(1, intResult.GetValue(0));

    state = NewAndInitState(minBoolean.get());
    minBoolean->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    Vector<bool> booleanResult(1);
    result.clear();
    result.push_back(&booleanResult);
    minBoolean->ExtractValues(state.get(), result, 0);
    EXPECT_EQ(true, booleanResult.GetValue(0));

    VectorHelper::FreeVecBatch(vectorBatch);
    delete minFactory;
}

TEST(AggregatorTest, max_test)
{
    int32_t rowPerVecBatch = 200;
    auto maxFactory = new MaxAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    std::vector<int32_t> channal1 = { 1 };
    std::vector<int32_t> channal2 = { 2 };
    std::vector<int32_t> channal4 = { 4 };
    std::vector<int32_t> channal5 = { 5 };
    std::vector<int32_t> channal6 = { 6 };
    // max test types : long + decimal + varchar + dictionary + null
    auto executionContext = std::make_unique<ExecutionContext>();
    auto maxLong = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, false, false);
    maxLong->SetExecutionContext(executionContext.get());
    auto maxDecimal = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()),
        *(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()), channal1, true, false, false);
    maxDecimal->SetExecutionContext(executionContext.get());
    auto maxVarchar = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(VarcharType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(VarcharType()).get()), channal2, true, false, false);
    maxVarchar->SetExecutionContext(executionContext.get());
    auto maxNull = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal4, true, false, false);
    maxNull->SetExecutionContext(executionContext.get());
    auto maxIntLong = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal5, true, false, false);
    maxIntLong->SetExecutionContext(executionContext.get());
    auto maxLongInt = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, false, false);
    maxLongInt->SetExecutionContext(executionContext.get());
    auto maxBoolean = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()), channal6, true, false, false);
    maxBoolean->SetExecutionContext(executionContext.get());

    auto longInputVec = BuildAggregateInput(LongType(), rowPerVecBatch);
    auto decimalInputVec = BuildAggregateInput(Decimal128Type(38, 0), rowPerVecBatch);
    std::string stringVals[rowPerVecBatch];
    for (int32_t i = 0; i < rowPerVecBatch; ++i) {
        stringVals[i] = "1";
    }
    auto varcharInputVec = CreateVarcharVector(stringVals, rowPerVecBatch);
    LongDataType longDataType;
    int32_t ids[rowPerVecBatch];
    int64_t dict[rowPerVecBatch];
    for (int32_t i = 0; i < rowPerVecBatch; ++i) {
        ids[i] = 0;
        dict[i] = 1;
    }
    auto dictInputVec = CreateDictionaryVector(longDataType, rowPerVecBatch, ids, rowPerVecBatch, dict);
    auto nullInputVec = BuildAggregateInput(NoneType(), rowPerVecBatch);
    auto intInputVec = BuildAggregateInput(IntType(), rowPerVecBatch);
    auto boolInputVec = BuildAggregateInput(BooleanType(), rowPerVecBatch);

    VectorBatch *vectorBatch = new VectorBatch(7);
    vectorBatch->Append(longInputVec);
    vectorBatch->Append(decimalInputVec);
    vectorBatch->Append(varcharInputVec);
    vectorBatch->Append(dictInputVec);
    vectorBatch->Append(nullInputVec);
    vectorBatch->Append(intInputVec);
    vectorBatch->Append(boolInputVec);

    // process long
    auto state = NewAndInitState(maxLong.get());
    maxLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    auto value = ExtractValueFromState<int64_t>(state.get());
    EXPECT_EQ(1, *value);

    // process null
    state = NewAndInitState(maxNull.get());
    maxNull->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    // use state.count = 0 to determine null
    auto maxNullState = ExtractValueFromState<BaseState<int64_t>>(state.get());
    EXPECT_TRUE(maxNullState->IsEmpty());

    // process varchar
    state = NewAndInitState(maxVarchar.get());
    maxVarchar->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    std::string expectedStr = "1";

    auto strPtrAddr = *ExtractValueFromState<int64_t>(state.get());
    auto len = *ExtractValueFromState<int32_t>(state.get() + sizeof(int64_t));
    EXPECT_EQ(1, len);
    EXPECT_EQ(0, std::memcmp(reinterpret_cast<const char *>(strPtrAddr), expectedStr.c_str(), 1));
    Vector<LargeStringContainer<std::string_view>> maxVarcharOutput(1);
    std::vector<BaseVector *> maxVarcharOutputVector;
    maxVarcharOutputVector.push_back(&maxVarcharOutput);
    maxVarchar->ExtractValues(state.get(), maxVarcharOutputVector, 0);
    std::string_view strRes = maxVarcharOutput.GetValue(0);
    EXPECT_EQ(1, strRes.size());
    EXPECT_EQ(0, std::memcmp(strRes.data(), expectedStr.c_str(), 1));

    // process decimal
    state = NewAndInitState(maxDecimal.get());
    maxDecimal->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    auto decimalValue = ExtractValueFromState<Decimal128>(state.get());
    Decimal128 expected(1);
    EXPECT_EQ(expected, *decimalValue);

    // process int to long
    state = NewAndInitState(maxIntLong.get());
    maxIntLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    auto int64Value = ExtractValueFromState<int32_t>(state.get());
    EXPECT_EQ(1, *int64Value);

    // process long to int
    state = NewAndInitState(maxLongInt.get());
    maxLongInt->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    auto int32Value = ExtractValueFromState<int32_t>(state.get());
    EXPECT_EQ(1, *int32Value);

    state = NewAndInitState(maxBoolean.get());
    maxBoolean->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    auto boolValue = ExtractValueFromState<bool>(state.get());
    EXPECT_EQ(true, *boolValue);

    VectorHelper::FreeVecBatch(vectorBatch);
    delete maxFactory;
}

TEST(AggregatorTest, verify_decimal_presision_improvement)
{
    auto isSupportDecimalPrecisionImprovementRule = ConfigUtil::GetSupportDecimalPrecisionImprovementRule();

    auto avgFactory = new AverageAggregatorFactory();
    std::vector<int32_t> channel0 = { 0 };
    ConfigUtil::SetSupportDecimalPrecisionImprovementRule(SupportDecimalPrecisionImprovementRule::IS_SUPPORT);
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAgg1 = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(25, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 10)).get()), channel0, true, false, true);
    avgDeciAgg1->SetExecutionContext(executionContext.get());

    Decimal128Wrapper deci0("60000000000000000.00000000");
    Decimal128Wrapper deci1("30000000000000000.00000000");
    Decimal128Wrapper deci2("10000000000000000.00000000");

    Decimal128 decimal0 = deci0.ToDecimal128();
    Decimal128 decimal1 = deci1.ToDecimal128();
    Decimal128 decimal2 = deci2.ToDecimal128();

    auto *deci25_8Vec = new Vector<Decimal128>(3);
    deci25_8Vec->SetValue(0, decimal0);
    deci25_8Vec->SetValue(1, decimal1);
    deci25_8Vec->SetValue(2, decimal2);

    auto *vecBatch = new VectorBatch(3);
    vecBatch->Append(deci25_8Vec);

    auto *resultVec1 = new Vector<Decimal128>(1);
    std::vector<BaseVector *> extractVec1 = { resultVec1 };
    auto state1 = NewAndInitState(avgDeciAgg1.get());
    avgDeciAgg1->ProcessGroup(state1.get(), vecBatch, 0, 3);
    avgDeciAgg1->ExtractValues(state1.get(), extractVec1, 0);

    Decimal128Wrapper expected1("33333333333333333.3333333333");
    EXPECT_EQ(expected1.ToDecimal128().ToString(), Decimal128Wrapper(resultVec1->GetValue(0)).ToString());
    EXPECT_EQ(expected1.ToDecimal128(), resultVec1->GetValue(0));

    ConfigUtil::SetSupportDecimalPrecisionImprovementRule(SupportDecimalPrecisionImprovementRule::IS_NOT_SUPPORT);
    auto avgDeciAgg2 = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(25, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 10)).get()), channel0, true, false, true);
    avgDeciAgg2->SetExecutionContext(executionContext.get());
    auto *resultVec2 = new Vector<Decimal128>(1);
    std::vector<BaseVector *> extractVec2 = { resultVec2 };
    auto state2 = NewAndInitState(avgDeciAgg2.get());
    avgDeciAgg2->ProcessGroup(state2.get(), vecBatch, 0, 3);
    avgDeciAgg2->ExtractValues(state2.get(), extractVec2, 0);

    Decimal128Wrapper expected2("33333333333333333.3333333300");
    EXPECT_EQ(expected2.ToDecimal128().ToString(), Decimal128Wrapper(resultVec2->GetValue(0)).ToString());
    EXPECT_EQ(expected2.ToDecimal128(), resultVec2->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec1;
    delete resultVec2;
    delete avgFactory;

    ConfigUtil::SetSupportDecimalPrecisionImprovementRule(isSupportDecimalPrecisionImprovementRule);
}

TEST(AggregatorTest, avg_test)
{
    int32_t rowPerVecBatch = 200;

    auto avgFactory = new AverageAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    std::vector<int32_t> channal1 = { 1 };
    std::vector<int32_t> channal3 = { 3 };
    // avg test types : long + decimal + dictionary + null
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgLong = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, true, false, false);
    avgLong->SetExecutionContext(executionContext.get());
    auto avgDecimal = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()),
        *(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()), channal1, true, false, false);
    avgDecimal->SetExecutionContext(executionContext.get());
    auto avgNull = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal3, true, false, false);
    avgNull->SetExecutionContext(executionContext.get());

    auto longInputVec = BuildAggregateInput(LongType(), rowPerVecBatch);
    auto decimalInputVec = BuildAggregateInput(Decimal128Type(38, 0), rowPerVecBatch);
    LongDataType longDataType;
    int32_t ids[rowPerVecBatch];
    int64_t dict[rowPerVecBatch];
    for (int32_t i = 0; i < rowPerVecBatch; ++i) {
        ids[i] = 0;
        dict[i] = 1;
    }
    auto dictInputVec = CreateDictionaryVector(longDataType, rowPerVecBatch, ids, rowPerVecBatch, dict);
    auto nullInputVec = BuildAggregateInput(NoneType(), rowPerVecBatch);

    VectorBatch *vectorBatch = new VectorBatch(4);
    vectorBatch->Append(longInputVec);
    vectorBatch->Append(decimalInputVec);
    vectorBatch->Append(dictInputVec);
    vectorBatch->Append(nullInputVec);

    // process long
    auto state = NewAndInitState(avgLong.get());
    avgLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    Vector<double> avgLongOutput(1);
    std::vector<BaseVector *> avgLongOutputVector;
    avgLongOutputVector.push_back(&avgLongOutput);
    avgLong->ExtractValues(state.get(), avgLongOutputVector, 0);
    EXPECT_TRUE(avgLongOutput.GetValue(0) - 1 <= DBL_EPSILON);

    // process null
    state = NewAndInitState(avgNull.get());
    avgNull->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
    Vector<double> avgNullOutput(1);
    std::vector<BaseVector *> avgNullOutputVector;
    avgNullOutputVector.push_back(&avgNullOutput);
    avgNull->ExtractValues(state.get(), avgNullOutputVector, 0);
    EXPECT_TRUE(avgNullOutput.IsNull(0));

    VectorHelper::FreeVecBatch(vectorBatch);
    delete avgFactory;
}

TEST(AggregatorTest, spark_sum_decimal64_normal)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumDeciAggPartial =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal64Type(18, 6)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(28, 6)).get()), channal0, true, true);
    sumDeciAggPartial->SetExecutionContext(executionContext.get());

    auto *deci18_6Vec = new Vector<int64_t>(3);
    deci18_6Vec->SetValue(0, 999999999999999999L);
    deci18_6Vec->SetValue(1, 999999999999999999L);
    deci18_6Vec->SetValue(2, 999999999999999999L);

    auto *isOverflowVec = new Vector<bool>(3);
    isOverflowVec->SetValue(0, false);
    isOverflowVec->SetValue(1, false);
    isOverflowVec->SetValue(2, false);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, isOverflowVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci18_6Vec);
    vecBatch->Append(isOverflowVec);

    auto state = NewAndInitState(sumDeciAggPartial.get());
    sumDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128Wrapper expected1("999999999999.999999");

    EXPECT_EQ(expected1.ToDecimal128(), resultVec->GetValue(0));
    EXPECT_EQ(expected1.ToDecimal128(), resultVec->GetValue(0));
    EXPECT_FALSE(isOverflowVec->GetValue(0));

    sumDeciAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);
    auto sumDeciAggFinal =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(28, 6)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(28, 6)).get()), channal0, false, false);

    sumDeciAggFinal->SetStateOffset(0);
    sumDeciAggFinal->ExtractValues(state.get(), extractVec, 0);

    Decimal128Wrapper expected2("2999999999999.999997");
    EXPECT_EQ(expected2.ToDecimal128(), resultVec->GetValue(0));
    EXPECT_EQ(expected2.ToDecimal128(), resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_decimal128_normal)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumDeciAggPartial =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(25, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 8)).get()), channal0, true, true);
    sumDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128 deci("99999999999999999.99999999");
    auto *deci25_8Vec = new Vector<Decimal128>(3);
    deci25_8Vec->SetValue(0, deci);
    deci25_8Vec->SetValue(1, deci);
    deci25_8Vec->SetValue(2, deci);

    auto *isOverflowVec = new Vector<bool>(3);
    isOverflowVec->SetValue(0, false);
    isOverflowVec->SetValue(1, false);
    isOverflowVec->SetValue(2, false);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, isOverflowVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci25_8Vec);
    vecBatch->Append(isOverflowVec);

    auto state = NewAndInitState(sumDeciAggPartial.get());
    sumDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128 expected1("99999999999999999.99999999");

    EXPECT_EQ(expected1.ToString(), resultVec->GetValue(0).ToString());
    EXPECT_FALSE(isOverflowVec->GetValue(0));

    auto sumDeciAggFinal =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 8)).get()), channal0, false, false);
    sumDeciAggFinal->SetExecutionContext(executionContext.get());
    sumDeciAggFinal->SetStateOffset(0);
    sumDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);
    sumDeciAggFinal->ExtractValues(state.get(), extractVec, 0);

    Decimal128Wrapper expected2("299999999999999999.99999997");
    EXPECT_EQ(expected2.ToDecimal128().ToString(), Decimal128Wrapper(resultVec->GetValue(0)).ToString());
    EXPECT_EQ(expected2.ToDecimal128(), resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_decimal128_overflow_throw_exception_when_isOverflowAsNull_is_false)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumDeciAggPartial =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()), channal0, true, true, false);
    sumDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128 deci("99999999999999999999999999999999999999");
    auto *deci38_0Vec = new Vector<Decimal128>(3);
    deci38_0Vec->SetValue(0, deci);
    deci38_0Vec->SetValue(1, deci);
    deci38_0Vec->SetValue(2, deci);

    auto *isOverflowVec = new Vector<bool>(3);
    isOverflowVec->SetValue(0, false);
    isOverflowVec->SetValue(1, false);
    isOverflowVec->SetValue(2, false);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, isOverflowVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci38_0Vec);
    vecBatch->Append(isOverflowVec);

    auto state = NewAndInitState(sumDeciAggPartial.get());
    sumDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128 expected1("99999999999999999999999999999999999999");

    EXPECT_EQ(expected1.ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected1, resultVec->GetValue(0));
    EXPECT_FALSE(isOverflowVec->GetValue(0));

    auto sumDeciAggFinal =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()), channal0, false, false, false);
    sumDeciAggFinal->SetExecutionContext(executionContext.get());
    sumDeciAggFinal->SetStateOffset(0);
    sumDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);

    bool isThrowException = false;
    try {
        sumDeciAggFinal->ExtractValues(state.get(), extractVec, 0);
    } catch (OmniException &e) {
        isThrowException = true;
    }
    EXPECT_TRUE(isThrowException);

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_decimal128_overflow_return_null_when_isOverflowAsNull_is_true)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumDeciAggPartial =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()), channal0, true, true, true);
    sumDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128 deci("99999999999999999999999999999999999999");
    auto *deci38_0Vec = new Vector<Decimal128>(3);
    deci38_0Vec->SetValue(0, deci);
    deci38_0Vec->SetValue(1, deci);
    deci38_0Vec->SetValue(2, deci);

    auto *isOverflowVec = new Vector<bool>(3);
    isOverflowVec->SetValue(0, false);
    isOverflowVec->SetValue(1, false);
    isOverflowVec->SetValue(2, false);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, isOverflowVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci38_0Vec);
    vecBatch->Append(isOverflowVec);

    auto state = NewAndInitState(sumDeciAggPartial.get());
    sumDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128 expected1("99999999999999999999999999999999999999");

    EXPECT_EQ(expected1.ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected1, resultVec->GetValue(0));
    EXPECT_FALSE(isOverflowVec->GetValue(0));

    auto sumDeciAggFinal =
        sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()), channal0, false, false, true);
    sumDeciAggFinal->SetExecutionContext(executionContext.get());
    sumDeciAggFinal->SetStateOffset(0);
    sumDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);
    sumDeciAggFinal->ExtractValues(state.get(), extractVec, 0);

    EXPECT_TRUE(resultVec->IsNull(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_avg_decimal64_normal)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAggPartial =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal64Type(18, 6)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(28, 6)).get()), channal0, true, true);
    avgDeciAggPartial->SetExecutionContext(executionContext.get());

    auto *deci18_6Vec = new Vector<int64_t>(3);
    deci18_6Vec->SetValue(0, 999999999999999999L);
    deci18_6Vec->SetValue(1, 999999999999999999L);
    deci18_6Vec->SetValue(2, 999999999999999999L);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci18_6Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciAggPartial.get());
    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128Wrapper expected1("999999999999.999999");

    EXPECT_EQ(expected1.ToDecimal128().ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected1, resultVec->GetValue(0));

    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);
    auto avgDeciAggFinal =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(28, 6)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(22, 10)).get()), channal0, false, false);
    avgDeciAggFinal->SetExecutionContext(executionContext.get());
    auto countState = ExtractValueFromState<BaseCountState<Decimal128>>(state.get());
    EXPECT_EQ(3, countState->count);
    auto stateFinal = NewAndInitState(avgDeciAggFinal.get());
    auto finalState = ExtractValueFromState<BaseCountState<Decimal128>>(stateFinal.get());
    finalState->value = countState->value;
    finalState->count = countState->count;
    avgDeciAggFinal->ExtractValues(stateFinal.get(), extractVec, 0);

    Decimal128Wrapper expected2 = Decimal128Wrapper("999999999999.9999990000");
    EXPECT_EQ(expected2.ToDecimal128().ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected2.ToDecimal128(), resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_decimal128_normal)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAggPartial =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(25, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 8)).get()), channal0, true, true);
    avgDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128Wrapper deci("99999999999999999.99999999");
    Decimal128 decimal128 = deci.ToDecimal128();

    auto *deci25_8Vec = new Vector<Decimal128>(3);
    deci25_8Vec->SetValue(0, decimal128);
    deci25_8Vec->SetValue(1, decimal128);
    deci25_8Vec->SetValue(2, decimal128);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci25_8Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciAggPartial.get());
    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128Wrapper expected1("99999999999999999.99999999");
    EXPECT_EQ(expected1.ToDecimal128().ToString(), Decimal128Wrapper(resultVec->GetValue(0)).ToString());
    EXPECT_EQ(expected1.ToDecimal128(), resultVec->GetValue(0));

    auto avgDeciAggFinal =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(35, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(29, 12)).get()), channal0, false, false);
    avgDeciAggFinal->SetExecutionContext(executionContext.get());
    avgDeciAggFinal->SetStateOffset(0);

    avgDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);
    avgDeciAggFinal->ExtractValues(state.get(), extractVec, 0);

    Decimal128Wrapper expected2("99999999999999999.999999990000");
    EXPECT_EQ(expected2.ToDecimal128().ToString(), Decimal128Wrapper(resultVec->GetValue(0)).ToString());
    EXPECT_EQ(expected2.ToDecimal128(), resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_decimal128_result_decimal64)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAggPartial =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(19, 8)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal64Type(18, 8)).get()), channal0, false, false);
    avgDeciAggPartial->SetExecutionContext(executionContext.get());
    int32_t totalRow = 1;
    auto *deci19_8Vec = new Vector<Decimal128>(totalRow);

    Decimal128Wrapper deci("9999999999.99999999");
    Decimal128 decimal128 = deci.ToDecimal128();
    deci19_8Vec->SetValue(0, decimal128);
    auto *avgCountVec = new Vector<int64_t>(1);
    avgCountVec->SetValue(0, 10);

    auto *resultVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci19_8Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciAggPartial.get());
    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    // 9999999999.99999999 + 10 =
    int64_t expect = 100000000000000000LL;
    EXPECT_EQ(expect, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_decimal128_overflow_throw_exception_when_isOverflowAsNull_is_false)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAggPartial =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()), channal0, true, true, false);
    avgDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128 deci("99999999999999999999999999999999999999");
    auto *deci38_0Vec = new Vector<Decimal128>(3);
    deci38_0Vec->SetValue(0, deci);
    deci38_0Vec->SetValue(1, deci);
    deci38_0Vec->SetValue(2, deci);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci38_0Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciAggPartial.get());
    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128 expected1("99999999999999999999999999999999999999");

    EXPECT_EQ(expected1.ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected1, resultVec->GetValue(0));

    auto avgDeciAggFinal =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 4)).get()), channal0, false, false, false);
    avgDeciAggFinal->SetExecutionContext(executionContext.get());
    avgDeciAggFinal->SetStateOffset(0);
    avgDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);

    bool isThrowException = false;
    try {
        avgDeciAggFinal->ExtractValues(state.get(), extractVec, 0);
    } catch (OmniException &e) {
        isThrowException = true;
    }
    EXPECT_TRUE(isThrowException);

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_decimal128_overflow_return_null_when_isOverflowAsNull_is_true)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAggPartial =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()), channal0, true, true, true);
    avgDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128 deci("99999999999999999999999999999999999999");
    auto *deci38_0Vec = new Vector<Decimal128>(3);
    deci38_0Vec->SetValue(0, deci);
    deci38_0Vec->SetValue(1, deci);
    deci38_0Vec->SetValue(2, deci);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci38_0Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciAggPartial.get());
    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    Decimal128 expected1("99999999999999999999999999999999999999");

    EXPECT_EQ(expected1.ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected1, resultVec->GetValue(0));

    auto avgDeciAggFinal =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 4)).get()), channal0, false, false, true);
    avgDeciAggFinal->SetExecutionContext(executionContext.get());

    avgDeciAggFinal->SetStateOffset(0);
    avgDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);
    avgDeciAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_TRUE(resultVec->IsNull(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_decimal128_count_cast_to_wider_type_overflow_return_null_when_isOverflowAsNull_is_true)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciAggPartial =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 38)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 38)).get()), channal0, true, true, true);
    avgDeciAggPartial->SetExecutionContext(executionContext.get());

    Decimal128Wrapper deci1("-0.99999999999999999999999999999999999999");
    Decimal128Wrapper deci2("0.14159265354378240000000000000000000000");
    Decimal128Wrapper deci3("0.00000000000000000000000000000000000000");

    Decimal128 decimal128_1 = deci1.ToDecimal128();
    Decimal128 decimal128_2 = deci2.ToDecimal128();
    Decimal128 decimal128_3 = deci3.ToDecimal128();

    auto *deci38_38Vec = new Vector<Decimal128>(3);
    deci38_38Vec->SetValue(0, decimal128_1);
    deci38_38Vec->SetValue(1, decimal128_2);
    deci38_38Vec->SetValue(2, decimal128_3);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci38_38Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciAggPartial.get());
    avgDeciAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgDeciAggPartial->ExtractValues(state.get(), extractVec, 0);

    auto avgDeciAggFinal =
        avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 38)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(38, 38)).get()), channal0, false, false, true);
    avgDeciAggFinal->SetExecutionContext(executionContext.get());
    avgDeciAggFinal->SetStateOffset(0);
    avgDeciAggFinal->ProcessGroup(state.get(), vecBatch, 1, 2);
    avgDeciAggFinal->ExtractValues(state.get(), extractVec, 0);
    // The test case is no longer valid because we fixed the IsCastToWiderTypeOverflow function
    // to not rescale count, which was causing the overflow. Now the function should return a valid result.
    EXPECT_FALSE(resultVec->IsNull(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_decimal128_normal_when_inputRaw_is_true_and_outputPartial_is_false)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDeciWindow = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(Decimal128Type(22, 0)).get()),
        *(AggregatorUtil::WrapWithDataTypes(Decimal128Type(26, 4)).get()), channal0, true, false, true);
    avgDeciWindow->SetExecutionContext(executionContext.get());

    Decimal128Wrapper deci1("1234567890123456789012");
    Decimal128Wrapper deci2("9999999999999999999999");

    Decimal128 decimal128_1 = deci1.ToDecimal128();
    Decimal128 decimal128_2 = deci2.ToDecimal128();

    auto *deci22_0Vec = new Vector<Decimal128>(2);
    deci22_0Vec->SetValue(0, decimal128_1);
    deci22_0Vec->SetValue(1, decimal128_2);

    auto *avgCountVec = new Vector<int64_t>(2);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);

    auto *resultVec = new Vector<Decimal128>(1);

    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(deci22_0Vec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDeciWindow.get());
    avgDeciWindow->ProcessGroup(state.get(), vecBatch, 0, 2);
    avgDeciWindow->ExtractValues(state.get(), extractVec, 0);

    Decimal128 expected("5617283945061728394505.5000");

    EXPECT_EQ(expected.ToString(), resultVec->GetValue(0).ToString());
    EXPECT_EQ(expected, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_sum_short_normal)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumShortAggPartial = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(ShortType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, true);
    sumShortAggPartial->SetExecutionContext(executionContext.get());

    auto *shortVec = new Vector<short>(3);
    shortVec->SetValue(0, 12345);
    shortVec->SetValue(1, 23451);
    shortVec->SetValue(2, 12345);

    auto *resultVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(shortVec);

    auto state = NewAndInitState(sumShortAggPartial.get());
    sumShortAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumShortAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(12345, resultVec->GetValue(0));

    sumShortAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto sumShortAggFinal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, false, false);
    sumShortAggFinal->SetStateOffset(0);
    sumShortAggFinal->SetExecutionContext(executionContext.get());
    sumShortAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(48141, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_int_normal)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumIntAggPartial = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, true);
    sumIntAggPartial->SetExecutionContext(executionContext.get());

    auto *intVec = new Vector<int32_t>(3);
    intVec->SetValue(0, 1234567890);
    intVec->SetValue(1, 2045678901);
    intVec->SetValue(2, 1234567890);

    auto *resultVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(intVec);

    auto state = NewAndInitState(sumIntAggPartial.get());
    sumIntAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumIntAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(1234567890, resultVec->GetValue(0));

    sumIntAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto sumIntAggFinal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, false, false);
    sumIntAggFinal->SetStateOffset(0);
    sumIntAggFinal->SetExecutionContext(executionContext.get());
    sumIntAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(4514814681, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_long_normal)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumLongAggPartial = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, true);
    sumLongAggPartial->SetExecutionContext(executionContext.get());

    auto *longVec = new Vector<int64_t>(3);
    longVec->SetValue(0, 1234567890123456789);
    longVec->SetValue(1, 2345678901234567891);
    longVec->SetValue(2, 3456789012345678912);

    auto *resultVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(longVec);

    auto state = NewAndInitState(sumLongAggPartial.get());
    sumLongAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumLongAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(1234567890123456789, resultVec->GetValue(0));

    sumLongAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto sumLongAggFinal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, false, false);
    sumLongAggFinal->SetExecutionContext(executionContext.get());
    sumLongAggFinal->SetStateOffset(0);
    sumLongAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(7037035803703703592, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_long_overflow)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumLongAggPartial = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, true);
    sumLongAggPartial->SetExecutionContext(executionContext.get());

    auto *longVec = new Vector<int64_t>(3);
    longVec->SetValue(0, 9223372036854774807);
    longVec->SetValue(1, 9223372036854774807);
    longVec->SetValue(2, 9223372036854774807);

    auto *resultVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(longVec);

    auto state = NewAndInitState(sumLongAggPartial.get());
    sumLongAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumLongAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(9223372036854774807, resultVec->GetValue(0));

    sumLongAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto sumLongAggFinal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, false, false);
    sumLongAggFinal->SetStateOffset(0);
    sumLongAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(9223372036854772805, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_long_final_stage)
{
    // 851451 + 9223372036854775807 = -9223372036853924358
    // -9223372036854775807 + -256 = 9223372036854775553
    // overflow  but same with vanilla spark
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };

    auto *longVec1 = new Vector<int64_t>(2);
    longVec1->SetValue(0, 851451);
    longVec1->SetValue(1, 9223372036854775807LL);


    auto *resultVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(longVec1);

    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumLongAggFinal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, false, false);
    sumLongAggFinal->SetExecutionContext(executionContext.get());
    auto state = NewAndInitState(sumLongAggFinal.get());
    sumLongAggFinal->ProcessGroup(state.get(), vecBatch1, 0, 2);
    sumLongAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(-9223372036853924358, resultVec->GetValue(0));

    auto *longVec2 = new Vector<int64_t>(2);
    longVec2->SetValue(0, -9223372036854775807ll);
    longVec2->SetValue(1, -256);

    auto *vecBatch2 = new VectorBatch(1);
    vecBatch2->Append(longVec2);
    state = NewAndInitState(sumLongAggFinal.get());
    sumLongAggFinal->ProcessGroup(state.get(), vecBatch2, 0, 2);
    sumLongAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(9223372036854775553, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultVec;
    delete sumFactory;
}

TEST(AggregatorTest, spark_sum_double_normal)
{
    auto sumFactory = new SumSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumDoubleAggPartial = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, true, true);
    sumDoubleAggPartial->SetExecutionContext(executionContext.get());

    auto *doubleVec = new Vector<double>(3);
    doubleVec->SetValue(0, 123456789012.3456789);
    doubleVec->SetValue(1, 234567890123.4567891);
    doubleVec->SetValue(2, 345678901234.5678912);

    auto *resultVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVec = { resultVec };

    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(doubleVec);

    auto state = NewAndInitState(sumDoubleAggPartial.get());
    sumDoubleAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    sumDoubleAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(123456789012.3456789, resultVec->GetValue(0));

    sumDoubleAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto sumDoubleAggFinal = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, false, false);
    sumDoubleAggFinal->SetExecutionContext(executionContext.get());
    sumDoubleAggFinal->SetStateOffset(0);
    sumDoubleAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(7.037035803703704E11, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete sumFactory;
}


TEST(AggregatorTest, spark_avg_short_normal)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgShortAggPartial = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(ShortType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, true, true);
    avgShortAggPartial->SetExecutionContext(executionContext.get());

    auto *shortVec = new Vector<short>(3);
    shortVec->SetValue(0, 12345);
    shortVec->SetValue(1, 23451);
    shortVec->SetValue(2, 12345);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(shortVec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgShortAggPartial.get());
    avgShortAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgShortAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(12345.0, resultVec->GetValue(0));

    avgShortAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto avgShortAggFinal = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, false, false);
    avgShortAggFinal->SetStateOffset(0);
    avgShortAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(16047.0, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_int_normal)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgIntAggPartial = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, true, true);
    avgIntAggPartial->SetExecutionContext(executionContext.get());

    auto *intVec = new Vector<int32_t>(3);
    intVec->SetValue(0, 1234567890);
    intVec->SetValue(1, 2045678901);
    intVec->SetValue(2, 1234567890);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(intVec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgIntAggPartial.get());
    avgIntAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgIntAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(1234567890.0, resultVec->GetValue(0));

    avgIntAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto avgIntAggFinal = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, false, false);
    avgIntAggFinal->SetExecutionContext(executionContext.get());
    avgIntAggFinal->SetStateOffset(0);
    avgIntAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(1.504938227E9, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_long_normal)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgLongAggPartial = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, true, true);
    avgLongAggPartial->SetExecutionContext(executionContext.get());

    auto *longVec = new Vector<int64_t>(3);
    longVec->SetValue(0, 9223372036854774807L);
    longVec->SetValue(1, 9223372036854774807L);
    longVec->SetValue(2, 9223372036854774807L);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(longVec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgLongAggPartial.get());
    avgLongAggPartial->ProcessGroup(state.get(), vecBatch, 0, 1);
    avgLongAggPartial->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(9.2233720368547748E18, resultVec->GetValue(0));

    avgLongAggPartial->ProcessGroup(state.get(), vecBatch, 1, 2);

    auto avgLongAggFinal = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, false, false);
    avgLongAggFinal->SetExecutionContext(executionContext.get());
    avgLongAggFinal->SetStateOffset(0);
    avgLongAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(9.2233720368547748E18, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

TEST(AggregatorTest, spark_avg_double_normal)
{
    auto avgFactory = new AverageSparkAggregatorFactory();
    std::vector<int32_t> channal0 = { 0, 1 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgDoubleAggPartial = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, true, true);
    avgDoubleAggPartial->SetExecutionContext(executionContext.get());

    auto *doubleVec = new Vector<double>(3);
    doubleVec->SetValue(0, 123456789012.3456789);
    doubleVec->SetValue(1, 234567890123.4567891);
    doubleVec->SetValue(2, 345678901234.5678912);

    auto *avgCountVec = new Vector<int64_t>(3);
    avgCountVec->SetValue(0, 1);
    avgCountVec->SetValue(1, 1);
    avgCountVec->SetValue(2, 1);

    auto *resultVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVec = { resultVec, avgCountVec };

    auto *vecBatch = new VectorBatch(2);
    vecBatch->Append(doubleVec);
    vecBatch->Append(avgCountVec);

    auto state = NewAndInitState(avgDoubleAggPartial.get());
    avgDoubleAggPartial->ProcessGroup(state.get(), vecBatch, 0, 3);

    auto avgDoubleAggFinal = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal0, false, false);
    avgDoubleAggFinal->SetStateOffset(0);
    avgDoubleAggFinal->SetExecutionContext(executionContext.get());
    avgDoubleAggFinal->ExtractValues(state.get(), extractVec, 0);
    EXPECT_EQ(2.345678601234568E11, resultVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultVec;
    delete avgFactory;
}

// first basic function:  first with ignore null
TEST(AggregatorTest, first_short_ignorenull_test)
{
    auto firstIgnoreNullFactory = new FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto firstIgnoreNullIntAggPartial =
        firstIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(ShortType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(ShortType()).get()), channal0, true, true);
    firstIgnoreNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputShortVec1 = new Vector<short>(5);
    for (int i = 0; i < 5; i++) {
        inputShortVec1->SetNull(i);
    }

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputShortVec1);

    auto *resultfirstVec1 = new Vector<short>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = { resultfirstVec1, resultValueSetVec1 };

    auto state = NewAndInitState(firstIgnoreNullIntAggPartial.get());

    // add first VectorBatch
    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 0);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 1);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 2);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 3);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 4);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    // add second VectorBatch,keep value not change
    auto *inputShortVec2 = new Vector<short>(3);
    inputShortVec2->SetNull(0);
    inputShortVec2->SetValue(1, 211);
    inputShortVec2->SetNull(2);
    auto *vecBatch2 = new VectorBatch(1);
    vecBatch2->Append(inputShortVec2);

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 0);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 1);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resultfirstVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 2);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resultfirstVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultfirstVec1;
    delete resultValueSetVec1;
    delete firstIgnoreNullFactory;
}

// first basic function:  first with ignore null
TEST(AggregatorTest, first_int_ignorenull_test)
{
    auto firstIgnoreNullFactory = new FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto firstIgnoreNullIntAggPartial =
        firstIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, true);
    firstIgnoreNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputIntVec1 = new Vector<int32_t>(5);
    for (int i = 0; i < 5; i++) {
        inputIntVec1->SetNull(i);
    }

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputIntVec1);

    auto *resultfirstVec1 = new Vector<int32_t>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = { resultfirstVec1, resultValueSetVec1 };

    auto state = NewAndInitState(firstIgnoreNullIntAggPartial.get());

    // add first VectorBatch
    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 0);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 1);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 2);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 3);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 4);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    // add second VectorBatch,keep value not change
    auto *inputIntVec2 = new Vector<int32_t>(3);
    inputIntVec2->SetNull(0);
    inputIntVec2->SetValue(1, 211);
    inputIntVec2->SetNull(2);
    auto *vecBatch2 = new VectorBatch(1);
    vecBatch2->Append(inputIntVec2);

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 0);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 1);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resultfirstVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 2);
    firstIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resultfirstVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultfirstVec1;
    delete resultValueSetVec1;
    delete firstIgnoreNullFactory;
}

// first basic function:  first include null
TEST(AggregatorTest, first_int_includenull_test)
{
    auto firstWithNullFactory = new FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL);
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto firstWithNullIntAggPartial =
        firstWithNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, true);
    firstWithNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputIntVec1 = new Vector<int32_t>(5);
    inputIntVec1->SetNull(0);
    inputIntVec1->SetNull(1);
    inputIntVec1->SetValue(2, 111);
    inputIntVec1->SetValue(3, 113);
    inputIntVec1->SetNull(4);

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputIntVec1);

    auto *resultfirstVec1 = new Vector<int32_t>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = { resultfirstVec1, resultValueSetVec1 };

    auto state = NewAndInitState(firstWithNullIntAggPartial.get());
    firstWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 0);
    firstWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    firstWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 1);
    firstWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    firstWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 2);
    firstWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    firstWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 3);
    firstWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    firstWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 4);
    firstWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    delete resultfirstVec1;
    delete resultValueSetVec1;
    delete firstWithNullFactory;
}

// first agg function for 2 steps partial  + final
TEST(AggregatorTest, first_int_ignorenull_2steps_test)
{
    auto firstIgnoreNullFactory = new FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);

    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto firstIgnoreNullIntAggPartial =
        firstIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, true);
    firstIgnoreNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputLongVec1 = new Vector<int64_t>(2);
    inputLongVec1->SetNull(0);
    inputLongVec1->SetNull(1);

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputLongVec1);

    auto *resultfirstVec1 = new Vector<int64_t>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs1 = { resultfirstVec1, resultValueSetVec1 };

    // add first VectorBatch
    auto partialState = NewAndInitState(firstIgnoreNullIntAggPartial.get());
    firstIgnoreNullIntAggPartial->ProcessGroup(partialState.get(), vecBatch1, 0);
    firstIgnoreNullIntAggPartial->ExtractValues(partialState.get(), extractVecs1, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    firstIgnoreNullIntAggPartial->ProcessGroup(partialState.get(), vecBatch1, 1);
    firstIgnoreNullIntAggPartial->ExtractValues(partialState.get(), extractVecs1, 0);
    EXPECT_TRUE(resultfirstVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    std::vector<DataTypePtr> vector;
    vector.push_back(LongType());
    vector.push_back(BooleanType());
    DataTypesPtr inputTypes = std::make_unique<DataTypes>(vector);

    std::vector<int32_t> channal2 = { 0, 1 };
    auto firstIgnoreNullIntAggFinal = firstIgnoreNullFactory->CreateAggregator(*inputTypes.get(),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal2, false, false);
    firstIgnoreNullIntAggFinal->SetExecutionContext(executionContext.get());

    auto *inputLongVec2 = new Vector<int64_t>(4);
    inputLongVec2->SetNull(0);
    inputLongVec2->SetValue(1, 111);
    inputLongVec2->SetNull(2);
    inputLongVec2->SetNull(3);

    auto *inputBooleanVec2 = new Vector<bool>(4);
    inputBooleanVec2->SetValue(0, false);
    inputBooleanVec2->SetValue(1, true);
    inputBooleanVec2->SetValue(2, false);
    inputBooleanVec2->SetValue(3, false);

    auto *vecBatch2 = new VectorBatch(2);
    vecBatch2->Append(inputLongVec2);
    vecBatch2->Append(inputBooleanVec2);

    auto *resultfirstVec2 = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVecs2 = { resultfirstVec2 };

    auto finalState = NewAndInitState(firstIgnoreNullIntAggFinal.get());
    firstIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 0);
    firstIgnoreNullIntAggFinal->ExtractValues(finalState.get(), extractVecs2, 0);
    EXPECT_TRUE(resultfirstVec2->IsNull(0));

    firstIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 1);
    firstIgnoreNullIntAggFinal->ExtractValues(finalState.get(), extractVecs2, 0);
    EXPECT_FALSE(resultfirstVec2->IsNull(0));
    EXPECT_EQ(111, resultfirstVec2->GetValue(0));

    firstIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 2);
    firstIgnoreNullIntAggFinal->ExtractValues(finalState.get(), extractVecs2, 0);
    EXPECT_FALSE(resultfirstVec2->IsNull(0));
    EXPECT_EQ(111, resultfirstVec2->GetValue(0));

    firstIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 3);
    firstIgnoreNullIntAggFinal->ExtractValues(finalState.get(), extractVecs2, 0);
    EXPECT_FALSE(resultfirstVec2->IsNull(0));
    EXPECT_EQ(111, resultfirstVec2->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultfirstVec1;
    delete resultValueSetVec1;
    delete resultfirstVec2;
    delete firstIgnoreNullFactory;
}

// last basic function: last with ignore null (single group, no groupKey)
TEST(AggregatorTest, last_short_ignorenull_test)
{
    auto lastIgnoreNullFactory = new LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto lastIgnoreNullShortAggPartial =
        lastIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(ShortType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(ShortType()).get()), channal0, true, true);
    lastIgnoreNullShortAggPartial->SetExecutionContext(executionContext.get());

    auto *inputShortVec1 = new Vector<short>(5);
    for (int i = 0; i < 5; i++) {
        inputShortVec1->SetNull(i);
    }

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputShortVec1);

    auto *resultLastVec1 = new Vector<short>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = { resultLastVec1, resultValueSetVec1 };

    auto state = NewAndInitState(lastIgnoreNullShortAggPartial.get());

    lastIgnoreNullShortAggPartial->ProcessGroup(state.get(), vecBatch1, 0);
    lastIgnoreNullShortAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    lastIgnoreNullShortAggPartial->ProcessGroup(state.get(), vecBatch1, 4);
    lastIgnoreNullShortAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    // second batch: null, 211, 212 -> last non-null is 212
    auto *inputShortVec2 = new Vector<short>(3);
    inputShortVec2->SetNull(0);
    inputShortVec2->SetValue(1, 211);
    inputShortVec2->SetValue(2, 212);
    auto *vecBatch2 = new VectorBatch(1);
    vecBatch2->Append(inputShortVec2);

    lastIgnoreNullShortAggPartial->ProcessGroup(state.get(), vecBatch2, 0);
    lastIgnoreNullShortAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    lastIgnoreNullShortAggPartial->ProcessGroup(state.get(), vecBatch2, 1);
    lastIgnoreNullShortAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resultLastVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    lastIgnoreNullShortAggPartial->ProcessGroup(state.get(), vecBatch2, 2);
    lastIgnoreNullShortAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(212, resultLastVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultLastVec1;
    delete resultValueSetVec1;
    delete lastIgnoreNullFactory;
}

// last basic function: last with ignore null, int type (single group, no groupKey)
TEST(AggregatorTest, last_int_ignorenull_test)
{
    auto lastIgnoreNullFactory = new LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto lastIgnoreNullIntAggPartial =
        lastIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, true);
    lastIgnoreNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputIntVec1 = new Vector<int32_t>(5);
    for (int i = 0; i < 5; i++) {
        inputIntVec1->SetNull(i);
    }

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputIntVec1);

    auto *resultLastVec1 = new Vector<int32_t>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = { resultLastVec1, resultValueSetVec1 };

    auto state = NewAndInitState(lastIgnoreNullIntAggPartial.get());

    lastIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 0);
    lastIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    // second batch: null, 211, 212 -> last non-null is 212
    auto *inputIntVec2 = new Vector<int32_t>(3);
    inputIntVec2->SetNull(0);
    inputIntVec2->SetValue(1, 211);
    inputIntVec2->SetValue(2, 212);
    auto *vecBatch2 = new VectorBatch(1);
    vecBatch2->Append(inputIntVec2);

    lastIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 1);
    lastIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resultLastVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    lastIgnoreNullIntAggPartial->ProcessGroup(state.get(), vecBatch2, 2);
    lastIgnoreNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(212, resultLastVec1->GetValue(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultLastVec1;
    delete resultValueSetVec1;
    delete lastIgnoreNullFactory;
}

// last basic function: last include null (single group, no groupKey)
TEST(AggregatorTest, last_int_includenull_test)
{
    auto lastWithNullFactory = new LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL);
    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto lastWithNullIntAggPartial =
        lastWithNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, true);
    lastWithNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputIntVec1 = new Vector<int32_t>(5);
    inputIntVec1->SetNull(0);
    inputIntVec1->SetNull(1);
    inputIntVec1->SetValue(2, 111);
    inputIntVec1->SetValue(3, 113);
    inputIntVec1->SetNull(4);

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputIntVec1);

    auto *resultLastVec1 = new Vector<int32_t>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = { resultLastVec1, resultValueSetVec1 };

    auto state = NewAndInitState(lastWithNullIntAggPartial.get());
    lastWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 0);
    lastWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    lastWithNullIntAggPartial->ProcessGroup(state.get(), vecBatch1, 4);
    lastWithNullIntAggPartial->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_TRUE(resultValueSetVec1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    delete resultLastVec1;
    delete resultValueSetVec1;
    delete lastWithNullFactory;
}

// last agg function for 2 steps partial + final (single group, no groupKey)
TEST(AggregatorTest, last_int_ignorenull_2steps_test)
{
    auto lastIgnoreNullFactory = new LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);

    std::vector<int32_t> channal0 = { 0 };
    auto executionContext = std::make_unique<ExecutionContext>();
    auto lastIgnoreNullIntAggPartial =
        lastIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal0, true, true);
    lastIgnoreNullIntAggPartial->SetExecutionContext(executionContext.get());

    auto *inputLongVec1 = new Vector<int64_t>(2);
    inputLongVec1->SetNull(0);
    inputLongVec1->SetValue(1, 222);

    auto *vecBatch1 = new VectorBatch(1);
    vecBatch1->Append(inputLongVec1);

    auto *resultLastVec1 = new Vector<int64_t>(1);
    auto *resultValueSetVec1 = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs1 = { resultLastVec1, resultValueSetVec1 };

    auto partialState = NewAndInitState(lastIgnoreNullIntAggPartial.get());
    lastIgnoreNullIntAggPartial->ProcessGroup(partialState.get(), vecBatch1, 0);
    lastIgnoreNullIntAggPartial->ExtractValues(partialState.get(), extractVecs1, 0);
    EXPECT_TRUE(resultLastVec1->IsNull(0));
    EXPECT_FALSE(resultValueSetVec1->GetValue(0));

    lastIgnoreNullIntAggPartial->ProcessGroup(partialState.get(), vecBatch1, 1);
    lastIgnoreNullIntAggPartial->ExtractValues(partialState.get(), extractVecs1, 0);
    EXPECT_FALSE(resultLastVec1->IsNull(0));
    EXPECT_EQ(222, resultLastVec1->GetValue(0));

    std::vector<DataTypePtr> vector;
    vector.push_back(LongType());
    vector.push_back(BooleanType());
    DataTypesPtr inputTypes = std::make_unique<DataTypes>(vector);

    std::vector<int32_t> channal2 = { 0, 1 };
    auto lastIgnoreNullIntAggFinal = lastIgnoreNullFactory->CreateAggregator(*inputTypes.get(),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal2, false, false);
    lastIgnoreNullIntAggFinal->SetExecutionContext(executionContext.get());

    // final input: partial results (val, valueSet); last one has value 333
    auto *inputLongVec2 = new Vector<int64_t>(4);
    inputLongVec2->SetValue(0, 111);
    inputLongVec2->SetValue(1, 222);
    inputLongVec2->SetNull(2);
    inputLongVec2->SetValue(3, 333);

    auto *inputBooleanVec2 = new Vector<bool>(4);
    inputBooleanVec2->SetValue(0, true);
    inputBooleanVec2->SetValue(1, true);
    inputBooleanVec2->SetValue(2, false);
    inputBooleanVec2->SetValue(3, true);

    auto *vecBatch2 = new VectorBatch(2);
    vecBatch2->Append(inputLongVec2);
    vecBatch2->Append(inputBooleanVec2);

    auto *resultLastVec2 = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVecs2 = { resultLastVec2 };

    auto finalState = NewAndInitState(lastIgnoreNullIntAggFinal.get());
    lastIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 0);
    lastIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 1);
    lastIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 2);
    lastIgnoreNullIntAggFinal->ProcessGroup(finalState.get(), vecBatch2, 3);
    lastIgnoreNullIntAggFinal->ExtractValues(finalState.get(), extractVecs2, 0);
    EXPECT_FALSE(resultLastVec2->IsNull(0));
    EXPECT_EQ(333, resultLastVec2->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);
    delete resultLastVec1;
    delete resultValueSetVec1;
    delete resultLastVec2;
    delete lastIgnoreNullFactory;
}

TEST(AggregatorTest, typed_aggregator_test)
{
    class TestTypeAggregator : public TypedAggregator {
    public:
        TestTypeAggregator(const FunctionType aggregateType, const DataTypes &inputTypes, const DataTypes &outputTypes,
            const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
            const bool isOverflowAsNull)
            : TypedAggregator(aggregateType, inputTypes, outputTypes, channels, inputRaw, outputPartial,
            isOverflowAsNull)
        {}

        void InitState(AggregateState *state) override
        {
            return;
        }

        size_t GetStateSize() override
        {
            return 0;
        };

        void InitStates(std::vector<AggregateState *> &groupStates) override
        {
            for (auto state : groupStates) {
                InitState(state);
            }
        };

        BaseVector *GetVector(VectorBatch *vectorBatch, const int32_t rowOffset, const int32_t rowCount,
            std::shared_ptr<NullsHelper> *nullMap)
        {
            return TypedAggregator::GetVector(vectorBatch, rowOffset, rowCount, nullMap);
        }
        void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, const int32_t rowIndex) {}

        void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
            int32_t rowOffset, int32_t rowCount)
        {}

        void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) {}

        void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
            const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
        {}

        virtual void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
            const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
        {}

        virtual void ProcessSingleInternalFilter(AggregateState &state, BaseVector *vector, Vector<bool> *booleanVector,
            const int32_t rowOffset, const int32_t rowCount, const uint8_t *nullMap)
        {
            return;
        }

        virtual void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
            const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
        {
            return;
        }
        virtual void ProcessGroupInternalFilter(std::vector<AggregateState *> &rowStates, BaseVector *vector,
            Vector<bool> *booleanVector, const int32_t rowOffset, const uint8_t *nullMap)
        {
            return;
        }
    };
    // just used to produce vector
    auto inputType = DoubleType();
    DataTypes inputTypes({ inputType }), outputTypes({ inputType });
    std::vector<int32_t> channels{ 0 };
    bool rawIn = false, partialOut = false, isOverflowAsNull = false;

    const int dataSize = 6;
    double data0[dataSize] = {0.0f, 1.0f, 2.0f, 0.0f, 1.0f, 2.0f};
    void *datas[1] = {data0};
    std::vector<DataTypePtr> sourceFieldTypes { IntType() };
    DataTypes sourceTypes(sourceFieldTypes);
    int32_t ids[] = {0, 1, 2, 3, 4, 5};
    VectorBatch *vectorBatch = new VectorBatch(dataSize);
    DataType type(inputType->GetId());
    Vector<double> *doubleVector = new Vector<double>(dataSize);
    for (int i = 0; i < dataSize; i++) {
        doubleVector->SetValue(i, data0[i]);
    }
    BaseVector *vector = VectorHelper::CreateDictionaryVector(ids, dataSize, doubleVector, inputType->GetId());
    vectorBatch->Append(vector);
    delete doubleVector;
    auto rawVectorBatch = CreateVectorBatch(inputTypes, dataSize, datas[0]);
    std::vector<DataTypePtr> typesPtr = {
        nullptr,         IntType(),        LongType(), DoubleType(),    BooleanType(),   ShortType(),
        Decimal64Type(), Decimal128Type(), IntType(),  LongType(),      IntType(),       LongType(),
        LongType(),      LongType(),       LongType(), VarcharType(15), VarcharType(16), VarcharType(15),
    };
    {
        TestTypeAggregator agg(omniruntime::op::FunctionType::OMNI_AGGREGATION_TYPE_SUM,
            DataTypes({ typesPtr.at(OMNI_SHORT) }), DataTypes({ typesPtr.at(OMNI_SHORT) }), channels, rawIn, partialOut,
            isOverflowAsNull);

        std::shared_ptr<NullsHelper> nullMap = nullptr;
        agg.GetVector(vectorBatch, 0, dataSize, &nullMap);
        agg.GetVector(rawVectorBatch, 0, dataSize, &nullMap);
        rawVectorBatch->Get(0)->SetNull(1);
        agg.GetVector(vectorBatch, 0, dataSize, &nullMap);
    }
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(rawVectorBatch);
}

template <typename T> class ExtremumValueCreator {
public:
    static constexpr T MinValue = std::numeric_limits<T>::lowest();
    static constexpr T MaxValue = std::numeric_limits<T>::max();
    // result will be (extrame_min_value, rand_data... extrame_max_value)
    std::vector<T> ProduceExtremumData(int32_t totoalNum)
    {
        std::vector<T> ret;
        ret.reserve(totoalNum);
        ret.emplace_back(MinValue);
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            for (int i = 1; i < totoalNum - 1; ++i) {
                float d = float(i) * 0.01f;
                ret.emplace_back(d);
            }
        } else if constexpr (std::is_same_v<T, Decimal128>) {
            for (int i = 1; i < totoalNum - 1; ++i) {
                Decimal128 d(i);
                ret.emplace_back(d);
            }
        } else {
            for (int i = 1; i < totoalNum - 1; ++i) {
                T d = (i % MaxValue);
                ret.emplace_back(d);
            }
        }
        ret.emplace_back(MaxValue);
        return ret;
    }

    std::vector<T> ProduceOnlyMinimum(int32_t totoalNum)
    {
        return std::vector<T>(totoalNum, MinValue);
    }

    std::vector<T> ProduceOnlyMaximum(int32_t totoalNum)
    {
        return std::vector<T>(totoalNum, MaxValue);
    }

    vec::BaseVector *ProduceVec(int32_t totoalNum)
    {
        auto value = ProduceExtremumData(totoalNum);
        auto dataPtr = const_cast<T *>(value.data());
        return TestUtil::CreateVector<T>(totoalNum, dataPtr);
    }

    vec::BaseVector *ProduceVecButOnlyMinimum(int32_t totoalNum)
    {
        auto value = ProduceOnlyMinimum(totoalNum);
        auto dataPtr = const_cast<T *>(value.data());
        return TestUtil::CreateVector<T>(totoalNum, dataPtr);
    }

    vec::BaseVector *ProduceVecButOnlyMaximum(int32_t totoalNum)
    {
        auto value = ProduceOnlyMaximum(totoalNum);
        auto dataPtr = const_cast<T *>(value.data());
        return TestUtil::CreateVector<T>(totoalNum, dataPtr);
    }
};

class DecimalCreator {
public:
    static Decimal128 MinValue;
    static Decimal128 MaxValue;
    // result will be (extrame_min_value, rand_data... extrame_max_value)
    std::vector<Decimal128> ProduceExtremumData(int32_t totoalNum)
    {
        std::vector<Decimal128> ret;
        ret.reserve(totoalNum);
        ret.emplace_back(MinValue);
        for (int i = 1; i < totoalNum - 1; ++i) {
            Decimal128 d(i);
            ret.emplace_back(d);
        }
        ret.emplace_back(MaxValue);
        return ret;
    }

    std::vector<Decimal128> ProduceOnlyMinimum(int32_t totoalNum)
    {
        return std::vector<Decimal128>(totoalNum, MinValue);
    }

    std::vector<Decimal128> ProduceOnlyMaximum(int32_t totoalNum)
    {
        return std::vector<Decimal128>(totoalNum, MaxValue);
    }

    vec::BaseVector *ProduceVec(int32_t totoalNum)
    {
        auto value = ProduceExtremumData(totoalNum);
        auto dataPtr = const_cast<type::Decimal128 *>(value.data());
        return TestUtil::CreateVector<Decimal128>(totoalNum, dataPtr);
    }

    vec::BaseVector *ProduceVecButOnlyMinimum(int32_t totoalNum)
    {
        auto value = ProduceOnlyMinimum(totoalNum);
        auto dataPtr = const_cast<type::Decimal128 *>(value.data());
        return TestUtil::CreateVector<Decimal128>(totoalNum, dataPtr);
    }

    vec::BaseVector *ProduceVecButOnlyMaximum(int32_t totoalNum)
    {
        auto value = ProduceOnlyMaximum(totoalNum);
        auto dataPtr = const_cast<type::Decimal128 *>(value.data());
        return TestUtil::CreateVector<Decimal128>(totoalNum, dataPtr);
    }
};

Decimal128 DecimalCreator::MinValue = Decimal128(type::DECIMAL128_MIN_VALUE);
Decimal128 DecimalCreator::MaxValue = Decimal128(type::DECIMAL128_MAX_VALUE);
TEST(AggregatorTest, max_agg_extrame_value_test)
{
    int32_t rowPerVecBatch = 200;
    auto maxFactory = new MaxAggregatorFactory();
    auto minFactory = new MinAggregatorFactory();
    std::vector<int32_t> channal0 = { 0 };
    std::vector<int32_t> channal1 = { 1 };
    std::vector<int32_t> channal2 = { 2 };
    std::vector<int32_t> channal3 = { 3 };
    std::vector<int32_t> channal4 = { 4 };
    std::vector<int32_t> channal5 = { 5 };
    // max test types : long + decimal + varchar + dictionary + null
    auto executionContext = std::make_unique<ExecutionContext>();
    auto maxBoolean = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()), channal0, true, false, false);
    maxBoolean->SetExecutionContext(executionContext.get());
    auto maxShort = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(ShortType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(ShortType()).get()), channal1, true, false, false);
    maxShort->SetExecutionContext(executionContext.get());
    auto maxInt = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal2, true, false, false);
    maxInt->SetExecutionContext(executionContext.get());
    auto maxLong = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal3, true, false, false);
    maxLong->SetExecutionContext(executionContext.get());
    auto maxDouble = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal4, true, false, false);
    maxDouble->SetExecutionContext(executionContext.get());
    auto maxDecimal = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()),
        *(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()), channal5, true, false, false);
    maxDecimal->SetExecutionContext(executionContext.get());

    auto minBoolean = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()), channal0, true, false, false);
    minBoolean->SetExecutionContext(executionContext.get());
    auto minShort = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(ShortType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(ShortType()).get()), channal1, true, false, false);
    minShort->SetExecutionContext(executionContext.get());
    auto minInt = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal2, true, false, false);
    minInt->SetExecutionContext(executionContext.get());
    auto minLong = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channal3, true, false, false);
    minLong->SetExecutionContext(executionContext.get());
    auto minDouble = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channal4, true, false, false);
    minDouble->SetExecutionContext(executionContext.get());
    auto minDecimal = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()),
        *(AggregatorUtil::WrapWithDataTypes(LONG_DECIMAL_TYPE).get()), channal5, true, false, false);
    minDecimal->SetExecutionContext(executionContext.get());

    ExtremumValueCreator<int8_t> int8Creator;
    auto boolVector = int8Creator.ProduceVec(rowPerVecBatch);
    auto minBoolVector = int8Creator.ProduceVecButOnlyMinimum(rowPerVecBatch);
    auto maxBoolVector = int8Creator.ProduceVecButOnlyMaximum(rowPerVecBatch);

    ExtremumValueCreator<int16_t> int16Creator;
    auto shortVector = int16Creator.ProduceVec(rowPerVecBatch);
    auto minShortVector = int16Creator.ProduceVecButOnlyMinimum(rowPerVecBatch);
    auto maxShortVector = int16Creator.ProduceVecButOnlyMaximum(rowPerVecBatch);

    ExtremumValueCreator<int32_t> int32Creator;
    auto intVector = int32Creator.ProduceVec(rowPerVecBatch);
    auto minIntVector = int32Creator.ProduceVecButOnlyMinimum(rowPerVecBatch);
    auto maxIntVector = int32Creator.ProduceVecButOnlyMaximum(rowPerVecBatch);

    ExtremumValueCreator<int64_t> int64Creator;
    auto longVector = int64Creator.ProduceVec(rowPerVecBatch);
    auto minLongVector = int64Creator.ProduceVecButOnlyMinimum(rowPerVecBatch);
    auto maxLongVector = int64Creator.ProduceVecButOnlyMaximum(rowPerVecBatch);

    ExtremumValueCreator<double> doubleCreator;
    auto doubleVector = doubleCreator.ProduceVec(rowPerVecBatch);
    auto minDoubleVector = doubleCreator.ProduceVecButOnlyMinimum(rowPerVecBatch);
    auto maxDoubleVector = doubleCreator.ProduceVecButOnlyMaximum(rowPerVecBatch);

    DecimalCreator decimalCreator;
    auto decimalVector = decimalCreator.ProduceVec(rowPerVecBatch);
    auto minDecimalVector = decimalCreator.ProduceVecButOnlyMinimum(rowPerVecBatch);
    auto maxDecimalVector = decimalCreator.ProduceVecButOnlyMaximum(rowPerVecBatch);

    VectorBatch *vectorBatch = new VectorBatch(rowPerVecBatch);
    vectorBatch->Append(boolVector);
    vectorBatch->Append(shortVector);
    vectorBatch->Append(intVector);
    vectorBatch->Append(longVector);
    vectorBatch->Append(doubleVector);
    vectorBatch->Append(decimalVector);

    VectorBatch *minVectorBatch = new VectorBatch(rowPerVecBatch);
    minVectorBatch->Append(minBoolVector);
    minVectorBatch->Append(minShortVector);
    minVectorBatch->Append(minIntVector);
    minVectorBatch->Append(minLongVector);
    minVectorBatch->Append(minDoubleVector);
    minVectorBatch->Append(minDecimalVector);

    VectorBatch *maxVectorBatch = new VectorBatch(rowPerVecBatch);
    maxVectorBatch->Append(maxBoolVector);
    maxVectorBatch->Append(maxShortVector);
    maxVectorBatch->Append(maxIntVector);
    maxVectorBatch->Append(maxLongVector);
    maxVectorBatch->Append(maxDoubleVector);
    maxVectorBatch->Append(maxDecimalVector);

    {
        auto state = NewAndInitState(maxBoolean.get());
        // process bool
        maxBoolean->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int8_t>(state.get());

        EXPECT_EQ(int8Creator.MaxValue, *maxState);
    }
    {
        // process short
        auto state = NewAndInitState(maxShort.get());
        maxShort->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int16_t>(state.get());

        EXPECT_EQ(int16Creator.MaxValue, *maxState);
    }
    {
        // process int
        auto state = NewAndInitState(maxInt.get());
        maxInt->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int32_t>(state.get());
        auto value = *maxState;
        EXPECT_EQ(int32Creator.MaxValue, value);
    }
    {
        // process int64
        auto state = NewAndInitState(maxLong.get());
        maxLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int64_t>(state.get());
        auto int64Value = *maxState;
        EXPECT_EQ(int64Creator.MaxValue, int64Value);
    }
    {
        // process double
        auto state = NewAndInitState(maxDouble.get());
        maxDouble->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<double>(state.get());

        EXPECT_EQ(doubleCreator.MaxValue, *maxState);
    }
    {
        // process decimal
        auto state = NewAndInitState(maxDecimal.get());
        maxDecimal->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<Decimal128>(state.get());

        EXPECT_EQ(decimalCreator.MaxValue, *maxState);
    }
    // test maxAggregator but only min value
    {
        auto state = NewAndInitState(maxBoolean.get());
        // process bool
        maxBoolean->ProcessGroup(state.get(), minVectorBatch, 0, minVectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int8_t>(state.get());
        auto maxValue = static_cast<int8_t>(*maxState);
        EXPECT_EQ(int8Creator.MinValue, maxValue);
    }
    {
        auto state = NewAndInitState(maxShort.get());
        maxShort->ProcessGroup(state.get(), minVectorBatch, 0, minVectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int16_t>(state.get());
        auto maxValue = *maxState;
        EXPECT_EQ(int16Creator.MinValue, maxValue);
    }
    {
        auto state = NewAndInitState(maxInt.get());
        // process bool
        maxInt->ProcessGroup(state.get(), minVectorBatch, 0, minVectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int32_t>(state.get());
        int32_t value = *maxState;
        EXPECT_EQ(int32Creator.MinValue, value);
    }
    {
        auto state = NewAndInitState(maxLong.get());
        maxLong->ProcessGroup(state.get(), minVectorBatch, 0, minVectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<int64_t>(state.get());
        auto maxValue = static_cast<int64_t>(*maxState);
        EXPECT_EQ(int64Creator.MinValue, maxValue);
    }
    {
        auto state = NewAndInitState(maxDouble.get());
        maxDouble->ProcessGroup(state.get(), minVectorBatch, 0, minVectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<double>(state.get());
        auto maxValue = *maxState;
        EXPECT_EQ(doubleCreator.MinValue, maxValue);
    }
    {
        auto state = NewAndInitState(maxDecimal.get());
        maxDecimal->ProcessGroup(state.get(), minVectorBatch, 0, minVectorBatch->GetRowCount());
        auto maxState = ExtractValueFromState<Decimal128>(state.get());

        EXPECT_EQ(decimalCreator.MinValue, *maxState);
    }


    // test min aggregator
    {
        auto state = NewAndInitState(minBoolean.get());
        minBoolean->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<int8_t>(state.get());
        auto minValue = static_cast<int8_t>(*minState);
        EXPECT_EQ(int8Creator.MinValue, minValue);
    }
    {
        // process short
        auto state = NewAndInitState(minShort.get());
        minShort->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<int16_t>(state.get());
        auto minValue = static_cast<int16_t>(*minState);
        EXPECT_EQ(int16Creator.MinValue, minValue);
    }

    {
        // process int
        auto state = NewAndInitState(minInt.get());
        minInt->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<int32_t>(state.get());
        int32_t value = *minState;
        EXPECT_EQ(int32Creator.MinValue, value);
    }
    {
        // process int64
        auto state = NewAndInitState(minLong.get());
        minLong->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<int64_t>(state.get());
        auto int64Value = static_cast<int64_t>(*minState);
        EXPECT_EQ(int64Creator.MinValue, int64Value);
    }
    {
        // process double
        auto state = NewAndInitState(minDouble.get());
        minDouble->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<double>(state.get());
        auto minValue = *minState;
        EXPECT_EQ(doubleCreator.MinValue, minValue);
    }
    {
        // process decimal
        auto state = NewAndInitState(minDecimal.get());
        minDecimal->ProcessGroup(state.get(), vectorBatch, 0, vectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<Decimal128>(state.get());
        EXPECT_EQ(decimalCreator.MinValue, *minState);
    }

    // test minAggregator but only max value
    {
        auto state = NewAndInitState(minBoolean.get());
        // process bool
        minBoolean->ProcessGroup(state.get(), maxVectorBatch, 0, maxVectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<int8_t>(state.get());
        auto minValue = static_cast<int8_t>(*minState);
        EXPECT_EQ(int8Creator.MaxValue, minValue);
    }
    {
        auto state = NewAndInitState(minShort.get());
        // process bool
        minShort->ProcessGroup(state.get(), maxVectorBatch, 0, maxVectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<int16_t>(state.get());
        auto minValue = static_cast<int16_t>(*minState);
        EXPECT_EQ(int16Creator.MaxValue, minValue);
    }
    {
        auto state = NewAndInitState(minInt.get());
        minInt->ProcessGroup(state.get(), maxVectorBatch, 0, maxVectorBatch->GetRowCount());
        auto* int32Value = reinterpret_cast<int32_t *>(state.get());
        int32_t value = *int32Value;
        EXPECT_EQ(int32Creator.MaxValue, value);
    }
    {
        auto state = NewAndInitState(minLong.get());
        minLong->ProcessGroup(state.get(), maxVectorBatch, 0, maxVectorBatch->GetRowCount());
        auto minValue = *reinterpret_cast<int64_t *>(state.get());
        EXPECT_EQ(int64Creator.MaxValue, minValue);
    }
    {
        auto state = NewAndInitState(minDouble.get());
        minDouble->ProcessGroup(state.get(), maxVectorBatch, 0, maxVectorBatch->GetRowCount());
        auto minValue = *reinterpret_cast<double *>(state.get());
        EXPECT_EQ(doubleCreator.MaxValue, minValue);
    }
    {
        auto state = NewAndInitState(minDecimal.get());
        minDecimal->ProcessGroup(state.get(), maxVectorBatch, 0, maxVectorBatch->GetRowCount());
        auto minState = ExtractValueFromState<Decimal128>(state.get());
        auto minValue = *minState;
        EXPECT_EQ(decimalCreator.MaxValue, minValue);
    }

    VectorHelper::FreeVecBatch(maxVectorBatch);
    VectorHelper::FreeVecBatch(minVectorBatch);
    VectorHelper::FreeVecBatch(vectorBatch);
    delete maxFactory;
    delete minFactory;
}

void CreateGroupBySingleSpillData(
    VectorBatch*& vecBatch0,
    VectorBatch*& vecBatch1,
    VectorBatch*& vecBatch2,
    VectorBatch*& expectVecBatch)
{
    // 1 : 1+2+3+1+2+3+4+5=21
    // null : 4+5+1+2+3+4+5=24
    int64_t data01[] = {1, 1, 1, 0, 0};
    int64_t data02[] = {1, 2, 3, 4, 5};
    int64_t data11[] = {0, 0, 0, 0, 0};
    int64_t data12[] = {1, 2, 3, 4, 5};
    int64_t data21[] = {1, 1, 1, 1, 1};
    int64_t data22[] = {1, 2, 3, 4, 5};

    DataTypes dataTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    vecBatch0 = CreateVectorBatch(dataTypes, 5, data01, data02);
    vecBatch1 = CreateVectorBatch(dataTypes, 5, data11, data12);
    vecBatch2 = CreateVectorBatch(dataTypes, 5, data21, data22);

    vecBatch0->Get(0)->SetNull(3);
    vecBatch0->Get(0)->SetNull(4);
    vecBatch1->Get(0)->SetNulls(0, true, 5);

    int64_t expectData01[] = {1, 0};
    int64_t expectData02[] = {21, 24};
    DataTypes expectDataTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    expectVecBatch = CreateVectorBatch(expectDataTypes, 2, expectData01, expectData02);
    expectVecBatch->Get(0)->SetNull(1);
}

TEST(HashAggregationOperatorTest, test_hashagg_groupbysingle_spill)
{
    VectorBatch *vecBatch0 = nullptr;
    VectorBatch *vecBatch1 = nullptr;
    VectorBatch *vecBatch2 = nullptr;
    VectorBatch *expectVecBatch1 = nullptr;

    CreateGroupBySingleSpillData(vecBatch0, vecBatch1, vecBatch2, expectVecBatch1);

    std::vector<uint32_t> groupByCol({0});
    DataTypes groupInputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<uint32_t> aggInputCols({1});
    auto aggInputColsWrap = AggregatorUtil::WrapWithVector(aggInputCols);
    DataTypes aggInputTypes(std::vector<DataTypePtr>({ LongType() }));
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskColsVector = { static_cast<uint32_t>(-1) };
    auto inputRaws = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartials = std::vector<bool>(aggFuncTypes.size(), true);
    SparkSpillConfig spillConfig(GenerateSpillPath(), INT32_MAX, 0);
    OperatorConfig operatorConfig(spillConfig);
    auto hashAggOperatorFactory = new HashAggregationOperatorFactory(groupByCol, groupInputTypes, aggInputColsWrap,
                                                                     aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypes, maskColsVector, inputRaws, outputPartials, operatorConfig);
    hashAggOperatorFactory->Init();
    auto *hashAggOperator = static_cast<HashAggregationOperator *>(hashAggOperatorFactory->CreateOperator());
    hashAggOperator->AddInput(vecBatch0);
    hashAggOperator->AddInput(vecBatch1);
    hashAggOperator->AddInput(vecBatch2);
    std::vector<VectorBatch *> result;
    while (hashAggOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        hashAggOperator->GetOutput(&outputVecBatch);
        result.emplace_back(outputVecBatch);
    }

    EXPECT_EQ(1, result.size());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(result[0], expectVecBatch1));

    omniruntime::op::Operator::DeleteOperator(hashAggOperator);
    delete hashAggOperatorFactory;
    VectorHelper::FreeVecBatches(result);
    VectorHelper::FreeVecBatch(expectVecBatch1);
}

// Array hash path + COUNT DISTINCT spill. Regression test for the issue where the
// array hash path returned before the NeedSpill() check, so low-cardinality int
// group keys never spilled and per-group distinct sets grew unbounded (OOM with
// large input). Spills on every input (elements threshold 2, two groups), forcing
// the arrayTable -> fixed hash map migration inside SpillHashMap and the disk
// merge path in GetOutput. Complete mode: input raw int, output int64 count.
TEST(HashAggregationOperatorTest, test_hashagg_arrayhash_countdistinct_spill)
{
    auto prevRule = ConfigUtil::GetAggHashTableRule();
    ConfigUtil::SetAggHashTableRule(AggHashTableRule::ARRAY);

    // 3 batches over 3 groups; distinct sets per group: {10,20,30,40}/{10,20}/{30,50}
    int32_t groupKeys0[] = {1, 1, 2};
    int64_t values0[] = {10, 20, 10};
    int32_t groupKeys1[] = {2, 1, 3};
    int64_t values1[] = {20, 30, 30};
    int32_t groupKeys2[] = {1, 2, 3};
    int64_t values2[] = {40, 20, 50};
    DataTypes dataTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    VectorBatch *vecBatch0 = CreateVectorBatch(dataTypes, 3, groupKeys0, values0);
    VectorBatch *vecBatch1 = CreateVectorBatch(dataTypes, 3, groupKeys1, values1);
    VectorBatch *vecBatch2 = CreateVectorBatch(dataTypes, 3, groupKeys2, values2);

    std::vector<uint32_t> groupByCol({0});
    DataTypes groupInputTypes(std::vector<DataTypePtr>({IntType()}));
    std::vector<uint32_t> aggInputCols({1});
    auto aggInputColsWrap = AggregatorUtil::WrapWithVector(aggInputCols);
    DataTypes aggInputTypes(std::vector<DataTypePtr>({LongType()}));
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({LongType()}));
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_COUNT_DISTINCT};
    std::vector<uint32_t> maskColsVector = {static_cast<uint32_t>(-1)};
    auto inputRaws = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartials = std::vector<bool>(aggFuncTypes.size(), false);
    // Spill once 2 groups exist, so every AddInput below goes through the spill
    // path (first one via arrayTable migration, later ones via the input spill).
    SparkSpillConfig spillConfig(GenerateSpillPath(), 2);
    OperatorConfig operatorConfig(spillConfig);
    auto hashAggOperatorFactory = new HashAggregationOperatorFactory(groupByCol, groupInputTypes, aggInputColsWrap,
                                                                     aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypes, maskColsVector, inputRaws, outputPartials, operatorConfig);
    hashAggOperatorFactory->Init();
    auto *hashAggOperator = static_cast<HashAggregationOperator *>(hashAggOperatorFactory->CreateOperator());

    hashAggOperator->AddInput(vecBatch0);
    hashAggOperator->AddInput(vecBatch1);
    hashAggOperator->AddInput(vecBatch2);
    std::vector<VectorBatch *> result;
    while (hashAggOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        hashAggOperator->GetOutput(&outputVecBatch);
        result.emplace_back(outputVecBatch);
    }

    // Spill must have actually happened on the array hash path.
    EXPECT_GT(hashAggOperator->GetSpilledBytes(), 0);

    int32_t expectData1[] = {1, 2, 3};
    int64_t expectData2[] = {4, 2, 2};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, 3, expectData1, expectData2);

    EXPECT_EQ(1, result.size());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(result[0], expectVecBatch));

    omniruntime::op::Operator::DeleteOperator(hashAggOperator);
    delete hashAggOperatorFactory;
    VectorHelper::FreeVecBatches(result);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(vecBatch0);
    VectorHelper::FreeVecBatch(vecBatch1);
    VectorHelper::FreeVecBatch(vecBatch2);

    ConfigUtil::SetAggHashTableRule(prevRule);
}

TEST(HashAggregationOperatorTest, test_step) {
        std::vector<uint32_t> groupByCol({0});
        DataTypes groupInputTypes(std::vector<DataTypePtr>({LongType()}));
        std::vector<uint32_t> aggInputCols({1});
        auto aggInputColsWrap = AggregatorUtil::WrapWithVector(aggInputCols);
        DataTypes aggInputTypes(std::vector<DataTypePtr>({ LongType() }));
        auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
        DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType() }));
        auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
        std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM };
        std::vector<uint32_t> maskColsVector = { static_cast<uint32_t>(-1) };
        auto inputRaws = std::vector<bool>(aggFuncTypes.size(), true);
        auto outputPartials = std::vector<bool>(aggFuncTypes.size(), true);
        SparkSpillConfig spillConfig(GenerateSpillPath(), INT32_MAX, 0);
        OperatorConfig operatorConfig(spillConfig);
        std::vector<int8_t> hasAggFilters = { static_cast<int8_t>(-1) };
        auto hashAggOperatorFactory = new HashAggregationOperatorFactory(groupByCol, groupInputTypes, aggInputColsWrap,
                                                                         aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypes, maskColsVector,
                                                                         inputRaws, outputPartials, hasAggFilters, operatorConfig,AggregationNode::Step::K_PARTIAL);
        hashAggOperatorFactory->Init();
        auto *hashAggOperator = static_cast<HashAggregationOperator *>(hashAggOperatorFactory->CreateOperator());
        EXPECT_EQ(true, hashAggOperator->IsStepPartials());

        auto hashAggOperatorFactory2 = new HashAggregationOperatorFactory(groupByCol, groupInputTypes, aggInputColsWrap,
                                                                    aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypes, maskColsVector,
                                                                    inputRaws, outputPartials, hasAggFilters, operatorConfig,AggregationNode::Step::K_SINGLE);
        hashAggOperatorFactory2->Init();
        auto *hashAggOperator2 = static_cast<HashAggregationOperator *>(hashAggOperatorFactory2->CreateOperator());
        EXPECT_EQ(false, hashAggOperator2->IsStepPartials());
        omniruntime::op::Operator::DeleteOperator(hashAggOperator);
        delete hashAggOperatorFactory;
        omniruntime::op::Operator::DeleteOperator(hashAggOperator2);
        delete hashAggOperatorFactory2;
}

// corr: Pearson correlation. Raw input 2 columns (numeric), output 1 double.
// Merge: 1 container (Spark order n,xAvg,yAvg,ck,xMk,yMk all Double). Partial output: same Spark order.
// Test data: x = [0,1,2,3,4], y = [0,1,4,9,16] => corr ≈ 0.9589
TEST(AggregattorTest, corr_aggregation_test) {
    const int32_t rowNum = 5;
    op::CorrAggregatorFactory corrFactory;
    DataTypes corrInputTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes corrOutputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1};
    auto corrAgg = corrFactory.CreateAggregator(corrInputTypes, corrOutputTypes, channels, true, false, false);
    ASSERT_NE(corrAgg.get(), nullptr);

    Vector<int64_t> *inputCol1 = new Vector<int64_t>(rowNum);
    Vector<int64_t> *inputCol2 = new Vector<int64_t>(rowNum);
    for (int i = 0; i < rowNum; ++i) {
        inputCol1->SetValue(i, static_cast<int64_t>(i));
        inputCol2->SetValue(i, static_cast<int64_t>(i * i));
    }
    VectorBatch *vectorBatch = new VectorBatch(2);
    vectorBatch->Append(inputCol1);
    vectorBatch->Append(inputCol2);

    auto state = NewAndInitState(corrAgg.get());
    corrAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    Vector<double> outputVec(1);
    std::vector<BaseVector *> extractVectors = {&outputVec};
    corrAgg->ExtractValues(state.get(), extractVectors, 0);
    double result = outputVec.GetValue(0);
    EXPECT_NEAR(result, 0.95892660297076826, 1e-9);

    delete vectorBatch;
}

// covar_pop: population covariance. Raw input 2 DOUBLE columns, output 1 double.
// Merge: 1 container (Spark order n,xAvg,yAvg,ck all Double). Partial output: same Spark order. Final = c2/count.
// Test: x=[0,1,2,3,4], y=[0,1,4,9,16] => covar_pop = 8
TEST(AggregattorTest, covar_pop_aggregation_test) {
    const int32_t rowNum = 5;
    op::CovarPopAggregatorFactory covarPopFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1};
    auto covarPopAgg = covarPopFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(covarPopAgg.get(), nullptr);

    Vector<double> *col1 = new Vector<double>(rowNum);
    Vector<double> *col2 = new Vector<double>(rowNum);
    for (int i = 0; i < rowNum; ++i) {
        col1->SetValue(i, static_cast<double>(i));
        col2->SetValue(i, static_cast<double>(i * i));
    }
    VectorBatch *vectorBatch = new VectorBatch(2);
    vectorBatch->Append(col1);
    vectorBatch->Append(col2);

    auto state = NewAndInitState(covarPopAgg.get());
    covarPopAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    Vector<double> outputVec(1);
    std::vector<BaseVector *> extractVectors = {&outputVec};
    covarPopAgg->ExtractValues(state.get(), extractVectors, 0);
    double result = outputVec.GetValue(0);
    EXPECT_NEAR(result, 8.0, 1e-9);

    delete vectorBatch;
}

// covar_samp: sample covariance. Raw input 2 DOUBLE columns, output 1 double.
// Merge: 1 container (Spark order n,xAvg,yAvg,ck all Double). Partial output: same Spark order. Final = c2/(count-1).
// Same data => covar_samp = 10
TEST(AggregattorTest, covar_samp_aggregation_test) {
    const int32_t rowNum = 5;
    op::CovarSampAggregatorFactory covarSampFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1};
    auto covarSampAgg = covarSampFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(covarSampAgg.get(), nullptr);

    Vector<double> *col1 = new Vector<double>(rowNum);
    Vector<double> *col2 = new Vector<double>(rowNum);
    for (int i = 0; i < rowNum; ++i) {
        col1->SetValue(i, static_cast<double>(i));
        col2->SetValue(i, static_cast<double>(i * i));
    }
    VectorBatch *vectorBatch = new VectorBatch(2);
    vectorBatch->Append(col1);
    vectorBatch->Append(col2);

    auto state = NewAndInitState(covarSampAgg.get());
    covarSampAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    Vector<double> outputVec(1);
    std::vector<BaseVector *> extractVectors = {&outputVec};
    covarSampAgg->ExtractValues(state.get(), extractVectors, 0);
    double result = outputVec.GetValue(0);
    EXPECT_NEAR(result, 10.0, 1e-9);

    delete vectorBatch;
}

// corr merge from 6 RAW Double columns (Gluten: Spark order n, xAvg, yAvg, ck, xMk, yMk). Same data => corr ≈ 0.9589
TEST(AggregattorTest, corr_merge_from_6_columns_test) {
    const int32_t rowNum = 1;
    op::CorrAggregatorFactory corrFactory;
    DataTypes corrInputTypes(std::vector<DataTypePtr>{
        DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes corrOutputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1, 2, 3, 4, 5};

    auto *vN = new Vector<double>(rowNum);
    auto *vXAvg = new Vector<double>(rowNum);
    auto *vYAvg = new Vector<double>(rowNum);
    auto *vCk = new Vector<double>(rowNum);
    auto *vXMk = new Vector<double>(rowNum);
    auto *vYMk = new Vector<double>(rowNum);
    // Spark partial for x=[0,1,2,3,4], y=[0,1,4,9,16]: n=5, xAvg=2, yAvg=6, ck=40, xMk=10, yMk=174
    vN->SetValue(0, 5.0);
    vXAvg->SetValue(0, 2.0);
    vYAvg->SetValue(0, 6.0);
    vCk->SetValue(0, 40.0);
    vXMk->SetValue(0, 10.0);
    vYMk->SetValue(0, 174.0);

    VectorBatch *vectorBatch = new VectorBatch(6);
    vectorBatch->Append(vN);
    vectorBatch->Append(vXAvg);
    vectorBatch->Append(vYAvg);
    vectorBatch->Append(vCk);
    vectorBatch->Append(vXMk);
    vectorBatch->Append(vYMk);

    auto corrAgg = corrFactory.CreateAggregator(corrInputTypes, corrOutputTypes, channels, false, false, false);
    ASSERT_NE(corrAgg.get(), nullptr);

    auto state = NewAndInitState(corrAgg.get());
    corrAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    Vector<double> outputVec(1);
    std::vector<BaseVector *> extractVectors = {&outputVec};
    corrAgg->ExtractValues(state.get(), extractVectors, 0);
    double result = outputVec.GetValue(0);
    EXPECT_NEAR(result, 0.95892660297076826, 1e-9);

    delete vectorBatch;
}

// covar_pop merge from 4 RAW Double columns (Gluten: Spark order n, xAvg, yAvg, ck). Same data => covar_pop = 8
TEST(AggregattorTest, covar_pop_merge_from_4_columns_test) {
    const int32_t rowNum = 1;
    op::CovarPopAggregatorFactory covarPopFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{
        DoubleType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1, 2, 3};

    auto *vN = new Vector<double>(rowNum);
    auto *vXAvg = new Vector<double>(rowNum);
    auto *vYAvg = new Vector<double>(rowNum);
    auto *vCk = new Vector<double>(rowNum);
    vN->SetValue(0, 5.0);
    vXAvg->SetValue(0, 2.0);
    vYAvg->SetValue(0, 6.0);
    vCk->SetValue(0, 40.0);

    VectorBatch *vectorBatch = new VectorBatch(4);
    vectorBatch->Append(vN);
    vectorBatch->Append(vXAvg);
    vectorBatch->Append(vYAvg);
    vectorBatch->Append(vCk);

    auto covarPopAgg = covarPopFactory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(covarPopAgg.get(), nullptr);

    auto state = NewAndInitState(covarPopAgg.get());
    covarPopAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    Vector<double> outputVec(1);
    std::vector<BaseVector *> extractVectors = {&outputVec};
    covarPopAgg->ExtractValues(state.get(), extractVectors, 0);
    EXPECT_NEAR(outputVec.GetValue(0), 8.0, 1e-9);

    delete vectorBatch;
}

// covar_samp merge from 4 RAW Double columns (Gluten: Spark order n, xAvg, yAvg, ck). Same data => covar_samp = 10
TEST(AggregattorTest, covar_samp_merge_from_4_columns_test) {
    const int32_t rowNum = 1;
    op::CovarSampAggregatorFactory covarSampFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{
        DoubleType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1, 2, 3};

    auto *vN = new Vector<double>(rowNum);
    auto *vXAvg = new Vector<double>(rowNum);
    auto *vYAvg = new Vector<double>(rowNum);
    auto *vCk = new Vector<double>(rowNum);
    vN->SetValue(0, 5.0);
    vXAvg->SetValue(0, 2.0);
    vYAvg->SetValue(0, 6.0);
    vCk->SetValue(0, 40.0);

    VectorBatch *vectorBatch = new VectorBatch(4);
    vectorBatch->Append(vN);
    vectorBatch->Append(vXAvg);
    vectorBatch->Append(vYAvg);
    vectorBatch->Append(vCk);

    auto covarSampAgg = covarSampFactory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(covarSampAgg.get(), nullptr);

    auto state = NewAndInitState(covarSampAgg.get());
    covarSampAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    Vector<double> outputVec(1);
    std::vector<BaseVector *> extractVectors = {&outputVec};
    covarSampAgg->ExtractValues(state.get(), extractVectors, 0);
    EXPECT_NEAR(outputVec.GetValue(0), 10.0, 1e-9);

    delete vectorBatch;
}

// corr ExtractValuesForSpill: 写出 partial 状态 (n, xAvg, yAvg, ck, xMk, yMk)，与 GetSpillType 一致
TEST(AggregattorTest, corr_extract_values_for_spill_test) {
    const int32_t rowNum = 5;
    op::CorrAggregatorFactory corrFactory;
    DataTypes corrInputTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes corrOutputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1};
    auto corrAgg = corrFactory.CreateAggregator(corrInputTypes, corrOutputTypes, channels, true, false, false);
    ASSERT_NE(corrAgg.get(), nullptr);

    Vector<int64_t> *inputCol1 = new Vector<int64_t>(rowNum);
    Vector<int64_t> *inputCol2 = new Vector<int64_t>(rowNum);
    for (int i = 0; i < rowNum; ++i) {
        inputCol1->SetValue(i, static_cast<int64_t>(i));
        inputCol2->SetValue(i, static_cast<int64_t>(i * i));
    }
    VectorBatch *vectorBatch = new VectorBatch(2);
    vectorBatch->Append(inputCol1);
    vectorBatch->Append(inputCol2);

    auto state = NewAndInitState(corrAgg.get());
    corrAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    auto spillTypes = corrAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 6u);
    const int32_t spillRowCount = 1;
    Vector<double> vN(spillRowCount), vXAvg(spillRowCount), vYAvg(spillRowCount);
    Vector<double> vCk(spillRowCount), vXMk(spillRowCount), vYMk(spillRowCount);
    std::vector<BaseVector *> spillVectors = {&vN, &vXAvg, &vYAvg, &vCk, &vXMk, &vYMk};
    std::vector<AggregateState *> groupStates = {state.get()};
    corrAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(vN.IsNull(0));
    EXPECT_NEAR(vN.GetValue(0), 5.0, 1e-9);
    EXPECT_NEAR(vXAvg.GetValue(0), 2.0, 1e-9);
    EXPECT_NEAR(vYAvg.GetValue(0), 6.0, 1e-9);
    EXPECT_NEAR(vCk.GetValue(0), 40.0, 1e-9);
    EXPECT_NEAR(vXMk.GetValue(0), 10.0, 1e-9);
    EXPECT_NEAR(vYMk.GetValue(0), 174.0, 1e-9);

    delete vectorBatch;
}

// covar_pop ExtractValuesForSpill: 写出 partial 状态 (n, meanX, meanY, c2)
TEST(AggregattorTest, covar_pop_extract_values_for_spill_test) {
    const int32_t rowNum = 5;
    op::CovarPopAggregatorFactory covarPopFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1};
    auto covarPopAgg = covarPopFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(covarPopAgg.get(), nullptr);

    Vector<double> *col1 = new Vector<double>(rowNum);
    Vector<double> *col2 = new Vector<double>(rowNum);
    for (int i = 0; i < rowNum; ++i) {
        col1->SetValue(i, static_cast<double>(i));
        col2->SetValue(i, static_cast<double>(i * i));
    }
    VectorBatch *vectorBatch = new VectorBatch(2);
    vectorBatch->Append(col1);
    vectorBatch->Append(col2);

    auto state = NewAndInitState(covarPopAgg.get());
    covarPopAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    auto spillTypes = covarPopAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 4u);
    const int32_t spillRowCount = 1;
    Vector<double> vN(spillRowCount), vMeanX(spillRowCount), vMeanY(spillRowCount), vC2(spillRowCount);
    std::vector<BaseVector *> spillVectors = {&vN, &vMeanX, &vMeanY, &vC2};
    std::vector<AggregateState *> groupStates = {state.get()};
    covarPopAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(vN.IsNull(0));
    EXPECT_NEAR(vN.GetValue(0), 5.0, 1e-9);
    EXPECT_NEAR(vMeanX.GetValue(0), 2.0, 1e-9);
    EXPECT_NEAR(vMeanY.GetValue(0), 6.0, 1e-9);
    EXPECT_NEAR(vC2.GetValue(0), 40.0, 1e-9);

    delete vectorBatch;
}

// covar_samp ExtractValuesForSpill: 写出 partial 状态 (n, meanX, meanY, c2)
TEST(AggregattorTest, covar_samp_extract_values_for_spill_test) {
    const int32_t rowNum = 5;
    op::CovarSampAggregatorFactory covarSampFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    std::vector<int32_t> channels = {0, 1};
    auto covarSampAgg = covarSampFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(covarSampAgg.get(), nullptr);

    Vector<double> *col1 = new Vector<double>(rowNum);
    Vector<double> *col2 = new Vector<double>(rowNum);
    for (int i = 0; i < rowNum; ++i) {
        col1->SetValue(i, static_cast<double>(i));
        col2->SetValue(i, static_cast<double>(i * i));
    }
    VectorBatch *vectorBatch = new VectorBatch(2);
    vectorBatch->Append(col1);
    vectorBatch->Append(col2);

    auto state = NewAndInitState(covarSampAgg.get());
    covarSampAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    auto spillTypes = covarSampAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 4u);
    const int32_t spillRowCount = 1;
    Vector<double> vN(spillRowCount), vMeanX(spillRowCount), vMeanY(spillRowCount), vC2(spillRowCount);
    std::vector<BaseVector *> spillVectors = {&vN, &vMeanX, &vMeanY, &vC2};
    std::vector<AggregateState *> groupStates = {state.get()};
    covarSampAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(vN.IsNull(0));
    EXPECT_NEAR(vN.GetValue(0), 5.0, 1e-9);
    EXPECT_NEAR(vMeanX.GetValue(0), 2.0, 1e-9);
    EXPECT_NEAR(vMeanY.GetValue(0), 6.0, 1e-9);
    EXPECT_NEAR(vC2.GetValue(0), 40.0, 1e-9);

    delete vectorBatch;
}

// bit_and ExtractValuesForSpill: 写出当前 value，unspill 时再 BitAnd 合并
TEST(AggregattorTest, bit_and_extract_values_for_spill_test) {
    const int32_t rowNum = 3;
    op::BitAndAggregatorFactory bitAndFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<int32_t> channels = {0};
    auto bitAndAgg = bitAndFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(bitAndAgg.get(), nullptr);

    Vector<int64_t> *col = new Vector<int64_t>(rowNum);
    col->SetValue(0, 3);
    col->SetValue(1, 5);
    col->SetValue(2, 7);
    VectorBatch *vectorBatch = new VectorBatch(1);
    vectorBatch->Append(col);

    auto state = NewAndInitState(bitAndAgg.get());
    bitAndAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    auto spillTypes = bitAndAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    Vector<int64_t> spillVec(1);
    std::vector<BaseVector *> spillVectors = {&spillVec};
    std::vector<AggregateState *> groupStates = {state.get()};
    bitAndAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(spillVec.IsNull(0));
    EXPECT_EQ(spillVec.GetValue(0), 3 & 5 & 7);  // 1

    delete vectorBatch;
}

// bit_or ExtractValuesForSpill
TEST(AggregattorTest, bit_or_extract_values_for_spill_test) {
    const int32_t rowNum = 3;
    op::BitOrAggregatorFactory bitOrFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<int32_t> channels = {0};
    auto bitOrAgg = bitOrFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(bitOrAgg.get(), nullptr);

    Vector<int64_t> *col = new Vector<int64_t>(rowNum);
    col->SetValue(0, 3);
    col->SetValue(1, 5);
    col->SetValue(2, 7);
    VectorBatch *vectorBatch = new VectorBatch(1);
    vectorBatch->Append(col);

    auto state = NewAndInitState(bitOrAgg.get());
    bitOrAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    auto spillTypes = bitOrAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    Vector<int64_t> spillVec(1);
    std::vector<BaseVector *> spillVectors = {&spillVec};
    std::vector<AggregateState *> groupStates = {state.get()};
    bitOrAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(spillVec.IsNull(0));
    EXPECT_EQ(spillVec.GetValue(0), 3 | 5 | 7);  // 7

    delete vectorBatch;
}

// bit_xor ExtractValuesForSpill
TEST(AggregattorTest, bit_xor_extract_values_for_spill_test) {
    const int32_t rowNum = 3;
    op::BitXorAggregatorFactory bitXorFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<int32_t> channels = {0};
    auto bitXorAgg = bitXorFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(bitXorAgg.get(), nullptr);

    Vector<int64_t> *col = new Vector<int64_t>(rowNum);
    col->SetValue(0, 3);
    col->SetValue(1, 5);
    col->SetValue(2, 7);
    VectorBatch *vectorBatch = new VectorBatch(1);
    vectorBatch->Append(col);

    auto state = NewAndInitState(bitXorAgg.get());
    bitXorAgg->ProcessGroup(state.get(), vectorBatch, 0, rowNum);

    auto spillTypes = bitXorAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    Vector<int64_t> spillVec(1);
    std::vector<BaseVector *> spillVectors = {&spillVec};
    std::vector<AggregateState *> groupStates = {state.get()};
    bitXorAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(spillVec.IsNull(0));
    EXPECT_EQ(spillVec.GetValue(0), 3 ^ 5 ^ 7);  // 1

    delete vectorBatch;
}

// ---- ConstVector tests: ProcessGroup with ConstVector input for each aggregator ----

// Sum aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, sum_const_vector_long_test)
{
    const int32_t rowCount = 5;
    auto sumFactory = new SumAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumLong = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    sumLong->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 10
    auto *constCol = new ConstVector<int64_t>(10L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);
    EXPECT_EQ(rowCount, vecBatch->GetRowCount());

    auto state = NewAndInitState(sumLong.get());
    sumLong->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto outputVector = VectorHelper::CreateVector(OMNI_FLAT, DataTypeId::OMNI_LONG, 1);
    std::vector<BaseVector *> extractVectors{outputVector};
    sumLong->ExtractValues(state.get(), extractVectors, 0);

    // Sum of 5 * 10 = 50
    EXPECT_FALSE(extractVectors[0]->IsNull(0));
    EXPECT_EQ(50L, dynamic_cast<Vector<int64_t> *>(extractVectors[0])->GetValue(0));

    delete outputVector;
    VectorHelper::FreeVecBatch(vecBatch);
    delete sumFactory;
}

// Sum aggregator with ConstVector<int32_t> input (Int -> Long)
TEST(AggregatorTest, sum_const_vector_int_test)
{
    const int32_t rowCount = 5;
    auto sumFactory = new SumAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumInt = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    sumInt->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 7
    auto *constCol = new ConstVector<int32_t>(7, OMNI_INT, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);
    EXPECT_EQ(rowCount, vecBatch->GetRowCount());

    auto state = NewAndInitState(sumInt.get());
    sumInt->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto outputVector = VectorHelper::CreateVector(OMNI_FLAT, DataTypeId::OMNI_LONG, 1);
    std::vector<BaseVector *> extractVectors{outputVector};
    sumInt->ExtractValues(state.get(), extractVectors, 0);

    // Sum of 5 * 7 = 35
    EXPECT_FALSE(extractVectors[0]->IsNull(0));
    EXPECT_EQ(35L, dynamic_cast<Vector<int64_t> *>(extractVectors[0])->GetValue(0));

    delete outputVector;
    VectorHelper::FreeVecBatch(vecBatch);
    delete sumFactory;
}

// CountColumn aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, count_column_const_vector_test)
{
    const int32_t rowCount = 5;
    auto countFactory = new CountColumnAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto countLong = countFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    countLong->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 10 (non-null)
    auto *constCol = new ConstVector<int64_t>(10L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(countLong.get());
    countLong->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto count = ExtractValueFromState<int64_t>(state.get());
    EXPECT_EQ(5, *count);

    VectorHelper::FreeVecBatch(vecBatch);
    delete countFactory;
}

// CountAll aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, count_all_const_vector_test)
{
    const int32_t rowCount = 5;
    auto countAllFactory = new CountAllAggregatorFactory();
    std::vector<int32_t> channel0 = {-1};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto countAll = countAllFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(NoneType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    countAll->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 10
    auto *constCol = new ConstVector<int64_t>(10L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(countAll.get());
    countAll->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto count = ExtractValueFromState<int64_t>(state.get());
    EXPECT_EQ(5, *count);

    VectorHelper::FreeVecBatch(vecBatch);
    delete countAllFactory;
}

// Min aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, min_const_vector_long_test)
{
    const int32_t rowCount = 5;
    auto minFactory = new MinAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto minLong = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    minLong->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 42
    auto *constCol = new ConstVector<int64_t>(42L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(minLong.get());
    minLong->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    Vector<int64_t> longResult(1);
    std::vector<BaseVector *> result;
    result.push_back(&longResult);
    minLong->ExtractValues(state.get(), result, 0);

    EXPECT_FALSE(longResult.IsNull(0));
    EXPECT_EQ(42L, longResult.GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete minFactory;
}

// Min aggregator with ConstVector<int32_t> input
TEST(AggregatorTest, min_const_vector_int_test)
{
    const int32_t rowCount = 5;
    auto minFactory = new MinAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto minInt = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    minInt->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 7
    auto *constCol = new ConstVector<int32_t>(7, OMNI_INT, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(minInt.get());
    minInt->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    Vector<int64_t> longResult(1);
    std::vector<BaseVector *> result;
    result.push_back(&longResult);
    minInt->ExtractValues(state.get(), result, 0);

    EXPECT_FALSE(longResult.IsNull(0));
    EXPECT_EQ(7L, longResult.GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete minFactory;
}

// Max aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, max_const_vector_long_test)
{
    const int32_t rowCount = 5;
    auto maxFactory = new MaxAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto maxLong = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    maxLong->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 42
    auto *constCol = new ConstVector<int64_t>(42L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(maxLong.get());
    maxLong->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto value = ExtractValueFromState<int64_t>(state.get());
    EXPECT_EQ(42L, *value);

    VectorHelper::FreeVecBatch(vecBatch);
    delete maxFactory;
}

// Max aggregator with ConstVector<int32_t> input
TEST(AggregatorTest, max_const_vector_int_test)
{
    const int32_t rowCount = 5;
    auto maxFactory = new MaxAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto maxInt = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    maxInt->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 7
    auto *constCol = new ConstVector<int32_t>(7, OMNI_INT, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(maxInt.get());
    maxInt->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto value = ExtractValueFromState<int32_t>(state.get());
    EXPECT_EQ(7, *value);

    VectorHelper::FreeVecBatch(vecBatch);
    delete maxFactory;
}

// Max aggregator with ConstVector<bool> input
TEST(AggregatorTest, max_const_vector_boolean_test)
{
    const int32_t rowCount = 5;
    auto maxFactory = new MaxAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto maxBool = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()), channel0, true, false, false);
    maxBool->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value true
    auto *constCol = new ConstVector<bool>(true, OMNI_BOOLEAN, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(maxBool.get());
    maxBool->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto value = ExtractValueFromState<bool>(state.get());
    EXPECT_EQ(true, *value);

    VectorHelper::FreeVecBatch(vecBatch);
    delete maxFactory;
}

// Min aggregator with ConstVector<bool> input
TEST(AggregatorTest, min_const_vector_boolean_test)
{
    const int32_t rowCount = 5;
    auto minFactory = new MinAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto minBool = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(BooleanType()).get()), channel0, true, false, false);
    minBool->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value false
    auto *constCol = new ConstVector<bool>(false, OMNI_BOOLEAN, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(minBool.get());
    minBool->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    Vector<bool> boolResult(1);
    std::vector<BaseVector *> result;
    result.push_back(&boolResult);
    minBool->ExtractValues(state.get(), result, 0);

    EXPECT_FALSE(boolResult.IsNull(0));
    EXPECT_EQ(false, boolResult.GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete minFactory;
}

// Average aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, avg_const_vector_long_test)
{
    const int32_t rowCount = 5;
    auto avgFactory = new AverageAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto avgLong = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(DoubleType()).get()), channel0, true, false, false);
    avgLong->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 10
    auto *constCol = new ConstVector<int64_t>(10L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(avgLong.get());
    avgLong->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    Vector<double> avgOutput(1);
    std::vector<BaseVector *> avgOutputVector;
    avgOutputVector.push_back(&avgOutput);
    avgLong->ExtractValues(state.get(), avgOutputVector, 0);

    // Average of 5 * 10 / 5 = 10.0
    EXPECT_FALSE(avgOutput.IsNull(0));
    EXPECT_TRUE(avgOutput.GetValue(0) - 10.0 <= DBL_EPSILON);

    VectorHelper::FreeVecBatch(vecBatch);
    delete avgFactory;
}

// TrySum aggregator with ConstVector<int64_t> input
TEST(AggregatorTest, try_sum_const_vector_long_test)
{
    const int32_t rowCount = 5;
    auto sumFactory = new TrySumSparkAggregatorFactory();
    std::vector<int32_t> channel0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto sumAgg = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(LongType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(LongType()).get()), channel0, true, false, false);
    sumAgg->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 10
    auto *constCol = new ConstVector<int64_t>(10L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);
    EXPECT_EQ(rowCount, vecBatch->GetRowCount());

    auto state = NewAndInitState(sumAgg.get());
    sumAgg->ProcessGroup(state.get(), vecBatch, 0, rowCount);
    auto outputVector = VectorHelper::CreateVector(OMNI_FLAT, DataTypeId::OMNI_LONG, 1);
    std::vector<BaseVector *> extractVectors{outputVector};
    sumAgg->ExtractValues(state.get(), extractVectors, 0);

    // Sum of 5 * 10 = 50
    EXPECT_FALSE(extractVectors[0]->IsNull(0));
    EXPECT_EQ(50L, dynamic_cast<Vector<int64_t> *>(extractVectors[0])->GetValue(0));

    delete outputVector;
    VectorHelper::FreeVecBatch(vecBatch);
    delete sumFactory;
}

// BitAnd aggregator with ConstVector<int64_t> input
TEST(AggregattorTest, bit_and_const_vector_test)
{
    const int32_t rowCount = 3;
    op::BitAndAggregatorFactory bitAndFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<int32_t> channels = {0};
    auto bitAndAgg = bitAndFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(bitAndAgg.get(), nullptr);

    // ConstVector: all 3 rows have value 7 (0b111)
    auto *constCol = new ConstVector<int64_t>(7L, OMNI_LONG, rowCount);
    VectorBatch *vectorBatch = new VectorBatch(rowCount);
    vectorBatch->Append(constCol);

    auto state = NewAndInitState(bitAndAgg.get());
    bitAndAgg->ProcessGroup(state.get(), vectorBatch, 0, rowCount);

    Vector<int64_t> spillVec(1);
    std::vector<BaseVector *> spillVectors = {&spillVec};
    std::vector<AggregateState *> groupStates = {state.get()};
    bitAndAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(spillVec.IsNull(0));
    EXPECT_EQ(spillVec.GetValue(0), 7L);  // 7 & 7 & 7 = 7

    delete vectorBatch;
}

// BitOr aggregator with ConstVector<int64_t> input
TEST(AggregattorTest, bit_or_const_vector_test)
{
    const int32_t rowCount = 3;
    op::BitOrAggregatorFactory bitOrFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<int32_t> channels = {0};
    auto bitOrAgg = bitOrFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(bitOrAgg.get(), nullptr);

    // ConstVector: all 3 rows have value 5 (0b101)
    auto *constCol = new ConstVector<int64_t>(5L, OMNI_LONG, rowCount);
    VectorBatch *vectorBatch = new VectorBatch(rowCount);
    vectorBatch->Append(constCol);

    auto state = NewAndInitState(bitOrAgg.get());
    bitOrAgg->ProcessGroup(state.get(), vectorBatch, 0, rowCount);

    Vector<int64_t> spillVec(1);
    std::vector<BaseVector *> spillVectors = {&spillVec};
    std::vector<AggregateState *> groupStates = {state.get()};
    bitOrAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(spillVec.IsNull(0));
    EXPECT_EQ(spillVec.GetValue(0), 5L);  // 5 | 5 | 5 = 5

    delete vectorBatch;
}

// BitXor aggregator with ConstVector<int64_t> input
TEST(AggregattorTest, bit_xor_const_vector_test)
{
    const int32_t rowCount = 3;
    op::BitXorAggregatorFactory bitXorFactory;
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<int32_t> channels = {0};
    auto bitXorAgg = bitXorFactory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(bitXorAgg.get(), nullptr);

    // ConstVector: all 3 rows have value 5 (0b101)
    auto *constCol = new ConstVector<int64_t>(5L, OMNI_LONG, rowCount);
    VectorBatch *vectorBatch = new VectorBatch(rowCount);
    vectorBatch->Append(constCol);

    auto state = NewAndInitState(bitXorAgg.get());
    bitXorAgg->ProcessGroup(state.get(), vectorBatch, 0, rowCount);

    Vector<int64_t> spillVec(1);
    std::vector<BaseVector *> spillVectors = {&spillVec};
    std::vector<AggregateState *> groupStates = {state.get()};
    bitXorAgg->ExtractValuesForSpill(groupStates, spillVectors);

    EXPECT_FALSE(spillVec.IsNull(0));
    EXPECT_EQ(spillVec.GetValue(0), 5L);  // 5 ^ 5 ^ 5 = 5 (odd count)

    delete vectorBatch;
}

// First aggregator (ignore null) with ConstVector<int32_t> input
TEST(AggregatorTest, first_int_ignorenull_const_vector_test)
{
    auto firstIgnoreNullFactory = new FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> channal0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto firstAgg = firstIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, true);
    firstAgg->SetExecutionContext(executionContext.get());

    // ConstVector: all 3 rows have value 42
    const int32_t rowCount = 3;
    auto *constCol = new ConstVector<int32_t>(42, OMNI_INT, rowCount);
    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(constCol);

    auto *resultFirstVec = new Vector<int32_t>(1);
    auto *resultValueSetVec = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resultFirstVec, resultValueSetVec};

    auto state = NewAndInitState(firstAgg.get());

    // Process row 0 => first non-null = 42
    firstAgg->ProcessGroup(state.get(), vecBatch, 0);
    firstAgg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(42, resultFirstVec->GetValue(0));
    EXPECT_TRUE(resultValueSetVec->GetValue(0));

    // Process row 1 => value should still be 42 (first already set)
    firstAgg->ProcessGroup(state.get(), vecBatch, 1);
    firstAgg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(42, resultFirstVec->GetValue(0));
    EXPECT_TRUE(resultValueSetVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultFirstVec;
    delete resultValueSetVec;
    delete firstIgnoreNullFactory;
}

// Last aggregator (ignore null) with ConstVector<int32_t> input
TEST(AggregatorTest, last_int_ignorenull_const_vector_test)
{
    auto lastIgnoreNullFactory = new LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
    std::vector<int32_t> channal0 = {0};
    auto executionContext = std::make_unique<ExecutionContext>();
    auto lastAgg = lastIgnoreNullFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(IntType()).get()),
        *(AggregatorUtil::WrapWithDataTypes(IntType()).get()), channal0, true, true);
    lastAgg->SetExecutionContext(executionContext.get());

    // ConstVector: all 3 rows have value 42
    const int32_t rowCount = 3;
    auto *constCol = new ConstVector<int32_t>(42, OMNI_INT, rowCount);
    auto *vecBatch = new VectorBatch(1);
    vecBatch->Append(constCol);

    auto *resultLastVec = new Vector<int32_t>(1);
    auto *resultValueSetVec = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resultLastVec, resultValueSetVec};

    auto state = NewAndInitState(lastAgg.get());

    // Process row 0 => last non-null = 42
    lastAgg->ProcessGroup(state.get(), vecBatch, 0);
    lastAgg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(42, resultLastVec->GetValue(0));
    EXPECT_TRUE(resultValueSetVec->GetValue(0));

    // Process row 2 => last should still be 42 (all same value)
    lastAgg->ProcessGroup(state.get(), vecBatch, 2);
    lastAgg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(42, resultLastVec->GetValue(0));
    EXPECT_TRUE(resultValueSetVec->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete resultLastVec;
    delete resultValueSetVec;
    delete lastIgnoreNullFactory;
}
}