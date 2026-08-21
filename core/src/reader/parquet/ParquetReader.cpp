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

#include <algorithm>
#include <unordered_map>
#include "ParquetReader.h"
#include "reader/common/UriInfo.h"
#include "reader/arrowadapter/FileSystemAdapter.h"
#include "arrow/dataset/file_parquet.h"
#include "mutex"
#include "reader/common/PredicateCondition.h"

using namespace arrow;
using namespace arrow::internal;
using namespace parquet::arrow;
using namespace omniruntime::reader;
using namespace omniruntime::type;

static std::mutex mutex_;
static std::mutex map_mutex;
static std::unordered_map<std::string, Filesystem*> restore_filesysptr;
static constexpr int32_t LOCAL_FILE_PREFIX = 5;
static constexpr int32_t LOCAL_FILE_PREFIX_EXT = 7;
static const std::string LOCAL_FILE = "file:";
static const std::string HDFS_FILE = "hdfs:";
static const std::string SPARK_LIST_FIELD_SUFFIX = ".list.element";
static const std::string SPARK_MAP_FIELD_SUFFIX = ".key_value.key";
static const std::string HIVE_LIST_FIELD_SUFFIX = ".bag.array_element";
static const std::string HIVE_MAP_FIELD_SUFFIX = ".map.key";

namespace
{

void ClearRecordBatch(std::vector<BaseVector*>& recordBatch)
{
    for (auto vec : recordBatch)
    {
        delete vec;
    }
    recordBatch.clear();
}

uint64_t FilterData(uint8_t *bitMark, std::vector<BaseVector*> *recordBatch, int32_t vectorSize,
                    const std::set<int32_t>& isNullSet, const std::set<int32_t>& isNotNullSet)
{
    std::vector<BaseVector*> resultBatch;
    if (common::GetFlatBaseVectorsFromBitMark(*recordBatch, resultBatch, bitMark, vectorSize, isNullSet, isNotNullSet)) {
        ClearRecordBatch(*recordBatch);
        *recordBatch = std::move(resultBatch);
        return (*recordBatch)[0]->GetSize();
    }
    ClearRecordBatch(resultBatch);
    return vectorSize;
}

bool ReadAndFilterData(ParquetRowReader& rowReaderPtr,
                       std::vector<BaseVector*> *recordBatch, uint64_t &batchRowSize, int *omniTypeId, uint64_t batchLen)
{
    batchRowSize = rowReaderPtr.NextDirect(recordBatch, omniTypeId, batchLen);
    std::shared_ptr<common::PredicateCondition>& predicateCondition = rowReaderPtr.GetPredicatePtr();
    if (batchRowSize == 0 || predicateCondition == nullptr) {
        return false;
    }
    predicateCondition->ClearSurvivingPositions();
    try {
        auto predicateResult = predicateCondition->compute(*recordBatch);
        // WHERE keeps TRUE only; FALSE and UNKNOWN are both rejected.
        uint8_t *bitMark = predicateResult.trueBits;
        int32_t vectorSize = (*recordBatch)[0]->GetSize();
        if (omniruntime::BitUtil::CountBits(reinterpret_cast<const uint64_t *>(bitMark), 0, vectorSize) == 0) {
            ClearRecordBatch(*recordBatch);
            return true;
        }
        // Capture file-absolute positions of surviving rows before FilterData compacts
        // the batch, so the connector can build a correct row_index vector afterwards.
        uint64_t batchStart = rowReaderPtr.LastReadRowPosition();
        std::vector<int64_t> positions;
        positions.reserve(vectorSize);
        for (int32_t i = 0; i < vectorSize; ++i) {
            if (omniruntime::BitUtil::IsBitSet(reinterpret_cast<const uint64_t *>(bitMark), i)) {
                positions.push_back(static_cast<int64_t>(batchStart) + i);
            }
        }
        batchRowSize = FilterData(bitMark, recordBatch, vectorSize, predicateCondition->getIsAllNullColumns(),
                                  predicateCondition->getIsAllNotNullColumns());
        predicateCondition->SetSurvivingPositions(std::move(positions));
    } catch (const std::exception &e) {
        LogError("filterData fail: %s", e.what());
    }
    return false;
}

bool ContainField(const SchemaField &field, std::shared_ptr<std::unordered_set<int>> &included_leaves)
{
    int colIndex = field.column_index;
    if (field.column_index != -1) {
        return included_leaves->find(colIndex) != included_leaves->end();
    } else {
        for (const auto &child : field.children) {
            if(ContainField(child, included_leaves)) {
                return true;
            }
        }
        return false;
    }
}

void CollectLeafColumnIndices(const SchemaField& field, std::vector<int>& out)
{
    if (field.column_index != -1) {
        out.push_back(field.column_index);
        return;
    }
    for (const auto& child : field.children) {
        CollectLeafColumnIndices(child, out);
    }
}

bool TryCollectColumnIndicesByFieldPath(
    const SchemaField& field,
    const std::vector<std::string>& path,
    size_t pathIndex,
    std::vector<int>& out)
{
    if (field.field == nullptr || pathIndex >= path.size() || field.field->name() != path[pathIndex]) {
        return false;
    }
    if (pathIndex + 1 == path.size()) {
        CollectLeafColumnIndices(field, out);
        return !out.empty();
    }
    for (const auto& child : field.children) {
        if (TryCollectColumnIndicesByFieldPath(child, path, pathIndex + 1, out)) {
            return true;
        }
    }
    return false;
}

bool TryCollectColumnIndicesFromComplexField(
    const std::vector<SchemaField>& schemaFields,
    const std::string& fieldName,
    std::vector<int>& out)
{
    std::vector<std::string> path;
    size_t start = 0;
    while (start <= fieldName.size()) {
        size_t dot = fieldName.find('.', start);
        path.push_back(fieldName.substr(start, dot == std::string::npos ? dot : dot - start));
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }

    if (path.empty()) {
        return false;
    }

    for (const auto& field : schemaFields) {
        if (TryCollectColumnIndicesByFieldPath(field, path, 0, out)) {
            return true;
        }
    }
    return false;
}
}

