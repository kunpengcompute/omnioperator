// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// Description: Unit tests for CollectSetComplexAggregator (collect_set on ARRAY/ROW columns).

#include <memory>
#include <set>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/collect_set_complex_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "vector/vector.h"
#include "vector/row_vector.h"
#include "type/data_type.h"
#include "util/type_util.h"
#include "operator/execution_context.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;

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

// Build an ArrayVector (array of int) with one row per group; each row i has lengths[i] elements from data.
static ArrayVector *BuildArrayIntColumn(int32_t rowCount, const std::vector<int32_t> &lengths,
    const std::vector<int32_t> &data)
{
    int32_t total = 0;
    for (int32_t len : lengths) {
        total += len;
    }
    auto *elemVec = new Vector<int32_t>(total);
    int32_t idx = 0;
    for (int32_t v : data) {
        elemVec->SetValue(idx++, v);
    }
    auto *arrVec = new ArrayVector(static_cast<int64_t>(rowCount), std::shared_ptr<BaseVector>(elemVec));
    int32_t offset = 0;
    for (int32_t i = 0; i < rowCount; i++) {
        arrVec->SetSize(i, lengths[static_cast<size_t>(i)]);
        arrVec->SetNotNull(i);
        offset += lengths[static_cast<size_t>(i)];
    }
    return arrVec;
}

// Build RowVector with one int field, rowCount rows.
static RowVector *BuildRowIntColumn(int32_t rowCount, const std::vector<int32_t> &data)
{
    auto *childVec = new Vector<int32_t>(rowCount);
    for (int32_t i = 0; i < rowCount; i++) {
        childVec->SetValue(i, data[static_cast<size_t>(i)]);
        childVec->SetNotNull(i);
    }
    std::vector<std::shared_ptr<BaseVector>> children;
    children.push_back(std::shared_ptr<BaseVector>(childVec));
    auto *rowVec = new RowVector(rowCount, children);
    for (int32_t i = 0; i < rowCount; i++) {
        rowVec->SetNotNull(i);
    }
    return rowVec;
}

static DataTypePtr RowOf(const std::vector<DataTypePtr> &fieldTypes)
{
    std::vector<std::string> names;
    for (size_t i = 0; i < fieldTypes.size(); i++) {
        names.push_back("f" + std::to_string(i));
    }
    return std::make_shared<RowType>(fieldTypes, names);
}

// ---- Factory tests ----
TEST(CollectSetComplexAggregatorTest, FactoryPartialArrayOfInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_EQ(agg->GetStateSize(), 8u);
    EXPECT_NE(dynamic_cast<CollectSetComplexAggregator *>(agg.get()), nullptr);
}

