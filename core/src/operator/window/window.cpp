/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: window implementations
 */

#include "window.h"
#include "lead_lag_function.h"

using namespace std;
using namespace omniruntime::vec;
namespace omniruntime {
namespace op {
const int MID_SEARCH_FACTOR = 2;

WindowOperatorFactory::WindowOperatorFactory(const DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,
    int32_t *windowFunctionTypes, int32_t windowFunctionCount, int32_t *partitionCols, int32_t partitionCount,
    int32_t *preGroupedCols, int32_t preGroupedCount, int32_t *sortCols, int32_t *sortAscendings,
    int32_t *sortNullFirsts, int32_t sortColCount, int32_t preSortedChannelPrefix, int32_t expectedPositions,
    const DataTypes &allTypes, int32_t *argumentChannels, int32_t argumentChannelsCount, int32_t *windowFrameTypesField,
    int32_t *windowFrameStartTypesField, int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField,
    int32_t *windowFrameEndChannelsField, const OperatorConfig &operatorConfig,
    WindowFunctionOptions *windowFunctionOptionsField, const config::QueryConfig &queryConfig)
    : sourceTypes(sourceTypes),
      outputColsCount(outputColsCount),
      windowFunctionCount(windowFunctionCount),
      partitionCount(partitionCount),
      preGroupedCount(preGroupedCount),
      sortColCount(sortColCount),
      preSortedChannelPrefix(preSortedChannelPrefix),
      expectedPositions(expectedPositions),
      allTypes(allTypes),
      argumentChannelsCount(argumentChannelsCount),
      operatorConfig(operatorConfig)
{
    this->queryConfig_ = queryConfig;
    this->outputCols.insert(this->outputCols.begin(), outputCols, outputCols + outputColsCount);
    this->windowFunctionTypes.insert(this->windowFunctionTypes.begin(), windowFunctionTypes,
        windowFunctionTypes + windowFunctionCount);
    this->partitionCols.insert(this->partitionCols.begin(), partitionCols, partitionCols + partitionCount);
    this->preGroupedCols.insert(this->preGroupedCols.begin(), preGroupedCols, preGroupedCols + preGroupedCount);
    this->sortCols.insert(this->sortCols.begin(), sortCols, sortCols + sortColCount);
    this->sortAscendings.insert(this->sortAscendings.begin(), sortAscendings, sortAscendings + sortColCount);
    this->sortNullFirsts.insert(this->sortNullFirsts.begin(), sortNullFirsts, sortNullFirsts + sortColCount);
    this->argumentChannels.insert(this->argumentChannels.begin(), argumentChannels,
        argumentChannels + argumentChannelsCount);
    this->windowFrameTypes.insert(this->windowFrameTypes.begin(), windowFrameTypesField,
        windowFrameTypesField + windowFunctionCount); // each windowFunction has one windowFrame
    this->windowFrameStartTypes.insert(this->windowFrameStartTypes.begin(), windowFrameStartTypesField,
        windowFrameStartTypesField + windowFunctionCount);
    this->windowFrameStartChannels.insert(this->windowFrameStartChannels.begin(), windowFrameStartChannelsField,
        windowFrameStartChannelsField + windowFunctionCount);
    this->windowFrameEndTypes.insert(this->windowFrameEndTypes.begin(), windowFrameEndTypesField,
        windowFrameEndTypesField + windowFunctionCount);
    this->windowFrameEndChannels.insert(this->windowFrameEndChannels.begin(), windowFrameEndChannelsField,
        windowFrameEndChannelsField + windowFunctionCount);
    if (windowFunctionOptionsField != nullptr) {
        this->windowFunctionOptions.insert(this->windowFunctionOptions.begin(), windowFunctionOptionsField,
            windowFunctionOptionsField + windowFunctionCount);
    } else {
        this->windowFunctionOptions.resize(windowFunctionCount);
        for (int32_t i = 0; i < windowFunctionCount; ++i) {
            if (this->windowFunctionTypes[i] == OMNI_WINDOW_TYPE_NTH_VALUE) {
                this->windowFunctionOptions[i].nthValueOffset = this->windowFrameStartChannels[i];
            }
        }
    }
}

OmniStatus WindowOperatorFactory::Init()
{
    return OMNI_STATUS_NORMAL;
}

WindowOperatorFactory::~WindowOperatorFactory() = default;

WindowOperatorFactory *WindowOperatorFactory::CreateWindowOperatorFactory(const DataTypes &sourceTypesField,
    int32_t *outputColsField, int32_t outputColsCountField, int32_t *windowFunctionTypesField,
    int32_t windowFunctionCountField, int32_t *partitionColsField, int32_t partitionCountField,
    int32_t *preGroupedColsField, int32_t preGroupedCountField, int32_t *sortColsField, int32_t *sortAscendingsField,
    int32_t *sortNullFirstsField, int32_t sortColCountField, int32_t preSortedChannelPrefixField,
    int32_t expectedPositionsField, const DataTypes &allTypesField, int32_t *argumentChannelsField,
    int32_t argumentChannelsCountField, int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField,
    int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField,
    WindowFunctionOptions *windowFunctionOptionsField)
{
    OperatorConfig defaultConfig;
    auto operatorFactory = new WindowOperatorFactory(sourceTypesField, outputColsField, outputColsCountField,
        windowFunctionTypesField, windowFunctionCountField, partitionColsField, partitionCountField,
        preGroupedColsField, preGroupedCountField, sortColsField, sortAscendingsField, sortNullFirstsField,
        sortColCountField, preSortedChannelPrefixField, expectedPositionsField, allTypesField, argumentChannelsField,
        argumentChannelsCountField, windowFrameTypesField, windowFrameStartTypesField, windowFrameStartChannelsField,
        windowFrameEndTypesField, windowFrameEndChannelsField, defaultConfig, windowFunctionOptionsField);
    operatorFactory->Init();
    return operatorFactory;
}

WindowOperatorFactory *WindowOperatorFactory::CreateWindowOperatorFactory(const DataTypes &sourceTypesField,
    int32_t *outputColsField, int32_t outputColsCountField, int32_t *windowFunctionTypesField,
    int32_t windowFunctionCountField, int32_t *partitionColsField, int32_t partitionCountField,
    int32_t *preGroupedColsField, int32_t preGroupedCountField, int32_t *sortColsField, int32_t *sortAscendingsField,
    int32_t *sortNullFirstsField, int32_t sortColCountField, int32_t preSortedChannelPrefixField,
    int32_t expectedPositionsField, const DataTypes &allTypesField, int32_t *argumentChannelsField,
    int32_t argumentChannelsCountField, int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField,
    int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField,
    const OperatorConfig &operatorConfig, WindowFunctionOptions *windowFunctionOptionsField,
    const config::QueryConfig &queryConfig)
{
    auto operatorFactory = new WindowOperatorFactory(sourceTypesField, outputColsField, outputColsCountField,
        windowFunctionTypesField, windowFunctionCountField, partitionColsField, partitionCountField,
        preGroupedColsField, preGroupedCountField, sortColsField, sortAscendingsField, sortNullFirstsField,
        sortColCountField, preSortedChannelPrefixField, expectedPositionsField, allTypesField, argumentChannelsField,
        argumentChannelsCountField, windowFrameTypesField, windowFrameStartTypesField, windowFrameStartChannelsField,
        windowFrameEndTypesField, windowFrameEndChannelsField, operatorConfig, windowFunctionOptionsField,
        queryConfig);
    operatorFactory->Init();
    return operatorFactory;
}

Operator *WindowOperatorFactory::CreateOperator()
{
    auto windowOperator = new WindowOperator(sourceTypes, outputCols, outputColsCount, windowFunctionTypes,
        windowFunctionCount, partitionCols, partitionCount, preGroupedCols, preGroupedCount, sortCols, sortAscendings,
        sortNullFirsts, sortColCount, preSortedChannelPrefix, expectedPositions, allTypes, argumentChannels,
        argumentChannelsCount, windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes,
        windowFrameEndChannels, windowFunctionOptions, operatorConfig, queryConfig_);
    windowOperator->Init();
    return windowOperator;
}

WindowOperator::WindowOperator(const type::DataTypes &sourceTypes, std::vector<int32_t> &outputCols,
    int32_t outputColsCount, std::vector<int32_t> &windowFunctionTypes, int32_t windowFunctionCount,
    std::vector<int32_t> &partitionCols, int32_t partitionCount, std::vector<int32_t> &preGroupedCols,
    int32_t preGroupedCount, std::vector<int32_t> &sortCols, std::vector<int32_t> &sortAscendings,
    std::vector<int32_t> &sortNullFirsts, int32_t sortColCount, int32_t preSortedChannelPrefix,
    int32_t expectedPositions, const type::DataTypes &allTypes, std::vector<int32_t> &argumentChannels,
    int32_t argumentChannelsCount, const std::vector<int32_t> &windowFrameTypes,
    const std::vector<int32_t> &windowFrameStartTypes, const std::vector<int32_t> &windowFrameStartChannels,
    const std::vector<int32_t> &windowFrameEndTypes, const std::vector<int32_t> &windowFrameEndChannels,
    const std::vector<WindowFunctionOptions> &windowFunctionOptions, const OperatorConfig &operatorConfig,
    const config::QueryConfig &queryConfig)
    : sourceTypes(sourceTypes),
      typesCount(sourceTypes.GetSize()),
      outputCols(outputCols),
      outputColsCount(outputColsCount),
      windowFunctionTypes(windowFunctionTypes),
      windowFunctionCount(windowFunctionCount),
      partitionCols(partitionCols),
      partitionCount(partitionCount),
      preGroupedCols(preGroupedCols),
      preGroupedCount(preGroupedCount),
      originSortCols(sortCols),
      originSortColCount(sortColCount),
      sortColCount(sortColCount + partitionCount),
      preSortedChannelPrefix(preSortedChannelPrefix),
      expectedPositions(expectedPositions),
      allTypes(allTypes),
      pendingInput(nullptr),
      partition(nullptr),
      argumentChannels(argumentChannels),
      argumentChannelsCount(argumentChannelsCount),
      windowFrameTypes(windowFrameTypes),
      windowFrameStartTypes(windowFrameStartTypes),
      windowFrameStartChannels(windowFrameStartChannels),
      windowFrameEndTypes(windowFrameEndTypes),
      windowFrameEndChannels(windowFrameEndChannels),
      windowFunctionOptions(windowFunctionOptions),
      operatorConfig(operatorConfig)
{
    executionContext->SetConfig(queryConfig);
    for (int32_t i = 0; i < partitionCount; i++) {
        this->sortCols.push_back(partitionCols[i]);
        this->sortAscendings.push_back(true);
        this->sortNullFirsts.push_back(false);
    }
    for (int32_t i = partitionCount; i < partitionCount + sortColCount; i++) {
        this->sortCols.push_back(sortCols[i - partitionCount]);
        this->sortAscendings.push_back(sortAscendings[i - partitionCount]);
        this->sortNullFirsts.push_back(sortNullFirsts[i - partitionCount]);
    }

    if (sourceTypes.GetSize() == 1 && sourceTypes.GetType(0)->GetId() != OMNI_VARCHAR &&
        sourceTypes.GetType(0)->GetId() != OMNI_CHAR && sourceTypes.GetType(0)->GetId() != OMNI_BOOLEAN) {
        canInplaceSort = true;
    }
    isOverflowAsNull = operatorConfig.GetOverflowConfig()->IsOverflowAsNull();
}

OmniStatus WindowOperator::Init()
{
    OmniStatus ret = OMNI_STATUS_NORMAL;
    pagesIndex = std::make_unique<PagesIndex>(sourceTypes);
    for (int32_t i = 0; i < windowFunctionCount; i++) {
        auto type = windowFunctionTypes[i];
        auto windowFrame = std::make_unique<WindowFrameInfo>(static_cast<FrameType>(windowFrameTypes[i]),
            static_cast<FrameBoundType>(windowFrameStartTypes[i]), windowFrameStartChannels[i],
            static_cast<FrameBoundType>(windowFrameEndTypes[i]), windowFrameEndChannels[i]);
        switch (type) {
            case OMNI_WINDOW_TYPE_ROW_NUMBER:
                windowFunctions.push_back(std::move(make_unique<RowNumberFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i))));
                break;
            case OMNI_WINDOW_TYPE_RANK:
                windowFunctions.push_back(std::move(make_unique<RankFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i))));
                break;
            case OMNI_WINDOW_TYPE_PERCENT_RANK:
                windowFunctions.push_back(std::move(make_unique<PercentRankFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i))));
                break;
            case OMNI_WINDOW_TYPE_CUME_DIST:
                windowFunctions.push_back(std::move(make_unique<CumeDistFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i))));
                break;
            case OMNI_WINDOW_TYPE_LEAD: {
                const auto &options = windowFunctionOptions[i];
                int64_t leadOffset = windowFrameStartChannels[i];
                int32_t leadDefaultChannel = windowFrameEndChannels[i];
                bool leadHasDefault = (leadDefaultChannel >= 0);
                windowFunctions.push_back(std::move(make_unique<LeadFunction>(std::move(windowFrame),
                    sourceTypes.GetType(argumentChannels[i]), allTypes.GetType(sourceTypes.GetSize() + i),
                    argumentChannels[i], leadOffset, leadHasDefault, leadDefaultChannel, options.IsIgnoreNulls())));
                break;
            }
            case OMNI_WINDOW_TYPE_LAG: {
                const auto &options = windowFunctionOptions[i];
                int64_t lagOffset = windowFrameStartChannels[i];
                int32_t lagDefaultChannel = windowFrameEndChannels[i];
                bool lagHasDefault = (lagDefaultChannel >= 0);
                windowFunctions.push_back(std::move(make_unique<LagFunction>(std::move(windowFrame),
                    sourceTypes.GetType(argumentChannels[i]), allTypes.GetType(sourceTypes.GetSize() + i),
                    argumentChannels[i], lagOffset, lagHasDefault, lagDefaultChannel, options.IsIgnoreNulls())));
                break;
            }
            // for aggregate function we use AggregateType
            case OMNI_AGGREGATION_TYPE_SUM:
            case OMNI_AGGREGATION_TYPE_COUNT_COLUMN:
            case OMNI_AGGREGATION_TYPE_AVG:
            case OMNI_AGGREGATION_TYPE_SAMP:
            case OMNI_AGGREGATION_TYPE_STD_POP:
            case OMNI_AGGREGATION_TYPE_VAR_SAMP:
            case OMNI_AGGREGATION_TYPE_VAR_POP:
            case OMNI_AGGREGATION_TYPE_MAX:
            case OMNI_AGGREGATION_TYPE_MIN:
            case OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL:
            case OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL:
            case OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL:
            case OMNI_AGGREGATION_TYPE_LAST_IGNORENULL:
                windowFunctions.push_back(std::move(make_unique<AggregateWindowFunction>(argumentChannels[i], type,
                    sourceTypes.GetType(argumentChannels[i]), allTypes.GetType(sourceTypes.GetSize() + i),
                    std::move(windowFrame), executionContext.get(), isOverflowAsNull)));
                break;
            case OMNI_AGGREGATION_TYPE_COUNT_ALL:
                windowFunctions.push_back(std::move(make_unique<CountAllWindowFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i))));
                break;
            case OMNI_WINDOW_TYPE_NTH_VALUE: {
                const auto &options = windowFunctionOptions[i];
                int64_t nthOffset = options.nthValueOffset;
                if (nthOffset < 1) {
                    nthOffset = 1;
                }
                windowFunctions.push_back(std::move(make_unique<NthValueFunction>(std::move(windowFrame),
                    sourceTypes.GetType(argumentChannels[i]), allTypes.GetType(sourceTypes.GetSize() + i),
                    argumentChannels[i], nthOffset, options.nthValueOffsetChannel, options.IsIgnoreNulls())));
                break;
            }
            case OMNI_WINDOW_TYPE_NTILE: {
                int32_t numBuckets = windowFrameStartChannels[i];
                if (numBuckets < 1) {
                    numBuckets = 1;
                }
                windowFunctions.push_back(std::move(make_unique<NtileFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i),
                    numBuckets)));
                break;
            }
            case OMNI_WINDOW_TYPE_DENSE_RANK:
                windowFunctions.push_back(std::move(make_unique<DenseRankFunction>(std::move(windowFrame),
                    NoneDataType::Instance(), allTypes.GetType(sourceTypes.GetSize() + i))));
                break;
            default:
                ret = OMNI_STATUS_ERROR;
                break;
        }
    }
    SetOperatorName(metricsNameWindow);
    return ret;
}

