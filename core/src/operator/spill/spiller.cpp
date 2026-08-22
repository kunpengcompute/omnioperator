/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
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

ErrorCode Spiller::Spill(AggregationSort *aggregationSort, Operator* op, bool comapreWithHashVal)
{
    size_t totalRowCount = aggregationSort->GetRowCount();
    if (totalRowCount <= 0) {
        return ErrorCode::SUCCESS;
    }

    auto lockedStats = op != nullptr ? &op->stats() : nullptr;

    // create spill writer object
    auto writer = std::make_unique<SpillWriter>(dataTypes, dirPaths[0], writeBufferSize, isSpillCompressEnabled);

    try {
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
        aggregationSort->SetSpillVectorBatch(spillVecBatchPtr, totalRowOffset, comapreWithHashVal);
        auto vecBatchSize = CollectVecBatchSize(spillVecBatchPtr);
        if (isSpillCompressEnabled) {
            if (writer->getTotalCompressBytes() + vecBatchSize> UINT64_MAX) {
                spillFiles.emplace_back(writer->GetSpillFileInfo());
                return ErrorCode::EXCEED_SPILL_THRESHOLD;
            }
        } else if (spillTracker->CheckIfExceedAndReserve(vecBatchSize)) {
            spillFiles.emplace_back(writer->GetSpillFileInfo());
            return ErrorCode::EXCEED_SPILL_THRESHOLD;
        }

        int64_t cpuTimeStartNs = ThreadCpuNanos();

        auto result = writer->WriteVecBatch(spillVecBatchPtr, vecBatchSize);

        const int64_t cpuTimeSegment = ThreadCpuNanos() - cpuTimeStartNs;
        if (lockedStats != nullptr) {
            lockedStats->AddSpilledBytes(vecBatchSize, rowCount, cpuTimeSegment);
        }
        if (result != ErrorCode::SUCCESS) {
            spillFiles.emplace_back(writer->GetSpillFileInfo());
            return result;
        }
        totalRowOffset += rowCount;
    }
    auto result = writer->Close();
    spillFiles.emplace_back(writer->GetSpillFileInfo());
    writer.reset();
    return result;
    } catch (...) {
        spillFiles.emplace_back(writer->GetSpillFileInfo());
        throw;
    }
}

ErrorCode Spiller::Spill(PagesIndex *pagesIndex, bool canInplaceSort, bool canRadixSort, Operator* op)
{
    int64_t totalRowCount = pagesIndex->GetRowCount();
    if (totalRowCount <= 0) {
        return ErrorCode::SUCCESS;
    }

    auto lockedStats = op != nullptr ? &op->stats() : nullptr;

    // create spill writer object
    auto writer = std::make_unique<SpillWriter>(dataTypes, dirPaths[0], writeBufferSize, isSpillCompressEnabled);

    try {
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
        if (isSpillCompressEnabled) {
            if (writer->getTotalCompressBytes() + vecBatchSize> UINT64_MAX) {
                spillFiles.emplace_back(writer->GetSpillFileInfo());
                return ErrorCode::EXCEED_SPILL_THRESHOLD;
            }
        } else if (spillTracker->CheckIfExceedAndReserve(vecBatchSize)) {
            spillFiles.emplace_back(writer->GetSpillFileInfo());
            return ErrorCode::EXCEED_SPILL_THRESHOLD;
        }

        int64_t cpuTimeStartNs = ThreadCpuNanos();
        auto result = writer->WriteVecBatch(spillVecBatchPtr, vecBatchSize);

        const int64_t cpuTimeSegment = ThreadCpuNanos() - cpuTimeStartNs;
        if (lockedStats != nullptr) {
            lockedStats->AddSpilledBytes(vecBatchSize, rowCount, cpuTimeSegment);
        }

        if (result != ErrorCode::SUCCESS) {
            spillFiles.emplace_back(writer->GetSpillFileInfo());
            return result;
        }
        totalRowOffset += rowCount;
    }

    auto result = writer->Close();
    spillFiles.emplace_back(writer->GetSpillFileInfo());
    writer.reset();
    return result;
    } catch (...) {
        spillFiles.emplace_back(writer->GetSpillFileInfo());
        throw;
    }
}


