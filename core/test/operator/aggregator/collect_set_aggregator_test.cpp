/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Unit tests for CollectSet aggregation (CollectSetAggregator and CollectSetAggregatorFactory).
 * Coverage target: collect_set_aggregator and factory >= 75%.
 */

#include <memory>
#include <vector>
#include <algorithm>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/collect_set_aggregator.h"
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

/** Call before freeing state buffer to avoid leak. CollectSet-specific (no base class DestroyState). */
static void DestroyCollectSetState(Aggregator *agg, AggregateState *state)
{
    if (auto *p = dynamic_cast<CollectSetAggregator<OMNI_INT, OMNI_INT> *>(agg)) {
        p->DestroyState(state);
    } else if (auto *q = dynamic_cast<CollectSetAggregator<OMNI_LONG, OMNI_LONG> *>(agg)) {
        q->DestroyState(state);
    } else if (auto *r = dynamic_cast<CollectSetAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128> *>(agg)) {
        r->DestroyState(state);
    } else if (auto *s = dynamic_cast<CollectSetAggregator<OMNI_BYTE, OMNI_BYTE> *>(agg)) {
        s->DestroyState(state);
    } else if (auto *t = dynamic_cast<CollectSetAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64> *>(agg)) {
        t->DestroyState(state);
    } else if (auto *d32 = dynamic_cast<CollectSetAggregator<OMNI_DATE32, OMNI_DATE32> *>(agg)) {
        d32->DestroyState(state);
    } else if (auto *d64 = dynamic_cast<CollectSetAggregator<OMNI_DATE64, OMNI_DATE64> *>(agg)) {
        d64->DestroyState(state);
    } else if (auto *ts = dynamic_cast<CollectSetAggregator<OMNI_TIMESTAMP, OMNI_TIMESTAMP> *>(agg)) {
        ts->DestroyState(state);
    }
}

// ---- CollectSetAggregatorFactory tests ----

TEST(CollectSetAggregatorTest, FactoryPartialInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_TRUE(agg->IsTypedAggregator());
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CollectSetAggregatorTest, FactoryPartialLong)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialBoolean)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{BooleanType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(BooleanType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialShort)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ShortType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ShortType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialFloat)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{FloatType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(FloatType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialDouble)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(DoubleType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialDecimal64)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal64Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal64Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialDecimal128)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal128Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal128Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialDate32)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Date32Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Date32Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CollectSetAggregatorTest, FactoryPartialDate64)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Date64Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Date64Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryPartialTimestamp)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{TimestampType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(TimestampType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryFinalArrayDate32)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(Date32Type())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Date32Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_FALSE(agg->IsInputRaw());
}

TEST(CollectSetAggregatorTest, FactoryFinalArrayTimestamp)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(TimestampType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(TimestampType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryFinalArrayInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_FALSE(agg->IsInputRaw());
}

TEST(CollectSetAggregatorTest, FactoryFinalArrayLong)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryFinalNonArrayThrows)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    EXPECT_THROW(
        factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false),
        omniruntime::exception::OmniException);
}

TEST(CollectSetAggregatorTest, FactoryPartialWithArrayInput)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, FactoryUnsupportedElementTypeThrows)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{NoneType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(NoneType())});
    EXPECT_THROW(
        factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false),
        omniruntime::exception::OmniException);
}

