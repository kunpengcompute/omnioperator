/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: sort implementations
 */
#include "sort.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>
#include "util/type_util.h"
#include "util/debug.h"
#include "vector/vector_helper.h"
#include "operator/util/operator_util.h"
#include "operator/reclaim_context.h"
#include "util/omni_exception.h"
#include "util/telemetry.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

SortOperatorFactory::SortOperatorFactory(const DataTypes &dataTypes, int32_t *outputCols, int32_t outputColCount,
    int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
    const OperatorConfig &operatorConfig)
    : sourceTypes(dataTypes),
      outputCols(outputCols, outputCols + outputColCount),
      sortCols(sortCols, sortCols + sortColCount),
      sortAscendings(sortAscendings, sortAscendings + sortColCount),
      sortNullFirsts(sortNullFirsts, sortNullFirsts + sortColCount),
      operatorConfig(operatorConfig)
{}

SortOperatorFactory::SortOperatorFactory(const type::DataTypes &dataTypes, std::vector<int32_t> outputCols,
    std::vector<int32_t> sortCols, std::vector<int32_t> sortAscendings, std::vector<int32_t> sortNullFirsts,
    const OperatorConfig &&operatorConfig)
    : sourceTypes(dataTypes),
      outputCols(outputCols),
      sortCols(sortCols),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts),
      operatorConfig(operatorConfig) {}

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
    const OperatorConfig &operatorConfig)
{
    auto pOperatorFactory = new SortOperatorFactory(dataTypes, outputCols, outputColCount, sortCols, sortAscendings,
        sortNullFirsts, sortColCount, operatorConfig);
    return pOperatorFactory;
}

SortOperatorFactory *SortOperatorFactory::CreateSortOperatorFactory(std::shared_ptr<const OrderByNode> planNode,
    const config::QueryConfig &queryConfig)
{
    auto spillConfig =  SparkSpillConfig(planNode->CanSpill(queryConfig) & queryConfig.orderBySpillEnabled(),
        queryConfig.SpillDir(), queryConfig.SpillDirDiskReserveSize(), queryConfig.SpillSortRowThreshold(),
        queryConfig.SpillMemFraction(), queryConfig.SpillWriteBufferSize(), queryConfig.SpillEnableCompress());
    auto dataTypes = planNode->GetSourceTypes();
    auto outputCols = planNode->GetOutputCols();
    auto sortCols = planNode->GetSortCols();
    auto sortAscending = planNode->GetSortAscending();
    auto sortNullFirsts = planNode->GetNullFirsts();
    OperatorConfig operatorConfig(spillConfig);
    operatorConfig.SetSortAlgorithm(queryConfig.PdqSortEnabled(), queryConfig.InplacePdqSortEnabled(),
        queryConfig.TimSortEnabled());
    auto pOperatorFactory = new SortOperatorFactory(*dataTypes.get(), outputCols, sortCols, sortAscending,
        sortNullFirsts, std::move(operatorConfig));
    return pOperatorFactory;
}

// W2b-06: pool-bound override
Operator *SortOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    auto pSortOperator =
        new SortOperator(pool, sourceTypes, outputCols, sortCols, sortAscendings, sortNullFirsts, operatorConfig);
    return pSortOperator;
}

// W2b-06: pool-bound ctor — same body, chains Operator(pool).
SortOperator::SortOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const DataTypes &dataTypes,
    std::vector<int32_t> &outputCols, std::vector<int32_t> &sortCols, std::vector<int32_t> &sortAscendings,
    std::vector<int32_t> &sortNullFirsts, const OperatorConfig &operatorConfig)
    : Operator(pool),
      sourceTypes(dataTypes),
      outputCols(outputCols),
      sortCols(sortCols),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts),
      pagesIndex(std::make_unique<PagesIndex>(sourceTypes, operatorConfig.PdqSortEnabled(),
          operatorConfig.InplacePdqSortEnabled(), operatorConfig.TimSortEnabled(), pool)),
      operatorConfig(operatorConfig)
{
    InitFromCtor(dataTypes);
}

