/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Description: spiller implementation
 */

#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include "vector/unsafe_vector.h"
#include "spiller.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
static std::string SPILL_TEMPLATE("spill-XXXXXX");
static const char *SPILL_TEMPLATE_CHARS = SPILL_TEMPLATE.c_str();
static int32_t SPILL_TEMPLATE_SIZE = static_cast<int32_t>(SPILL_TEMPLATE.size());
// When PID and TID is converted into a character string, the maximum length is 10.
constexpr int PID_LENGTH = 10;
constexpr int TID_LENGTH = 10;

ErrorCode Spiller::Spill(AggregationSort *aggregationSort)
{
    size_t totalRowCount = aggregationSort->GetRowCount();
    if (totalRowCount <= 0) {
        return ErrorCode::SUCCESS;
    }
    // create spill writer object
    auto writer = new SpillWriter(dataTypes, dirPaths[0], writeBufferSize);
    writers.emplace_back(writer);

    int64_t totalRowOffset = 0;
    int32_t vecBatchCount = OperatorUtil::GetVecBatchCount(totalRowCount, maxRowCountPerBatch);
    int32_t maxRowCount = 0; // for reuse vector batch memory
    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; vecBatchIdx++) {
        auto rowCount = std::min(maxRowCountPerBatch, static_cast<int32_t>(totalRowCount - totalRowOffset));
        if (spillVecBatch == nullptr || rowCount > maxRowCount) {
            spillVecBatch = std::make_unique<VectorBatch>(rowCount);
            VectorHelper::AppendVectors(spillVecBatch.get(), dataTypes, rowCount);
            maxRowCount = rowCount;
        } else {
            spillVecBatch->Resize(rowCount);
        }
        auto spillVecBatchPtr = spillVecBatch.get();
        aggregationSort->SetSpillVectorBatch(spillVecBatchPtr, totalRowOffset);
        auto vecBatchSize = CollectVecBatchSize(spillVecBatchPtr);
        if (spillTracker->CheckIfExceedAndReserve(vecBatchSize)) {
            return ErrorCode::EXCEED_SPILL_THRESHOLD;
        }

        auto result = writer->WriteVecBatch(spillVecBatchPtr, vecBatchSize);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }
        totalRowOffset += rowCount;
    }
    return writer->Close();
}

ErrorCode Spiller::Spill(PagesIndex *pagesIndex, bool canInplaceSort, bool canRadixSort)
{
    int64_t totalRowCount = pagesIndex->GetRowCount();
    if (totalRowCount <= 0) {
        return ErrorCode::SUCCESS;
    }

    // create spill writer object
    auto writer = new SpillWriter(dataTypes, dirPaths[0], writeBufferSize);
    writers.emplace_back(writer);

    int64_t totalRowOffset = 0;
    int32_t vecBatchCount = OperatorUtil::GetVecBatchCount(totalRowCount, maxRowCountPerBatch);
    int32_t maxRowCount = 0; // for reuse vector batch memory
    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; vecBatchIdx++) {
        auto rowCount = std::min(maxRowCountPerBatch, static_cast<int32_t>(totalRowCount - totalRowOffset));
        if (spillVecBatch == nullptr || rowCount > maxRowCount) {
            spillVecBatch = std::make_unique<VectorBatch>(rowCount);
            VectorHelper::AppendVectors(spillVecBatch.get(), dataTypes, rowCount);
            maxRowCount = rowCount;
        } else {
            spillVecBatch->Resize(rowCount);
        }

        auto spillVecBatchPtr = spillVecBatch.get();
        pagesIndex->SetSpillVecBatch(spillVecBatchPtr, outputCols, totalRowOffset, canInplaceSort, canRadixSort);
        auto vecBatchSize = CollectVecBatchSize(spillVecBatchPtr);
        if (spillTracker->CheckIfExceedAndReserve(vecBatchSize)) {
            return ErrorCode::EXCEED_SPILL_THRESHOLD;
        }

        auto result = writer->WriteVecBatch(spillVecBatchPtr, vecBatchSize);
        if (result != ErrorCode::SUCCESS) {
            return result;
        }
        totalRowOffset += rowCount;
    }

    return writer->Close();
}