// VARCHAR/CHAR/VARBINARY are handled by CollectSetVarcharAggregator (see collect_set_varchar_aggregator_test.cpp).
TEST(CollectSetAggregatorTest, FactoryVarcharVarBinaryUsesVarcharAggregator)
{
    CollectSetAggregatorFactory factory;
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

// ---- CollectSetAggregator direct Create / behaviour tests ----

TEST(CollectSetAggregatorTest, CreateBasicTypeSuccess)
{
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = CollectSetAggregator<OMNI_INT, OMNI_INT>::Create(
        inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CollectSetAggregatorTest, PartialProcessGroupAndExtractValuesInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 8;
    int32_t data[rowCnt] = {10, 20, 10, 30, 20, 10, 40, 30};
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
    int32_t uniqueCount = static_cast<int32_t>(intVec->GetSize());
    EXPECT_EQ(uniqueCount, 4);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCollectSetState(agg.get(), state.get());
}

// Dictionary-encoded input: same semantics as flat; verifies isDictionary path in UpdatePartialState.
TEST(CollectSetAggregatorTest, PartialProcessGroupAndExtractValuesIntDictionary)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Dictionary: distinct values [10, 20, 30, 40]. Row values via ids: 10, 20, 10, 30, 20, 10, 40, 30 -> 4 unique.
    const int32_t dictSize = 4;
    const int32_t rowCnt = 8;
    Vector<int32_t> *dict = new Vector<int32_t>(dictSize);
    dict->SetValue(0, 10);
    dict->SetValue(1, 20);
    dict->SetValue(2, 30);
    dict->SetValue(3, 40);
    int32_t ids[rowCnt] = {0, 1, 0, 2, 1, 0, 3, 2};
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
    int32_t uniqueCount = static_cast<int32_t>(intVec->GetSize());
    EXPECT_EQ(uniqueCount, 4);

    std::vector<int32_t> extracted(uniqueCount);
    for (int32_t i = 0; i < uniqueCount; i++) {
        extracted[i] = intVec->GetValue(i);
    }
    std::sort(extracted.begin(), extracted.end());
    EXPECT_EQ(extracted[0], 10);
    EXPECT_EQ(extracted[1], 20);
    EXPECT_EQ(extracted[2], 30);
    EXPECT_EQ(extracted[3], 40);

    VectorHelper::FreeVecBatch(vecBatch);
    delete dict;
    delete outputVector;
    DestroyCollectSetState(agg.get(), state.get());
}

TEST(CollectSetAggregatorTest, PartialProcessGroupAndExtractValuesDecimal128)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal128Type()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(Decimal128Type())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 6;
    Decimal128 data[rowCnt] = {Decimal128("10"), Decimal128("20"), Decimal128("10"), Decimal128("30"),
                               Decimal128("20"), Decimal128("10")};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{Decimal128Type()}), rowCnt, data);
    ASSERT_NE(vecBatch, nullptr);
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
    int32_t uniqueCount = static_cast<int32_t>(decimalVec->GetSize());
    EXPECT_EQ(uniqueCount, 3);

    std::vector<Decimal128> extracted(uniqueCount);
    for (int32_t i = 0; i < uniqueCount; i++) {
        extracted[i] = decimalVec->GetValue(i);
    }
    std::sort(extracted.begin(), extracted.end(), [](const Decimal128 &a, const Decimal128 &b) { return a.Compare(b) < 0; });
    EXPECT_EQ(extracted[0], Decimal128("10"));
    EXPECT_EQ(extracted[1], Decimal128("20"));
    EXPECT_EQ(extracted[2], Decimal128("30"));

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCollectSetState(agg.get(), state.get());
}

TEST(CollectSetAggregatorTest, PartialProcessGroupEmptyInputExtractNull)
{
    CollectSetAggregatorFactory factory;
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
    // Empty set is [] (not SQL null), aligned with Spark / ColumnarBatchToInternalRow.
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(static_cast<Vector<int64_t> *>(elem.get())->GetSize(), 0);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCollectSetState(agg.get(), state.get());
}

TEST(CollectSetAggregatorTest, PartialProcessGroupAllNullsExtractNull)
{
    CollectSetAggregatorFactory factory;
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
    col->SetNullFlag(true);  // ensure aggregator gets nullMap (nullMap: true = null)
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
    DestroyCollectSetState(agg.get(), state.get());
}

TEST(CollectSetAggregatorTest, InitStatesMultiGroup)
{
    CollectSetAggregatorFactory factory;
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
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCollectSetState(agg.get(), groupStates[i]);
    }
}

// Grouped aggregation (with groupby): 3 groups, each group gets one value; verifies baseRowIndex fix for byte/decimal64/decimal128.
TEST(CollectSetAggregatorTest, GroupedAggregationByte)
{
    CollectSetAggregatorFactory factory;
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
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCollectSetState(agg.get(), groupStates[i]);
    }
}

TEST(CollectSetAggregatorTest, GroupedAggregationDecimal64)
{
    CollectSetAggregatorFactory factory;
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
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCollectSetState(agg.get(), groupStates[i]);
    }
}

TEST(CollectSetAggregatorTest, GroupedAggregationDecimal128)
{
    CollectSetAggregatorFactory factory;
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
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCollectSetState(agg.get(), groupStates[i]);
    }
}