ParquetRowReader::ParquetRowReader(ParquetReader &parquetReader) : parquetReader_(parquetReader) {
    this->predicatePtr = parquetReader.GetOptions()->GetPredicatePtr();
    this->rowType_ = parquetReader.GetOptions()->GetRowType();
    this->fileRowType_ = parquetReader.GetOptions()->GetFileRowType();
    this->julianPtr = parquetReader.GetOptions()->GetJulianPtr();
}

uint64_t ParquetRowReader::Next(std::vector<BaseVector *> **batch, int *omniTypeId, uint64_t batchLen)
{
    auto recordBatch = new std::vector<omniruntime::vec::BaseVector *>(parquetReader_.columnReaders.size(), nullptr);
    uint64_t batchRowSize = 0;
    bool needReadAgain = ReadAndFilterData(*this, recordBatch, batchRowSize, omniTypeId, batchLen);
    while (needReadAgain) {
        ClearRecordBatch(*recordBatch);
        recordBatch->assign(parquetReader_.columnReaders.size(), nullptr);
        needReadAgain = ReadAndFilterData(*this, recordBatch, batchRowSize, omniTypeId, batchLen);
    }

    *batch = recordBatch;
    if (batchRowSize <= 0) {
        ClearRecordBatch(*recordBatch);
        return 0;
    }

    return batchRowSize;
}

uint64_t ParquetRowReader::NextDirect(std::vector<BaseVector *> *batch, int *omniTypeId, uint64_t batchLen)
{
    long batchRowSize = 0;
    auto state = parquetReader_.ReadNextBatch(*batch, &batchRowSize);
    if (!state.ok()) {
        ClearRecordBatch(*batch);
        throw OmniException(state.ToString().c_str());
        return 0;
    }
    return static_cast<uint64_t>(batchRowSize);
}

uint64_t ParquetRowReader::LastReadRowPosition() const
{
    return parquetReader_.LastReadRowPosition();
}

// the ugi is UserGroupInformation
std::string omniruntime::reader::GetFileSystemKey(std::string& path, std::string& ugi)
{
    // if the local file, all the files are the same key "file:"
    std::string result = ugi;

    // if the hdfs file, only get the ip and port just like the ugi + ip + port as key
    if (path.substr(0, LOCAL_FILE_PREFIX) == HDFS_FILE) {
        auto end = path.find("/", LOCAL_FILE_PREFIX_EXT);
        std::string ip_and_port = path.substr(LOCAL_FILE_PREFIX_EXT, end - LOCAL_FILE_PREFIX_EXT);
        result += ip_and_port;
        return result;
    }

    // if the local file, get the ugi + "file" as the key
    if (path.substr(0, LOCAL_FILE_PREFIX) == LOCAL_FILE) {
        // process the path "file://" head, the arrow could not read the head
        path = path.substr(LOCAL_FILE_PREFIX);
        result += "file:";
        return result;
    }

    // if not the local, not the hdfs, get the ugi + path as the key
    result += path;
    return result;
}

