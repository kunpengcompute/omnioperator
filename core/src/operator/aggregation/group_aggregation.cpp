/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Hash Aggregation Source File
 */
#include "group_aggregation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include "vector/vector_helper.h"
#include "operator/status.h"
#include "operator/util/operator_util.h"
#include "operator/reclaim_context.h"
#include "util/type_util.h"
#include "util/debug.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "type/data_type.h"
#include "util/telemetry.h"

#if defined(DEBUG_OPERATOR) && defined(TRACE)
#include <sstream>
#endif
namespace omniruntime {
namespace op {
using namespace omniruntime::type;

static constexpr int32_t UNSPILL_ROW_COUNT_ONE_BATCH = 128;
// Rows per sorted run when spilling fixed-key groups (hash map or array table).
static constexpr int32_t kMaxFixedRowsPerSpillRun = 1024 * 1024;

static ALWAYS_INLINE uint8_t PackedBitWidthForType(int32_t typeId)
{
    switch (typeId) {
        case OMNI_BYTE:
            return 8;
        case OMNI_SHORT:
            return 16;
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_TIME32:
            return 32;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
        case OMNI_DATE64:
        case OMNI_TIME64:
            return 64;
        default:
            return 0;
    }
}

// The function-pointer dispatch table carries a trailing OmniTaskMemoryPool*,
// so every Set* helper allocates its output vectors from the task pool.
using SetVector = void (*)(VectorBatch *vecBatch, int32_t rowCount,
                           omniruntime::mem::OmniTaskMemoryPool *pool);
template <typename V> void SetVectorImpl(VectorBatch *vecBatch, int32_t rowCount,
                                         omniruntime::mem::OmniTaskMemoryPool *pool)
{
    vecBatch->Append(new V(rowCount, pool));
}

void SetVarcharVector(VectorBatch *vecBatch, int32_t rowCount,
                      omniruntime::mem::OmniTaskMemoryPool *pool)
{
    vecBatch->Append(new Vector<LargeStringContainer<std::string_view>>(rowCount, pool));
}

void SetContainerVector(VectorBatch *vecBatch, int32_t rowCount,
                        omniruntime::mem::OmniTaskMemoryPool *pool)
{
    // ContainerVector is pool-bound: its NullsBuffer and values buffer are charged
    // to the task pool and released symmetrically by ~ContainerVector.
    // ArrayVector/MapVector/RowVector containers are still not pool-bound because
    // BaseVector has no pool member.
    auto doubleVector = new Vector<double>(rowCount, pool);
    auto longVector = new Vector<int64_t>(rowCount, pool);
    std::vector<int64_t> vectorAddresses(AVG_VECTOR_COUNT);
    vectorAddresses[0] = reinterpret_cast<int64_t>(doubleVector);
    vectorAddresses[1] = reinterpret_cast<int64_t>(longVector);
    std::vector<DataTypePtr> dataTypes{ DoubleType(), LongType() };
    auto containerVector = new ContainerVector(rowCount, vectorAddresses, dataTypes, pool);
    vecBatch->Append(containerVector);
}

static constexpr SetVector GROUP_AGG_FUNCTIONS[DATA_TYPE_MAX_COUNT] = {
    nullptr,
    SetVectorImpl<Vector<int32_t>>,
    SetVectorImpl<Vector<int64_t>>,
    SetVectorImpl<Vector<double>>,
    SetVectorImpl<Vector<bool>>,
    SetVectorImpl<Vector<short>>,
    SetVectorImpl<Vector<int64_t>>,
    SetVectorImpl<Vector<Decimal128>>,
    SetVectorImpl<Vector<int32_t>>,
    SetVectorImpl<Vector<int64_t>>,
    SetVectorImpl<Vector<int32_t>>,
    SetVectorImpl<Vector<int64_t>>,
    SetVectorImpl<Vector<int64_t>>,
    nullptr,
    nullptr,
    SetVarcharVector,
    SetVarcharVector,
    SetContainerVector,
    SetVectorImpl<Vector<int8_t>>,  // OMNI_BYTE (tinyint), e.g. approx_percentile(byte_col,...) with GROUP BY
    SetVectorImpl<Vector<float>>,
    SetVarcharVector,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

OmniStatus HashAggregationOperatorFactory::Init()
{
    for (auto groupByCol: groupByColsVector) {
        groupByColIndices.push_back(groupByCol);
    }
    for (auto aggInputColsVector: aggsInputColsVector) {
        std::vector<int32_t> aggInputCols;
        for (uint32_t i = 0; i < aggInputColsVector.size(); ++i) {
            aggInputCols.push_back(aggInputColsVector[i]);
        }
        aggsInputCols.push_back(aggInputCols);
    }
    ChooseGroupByType();
    auto ret = CreateAggregatorFactories(aggregatorFactories, aggFuncTypesVector, GetMaskColumns(), aggUdafNamesVector);

    return ret;
}

OmniStatus HashAggregationOperatorFactory::Close()
{
    return OMNI_STATUS_NORMAL;
}

Operator *HashAggregationOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    std::vector<ColumnIndex> groupByIndex(groupByColIndices.size(), ColumnIndex());
    std::vector<std::unique_ptr<Aggregator>> aggs;

    for (uint32_t i = 0; i < this->groupByColIndices.size(); ++i) {
        auto &type = this->groupByTypes.GetType(i);
        groupByIndex[i] = {this->groupByColIndices[i], type, type};
    }

    // refresh inputDateTypes and inputColumnar index for OMNI_AGGREGATION_TYPE_COUNT_ALL type aggregator
    uint32_t aggInputColsSize = 0;
    uint32_t aggCountAllSkipCnt = 0;
    uint32_t aggregateType = OMNI_AGGREGATION_TYPE_INVALID;
    for (uint32_t i = 0; i < this->aggregatorFactories.size(); i++) {
        std::vector<int32_t> aggInputColIdxVec;
        std::vector<DataTypePtr> inputDataTypesPtr;
        aggregateType = aggFuncTypesVector[i];

        // for COUNT_ALL aggregator no input(key and columnar index)
        // use aggCountAllSkipCnt to align with aggsInputCols and aggregatorFactories index not same
        if (aggregateType == OMNI_AGGREGATION_TYPE_COUNT_ALL && inputRaws[i]) {
            inputDataTypesPtr.push_back(NoneType());
            aggInputColIdxVec.push_back(-1);
            aggCountAllSkipCnt++;
        } else {
            auto aggInputIdx = i - aggCountAllSkipCnt;
            for (uint32_t j = 0; j < this->aggsInputCols[aggInputIdx].size(); j++) {
                inputDataTypesPtr.push_back(aggInputTypes[aggInputIdx].GetType(j));
                aggInputColIdxVec.push_back(aggsInputCols[aggInputIdx][j]);
                aggInputColsSize++;
            }
        }

        auto inputTypes = DataTypes(inputDataTypesPtr).Instance();
        auto outputTypes = aggOutputTypes[i].Instance();
        auto aggregator = aggregatorFactories[i]->CreateAggregator(*inputTypes, *outputTypes, aggInputColIdxVec,
            inputRaws[i], outputPartials[i], isOverflowAsNull);
        if (UNLIKELY(aggregator == nullptr)) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", "Unable to create aggregator " + std::to_string(i) + " / " +
                std::to_string(this->aggregatorFactories.size()));
        }
        aggregator->SetStatisticalAggregate(isStatisticalAggregate);
        // Bind the per-task pool so result/spill Vector<T> allocations
        // route through it (sum/avg/min/max/var/bit_*/regr_* helpers).
        aggregator->SetPool(pool);
        aggs.push_back(std::move(aggregator));
    }

    auto groupByOperator = new HashAggregationOperator(pool, groupByIndex, aggsInputCols, aggInputColsSize,
        aggInputTypes, aggOutputTypes, std::move(aggs), inputRaws, outputPartials, hasAggFilters, operatorConfig,
        aggFuncTypesVector, step);
    groupByOperator->SetGroupByColumnsHandleType(handleType);
    groupByOperator->Init();
    return groupByOperator;
}

void HashAggregationOperatorFactory::ChooseGroupByType()
{
    // Currently, the serialization and singleFix method is used for column types that need to be grouped by.
    // The serialization method can be continuously evolved based on different types.
    // The singleFix method is used for OMNI_INT/OMNI_LONG which is only one column.
    if (groupByTypes.GetSize() == 1) {
        auto &type = groupByTypes.GetIds()[0];
        if (type == OMNI_INT || type == OMNI_DATE32) {
            handleType = HandleType::fixedInt32;
            return;
        } else if (type == OMNI_LONG || type == OMNI_TIMESTAMP || type == OMNI_DECIMAL64) {
            handleType = HandleType::fixedInt64;
            return;
        } else if (type == OMNI_SHORT) {
            handleType = HandleType::fixedInt16;
            return;
        }
    }
    if (groupByTypes.GetSize() > 1) {
        int32_t valueBits = 0;
        for (int32_t i = 0; i < groupByTypes.GetSize(); ++i) {
            auto typeId = groupByTypes.GetIds()[i];
            auto bits = PackedBitWidthForType(typeId);
            if (bits == 0) {
                handleType = HandleType::serialize;
                return;
            }
            valueBits += bits;
        }
        int32_t totalBits = valueBits + static_cast<int32_t>(groupByTypes.GetSize()); // + null mask bits
        if (totalBits > 0 && totalBits <= 32) {
            handleType = HandleType::packedInt32;
            return;
        } else if (totalBits <= 64) {
            handleType = HandleType::packedInt64;
            return;
        }
    }
    handleType = HandleType::serialize;
}

void HashAggregationOperator::SetGroupByColumnsHandleType(HandleType t)
{
    groupByColumnsHandleType = t;
}

HashAggregationOperator::~HashAggregationOperator()
{
    if (pool_ != nullptr) {
        pool_->unregisterSpillable(this);
    }
}

uint64_t HashAggregationOperator::EstimateRehashReservation(uint64_t incomingRows) const
{
    switch (groupByColumnsHandleType) {
        case HandleType::serialize:
            return serialize == nullptr ? 0 : serialize->table->EstimateRehashAllocation(incomingRows);
        case HandleType::fixedInt16:
            return fixedInt16 == nullptr ? 0 : fixedInt16->table->EstimateRehashAllocation(incomingRows);
        case HandleType::fixedInt32:
            return fixedInt32 == nullptr ? 0 : fixedInt32->table->EstimateRehashAllocation(incomingRows);
        case HandleType::fixedInt64:
            return fixedInt64 == nullptr ? 0 : fixedInt64->table->EstimateRehashAllocation(incomingRows);
        case HandleType::packedInt32:
            return packedInt32 == nullptr ? 0 : packedInt32->table->EstimateRehashAllocation(incomingRows);
        case HandleType::packedInt64:
            return packedInt64 == nullptr ? 0 : packedInt64->table->EstimateRehashAllocation(incomingRows);
        case HandleType::packedInt128:
            return packedInt128 == nullptr ? 0 : packedInt128->table->EstimateRehashAllocation(incomingRows);
        default:
            return 0;
    }
}

void HashAggregationOperator::EnsureReservationPctsCached()
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

