/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: spill merger implementation
 */

#ifndef OMNI_RUNTIME_SPILL_MERGER_H
#define OMNI_RUNTIME_SPILL_MERGER_H

#include "type/data_types.h"
#include "vector/vector_batch.h"
#include "loser_tree.h"
#include "operator/util/operator_util.h"
#include "spill_tracker.h"
#include "io/ColumnWriter.hh"
#include "io/Decompression.hh"

namespace omniruntime {
namespace op {
static constexpr int32_t ERROR_BUFFER_SIZE = 128;
// The ORC-style compression header stores the block length in 23 bits
// (7 bits in byte 0 and 8 bits each in bytes 1 and 2). Keep the writer block
// comfortably below that wire-format limit.
static constexpr size_t SPILL_COMPRESSION_MAX_BLOCK_SIZE = (1U << 23) - 1;
static constexpr int32_t SPILL_COMPRESSION_BLOCK_SIZE = 4 * 1024 * 1024;

static void GetErrorMsg(int32_t errorCode, char *buffer, int32_t bufferLen)
{
    if (buffer == nullptr || bufferLen <= 0) return;

    const char* msg = strerror(errorCode);
    if (msg == nullptr) {
        msg = "Unknown error";
    }

    strncpy(buffer, msg, bufferLen - 1);
    buffer[bufferLen - 1] = '\0';
}

class SortOrder {
public:
    SortOrder() = default;

    SortOrder(bool sortAscending, bool sortNullsFirst) : sortAscending(sortAscending), sortNullsFirst(sortNullsFirst) {}

    ~SortOrder() = default;

    bool IsAscending() const
    {
        return sortAscending;
    }

    bool IsNullsFirst() const
    {
        return sortNullsFirst;
    }

private:
    bool sortAscending = true;
    bool sortNullsFirst = true;
};

struct SpillFileInfo {
    std::string filePath;
    uint64_t fileLength;
    uint64_t totalRowCount;
};

class SpillReader {
public:
    SpillReader(const type::DataTypes &dataTypes, const std::string &filePath, uint64_t fileLength,
        uint64_t totalRowCount, bool isSpillCompressEnabled, bool compareWithHashVal = false,
        bool deleteFileOnClose = true)
        : dataTypes(dataTypes), filePath(filePath), fileLength(fileLength), totalRowCount(totalRowCount),
          isSpillCompressEnabled(isSpillCompressEnabled), compareWithHashVal(compareWithHashVal),
          deleteFileOnClose(deleteFileOnClose)
    {}

    ~SpillReader();

    ErrorCode ReadVecBatch(std::unique_ptr<vec::VectorBatch> &vectorBatch, bool &isEnd);

    uint64_t GetFileLength() const
    {
        return fileLength;
    }

    std::string GetFilePath() const
    {
        return filePath;
    }

private:
    template <typename T> ErrorCode ReadVector(vec::BaseVector *vector, int32_t rowCount);

    ErrorCode ReadArrayVector(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount);

    ErrorCode ReadMapVector(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount);

    ErrorCode ReadRowVector(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount);

    ErrorCode ReadComplexVector(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount);

    ErrorCode Read(void *buf, size_t bufSize);

    ErrorCode ReadWithCompress(void *buf, size_t bufSize);

    DataTypes dataTypes;
    std::string filePath;
    uint64_t fileLength = 0;
    FILE *file = nullptr;
    uint64_t totalRowCount = 0;
    uint64_t rowOffset = 0;
    int32_t maxRowCount = 0; // for reuse vector batch memory

    char* currentBuffer_ = nullptr;
    char* prevBuffer_ = nullptr;
    int32_t remainLength = 0;
    bool isSpillCompressEnabled = false;
    bool compareWithHashVal = false;
    // A temporary merger must leave its input runs intact so its caller can
    // fall back to read-back merge when one input is corrupt.  The final
    // merger owns its inputs and removes them after they are consumed.
    bool deleteFileOnClose = true;
};

class SpillMergeStream {
public:
    static SpillMergeStream *Create(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs,
        SpillTracker *spillTracker, bool isSpillCompressEnabled, bool deleteFileOnClose = true)
    {
        auto *stream = new SpillMergeStream(dataTypes, fileInfo, sortCols, sortCompareFuncs, spillTracker,
            isSpillCompressEnabled, deleteFileOnClose);
        auto result = stream->GetNextBatch();
        if (result != ErrorCode::SUCCESS) {
            delete stream;
            return nullptr;
        }
        return stream;
    }

