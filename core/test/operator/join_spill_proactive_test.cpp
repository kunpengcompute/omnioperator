/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Compact proactive Join Spill coverage for the upstream branch.
 */
#include <climits>
#include <sys/stat.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "expression/expressions.h"
#include "gtest/gtest.h"
#include "join_spill_dual_run_test_util.h"
#include "memory/memory_manager.h"
#include "operator/join/hash_builder.h"
#include "operator/join/hash_builder_expr.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_join_expr.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/test_util.h"
#include "vector/large_string_container.h"
#include "vector/vector_helper.h"
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using omniruntime::OMNI_BUILD_LEFT;
using omniruntime::OMNI_BUILD_RIGHT;
using omniruntime::OMNI_BUILD_UNKNOWN;
using omniruntime::TestUtil::GenerateSpillPath;
namespace expr = omniruntime::expressions;
namespace op = omniruntime::op;
namespace {
using join_spill_dual_run::DrainProbeRows;
using join_spill_dual_run::FindLongKeyForSubPartition;
using join_spill_dual_run::JoinSpillTestSubPartitionConfig;
using join_spill_dual_run::MakeJoinSpillTestSparkConfig;
using join_spill_dual_run::MakeLongKeyValueBatch;
VectorBatch *MakeLongBatch(const std::vector<int64_t> &keys, const std::vector<int64_t> &values,
    const std::vector<int32_t> &nullRows = {})
{
    auto *batch = MakeLongKeyValueBatch(keys, values);
    if (!nullRows.empty()) {
        auto *keyVec = reinterpret_cast<Vector<int64_t> *>(batch->Get(0));
        for (int32_t row : nullRows) {
            keyVec->SetNull(row);
        }
    }
    return batch;
}
VectorBatch *MakeDateBatch(const std::vector<int32_t> &keys, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<int32_t>(static_cast<int32_t>(keys.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    keyVec->SetDataType(Date32Type(DAY));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), keys[i]);
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}
VectorBatch *MakeVarcharBatch(const std::vector<std::string> &keys, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<LargeStringContainer<std::string_view>>(static_cast<int32_t>(keys.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), std::string_view(keys[i]));
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}
VectorBatch *MakeCompositeBatch(const std::vector<int64_t> &k1, const std::vector<int64_t> &k2, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(k1.size()));
    auto *key1 = new Vector<int64_t>(static_cast<int32_t>(k1.size()));
    auto *key2 = new Vector<int64_t>(static_cast<int32_t>(k2.size()));
    auto *value = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < k1.size(); ++i) {
        key1->SetValue(static_cast<int32_t>(i), k1[i]);
        key2->SetValue(static_cast<int32_t>(i), k2[i]);
        value->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(key1);
    batch->Append(key2);
    batch->Append(value);
    return batch;
}
struct Harness {
    std::shared_ptr<JoinSpillState> state;
    HashBuilderOperatorFactory *hashFactory = nullptr;
    LookupJoinOperatorFactory *lookupFactory = nullptr;
};
Harness MakeHarness(const std::string &dir, const DataTypes &types, int32_t hashCount, bool spill, bool force,
    JoinType joinType, BuildSide buildSide)
{
    Harness harness;
    std::vector<int32_t> hashCols(static_cast<size_t>(hashCount));
    for (int32_t i = 0; i < hashCount; ++i) {
        hashCols[static_cast<size_t>(i)] = i;
    }
    harness.hashFactory = buildSide == OMNI_BUILD_UNKNOWN
        ? HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, types, hashCols.data(), hashCount, 1)
        : HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide, types, hashCols.data(),
              hashCount, 1);
    if (spill) {
        harness.state = std::make_shared<JoinSpillState>(dir, types, types, 4);
        harness.hashFactory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
        harness.hashFactory->SetJoinSpillState(harness.state);
        harness.hashFactory->SetSpillConfig(MakeJoinSpillTestSparkConfig(dir));
        harness.hashFactory->SetJoinSpillMemThresholdForTest(force ? 0 : LLONG_MAX);
    }
    std::vector<int32_t> outputs(static_cast<size_t>(types.GetSize()));
    for (int32_t i = 0; i < types.GetSize(); ++i) {
        outputs[static_cast<size_t>(i)] = i;
    }
    harness.lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, outputs.data(),
        types.GetSize(), hashCols.data(), hashCount, outputs.data(), types.GetSize(), types,
        reinterpret_cast<int64_t>(harness.hashFactory), nullptr, false, nullptr);
    if (spill) {
        harness.lookupFactory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
        harness.lookupFactory->SetJoinSpillState(harness.state);
    }
    return harness;
}
void Destroy(const Harness &harness, op::Operator *build, op::Operator *probe)
{
    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(build);
    delete harness.lookupFactory;
    delete harness.hashFactory;
}
join_spill_dual_run::TupleMultiset RunOnce(const std::string &dir, const DataTypes &types, int32_t hashCount,
    bool spill, bool force, JoinType joinType, BuildSide buildSide, const std::function<VectorBatch *()> &buildBatch,
    const std::function<VectorBatch *()> &probeBatch, uint64_t *spilledRows)
{
    auto harness = MakeHarness(dir, types, hashCount, spill, force, joinType, buildSide);
    auto *build = harness.hashFactory->CreateOperator();
    auto *probe = harness.lookupFactory->CreateOperator();
    build->AddInput(buildBatch());
    probe->AddInput(probeBatch());
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, harness.hashFactory, buildSide == OMNI_BUILD_UNKNOWN ? OMNI_BUILD_RIGHT : buildSide);
    if (spilledRows != nullptr && harness.state != nullptr) {
        *spilledRows = harness.state->BuildSpilledRows();
    }
    Destroy(harness, build, probe);
    return rows;
}
void ExpectEqual(join_spill_dual_run::TupleMultiset expected, join_spill_dual_run::TupleMultiset actual)
{
    join_spill_dual_run::SortTupleMultiset(&expected);
    join_spill_dual_run::SortTupleMultiset(&actual);
    EXPECT_EQ(expected, actual);
}
enum class ParityKind {
    LongInnerRight,
    VarcharInnerRight,
    Date32InnerRight,
    CompositeInnerRight,
    ExprInnerRight,
    LongInnerLeft,
    LongLeftRight,
    LongRightRight,
    LongFullRight,
};
struct ParityParam {
    ParityKind kind;
    const char *name;
};
class ForcedSpillEqualsResidentTest : public testing::TestWithParam<ParityParam> {};
join_spill_dual_run::TupleMultiset RunExpr(const std::string &dir, bool force)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto overflow = new expr::OverflowConfig();
    std::vector<expr::Expr *> buildKeys{new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, LongType()),
        new expr::LiteralExpr(0L, LongType()), LongType())};
    auto *hashExpr = HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildKeys, 1, overflow);
    auto *hashFactory = hashExpr->GetHashBuilderOperatorFactory();
    auto state = std::make_shared<JoinSpillState>(dir, hashFactory->GetBuildTypes(), hashFactory->GetBuildTypes(), 4);
    hashFactory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
    hashFactory->SetJoinSpillState(state);
    hashFactory->SetSpillConfig(MakeJoinSpillTestSparkConfig(dir));
    hashFactory->SetJoinSpillMemThresholdForTest(force ? 0 : LLONG_MAX);
    std::vector<expr::Expr *> probeKeys{new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, LongType()),
        new expr::LiteralExpr(0L, LongType()), LongType())};
    int32_t outputs[] = {0, 1};
    auto *lookupExpr = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(buildTypes, outputs,
        2, probeKeys, 1, outputs, 2, buildTypes, reinterpret_cast<int64_t>(hashFactory), nullptr, false, overflow);
    lookupExpr->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
    lookupExpr->SetJoinSpillState(state);
    auto *build = hashExpr->CreateOperator();
    auto *probe = lookupExpr->CreateOperator();
    build->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    probe->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, hashFactory, OMNI_BUILD_RIGHT);
    EXPECT_EQ(force, state->BuildSpilledRows() > 0);
    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(build);
    expr::Expr::DeleteExprs(buildKeys);
    expr::Expr::DeleteExprs(probeKeys);
    delete lookupExpr;
    delete hashExpr;
    delete overflow;
    return rows;
}
void PumpBuildUntilHandoff(HashBuilderOperator *build, const std::shared_ptr<JoinSpillState> &state)
{
    for (int32_t i = 0; i < 1000 && state->GetHandoffPhase() == JoinHandoffPhase::Idle; ++i) {
        ContinueFuture future;
        if (build->IsBlocked(&future) != BlockingReason::kNotBlocked) {
            continue;
        }
        VectorBatch *out = nullptr;
        build->GetOutput(&out);
        if (out != nullptr) {
            VectorHelper::FreeVecBatch(out);
        }
    }
}
join_spill_dual_run::TupleMultiset RunHeldRestoreTwoProbeFiles(const std::string &path, bool forceSpill)
{
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    const int64_t matched = FindLongKeyForSubPartition(1);
    const int64_t unmatched = FindLongKeyForSubPartition(1, matched);
    const int64_t miss = FindLongKeyForSubPartition(2);
    auto harness = MakeHarness(path, types, 1, true, forceSpill, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);
    if (forceSpill) {
        harness.hashFactory->SetJoinSpillMemThresholdForTest(LLONG_MIN);
    }
    harness.state->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);
    auto *build = dynamic_cast<HashBuilderOperator *>(harness.hashFactory->CreateOperator());
    auto *probe = dynamic_cast<LookupJoinOperator *>(harness.lookupFactory->CreateOperator());
    build->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    build->noMoreInput();
    PumpBuildUntilHandoff(build, harness.state);
    if (forceSpill) {
        EXPECT_EQ(JoinHandoffPhase::ProbePathA, harness.state->GetHandoffPhase());
        probe->AddInput(MakeLongBatch({matched}, {200}));
        probe->AddInput(MakeLongBatch({matched, miss}, {201, 990}));
    } else {
        probe->AddInput(MakeLongBatch({matched, matched, miss}, {200, 201, 990}));
    }
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, harness.hashFactory, OMNI_BUILD_RIGHT);
    if (forceSpill) {
        EXPECT_TRUE(harness.state->WasRestorePartitionFlushed(1));
    }
    Destroy(harness, build, probe);
    return rows;
}
} // namespace
TEST_P(ForcedSpillEqualsResidentTest, SpillMatchesResident)
{
    const auto dir = join_spill_dual_run::ScopedTempDirectory(GetParam().name);
    const std::string path = dir.path().string();
    const DataTypes longTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    uint64_t spilled = 0;
    join_spill_dual_run::TupleMultiset resident;
    join_spill_dual_run::TupleMultiset spilledRows;
    const auto both = [&](const DataTypes &types, int32_t keyCount, JoinType joinType, BuildSide buildSide,
                           const std::function<VectorBatch *()> &build, const std::function<VectorBatch *()> &probe) {
        resident = RunOnce(path, types, keyCount, true, false, joinType, buildSide, build, probe, nullptr);
        spilledRows = RunOnce(path, types, keyCount, true, true, joinType, buildSide, build, probe, &spilled);
    };
    const auto longBuild = []() { return MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}); };
    const auto longProbe = []() { return MakeLongBatch({2, 4}, {200, 400}); };
    const auto outerBuild = []() { return MakeLongBatch({1, 2}, {10, 20}); };
    const auto outerProbe = []() { return MakeLongBatch({2, 99}, {200, 990}); };
    switch (GetParam().kind) {
        case ParityKind::LongInnerRight:
            both(longTypes, 1, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, longBuild, longProbe); break;
        case ParityKind::VarcharInnerRight: {
            const DataTypes types(std::vector<DataTypePtr>({VarcharType(), LongType()}));
            both(types, 1, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT,
                []() { return MakeVarcharBatch({"alpha", "beta", "gamma", "delta"}, {10, 20, 30, 40}); },
                []() { return MakeVarcharBatch({"alpha", "delta", "missing"}, {101, 104, 199}); });
            break;
        }
        case ParityKind::Date32InnerRight: {
            const DataTypes types(std::vector<DataTypePtr>({Date32Type(DAY), LongType()}));
            both(types, 1, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT,
                []() { return MakeDateBatch({10, 20, 30, 40}, {10, 20, 30, 40}); },
                []() { return MakeDateBatch({20, 40}, {200, 400}); });
            break;
        }
        case ParityKind::CompositeInnerRight: {
            const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType(), LongType()}));
            both(types, 2, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT,
                []() { return MakeCompositeBatch({1, 1, 2, 3}, {10, 11, 20, 30}, {100, 101, 200, 300}); },
                []() { return MakeCompositeBatch({1, 2, 99}, {11, 20, 1}, {1100, 2000, 199}); });
            break;
        }
        case ParityKind::ExprInnerRight:
            resident = RunExpr(path, false);
            spilledRows = RunExpr(path, true);
            spilled = 1;
            break;
        case ParityKind::LongInnerLeft:
            both(longTypes, 1, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT, longBuild, longProbe); break;
        case ParityKind::LongLeftRight:
            both(longTypes, 1, OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT, outerBuild, outerProbe); break;
        case ParityKind::LongRightRight:
            both(longTypes, 1, OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT, outerBuild, outerProbe); break;
        case ParityKind::LongFullRight:
            both(longTypes, 1, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, outerBuild, outerProbe); break;
    }
    ExpectEqual(resident, spilledRows);
    EXPECT_GT(spilled, 0u);
}
INSTANTIATE_TEST_SUITE_P(JoinSpillProactiveTest, ForcedSpillEqualsResidentTest, testing::Values(
    ParityParam{ParityKind::LongInnerRight, "LongInnerRight"}, ParityParam{ParityKind::VarcharInnerRight, "VarcharInnerRight"},
    ParityParam{ParityKind::Date32InnerRight, "Date32InnerRight"}, ParityParam{ParityKind::CompositeInnerRight, "CompositeInnerRight"},
    ParityParam{ParityKind::ExprInnerRight, "ExprInnerRight"}, ParityParam{ParityKind::LongInnerLeft, "LongInnerLeft"},
    ParityParam{ParityKind::LongLeftRight, "LongLeftRight"}, ParityParam{ParityKind::LongRightRight, "LongRightRight"},
    ParityParam{ParityKind::LongFullRight, "LongFullRight"}),
    [](const testing::TestParamInfo<ParityParam> &info) { return info.param.name; });
