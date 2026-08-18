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
#include "reader/ReaderFactory.h"
#include "util/bit_util.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"
#include <algorithm>
#include <list>
#include <limits>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace omniruntime::connector::hive {
namespace {
const std::string kTableFormat = "table_format";
const std::string kIcebergTableFormat = "iceberg";
const std::string kIcebergDeleteFiles = "iceberg_delete_files";
const std::string kPositionDelete = "position";
const std::string kEqualityDelete = "equality";
const std::string kDeleteFilePathColumn = "file_path";
const std::string kDeletePositionColumn = "pos";

bool IsIcebergSplit(const std::unordered_map<std::string, std::string>& customSplitInfo)
{
    auto it = customSplitInfo.find(kTableFormat);
    return it != customSplitInfo.end() && it->second == kIcebergTableFormat;
}

bool IsPartitionColumn(
    const std::string& name,
    const std::unordered_map<std::string, std::shared_ptr<HiveColumnHandle>> *partitionKeys,
    const std::unordered_map<std::string, std::optional<std::string>>& splitPartitionKeys)
{
    auto splitPartitionIt = splitPartitionKeys.find(name);
    if (splitPartitionIt != splitPartitionKeys.end()) {
        return true;
    }
    auto handleIt = partitionKeys->find(name);
    if (handleIt == partitionKeys->end()) {
        return false;
    }
    auto *handle = static_cast<const HiveColumnHandle *>(handleIt->second.get());
    return handle->columnType() == HiveColumnHandle::ColumnType::kPartitionKey;
}

codegen::FileFormat ParseDeleteFileFormat(const std::string& format)
{
    if (format == "parquet") {
        return codegen::FileFormat::PARQUET;
    }
    if (format == "orc") {
        return codegen::FileFormat::ORC;
    }
    return codegen::FileFormat::UNKNOWN;
}

std::string BuildDeleteEnhancementJson()
{
    nlohmann::json json;
    json["ugi"] = "";
    json["includedColumns"] = kDeleteFilePathColumn + "," + kDeletePositionColumn;
    json["allColumns"] = kDeleteFilePathColumn + "," + kDeletePositionColumn;
    return json.dump();
}

std::string BuildBaseEnhancementJson(
    const std::string& enhancementJson,
    const std::unordered_map<std::string, std::string>& customSplitInfo)
{
    if (customSplitInfo.find(kIcebergDeleteFiles) == customSplitInfo.end()) {
        return enhancementJson;
    }

    const std::string jsonStr = enhancementJson.empty() ? "{}" : enhancementJson;
    auto json = nlohmann::json::parse(jsonStr);
    json.erase("expressionTree");
    json.erase("leaves");
    json.erase("vecPredicateCondition");
    return json.dump();
}

std::list<std::string> BuildFileReadColumns(
    const type::RowTypePtr& rowType,
    const std::unordered_map<std::string, std::shared_ptr<HiveColumnHandle>> *partitionKeys,
    const std::unordered_map<std::string, std::optional<std::string>>& splitPartitionKeys)
{
    std::list<std::string> columns;
    for (int i = 0; i < rowType->names().size(); i++) {
        const auto& outputName = rowType->names()[i];
        if (!IsPartitionColumn(outputName, partitionKeys, splitPartitionKeys)) {
            columns.push_back(outputName);
        }
    }
    return columns;
}

void EnsureIcebergIncludedColumns(
    const std::unordered_map<std::string, std::string>& customSplitInfo,
    const type::RowTypePtr& readerOutputType,
    const type::RowTypePtr& dataColumns,
    const std::unordered_map<std::string, std::shared_ptr<HiveColumnHandle>> *partitionKeys,
    const std::unordered_map<std::string, std::optional<std::string>>& splitPartitionKeys,
    const std::shared_ptr<ReaderOptions>& readerOptions)
{
    if (!IsIcebergSplit(customSplitInfo)) {
        return;
    }
    if (readerOptions->GetIncludedColumnsList().empty()) {
        readerOptions->SetIncludedColumnsList(BuildFileReadColumns(readerOutputType, partitionKeys, splitPartitionKeys));
    }
    const auto& allFileColumns = dataColumns != nullptr ? dataColumns : readerOutputType;
    auto& allColumns = readerOptions->GetAllColumnsList();
    allColumns.clear();
    for (const auto& name : allFileColumns->names()) {
        if (!IsPartitionColumn(name, partitionKeys, splitPartitionKeys)) {
            allColumns.push_back(name);
        }
    }
}
} // namespace