    static SpillMergeStream *Create(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs,
        SpillTracker *tracker, bool spillCompressEnabled, bool isCompareWithHashVal = false,
        bool deleteFileOnClose = true)
    {
        auto *stream = new SpillMergeStream(dataTypes, fileInfo, sortCols, sortCompareFuncs, tracker,
            spillCompressEnabled, isCompareWithHashVal, deleteFileOnClose);
        auto result = stream->GetNextBatch();
        if (result != ErrorCode::SUCCESS) {
            delete stream;
            return nullptr;
        }
        return stream;
    }

    ~SpillMergeStream()
    {
        delete reader;
    }

    int32_t CompareTo(const SpillMergeStream &other);

    bool operator < (const SpillMergeStream &other)
    {
        return CompareTo(other) < 0;
    }

    bool HasData()
    {
        return currentRowIdx < currentRowCount;
    }

    ErrorCode GetNextBatch()
    {
        bool isEnd = false;
        auto result = reader->ReadVecBatch(currentBatch, isEnd);
        currentBatchPtr = currentBatch.get();
        if (isEnd) {
            currentRowIdx = 0;
            currentRowCount = 0;
            // Release the batch buffer as soon as this stream is exhausted: an
            // exhausted stream is never selected by the loser tree (HasData()
            // is false), so nothing dereferences the batch afterwards. Without
            // this, every stream's peak-sized buffer stays resident until the
            // whole SpillMerger is destroyed.
            currentBatch.reset();
            currentBatchPtr = nullptr;
            return result;
        }

        currentRowIdx = 0;
        currentRowCount = currentBatchPtr->GetRowCount();
        return result;
    }

    VectorBatch *GetCurrentBatch()
    {
        return currentBatchPtr;
    }

    int32_t GetCurrentRowIdx()
    {
        return currentRowIdx;
    }

    int32_t GetCurrentRowIdx(bool &isLastRow)
    {
        isLastRow = (currentRowIdx == currentRowCount - 1);
        return currentRowIdx;
    }

    void Pop()
    {
        if (++currentRowIdx >= currentRowCount) {
            auto result = GetNextBatch();
            if (result != ErrorCode::SUCCESS) {
                std::string errMsg = "Read from spill file " + reader->GetFilePath() + " failed.";
                throw omniruntime::exception::OmniException("SPILL_READ_FAILED", errMsg);
            }
        }
    }

private:
    SpillMergeStream(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs,
        SpillTracker *spillTracker, bool isSpillCompressEnabled, bool deleteFileOnClose)
        : sortCols(sortCols), sortCompareFuncs(sortCompareFuncs), spillTracker(spillTracker)
    {
        reader = new SpillReader(dataTypes, fileInfo.filePath, fileInfo.fileLength, fileInfo.totalRowCount,
            isSpillCompressEnabled, false, deleteFileOnClose);
    }

    SpillMergeStream(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs,
        SpillTracker *spillTracker, bool isSpillCompressEnabled, bool isCompareWithHashVal,
        bool deleteFileOnClose)
        : sortCols(sortCols), sortCompareFuncsWithHashVal(sortCompareFuncs), spillTracker(spillTracker), compareWithHashVal(isCompareWithHashVal)
    {
        reader = new SpillReader(dataTypes, fileInfo.filePath, fileInfo.fileLength, fileInfo.totalRowCount,
            isSpillCompressEnabled, compareWithHashVal, deleteFileOnClose);
    }

