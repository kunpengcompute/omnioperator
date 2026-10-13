/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Unit tests for CollectList aggregation (CollectListAggregator and CollectListAggregatorFactory).
 * CollectList preserves order and allows duplicates (unlike CollectSet).
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/collect_list_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "type/data_type.h"
#include "type/decimal128.h"
#include "util/type_util.h"
#include "util/test_util.h"
#include "operator/execution_context.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

static std::unique_ptr<AggregateState[]> NewAndInitState(Aggregator *agg, int32_t off = 0)
{
    auto state = std::make_unique<AggregateState[]>(agg->GetStateSize());
    agg->SetStateOffset(off);
    agg->InitState(state.get());
    return state;
}

static DataTypePtr ArrayOf(const DataTypePtr &elementType)
{
    return std::make_shared<ArrayType>(elementType);
}

// ---- CollectListAggregatorFactory tests ----

TEST(CollectListAggregatorTest, FactoryPartialInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_TRUE(agg->IsTypedAggregator());
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CollectListAggregatorTest, FactoryPartialLong)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectListAggregatorTest, FactoryPartialDecimal128)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal128Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal128Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectListAggregatorTest, FactoryPartialDate32)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Date32Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Date32Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CollectListAggregatorTest, FactoryPartialDate64)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Date64Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Date64Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectListAggregatorTest, FactoryPartialTimestamp)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{TimestampType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(TimestampType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectListAggregatorTest, FactoryFinalArrayDate32)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(Date32Type())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Date32Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_FALSE(agg->IsInputRaw());
}

TEST(CollectListAggregatorTest, FactoryFinalArrayTimestamp)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(TimestampType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(TimestampType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectListAggregatorTest, FactoryFinalArrayInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_FALSE(agg->IsInputRaw());
}

TEST(CollectListAggregatorTest, FactoryFinalNonArrayThrows)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    EXPECT_THROW(
        factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false),
        omniruntime::exception::OmniException);
}

TEST(CollectListAggregatorTest, CreateBasicTypeSuccess)
{
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = CollectListAggregator<OMNI_INT, OMNI_INT>::Create(
        inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

// ---- Order and duplicates: input [1,2,1,3] -> output [1,2,1,3] ----

TEST(CollectListAggregatorTest, PartialProcessGroupOrderAndDuplicatesInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 4;
    int32_t data[rowCnt] = {1, 2, 1, 3};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), rowCnt, data);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCnt);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *intVec = static_cast<Vector<int32_t> *>(elemVec.get());
    EXPECT_EQ(intVec->GetSize(), 4);
    EXPECT_EQ(intVec->GetValue(0), 1);
    EXPECT_EQ(intVec->GetValue(1), 2);
    EXPECT_EQ(intVec->GetValue(2), 1);
    EXPECT_EQ(intVec->GetValue(3), 3);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

// Dictionary-encoded input: same semantics as flat; verifies isDictionary path in UpdatePartialState.
TEST(CollectListAggregatorTest, PartialProcessGroupOrderAndDuplicatesIntDictionary)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Dictionary: distinct values [1, 2, 3]. Row values via ids: 1, 2, 1, 3 -> list [1, 2, 1, 3].
    const int32_t dictSize = 3;
    const int32_t rowCnt = 4;
    Vector<int32_t> *dict = new Vector<int32_t>(dictSize);
    dict->SetValue(0, 1);
    dict->SetValue(1, 2);
    dict->SetValue(2, 3);
    int32_t ids[rowCnt] = {0, 1, 0, 2};
    BaseVector *dictCol = VectorHelper::CreateDictionary(ids, rowCnt, dict);
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(dictCol);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCnt);
    ASSERT_EQ(vecBatch->Get(0)->GetEncoding(), vec::OMNI_DICTIONARY);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *intVec = static_cast<Vector<int32_t> *>(elemVec.get());
    EXPECT_EQ(intVec->GetSize(), 4);
    EXPECT_EQ(intVec->GetValue(0), 1);
    EXPECT_EQ(intVec->GetValue(1), 2);
    EXPECT_EQ(intVec->GetValue(2), 1);
    EXPECT_EQ(intVec->GetValue(3), 3);

    VectorHelper::FreeVecBatch(vecBatch);
    delete dict;
    delete outputVector;
}