TEST(JoinSpillProactiveTest, DisabledAndBelowThresholdDoNotCreateSession)
{
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto disabled = MakeHarness(path, types, 1, false, false, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_UNKNOWN);
    auto *build = disabled.hashFactory->CreateOperator();
    auto *probe = disabled.lookupFactory->CreateOperator();
    build->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    probe->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, nullptr, OMNI_BUILD_RIGHT);
    EXPECT_EQ(rows.size(), 2u);
    EXPECT_EQ(disabled.state, nullptr);
    Destroy(disabled, build, probe);
    auto below = MakeHarness(path, types, 1, true, false, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT);
    build = below.hashFactory->CreateOperator();
    probe = below.lookupFactory->CreateOperator();
    build->AddInput(MakeLongBatch({1, 2}, {10, 20}));
    build->AddInput(MakeLongBatch({3, 4}, {30, 40}));
    probe->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    rows.clear();
    DrainProbeRows(build, probe, &rows, below.hashFactory, OMNI_BUILD_RIGHT);
    EXPECT_EQ(rows.size(), 2u);
    EXPECT_EQ(below.state->BuildSpilledRows(), 0u);
    Destroy(below, build, probe);
}
TEST(JoinSpillProactiveTest, EobForecastSpillsBeforeSlotOpen)
{
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto harness = MakeHarness(path, types, 1, true, false, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT);
    auto *build = dynamic_cast<HashBuilderOperator *>(harness.hashFactory->CreateOperator());
    build->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    EXPECT_EQ(harness.state->BuildSpilledRows(), 0u);
    harness.hashFactory->SetJoinSpillMemThresholdForTest(
        omniruntime::mem::MemoryManager::GetGlobalAccountedMemory() + 1);
    auto *probe = harness.lookupFactory->CreateOperator();
    probe->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, harness.hashFactory, OMNI_BUILD_RIGHT);
    EXPECT_EQ(rows.size(), 2u);
    EXPECT_GT(harness.state->BuildSpilledRows(), 0u);
    Destroy(harness, build, probe);
}
TEST(JoinSpillProactiveTest, PathAMixedBatchRestoresAscending)
{
    const int64_t emptyKey = FindLongKeyForSubPartition(0);
    const int64_t keptKey = FindLongKeyForSubPartition(1);
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto harness = MakeHarness(path, types, 1, true, true, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT);
    auto *build = harness.hashFactory->CreateOperator();
    auto *probe = harness.lookupFactory->CreateOperator();
    build->AddInput(MakeLongBatch({keptKey}, {10}));
    probe->AddInput(MakeLongBatch({emptyKey, keptKey}, {100, 200}));
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, harness.hashFactory, OMNI_BUILD_RIGHT);
    EXPECT_EQ(rows.size(), 1u);
    EXPECT_EQ(JoinHandoffPhase::Complete, harness.state->GetHandoffPhase());
    EXPECT_GT(harness.state->BuildSpilledRows(), 0u);
    Destroy(harness, build, probe);
}
TEST(JoinSpillProactiveTest, AdaptiveRestoreOpen)
{
    const int64_t key1 = FindLongKeyForSubPartition(1);
    const int64_t key2 = FindLongKeyForSubPartition(2);
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto harness = MakeHarness(path, types, 1, true, true, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT);
    auto *build = dynamic_cast<HashBuilderOperator *>(harness.hashFactory->CreateOperator());
    auto *probe = dynamic_cast<LookupJoinOperator *>(harness.lookupFactory->CreateOperator());
    build->setNoMoreInput(false);
    harness.hashFactory->SetJoinSpillMemThresholdForTest(LLONG_MIN);
    build->AddInput(MakeLongBatch({key1}, {10}));
    ASSERT_GT(harness.state->BuildSpilledRows(), 0u);
    harness.hashFactory->SetJoinSpillMemThresholdForTest(LLONG_MAX);
    std::visit([&](auto &&arg) { arg.AddVecBatch(0, MakeLongBatch({key2}, {11})); },
        *harness.hashFactory->GetHashTablesVariants());
    build->setNoMoreInput(true);
    for (int32_t i = 0; i < 1000 && harness.state->GetHandoffPhase() == JoinHandoffPhase::Idle; ++i) {
        VectorBatch *out = nullptr;
        build->GetOutput(&out);
        if (out != nullptr) {
            VectorHelper::FreeVecBatch(out);
        }
    }
    ASSERT_EQ(JoinHandoffPhase::ProbePathA, harness.state->GetHandoffPhase());
    EXPECT_EQ(std::vector<uint32_t>({1, 2}), harness.state->SpillPartitionIds());
    harness.state->MarkProbePathAFinished();
    harness.hashFactory->SetJoinSpillMemThresholdForTest(1);
    for (int32_t round = 0; round < 2; ++round) {
        ASSERT_TRUE(build->TryProduceNextRestoreRound());
        EXPECT_TRUE(build->LastRestoreUsedAdaptiveOpen());
        auto snap = harness.state->WaitHandoffForProbe();
        ASSERT_TRUE(snap.restoredPartitionId.has_value());
        harness.state->ProbeFinished(*snap.restoredPartitionId);
    }
    EXPECT_EQ(JoinHandoffPhase::Complete, harness.state->GetHandoffPhase());
    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(build);
    delete harness.lookupFactory;
    delete harness.hashFactory;
}
TEST(JoinSpillProactiveTest, MultipleProbeRunsAreAllRestored)
{
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto resident = RunOnce(path, types, 1, true, false, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT,
        []() { return MakeLongBatch({1, 2, 3}, {10, 20, 30}); },
        []() { return MakeLongBatch({1, 2, 3}, {100, 200, 300}); }, nullptr);
    auto harness = MakeHarness(path, types, 1, true, true, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT);
    auto *build = dynamic_cast<HashBuilderOperator *>(harness.hashFactory->CreateOperator());
    auto *probe = dynamic_cast<LookupJoinOperator *>(harness.lookupFactory->CreateOperator());
    build->AddInput(MakeLongBatch({1, 2, 3}, {10, 20, 30}));
    build->noMoreInput();
    PumpBuildUntilHandoff(build, harness.state);
    ASSERT_EQ(JoinHandoffPhase::ProbePathA, harness.state->GetHandoffPhase());
    probe->AddInput(MakeLongBatch({1}, {100}));
    probe->AddInput(MakeLongBatch({2, 3}, {200, 300}));
    join_spill_dual_run::TupleMultiset spilled;
    DrainProbeRows(build, probe, &spilled, harness.hashFactory, OMNI_BUILD_RIGHT);
    EXPECT_GT(probe->ProbeSpilledRows(), 1u);
    ExpectEqual(resident, spilled);
    Destroy(harness, build, probe);
}
TEST(JoinSpillProactiveTest, OuterPathABehavior)
{
    const int64_t emptyKey = FindLongKeyForSubPartition(0);
    const int64_t keptKey = FindLongKeyForSubPartition(1);
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto left = RunOnce(path, types, 1, true, true, OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT,
        [&]() { return MakeLongBatch({keptKey}, {10}); },
        [&]() { return MakeLongBatch({emptyKey}, {100}); }, nullptr);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_TRUE(left[0].back().isNull);
    auto innerLeft = RunOnce(path, types, 1, true, true, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT,
        [&]() { return MakeLongBatch({keptKey}, {10}); },
        [&]() { return MakeLongBatch({emptyKey}, {100}); }, nullptr);
    EXPECT_EQ(innerLeft.size(), 0u);
    auto full = RunOnce(path, types, 1, true, true, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT,
        [&]() { return MakeLongBatch({keptKey}, {10}); },
        [&]() { return MakeLongBatch({emptyKey}, {100}); }, nullptr);
    EXPECT_GE(full.size(), 2u);
    auto nullProbe = RunOnce(path, types, 1, true, true, OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT,
        [&]() { return MakeLongBatch({keptKey}, {10}); },
        [&]() { return MakeLongBatch({0}, {100}, {0}); }, nullptr);
    ASSERT_EQ(nullProbe.size(), 1u);
    EXPECT_TRUE(nullProbe[0][0].isNull);
}
TEST(JoinSpillProactiveTest, RestoreFilterAndUnmatchedRows)
{
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto make = [&](bool force) {
        auto *filter = new expr::LiteralExpr(false, BooleanType());
        auto harness = MakeHarness(path, types, 1, true, force, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);
        int32_t hashCols[] = {0};
        int32_t outputs[] = {0, 1};
        delete harness.lookupFactory;
        harness.lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, outputs, 2,
            hashCols, 1, outputs, 2, types, reinterpret_cast<int64_t>(harness.hashFactory), filter, false, nullptr);
        harness.lookupFactory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
        harness.lookupFactory->SetJoinSpillState(harness.state);
        auto *build = harness.hashFactory->CreateOperator();
        auto *probe = harness.lookupFactory->CreateOperator();
        build->AddInput(MakeLongBatch({1, 2}, {10, 20}));
        probe->AddInput(MakeLongBatch({2, 99}, {200, 990}));
        join_spill_dual_run::TupleMultiset rows;
        DrainProbeRows(build, probe, &rows, harness.hashFactory, OMNI_BUILD_RIGHT);
        Destroy(harness, build, probe);
        delete filter;
        return rows;
    };
    auto resident = make(false);
    auto spilled = make(true);
    EXPECT_GE(resident.size(), 3u);
    ExpectEqual(resident, spilled);
}
TEST(JoinSpillProactiveTest, HeldRestoreFlushWithTwoProbeRuns)
{
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    ExpectEqual(RunHeldRestoreTwoProbeFiles(path, false), RunHeldRestoreTwoProbeFiles(path, true));
}
TEST(JoinSpillProactiveTest, HeldFullTableFlushAfterPartialProbe)
{
    const std::string path = GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    const DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    const int64_t matched = FindLongKeyForSubPartition(1);
    const int64_t unmatched = FindLongKeyForSubPartition(1, matched);
    const int64_t miss = FindLongKeyForSubPartition(2);
    auto resident = RunOnce(path, types, 1, true, false, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT,
        [&]() { return MakeLongBatch({matched, unmatched}, {10, 19}); },
        [&]() { return MakeLongBatch({matched, miss}, {200, 990}); }, nullptr);
    auto harness = MakeHarness(path, types, 1, true, false, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);
    harness.state->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);
    auto *build = dynamic_cast<HashBuilderOperator *>(harness.hashFactory->CreateOperator());
    auto *probe = dynamic_cast<LookupJoinOperator *>(harness.lookupFactory->CreateOperator());
    build->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    build->noMoreInput();
    PumpBuildUntilHandoff(build, harness.state);
    ASSERT_EQ(JoinHandoffPhase::RestoreRound, harness.state->GetHandoffPhase());
    probe->AddInput(MakeLongBatch({matched}, {200}));
    join_spill_dual_run::TupleMultiset actual;
    VectorBatch *probed = nullptr;
    probe->GetOutput(&probed);
    if (probed != nullptr) {
        join_spill_dual_run::AppendCanonicalRowsAndFree(probed, &actual);
    }
    ASSERT_EQ(0u, harness.state->BuildSpilledFiles());
    harness.hashFactory->SetJoinSpillMemThresholdForTest(LLONG_MIN);
    probe->AddInput(MakeLongBatch({miss}, {990}));
    ASSERT_GT(harness.state->BuildSpilledFiles(), 0u);
    ASSERT_EQ(JoinSpillTableInHandKind::None, harness.state->TableInHand().kind);
    DrainProbeRows(build, probe, &actual, harness.hashFactory, OMNI_BUILD_RIGHT);
    ExpectEqual(resident, actual);
    Destroy(harness, build, probe);
}
