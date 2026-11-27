/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2026. All rights reserved.
 * Description: Unit tests for First aggregation (first_ignore_null, first_include_null).
 *              Covers INT, SHORT, LONG, OMNI_BYTE, OMNI_VARBINARY.
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
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

// ---- First ignore null: Short ----
TEST(FirstAggregatorTest, IgnoreNull_Short_AllNullThenValue)
{
    FirstAggregatorFactory factory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(ShortType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(ShortType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input1 = new Vector<short>(5);
    for (int i = 0; i < 5; i++)
        input1->SetNull(i);
    auto *batch1 = new VectorBatch(1);
    batch1->Append(input1);

    auto *resVal = new Vector<short>(1);
    auto *resSet = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resVal, resSet};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch1, 0);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));
    EXPECT_FALSE(resSet->GetValue(0));

    auto *input2 = new Vector<short>(3);
    input2->SetNull(0);
    input2->SetValue(1, 211);
    input2->SetNull(2);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 0);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resVal->GetValue(0));
    EXPECT_TRUE(resSet->GetValue(0));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- First ignore null: Int ----
TEST(FirstAggregatorTest, IgnoreNull_Int_AllNullThenValue)
{
    FirstAggregatorFactory factory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input1 = new Vector<int32_t>(5);
    for (int i = 0; i < 5; i++)
        input1->SetNull(i);
    auto *batch1 = new VectorBatch(1);
    batch1->Append(input1);

    auto *resVal = new Vector<int32_t>(1);
    auto *resSet = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resVal, resSet};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch1, 0);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));
    EXPECT_FALSE(resSet->GetValue(0));

    auto *input2 = new Vector<int32_t>(3);
    input2->SetNull(0);
    input2->SetValue(1, 211);
    input2->SetNull(2);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resVal->GetValue(0));
    EXPECT_TRUE(resSet->GetValue(0));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- First ignore null: Byte (OMNI_BYTE) ----
TEST(FirstAggregatorTest, IgnoreNull_Byte_AllNullThenValue)
{
    FirstAggregatorFactory factory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(ByteType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(ByteType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input1 = new Vector<int8_t>(5);
    for (int i = 0; i < 5; i++)
        input1->SetNull(i);
    auto *batch1 = new VectorBatch(1);
    batch1->Append(input1);

    auto *resVal = new Vector<int8_t>(1);
    auto *resSet = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resVal, resSet};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch1, 0);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));
    EXPECT_FALSE(resSet->GetValue(0));

    auto *input2 = new Vector<int8_t>(3);
    input2->SetNull(0);
    input2->SetValue(1, 100);
    input2->SetNull(2);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(100, resVal->GetValue(0));
    EXPECT_TRUE(resSet->GetValue(0));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- First ignore null: Varbinary (OMNI_VARBINARY) ----
TEST(FirstAggregatorTest, IgnoreNull_Varbinary_AllNullThenValue)
{
    FirstAggregatorFactory factory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL);
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(VarBinaryType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(VarBinaryType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input1 = new Vector<LargeStringContainer<std::string_view>>(5);
    for (int i = 0; i < 5; i++)
        input1->SetNull(i);
    auto *batch1 = new VectorBatch(1);
    batch1->Append(input1);

    auto *resVal = new Vector<LargeStringContainer<std::string_view>>(1);
    auto *resSet = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resVal, resSet};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch1, 0);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));
    EXPECT_FALSE(resSet->GetValue(0));

    std::string valAbc("abc");
    std::string_view svAbc(valAbc.data(), valAbc.size());
    auto *input2 = new Vector<LargeStringContainer<std::string_view>>(3);
    input2->SetNull(0);
    input2->SetValue(1, svAbc);
    input2->SetNull(2);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_FALSE(resVal->IsNull(0));
    EXPECT_TRUE(resSet->GetValue(0));
    EXPECT_EQ(3u, resVal->GetValue(0).size());
    EXPECT_EQ(0, std::memcmp(resVal->GetValue(0).data(), "abc", 3));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- First include null: Int ----
TEST(FirstAggregatorTest, IncludeNull_Int_FirstIsNull)
{
    FirstAggregatorFactory factory(OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL);
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input = new Vector<int32_t>(5);
    input->SetNull(0);
    input->SetNull(1);
    input->SetValue(2, 111);
    input->SetValue(3, 113);
    input->SetNull(4);
    auto *batch = new VectorBatch(1);
    batch->Append(input);

    auto *resVal = new Vector<int32_t>(1);
    auto *resSet = new Vector<bool>(1);
    std::vector<BaseVector *> extractVecs = {resVal, resSet};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch, 0);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));
    EXPECT_TRUE(resSet->GetValue(0));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
    delete resSet;
}

}  // namespace omniruntime
