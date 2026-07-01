/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: sort implementations
 */
#include "sort.h"
#include "util/type_util.h"
#include "util/debug.h"
#include "vector/vector_helper.h"
#include "operator/util/operator_util.h"
#include "util/omni_exception.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

SortOperatorFactory::SortOperatorFactory(const DataTypes &dataTypes, int32_t *outputCols, int32_t outputColCount,
    int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
    const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig)
    : sourceTypes(dataTypes),
      outputCols(outputCols, outputCols + outputColCount),
      sortCols(sortCols, sortCols + sortColCount),
      sortAscendings(sortAscendings, sortAscendings + sortColCount),
      sortNullFirsts(sortNullFirsts, sortNullFirsts + sortColCount),
      operatorConfig(operatorConfig)
{
    this->queryConfig_ = queryConfig;
}

SortOperatorFactory::SortOperatorFactory(const type::DataTypes &dataTypes, std::vector<int32_t> outputCols,
    std::vector<int32_t> sortCols, std::vector<int32_t> sortAscendings, std::vector<int32_t> sortNullFirsts,
    const OperatorConfig &&operatorConfig, const config::QueryConfig &queryConfig)
    : sourceTypes(dataTypes),
      outputCols(outputCols),
      sortCols(sortCols),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts),
      operatorConfig(operatorConfig)
{
    this->queryConfig_ = queryConfig;
}

SortOperatorFactory::~SortOperatorFactory() = default;

SortOperatorFactory *SortOperatorFactory::CreateSortOperatorFactory(const DataTypes &dataTypes, int32_t *outputCols,
    int32_t outputColCount, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount)
{
    OperatorConfig defaultConfig;
    return CreateSortOperatorFactory(dataTypes, outputCols, outputColCount, sortCols, sortAscendings, sortNullFirsts,
        sortColCount, defaultConfig);
}

SortOperatorFactory *SortOperatorFactory::CreateSortOperatorFactory(const DataTypes &dataTypes, int32_t *outputCols,
    int32_t outputColCount, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
    const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig)
{
    auto pOperatorFactory = new SortOperatorFactory(dataTypes, outputCols, outputColCount, sortCols, sortAscendings,
        sortNullFirsts, sortColCount, operatorConfig, queryConfig);
    return pOperatorFactory;
}

SortOperatorFactory *SortOperatorFactory::CreateSortOperatorFactory(std::shared_ptr<const OrderByNode> planNode,
    const config::QueryConfig &queryConfig)
{
    auto spillConfig =  SparkSpillConfig(planNode->CanSpill(queryConfig) & queryConfig.orderBySpillEnabled(),
        queryConfig.SpillDir(), queryConfig.SpillDirDiskReserveSize(), queryConfig.SpillSortRowThreshold(),
        queryConfig.SpillMemThreshold(), queryConfig.SpillWriteBufferSize(), queryConfig.SpillEnableCompress());
    auto dataTypes = planNode->GetSourceTypes();
    auto outputCols = planNode->GetOutputCols();
    auto sortCols = planNode->GetSortCols();
    auto sortAscending = planNode->GetSortAscending();
    auto sortNullFirsts = planNode->GetNullFirsts();
    auto pOperatorFactory = new SortOperatorFactory(*dataTypes.get(), outputCols, sortCols, sortAscending,
        sortNullFirsts, std::move(OperatorConfig(spillConfig)), queryConfig);
    return pOperatorFactory;
}

Operator *SortOperatorFactory::CreateOperator()
{
    auto pSortOperator =
        new SortOperator(sourceTypes, outputCols, sortCols, sortAscendings, sortNullFirsts, operatorConfig,
            queryConfig_);
    return pSortOperator;
}