    std::vector<int32_t> sortCols; // which columns in currentBatch will be used to sort ?
    std::vector<OperatorUtil::CompareFunc> sortCompareFuncs;
    std::vector<OperatorUtil::CompareFuncWithHashVal> sortCompareFuncsWithHashVal;
    SpillTracker *spillTracker = nullptr;
    SpillReader *reader = nullptr;
    std::unique_ptr<vec::VectorBatch> currentBatch = nullptr;
    vec::VectorBatch *currentBatchPtr = nullptr;
    int32_t currentRowIdx = 0;
    int32_t currentRowCount = 0;
    bool isSpillCompressEnabled = false;
    bool compareWithHashVal = false;
};

class SpillMerger {
public:
    static SpillMerger *Create(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
        const std::vector<SortOrder> &sortOrders, SpillTracker *spillTracker,
        const std::vector<SpillFileInfo> &spillFiles, bool isSpillCompressEnabled, bool compareWithHashVal = false,
        bool deleteFileOnClose = true)
    {
        std::vector<OperatorUtil::CompareFunc> sortCompareFuncs;
        std::vector<OperatorUtil::CompareFuncWithHashVal> sortCompareFuncsWithHashVal;
        if (compareWithHashVal) {
            SetCompareFunctionsWithHashVal(dataTypes, sortCols, sortOrders, sortCompareFuncsWithHashVal);
        } else {
            SetCompareFunctions(dataTypes, sortCols, sortOrders, sortCompareFuncs);
        }

        std::vector<SpillMergeStream *> streams;
        uint64_t totalRowCount = 0;
        for (auto &fileInfo : spillFiles) {
            auto stream = compareWithHashVal ? SpillMergeStream::Create(dataTypes, fileInfo, sortCols,
                sortCompareFuncsWithHashVal, spillTracker, isSpillCompressEnabled, compareWithHashVal,
                deleteFileOnClose)
                : SpillMergeStream::Create(dataTypes, fileInfo, sortCols, sortCompareFuncs, spillTracker,
                    isSpillCompressEnabled, deleteFileOnClose);
            if (stream == nullptr) {
                for (auto createdStream : streams) {
                    delete createdStream;
                }
                return nullptr;
            }
            streams.emplace_back(stream);
            totalRowCount += fileInfo.totalRowCount;
        }
        return new SpillMerger(streams, totalRowCount, spillTracker);
    }

    ~SpillMerger()
    {
        delete mergeStreams;
        delete spillTracker;
    }

    vec::VectorBatch *CurrentBatch()
    {
        currentStream = mergeStreams->Next();
        return currentStream->GetCurrentBatch();
    }

    vec::VectorBatch *CurrentBatchWithEqual(bool &isEqual)
    {
        auto stream = mergeStreams->NextWithEqual(isEqual);
        if (stream == nullptr) {
            return nullptr;
        }
        currentStream = stream;
        return currentStream->GetCurrentBatch();
    }

    int32_t CurrentRowIndex()
    {
        return currentStream->GetCurrentRowIdx();
    }

    int32_t CurrentRowIndex(bool &isLastRow)
    {
        return currentStream->GetCurrentRowIdx(isLastRow);
    }

    void Pop()
    {
        currentStream->Pop();
    }

    uint64_t GetTotalRowCount()
    {
        return totalRowCount;
    }

private:
    static void SetCompareFunctionsWithHashVal(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
        const std::vector<SortOrder> &sortOrders, std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs);

    static void SetCompareFunctions(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
        const std::vector<SortOrder> &sortOrders, std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs);

    template <typename T>
    static void SetCompareFunctionWithHashVal(bool isAscending, bool isNullsFirst,
        std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs);

    template <typename T>
    static void SetCompareFunction(bool isAscending, bool isNullsFirst,
        std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs);

    SpillMerger(const std::vector<SpillMergeStream *> &streams, uint64_t totalRowCount, SpillTracker *spillTracker)
        : mergeStreams(new LoserTree<SpillMergeStream>(streams)),
          totalRowCount(totalRowCount),
          spillTracker(spillTracker)
    {}

    LoserTree<SpillMergeStream> *mergeStreams;
    uint64_t totalRowCount = 0;
    SpillTracker *spillTracker = nullptr;
    SpillMergeStream *currentStream = nullptr;
};
}
}

#endif // OMNI_RUNTIME_SPILL_MERGER_H
