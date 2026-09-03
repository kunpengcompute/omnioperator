/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * @Description: spiller implementation
 */

#ifndef OMNI_RUNTIME_SPILLER_H
#define OMNI_RUNTIME_SPILLER_H

#include "type/data_types.h"
#include "spill_merger.h"
#include "operator/pages_index.h"
#include "operator/aggregation/group_aggregation_sort.h"
#include "io/ColumnWriter.hh"

namespace omniruntime {
namespace op {
class SpillWriter {
public:
    SpillWriter(const type::DataTypes &dataTypes, const std::string &dirPath, uint64_t writeBufferSize = 0,
                bool IsSpillCompressEnabled = false)
        : dataTypes(dataTypes), dirPath(dirPath), writeBufferSize(writeBufferSize), writeBufferOffset(0),
          IsSpillCompressEnabled(IsSpillCompressEnabled)
    {
        if (writeBufferSize != 0) {
            writeBuffer = new char[writeBufferSize];
        }
    }

    ~SpillWriter()
    {
        delete[] writeBuffer;
    }

    ErrorCode WriteVecBatch(vec::VectorBatch *vectorBatch, uint64_t vectorBatchSize);

    SpillFileInfo GetSpillFileInfo()
    {
        SpillFileInfo file{ filePath, fileLength, totalRowCount };
        return file;
    }

    uint64_t getTotalCompressBytes() const;

    ErrorCode Close();

private:
    ErrorCode CreateTempFile();

    ErrorCode WriteVecBatchToBuffer(vec::VectorBatch *vectorBatch);

    template <typename T>
    ErrorCode WriteVectorToBuffer(vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset);

    ErrorCode WriteArrayVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset);

    ErrorCode WriteMapVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset);

    ErrorCode WriteRowVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset);

    ErrorCode WriteComplexVectorToBuffer(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount, int32_t &writeOffset);

    ErrorCode WriteVecBatchToFile(vec::VectorBatch *vectorBatch);

    template <typename T> ErrorCode WriteVector(omniruntime::vec::BaseVector *vector, int32_t rowCount);

    ErrorCode WriteArrayVector(const DataTypePtr& dataType, omniruntime::vec::BaseVector* vector, int32_t rowCount);

    ErrorCode WriteMapVector(const DataTypePtr &dataType, omniruntime::vec::BaseVector *vector, int32_t rowCount);

    ErrorCode WriteRowVector(const DataTypePtr &dataType, omniruntime::vec::BaseVector *vector, int32_t rowCount);

    ErrorCode WriteComplexVector(const DataTypePtr& dataType, omniruntime::vec::BaseVector* vector, int32_t rowCount);

    ErrorCode Write(void *buf, size_t length);

    void InitCompressStream();

    type::DataTypes dataTypes;
    std::string dirPath;
    int32_t fd = -1;
    std::string filePath;
    uint64_t writeBufferSize = 0;
    uint64_t writeBufferOffset = 0;
    char *writeBuffer = nullptr;
    uint64_t fileLength = 0;
    uint64_t totalRowCount = 0;

    std::unique_ptr<omniSpark::OutputStream> out_stream;
    std::unique_ptr<omniSpark::StreamsFactory> stream_factory;
    std::unique_ptr<omniSpark::BufferedOutputStream> compress_stream;
    std::vector<char> scratch_buffer;
    uint64_t unflushed_size = 0;
    constexpr static uint64_t FLUSH_THRESHOLD = 64 * 1024 * 1024;
    bool IsSpillCompressEnabled = false;
    uint64_t totalCompressBytes = 0;
};

class Spiller {
public:
    Spiller(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
        const std::vector<SortOrder> &sortOrders, const std::string &spillPath, uint64_t maxSpillBytes,
        uint64_t writeBufferSize = 0, bool isSpillCompressEnabled = false)
        : dataTypes(dataTypes), sortCols(sortCols), sortOrders(sortOrders), writeBufferSize(writeBufferSize),
          isSpillCompressEnabled(isSpillCompressEnabled)
    {
        dirPaths.emplace_back(spillPath);
        int32_t dataTypeCount = dataTypes.GetSize();
        for (int32_t i = 0; i < dataTypeCount; i++) {
            outputCols.emplace_back(i);
        }

        maxRowCountPerBatch = OperatorUtil::GetMaxRowCount(dataTypes.Get(), outputCols.data(), dataTypeCount);
        spillTracker = GetRootSpillTracker().CreateSpillTracker(maxSpillBytes);
    }

    ~Spiller()
    {
        spillFiles.clear();
    }

    ErrorCode Spill(PagesIndex *pagesIndex, bool canInplaceSort, bool canRadixSort, Operator* op);

    ErrorCode Spill(AggregationSort *aggregationSort, Operator* op, bool compareWithHashVal = false);

    std::vector<SpillFileInfo> FinishSpill()
    {
        return spillFiles;
    }

    SpillMerger *CreateSpillMerger(const std::vector<SpillFileInfo> &spillFiles, bool isSpillCompressEnabled)
    {
        auto merger = SpillMerger::Create(dataTypes, sortCols, sortOrders, spillTracker, spillFiles, isSpillCompressEnabled);
        return merger;
    }

    SpillMerger *CreateSpillMerger(const std::vector<SpillFileInfo> &spillFiles, bool spillCompressEnabled, bool comapreWithHashVal)
    {
        auto merger = SpillMerger::Create(dataTypes, sortCols, sortOrders, spillTracker, spillFiles, spillCompressEnabled, comapreWithHashVal);
        return merger;
    }

    uint64_t GetSpilledBytes()
    {
        return spillTracker->GetSpilledBytes();
    }

    SpillTracker *GetSpillTracker() const
    {
        return spillTracker;
    }

    // the spill files should be deleted if there is exception
    void RemoveSpillFiles()
    {
        for (const auto &spillFile : spillFiles) {
            if (!spillFile.filePath.empty()) {
                remove(spillFile.filePath.c_str());
            }
        }
    }

    bool isSpillCompressEnable() const;

private:
    uint64_t CollectVecBatchSize(vec::VectorBatch *vectorBatch);

    template <typename T> uint64_t CollectVectorSize(vec::BaseVector *vector);

    uint64_t CollectComplexVectorSize(const DataTypePtr &dataType, vec::BaseVector *vector, int32_t rowCount);

    uint64_t CollectArrayVectorSize(const DataTypePtr &arrayType, vec::BaseVector *vector, int32_t rowCount);

    uint64_t CollectMapVectorSize(const DataTypePtr &mapType, vec::BaseVector *vector, int32_t rowCount);

    uint64_t CollectRowVectorSize(const DataTypePtr &rowType, vec::BaseVector *vector, int32_t rowCount);

    DataTypes dataTypes;
    std::vector<int32_t> sortCols;
    std::vector<SortOrder> sortOrders;
    std::vector<std::string> dirPaths;
    std::vector<int32_t> outputCols;
    int32_t maxRowCountPerBatch;
    std::unique_ptr<vec::VectorBatch> spillVecBatch = nullptr;
    // Writers own sizeable buffers and compression streams. Keep only file metadata after Close().
    std::vector<SpillFileInfo> spillFiles;
    SpillTracker *spillTracker = nullptr;
    uint64_t writeBufferSize = 0;
    bool isSpillCompressEnabled = false;
};
}
}
#endif // OMNI_RUNTIME_SPILLER_H
