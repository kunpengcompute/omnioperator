#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/sort_aggregation.h"
#include "operator/aggregation/aggregator/all_aggregators.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/config/operator_config.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "util/test_agg_util.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

class SortAggregationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    static VectorBatch *MakeIntBatch(const std::vector<int32_t> &keys, const std::vector<int64_t> &values) {
        int32_t numRows = static_cast<int32_t>(keys.size());
        auto *batch = new VectorBatch(numRows);
        auto *keyVec = new FlatVector(IntType(), numRows);
        auto *valVec = new FlatVector(LongType(), numRows);
        for (int32_t i = 0; i < numRows; i++) {
            keyVec->Set(i, keys[i]);
            valVec->Set(i, values[i]);
        }
        batch->Append(keyVec);
        batch->Append(valVec);
        return batch;
    }

    static VectorBatch *MakeIntBatchWithNulls(const std::vector<int32_t> &keys,
        const std::vector<bool> &keyNulls, const std::vector<int64_t> &values,
        const std::vector<bool> &valNulls) {
        int32_t numRows = static_cast<int32_t>(keys.size());
        auto *batch = new VectorBatch(numRows);
        auto *keyVec = new FlatVector(IntType(), numRows);
        auto *valVec = new FlatVector(LongType(), numRows);
        for (int32_t i = 0; i < numRows; i++) {
            if (keyNulls[i]) { keyVec->SetNull(i); } else { keyVec->Set(i, keys[i]); }
            if (valNulls[i]) { valVec->SetNull(i); } else { valVec->Set(i, values[i]); }
        }
        batch->Append(keyVec);
        batch->Append(valVec);
        return batch;
    }

    static SortAggregationOperatorFactory *CreateSumFactory(
        std::vector<uint32_t> groupByCols,
        std::vector<std::vector<uint32_t>> aggCols) {
        DataTypes groupTypes{IntType()};
        std::vector<DataTypes> aggInputTypes{{LongType()}};
        std::vector<DataTypes> aggOutputTypes{{LongType()}};
        std::vector<uint32_t> aggFuncTypes{OMNI_AGGREGATION_TYPE_SUM};
        std::vector<uint32_t> maskCols{};
        std::vector<bool> inputRaws{true};
        std::vector<bool> outputPartials{false};
        std::vector<int8_t> hasAggFilters(1, 0);
        OperatorConfig config;
        auto *factory = new SortAggregationOperatorFactory(
            groupByCols, groupTypes, aggCols, aggInputTypes, aggOutputTypes,
            aggFuncTypes, maskCols, inputRaws, outputPartials, hasAggFilters,
            config, AggregationNode::Step::K_SINGLE);
        factory->Init();
        return factory;
    }

    static SortAggregationOperatorFactory *CreateCountFactory() {
        std::vector<uint32_t> groupByCols{0};
        std::vector<std::vector<uint32_t>> aggCols{};
        DataTypes groupTypes{IntType()};
        std::vector<DataTypes> aggInputTypes{};
        std::vector<DataTypes> aggOutputTypes{{LongType()}};
        std::vector<uint32_t> aggFuncTypes{OMNI_AGGREGATION_TYPE_COUNT_STAR};
        std::vector<uint32_t> maskCols{};
        std::vector<bool> inputRaws{true};
        std::vector<bool> outputPartials{false};
        std::vector<int8_t> hasAggFilters(1, 0);
        OperatorConfig config;
        auto *factory = new SortAggregationOperatorFactory(
            groupByCols, groupTypes, aggCols, aggInputTypes, aggOutputTypes,
            aggFuncTypes, maskCols, inputRaws, outputPartials, hasAggFilters,
            config, AggregationNode::Step::K_SINGLE);
        factory->Init();
        return factory;
    }
};

// ---- Phase 1: critical path ----

TEST_F(SortAggregationTest, BasicIntGroupBy_Sum) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatch({1, 1, 1, 2, 2, 3}, {10, 20, 30, 40, 50, 60});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 3);
    auto *keyCol = dynamic_cast<FlatVector *>(output->Get(0));
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    EXPECT_EQ(keyCol->Get<int32_t>(0), 1);
    EXPECT_EQ(sumCol->Get<int64_t>(0), 60);
    EXPECT_EQ(keyCol->Get<int32_t>(1), 2);
    EXPECT_EQ(sumCol->Get<int64_t>(1), 90);
    EXPECT_EQ(keyCol->Get<int32_t>(2), 3);
    EXPECT_EQ(sumCol->Get<int64_t>(2), 60);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, EmptyBatchInput) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *emptyBatch = MakeIntBatch({}, {});
    op->AddInput(emptyBatch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_EQ(output, nullptr);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, GroupSpanningTwoBatches) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch1 = MakeIntBatch({1, 1}, {10, 20});
    auto *batch2 = MakeIntBatch({1, 1}, {30, 40});
    op->AddInput(batch1);
    op->AddInput(batch2);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 1);
    auto *keyCol = dynamic_cast<FlatVector *>(output->Get(0));
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    EXPECT_EQ(keyCol->Get<int32_t>(0), 1);
    EXPECT_EQ(sumCol->Get<int64_t>(0), 100);
    op->Close();
    delete op;
    delete factory;
}