void HashAggregationOperator::EnsureInputFits(const VectorBatch *vecBatch)
{
    // Match Velox GroupingSet::ensureInputFits: an empty table relies on normal
    // allocation/arbitration instead of reserving. Unlike Velox, partial
    // aggregation reserves too: Omni has no memory-driven partial flush, so a
    // partial table otherwise grows only inside AddInput, where arbitration
    // cannot spill it, and the task fails once its memory share is used up.
    if (pool_ == nullptr || !operatorConfig.GetSpillConfig()->IsSpillEnabled() || GetElementsSize() == 0) {
        return;
    }
    constexpr uint64_t kMax = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    const auto add = [](uint64_t left, uint64_t right) {
        constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        return left > max - std::min(right, max) ? max : left + right;
    };
    const auto multiply = [](uint64_t left, uint64_t right) {
        constexpr uint64_t max = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        return right != 0 && left > max / right ? max : left * right;
    };

    const uint64_t incomingRows = static_cast<uint64_t>(vecBatch->GetRowCount());
    const uint64_t aggregateStateBytes = multiply(
        incomingRows, static_cast<uint64_t>(std::max<int32_t>(totalAggStatesSize, 0)));
    const uint64_t serializedKeyBytes = groupByColumnsHandleType == HandleType::serialize
        ? multiply(std::min<uint64_t>(vecBatch->EstimateFlatSize(), kMax), 2)
        : 0;
    const uint64_t arenaDemand = add(aggregateStateBytes, serializedKeyBytes);
    const uint64_t arenaFree = executionContext == nullptr || executionContext->GetArena() == nullptr
        ? 0
        : executionContext->GetArena()->AvailBytes();
    const uint64_t arenaGrowth = arenaDemand > arenaFree ? arenaDemand - arenaFree : 0;
    const uint64_t tableIncrement = EstimateRehashReservation(incomingRows);
    // tableIncrement is the whole new ctrl/slot array of a rehash. The rehash
    // fills it while the old array is still live and frees the old one after,
    // so the new array is needed once. Doubling it asked for twice the real
    // peak, and the request that did not fit spilled a table that would have.
    // Only the arena estimate keeps Velox's factor of two.
    const uint64_t headroom = add(multiply(arenaGrowth, 2), tableIncrement);
    // Velox scales both percentages by the operator's own pool. pool_ is the
    // task pool here, shared with the shuffle writer, readers and every other
    // operator, so scaling by it asks for a tenth of the whole task: when other
    // operators hold most of the task's Spark share, each request is denied
    // and the table is spilled while still small. Scale by what this
    // aggregation could spill.
    const uint64_t currentUsage = reclaimableBytes();
    EnsureReservationPctsCached();
    const uint64_t minimumReservation =
        multiply(currentUsage, static_cast<uint64_t>(minSpillableReservationPct_)) / 100;
    const uint64_t availableReservation = static_cast<uint64_t>(pool_->availableReservation());

    if (availableReservation >= minimumReservation) {
        // EstimateRehashAllocation()==0 is Omni's equivalent of enough free
        // hash-table rows; the arena check covers fixed states and serialized
        // out-of-line keys.
        if (tableIncrement == 0 && arenaFree >= arenaDemand) {
            return;
        }
        if (availableReservation > headroom) {
            return;
        }
    }

    const uint64_t growthReservation =
        multiply(currentUsage, static_cast<uint64_t>(spillableReservationGrowthPct_)) / 100;
    const int64_t target = static_cast<int64_t>(std::max(headroom, growthReservation));
    ReservationRequestSection request(
        this, reclaimableName(), "HashAggregationOperator::EnsureInputFits", target);
    ReclaimableReservationSection safeReservation(this);
    if (!pool_->maybeReserve(target) && omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        // Velox treats a failed speculative reservation as non-fatal. Normal
        // allocation remains authoritative and can still trigger arbitration.
        std::fprintf(stderr,
            "[OMNI-HASHAGG] event=ensure_input_fits outcome=reservation_denied pool=%p incoming_rows=%ld target=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(vecBatch->GetRowCount()),
            static_cast<long>(target), static_cast<long>(pool_->currentBytes()),
            static_cast<long>(pool_->reservedBytes()));
    }
}

void NO_INLINE HashAggregationOperator::ReinitializeHashmapAfterRelease()
{
    switch (groupByColumnsHandleType) {
        case HandleType::serialize:
            serialize->table->ReinitializeAfterRelease();
            break;
        case HandleType::fixedInt16:
            fixedInt16->table->ReinitializeAfterRelease();
            break;
        case HandleType::fixedInt32:
            fixedInt32->table->ReinitializeAfterRelease();
            break;
        case HandleType::fixedInt64:
            fixedInt64->table->ReinitializeAfterRelease();
            break;
        case HandleType::packedInt32:
            packedInt32->table->ReinitializeAfterRelease();
            break;
        case HandleType::packedInt64:
            packedInt64->table->ReinitializeAfterRelease();
            break;
        case HandleType::packedInt128:
            packedInt128->table->ReinitializeAfterRelease();
            break;
        default:
            return;
    }
    hashmapCapacityReleased_ = false;
}

void HashAggregationOperator::EnsureOutputFits()
{
    if (pool_ == nullptr || isStepPartials ||
        !operatorConfig.GetSpillConfig()->IsSpillEnabled() || hasSpill ||
        spillMerger != nullptr || GetElementsSize() == 0) {
        return;
    }
    // Omni output batches are capped by this byte budget. Keep Velox's 1.2x
    // output cushion without introducing floating-point truncation.
    constexpr int64_t kOutputReservation =
        static_cast<int64_t>(OperatorUtil::MAX_VEC_BATCH_SIZE_IN_BYTES) * 6 / 5;
    // maybeReserve quantizes the request to the 8 MiB growth quantum before it
    // subtracts the headroom already available in the pool, so it grants this
    // request without touching the ledgers when `availableReservation()` is at
    // least the quantized size. Match that admission test first and skip the
    // request context / reclaim guard / mutex path entirely (mirrors
    // HashBuilderOperator::TryReserveTableBuild). The caller's hasSpill /
    // spillMerger guard above means the post-reserve spill handoff cannot be
    // missed on this path: no listener callback runs here, so no arbitration
    // can select this operator between the check and the return.
    const int64_t desiredHeadroom =
        omniruntime::mem::OmniTaskMemoryPool::quantizedReservationSize(kOutputReservation);
    if (desiredHeadroom >= 0 && pool_->availableReservation() >= desiredHeadroom) {
        if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
            std::fprintf(stderr,
                "[OMNI-HASHAGG] event=ensure_output_fits outcome=reservation_already_available pool=%p available=%ld desired=%ld\n",
                static_cast<void *>(pool_), static_cast<long>(pool_->availableReservation()),
                static_cast<long>(desiredHeadroom));
        }
        return;
    }
    ReservationRequestSection request(
        this, reclaimableName(), "HashAggregationOperator::EnsureOutputFits", kOutputReservation);
    ReclaimableReservationSection safeReservation(this);
    if (pool_->maybeReserve(kOutputReservation)) {
        // Arbitration may have selected and spilled this operator while the
        // request was in flight. Its disk output path does not need the full
        // in-memory output cushion.
        if (hasSpill) {
            pool_->releaseUnusedReservation();
        }
        return;
    }
    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-HASHAGG] event=ensure_output_fits outcome=reservation_denied pool=%p target=%ld used=%ld reserved=%ld\n",
            static_cast<void *>(pool_), static_cast<long>(kOutputReservation),
            static_cast<long>(pool_->currentBytes()), static_cast<long>(pool_->reservedBytes()));
    }
}

uint64_t HashAggregationOperator::reclaimableBytes() const
{
    if (!isInited || outputStarted_ || spillMerger != nullptr ||
        reclaimInProgress_.load(std::memory_order_acquire) ||
        const_cast<HashAggregationOperator *>(this)->GetElementsSize() == 0) {
        return 0;
    }
    // reclaim() resets the arena but deliberately retains its first chunk;
    // BaseHashMap::Release returns all ctrl/slot/null storage. Advertise only
    // bytes those exact operations can return to the task pool. Element-count
    // times row-width could overstate the real reclaimable allocation.
    uint64_t bytes = executionContext->GetArena()->ReclaimableBytesOnReset();
    const auto addReclaimable = [&bytes](uint64_t value) {
        bytes = value > std::numeric_limits<uint64_t>::max() - bytes
            ? std::numeric_limits<uint64_t>::max()
            : bytes + value;
    };
    if (arrayTable != nullptr) {
        addReclaimable(arrayTable->AllocatedBytes());
    } else if (serialize != nullptr) {
        addReclaimable(serialize->table->ReclaimableBytesOnRelease());
    } else if (fixedInt32 != nullptr) {
        addReclaimable(fixedInt32->table->ReclaimableBytesOnRelease());
    } else if (fixedInt64 != nullptr) {
        addReclaimable(fixedInt64->table->ReclaimableBytesOnRelease());
    } else if (fixedInt16 != nullptr) {
        addReclaimable(fixedInt16->table->ReclaimableBytesOnRelease());
    } else if (packedInt32 != nullptr) {
        addReclaimable(packedInt32->table->ReclaimableBytesOnRelease());
    } else if (packedInt64 != nullptr) {
        addReclaimable(packedInt64->table->ReclaimableBytesOnRelease());
    } else if (packedInt128 != nullptr) {
        addReclaimable(packedInt128->table->ReclaimableBytesOnRelease());
    }
    return bytes;
}

int64_t HashAggregationOperator::reclaim(int64_t targetBytes)
{
    if (targetBytes <= 0 || pool_ == nullptr || isInNonReclaimableSection() || reclaimableBytes() == 0) {
        return 0;
    }
    bool expected = false;
    if (!reclaimInProgress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return 0;
    }
    struct ResetFlag {
        std::atomic<bool> &flag;
        ~ResetFlag() { flag.store(false, std::memory_order_release); }
    } resetFlag{reclaimInProgress_};

    const int64_t before = pool_->currentBytes();
    const auto result = SpillHashMap();
    if (UNLIKELY(result != ErrorCode::SUCCESS)) {
        throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
    }
    executionContext->GetArena()->Reset();
    ReleaseHashmapCapacity();
    return std::max<int64_t>(0, before - pool_->currentBytes());
}

OmniStatus HashAggregationOperator::Init()
{
    // 1. avoid init more than once
    if (isInited) {
        return OMNI_STATUS_NORMAL;
    }
    isInited = true;

    // 2. set op name for metrics
    SetOperatorName(metricsNameHashAgg);

    // 6 calculate every aggregator's size and set offset of aggregator
    CalcAndSetStatesSize();

    // 3. check group by handle methcd
    // put at beginning so that we do not allocate memory if there is error
    if (groupByColumnsHandleType == HandleType::serialize) {
        serialize = std::make_unique<TaperColumnSerializeHandler>(*executionContext->GetArena(), totalAggStatesSize);
        serialize->InitSize(groupByCols.size());
        // Initialize RowContainer with key type sizes for the serialize handler
        // Multi-column group-by uses nullable keys (speculative mode)
        std::vector<int32_t> keySizes;
        keySizes.reserve(groupByCols.size());
        for (const auto &c : groupByCols) {
            keySizes.push_back(OperatorUtil::GetTypeSize(c.input));
        }
        // For variable-length types, store a pointer to the serialized data in the row.
        // VARCHAR/CHAR/VARBINARY: only sizeof(char*) needed (length derivable from serialized format).
        // Complex types (ARRAY, MAP, ROW): sizeof(char*) + sizeof(size_t) for StringRef storage.
        std::vector<bool> isVariableLen(groupByCols.size(), false);
        std::vector<int32_t> typeIds(groupByCols.size());
        std::vector<int32_t> varcharColIndices;
        for (size_t i = 0; i < groupByCols.size(); ++i) {
            auto typeId = groupByCols[i].input->GetId();
            typeIds[i] = typeId;
            if (typeId == type::OMNI_CHAR || typeId == type::OMNI_VARCHAR || typeId == type::OMNI_VARBINARY) {
                varcharColIndices.push_back(i);
                isVariableLen[i] = true;
                keySizes[i] = sizeof(char*);
            } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_MAP || typeId == type::OMNI_ROW) {
                keySizes[i] = sizeof(char*) + sizeof(size_t);
                isVariableLen[i] = true;
            }
        }
        // Merge multiple VARCHAR columns into a single slot to reduce fixed row size
        if (varcharColIndices.size() > 1) {
            int32_t firstVarcharIdx = varcharColIndices[0];
            keySizes[firstVarcharIdx] = sizeof(char*);
            for (size_t i = 1; i < varcharColIndices.size(); ++i) {
                keySizes[varcharColIndices[i]] = 0;
            }
        }
        serialize->InitRowContainer(keySizes, isVariableLen, typeIds, varcharColIndices, *executionContext->GetArena());
    } else if (groupByColumnsHandleType == HandleType::fixedInt32) {
        fixedInt32 = std::make_unique<TaperGroupbySingleFixHandler<int32_t>>(*executionContext->GetArena(), totalAggStatesSize);
    } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
        fixedInt64 = std::make_unique<TaperGroupbySingleFixHandler<int64_t>>(*executionContext->GetArena(), totalAggStatesSize);
    } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
        fixedInt16 = std::make_unique<TaperGroupbySingleFixHandler<int16_t>>(*executionContext->GetArena(), totalAggStatesSize);
    } else if (groupByColumnsHandleType == HandleType::packedInt32 || groupByColumnsHandleType == HandleType::packedInt64 ||
        groupByColumnsHandleType == HandleType::packedInt128) {
        std::vector<int32_t> typeIds;
        std::vector<uint8_t> bitWidths;
        typeIds.reserve(groupByCols.size());
        bitWidths.reserve(groupByCols.size());
        for (const auto &c : groupByCols) {
            auto typeId = static_cast<int32_t>(c.input->GetId());
            auto bits = PackedBitWidthForType(typeId);
            if (bits == 0) {
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                    "Packed group-by key does not support typeId " + std::to_string(typeId));
            }
            typeIds.emplace_back(typeId);
            bitWidths.emplace_back(bits);
        }
        if (groupByColumnsHandleType == HandleType::packedInt32) {
            packedInt32 = std::make_unique<TaperGroupbySingleFixHandler<int32_t, true>>(*executionContext->GetArena(),
                totalAggStatesSize, std::move(typeIds), std::move(bitWidths));
        } else if (groupByColumnsHandleType == HandleType::packedInt64) {
            packedInt64 = std::make_unique<TaperGroupbySingleFixHandler<int64_t, true>>(*executionContext->GetArena(),
                totalAggStatesSize, std::move(typeIds), std::move(bitWidths));
        } else {
            packedInt128 = std::make_unique<TaperGroupbySingleFixHandler<omniruntime::type::int128_t, true>>(
                *executionContext->GetArena(), totalAggStatesSize, std::move(typeIds), std::move(bitWidths));
        }
    } else {
        // only the serialization method is used now
        std::string omniExceptionInfo =
            "In function HashAggregationOperator::Init, can not support groupByColumnsHandleType " +
            std::to_string(static_cast<int>(groupByColumnsHandleType));
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }

    // 4. init group by column and aggregator column
    auto colSize = groupByCols.size() + aggInputColsSize;
    sourceTypes = new int32_t[colSize];
    // group by source types
    for (const auto &c: groupByCols) {
        sourceTypes[c.idx] = static_cast<int32_t>(c.input->GetId());
        memoryChunkSize += OperatorUtil::GetTypeSize(c.input);
    }

    // agg source types
    for (size_t i = 0; i < aggInputCols.size(); ++i) {
        for (size_t j = 0; j < aggInputCols[i].size(); ++j) {
            sourceTypes[aggInputCols[i][j]] = aggInputTypes[i].GetType(j)->GetId();
        }
    }

    for (auto &aggregator: aggregators) {
        const std::vector<DataTypePtr> &aggTypes = aggregator->GetOutputTypes().Get();
        for (auto dataType: aggTypes) {
            memoryChunkSize += OperatorUtil::GetTypeSize(dataType);
        }
    }
    memoryChunkSize += static_cast<int64_t>(aggregators.size() * sizeof(AggregateState));
    executionContext->GetArena()->SetMinChunkSize(memoryChunkSize * 8);

    // 5 init max row when getoutput
    int32_t rowByteSize = InitMaxRowCountAndOutputTypes();
    rowsPerBatch = OperatorUtil::GetMaxRowCount(rowByteSize);

    // 7 vector analyzer
    vectorAnalyzer = new VectorAnalyzer(groupByCols);

    if (pool_ != nullptr && operatorConfig.GetSpillConfig()->IsSpillEnabled()) {
        pool_->registerSpillable(this);
    }

    return OMNI_STATUS_NORMAL;
}

