/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: window implementations
 */
#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <vector>
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/pages_index.h"
#include "operator/spill/spiller.h"
#include "operator/spill/spill_merger.h"
#include "type/data_types.h"
#include "window_function_options.h"
#include "window_partition.h"

namespace omniruntime {
namespace op {
class WindowOperatorFactory : public OperatorFactory {
public:
    WindowOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,
        int32_t *windowFunctionTypes, int32_t windowFunctionCount, int32_t *partitionCols, int32_t partitionCount,
        int32_t *preGroupedCols, int32_t preGroupedCount, int32_t *sortCols, int32_t *sortAscendings,
        int32_t *sortNullFirsts, int32_t sortColCount, int32_t preSortedChannelPrefix, int32_t expectedPositions,
        const type::DataTypes &allTypes, int32_t *argumentChannels, int32_t argumentChannelsCount,
        int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField, int32_t *windowFrameStartChannelsField,
        int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField, const OperatorConfig &operatorConfig,
        WindowFunctionOptions *windowFunctionOptionsField = nullptr,
        const config::QueryConfig &queryConfig = config::QueryConfig{});

    ~WindowOperatorFactory() override;

    static WindowOperatorFactory *CreateWindowOperatorFactory(const type::DataTypes &sourceTypesField,
        int32_t *outputColsField, int32_t outputColsCountField, int32_t *windowFunctionTypesField,
        int32_t windowFunctionCountField, int32_t *partitionColsField, int32_t partitionCountField,
        int32_t *preGroupedColsField, int32_t preGroupedCountField, int32_t *sortColsField,
        int32_t *sortAscendingsField, int32_t *sortNullFirstsField, int32_t sortColCountField,
        int32_t preSortedChannelPrefixField, int32_t expectedPositionsField, const type::DataTypes &allTypesField,
        int32_t *argumentChannelsField, int32_t argumentChannelsCountField, int32_t *windowFrameTypesField,
        int32_t *windowFrameStartTypesField, int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField,
        int32_t *windowFrameEndChannelsField, WindowFunctionOptions *windowFunctionOptionsField = nullptr);

    static WindowOperatorFactory *CreateWindowOperatorFactory(const type::DataTypes &sourceTypesField,
        int32_t *outputColsField, int32_t outputColsCountField, int32_t *windowFunctionTypesField,
        int32_t windowFunctionCountField, int32_t *partitionColsField, int32_t partitionCountField,
        int32_t *preGroupedColsField, int32_t preGroupedCountField, int32_t *sortColsField,
        int32_t *sortAscendingsField, int32_t *sortNullFirstsField, int32_t sortColCountField,
        int32_t preSortedChannelPrefixField, int32_t expectedPositionsField, const type::DataTypes &allTypesField,
        int32_t *argumentChannelsField, int32_t argumentChannelsCountField, int32_t *windowFrameTypesField,
        int32_t *windowFrameStartTypesField, int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField,
        int32_t *windowFrameEndChannelsField, const OperatorConfig &operatorConfig,
        WindowFunctionOptions *windowFunctionOptionsField = nullptr,
        const config::QueryConfig &queryConfig = config::QueryConfig{});

    Operator *CreateOperator() override;

    const DataTypes &GetSourceTypes() const
    {
        return sourceTypes;
    }

    int32_t GetTypesCount() const
    {
        return sourceTypes.GetSize();
    }

    int32_t *GetOutputCols() const
    {
        return const_cast<int32_t *>(outputCols.data());
    }

    int32_t GetOutputColsCount() const
    {
        return outputColsCount;
    }

    int32_t *GetWindowFunctionTypes() const
    {
        return const_cast<int32_t *>(windowFunctionTypes.data());
    }

    int32_t GetWindowFunctionCount() const
    {
        return windowFunctionCount;
    }

    int32_t *GetPartitionCols() const
    {
        return const_cast<int32_t *>(partitionCols.data());
    }

    int32_t GetPartitionCount() const
    {
        return partitionCount;
    }

    int32_t *GetPreGroupedCols() const
    {
        return const_cast<int32_t *>(preGroupedCols.data());
    }

    int32_t GetPreGroupedCount() const
    {
        return preGroupedCount;
    }

