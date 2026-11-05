/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef __LOCAL_PLANNER_H__
#define __LOCAL_PLANNER_H__

#include <vector>
#include <memory>
#include "compute/driver.h"
#include "plannode/planFragment.h"
#include "plannode/planNode.h"
#include "compute/task.h"


namespace omniruntime {
namespace compute {

std::shared_ptr<omniruntime::op::Operator> createOperator(OperatorFactory* factory);

bool IsJoinSpillSupported(const std::shared_ptr<const HashJoinNode> &joinNode);

OperatorFactory* createOperatorFactory(
    const std::shared_ptr<const PlanNode>& planNode,
    const config::QueryConfig& queryConfig);

class LocalPlanner {
public:
    static void buildOperatorStats(std::vector<std::shared_ptr<OmniDriver>>* drivers);
    static void plan(
        const omniruntime::PlanFragment& fragment,
        std::vector<std::shared_ptr<omniruntime::compute::OmniDriver>>* drivers,
        const config::QueryConfig& queryConfig,
        std::shared_ptr<SplitsStore> splitsStore);
};
}  // namespace compute
}  // namespace omniruntime
#endif  // __LOCAL_PLANNER_H__