Filesystem* omniruntime::reader::GetFileSystemPtr(UriInfo &uri, std::string& ugi, arrow::Status &status)
{
    std::string fullPath = uri.ToString();
    auto key = GetFileSystemKey(fullPath, ugi);

    // if not find key, create the filesystem ptr
    std::lock_guard<std::mutex> lock(map_mutex);
    auto iter = restore_filesysptr.find(key);
    if (iter == restore_filesysptr.end()) {
        Filesystem* fs = new Filesystem();
        auto result = arrow_adapter::FileSystemFromUriOrPath(uri);
        status = result.status();
        if (!status.ok()) {
          delete fs;
          return nullptr;
        }
        fs->filesys_ptr = std::move(result).ValueUnsafe();
        restore_filesysptr[key] = fs;
    }

    return restore_filesysptr[key];
}

Status ParquetReader::InitReader(UriInfo &uri, int64_t capacity, std::string& ugi)
{
    // Configure reader settings
    auto reader_properties = parquet::ReaderProperties(pool);

    // Configure Arrow-specific reader settings
    auto arrow_reader_properties = parquet::ArrowReaderProperties();
    arrow_reader_properties.set_batch_size(capacity);

    // Get the file from filesystem
    Status result;
    mutex_.lock();
    Filesystem* fs = GetFileSystemPtr(uri, ugi, result);
    mutex_.unlock();
    if (fs == nullptr || fs->filesys_ptr == nullptr) {
        return Status::IOError(result);
    }
    std::string path = uri.ToString();
    ARROW_ASSIGN_OR_RAISE(file, fs->filesys_ptr->OpenInputFile(path));

    FileReaderBuilder reader_builder;
    ARROW_RETURN_NOT_OK(reader_builder.Open(file, reader_properties));
    reader_builder.memory_pool(pool);
    reader_builder.properties(arrow_reader_properties);

    ARROW_ASSIGN_OR_RAISE(arrow_reader, reader_builder.Build());
    return arrow::Status::OK();
}

Status ParquetReader::InitRecordReader(int64_t start, int64_t end, bool hasExpressionTree,
    Expression pushedFilterArray, const std::vector<std::string>& fieldNames)
{
    std::vector<int> row_group_indices;
    auto filesource = std::make_shared<dataset::FileSource>(file);
    ARROW_RETURN_NOT_OK(GetRowGroupIndices(*filesource, start, end, hasExpressionTree, pushedFilterArray, row_group_indices));
    std::vector<int> column_indices;
    ARROW_RETURN_NOT_OK(GetColumnIndices(fieldNames, column_indices));
    ARROW_RETURN_NOT_OK(GetRecordBatchReader(row_group_indices, column_indices));
    return arrow::Status::OK();
}

Status ParquetReader::GetRowGroupIndices(dataset::FileSource filesource, int64_t start, int64_t end,
    bool hasExpressionTree, Expression pushedFilterArray, std::vector<int> &out)
{
    auto metadata = arrow_reader->parquet_reader()->metadata();
    std::vector<int> groups;
    for (int i = 0; i < metadata->num_row_groups(); i++) {
        auto startIndex = metadata->RowGroup(i)->file_offset();
        if (startIndex >= start && startIndex < end) {
            groups.push_back(i);
        }
    }
    if (!hasExpressionTree) {
        out = groups;
        return arrow::Status::OK();
    }
    auto parquet_format = std::make_shared<arrow::dataset::ParquetFileFormat>();
    std::shared_ptr<Schema> schema;
    ARROW_RETURN_NOT_OK(arrow_reader->GetSchema(&schema));
    ARROW_ASSIGN_OR_RAISE(auto parquetFileFragment, parquet_format->MakeFragment(filesource, literal(true), schema, groups));
    ARROW_ASSIGN_OR_RAISE(auto finalExpr, pushedFilterArray.Bind(*schema));
    ARROW_RETURN_NOT_OK(parquetFileFragment->EnsureCompleteMetadata(arrow_reader.get()));
    ARROW_ASSIGN_OR_RAISE(auto readFragment, parquetFileFragment->Subset(finalExpr));
    auto result = static_cast<arrow::dataset::ParquetFileFragment*>(readFragment.get())->row_groups();
    out = result;
    return Status::OK();
}