uint64_t Spiller::CollectVecBatchSize(vec::VectorBatch *vectorBatch)
{
    uint64_t result = sizeof(int32_t); // for row count size
    int32_t vecCount = vectorBatch->GetVectorCount();
    for (int32_t i = 0; i < vecCount; i++) {
        auto vector = vectorBatch->Get(i);
        auto dataType = dataTypes.GetType(i);
        int32_t rowCount = vector->GetSize();
        result += CollectComplexVectorSize(dataType, vector, rowCount);
    }
    return result;
}

uint64_t Spiller::CollectComplexVectorSize(const DataTypePtr& dataType, vec::BaseVector* vector, int32_t rowCount)
{
    switch (dataType->GetId()) {
    case OMNI_BOOLEAN:
        return CollectVectorSize<bool>(vector);
    case OMNI_INT:
    case OMNI_DATE32:
        return CollectVectorSize<int32_t>(vector);
    case OMNI_SHORT:
        return CollectVectorSize<int16_t>(vector);
    case OMNI_LONG:
    case OMNI_DECIMAL64:
    case OMNI_TIMESTAMP:
        return CollectVectorSize<int64_t>(vector);
    case OMNI_DOUBLE:
        return CollectVectorSize<double>(vector);
    case OMNI_FLOAT:
        return CollectVectorSize<float>(vector);
    case OMNI_DECIMAL128:
        return CollectVectorSize<Decimal128>(vector);
    case OMNI_VARBINARY:
    case OMNI_VARCHAR:
    case OMNI_CHAR:
        return CollectVectorSize<std::string_view>(vector);
    case OMNI_BYTE:
        return CollectVectorSize<int8_t>(vector);
    case OMNI_ARRAY:
        return CollectArrayVectorSize(dataType, vector, rowCount);
    case OMNI_MAP:
        return CollectMapVectorSize(dataType, vector, rowCount);
    case OMNI_ROW:
        return CollectRowVectorSize(dataType, vector, rowCount);
    default:
        std::string errStr = "Do not support the data type" + std::to_string(dataType->GetId()) +
            " in CollectComplexVectorSize.";
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
    }
}

uint64_t Spiller::CollectArrayVectorSize(const DataTypePtr &arrayType, vec::BaseVector *vector, int32_t rowCount)
{
    auto arrayVec = static_cast<omniruntime::vec::ArrayVector*>(vector);
    // nulls
    uint64_t result = BitUtil::Nbytes(rowCount);

    // offsets
    result += (rowCount + 1) * sizeof(int64_t);

    // elementRowCount = last offset
    int64_t* rawOffsets = arrayVec->GetOffsets();
    int32_t elementRowCount = static_cast<int32_t>(rawOffsets[rowCount]);

    // element vector
    auto elementVec = arrayVec->GetElementVector();
    if (!elementVec) {
        LogError("ArrayVector has null element vector in size collection");
        return result;
    }

    auto elementType = std::dynamic_pointer_cast<ArrayType>(arrayType)->ElementType();
    result += CollectComplexVectorSize(elementType, elementVec.get(), elementRowCount);

    return result;
}

uint64_t Spiller::CollectMapVectorSize(const DataTypePtr &mapType, vec::BaseVector *vector, int32_t rowCount)
{
    auto mapVec = static_cast<omniruntime::vec::MapVector *>(vector);
    uint64_t result = BitUtil::Nbytes(rowCount);
    result += (rowCount + 1) * sizeof(int64_t);

    int64_t *rawOffsets = mapVec->GetOffsets();
    int32_t entryCount = static_cast<int32_t>(rawOffsets[rowCount]);

    auto mapTypePtr = std::dynamic_pointer_cast<MapType>(mapType);
    if (!mapTypePtr) {
        LogError("CollectMapVectorSize: dataType is not MapType");
        return result;
    }
    auto keyVec = mapVec->GetKeyVector();
    auto valueVec = mapVec->GetValueVector();
    if (keyVec) {
        result += CollectComplexVectorSize(mapTypePtr->Key(), keyVec.get(), entryCount);
    }
    if (valueVec) {
        result += CollectComplexVectorSize(mapTypePtr->Value(), valueVec.get(), entryCount);
    }
    return result;
}