// CollectList with Decimal128: state is std::vector<Decimal128>; order and duplicates preserved.
TEST(CollectListAggregatorTest, PartialProcessGroupOrderAndDuplicatesDecimal128)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal128Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal128Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 4;
    Decimal128 data[rowCnt] = {Decimal128("10"), Decimal128("20"), Decimal128("10"), Decimal128("30")};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{Decimal128Type()}), rowCnt, data);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCnt);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    Vector<Decimal128> *outputElements = new Vector<Decimal128>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *decimalVec = static_cast<Vector<Decimal128> *>(elemVec.get());
    EXPECT_EQ(decimalVec->GetSize(), 4);
    EXPECT_EQ(decimalVec->GetValue(0), Decimal128("10"));
    EXPECT_EQ(decimalVec->GetValue(1), Decimal128("20"));
    EXPECT_EQ(decimalVec->GetValue(2), Decimal128("10"));
    EXPECT_EQ(decimalVec->GetValue(3), Decimal128("30"));

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, PartialProcessGroupEmptyInputExtractNull)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 0;
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(new Vector<int64_t>(rowCnt));

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    Vector<int64_t> *outputElements = new Vector<int64_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(static_cast<Vector<int64_t> *>(elem.get())->GetSize(), 0);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, PartialProcessGroupAllNullsExtractNull)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 3;
    Vector<int32_t> *col = new Vector<int32_t>(rowCnt);
    col->SetNull(0);
    col->SetNull(1);
    col->SetNull(2);
    col->SetNullFlag(true);
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(col);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem.get())->GetSize(), 0);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, InitStatesMultiGroup)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t groupCount = 4;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(groupStates);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