    int32_t *GetSortCols() const
    {
        return const_cast<int32_t *>(sortCols.data());
    }

    int32_t *GetSortAscendings() const
    {
        return const_cast<int32_t *>(sortAscendings.data());
    }

    int32_t *GetSortNullFirsts() const
    {
        return const_cast<int32_t *>(sortNullFirsts.data());
    }

    int32_t GetSortColCount() const
    {
        return sortColCount;
    }

    int32_t GetPreSortedChannelPrefix() const
    {
        return preSortedChannelPrefix;
    }

    int32_t GetExpectedPositions() const
    {
        return expectedPositions;
    }

    const DataTypes &GetAllTypes() const
    {
        return allTypes;
    }

    int32_t GetAllCount() const
    {
        return allTypes.GetSize();
    }

    int32_t *GetArgumentChannels() const
    {
        return const_cast<int32_t *>(argumentChannels.data());
    }

    int32_t GetArgumentChannelsCount() const
    {
        return argumentChannelsCount;
    }

    int32_t *GetWindowFrameTypes() const
    {
        return const_cast<int32_t *>(windowFrameTypes.data());
    }

    int32_t *GetWindowFrameStartTypes() const
    {
        return const_cast<int32_t *>(windowFrameStartTypes.data());
    }

    int32_t *GetWindowFrameStartChannels() const
    {
        return const_cast<int32_t *>(windowFrameStartChannels.data());
    }

    int32_t *GetWindowFrameEndTypes() const
    {
        return const_cast<int32_t *>(windowFrameEndTypes.data());
    }

    int32_t *GetWindowFrameEndChannels() const
    {
        return const_cast<int32_t *>(windowFrameEndChannels.data());
    }

    WindowFunctionOptions *GetWindowFunctionOptions() const
    {
        return const_cast<WindowFunctionOptions *>(windowFunctionOptions.data());
    }

    OmniStatus Init();

private:
    DataTypes sourceTypes;
    std::vector<int32_t> outputCols;
    int32_t outputColsCount;
    std::vector<int32_t> windowFunctionTypes;
    int32_t windowFunctionCount;
    std::vector<int32_t> partitionCols;
    int32_t partitionCount;
    std::vector<int32_t> preGroupedCols;
    int32_t preGroupedCount;
    std::vector<int32_t> sortCols;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    int32_t sortColCount;
    int32_t preSortedChannelPrefix;
    int32_t expectedPositions;
    DataTypes allTypes;
    std::vector<int32_t> argumentChannels;
    int32_t argumentChannelsCount;
    std::vector<int32_t> windowFrameTypes;
    std::vector<int32_t> windowFrameStartTypes;
    std::vector<int32_t> windowFrameStartChannels;
    std::vector<int32_t> windowFrameEndTypes;
    std::vector<int32_t> windowFrameEndChannels;
    std::vector<WindowFunctionOptions> windowFunctionOptions;
    OperatorConfig operatorConfig;
};

class WindowOperator : public Operator {
public:
    WindowOperator(const type::DataTypes &sourceTypes, std::vector<int32_t> &outputCols, int32_t outputColsCount,
        std::vector<int32_t> &windowFunctionTypes, int32_t windowFunctionCount, std::vector<int32_t> &partitionCols,
        int32_t partitionCount, std::vector<int32_t> &preGroupedCols, int32_t preGroupedCount,
        std::vector<int32_t> &sortCols, std::vector<int32_t> &sortAscendings, std::vector<int32_t> &sortNullFirsts,
        int32_t sortColCount, int32_t preSortedChannelPrefix, int32_t expectedPositions,
        const type::DataTypes &allTypes, std::vector<int32_t> &argumentChannels, int32_t argumentChannelsCount,
        const std::vector<int32_t> &windowFrameTypes, const std::vector<int32_t> &windowFrameStartTypes,
        const std::vector<int32_t> &windowFrameStartChannels, const std::vector<int32_t> &windowFrameEndTypes,
        const std::vector<int32_t> &windowFrameEndChannels,
        const std::vector<WindowFunctionOptions> &windowFunctionOptions, const OperatorConfig &operatorConfig,
        const config::QueryConfig &queryConfig);

    ~WindowOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;
    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    uint64_t GetSpilledBytes() override;

    void SortPagesIndexIfNecessary();
    void FinishPagesIndex();

