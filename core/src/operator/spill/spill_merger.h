/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: spill merger implementation
 */

#ifndef OMNI_RUNTIME_SPILL_MERGER_H
#define OMNI_RUNTIME_SPILL_MERGER_H

#include <vector>
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

// LZ4 block size for compressed spill files; the reader sizes its decompress buffer to it.
// Kept below jemalloc's 8 MiB oversize threshold: larger buffers go to a dedicated arena that
// returns pages to the OS on every free, and spill opens many small files (q11 SF1000 wrote
// ~9k files of ~1 MB each), so a 32 MiB block paid an madvise + page faults per file and per block.
constexpr int32_t SPILL_COMPRESS_BLOCK_SIZE = 1 * 1024 * 1024;

struct SpillFileInfo {
    std::string filePath;
    uint64_t fileLength;
    uint64_t totalRowCount;
};

class SpillReader {
public:
    SpillReader(const type::DataTypes &dataTypes, const std::string &filePath, uint64_t fileLength,
        uint64_t totalRowCount, bool isSpillCompressEnabled, omniruntime::mem::OmniTaskMemoryPool *pool,
        bool compareWithHashVal = false)
        : dataTypes(dataTypes), filePath(filePath), fileLength(fileLength), totalRowCount(totalRowCount),
          isSpillCompressEnabled(isSpillCompressEnabled), compareWithHashVal(compareWithHashVal), pool_(pool)
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
    // Per-task pool threaded from the spilling operator (HashAggregation/Sort/Window) so the
    // deserialized spill VectorBatch — the actual data source for downstream — is pool-accounted.
    omniruntime::mem::OmniTaskMemoryPool *pool_ = nullptr;

    // Reused across compressed blocks (grow-only) instead of allocating per block.
    std::vector<char> compressedBuffer_;
    std::vector<char> decompressedBuffer_;
    const char* currentBuffer_ = nullptr;
    int32_t remainLength = 0;
    bool isSpillCompressEnabled = false;
    bool compareWithHashVal = false;
};

class SpillMergeStream {
public:
    static SpillMergeStream *Create(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs,
        SpillTracker *tracker, bool compressEnabled, omniruntime::mem::OmniTaskMemoryPool *pool)
    {
        auto *stream = new SpillMergeStream(dataTypes, fileInfo, sortCols, sortCompareFuncs, tracker, compressEnabled, pool);
        auto result = stream->GetNextBatch();
        if (result != ErrorCode::SUCCESS) {
            delete stream;
            return nullptr;
        }
        return stream;
    }

    static SpillMergeStream *Create(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs,
        SpillTracker *tracker, bool spillCompressEnabled, omniruntime::mem::OmniTaskMemoryPool *pool,
        bool isCompareWithHashVal = false)
    {
        auto *stream = new SpillMergeStream(dataTypes, fileInfo, sortCols, sortCompareFuncs, tracker, spillCompressEnabled, isCompareWithHashVal, pool);
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
        SpillTracker *spillTracker, bool isSpillCompressEnabled, omniruntime::mem::OmniTaskMemoryPool *pool)
        : sortCols(sortCols), sortCompareFuncs(sortCompareFuncs), spillTracker(spillTracker)
    {
        reader = new SpillReader(dataTypes, fileInfo.filePath, fileInfo.fileLength, fileInfo.totalRowCount, isSpillCompressEnabled, pool);
    }

    SpillMergeStream(const type::DataTypes &dataTypes, const SpillFileInfo &fileInfo,
        const std::vector<int32_t> &sortCols, std::vector<OperatorUtil::CompareFuncWithHashVal> &sortCompareFuncs,
        SpillTracker *spillTracker, bool isSpillCompressEnabled, bool isCompareWithHashVal,
        omniruntime::mem::OmniTaskMemoryPool *pool)
        : sortCols(sortCols), sortCompareFuncsWithHashVal(sortCompareFuncs), spillTracker(spillTracker), compareWithHashVal(isCompareWithHashVal)
    {
        reader = new SpillReader(dataTypes, fileInfo.filePath, fileInfo.fileLength, fileInfo.totalRowCount, isSpillCompressEnabled, pool, compareWithHashVal);
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
        const std::vector<SpillFileInfo> &spillFiles, bool isSpillCompressEnabled,
        omniruntime::mem::OmniTaskMemoryPool *pool, bool compareWithHashVal = false)
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
            auto stream = compareWithHashVal ? SpillMergeStream::Create(dataTypes, fileInfo, sortCols, sortCompareFuncsWithHashVal, spillTracker, isSpillCompressEnabled, pool, compareWithHashVal)
                            : SpillMergeStream::Create(dataTypes, fileInfo, sortCols, sortCompareFuncs, spillTracker, isSpillCompressEnabled, pool);
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