// Grouped aggregation (with groupby): verifies baseRowIndex fix for byte/decimal64/decimal128.
TEST(CollectListAggregatorTest, GroupedAggregationByte)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ByteType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ByteType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t groupCount = 3;
    int8_t data[groupCount] = {1, 2, 1};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{ByteType()}), groupCount, data);
    ASSERT_EQ(vecBatch->GetRowCount(), groupCount);

    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf = std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(groupStates);
    agg->ProcessGroup(groupStates, vecBatch, 0);

    Vector<int8_t> *outputElements = new Vector<int8_t>(0);
    BaseVector *outputVector = new ArrayVector(groupCount, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, groupCount);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    for (int32_t g = 0; g < groupCount; g++) {
        EXPECT_FALSE(arrayVec->IsNull(g));
        std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(g, false);
        ASSERT_NE(elemVec, nullptr);
        auto *byteVec = static_cast<Vector<int8_t> *>(elemVec.get());
        EXPECT_EQ(byteVec->GetSize(), 1u);
        EXPECT_EQ(byteVec->GetValue(0), data[g]);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, GroupedAggregationDecimal64)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal64Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal64Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t groupCount = 3;
    int64_t data[groupCount] = {100, 200, 100};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{Decimal64Type()}), groupCount, data);
    ASSERT_EQ(vecBatch->GetRowCount(), groupCount);

    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf = std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(groupStates);
    agg->ProcessGroup(groupStates, vecBatch, 0);

    Vector<int64_t> *outputElements = new Vector<int64_t>(0);
    BaseVector *outputVector = new ArrayVector(groupCount, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, groupCount);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    for (int32_t g = 0; g < groupCount; g++) {
        EXPECT_FALSE(arrayVec->IsNull(g));
        std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(g, false);
        ASSERT_NE(elemVec, nullptr);
        auto *decVec = static_cast<Vector<int64_t> *>(elemVec.get());
        EXPECT_EQ(decVec->GetSize(), 1u);
        EXPECT_EQ(decVec->GetValue(0), data[g]);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, GroupedAggregationDecimal128)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal128Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal128Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t groupCount = 3;
    Decimal128 data[groupCount] = {Decimal128("10"), Decimal128("20"), Decimal128("10")};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{Decimal128Type()}), groupCount, data);
    ASSERT_EQ(vecBatch->GetRowCount(), groupCount);

    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf = std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(groupStates);
    agg->ProcessGroup(groupStates, vecBatch, 0);

    Vector<Decimal128> *outputElements = new Vector<Decimal128>(0);
    BaseVector *outputVector = new ArrayVector(groupCount, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, groupCount);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    for (int32_t g = 0; g < groupCount; g++) {
        EXPECT_FALSE(arrayVec->IsNull(g));
        std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(g, false);
        ASSERT_NE(elemVec, nullptr);
        auto *decimalVec = static_cast<Vector<Decimal128> *>(elemVec.get());
        EXPECT_EQ(decimalVec->GetSize(), 1u);
        EXPECT_EQ(decimalVec->GetValue(0), data[g]);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, GetStateSize)
{
    std::vector<int32_t> channels = {0};
    auto agg = CollectListAggregator<OMNI_LONG, OMNI_LONG>::Create(
        DataTypes(std::vector<DataTypePtr>{LongType()}),
        DataTypes(std::vector<DataTypePtr>{ArrayOf(LongType())}),
        channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CollectListAggregatorTest, CreateAggregatorFactorySwitch)
{
    auto factory = CreateAggregatorFactory(OMNI_AGGREGATION_TYPE_COLLECT_LIST);
    ASSERT_NE(factory, nullptr);

    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory->CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

// ---- ProcessGroupUnspill: order preserved ----

TEST(CollectListAggregatorTest, ProcessGroupUnspillMergeSpilledArraysOrderPreserved)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    const int32_t groupCount = 2;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->InitStates(groupStates);

    Vector<int32_t> *allElements = new Vector<int32_t>(5);
    int32_t flatData[] = {1, 2, 3, 4, 5};
    for (int32_t i = 0; i < 5; i++) {
        allElements->SetValue(i, flatData[i]);
    }
    ArrayVector *spillArray = new ArrayVector(2, std::shared_ptr<BaseVector>(allElements));
    spillArray->SetOffset(0, 0);
    spillArray->SetSize(0, 3);
    spillArray->SetSize(1, 2);

    VectorBatch *spillBatch = new VectorBatch(2);
    spillBatch->Append(spillArray);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[1], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);
    EXPECT_EQ(vectorIndex, 1);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(2, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, 2);

    ArrayVector *resultArray = static_cast<ArrayVector *>(extractVectors[0]);
    ASSERT_FALSE(resultArray->IsNull(0));
    std::shared_ptr<BaseVector> elem0 = resultArray->GetArrayAt(0, false);
    ASSERT_NE(elem0, nullptr);
    Vector<int32_t> *intVec0 = static_cast<Vector<int32_t> *>(elem0.get());
    EXPECT_EQ(intVec0->GetSize(), 3);
    EXPECT_EQ(intVec0->GetValue(0), 1);
    EXPECT_EQ(intVec0->GetValue(1), 2);
    EXPECT_EQ(intVec0->GetValue(2), 3);

    ASSERT_FALSE(resultArray->IsNull(1));
    std::shared_ptr<BaseVector> elem1 = resultArray->GetArrayAt(1, false);
    ASSERT_NE(elem1, nullptr);
    Vector<int32_t> *intVec1 = static_cast<Vector<int32_t> *>(elem1.get());
    EXPECT_EQ(intVec1->GetSize(), 2);
    EXPECT_EQ(intVec1->GetValue(0), 4);
    EXPECT_EQ(intVec1->GetValue(1), 5);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, ProcessGroupUnspillSkipsNullRow)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    const int32_t groupCount = 2;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->InitStates(groupStates);

    Vector<int32_t> *elements = new Vector<int32_t>(2);
    elements->SetValue(0, 10);
    elements->SetValue(1, 20);
    ArrayVector *spillArray = new ArrayVector(2, std::shared_ptr<BaseVector>(elements));
    spillArray->SetOffset(0, 0);
    spillArray->SetSize(0, 2);
    spillArray->SetNull(1);

    VectorBatch *spillBatch = new VectorBatch(2);
    spillBatch->Append(spillArray);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[1], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(2, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, 2);

    ArrayVector *resultArray = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(resultArray->IsNull(0));
    std::shared_ptr<BaseVector> elem0 = resultArray->GetArrayAt(0, false);
    ASSERT_NE(elem0, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem0.get())->GetSize(), 2);

    EXPECT_FALSE(resultArray->IsNull(1));
    std::shared_ptr<BaseVector> elem1 = resultArray->GetArrayAt(1, false);
    ASSERT_NE(elem1, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem1.get())->GetSize(), 0);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outputVector;
}

TEST(CollectListAggregatorTest, FactoryUnsupportedElementTypeThrows)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{NoneType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(NoneType())});
    EXPECT_THROW(
        factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false),
        omniruntime::exception::OmniException);
}