void HashAggregationOperator::ResizeArrayMap(int64_t oldMin)
{
    auto offSet = oldMin - vectorAnalyzer->MinValue();
    auto newTableSize = vectorAnalyzer->GetRange();
    auto newArrayTable = std::make_unique<DefaultArrayMap<AggregateState>>(
        newTableSize, OmniHashmapAllocator(executionContext->GetAllocator()));
    auto newAssigned = newArrayTable->GetAssigned();
    auto newSlots = newArrayTable->GetSlots();
    auto oldTableSize = arrayTable->Size();
    auto oldAssigned = arrayTable->GetAssigned();
    auto oldSlots = arrayTable->GetSlots();
    newSlots[0] = oldSlots[0];
    newAssigned[0] = oldAssigned[0];
    bool hasAgg = aggregators.size() > 0;
    errno_t res1 = EOK;
    if (hasAgg) {
        memcpy(newSlots + 1 + offSet, oldSlots + 1, (oldTableSize - 1) * sizeof(void *));
    }
    memcpy(newAssigned + 1 + offSet, oldAssigned + 1, (oldTableSize - 1) * sizeof(bool));
    newArrayTable->AddElementsSize(arrayTable->GetElementsSize());
    arrayTable.reset(newArrayTable.release());
    resizeArrayMapCnt++;
}

void HashAggregationOperator::MoveEntryArrayTableToHashMap(int64_t minValue)
{
    bool hasAgg = aggregators.size() > 0;
    arrayTable->ForEachValue([&](const auto &value, const auto &index) {
        if (index != 0) {
            if (groupByColumnsHandleType == HandleType::fixedInt32) {
                fixedInt32->InsertOneValueToHashmap<false>(static_cast<int32_t>(index + minValue - 1),
                            reinterpret_cast<AggregateState *>(value));
            } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
                fixedInt64->InsertOneValueToHashmap<false>(static_cast<int64_t>(index + minValue - 1),
                            reinterpret_cast<AggregateState *>(value));
            } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
                fixedInt16->InsertOneValueToHashmap<false>(static_cast<int16_t>(index + minValue - 1),
                            reinterpret_cast<AggregateState *>(value));
            }
            return;
        }
        if (groupByColumnsHandleType == HandleType::fixedInt32) {
            fixedInt32->InsertOneValueToHashmap<true>(0, reinterpret_cast<AggregateState *>(value));
        } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
            fixedInt64->InsertOneValueToHashmap<true>(0, reinterpret_cast<AggregateState *>(value));
        } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
            fixedInt16->InsertOneValueToHashmap<true>(0, reinterpret_cast<AggregateState *>(value));
        }
    });
    arrayTable.reset();
}