void SortOperator::InitFromCtor(const DataTypes &dataTypes)
{
    for (auto outputCol : outputCols) {
        outputTypes.emplace_back(dataTypes.GetType(outputCol));
    }
    maxRowCountPerBatch = OperatorUtil::GetMaxRowCount(dataTypes.Get(), outputCols.data(), outputCols.size());
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

SortOperator::~SortOperator()
{
    if (pool_ != nullptr) {
        pool_->unregisterSpillable(this);
    }
}

OmniStatus SortOperator::Init()
{
    const auto status = Operator::Init();
    const bool spillEnabled = operatorConfig.GetSpillConfig()->IsSpillEnabled();
    if (status == OMNI_STATUS_NORMAL && pool_ != nullptr && spillEnabled) {
        pool_->registerSpillable(this);
    }
    return status;
}

int64_t SortOperator::EstimateInputReservation(const VectorBatch *vecBatch) const
{
    const auto add = [](uint64_t left, uint64_t right) {
        constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        return left > max - std::min(right, max) ? max : left + right;
    };
    const auto multiply = [](uint64_t left, uint64_t right) {
        constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        return right != 0 && left > max / right ? max : left * right;
    };
    const uint64_t incomingRows = static_cast<uint64_t>(vecBatch->GetRowCount());
    const uint64_t columns = sourceTypes.GetSize();

    // PagesIndex retains the already pool-accounted input vectors by pointer;
    // it does not copy their payload in AddVecBatch. Reserving EstimateFlatSize
    // here would therefore bill the same input twice. Account instead for the
    // future scratch introduced by these rows. 28 bytes/row is the structural
    // upper bound of Omni's current sort modes: two radix buffers containing
    // an 8-byte key plus the 6-byte synthetic address. Generic sort needs at
    // most 20 bytes/row (address, value and varchar length), and inplace sort
    // needs less.
    constexpr uint64_t kMaxSortScratchPerRow = 2 * (sizeof(int64_t) + sizeof(int32_t) + sizeof(int16_t));
    const uint64_t rowScratch = multiply(incomingRows, kMaxSortScratchPerRow);
    const uint64_t indexScratch = multiply(columns, sizeof(BaseVector *));
    return static_cast<int64_t>(add(rowScratch, indexScratch));
}

void SortOperator::EnsureReservationPctsCached()
{
    if (reservationPctsCached_) {
        return;
    }
    // QueryConfig is immutable for the lifetime of a query, so the clamped
    // percentages are safe to cache. Read lazily instead of in the constructor:
    // on the wrapper path the ExecutionContext receives its config after the
    // operator object exists.
    const auto queryConfig = executionContext->queryConfig();
    minSpillableReservationPct_ = std::max<int32_t>(queryConfig.minSpillableReservationPct(), 0);
    spillableReservationGrowthPct_ = std::max<int32_t>(queryConfig.spillableReservationGrowthPct(), 0);
    reservationPctsCached_ = true;
}

void SortOperator::ReserveForInput(const VectorBatch *vecBatch)
{
    if (pool_ == nullptr || !operatorConfig.GetSpillConfig()->IsSpillEnabled() ||
        pagesIndex->GetRowCount() == 0) {
        return;
    }
    const auto multiply = [](uint64_t left, uint64_t right) {
        constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        return right != 0 && left > max / right ? max : left * right;
    };
    const uint64_t increment = static_cast<uint64_t>(EstimateInputReservation(vecBatch));
    const uint64_t currentUsage = static_cast<uint64_t>(std::max<int64_t>(pool_->currentBytes(), 0));
    EnsureReservationPctsCached();
    const uint64_t minimumReservation =
        multiply(currentUsage, static_cast<uint64_t>(minSpillableReservationPct_)) / 100;
    const uint64_t availableReservation = static_cast<uint64_t>(pool_->availableReservation());
    if (availableReservation >= minimumReservation &&
        availableReservation > multiply(increment, 2)) {
        return;
    }
    const uint64_t growthReservation =
        multiply(currentUsage, static_cast<uint64_t>(spillableReservationGrowthPct_)) / 100;
    const int64_t target = static_cast<int64_t>(std::max(multiply(increment, 2), growthReservation));
    ReservationRequestSection request(
        this, reclaimableName(), "SortOperator::ReserveForInput", target);
    ReclaimableReservationSection safeReservation(this);
    if (!pool_->maybeReserve(target) && omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        // Match Velox SortBuffer::ensureInputFits: a speculative reservation
        // failure is advisory. The following real allocation is authoritative
        // and may still arbitrate, spill or fail with an actual OOM.
        std::fprintf(stderr,
            "[OMNI-SORT] event=ensure_input_fits outcome=reservation_denied pool=%p incoming_rows=%ld target=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(vecBatch->GetRowCount()),
            static_cast<long>(target),
            static_cast<long>(pool_->currentBytes()), static_cast<long>(pool_->reservedBytes()));
    }
}

int32_t SortOperator::AddInput(VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    // Reservation happens before totalRowCount and PagesIndex mutation, so the
    // active operator can safely participate in synchronous arbitration.
    ReserveForInput(vecBatch);
    totalRowCount += rowCount;
    pagesIndex->AddVecBatch(vecBatch);
    ResetInputVecBatch();
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
        if (pool_ != nullptr) {
            pool_->release();
        }
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

    // Preparing the final in-memory sort allocates scratch proportional to the
    // complete resident run. AddInput reservations can be consumed by the
    // retained input itself, so establish a fresh safe boundary immediately
    // before PrepareSort mutates PagesIndex. If Spark cannot grant the scratch,
    // the synchronous callback may spill this active run and switch output to
    // the existing external-merge path.
    if (!hasSorted && spillMerger == nullptr && pagesIndex->GetRowCount() > 0 && pool_ != nullptr &&
        operatorConfig.GetSpillConfig()->IsSpillEnabled()) {
        constexpr uint64_t kMax = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        const auto add = [](uint64_t left, uint64_t right) {
            constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
            return left > max - std::min(right, max) ? max : left + right;
        };
        const auto multiply = [](uint64_t left, uint64_t right) {
            constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
            return right != 0 && left > max / right ? max : left * right;
        };
        const uint64_t rows = pagesIndex->GetRowCount();
        const uint64_t batchesCount = pagesIndex->GetVectorBatchSize();
        const uint64_t columns = sourceTypes.GetSize();
        constexpr uint64_t kMaxSortScratchPerRow =
            2 * (sizeof(int64_t) + sizeof(int32_t) + sizeof(int16_t));
        const uint64_t rowScratch = multiply(rows, kMaxSortScratchPerRow);
        const uint64_t indexScratch = multiply(multiply(batchesCount, columns), sizeof(BaseVector *));
        const uint64_t ioScratch = std::max<uint64_t>(
            static_cast<uint64_t>(OperatorUtil::MAX_VEC_BATCH_SIZE_IN_BYTES) * 2,
            operatorConfig.GetSpillConfig()->GetWriteBufferSize());
        constexpr uint64_t kReservationCushion = 1ULL << 20;
        const int64_t estimate = static_cast<int64_t>(std::min<uint64_t>(
            add(add(add(rowScratch, indexScratch), ioScratch), kReservationCushion), kMax));
        ReservationRequestSection request(this, reclaimableName(),
            "SortOperator::GetOutput::PrepareSort", estimate);
        ReclaimableReservationSection safeReservation(this);
        if (!pool_->maybeReserve(estimate) && omniruntime::telemetry::IsReclaimVerboseEnabled()) {
            std::fprintf(stderr,
                "[OMNI-SORT] event=ensure_sort_fits outcome=reservation_denied pool=%p rows=%lu target=%ld used=%ld reserved=%ld\n",
                static_cast<void *>(pool_), static_cast<unsigned long>(rows),
                static_cast<long>(estimate), static_cast<long>(pool_->currentBytes()),
                static_cast<long>(pool_->reservedBytes()));
        }
    }

    if (!hasSpill) {
        GetOutputFromMemory(outputVecBatch);
    } else {
        GetOutputFromDisk(outputVecBatch);
    }

    // through the reference counting mechanism, can vecBatch be released early?
    if (totalRowCount == rowCountOutputted) { // all result have been generated
        pagesIndex->Clear();
        if (pool_ != nullptr) {
            pool_->release();
        }
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
    if (pool_ != nullptr) {
        pool_->release();
    }
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

    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-SORT] event=spill_phase phase=prepare_begin pool=%p rows=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(rowCount),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->currentBytes()),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->reservedBytes()));
    }
    // first step prepare for sort
    PrepareSort();
    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-SORT] event=spill_phase phase=prepare_end pool=%p rows=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(rowCount),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->currentBytes()),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->reservedBytes()));
    }

    // second step sort
    Sort();
    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-SORT] event=spill_phase phase=sort_end pool=%p rows=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(rowCount),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->currentBytes()),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->reservedBytes()));
    }

    LogDebug("Spill data to disk starting in sort operator, rowCount=%lld\n", rowCount);
    UpdateSpillTimesInfo();
    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-SORT] event=spill_phase phase=write_begin pool=%p rows=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(rowCount),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->currentBytes()),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->reservedBytes()));
    }
    auto result = spiller->Spill(pagesIndex.get(), canInplaceSort, canRadixSort, this);
    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-SORT] event=spill_phase phase=write_end pool=%p rows=%ld used=%ld reserved=%ld result=%d\n",
            static_cast<void *>(pool_), static_cast<long>(rowCount),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->currentBytes()),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->reservedBytes()), static_cast<int>(result));
    }
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
    VectorHelper::AppendVectors(resultPtr, outputDataTypes, rowCountToOutput, this->pool());

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
        // FinishSpill is the authoritative list of successfully created runs.
        // Keep production stats independent from debug-only spill reporting.
        stats_.spilledFiles = spillFiles.size();
        if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
            std::fprintf(stderr,
                "[OMNI-SORT] event=spill_stats files=%u bytes=%lu rows=%lu\n",
                stats_.spilledFiles, static_cast<unsigned long>(stats_.spilledBytes),
                static_cast<unsigned long>(stats_.spilledRows));
        }
        spillMerger = spiller->CreateSpillMerger(spillFiles, spiller->isSpillCompressEnable(), this->pool());
        UpdateSpillFileInfo(spillFiles.size());
        // when the spill completed, the spiller object can be released in advance
        if (spillMerger == nullptr) {
            delete spiller;
            spiller = nullptr;
            throw omniruntime::exception::OmniException("SPILL_FAILED", "Create spill merger failed.");
        }
        delete spiller;
        spiller = nullptr;
        // The final resident run has been prepared, sorted, written, and the
        // merge readers now own the external-run state.  Whole-run preparation
        // scratch will never be used again.  Keeping its unused reservation
        // until the last output batch unnecessarily starves downstream
        // consumers for the entire (potentially millions-of-rows) merge.
        if (pool_ != nullptr) {
            pool_->releaseUnusedReservation();
            if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
                std::fprintf(stderr,
                    "[OMNI-SORT] event=disk_merge_release pool=%p used=%ld reserved=%ld files=%lu\n",
                    static_cast<void *>(pool_), static_cast<long>(pool_->currentBytes()),
                    static_cast<long>(pool_->reservedBytes()), static_cast<unsigned long>(spillFiles.size()));
            }
        }
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
    // The result schema is the projected output schema, not the complete sort
    // input schema. maxRowCountPerBatch is calculated from these same output
    // columns and SetSpillOutputVecBatch writes output vector i from
    // source column outputCols[i]. Allocating sourceTypes here both violated
    // that contract and could make the supposedly 1 MiB output batch much
    // larger than the reservation estimate.
    DataTypes outputDataTypes(outputTypes);
    VectorHelper::AppendVectors(resultPtr, outputDataTypes, rowCount, this->pool());

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