uint64_t Spiller::CollectVecBatchSize(vec::VectorBatch *vectorBatch)
{
    uint64_t result = sizeof(int32_t); // for row count size
    int32_t vecCount = vectorBatch->GetVectorCount();
    for (int32_t i = 0; i < vecCount; i++) {
        auto vector = vectorBatch->Get(i);
        switch (dataTypes.GetType(i)->GetId()) {
            case OMNI_BOOLEAN: result += CollectVectorSize<bool>(vector); break;
            case OMNI_INT:
            case OMNI_DATE32: result += CollectVectorSize<int32_t>(vector); break;
            case OMNI_SHORT: result += CollectVectorSize<int16_t>(vector); break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP: result += CollectVectorSize<int64_t>(vector); break;
            case OMNI_DOUBLE: result += CollectVectorSize<double>(vector); break;
            case OMNI_DECIMAL128: result += CollectVectorSize<Decimal128>(vector); break;
            case OMNI_VARCHAR:
            case OMNI_CHAR: result += CollectVectorSize<std::string_view>(vector); break;
            case OMNI_BYTE: result += CollectVectorSize<int8_t>(vector); break;
            default: {
                break;
            }
        }
    }
    return result;
}

template <typename T> uint64_t Spiller::CollectVectorSize(vec::BaseVector *vector)
{
    int32_t rowCount = vector->GetSize();
    uint64_t result = BitUtil::Nbytes(rowCount); // nulls byte size
    if constexpr (std::is_same_v<T, std::string_view>) {
        // offsets
        result += (rowCount + 1) * sizeof(int32_t);
        // value length
        auto varcharVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
        auto offsets = unsafe::UnsafeStringVector::GetOffsets(varcharVector);
        result += offsets[rowCount] - offsets[0];
    } else {
        // value length
        result += rowCount * sizeof(T);
    }
    return result;
}