struct SplitReader::PositionDeleteReader {
    PositionDeleteReader(
        const std::string& dataFilePath,
        const std::string& deleteFilePath,
        codegen::FileFormat format,
        uint64_t fileSize,
        uint64_t recordCount,
        uint64_t batchLen)
        : dataFilePath_(dataFilePath),
          deleteFilePath_(deleteFilePath),
          format_(format),
          fileSize_(fileSize),
          recordCount_(recordCount),
          batchLen_(batchLen)
    {
        initReader();
    }

    void readDeletedPositions(uint64_t lower, uint64_t size, std::vector<uint8_t>& keepBitmap)
    {
        uint64_t upper = lower + size;
        processPendingPositions(lower, upper, keepBitmap);
        if (pendingOffset_ < pendingPositions_.size()) {
            return;
        }

        while (rowsRead_ < recordCount_) {
            std::vector<omniruntime::vec::BaseVector *> *recordBatch = nullptr;
            int omniTypeId[2] = {0, 0};
            uint64_t batchRowSize = rowReader_->Next(&recordBatch, omniTypeId, batchLen_);
            if (batchRowSize == 0) {
                rowsRead_ = recordCount_;
                break;
            }
            rowsRead_ += batchRowSize;
            appendDeletePositions(recordBatch, batchRowSize);
            freeRecordBatch(recordBatch);
            processPendingPositions(lower, upper, keepBitmap);
            if (pendingOffset_ < pendingPositions_.size()) {
                return;
            }
        }
    }

private:
    void initReader()
    {
        readerOptions_ = std::make_shared<ReaderOptions>();
        readerOptions_->SetBatchLen(static_cast<int32_t>(batchLen_));
        readerOptions_->ParseEnhanceJson(BuildDeleteEnhancementJson(), format_);
        readerOptions_->SetUri(stringToUriInfo(deleteFilePath_));
        auto rowType = omniruntime::type::ROW(
            std::vector<std::string>{kDeleteFilePathColumn, kDeletePositionColumn},
            std::vector<type::DataTypePtr>{omniruntime::type::VarcharType(), omniruntime::type::LongType()});
        readerOptions_->SetRowType(rowType);
        readerOptions_->SetFileRowType(rowType);

        if (format_ == codegen::FileFormat::ORC) {
            auto orcReaderOptions = std::make_shared<::orc::ReaderOptions>();
            orc::MemoryPool *pool = orc::getDefaultPool();
            orcReaderOptions->setMemoryPool(*pool);
            orcReaderOptions->setTailLocation(std::numeric_limits<uint64_t>::max());
            orcReaderOptions->setSerializedFileTail("");
            readerOptions_->SetOrcReaderOptions(orcReaderOptions);
        } else if (format_ == codegen::FileFormat::PARQUET) {
            readerOptions_->SetSplitStart(0);
            int64_t splitEnd = fileSize_ == 0
                ? std::numeric_limits<int64_t>::max()
                : static_cast<int64_t>(fileSize_);
            readerOptions_->SetSplitEnd(splitEnd);
        } else {
            throw std::runtime_error("Unsupported Iceberg position delete file format.");
        }

        reader_ = omniruntime::reader::GetReaderFactory(format_)->CreateReader(readerOptions_);
        if (format_ == codegen::FileFormat::ORC) {
            auto rowReaderOptions = std::make_shared<::orc::RowReaderOptions>();
            uint64_t length = fileSize_ == 0 ? std::numeric_limits<uint64_t>::max() : fileSize_;
            rowReaderOptions->range(0, length);
            rowReaderOptions->include(readerOptions_->GetIncludedColumnsList());
            rowReaderOptions->searchArgument(readerOptions_->releaseSearchArgument());
            readerOptions_->SetOrcRowReaderOptions(rowReaderOptions);
        }
        rowReader_ = reader_->CreateRowReader();
    }

    void appendDeletePositions(std::vector<omniruntime::vec::BaseVector *> *recordBatch, uint64_t batchRowSize)
    {
        if (recordBatch == nullptr || recordBatch->size() < 2) {
            return;
        }
        auto pathVector = recordBatch->at(0);
        auto positionVector = recordBatch->at(1);
        for (uint64_t i = 0; i < batchRowSize; i++) {
            if (pathVector->IsNull(static_cast<int32_t>(i)) || positionVector->IsNull(static_cast<int32_t>(i))) {
                continue;
            }
            auto filePath = vec::VectorHelper::GetStringValueFromVector(pathVector, static_cast<int32_t>(i));
            if (filePath != std::string_view(dataFilePath_.data(), dataFilePath_.size())) {
                continue;
            }
            int64_t position = 0;
            if (!vec::VectorHelper::GetValueFromVector<int64_t>(
                    positionVector,
                    static_cast<int32_t>(i),
                    position)) {
                continue;
            }
            if (position >= 0) {
                pendingPositions_.push_back(static_cast<uint64_t>(position));
            }
        }
        std::sort(pendingPositions_.begin() + pendingOffset_, pendingPositions_.end());
    }

