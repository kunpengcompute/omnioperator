/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Unit tests for MaxBy aggregator: factory, ProcessGroup, GetSpillType,
 *              ExtractValuesForSpill, ProcessGroupUnspill for all supported scalar type pairs.
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
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

// ---- Factory: create for each supported (col1, col2) scalar pair ----

TEST(MaxByAggregatorTest, FactoryIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryIntVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryLongLong)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryDoubleDouble)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryBooleanInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{BooleanType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{BooleanType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryShortShort)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{ShortType(), ShortType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ShortType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryFloatFloat)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{FloatType(), FloatType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{FloatType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryDecimal128Varchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{Decimal128Type(10, 2), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{Decimal128Type(10, 2)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryVarcharVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{VarcharType(10)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryCharChar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{CharType(10), CharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{CharType(10)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

// Complex type: target=ARRAY, sort key=scalar -> MaxByComplexAggregator
TEST(MaxByAggregatorTest, FactoryArrayInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    auto arrayType = std::make_shared<ArrayType>(IntType());
    DataTypes inputTypes(std::vector<DataTypePtr>{arrayType, IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{arrayType});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

// Complex type: target=ARRAY, sort key=VARCHAR -> MaxByComplexVarcharAggregator
TEST(MaxByAggregatorTest, FactoryArrayVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    auto arrayType = std::make_shared<ArrayType>(IntType());
    DataTypes inputTypes(std::vector<DataTypePtr>{arrayType, VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{arrayType});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

// Complex type: target=MAP, sort key=scalar/varchar
TEST(MaxByAggregatorTest, FactoryMapInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    auto mapType = std::make_shared<MapType>(IntType(), IntType());
    DataTypes inputTypes(std::vector<DataTypePtr>{mapType, IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{mapType});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryMapVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    auto mapType = std::make_shared<MapType>(IntType(), IntType());
    DataTypes inputTypes(std::vector<DataTypePtr>{mapType, VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{mapType});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

// Complex type: target=ROW, sort key=scalar/varchar
TEST(MaxByAggregatorTest, FactoryRowInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    std::vector<DataTypePtr> fieldTypes = {IntType()};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"f0"});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType, IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{rowType});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(MaxByAggregatorTest, FactoryRowVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    std::vector<DataTypePtr> fieldTypes = {IntType()};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"f0"});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType, VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{rowType});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_GT(agg->GetStateSize(), 0u);
}

// ---- ProcessGroup + ExtractValues: max_by picks target with maximum sort key ----
// Use outputPartial=false so ExtractValues expects a single final result vector (not target+sortKey).

TEST(MaxByAggregatorTest, ProcessGroupExtractValuesIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Rows: (target, key) = (10, 3), (20, 1), (30, 2). Max key=3 -> target=10.
    int32_t targetCol[] = {10, 20, 30};
    int32_t keyCol[] = {3, 1, 2};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{IntType(), IntType()}), 3, targetCol, keyCol);
    ASSERT_EQ(vecBatch->GetRowCount(), 3);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 10);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupExtractValuesLongLong)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    int64_t targetCol[] = {100, 200, 300};
    int64_t keyCol[] = {30, 10, 20};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{LongType(), LongType()}), 3, targetCol, keyCol);
    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    Vector<int64_t> *outVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int64_t> *>(extractVectors[0])->GetValue(0), 100);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupExtractValuesDoubleDouble)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    double targetCol[] = {1.1, 2.2, 3.3};
    double keyCol[] = {3.0, 1.0, 2.0};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()}), 3, targetCol, keyCol);
    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    Vector<double> *outVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_DOUBLE_EQ(static_cast<Vector<double> *>(extractVectors[0])->GetValue(0), 1.1);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupExtractValuesIntVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    int32_t targetCol[] = {10, 20, 30};
    std::string keyCol[] = {"c", "a", "b"};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)}), 3, targetCol, keyCol);
    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 10);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// Varchar+Varchar: max_by by string key (lex order). Keys "c","a","b" -> max "c" -> target "cc".
