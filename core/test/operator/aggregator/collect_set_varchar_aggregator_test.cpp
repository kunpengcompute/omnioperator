/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Unit tests for CollectSetVarcharAggregator (VARCHAR/CHAR/VARBINARY collect_set).
 */

#include <memory>
#include <vector>
#include <algorithm>
#include <set>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/collect_set_varchar_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "vector/vector.h"
#include "type/data_type.h"
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

// ---- Factory tests ----

TEST(CollectSetVarcharAggregatorTest, FactoryPartialVarchar)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_GT(agg->GetStateSize(), 0u);
    auto *varcharAgg = dynamic_cast<CollectSetVarcharAggregator *>(agg.get());
    EXPECT_NE(varcharAgg, nullptr);
}

TEST(CollectSetVarcharAggregatorTest, FactoryPartialChar)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{CharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(CharType(10))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(dynamic_cast<CollectSetVarcharAggregator *>(agg.get()) != nullptr);
}

TEST(CollectSetVarcharAggregatorTest, FactoryPartialVarBinary)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarBinaryType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarBinaryType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(dynamic_cast<CollectSetVarcharAggregator *>(agg.get()) != nullptr);
}

TEST(CollectSetVarcharAggregatorTest, FactoryFinalArrayVarchar)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_FALSE(agg->IsInputRaw());
}

// ---- Behaviour tests ----

TEST(CollectSetVarcharAggregatorTest, PartialProcessGroupAndExtractValuesVarcharDedup)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    std::string data[] = {"a", "b", "a", "c", "b", "a"};
    const int32_t rowCnt = 6;
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{VarcharType(100)}), rowCnt, data);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCnt);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    auto *emptyElem = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, 0));
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(emptyElem));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *strVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get());
    int32_t uniqueCount = strVec->GetSize();
    EXPECT_EQ(uniqueCount, 3);  // distinct: a, b, c

    std::set<std::string> extracted;
    for (int32_t i = 0; i < uniqueCount; i++) {
        extracted.insert(std::string(strVec->GetValue(i)));
    }
    EXPECT_TRUE(extracted.count("a") != 0);
    EXPECT_TRUE(extracted.count("b") != 0);
    EXPECT_TRUE(extracted.count("c") != 0);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    static_cast<CollectSetVarcharAggregator *>(agg.get())->DestroyState(state.get());
}

TEST(CollectSetVarcharAggregatorTest, PartialProcessGroupEmptyInputExtractNull)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 0;
    auto *col = new Vector<LargeStringContainer<std::string_view>>(0);
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(col);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    auto *emptyElem = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, 0));
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(emptyElem));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    EXPECT_EQ(static_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get())->GetSize(), 0);

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    static_cast<CollectSetVarcharAggregator *>(agg.get())->DestroyState(state.get());
}

TEST(CollectSetVarcharAggregatorTest, GroupedAggregationVarchar)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    std::string data[] = {"x", "y", "z"};
    const int32_t groupCount = 3;
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{VarcharType(100)}), groupCount, data);
    ASSERT_EQ(vecBatch->GetRowCount(), groupCount);

    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf = std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(groupStates);
    agg->ProcessGroup(groupStates, vecBatch, 0);

    auto *emptyElem = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, 0));
    BaseVector *outputVector = new ArrayVector(groupCount, std::shared_ptr<BaseVector>(emptyElem));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, groupCount);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    for (int32_t g = 0; g < groupCount; g++) {
        EXPECT_FALSE(arrayVec->IsNull(g));
        std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(g, false);
        ASSERT_NE(elemVec, nullptr);
        auto *strVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get());
        EXPECT_EQ(strVec->GetSize(), 1u);
        EXPECT_EQ(std::string(strVec->GetValue(0)), data[g]);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    for (int32_t i = 0; i < groupCount; i++) {
        static_cast<CollectSetVarcharAggregator *>(agg.get())->DestroyState(groupStates[i]);
    }
}

TEST(CollectSetVarcharAggregatorTest, GetSpillType)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);
}

TEST(CollectSetVarcharAggregatorTest, InitStatesMultiGroup)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t groupCount = 4;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf = std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(groupStates);
    EXPECT_EQ(agg->GetStateSize(), 8u);
    for (int32_t i = 0; i < groupCount; i++) {
        static_cast<CollectSetVarcharAggregator *>(agg.get())->DestroyState(groupStates[i]);
    }
}

// ---- ConstVector tests ----

// CollectSetVarchar with ConstVector<std::string_view> input: all same value -> set {value}
TEST(CollectSetVarcharAggregatorTest, PartialProcessGroupConstVectorVarchar)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 4;
    std::string constStr = "hello";
    auto *constCol = new ConstVector<std::string_view>(
        std::string_view(constStr.data(), constStr.size()), OMNI_VARCHAR, rowCount);
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(constCol);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCount);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCount);

    auto *emptyElem = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, 0));
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(emptyElem));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(elemVec, nullptr);
    auto *strVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get());
    // Set should deduplicate: all "hello" -> {"hello"}
    EXPECT_EQ(strVec->GetSize(), 1);
    EXPECT_EQ(std::string(strVec->GetValue(0)), "hello");

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    static_cast<CollectSetVarcharAggregator *>(agg.get())->DestroyState(state.get());
}

}  // namespace omniruntime