// VARCHAR/CHAR/VARBINARY are handled by CollectListVarcharAggregator (see collect_list_varchar_aggregator_test.cpp).
TEST(CollectListAggregatorTest, FactoryVarcharVarBinaryUsesVarcharAggregator)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputVarchar(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputVarchar(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto aggVarchar = factory.CreateAggregator(inputVarchar, outputVarchar, channels, true, true, false);
    ASSERT_NE(aggVarchar, nullptr);

    DataTypes inputVarbinary(std::vector<DataTypePtr>{VarBinaryType(100)});
    DataTypes outputVarbinary(std::vector<DataTypePtr>{ArrayOf(VarBinaryType(100))});
    auto aggVarbinary = factory.CreateAggregator(inputVarbinary, outputVarbinary, channels, true, true, false);
    ASSERT_NE(aggVarbinary, nullptr);
}

// ---- ProcessAlignAggSchema (Skip Partial) tests ----

TEST(CollectListAggregatorTest, ProcessAlignAggSchemaRawInputSingleElementArrayPerRow)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    const int32_t rowCnt = 4;
    int32_t data[rowCnt] = {1, 2, 1, 3};
    VectorBatch *inputBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), rowCnt, data);
    ASSERT_EQ(inputBatch->GetRowCount(), rowCnt);
    ASSERT_EQ(inputBatch->GetVectorCount(), 1);

    VectorBatch *result = new VectorBatch(rowCnt);
    agg->AlignAggSchema(result, inputBatch);

    ASSERT_EQ(result->GetVectorCount(), 1);
    ASSERT_EQ(result->GetRowCount(), rowCnt);
    BaseVector *outCol = result->Get(0);
    ASSERT_NE(outCol, nullptr);
    ASSERT_EQ(outCol->GetTypeId(), OMNI_ARRAY);
    auto *arrayVec = static_cast<ArrayVector *>(outCol);
    for (int32_t i = 0; i < rowCnt; i++) {
        EXPECT_FALSE(arrayVec->IsNull(i));
        std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(i, false);
        ASSERT_NE(elemVec, nullptr);
        EXPECT_EQ(elemVec->GetSize(), 1);
        EXPECT_EQ(static_cast<Vector<int32_t> *>(elemVec.get())->GetValue(0), data[i]);
    }

    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

TEST(CollectListAggregatorTest, ProcessAlignAggSchemaRawInputWithNullMap)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    const int32_t rowCnt = 3;
    int32_t data[rowCnt] = {5, 6, 7};
    VectorBatch *inputBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), rowCnt, data);
    inputBatch->Get(0)->SetNull(1);

    VectorBatch *result = new VectorBatch(rowCnt);
    agg->AlignAggSchema(result, inputBatch);

    ASSERT_EQ(result->GetVectorCount(), 1);
    ASSERT_EQ(result->GetRowCount(), rowCnt);
    auto *arrayVec = static_cast<ArrayVector *>(result->Get(0));
    EXPECT_FALSE(arrayVec->IsNull(0));
    EXPECT_TRUE(arrayVec->IsNull(1));
    EXPECT_FALSE(arrayVec->IsNull(2));
    std::shared_ptr<BaseVector> elem0 = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elem0, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem0.get())->GetValue(0), 5);
    std::shared_ptr<BaseVector> elem2 = arrayVec->GetArrayAt(2, false);
    ASSERT_NE(elem2, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem2.get())->GetValue(0), 7);

    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

TEST(CollectListAggregatorTest, ProcessAlignAggSchemaEmptyInput)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    VectorBatch *inputBatch = new VectorBatch(0);
    VectorBatch *result = new VectorBatch(0);
    agg->AlignAggSchema(result, inputBatch);

    ASSERT_EQ(result->GetVectorCount(), 1);
    ASSERT_EQ(result->GetRowCount(), 0);

    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

