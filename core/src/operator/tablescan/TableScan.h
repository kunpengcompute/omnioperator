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

#ifndef __TABLESCAN_OPERATOR_H__
#define __TABLESCAN_OPERATOR_H__

#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "vector/vector_batch.h"
#include "compute/task.h"
#include "reader/common/Filter.h"
#include <unordered_map>

namespace omniruntime {
namespace op {

class TableScanOperatorFactory : public OperatorFactory {
public:
    TableScanOperatorFactory(const std::shared_ptr<const TableScanNode> tableScanNode,
        const uint64_t maxRowCount, std::shared_ptr <SplitsStore> splitsStore);

    ~TableScanOperatorFactory() override;

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

    static TableScanOperatorFactory *
    CreateTableScanOperatorFactory(const std::shared_ptr<const TableScanNode> tableScanNode,
        const config::QueryConfig &queryConfig, std::shared_ptr <SplitsStore> tableScanSplitsStore);

private:
    const std::shared_ptr<const TableScanNode> planNode_;
    uint64_t maxRowCount_;
    std::shared_ptr <SplitsStore> splitsStore_;
};

class TableScanOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    TableScanOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        const std::shared_ptr<const TableScanNode> tableScanNode, const uint64_t maxRowCount,
        std::shared_ptr <SplitsStore> splitsStore);

    ~TableScanOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    bool startDrain() {
        return false;
    }

    bool isFinished();

    /// TableScan can reach isFinished() before noMoreInput (when noMoreSplits_ is set).
    bool isEarlyFinish() const override
    {
        return true;
    }

    bool canAddDynamicFilter() const override
    {
        return true;
    }

    void addDynamicFilter(uint32_t channel, ::common::FilterPtr filter) override;

    /// The name of runtime stats specific to table scan.
    /// The number of running table scan drivers.
    ///
    /// NOTE: we only report the number of running scan drivers at the point that
    /// all the splits have been dispatched.
    static inline const std::string kNumRunningScaleThreads{
        "numRunningScaleThreads"};

private:
    bool getSplit();

    void flushPendingDynamicFilters();

    const std::shared_ptr <connector::ConnectorTableHandle> tableHandle_;
    const std::unordered_map <std::string, std::shared_ptr<connector::ColumnHandle>> columnHandles_;
    const int32_t maxReadBatchSize_;
    const std::shared_ptr <connector::Connector> connector_;
    const size_t getOutputTimeLimitMs_{0};

    bool needNewSplit_ = true;
    std::unique_ptr <connector::DataSource> dataSource_;
    bool noMoreSplits_ = false;

    RowTypePtr outputType_;

    std::shared_ptr <SplitsStore> splitsStore_;

    int32_t readySplitIndex = 0;
    std::unordered_map<uint32_t, ::common::FilterPtr> pendingDynamicFilters_;
};
} // namespace op
} // namespace omniruntime

#endif // __TABLESCAN_OPERATOR_H__