ErrorCode SpillWriter::CreateTempFile()
{
    // the spill directory will be created when CheckOperatorConfig if it does not exist
    int32_t fileNameLen = dirPath.size() + SPILL_TEMPLATE_SIZE + PID_LENGTH + TID_LENGTH + 1;
    auto dirPathChars = dirPath.c_str();
    char filePathChars[fileNameLen];
    auto pid = static_cast<int>(getpid());
    auto tid = static_cast<uint32_t>(pthread_self());
    LogDebug("Spill writer create temp file at dir: %s.", dirPathChars);
    if (snprintf_s(filePathChars, fileNameLen, fileNameLen, "%s/%d-%u-%s", dirPathChars, pid, tid,
                   SPILL_TEMPLATE_CHARS) < 0) {
        auto errorNum = errno;
        char errorBuf[ERROR_BUFFER_SIZE];
        GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
        LogError("Snprintf for %s and %s failed since %s.", dirPathChars, SPILL_TEMPLATE_CHARS, errorBuf);
        return ErrorCode::WRITE_FAILED;
    }

    // it will open the file and the file permission is 600
    int32_t tempFd = mkstemp(const_cast<char *>(filePathChars));
    if (tempFd == -1) {
        auto errorNum = errno;
        char errorBuf[ERROR_BUFFER_SIZE];
        GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
        LogError("Mkstemp in %s for %s failed since %s.", dirPathChars, filePathChars, errorBuf);
        return ErrorCode::MKSTEMP_FAILED;
    }
    // set the file permission to 600
    if (fchmod(tempFd, S_IRUSR | S_IWUSR) == -1) {
        auto errorNum = errno;
        char errorBuf[ERROR_BUFFER_SIZE];
        GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
        LogError("Fchmod %s failed since %s.", filePathChars, errorBuf);
        return ErrorCode::WRITE_FAILED;
    }
    filePath = filePathChars;
    fd = tempFd;
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::WriteVecBatch(vec::VectorBatch *vectorBatch, uint64_t vectorBatchSize)
{
    ErrorCode result = ErrorCode::SUCCESS;
    if (fd == -1) {
        result = CreateTempFile();
        if (result != ErrorCode::SUCCESS) {
            return result;
        }
    }

    if (writeBufferSize != 0) {
        if (writeBufferOffset + vectorBatchSize > writeBufferSize) {
            if (Write(writeBuffer, writeBufferOffset) != ErrorCode::SUCCESS) {
                LogError("Write buffer to file %s failed.", filePath.c_str());
                return ErrorCode::WRITE_FAILED;
            }
            fileLength += writeBufferOffset;
            writeBufferOffset = 0;
        }
        if (vectorBatchSize > writeBufferSize) {
            result = WriteVecBatchToFile(vectorBatch);
            if (result == ErrorCode::SUCCESS) {
                fileLength += vectorBatchSize;
            }
        } else {
            // write vector batch to writer buffer
            result = WriteVecBatchToBuffer(vectorBatch);
            if (result == ErrorCode::SUCCESS) {
                writeBufferOffset += vectorBatchSize;
            }
        }
        return result;
    }

    result = WriteVecBatchToFile(vectorBatch);
    if (result == ErrorCode::SUCCESS) {
        fileLength += vectorBatchSize;
    }
    return result;
}

ErrorCode SpillWriter::WriteVecBatchToBuffer(vec::VectorBatch *vectorBatch)
{
    int32_t rowCount = vectorBatch->GetRowCount();
    int32_t writeOffset = writeBufferOffset;
    auto writeBufferStart = writeBuffer + writeOffset;
    *reinterpret_cast<int32_t *>(writeBufferStart) = rowCount;
    writeOffset += sizeof(rowCount);

    int32_t vecCount = vectorBatch->GetVectorCount();
    for (int32_t i = 0; i < vecCount; i++) {
        auto vector = vectorBatch->Get(i);
        auto result = ErrorCode::SUCCESS;
        switch (dataTypes.GetType(i)->GetId()) {
            case OMNI_BOOLEAN: result = WriteVectorToBuffer<bool>(vector, rowCount, writeOffset); break;
            case OMNI_INT:
            case OMNI_DATE32: result = WriteVectorToBuffer<int32_t>(vector, rowCount, writeOffset); break;
            case OMNI_SHORT: result = WriteVectorToBuffer<int16_t>(vector, rowCount, writeOffset); break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP: result = WriteVectorToBuffer<int64_t>(vector, rowCount, writeOffset); break;
            case OMNI_DOUBLE: result = WriteVectorToBuffer<double>(vector, rowCount, writeOffset); break;
            case OMNI_DECIMAL128: result = WriteVectorToBuffer<Decimal128>(vector, rowCount, writeOffset); break;
            case OMNI_VARCHAR:
            case OMNI_CHAR: result = WriteVectorToBuffer<std::string_view>(vector, rowCount, writeOffset); break;
            case OMNI_BYTE: result = WriteVectorToBuffer<int8_t>(vector, rowCount, writeOffset); break;
            default: {
                result = ErrorCode::WRITE_FAILED;
                break;
            }
        }
        if (result != ErrorCode::SUCCESS) {
            return result;
        }
    }
    totalRowCount += rowCount;
    return ErrorCode::SUCCESS;
}

template <typename T>
ErrorCode SpillWriter::WriteVectorToBuffer(vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset)
{
    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(vector);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    errno_t ret = memcpy_s(writeBuffer + writeOffset, nullsSize, nulls, nullsSize);
    if (ret != EOK) {
        auto errorNum = errno;
        char errorBuf[ERROR_BUFFER_SIZE];
        GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
        LogError("Write value nulls to buffer failed since %s.", errorBuf);
        return ErrorCode::WRITE_FAILED;
    }
    writeOffset += nullsSize;

    if constexpr (std::is_same_v<T, std::string_view>) {
        // write offsets
        auto valueOffsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector));
        auto offsetLength = (static_cast<ssize_t>(rowCount) + 1) * sizeof(int32_t);
        ret = memcpy_s(writeBuffer + writeOffset, offsetLength, valueOffsets, offsetLength);
        if (ret != EOK) {
            auto errorNum = errno;
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Write value offsets to buffer failed since %s.", errorBuf);
            return op::ErrorCode::WRITE_FAILED;
        }
        writeOffset += offsetLength;

        // write values
        char *values = unsafe::UnsafeStringVector::GetValues(reinterpret_cast<VarcharVector *>(vector));
        auto valueLength = static_cast<ssize_t>(valueOffsets[rowCount] - valueOffsets[0]);
        ret = valueLength == 0 ? EOK : memcpy_s(writeBuffer + writeOffset, valueLength, values, valueLength);
        if (ret != EOK) {
            auto errorNum = errno;
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Write values to buffer failed since %s.", errorBuf);
            return op::ErrorCode::WRITE_FAILED;
        }
        writeOffset += valueLength;
        return ErrorCode::SUCCESS;
    } else {
        auto length = static_cast<ssize_t>(rowCount * sizeof(T));
        T *values = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<T> *>(vector));
        ret = memcpy_s(writeBuffer + writeOffset, length, values, length);
        if (ret != EOK) {
            auto errorNum = errno;
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Write values to buffer failed since %s.", errorBuf);
            return ErrorCode::WRITE_FAILED;
        }
        writeOffset += length;
        return ErrorCode::SUCCESS;
    }
}