uint64_t Spiller::CollectRowVectorSize(const DataTypePtr &rowType, vec::BaseVector *vector, int32_t rowCount)
{
    auto rowVec = static_cast<omniruntime::vec::RowVector *>(vector);
    uint64_t result = BitUtil::Nbytes(rowCount);

    auto rowTypePtr = std::dynamic_pointer_cast<RowType>(rowType);
    if (!rowTypePtr) {
        LogError("CollectRowVectorSize: dataType is not RowType");
        return result;
    }
    auto &children = rowVec->Children();
    for (size_t i = 0; i < children.size(); i++) {
        if (i < static_cast<size_t>(rowTypePtr->Size()) && children[i]) {
            result += CollectComplexVectorSize(rowTypePtr->Type(i), children[i].get(), rowCount);
        }
    }
    return result;
}

    bool Spiller::isSpillCompressEnable() const {
        return isSpillCompressEnabled;
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
    if (snprintf(filePathChars, fileNameLen, "%s/%d-%u-%s", dirPathChars, pid, tid,
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
        close(tempFd);
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
        if (IsSpillCompressEnabled) {
            InitCompressStream();
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
        auto result = WriteComplexVectorToBuffer(dataTypes.GetType(i), vector, rowCount, writeOffset);
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
    memcpy(writeBuffer + writeOffset, nulls, nullsSize);
    writeOffset += nullsSize;

    if constexpr (std::is_same_v<T, std::string_view>) {
        // write offsets
        auto valueOffsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector));
        auto offsetLength = (static_cast<ssize_t>(rowCount) + 1) * sizeof(int32_t);
        memcpy(writeBuffer + writeOffset, valueOffsets, offsetLength);
        writeOffset += offsetLength;

        // write values
        char *values = unsafe::UnsafeStringVector::GetValues(reinterpret_cast<VarcharVector *>(vector));
        auto valueLength = static_cast<ssize_t>(valueOffsets[rowCount] - valueOffsets[0]);
        memcpy(writeBuffer + writeOffset, values, valueLength);
        writeOffset += valueLength;
        return ErrorCode::SUCCESS;
    } else {
        auto length = static_cast<ssize_t>(rowCount * sizeof(T));
        T *values = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<T> *>(vector));
        memcpy(writeBuffer + writeOffset, values, length);
        writeOffset += length;
        return ErrorCode::SUCCESS;
    }
}

ErrorCode SpillWriter::WriteArrayVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset)
{
    auto arrayType = std::dynamic_pointer_cast<ArrayType>(dataType);
    auto arrayVec = static_cast<omniruntime::vec::ArrayVector*>(vector);

    // nulls
    uint8_t* nulls = unsafe::UnsafeBaseVector::GetNulls(arrayVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    memcpy(writeBuffer + writeOffset, nulls, nullsSize);
    writeOffset += nullsSize;

    // offsets (int64_t[RowCount + 1])
    int64_t* offsets = arrayVec->GetOffsets();
    ssize_t offsetsSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int64_t));

    memcpy(writeBuffer + writeOffset, offsets, static_cast<size_t>(offsetsSize));
    writeOffset += offsetsSize;

    // element vector
    auto elementVec = arrayVec->GetElementVector();
    if (!elementVec) {
        LogError("ArrayVector has null element vector");
        return ErrorCode::WRITE_FAILED;
    }

    int32_t elementRowCount = static_cast<int32_t>(offsets[rowCount]);
    auto res = WriteComplexVectorToBuffer(arrayType->ElementType(), elementVec.get(), elementRowCount, writeOffset);
    return res;
}