WindowOperator::~WindowOperator()
{
    delete pendingInput;
}

int32_t WindowOperator::AddInput(VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    UpdateAddInputInfo(rowCount);
    totalRowCount += rowCount;
    pagesIndex->AddVecBatch(vecBatch);
    ResetInputVecBatch();
    if (operatorConfig.GetSpillConfig()->NeedSpill(pagesIndex.get())) {
        auto result = SpillToDisk();
        pagesIndex->Clear();
        executionContext->GetArena()->Reset();
        if (result != ErrorCode::SUCCESS) {
            throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
        }
    }
    return 0;
}

void WindowOperator::PrepareOutput()
{
    // first, build the final output col number according to the outputCols and additional cols created by the window
    int32_t allCount = allTypes.GetSize();
    int finalOutputCols[allCount];
    for (int32_t i = 0; i < outputColsCount; i++) {
        finalOutputCols[finalOutputColsCount] = outputCols[i];
        finalOutputColsCount++;
    }
    for (int32_t i = typesCount; i < allCount; i++) {
        finalOutputCols[finalOutputColsCount] = i;
        finalOutputColsCount++;
    }

    // next, get output
    maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(
        allTypes.Get(), finalOutputCols, finalOutputColsCount, &executionContext->queryConfigRef());

    outputTypes.reserve(finalOutputColsCount);
    for (int colIdx = 0; colIdx < finalOutputColsCount; ++colIdx) {
        outputTypes.push_back(allTypes.GetType(finalOutputCols[colIdx]));
    }

    if (spiller == nullptr) {
        Initialization();
        if (totalRowCount == 0) {
            return;
        }
        FinishPagesIndex();
    } else {
        maxRowCountPerVecBatch =
            OperatorUtil::GetConfiguredMaxRowCount(sourceTypes.GetSize(), &executionContext->queryConfigRef());
    }
    inputVecBatchForAgg = make_unique<VectorBatch>(totalRowCount);
    inputVecBatchForAgg->ResizeVectorCount(1);
}