int32_t HashAggregationOperator::AddInput(VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }

    // The active operator is reclaimable only here: no analyzer, hashmap,
    // aggregate state, or ownership mutation has happened for this batch yet.
    EnsureInputFits(vecBatch);
    setInputedData(true);
    partialInputRowsSinceFlush_ += static_cast<uint64_t>(rowCount);

    UpdateAddInputInfo(rowCount);
    // do decide hash table mode
    auto oldMin = vectorAnalyzer->MinValue();
    auto preIsArrayMap = vectorAnalyzer->IsArrayHashTableType();
    vectorAnalyzer->DecideHashMode(vecBatch);
    if (vectorAnalyzer->IsArrayHashTableType()) {
        if (arrayTable == nullptr) {
            arrayTable = std::make_unique<DefaultArrayMap<AggregateState>>(
                vectorAnalyzer->GetRange(), OmniHashmapAllocator(executionContext->GetAllocator()));
        } else if (vectorAnalyzer->MinMaxChanged() && resizeArrayMapCnt == 0) {
            ResizeArrayMap(oldMin);
        } else if (vectorAnalyzer->MinMaxChanged() && resizeArrayMapCnt >= 1) {
            vectorAnalyzer->SetNormalHashTable();
        }
        if (vectorAnalyzer->IsArrayHashTableType()) {
            // array hash mode
            auto &groupByCol = this->groupByCols[0];
            BaseVector *groupVector = vecBatch->Get(groupByCol.idx);
            rowsAggStates.resize(rowCount);
            EmplaceToArrayMap(vecBatch, groupVector);
            VectorHelper::FreeVecBatch(vecBatch);
            ResetInputVecBatch();
            MaybeMarkPartialFull();
            return 0;
        }
    }

    if (UNLIKELY(hashmapCapacityReleased_)) {
        ReinitializeHashmapAfterRelease();
    }
    if (UNLIKELY(preIsArrayMap && arrayTable != nullptr)) {
        MoveEntryArrayTableToHashMap(oldMin);
    }

    auto groupColNum = static_cast<int32_t>(this->groupByCols.size());
    if (serialize != nullptr) {
        serialize->ResetSerializer();
    }
    BaseVector *groupVectors[groupColNum];
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto &groupByCol = this->groupByCols[i];
        auto curVector = vecBatch->Get(groupByCol.idx);
        auto omniId = groupByCol.input->GetId();

        if (serialize != nullptr) {
            if (curVector->GetEncoding() == Encoding::OMNI_DICTIONARY) {
                if (dicVectorSerializerCenter[omniId] != nullptr) {
                    serialize->PushBackSerializer(dicVectorSerializerCenter[omniId]);
                    serialize->PushBackComparator(dicVectorComparatorCenter[omniId]);
                } else {
                    serialize->PushBackSerializer(complexVectorSerializerCenter[omniId]);
                }
            } else if (curVector->GetEncoding() == Encoding::OMNI_ENCODING_CONST) {
                if (constVectorSerializerCenter[omniId] != nullptr) {
                    serialize->PushBackSerializer(constVectorSerializerCenter[omniId]);
                    serialize->PushBackComparator(constVectorComparatorCenter[omniId]);
                } else {
                    serialize->PushBackSerializer(complexVectorSerializerCenter[omniId]);
                }
            } else if (omniId == type::OMNI_ARRAY || omniId == type::OMNI_ROW) {
                serialize->PushBackSerializer(complexVectorSerializerCenter[omniId]);
                serialize->PushBackComparator(vectorComparatorCenter[omniId]);
            } else {
                serialize->PushBackSerializer(vectorSerializerCenter[omniId]);
                serialize->PushBackComparator(vectorComparatorCenter[omniId]);
            }
            if (omniId == type::OMNI_ARRAY || omniId == type::OMNI_ROW) {
                serialize->PushBackDeSerializer(complexVectorDeSerializerCenter[omniId]);
            } else {
                serialize->PushBackDeSerializer(vectorDeSerializerCenter[omniId]);
            }
        }
        groupVectors[i] = curVector;
    }

    if (LIKELY(groupByColumnsHandleType == HandleType::serialize)) {
        // Decode all group-by columns upfront to eliminate encoding branches in hot path
        serialize->DecodeGroupByColumns(groupVectors, groupColNum, rowCount);
        Emplace(serialize, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32) {
        Emplace(fixedInt32, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
        Emplace(fixedInt64, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
        Emplace(fixedInt16, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::packedInt32) {
        Emplace(packedInt32, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::packedInt64) {
        Emplace(packedInt64, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::packedInt128) {
        Emplace(packedInt128, vecBatch, groupVectors, groupColNum);
    } else {
        // only serialize method are used now
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        LogError("can not support groupByColumnsHandleType : %d.", groupByColumnsHandleType);
        throw OmniException("no t supported operation", "groupByColumnsHandleType error");
    }
    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    MaybeMarkPartialFull();
    return 0;
}

bool HashAggregationOperator::needsInput()
{
    return Operator::needsInput() && !partialFull_;
}

void HashAggregationOperator::SetPartialFlushMemory(uint64_t maxBytes, uint64_t maxExtendedBytes)
{
    maxPartialAggregationMemory_ = maxBytes;
    maxExtendedPartialAggregationMemory_ = std::max(maxBytes, maxExtendedBytes);
}

uint64_t HashAggregationOperator::PartialTableBytes()
{
    // Arena (aggregate states, serialized keys, row container) plus the hash or
    // array table's own storage: the same bytes a reclaim would return.
    uint64_t bytes = executionContext->GetArena()->TotalBytes();
    if (arrayTable != nullptr) {
        bytes += arrayTable->AllocatedBytes();
    } else if (serialize != nullptr) {
        bytes += serialize->table->ReclaimableBytesOnRelease();
    } else if (fixedInt32 != nullptr) {
        bytes += fixedInt32->table->ReclaimableBytesOnRelease();
    } else if (fixedInt64 != nullptr) {
        bytes += fixedInt64->table->ReclaimableBytesOnRelease();
    } else if (fixedInt16 != nullptr) {
        bytes += fixedInt16->table->ReclaimableBytesOnRelease();
    } else if (packedInt32 != nullptr) {
        bytes += packedInt32->table->ReclaimableBytesOnRelease();
    } else if (packedInt64 != nullptr) {
        bytes += packedInt64->table->ReclaimableBytesOnRelease();
    } else if (packedInt128 != nullptr) {
        bytes += packedInt128->table->ReclaimableBytesOnRelease();
    }
    return bytes;
}

void HashAggregationOperator::MaybeMarkPartialFull()
{
    // Velox HashAggregation::addInput: a partial (non-global) aggregation whose
    // table passed the cap stops taking input and flushes. Once the table has
    // spilled the disk path owns the rest of the operator's life, so no flush.
    if (!isStepPartials || maxPartialAggregationMemory_ == 0 || partialFull_ || hasSpill ||
        abandonPartialRequested_ || GetElementsSize() == 0) {
        return;
    }
    if (PartialTableBytes() > maxPartialAggregationMemory_) {
        partialFull_ = true;
    }
}

void HashAggregationOperator::FinishOutput(int32_t outputRows)
{
    if (partialFlushing_) {
        CompletePartialFlush(outputRows);
        return;
    }
    SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
    ReleaseFinishedOutputState();
}

void HashAggregationOperator::CompletePartialFlush(int32_t outputRows)
{
    // Velox HashAggregation::resetPartialOutputIfNeed +
    // maybeIncreasePartialAggregationMemoryUsage. The reset is the sequence a
    // reclaim already uses (arena reset + table release), minus the disk write;
    // the next AddInput reinitializes the table.
    const uint64_t inputRows = partialInputRowsSinceFlush_;
    const double aggregationPct = inputRows == 0 ? 0.0 : 100.0 * static_cast<double>(outputRows) / static_cast<double>(inputRows);
    const int64_t usedBefore = pool_ == nullptr ? 0 : pool_->currentBytes();

    flushedRows_ += static_cast<uint64_t>(outputRows);
    flushTimes_++;
    executionContext->GetArena()->Reset();
    ReleaseHashmapCapacity();
    outputState = OutputState();
    outputStarted_ = false;
    partialFull_ = false;
    partialFlushing_ = false;
    partialInputRowsSinceFlush_ = 0;

    // If more than this many are unique at full memory, give up on partial agg.
    constexpr double kPartialMinFinalPct = 40.0;
    if (aggregationPct > kPartialMinFinalPct &&
        maxPartialAggregationMemory_ >= maxExtendedPartialAggregationMemory_) {
        abandonPartialRequested_ = true;
    } else {
        maxPartialAggregationMemory_ =
            std::min(maxPartialAggregationMemory_ * 2, maxExtendedPartialAggregationMemory_);
    }

    if (pool_ != nullptr && omniruntime::telemetry::IsReclaimSuccessTelemetryEnabled()) {
        const int64_t usedAfter = pool_->currentBytes();
        std::fprintf(stderr,
            "[OMNI-HASHAGG] event=partial_flush operator=%p flush_times=%lu output_rows=%d input_rows=%lu "
            "aggregation_pct=%.1f next_cap=%lu abandon=%d used_before=%ld used_after=%ld reserved=%ld\n",
            static_cast<void *>(this), static_cast<unsigned long>(flushTimes_), outputRows,
            static_cast<unsigned long>(inputRows), aggregationPct,
            static_cast<unsigned long>(maxPartialAggregationMemory_), static_cast<int>(abandonPartialRequested_),
            static_cast<long>(usedBefore), static_cast<long>(usedAfter), static_cast<long>(pool_->reservedBytes()));
    }
}

/**
 * @param types
 * @return rowSize
 * All the output data types are determined in this function. Following allocation for output vectors and filling
 * value should use the 'types' parameter instead of using input vector types.
 */
int32_t HashAggregationOperator::InitMaxRowCountAndOutputTypes()
{
    int32_t rowSize = 0;
    for (auto &i: groupByCols) {
        outputTypes.push_back(i.input);
        rowSize += OperatorUtil::GetTypeSize(i.input);
    }
    for (auto &aggregator: aggregators) {
        const std::vector<DataTypePtr> &aggTypes = aggregator->GetOutputTypes().Get();
        for (auto dataType: aggTypes) {
            outputTypes.push_back(dataType);
            rowSize += OperatorUtil::GetTypeSize(dataType);
        }
    }
    return rowSize;
}

void HashAggregationOperator::InitSpillInfos()
{
    if (serialize != nullptr) {
        spillTypes.push_back(LongType());
    }
    spillTypes.push_back(VarcharType());
    for (auto &aggregator: aggregators) {
        auto currentSpillType = aggregator->GetSpillType();
        aggTypes.insert(aggTypes.end(), currentSpillType.begin(), currentSpillType.end());
        spillTypes.insert(spillTypes.end(), currentSpillType.begin(), currentSpillType.end());
    }
    SortOrder sortOrder;
    sortOrders.resize(1, sortOrder);
    if (serialize != nullptr) {
        groupByCloIdx.resize(1, 1);
    } else {
        groupByCloIdx.resize(1, 0);
    }
    aggregationSort = std::make_unique<AggregationSort>(aggregators);
}

// Scalar leaf vectors are pool-bound; the ArrayVector/MapVector/RowVector
// containers themselves are not. Nested complex types go through the
// CreateComplexVector(pool, ...) overload (the no-pool overload is deprecated).
void SetArrayVector(VectorBatch *vecBatch, DataTypePtr elementType, int32_t rowCount,
                    omniruntime::mem::OmniTaskMemoryPool *pool)
{
    std::shared_ptr<BaseVector> elementVector;
    auto elemTypeId = elementType->GetId();
    switch (elemTypeId) {
        case type::OMNI_BYTE:
            elementVector = std::make_shared<Vector<int8_t>>(0, pool);
            break;
        case type::OMNI_INT:
        case type::OMNI_DATE32:
        case type::OMNI_TIME32:
            elementVector = std::make_shared<Vector<int32_t>>(0, pool);
            break;
        case type::OMNI_LONG:
        case type::OMNI_DATE64:
        case type::OMNI_TIME64:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DECIMAL64:
            elementVector = std::make_shared<Vector<int64_t>>(0, pool);
            break;
        case type::OMNI_SHORT:
            elementVector = std::make_shared<Vector<int16_t>>(0, pool);
            break;
        case type::OMNI_DOUBLE:
            elementVector = std::make_shared<Vector<double>>(0, pool);
            break;
        case type::OMNI_FLOAT:
            elementVector = std::make_shared<Vector<float>>(0, pool);
            break;
        case type::OMNI_BOOLEAN:
            elementVector = std::make_shared<Vector<bool>>(0, pool);
            break;
        case type::OMNI_DECIMAL128:
            elementVector = std::make_shared<Vector<Decimal128>>(0, pool);
            break;
        case type::OMNI_CHAR:
        case type::OMNI_VARCHAR:
        case type::OMNI_VARBINARY:
            elementVector = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(0, pool);
            break;
        case type::OMNI_ARRAY:
        case type::OMNI_MAP:
        case type::OMNI_ROW:
            // Pool-bound CreateComplexVector overload (vector_helper.h:142) recursively
            // pool-threads scalar leaves; nested Array/Map/Row container ctors stay no-pool.
            elementVector = std::shared_ptr<BaseVector>(VectorHelper::CreateComplexVector(pool, elementType.get(), 0));
            break;
        default:
            throw omniruntime::exception::OmniException("Set ArrayVector error, unsupport element type:", std::to_string(elemTypeId));
    }

    auto arrayVector = new ArrayVector(rowCount, elementVector, pool);
    vecBatch->Append(arrayVector);
}

void SetMapVector(VectorBatch *vecBatch, DataTypePtr keyType, DataTypePtr valueType, int32_t rowCount,
                  omniruntime::mem::OmniTaskMemoryPool *pool)
{
    std::shared_ptr<BaseVector> keyVector(VectorHelper::CreateComplexVector(pool, keyType.get(), 0));
    std::shared_ptr<BaseVector> valueVector(VectorHelper::CreateComplexVector(pool, valueType.get(), 0));
    auto *mapVector = new MapVector(rowCount, keyVector, valueVector, pool);
    vecBatch->Append(mapVector);
}

void SetRowVector(VectorBatch *vecBatch, const std::vector<DataTypePtr> &fieldTypes, int32_t rowCount,
                  omniruntime::mem::OmniTaskMemoryPool *pool)
{
    std::vector<std::shared_ptr<BaseVector>> children;
    for (const auto &fieldType : fieldTypes) {
        children.push_back(std::shared_ptr<BaseVector>(VectorHelper::CreateComplexVector(pool, fieldType.get(), rowCount)));
    }
    auto *rowVector = new RowVector(rowCount, children, pool);
    vecBatch->Append(rowVector);
}

void HashAggregationOperator::SetVectors(VectorBatch *output, const std::vector<DataTypePtr> &types, int32_t rowCount)
{
    // Every dispatch site passes this->pool(), so the function-pointer table and
    // the Set{Array,Map,Row}Vector helpers allocate scalar leaf vectors from the
    // task pool.
    auto *pool = this->pool();
    auto colSize = types.size();
    for (size_t colIndex = 0; colIndex < colSize; ++colIndex) {
        const DataTypePtr &type = types[colIndex];
        auto typeId = type->GetId();
        if (typeId < DATA_TYPE_MAX_COUNT) {
            GROUP_AGG_FUNCTIONS[type->GetId()](output, rowCount, pool);
        } else if (typeId == OMNI_ARRAY) {
            auto arrayType = std::static_pointer_cast<ArrayType>(type);
            DataTypePtr elementType = arrayType->ElementType();
            SetArrayVector(output, elementType, rowCount, pool);
        } else if (typeId == OMNI_MAP) {
            auto mapType = std::static_pointer_cast<MapType>(type);
            SetMapVector(output, mapType->Key(), mapType->Value(), rowCount, pool);
        } else if (typeId == OMNI_ROW) {
            auto rowType = std::static_pointer_cast<RowType>(type);
            std::vector<DataTypePtr> fieldTypes;
            for (size_t i = 0; i < rowType->Size(); i++) {
                fieldTypes.push_back(rowType->Type(i));
            }
            SetRowVector(output, fieldTypes, rowCount, pool);
        }
    }
}

int32_t HashAggregationOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!hasInputedData()) {
        return 0;
    }
    if (partialFull_) {
        if (hasSpill || noMoreInput_) {
            // A reclaim spilled the table between AddInput and this call (the
            // disk path drains it at the end of input), or the input ended
            // before the flush started: the normal final output is the flush.
            partialFull_ = false;
        } else {
            partialFlushing_ = true;
        }
    }
    EnsureOutputFits();
    outputStarted_ = true;
    int32_t expectedBatchSize = 0;
    if (LIKELY(groupByColumnsHandleType == HandleType::serialize)) {
        expectedBatchSize = Output(serialize, outputVecBatch);
    } else if (LIKELY(groupByColumnsHandleType == HandleType::fixedInt32)) {
        expectedBatchSize = Output(fixedInt32, outputVecBatch);
    } else if (LIKELY(groupByColumnsHandleType == HandleType::fixedInt64)) {
        expectedBatchSize = Output(fixedInt64, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
        expectedBatchSize = Output(fixedInt16, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::packedInt32) {
        expectedBatchSize = Output(packedInt32, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::packedInt64) {
        expectedBatchSize = Output(packedInt64, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::packedInt128) {
        expectedBatchSize = Output(packedInt128, outputVecBatch);
    } else {
        SetStatus(OMNI_STATUS_ERROR);
        LogError("other groupby field handle type %d not implement now ", groupByColumnsHandleType);
        throw std::out_of_range("other groupby field handle type not implement");
    }
    // Reservation belongs to the shared task pool. Keep unused headroom for
    // subsequent output batches and sibling operators instead of returning it
    // to Spark after every batch. Pre-reservation and task teardown are unchanged.
    return expectedBatchSize;
}

OmniStatus HashAggregationOperator::Close()
{
    if (pool_ != nullptr) {
        pool_->unregisterSpillable(this);
    }
    delete[] sourceTypes;
    sourceTypes = nullptr;
    // delete spiller object when exception occurs
    if (spiller != nullptr) {
        spiller->RemoveSpillFiles();
    }
    delete spiller;
    spiller = nullptr;
    delete spillMerger;
    spillMerger = nullptr;
    delete vectorAnalyzer;
    vectorAnalyzer = nullptr;

    executionContext->GetArena()->Reset();
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}

template<typename T, typename GroupMap>
void HashAggregationOperator::InsertValueToArrayMap(GroupMap &arrayMap, BaseVector *groupVector,
                                                    int32_t rowIdx)
{
    // just one columnar
    auto curVector = reinterpret_cast<Vector<T> *>(groupVector);
    if (!curVector->IsNull(rowIdx)) {
        auto key = curVector->GetValue(rowIdx);
        arrayMap.InsertJoinKeysToHashmap(static_cast<size_t>(vectorAnalyzer->ComputeKey(key)));
    } else {
        arrayMap.InsertJoinKeysToHashmap(0);
    }
}

void HashAggregationOperator::InitState(int64_t aggStateAddress)
{
    size_t aggNum = aggregators.size();
    auto aggState = reinterpret_cast<AggregateState *>(aggStateAddress);
    for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
        auto &aggregator = aggregators[aggIdx];
        aggregator->InitState(aggState);
    }
}

void HashAggregationOperator::ProcessStates(VectorBatch *vecBatch)
{
    size_t aggNum = aggregators.size();
    if (aggFiltersCount > 0) {
        int32_t filterOffset = vecBatch->GetVectorCount() - aggFiltersCount;
        for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            if (hasAggFilters[aggIdx] == 1) {
                aggregator->ProcessGroupFilter(rowsAggStates, aggIdx, vecBatch, filterOffset, 0);
                filterOffset++;
            } else {
                aggregator->ProcessGroup(rowsAggStates, vecBatch, 0);
            }
        }
    } else {
        for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            aggregator->ProcessGroup(rowsAggStates, vecBatch, 0);
        }
    }
}

template <bool hasNull>
void ComputeHashSIMD(int64_t *key, uint8_t nullMask, uint64_t *hashes, int64x2_t vMin, uint64x2_t vOne)
{
    constexpr int32_t vecLanes = 8;
    int64x2x4_t vKey = vld4q_s64(key);
    uint64x2x4_t vHashes;
    vHashes.val[0] = vaddq_u64((uint64x2_t)vsubq_s64(vKey.val[0], vMin), vOne);
    vHashes.val[1] = vaddq_u64((uint64x2_t)vsubq_s64(vKey.val[1], vMin), vOne);
    vHashes.val[2] = vaddq_u64((uint64x2_t)vsubq_s64(vKey.val[2], vMin), vOne);
    vHashes.val[3] = vaddq_u64((uint64x2_t)vsubq_s64(vKey.val[3], vMin), vOne);
    if constexpr (hasNull) {
        alignas(ALIGNMENT_SIZE) int64_t nulls[8];
        nullMask = ~nullMask;
        for (int i = 0; i < vecLanes; i++) {
            nulls[i] = -(((nullMask) >> i) & 1);
        }
        uint64x2x4_t vNull = vld4q_u64(reinterpret_cast<uint64_t*>(nulls));
        vHashes.val[0] = vandq_u64(vHashes.val[0], vNull.val[0]);
        vHashes.val[1] = vandq_u64(vHashes.val[1], vNull.val[1]);
        vHashes.val[2] = vandq_u64(vHashes.val[2], vNull.val[2]);
        vHashes.val[3] = vandq_u64(vHashes.val[3], vNull.val[3]);
    }
    vst4q_u64(hashes, vHashes);
}

template <bool hasNull>
void ComputeHashSIMD(int32_t *key, uint16_t nullMask, uint32_t *hashes, int32x4_t vMin, uint32x4_t vOne)
{
    constexpr int32_t vecLanes = 16;
    int32x4x4_t vKey = vld4q_s32(key);
    uint32x4x4_t vHashes;
    vHashes.val[0] = vaddq_u32((uint32x4_t)vsubq_s32(vKey.val[0], vMin), vOne);
    vHashes.val[1] = vaddq_u32((uint32x4_t)vsubq_s32(vKey.val[1], vMin), vOne);
    vHashes.val[2] = vaddq_u32((uint32x4_t)vsubq_s32(vKey.val[2], vMin), vOne);
    vHashes.val[3] = vaddq_u32((uint32x4_t)vsubq_s32(vKey.val[3], vMin), vOne);
    if constexpr (hasNull) {
        alignas(ALIGNMENT_SIZE) int32_t nulls[16];
        nullMask = ~nullMask;
        for (int i = 0; i < vecLanes; i++) {
            nulls[i] = -(((nullMask) >> i) & 1);
        }
        uint32x4x4_t vNull = vld4q_u32(reinterpret_cast<uint32_t*>(nulls));
        vHashes.val[0] = vandq_u32(vHashes.val[0], vNull.val[0]);
        vHashes.val[1] = vandq_u32(vHashes.val[1], vNull.val[1]);
        vHashes.val[2] = vandq_u32(vHashes.val[2], vNull.val[2]);
        vHashes.val[3] = vandq_u32(vHashes.val[3], vNull.val[3]);
    }
    vst4q_u32(hashes, vHashes);
}

template <bool hasNull>
void ComputeHashSIMD(int16_t *key, uint32_t nullMask, uint16_t *hashes, int16x8_t vMin, uint16x8_t vOne)
{
    constexpr int32_t vecLanes = 32;
    int16x8x4_t vKey = vld4q_s16(key);
    uint16x8x4_t vHashes;
    vHashes.val[0] = vaddq_u16((uint16x8_t)vsubq_s16(vKey.val[0], vMin), vOne);
    vHashes.val[1] = vaddq_u16((uint16x8_t)vsubq_s16(vKey.val[1], vMin), vOne);
    vHashes.val[2] = vaddq_u16((uint16x8_t)vsubq_s16(vKey.val[2], vMin), vOne);
    vHashes.val[3] = vaddq_u16((uint16x8_t)vsubq_s16(vKey.val[3], vMin), vOne);
    if constexpr (hasNull) {
        alignas(ALIGNMENT_SIZE) int16_t nulls[32];
        nullMask = ~nullMask;
        for (int i = 0; i < vecLanes; i++) {
            nulls[i] = static_cast<int16_t>(-(((nullMask) >> i) & 1));
        }
        uint16x8x4_t vNull = vld4q_u16(reinterpret_cast<uint16_t*>(nulls));
        vHashes.val[0] = vandq_u16(vHashes.val[0], vNull.val[0]);
        vHashes.val[1] = vandq_u16(vHashes.val[1], vNull.val[1]);
        vHashes.val[2] = vandq_u16(vHashes.val[2], vNull.val[2]);
        vHashes.val[3] = vandq_u16(vHashes.val[3], vNull.val[3]);
    }
    vst4q_u16(hashes, vHashes);
}

template <typename T>
void HashAggregationOperator::InsertAggStatesToArrayMap(T *hashes, int32_t vecLanes, bool *isAssigned, int64_t *slots,
                                                        mem::SimpleArenaAllocator &arenaAllocator,
                                                        int32_t probePosition)
{
    int64_t *matchSlotsData = reinterpret_cast<int64_t*>(rowsAggStates.data());
    auto arrayTablePtr = arrayTable.get();
    int32_t missCnt = 0;
    for (auto miss = 0; miss < vecLanes; ++miss) {
        auto hash = hashes[miss];
        if (!isAssigned[hash]) {
            missCnt++;
            auto aggSateAddress = reinterpret_cast<int64_t>(arenaAllocator.Allocate(totalAggStatesSize));
            InitState(aggSateAddress);
            slots[hash] = aggSateAddress;
            isAssigned[hash] = true;
        }
    }
    arrayTablePtr->AddElementsSize(missCnt);
    // store matched rowIndex ,rowRefList* and no matched rowIndex
    for (int j = 0; j < vecLanes; j++) {
        matchSlotsData[probePosition + j] = slots[hashes[j]];
    }
}

template<typename T, bool hasNull>
void HashAggregationOperator::ArrayGroupProbeSIMD(BaseVector *groupVector, VectorBatch *vecBatch)
{
    using namespace omniruntime::type;
    using unsignedT = typename std::make_unsigned<T>::type;
    auto &arenaAllocator = *(executionContext->GetArena());
    T *groupValueBase = reinterpret_cast<T *>(VectorHelper::UnsafeGetValues(groupVector));
    __builtin_prefetch(groupValueBase, 0, 3);
    auto rowCount = groupVector->GetSize();
    auto arrayTablePtr = arrayTable.get();
    bool *isAssigned = arrayTablePtr->GetAssigned();
    auto slots = reinterpret_cast<int64_t *>(arrayTablePtr->GetSlots());
    int32_t probePosition = 0;
    T min = static_cast<T>(vectorAnalyzer->MinValue());
    // 4*128/8
    constexpr int32_t simdLen = 64;
    constexpr int32_t vecLanes = simdLen / sizeof(T);
    int32_t end = rowCount / vecLanes * vecLanes;
    alignas(ALIGNMENT_SIZE) unsignedT hashes[vecLanes];
    int64_t *matchSlotsData = reinterpret_cast<int64_t *>(rowsAggStates.data());
    if constexpr (std::is_same_v<T, int64_t>) {
        auto nulls = reinterpret_cast<uint8_t *>(unsafe::UnsafeBaseVector::GetNulls(groupVector));
        int64x2_t vMin = vdupq_n_s64(min);
        uint64x2_t vOne = vdupq_n_u64(1);
        for (; probePosition < end; probePosition += vecLanes) {
            if constexpr (hasNull) {
                ComputeHashSIMD<hasNull>(groupValueBase, *nulls, hashes, vMin, vOne);
                nulls += 1;
            } else {
                ComputeHashSIMD<hasNull>(groupValueBase, 0, hashes, vMin, vOne);
            }
            groupValueBase += vecLanes;
            __builtin_prefetch(groupValueBase, 0, 3);
            InsertAggStatesToArrayMap(hashes, vecLanes, isAssigned, slots, arenaAllocator, probePosition);
        }
    }
    if constexpr (std::is_same_v<T, int32_t>) {
        auto nulls = reinterpret_cast<uint16_t *>(unsafe::UnsafeBaseVector::GetNulls(groupVector));
        int32x4_t vMin = vdupq_n_s32(min);
        uint32x4_t vOne = vdupq_n_u32(1);
        for (; probePosition < end; probePosition += vecLanes) {
            if constexpr (hasNull) {
                ComputeHashSIMD<hasNull>(groupValueBase, *nulls, hashes, vMin, vOne);
                nulls += 1;
            } else {
                ComputeHashSIMD<hasNull>(groupValueBase, 0, hashes, vMin, vOne);
            }
            groupValueBase += vecLanes;
            __builtin_prefetch(groupValueBase, 0, 3);
            InsertAggStatesToArrayMap(hashes, vecLanes, isAssigned, slots, arenaAllocator, probePosition);
        }
    }
    if constexpr (std::is_same_v<T, int16_t>) {
        auto nulls = reinterpret_cast<uint32_t *>(unsafe::UnsafeBaseVector::GetNulls(groupVector));
        int16x8_t vMin = vdupq_n_s16(min);
        uint16x8_t vOne = vdupq_n_u16(1);
        for (; probePosition < end; probePosition += vecLanes) {
            if constexpr (hasNull) {
                ComputeHashSIMD<hasNull>(groupValueBase, *nulls, hashes, vMin, vOne);
                nulls += 1;
            } else {
                ComputeHashSIMD<hasNull>(groupValueBase, 0, hashes, vMin, vOne);
            }
            groupValueBase += vecLanes;
            __builtin_prefetch(groupValueBase, 0, 3);
            InsertAggStatesToArrayMap(hashes, vecLanes, isAssigned, slots, arenaAllocator, probePosition);
        }
    }
    // deal rest group values
    for (; probePosition < rowCount; probePosition++, groupValueBase += 1) {
        int64_t hashValue;
        if constexpr (hasNull) {
            if (UNLIKELY(groupVector->IsNull(probePosition))) {
                hashValue = 0;
            } else {
                hashValue = *groupValueBase - min + 1;
            }
        } else {
            hashValue = *groupValueBase - min + 1;
        }
        if (!isAssigned[hashValue]) {
            arrayTablePtr->AddElementsSize(1);
            auto aggSateAddress = reinterpret_cast<int64_t>(arenaAllocator.Allocate(totalAggStatesSize));
            InitState(aggSateAddress);
            slots[hashValue] = aggSateAddress;
            isAssigned[hashValue] = true;
        }
        matchSlotsData[probePosition] = slots[hashValue];
    }
    ProcessStates(vecBatch);
}

void HashAggregationOperator::EmplaceToArrayMap(VectorBatch *vecBatch, BaseVector *groupVector)
{
    int32_t rowCount = vecBatch->GetRowCount();
    size_t aggNum = aggregators.size();
    auto typeId = groupVector->GetTypeId();
    if (aggNum == 0) {
        // no aggregator, so just perform groupby
        switch (typeId) {
            case OMNI_INT:
            case OMNI_DATE32:
                for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                    InsertValueToArrayMap<int32_t>(*this->arrayTable, groupVector, rowIdx);
                }
                break;
            case OMNI_SHORT:
                for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                    InsertValueToArrayMap<int16_t>(*this->arrayTable, groupVector, rowIdx);
                }
                break;
            case OMNI_BYTE:
                for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                    InsertValueToArrayMap<int8_t>(*this->arrayTable, groupVector, rowIdx);
                }
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                    InsertValueToArrayMap<int64_t>(*this->arrayTable, groupVector, rowIdx);
                }
                break;
            default:
                std::string omniExceptionInfo = std::to_string(typeId) + "should not call EmplaceToArrayMap";
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
        return;
    }

    // aggNum > 0
    switch (typeId) {
        case OMNI_INT:
        case OMNI_DATE32:
            if (groupVector->HasNull()) {
                ArrayGroupProbeSIMD<int32_t, true>(groupVector, vecBatch);
            } else {
                ArrayGroupProbeSIMD<int32_t, false>(groupVector, vecBatch);
            }
            break;
        case OMNI_SHORT:
            if (groupVector->HasNull()) {
                ArrayGroupProbeSIMD<int16_t, true>(groupVector, vecBatch);
            } else {
                ArrayGroupProbeSIMD<int16_t, false>(groupVector, vecBatch);
            }
            break;
        case OMNI_BYTE:
            if (groupVector->HasNull()) {
                ArrayGroupProbeSIMD<int8_t, true>(groupVector, vecBatch);
            } else {
                ArrayGroupProbeSIMD<int8_t, false>(groupVector, vecBatch);
            }
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            if (groupVector->HasNull()) {
                ArrayGroupProbeSIMD<int64_t, true>(groupVector, vecBatch);
            } else {
                ArrayGroupProbeSIMD<int64_t, false>(groupVector, vecBatch);
            }
            break;
        default:
            std::string omniExceptionInfo = std::to_string(typeId) + "should not call EmplaceToArrayMap";
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }
}

template<typename Serialize>
void HashAggregationOperator::Emplace(Serialize &emplaceKey, VectorBatch *vecBatch, BaseVector **groupVectors,
                                      int32_t groupColNum)
{
    int32_t rowCount = vecBatch->GetRowCount();
    size_t aggNum = aggregators.size();
    currentRowStates.resize(rowCount);
    newGroupStates.reserve(rowCount);

    auto *curVector = groupVectors[0];
    auto curEncoding = curVector->GetEncoding();
    emplaceKey->EmplaceTable(groupVectors, groupColNum, rowCount, currentRowStates, newGroupStates, curEncoding);
    if (aggNum == 0) {
        return;
    }

    // Adjust state pointers to point to AggState offset
    // In the new RowContainer layout, keys are at the beginning and
    // AggState is at aggStateOffset(). The pointers returned by EmplaceTable
    // point to the row beginning (key data). We need to shift them to
    // point to the AggState region for aggregator processing.
    if (groupByColumnsHandleType == HandleType::serialize && serialize != nullptr) {
        int32_t aggStateOffset = serialize->AggStateOffset();
        for (int32_t i = 0; i < rowCount; ++i) {
            currentRowStates[i] = currentRowStates[i] + aggStateOffset;
        }
        for (auto &state : newGroupStates) {
            state = state + aggStateOffset;
        }
    }

    if (aggFiltersCount > 0) {
        int32_t filterOffset = vecBatch->GetVectorCount() - aggFiltersCount;
        for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            if (!newGroupStates.empty()) {
                aggregator->InitStates(newGroupStates);
            }
            if (aggIdx < hasAggFilters.size() && hasAggFilters[aggIdx] == 1) {
                aggregator->ProcessGroupFilter(currentRowStates, aggIdx, vecBatch, filterOffset, 0);
                filterOffset++;
            } else {
                aggregator->ProcessGroup(currentRowStates, vecBatch, 0);
            }
        }
    } else {
        for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            if (!newGroupStates.empty()) {
                aggregator->InitStates(newGroupStates);
            }
            aggregator->ProcessGroup(currentRowStates, vecBatch, 0);
        }
    }
    newGroupStates.clear();
}

