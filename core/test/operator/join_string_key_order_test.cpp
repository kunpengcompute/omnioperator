/* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * String-key hash join builds must find every key whatever order the build rows arrive in.
 */
#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "operator/join/hash_builder.h"
#include "operator/join/lookup_join.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace TestUtil;

namespace {
// xc_case q3_2 lost one or two string build keys of this 24-key shuffle partition in some runs.
// Shuffle fetch order varies, so build the same keys in many orders and batch splits and require
// every key to match.
TEST(JoinStringKeyOrderTest, LeftJoinStringKeysMatchInEveryBuildOrder)
{
    const std::vector<std::string> keys = {"room_87981", "room_21981", "room_12891", "room_13221", "room_13701",
        "room_15091", "room_97361", "room_105100", "room_111631", "room_48491", "room_52391", "room_52541",
        "room_24600", "room_124371", "room_125351", "room_127100", "room_127861", "room_62201", "room_62701",
        "room_6821", "room_74571", "room_89631", "room_90061", "room_93161"};
    DataTypes buildTypes(std::vector<DataTypePtr>({ VarcharType(50) }));
    DataTypes probeTypes(std::vector<DataTypePtr>({ VarcharType(50) }));
    int32_t buildJoinCols[1] = {0};
    int32_t probeOutputCols[1] = {0};
    int32_t probeHashCols[1] = {0};
    int32_t buildOutputCols[1] = {0};
    std::mt19937 rng(20261031);
    int failures = 0;
    for (int iter = 0; iter < 3000 && failures < 5; ++iter) {
        std::vector<std::string> order = keys;
        std::shuffle(order.begin(), order.end(), rng);
        auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, 1, 1);
        auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(nullptr));
        // Split the build rows into up to 16 batches, as a shuffle read of up to 16 map outputs would.
        size_t pos = 0;
        const int batches = 1 + static_cast<int>(rng() % 16);
        for (int b = 0; b < batches && pos < order.size(); ++b) {
            size_t take = (b == batches - 1) ? order.size() - pos : 1 + rng() % 3;
            take = std::min(take, order.size() - pos);
            std::vector<std::string> part(order.begin() + pos, order.begin() + pos + take);
            pos += take;
            hashBuilderOperator->AddInput(CreateVectorBatch(buildTypes, static_cast<int32_t>(part.size()), part.data()));
        }
        VectorBatch *hashBuildOutput = nullptr;
        hashBuilderOperator->GetOutput(&hashBuildOutput);

        auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
            1, probeHashCols, 1, buildOutputCols, 1, buildTypes, (int64_t)hashBuilderFactory, nullptr, false, nullptr);
        auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator(nullptr));
        std::vector<std::string> probe = keys;
        probe.push_back("room_1");
        probe.push_back("room_999999");
        std::shuffle(probe.begin(), probe.end(), rng);
        lookupJoinOperator->AddInput(CreateVectorBatch(probeTypes, static_cast<int32_t>(probe.size()), probe.data()));
        std::set<std::string> matched;
        for (int guard = 0; guard < 16; ++guard) {
            VectorBatch *out = nullptr;
            lookupJoinOperator->GetOutput(&out);
            if (out == nullptr) {
                break;
            }
            auto *probeCol = static_cast<Vector<LargeStringContainer<std::string_view>> *>(out->Get(0));
            auto *buildCol = static_cast<Vector<LargeStringContainer<std::string_view>> *>(out->Get(1));
            for (int32_t r = 0; r < out->GetRowCount(); ++r) {
                if (!buildCol->IsNull(r)) {
                    EXPECT_EQ(std::string(probeCol->GetValue(r)), std::string(buildCol->GetValue(r)));
                    matched.insert(std::string(probeCol->GetValue(r)));
                }
            }
            VectorHelper::FreeVecBatch(out);
        }
        for (const auto &k : keys) {
            if (matched.count(k) == 0) {
                ++failures;
                std::string seq;
                for (const auto &o : order) {
                    seq += o + " ";
                }
                ADD_FAILURE() << "iter " << iter << " batches " << batches << ": build key " << k
                              << " did not match; build order: " << seq;
            }
        }
        omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
        omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
        delete hashBuilderFactory;
        delete lookupJoinFactory;
    }
    EXPECT_EQ(failures, 0);
}
} // namespace
