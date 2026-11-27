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

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <random>

#include "connectors/Connector.h"
#include "HiveConnectorSplit.h"
#include "HiveConnectorUtil.h"
#include "SplitReader.h"
#include "HivePartitionFunction.h"
#include "TableHandle.h"
#include "HiveConfig.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"
#include "type/data_type.h"
#include "expression/expressions.h"
#include "codegen/ScanSpec.h"
#include "type/Subfield.h"

namespace omniruntime::connector::hive {

class HiveConfig;

class HiveDataSource : public DataSource {
public:
    HiveDataSource(
            const type::RowTypePtr& outputType,
            const std::shared_ptr<connector::ConnectorTableHandle>& tableHandle,
            const std::unordered_map<
                    std::string,
                    std::shared_ptr<connector::ColumnHandle>>& columnHandles,
            const std::shared_ptr<HiveConfig>& hiveConfig,
            omniruntime::mem::OmniTaskMemoryPool *taskPool = nullptr);

    void addSplit(std::shared_ptr<ConnectorSplit> split, uint64_t size) override;

    std::optional<vec::VectorBatch *> next(uint64_t size) override;

    void addDynamicFilter(type::column_index_t channel, ::common::FilterPtr filter) override;

    uint64_t getCompletedRows() const override
    {
        return completedRows_;
    }

    uint64_t getCompletedBytes() const override
    {
        return completedBytes_;
    }

protected:
  virtual std::unique_ptr<SplitReader> createSplitReader();
    const std::shared_ptr<HiveConfig> hiveConfig_;

    std::shared_ptr<HiveConnectorSplit> split_;
    std::shared_ptr<HiveTableHandle> hiveTableHandle_;
    std::shared_ptr<codegen::ScanSpec> scanSpec_;
    vec::VectorBatch* output_;
    std::unique_ptr<SplitReader> splitReader_;
    // Per-task accounting pool, threaded TableScanOperator → Connector →
    // DataSource → SplitReader. Used to charge partition-column /
    // missing-field vector allocations to the per-task pool visible to Spark
    // TMM. nullptr = legacy path (accounting skipped).
    omniruntime::mem::OmniTaskMemoryPool *taskPool_{nullptr};

    // Output type from file reader.  This is different from outputType_ that it
    // contains column names before assignment, and columns that only used in
    // remaining filter.
    type::RowTypePtr readerOutputType_;

    // Column handles for the partition key columns keyed on partition key column
    // name.
    std::unordered_map<std::string, std::shared_ptr<HiveColumnHandle>>
            partitionKeys_;

private:

    // Clear split_ after split has been fully processed.  Keep readers around to
    // hold adaptation.
    void resetSplit();

    const std::optional<vec::VectorBatch *> getEmptyOutput()
    {
        if (!emptyOutput_.has_value()) {
            auto *batch = new vec::VectorBatch(0);
            emptyOutput_ = batch;
        }
        return emptyOutput_;
    }

    // The row type for the data source output, not including filter-only columns
    const type::RowTypePtr outputType_;

    // Column handles for the Split info columns keyed on their column names.
    std::unordered_map <std::string, std::shared_ptr<HiveColumnHandle>>
        infoColumns_;
    SpecialColumnNames specialColumns_{};

    std::unordered_map <std::string, std::vector<const type::Subfield *>> subfields_;

    std::unique_ptr <codegen::ExprSet> remainingFilterExprSet_;
    std::optional<vec::VectorBatch *> emptyOutput_;
    std::atomic <uint64_t> totalRemainingFilterTime_{0};
    uint64_t completedRows_{0};
    uint64_t completedBytes_{0};

    // Field indices referenced in both remaining filter and output type. These
    // columns need to be materialized eagerly to avoid missing values in output.
    std::vector <type::column_index_t> multiReferencedFields_;

    int64_t numBucketConversion_ = 0;
    std::unique_ptr <HivePartitionFunction> partitionFunction_;
    std::vector <uint32_t> partitions_;

    // Reusable memory for remaining filter evaluation.
    vec::VectorPtr filterResult_;

};
} // namespace omniruntime::connector::hive