TEST(CollectSetAggregatorTest, GetStateSize)
{
    std::vector<int32_t> channels = {0};
    auto agg = CollectSetAggregator<OMNI_LONG, OMNI_LONG>::Create(
        DataTypes(std::vector<DataTypePtr>{LongType()}),
        DataTypes(std::vector<DataTypePtr>{ArrayOf(LongType())}),
        channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CollectSetAggregatorTest, CreateAggregatorFactorySwitch)
{
    auto factory = CreateAggregatorFactory(OMNI_AGGREGATION_TYPE_COLLECT_SET);
    ASSERT_NE(factory, nullptr);

    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory->CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

// ---- ProcessGroupUnspill tests ----

TEST(CollectSetAggregatorTest, ProcessGroupUnspillMergeSpilledArrays)
{
    CollectSetAggregatorFactory factory;
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

    // Build spill batch: one ArrayVector column with 2 rows: row0 = [1,2,3], row1 = [4,5]
    const int32_t elemCount = 5;
    Vector<int32_t> *allElements = new Vector<int32_t>(elemCount);
    int32_t flatData[] = {1, 2, 3, 4, 5};
    for (int32_t i = 0; i < elemCount; i++) {
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

    // ArrayVector(size) does not initialize elements; SetValue would crash. Use constructor with element vector.
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
    std::vector<int32_t> row0Values = {intVec0->GetValue(0), intVec0->GetValue(1), intVec0->GetValue(2)};
    std::sort(row0Values.begin(), row0Values.end());
    EXPECT_EQ(row0Values[0], 1);
    EXPECT_EQ(row0Values[1], 2);
    EXPECT_EQ(row0Values[2], 3);

    ASSERT_FALSE(resultArray->IsNull(1));
    std::shared_ptr<BaseVector> elem1 = resultArray->GetArrayAt(1, false);
    ASSERT_NE(elem1, nullptr);
    Vector<int32_t> *intVec1 = static_cast<Vector<int32_t> *>(elem1.get());
    EXPECT_EQ(intVec1->GetSize(), 2);
    std::vector<int32_t> row1Values = {intVec1->GetValue(0), intVec1->GetValue(1)};
    std::sort(row1Values.begin(), row1Values.end());
    EXPECT_EQ(row1Values[0], 4);
    EXPECT_EQ(row1Values[1], 5);

    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCollectSetState(agg.get(), groupStates[i]);
    }
    VectorHelper::FreeVecBatch(spillBatch);
    delete outputVector;
}

TEST(CollectSetAggregatorTest, ProcessGroupUnspillSkipsNullRow)
{
    CollectSetAggregatorFactory factory;
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

    // Spill batch: row0 = [10, 20], row1 = null
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

    // ArrayVector(size) does not initialize elements; SetValue would crash. Use constructor with element vector.
    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(2, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, 2);

    ArrayVector *resultArray = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(resultArray->IsNull(0));
    std::shared_ptr<BaseVector> elem0 = resultArray->GetArrayAt(0, false);
    ASSERT_NE(elem0, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem0.get())->GetSize(), 2);

    // Row1 skipped on unspill: empty set is [], not null
    EXPECT_FALSE(resultArray->IsNull(1));
    std::shared_ptr<BaseVector> elem1 = resultArray->GetArrayAt(1, false);
    ASSERT_NE(elem1, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem1.get())->GetSize(), 0);

    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCollectSetState(agg.get(), groupStates[i]);
    }
    VectorHelper::FreeVecBatch(spillBatch);
    delete outputVector;
}

// ---- ProcessAlignAggSchema (Skip Partial) tests ----

TEST(CollectSetAggregatorTest, ProcessAlignAggSchemaRawInputSingleElementArrayPerRow)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    const int32_t rowCnt = 4;
    int32_t data[rowCnt] = {10, 20, 30, 40};
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

TEST(CollectSetAggregatorTest, ProcessAlignAggSchemaRawInputWithNullMap)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    const int32_t rowCnt = 3;
    int32_t data[rowCnt] = {1, 2, 3};
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
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem0.get())->GetValue(0), 1);
    std::shared_ptr<BaseVector> elem2 = arrayVec->GetArrayAt(2, false);
    ASSERT_NE(elem2, nullptr);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(elem2.get())->GetValue(0), 3);

    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

