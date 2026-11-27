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
#include "operator/join/sortmergejoin/sort_merge_join_expr.h"
#include "tpcds_common.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
using namespace tpc_ds_data_loader;
class TpcDsSortMergeJoin : public TpcDsOperatorFixture {
protected:
    StreamedTableWithExprOperatorFactory *streamedTableWithExprOperatorFactory = nullptr;
    BufferedTableWithExprOperatorFactory *bufferedTableWithExprOperatorFactory = nullptr;
    omniruntime::op::Operator *bufferOperator = nullptr;

    void SetUpPath(const std::string &dataPath) override
    {
        if (!std::ifstream(dataPath + "/operator_dep_meta.link").good()) {
            streamedTableWithExprOperatorFactory = nullptr;
            bufferedTableWithExprOperatorFactory = nullptr;
            return;
        }

        auto dependentDataPath = LoadDependentDataPathFromPath(dataPath);
        auto streamMeta = LoadBenchmarkMetaFromPath(dependentDataPath);
        auto streamedTypes = loadVectorBatchTypes(streamMeta);
        auto equalKeyExprs = GetExprsFromJson(streamMeta["equalKeyExprs"].get<std::vector<std::string>>());
        auto outputChannels = streamMeta["outputChannels"].get<std::vector<int32_t>>();
        auto filter = std::string();

        if (!streamMeta["filter"]["value"].is_null()) {
            filter = streamMeta["filter"]["value"].get<std::string>();
        }

        streamedTableWithExprOperatorFactory = new StreamedTableWithExprOperatorFactory(
            DataTypes(streamedTypes), equalKeyExprs, (int32_t)equalKeyExprs.size(), outputChannels.data(),
            (int32_t)outputChannels.size(), JoinType(streamMeta["joinType"]["value"].get<int>()), filter,
            new OverflowConfig());

        auto bufferMeta = LoadBenchmarkMetaFromPath(dataPath);
        auto bufferTypes = LoadDataTypesFromJson(bufferMeta, "soruceTypes");
        auto bufferOutputChannels = bufferMeta["outputChannels"].get<std::vector<int32_t>>();
        auto bufferEqualKeyExprs = GetExprsFromJson(bufferMeta["equalKeyExprs"].get<std::vector<std::string>>());

        bufferedTableWithExprOperatorFactory = new BufferedTableWithExprOperatorFactory(
            DataTypes(bufferTypes), bufferEqualKeyExprs, (int32_t)bufferEqualKeyExprs.size(),
            bufferOutputChannels.data(), (int32_t)bufferOutputChannels.size(),
            (int64_t)streamedTableWithExprOperatorFactory, new OverflowConfig());
        bufferOperator = bufferedTableWithExprOperatorFactory->CreateOperator();
        for (const auto &vb : LoadVectorBatchFromPath(dataPath, bufferTypes)) {
            bufferOperator->AddInput(vb());
        }
    }

    std::string GetRealDataPath(const std::string &dataPath) override
    {
        return LoadDependentDataPathFromPath(dataPath);
    }

    OperatorFactory *createOperatorFactory(const nlohmann::json &metaJson) override
    {
        return streamedTableWithExprOperatorFactory;
    }

    std::vector<omniruntime::type::DataTypePtr> loadVectorBatchTypes(const nlohmann::json &metaJson) override
    {
        return LoadDataTypesFromJson(metaJson, "sourceTypes");
    }

    BaseFixtureGetOutputStrategy GetOutputStrategy() override { return AFTER_EACH_INPUT_FINISHED; }

    std::string OperatorIdentifier() override { return "OmniSmjBufferedTableWithExpr.id"; }

    void TearDown(State &state) override
    {
        BaseOperatorFixture::TearDown(state);
        if (bufferOperator != nullptr) {
            omniruntime::op::Operator::DeleteOperator(bufferOperator);
        }
        delete bufferedTableWithExprOperatorFactory;

        bufferedTableWithExprOperatorFactory = nullptr;
        streamedTableWithExprOperatorFactory = nullptr;
    }
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(TpcDsSortMergeJoin);
} // namespace om_benchmark