Status ParquetReader::GetColumnIndices(const std::vector<std::string>& fieldNames, std::vector<int> &out)
{
    auto schema = arrow_reader->parquet_reader()->metadata()->schema();
    const auto& schemaFields = arrow_reader->manifest().schema_fields;
    auto appendColumnIndex = [&out](int index) {
        if (std::find(out.begin(), out.end(), index) == out.end()) {
            out.push_back(index);
        }
    };

    int index = 0;
    for (const auto& fieldName : fieldNames) {
        if ((index = schema->ColumnIndex(fieldName)) != -1) {
            appendColumnIndex(index);
        } else if ((index = schema->ColumnIndex(fieldName + SPARK_LIST_FIELD_SUFFIX)) != -1 ||
            (index = schema->ColumnIndex(fieldName + HIVE_LIST_FIELD_SUFFIX)) != -1) {
            appendColumnIndex(index);
        } else if ((index = schema->ColumnIndex(fieldName + SPARK_MAP_FIELD_SUFFIX)) != -1 ||
            (index = schema->ColumnIndex(fieldName + HIVE_MAP_FIELD_SUFFIX)) != -1) {
            appendColumnIndex(index);
            appendColumnIndex(index + 1);
        } else {
            std::vector<int> nestedIndices;
            if (TryCollectColumnIndicesFromComplexField(schemaFields, fieldName, nestedIndices)) {
                for (int nestedIndex : nestedIndices) {
                    appendColumnIndex(nestedIndex);
                }
                continue;
            }
            return Status::Invalid("Field not found in schema: " + fieldName);
        }
    }
    return Status::OK();
}

Status ParquetReader::ReadNextBatch(std::vector<omniruntime::vec::BaseVector*> &batch, long *batchRowSize)
{
    lastReadRowPosition_ = nextRowPosition_;
    ARROW_RETURN_NOT_OK(rb_reader->ReadNext(batch, batchRowSize));
    if (*batchRowSize > 0) {
        nextRowPosition_ += static_cast<uint64_t>(*batchRowSize);
    }
    return arrow::Status::OK();
}

Status ParquetReader::GetRecordBatchReader(const std::vector<int> &row_group_indices,
    const std::vector<int> &column_indices)
{
    std::shared_ptr<::arrow::Schema> batch_schema;
    RETURN_NOT_OK(GetFieldReaders(row_group_indices, column_indices, &columnReaders, &batch_schema));

    auto metadata = arrow_reader->parquet_reader()->metadata();
    nextRowPosition_ = 0;
    lastReadRowPosition_ = 0;
    if (!row_group_indices.empty()) {
        auto firstRowGroup = *std::min_element(row_group_indices.begin(), row_group_indices.end());
        for (int i = 0; i < firstRowGroup; i++) {
            nextRowPosition_ += static_cast<uint64_t>(metadata->RowGroup(i)->num_rows());
        }
        lastReadRowPosition_ = nextRowPosition_;
    }

    int64_t num_rows = 0;
    for(int row_group : row_group_indices) {
        num_rows += metadata->RowGroup(row_group)->num_rows();
    }
    // Use lambda function to generate BaseVectors
    auto batches = [num_rows, this](std::vector<omniruntime::vec::BaseVector*> &batch,
        long *batchRowSize) mutable -> Status {
            int64_t read_size = std::min(arrow_reader->properties().batch_size(), num_rows);
            num_rows -= read_size;
            *batchRowSize = read_size;

            if (columnReaders.empty() || read_size <= 0) {
                return Status::OK();
            }

            try {
                for (uint64_t i = 0; i < columnReaders.size(); i++) {
                    RETURN_NOT_OK(columnReaders[i]->NextBatch(read_size, &batch[i]));
                }
            } catch (const std::exception &e) {
                return Status::Invalid(e.what());
            }

            // Check BaseVector
            for (const auto& column : batch) {
                if (column == nullptr) {
                    return Status::Invalid("BaseVector should not be nullptr after reading");
                }
            }

            return Status::OK();
    };

    // todo zhangxin
    rb_reader = std::make_unique<OmniRecordBatchReader>(std::move(batches));
    return Status::OK();
}