TEST(CollectSetAggregatorTest, ProcessAlignAggSchemaEmptyInput)
{
    CollectSetAggregatorFactory factory;
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

// Two collect_set in same aggregation: state buffer has two slots; offsets must not overlap.
TEST(CollectSetAggregatorTest, TwoCollectSetSharedStateBufferOffsets)
{
    CollectSetAggregatorFactory factory;
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
    EXPECT_EQ(static_cast<Vector<int32_t> *>(e0.get())->GetSize(), 3);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(e1.get())->GetSize(), 3);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outArr0;
    delete outArr1;
    if (auto *p = dynamic_cast<CollectSetAggregator<OMNI_INT, OMNI_INT> *>(agg0.get())) {
        p->DestroyState(stateBuffer.get());
    }
    if (auto *q = dynamic_cast<CollectSetAggregator<OMNI_INT, OMNI_INT> *>(agg1.get())) {
        q->DestroyState(stateBuffer.get());
    }
}

// collect_set(int_col) + collect_set(boolean_col): different element types must use same state slot size (8).
TEST(CollectSetAggregatorTest, TwoCollectSetDifferentTypesIntAndBoolean)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> ch0 = {0};
    std::vector<int32_t> ch1 = {1};
    auto agg0 = factory.CreateAggregator(
        DataTypes(std::vector<DataTypePtr>{IntType()}), DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}),
        ch0, true, true, false);
    auto agg1 = factory.CreateAggregator(
        DataTypes(std::vector<DataTypePtr>{BooleanType()}), DataTypes(std::vector<DataTypePtr>{ArrayOf(BooleanType())}),
        ch1, true, true, false);
    ASSERT_NE(agg0, nullptr);
    ASSERT_NE(agg1, nullptr);

    EXPECT_EQ(agg0->GetStateSize(), 8u);
    EXPECT_EQ(agg1->GetStateSize(), 8u);

    agg0->SetStateOffset(0);
    agg1->SetStateOffset(static_cast<int32_t>(agg0->GetStateSize()));
    size_t totalSize = agg0->GetStateSize() + agg1->GetStateSize();
    ASSERT_EQ(totalSize, 16u);

    auto stateBuffer = std::make_unique<AggregateState[]>(totalSize);
    agg0->InitState(stateBuffer.get());
    agg1->InitState(stateBuffer.get());

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg0->SetExecutionContext(executionContext.get());
    agg1->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 4;
    int32_t col0[rowCnt] = {1, 2, 1, 3};
    bool col1[rowCnt] = {true, false, true, true};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{IntType(), BooleanType()}), rowCnt, col0, col1);
    ASSERT_EQ(vecBatch->GetVectorCount(), 2);

    agg0->ProcessGroup(stateBuffer.get(), vecBatch, 0, rowCnt);
    agg1->ProcessGroup(stateBuffer.get(), vecBatch, 0, rowCnt);

    std::vector<AggregateState *> groupStates = {stateBuffer.get()};
    Vector<int32_t> *outElem0 = new Vector<int32_t>(0);
    Vector<int8_t> *outElem1 = new Vector<int8_t>(0);
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
    EXPECT_EQ(static_cast<Vector<int32_t> *>(e0.get())->GetSize(), 3);
    EXPECT_EQ(static_cast<Vector<int8_t> *>(e1.get())->GetSize(), 2);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outArr0;
    delete outArr1;
    if (auto *p = dynamic_cast<CollectSetAggregator<OMNI_INT, OMNI_INT> *>(agg0.get())) {
        p->DestroyState(stateBuffer.get());
    }
    if (auto *q = dynamic_cast<CollectSetAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN> *>(agg1.get())) {
        q->DestroyState(stateBuffer.get());
    }
}

// ---- ConstVector tests ----

// CollectSet with ConstVector<int32_t> input: all same value should produce set with 1 element
TEST(CollectSetAggregatorTest, PartialProcessGroupConstVectorInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // ConstVector: all 5 rows have value 42
    const int32_t rowCount = 5;
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
    // Set should deduplicate: all 42s -> {42}
    EXPECT_EQ(intVec->GetSize(), 1);
    EXPECT_EQ(intVec->GetValue(0), 42);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCollectSetState(agg.get(), state.get());
}

// CollectSet with ConstVector<int64_t> input
TEST(CollectSetAggregatorTest, PartialProcessGroupConstVectorLong)
{
    CollectSetAggregatorFactory factory;
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
    EXPECT_EQ(longVec->GetSize(), 1);
    EXPECT_EQ(longVec->GetValue(0), 100L);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCollectSetState(agg.get(), state.get());
}
}