TEST(MaxByAggregatorTest, ProcessGroupExtractValuesVarcharVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{VarcharType(10)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    std::string targetCol[] = {"aa", "bb", "cc"};
    std::string keyCol[] = {"a", "b", "c"};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)}), 3, targetCol, keyCol);
    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    using VarcharVec = Vector<LargeStringContainer<std::string_view>>;
    VarcharVec *outVec = new VarcharVec(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(std::string(outVec->GetValue(0)), "cc");

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// ---- GetSpillType: returns [target_type, sort_key_type] ----

TEST(MaxByAggregatorTest, GetSpillTypeIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 2u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_INT);
    EXPECT_EQ(spillTypes[1]->GetId(), OMNI_INT);
}

TEST(MaxByAggregatorTest, GetSpillTypeIntVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 2u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_INT);
    EXPECT_EQ(spillTypes[1]->GetId(), OMNI_VARCHAR);
}

TEST(MaxByAggregatorTest, GetSpillTypeVarcharVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{VarcharType(10)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 2u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_VARCHAR);
    EXPECT_EQ(spillTypes[1]->GetId(), OMNI_VARCHAR);
}

// ---- ExtractValuesForSpill: state -> vectors[0]=target, vectors[1]=sort key ----

TEST(MaxByAggregatorTest, ExtractValuesForSpillIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    int32_t targetCol[] = {42};
    int32_t keyCol[] = {7};
    VectorBatch *vecBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{IntType(), IntType()}), 1, targetCol, keyCol);
    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 1);

    Vector<int32_t> *spillTarget = new Vector<int32_t>(1);
    Vector<int32_t> *spillKey = new Vector<int32_t>(1);
    std::vector<AggregateState *> states = {state.get()};
    std::vector<BaseVector *> spillVectors = {spillTarget, spillKey};
    agg->ExtractValuesForSpill(states, spillVectors);

    EXPECT_FALSE(spillTarget->IsNull(0));
    EXPECT_FALSE(spillKey->IsNull(0));
    EXPECT_EQ(spillTarget->GetValue(0), 42);
    EXPECT_EQ(spillKey->GetValue(0), 7);

    VectorHelper::FreeVecBatch(vecBatch);
    delete spillTarget;
    delete spillKey;
}

// ---- ProcessGroupUnspill: merge spilled rows into state, then ExtractValues ----

TEST(MaxByAggregatorTest, ProcessGroupUnspillIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    const int32_t groupCount = 1;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->InitStates(groupStates);

    // Spill batch: 2 rows (target, key) = (100, 5), (200, 2). Max key=5 -> target=100.
    Vector<int32_t> *spillTarget = new Vector<int32_t>(2);
    spillTarget->SetValue(0, 100);
    spillTarget->SetValue(1, 200);
    Vector<int32_t> *spillKey = new Vector<int32_t>(2);
    spillKey->SetValue(0, 5);
    spillKey->SetValue(1, 2);
    VectorBatch *spillBatch = new VectorBatch(2);
    spillBatch->Append(spillTarget);
    spillBatch->Append(spillKey);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[0], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);
    EXPECT_EQ(vectorIndex, 2);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(groupStates[0], extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 100);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupUnspillLongLong)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    std::vector<AggregateState *> groupStates(1);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize());
    groupStates[0] = stateBuf.get();
    agg->InitStates(groupStates);

    Vector<int64_t> *spillTarget = new Vector<int64_t>(2);
    spillTarget->SetValue(0, 1000);
    spillTarget->SetValue(1, 2000);
    Vector<int64_t> *spillKey = new Vector<int64_t>(2);
    spillKey->SetValue(0, 50);
    spillKey->SetValue(1, 10);
    VectorBatch *spillBatch = new VectorBatch(2);
    spillBatch->Append(spillTarget);
    spillBatch->Append(spillKey);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[0], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);

    Vector<int64_t> *outVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(groupStates[0], extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int64_t> *>(extractVectors[0])->GetValue(0), 1000);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupUnspillDoubleDouble)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    std::vector<AggregateState *> groupStates(1);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize());
    groupStates[0] = stateBuf.get();
    agg->InitStates(groupStates);

    Vector<double> *spillTarget = new Vector<double>(2);
    spillTarget->SetValue(0, 1.5);
    spillTarget->SetValue(1, 2.5);
    Vector<double> *spillKey = new Vector<double>(2);
    spillKey->SetValue(0, 10.0);
    spillKey->SetValue(1, 2.0);
    VectorBatch *spillBatch = new VectorBatch(2);
    spillBatch->Append(spillTarget);
    spillBatch->Append(spillKey);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[0], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);

    Vector<double> *outVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(groupStates[0], extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_DOUBLE_EQ(static_cast<Vector<double> *>(extractVectors[0])->GetValue(0), 1.5);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupUnspillIntVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    std::vector<AggregateState *> groupStates(1);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize());
    groupStates[0] = stateBuf.get();
    agg->InitStates(groupStates);

    int32_t targetData[] = {88, 99};
    std::string keyData[] = {"z", "a"};
    VectorBatch *spillBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)}), 2, targetData, keyData);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[0], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(groupStates[0], extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 88);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outVec;
}