std::shared_ptr<std::unordered_set<int>> VectorToSharedSet(const std::vector<int> &values) {
    std::shared_ptr<std::unordered_set<int>> result(new std::unordered_set<int>());
    result->insert(values.begin(), values.end());
    return result;
}

Status ParquetReader::GetFieldReaders(const std::vector<int> &row_group_indices, const std::vector<int> &column_indices,
    std::vector<std::shared_ptr<ParquetColumnReader>>* out, std::shared_ptr<::arrow::Schema>* out_schema)
{
    // We only read schema fields which have columns indicated in the indices vector
    ARROW_ASSIGN_OR_RAISE(std::vector<int> field_indices, arrow_reader->manifest().GetFieldIndices(column_indices));
    auto included_leaves = VectorToSharedSet(column_indices);
    out->resize(field_indices.size());
    ::arrow::FieldVector out_fields(field_indices.size());

    for (size_t i = 0; i < out->size(); i++) {
        std::unique_ptr<ParquetColumnReader> reader;
        RETURN_NOT_OK(GetFieldReader(field_indices[i], included_leaves, row_group_indices, &reader));
        out_fields[i] = reader->field();
        out->at(i) = std::move(reader);
    }

    *out_schema = ::arrow::schema(std::move(out_fields), arrow_reader->manifest().schema_metadata);
    return Status::OK();
}

FileColumnIteratorFactory SomeRowGroupsFactory(std::vector<int> row_group_indices) {
    return [row_group_indices] (int i, parquet::ParquetFileReader* reader) {
        return new FileColumnIterator(i, reader, row_group_indices);
    };
}

Status ParquetReader::GetFieldReader(int i, const std::shared_ptr<std::unordered_set<int>>& included_leaves,
    const std::vector<int> &row_group_indices, std::unique_ptr<ParquetColumnReader>* out)
{
    if (ARROW_PREDICT_FALSE(i < 0 || static_cast<size_t>(i) >= arrow_reader->manifest().schema_fields.size())) {
        return Status::Invalid("Column index out of bounds (got ", i,
            ", should be between 0 and ", arrow_reader->manifest().schema_fields.size(), ")");
    }
    auto ctx = std::make_shared<ReaderContext>();
    ctx->reader = arrow_reader->parquet_reader();
    ctx->pool = pool;
    ctx->iterator_factory = SomeRowGroupsFactory(row_group_indices);
    ctx->filter_leaves = true;
    ctx->included_leaves = included_leaves;
    auto field = arrow_reader->manifest().schema_fields[i];
    return GetReader(field, field.field, ctx, out);
}