    void freeRecordBatch(std::vector<omniruntime::vec::BaseVector *> *recordBatch)
    {
        if (recordBatch == nullptr) {
            return;
        }
        std::unordered_set<omniruntime::vec::BaseVector *> deleted;
        for (auto* vector : *recordBatch) {
            if (deleted.insert(vector).second) {
                delete vector;
            }
        }
        recordBatch->clear();
    }

    void processPendingPositions(uint64_t lower, uint64_t upper, std::vector<uint8_t>& keepBitmap)
    {
        while (pendingOffset_ < pendingPositions_.size() && pendingPositions_[pendingOffset_] < lower) {
            pendingOffset_++;
        }
        while (pendingOffset_ < pendingPositions_.size() && pendingPositions_[pendingOffset_] < upper) {
            uint64_t offset = pendingPositions_[pendingOffset_] - lower;
            omniruntime::BitUtil::ClearBit(keepBitmap.data(), static_cast<uint32_t>(offset));
            pendingOffset_++;
        }
        if (pendingOffset_ == pendingPositions_.size()) {
            pendingPositions_.clear();
            pendingOffset_ = 0;
        }
    }

    std::string dataFilePath_;
    std::string deleteFilePath_;
    codegen::FileFormat format_;
    uint64_t fileSize_;
    uint64_t recordCount_;
    uint64_t batchLen_;
    uint64_t rowsRead_ = 0;
    std::vector<uint64_t> pendingPositions_;
    size_t pendingOffset_ = 0;
    std::shared_ptr<ReaderOptions> readerOptions_;
    std::unique_ptr<omniruntime::reader::Reader> reader_;
    std::unique_ptr<omniruntime::reader::RowReader> rowReader_;
};

SplitReader::SplitReader(
    const std::shared_ptr<const hive::HiveConnectorSplit> &hiveSplit,
    const std::shared_ptr<const HiveTableHandle> &hiveTableHandle,
    const std::unordered_map <std::string, std::shared_ptr<HiveColumnHandle>> * partitionKeys,
    const std::shared_ptr<const HiveConfig> &hiveConfig,
    const type::RowTypePtr &readerOutputType,
    const std::shared_ptr <codegen::ScanSpec> &scanSpec)
    : hiveSplit_(hiveSplit),
      hiveTableHandle_(hiveTableHandle),
      partitionKeys_(partitionKeys),
      hiveConfig_(hiveConfig),
      readerOutputType_(readerOutputType),
      pool_(omniruntime::mem::GetMemoryPool()),
      scanSpec_(scanSpec),
      emptySplit_(false)
{
    baseReaderOpts_ = std::make_shared<ReaderOptions>();
}

SplitReader::~SplitReader() = default;

std::unique_ptr <SplitReader> SplitReader::create(
    const std::shared_ptr <hive::HiveConnectorSplit> &hiveSplit,
    const std::shared_ptr<const HiveTableHandle> &hiveTableHandle,
    const std::unordered_map <std::string, std::shared_ptr<HiveColumnHandle>> *partitionKeys,
    const std::shared_ptr<const HiveConfig> &hiveConfig,
    const type::RowTypePtr &readerOutputType,
    const std::shared_ptr <codegen::ScanSpec> &scanSpec)
{
    return std::make_unique<SplitReader>(
        hiveSplit,
        hiveTableHandle,
        partitionKeys,
        hiveConfig,
        readerOutputType,
        scanSpec);
}

void SplitReader::prepareSplit(omniruntime::type::RowTypePtr &rowType, uint64_t batchLen, common::ReadMode readMode)
{
    baseReaderOpts_->SetBatchLen(batchLen);
    baseReaderOpts_->SetreadMode(readMode);
    baseReaderOpts_->SetCoalesceMaxBytes(hiveConfig_->maxCoalescedBytes());
    baseReaderOpts_->SetCoalesceMaxDistance(hiveConfig_->maxCoalescedDistance());
    baseReaderOpts_->SetFilePreloadThreshold(static_cast<int64_t>(hiveConfig_->filePreloadThreshold()));
    createReader();
    createRowReader(rowType, batchLen);
    initIcebergPositionDeleteReaders(batchLen);
}