    OmniStatus Init() override;

private:
    type::DataTypes sourceTypes;
    std::vector<type::DataTypePtr> outputTypes;
    int32_t typesCount;
    std::vector<int32_t> outputCols;
    int32_t outputColsCount;
    std::vector<int32_t> windowFunctionTypes;
    int32_t windowFunctionCount;
    std::vector<int32_t> partitionCols;
    int32_t partitionCount;
    std::vector<int32_t> preGroupedCols;
    int32_t preGroupedCount;
    std::vector<int32_t> originSortCols;
    int32_t originSortColCount;
    std::vector<int32_t> sortCols;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    int32_t sortColCount;
    int32_t preSortedChannelPrefix;
    int32_t expectedPositions;
    type::DataTypes allTypes;
    std::unique_ptr<PagesIndex> pagesIndex;
    omniruntime::vec::VectorBatch *pendingInput;
    std::unique_ptr<PagesHashStrategy> preGroupedPartitionHashStrategy = nullptr;
    std::unique_ptr<PagesHashStrategy> unGroupedPartitionHashStrategy = nullptr;
    std::unique_ptr<PagesHashStrategy> preSortedPartitionHashStrategy = nullptr;
    std::unique_ptr<PagesHashStrategy> peerGroupHashStrategy = nullptr;
    std::unique_ptr<VectorBatch> inputVecBatchForAgg = nullptr;
    std::unique_ptr<WindowPartition> partition;
    std::vector<std::unique_ptr<WindowFunction>> windowFunctions;
    std::vector<int32_t> argumentChannels;
    int32_t argumentChannelsCount;
    const std::vector<int32_t> &windowFrameTypes;
    const std::vector<int32_t> &windowFrameStartTypes;
    const std::vector<int32_t> &windowFrameStartChannels;
    const std::vector<int32_t> &windowFrameEndTypes;
    const std::vector<int32_t> &windowFrameEndChannels;
    const std::vector<WindowFunctionOptions> &windowFunctionOptions;
    OperatorConfig operatorConfig;
    bool isOverflowAsNull;

    void Initialization();
    void ProcessData(omniruntime::vec::VectorBatch *&outputVecBatch, int32_t rowCount);
    void ProcessDataFromDisk(omniruntime::vec::VectorBatch *&outputVecBatch, int32_t rowCount);
    void PrepareOutput();
    ErrorCode SpillToDisk();
    void GetOutputFromMemory(VectorBatch **outputVecBatch);
    void GetOutputFromDisk(VectorBatch **outputVecBatch);
    void Sort();
    void PaddingPartitionVecBatch(vec::VectorBatch *partitionVecBatch, int32_t rowIdx);
    template <typename T> void PaddingPartitionVector(vec::BaseVector *groupedVector, int32_t rowIdx, int32_t colIdx);
    void WindowOperator::PaddingPartitionArrayVector(vec::BaseVector *groupedVector, int32_t rowIdx, int32_t colIdx);
    void WindowOperator::PaddingPartitionRowVector(vec::BaseVector *groupedVector, int32_t rowIdx, int32_t colIdx);
    bool ProcessNextWindowPartition();
    bool IsSamePartition(VectorBatch *lastBatch, int32_t lastIdx);
    vec::VectorBatch* createSpillVectorBatch(int32_t rowCnt, int32_t offset);

    bool hasPrepare = false;
    size_t totalRowCount = 0;
    size_t rowCountOutputted = 0;
    size_t maxRowCount;
    int finalOutputColsCount = 0;

    // for spill
    bool hasSpill = false;
    Spiller *spiller = nullptr;
    SpillMerger *spillMerger = nullptr;
    bool canInplaceSort = false;
    vec::VectorBatch *currentBatch = nullptr;
    int32_t currentRowIdx = 0;
    int32_t partitionRowCount = 0;
    int32_t partitionOutputted = 0;
    size_t maxRowCountPerVecBatch = 0;
    uint64_t spilledBytes = 0;
    vec::VectorBatch *spillOutBatch = nullptr;
    int32_t spillBatchOffset = 0;
};

int32_t FindGroupEnd(PagesIndex *pagesIndex, PagesHashStrategy *pagesHashStrategy, int32_t startPosition);
}
}
#endif