TEST(CollectSetComplexAggregatorTest, FactoryPartialRowOfInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto rowType = RowOf({IntType()});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(rowType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_NE(dynamic_cast<CollectSetComplexAggregator *>(agg.get()), nullptr);
}

// ---- Array<Array<int>>: collect_set on array column ----
TEST(CollectSetComplexAggregatorTest, PartialProcessGroupArrayOfIntDedup)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Input: 4 rows, arrays [1,2], [3], [1,2], [4,5] - [1,2] is duplicated
    std::vector<int32_t> lengths = {2, 1, 2, 2};
    std::vector<int32_t> data = {1, 2, 3, 1, 2, 4, 5};
    ArrayVector *colVec = BuildArrayIntColumn(4, lengths, data);
    VectorBatch *vecBatch = new VectorBatch(4);
    vecBatch->Append(colVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 4);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> setElem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(setElem, nullptr);
    auto *innerArr = static_cast<ArrayVector *>(setElem.get());
    // collect_set deduplicates: [1,2], [3], [1,2], [4,5] -> 3 unique arrays
    EXPECT_EQ(innerArr->GetSize(), 3);

    // Verify we have [1,2], [3], [4,5] (order may vary in set)
    std::set<std::vector<int32_t>> seen;
    for (int64_t i = 0; i < innerArr->GetSize(); i++) {
        std::shared_ptr<BaseVector> arr = innerArr->GetArrayAt(i, false);
        auto *flat = static_cast<Vector<int32_t> *>(arr.get());
        std::vector<int32_t> vals;
        for (int32_t j = 0; j < flat->GetSize(); j++) {
            vals.push_back(flat->GetValue(j));
        }
        seen.insert(vals);
    }
    EXPECT_TRUE(seen.count({1, 2}));
    EXPECT_TRUE(seen.count({3}));
    EXPECT_TRUE(seen.count({4, 5}));
    EXPECT_EQ(seen.size(), 3u);

    delete outVec;
    static_cast<CollectSetComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

// ---- Array<struct>: collect_set on struct column ----
TEST(CollectSetComplexAggregatorTest, PartialProcessGroupRowOfIntDedup)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto rowType = RowOf({IntType()});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(rowType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Input: 4 rows (42), (99), (42), (7) - 42 is duplicated
    RowVector *colVec = BuildRowIntColumn(4, {42, 99, 42, 7});
    VectorBatch *vecBatch = new VectorBatch(4);
    vecBatch->Append(colVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 4);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> setElem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(setElem, nullptr);
    auto *rowsVec = static_cast<RowVector *>(setElem.get());
    // collect_set deduplicates: 3 unique structs (42, 99, 7)
    EXPECT_EQ(rowsVec->GetSize(), 3);

    std::set<int32_t> seen;
    for (int32_t i = 0; i < rowsVec->GetSize(); i++) {
        RowVector *rowSlice = rowsVec->Slice(i, 1);
        auto *c = static_cast<Vector<int32_t> *>(rowSlice->ChildAt(0).get());
        seen.insert(c->GetValue(0));
        delete rowSlice;
    }
    EXPECT_TRUE(seen.count(42));
    EXPECT_TRUE(seen.count(99));
    EXPECT_TRUE(seen.count(7));
    EXPECT_EQ(seen.size(), 3u);

    delete outVec;
    static_cast<CollectSetComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(CollectSetComplexAggregatorTest, PartialEmptyInputExtractNull)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    ArrayVector *emptyCol = BuildArrayIntColumn(0, {}, {});
    VectorBatch *vecBatch = new VectorBatch(0);
    vecBatch->Append(emptyCol);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 0);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> inner = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->GetSize(), 0);

    delete outVec;
    static_cast<CollectSetComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(CollectSetComplexAggregatorTest, GetSpillType)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);
}

// ---- ProcessAlignAggSchema ----
TEST(CollectSetComplexAggregatorTest, ProcessAlignAggSchemaEmptyInput)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    VectorBatch *inputBatch = new VectorBatch(0);
    VectorBatch *result = new VectorBatch(0);
    agg->AlignAggSchema(result, inputBatch);
    EXPECT_EQ(result->GetVectorCount(), 1);
    EXPECT_EQ(result->GetRowCount(), 0);
    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

TEST(CollectSetComplexAggregatorTest, ProcessAlignAggSchemaRawInputArrayOfArray)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    std::vector<int32_t> lengths = {2};
    std::vector<int32_t> data = {10, 20};
    ArrayVector *originVec = BuildArrayIntColumn(1, lengths, data);

    VectorBatch *inputBatch = new VectorBatch(1);
    inputBatch->Append(originVec);
    VectorBatch *result = new VectorBatch(1);
    agg->AlignAggSchema(result, inputBatch);
    EXPECT_EQ(result->GetVectorCount(), 1);
    EXPECT_EQ(result->GetRowCount(), 1);
    BaseVector *outCol = result->Get(0);
    ASSERT_NE(outCol, nullptr);
    auto *outArr = static_cast<ArrayVector *>(outCol);
    EXPECT_FALSE(outArr->IsNull(0));
    EXPECT_EQ(outArr->GetSize(0), 1);
    std::shared_ptr<BaseVector> inner = outArr->GetArrayAt(0, false);
    ASSERT_NE(inner, nullptr);
    auto *innerArr = static_cast<ArrayVector *>(inner.get());
    EXPECT_EQ(innerArr->GetSize(), 1);
    std::shared_ptr<BaseVector> elem = innerArr->GetArrayAt(0, false);
    auto *flat = static_cast<Vector<int32_t> *>(elem.get());
    EXPECT_EQ(flat->GetSize(), 2);
    EXPECT_EQ(flat->GetValue(0), 10);
    EXPECT_EQ(flat->GetValue(1), 20);

    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