ErrorCode SpillWriter::WriteVecBatchToFile(vec::VectorBatch *vectorBatch)
{
    int32_t rowCount = vectorBatch->GetRowCount();
    if (Write(&rowCount, sizeof(rowCount)) != ErrorCode::SUCCESS) {
        LogError("Write row count to %s failed.", filePath.c_str());
        return ErrorCode::WRITE_FAILED;
    }

    int32_t vecCount = vectorBatch->GetVectorCount();
    for (int32_t i = 0; i < vecCount; i++) {
        auto vector = vectorBatch->Get(i);
        auto result = ErrorCode::SUCCESS;
        switch (dataTypes.GetType(i)->GetId()) {
            case OMNI_BOOLEAN: result = WriteVector<bool>(vector, rowCount); break;
            case OMNI_INT:
            case OMNI_DATE32: result = WriteVector<int32_t>(vector, rowCount); break;
            case OMNI_SHORT: result = WriteVector<int16_t>(vector, rowCount); break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP: result = WriteVector<int64_t>(vector, rowCount); break;
            case OMNI_DOUBLE: result = WriteVector<double>(vector, rowCount); break;
            case OMNI_DECIMAL128: result = WriteVector<Decimal128>(vector, rowCount); break;
            case OMNI_VARCHAR:
            case OMNI_CHAR: result = WriteVector<std::string_view>(vector, rowCount); break;
            case OMNI_BYTE: result = WriteVector<int8_t>(vector, rowCount); break;
            default: {
                result = ErrorCode::WRITE_FAILED;
                break;
            }
        }

        if (result != ErrorCode::SUCCESS) {
            return result;
        }
    }

    totalRowCount += rowCount;
    return ErrorCode::SUCCESS;
}

/**
 * vector format stored in file column by column, {nulls meta column, offsets meta column, values}
 * -nulls--offsets--values
 * 0        0      "aab"
 * 1        3        -
 * 0        3      "bbcd"
 * 7
 */
template <typename T> ErrorCode SpillWriter::WriteVector(omniruntime::vec::BaseVector *vector, int32_t rowCount)
{
    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(vector);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    if (Write(nulls, nullsSize) != ErrorCode::SUCCESS) {
        LogError("Write value nulls to %s failed.", filePath.c_str());
        return ErrorCode::WRITE_FAILED;
    }

    if constexpr (std::is_same_v<T, std::string_view>) {
        // write offsets
        auto offsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector));
        auto offsetSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int32_t));
        if (Write(offsets, offsetSize) != ErrorCode::SUCCESS) {
            LogError("Write value offsets to %s failed.", filePath.c_str());
            return op::ErrorCode::WRITE_FAILED;
        }

        auto valueLength = static_cast<ssize_t>(offsets[rowCount] - offsets[0]);
        if (valueLength > 0) {
            // write values
            char *values = unsafe::UnsafeStringVector::GetValues(reinterpret_cast<VarcharVector *>(vector));
            if (Write(values, valueLength) != ErrorCode::SUCCESS) {
                LogError("Write values to %s failed.", filePath.c_str());
                return op::ErrorCode::WRITE_FAILED;
            }
        }
        return ErrorCode::SUCCESS;
    } else {
        auto length = static_cast<ssize_t>(rowCount * sizeof(T));
        T *values = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<T> *>(vector));
        if (Write(values, length) != ErrorCode::SUCCESS) {
            LogError("Write values to %s failed.", filePath.c_str());
            return ErrorCode::WRITE_FAILED;
        }
        return ErrorCode::SUCCESS;
    }
}

ErrorCode SpillWriter::Write(void *buf, size_t length)
{
    size_t bytesWritten = 0;
    while (bytesWritten < length) {
        auto expectBytes = length - bytesWritten;
        ssize_t actualBytes = write(fd, static_cast<char *>(buf) + bytesWritten, expectBytes);
        if (actualBytes <= 0) {
            auto errorNum = errno;
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Write to %s failed since %s, expect write bytes is %lld but actual write bytes is %lld.",
                     filePath.c_str(), errorBuf, expectBytes, actualBytes);
            return ErrorCode::WRITE_FAILED;
        }
        bytesWritten += actualBytes;
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::Close()
{
    ErrorCode result = ErrorCode::SUCCESS;
    if (writeBufferOffset != 0) {
        if (Write(writeBuffer, writeBufferOffset) != ErrorCode::SUCCESS) {
            LogError("Write buffer to %s failed.", filePath.c_str());
            result = ErrorCode::WRITE_FAILED;
        } else {
            fileLength += writeBufferOffset;
        }
        writeBufferOffset = 0;
    }
    close(fd);
    return result;
}
} // namespace op
} // namespace omniruntime