template<typename Deserialize>
void HashAggregationOperator::TraverseHashmapToGetOneResult(Deserialize &deserializeHashmap, VectorBatch *output)
{
    const int32_t expectSize = output->GetRowCount();
    int32_t groupColNum = static_cast<int32_t>(this->groupByCols.size());
    std::vector<BaseVector *> groupOutputVectors(groupColNum);
    for (int32_t i = 0; i < groupColNum; i++) {
        groupOutputVectors[i] = output->Get(i);
    }

    std::vector<AggregateState *> groupStates(expectSize);

    // For the serialize handler, use the new RowContainer-based Extract
    // which returns row pointers with key data at column offsets and
    // AggState at aggStateOffset
    if (groupByColumnsHandleType == HandleType::serialize && serialize != nullptr) {
        int32_t aggStateOffset = serialize->AggStateOffset();
        serialize->Extract(expectSize, outputState,
            [&](uint8_t* rowPtr, uint8_t* value, int32_t idx) mutable {
                // Parse key columns from RowContainer row
                serialize->ParseKeyToCols(rowPtr, groupOutputVectors, groupColNum, idx);
                // AggState is at row + aggStateOffset
                groupStates[idx] = reinterpret_cast<AggregateState*>(rowPtr + aggStateOffset);
            }, [&](uint8_t* rowPtr, uint8_t* value, int32_t idx) mutable {
                // Null key row
                serialize->ParseNull(reinterpret_cast<char*>(rowPtr), groupOutputVectors, groupColNum, idx);
                groupStates[idx] = reinterpret_cast<AggregateState*>(rowPtr + aggStateOffset);
            });
    } else {
        // For fixed/packed handlers, use the existing hash table traversal
        deserializeHashmap->Extract(expectSize, outputState,
            [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                deserializeHashmap->ParseKeyToCols(key, groupOutputVectors, groupColNum, idx);
                groupStates[idx] = value;
            }, [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                deserializeHashmap->ParseNull(key, groupOutputVectors, groupColNum, idx);
                groupStates[idx] = value;
        });
    }

    const size_t aggNum = this->aggregators.size();
    if (aggNum > 0) {
        auto aggOutputStartIndex = groupColNum;
        for (size_t aggIndex = 0; aggIndex < aggNum; ++aggIndex) {
            auto &aggregator = aggregators[aggIndex];
            const auto oneAggOutputCols = aggOutputTypes[aggIndex].GetSize();
            std::vector<BaseVector *> adaptAggVectors(oneAggOutputCols);
            for (auto j = 0; j < oneAggOutputCols; j++) {
                adaptAggVectors[j] = output->Get(aggOutputStartIndex + j);
            }
            aggOutputStartIndex += oneAggOutputCols;
            aggregator->ExtractValuesBatch(groupStates, adaptAggVectors, 0, expectSize);
        }
    }
}