uint64_t SplitReader::next(vec::VectorBatch **output_, int * /*omniTypeId*/, uint64_t batchLen)
{
    std::vector<int> fileOmniTypeIds;
    fileOmniTypeIds.reserve(fileRowType_->size());
    for (int i = 0; i < fileRowType_->size(); ++i) {
        fileOmniTypeIds.push_back(fileRowType_->children_()[i]->GetId());
    }

    while (true) {
        std::vector<omniruntime::vec::BaseVector *> *recordBatch;
        uint64_t batchRowSize = baseRowReader_->Next(&recordBatch, fileOmniTypeIds.data(), batchLen);

        auto output = new vec::VectorBatch(batchRowSize);
        *output_ = output;
        if (batchRowSize <= 0) {
            return batchRowSize;
        }

        std::vector<vec::BaseVector *> fileVectors(fileRowType_->size(), nullptr);
        for (int i = 0; i < fileRowType_->size(); ++i) {
            if (i < recordBatch->size()) {
                fileVectors[i] = recordBatch->at(i);
            } else {
                auto missingFieldVec = vec::VectorHelper::CreateFlatVector(
                    fileRowType_->children_()[i]->GetId(),
                    batchRowSize);
                for (int j = 0; j < batchRowSize; ++j) {
                    omniruntime::vec::VectorHelper::SetValue(missingFieldVec, j, nullptr);
                }
                fileVectors[i] = missingFieldVec;
            }
        }
        delete recordBatch;

        for (int i = 0; i < rowType_->size(); ++i) {
            const auto& outputName = rowType_->nameOf(i);
            if (isRowIndexColumn(outputName)) {
                output->Append(createRowIndexVec(static_cast<int32_t>(batchRowSize)));
                continue;
            }
            if (IsPartitionColumn(outputName, partitionKeys_, hiveSplit_->partitionKeys)) {
                auto dataTypeId = rowType_->children_()[i]->GetId();
                auto splitPartitionIt = hiveSplit_->partitionKeys.find(outputName);
                vec::BaseVector *partitionVec = nullptr;
                if (splitPartitionIt != hiveSplit_->partitionKeys.end() && splitPartitionIt->second.has_value()) {
                    const std::string &val = splitPartitionIt->second.value();
                    partitionVec = createConstPartitionVec(dataTypeId, batchRowSize, val);
                } else {
                    partitionVec = createNullConstVec(dataTypeId, batchRowSize);
                }
                output->Append(partitionVec);
                continue;
            }

            auto fileColumnIndex = fileRowType_->getChildIdxIfExists(outputName);
            if (fileColumnIndex.has_value()) {
                output->Append(fileVectors[*fileColumnIndex]);
                fileVectors[*fileColumnIndex] = nullptr;
                continue;
            }

            auto missingOutputVec = vec::VectorHelper::CreateFlatVector(
                rowType_->children_()[i]->GetId(),
                batchRowSize);
            for (int j = 0; j < batchRowSize; ++j) {
                omniruntime::vec::VectorHelper::SetValue(missingOutputVec, j, nullptr);
            }
            output->Append(missingOutputVec);
        }

        for (auto *fileVector : fileVectors) {
            if (fileVector != nullptr) {
                delete fileVector;
            }
        }

        batchRowSize = applyIcebergPositionDeletes(&output, batchRowSize);
        *output_ = output;
        if (batchRowSize == 0) {
            delete output;
            *output_ = nullptr;
            continue;
        }
        return batchRowSize;
    }
}

void SplitReader::createReader()
{
    auto enhancementJson = BuildBaseEnhancementJson(
        hiveTableHandle_->GetEnhancementJson(),
        hiveSplit_->customSplitInfo);
    baseReaderOpts_->ParseEnhanceJson(enhancementJson, hiveSplit_->fileFormat);
    EnsureIcebergIncludedColumns(
        hiveSplit_->customSplitInfo,
        readerOutputType_,
        hiveTableHandle_->dataColumns(),
        partitionKeys_,
        hiveSplit_->partitionKeys,
        baseReaderOpts_);
    configureReaderOptions(hiveConfig_, hiveSplit_, baseReaderOpts_);
    baseReader_ = omniruntime::reader::GetReaderFactory(hiveSplit_->fileFormat)
        ->CreateReader(baseReaderOpts_);
}

bool SplitReader::isRowIndexColumn(const std::string &name) const
{
    auto *fieldSpec = scanSpec_->childByName(name);
    return fieldSpec != nullptr &&
        fieldSpec->columnType() == codegen::ScanSpec::ColumnType::kRowIndex;
}