// function implements for class Sort
SortOperator::SortOperator(const DataTypes &dataTypes, std::vector<int32_t> &outputCols, std::vector<int32_t> &sortCols,
    std::vector<int32_t> &sortAscendings, std::vector<int32_t> &sortNullFirsts, const OperatorConfig &operatorConfig,
    const config::QueryConfig &queryConfig)
    : sourceTypes(dataTypes),
      outputCols(outputCols),
      sortCols(sortCols),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts),
      pagesIndex(std::make_unique<PagesIndex>(sourceTypes)),
      operatorConfig(operatorConfig)
{
    executionContext->SetConfig(queryConfig);
    for (auto outputCol : outputCols) {
        outputTypes.emplace_back(dataTypes.GetType(outputCol));
    }
    maxRowCountPerBatch =
        OperatorUtil::GetConfiguredMaxRowCount(
            dataTypes.Get(), outputCols.data(), outputCols.size(), &executionContext->queryConfigRef());
    maxRowCountPerBatch = maxRowCountPerBatch == 0 ? 1 : maxRowCountPerBatch;
    if (sourceTypes.GetSize() == 1) {
        const auto &firstSourceTypeId = sourceTypes.GetType(0)->GetId();
        canInplaceSort = (firstSourceTypeId != OMNI_VARCHAR) && (firstSourceTypeId != OMNI_CHAR) &&
            (firstSourceTypeId != OMNI_VARBINARY) &&
            (firstSourceTypeId != OMNI_BOOLEAN) && (firstSourceTypeId != OMNI_ARRAY) &&
            (firstSourceTypeId != OMNI_ROW);
    }

    if (!canInplaceSort) {
        this->radixSortSizeThreshold = operatorConfig.GetAdaptivityThreshold();
        canRadixSort = this->CanUseRadixSort();
    }
}

SortOperator::~SortOperator() = default;

int32_t SortOperator::AddInput(VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    totalRowCount += rowCount;
    pagesIndex->AddVecBatch(vecBatch);
    ResetInputVecBatch();
    if (operatorConfig.GetSpillConfig()->NeedSpill(pagesIndex.get())) {
        auto result = SpillToDisk();
        pagesIndex->Clear();
        if (UNLIKELY(result != ErrorCode::SUCCESS)) {
            throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
        }
    }
    return 0;
}

