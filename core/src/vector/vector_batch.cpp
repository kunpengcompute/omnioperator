/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 */

#include "unsafe_vector.h"
#include "vector_batch.h"
#include "array_vector.h"
#include "map_vector.h"
#include "row_vector.h"

#include <algorithm>
#include <limits>

namespace omniruntime::vec {
namespace {
constexpr uint64_t kMaxEstimate = std::numeric_limits<uint64_t>::max();

uint64_t SaturatingAdd(uint64_t left, uint64_t right)
{
    return left > kMaxEstimate - right ? kMaxEstimate : left + right;
}

uint64_t SaturatingMultiply(uint64_t left, uint64_t right)
{
    return right != 0 && left > kMaxEstimate / right ? kMaxEstimate : left * right;
}

uint64_t FixedWidth(DataTypeId type)
{
    switch (type) {
        case OMNI_BYTE:
        case OMNI_BOOLEAN:
            return sizeof(int8_t);
        case OMNI_SHORT:
            return sizeof(int16_t);
        case OMNI_INT:
        case OMNI_FLOAT:
        case OMNI_DATE32:
        case OMNI_TIME32:
        case OMNI_INTERVAL_MONTHS:
            return sizeof(int32_t);
        case OMNI_DECIMAL128:
        case OMNI_INTERVAL_DAY_TIME:
            return sizeof(type::Decimal128);
        case OMNI_LONG:
        case OMNI_DOUBLE:
        case OMNI_DECIMAL64:
        case OMNI_DATE64:
        case OMNI_TIME64:
        case OMNI_TIMESTAMP:
        case OMNI_TIME_WITHOUT_TIME_ZONE:
        case OMNI_TIMESTAMP_WITHOUT_TIME_ZONE:
        case OMNI_TIMESTAMP_WITH_TIME_ZONE:
        case OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE:
        case OMNI_CONTAINER:
            return sizeof(int64_t);
        default:
            return 0;
    }
}

bool IsStringType(DataTypeId type)
{
    return type == OMNI_VARCHAR || type == OMNI_CHAR || type == OMNI_VARBINARY;
}

uint64_t EstimateStringPayload(BaseVector *vector)
{
    const int32_t size = std::max(vector->GetSize(), 0);
    if (vector->GetEncoding() == OMNI_FLAT) {
        auto *strings = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
        return unsafe::UnsafeStringVector::GetContainer(strings)->GetCapacityInBytes();
    }
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        auto *strings = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector);
        const auto dictionary = unsafe::UnsafeDictionaryContainer::GetStringDictionaryOriginal(
            unsafe::UnsafeDictionaryVector::GetDictionaryOriginal(strings).get());
        // Flattening a dictionary can repeat values. Keep this O(1) by using
        // the larger of its retained payload and Omni's standard variable-width
        // row estimate instead of scanning every input row.
        constexpr uint64_t kDefaultVariableWidth = 200;
        return std::max<uint64_t>(dictionary->GetCapacityInBytes(),
            SaturatingMultiply(static_cast<uint64_t>(size), kDefaultVariableWidth));
    }
    if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constant = static_cast<ConstVector<std::string_view> *>(vector);
        const uint64_t nonNullRows = static_cast<uint64_t>(size - vector->GetNullCount());
        return SaturatingMultiply(nonNullRows, constant->GetConstValueRef().size());
    }
    // Unknown string encodings still need a non-zero byte estimate.
    return SaturatingMultiply(static_cast<uint64_t>(size), sizeof(std::string_view));
}

