/**
 * Copyright (C) 2023-2023. Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OMNI_RUNTIME_PARQUETREADER_H
#define OMNI_RUNTIME_PARQUETREADER_H

#include <vector/vector_common.h>
#include <arrow/filesystem/filesystem.h>
#include <arrow/dataset/file_base.h>
#include "ParquetColumnReader.h"
#include "reader/common/UriInfo.h"
#include "ParquetExpression.h"
#include "reader/Reader.h"
#include "reader/common/TimeRebaseInfo.h"
#include <set>
#include "reader/common/PredicateUtil.h"

using namespace arrow::internal;

namespace omniruntime::reader {

class OmniRecordBatchReader {
public:
    OmniRecordBatchReader(std::function<arrow::Status(std::vector<omniruntime::vec::BaseVector*> &batch, long *batchRowSize)> batches)
        : batches_(std::move(batches)) {}

    ~OmniRecordBatchReader() {}

    arrow::Status ReadNext(std::vector<omniruntime::vec::BaseVector*> &out, long *batchRowSize) {
        return batches_(out, batchRowSize);
    }

private:
    std::function<arrow::Status(std::vector<omniruntime::vec::BaseVector*> &batch, long *batchRowSize)> batches_;
};

class ParquetReader;
class ParquetRowReader : public omniruntime::reader::RowReader {
public:
    explicit ParquetRowReader(ParquetReader &parquetReader);

    ~ParquetRowReader() override = default;

    uint64_t Next(uint64_t size, vec::VectorPtr &result) override {
        return 0;
    };

    uint64_t NextDirect(std::vector<BaseVector *> *batch, int *omniTypeId, uint64_t batchLen) override;

    uint64_t Next(std::vector<BaseVector *> **batch, int *omniTypeId, uint64_t batchLen) override;

    // Expose the per-task pool stashed on ReaderOptions so the file-scope
    // FilterData/ReadAndFilterData helpers can thread it through
    // common::GetFlatBaseVectorsFromBitMark. Symmetric with OrcRowReader::GetTaskPool().
    // Defined out-of-line below ParquetReader because parquetReader_.GetOptions()
    // needs the ParquetReader full definition (only forward-declared above this class).
    omniruntime::mem::OmniTaskMemoryPool *GetTaskPool();

private:
    ParquetReader &parquetReader_;
};


class ParquetReader : public omniruntime::reader::Reader {
public:
    ParquetReader(
        std::unique_ptr<common::TimeRebaseInfo> &rebaseInfoPtr,
        const std::shared_ptr<ReaderOptions> &options) : rebaseInfoPtr(std::move(rebaseInfoPtr))
    {
        options_ = options;
    }

    std::shared_ptr<ReaderOptions> GetOptions() { return options_; }

    arrow::Status InitReader(UriInfo &uri, int64_t capacity, std::string& ugi);

    arrow::Status InitRecordReader(int64_t start, int64_t end, bool hasExpressionTree,
        Expression pushedFilterArray, const std::vector<std::string>& fieldNames);

    arrow::Status ReadNextBatch(std::vector<omniruntime::vec::BaseVector*> &batch, long *batchRowSize);

    std::unique_ptr<parquet::arrow::FileReader> arrow_reader;

    std::unique_ptr<OmniRecordBatchReader> rb_reader;

    std::vector<std::shared_ptr<ParquetColumnReader>> columnReaders;

    arrow::MemoryPool* pool = arrow::default_memory_pool();

    std::unique_ptr<RowReader> CreateRowReader() override
    {
        int64_t start = options_->GetSplitStart();
        int64_t end = options_->GetSplitEnd();

        auto jsonConfig = options_->GetEnhancementJson();
        bool hasExpressionTree = jsonConfig->contains("expressionTree");
        Expression pushedFilterArray = options_->GetParquetPushedFilterArray();

        const auto& fieldNames = options_->GetParquetIncludedColumns();

        auto state = InitRecordReader(start, end, hasExpressionTree, pushedFilterArray, fieldNames);
        if (state != Status::OK()) {
            throw OmniException(state.ToString().c_str());
            return nullptr;
        }

        auto rowReader = std::make_unique<ParquetRowReader>(*this);
        return std::move(rowReader);
    }

private:
    arrow::Status GetRowGroupIndices(arrow::dataset::FileSource filesource, int64_t start, int64_t end,
        bool hasExpressionTree, Expression pushedFilterArray, std::vector<int>& out);

    arrow::Status GetColumnIndices(const std::vector<std::string>& vector, std::vector<int>& out);

    arrow::Status GetRecordBatchReader(const std::vector<int> &row_group_indices, const std::vector<int> &column_indices);

    arrow::Status GetFieldReaders(const std::vector<int> &row_group_indices, const std::vector<int> &column_indices,
        std::vector<std::shared_ptr<ParquetColumnReader>>* out, std::shared_ptr<::arrow::Schema>* out_schema);

    arrow::Status GetFieldReader(int i, const std::shared_ptr<std::unordered_set<int>>& included_leaves,
        const std::vector<int> &row_group_indices, std::unique_ptr<ParquetColumnReader>* out);

    arrow::Status GetReader(const parquet::arrow::SchemaField &field, const std::shared_ptr<arrow::Field> &arrow_field,
        const std::shared_ptr<parquet::arrow::ReaderContext> &ctx, std::unique_ptr<ParquetColumnReader>* out);

    std::unique_ptr<common::TimeRebaseInfo> rebaseInfoPtr;

    std::shared_ptr<arrow::io::RandomAccessFile> file;
};

// Out-of-line definition — ParquetReader is now complete at this point in the
// header, so parquetReader_.GetOptions() can be resolved.
inline omniruntime::mem::OmniTaskMemoryPool *ParquetRowReader::GetTaskPool()
{
    auto opts = parquetReader_.GetOptions();
    return opts != nullptr ? opts->GetTaskPool() : nullptr;
}

class Filesystem {
public:
    Filesystem() {}

    /**
     * File system holds the hdfs client, which should outlive the RecordBatchReader.
     */
    std::shared_ptr<arrow::fs::FileSystem> filesys_ptr;
};

std::string GetFileSystemKey(std::string& path, std::string& ugi);

Filesystem* GetFileSystemPtr(UriInfo &uri, std::string& ugi, arrow::Status &status);
}
#endif // OMNI_RUNTIME_PARQUETREADER_H