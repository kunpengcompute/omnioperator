/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 */
#include "reader/text/TextReader.h"

#include <arrow/result.h>

#include <stdexcept>
#include <algorithm>
#include <utility>

#include "reader/arrowadapter/FileSystemAdapter.h"
#include "reader/text/TextCompressionStream.h"
#include "util/omni_exception.h"
#include "vector/unsafe_vector.h"
#include "vector/vector_helper.h"

namespace omniruntime::reader::text {

using omniruntime::exception::OmniException;

namespace {

// CSV's scalar lexical rules differ from SQL CAST. Keep normalization here,
// before the shared column conversion, and never apply it to LazySimple.
bool NormalizeCsvValue(std::string_view& value, type::DataTypeId typeId, std::string& scratch)
{
    if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR) {
        return true;
    }
    if (typeId == type::OMNI_BOOLEAN) {
        auto matches = [&](std::string_view expected) {
            return value.size() == expected.size() &&
                std::equal(value.begin(), value.end(), expected.begin(),
                    [](unsigned char left, char right) { return (left | 0x20) == right; });
        };
        return matches("true") || matches("false");
    }
    const bool integer = typeId == type::OMNI_BYTE || typeId == type::OMNI_SHORT ||
        typeId == type::OMNI_INT || typeId == type::OMNI_LONG;
    const bool decimal = typeId == type::OMNI_DECIMAL64 || typeId == type::OMNI_DECIMAL128;
    if (decimal && value.find(',') != std::string_view::npos) {
        scratch.clear();
        for (const char byte : value) {
            if (byte != ',') {
                scratch.push_back(byte);
            }
        }
        value = scratch;
    }
    if (integer || decimal) {
        if (value.empty()) {
            return false;
        }
        size_t position = value.front() == '+' || value.front() == '-' ? 1 : 0;
        bool digits = false;
        bool dot = false;
        for (; position < value.size(); ++position) {
            const char byte = value[position];
            if (byte >= '0' && byte <= '9') {
                digits = true;
            } else if (decimal && byte == '.' && !dot) {
                dot = true;
            } else if (decimal && (byte == 'e' || byte == 'E') && digits) {
                ++position;
                if (position < value.size() && (value[position] == '+' || value[position] == '-')) {
                    ++position;
                }
                return position < value.size() &&
                    std::all_of(value.begin() + position, value.end(),
                        [](char digit) { return digit >= '0' && digit <= '9'; });
            } else {
                return false;
            }
        }
        return digits;
    }
    return true;
}

} // namespace

TextReader::TextReader(std::shared_ptr<ReaderOptions> options, TextFormatOptions textOptions)
    : textOptions_(std::move(textOptions))
{
    options_ = std::move(options);
}

void TextReader::InitReader()
{
    auto uri = options_->GetUri();
    if (uri == nullptr) {
        throw std::runtime_error("Text reader URI is null.");
    }
    std::string fileSystemPath;
    auto fileSystemResult = arrow_adapter::FileSystemFromUriOrPath(*uri, &fileSystemPath);
    if (!fileSystemResult.ok()) {
        throw OmniException(fileSystemResult.status().ToString().c_str());
    }
    fileSystem_ = std::move(fileSystemResult).ValueUnsafe();
    auto fileResult = fileSystem_->OpenInputFile(fileSystemPath);
    if (!fileResult.ok()) {
        throw OmniException(fileResult.status().ToString().c_str());
    }
    file_ = std::move(fileResult).ValueUnsafe();
    auto sizeResult = file_->GetSize();
    if (!sizeResult.ok()) {
        throw OmniException(sizeResult.status().ToString().c_str());
    }
    fileSize_ = std::move(sizeResult).ValueUnsafe();
}

std::unique_ptr<RowReader> TextReader::CreateRowReader()
{
    return std::make_unique<TextRowReader>(*this);
}

const std::shared_ptr<arrow::io::RandomAccessFile>& TextReader::GetFile() const
{
    return file_;
}

int64_t TextReader::GetFileSize() const
{
    return fileSize_;
}

const TextFormatOptions& TextReader::GetTextOptions() const
{
    return textOptions_;
}

const std::shared_ptr<ReaderOptions>& TextReader::GetOptions() const
{
    return options_;
}

