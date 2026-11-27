/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2026. All rights reserved.
 * Description: Unit tests for Min aggregation. Covers INT, LONG, OMNI_BYTE, OMNI_VARBINARY.
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

// ---- Min: Int ----
TEST(MinAggregatorTest, Int_Basic)
{
    MinAggregatorFactory factory;
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input = new Vector<int32_t>(5);
    input->SetValue(0, 10);
    input->SetValue(1, 3);
    input->SetValue(2, 7);
    input->SetValue(3, -5);
    input->SetValue(4, 2);
    auto *batch = new VectorBatch(1);
    batch->Append(input);

    auto *resVal = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVecs = {resVal};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch, 0, 5);
    agg->ExtractValues(state.get(), extractVecs, 0);

    EXPECT_FALSE(resVal->IsNull(0));
    EXPECT_EQ(-5, resVal->GetValue(0));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
}

// ---- Min: Long ----
TEST(MinAggregatorTest, Long_Basic)
{
    MinAggregatorFactory factory;
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(LongType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(LongType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input = new Vector<int64_t>(4);
    input->SetValue(0, 100);
    input->SetValue(1, 50);
    input->SetValue(2, 200);
    input->SetValue(3, 25);
    auto *batch = new VectorBatch(1);
    batch->Append(input);

    auto *resVal = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVecs = {resVal};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch, 0, 4);
    agg->ExtractValues(state.get(), extractVecs, 0);

    EXPECT_FALSE(resVal->IsNull(0));
    EXPECT_EQ(25, resVal->GetValue(0));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
}

// ---- Min: Byte (OMNI_BYTE) ----
TEST(MinAggregatorTest, Byte_Basic)
{
    MinAggregatorFactory factory;
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(ByteType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(ByteType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input = new Vector<int8_t>(5);
    input->SetValue(0, 10);
    input->SetValue(1, 3);
    input->SetValue(2, 7);
    input->SetValue(3, -5);
    input->SetValue(4, 2);
    auto *batch = new VectorBatch(1);
    batch->Append(input);

    auto *resVal = new Vector<int8_t>(1);
    std::vector<BaseVector *> extractVecs = {resVal};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch, 0, 5);
    agg->ExtractValues(state.get(), extractVecs, 0);

    EXPECT_FALSE(resVal->IsNull(0));
    EXPECT_EQ(-5, resVal->GetValue(0));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
}

// ---- Min: Varbinary (OMNI_VARBINARY), byte-order ----
TEST(MinAggregatorTest, Varbinary_Basic)
{
    MinAggregatorFactory factory;
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(VarBinaryType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(VarBinaryType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    std::string vals[] = {"xyz", "abc", "mno"};
    auto *input = new Vector<LargeStringContainer<std::string_view>>(3);
    for (int i = 0; i < 3; i++) {
        std::string_view sv(vals[i].data(), vals[i].size());
        input->SetValue(i, sv);
    }
    auto *batch = new VectorBatch(1);
    batch->Append(input);

    auto *resVal = new Vector<LargeStringContainer<std::string_view>>(1);
    std::vector<BaseVector *> extractVecs = {resVal};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch, 0, 3);
    agg->ExtractValues(state.get(), extractVecs, 0);

    EXPECT_FALSE(resVal->IsNull(0));
    auto res = resVal->GetValue(0);
    EXPECT_EQ(3u, res.size());
    EXPECT_EQ(0, std::memcmp(res.data(), "abc", 3));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
}

// ---- Min: Null input ----
TEST(MinAggregatorTest, Int_AllNull)
{
    MinAggregatorFactory factory;
    std::vector<int32_t> ch = {0};
    auto inTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto outTypes = AggregatorUtil::WrapWithDataTypes(IntType());
    auto agg = factory.CreateAggregator(*inTypes, *outTypes, ch, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto ctx = std::make_unique<ExecutionContext>(4096, nullptr);
    agg->SetExecutionContext(ctx.get());

    auto *input = new Vector<int32_t>(3);
    input->SetNull(0);
    input->SetNull(1);
    input->SetNull(2);
    auto *batch = new VectorBatch(1);
    batch->Append(input);

    auto *resVal = new Vector<int32_t>(1);
    std::vector<BaseVector *> extractVecs = {resVal};

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), batch, 0, 3);
    agg->ExtractValues(state.get(), extractVecs, 0);

    EXPECT_TRUE(resVal->IsNull(0));

    VectorHelper::FreeVecBatch(batch);
    delete resVal;
}

}  // namespace omniruntime
