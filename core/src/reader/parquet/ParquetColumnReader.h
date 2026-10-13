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

#ifndef OMNI_RUNTIME_COLUMN_READER_H
#define OMNI_RUNTIME_COLUMN_READER_H

#include "ParquetTypedRecordReader.h"
#include <parquet/column_reader.h>
#include <parquet/arrow/reader_internal.h>
#include "reader/common/TimeRebaseInfo.h"

namespace omniruntime::reader {
    class ParquetColumnReader {
    public:
        ParquetColumnReader(std::shared_ptr<::parquet::arrow::ReaderContext> ctx)
                : ctx_(std::move(ctx)) {}
        ParquetColumnReader(std::shared_ptr<::parquet::arrow::ReaderContext> ctx, std::shared_ptr<::arrow::Field> field)
                : ctx_(std::move(ctx)), field_(std::move(field)) {}
        ParquetColumnReader(std::shared_ptr<::parquet::arrow::ReaderContext> ctx, std::shared_ptr<::arrow::Field> field,
                            std::unique_ptr<::parquet::arrow::FileColumnIterator> input, ::parquet::internal::LevelInfo leaf_info,
                            common::TimeRebaseInfo *rebaseInfoPtr)
                : ctx_(std::move(ctx)),
                  field_(std::move(field)),
                  input_(std::move(input)),
                  descr_(input_->descr()) {
            record_reader_ = MakeRecordReader(descr_, leaf_info, ctx_->pool,
                                              field_->type()->id() == ::arrow::Type::DICTIONARY, field_->type(), rebaseInfoPtr);
            NextRowGroup();
        }

        ::arrow::Status NextBatch(int64_t batch_size, omniruntime::vec::BaseVector** out);

        virtual ::arrow::Status LoadBatch(int64_t records_to_read, omniruntime::vec::BaseVector** out);
        virtual ::arrow::Status GetDefLevels(const int16_t** data, int64_t* length);
        virtual ::arrow::Status GetRepLevels(const int16_t** data, int64_t* length);
        virtual bool IsOrHasRepeatedChild() const { return false; }

        virtual const std::shared_ptr<::arrow::Field> field() {
            return field_;
        }

        // Thread the per-task pool down to the leaf record reader so its InitVec
        // allocates pool-accounted output Vectors. Fanned out by ParquetReader after
        // the reader tree is built. ListReader/StructReader override to recurse into
        // their children. Symmetric with ORC's OmniColumnReader::SetTaskPool.
        virtual void SetTaskPool(omniruntime::mem::OmniTaskMemoryPool *pool) {
            taskPool_ = pool;
            if (record_reader_ != nullptr) {
                record_reader_->SetTaskPool(pool);
            }
        }

    protected:
        std::shared_ptr<::parquet::arrow::ReaderContext> ctx_;
        std::shared_ptr<::arrow::Field> field_;
        // Per-task pool delivered by SetTaskPool; threaded into the complex-vector
        // (ArrayVector/MapVector/RowVector) wrappers built in ListReader/StructReader
        // LoadBatch so their top-level offsets/validity buffers are pool-accounted.
        omniruntime::mem::OmniTaskMemoryPool *taskPool_ = nullptr;
    private:
        void NextRowGroup();
        std::unique_ptr<::parquet::arrow::FileColumnIterator> input_;
        const ::parquet::ColumnDescriptor* descr_;
        std::shared_ptr<OmniRecordReader> record_reader_;
    };

    class ListReader : public ParquetColumnReader {
    public:
        ListReader(std::shared_ptr<::parquet::arrow::ReaderContext> ctx, std::shared_ptr<::arrow::Field> field,
                   ::parquet::internal::LevelInfo level_info,
                   std::unique_ptr<ParquetColumnReader> child_reader)
                : ParquetColumnReader(std::move(ctx), std::move(field)), level_info_(level_info),
                  item_reader_(std::move(child_reader)) {}
        ::arrow::Status GetDefLevels(const int16_t** data, int64_t* length) override;
        ::arrow::Status GetRepLevels(const int16_t** data, int64_t* length) override;

        ::arrow::Status LoadBatch(int64_t records_to_read, BaseVector** out) override;

        void SetTaskPool(omniruntime::mem::OmniTaskMemoryPool *pool) override {
            ParquetColumnReader::SetTaskPool(pool);
            if (item_reader_ != nullptr) {
                item_reader_->SetTaskPool(pool);
            }
        }
    private:
        ::parquet::internal::LevelInfo level_info_;
        std::unique_ptr<ParquetColumnReader> item_reader_;
    };

    class StructReader : public ParquetColumnReader {
    public:
        explicit StructReader(std::shared_ptr<::parquet::arrow::ReaderContext> ctx,
                              std::shared_ptr<::arrow::Field> filtered_field,
                              ::parquet::internal::LevelInfo level_info,
                              std::vector<std::unique_ptr<ParquetColumnReader>> children)
                : ParquetColumnReader(std::move(ctx)), filtered_field_(std::move(filtered_field)),
                  level_info_(level_info),
                  children_(std::move(children)) {
            // There could be a mix of children some might be repeated some might not be.
            // If possible use one that isn't since that will be guaranteed to have the least
            // number of levels to reconstruct a nullable bitmap.
            auto result = std::find_if(children_.begin(), children_.end(),
                                       [](const std::unique_ptr<ParquetColumnReader>& child) {
                                           return !child->IsOrHasRepeatedChild();
                                       });
            if (result != children_.end()) {
                def_rep_level_child_ = result->get();
                has_repeated_child_ = false;
            } else if (!children_.empty()) {
                def_rep_level_child_ = children_.front().get();
                has_repeated_child_ = true;
            }
        }

        bool IsOrHasRepeatedChild() const final { return has_repeated_child_; }

        ::arrow::Status GetDefLevels(const int16_t **data, int64_t *length) override;

        ::arrow::Status GetRepLevels(const int16_t **data, int64_t *length) override;

        ::arrow::Status LoadBatch(int64_t records_to_read, BaseVector** out) override;

        const std::shared_ptr<Field> field() override { return filtered_field_; }

        void SetTaskPool(omniruntime::mem::OmniTaskMemoryPool *pool) override {
            ParquetColumnReader::SetTaskPool(pool);
            for (const auto& child : children_) {
                if (child != nullptr) {
                    child->SetTaskPool(pool);
                }
            }
        }

    private:
        const std::shared_ptr<::arrow::Field> filtered_field_;
        const ::parquet::internal::LevelInfo level_info_;
        const std::vector<std::unique_ptr<ParquetColumnReader>> children_;
        ParquetColumnReader* def_rep_level_child_ = nullptr;
        bool has_repeated_child_;
    };
}
#endif // OMNI_RUNTIME_COLUMN_READER_H