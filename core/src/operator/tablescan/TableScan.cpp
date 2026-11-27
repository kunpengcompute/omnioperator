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

#include <cstdint>
#include <chrono>
#include "TableScan.h"
#include "vector/vector_helper.h"
#include "vector/vector_batch.h"
#include "connectors/Split.h"
#include "plannode/planNode.h"
#include "compute/task.h"
#include "codegen/time_util.h"
#include "type/data_type.h"
#include "util/debug.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace omniruntime::connector;
using namespace std;

TableScanOperatorFactory::TableScanOperatorFactory(const std::shared_ptr<const TableScanNode> planNode,
    const uint64_t maxRowCount, std::shared_ptr <SplitsStore> splitsStore)
    : planNode_(planNode),
    maxRowCount_(maxRowCount),
    splitsStore_(splitsStore) {}

TableScanOperatorFactory::~TableScanOperatorFactory() = default;

TableScanOperatorFactory *TableScanOperatorFactory::CreateTableScanOperatorFactory(
    const std::shared_ptr<const TableScanNode> planNode,
    const config::QueryConfig &queryConfig,
    std::shared_ptr <SplitsStore> tableScanSplitsStore)
{
    uint64_t maxRowCount = queryConfig.MaxBatchSize();
    auto pOperatorFactory = new TableScanOperatorFactory(planNode, maxRowCount, tableScanSplitsStore);
    return pOperatorFactory;
}

// Pool-bound override
Operator *TableScanOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    auto pTableScanOperator = new TableScanOperator(pool, planNode_, maxRowCount_, splitsStore_);
    return pTableScanOperator;
}

// Pool-bound ctor chains Operator(pool).
TableScanOperator::TableScanOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
    const std::shared_ptr<const TableScanNode> tableScanNode, const uint64_t maxRowCount,
    std::shared_ptr <SplitsStore> splitsStore)
    : Operator(pool),
    tableHandle_(tableScanNode->tableHandle()),
    columnHandles_(tableScanNode->assignments()),
    outputType_(tableScanNode->getRowTypePtr()),
    splitsStore_(splitsStore),
    maxReadBatchSize_(maxRowCount),
    connector_(connector::getConnector(tableHandle_->connectorId())) {}

TableScanOperator::~TableScanOperator() {}

int32_t TableScanOperator::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    return 0;
}

void TableScanOperator::addDynamicFilter(uint32_t channel, ::common::FilterPtr filter)
{
    if (filter == nullptr) {
        return;
    }
    auto it = pendingDynamicFilters_.find(channel);
    if (it != pendingDynamicFilters_.end() && it->second != nullptr) {
        const int oldKind = static_cast<int>(it->second->kind());
        auto merged = it->second->mergeWith(filter.get());
        if (merged == nullptr) {
            LogDebug("DFP: TableScan pending mergeWith returned null channel=%u existingKind=%d incomingKind=%d; "
                    "keeping incoming filter (existing predicate dropped; Conjunction not implemented)",
                static_cast<unsigned>(channel), oldKind, static_cast<int>(filter->kind()));
            pendingDynamicFilters_[channel] = std::move(filter);
        } else {
            pendingDynamicFilters_[channel] = std::move(merged);
        }
    } else {
        pendingDynamicFilters_[channel] = std::move(filter);
    }
    LogDebug("DFP: TableScan pending filter channel=%u dataSource=%s", static_cast<unsigned>(channel),
        dataSource_ != nullptr ? "ready" : "null");
    if (dataSource_ != nullptr) {
        dataSource_->addDynamicFilter(channel, pendingDynamicFilters_[channel]);
        pendingDynamicFilters_.erase(channel);
    }
}

void TableScanOperator::flushPendingDynamicFilters()
{
    if (dataSource_ == nullptr || pendingDynamicFilters_.empty()) {
        return;
    }
    LogDebug("DFP: TableScan flushing %zu pending filter(s) onto DataSource", pendingDynamicFilters_.size());
    for (auto &[channel, filter] : pendingDynamicFilters_) {
        dataSource_->addDynamicFilter(channel, filter);
    }
    pendingDynamicFilters_.clear();
}

int32_t TableScanOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (noMoreSplits_) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    } else {
        SetStatus(OMNI_STATUS_NORMAL);
    }

    for (;;) {
        if (noMoreSplits_) {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        } else {
            SetStatus(OMNI_STATUS_NORMAL);
        }

        if (needNewSplit_) {
            const auto hasNewSplit = getSplit();
            if (!hasNewSplit) {
                continue;
            }
        }

        std::optional < VectorBatch * > dataOptional;
        uint64_t nextWallNanos = 0;
        {
            const auto t0 = std::chrono::steady_clock::now();
            dataOptional = dataSource_->next(maxReadBatchSize_);
            const auto t1 = std::chrono::steady_clock::now();
            nextWallNanos = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        }
        if (dataSource_ != nullptr) {
            stats().totalScanTimeNanos += nextWallNanos;
        }
        if (!dataOptional.has_value()) {
            return 0;
        }

        VectorBatch *data = std::move(dataOptional).value();
        if (data != nullptr) {
            if (data->GetRowCount() == 0) {
                needNewSplit_ = true;
                continue;
            }
            *outputVecBatch = data;
            stats().AddInputVector(
                data->EstimateFlatSize(), 1, static_cast<uint64_t>(data->GetRowCount()));
            return 0;
        }
        needNewSplit_ = true;
    }
}

bool TableScanOperator::getSplit()
{
    if (splitsStore_ == nullptr) {
        noMoreSplits_ = true;
        return false;
    }

    if (splitsStore_->splits.empty()) {
        if (splitsStore_->noMoreSplits) {
            noMoreSplits_ = true;
        }
        return false;
    }

    Split split;

    split = std::move(splitsStore_->splits.front());
    splitsStore_->splits.pop_front();

    if (splitsStore_->splits.empty()) {
        splitsStore_->noMoreSplits = true;
    }

    if (!split.hasConnectorSplit()) {
        noMoreSplits_ = true;
        return false;
    }

    const auto &connectorSplit = split.connectorSplit;
    needNewSplit_ = false;

    if (dataSource_ == nullptr) {
        // Thread the per-task pool from Operator::pool() into the
        // DataSource → SplitReader chain so that partition-column and
        // missing-field allocations are visible to Spark TMM via the
        // per-task pool's listener.
        dataSource_ = connector_->createDataSource(outputType_, tableHandle_, columnHandles_, pool());
    }
    flushPendingDynamicFilters();

    dataSource_->addSplit(connectorSplit, maxReadBatchSize_);

    return true;
}

bool TableScanOperator::isFinished()
{
    return noMoreSplits_;
}

OmniStatus TableScanOperator::Close()
{
    if (dataSource_ != nullptr) {
        dataSource_->cancel();
    }

    return OMNI_STATUS_NORMAL;
}

} // namespace op
} // namespace omniruntime