template<bool hasAgg, typename T>
void HashAggregationOperator::TraverseArrayMapGetOutput(BaseVector *groupVector,
                                                        std::vector<AggregateState *> *states, int64_t minValue)
{
    int32_t lambdaRowIndex = 0;
    int32_t outputRows = groupVector->GetSize();
    auto func = [&](const auto &value, const auto &index) {
        if (index != 0) {
            static_cast<Vector<T> *>(groupVector)->SetValue(lambdaRowIndex, index + minValue - 1);
        } else {
            groupVector->SetNull(lambdaRowIndex);
        }
        if constexpr (hasAgg) {
            (*states)[lambdaRowIndex] = value;
        }
        lambdaRowIndex++;
    };
    this->arrayTable->OutputEachValue(func, outputState.outputHashmapPos, outputRows);
}

template<bool hasAgg>
void HashAggregationOperator::TraverseArrayMap(BaseVector *groupVector,
                                               std::vector<AggregateState *> *states)
{
    auto typeId = groupVector->GetTypeId();
    auto minValue = vectorAnalyzer->MinValue();
    int32_t outputRows = groupVector->GetSize();
    switch (typeId) {
        case OMNI_INT:
        case OMNI_DATE32:
            TraverseArrayMapGetOutput<hasAgg, int32_t>(groupVector, states, minValue);
            break;
        case OMNI_SHORT:
            TraverseArrayMapGetOutput<hasAgg, int16_t>(groupVector, states, minValue);
            break;
        case OMNI_BYTE:
            TraverseArrayMapGetOutput<hasAgg, int8_t>(groupVector, states, minValue);
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            TraverseArrayMapGetOutput<hasAgg, int64_t>(groupVector, states, minValue);
            break;
        default:
            std::string omniExceptionInfo =
                    std::to_string(typeId) + "should not call TraverseArrayMapToGetOneResult";
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }
    outputState.hasBeenOutputNum += outputRows;
}

void HashAggregationOperator::TraverseArrayMapToGetOneResult(VectorBatch *output)
{
    const size_t aggNum = this->aggregators.size();
    const int32_t expectSize = output->GetRowCount();
    auto groupVector = output->Get(0);
    if (aggNum == 0) {
        TraverseArrayMap<false>(groupVector, nullptr);
        return;
    }

    std::vector<AggregateState *> states(expectSize);
    TraverseArrayMap<true>(groupVector, &states);
    auto aggOutputStartIndex = 1;
    for (size_t aggIndex = 0; aggIndex < aggNum; ++aggIndex) {
        auto &aggregator = aggregators[aggIndex];
        const auto oneAggOutputCols = aggOutputTypes[aggIndex].GetSize();
        std::vector<BaseVector *> adaptAggVectors(oneAggOutputCols);
        for (auto j = 0; j < oneAggOutputCols; j++) {
            adaptAggVectors[j] = output->Get(aggOutputStartIndex + j);
        }
        aggOutputStartIndex += oneAggOutputCols;
        aggregator->ExtractValuesBatch(states, adaptAggVectors, 0, expectSize);
    }
}

ErrorCode HashAggregationOperator::SpillToDisk()
{
    auto totalSpillCount = 0;
    if (serialize != nullptr) {
        totalSpillCount = serialize->GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        totalSpillCount = fixedInt32->GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        totalSpillCount = fixedInt64->GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        totalSpillCount = fixedInt16->GetElementsSize();
    } else if (packedInt32 != nullptr) {
        totalSpillCount = packedInt32->GetElementsSize();
    } else if (packedInt64 != nullptr) {
        totalSpillCount = packedInt64->GetElementsSize();
    } else if (packedInt128 != nullptr) {
        totalSpillCount = packedInt128->GetElementsSize();
    }
    auto aggregationSortPtr = aggregationSort.get();
    if (serialize != nullptr) {
        // Serialized keys must be reconstructed while the original hash table
        // remains alive. Keep only one sorted spill run in scratch memory at a
        // time; retaining all reconstructed keys can exhaust the task pool.
        constexpr int32_t kMaxRowsPerRun = 64 * 1024;
        constexpr size_t kMaxScratchBytesPerRun = 8 * 1024 * 1024;
        // The scratch is spill working memory, not operator state: it is taken outside the task
        // pool because the spill usually runs inside Spark-driven reclaim with no headroom left,
        // and it is released before the spill returns. Its peak is reported in the operator stats.
        mem::Allocator unpooledScratchAllocator(nullptr);
        mem::SimpleArenaAllocator scratch(64 * 1024, &unpooledScratchAllocator);
        int32_t spilledRows = 0;
        while (spilledRows < totalSpillCount) {
            const int32_t runRows = std::min(kMaxRowsPerRun, totalSpillCount - spilledRows);
            aggregationSort->ResizeKvVector(runRows);
            const int32_t extractedRows = serialize->SpillExtract(runRows, kMaxScratchBytesPerRun,
                scratch, spillOutputState,
                [&](const auto &key, int64_t hashVal, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVectorWithHashVal(key, value, idx, hashVal);
                }, [&](const auto &key, int64_t hashVal, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVectorWithHashVal(key, value, idx, hashVal);
            });
            if (extractedRows == 0) {
                aggregationSort->ClearVector();
                LogError("Serialized hash aggregation spill stopped before all rows were extracted.");
                return ErrorCode::UNSUPPORTED;
            }
            aggregationSort->ResizeKvVector(extractedRows);
            aggregationSort->SortKvVector(true);
            auto result = spiller->Spill(aggregationSort.get(), this, true);
            stats_.spillScratchPeakBytes = std::max<uint64_t>(stats_.spillScratchPeakBytes,
                scratch.TotalBytes() + spiller->ScratchPeakBytes());
            aggregationSort->ClearVector();
            if (result != ErrorCode::SUCCESS) {
                return result;
            }
            scratch.Reset();
            spilledRows += extractedRows;
        }
        aggregationSort->ReleaseVectors();
        return ErrorCode::SUCCESS;
    }

    // Fixed keys are materialized into kv entries plus a std::string per group. Spilling the
    // whole table in one run doubles its footprint exactly when the pool is short of memory,
    // so write bounded sorted runs; the unspill merge already handles several runs.
    constexpr int32_t kMaxFixedRowsPerRun = kMaxFixedRowsPerSpillRun;
    auto extractRun = [&](int32_t runRows) -> int32_t {
        auto parse = [&](const auto &key, uint8_t *value, int32_t idx) mutable {
            aggregationSortPtr->ParseHashMapToVector(key, value, idx);
        };
        auto parseBytes = [&](const auto &key, uint8_t *value, int32_t idx) mutable {
            aggregationSortPtr->ParseHashMapToVectorAsBytes(key, value, idx);
        };
        auto parseNull = [&](const auto &key, uint8_t *value, int32_t idx) mutable {
            aggregationSortPtr->ParseNullHashMapToVector(key, value, idx);
        };
        if (fixedInt32 != nullptr) {
            return fixedInt32->Extract(runRows, spillOutputState, parse, parseNull);
        } else if (fixedInt64 != nullptr) {
            return fixedInt64->Extract(runRows, spillOutputState, parse, parseNull);
        } else if (fixedInt16 != nullptr) {
            return fixedInt16->Extract(runRows, spillOutputState, parse, parseNull);
        } else if (packedInt32 != nullptr) {
            return packedInt32->Extract(runRows, spillOutputState, parseBytes, parseNull);
        } else if (packedInt64 != nullptr) {
            return packedInt64->Extract(runRows, spillOutputState, parseBytes, parseNull);
        } else if (packedInt128 != nullptr) {
            return packedInt128->Extract(runRows, spillOutputState, parseBytes, parseNull);
        }
        return 0;
    };
    ErrorCode result = ErrorCode::SUCCESS;
    int32_t spilledRows = 0;
    while (spilledRows < totalSpillCount) {
        const int32_t runRows = std::min(kMaxFixedRowsPerRun, totalSpillCount - spilledRows);
        aggregationSort->ResizeKvVector(runRows);
        const int32_t extractedRows = extractRun(runRows);
        if (extractedRows <= 0) {
            break;
        }
        aggregationSort->ResizeKvVector(extractedRows);
        aggregationSort->SortKvVector(false);
        LogDebug("Spill data to disk starting in hash aggregation operator, rowCount=%d\n", extractedRows);
        result = spiller->Spill(aggregationSort.get(), this, false);
        aggregationSort->ClearVector();
        if (result != ErrorCode::SUCCESS) {
            break;
        }
        spilledRows += extractedRows;
    }
    aggregationSort->ReleaseVectors();
    return result;
}

// Spill the array table straight into sorted runs, in slot order, with the same key encoding
// the fixed-key handlers produce. Moving the entries into a hash map first would allocate a
// table sized by the group count from the task pool while the array table is still alive,
// which is exactly when reclaim has no headroom for it.
ErrorCode HashAggregationOperator::SpillArrayTable()
{
    const int64_t minValue = vectorAnalyzer->MinValue();
    const auto totalRows = static_cast<int32_t>(arrayTable->GetElementsSize());
    auto aggregationSortPtr = aggregationSort.get();
    uint32_t slotCursor = 0;
    auto extractRun = [&](auto typedKey, int32_t runRows) {
        using KeyType = decltype(typedKey);
        int32_t row = 0;
        arrayTable->OutputEachValue([&](const auto &value, const auto &index) {
            auto *state = reinterpret_cast<AggregateState *>(value);
            if (index != 0) {
                aggregationSortPtr->ParseHashMapToVector(static_cast<KeyType>(index + minValue - 1), state, row);
            } else {
                aggregationSortPtr->ParseNullHashMapToVector(KeyType{}, state, row);
            }
            ++row;
        }, slotCursor, runRows);
    };
    ErrorCode result = ErrorCode::SUCCESS;
    int32_t spilledRows = 0;
    while (spilledRows < totalRows) {
        const int32_t runRows = std::min(kMaxFixedRowsPerSpillRun, totalRows - spilledRows);
        aggregationSort->ResizeKvVector(runRows);
        if (groupByColumnsHandleType == HandleType::fixedInt32) {
            extractRun(int32_t{}, runRows);
        } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
            extractRun(int64_t{}, runRows);
        } else {
            extractRun(int16_t{}, runRows);
        }
        aggregationSort->SortKvVector(false);
        LogDebug("Spill array table to disk in hash aggregation operator, rowCount=%d\n", runRows);
        result = spiller->Spill(aggregationSort.get(), this, false);
        aggregationSort->ClearVector();
        if (result != ErrorCode::SUCCESS) {
            break;
        }
        spilledRows += runRows;
    }
    aggregationSort->ReleaseVectors();
    return result;
}

