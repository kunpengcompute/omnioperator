/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for CountColumn and CountAll aggregators.
 * Covers all supported input types: scalar, string, decimal, complex (array, map, row).
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/count_column_aggregator.h"
#include "operator/aggregation/aggregator/count_all_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/large_string_container.h"
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

    template <typename T>
    static T *ExtractValueFromState(AggregateState *state)
    {
        return reinterpret_cast<T *>(state);
    }

    // Build a column of given type with rowCount rows: first nonNullCount rows non-null, rest null.
    // Caller owns returned BaseVector (append to VectorBatch then FreeVecBatch).
    static BaseVector *BuildCountInputColumn(const DataTypePtr &aggType, int32_t rowCount, int32_t nonNullCount)
    {
        switch (aggType->GetId()) {
            case OMNI_NONE: {
                auto *col = new Vector<int64_t>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    col->SetNull(j);
                }
                return col;
            }
            case OMNI_BOOLEAN: {
                auto *col = new Vector<bool>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, true);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_BYTE: {
                auto *col = new Vector<int8_t>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_SHORT: {
                auto *col = new Vector<int16_t>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_TIME32: {
                auto *col = new Vector<int32_t>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64: {
                auto *col = new Vector<int64_t>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_FLOAT: {
                auto *col = new Vector<float>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1.0f);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_DOUBLE: {
                auto *col = new Vector<double>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1.0);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_DECIMAL128: {
                auto *col = new Vector<Decimal128>(rowCount);
                Decimal128 val(0, 1);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, val);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_CONTAINER: {
                // Container uses double for count context
                auto *col = new Vector<double>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        col->SetValue(j, 1.0);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY: {
                auto *col = new Vector<LargeStringContainer<std::string_view>>(rowCount);
                for (int32_t j = 0; j < rowCount; ++j) {
                    if (j < nonNullCount) {
                        std::string str = "x";
                        std::string_view sv(str.c_str(), str.size());
                        col->SetValue(j, sv);
                    } else {
                        col->SetNull(j);
                    }
                }
                return col;
            }
            default:
                return nullptr;
        }
    }

    // ---- CountColumnAggregatorFactory: create for each supported type ----

    TEST(CountColumnAggregatorTest, FactoryCreatesForAllSupportedTypes)
    {
        CountColumnAggregatorFactory factory;
        std::vector<int32_t> channel = {0};
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);

        auto testType = [&](const DataTypePtr &inputType) {
            auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(inputType).get(),
                *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
            ASSERT_NE(agg, nullptr) << "CountColumn factory failed for type " << inputType->GetId();
            agg->SetExecutionContext(executionContext.get());
            EXPECT_EQ(agg->GetStateSize(), sizeof(int64_t));
        };

        testType(NoneType());
        testType(BooleanType());
        testType(ByteType());
        testType(ShortType());
        testType(Date32Type());
        testType(Time32Type());
        testType(IntType());
        testType(LongType());
        testType(Date64Type());
        testType(Time64Type());
        testType(TimestampType());
        testType(FloatType());
        testType(DoubleType());
        testType(Decimal64Type(18, 2));
        testType(Decimal128Type(38, 0));
        testType(ContainerType());
        testType(VarcharType(10));
        testType(CharType(10));
        testType(VarBinaryType(10));
        testType(std::make_shared<ArrayType>(IntType()));
        testType(std::make_shared<MapType>(IntType(), IntType()));
        std::vector<DataTypePtr> fieldTypes = {IntType()};
        testType(std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"f0"}));
    }

    // ---- CountColumn: ProcessGroup returns count of non-null rows ----

    TEST(CountColumnAggregatorTest, CountColumnLongNonNullAndNull)
    {
        const int32_t rowCount = 200;
        const int32_t nonNullCount = 200;
        CountColumnAggregatorFactory factory;
        std::vector<int32_t> channel0 = {0};
        std::vector<int32_t> channelNull = {1};
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);

        auto countCol = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(LongType()).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel0, true, false, false);
        auto countNullCol = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(LongType()).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channelNull, true, false, false);
        countCol->SetExecutionContext(executionContext.get());
        countNullCol->SetExecutionContext(executionContext.get());

        BaseVector *longCol = BuildCountInputColumn(LongType(), rowCount, nonNullCount);
        BaseVector *nullCol = BuildCountInputColumn(NoneType(), rowCount, 0);
        VectorBatch *vecBatch = new VectorBatch(rowCount);
        vecBatch->Append(longCol);
        vecBatch->Append(nullCol);

        auto state = NewAndInitState(countCol.get());
        countCol->ProcessGroup(state.get(), vecBatch, 0, rowCount);
        EXPECT_EQ(nonNullCount, *ExtractValueFromState<int64_t>(state.get()));

        state = NewAndInitState(countNullCol.get());
        countNullCol->ProcessGroup(state.get(), vecBatch, 0, rowCount);
        EXPECT_EQ(0, *ExtractValueFromState<int64_t>(state.get()));

        VectorHelper::FreeVecBatch(vecBatch);
    }

    TEST(CountColumnAggregatorTest, CountColumnMixedNullScalarTypes)
    {
        const int32_t rowCount = 10;
        const int32_t nonNullCount = 6;
        CountColumnAggregatorFactory factory;
        std::vector<int32_t> channel = {0};
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);

        auto testScalarType = [&](const DataTypePtr &inputType) {
            auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(inputType).get(),
                *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
            ASSERT_NE(agg, nullptr);
            agg->SetExecutionContext(executionContext.get());
            BaseVector *col = BuildCountInputColumn(inputType, rowCount, nonNullCount);
            if (col == nullptr) {
                return;
            }
            VectorBatch *batch = new VectorBatch(rowCount);
            batch->Append(col);
            auto state = NewAndInitState(agg.get());
            agg->ProcessGroup(state.get(), batch, 0, rowCount);
            EXPECT_EQ(nonNullCount, *ExtractValueFromState<int64_t>(state.get())) << "type id " << inputType->GetId();
            VectorHelper::FreeVecBatch(batch);
        };

        testScalarType(BooleanType());
        testScalarType(ByteType());
        testScalarType(ShortType());
        testScalarType(IntType());
        testScalarType(LongType());
        testScalarType(FloatType());
        testScalarType(DoubleType());
        testScalarType(Decimal64Type(18, 2));
        testScalarType(Decimal128Type(38, 0));
        testScalarType(VarcharType(10));
        testScalarType(CharType(10));
        testScalarType(VarBinaryType(10));
    }

    TEST(CountColumnAggregatorTest, CountColumnComplexTypeArray)
    {
        const int32_t rowCount = 5;
        const int32_t nonNullCount = 3;
        CountColumnAggregatorFactory factory;
        std::vector<int32_t> channel = {0};
        auto arrayType = std::make_shared<ArrayType>(IntType());
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
        auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(arrayType).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
        ASSERT_NE(agg, nullptr);
        agg->SetExecutionContext(executionContext.get());

        auto *elemVec = new Vector<int32_t>(nonNullCount);
        for (int32_t i = 0; i < nonNullCount; i++) {
            elemVec->SetValue(i, i + 1);
        }
        auto *arrayCol = new ArrayVector(rowCount, std::shared_ptr<BaseVector>(elemVec));
        arrayCol->SetOffset(0, 0);
        arrayCol->SetSize(0, 1);
        arrayCol->SetOffset(1, 1);
        arrayCol->SetSize(1, 1);
        arrayCol->SetOffset(2, 2);
        arrayCol->SetSize(2, 1);
        arrayCol->SetOffset(3, 3);
        arrayCol->SetSize(3, 0);
        arrayCol->SetOffset(4, 3);
        arrayCol->SetSize(4, 0);
        arrayCol->SetNull(3);
        arrayCol->SetNull(4);

        VectorBatch *batch = new VectorBatch(rowCount);
        batch->Append(arrayCol);
        auto state = NewAndInitState(agg.get());
        agg->ProcessGroup(state.get(), batch, 0, rowCount);
        EXPECT_EQ(nonNullCount, *ExtractValueFromState<int64_t>(state.get()));
        VectorHelper::FreeVecBatch(batch);
    }

    TEST(CountColumnAggregatorTest, CountColumnComplexTypeMap)
    {
        const int32_t rowCount = 5;
        const int32_t nonNullCount = 3;
        CountColumnAggregatorFactory factory;
        std::vector<int32_t> channel = {0};
        auto mapType = std::make_shared<MapType>(IntType(), IntType());
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
        auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(mapType).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
        ASSERT_NE(agg, nullptr);
        agg->SetExecutionContext(executionContext.get());

        auto keyVec = std::make_shared<Vector<int32_t>>(nonNullCount);
        auto valVec = std::make_shared<Vector<int32_t>>(nonNullCount);
        for (int32_t i = 0; i < nonNullCount; i++) {
            keyVec->SetValue(i, i);
            valVec->SetValue(i, i + 10);
        }
        auto *mapCol = new MapVector(rowCount, keyVec, valVec);
        mapCol->SetOffset(0, 0);
        mapCol->SetSize(0, 1);
        mapCol->SetOffset(1, 1);
        mapCol->SetSize(1, 1);
        mapCol->SetOffset(2, 2);
        mapCol->SetSize(2, 1);
        mapCol->SetOffset(3, 3);
        mapCol->SetSize(3, 0);
        mapCol->SetOffset(4, 3);
        mapCol->SetSize(4, 0);
        mapCol->SetNull(3);
        mapCol->SetNull(4);

        VectorBatch *batch = new VectorBatch(rowCount);
        batch->Append(mapCol);
        auto state = NewAndInitState(agg.get());
        agg->ProcessGroup(state.get(), batch, 0, rowCount);
        EXPECT_EQ(nonNullCount, *ExtractValueFromState<int64_t>(state.get()));
        VectorHelper::FreeVecBatch(batch);
    }

    TEST(CountColumnAggregatorTest, CountColumnComplexTypeRow)
    {
        const int32_t rowCount = 5;
        const int32_t nonNullCount = 3;
        CountColumnAggregatorFactory factory;
        std::vector<int32_t> channel = {0};
        std::vector<DataTypePtr> fieldTypes = {IntType()};
        auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"f0"});
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
        auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(rowType).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
        ASSERT_NE(agg, nullptr);
        agg->SetExecutionContext(executionContext.get());

        auto childVec = std::make_shared<Vector<int32_t>>(rowCount);
        for (int32_t i = 0; i < rowCount; i++) {
            childVec->SetValue(i, i + 1);
        }
        childVec->SetNull(3);
        childVec->SetNull(4);
        std::vector<std::shared_ptr<BaseVector>> children = {childVec};
        auto *rowCol = new RowVector(rowCount, children);
        rowCol->SetNull(3);
        rowCol->SetNull(4);

        VectorBatch *batch = new VectorBatch(rowCount);
        batch->Append(rowCol);
        auto state = NewAndInitState(agg.get());
        agg->ProcessGroup(state.get(), batch, 0, rowCount);
        EXPECT_EQ(nonNullCount, *ExtractValueFromState<int64_t>(state.get()));
        VectorHelper::FreeVecBatch(batch);
    }

    // ---- CountAllAggregator: counts all rows (ignores column nulls) ----

    TEST(CountColumnAggregatorTest, CountAllCountsAllRows)
    {
        const int32_t rowCount = 200;
        CountAllAggregatorFactory factory;
        std::vector<int32_t> channel = {-1};
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
        auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(NoneType()).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
        ASSERT_NE(agg, nullptr);
        agg->SetExecutionContext(executionContext.get());

        BaseVector *anyCol = BuildCountInputColumn(LongType(), rowCount, rowCount);
        VectorBatch *batch = new VectorBatch(rowCount);
        batch->Append(anyCol);
        auto state = NewAndInitState(agg.get());
        agg->ProcessGroup(state.get(), batch, 0, rowCount);
        EXPECT_EQ(rowCount, *ExtractValueFromState<int64_t>(state.get()));
        VectorHelper::FreeVecBatch(batch);
    }

    TEST(CountColumnAggregatorTest, CountAllCountsAllRowsEvenWhenColumnIsNull)
    {
        const int32_t rowCount = 200;
        CountAllAggregatorFactory factory;
        std::vector<int32_t> channel = {-1};
        auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
        auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(NoneType()).get(),
            *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channel, true, false, false);
        ASSERT_NE(agg, nullptr);
        agg->SetExecutionContext(executionContext.get());

        BaseVector *nullCol = BuildCountInputColumn(NoneType(), rowCount, 0);
        VectorBatch *batch = new VectorBatch(rowCount);
        batch->Append(nullCol);
        auto state = NewAndInitState(agg.get());
        agg->ProcessGroup(state.get(), batch, 0, rowCount);
        EXPECT_EQ(rowCount, *ExtractValueFromState<int64_t>(state.get()));
        VectorHelper::FreeVecBatch(batch);
    }

    // ---- Exception: Create with wrong output type returns nullptr ----

    TEST(CountColumnAggregatorTest, CreateWithNonLongOutputReturnsNull)
    {
        DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType()});
        DataTypes outputTypes(std::vector<DataTypePtr>{DoubleType()});
        std::vector<int32_t> channels = {0};
        bool rawIn = false, partialOut = false, isOverflowAsNull = false;

        auto countAgg = CountColumnAggregator<OMNI_DOUBLE, OMNI_DOUBLE>::Create(
            inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull);
        EXPECT_EQ(countAgg, nullptr);

        auto countAllAgg = CountAllAggregator<OMNI_NONE, OMNI_DOUBLE>::Create(
            inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull);
        EXPECT_EQ(countAllAgg, nullptr);
    }

    TEST(CountColumnAggregatorTest, PartialMergeExpectsLongInput)
    {
        DataTypes inputTypes(std::vector<DataTypePtr>{DoubleType()});
        DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
        std::vector<int32_t> channels = {0};
        bool rawIn = false, partialOut = false, isOverflowAsNull = false;

        auto countAgg = CountColumnAggregator<OMNI_DOUBLE, OMNI_LONG>::Create(
            inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull);
        EXPECT_EQ(countAgg, nullptr);

        auto countAllAgg = CountAllAggregator<OMNI_DOUBLE, OMNI_LONG>::Create(
            inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull);
        EXPECT_EQ(countAllAgg, nullptr);
    }
}