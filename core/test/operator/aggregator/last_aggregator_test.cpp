/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2026. All rights reserved.
 * Description: Unit tests for Last aggregation (last_ignore_null, last_include_null).
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

// ---- Last ignore null: Short ----
TEST(LastAggregatorTest, IgnoreNull_Short_LastNonNull)
{
    LastAggregatorFactory factory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
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
    input2->SetValue(2, 212);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resVal->GetValue(0));
    EXPECT_TRUE(resSet->GetValue(0));

    agg->ProcessGroup(state.get(), batch2, 2);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(212, resVal->GetValue(0));
    EXPECT_TRUE(resSet->GetValue(0));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- Last ignore null: Int ----
TEST(LastAggregatorTest, IgnoreNull_Int_LastNonNull)
{
    LastAggregatorFactory factory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
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
    input2->SetValue(2, 212);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(211, resVal->GetValue(0));

    agg->ProcessGroup(state.get(), batch2, 2);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(212, resVal->GetValue(0));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- Last ignore null: Byte (OMNI_BYTE) ----
TEST(LastAggregatorTest, IgnoreNull_Byte_LastNonNull)
{
    LastAggregatorFactory factory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
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
    input2->SetValue(2, 101);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(100, resVal->GetValue(0));

    agg->ProcessGroup(state.get(), batch2, 2);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(101, resVal->GetValue(0));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- Last ignore null: Varbinary (OMNI_VARBINARY) ----
TEST(LastAggregatorTest, IgnoreNull_Varbinary_LastNonNull)
{
    LastAggregatorFactory factory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL);
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
    std::string valXyz("xyz");
    std::string_view svAbc(valAbc.data(), valAbc.size());
    std::string_view svXyz(valXyz.data(), valXyz.size());
    auto *input2 = new Vector<LargeStringContainer<std::string_view>>(3);
    input2->SetNull(0);
    input2->SetValue(1, svAbc);
    input2->SetValue(2, svXyz);
    auto *batch2 = new VectorBatch(1);
    batch2->Append(input2);

    agg->ProcessGroup(state.get(), batch2, 1);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(0, std::memcmp(resVal->GetValue(0).data(), "abc", 3));

    agg->ProcessGroup(state.get(), batch2, 2);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_EQ(0, std::memcmp(resVal->GetValue(0).data(), "xyz", 3));

    VectorHelper::FreeVecBatch(batch1);
    VectorHelper::FreeVecBatch(batch2);
    delete resVal;
    delete resSet;
}

// ---- Last include null: Int ----
TEST(LastAggregatorTest, IncludeNull_Int_LastIsNull)
{
    LastAggregatorFactory factory(OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL);
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

    agg->ProcessGroup(state.get(), batch, 4);
    agg->ExtractValues(state.get(), extractVecs, 0);
    EXPECT_TRUE(resVal->IsNull(0));
    EXPECT_TRUE(resSet->GetValue(0));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
    delete resSet;
}

}  // namespace omniruntime