ErrorCode SpillWriter::WriteMapVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset)
{
    auto mapType = std::dynamic_pointer_cast<MapType>(dataType);
    auto mapVec = static_cast<omniruntime::vec::MapVector *>(vector);
    if (!mapType) {
        LogError("WriteMapVectorToBuffer: dataType is not MapType");
        return ErrorCode::WRITE_FAILED;
    }
    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(mapVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    memcpy(writeBuffer + writeOffset, nulls, nullsSize);
    writeOffset += nullsSize;

    int64_t *offsets = mapVec->GetOffsets();
    ssize_t offsetsSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int64_t));
    memcpy(writeBuffer + writeOffset, offsets, static_cast<size_t>(offsetsSize));
    writeOffset += offsetsSize;

    int32_t entryCount = static_cast<int32_t>(offsets[rowCount]);
    auto keyVec = mapVec->GetKeyVector();
    auto valueVec = mapVec->GetValueVector();
    if (keyVec) {
        auto res = WriteComplexVectorToBuffer(mapType->Key(), keyVec.get(), entryCount, writeOffset);
        if (res != ErrorCode::SUCCESS) {
            return res;
        }
    }
    if (valueVec) {
        return WriteComplexVectorToBuffer(mapType->Value(), valueVec.get(), entryCount, writeOffset);
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::WriteRowVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset)
{
    auto rowType = std::dynamic_pointer_cast<RowType>(dataType);
    auto rowVec = static_cast<omniruntime::vec::RowVector *>(vector);
    if (!rowType) {
        LogError("WriteRowVectorToBuffer: dataType is not RowType");
        return ErrorCode::WRITE_FAILED;
    }
    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(rowVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    memcpy(writeBuffer + writeOffset, nulls, nullsSize);
    writeOffset += nullsSize;

    auto &children = rowVec->Children();
    for (size_t i = 0; i < children.size(); i++) {
        if (i < static_cast<size_t>(rowType->Size()) && children[i]) {
            auto res = WriteComplexVectorToBuffer(rowType->Type(static_cast<int>(i)), children[i].get(), rowCount, writeOffset);
            if (res != ErrorCode::SUCCESS) {
                return res;
            }
        }
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::WriteComplexVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset)
{
    auto result = ErrorCode::SUCCESS;
    switch (dataType->GetId()) {
        case OMNI_BOOLEAN:
            result = WriteVectorToBuffer<bool>(vector, rowCount, writeOffset);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            result = WriteVectorToBuffer<int32_t>(vector, rowCount, writeOffset);
            break;
        case OMNI_SHORT:
            result = WriteVectorToBuffer<int16_t>(vector, rowCount, writeOffset);
            break;
        case OMNI_LONG:
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP:
            result = WriteVectorToBuffer<int64_t>(vector, rowCount, writeOffset);
            break;
        case OMNI_DOUBLE:
            result = WriteVectorToBuffer<double>(vector, rowCount, writeOffset);
            break;
        case OMNI_FLOAT:
            result = WriteVectorToBuffer<float>(vector, rowCount, writeOffset);
            break;
        case OMNI_DECIMAL128:
            result = WriteVectorToBuffer<Decimal128>(vector, rowCount, writeOffset);
            break;
        case OMNI_VARBINARY:
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            result = WriteVectorToBuffer<std::string_view>(vector, rowCount, writeOffset);
            break;
        case OMNI_BYTE:
            result = WriteVectorToBuffer<int8_t>(vector, rowCount, writeOffset);
            break;
        case OMNI_ARRAY:
            result = WriteArrayVectorToBuffer(dataType, vector, rowCount, writeOffset);
            break;
        case OMNI_MAP:
            result = WriteMapVectorToBuffer(dataType, vector, rowCount, writeOffset);
            break;
        case OMNI_ROW:
            result = WriteRowVectorToBuffer(dataType, vector, rowCount, writeOffset);
            break;
        default: {
            std::string errStr = "Do not support the data type" + std::to_string(dataType->GetId()) +
                " in WriteComplexVectorToBuffer.";
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
        }
    }
    return result;
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
        auto result = WriteComplexVector(dataTypes.GetType(i), vector, rowCount);
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
    if (!IsSpillCompressEnabled) {
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

    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(vector);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);

    uint64_t totalSize = nullsSize;
    int32_t valuesSize = 0;
    int32_t offsetsSize = 0;

    if constexpr (std::is_same_v<T, std::string_view>) {
        offsetsSize = (rowCount + 1) * sizeof(int32_t);
        auto offsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector));
        valuesSize = offsets[rowCount] - offsets[0];
        totalSize += offsetsSize + valuesSize;
    } else {
        valuesSize = rowCount * sizeof(T);
        totalSize += valuesSize;
    }

    // 2. 调整临时缓冲区大小（复用，避免频繁内存分配）
    if (scratch_buffer.size() < totalSize) {
        scratch_buffer.resize(totalSize);
    }

    // 3. 将数据按顺序拷贝到临时缓冲区
    char *tmp_ptr = scratch_buffer.data();
    size_t current_offset = 0;

    // 拷贝 nulls
    memcpy(tmp_ptr + current_offset, nulls, nullsSize);
    current_offset += nullsSize;

    // 拷贝 offsets (仅VARCHAR)
    if constexpr (std::is_same_v<T, std::string_view>) {
        auto offsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector));
        memcpy(tmp_ptr + current_offset, offsets, offsetsSize);
        current_offset += offsetsSize;
    }

    // 拷贝 values
    if (valuesSize > 0) {
        void *values_ptr = nullptr;
        if constexpr (std::is_same_v<T, std::string_view>) {
            values_ptr = unsafe::UnsafeStringVector::GetValues(reinterpret_cast<VarcharVector *>(vector));
        } else {
            values_ptr = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<T> *>(vector));
        }
        memcpy(tmp_ptr + current_offset, values_ptr, valuesSize);
    }

    // 4. 一次性写入所有数据
    if (Write(scratch_buffer.data(), totalSize) != ErrorCode::SUCCESS) {
        LogError("Write merged data to file %s failed.", filePath.c_str());
        return ErrorCode::WRITE_FAILED;
    }

    return ErrorCode::SUCCESS;

}