// Varchar+Varchar unspill: (target, key) = ("xx", "z"), ("yy", "a") -> max key "z" -> target "xx".
TEST(MaxByAggregatorTest, ProcessGroupUnspillVarcharVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{VarcharType(10)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    std::vector<AggregateState *> groupStates(1);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize());
    groupStates[0] = stateBuf.get();
    agg->InitStates(groupStates);

    std::string targetData[] = {"xx", "yy"};
    std::string keyData[] = {"z", "a"};
    VectorBatch *spillBatch = CreateVectorBatch(
        DataTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)}), 2, targetData, keyData);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[0], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);

    using VarcharVec = Vector<LargeStringContainer<std::string_view>>;
    VarcharVec *outVec = new VarcharVec(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(groupStates[0], extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(std::string(outVec->GetValue(0)), "xx");

    VectorHelper::FreeVecBatch(spillBatch);
    delete outVec;
}

TEST(MaxByAggregatorTest, ProcessGroupUnspillSkipsNullRow)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    std::vector<AggregateState *> groupStates(1);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize());
    groupStates[0] = stateBuf.get();
    agg->InitStates(groupStates);

    Vector<int32_t> *spillTarget = new Vector<int32_t>(2);
    spillTarget->SetValue(0, 10);
    spillTarget->SetNull(1);
    Vector<int32_t> *spillKey = new Vector<int32_t>(2);
    spillKey->SetValue(0, 1);
    spillKey->SetValue(1, 0);
    VectorBatch *spillBatch = new VectorBatch(2);
    spillBatch->Append(spillTarget);
    spillBatch->Append(spillKey);

    std::vector<UnspillRowInfo> unspillRows(2);
    unspillRows[0] = UnspillRowInfo{groupStates[0], spillBatch, 0};
    unspillRows[1] = UnspillRowInfo{groupStates[0], spillBatch, 1};

    int32_t vectorIndex = 0;
    agg->ProcessGroupUnspill(unspillRows, 2, vectorIndex);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(groupStates[0], extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 10);

    VectorHelper::FreeVecBatch(spillBatch);
    delete outVec;
}

// ---- ConstVector tests: ProcessGroup with ConstVector columns ----

// ConstVector target (col1), FlatVector sort key (col2) - scalar types
TEST(MaxByAggregatorTest, ProcessGroupConstTargetIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Build VectorBatch: col1=ConstVector(42), col2=FlatVector{3,1,2}
    const int32_t rowCount = 3;
    auto *constTarget = new ConstVector<int32_t>(42, OMNI_INT, rowCount);
    auto *keyVec = new Vector<int32_t>(rowCount);
    keyVec->SetValue(0, 3);
    keyVec->SetValue(1, 1);
    keyVec->SetValue(2, 2);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constTarget);
    vecBatch->Append(keyVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 42);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// FlatVector target (col1), ConstVector sort key (col2) - scalar types
TEST(MaxByAggregatorTest, ProcessGroupConstKeyIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Build VectorBatch: col1=FlatVector{10,20,30}, col2=ConstVector(5)
    const int32_t rowCount = 3;
    auto *targetVec = new Vector<int32_t>(rowCount);
    targetVec->SetValue(0, 10);
    targetVec->SetValue(1, 20);
    targetVec->SetValue(2, 30);
    auto *constKey = new ConstVector<int32_t>(5, OMNI_INT, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(targetVec);
    vecBatch->Append(constKey);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    // All keys are equal (5), so the last row processed (key >= current) wins -> target=30
    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 30);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// Both ConstVector target and sort key
TEST(MaxByAggregatorTest, ProcessGroupBothConstIntInt)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    auto *constTarget = new ConstVector<int32_t>(99, OMNI_INT, rowCount);
    auto *constKey = new ConstVector<int32_t>(7, OMNI_INT, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constTarget);
    vecBatch->Append(constKey);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 99);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// ConstVector target with Long type sort key
