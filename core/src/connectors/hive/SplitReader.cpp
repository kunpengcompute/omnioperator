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

#include "SplitReader.h"
#include "reader/common/Filter.h"
#include "util/debug.h"

#include <string>

#include <nlohmann/json.hpp>

namespace omniruntime::connector::hive {

bool SplitReader::partitionValuePassesFilter(
    const ::common::Filter *filter, type::DataTypeId typeId, const std::string &val)
{
    if (filter == nullptr) {
        return true;
    }
    try {
        switch (typeId) {
            case omniruntime::type::OMNI_BYTE:
            case omniruntime::type::OMNI_SHORT:
            case omniruntime::type::OMNI_INT:
            case omniruntime::type::OMNI_LONG:
            case omniruntime::type::OMNI_DECIMAL64:
            case omniruntime::type::OMNI_DATE64:
                return filter->testInt64(std::stoll(val));
            case omniruntime::type::OMNI_DATE32:
                if (val.size() == 10 && val[4] == '-' && val[7] == '-') {
                    return filter->testInt64(ParseDate32(val));
                }
                return filter->testInt64(std::stoll(val));
            case omniruntime::type::OMNI_BOOLEAN:
                return filter->testBool(val == "true" || val == "1");
            case omniruntime::type::OMNI_FLOAT:
            case omniruntime::type::OMNI_DOUBLE:
                return filter->testDouble(std::stod(val));
            case omniruntime::type::OMNI_CHAR:
            case omniruntime::type::OMNI_VARCHAR:
            case omniruntime::type::OMNI_VARBINARY:
                return filter->testBytes(val.data(), static_cast<int32_t>(val.size()));
            default:
                return filter->testInt64(std::stoll(val));
        }
    } catch (...) {
        LogDebug("DFP: failed to parse partition value '%s' typeId=%d; keeping split", val.c_str(),
            static_cast<int>(typeId));
        return true;
    }
}

bool SplitReader::partitionKeysPassFilters()
{
    if (scanSpec_ == nullptr || hiveSplit_ == nullptr) {
        return true;
    }
    int filterCount = 0;
    int matchedPartKeys = 0;
    for (const auto &child : scanSpec_->children()) {
        if (child == nullptr || child->filter() == nullptr) {
            continue;
        }
        ++filterCount;
        const auto &name = child->fieldName();
        auto partIt = hiveSplit_->partitionKeys.find(name);
        if (partIt == hiveSplit_->partitionKeys.end()) {
            continue;
        }
        ++matchedPartKeys;
        const auto *filter = child->filter();
        if (!partIt->second.has_value()) {
            if (!filter->testNull()) {
                LogDebug("DFP: skip split null partition %s fails filter", name.c_str());
                return false;
            }
            continue;
        }
        type::DataTypeId typeId = type::OMNI_LONG;
        if (partitionKeys_ != nullptr) {
            auto handleIt = partitionKeys_->find(name);
            if (handleIt != partitionKeys_->end() && handleIt->second != nullptr &&
                handleIt->second->dataType() != nullptr) {
                typeId = handleIt->second->dataType()->GetId();
            }
        }
        if (!partitionValuePassesFilter(filter, typeId, partIt->second.value())) {
            LogDebug("DFP: skip split partition %s='%s' fails filter kind=%d typeId=%d path=%s", name.c_str(),
                partIt->second.value().c_str(), static_cast<int>(filter->kind()), static_cast<int>(typeId),
                hiveSplit_->filePath.c_str());
            return false;
        }
        if (!partitionFilterDiagLogged_) {
            partitionFilterDiagLogged_ = true;
            LogDebug("DFP: partition filter matched spec=%s splitVal='%s' kind=%d typeId=%d kept=1 path=%s",
                name.c_str(), partIt->second.value().c_str(), static_cast<int>(filter->kind()),
                static_cast<int>(typeId), hiveSplit_->filePath.c_str());
        }
    }
    if (filterCount > 0 && matchedPartKeys == 0 && !partitionFilterDiagLogged_) {
        partitionFilterDiagLogged_ = true;
        std::string specNames;
        for (const auto &child : scanSpec_->children()) {
            if (child != nullptr && child->filter() != nullptr) {
                specNames.append(child->fieldName()).append(",");
            }
        }
        std::string splitNames;
        for (const auto &kv : hiveSplit_->partitionKeys) {
            splitNames.append(kv.first);
            if (kv.second.has_value()) {
                splitNames.append("=").append(kv.second.value());
            }
            splitNames.append(",");
        }
        LogDebug("DFP: partition filter NOT applied specFilters=%s splitPartKeys=%s splitPartKeyCount=%zu path=%s",
            specNames.c_str(), splitNames.c_str(), hiveSplit_->partitionKeys.size(), hiveSplit_->filePath.c_str());
    }
    return true;
}

namespace {

std::string BuildTextEnhancementJson(
    const std::string& enhancementJson,
    const std::unordered_map<std::string, std::string>& customSplitInfo,
    const std::shared_ptr<const HiveConfig>& hiveConfig)
{
    const std::string jsonStr = enhancementJson.empty() ? "{}" : enhancementJson;
    auto json = nlohmann::json::parse(jsonStr);
    for (const auto& [key, value] : customSplitInfo) {
        if (key.rfind("text.", 0) == 0) {
            json[key] = value;
        }
    }
    const auto& config = hiveConfig->config();
    if (!json.contains("text.session_timezone") || json["text.session_timezone"].get<std::string>().empty()) {
        if (config->ValueExists("spark.sql.session.timeZone")) {
            json["text.session_timezone"] =
                config->Get<std::string>("spark.sql.session.timeZone", "");
        } else if (config->ValueExists("spark.gluten.sql.session.timeZone.default")) {
            json["text.session_timezone"] =
                config->Get<std::string>("spark.gluten.sql.session.timeZone.default", "");
        }
    }
    return json.dump();
}

} // namespace

SplitReader::SplitReader(
    const std::shared_ptr<const hive::HiveConnectorSplit> &hiveSplit,
    const std::shared_ptr<const HiveTableHandle> &hiveTableHandle,
    const std::unordered_map <std::string, std::shared_ptr<HiveColumnHandle>> * partitionKeys,
    const std::shared_ptr<const HiveConfig> &hiveConfig,
    const type::RowTypePtr &readerOutputType,
    const std::shared_ptr <codegen::ScanSpec> &scanSpec,
    omniruntime::mem::OmniTaskMemoryPool *taskPool)
    : hiveSplit_(hiveSplit),
      hiveTableHandle_(hiveTableHandle),
      partitionKeys_(partitionKeys),
      hiveConfig_(hiveConfig),
      readerOutputType_(readerOutputType),
      pool_(omniruntime::mem::GetMemoryPool()),
      taskPool_(taskPool),
      // The ORC pool adapter is constructed
      // unconditionally; when taskPool_ is null its malloc/free fall back to
      // std::malloc/free (no per-task accounting, ORC-default behaviour).
      orcPoolAdapter_(taskPool),
      scanSpec_(scanSpec),
      emptySplit_(false)
{
    baseReaderOpts_ = std::make_shared<ReaderOptions>();
}

std::unique_ptr <SplitReader> SplitReader::create(
    const std::shared_ptr <hive::HiveConnectorSplit> &hiveSplit,
    const std::shared_ptr<const HiveTableHandle> &hiveTableHandle,
    const std::unordered_map <std::string, std::shared_ptr<HiveColumnHandle>> *partitionKeys,
    const std::shared_ptr<const HiveConfig> &hiveConfig,
    const type::RowTypePtr &readerOutputType,
    const std::shared_ptr <codegen::ScanSpec> &scanSpec,
    omniruntime::mem::OmniTaskMemoryPool *taskPool)
{
    return std::make_unique<SplitReader>(
        hiveSplit,
        hiveTableHandle,
        partitionKeys,
        hiveConfig,
        readerOutputType,
        scanSpec,
        taskPool);
}

void SplitReader::prepareSplit(omniruntime::type::RowTypePtr &rowType, uint64_t batchLen)
{
    baseReaderOpts_->SetBatchLen(batchLen);
    baseReaderOpts_->SetCoalesceMaxBytes(hiveConfig_->maxCoalescedBytes());
    baseReaderOpts_->SetCoalesceMaxDistance(hiveConfig_->maxCoalescedDistance());
    baseReaderOpts_->SetFilePreloadThreshold(static_cast<int64_t>(hiveConfig_->filePreloadThreshold()));
    if (!partitionKeysPassFilters()) {
        emptySplit_ = true;
        return;
    }
    createReader();
    createRowReader(rowType, batchLen);
}

uint64_t SplitReader::next(vec::VectorBatch **output_, int *omniTypeId, uint64_t batchLen)
{
    // Filter may arrive after prepareSplit; re-check so a late DF still skips.
    if (!emptySplit_ && !partitionKeysPassFilters()) {
        emptySplit_ = true;
    }
    if (emptySplit_ || baseRowReader_ == nullptr) {
        emptySplit_ = true;
        *output_ = new vec::VectorBatch(0);
        return 0;
    }
    std::vector<omniruntime::vec::BaseVector *> *recordBatch;
    uint64_t batchRowSize = baseRowReader_->Next(&recordBatch, omniTypeId, batchLen);

    auto output = new vec::VectorBatch(batchRowSize);
    *output_ = output;
    if (batchRowSize <= 0) {
        return batchRowSize;
    }
    for (int i = 0; i < recordBatch->size(); ++i) {
        output->Append(recordBatch->at(i));
    }
    if (fileRowType_->size() > output->GetVectorCount()) {
        for (int i = output->GetVectorCount(); i < fileRowType_->size(); ++i) {
            // Use the per-task accounting pool threaded from TableScanOperator
            // via Connector/HiveDataSource.
            // taskPool_ may still be nullptr in legacy test rigs that bypass
            // TableScanOperator — that falls back to the [[deprecated]] no-pool
            // CreateFlatVector overload (accounting skipped, behaviour unchanged).
            auto missingFieldVec = vec::VectorHelper::CreateFlatVector(taskPool_,
                fileRowType_->children_()[i]->GetId(), batchRowSize);
            for (int j = 0; j < batchRowSize; ++j) {
                omniruntime::vec::VectorHelper::SetValue(missingFieldVec, j, nullptr);
            }
            output->Append(missingFieldVec);
        }
    }
    // Partition columns: not stored in data files, injected from split metadata
    if (rowType_->size() > fileRowType_->size()) {
        for (int i = fileRowType_->size(); i < rowType_->size(); ++i) {
            auto dataTypeId = rowType_->children_()[i]->GetId();
            auto it = hiveSplit_->partitionKeys.find(rowType_->nameOf(i));
            vec::BaseVector *partitionVec = nullptr;
            if (it->second.has_value()) {
                const std::string &val = it->second.value();
                partitionVec = createConstPartitionVec(dataTypeId, batchRowSize, val);
            } else {
                partitionVec = createNullConstVec(dataTypeId, batchRowSize);
            }
            output->Append(partitionVec);
        }
    }
    return batchRowSize;
}

void SplitReader::createReader()
{
    auto enhancementJson = hiveTableHandle_->GetEnhancementJson();
    if (hiveSplit_->fileFormat == codegen::FileFormat::TEXT) {
        enhancementJson = BuildTextEnhancementJson(
            enhancementJson, hiveSplit_->customSplitInfo, hiveConfig_);
    }
    baseReaderOpts_->ParseEnhanceJson(enhancementJson, hiveSplit_->fileFormat);
    // Pass the per-task adapter
    // so the ORC branch in configureReaderOptions wires our pool into
    // orc::ReaderOptions::setMemoryPool. Adapter lifetime is this
    // SplitReader's lifetime; the resulting orc::Reader (baseReader_) is
    // also our member, so the adapter outlives it via reverse-destruction.
    configureReaderOptions(hiveConfig_, hiveSplit_, baseReaderOpts_, &orcPoolAdapter_);
    // Stash the per-task pool on
    // ReaderOptions so OrcRowReader's omniBuildReader call propagates it down
    // to every ColumnReader (output Vector ctors will then use the pool-bound
    // primary ctor instead of the [[deprecated]] no-pool bridge).
    baseReaderOpts_->SetTaskPool(taskPool_);
    baseReader_ = omniruntime::reader::GetReaderFactory(hiveSplit_->fileFormat)
        ->CreateReader(baseReaderOpts_);
}

void SplitReader::createRowReader(omniruntime::type::RowTypePtr &rowType, uint64_t batchLen)
{
    std::vector <std::string> readColumnNames;
    std::vector <std::shared_ptr<omniruntime::type::DataType>> readColumnTypes;
    for (int i = 0; i < rowType->names().size(); i++) {
        const auto &outputName = rowType->names()[i];
        auto it = partitionKeys_->find(outputName);
        if (it == partitionKeys_->end()) {
            readColumnNames.push_back(outputName);
            readColumnTypes.push_back(rowType->children_()[i]);
            continue;
        }
        auto *handle = static_cast<const HiveColumnHandle *>(it->second.get());
        if (handle->columnType() == HiveColumnHandle::ColumnType::kPartitionKey) {
            continue;
        }
    }
    fileRowType_ = ROW(std::move(readColumnNames), std::move(readColumnTypes));
    rowType_ = rowType;

    // baseReaderOpts_->SetBatchLen(static_cast<int32_t>(batchLen));
    configureRowReaderOptions(
        hiveTableHandle_,
        rowType_,
        fileRowType_,
        scanSpec_,
        hiveSplit_,
        hiveConfig_,
        baseReaderOpts_);
    baseRowReader_ = baseReader_->CreateRowReader();
}
} // namespace hive