uint64_t EstimateFlatVectorSize(BaseVector *vector)
{
    if (vector == nullptr) {
        return 0;
    }

    const uint64_t size = static_cast<uint64_t>(std::max(vector->GetSize(), 0));
    uint64_t bytes = static_cast<uint64_t>(NullsBuffer::CalculateNbytes(static_cast<int32_t>(size)));
    const auto type = vector->GetTypeId();

    if (const uint64_t width = FixedWidth(type); width != 0) {
        return SaturatingAdd(bytes, SaturatingMultiply(size, width));
    }
    if (IsStringType(type)) {
        bytes = SaturatingAdd(bytes, SaturatingMultiply(size + 1, sizeof(int32_t)));
        return SaturatingAdd(bytes, EstimateStringPayload(vector));
    }
    if (type == OMNI_ARRAY && vector->GetEncoding() == OMNI_ENCODING_ARRAY) {
        auto *array = static_cast<ArrayVector *>(vector);
        bytes = SaturatingAdd(bytes, SaturatingMultiply(size + 1, sizeof(int64_t)));
        return SaturatingAdd(bytes, EstimateFlatVectorSize(array->GetElementVector().get()));
    }
    if (type == OMNI_MAP && vector->GetEncoding() == OMNI_ENCODING_MAP) {
        auto *map = static_cast<MapVector *>(vector);
        bytes = SaturatingAdd(bytes, SaturatingMultiply(size + 1, sizeof(int64_t)));
        bytes = SaturatingAdd(bytes, EstimateFlatVectorSize(map->GetKeyVector().get()));
        return SaturatingAdd(bytes, EstimateFlatVectorSize(map->GetValueVector().get()));
    }
    if (type == OMNI_ROW && vector->GetEncoding() == OMNI_ENCODING_STRUCT) {
        auto *row = static_cast<RowVector *>(vector);
        if (!row->Children().empty()) {
            for (auto &child : row->Children()) {
                bytes = SaturatingAdd(bytes, EstimateFlatVectorSize(child.get()));
            }
        } else {
            for (auto *child : row->GetRawChildren()) {
                bytes = SaturatingAdd(bytes, EstimateFlatVectorSize(child));
            }
        }
        return bytes;
    }

    // Preserve a conservative byte estimate for extension/unknown encodings.
    return SaturatingAdd(bytes, SaturatingMultiply(size, sizeof(int64_t)));
}
} // namespace

/**
 * @param rowCnt
 */
VectorBatch::VectorBatch(size_t rowCnt) : capacity(rowCnt), rowCnt(rowCnt) {}

VectorBatch::~VectorBatch()
{
    FreeAllVectors();
}

/**
 * Set the vector at the indicated index, need ResizeVectorCount before SetVector
 * @param vector
 */
void VectorBatch::SetVector(int32_t index, BaseVector *vector)
{
    vectors[index] = vector;
}

/**
 * @param vector
 */
void VectorBatch::Append(BaseVector *vector)
{
    vectors.emplace_back(vector);
}

void VectorBatch::AppendFlat(BaseVector *vector)
{
    vectors.emplace_back(vector);
    ++flatSize;
}

BaseVector **VectorBatch::GetVectors()
{
    return vectors.data();
}

int32_t VectorBatch::GetRowCount() const
{
    return static_cast<int32_t>(rowCnt);
}

int32_t VectorBatch::GetVectorCount()
{
    return static_cast<int32_t>(vectors.size());
}

/**
 * @param vectorCnt
 */
void VectorBatch::ResizeVectorCount(size_t vectorCnt)
{
    vectors.resize(vectorCnt);
}

void VectorBatch::FreeAllVectors()
{
    auto vectorSize = vectors.size();
    std::unordered_set<BaseVector *> deleted;
    for (size_t vecIndex = 0; vecIndex < vectorSize; ++vecIndex) {
        if (deleted.count(vectors[vecIndex])) {
            continue;
        }
        delete vectors[vecIndex];
        try {
            deleted.insert(vectors[vecIndex]);
        } catch (const std::exception &e) {
            throw std::runtime_error("fail to insert");
        }
        vectors[vecIndex] = nullptr;
    }
    vectors.clear();
}

// This API is used to reuse the vector batch memory.
// The caller must ensure that the row count does not exceed the row count of the first memory allocation
void VectorBatch::Resize(size_t rowCount)
{
    if (rowCount > capacity) {
        throw OmniException("UNSUPPORTED_ERROR", "Can only resize vector batch to a smaller value.");
    }

    for (auto *vector : vectors) {
        vector->SetNulls(0, false, static_cast<int32_t>(rowCount));
        unsafe::UnsafeBaseVector::SetSize(vector, static_cast<int32_t>(rowCount));
    }

    rowCnt = rowCount;
}

void VectorBatch::ClearVectors()
{
    vectors.clear();
}

size_t VectorBatch::GetCapacity()
{
    return capacity;
}

uint64_t VectorBatch::CalculateTotalSize() const
{
    if (vectors.empty()) {
        return 0;
    }

    uint64_t totalSize = 0;
    for (const auto& vector : vectors) {
        totalSize += static_cast<uint64_t>(vector->GetSize());
    }

    return totalSize;
}

uint64_t VectorBatch::EstimateFlatSize() const
{
    uint64_t totalBytes = 0;
    for (auto *vector : vectors) {
        totalBytes = SaturatingAdd(totalBytes, EstimateFlatVectorSize(vector));
    }
    return totalBytes;
}
}