TEST(MaxByAggregatorTest, ProcessGroupConstTargetLongLong)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    auto *constTarget = new ConstVector<int64_t>(100L, OMNI_LONG, rowCount);
    auto *keyVec = new Vector<int64_t>(rowCount);
    keyVec->SetValue(0, 30L);
    keyVec->SetValue(1, 10L);
    keyVec->SetValue(2, 20L);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constTarget);
    vecBatch->Append(keyVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int64_t> *outVec = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int64_t> *>(extractVectors[0])->GetValue(0), 100L);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// ConstVector target with Double type
TEST(MaxByAggregatorTest, ProcessGroupConstTargetDoubleDouble)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType(), DoubleType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    auto *constTarget = new ConstVector<double>(5.5, OMNI_DOUBLE, rowCount);
    auto *keyVec = new Vector<double>(rowCount);
    keyVec->SetValue(0, 3.0);
    keyVec->SetValue(1, 1.0);
    keyVec->SetValue(2, 2.0);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constTarget);
    vecBatch->Append(keyVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<double> *outVec = new Vector<double>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_DOUBLE_EQ(static_cast<Vector<double> *>(extractVectors[0])->GetValue(0), 5.5);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// ConstVector target (Int), Varchar sort key (FlatVector)
TEST(MaxByAggregatorTest, ProcessGroupConstTargetIntVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    auto *constTarget = new ConstVector<int32_t>(42, OMNI_INT, rowCount);
    std::string keyData[] = {"c", "a", "b"};
    auto *keyVec = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int i = 0; i < rowCount; i++) {
        keyVec->SetValue(i, std::string_view(keyData[i].data(), keyData[i].size()));
    }
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constTarget);
    vecBatch->Append(keyVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    // max key = "c" -> target = 42 (const)
    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 42);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// FlatVector target (Int), ConstVector varchar sort key
TEST(MaxByAggregatorTest, ProcessGroupConstVarcharKeyIntVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType(), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{IntType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    int32_t targetData[] = {10, 20, 30};
    auto *targetVec = new Vector<int32_t>(rowCount);
    for (int i = 0; i < rowCount; i++) {
        targetVec->SetValue(i, targetData[i]);
    }
    std::string constKeyStr = "xyz";
    auto *constKey = new ConstVector<std::string_view>(
        std::string_view(constKeyStr.data(), constKeyStr.size()), OMNI_VARCHAR, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(targetVec);
    vecBatch->Append(constKey);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    Vector<int32_t> *outVec = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    // All keys are equal ("xyz"), first processed row wins (equal keys will update state) -> target = 30
    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(extractVectors[0])->GetValue(0), 30);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

// ConstVector target (Varchar), Varchar sort key (FlatVector)
TEST(MaxByAggregatorTest, ProcessGroupConstTargetVarcharVarchar)
{
    MaxByAggregatorFactory factory;
    std::vector<int32_t> channels = {0, 1};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(10), VarcharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{VarcharType(10)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    std::string constTargetStr = "hello";
    auto *constTarget = new ConstVector<std::string_view>(
        std::string_view(constTargetStr.data(), constTargetStr.size()), OMNI_VARCHAR, rowCount);
    std::string keyData[] = {"a", "c", "b"};
    auto *keyVec = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int i = 0; i < rowCount; i++) {
        keyVec->SetValue(i, std::string_view(keyData[i].data(), keyData[i].size()));
    }
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constTarget);
    vecBatch->Append(keyVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    using VarcharVec = Vector<LargeStringContainer<std::string_view>>;
    VarcharVec *outVec = new VarcharVec(1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    // max key = "c" -> target = "hello" (const)
    EXPECT_FALSE(outVec->IsNull(0));
    EXPECT_EQ(std::string(outVec->GetValue(0)), "hello");

    VectorHelper::FreeVecBatch(vecBatch);
    delete outVec;
}

} // namespace omniruntime
