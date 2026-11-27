/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <fstream>
#include <nlohmann/json.hpp>
#include "../common/common.h"
#include "../common/vector_util.h"
#include "operator/join/lookup_join_expr.h"
#include "operator/join/hash_builder_expr.h"
#include "tpcds_common.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace om_benchmark::tpc_ds_data_loader;

namespace om_benchmark {
class TpcDsHashJoin : public TpcDsOperatorFixture {
protected:
    HashBuilderWithExprOperatorFactory *hashBuilderOperatorFactory = nullptr;
    omniruntime::op::Operator *hashBuilderOperator = nullptr;

    void SetUpPath(const std::string &dataPath) override
    {
        if (!std::ifstream(dataPath + "/operator_dep_meta.link").good()) {
            hashBuilderOperatorFactory = nullptr;
            return;
        }

        auto dependentDataPath = LoadDependentDataPathFromPath(dataPath);
        auto buildMetaData = LoadBenchmarkMetaFromPath(dependentDataPath);
        auto buildTypes = LoadDataTypesFromJson(buildMetaData, "buildTypes");
        auto hashTableCount = buildMetaData["operatorCount"].get<int32_t>();
        auto buildHashKeys = GetExprsFromJson(buildMetaData["buildHashKeys"].get<std::vector<std::string>>());
        hashBuilderOperatorFactory = new HashBuilderWithExprOperatorFactory(
            JoinType(buildMetaData["joinType"]["value"].get<int>()), DataTypes(buildTypes), buildHashKeys,
            hashTableCount, new OverflowConfig());
        hashBuilderOperator = hashBuilderOperatorFactory->CreateOperator();

        for (const auto &vb : LoadVectorBatchFromPath(dependentDataPath, buildTypes)) {
            hashBuilderOperator->AddInput(vb());
        }

        VectorBatch *hashBuildOutput = nullptr;
        hashBuilderOperator->GetOutput(&hashBuildOutput);
        Expr::DeleteExprs(buildHashKeys);
    }

    OperatorFactory *createOperatorFactory(const nlohmann::json &probeMetaData) override
    {
        if (hashBuilderOperatorFactory == nullptr) {
            return nullptr;
        }

        auto probeTypes = LoadDataTypesFromJson(probeMetaData, "probeTypes");
        auto probeOutputCols = probeMetaData["probeOutputCols"].get<std::vector<int32_t>>();
        auto buildOutputTypes = LoadDataTypesFromJson(probeMetaData, "buildOutputTypes");
        auto buildOutputCols = probeMetaData["buildOutputCols"].get<std::vector<int32_t>>();
        auto probeHashKeys = GetExprsFromJson(probeMetaData["probeHashKeys"].get<std::vector<std::string>>());

        auto factory = new LookupJoinWithExprOperatorFactory(
            DataTypes(probeTypes), probeOutputCols.data(), (int32_t)probeOutputCols.size(), probeHashKeys,
            (int32_t)probeHashKeys.size(), buildOutputCols.data(), (int32_t)buildOutputCols.size(),
            DataTypes(buildOutputTypes), (int64_t)hashBuilderOperatorFactory, nullptr, new OverflowConfig());

        Expr::DeleteExprs(probeHashKeys);
        return factory;
    }

    std::vector<omniruntime::type::DataTypePtr> loadVectorBatchTypes(const nlohmann::json &metaJson) override
    {
        return LoadDataTypesFromJson(metaJson, "probeTypes");
    }

    BaseFixtureGetOutputStrategy GetOutputStrategy() override { return AFTER_EACH_INPUT_FINISHED; }

    std::string OperatorIdentifier() override { return "OmniLookupJoinWithExpr.id"; }

    void TearDown(State &state) override
    {
        BaseOperatorFixture::TearDown(state);
        if (hashBuilderOperator != nullptr) {
            omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
        }
        delete hashBuilderOperatorFactory;
    }
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(TpcDsHashJoin);
} // namespace om_benchmark