TEST(CollectSetComplexAggregatorTest, ProcessAlignAggSchemaRawInputArrayOfStruct)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto rowType = RowOf({IntType()});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(rowType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    RowVector *originVec = BuildRowIntColumn(1, {100});

    VectorBatch *inputBatch = new VectorBatch(1);
    inputBatch->Append(originVec);
    VectorBatch *result = new VectorBatch(1);
    agg->AlignAggSchema(result, inputBatch);
    EXPECT_EQ(result->GetVectorCount(), 1);
    EXPECT_EQ(result->GetRowCount(), 1);
    BaseVector *outCol = result->Get(0);
    ASSERT_NE(outCol, nullptr);
    auto *outArr = static_cast<ArrayVector *>(outCol);
    EXPECT_FALSE(outArr->IsNull(0));
    EXPECT_EQ(outArr->GetSize(0), 1);
    std::shared_ptr<BaseVector> inner = outArr->GetArrayAt(0, false);
    ASSERT_NE(inner, nullptr);
    auto *rowsVec = static_cast<RowVector *>(inner.get());
    EXPECT_EQ(rowsVec->GetSize(), 1);
    RowVector *row0 = rowsVec->Slice(0, 1);
    auto *c0 = static_cast<Vector<int32_t> *>(row0->ChildAt(0).get());
    EXPECT_EQ(c0->GetValue(0), 100);
    delete row0;

    VectorHelper::FreeVecBatch(inputBatch);
    VectorHelper::FreeVecBatch(result);
}

// ---- Grouped aggregation (each row is one group) ----
TEST(CollectSetComplexAggregatorTest, GroupedAggregationRowOfInt)
{
    CollectSetAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto rowType = RowOf({IntType()});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(rowType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // 2 rows, each is its own group: (10), (20)
    RowVector *valCol = BuildRowIntColumn(2, {10, 20});
    VectorBatch *vecBatch = new VectorBatch(2);
    vecBatch->Append(valCol);

    agg->SetStateOffset(0);
    std::vector<AggregateState *> groupStates(2);
    auto stateBuf = std::make_unique<AggregateState[]>(agg->GetStateSize() * 2);
    for (int32_t i = 0; i < 2; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }
    agg->InitStates(groupStates);
    agg->ProcessGroup(groupStates, vecBatch, 0);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 2);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, 2);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> g0 = arrayVec->GetArrayAt(0, false);
    EXPECT_EQ(static_cast<RowVector *>(g0.get())->GetSize(), 1);
    RowVector *r0 = static_cast<RowVector *>(g0.get())->Slice(0, 1);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(r0->ChildAt(0).get())->GetValue(0), 10);
    delete r0;

    EXPECT_FALSE(arrayVec->IsNull(1));
    std::shared_ptr<BaseVector> g1 = arrayVec->GetArrayAt(1, false);
    EXPECT_EQ(static_cast<RowVector *>(g1.get())->GetSize(), 1);
    RowVector *r1 = static_cast<RowVector *>(g1.get())->Slice(0, 1);
    EXPECT_EQ(static_cast<Vector<int32_t> *>(r1->ChildAt(0).get())->GetValue(0), 20);
    delete r1;

    delete outVec;
    for (int32_t i = 0; i < 2; i++) {
        static_cast<CollectSetComplexAggregator *>(agg.get())->DestroyState(groupStates[i]);
    }
    VectorHelper::FreeVecBatch(vecBatch);
}

}  // namespace omniruntime
