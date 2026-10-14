/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Compact driver and Limit early-finish coverage for the upstream branch.
 */
#include <climits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include "compute/local_planner.h"
#include "compute/reason.h"
#include "gtest/gtest.h"
#include "join_spill_dual_run_test_util.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "operator/util/operator_util.h"
#include "plannode/RowVectorStream.h"
#include "plannode/planFragment.h"
#include "plannode/planNode.h"
#include "type/data_type.h"
#include "util/test_util.h"
#include "vector/vector_helper.h"
using namespace omniruntime;
using namespace omniruntime::compute;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace TestUtil;
using join_spill_dual_run::DrainProbeRows;
using join_spill_dual_run::FindLongKeyForSubPartition;
using join_spill_dual_run::JoinSpillTestSubPartitionConfig;
using join_spill_dual_run::MakeJoinSpillTestSparkConfig;
using join_spill_dual_run::MakeLongKeyValueBatch;
namespace {
constexpr const char *kSpillRoot = "/tmp/join-spill-upstream-driver";
std::shared_ptr<JoinSpillState> MakeState()
{
    const DataTypes types({LongType(), LongType()});
    auto state = std::make_shared<JoinSpillState>(kSpillRoot, types, types, 4);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    return state;
}
HashBuilderOperatorFactory *MakeHashFactory(const std::shared_ptr<JoinSpillState> &state, bool force,
    JoinType joinType = OMNI_JOIN_TYPE_INNER, BuildSide buildSide = OMNI_BUILD_UNKNOWN)
{
    const DataTypes types({LongType(), LongType()});
    int32_t hashCols[] = {0};
    auto *factory = buildSide == OMNI_BUILD_UNKNOWN
        ? HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, types, hashCols, 1, 1)
        : HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide, types, hashCols, 1, 1);
    factory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
    factory->SetJoinSpillState(state);
    factory->SetSpillConfig(MakeJoinSpillTestSparkConfig(kSpillRoot));
    factory->SetJoinSpillMemThresholdForTest(force ? 0 : LLONG_MAX);
    return factory;
}
LookupJoinOperatorFactory *MakeLookupFactory(HashBuilderOperatorFactory *hashFactory,
    const std::shared_ptr<JoinSpillState> &state)
{
    const DataTypes types({LongType(), LongType()});
    int32_t outputs[] = {0, 1};
    int32_t hashCols[] = {0};
    auto *factory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, outputs, 2, hashCols, 1,
        outputs, 2, types, reinterpret_cast<int64_t>(hashFactory), nullptr, false, nullptr);
    factory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
    factory->SetJoinSpillState(state);
    return factory;
}
uint32_t HashTableSize(HashBuilderOperatorFactory *factory)
{
    auto *tables = factory->GetHashTablesVariants();
    return tables == nullptr ? 0 : std::visit([](auto &&arg) -> uint32_t { return arg.GetHashTableSize(); }, *tables);
}
void GetOutput(op::Operator *op)
{
    VectorBatch *out = nullptr;
    op->GetOutput(&out);
    if (out != nullptr) VectorHelper::FreeVecBatch(out);
}
void PumpLimitDrivers(JoinType joinType, int32_t *outputRows, bool *finished)
{
    DataTypes types(std::vector<DataTypePtr>({LongType(), LongType()}));
    const DataTypesPtr typePtr = types.Instance();
    std::vector<VectorBatch *> probeBatches;
    std::vector<VectorBatch *> buildBatches;
    int32_t limitOffset = 0;
    if (joinType == OMNI_JOIN_TYPE_FULL) {
        constexpr int32_t kMatched = 2;
        const int32_t unmatched = OperatorUtil::GetMaxRowCount(OperatorUtil::GetRowSize(types.Get()) * 2) + 1;
        limitOffset = kMatched * kMatched;
        probeBatches.push_back(CreateVectorBatch(types, kMatched, std::vector<int64_t>(kMatched, 1).data(),
            std::vector<int64_t>{10, 11}.data()));
        std::vector<int64_t> buildKeys(kMatched, 1);
        std::vector<int64_t> buildValues{100, 101};
        buildKeys.insert(buildKeys.end(), static_cast<size_t>(unmatched), 9);
        for (int32_t i = 0; i < unmatched; ++i) {
            buildValues.push_back(900 + i);
        }
        buildBatches.push_back(CreateVectorBatch(types, static_cast<int32_t>(buildKeys.size()), buildKeys.data(),
            buildValues.data()));
    } else {
        probeBatches.push_back(MakeLongKeyValueBatch({1, 1}, {10, 11}));
        probeBatches.push_back(MakeLongKeyValueBatch({2, 2, 3, 3}, {20, 21, 30, 31}));
        buildBatches.push_back(MakeLongKeyValueBatch({1, 1, 2, 2, 3, 3}, {100, 101, 200, 201, 300, 301}));
    }
    class It : public ColumnarBatchIterator {
        std::vector<VectorBatch *> b_;
        size_t i_ = 0;
    public:
        explicit It(std::vector<VectorBatch *> b) : b_(std::move(b)) {}
        ~It() override
        {
            while (i_ < b_.size()) {
                VectorHelper::FreeVecBatch(b_[i_++]);
            }
        }
        VectorBatch *Next() override
        {
            return i_ < b_.size() ? b_[i_++] : nullptr;
        }
    };
    auto stream = [&](const std::string &id, std::vector<VectorBatch *> batches) {
        return std::make_shared<const ValueStreamNode>(id, typePtr,
            std::make_shared<ResultIterator>(std::make_unique<It>(std::move(batches))));
    };
    std::vector<ExprPtr> leftKeys{new FieldExpr(0, LongType())};
    std::vector<ExprPtr> rightKeys{new FieldExpr(0, LongType())};
    std::vector<Expr *> partitionKeys;
    for (int32_t col = 0; col < types.GetSize() * 2; ++col) {
        partitionKeys.push_back(new FieldExpr(col, LongType()));
    }
    auto join = std::make_shared<const HashJoinNode>("join", joinType, OMNI_BUILD_RIGHT, false, true, leftKeys,
        rightKeys, nullptr, stream("probe", std::move(probeBatches)), stream("build", std::move(buildBatches)),
        typePtr, typePtr, partitionKeys);
    PlanFragment fragment{std::make_shared<const LimitNode>("limit", limitOffset, 1, false, join),
        ExecutionStrategy::K_UNGROUPED, 1, std::unordered_set<PlanNodeId>{}};
    static int seq = 0;
    config::QueryConfig queryConfig({
        {config::QueryConfig::kSpillEnabled, "true"},
        {config::QueryConfig::kJoinSpillEnabled, "true"},
        {config::QueryConfig::KSpillDir, std::string("/tmp/join-spill-upstream-limit-") + std::to_string(++seq)},
    });
    std::vector<std::shared_ptr<OmniDriver>> drivers;
    LocalPlanner::plan(fragment, &drivers, queryConfig, nullptr);
    ASSERT_GE(drivers.size(), 2u);
    int32_t quiet = 0;
    *outputRows = 0;
    *finished = false;
    while (quiet < 1000) {
        int32_t runnable = 0;
        bool produced = false;
        for (int pass = 0; pass < 2; ++pass) {
            for (auto &driver : drivers) {
                if (pass == 0 ? driver->outputDriver : !driver->outputDriver) {
                    continue;
                }
                if (driver->isFinished()) {
                    continue;
                }
                ++runnable;
                auto future = OmniFuture::makeEmpty();
                StopReason reason = StopReason::kNone;
                VectorBatch *batch = driver->Next(&future, &reason);
                if (batch == nullptr) {
                    continue;
                }
                if (driver->outputDriver) {
                    *outputRows += batch->GetRowCount();
                    produced = true;
                }
                VectorHelper::FreeVecBatch(batch);
            }
        }
        if (runnable == 0) {
            *finished = true;
            break;
        }
        quiet = produced ? 0 : quiet + 1;
    }
    for (auto &driver : drivers) {
        driver->close();
    }
}
join_spill_dual_run::TupleMultiset MultiSpRows(JoinType joinType, BuildSide side, bool force)
{
    const int64_t key0 = FindLongKeyForSubPartition(0);
    const int64_t key1 = FindLongKeyForSubPartition(1);
    const int64_t key2 = FindLongKeyForSubPartition(2);
    const int32_t matches = OperatorUtil::GetMaxRowCount(2 * 8 * static_cast<int32_t>(sizeof(int64_t)));
    std::vector<int64_t> buildKeys;
    for (int64_t key : {key0, key1, key2}) {
        buildKeys.insert(buildKeys.end(), static_cast<size_t>(matches), key);
    }
    const std::vector<int64_t> probeKeys{key2, key0, key1};
    std::vector<DataTypePtr> wide(8, LongType());
    const DataTypes types(wide);
    int32_t hashCols[] = {0};
    int32_t outputs[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    auto state = std::make_shared<JoinSpillState>("/tmp/join-spill-upstream-wide", types, types, 4);
    auto *hashFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, side, types, hashCols,
        1, 1);
    hashFactory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
    hashFactory->SetJoinSpillState(state);
    hashFactory->SetSpillConfig(MakeJoinSpillTestSparkConfig("/tmp/join-spill-upstream-wide"));
    hashFactory->SetJoinSpillMemThresholdForTest(force ? 0 : LLONG_MAX);
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, outputs, 8, hashCols, 1,
        outputs, 8, types, reinterpret_cast<int64_t>(hashFactory), nullptr, false, nullptr);
    lookupFactory->SetJoinSpillPolicy(true, JoinSpillTestSubPartitionConfig());
    lookupFactory->SetJoinSpillState(state);
    auto *build = hashFactory->CreateOperator();
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
    auto wideBatch = [&](const std::vector<int64_t> &keys) {
        auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
        for (int32_t col = 0; col < 8; ++col) {
            auto *vector = new Vector<int64_t>(static_cast<int32_t>(keys.size()));
            for (size_t row = 0; row < keys.size(); ++row) {
                vector->SetValue(static_cast<int32_t>(row), col == 0 ? keys[row] : keys[row] * 100 + col);
            }
            batch->Append(vector);
        }
        return batch;
    };
    build->AddInput(wideBatch(buildKeys));
    probe->AddInput(wideBatch(probeKeys));
    join_spill_dual_run::TupleMultiset rows;
    DrainProbeRows(build, probe, &rows, hashFactory, side);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
    EXPECT_EQ(rows.size(), static_cast<size_t>(matches) * probeKeys.size());
    join_spill_dual_run::SortTupleMultiset(&rows);
    return rows;
}
} // namespace
TEST(JoinSpillDriverTest, RePumpDrivesAscendingRestoreToCompletion)
{
    auto state = MakeState();
    auto *hashFactory = MakeHashFactory(state, true);
    const int64_t key1 = FindLongKeyForSubPartition(1);
    const int64_t key2 = FindLongKeyForSubPartition(2);
    const int64_t key3 = FindLongKeyForSubPartition(2, key2);
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator());
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
    build->setNoMoreInput(false);
    build->AddInput(MakeLongKeyValueBatch({key1, key2, key3}, {10, 11, 12}));
    build->setNoMoreInput(true);
    int32_t rows = 0;
    bool fed = false;
    for (int32_t iter = 0; iter < 100000; ++iter) {
        if (!build->isFinished()) {
            ContinueFuture future;
            if (build->IsBlocked(&future) == BlockingReason::kNotBlocked) {
                GetOutput(build);
            }
        }
        if (!fed && state->GetHandoffPhase() == JoinHandoffPhase::ProbePathA) {
            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongKeyValueBatch({key1, key1, key2}, {201, 202, 203}));
            probe->setNoMoreInput(true);
            fed = true;
        }
        if (fed) {
            ContinueFuture future;
            if (probe->IsBlocked(&future) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    rows += out->GetRowCount();
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }
        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() && probe->isFinished()) {
            break;
        }
        ASSERT_LT(iter, 99999);
    }
    EXPECT_EQ(3, rows);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete hashFactory;
}
TEST(JoinSpillDriverTest, RuntimeLayoutChangesAfterForcedSpill)
{
    auto resident = MakeState();
    auto *residentFactory = MakeHashFactory(resident, false);
    auto *residentBuild = dynamic_cast<HashBuilderOperator *>(residentFactory->CreateOperator());
    residentBuild->AddInput(MakeLongKeyValueBatch({FindLongKeyForSubPartition(0)}, {10}));
    residentBuild->noMoreInput();
    GetOutput(residentBuild);
    EXPECT_EQ(JoinBuildRuntimeLayout::Single, resident->GetRuntimeLayout());
    omniruntime::op::Operator::DeleteOperator(residentBuild);
    delete residentFactory;
    auto spilled = MakeState();
    auto *spilledFactory = MakeHashFactory(spilled, true);
    auto *spilledBuild = dynamic_cast<HashBuilderOperator *>(spilledFactory->CreateOperator());
    spilledBuild->AddInput(MakeLongKeyValueBatch({1, 2, 3, 4}, {10, 11, 12, 13}));
    spilledBuild->noMoreInput();
    for (int32_t i = 0; i < 16 && spilled->GetHandoffPhase() == JoinHandoffPhase::Idle; ++i) {
        GetOutput(spilledBuild);
    }
    EXPECT_EQ(JoinHandoffPhase::ProbePathA, spilled->GetHandoffPhase());
    EXPECT_EQ(JoinBuildRuntimeLayout::Partitioned, spilled->GetRuntimeLayout());
    omniruntime::op::Operator::DeleteOperator(spilledBuild);
    delete spilledFactory;
}
TEST(JoinSpillDriverTest, NoSessionBuildStateRelease)
{
    const DataTypes types({LongType(), LongType()});
    int32_t hashCols[] = {0};
    int32_t outputs[] = {0, 1};
    for (JoinType joinType : {OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_FULL}) {
        auto *hashFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            joinType, OMNI_BUILD_RIGHT, types, hashCols, 1, 1);
        auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator());
        build->AddInput(MakeLongKeyValueBatch({1, 2}, {10, 19}));
        build->noMoreInput();
        GetOutput(build);
        auto *probe = dynamic_cast<LookupJoinOperator *>(LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
            types, outputs, 2, hashCols, 1, outputs, 2, types, reinterpret_cast<int64_t>(hashFactory), nullptr, false,
            nullptr)->CreateOperator());
        probe->AddInput(MakeLongKeyValueBatch({1}, {201}));
        probe->noMoreInput();
        for (int32_t i = 0; i < 100000 && !probe->isFinished(); ++i) GetOutput(probe);
        if (joinType == OMNI_JOIN_TYPE_FULL) {
            auto *outer = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(types, outputs, 2,
                outputs, types, reinterpret_cast<int64_t>(hashFactory), OMNI_BUILD_RIGHT)->CreateOperator();
            for (int32_t i = 0; i < 100000 && !outer->isFinished(); ++i) GetOutput(outer);
            probe->FinishFullTableProbeAfterOuter();
            omniruntime::op::Operator::DeleteOperator(outer);
        }
        EXPECT_EQ(0u, HashTableSize(hashFactory));
        omniruntime::op::Operator::DeleteOperator(probe);
        omniruntime::op::Operator::DeleteOperator(build);
        delete hashFactory;
    }
}
TEST(JoinSpillDriverTest, BuildPreservingOuterDefersCompletion)
{
    auto state = MakeState();
    auto *hashFactory = MakeHashFactory(state, false, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator());
    build->AddInput(MakeLongKeyValueBatch({1, 2}, {10, 19}));
    build->noMoreInput();
    GetOutput(build);
    auto *probe = dynamic_cast<LookupJoinOperator *>(MakeLookupFactory(hashFactory, state)->CreateOperator());
    probe->AddInput(MakeLongKeyValueBatch({1}, {201}));
    probe->noMoreInput();
    for (int32_t i = 0; i < 1000 && !probe->isFinished(); ++i) GetOutput(probe);
    EXPECT_NE(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_GT(HashTableSize(hashFactory), 0u);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete hashFactory;
}
TEST(JoinSpillDriverTest, MultiSpChunkPreservesColumnPermutation)
{
    EXPECT_EQ(MultiSpRows(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, false),
        MultiSpRows(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, true));
    EXPECT_EQ(MultiSpRows(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, false),
        MultiSpRows(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, true));
}
class LimitEarlyCloseTest : public testing::TestWithParam<JoinType> {};
TEST_P(LimitEarlyCloseTest, AbandonsSpilledProbe)
{
    int32_t rows = 0;
    bool finished = false;
    PumpLimitDrivers(GetParam(), &rows, &finished);
    EXPECT_EQ(rows, 1);
    EXPECT_TRUE(finished);
}
INSTANTIATE_TEST_SUITE_P(JoinSpillDriverTest, LimitEarlyCloseTest,
    testing::Values(OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_FULL));

// Build driver close deletes the hash-table object. Probe IsBlocked must not read it
// once the handoff is Complete. Status is set back to NORMAL first: a read would wait
// for build, so kNotBlocked means the table was not touched.
TEST(JoinSpillDriverTest, ProbeDoesNotTouchTableAfterBuildFactoryDeleted)
{
    auto state = MakeState();
    auto *hashFactory = MakeHashFactory(state, false);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator());
    build->AddInput(MakeLongKeyValueBatch({1, 2}, {10, 19}));
    build->noMoreInput();
    GetOutput(build);
    ASSERT_NE(JoinHandoffPhase::Idle, state->GetHandoffPhase());

    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
    probe->AddInput(MakeLongKeyValueBatch({1}, {201}));
    probe->noMoreInput();
    for (int32_t i = 0; i < 100000 && !probe->isFinished(); ++i) {
        GetOutput(probe);
    }
    ASSERT_TRUE(probe->isFinished());
    ASSERT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());

    auto *tables = hashFactory->GetHashTablesVariants();
    ASSERT_NE(tables, nullptr);
    std::visit([](auto &&arg) { arg.SetStatus(OMNI_STATUS_NORMAL); }, *tables);

    omniruntime::op::Operator::DeleteOperator(build);
    delete hashFactory;

    auto future = OmniFuture::makeEmpty();
    EXPECT_EQ(BlockingReason::kNotBlocked, probe->IsBlocked(&future));

    omniruntime::op::Operator::DeleteOperator(probe);
    delete lookupFactory;
}