// ---- Phase 2: full coverage ----

TEST_F(SortAggregationTest, SingleGroup_AllSameKey) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatch({7, 7, 7, 7, 7}, {10, 20, 30, 40, 50});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 1);
    auto *keyCol = dynamic_cast<FlatVector *>(output->Get(0));
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    EXPECT_EQ(keyCol->Get<int32_t>(0), 7);
    EXPECT_EQ(sumCol->Get<int64_t>(0), 150);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, SingleRowInput) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatch({42}, {100});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 1);
    auto *keyCol = dynamic_cast<FlatVector *>(output->Get(0));
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    EXPECT_EQ(keyCol->Get<int32_t>(0), 42);
    EXPECT_EQ(sumCol->Get<int64_t>(0), 100);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, NoMoreInputWithoutData) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_EQ(output, nullptr);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, NullGroupKey) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatchWithNulls(
        {1, 0, 1, 0, 2}, {false, true, false, true, false},
        {10, 20, 30, 40, 50}, {false, false, false, false, false});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_GE(output->GetRowCount(), 2);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, NullAggValues_Sum) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatchWithNulls(
        {1, 1, 1}, {false, false, false},
        {10, 0, 30}, {false, true, false});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 1);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, MultipleGroupChangesInOneBatch) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatch({1, 1, 2, 2, 3, 3, 4, 4, 5, 5},
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 5);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, GroupStartsInOneBatchEndsInNext) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch1 = MakeIntBatch({1, 1, 2}, {10, 20, 100});
    auto *batch2 = MakeIntBatch({2, 2, 3, 3}, {200, 300, 400, 500});
    op->AddInput(batch1);
    op->AddInput(batch2);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 3);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, KeyTransitionAtBatchBoundary) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    auto *batch1 = MakeIntBatch({1, 1}, {10, 20});
    auto *batch2 = MakeIntBatch({2, 2}, {30, 40});
    op->AddInput(batch1);
    op->AddInput(batch2);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 2);
    auto *keyCol = dynamic_cast<FlatVector *>(output->Get(0));
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    EXPECT_EQ(keyCol->Get<int32_t>(0), 1);
    EXPECT_EQ(sumCol->Get<int64_t>(0), 30);
    EXPECT_EQ(keyCol->Get<int32_t>(1), 2);
    EXPECT_EQ(sumCol->Get<int64_t>(1), 70);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, LargeGroupSize) {
    auto *factory = CreateSumFactory({0}, {{1}});
    auto *op = factory->CreateOperator();
    int32_t numRows = 1000;
    std::vector<int32_t> keys(numRows, 1);
    std::vector<int64_t> values(numRows);
    int64_t expectedSum = 0;
    for (int32_t i = 0; i < numRows; i++) {
        values[i] = i + 1;
        expectedSum += i + 1;
    }
    auto *batch = MakeIntBatch(keys, values);
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 1);
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    EXPECT_EQ(sumCol->Get<int64_t>(0), expectedSum);
    op->Close();
    delete op;
    delete factory;
}

TEST_F(SortAggregationTest, MultipleAggFunctions) {
    std::vector<uint32_t> groupByCols{0};
    DataTypes groupTypes{IntType()};
    std::vector<std::vector<uint32_t>> aggCols{{1}, {1}, {1}};
    std::vector<DataTypes> aggInputTypes{{LongType()}, {LongType()}, {LongType()}};
    std::vector<DataTypes> aggOutputTypes{{LongType()}, {LongType()}, {DoubleType()}};
    std::vector<uint32_t> aggFuncTypes{
        OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_COUNT,
        OMNI_AGGREGATION_TYPE_AVG};
    std::vector<uint32_t> maskCols{};
    std::vector<bool> inputRaws{true, true, true};
    std::vector<bool> outputPartials{false, false, false};
    std::vector<int8_t> hasAggFilters(3, 0);
    OperatorConfig config;
    auto *factory = new SortAggregationOperatorFactory(
        groupByCols, groupTypes, aggCols, aggInputTypes, aggOutputTypes,
        aggFuncTypes, maskCols, inputRaws, outputPartials, hasAggFilters,
        config, AggregationNode::Step::K_SINGLE);
    factory->Init();
    auto *op = factory->CreateOperator();
    auto *batch = MakeIntBatch({1, 1, 1}, {10, 20, 30});
    op->AddInput(batch);
    op->SetNoMoreInput();
    VectorBatch *output = nullptr;
    op->GetOutput(&output);
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetRowCount(), 1);
    auto *sumCol = dynamic_cast<FlatVector *>(output->Get(1));
    auto *countCol = dynamic_cast<FlatVector *>(output->Get(2));
    EXPECT_EQ(sumCol->Get<int64_t>(0), 60);
    EXPECT_EQ(countCol->Get<int64_t>(0), 3);
    op->Close();
    delete op;
    delete factory;
}
