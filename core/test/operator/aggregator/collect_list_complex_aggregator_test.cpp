// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// Description: Unit tests for CollectListComplexAggregator (collect_list on ARRAY/MAP/ROW columns).

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/collect_list_complex_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "vector/vector.h"
#include "vector/map_vector.h"
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

// Build MapVector (Map<int,int>) with rowCount rows. Each row i has keys[i], values[i] pairs.
static MapVector *BuildMapIntIntColumn(int32_t rowCount, const std::vector<std::vector<int32_t>> &keysPerRow,
    const std::vector<std::vector<int32_t>> &valuesPerRow)
{
    int32_t totalEntries = 0;
    for (const auto &k : keysPerRow) {
        totalEntries += static_cast<int32_t>(k.size());
    }
    auto *keyVec = new Vector<int32_t>(totalEntries);
    auto *valVec = new Vector<int32_t>(totalEntries);
    int32_t pos = 0;
    for (int32_t r = 0; r < rowCount; r++) {
        const auto &keys = keysPerRow[static_cast<size_t>(r)];
        const auto &vals = valuesPerRow[static_cast<size_t>(r)];
        for (size_t i = 0; i < keys.size(); i++) {
            keyVec->SetValue(pos, keys[i]);
            valVec->SetValue(pos, vals[i]);
            pos++;
        }
    }
    auto *mapVec = new MapVector(static_cast<int64_t>(rowCount),
        std::shared_ptr<BaseVector>(keyVec), std::shared_ptr<BaseVector>(valVec));
    int32_t off = 0;
    for (int32_t r = 0; r < rowCount; r++) {
        int32_t sz = static_cast<int32_t>(keysPerRow[static_cast<size_t>(r)].size());
        mapVec->SetSize(r, sz);
        mapVec->SetNotNull(r);
        off += sz;
    }
    return mapVec;
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

static DataTypePtr MapOf(const DataTypePtr &keyType, const DataTypePtr &valueType)
{
    return std::make_shared<MapType>(keyType, valueType);
}

static DataTypePtr RowOf(const std::vector<DataTypePtr> &fieldTypes)
{
    std::vector<std::string> names;
    for (size_t i = 0; i < fieldTypes.size(); i++) {
        names.push_back("f" + std::to_string(i));
    }
    return std::make_shared<RowType>(fieldTypes, names);
}

TEST(CollectListComplexAggregatorTest, FactoryPartialArrayOfInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_EQ(agg->GetStateSize(), 8u);
    EXPECT_NE(dynamic_cast<CollectListComplexAggregator *>(agg.get()), nullptr);
}

TEST(CollectListComplexAggregatorTest, PartialProcessGroupArrayOfIntOrderPreserved)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Input: 3 rows, arrays [1,2], [3], [4,5]
    std::vector<int32_t> lengths = {2, 1, 2};
    std::vector<int32_t> data = {1, 2, 3, 4, 5};
    ArrayVector *colVec = BuildArrayIntColumn(3, lengths, data);
    VectorBatch *vecBatch = new VectorBatch(3);
    vecBatch->Append(colVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    std::shared_ptr<BaseVector> listElem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(listElem, nullptr);
    auto *innerArr = static_cast<ArrayVector *>(listElem.get());
    EXPECT_EQ(innerArr->GetSize(), 3);  // 3 arrays in the list

    // First element: [1,2]
    std::shared_ptr<BaseVector> first = innerArr->GetArrayAt(0, false);
    ASSERT_NE(first, nullptr);
    auto *firstFlat = static_cast<Vector<int32_t> *>(first.get());
    EXPECT_EQ(firstFlat->GetSize(), 2);
    EXPECT_EQ(firstFlat->GetValue(0), 1);
    EXPECT_EQ(firstFlat->GetValue(1), 2);
    // Second: [3]
    std::shared_ptr<BaseVector> second = innerArr->GetArrayAt(1, false);
    auto *secondFlat = static_cast<Vector<int32_t> *>(second.get());
    EXPECT_EQ(secondFlat->GetSize(), 1);
    EXPECT_EQ(secondFlat->GetValue(0), 3);
    // Third: [4,5]
    std::shared_ptr<BaseVector> third = innerArr->GetArrayAt(2, false);
    auto *thirdFlat = static_cast<Vector<int32_t> *>(third.get());
    EXPECT_EQ(thirdFlat->GetSize(), 2);
    EXPECT_EQ(thirdFlat->GetValue(0), 4);
    EXPECT_EQ(thirdFlat->GetValue(1), 5);

    delete outVec;
    static_cast<CollectListComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(CollectListComplexAggregatorTest, PartialEmptyInputExtractNull)
{
    CollectListAggregatorFactory factory;
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
    static_cast<CollectListComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(CollectListComplexAggregatorTest, GetSpillType)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(ArrayOf(IntType()))});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), 1u);
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);
}