Status ParquetReader::GetReader(const SchemaField &field, const std::shared_ptr<Field> &arrow_field,
                                const std::shared_ptr<ReaderContext> &ctx, std::unique_ptr<ParquetColumnReader> *out)
{
    BEGIN_PARQUET_CATCH_EXCEPTIONS

    if (arrow_field == nullptr) {
        return Status::Invalid("Parquet schema field is nullptr");
    }

    auto type_id = arrow_field->type()->id();

    if (type_id == ::arrow::Type::EXTENSION) {
        return Status::Invalid("Unsupported type: ", arrow_field->ToString());
    }

    if (field.children.size() == 0) {
        if (!field.is_leaf()) {
            return Status::Invalid("Parquet non-leaf node has no children");
        }
        std::unique_ptr<FileColumnIterator> input(ctx->iterator_factory(field.column_index, ctx->reader));
        *out = std::make_unique<ParquetColumnReader>(ctx, arrow_field, std::move(input), field.level_info,
                                                     rebaseInfoPtr.get());
    } else if (type_id == ::arrow::Type::LIST || type_id == ::arrow::Type::MAP ||
               type_id == ::arrow::Type::FIXED_SIZE_LIST ||
               type_id == ::arrow::Type::LARGE_LIST) {
        auto list_field = arrow_field;
        auto child = &field.children[0];
        if (child->field == nullptr) {
            return Status::Invalid("Parquet list/map child field is nullptr");
        }
        std::unique_ptr<ParquetColumnReader> child_reader;
        RETURN_NOT_OK(GetReader(*child, child->field, ctx, &child_reader));
        if (child_reader == nullptr) {
            *out = nullptr;
            return Status::OK();
        }
        if (child_reader->field() == nullptr) {
            return Status::Invalid("Parquet child reader field is nullptr");
        }

        // These two types might not be equal if there column pruning occurred.
        // further down the stack.
        const std::shared_ptr<::arrow::DataType> reader_child_type = child_reader->field()->type();
        // This should really never happen but was raised as a question on the code
        // review, this should  be pretty cheap check so leave it in.
        if (ARROW_PREDICT_FALSE(list_field->type()->num_fields() != 1)) {
            return Status::Invalid("expected exactly one child field for: ",
                                   list_field->ToString());
        }
        const ::arrow::DataType& schema_child_type = *(list_field->type()->field(0)->type());
        if (type_id == ::arrow::Type::MAP) {
            list_field = list_field->WithType(std::make_shared<::arrow::MapType>(
                    field.children[0].field->type()->field(0),
                    field.children[0].field->type()->field(1)));
            *out = std::make_unique<ListReader>(ctx, list_field, field.level_info,
                                                std::move(child_reader));
        } else if (type_id == ::arrow::Type::LIST) {
            if (!reader_child_type->Equals(schema_child_type)) {
                list_field = list_field->WithType(::arrow::list(reader_child_type));
            }

            *out = std::make_unique<ListReader>(ctx, list_field, field.level_info,
                                                std::move(child_reader));
        } else if (type_id == ::arrow::Type::LARGE_LIST) {
            if (!reader_child_type->Equals(schema_child_type)) {
                list_field = list_field->WithType(::arrow::large_list(reader_child_type));
            }

            *out = std::make_unique<ListReader>(ctx, list_field, field.level_info,
                                                std::move(child_reader));
        } else if (type_id == ::arrow::Type::FIXED_SIZE_LIST) {
            if (!reader_child_type->Equals(schema_child_type)) {
                auto& fixed_list_type =
                        checked_cast<const ::arrow::FixedSizeListType&>(*list_field->type());
                int32_t list_size = fixed_list_type.list_size();
                list_field =
                        list_field->WithType(::arrow::fixed_size_list(reader_child_type, list_size));
            }

            *out = std::make_unique<ListReader>(ctx, list_field, field.level_info,
                                                std::move(child_reader));
        } else {
            return Status::UnknownError("Unknown list type: ", field.field->ToString());
        }
    } else if (type_id == ::arrow::Type::STRUCT) {
        std::vector<std::shared_ptr<Field>> child_fields;
        int arrow_field_idx = 0;
        std::vector<std::unique_ptr<ParquetColumnReader>> child_readers;
        for (const auto& child : field.children) {
            if (!ContainField(child, ctx->included_leaves)) {
                continue;
            }
            std::unique_ptr<ParquetColumnReader> child_reader;
            RETURN_NOT_OK(GetReader(child, child.field , ctx, &child_reader));
            if (!child_reader) {
                arrow_field_idx++;
                // If all children were pruned, then we do not try to read this field
                continue;
            }
            std::shared_ptr<::arrow::Field> child_field = child.field;
            const ::arrow::DataType& reader_child_type = *child_reader->field()->type();
            const ::arrow::DataType& schema_child_type =
                    *arrow_field->type()->field(arrow_field_idx++)->type();
            // These might not be equal if column pruning occurred.
            if (!schema_child_type.Equals(reader_child_type)) {
                child_field = child_field->WithType(child_reader->field()->type());
            }
            child_fields.push_back(child_field);
            child_readers.emplace_back(std::move(child_reader));
        }
        if (child_fields.empty()) {
            *out = nullptr;
            return Status::OK();
        }
        auto filtered_field =
                ::arrow::field(arrow_field->name(), ::arrow::struct_(child_fields),
                               arrow_field->nullable(), arrow_field->metadata());
        *out = std::make_unique<StructReader>(ctx, filtered_field, field.level_info,
                                              std::move(child_readers));
    } else {
        return Status::Invalid("Unsupported type: ", arrow_field->ToString());
    }
    return Status::OK();

    END_PARQUET_CATCH_EXCEPTIONS
}
