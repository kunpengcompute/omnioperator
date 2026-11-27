/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Unit tests for CollectListVarcharAggregator (VARCHAR/CHAR/VARBINARY collect_list).
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/collect_list_varchar_aggregator.h"
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

TEST(CollectListVarcharAggregatorTest, FactoryPartialVarchar)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_GT(agg->GetStateSize(), 0u);
    EXPECT_NE(dynamic_cast<CollectListVarcharAggregator *>(agg.get()), nullptr);
}

TEST(CollectListVarcharAggregatorTest, FactoryPartialChar)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{CharType(10)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(CharType(10))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_NE(dynamic_cast<CollectListVarcharAggregator *>(agg.get()), nullptr);
}

TEST(CollectListVarcharAggregatorTest, FactoryPartialVarBinary)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarBinaryType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarBinaryType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_NE(dynamic_cast<CollectListVarcharAggregator *>(agg.get()), nullptr);
}

TEST(CollectListVarcharAggregatorTest, FactoryFinalArrayVarchar)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_FALSE(agg->IsInputRaw());
}

// ---- Order and duplicates preserved ----

TEST(CollectListVarcharAggregatorTest, PartialProcessGroupOrderAndDuplicatesVarchar)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    std::string data[] = {"a", "b", "a", "c"};
    const int32_t rowCnt = 4;
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
    EXPECT_EQ(strVec->GetSize(), 4u);
    EXPECT_EQ(std::string(strVec->GetValue(0)), "a");
    EXPECT_EQ(std::string(strVec->GetValue(1)), "b");
    EXPECT_EQ(std::string(strVec->GetValue(2)), "a");
    EXPECT_EQ(std::string(strVec->GetValue(3)), "c");

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    static_cast<CollectListVarcharAggregator *>(agg.get())->DestroyState(state.get());
}

TEST(CollectListVarcharAggregatorTest, PartialProcessGroupEmptyInputExtractNull)
{
    CollectListAggregatorFactory factory;
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
    static_cast<CollectListVarcharAggregator *>(agg.get())->DestroyState(state.get());
}

TEST(CollectListVarcharAggregatorTest, GroupedAggregationVarchar)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    std::string data[] = {"p", "q", "r"};
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
        static_cast<CollectListVarcharAggregator *>(agg.get())->DestroyState(groupStates[i]);
    }
}

TEST(CollectListVarcharAggregatorTest, GetSpillType)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);
}

TEST(CollectListVarcharAggregatorTest, InitStatesMultiGroup)
{
    CollectListAggregatorFactory factory;
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
        static_cast<CollectListVarcharAggregator *>(agg.get())->DestroyState(groupStates[i]);
    }
}

// ---- ConstVector tests ----

// CollectListVarchar with ConstVector<std::string_view> input: preserves all elements
TEST(CollectListVarcharAggregatorTest, PartialProcessGroupConstVectorVarchar)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType(100)});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(VarcharType(100))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCount = 3;
    std::string constStr = "world";
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
    // List preserves all: ["world", "world", "world"]
    EXPECT_EQ(strVec->GetSize(), 3);
    for (int32_t i = 0; i < 3; ++i) {
        EXPECT_EQ(std::string(strVec->GetValue(i)), "world");
    }

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    static_cast<CollectListVarcharAggregator *>(agg.get())->DestroyState(state.get());
}

}  // namespace omniruntime