TEST(CollectListComplexAggregatorTest, ProcessAlignAggSchemaEmptyInput)
{
    CollectListAggregatorFactory factory;
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

TEST(CollectListComplexAggregatorTest, ProcessAlignAggSchemaRawInputSingleRow)
{
    CollectListAggregatorFactory factory;
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
    EXPECT_EQ(outArr->GetSize(0), 1);  // single-element array [ [10,20] ]
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

// ---- Map and Row coverage ----
TEST(CollectListComplexAggregatorTest, FactoryPartialMapOfIntInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto mapType = MapOf(IntType(), IntType());
    DataTypes inputTypes(std::vector<DataTypePtr>{mapType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(mapType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_NE(dynamic_cast<CollectListComplexAggregator *>(agg.get()), nullptr);
}

TEST(CollectListComplexAggregatorTest, PartialProcessGroupMapOfIntIntOrderPreserved)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto mapType = MapOf(IntType(), IntType());
    DataTypes inputTypes(std::vector<DataTypePtr>{mapType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(mapType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Input: 2 rows, maps {1:10, 2:20} and {3:30}
    std::vector<std::vector<int32_t>> keysPerRow = {{1, 2}, {3}};
    std::vector<std::vector<int32_t>> valsPerRow = {{10, 20}, {30}};
    MapVector *colVec = BuildMapIntIntColumn(2, keysPerRow, valsPerRow);
    VectorBatch *vecBatch = new VectorBatch(2);
    vecBatch->Append(colVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 2);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    // For Array<Map>, GetArrayAt returns MapVector slice (all collected maps)
    std::shared_ptr<BaseVector> listElem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(listElem, nullptr);
    auto *mapsVec = static_cast<MapVector *>(listElem.get());
    EXPECT_EQ(mapsVec->GetSize(), 2);  // 2 maps in the list

    // First map: {1:10, 2:20}
    MapVector *firstMap = mapsVec->Slice(0, 1);
    EXPECT_EQ(firstMap->GetSize(0), 2);
    auto *k0 = static_cast<Vector<int32_t> *>(firstMap->GetKeyVector().get());
    auto *v0 = static_cast<Vector<int32_t> *>(firstMap->GetValueVector().get());
    EXPECT_EQ(k0->GetValue(0), 1);
    EXPECT_EQ(v0->GetValue(0), 10);
    EXPECT_EQ(k0->GetValue(1), 2);
    EXPECT_EQ(v0->GetValue(1), 20);
    delete firstMap;

    // Second map: {3:30}
    MapVector *secondMap = mapsVec->Slice(1, 1);
    EXPECT_EQ(secondMap->GetSize(0), 1);
    auto *k1 = static_cast<Vector<int32_t> *>(secondMap->GetKeyVector().get());
    auto *v1 = static_cast<Vector<int32_t> *>(secondMap->GetValueVector().get());
    EXPECT_EQ(k1->GetValue(0), 3);
    EXPECT_EQ(v1->GetValue(0), 30);
    delete secondMap;

    delete outVec;
    static_cast<CollectListComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(CollectListComplexAggregatorTest, FactoryPartialRowOfInt)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto rowType = RowOf({IntType()});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(rowType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_NE(dynamic_cast<CollectListComplexAggregator *>(agg.get()), nullptr);
}

TEST(CollectListComplexAggregatorTest, PartialProcessGroupRowOfIntOrderPreserved)
{
    CollectListAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto rowType = RowOf({IntType()});
    DataTypes inputTypes(std::vector<DataTypePtr>{rowType});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(rowType)});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(executionContext.get());

    // Input: 3 rows, (42), (99), (7)
    RowVector *colVec = BuildRowIntColumn(3, {42, 99, 7});
    VectorBatch *vecBatch = new VectorBatch(3);
    vecBatch->Append(colVec);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, 3);

    BaseVector *outVec = VectorHelper::CreateComplexVector(outputTypes.GetType(0).get(), 1);
    std::vector<BaseVector *> extractVectors = {outVec};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    EXPECT_FALSE(arrayVec->IsNull(0));
    // For Array<Row>, GetArrayAt returns RowVector slice (all collected rows)
    std::shared_ptr<BaseVector> listElem = arrayVec->GetArrayAt(0, false);
    ASSERT_NE(listElem, nullptr);
    auto *rowsVec = static_cast<RowVector *>(listElem.get());
    EXPECT_EQ(rowsVec->GetSize(), 3);  // 3 rows in the list

    RowVector *row0 = rowsVec->Slice(0, 1);
    auto *c0 = static_cast<Vector<int32_t> *>(row0->ChildAt(0).get());
    EXPECT_EQ(c0->GetValue(0), 42);
    delete row0;

    RowVector *row1 = rowsVec->Slice(1, 1);
    auto *c1 = static_cast<Vector<int32_t> *>(row1->ChildAt(0).get());
    EXPECT_EQ(c1->GetValue(0), 99);
    delete row1;

    RowVector *row2 = rowsVec->Slice(2, 1);
    auto *c2 = static_cast<Vector<int32_t> *>(row2->ChildAt(0).get());
    EXPECT_EQ(c2->GetValue(0), 7);
    delete row2;

    delete outVec;
    static_cast<CollectListComplexAggregator *>(agg.get())->DestroyState(state.get());
    VectorHelper::FreeVecBatch(vecBatch);
}

}  // namespace omniruntime