// Two collect_list in same aggregation: state buffer has two slots; offsets must not overlap.
TEST(CollectListAggregatorTest, TwoCollectListSharedStateBufferOffsets)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> ch0 = {0};
    std::vector<int32_t> ch1 = {1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType()), ArrayOf(IntType())});

    auto agg0 = factory.CreateAggregator(
        DataTypes(std::vector<DataTypePtr>{IntType()}), DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}),
        ch0, true, true, false);
    auto agg1 = factory.CreateAggregator(
        DataTypes(std::vector<DataTypePtr>{IntType()}), DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}),
        ch1, true, true, false);
    ASSERT_NE(agg0, nullptr);
    ASSERT_NE(agg1, nullptr);

    agg0->SetStateOffset(0);
    agg1->SetStateOffset(static_cast<int32_t>(agg0->GetStateSize()));
    size_t totalSize = agg0->GetStateSize() + agg1->GetStateSize();
    ASSERT_GE(totalSize, sizeof(int64_t) * 2);

    auto stateBuffer = std::make_unique<AggregateState[]>(totalSize);
    agg0->InitState(stateBuffer.get());
    agg1->InitState(stateBuffer.get());

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg0->SetExecutionContext(executionContext.get());
    agg1->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 4;
    int32_t col0[rowCnt] = {1, 2, 1, 3};
    int32_t col1[rowCnt] = {10, 20, 10, 40};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{IntType(), IntType()}), rowCnt, col0, col1);
    ASSERT_EQ(vecBatch->GetVectorCount(), 2);

    agg0->ProcessGroup(stateBuffer.get(), vecBatch, 0, rowCnt);
    agg1->ProcessGroup(stateBuffer.get(), vecBatch, 0, rowCnt);

    std::vector<AggregateState *> groupStates = {stateBuffer.get()};
    Vector<int32_t> *outElem0 = new Vector<int32_t>(0);
    Vector<int32_t> *outElem1 = new Vector<int32_t>(0);
    BaseVector *outArr0 = new ArrayVector(1, std::shared_ptr<BaseVector>(outElem0));
    BaseVector *outArr1 = new ArrayVector(1, std::shared_ptr<BaseVector>(outElem1));
    std::vector<BaseVector *> vecs0 = {outArr0};
    std::vector<BaseVector *> vecs1 = {outArr1};
    agg0->ExtractValuesBatch(groupStates, vecs0, 0, 1);
    agg1->ExtractValuesBatch(groupStates, vecs1, 0, 1);

    auto *arr0 = static_cast<ArrayVector *>(vecs0[0]);
    auto *arr1 = static_cast<ArrayVector *>(vecs1[0]);
    EXPECT_FALSE(arr0->IsNull(0));
    EXPECT_FALSE(arr1->IsNull(0));
    std::shared_ptr<BaseVector> e0 = arr0->GetArrayAt(0, false);
    std::shared_ptr<BaseVector> e1 = arr1->GetArrayAt(0, false);
    ASSERT_NE(e0, nullptr);
    ASSERT_NE(e1, nullptr);
    auto *list0 = static_cast<Vector<int32_t> *>(e0.get());
    auto *list1 = static_cast<Vector<int32_t> *>(e1.get());
    EXPECT_EQ(list0->GetSize(), 4);
    EXPECT_EQ(list1->GetSize(), 4);
    EXPECT_EQ(list0->GetValue(0), 1);
    EXPECT_EQ(list0->GetValue(1), 2);
    EXPECT_EQ(list0->GetValue(2), 1);
    EXPECT_EQ(list0->GetValue(3), 3);
    EXPECT_EQ(list1->GetValue(0), 10);
    EXPECT_EQ(list1->GetValue(1), 20);
    EXPECT_EQ(list1->GetValue(2), 10);
    EXPECT_EQ(list1->GetValue(3), 40);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outArr0;
    delete outArr1;
    if (auto *p = dynamic_cast<CollectListAggregator<OMNI_INT, OMNI_INT> *>(agg0.get())) {
        p->DestroyState(stateBuffer.get());
    }
    if (auto *q = dynamic_cast<CollectListAggregator<OMNI_INT, OMNI_INT> *>(agg1.get())) {
        q->DestroyState(stateBuffer.get());
    }
}

// ---- ConstVector tests ----

// CollectList with ConstVector<int32_t> input: all same value should produce list with all elements
TEST(CollectListAggregatorTest, PartialProcessGroupConstVectorInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // ConstVector: all 4 rows have value 42
    const int32_t rowCount = 4;
    auto *constCol = new ConstVector<int32_t>(42, OMNI_INT, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCount);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *intVec = static_cast<Vector<int32_t> *>(elemVec.get());
    // List preserves all elements: [42, 42, 42, 42]
    EXPECT_EQ(intVec->GetSize(), 4);
    for (int32_t i = 0; i < 4; ++i) {
        EXPECT_EQ(intVec->GetValue(i), 42);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

// CollectList with ConstVector<int64_t> input
TEST(CollectListAggregatorTest, PartialProcessGroupConstVectorLong)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    auto *constCol = new ConstVector<int64_t>(100L, OMNI_LONG, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int64_t> *outputElements = new Vector<int64_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *longVec = static_cast<Vector<int64_t> *>(elemVec.get());
    EXPECT_EQ(longVec->GetSize(), 3);
    for (int32_t i = 0; i < 3; ++i) {
        EXPECT_EQ(longVec->GetValue(i), 100L);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
}

}  // namespace omniruntime