// return error code
int32_t SortOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    // input data is empty, or all data has been returned.
    if (totalRowCount == 0 || totalRowCount == rowCountOutputted) {
        pagesIndex->Clear();
        if (totalRowCount == 0 && noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        if (totalRowCount == 0) {
            SetStatus(OMNI_STATUS_NORMAL);
        } else {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }

    if (!hasSpill) {
        GetOutputFromMemory(outputVecBatch);
    } else {
        GetOutputFromDisk(outputVecBatch);
    }

    // through the reference counting mechanism, can vecBatch be released early?
    if (totalRowCount == rowCountOutputted) { // all result have been generated
        pagesIndex->Clear();
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }
    return 0;
}

OmniStatus SortOperator::Close()
{
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

uint64_t SortOperator::GetSpilledBytes()
{
    return spilledBytes;
}

ErrorCode SortOperator::SpillToDisk()
{
    const auto rowCount = pagesIndex->GetRowCount();
    if (rowCount <= 0) {
        return ErrorCode::SUCCESS;
    }

    if (spiller == nullptr) {
        auto spillConfig = operatorConfig.GetSpillConfig();
        OperatorConfig::CheckSpillConfig(spillConfig);
        size_t sortColsCount = sortCols.size();
        std::vector<SortOrder> sortOrders;
        for (size_t i = 0; i < sortColsCount; i++) {
            SortOrder sortOrder { sortAscendings[i] == 1, sortNullFirsts[i] == 1 };
            sortOrders.emplace_back(sortOrder);
        }
        spiller = new Spiller(sourceTypes, sortCols, sortOrders, spillConfig->GetSpillPath(),
            spillConfig->GetMaxSpillBytes(), spillConfig->GetWriteBufferSize(), spillConfig->IsSpillCompressEnabled());
        hasSpill = true;
    }

    // first step prepare for sort
    PrepareSort();

    // second step sort
    Sort();

    LogDebug("Spill data to disk starting in sort operator, rowCount=%lld\n", rowCount);
    UpdateSpillTimesInfo();
    auto result = spiller->Spill(pagesIndex.get(), canInplaceSort, canRadixSort, this);
    LogDebug("Spill data to disk finished in sort operator, rowCount=%lld\n", rowCount);
    return result;
}

void SortOperator::Sort()
{
    const int32_t positionCount = pagesIndex->GetRowCount();
    const int32_t sortColCount = sortCols.size();
    if (canInplaceSort) {
        pagesIndex->SortInplace(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, 0,
            positionCount);
    } else if (canRadixSort) {
        pagesIndex->SortWithRadixSort(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, 0,
            positionCount);
    } else {
        pagesIndex->Sort(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, 0, positionCount);
    }
}

bool SortOperator::CanUseRadixSort()
{
    const bool canUseRadixSort = (radixSortSizeThreshold != -1 && sortCols.size() == 1);
    if (!canUseRadixSort) {
        return false;
    }
    const auto &sortDataType = sourceTypes.GetType(sortCols[0])->GetId();
    bool isAllowedDataType = (sortDataType == OMNI_LONG || sortDataType == OMNI_DATE32 || sortDataType == OMNI_INT ||
        sortDataType == OMNI_SHORT || sortDataType == OMNI_BOOLEAN || sortDataType == OMNI_DECIMAL64 ||
        sortDataType == OMNI_TIMESTAMP);
    return isAllowedDataType;
}

bool SortOperator::CanUseRadixSortByRuntimeInfo()
{
    return pagesIndex->GetVectorBatchSize() <= UINT16_MAX && pagesIndex->GetRowCount() > radixSortSizeThreshold &&
        !pagesIndex->HasDictionary(sortCols[0]);
}

void SortOperator::PrepareSort()
{
    // update radix sort state
    if (canRadixSort) {
        canRadixSort = CanUseRadixSortByRuntimeInfo();
    }

    if (canInplaceSort) {
        DYNAMIC_TYPE_DISPATCH(pagesIndex->PrepareInplaceSort, sourceTypes.GetType(0)->GetId(), sortNullFirsts[0]);
    } else if (canRadixSort) {
        LogDebug("radix Sort size threshold is %d\n", radixSortSizeThreshold);
        auto typeId = sourceTypes.GetType(sortCols[0])->GetId();
        switch (typeId) {
            case OMNI_TIMESTAMP:
            case OMNI_LONG:
                pagesIndex->PrepareRadixSort<OMNI_LONG>(sortAscendings[0], sortNullFirsts[0], sortCols[0]);
                break;
            case OMNI_DATE32:
            case OMNI_INT:
                pagesIndex->PrepareRadixSort<OMNI_INT>(sortAscendings[0], sortNullFirsts[0], sortCols[0]);
                break;
            case OMNI_SHORT:
                pagesIndex->PrepareRadixSort<OMNI_SHORT>(sortAscendings[0], sortNullFirsts[0], sortCols[0]);
                break;
            case OMNI_BOOLEAN:
                pagesIndex->PrepareRadixSort<OMNI_BOOLEAN>(sortAscendings[0], sortNullFirsts[0], sortCols[0]);
                break;
            case OMNI_DECIMAL64:
                pagesIndex->PrepareRadixSort<OMNI_DECIMAL64>(sortAscendings[0], sortNullFirsts[0], sortCols[0]);
                break;
            case OMNI_BYTE:
                pagesIndex->PrepareRadixSort<OMNI_BYTE>(sortAscendings[0], sortNullFirsts[0], sortCols[0]);
                break;
            default:
                std::string errStr = "Do not support the data type" + std::to_string(typeId) + " in radix sort.";
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
        }
    } else {
        pagesIndex->Prepare();
    }
}

void SortOperator::GetOutputFromMemory(VectorBatch **outputVecBatch)
{
    if (!hasSorted) {
        // first step prepare for sort
        PrepareSort();

        // second step sort
        Sort();
        hasSorted = true;
    }

    // third step, get sorted vector batches
    // Use same source as Sort(): pagesIndex->GetRowCount() is the number of rows just sorted
    const int32_t totalSortedRows = static_cast<int32_t>(pagesIndex->GetRowCount());
    const int32_t remaining =
        std::max<int32_t>(0, totalSortedRows - static_cast<int32_t>(rowCountOutputted));
    int32_t rowCountToOutput = std::min(maxRowCountPerBatch, remaining);
    if (maxRowCountPerBatch <= 1 && remaining > 1) {
        const int32_t fallbackMaxRows =
            OperatorUtil::MAX_VEC_BATCH_SIZE_IN_BYTES / OperatorUtil::DEFAULT_CHAR_LENGTH;
        rowCountToOutput = std::min(remaining, fallbackMaxRows);
    }

    auto result = std::make_unique<VectorBatch>(rowCountToOutput);
    auto resultPtr = result.get();
    DataTypes outputDataTypes(outputTypes);
    VectorHelper::AppendVectors(resultPtr, outputDataTypes, rowCountToOutput);

    if (canInplaceSort) {
        pagesIndex->GetOutputInplaceSort(outputCols.data(), outputCols.size(), resultPtr, sourceTypes.GetIds(),
            rowCountOutputted, rowCountToOutput);
    } else if (canRadixSort) {
        pagesIndex->GetOutputRadixSort(outputCols.data(), outputCols.size(), resultPtr, sourceTypes.GetIds(),
            rowCountOutputted, rowCountToOutput);
    } else {
        pagesIndex->GetOutput(outputCols.data(), outputCols.size(), resultPtr, sourceTypes.GetIds(), rowCountOutputted,
            rowCountToOutput);
    }

    rowCountOutputted += rowCountToOutput;
    *outputVecBatch = result.release();
}

void SortOperator::GetOutputFromDisk(VectorBatch **outputVecBatch)
{
    if (spillMerger == nullptr) {
        auto result = SpillToDisk();
        pagesIndex->Clear();
        spilledBytes = spiller->GetSpilledBytes();
        if (result != ErrorCode::SUCCESS) {
            throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
        }
        auto spillFiles = spiller->FinishSpill();
        spillMerger = spiller->CreateSpillMerger(spillFiles, spiller->isSpillCompressEnable());
        UpdateSpillFileInfo(spillFiles.size());
        // when the spill completed, the spiller object can be released in advance
        if (spillMerger == nullptr) {
            delete spiller;
            spiller = nullptr;
            throw omniruntime::exception::OmniException("SPILL_FAILED", "Create spill merger failed.");
        }
        delete spiller;
        spiller = nullptr;
    }
    int32_t remaining = static_cast<int32_t>(totalRowCount - rowCountOutputted);
    int32_t rowCount = std::min(maxRowCountPerBatch, remaining);
    if (maxRowCountPerBatch <= 1 && remaining > 1) {
        const int32_t fallbackMaxRows =
            OperatorUtil::MAX_VEC_BATCH_SIZE_IN_BYTES / OperatorUtil::DEFAULT_CHAR_LENGTH;
        rowCount = std::min(remaining, fallbackMaxRows);
    }
    batches.resize(rowCount);
    rowIdxes.resize(rowCount);

    // create a empty vector batch, and copy data
    auto result = std::make_unique<VectorBatch>(rowCount);
    auto resultPtr = result.get();
    VectorHelper::AppendVectors(resultPtr, sourceTypes, rowCount);

    int32_t rowOffset = 0;
    int32_t resultRowOffset = 0;
    int32_t rowIdx = 0;
    while (rowOffset < rowCount) {
        bool isLastRow = false;
        batches[rowIdx] = spillMerger->CurrentBatch();
        rowIdxes[rowIdx] = spillMerger->CurrentRowIndex(isLastRow);
        rowIdx++;
        rowOffset++;
        if (isLastRow) {
            SetSpillOutputVecBatch(resultPtr, resultRowOffset, rowIdx);
            rowIdx = 0;
        }
        spillMerger->Pop();
    }
    int32_t remainingCount = rowCount - resultRowOffset;
    if (remainingCount > 0) {
        SetSpillOutputVecBatch(resultPtr, resultRowOffset, rowIdx);
    }

    rowCountOutputted += rowCount;
    *outputVecBatch = result.release();
}

void SortOperator::SetSpillOutputVecBatch(VectorBatch *outputVecBatch, int32_t &rowOffset, int32_t rowCount)
{
    int32_t offset = rowOffset;
    auto outputColSize = static_cast<int32_t>(outputCols.size());
    for (int32_t i = 0; i < outputColSize; i++) {
        auto outputVector = outputVecBatch->Get(i);
        auto outputCol = outputCols[i];
        auto outputTypeId = sourceTypes.GetType(outputCol)->GetId();
        switch (outputTypeId) {
            case OMNI_INT:
            case OMNI_DATE32:
                SetSpillOutputVector<int32_t>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP:
                SetSpillOutputVector<int64_t>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_DOUBLE:
                SetSpillOutputVector<double>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_FLOAT:
                SetSpillOutputVector<float>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_BOOLEAN:
                SetSpillOutputVector<bool>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_SHORT:
                SetSpillOutputVector<int16_t>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_DECIMAL128:
                SetSpillOutputVector<Decimal128>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY:
                SetSpillOutputVector<std::string_view>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_BYTE:
                SetSpillOutputVector<int8_t>(outputVector, offset, rowCount, outputCol);
                break;
            case OMNI_ARRAY:
                SetSpillOutputArrayVector(outputVector, offset, rowCount, outputCol);
                break;
            default:
                std::string errStr = "Do not support the data type" + std::to_string(outputTypeId) + " in SetSpillOutputVecBatch.";
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errStr);
        }
    }

    rowOffset += rowCount;
}

template <typename T>
void SortOperator::SetSpillOutputVector(BaseVector *outputVector, int32_t outputRowIdx, int32_t outputRowCount,
    int32_t outputCol)
{
    for (int32_t i = 0; i < outputRowCount; i++) {
        auto batch = batches[i];
        auto inputRowIdx = rowIdxes[i];
        if constexpr (std::is_same_v<T, std::string_view>) {
            using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
            auto inputVector = static_cast<VarcharVector *>(batch->Get(outputCol));
            if (inputVector->IsNull(inputRowIdx)) {
                static_cast<VarcharVector *>(outputVector)->SetNull(outputRowIdx);
            } else {
                auto value = inputVector->GetValue(inputRowIdx);
                static_cast<VarcharVector *>(outputVector)->SetValue(outputRowIdx, value);
            }
        } else {
            auto inputVector = static_cast<Vector<T> *>(batch->Get(outputCol));
            if (inputVector->IsNull(inputRowIdx)) {
                static_cast<Vector<T> *>(outputVector)->SetNull(outputRowIdx);
            } else {
                static_cast<Vector<T> *>(outputVector)->SetValue(outputRowIdx, inputVector->GetValue(inputRowIdx));
            }
        }
        outputRowIdx++;
    }
}

void SortOperator::SetSpillOutputArrayVector(BaseVector *outputVector, int32_t outputRowIdx, int32_t outputRowCount,
    int32_t outputCol)
{
    for (int32_t i = 0; i < outputRowCount; i++) {
        auto batch = batches[i];
        auto inputRowIdx = rowIdxes[i];
        auto inputVector = static_cast<ArrayVector *>(batch->Get(outputCol));
        if (inputVector->IsNull(inputRowIdx)) {
            static_cast<ArrayVector *>(outputVector)->SetNull(outputRowIdx);
        } else {
            static_cast<ArrayVector *>(outputVector)->SetValue(outputRowIdx, inputVector->GetValue(inputRowIdx));
        }
        outputRowIdx++;
    }
}
} // end of namespace op
} // end of namespace omniruntime