vec::BaseVector* SplitReader::createRowIndexVec(int32_t batchRowSize)
{
    auto *rowIndexVec = reinterpret_cast<vec::Vector<int64_t> *>(
        vec::VectorHelper::CreateFlatVector(type::OMNI_LONG, batchRowSize));
    // When vec-predicate filtering is applied inside the native reader (ORC/Parquet),
    // the surviving rows are not sequential. Use the exact file-absolute positions
    // captured during filtering (stored in the shared PredicateCondition) to keep
    // row_index semantically correct. Falls back to sequential numbering otherwise.
    auto &predicatePtr = baseRowReader_->GetPredicatePtr();
    if (predicatePtr != nullptr) {
        const std::vector<int64_t> *survivingPositions = predicatePtr->GetSurvivingPositions();
        if (survivingPositions != nullptr) {
            for (int32_t i = 0; i < batchRowSize && i < static_cast<int32_t>(survivingPositions->size()); ++i) {
                rowIndexVec->SetValue(i, survivingPositions->at(i));
            }
            return rowIndexVec;
        }
    }
    // Fallback: sequential row_index starting from the reader's file-absolute position.
    int64_t batchStart = static_cast<int64_t>(baseRowReader_->LastReadRowPosition());
    for (int32_t i = 0; i < batchRowSize; ++i) {
        rowIndexVec->SetValue(i, batchStart + i);
    }
    return rowIndexVec;
}

void SplitReader::createRowReader(omniruntime::type::RowTypePtr &rowType, uint64_t batchLen)
{
    std::vector <std::string> readColumnNames;
    std::vector <std::shared_ptr<omniruntime::type::DataType>> readColumnTypes;
    for (int i = 0; i < rowType->names().size(); i++) {
        const auto &outputName = rowType->names()[i];
        if (isRowIndexColumn(outputName)) {
            continue;
        }
        if (!IsPartitionColumn(outputName, partitionKeys_, hiveSplit_->partitionKeys)) {
            readColumnNames.push_back(outputName);
            readColumnTypes.push_back(rowType->children_()[i]);
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

void SplitReader::initIcebergPositionDeleteReaders(uint64_t batchLen)
{
    auto it = hiveSplit_->customSplitInfo.find(kIcebergDeleteFiles);
    if (it == hiveSplit_->customSplitInfo.end()) {
        return;
    }

    auto deleteFiles = nlohmann::json::parse(it->second);
    for (const auto& deleteFile : deleteFiles) {
        const auto content = deleteFile.at("content").get<std::string>();
        if (content == kEqualityDelete) {
            throw std::runtime_error("Iceberg equality delete is not supported by Omni pipeline scan.");
        }
        if (content != kPositionDelete) {
            continue;
        }
        auto format = ParseDeleteFileFormat(deleteFile.at("format").get<std::string>());
        positionDeleteReaders_.push_back(std::make_unique<PositionDeleteReader>(
            hiveSplit_->filePath,
            deleteFile.at("path").get<std::string>(),
            format,
            deleteFile.value("fileSize", 0ULL),
            deleteFile.value("recordCount", std::numeric_limits<uint64_t>::max()),
            batchLen));
    }
}

uint64_t SplitReader::applyIcebergPositionDeletes(vec::VectorBatch **output, uint64_t batchRowSize)
{
    if (positionDeleteReaders_.empty()) {
        return batchRowSize;
    }

    std::vector<uint8_t> keepBitmap(omniruntime::BitUtil::Nbytes(static_cast<int32_t>(batchRowSize)) + 8, 0xff);
    uint64_t batchStartPosition = baseRowReader_->LastReadRowPosition();
    for (auto& deleteReader : positionDeleteReaders_) {
        deleteReader->readDeletedPositions(batchStartPosition, batchRowSize, keepBitmap);
    }

    std::vector<int> positions;
    positions.reserve(batchRowSize);
    for (uint64_t i = 0; i < batchRowSize; i++) {
        if (omniruntime::BitUtil::IsBitSet(keepBitmap.data(), static_cast<int32_t>(i))) {
            positions.push_back(static_cast<int>(i));
        }
    }
    if (positions.size() == batchRowSize) {
        return batchRowSize;
    }
    if (positions.empty()) {
        return 0;
    }

    auto filtered = new vec::VectorBatch(positions.size());
    auto vectorCount = (*output)->GetVectorCount();
    for (int i = 0; i < vectorCount; i++) {
        filtered->Append(vec::VectorHelper::CopyPositionsVector(
            (*output)->Get(i),
            positions.data(),
            0,
            static_cast<int>(positions.size())));
    }
    delete *output;
    *output = filtered;
    return positions.size();
}
} // namespace hive