TextRowReader::TextRowReader(TextReader& reader)
    : reader_(reader)
{
    options_ = reader.GetOptions();
    rowType_ = options_->GetRowType();
    fileRowType_ = options_->GetFileRowType();
    auto splitStart = options_->GetSplitStart();
    const auto& format = reader.GetTextOptions();
    if (format.sourceKind == TextSourceKind::SPARK_CSV) {
        csvComment_ = format.Csv().comment;
    }
    if (format.Compression() == TextCompressionKind::NONE) {
        lineScanner_ = std::make_unique<TextLineScanner>(
            reader.GetFile(),
            reader.GetFileSize(),
            splitStart,
            options_->GetSplitEnd(), DEFAULT_TEXT_READ_BUFFER_SIZE,
            format.IsCsv());
    } else {
        if (splitStart != 0 || options_->GetSplitEnd() < reader.GetFileSize()) {
            throw std::runtime_error("Compressed Text reader requires a whole-file split.");
        }
        sequentialLineScanner_ = std::make_unique<SequentialTextLineScanner>(
            CreateTextSequentialInput(reader.GetFile(), format.Compression()),
            DEFAULT_TEXT_READ_BUFFER_SIZE, format.IsCsv());
    }
    uint32_t skipInputLines = 0;
    if (format.IsCsv()) {
        skipInputLines = format.Csv().delimited.skipInputLines;
    } else if (format.IsLazySimple()) {
        skipInputLines = format.LazySimple().delimited.skipInputLines;
    }
    if (splitStart == 0 && skipInputLines != 0) {
        // Spark CSV selects its header from non-empty, non-comment records. Hive
        // skip.header.line.count counts physical records, including empty records.
        if (format.sourceKind == TextSourceKind::SPARK_CSV) {
            CountSparkCsvRows(skipInputLines);
        } else {
            CountRows(skipInputLines, false);
        }
    }
    if (rowType_ == nullptr) {
        throw std::runtime_error("Text reader projected row type is missing.");
    }
    if ((format.IsLazySimple() || format.IsCsv()) && rowType_->size() > 0) {
        if (fileRowType_ == nullptr) {
            throw std::runtime_error("LazySimple reader requires projected and full file schemas.");
        }
        valueConverter_ = std::make_unique<TextValueConverter>(
            reader.GetTextOptions().common.sessionTimezone);
        std::vector<int32_t> projectedFieldIndices;
        projectedFieldIndices.reserve(rowType_->size());
        for (const auto& name : rowType_->names()) {
            const auto index = fileRowType_->getChildIdxIfExists(name);
            if (!index.has_value()) {
                throw std::runtime_error("LazySimple projected column is missing from the file schema: " + name);
            }
            projectedFieldIndices.push_back(static_cast<int32_t>(*index));
        }
        codec_ = CreateTextCodec(
            reader.GetTextOptions(), projectedFieldIndices,
            static_cast<size_t>(fileRowType_->size()));
    } else if (rowType_->size() > 0) {
        codec_ = CreateTextCodec(reader.GetTextOptions());
    }
}

bool TextRowReader::NextLine(std::string_view& line)
{
    return lineScanner_ != nullptr
        ? lineScanner_->NextLine(line)
        : sequentialLineScanner_->NextLine(line);
}

uint64_t TextRowReader::CountRows(uint64_t maxRows, bool skipBlankLines)
{
    return lineScanner_ != nullptr
        ? lineScanner_->CountRows(maxRows, skipBlankLines)
        : sequentialLineScanner_->CountRows(maxRows, skipBlankLines);
}

bool TextRowReader::IsIgnoredSparkCsvRecord(std::string_view record) const
{
    const bool blank = std::all_of(record.begin(), record.end(),
        [](unsigned char byte) { return byte <= ' '; });
    return blank || (csvComment_ != '\0' && !record.empty() && record.front() == csvComment_);
}

uint64_t TextRowReader::CountSparkCsvRows(uint64_t maxRows)
{
    if (csvComment_ == '\0') {
        return CountRows(maxRows, true);
    }
    uint64_t rows = 0;
    std::string_view record;
    while (rows < maxRows && NextLine(record)) {
        if (!IsIgnoredSparkCsvRecord(record)) {
            ++rows;
        }
    }
    return rows;
}

uint64_t TextRowReader::Next(uint64_t, vec::VectorPtr&)
{
    return 0;
}