ErrorCode SpillWriter::WriteArrayVector(const DataTypePtr& dataType, omniruntime::vec::BaseVector* vector, int32_t rowCount)
{
    auto arrayType = std::dynamic_pointer_cast<ArrayType>(dataType);
    auto arrayVec = static_cast<omniruntime::vec::ArrayVector*>(vector);

    uint8_t* nulls = unsafe::UnsafeBaseVector::GetNulls(vector);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    if (Write(nulls, nullsSize) != ErrorCode::SUCCESS) {
        LogError("Write value nulls to %s failed.", filePath.c_str());
        return ErrorCode::WRITE_FAILED;
    }

    // write offsets
    int64_t* offsets = arrayVec->GetOffsets();
    ssize_t offsetsSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int64_t));
    if (Write(offsets, offsetsSize) != ErrorCode::SUCCESS) {
        LogError("Write value offsets to %s failed.", filePath.c_str());
        return op::ErrorCode::WRITE_FAILED;
    }

    auto valueLength = static_cast<ssize_t>(offsets[rowCount] - offsets[0]);
    if (valueLength > 0) {
        // write values
        auto elementVec = arrayVec->GetElementVector();
        if (!elementVec) {
            LogError("ArrayVector has null element vector");
            return ErrorCode::WRITE_FAILED;
        }
        int32_t elementRowCount = static_cast<int32_t>(offsets[rowCount]);
        auto res = WriteComplexVector(arrayType->ElementType(), elementVec.get(), elementRowCount);
        return res;
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::WriteMapVector(const DataTypePtr &dataType, omniruntime::vec::BaseVector *vector, int32_t rowCount)
{
    auto mapType = std::dynamic_pointer_cast<MapType>(dataType);
    auto mapVec = static_cast<omniruntime::vec::MapVector *>(vector);
    if (!mapType) {
        LogError("WriteMapVector: dataType is not MapType");
        return ErrorCode::WRITE_FAILED;
    }
    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(mapVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    if (Write(nulls, nullsSize) != ErrorCode::SUCCESS) {
        return ErrorCode::WRITE_FAILED;
    }
    int64_t *offsets = mapVec->GetOffsets();
    ssize_t offsetsSize = static_cast<ssize_t>((rowCount + 1) * sizeof(int64_t));
    if (Write(offsets, offsetsSize) != ErrorCode::SUCCESS) {
        return ErrorCode::WRITE_FAILED;
    }
    int32_t entryCount = static_cast<int32_t>(offsets[rowCount]);
    auto keyVec = mapVec->GetKeyVector();
    auto valueVec = mapVec->GetValueVector();
    if (keyVec) {
        auto res = WriteComplexVector(mapType->Key(), keyVec.get(), entryCount);
        if (res != ErrorCode::SUCCESS) {
            return res;
        }
    }
    if (valueVec) {
        return WriteComplexVector(mapType->Value(), valueVec.get(), entryCount);
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::WriteRowVector(const DataTypePtr &dataType, omniruntime::vec::BaseVector *vector, int32_t rowCount)
{
    auto rowType = std::dynamic_pointer_cast<RowType>(dataType);
    auto rowVec = static_cast<omniruntime::vec::RowVector *>(vector);
    if (!rowType) {
        LogError("WriteRowVector: dataType is not RowType");
        return ErrorCode::WRITE_FAILED;
    }
    uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(rowVec);
    int32_t nullsSize = BitUtil::Nbytes(rowCount);
    if (Write(nulls, nullsSize) != ErrorCode::SUCCESS) {
        return ErrorCode::WRITE_FAILED;
    }
    auto &children = rowVec->Children();
    for (size_t i = 0; i < children.size(); i++) {
        if (i < static_cast<size_t>(rowType->Size()) && children[i]) {
            auto res = WriteComplexVector(rowType->Type(static_cast<int>(i)), children[i].get(), rowCount);
            if (res != ErrorCode::SUCCESS) {
                return res;
            }
        }
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillWriter::WriteComplexVector(const DataTypePtr &dataType, omniruntime::vec::BaseVector* vector, int32_t rowCount)
{
    auto result = ErrorCode::SUCCESS;
    switch (dataType->GetId()) {
    case OMNI_BOOLEAN:
        result = WriteVector<bool>(vector, rowCount);
        break;
    case OMNI_INT:
    case OMNI_DATE32:
        result = WriteVector<int32_t>(vector, rowCount);
        break;
    case OMNI_SHORT:
        result = WriteVector<int16_t>(vector, rowCount);
        break;
    case OMNI_LONG:
    case OMNI_DECIMAL64:
    case OMNI_TIMESTAMP:
        result = WriteVector<int64_t>(vector, rowCount);
        break;
    case OMNI_DOUBLE:
        result = WriteVector<double>(vector, rowCount);
        break;
    case OMNI_FLOAT:
        result = WriteVector<float>(vector, rowCount);
        break;
    case OMNI_DECIMAL128:
        result = WriteVector<Decimal128>(vector, rowCount);
        break;
    case OMNI_VARBINARY:
    case OMNI_VARCHAR:
    case OMNI_CHAR:
        result = WriteVector<std::string_view>(vector, rowCount);
        break;
    case OMNI_BYTE:
        result = WriteVector<int8_t>(vector, rowCount);
        break;
    case OMNI_ARRAY:
        result = WriteArrayVector(dataType, vector, rowCount);
        break;
    case OMNI_MAP:
        result = WriteMapVector(dataType, vector, rowCount);
        break;
    case OMNI_ROW:
        result = WriteRowVector(dataType, vector, rowCount);
        break;
    default:
        {
            std::string errStr = "Do not support the data type" + std::to_string(dataType->GetId()) +
                " in WriteVecBatchToFile.";
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
        }
    }
    return result;
}

ErrorCode SpillWriter::Write(void *buf, size_t length)
{
    if (!IsSpillCompressEnabled) {
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
    size_t remaining_len = length;
    const char* data_ptr = static_cast<const char *>(buf);
    void* buffer = nullptr;
    int buffer_size = 0;
    while (remaining_len > 0 && compress_stream->Next(&buffer, &buffer_size)) {
        size_t write_len = std::min(remaining_len, static_cast<size_t>(buffer_size));
        memcpy(buffer, data_ptr, write_len);
        remaining_len -= write_len;
        data_ptr += write_len;

        if (remaining_len == 0 && write_len < static_cast<size_t>(buffer_size)) {
            compress_stream->BackUp(static_cast<int>(buffer_size - write_len));
        }
    }
    unflushed_size += length;
    if (unflushed_size >= FLUSH_THRESHOLD) {
        totalCompressBytes += compress_stream->flush();
        unflushed_size = 0;
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
    if (IsSpillCompressEnabled && compress_stream && unflushed_size > 0) {
        totalCompressBytes += compress_stream->flush();
        unflushed_size = 0;
    }
    close(fd);
    return result;
}

void SpillWriter::InitCompressStream()
{
    const int32_t compress_block_size = 32 * 1024 * 1024;
    omniSpark::WriterOptions writer_options;
    writer_options.setCompression(omniSpark::CompressionKind_LZ4);
    writer_options.setCompressionBlockSize(compress_block_size);
    writer_options.setCompressionStrategy(omniSpark::CompressionStrategy_COMPRESSION);
    out_stream = omniSpark::writeLocalFile(filePath);
    stream_factory = createStreamsFactory(writer_options, out_stream.get());
    compress_stream = stream_factory->createStream();
}

    uint64_t SpillWriter::getTotalCompressBytes() const {
        return totalCompressBytes;
    }
}
}