ErrorCode HashAggregationOperator::SpillHashMap()
{
    // Array-map groups are spilled directly from the array table (SpillArrayTable); afterwards
    // the operator continues on the normal hash table. Handler types the array path does not
    // cover keep the old route through the already-initialized fixed-key handler.
    const bool spillArray = vectorAnalyzer != nullptr && vectorAnalyzer->IsArrayHashTableType() &&
        arrayTable != nullptr && (groupByColumnsHandleType == HandleType::fixedInt32 ||
        groupByColumnsHandleType == HandleType::fixedInt64 || groupByColumnsHandleType == HandleType::fixedInt16);
    if (!spillArray && vectorAnalyzer != nullptr && vectorAnalyzer->IsArrayHashTableType() && arrayTable != nullptr) {
        if (UNLIKELY(hashmapCapacityReleased_)) {
            ReinitializeHashmapAfterRelease();
        }
        MoveEntryArrayTableToHashMap(vectorAnalyzer->MinValue());
        vectorAnalyzer->SetNormalHashTable();
    }
    auto rowCount = 0;
    if (spillArray) {
        rowCount = static_cast<int32_t>(arrayTable->GetElementsSize());
        if (rowCount == 0) {
            arrayTable.reset();
            vectorAnalyzer->SetNormalHashTable();
        }
    } else if (serialize != nullptr) {
        rowCount = serialize->GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        rowCount = fixedInt32->GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        rowCount = fixedInt64->GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        rowCount = fixedInt16->GetElementsSize();
    } else if (packedInt32 != nullptr) {
        rowCount = packedInt32->GetElementsSize();
    } else if (packedInt64 != nullptr) {
        rowCount = packedInt64->GetElementsSize();
    } else if (packedInt128 != nullptr) {
        rowCount = packedInt128->GetElementsSize();
    }
    if (rowCount == 0) {
        return ErrorCode::SUCCESS;
    }

    if (spiller == nullptr) {
        auto spillConfig = operatorConfig.GetSpillConfig();
        OperatorConfig::CheckSpillConfig(spillConfig);
        InitSpillInfos();
        spiller = new Spiller(DataTypes(spillTypes), groupByCloIdx, sortOrders,
                              operatorConfig.GetSpillConfig()->GetSpillPath(), spillConfig->GetMaxSpillBytes(),
                              spillConfig->GetWriteBufferSize(), spillConfig->IsSpillCompressEnabled());
        hasSpill = true;
    }
    UpdateSpillTimesInfo();
    ErrorCode result;
    if (spillArray) {
        result = SpillArrayTable();
        if (result == ErrorCode::SUCCESS) {
            arrayTable.reset();
            vectorAnalyzer->SetNormalHashTable();
        }
    } else {
        result = SpillToDisk();
    }
    stats_.spillScratchPeakBytes = std::max(stats_.spillScratchPeakBytes, spiller->ScratchPeakBytes());
    if (result == ErrorCode::SUCCESS) {
        spilledRows_ += static_cast<uint64_t>(rowCount);
    }
    spillOutputState.hasBeenOutputNum = 0;
    spillOutputState.outputHashmapPos = 0;
    spillOutputState.rowBegin = nullptr;
    spillOutputState.rowOffset = 0;
    return result;
}

uint64_t HashAggregationOperator::GetSpilledBytes()
{
    return spilledBytes;
}

uint64_t HashAggregationOperator::GetUsedMemBytes()
{
    return usedMemBytes;
}

uint64_t HashAggregationOperator::GetTotalMemBytes()
{
    return totalMemBytes;
}

std::vector<uint64_t> HashAggregationOperator::GetSpecialMetricsInfo()
{
    int arrayLength = 2;  // 根据返回元素个数修改长度
    std::vector<uint64_t> specialMetricsInfoArray(arrayLength);
    specialMetricsInfoArray[0] = GetUsedMemBytes();
    specialMetricsInfoArray[1] = GetTotalMemBytes();

    return specialMetricsInfoArray;
}

uint64_t HashAggregationOperator::GetHashMapUniqueKeys()
{
    // Groups currently in the table, hash or array mode (the array table used
    // to report 0 here, which hid array-keyed partial aggregations from the
    // wrapper's abandon rule).
    return static_cast<uint64_t>(GetElementsSize());
}

uint64_t HashAggregationOperator::CumulativeDistinctKeys()
{
    // Cumulative over the operator's life: groups in the table plus the groups
    // already emitted by a partial flush or written by a spill. The wrapper's
    // abandon rule divides this by cumulative input rows; the in-memory count
    // alone can never reach the ratio again once a reclaim emptied the table.
    return static_cast<uint64_t>(GetElementsSize()) + flushedRows_ + spilledRows_;
}

VectorBatch *HashAggregationOperator::AlignSchema(VectorBatch *inputVecBatch)
{
    // release hashmap memory
    executionContext->GetArena()->Reset();

    int32_t rowCount = inputVecBatch->GetRowCount();
    VectorBatch *result = new VectorBatch(rowCount);
    // handle group columns
    auto groupColNum = static_cast<int32_t>(this->groupByCols.size());
    for (int i = 0; i < groupColNum; ++i) {
        auto &groupByCol = this->groupByCols[i];
        auto curVector = inputVecBatch->Get(groupByCol.idx);
        result->Append(VectorHelper::SliceVector(curVector, 0, rowCount));
    }

    // handle agg columns
    auto aggNum = static_cast<int32_t>(aggregators.size());
    if (aggFiltersCount > 0) {
        int32_t filterOffset = inputVecBatch->GetVectorCount() - aggFiltersCount;
        for (int32_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            if (hasAggFilters[aggIdx] == 1) {
                aggregator->AlignAggSchemaWithFilter(result, inputVecBatch, filterOffset);
                filterOffset++;
            } else {
                aggregator->AlignAggSchema(result, inputVecBatch);
            }
        }
    } else {
        for (int32_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            aggregator->AlignAggSchema(result, inputVecBatch);
        }
    }
    VectorHelper::FreeVecBatch(inputVecBatch);
    return result;
}

void HashAggregationOperator::SetStateOutputVecBatch(VectorBatch *outputVecBatch, int32_t rowCount, int32_t groupColNum,
                                                     int32_t aggNum)
{
    std::vector<BaseVector *> adaptAggVectors;
    auto aggOutputStartIndex = groupColNum;
    for (int32_t aggIndex = 0; aggIndex < aggNum; aggIndex++) {
        auto &aggregator = aggregators[aggIndex];
        const auto oneAggOutputCols = aggOutputTypes[aggIndex].GetSize();
        adaptAggVectors.resize(oneAggOutputCols);
        for (auto j = 0; j < oneAggOutputCols; j++) {
            adaptAggVectors[j] = outputVecBatch->Get(aggOutputStartIndex + j);
        }
        aggOutputStartIndex += oneAggOutputCols;
        try {
            aggregator->ExtractValuesBatch(rowStates, adaptAggVectors, 0, rowCount);
        } catch (const OmniException &oneException) {
            // release VectorBatch when aggregator.ExtractValues throw exception
            // when spark hash agg sum/avg decimal overflow, it will throw exception when
            // OverflowConfigId==OVERFLOW_CONFIG_EXCEPTION
            throw oneException;
        }
    }
}

static VectorBatch *GetVectorBatchFromSlice(VectorBatch *vectorBatch, int32_t rowCount)
{
    auto outputColCount = vectorBatch->GetVectorCount();
    auto *sliceBatch = new VectorBatch(rowCount);
    for (int32_t columnIdx = 0; columnIdx < outputColCount; columnIdx++) {
        auto *vector = vectorBatch->Get(columnIdx);
        sliceBatch->Append(vec::VectorHelper::SliceVector(vector, 0, rowCount));
    }
    return sliceBatch;
}

VectorBatch *HashAggregationOperator::GetOutputFromDiskWithoutAgg(VectorBatch *output)
{
    auto groupColNum = static_cast<int32_t>(this->groupByCols.size());
    std::vector<BaseVector *> groupOutputVectors(groupColNum);
    for (int32_t i = 0; i < groupColNum; i++) {
        groupOutputVectors[i] = output->Get(i);
    }

    auto rowCount = output->GetRowCount();
    int32_t rowIdx = 0;
    bool isEqual = false;
    bool nextKeyIsNew = true;
    for (;;) {
        auto currentVecBatch = spillMerger->CurrentBatchWithEqual(isEqual);
        if (currentVecBatch == nullptr) {
            // construct the final output
            if (rowIdx < rowCount) {
                return GetVectorBatchFromSlice(output, rowIdx);
            } else {
                return output;
            }
        }
        auto currentRowIndex = spillMerger->CurrentRowIndex();
        if (nextKeyIsNew) {
            // construct the final output
            auto keyIndex = serialize != nullptr ? 1 : 0;
            auto keyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(currentVecBatch->Get(keyIndex));
            auto key = keyVector->GetValue(currentRowIndex);
            StringRef keyRef(const_cast<char *>(key.data()), key.size());
            if (serialize != nullptr) {
                serialize->ParseKeyToCols(keyRef, groupOutputVectors, groupColNum, rowIdx);
            } else if (fixedInt32 != nullptr) {
                if (keyRef.size > 0) {
                    auto key = static_cast<int32_t>(std::stoi(keyRef.ToString()));
                    fixedInt32->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
                } else {
                    fixedInt32->ParseNull(0, groupOutputVectors, groupColNum, rowIdx);
                }
            } else if (fixedInt64 != nullptr) {
                if (keyRef.size > 0) {
                    auto key = static_cast<int64_t>(std::stoll(keyRef.ToString()));
                    fixedInt64->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
                } else {
                    fixedInt64->ParseNull(0, groupOutputVectors, groupColNum, rowIdx);
                }
            } else if (fixedInt16 != nullptr) {
                if (keyRef.size > 0) {
                    auto key = static_cast<int16_t>(std::stoi(keyRef.ToString()));
                    fixedInt16->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
                } else {
                    fixedInt16->ParseNull(0, groupOutputVectors, groupColNum, rowIdx);
                }
            } else if (packedInt32 != nullptr) {
                if (UNLIKELY(key.size() != sizeof(int32_t))) {
                    throw omniruntime::exception::OmniException("SPILL_FAILED",
                        "Invalid spilled packedInt32 key size " + std::to_string(key.size()));
                }
                int32_t packedKey = 0;
                memcpy(&packedKey, key.data(), sizeof(packedKey));
                packedInt32->ParseKeyToCols(packedKey, groupOutputVectors, groupColNum, rowIdx);
            } else if (packedInt64 != nullptr) {
                if (UNLIKELY(key.size() != sizeof(int64_t))) {
                    throw omniruntime::exception::OmniException("SPILL_FAILED",
                        "Invalid spilled packedInt64 key size " + std::to_string(key.size()));
                }
                int64_t packedKey = 0;
                memcpy(&packedKey, key.data(), sizeof(packedKey));
                packedInt64->ParseKeyToCols(packedKey, groupOutputVectors, groupColNum, rowIdx);
            } else if (packedInt128 != nullptr) {
                if (UNLIKELY(key.size() != sizeof(omniruntime::type::int128_t))) {
                    throw omniruntime::exception::OmniException("SPILL_FAILED",
                        "Invalid spilled packedInt128 key size " + std::to_string(key.size()));
                }
                omniruntime::type::int128_t packedKey = 0;
                memcpy(&packedKey, key.data(), sizeof(packedKey));
                packedInt128->ParseKeyToCols(packedKey, groupOutputVectors, groupColNum, rowIdx);
            }
            rowIdx++;
        }

        nextKeyIsNew = !isEqual;
        spillMerger->Pop();
        spillRowOffset++;
        if (nextKeyIsNew && rowIdx >= rowCount) {
            return output;
        }
    }
}