uint64_t SortOperator::reclaimableBytes() const
{
    if (pagesIndex == nullptr || pagesIndex->GetRowCount() == 0 || hasSorted ||
        spillMerger != nullptr || rowCountOutputted != 0 || reclaimInProgress_.load(std::memory_order_acquire)) {
        return 0;
    }
    return std::max<uint64_t>(1, pagesIndex->GetRetainedBytes());
}

int64_t SortOperator::reclaim(int64_t targetBytes)
{
    (void)targetBytes;
    if (isInNonReclaimableSection()) {
        LogDebug("Sort reclaim skipped: non-reclaimable section");
        return 0;
    }
    bool expected = false;
    if (!reclaimInProgress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        LogDebug("Sort reclaim skipped: recursive reclaim");
        return 0;
    }
    struct ReclaimReset {
        std::atomic<bool> &flag;
        ~ReclaimReset() { flag.store(false, std::memory_order_release); }
    } reset{reclaimInProgress_};

    if (pagesIndex == nullptr || pagesIndex->GetRowCount() == 0 || hasSorted ||
        spillMerger != nullptr || rowCountOutputted != 0) {
        LogDebug("Sort reclaim skipped: empty or output/merge lifecycle");
        return 0;
    }

    const int64_t before = pool_ == nullptr ? 0 : pool_->currentBytes();
    const int64_t reservedBefore = pool_ == nullptr ? before : pool_->reservedBytes();
    const auto result = SpillToDisk();
    if (UNLIKELY(result != ErrorCode::SUCCESS)) {
        throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
    }
    pagesIndex->Clear();
    const int64_t after = pool_ == nullptr ? before : pool_->currentBytes();
    // Sort's retained input vectors can belong to upstream pools, while its
    // pre-reserved preparation scratch belongs to this operator pool. A spill
    // may therefore release task memory without decreasing this pool's used
    // bytes. Report the now-invalidated reservation headroom as reclaimable so
    // OmniTaskMemoryPool resets the reservation and returns that credit through
    // the synchronous shrink result. Do not listener-unreserve here: doing both
    // would credit Spark twice.
    const int64_t freed = std::max<int64_t>(0, std::max(before, reservedBefore) - after);
    LogDebug("Sort reclaim completed: requested=%ld before=%ld reservedBefore=%ld after=%ld freed=%ld",
        static_cast<long>(targetBytes), static_cast<long>(before), static_cast<long>(reservedBefore),
        static_cast<long>(after), static_cast<long>(freed));
    return freed;
}
} // end of namespace op
} // end of namespace omniruntime
