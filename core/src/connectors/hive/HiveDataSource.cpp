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

#include "HiveDataSource.h"

#include <string>
#include <unordered_map>
#include "util/debug.h"

namespace omniruntime::connector::hive {

class HiveTableHandle;
class HiveColumnHandle;

HiveDataSource::HiveDataSource(
    const type::RowTypePtr &outputType,
    const std::shared_ptr <connector::ConnectorTableHandle> &tableHandle,
    const std::unordered_map <std::string, std::shared_ptr<connector::ColumnHandle>> &columnHandles,
    const std::shared_ptr <HiveConfig> &hiveConfig,
    omniruntime::mem::OmniTaskMemoryPool *taskPool)
    : hiveConfig_(hiveConfig), taskPool_(taskPool), outputType_(outputType) {
    // Column handled keyed on the column alias, the name used in the query.
    for (const auto &[canonicalizedName, columnHandle]: columnHandles) {
        auto handle = std::dynamic_pointer_cast<HiveColumnHandle>(columnHandle);
        switch (handle->columnType()) {
            case HiveColumnHandle::ColumnType::kRegular:
                break;
            case HiveColumnHandle::ColumnType::kPartitionKey:
                partitionKeys_.emplace(handle->name(), handle);
                break;
            case HiveColumnHandle::ColumnType::kSynthesized:
                infoColumns_.emplace(handle->name(), handle);
                break;
            case HiveColumnHandle::ColumnType::kRowIndex:
                specialColumns_.rowIndex = handle->name();
                break;
            case HiveColumnHandle::ColumnType::kRowId:
                specialColumns_.rowId = handle->name();
                break;
        }
    }

    std::vector <std::string> readColumnNames;
    auto readColumnTypes = outputType_->children_();
    for (const auto &outputName: outputType_->names()) {
        auto it = columnHandles.find(outputName);

        auto *handle = static_cast<const HiveColumnHandle *>(it->second.get());
        readColumnNames.push_back(handle->name());
        for (auto &subfield: handle->requiredSubfields()) {
            subfields_[handle->name()].push_back(&subfield);
        }
    }

    hiveTableHandle_ = std::dynamic_pointer_cast<HiveTableHandle>(tableHandle);
    readerOutputType_ =
        ROW(std::move(readColumnNames), std::move(readColumnTypes));
    scanSpec_ = makeScanSpec(readerOutputType_, subfields_, hiveTableHandle_->dataColumns(),
                             partitionKeys_, infoColumns_, specialColumns_);
}

void HiveDataSource::addDynamicFilter(type::column_index_t channel, ::common::FilterPtr filter)
{
    if (scanSpec_ == nullptr || filter == nullptr) {
        return;
    }
    auto *child = scanSpec_->getChildByChannel(channel);
    if (child == nullptr) {
        LogDebug("DFP: HiveDataSource has no ScanSpec child for channel %u", static_cast<unsigned>(channel));
        return;
    }
    if (child->filter() != nullptr) {
        const int oldKind = static_cast<int>(child->filter()->kind());
        auto merged = child->filter()->mergeWith(filter.get());
        if (merged == nullptr) {
            LogDebug("DFP: mergeWith returned null channel=%u name=%s existingKind=%d incomingKind=%d; "
                    "keeping incoming filter (existing predicate dropped; Conjunction not implemented)",
                static_cast<unsigned>(channel), child->fieldName().c_str(), oldKind,
                static_cast<int>(filter->kind()));
            child->setFilter(std::move(filter));
        } else {
            child->setFilter(std::move(merged));
        }
    } else {
        child->setFilter(std::move(filter));
    }
    LogDebug("DFP: HiveDataSource applied filter channel=%u kind=%d name=%s",
        static_cast<unsigned>(channel),
        child->filter() != nullptr ? static_cast<int>(child->filter()->kind()) : -1,
        child->fieldName().c_str());
}

std::unique_ptr<SplitReader> HiveDataSource::createSplitReader() {
    // Thread per-task pool into SplitReader.
    return SplitReader::create(
            split_,
            hiveTableHandle_,
            &partitionKeys_,
            hiveConfig_,
            readerOutputType_,
            scanSpec_,
            taskPool_);
}

void HiveDataSource::addSplit(std::shared_ptr<ConnectorSplit> split, uint64_t size) {
    split_ = std::dynamic_pointer_cast<HiveConnectorSplit>(split);

    if (splitReader_) {
        splitReader_.reset();
    }

    splitReader_ = createSplitReader();
    splitReader_->prepareSplit(readerOutputType_, size);
}

std::optional<vec::VectorBatch *> HiveDataSource::next(uint64_t size) {
    if (splitReader_ != nullptr && splitReader_->emptySplit()) {
        return getEmptyOutput();
    }
    const std::vector <std::shared_ptr<omniruntime::type::DataType>> children = readerOutputType_->Children();
    std::vector<int> dataTypeIdVector;
    for (int i = 0; i < children.size(); i++) {
        auto dataType = children[i];
        dataTypeIdVector.push_back(dataType->GetId());
    }

    const auto rowsScanned = splitReader_->next(&output_, dataTypeIdVector.data(), size);
    auto rowsRemaining = output_->GetRowCount();
    if (rowsRemaining == 0) {
        // no rows passed the pushed down filters.
        return getEmptyOutput();
    }
    completedRows_ += static_cast<uint64_t>(rowsRemaining);
    completedBytes_ += output_->EstimateFlatSize();
    return output_;
}

} // namespace connector::hive