VectorBatch *HashAggregationOperator::GetOutputFromDiskWithAgg(VectorBatch *output)
{
    auto groupColNum = static_cast<int32_t>(this->groupByCols.size());
    std::vector<BaseVector *> groupOutputVectors(groupColNum);
    for (int32_t i = 0; i < groupColNum; i++) {
        groupOutputVectors[i] = output->Get(i);
    }

    auto rowCount = output->GetRowCount();
    int32_t rowIdx = 0;
    auto aggNum = static_cast<int32_t>(aggregators.size());
    auto groupStatesPtr = groupStates.get();
    rowStates.resize(rowCount);

    std::vector<UnspillRowInfo> unspillRows(UNSPILL_ROW_COUNT_ONE_BATCH);
    std::vector<AggregateState *> newGroupStates;
    int32_t offset = 0;

    bool isEqual = false;
    bool nextKeyIsNew = true;
    AggregateState *currentGroupStates = nullptr;
    for (;;) {
        auto currentVecBatch = spillMerger->CurrentBatchWithEqual(isEqual);
        if (currentVecBatch == nullptr) {
            if (!newGroupStates.empty()) {
                for (int32_t aggIdx = 0; aggIdx < aggNum; aggIdx++) {
                    aggregators[aggIdx]->InitStates(newGroupStates);
                }
                newGroupStates.clear();
            }
            if (offset > 0) {
                int32_t vectorIndex = serialize != nullptr ? 2 : 1;
                for (int32_t aggIdx = 0; aggIdx < aggNum; aggIdx++) {
                    aggregators[aggIdx]->ProcessGroupUnspill(unspillRows, offset, vectorIndex);
                }
            }

            // construct the final output
            SetStateOutputVecBatch(output, rowIdx, groupColNum, aggNum);
            if (rowIdx < rowCount) {
                return GetVectorBatchFromSlice(output, rowIdx);
            } else {
                return output;
            }
        }
        bool isLastRow = false;
        auto currentRowIndex = spillMerger->CurrentRowIndex(isLastRow);
        if (nextKeyIsNew) {
            // this is a new key
            auto keyIndex = serialize != nullptr ? 1 : 0;
            auto keyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(currentVecBatch->Get(keyIndex));
            auto key = keyVector->GetValue(currentRowIndex);
            StringRef keyRef(const_cast<char *>(key.data()), key.size());
            if (serialize != nullptr) {
                serialize->ParseKeyToCols(keyRef, groupOutputVectors, groupColNum, rowIdx);
            } else if (fixedInt32 != nullptr) {
                if (keyRef.size > 0) {
                    auto key = static_cast<int32_t>(std::stoi(keyRef.ToString()));
                    fixedInt32->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
                } else {
                    fixedInt32->ParseNull(0, groupOutputVectors, groupColNum, rowIdx);
                }
            } else if (fixedInt64 != nullptr) {
                if (keyRef.size > 0) {
                    auto key = static_cast<int64_t>(std::stoll(keyRef.ToString()));
                    fixedInt64->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
                } else {
                    fixedInt64->ParseNull(0, groupOutputVectors, groupColNum, rowIdx);
                }
            } else if (fixedInt16 != nullptr) {
                if (keyRef.size > 0) {
                    auto key = static_cast<int16_t>(std::stoi(keyRef.ToString()));
                    fixedInt16->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
                } else {
                    fixedInt16->ParseNull(0, groupOutputVectors, groupColNum, rowIdx);
                }
            } else if (packedInt32 != nullptr) {
                if (UNLIKELY(key.size() != sizeof(int32_t))) {
                    throw omniruntime::exception::OmniException("SPILL_FAILED",
                        "Invalid spilled packedInt32 key size " + std::to_string(key.size()));
                }
                int32_t packedKey = 0;
                memcpy(&packedKey, key.data(), sizeof(packedKey));
                packedInt32->ParseKeyToCols(packedKey, groupOutputVectors, groupColNum, rowIdx);
            } else if (packedInt64 != nullptr) {
                if (UNLIKELY(key.size() != sizeof(int64_t))) {
                    throw omniruntime::exception::OmniException("SPILL_FAILED",
                        "Invalid spilled packedInt64 key size " + std::to_string(key.size()));
                }
                int64_t packedKey = 0;
                memcpy(&packedKey, key.data(), sizeof(packedKey));
                packedInt64->ParseKeyToCols(packedKey, groupOutputVectors, groupColNum, rowIdx);
            } else if (packedInt128 != nullptr) {
                if (UNLIKELY(key.size() != sizeof(omniruntime::type::int128_t))) {
                    throw omniruntime::exception::OmniException("SPILL_FAILED",
                        "Invalid spilled packedInt128 key size " + std::to_string(key.size()));
                }
                omniruntime::type::int128_t packedKey = 0;
                memcpy(&packedKey, key.data(), sizeof(packedKey));
                packedInt128->ParseKeyToCols(packedKey, groupOutputVectors, groupColNum, rowIdx);
            }

            currentGroupStates = groupStatesPtr + rowIdx * totalAggStatesSize;
            newGroupStates.emplace_back(currentGroupStates);
            rowStates[rowIdx] = currentGroupStates;
            rowIdx++;
        }
        auto &unspillRow = unspillRows[offset];
        unspillRow.state = currentGroupStates;
        unspillRow.batch = currentVecBatch;
        unspillRow.rowIdx = currentRowIndex;
        offset++;
        if (offset >= UNSPILL_ROW_COUNT_ONE_BATCH || isLastRow) {
            if (!newGroupStates.empty()) {
                for (int32_t aggIdx = 0; aggIdx < aggNum; aggIdx++) {
                    aggregators[aggIdx]->InitStates(newGroupStates);
                }
                newGroupStates.clear();
            }
            int32_t vectorIndex = serialize != nullptr ? 2 : 1;
            for (int32_t aggIdx = 0; aggIdx < aggNum; aggIdx++) {
                aggregators[aggIdx]->ProcessGroupUnspill(unspillRows, offset, vectorIndex);
            }
            offset = 0;
        }

        nextKeyIsNew = !isEqual;
        spillMerger->Pop();
        spillRowOffset++;
        if (nextKeyIsNew && rowIdx >= rowCount) {
            if (!newGroupStates.empty()) {
                for (int32_t aggIdx = 0; aggIdx < aggNum; aggIdx++) {
                    aggregators[aggIdx]->InitStates(newGroupStates);
                }
                newGroupStates.clear();
            }
            if (offset > 0) {
                int32_t vectorIndex = serialize != nullptr ? 2 : 1;
                for (int32_t aggIdx = 0; aggIdx < aggNum; aggIdx++) {
                    aggregators[aggIdx]->ProcessGroupUnspill(unspillRows, offset, vectorIndex);
                }
            }

            SetStateOutputVecBatch(output, rowIdx, groupColNum, aggNum);
            return output;
        }
    }
}

void HashAggregationOperator::GetOutputFromDisk(VectorBatch **outputVecBatch)
{
    if (spillMerger == nullptr) {
        if (GetElementsSize() > 0) {
            auto result = SpillHashMap();
            if (UNLIKELY(result != ErrorCode::SUCCESS)) {
                throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
            }
            executionContext->GetArena()->Reset();
            ReleaseHashmapCapacity();
        }

        spilledBytes = spiller->GetSpilledBytes();
        auto spillFiles = spiller->FinishSpill();
        stats_.spilledFiles = spillFiles.size();
        UpdateSpillFileInfo(spillFiles.size());
        spillMerger = spiller->CreateSpillMerger(spillFiles, spiller->isSpillCompressEnable(), serialize != nullptr, this->pool());
        delete spiller;
        spiller = nullptr;
        if (spillMerger == nullptr) {
            throw omniruntime::exception::OmniException("SPILL_FAILED", "Create spill merger failed.");
        }
        spillTotalRowCount = spillMerger->GetTotalRowCount();

        if (!aggregators.empty()) {
            groupStates = std::make_unique<AggregateState[]>(totalAggStatesSize * rowsPerBatch);
        }
    }

    auto rowCount = std::min(rowsPerBatch, static_cast<int32_t>(spillTotalRowCount - spillRowOffset));
    auto output = std::make_unique<VectorBatch>(rowCount);
    auto outputPtr = output.get();
    SetVectors(outputPtr, outputTypes, rowCount);
    VectorBatch *result = nullptr;
    if (aggregators.empty()) {
        result = GetOutputFromDiskWithoutAgg(outputPtr);
    } else {
        result = GetOutputFromDiskWithAgg(outputPtr);
    }

    if (result == outputPtr) {
        // it means the result is not sliced
        result = output.release();
    }
    *outputVecBatch = result;
}

void HashAggregationOperator::CalcAndSetStatesSize()
{
    totalAggStatesSize = 0;
    for (auto &agg: aggregators) {
        agg->SetStateOffset(totalAggStatesSize);
        totalAggStatesSize += agg->GetStateSize();
    }
}

template<typename Deserialize>
int32_t HashAggregationOperator::Output(Deserialize &deserializeHashmap, VectorBatch **outputVecBatch)
{
    usedMemBytes = executionContext->GetArena()->UsedBytes();
    totalMemBytes = executionContext->GetArena()->TotalBytes();

    if (hasSpill) {
        GetOutputFromDisk(outputVecBatch);
        executionContext->GetArena()->Reset();
        if (*outputVecBatch != nullptr) {
            UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
        } else {
            UpdateGetOutputInfo(0);
        }
        if (spillTotalRowCount == spillRowOffset) {
            SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
            ReleaseFinishedOutputState();
        }
        return 1;
    }

    int32_t totalRowCount = 0;
    if (vectorAnalyzer->IsArrayHashTableType()) {
        totalRowCount = arrayTable == nullptr ? 0 : this->arrayTable->GetElementsSize();
    } else {
        totalRowCount = deserializeHashmap->GetElementsSize();
    }

    if (totalRowCount == 0) {
        FinishOutput(0);
        return 0;
    }

    // The iteration output only contains one result, create only one output vector batches
    int32_t curRemainHandleOutput = totalRowCount - static_cast<int32_t>(outputState.hasBeenOutputNum);
    auto curRowCount = std::min(rowsPerBatch, curRemainHandleOutput);
    auto output = std::make_unique<VectorBatch>(curRowCount);
    auto outputPtr = output.get();
    SetVectors(outputPtr, outputTypes, curRowCount);

    if (vectorAnalyzer->IsArrayHashTableType()) {
        TraverseArrayMapToGetOneResult(outputPtr);
    } else {
        TraverseHashmapToGetOneResult(deserializeHashmap, outputPtr);
    }

    *outputVecBatch = output.release();
    UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
    if (static_cast<int32_t>(outputState.hasBeenOutputNum) == totalRowCount) {
        FinishOutput(totalRowCount);
    }
    return 1;
}

ALWAYS_INLINE size_t HashAggregationOperator::GetElementsSize()
{
    size_t elementSize = 0;
    if (vectorAnalyzer != nullptr && vectorAnalyzer->IsArrayHashTableType()) {
        elementSize = arrayTable == nullptr ? 0 : arrayTable->GetElementsSize();
    } else if (serialize != nullptr) {
        elementSize = serialize->GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        elementSize = fixedInt32->GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        elementSize = fixedInt64->GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        elementSize = fixedInt16->GetElementsSize();
    } else if (packedInt32 != nullptr) {
        elementSize = packedInt32->GetElementsSize();
    } else if (packedInt64 != nullptr) {
        elementSize = packedInt64->GetElementsSize();
    } else if (packedInt128 != nullptr) {
        elementSize = packedInt128->GetElementsSize();
    }
    return elementSize;
}

ALWAYS_INLINE void HashAggregationOperator::ReleaseHashmapCapacity()
{
    arrayTable.reset();
    if (serialize != nullptr) {
        serialize->table->Release();
        serialize->ResetRowContainer();
    } else if (fixedInt32 != nullptr) {
        fixedInt32->ReleaseHashmapCapacity();
    } else if (fixedInt64 != nullptr) {
        fixedInt64->ReleaseHashmapCapacity();
    } else if (fixedInt16 != nullptr) {
        fixedInt16->ReleaseHashmapCapacity();
    } else if (packedInt32 != nullptr) {
        packedInt32->ReleaseHashmapCapacity();
    } else if (packedInt64 != nullptr) {
        packedInt64->ReleaseHashmapCapacity();
    } else if (packedInt128 != nullptr) {
        packedInt128->ReleaseHashmapCapacity();
    }
    hashmapCapacityReleased_ = true;
}

void HashAggregationOperator::ReleaseFinishedOutputState()
{
    const int64_t usedBefore = pool_ == nullptr ? 0 : pool_->currentBytes();
    executionContext->GetArena()->Reset();
    ReleaseHashmapCapacity();
    groupStates.reset();
    std::vector<AggregateState *>().swap(rowStates);
    aggregationSort.reset();
    delete spillMerger;
    spillMerger = nullptr;
    if (pool_ != nullptr) {
        pool_->unregisterSpillable(this);
    }

    if (pool_ != nullptr && omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        const int64_t usedAfter = pool_->currentBytes();
        std::fprintf(stderr,
            "[OMNI-HASHAGG] event=finished_state_release operator=%p has_spill=%d used_before=%ld used_after=%ld freed=%ld reserved=%ld\n",
            static_cast<void *>(this), static_cast<int>(hasSpill),
            static_cast<long>(usedBefore), static_cast<long>(usedAfter),
            static_cast<long>(std::max<int64_t>(0, usedBefore - usedAfter)),
            static_cast<long>(pool_->reservedBytes()));
    }
}
} // end of namespace op
} // end of namespace omniruntime