uint64_t TextRowReader::NextDirect(
    std::vector<BaseVector*>* batch, int*, uint64_t batchLen)
{
    if (batch == nullptr || batchLen == 0) {
        return 0;
    }
    const auto& textOptions = reader_.GetTextOptions();
    if (rowType_->size() == 0) {
        return textOptions.sourceKind == TextSourceKind::SPARK_CSV
            ? CountSparkCsvRows(batchLen)
            : CountRows(batchLen, false);
    }
    if (textOptions.IsRawLine()) {
        if (rowType_->size() > 1) {
            throw std::runtime_error("RawLine reader requires zero or one projected file column.");
        }
        if (rowType_->size() == 1) {
            auto typeId = rowType_->childAt(0)->GetId();
            if (typeId != type::OMNI_VARCHAR && typeId != type::OMNI_CHAR) {
                throw std::runtime_error("RawLine reader projected column must be String.");
            }
        }
    }

    if (textOptions.IsRawLine() && rowType_->size() == 1) {
        std::vector<std::string> records;
        records.reserve(batchLen);
        while (records.size() < batchLen) {
            std::string_view record;
            if (!NextLine(record)) {
                break;
            }
            records.emplace_back(record);
        }
        if (records.empty()) {
            return 0;
        }
        std::unique_ptr<vec::BaseVector> outputBase(
            vec::VectorHelper::CreateStringVector(options_->GetTaskPool(), records.size()));
        auto* output = reinterpret_cast<vec::Vector<vec::LargeStringContainer<std::string_view>>*>(
            outputBase.get());
        DecodedTextRecord decoded;
        for (size_t index = 0; index < records.size(); ++index) {
            codec_->DecodeRecord(records[index], decoded);
            if (decoded.fields.size() != 1) {
                throw std::runtime_error("RawLineCodec must decode exactly one field.");
            }
            if (decoded.fields[0].isNull) {
                output->SetNull(static_cast<int32_t>(index));
            } else {
                output->SetValue(static_cast<int32_t>(index), decoded.fields[0].value);
            }
        }
        batch->push_back(outputBase.release());
        return records.size();
    }
    if ((textOptions.IsLazySimple() || textOptions.IsCsv()) && rowType_->size() > 0) {
        using StringVector = vec::Vector<vec::LargeStringContainer<std::string_view>>;
        std::vector<std::unique_ptr<vec::BaseVector>> stringColumns;
        std::vector<StringVector*> writableColumns;
        stringColumns.reserve(rowType_->size());
        writableColumns.reserve(rowType_->size());
        for (int32_t column = 0; column < rowType_->size(); ++column) {
            std::unique_ptr<vec::BaseVector> stringColumn(
                vec::VectorHelper::CreateStringVector(
                    options_->GetTaskPool(), static_cast<uint32_t>(batchLen)));
            writableColumns.push_back(reinterpret_cast<StringVector*>(stringColumn.get()));
            stringColumns.emplace_back(std::move(stringColumn));
        }

        DecodedTextRecord decoded;
        uint64_t rows = 0;
        std::string_view record;
        std::string valueScratch;
        while (rows < batchLen && NextLine(record)) {
            if (textOptions.sourceKind == TextSourceKind::SPARK_CSV &&
                IsIgnoredSparkCsvRecord(record)) {
                continue;
            }
            codec_->DecodeRecord(record, decoded);
            if (decoded.fields.size() != writableColumns.size()) {
                throw std::runtime_error("LazySimpleCodec output does not match projected schema.");
            }
            for (size_t column = 0; column < writableColumns.size(); ++column) {
                auto value = decoded.fields[column].value;
                const bool valid = decoded.fields[column].isNull ||
                    textOptions.sourceKind != TextSourceKind::SPARK_CSV ||
                    NormalizeCsvValue(value, rowType_->childAt(static_cast<int32_t>(column))->GetId(),
                        valueScratch);
                if (decoded.fields[column].isNull || !valid) {
                    writableColumns[column]->SetNull(static_cast<int32_t>(rows));
                } else {
                    writableColumns[column]->SetValue(
                        static_cast<int32_t>(rows), value);
                }
            }
            ++rows;
        }
        if (rows == 0) {
            return 0;
        }
        for (int32_t column = 0; column < rowType_->size(); ++column) {
            vec::unsafe::UnsafeBaseVector::SetSize(
                stringColumns[column].get(), static_cast<int32_t>(rows));
            auto converted = valueConverter_->DecodeColumn(
                std::move(stringColumns[column]), rowType_->childAt(column),
                textOptions.temporal.dateFormat,
                textOptions.temporal.timestampFormats);
            batch->push_back(converted.release());
        }
        return rows;
    }
    throw std::runtime_error("Unsupported Text reader codec.");
}

uint64_t TextRowReader::Next(
    std::vector<BaseVector*>** batch, int* omniTypeId, uint64_t batchLen)
{
    auto output = std::make_unique<std::vector<BaseVector*>>();
    auto rows = NextDirect(output.get(), omniTypeId, batchLen);
    *batch = rows == 0 ? nullptr : output.release();
    return rows;
}

} // namespace omniruntime::reader::text
