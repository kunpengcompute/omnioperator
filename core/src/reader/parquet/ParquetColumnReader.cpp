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

#include "ParquetColumnReader.h"

using namespace omniruntime::vec;

namespace omniruntime::reader {

Status ParquetColumnReader::NextBatch(int64_t batch_size, BaseVector** out)
{
    RETURN_NOT_OK(LoadBatch(batch_size, out));
    return Status::OK();
}

Status ParquetColumnReader::LoadBatch(int64_t records_to_read, BaseVector** out)
{
    BEGIN_PARQUET_CATCH_EXCEPTIONS
    record_reader_->Reset();
    record_reader_->Reserve(records_to_read);
    while (records_to_read > 0) {
        if (!record_reader_->HasMoreData()) {
            break;
        }
        int64_t records_read = record_reader_->ReadRecords(records_to_read);
        records_to_read -= records_read;
        if (records_read == 0) {
            NextRowGroup();
        }
    }

    *out = record_reader_->GetBaseVec();
    if (*out == nullptr) {
        return Status::Invalid("Parquet Read OmniVector is nullptr!");
    }
    return Status::OK();
    END_PARQUET_CATCH_EXCEPTIONS
}

void ParquetColumnReader::NextRowGroup()
{
    std::unique_ptr<parquet::PageReader> page_reader = input_->NextChunk();
    record_reader_->SetPageReader(std::move(page_reader));
}

::arrow::Status ParquetColumnReader::GetDefLevels(const int16_t **data, int64_t *length) {
    *data = record_reader_->def_levels();
    *length = record_reader_->levels_position();
    return Status::OK();
}

::arrow::Status ParquetColumnReader::GetRepLevels(const int16_t **data, int64_t *length) {
    *data = record_reader_->rep_levels();
    *length = record_reader_->levels_position();
    return Status::OK();
}

::arrow::Status ListReader::GetDefLevels(const int16_t **data, int64_t *length) {
    return item_reader_->GetDefLevels(data, length);
}

::arrow::Status ListReader::GetRepLevels(const int16_t **data, int64_t *length) {
    return item_reader_->GetRepLevels(data, length);
}

::arrow::Status ListReader::LoadBatch(int64_t records_to_read, BaseVector **out) {
    auto type_id = field_->type()->id();
    BaseVector *childOut;
    item_reader_->LoadBatch(records_to_read, &childOut);
    const int16_t *def_levels;
    const int16_t *rep_levels;
    int64_t num_levels;
    GetDefLevels(&def_levels, &num_levels);
    GetRepLevels(&rep_levels, &num_levels);
    std::shared_ptr<::arrow::ResizableBuffer> validity_buffer;
    ::parquet::internal::ValidityBitmapInputOutput validity_io;
    validity_io.values_read_upper_bound = records_to_read;
    if (field_->nullable()) {
        ARROW_ASSIGN_OR_RAISE(validity_buffer,
                              ::arrow::AllocateResizableBuffer(
                                      ::arrow::bit_util::BytesForBits(records_to_read), ctx_->pool));
        validity_io.valid_bits = validity_buffer->mutable_data();
    }
    ARROW_ASSIGN_OR_RAISE(
            std::shared_ptr < ::arrow::ResizableBuffer > offsets_buffer,
            ::arrow::AllocateResizableBuffer(
                    sizeof(int64_t) * std::max(int64_t{1}, records_to_read + 1),
                    ctx_->pool));
    int64_t *offset_data = reinterpret_cast<int64_t *>(offsets_buffer->mutable_data());
    offset_data[0] = 0;
    BaseVector *outVec;

    ::parquet::internal::DefRepLevelsToList(def_levels, rep_levels, num_levels,
                                            level_info_, &validity_io, offset_data);

    uint8_t *valid_bits = validity_io.valid_bits;
    int64_t null_count = validity_io.null_count;
    int64_t nRead = validity_io.values_read;
    int64_t nBytes = nRead >> 3;
    uint8_t *nulls;
    int64_t *offsets;
    if (type_id == ::arrow::Type::LIST) {
        std::unique_ptr <omniruntime::vec::ArrayVector> arrayVec = std::make_unique<omniruntime::vec::ArrayVector>(
                nRead, taskPool_);
        arrayVec->SetElementVectorFromRaw(childOut);
        offsets = arrayVec->GetOffsets();
        nulls = reinterpret_cast<uint8_t *>(arrayVec->GetNullsBuffer()->GetNulls());
        outVec = arrayVec.release();
    } else if (type_id == ::arrow::Type::MAP) {
        std::unique_ptr <omniruntime::vec::MapVector> mapVec =
            std::make_unique<omniruntime::vec::MapVector>(nRead, taskPool_);
        omniruntime::vec::RowVector *elementVec = reinterpret_cast<omniruntime::vec::RowVector *>(childOut);
        mapVec->SetKeyVector(elementVec->ChildAt(0));
        mapVec->SetValueVector(elementVec->ChildAt(1));
        delete elementVec;
        offsets = mapVec->GetOffsets();
        nulls = reinterpret_cast<uint8_t *>(mapVec->GetNullsBuffer()->GetNulls());
        outVec = mapVec.release();
    }
    std::copy(offset_data, offset_data + nRead + 1, offsets);
    if (null_count == nRead) {
        for (int64_t i = 0; i <= nBytes; ++i) {
            nulls[i] = 0xff;
        }
    } else if (valid_bits != nullptr) {
        valid_bits += validity_io.valid_bits_offset;
        for (int64_t i = 0; i <= nBytes; ++i) {
            nulls[i] = ~valid_bits[i];
        }
    }
    *out = outVec;
    return ::arrow::Status::OK();
}

::arrow::Status StructReader::GetDefLevels(const int16_t **data, int64_t *length) {
    *data = nullptr;
    if (children_.size() == 0) {
        *length = 0;
        return ::arrow::Status::Invalid("StructReader had no children");
    }
    // This method should only be called when this struct or one of its parents
    // are optional/repeated or it has a repeated child.
    // Meaning all children must have rep/def levels associated
    // with them.
    RETURN_NOT_OK(def_rep_level_child_->GetDefLevels(data, length));
    return ::arrow::Status::OK();
}

::arrow::Status StructReader::GetRepLevels(const int16_t **data, int64_t *length) {
    *data = nullptr;
    if (children_.size() == 0) {
        *length = 0;
        return ::arrow::Status::Invalid("StructReader had no children");
    }
    // This method should only be called when this struct or one of its parents
    // are optional/repeated or it has a repeated child.
    // Meaning all children must have rep/def levels associated
    // with them.
    RETURN_NOT_OK(def_rep_level_child_->GetRepLevels(data, length));
    return ::arrow::Status::OK();
}

::arrow::Status StructReader::LoadBatch(int64_t records_to_read, BaseVector **out) {
    BaseVector *outVec;
    std::vector < BaseVector * > childOuts;
    BaseVector *childOut;
    for (const std::unique_ptr <ParquetColumnReader> &reader: children_) {
        RETURN_NOT_OK(reader->LoadBatch(records_to_read, &childOut));
        childOuts.push_back(childOut);
    }

    std::shared_ptr<::arrow::ResizableBuffer> null_bitmap;
    ::parquet::internal::ValidityBitmapInputOutput validity_io;
    validity_io.values_read_upper_bound = records_to_read;
    // This simplifies accounting below.
    validity_io.values_read = records_to_read;


    const int16_t *def_levels;
    const int16_t *rep_levels;
    int64_t num_levels;
    if (has_repeated_child_) {
        ARROW_ASSIGN_OR_RAISE(
                null_bitmap,
                ::arrow::AllocateResizableBuffer(::arrow::bit_util::BytesForBits(records_to_read), ctx_->pool));
        validity_io.valid_bits = null_bitmap->mutable_data();
        RETURN_NOT_OK(GetDefLevels(&def_levels, &num_levels));
        RETURN_NOT_OK(GetRepLevels(&rep_levels, &num_levels));
        DefRepLevelsToBitmap(def_levels, rep_levels, num_levels, level_info_, &validity_io);
    } else if (filtered_field_->nullable()) {
        ARROW_ASSIGN_OR_RAISE(
                null_bitmap,
                ::arrow::AllocateResizableBuffer(::arrow::bit_util::BytesForBits(records_to_read), ctx_->pool));
        validity_io.valid_bits = null_bitmap->mutable_data();
        RETURN_NOT_OK(GetDefLevels(&def_levels, &num_levels));
        DefLevelsToBitmap(def_levels, num_levels, level_info_, &validity_io);
    }

    // Ensure all values are initialized.
    if (null_bitmap) {
        RETURN_NOT_OK(null_bitmap->Resize(::arrow::bit_util::BytesForBits(validity_io.values_read)));
        null_bitmap->ZeroPadding();
    }

    int64_t nRead = validity_io.values_read;
    // Gather children arrays and def levels
    std::unique_ptr <omniruntime::vec::RowVector> rowVec =
        std::make_unique<omniruntime::vec::RowVector>(nRead, taskPool_);
    for (auto childVec: childOuts) {
        rowVec->AddChild(childVec);
    }

    uint8_t *valid_bits = validity_io.valid_bits;
    int64_t null_count = validity_io.null_count;
    int64_t nBytes = nRead >> 3;
    uint8_t *nulls = reinterpret_cast<uint8_t *>(rowVec->GetNullsBuffer()->GetNulls());
    if (nRead == null_count) {
        for (int64_t i = 0; i <= nBytes; ++i) {
            nulls[i] = 0xff;
        }
    } else if (valid_bits != nullptr) {
        valid_bits += validity_io.valid_bits_offset;
        for (int64_t i = 0; i <= nBytes; ++i) {
            nulls[i] = ~valid_bits[i];
        }
    }
    outVec = rowVec.release();
    *out = outVec;
    return ::arrow::Status::OK();
}
}