int32_t WindowOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    // check whether the output is completed
    if (totalRowCount == 0 || totalRowCount == rowCountOutputted) {
        if (totalRowCount == 0 && noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        totalRowCount = 0;
        rowCountOutputted = 0;
        hasPrepare = false;
        if (inputVecBatchForAgg != nullptr) {
            inputVecBatchForAgg->SetVector(0, nullptr);
        }
        pagesIndex->Clear();
        executionContext->GetArena()->Reset();
        UpdateGetOutputInfo(0);
        if (totalRowCount == 0) {
            SetStatus(OMNI_STATUS_NORMAL);
        } else {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }

    if (!hasPrepare) {
        PrepareOutput();
        hasPrepare = true;
    }

    if (hasSpill) {
        GetOutputFromDisk(outputVecBatch);
    } else {
        GetOutputFromMemory(outputVecBatch);
    }
    if (*outputVecBatch != nullptr) {
        UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
    } else {
        UpdateGetOutputInfo(0);
    }

    if (totalRowCount == rowCountOutputted) {
        totalRowCount = 0;
        rowCountOutputted = 0;
        hasPrepare = false;
        inputVecBatchForAgg->SetVector(0, nullptr);
        // reset
        VectorHelper::FreeVecBatch(spillOutBatch);
        spillBatchOffset = 0;
        pagesIndex->Clear();
        executionContext->GetArena()->Reset();
        SetStatus(OMNI_STATUS_FINISHED);
    }

    return 0;
}

void WindowOperator::GetOutputFromMemory(VectorBatch **outputVecBatch)
{
    /*
     * First, new a vectorBatch dedicated to calling the agg interface with a single column;
     * Second, the vector in the vectorBatch is obtained from the AggregateWindowFunction::Accumulate with
     * argumentChannels.
     * This is mainly to avoid using location information to copy data and construct a continuous vectorBatch when
     * calling agg interface.
     */
    int32_t rowCount = static_cast<int32_t>(min(maxRowCount, totalRowCount - rowCountOutputted));
    auto output = std::make_unique<VectorBatch>(rowCount);
    auto outputPtr = output.get();
    DataTypes dataTypes(outputTypes);
    VectorHelper::AppendVectors(outputPtr, dataTypes, rowCount);

    try {
        ProcessData(outputPtr, rowCount);
    } catch (const OmniException &e) {
        // in ProcessData, WindowFunction may be throw exception:
        // when spark sum/avg decimal overflow, it will throw exception when
        // OverflowConfigId==OVERFLOW_CONFIG_EXCEPTION
        totalRowCount = 0;
        rowCountOutputted = 0;
        hasPrepare = false;
        inputVecBatchForAgg->SetVector(0, nullptr);
        pagesIndex->Clear();
        throw e;
    }

    *outputVecBatch = output.release();
    rowCountOutputted += rowCount;
}

void WindowOperator::ProcessData(VectorBatch *&outputVecBatch, int32_t rowCount)
{
    // build the output data with original input vecBatch, input data are not changed in window operator
    // we add extra columns of window result to the output vecBatch in window partition
    pagesIndex->GetOutput(outputCols.data(), outputColsCount, outputVecBatch, sourceTypes.GetIds(), rowCountOutputted,
        rowCount);

    for (int32_t j = 0; j < rowCount; j++) {
        if (partition == nullptr || !partition->HasNext()) {
            int32_t partitionStart = partition == nullptr ? 0 : partition->GetPartitionEnd();
            if (partitionStart >= pagesIndex->GetRowCount()) {
                partition = nullptr;
                break;
            }
            int32_t partitionEnd = FindGroupEnd(pagesIndex.get(), unGroupedPartitionHashStrategy.get(), partitionStart);
            partition = make_unique<WindowPartition>(sourceTypes, pagesIndex.get(), partitionStart, partitionEnd,
                outputCols.data(), outputColsCount, windowFunctions, peerGroupHashStrategy.get());
        }
        partition->ProcessNextRow(inputVecBatchForAgg.get(), outputVecBatch, j);
    }
}

void WindowOperator::Initialization()
{
    pagesIndex->Prepare();

    // right now we assume the pregroup and presort are null
    preGroupedPartitionHashStrategy = make_unique<PagesHashStrategy>(pagesIndex->GetColumns(), pagesIndex->GetTypes(),
        preGroupedCols.data(), preGroupedCount);
    unGroupedPartitionHashStrategy = make_unique<PagesHashStrategy>(pagesIndex->GetColumns(), pagesIndex->GetTypes(),
        partitionCols.data(), partitionCount);
    preSortedPartitionHashStrategy = make_unique<PagesHashStrategy>(pagesIndex->GetColumns(), pagesIndex->GetTypes(),
        preGroupedCols.data(), preGroupedCount);
    peerGroupHashStrategy = make_unique<PagesHashStrategy>(pagesIndex->GetColumns(), pagesIndex->GetTypes(),
        originSortCols.data(), originSortColCount);
}

void WindowOperator::FinishPagesIndex()
{
    SortPagesIndexIfNecessary();
}

void WindowOperator::SortPagesIndexIfNecessary()
{
    if (pagesIndex->GetRowCount() > 1 && sortColCount != 0) {
        int32_t startPosition = 0;
        auto positionCount = pagesIndex->GetRowCount();
        while (startPosition < positionCount) {
            int32_t endPosition = FindGroupEnd(pagesIndex.get(), preSortedPartitionHashStrategy.get(), startPosition);
            pagesIndex->Sort(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, startPosition,
                endPosition);
            startPosition = endPosition;
        }
    }
}

int32_t FindGroupEnd(PagesIndex *pagesIndex, PagesHashStrategy *pagesHashStrategy, int32_t startPosition)
{
    int32_t left = startPosition;
    int32_t right = pagesIndex->GetRowCount();

    while (left + 1 < right) {
        int32_t middle = left + (right - left) / MID_SEARCH_FACTOR;

        if (PositionEqualsPosition(pagesIndex, pagesHashStrategy, startPosition, middle)) {
            left = middle;
        } else {
            right = middle;
        }
    }

    return right;
}

OmniStatus WindowOperator::Close()
{
    if (inputVecBatchForAgg != nullptr) {
        inputVecBatchForAgg->SetVector(0, nullptr);
    }
    // delete spiller object when exception occurs
    if (spiller != nullptr) {
        spiller->RemoveSpillFiles();
    }
    delete spiller;
    spiller = nullptr;
    delete spillMerger;
    spillMerger = nullptr;

    // ensure free pagesIndex if exception occurs
    pagesIndex->Clear();
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}

uint64_t WindowOperator::GetSpilledBytes()
{
    return spilledBytes;
}

ErrorCode WindowOperator::SpillToDisk()
{
    auto rowCount = pagesIndex->GetRowCount();
    if (rowCount <= 0) {
        return ErrorCode::SUCCESS;
    }

    if (spiller == nullptr) {
        auto spillConfig = operatorConfig.GetSpillConfig();
        OperatorConfig::CheckSpillConfig(spillConfig);
        size_t sortColsCount = sortCols.size();
        std::vector<SortOrder> sortOrders;
        for (size_t i = 0; i < sortColsCount; i++) {
            SortOrder sortOrder{ sortAscendings[i] == 1, sortNullFirsts[i] == 1 };
            sortOrders.emplace_back(sortOrder);
        }
        spiller = new Spiller(sourceTypes, sortCols, sortOrders, spillConfig->GetSpillPath(),
            spillConfig->GetMaxSpillBytes(), spillConfig->GetWriteBufferSize(), spillConfig->IsSpillCompressEnabled());
        hasSpill = true;
    }

    Sort();

    LogDebug("Spill data to disk starting in window operator, rowCount=%lld\n", rowCount);
    auto result = spiller->Spill(pagesIndex.get(), canInplaceSort, false, this);
    UpdateSpillTimesInfo();
    LogDebug("Spill data to disk finished in window operator, rowCount=%lld\n", rowCount);
    return result;
}

void WindowOperator::Sort()
{
    if (canInplaceSort) {
        DYNAMIC_TYPE_DISPATCH(pagesIndex->PrepareInplaceSort, sourceTypes.GetType(0)->GetId(), sortNullFirsts[0]);
    } else {
        pagesIndex->Prepare();
    }

    int32_t positionCount = pagesIndex->GetRowCount();

    if (canInplaceSort) {
        pagesIndex->SortInplace(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, 0,
            positionCount);
    } else {
        pagesIndex->Sort(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, 0, positionCount);
    }
}

void WindowOperator::GetOutputFromDisk(VectorBatch **outputVecBatch)
{
    if (spillMerger == nullptr) {
        auto result = SpillToDisk();
        pagesIndex->Clear();
        spilledBytes = spiller->GetSpilledBytes();
        if (result != ErrorCode::SUCCESS) {
            throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
        }
        auto spillFiles = spiller->FinishSpill();
        UpdateSpillFileInfo(spillFiles.size());
        spillMerger = spiller->CreateSpillMerger(spillFiles, spiller->isSpillCompressEnable());
        if (spillMerger == nullptr) {
            delete spiller;
            spiller = nullptr;
            throw omniruntime::exception::OmniException("SPILL_FAILED", "Create spill merger failed.");
        }
        delete spiller;
        spiller = nullptr;

        currentBatch = spillMerger->CurrentBatch();
        currentRowIdx = spillMerger->CurrentRowIndex();
    }

    int32_t rowCount = min(maxRowCount, totalRowCount - rowCountOutputted);
    auto output = std::make_unique<VectorBatch>(rowCount);
    auto outputPtr = output.get();
    DataTypes dataTypes(outputTypes);
    VectorHelper::AppendVectors(outputPtr, dataTypes, rowCount);
    if (this->spillOutBatch == nullptr) {
        DataTypes inDataTypes(sourceTypes);
        this->spillOutBatch = new VectorBatch(totalRowCount - rowCountOutputted);
        VectorHelper::AppendVectors(this->spillOutBatch, inDataTypes, totalRowCount - rowCountOutputted);
    }

    try {
        ProcessDataFromDisk(outputPtr, rowCount);
    } catch (const OmniException &e) {
        // in ProcessData, WindowFunction may be throw exception:
        // when spark sum/avg decimal overflow, it will throw exception when
        // OverflowConfigId==OVERFLOW_CONFIG_EXCEPTION
        totalRowCount = 0;
        rowCountOutputted = 0;
        hasPrepare = false;
        inputVecBatchForAgg->SetVector(0, nullptr);
        pagesIndex->Clear();
        VectorHelper::FreeVecBatch(spillOutBatch);
        throw e;
    }
    *outputVecBatch = output.release();
}

void WindowOperator::ProcessDataFromDisk(VectorBatch *&outputVecBatch, int32_t rowCount)
{
    bool needClear = true;
    if (partition != nullptr && partition->HasNext()) {
        int32_t outputCount = min(partitionRowCount - partitionOutputted, rowCount);
        pagesIndex->GetOutput(outputCols.data(), outputColsCount, outputVecBatch, sourceTypes.GetIds(),
            partitionOutputted, outputCount);
    }

    for (int32_t i = 0; i < rowCount; i++) {
        if (partition == nullptr || !partition->HasNext()) {
            if (needClear) {
                spillOutBatch->Resize(totalRowCount - rowCountOutputted);
                needClear = false;
                spillBatchOffset = 0;
            }
            if (!ProcessNextWindowPartition()) {
                partition = nullptr;
                break;
            }
            inputVecBatchForAgg->SetVector(0, nullptr);
            inputVecBatchForAgg = make_unique<VectorBatch>(partitionRowCount);
            inputVecBatchForAgg->ResizeVectorCount(1);
            int32_t outputCount = min(partitionRowCount, rowCount - i);
            pagesIndex->GetOutput(outputCols.data(), outputColsCount, outputVecBatch, sourceTypes.GetIds(), 0,
                outputCount, i);
        }
        partition->ProcessNextRow(inputVecBatchForAgg.get(), outputVecBatch, i);
        rowCountOutputted++;
        partitionOutputted++;
    }
}

vec::VectorBatch* WindowOperator::createSpillVectorBatch(int32_t rowCnt, int32_t offset) {
    auto *vecBatch = new VectorBatch(rowCnt);
    for (int32_t col = 0; col < sourceTypes.GetSize(); ++col) {
        auto vector = VectorHelper::SliceVector(this->spillOutBatch->Get(col), offset, rowCnt);
        vecBatch->Append(vector);
        }
    return vecBatch;
}

bool WindowOperator::ProcessNextWindowPartition()
{
    if (rowCountOutputted == totalRowCount) {
        return false;
    }

    pagesIndex->Clear();
    partitionRowCount = 0;
    partitionOutputted = 0;

    int32_t vecBatchRowCount = min(maxRowCountPerVecBatch, totalRowCount - rowCountOutputted);
    int32_t rowIdx = 0;
    VectorBatch *lastVecBatch = nullptr;
    int32_t lastRowIdx = 0;
    bool isFirstRow = true;

    int32_t offset = spillBatchOffset;

    while (isFirstRow || IsSamePartition(lastVecBatch, lastRowIdx)) {
        PaddingPartitionVecBatch(spillOutBatch, spillBatchOffset++);

        isFirstRow = false;
        lastVecBatch = spillOutBatch;
        lastRowIdx = spillBatchOffset - 1;
        rowIdx++;
        partitionRowCount++;

        if (rowIdx == vecBatchRowCount) {
            auto *vecBatch = createSpillVectorBatch(rowIdx, offset);
            pagesIndex->AddVecBatch(vecBatch);
            offset += rowIdx;
            rowIdx = 0;
        }

        if (rowCountOutputted + partitionRowCount == totalRowCount) {
            break;
        }

        spillMerger->Pop();
        currentBatch = spillMerger->CurrentBatch();
        currentRowIdx = spillMerger->CurrentRowIndex();
    }

    if (rowIdx > 0) {
        auto *vecBatch = createSpillVectorBatch(rowIdx, offset);
        pagesIndex->AddVecBatch(vecBatch);
    }

    pagesIndex->Prepare();
    peerGroupHashStrategy = make_unique<PagesHashStrategy>(pagesIndex->GetColumns(), pagesIndex->GetTypes(),
        originSortCols.data(), originSortColCount);

    inputVecBatchForAgg->SetVector(0, nullptr);
    inputVecBatchForAgg = make_unique<VectorBatch>(totalRowCount);
    inputVecBatchForAgg->ResizeVectorCount(1);

    partition = make_unique<WindowPartition>(sourceTypes, pagesIndex.get(), 0, partitionRowCount, outputCols.data(),
        outputColsCount, windowFunctions, peerGroupHashStrategy.get());

    return true;
}

template <typename T>
static ALWAYS_INLINE bool ValueEqualsLastValue(vec::VectorBatch *partitionVecBatch, vec::VectorBatch *currentVecBatch,
    int32_t columnId, int32_t lastIdx, int32_t nextIdx)
{
    auto partitionVector = partitionVecBatch->Get(columnId);
    auto currentVector = currentVecBatch->Get(columnId);

    if constexpr (std::is_same_v<T, std::string_view>) {
        std::string_view lastValue;
        std::string_view nextValue;
        lastValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(partitionVector)->GetValue(lastIdx);
        nextValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(currentVector)->GetValue(nextIdx);
        auto lastValueLength = lastValue.length();
        if (lastValueLength != nextValue.length()) {
            return false;
        }
        if (memcmp(lastValue.data(), nextValue.data(), lastValueLength) == 0) {
            return true;
        } else {
            return false;
        }
    } else if constexpr (std::is_same_v<T, double>) {
        double lastValue = static_cast<Vector<double> *>(partitionVector)->GetValue(lastIdx);
        double nextValue = static_cast<Vector<double> *>(currentVector)->GetValue(nextIdx);

        if (std::abs(lastValue - nextValue) < __DBL_EPSILON__) {
            return true;
        } else {
            return false;
        }
    } else if constexpr (std::is_same_v<T, float>) {
        float lastValue = static_cast<Vector<float> *>(partitionVector)->GetValue(lastIdx);
        float nextValue = static_cast<Vector<float> *>(currentVector)->GetValue(nextIdx);

        if (std::abs(lastValue - nextValue) < __DBL_EPSILON__) {
            return true;
        } else {
            return false;
        }
    } else {
        T lastValue = static_cast<Vector<T> *>(partitionVector)->GetValue(lastIdx);
        T nextValue = static_cast<Vector<T> *>(currentVector)->GetValue(nextIdx);
        return lastValue == nextValue;
    }
}

bool WindowOperator::IsSamePartition(VectorBatch *lastBatch, int32_t lastIdx)
{
    for (int32_t i = 0; i < partitionCount; i++) {
        int32_t columnId = partitionCols.data()[i];
        auto lastVector = lastBatch->Get(columnId);
        auto currentVector = currentBatch->Get(columnId);
        auto lastIsNull = lastVector->IsNull(lastIdx);
        auto currentIsNull = currentVector->IsNull(currentRowIdx);
        if (lastIsNull && currentIsNull) {
            continue;
        }
        if (lastIsNull || currentIsNull) {
            return false;
        }

        bool isSame;
        auto columnTypeId = sourceTypes.GetType(columnId)->GetId();
        switch (columnTypeId) {
            case OMNI_BYTE:
                isSame = ValueEqualsLastValue<int8_t>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                isSame = ValueEqualsLastValue<int32_t>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP:
                isSame = ValueEqualsLastValue<int64_t>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_DOUBLE:
                isSame = ValueEqualsLastValue<double>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_FLOAT:
                isSame = ValueEqualsLastValue<float>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_BOOLEAN:
                isSame = ValueEqualsLastValue<bool>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_SHORT:
                isSame = ValueEqualsLastValue<int16_t>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_DECIMAL128:
                isSame = ValueEqualsLastValue<Decimal128>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                isSame = ValueEqualsLastValue<string_view>(lastBatch, currentBatch, columnId, lastIdx, currentRowIdx);
                break;
            case OMNI_ARRAY:
                // Reuse OperatorUtil::CompareArrayValue (lexicographic, null-aware) for equality check
                isSame = (OperatorUtil::CompareArrayValue(lastVector, lastIdx, currentVector, currentRowIdx) ==
                    OperatorUtil::COMPARE_STATUS_EQUAL);
                break;
            case OMNI_ROW:
                // Use OperatorUtil::CompareStructValue for struct equality check
                isSame = (OperatorUtil::CompareStructValue(lastVector, lastIdx, currentVector, currentRowIdx) ==
                    OperatorUtil::COMPARE_STATUS_EQUAL);
                break;
            default:
                break;
        }
        if (!isSame) {
            return false;
        }
    }

    return true;
}

void WindowOperator::PaddingPartitionVecBatch(vec::VectorBatch *partitionVecBatch, int32_t rowIdx)
{
    auto outputColSize = static_cast<int32_t>(sourceTypes.GetSize());
    for (int32_t i = 0; i < outputColSize; i++) {
        auto partitionVector = partitionVecBatch->Get(i);
        auto typeId = sourceTypes.GetType(i)->GetId();
        switch (typeId) {
            case OMNI_BYTE:
                PaddingPartitionVector<int8_t>(partitionVector, rowIdx, i);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                PaddingPartitionVector<int32_t>(partitionVector, rowIdx, i);
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP:
                PaddingPartitionVector<int64_t>(partitionVector, rowIdx, i);
                break;
            case OMNI_DOUBLE:
                PaddingPartitionVector<double>(partitionVector, rowIdx, i);
                break;
            case OMNI_FLOAT:
                PaddingPartitionVector<float>(partitionVector, rowIdx, i);
                break;
            case OMNI_BOOLEAN:
                PaddingPartitionVector<bool>(partitionVector, rowIdx, i);
                break;
            case OMNI_SHORT:
                PaddingPartitionVector<int16_t>(partitionVector, rowIdx, i);
                break;
            case OMNI_DECIMAL128:
                PaddingPartitionVector<Decimal128>(partitionVector, rowIdx, i);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                PaddingPartitionVector<std::string_view>(partitionVector, rowIdx, i);
                break;
            case OMNI_ARRAY:
                PaddingPartitionArrayVector(partitionVector, rowIdx, i);
                break;
            case OMNI_ROW:
                PaddingPartitionRowVector(partitionVector, rowIdx, i);
                break;
        }
    }
}

template <typename T>
void WindowOperator::PaddingPartitionVector(vec::BaseVector *groupedVector, int32_t rowIdx, int32_t colIdx)
{
    if constexpr (std::is_same_v<T, std::string_view>) {
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto currentVector = static_cast<VarcharVector *>(currentBatch->Get(colIdx));
        if (currentVector->IsNull(currentRowIdx)) {
            static_cast<VarcharVector *>(groupedVector)->SetNull(rowIdx);
        } else {
            auto value = currentVector->GetValue(currentRowIdx);
            static_cast<VarcharVector *>(groupedVector)->SetValue(rowIdx, value);
        }
    } else {
        auto currentVector = static_cast<Vector<T> *>(currentBatch->Get(colIdx));
        if (currentVector->IsNull(currentRowIdx)) {
            static_cast<Vector<T> *>(groupedVector)->SetNull(rowIdx);
        } else {
            static_cast<Vector<T> *>(groupedVector)->SetValue(rowIdx, currentVector->GetValue(currentRowIdx));
        }
    }
}

void WindowOperator::PaddingPartitionArrayVector(vec::BaseVector *groupedVector, int32_t rowIdx, int32_t colIdx)
{
    auto *srcArray = static_cast<ArrayVector *>(currentBatch->Get(colIdx));
    auto *dstArray = static_cast<ArrayVector *>(groupedVector);
    if (srcArray->IsNull(currentRowIdx)) {
        dstArray->SetNull(rowIdx);
    } else {
        // 对齐Velox：使用GetValue获取数组切片，然后设置到目标位置
        BaseVector *arraySlice = srcArray->GetValue(currentRowIdx);
        dstArray->SetValue(rowIdx, arraySlice);
        delete arraySlice;  // GetValue返回的是新分配的向量，需要手动释放
    }
}

void WindowOperator::PaddingPartitionRowVector(vec::BaseVector *groupedVector, int32_t rowIdx, int32_t colIdx)
{
    auto *srcRow = static_cast<RowVector *>(currentBatch->Get(colIdx));
    auto *dstRow = static_cast<RowVector *>(groupedVector);
    if (srcRow->IsNull(currentRowIdx)) {
        dstRow->SetNull(rowIdx);
    } else {
        // For RowVector, we need to copy each field individually
        for (int i = 0; i < srcRow->ChildSize(); i++) {
            auto *srcField = srcRow->ChildAt(i).get();
            auto *dstField = dstRow->ChildAt(i).get();
            
            // Copy the value from source to destination
            if (srcField->IsNull(currentRowIdx)) {
                dstField->SetNull(rowIdx);
            } else {
                // 对齐Velox：使用Slice获取指定行的数据，然后Append
                BaseVector* srcSlice = srcField->Slice(currentRowIdx, 1);
                VectorHelper::AppendVector(dstField, rowIdx, srcSlice, 1);
                delete srcSlice;  // Slice返回新分配的向量，需要释放
            }
        }
    }
}

}
}
