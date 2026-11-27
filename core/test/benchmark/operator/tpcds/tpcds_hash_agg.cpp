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

#include <nlohmann/json.hpp>
#include "../common/common.h"
#include "operator/aggregation/group_aggregation_expr.h"
#include "type/data_type_serializer.h"
#include "tpcds_common.h"
#include "util/config_util.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace om_benchmark::tpc_ds_data_loader;
namespace om_benchmark {
class TpcDsHashAgg : public TpcDsOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const nlohmann::json &metaJson) override
    {
        ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);

        auto maskChannels = metaJson["maskChannels"].get<std::vector<uint32_t>>();
        auto sourceTypes = LoadDataTypesFromJson(metaJson, "sourceTypes");
        auto isInputRaws = metaJson["isInputRaws"].get<std::vector<bool>>();
        auto isOutputPartials = metaJson["isOutputPartials"].get<std::vector<bool>>();

        auto groupByKeys = GetExprsFromJson(metaJson["groupByChanel"].get<std::vector<std::string>>());

        auto aggsKeys = std::vector<std::vector<Expr *>>();
        for (const auto &item : metaJson["aggChannels"].items()) {
            aggsKeys.emplace_back(GetExprsFromJson(item.value().get<std::vector<std::string>>()));
        }

        auto aggFilters = GetExprsFromJson(metaJson["aggChannelsFilter"].get<std::vector<std::string>>());
        auto aggOutputTypes = std::vector<DataTypes>();
        for (const auto &item : metaJson["aggOutputTypes"].items()) {
            std::vector<DataTypePtr> outputDataTypes;
            for (const auto &innerItem : item.value().items()) {
                outputDataTypes.emplace_back(omniruntime::type::DataTypeJsonParser(innerItem.value()));
            }
            aggOutputTypes.emplace_back(DataTypes(outputDataTypes));
        }

        auto aggFuncTypes = std::vector<uint32_t>();
        for (const auto &item : metaJson["aggFunctionTypes"].items()) {
            aggFuncTypes.emplace_back(item.value()["value"].get<int>());
        }

        auto source = DataTypes(sourceTypes);

        auto factory = new HashAggregationWithExprOperatorFactory(groupByKeys, groupByKeys.size(), aggsKeys, aggFilters,
                                                                  source, aggOutputTypes, aggFuncTypes, maskChannels,
                                                                  isInputRaws, isOutputPartials, new OverflowConfig());

        Expr::DeleteExprs(groupByKeys);
        Expr::DeleteExprs(aggsKeys);

        return factory;
    }

    std::string OperatorIdentifier() override { return "OmniHashAggregationWithExpr.id"; }
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(TpcDsHashAgg);
} // namespace om_benchmark
