/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: spill merger implementation
 */

#include <unistd.h>
#include "spill_merger.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"

namespace omniruntime {
namespace op {
using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

static constexpr size_t NUMBER_OF_ELEMENTS = 1;
using namespace omniSpark;
SpillReader::~SpillReader()
{
    // the spill file can be deleted if there is exception
    remove(filePath.c_str());
    if (prevBuffer_ != nullptr) {
        delete[] prevBuffer_;
        prevBuffer_ = nullptr;
    }
}

ErrorCode SpillReader::ReadVecBatch(std::unique_ptr<vec::VectorBatch> &vectorBatch, bool &isEnd)
{
    if (rowOffset >= totalRowCount) {
        isEnd = true;
        fclose(file);
        // the spill file can be deleted if all data has been read
        remove(filePath.c_str());
        return ErrorCode::SUCCESS;
    }

    if (file == nullptr) {
        file = fopen(filePath.c_str(), "r");
        if (file == nullptr) {
            auto errorNum = errno;
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Open file %s when read failed since %s.", filePath.c_str(), errorBuf);
            isEnd = true;
            return ErrorCode::READ_FAILED;
        }
    }

    int32_t rowCount = 0;
    if (Read(&rowCount, sizeof(int32_t)) != ErrorCode::SUCCESS) {
        isEnd = true;
        fclose(file);
        LogError("Read row count from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    auto vectorBatchPtr = vectorBatch.get();
    if (vectorBatchPtr == nullptr || rowCount > maxRowCount) {
        vectorBatch = std::make_unique<VectorBatch>(rowCount);
        VectorHelper::AppendVectors(vectorBatch.get(), dataTypes, rowCount);
        vectorBatchPtr = vectorBatch.get();
        maxRowCount = rowCount;
    } else {
        vectorBatchPtr->Resize(rowCount);
    }

    int32_t vecCount = vectorBatchPtr->GetVectorCount();
    for (int32_t vecIndex = 0; vecIndex < vecCount; vecIndex++) {
        BaseVector *vector = vectorBatchPtr->Get(vecIndex);
        ErrorCode result = ReadComplexVector(dataTypes.GetType(vecIndex), vector, rowCount);
        if (result != ErrorCode::SUCCESS) {
            isEnd = true;
            fclose(file);
            return result;
        }
    }
    rowOffset += rowCount;
    isEnd = false;
    return ErrorCode::SUCCESS;
}

template <typename T> ErrorCode SpillReader::ReadVector(BaseVector *vector, int32_t rowCount)
{
    if constexpr (std::is_same_v<T, std::string_view>) {
        /* *
         * vector format stored in file column by column, {nulls meta column, offsets meta column, values}
         * -nulls--offsets--values
         * 0        0      "aab"
         * 1        3        -
         * 0        3      "bbcd"
         * 7
         */
        auto resultVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);

        // read nulls
        uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(resultVector);
        int32_t nullsSize = BitUtil::Nbytes(rowCount);
        if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
            LogError("Read value nulls from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }

        // read offsets
        auto offsetSize = (static_cast<ssize_t>(rowCount) + 1) * sizeof(int32_t);
        auto offsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(resultVector));
        if (Read(offsets, offsetSize) != ErrorCode::SUCCESS) {
            LogError("Read value offsets from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }

        auto length = offsets[rowCount] - offsets[0];
        if (length > 0) {
            // read values
            char *valuesBuffer;
            if (length <= INITIAL_STRING_SIZE) {
                valuesBuffer = unsafe::UnsafeStringVector::GetValues(resultVector);
            } else {
                valuesBuffer = unsafe::UnsafeStringVector::ExpandStringBuffer(resultVector, length);
            }
            if (Read(valuesBuffer, length) != ErrorCode::SUCCESS) {
                LogError("Read values from %s failed.", filePath.c_str());
                return ErrorCode::READ_FAILED;
            }
        }
    } else {
        auto resultVector = static_cast<Vector<T> *>(vector);

        // read valueNulls
        uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(resultVector);
        int32_t nullsSize = BitUtil::Nbytes(rowCount);
        if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
            LogError("Read value nulls from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }

        // read values
        T *values = unsafe::UnsafeVector::GetRawValues(resultVector);
        auto length = static_cast<ssize_t>(rowCount * sizeof(T));
        if (Read(values, length) != ErrorCode::SUCCESS) {
            LogError("Read values from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }
    }
    return ErrorCode::SUCCESS;
}

std::pair<size_t, bool> parseCompressionHeader(const char* header_buf) {
    size_t compressedSize = 0;
    bool is_original = false;

    compressedSize |= (static_cast<uint8_t>(header_buf[0]) >> 1);
    is_original = (static_cast<uint8_t>(header_buf[0]) & 0x01) == 1;

    compressedSize |= (static_cast<uint8_t>(header_buf[1]) << 7);

    compressedSize |= (static_cast<uint8_t>(header_buf[2]) << 15);

    return {compressedSize, is_original};
}

ErrorCode SpillReader::ReadArrayVector(const DataTypePtr &dataType, BaseVector *vector, int32_t rowCount)
{
    auto arrayVec = static_cast<omniruntime::vec::ArrayVector*>(vector);

    // read null
    uint8_t* nulls = unsafe::UnsafeBaseVector::GetNulls(arrayVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);

    if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
        LogError("Read array nulls from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    // read offsets (int64_t[RowCount + 1])
    int64_t* offsets = arrayVec->GetOffsets();
    ssize_t offsetsSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int64_t));
    if (Read(offsets, offsetsSize) != ErrorCode::SUCCESS) {
        LogError("Read array offsets from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    auto arrayType = std::dynamic_pointer_cast<ArrayType>(dataType);
    if (!arrayType) {
        LogError("DataType is not ArrayType for OMNI_ARRAY column.");
        return ErrorCode::READ_FAILED;
    }
    const auto& elementType = arrayType->ElementType();

    int32_t elementRowCount = static_cast<int32_t>(offsets[rowCount]);
    auto elementVec = arrayVec->GetElementVector();
    if (!elementVec) {
        LogError("arrayVec->GetElementVector() is null.");
        return ErrorCode::READ_FAILED;
    }
    elementVec->Expand(elementRowCount);

    // read element vector
    auto result = ReadComplexVector(elementType, elementVec.get(), elementRowCount);
    if (result != ErrorCode::SUCCESS) {
        LogError("Failed to read array element vector from %s.", filePath.c_str());
        return result;
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillReader::ReadMapVector(const DataTypePtr &dataType, BaseVector *vector, int32_t rowCount)
{
    auto mapType = std::dynamic_pointer_cast<MapType>(dataType);
    auto mapVec = static_cast<omniruntime::vec::MapVector *>(vector);
    if (!mapType) {
        LogError("ReadMapVector: dataType is not MapType");
        return ErrorCode::READ_FAILED;
    }

    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(mapVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
        LogError("Read map nulls from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    int64_t *offsets = mapVec->GetOffsets();
    ssize_t offsetsSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int64_t));
    if (Read(offsets, offsetsSize) != ErrorCode::SUCCESS) {
        LogError("Read map offsets from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    int32_t entryCount = static_cast<int32_t>(offsets[rowCount]);
    auto keyVec = mapVec->GetKeyVector();
    if (keyVec) {
        keyVec->Expand(entryCount);
        auto res = ReadComplexVector(mapType->Key(), keyVec.get(), entryCount);
        if (res != ErrorCode::SUCCESS) {
            LogError("Failed to read map key vector from %s.", filePath.c_str());
            return res;
        }
    }
    auto valueVec = mapVec->GetValueVector();
    if (valueVec) {
        valueVec->Expand(entryCount);
        auto res = ReadComplexVector(mapType->Value(), valueVec.get(), entryCount);
        if (res != ErrorCode::SUCCESS) {
            LogError("Failed to read map value vector from %s.", filePath.c_str());
            return res;
        }
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillReader::ReadRowVector(const DataTypePtr &dataType, BaseVector *vector, int32_t rowCount)
{
    auto rowType = std::dynamic_pointer_cast<RowType>(dataType);
    auto rowVec = static_cast<omniruntime::vec::RowVector *>(vector);
    if (!rowType) {
        LogError("ReadRowVector: dataType is not RowType");
        return ErrorCode::READ_FAILED;
    }

    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(rowVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
        LogError("Read row nulls from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    auto &children = rowVec->Children();
    for (size_t i = 0; i < children.size(); i++) {
        if (i < static_cast<size_t>(rowType->Size()) && children[i]) {
            auto res = ReadComplexVector(rowType->Type(static_cast<int>(i)), children[i].get(), rowCount);
            if (res != ErrorCode::SUCCESS) {
                LogError("Failed to read row child vector from %s.", filePath.c_str());
                return res;
            }
        }
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillReader::ReadComplexVector(const DataTypePtr &dataType, BaseVector *vector, int32_t rowCount)
{
    ErrorCode result = ErrorCode::SUCCESS;
    int32_t typeId = dataType->GetId();
    switch (typeId) {
        case OMNI_BOOLEAN:
            result = ReadVector<bool>(vector, rowCount);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            result = ReadVector<int32_t>(vector, rowCount);
            break;
        case OMNI_SHORT:
            result = ReadVector<int16_t>(vector, rowCount);
            break;
        case OMNI_LONG:
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP:
            result = ReadVector<int64_t>(vector, rowCount);
            break;
        case OMNI_DOUBLE:
            result = ReadVector<double>(vector, rowCount);
            break;
        case OMNI_FLOAT:
            result = ReadVector<float>(vector, rowCount);
            break;
        case OMNI_VARBINARY:
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            result = ReadVector<std::string_view>(vector, rowCount);
            break;
        case OMNI_DECIMAL128:
            result = ReadVector<Decimal128>(vector, rowCount);
            break;
        case OMNI_BYTE:
            result = ReadVector<int8_t>(vector, rowCount);
            break;
        case OMNI_ARRAY:
            result = ReadArrayVector(dataType, vector, rowCount);
            break;
        case OMNI_MAP:
            result = ReadMapVector(dataType, vector, rowCount);
            break;
        case OMNI_ROW:
            result = ReadRowVector(dataType, vector, rowCount);
            break;
        default:
            result = ErrorCode::READ_FAILED;
            LogError("ReadComplexVector failed for unsupported typeId=%s", std::to_string(typeId));
            break;
    }
    return result;
}

ErrorCode SpillReader::Read(void *buf, size_t bufSize)
{
    if (bufSize == 0) {
        return ErrorCode::SUCCESS;
    }
    // clear before error
    clearerr(file);
    if (!isSpillCompressEnabled) {
        size_t nread = fread(buf, bufSize, NUMBER_OF_ELEMENTS, file);
        if (nread < NUMBER_OF_ELEMENTS) {
            // save errno
            int savedErrno = ferror(file) ? errno : 0;
            if (savedErrno == 0 && feof(file)) {
                savedErrno = EIO;
            }
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(savedErrno, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Read from %s failed: %s (errno=%d)", filePath.c_str(), errorBuf, savedErrno);
            return ErrorCode::READ_FAILED;
        }
        return ErrorCode::SUCCESS;
    }
    return ReadWithCompress(buf, bufSize);
}

ErrorCode SpillReader::ReadWithCompress(void *buf, size_t bufSize)
{
    if (bufSize == 0) {
        return ErrorCode::SUCCESS;
    }

    if (remainLength == 0) {
        if (prevBuffer_ != nullptr) {
            delete[] prevBuffer_;
            prevBuffer_ = nullptr;
        }
        char* header_data = new char[3];
        size_t nread = fread(header_data, 1, 3, file);
        if (nread != 3) {
            int savedErrno = ferror(file) ? errno : 0;
            if (savedErrno == 0 && feof(file)) {
                savedErrno = EIO;
            }
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(savedErrno, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Read header from %s failed: %s (errno=%d)", filePath.c_str(), errorBuf, savedErrno);
            delete[] header_data;
            return ErrorCode::READ_FAILED;
        }
        std::pair<size_t, bool> header_info = parseCompressionHeader(header_data);
        delete[] header_data;
        size_t compressed_block_size = header_info.first;
        bool is_original_data = header_info.second;
        int32_t actual_decompress_len = 0;
        char* compressed_data = new char[compressed_block_size];
        nread = fread(compressed_data, 1, compressed_block_size, file);
        if (nread != compressed_block_size) {
            int savedErrno = ferror(file) ? errno : 0;
            if (savedErrno == 0 && feof(file)) {
                savedErrno = EIO;
            }
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(savedErrno, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Read compressed data from %s failed: %s (errno=%d)", filePath.c_str(), errorBuf, savedErrno);
            delete[] compressed_data;
            return ErrorCode::READ_FAILED;
        }
        if (is_original_data) {
            actual_decompress_len = static_cast<int32_t>(compressed_block_size);
            prevBuffer_ = compressed_data;
            currentBuffer_ = prevBuffer_;
            remainLength = actual_decompress_len;
        } else {
            auto [decomp_data, decomp_len] = decompressLZ4(
                    compressed_data, static_cast<int32_t>(compressed_block_size), 32 * 1024 * 1024
            );
            delete[] compressed_data;
            if (decomp_data == nullptr || decomp_len <= 0) {
                return ErrorCode::READ_FAILED;
            }

            prevBuffer_ = decomp_data;
            currentBuffer_ = prevBuffer_;
            remainLength = decomp_len;
        }
    }

    size_t bytes_to_copy = remainLength <bufSize ? remainLength : bufSize;
    memcpy(buf, currentBuffer_, bytes_to_copy);

    currentBuffer_ += bytes_to_copy;
    remainLength -= bytes_to_copy;

    return ErrorCode::SUCCESS;
}

int32_t SpillMergeStream::CompareTo(const SpillMergeStream &other)
{
    if (compareWithHashVal) {
        auto leftVectorBatch = currentBatchPtr;
        auto rightVectorBatch = other.currentBatchPtr;
        auto leftPosition = currentRowIdx;
        auto rightPosition = other.currentRowIdx;
        auto sortColsCount = sortCols.size();

        BaseVector *leftHashVector = leftVectorBatch->Get(0);
        BaseVector *rightHashVector = rightVectorBatch->Get(0);

        for (size_t i = 0; i < sortColsCount; i++) {
            int32_t sortCol = sortCols[i];
            BaseVector *leftVector = leftVectorBatch->Get(sortCol);
            BaseVector *rightVector = rightVectorBatch->Get(sortCol);
            int32_t compare = sortCompareFuncsWithHashVal[i](leftHashVector, leftVector, leftPosition, rightHashVector, rightVector, rightPosition);
            if (compare != 0) {
                return compare;
            }
        }
        return 0;
    } else {
        auto leftVectorBatch = currentBatchPtr;
        auto rightVectorBatch = other.currentBatchPtr;
        auto leftPosition = currentRowIdx;
        auto rightPosition = other.currentRowIdx;
        auto sortColsCount = sortCols.size();
        for (size_t i = 0; i < sortColsCount; i++) {
            int32_t sortCol = sortCols[i];
            BaseVector *leftVector = leftVectorBatch->Get(sortCol);
            BaseVector *rightVector = rightVectorBatch->Get(sortCol);
            int32_t compare = sortCompareFuncs[i](leftVector, leftPosition, rightVector, rightPosition);
            if (compare != 0) {
                return compare;
            }
        }
        return 0;
    }

}

void SpillMerger::SetCompareFunctionsWithHashVal(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
const std::vector<SortOrder> &sortOrders, std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs)
{
    auto dataTypeIds = dataTypes.GetIds();
    auto sortColSize = sortCols.size();
    for (size_t i = 0; i < sortColSize; i++) {
        auto typeId = dataTypeIds[sortCols[i]];
        bool isAscending = sortOrders[i].IsAscending();
        bool isNullsFirst = sortOrders[i].IsNullsFirst();
        switch (typeId) {
            case OMNI_CHAR:
            case OMNI_VARCHAR:
                SetCompareFunctionWithHashVal<std::string_view>(isAscending, isNullsFirst, sortCompareFuncs);
            break;

            default:
                std::string errStr = "Do not support the data type" + std::to_string(typeId) +
                    " in SetCompareFunctions.";
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
        }
    }
}

void SpillMerger::SetCompareFunctions(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
    const std::vector<SortOrder> &sortOrders, std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs)
{
    auto dataTypeIds = dataTypes.GetIds();
    auto sortColSize = sortCols.size();
    for (size_t i = 0; i < sortColSize; i++) {
        auto typeId = dataTypeIds[sortCols[i]];
        bool isAscending = sortOrders[i].IsAscending();
        bool isNullsFirst = sortOrders[i].IsNullsFirst();
        switch (typeId) {
            case OMNI_INT:
            case OMNI_DATE32:
                SetCompareFunction<int32_t>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_SHORT:
                SetCompareFunction<int16_t>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_DECIMAL64:
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
                SetCompareFunction<int64_t>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_BOOLEAN:
                SetCompareFunction<bool>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_DOUBLE:
                SetCompareFunction<double>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_FLOAT:
                SetCompareFunction<float>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_VARBINARY:
            case OMNI_CHAR:
            case OMNI_VARCHAR:
                SetCompareFunction<std::string_view>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_DECIMAL128:
                SetCompareFunction<Decimal128>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            case OMNI_BYTE:
                SetCompareFunction<int8_t>(isAscending, isNullsFirst, sortCompareFuncs);
                break;
            default:
                std::string errStr = "Do not support the data type" + std::to_string(typeId) +
                    " in SetCompareFunctions.";
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
        }
    }
}

// The CompareNull<IsAscending, IsNullsFirst> template folded in operator_util.h
// treats IsNullsFirst as "null is smaller than any value" (it negates the null
// result when IsAscending is false), while the plan flag 'isNullsFirst' is
// absolute: the position of NULLs in the final order. Convert the flag for the
// descending case, otherwise the spill merger places NULLs at the opposite end.
static inline bool NullsCompareLessThanValues(bool isAscending, bool isNullsFirst)
{
    return isAscending ? isNullsFirst : !isNullsFirst;
}

template <typename T>
void SpillMerger::SetCompareFunctionWithHashVal(bool isAscending, bool isNullsFirst,
    std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs)
{
    const bool nullsLess = NullsCompareLessThanValues(isAscending, isNullsFirst);
    if (isAscending && nullsLess) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplateWithHashVal<T, true, true, true>);
    } else if (isAscending && !nullsLess) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplateWithHashVal<T, true, true, false>);
    } else if (!isAscending && nullsLess) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplateWithHashVal<T, false, true, true>);
    } else {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplateWithHashVal<T, false, true, false>);
    }
}

template <typename T>
void SpillMerger::SetCompareFunction(bool isAscending, bool isNullsFirst,
    std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs)
{
    const bool nullsLess = NullsCompareLessThanValues(isAscending, isNullsFirst);
    if (isAscending && nullsLess) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, true, true, true>);
    } else if (isAscending && !nullsLess) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, true, true, false>);
    } else if (!isAscending && nullsLess) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, false, true, true>);
    } else {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, false, true, false>);
    }
}
}
}
