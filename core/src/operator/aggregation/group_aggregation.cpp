/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Hash Aggregation Source File
 */
#include "group_aggregation.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "vector/vector_helper.h"
#include "operator/status.h"
#include "operator/util/operator_util.h"
#include "util/type_util.h"
#include "util/debug.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/state_flag_operation.h"
#include "type/data_type.h"
#include "vector/unsafe_vector.h"
#include "vector/mixed_vector.h"
#include "util/null_bits.h"

#if defined(DEBUG_OPERATOR) && defined(TRACE)
#include <sstream>
#endif

#if defined(SVEHTMISSES) && !defined(OMNI_SVEHT32_HASH_AGG)
#error "SVEHTMISSES requires OMNI_SVEHT32_HASH_AGG"
#endif

namespace omniruntime {
namespace op {
using namespace omniruntime::type;

static constexpr int32_t UNSPILL_ROW_COUNT_ONE_BATCH = 128;

#ifdef SVEHTMISSES
static constexpr int32_t SVE_MISS_ESTIMATE_THRESHOLD = 32;
static constexpr int32_t SVE_MISS_ESTIMATE_SAMPLE_SIZE = 128;
static constexpr uint32_t SVE_MISS_ESTIMATE_SET_SIZE = 256;
static constexpr uint32_t SVE_MISS_ESTIMATE_SET_MASK = SVE_MISS_ESTIMATE_SET_SIZE - 1;

static ALWAYS_INLINE uint32_t HashEstimateKey(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x85ebca6bu;
    value ^= value >> 13;
    value *= 0xc2b2ae35u;
    value ^= value >> 16;
    return value;
}

static ALWAYS_INLINE uint32_t EstimateDistinctFromSample(uint32_t sampleDistinct, int32_t sampleCount, int32_t totalCount)
{
    if (totalCount <= 0) {
        return 0;
    }
    if (sampleCount <= 0 || sampleDistinct == 0) {
        return 1;
    }
    uint64_t estimated = (static_cast<uint64_t>(sampleDistinct) * static_cast<uint64_t>(totalCount) +
                         static_cast<uint64_t>(sampleCount - 1)) /
                         static_cast<uint64_t>(sampleCount);
    estimated = std::max<uint64_t>(1, estimated);
    estimated = std::min<uint64_t>(estimated, static_cast<uint64_t>(totalCount));
    return static_cast<uint32_t>(estimated);
}

static uint32_t EstimateDistinctMisses32(const uint32_t *keys, int32_t missCount)
{
    const int32_t sampleCount = std::min(missCount, SVE_MISS_ESTIMATE_SAMPLE_SIZE);
    bool used[SVE_MISS_ESTIMATE_SET_SIZE] = {};
    uint32_t storedKeys[SVE_MISS_ESTIMATE_SET_SIZE] = {};
    uint32_t sampleDistinct = 0;

    for (int32_t i = 0; i < sampleCount; ++i) {
        uint32_t slot = HashEstimateKey(keys[i]) & SVE_MISS_ESTIMATE_SET_MASK;
        bool found = false;
        while (used[slot]) {
            if (storedKeys[slot] == keys[i]) {
                found = true;
                break;
            }
            slot = (slot + 1) & SVE_MISS_ESTIMATE_SET_MASK;
        }
        if (found) {
            continue;
        }
        used[slot] = true;
        storedKeys[slot] = keys[i];
        ++sampleDistinct;
    }

    return EstimateDistinctFromSample(sampleDistinct, sampleCount, missCount);
}

static ALWAYS_INLINE bool EstimatePairKeyEquals(
    const hashmap::SveAggAosHashTable32Pair::Key &left,
    const hashmap::SveAggAosHashTable32Pair::Key &right)
{
    return left.key0 == right.key0 && left.key1 == right.key1 && left.nullMask == right.nullMask;
}

static ALWAYS_INLINE uint32_t HashEstimatePairKey(const hashmap::SveAggAosHashTable32Pair::Key &key)
{
    uint32_t hash = HashEstimateKey(key.key0);
    hash ^= (HashEstimateKey(key.key1) << 16) | (HashEstimateKey(key.key1) >> 16);
    hash ^= HashEstimateKey(key.nullMask);
    return HashEstimateKey(hash);
}

static uint32_t EstimateDistinctMisses32Pair(
    const hashmap::SveAggAosHashTable32Pair::Key *keys, int32_t missCount)
{
    const int32_t sampleCount = std::min(missCount, SVE_MISS_ESTIMATE_SAMPLE_SIZE);
    bool used[SVE_MISS_ESTIMATE_SET_SIZE] = {};
    hashmap::SveAggAosHashTable32Pair::Key storedKeys[SVE_MISS_ESTIMATE_SET_SIZE] = {};
    uint32_t sampleDistinct = 0;

    for (int32_t i = 0; i < sampleCount; ++i) {
        uint32_t slot = HashEstimatePairKey(keys[i]) & SVE_MISS_ESTIMATE_SET_MASK;
        bool found = false;
        while (used[slot]) {
            if (EstimatePairKeyEquals(storedKeys[slot], keys[i])) {
                found = true;
                break;
            }
            slot = (slot + 1) & SVE_MISS_ESTIMATE_SET_MASK;
        }
        if (found) {
            continue;
        }
        used[slot] = true;
        storedKeys[slot] = keys[i];
        ++sampleDistinct;
    }

    return EstimateDistinctFromSample(sampleDistinct, sampleCount, missCount);
}

template<typename Table, typename EstimateFn>
static bool ShouldUseKnownMissInsert(Table *table, int32_t missCount, EstimateFn estimateDistinctMisses)
{
    if (missCount <= 0) {
        return false;
    }
    if (table->CanInsertAdditional(static_cast<uint32_t>(missCount))) {
        return true;
    }
    if (missCount < SVE_MISS_ESTIMATE_THRESHOLD) {
        return false;
    }
    return table->CanInsertAdditional(estimateDistinctMisses());
}
#endif

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

using SetVector = void (*)(VectorBatch *vecBatch, int32_t rowCount);
template <typename V> void SetVectorImpl(VectorBatch *vecBatch, int32_t rowCount)
{
    vecBatch->Append(new V(rowCount));
}

void SetVarcharVector(VectorBatch *vecBatch, int32_t rowCount)
{
    vecBatch->Append(new Vector<LargeStringContainer<std::string_view>>(rowCount));
}

void SetContainerVector(VectorBatch *vecBatch, int32_t rowCount)
{
    auto doubleVector = new Vector<double>(rowCount);
    auto longVector = new Vector<int64_t>(rowCount);
    std::vector<int64_t> vectorAddresses(AVG_VECTOR_COUNT);
    vectorAddresses[0] = reinterpret_cast<int64_t>(doubleVector);
    vectorAddresses[1] = reinterpret_cast<int64_t>(longVector);
    std::vector<DataTypePtr> dataTypes{ DoubleType(), LongType() };
    auto containerVector = new ContainerVector(rowCount, vectorAddresses, dataTypes);
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
    auto ret = CreateAggregatorFactories(aggregatorFactories, aggFuncTypesVector, GetMaskColumns());

    return ret;
}

OmniStatus HashAggregationOperatorFactory::Close()
{
    return OMNI_STATUS_NORMAL;
}

Operator *HashAggregationOperatorFactory::CreateOperator()
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
        aggs.push_back(std::move(aggregator));
    }

    auto groupByOperator = new HashAggregationOperator(groupByIndex, aggsInputCols, aggInputColsSize, aggInputTypes,
        aggOutputTypes, std::move(aggs), inputRaws, outputPartials, hasAggFilters, operatorConfig, aggFuncTypesVector,
        step, mixedInputExpected, mixedOutputEnabled);
    groupByOperator->SetGroupByColumnsHandleType(handleType);
    groupByOperator->Init();
    return groupByOperator;
}

void HashAggregationOperatorFactory::ChooseGroupByType()
{
    // Currently, the serialization and singleFix method is used for column types that need to be grouped by.
    // The serialization method can be continuously evolved based on different types.
    // The singleFix method is used for OMNI_INT/OMNI_LONG which is only one column.
    const auto groupBySize = groupByTypes.GetSize();
#ifdef OMNI_SVEHT32_HASH_AGG
    if (!operatorConfig.GetSpillConfig()->IsSpillEnabled() && groupBySize >= 1 && groupBySize <= 2) {
        bool all32Bit = true;
        for (uint32_t i = 0; i < groupBySize; ++i) {
            const auto &type = groupByTypes.GetIds()[i];
            all32Bit = all32Bit && (type == OMNI_INT || type == OMNI_DATE32);
        }
        if (all32Bit) {
            if (groupBySize == 1) {
                handleType = HandleType::fixedInt32SveAos;
            } else {
                handleType = HandleType::fixedInt32PairSveAos;
            }
            return;
        }
    }
#endif
    if (groupBySize == 1) {
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
    if (groupBySize > 1) {
        int32_t valueBits = 0;
        for (int32_t i = 0; i < groupBySize; ++i) {
            auto typeId = groupByTypes.GetIds()[i];
            auto bits = PackedBitWidthForType(typeId);
            if (bits == 0) {
                handleType = HandleType::serialize;
                return;
            }
            valueBits += bits;
        }
        int32_t totalBits = valueBits + static_cast<int32_t>(groupBySize); // + null mask bits
        if (totalBits > 0 && totalBits <= 32) {
            handleType = HandleType::packedInt32;
            return;
        } else if (totalBits <= 64) {
            handleType = HandleType::packedInt64;
            return;
        }
    }
    if (normalizedKeyEnabled && groupBySize > 1) {
        std::vector<type::DataTypeId> normalizedKeyTypes;
        normalizedKeyTypes.reserve(groupBySize);
        const auto *typeIds = groupByTypes.GetIds();
        for (int32_t i = 0; i < groupBySize; ++i) {
            normalizedKeyTypes.push_back(static_cast<type::DataTypeId>(typeIds[i]));
        }
        using Handler = NormalizeKeyHandler<
            TaperFixedKeyMap<omniruntime::type::int128_t, AggregateState *>>;
        if (Handler::CanUse(normalizedKeyTypes)) {
            handleType = HandleType::NormalizeKey;
            LogDebug("Use normalize key hash mode for %d group-by columns.", groupBySize);
            return;
        }
    }
    handleType = HandleType::serialize;
}

void HashAggregationOperator::SetGroupByColumnsHandleType(HandleType t)
{
    groupByColumnsHandleType = t;
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
    mixedStateSerdeSupported = CanUseMixedStateSerde();

    auto initSerializeHandler = [&]() {
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
        bool hasComplexTypes = false;
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
                if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_ROW) {
                    hasComplexTypes = true;
                }
            }
        }
        // Merge multiple VARCHAR columns into a single slot to reduce fixed row size.
        // 有复杂类型时禁止合并：行段复杂列以长度前缀内联（非合并布局），
        // StoreKeyFromRow 的 fallback 逐列循环按每列独立 char* slot 写入，合并会破坏列 offset。
        // 关键：跳过合并时必须清空 varcharColIndices，保证 hasMergedVarchar (size>1) 全局为 false。
        if (varcharColIndices.size() > 1 && !hasComplexTypes) {
            int32_t firstVarcharIdx = varcharColIndices[0];
            keySizes[firstVarcharIdx] = sizeof(char*);
            for (size_t i = 1; i < varcharColIndices.size(); ++i) {
                keySizes[varcharColIndices[i]] = 0;
            }
        } else if (varcharColIndices.size() > 1) {
            // hasComplexTypes：每列 VARCHAR 独立 slot（不合并）。
            varcharColIndices.clear();
        }
        serialize->InitRowContainer(keySizes, isVariableLen, typeIds, varcharColIndices, *executionContext->GetArena());
    };

    // 3. check group by handle methcd
    // put at beginning so that we do not allocate memory if there is error
    if (groupByColumnsHandleType == HandleType::serialize) {
        initSerializeHandler();
    } else if (groupByColumnsHandleType == HandleType::NormalizeKey) {
        std::vector<type::DataTypeId> typeIds;
        std::vector<int32_t> keySizes;
        typeIds.reserve(groupByCols.size());
        keySizes.reserve(groupByCols.size());
        for (const auto &col : groupByCols) {
            typeIds.push_back(col.input->GetId());
            keySizes.push_back(OperatorUtil::GetTypeSize(col.input));
        }
        if (aggregators.empty()) {
            normalizeKeyWithoutAgg = std::make_unique<decltype(normalizeKeyWithoutAgg)::element_type>();
            if (!normalizeKeyWithoutAgg->Init(typeIds, keySizes, 0, *executionContext->GetArena())) {
                throw omniruntime::exception::OmniException(
                    "UNSUPPORTED_ERROR", "Failed to initialize normalize key handler without aggregate");
            }
        } else {
            normalizeKey = std::make_unique<decltype(normalizeKey)::element_type>();
            if (!normalizeKey->Init(typeIds, keySizes, totalAggStatesSize, *executionContext->GetArena())) {
                throw omniruntime::exception::OmniException(
                    "UNSUPPORTED_ERROR", "Failed to initialize normalize key handler");
            }
        }
    } else if (groupByColumnsHandleType == HandleType::fixedInt32) {
        fixedInt32 = std::make_unique<TaperGroupbySingleFixHandler<int32_t>>(*executionContext->GetArena(), totalAggStatesSize);
#ifdef OMNI_SVEHT32_HASH_AGG
    } else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos) {
        fixedInt32SveAos = std::make_unique<hashmap::SveAggAosHashTable32>();
        fixedInt32 = std::make_unique<TaperGroupbySingleFixHandler<int32_t>>(*executionContext->GetArena(),
            totalAggStatesSize);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32PairSveAos) {
        fixedInt32PairSveAos = std::make_unique<hashmap::SveAggAosHashTable32Pair>();
        initSerializeHandler();
#endif
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

    // 8 pre-compute canOutputMixed (static conditions only; hasSpill is checked at runtime)
    canOutputMixed_ = mixedOutputEnabled && mixedStateSerdeSupported && aggFiltersCount == 0 &&
        groupByColumnsHandleType == HandleType::serialize;

    return OMNI_STATUS_NORMAL;
}

void HashAggregationOperator::ResizeArrayMap(int64_t oldMin)
{
    auto offSet = oldMin - vectorAnalyzer->MinValue();
    auto newTableSize = vectorAnalyzer->GetRange();
    auto newArrayTable = std::make_unique<DefaultArrayMap<AggregateState>>(newTableSize);
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
#ifdef OMNI_SVEHT32_HASH_AGG
    if (groupByColumnsHandleType == HandleType::fixedInt32SveAos) {
        bool hasReservedKey = false;
        arrayTable->ForEachValue([&](const auto &value, const auto &index) {
            if (index != 0) {
                const auto key = static_cast<uint32_t>(static_cast<int32_t>(index + minValue - 1));
                if (key == hashmap::SveAggAosHashTable32::kEmptyKey) {
                    hasReservedKey = true;
                }
            }
        });
        if (hasReservedKey) {
            FallbackSveAggToFixedInt32();
        }
    }
#endif
    arrayTable->ForEachValue([&](const auto &value, const auto &index) {
        if (index != 0) {
            if (groupByColumnsHandleType == HandleType::fixedInt32
#ifdef OMNI_SVEHT32_HASH_AGG
                || (groupByColumnsHandleType == HandleType::fixedInt32SveAos && sveAosFallbackToFixedInt32)
#endif
            ) {
                fixedInt32->InsertOneValueToHashmap<false>(static_cast<int32_t>(index + minValue - 1),
                            reinterpret_cast<AggregateState *>(value));
#ifdef OMNI_SVEHT32_HASH_AGG
            } else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos) {
                const auto key = static_cast<uint32_t>(static_cast<int32_t>(index + minValue - 1));
                auto ret = fixedInt32SveAos->EmplaceScalar(key);
                if (ret.IsInsert() && hasAgg) {
                    ret.SetValue(RegisterSveAggState(reinterpret_cast<AggregateState *>(value)));
                }
#endif
            } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
                fixedInt64->InsertOneValueToHashmap<false>(static_cast<int64_t>(index + minValue - 1),
                            reinterpret_cast<AggregateState *>(value));
            } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
                fixedInt16->InsertOneValueToHashmap<false>(static_cast<int16_t>(index + minValue - 1),
                            reinterpret_cast<AggregateState *>(value));
            }
            return;
        }
        if (groupByColumnsHandleType == HandleType::fixedInt32
#ifdef OMNI_SVEHT32_HASH_AGG
            || (groupByColumnsHandleType == HandleType::fixedInt32SveAos && sveAosFallbackToFixedInt32)
#endif
        ) {
            fixedInt32->InsertOneValueToHashmap<true>(0, reinterpret_cast<AggregateState *>(value));
#ifdef OMNI_SVEHT32_HASH_AGG
        } else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos) {
            hasNullGroupState32 = true;
            if (hasAgg) {
                nullGroupState32 = reinterpret_cast<AggregateState *>(value);
            }
#endif
        } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
            fixedInt64->InsertOneValueToHashmap<true>(0, reinterpret_cast<AggregateState *>(value));
        } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
            fixedInt16->InsertOneValueToHashmap<true>(0, reinterpret_cast<AggregateState *>(value));
        }
    });
    arrayTable.reset();
}

void HashAggregationOperator::PrepareSerializeMarshallers(BaseVector **groupVectors, int32_t groupColNum)
{
    if (serialize == nullptr) {
        serialize = std::make_unique<TaperColumnSerializeHandler>(
            *executionContext->GetArena(), totalAggStatesSize);
        serialize->InitSize(groupByCols.size());
        std::vector<int32_t> keySizes(groupByCols.size());
        std::vector<bool> isVariableLen(groupByCols.size(), false);
        std::vector<int32_t> typeIds(groupByCols.size());
        std::vector<int32_t> varcharColIndices;
        for (size_t i = 0; i < groupByCols.size(); ++i) {
            keySizes[i] = OperatorUtil::GetTypeSize(groupByCols[i].input);
            typeIds[i] = groupByCols[i].input->GetId();
        }
        serialize->InitRowContainer(
            keySizes, isVariableLen, typeIds, varcharColIndices, *executionContext->GetArena());
    }

    serialize->ResetSerializer();
    for (int32_t col = 0; col < groupColNum; ++col) {
        auto *vector = groupVectors[col];
        const auto typeId = groupByCols[col].input->GetId();
        if (vector->GetEncoding() == Encoding::OMNI_DICTIONARY) {
            serialize->PushBackSerializer(dicVectorSerializerCenter[typeId]);
            serialize->PushBackComparator(dicVectorComparatorCenter[typeId]);
        } else if (vector->GetEncoding() == Encoding::OMNI_ENCODING_CONST) {
            serialize->PushBackSerializer(constVectorSerializerCenter[typeId]);
            serialize->PushBackComparator(constVectorComparatorCenter[typeId]);
        } else {
            serialize->PushBackSerializer(vectorSerializerCenter[typeId]);
            serialize->PushBackComparator(vectorComparatorCenter[typeId]);
        }
        serialize->PushBackDeSerializer(vectorDeSerializerCenter[typeId]);
    }
}

void HashAggregationOperator::FallbackNormalizeKeyToSerialize(
    BaseVector **groupVectors, int32_t groupColNum)
{
    (void)groupVectors;
    if (normalizeKey == nullptr && normalizeKeyWithoutAgg == nullptr) {
        return;
    }

    std::vector<std::unique_ptr<BaseVector>> temporaryVectors;
    std::vector<BaseVector *> temporaryVectorPtrs(groupColNum);
    temporaryVectors.reserve(groupColNum);
    for (int32_t col = 0; col < groupColNum; ++col) {
        switch (groupByCols[col].input->GetId()) {
            case type::OMNI_BYTE:
                temporaryVectors.push_back(std::make_unique<Vector<int8_t>>(1));
                break;
            case type::OMNI_SHORT:
                temporaryVectors.push_back(std::make_unique<Vector<int16_t>>(1));
                break;
            case type::OMNI_INT:
            case type::OMNI_DATE32:
            case type::OMNI_TIME32:
                temporaryVectors.push_back(std::make_unique<Vector<int32_t>>(1));
                break;
            case type::OMNI_LONG:
            case type::OMNI_DATE64:
            case type::OMNI_TIME64:
            case type::OMNI_TIMESTAMP:
            case type::OMNI_DECIMAL64:
                temporaryVectors.push_back(std::make_unique<Vector<int64_t>>(1));
                break;
            default:
                throw omniruntime::exception::OmniException(
                    "UNSUPPORTED_ERROR", "Normalize key fallback has an unsupported key type");
        }
        temporaryVectorPtrs[col] = temporaryVectors.back().get();
    }

    PrepareSerializeMarshallers(temporaryVectorPtrs.data(), groupColNum);
    auto emplaceSerializedKey = [&]() {
        std::vector<uint8_t *> groups(1, nullptr);
        std::vector<uint8_t *> newGroups;
        serialize->DecodeGroupByColumns(temporaryVectorPtrs.data(), groupColNum, 1);
        serialize->EmplaceTable(temporaryVectorPtrs.data(), groupColNum, 1, groups, newGroups,
            temporaryVectorPtrs[0]->GetEncoding());
        return groups[0] + serialize->AggStateOffset();
    };

    if (normalizeKeyWithoutAgg != nullptr) {
        normalizeKeyWithoutAgg->ForEachKey([&](const auto &key) {
            normalizeKeyWithoutAgg->ParseKeyToCols(key, temporaryVectorPtrs, groupColNum, 0);
            emplaceSerializedKey();
        });
    } else {
        normalizeKey->ForEachRow([&](auto mapped) {
            normalizeKey->ParseRowToCols(mapped, temporaryVectorPtrs, groupColNum, 0);
            auto *state = emplaceSerializedKey();
            if (totalAggStatesSize > 0) {
                std::memcpy(state, mapped, totalAggStatesSize);
            }
        });
    }

    normalizeKey.reset();
    normalizeKeyWithoutAgg.reset();
    groupByColumnsHandleType = HandleType::serialize;
    vectorAnalyzer->SetNormalHashTable();
    LogDebug("Fallback normalize key hash table to serialize hash table.");
}

bool HashAggregationOperator::TryEmplaceNormalizeKey(
    VectorBatch *vecBatch, BaseVector **groupVectors, int32_t groupColNum)
{
    const int32_t rowCount = vecBatch->GetRowCount();
    if (normalizeKeyWithoutAgg != nullptr) {
        if (!normalizeKeyWithoutAgg->TryEncodeBatch(groupVectors, groupColNum, rowCount)) {
            return false;
        }
        normalizeKeyWithoutAgg->EmplaceKeys(rowCount);
        return true;
    }

    if (normalizeKey == nullptr) {
        return false;
    }
    if (!normalizeKey->TryEncodeBatch(groupVectors, groupColNum, rowCount)) {
        return false;
    }

    rowsAggStates.resize(static_cast<size_t>(rowCount));
    std::vector<AggregateState *> newStates;
    newStates.reserve(static_cast<size_t>(rowCount));
    normalizeKey->EmplaceStates(groupVectors, rowCount, rowsAggStates, newStates);
    if (aggFiltersCount > 0) {
        int32_t filterOffset = vecBatch->GetVectorCount() - aggFiltersCount;
        for (size_t aggIdx = 0; aggIdx < aggregators.size(); ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            if (!newStates.empty()) {
                aggregator->InitStates(newStates);
            }
            if (aggIdx < hasAggFilters.size() && hasAggFilters[aggIdx] == 1) {
                aggregator->ProcessGroupFilter(rowsAggStates, aggIdx, vecBatch, filterOffset, 0);
                ++filterOffset;
            } else {
                aggregator->ProcessGroup(rowsAggStates, vecBatch, 0);
            }
        }
    } else {
        for (auto &aggregator : aggregators) {
            if (!newStates.empty()) {
                aggregator->InitStates(newStates);
            }
            aggregator->ProcessGroup(rowsAggStates, vecBatch, 0);
        }
    }
    return true;
}

void HashAggregationOperator::PrepareSerializeHandlers(BaseVector **groupVectors, int32_t groupColNum)
{
    serialize->ResetSerializer();
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto curVector = groupVectors[i];
        auto omniId = groupByCols[i].input->GetId();
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
}

int32_t HashAggregationOperator::AddInput(VectorBatch *vecBatch)
{
    setInputedData(true);
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }

    UpdateAddInputInfo(rowCount);
    if (vecBatch->MixType() == 1) {
        if (!mixedStateSerdeSupported || aggFiltersCount != 0 || serialize == nullptr) {
            VectorHelper::FreeVecBatch(vecBatch);
            ResetInputVecBatch();
            throw OmniException("UNSUPPORTED_ERROR", "HashAgg cannot consume this mixed row batch");
        }
        return AddMixedInput(static_cast<MixedVectorBatch *>(vecBatch));
    }
    // do decide hash table mode
    auto oldMin = vectorAnalyzer->MinValue();
    auto preIsArrayMap = vectorAnalyzer->IsArrayHashTableType();
    vectorAnalyzer->DecideHashMode(vecBatch);
    if (vectorAnalyzer->IsArrayHashTableType()) {
        if (arrayTable == nullptr) {
            arrayTable = std::make_unique<DefaultArrayMap<AggregateState>>(vectorAnalyzer->GetRange());
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
            return 0;
        }
    }

    if (UNLIKELY(preIsArrayMap && arrayTable != nullptr)) {
        MoveEntryArrayTableToHashMap(oldMin);
    }

    auto groupColNum = static_cast<int32_t>(this->groupByCols.size());
    const bool isNormalizeKeyMode =
        groupByColumnsHandleType == HandleType::NormalizeKey &&
        (normalizeKey != nullptr || normalizeKeyWithoutAgg != nullptr);
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

    if (isNormalizeKeyMode) {
        if (!TryEmplaceNormalizeKey(vecBatch, groupVectors, groupColNum)) {
            FallbackNormalizeKeyToSerialize(groupVectors, groupColNum);
            PrepareSerializeMarshallers(groupVectors, groupColNum);
            serialize->DecodeGroupByColumns(groupVectors, groupColNum, rowCount);
            Emplace(serialize, vecBatch, groupVectors, groupColNum);
        }
    }
#ifdef OMNI_SVEHT32_HASH_AGG
    else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos) {
        if (sveAosFallbackToFixedInt32) {
            Emplace(fixedInt32, vecBatch, groupVectors, groupColNum);
        } else {
            EmplaceFixedInt32SveAos(vecBatch, groupVectors[0]);
        }
    } else if (groupByColumnsHandleType == HandleType::fixedInt32PairSveAos) {
        if (svePairFallbackToSerialize) {
            serialize->DecodeGroupByColumns(groupVectors, groupColNum, rowCount);
            Emplace(serialize, vecBatch, groupVectors, groupColNum);
        } else {
            EmplaceFixedInt32PairSveAos(vecBatch, groupVectors[0], groupVectors[1]);
        }
    }
#endif
    else if (LIKELY(groupByColumnsHandleType == HandleType::serialize)) {
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
    if (operatorConfig.GetSpillConfig()->NeedSpill(GetElementsSize())) {
        auto result = SpillHashMap();
        executionContext->GetArena()->Reset();
        ResetHashmap();
        if (UNLIKELY(result != ErrorCode::SUCCESS)) {
            throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
        }
    }
    return 0;
}

bool HashAggregationOperator::CanUseMixedStateSerde() const
{
    if (groupByCols.empty()) {
        return false;
    }
    if (aggregators.empty()) {
        return true;
    }
    if (aggFiltersCount != 0) {
        return false;
    }
    for (const auto &aggregator : aggregators) {
        if (!aggregator->SupportsMixedStateSerde()) {
            return false;
        }
    }
    return true;
}

// for final agg consume mixed VectorBatch
int32_t HashAggregationOperator::AddMixedInput(MixedVectorBatch *mixedBatch)
{
    auto rowCount = mixedBatch->GetRowCount();
    auto groupColNum = static_cast<int32_t>(groupByCols.size());

    // EmplaceTableFromRow: compute hashes directly from row segments
    // without deserializing to intermediate column vectors.
    // Must initialize serializers/comparators for spill path (Bug-001:
    // when only mixed+spill runs, serializers.size()==0 causes crash).
    // Mixed path has no groupVectors, so use non-dictionary serializers
    // built from groupByCols type IDs (same as AddInput's non-dic branch).
    if (serialize != nullptr) {
        serialize->ResetSerializer();
        for (int32_t i = 0; i < groupColNum; ++i) {
            auto omniId = groupByCols[i].input->GetId();
            if (omniId == type::OMNI_ARRAY || omniId == type::OMNI_ROW) {
                serialize->PushBackSerializer(complexVectorSerializerCenter[omniId]);
                serialize->PushBackComparator(vectorComparatorCenter[omniId]);
                serialize->PushBackDeSerializer(complexVectorDeSerializerCenter[omniId]);
            } else {
                serialize->PushBackSerializer(vectorSerializerCenter[omniId]);
                serialize->PushBackComparator(vectorComparatorCenter[omniId]);
                serialize->PushBackDeSerializer(vectorDeSerializerCenter[omniId]);
            }
        }
    }
    vectorAnalyzer->SetNormalHashTable();
    currentRowStates.resize(rowCount);
    newGroupStates.clear();
    newGroupStates.reserve(rowCount);
    serialize->EmplaceTableFromRow(mixedBatch, rowCount, currentRowStates, newGroupStates);

    // EmplaceTable returns RowContainer row-start pointers. Convert them to
    // AggState pointers before passing them to aggregator init/merge routines.
    auto aggStateOffset = serialize->AggStateOffset();
    for (auto &rowState : currentRowStates) {
        rowState += aggStateOffset;
    }
    for (auto &rowState : newGroupStates) {
        rowState += aggStateOffset;
    }
    for (auto &aggregator : aggregators) {
        if (!newGroupStates.empty()) {
            aggregator->InitStates(newGroupStates);
        }
    }
    // 缓存 ops + sizes（首次计算，跨批次复用）
    if (!mergeOpsCached_) {
        cachedMergeOps_.resize(aggregators.size());
        cachedMergeStateSizes_.resize(aggregators.size());
        for (size_t i = 0; i < aggregators.size(); ++i) {
            cachedMergeOps_[i] = aggregators[i]->GetMixedStateSerdeOps();
            cachedMergeStateSizes_[i] = static_cast<int32_t>(aggregators[i]->GetStateSize());
        }
        mergeOpsCached_ = true;
    }
    // batchMerge: O(aggs) 次间接调用替代 O(rows×aggs) 次，MergeFn 编译期内联
    int32_t mergeStateOffset = 0;
    for (size_t i = 0; i < aggregators.size(); ++i) {
        cachedMergeOps_[i]->batchMerge(
            aggregators[i].get(), currentRowStates.data(),
            mixedBatch, mergeStateOffset, rowCount);
        mergeStateOffset += cachedMergeStateSizes_[i];
    }
    newGroupStates.clear();

    VectorHelper::FreeVecBatch(mixedBatch);
    ResetInputVecBatch();
    if (operatorConfig.GetSpillConfig()->NeedSpill(GetElementsSize())) {
        auto result = SpillHashMap();
        executionContext->GetArena()->Reset();
        ResetHashmap();
        if (UNLIKELY(result != ErrorCode::SUCCESS)) {
            throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
        }
    }
    return 0;
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

void SetArrayVector(VectorBatch *vecBatch, DataTypePtr elementType, int32_t rowCount)
{
    std::shared_ptr<BaseVector> elementVector;
    auto elemTypeId = elementType->GetId();
    switch (elemTypeId) {
        case type::OMNI_BYTE:
            elementVector = std::make_shared<Vector<int8_t>>(0);
            break;
        case type::OMNI_INT:
        case type::OMNI_DATE32:
        case type::OMNI_TIME32:
            elementVector = std::make_shared<Vector<int32_t>>(0);
            break;
        case type::OMNI_LONG:
        case type::OMNI_DATE64:
        case type::OMNI_TIME64:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DECIMAL64:
            elementVector = std::make_shared<Vector<int64_t>>(0);
            break;
        case type::OMNI_SHORT:
            elementVector = std::make_shared<Vector<int16_t>>(0);
            break;
        case type::OMNI_DOUBLE:
            elementVector = std::make_shared<Vector<double>>(0);
            break;
        case type::OMNI_FLOAT:
            elementVector = std::make_shared<Vector<float>>(0);
            break;
        case type::OMNI_BOOLEAN:
            elementVector = std::make_shared<Vector<bool>>(0);
            break;
        case type::OMNI_DECIMAL128:
            elementVector = std::make_shared<Vector<Decimal128>>(0);
            break;
        case type::OMNI_CHAR:
        case type::OMNI_VARCHAR:
        case type::OMNI_VARBINARY:
            elementVector = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(0);
            break;
        case type::OMNI_ARRAY:
        case type::OMNI_MAP:
        case type::OMNI_ROW:
            elementVector = std::shared_ptr<BaseVector>(VectorHelper::CreateComplexVector(elementType.get(), 0));
            break;
        default:
            throw omniruntime::exception::OmniException("Set ArrayVector error, unsupport element type:", std::to_string(elemTypeId));
    }

    auto arrayVector = new ArrayVector(rowCount, elementVector);
    vecBatch->Append(arrayVector);
}

void SetMapVector(VectorBatch *vecBatch, DataTypePtr keyType, DataTypePtr valueType, int32_t rowCount)
{
    std::shared_ptr<BaseVector> keyVector(VectorHelper::CreateComplexVector(keyType.get(), 0));
    std::shared_ptr<BaseVector> valueVector(VectorHelper::CreateComplexVector(valueType.get(), 0));
    auto *mapVector = new MapVector(rowCount, keyVector, valueVector);
    vecBatch->Append(mapVector);
}

void SetRowVector(VectorBatch *vecBatch, const std::vector<DataTypePtr> &fieldTypes, int32_t rowCount)
{
    std::vector<std::shared_ptr<BaseVector>> children;
    for (const auto &fieldType : fieldTypes) {
        children.push_back(std::shared_ptr<BaseVector>(VectorHelper::CreateComplexVector(fieldType.get(), rowCount)));
    }
    auto *rowVector = new RowVector(rowCount, children);
    vecBatch->Append(rowVector);
}

void HashAggregationOperator::SetVectors(VectorBatch *output, const std::vector<DataTypePtr> &types, int32_t rowCount)
{
    auto colSize = types.size();
    for (size_t colIndex = 0; colIndex < colSize; ++colIndex) {
        const DataTypePtr &type = types[colIndex];
        auto typeId = type->GetId();
        if (typeId < DATA_TYPE_MAX_COUNT) {
            GROUP_AGG_FUNCTIONS[type->GetId()](output, rowCount);
        } else if (typeId == OMNI_ARRAY) {
            auto arrayType = std::static_pointer_cast<ArrayType>(type);
            DataTypePtr elementType = arrayType->ElementType();
            SetArrayVector(output, elementType, rowCount);
        } else if (typeId == OMNI_MAP) {
            auto mapType = std::static_pointer_cast<MapType>(type);
            SetMapVector(output, mapType->Key(), mapType->Value(), rowCount);
        } else if (typeId == OMNI_ROW) {
            auto rowType = std::static_pointer_cast<RowType>(type);
            std::vector<DataTypePtr> fieldTypes;
            for (size_t i = 0; i < rowType->Size(); i++) {
                fieldTypes.push_back(rowType->Type(i));
            }
            SetRowVector(output, fieldTypes, rowCount);
        }
    }
}

int32_t HashAggregationOperator::OutputMixed(VectorBatch **outputVecBatch)
{
    usedMemBytes = executionContext->GetArena()->UsedBytes();
    totalMemBytes = executionContext->GetArena()->TotalBytes();
    auto totalRowCount = static_cast<int32_t>(serialize->GetElementsSize());
    if (totalRowCount == 0) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
        return 0;
    }

    auto remaining = totalRowCount - static_cast<int32_t>(outputState.hasBeenOutputNum);
    auto rowCount = std::min(rowsPerBatch, remaining);
    std::vector<DataTypeId> keyTypeIds;
    keyTypeIds.reserve(groupByCols.size());
    for (const auto &groupByCol : groupByCols) {
        keyTypeIds.push_back(groupByCol.input->GetId());
    }
    auto output = std::make_unique<MixedVectorBatch>(rowCount, keyTypeIds);
    output->SetMode(COMPLETE_ROW_ONLY);
    output->PrepareRowArena(static_cast<int64_t>(rowCount) * (totalAggStatesSize + 64));

    auto aggStateOffset = serialize->AggStateOffset();

    // === 缓存列元数据（typeId/offset/nullBits/fixedKeySizes），消除每行重复查找 ===
    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    int32_t numNullBytes = util::NullBits::NumBytes(groupColNum);
    bool hasMergedVarchar = (serialize->varcharColIndices.size() > 1);
    bool hasComplexTypes = false;
    std::vector<type::DataTypeId> colTypeIds(groupColNum);
    std::vector<int32_t> colOffsets(groupColNum);
    std::vector<int32_t> colNullBytes(groupColNum);
    std::vector<uint8_t> colNullMasks(groupColNum);
    std::vector<bool> isMergedVarcharCol(groupColNum, false);
    std::vector<int32_t> fixedKeySizes(groupColNum, 0);
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto typeId = groupByCols[i].input->GetId();
        colTypeIds[i] = typeId;
        if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_ROW) {
            hasComplexTypes = true;
        }
        auto col = serialize->aggRows->ColumnAt(i);
        colOffsets[i] = col.Offset();
        colNullBytes[i] = col.NullByte();
        colNullMasks[i] = col.NullMask();
        if (hasMergedVarchar) {
            for (int32_t vcIdx : serialize->varcharColIndices) {
                if (vcIdx == i) { isMergedVarcharCol[i] = true; break; }
            }
        }
        if (!isMergedVarcharCol[i]) {
            switch (typeId) {
                case type::OMNI_BYTE: case type::OMNI_BOOLEAN: fixedKeySizes[i] = 1; break;
                case type::OMNI_SHORT: fixedKeySizes[i] = 2; break;
                case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_TIME32: fixedKeySizes[i] = 4; break;
                case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                case type::OMNI_DATE64: case type::OMNI_TIME64: fixedKeySizes[i] = 8; break;
                case type::OMNI_DOUBLE: fixedKeySizes[i] = 8; break;
                case type::OMNI_FLOAT: fixedKeySizes[i] = 4; break;
                case type::OMNI_DECIMAL128: fixedKeySizes[i] = 16; break;
                default: break;
            }
        }
    }
    int32_t sumFixedKeySizes = 0;
    for (int32_t i = 0; i < groupColNum; ++i) {
        sumFixedKeySizes += fixedKeySizes[i];
    }
    // === 优化2: 预缓存 state ops + sizes（消除每行虚函数 + GetMixedStateSerializeSize） ===
    int32_t totalAggStateSize = 0;
    std::vector<const MixedStateSerdeOps*> aggOps(aggregators.size());
    std::vector<int32_t> aggStateSizes(aggregators.size());
    for (size_t i = 0; i < aggregators.size(); ++i) {
        aggOps[i] = aggregators[i]->GetMixedStateSerdeOps();
        aggStateSizes[i] = static_cast<int32_t>(aggregators[i]->GetStateSize());
        totalAggStateSize += aggStateSizes[i];
    }
    int32_t varcharSlotOffset = 0;
    if (hasMergedVarchar) {
        varcharSlotOffset = serialize->aggRows->ColumnAt(serialize->varcharSlotColIdx).Offset();
    }
    output->SetVarcharSlotOffset(hasMergedVarchar ? varcharSlotOffset : -1);

    auto copyRow = [&](uint8_t *rowPtr, uint8_t *, int32_t rowIdx) {
        auto* row = reinterpret_cast<char*>(rowPtr);

        // 方案G: 预取 state 数据（给后续整体 memcpy 用）
        __builtin_prefetch(rowPtr + aggStateOffset, 0, 0);

        // 方案 B：merged VARCHAR 走 fixRowSize memcpy + VARCHAR 追加行末
        if (hasMergedVarchar && !hasComplexTypes) {
            const char* slotPtr = *reinterpret_cast<char**>(row + varcharSlotOffset);
            int32_t varcharCount = static_cast<int32_t>(serialize->varcharColIndices.size());
            int32_t headerSize = varcharCount * static_cast<int32_t>(sizeof(int32_t));
            int32_t varcharTotalSize = 0;
            const char* varcharData = nullptr;
            if (slotPtr != nullptr) {
                const int32_t* header = reinterpret_cast<const int32_t*>(slotPtr);
                varcharData = slotPtr + headerSize;
                for (int32_t v = 0; v < varcharCount; ++v) {
                    varcharTotalSize += header[v];
                }
            }
            int32_t fixRowSize = serialize->aggRows->FixedRowSize();
            int32_t length = fixRowSize + varcharTotalSize;
            auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(length));
            memcpy(rowData, rowPtr, fixRowSize);
            if (varcharTotalSize > 0 && varcharData != nullptr) {
                memcpy(rowData + fixRowSize, varcharData, varcharTotalSize);
            }
            if (slotPtr != nullptr) {
                *reinterpret_cast<int32_t*>(rowData + varcharSlotOffset) = varcharTotalSize;
                *reinterpret_cast<int32_t*>(rowData + varcharSlotOffset + sizeof(int32_t)) = fixRowSize;
            }
            int32_t keyLength = aggStateOffset - numNullBytes;
            output->SetArenaRow(rowIdx, rowData, keyLength, aggStateOffset, length);
            return;
        }

        // P2 fast path: all fixed-width, no nulls — single memcpy (skip two-pass scan)
        if (!hasComplexTypes && serialize->varcharColIndices.empty() &&
            sumFixedKeySizes >= static_cast<int32_t>(sizeof(void*))) {
            const uint8_t* nullBitsInRow = rowPtr + (aggStateOffset - numNullBytes);
            bool hasNulls = false;
            for (int32_t i = 0; i < numNullBytes; ++i) {
                if (nullBitsInRow[i] != 0) { hasNulls = true; break; }
            }
            if (!hasNulls) {
                int32_t keyLength = sumFixedKeySizes;
                int32_t stateOffset = (sumFixedKeySizes + numNullBytes + 7) & ~7;
                int32_t totalLength = (stateOffset + totalAggStateSize + 7) & ~7;
                auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(totalLength));
                memcpy(rowData, rowPtr, aggStateOffset);
                if (stateOffset > aggStateOffset) {
                    memset(rowData + aggStateOffset, 0, stateOffset - aggStateOffset);
                }
                auto *states = reinterpret_cast<uint8_t *>(rowPtr) + aggStateOffset;
                memcpy(rowData + stateOffset, states, totalAggStateSize);
                int32_t trailingPad = totalLength - (stateOffset + totalAggStateSize);
                if (trailingPad > 0) {
                    memset(rowData + stateOffset + totalAggStateSize, 0, trailingPad);
                }
                output->SetArenaRow(rowIdx, rowData, keyLength, stateOffset, totalLength);
                return;
            }
        }

        // 非 merged：原路径（nullBits 行首 + VARCHAR 内联）
        // 第一遍：解析 + 缓存 (ptr, sz, isNull) + 算 keySerializedSize
        // 消除 keyTmpBuf 中转：第二遍直接写 arena，VARCHAR 零重复解析
        // Assumes groupColNum <= 128 (group-by column counts are far below this).
        struct ColEntry { const uint8_t* ptr; int32_t sz; bool isNull; bool isComplex; };
        constexpr int32_t kMaxStackCols = 128;
        ColEntry entries[kMaxStackCols];

        int32_t keySerializedSize = 0;
        const int32_t* mergedVarcharHeader = nullptr;
        const char* mergedVarcharDataPos = nullptr;
        int32_t mergedVarcharIdx = 0;
        if (hasMergedVarchar) {
            const char* slotPtr = *reinterpret_cast<char**>(row + varcharSlotOffset);
            if (slotPtr != nullptr) {
                int32_t headerSize = static_cast<int32_t>(serialize->varcharColIndices.size())
                    * static_cast<int32_t>(sizeof(int32_t));
                mergedVarcharHeader = reinterpret_cast<const int32_t*>(slotPtr);
                mergedVarcharDataPos = slotPtr + headerSize;
            }
        }

        for (int32_t i = 0; i < groupColNum; ++i) {
            bool isNull = RowContainer::IsNullAt(row, colNullBytes[i], colNullMasks[i]);
            entries[i].isNull = isNull;
            entries[i].ptr = nullptr;
            entries[i].sz = 0;
            entries[i].isComplex = false;
            if (isNull) {
                if (isMergedVarcharCol[i] && mergedVarcharDataPos != nullptr) {
                    mergedVarcharDataPos += mergedVarcharHeader[mergedVarcharIdx];
                    mergedVarcharIdx++;
                }
                continue;
            }
            if (isMergedVarcharCol[i]) {
                if (mergedVarcharDataPos == nullptr) {
                    throw OmniException("INVALID_DATA", "mergedVarcharDataPos is null for non-null VARCHAR column");
                }
                int32_t sz = mergedVarcharHeader[mergedVarcharIdx];
                if (sz <= 1) {
                    throw OmniException("INVALID_DATA", "Found null marker or invalid size in merged block for non-null VARCHAR column");
                }
                entries[i].ptr = reinterpret_cast<const uint8_t*>(mergedVarcharDataPos);
                entries[i].sz = sz;
                keySerializedSize += sz;
                mergedVarcharDataPos += sz;
                mergedVarcharIdx++;
            } else if (colTypeIds[i] == type::OMNI_VARCHAR || colTypeIds[i] == type::OMNI_CHAR
                       || colTypeIds[i] == type::OMNI_VARBINARY) {
                char* dataPtr = *reinterpret_cast<char**>(row + colOffsets[i]);
                if (dataPtr == nullptr) {
                    entries[i].sz = 1;
                    keySerializedSize += 1;
                } else {
                    uint8_t rowLenSize = *reinterpret_cast<const uint8_t*>(dataPtr);
                    if (rowLenSize == 0) {
                        entries[i].sz = 1;
                        keySerializedSize += 1;
                    } else {
                        size_t stringLen = 0;
                        switch (rowLenSize) {
                            case 1: stringLen = *reinterpret_cast<const uint8_t*>(dataPtr + 1); break;
                            case 2: stringLen = *reinterpret_cast<const uint16_t*>(dataPtr + 1); break;
                            case 4: stringLen = *reinterpret_cast<const uint32_t*>(dataPtr + 1); break;
                        }
                        int32_t written = sizeof(uint8_t) + rowLenSize + stringLen;
                        entries[i].ptr = reinterpret_cast<const uint8_t*>(dataPtr);
                        entries[i].sz = written;
                        keySerializedSize += written;
                    }
                }
            } else if (colTypeIds[i] == type::OMNI_ARRAY || colTypeIds[i] == type::OMNI_ROW) {
                // 复杂类型：StringRef (char* + size_t) 指向 pool 中 canonical 序列化字节。
                // 内联进行段：数据前加 [rowLenSize][size] 长度前缀（与 VARCHAR 同格式，无指针）。
                char* dataPtr = *reinterpret_cast<char**>(row + colOffsets[i]);
                size_t dataSize = *reinterpret_cast<size_t*>(row + colOffsets[i] + sizeof(char*));
                if (dataPtr == nullptr || dataSize == 0) {
                    entries[i].sz = 1;  // 防御：非 null 但无数据 → 1 字节 0 标记（与 varchar 一致）
                    keySerializedSize += 1;
                } else {
                    uint8_t rowLenSize = (dataSize <= 0xFF) ? 1 : (dataSize <= 0xFFFF) ? 2 : 4;
                    entries[i].ptr = reinterpret_cast<const uint8_t*>(dataPtr);
                    entries[i].sz = static_cast<int32_t>(dataSize);
                    entries[i].isComplex = true;
                    keySerializedSize += 1 + rowLenSize + static_cast<int32_t>(dataSize);
                }
            } else {
                int32_t needed = fixedKeySizes[i];
                entries[i].ptr = reinterpret_cast<const uint8_t*>(row + colOffsets[i]);
                entries[i].sz = needed;
                keySerializedSize += needed;
            }
        }

        // 方案C: 统一非 merged 布局为 [key data][null bits][AggState]（与 RowContainer 一致）
        auto *states = reinterpret_cast<uint8_t *>(rowPtr) + aggStateOffset;
        int32_t keyLength = keySerializedSize;
        int32_t stateOffset = keySerializedSize + numNullBytes;
        int32_t totalLength = (stateOffset + totalAggStateSize + 7) & ~7;

        auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(totalLength));

        // 第二遍：key data 写入 rowData[0..keySerializedSize)，合并连续列段 memcpy
        uint8_t* current = rowData;
        int32_t i = 0;
        while (i < groupColNum) {
            const auto& e = entries[i];
            if (e.isNull) {
                i++;
                continue;
            }
            if (e.ptr == nullptr) {
                *current = 0;
                current += 1;
                i++;
                continue;
            }
            if (e.isComplex) {
                // 复杂列：写 [rowLenSize][size][data]，单独处理（不参与连续段合并，
                // 因其数据前有长度前缀，与 pool 中原始数据布局不一致）。
                size_t dataSize = static_cast<size_t>(e.sz);
                uint8_t rowLenSize = (dataSize <= 0xFF) ? 1 : (dataSize <= 0xFFFF) ? 2 : 4;
                *current = rowLenSize; current += 1;
                memcpy(current, &dataSize, rowLenSize); current += rowLenSize;
                memcpy(current, e.ptr, dataSize); current += dataSize;
                i++;
                continue;
            }
            const uint8_t* segSrc = e.ptr;
            int32_t segTotal = e.sz;
            int32_t segEnd = i;
            while (segEnd + 1 < groupColNum
                   && !entries[segEnd + 1].isNull
                   && entries[segEnd + 1].ptr != nullptr
                   && !entries[segEnd + 1].isComplex
                   && entries[segEnd + 1].ptr == entries[segEnd].ptr + entries[segEnd].sz) {
                segTotal += entries[segEnd + 1].sz;
                segEnd++;
            }
            memcpy(current, segSrc, segTotal);
            current += segTotal;
            i = segEnd + 1;
        }

        // null bits 写在 key data 之后
        memset(rowData + keySerializedSize, 0, numNullBytes);
        for (int32_t col = 0; col < groupColNum; ++col) {
            if (entries[col].isNull) {
                util::NullBits::SetNull(rowData + keySerializedSize, col);
            }
        }
        // 方案L: state 整体 memcpy（一次 memcpy 替代逐 agg ops->serialize 函数指针）
        memcpy(rowData + stateOffset, states, totalAggStateSize);

        output->SetArenaRow(rowIdx, rowData, keyLength, stateOffset, totalLength);
    };
    serialize->Extract(rowCount, outputState, copyRow, copyRow);

    *outputVecBatch = output.release();
    UpdateGetOutputInfo(rowCount);
    if (static_cast<int32_t>(outputState.hasBeenOutputNum) == totalRowCount) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
    }
    return 1;
}

int32_t HashAggregationOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!hasInputedData()) {
        return 0;
    }
    if (canOutputMixed_) {
        if (!hasSpill) {
            return OutputMixed(outputVecBatch);
        } else {
            return OutputMixedFromDisk(outputVecBatch);
        }
    }
    int32_t expectedBatchSize = 0;
    if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKeyWithoutAgg != nullptr) {
        expectedBatchSize = Output(normalizeKeyWithoutAgg, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKey != nullptr) {
        expectedBatchSize = Output(normalizeKey, outputVecBatch);
#ifdef OMNI_SVEHT32_HASH_AGG
    } else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos && arrayTable != nullptr &&
        vectorAnalyzer != nullptr && vectorAnalyzer->IsArrayHashTableType()) {
        return Output(fixedInt32, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos && !sveAosFallbackToFixedInt32) {
        return OutputFixedInt32SveAos(outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32SveAos && sveAosFallbackToFixedInt32) {
        return Output(fixedInt32, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32PairSveAos && !svePairFallbackToSerialize) {
        return OutputFixedInt32PairSveAos(outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32PairSveAos && svePairFallbackToSerialize) {
        return Output(serialize, outputVecBatch);
#endif
    } else if (LIKELY(groupByColumnsHandleType == HandleType::serialize)) {
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
    return expectedBatchSize;
}

OmniStatus HashAggregationOperator::Close()
{
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

#ifdef OMNI_SVEHT32_HASH_AGG
AggregateState *HashAggregationOperator::AllocateAggState()
{
    return reinterpret_cast<AggregateState *>(executionContext->GetArena()->Allocate(totalAggStatesSize));
}

uint32_t HashAggregationOperator::RegisterSveAggState(AggregateState *state)
{
    fixedInt32SveAosStates.emplace_back(state);
    return static_cast<uint32_t>(fixedInt32SveAosStates.size() - 1);
}

uint32_t HashAggregationOperator::RegisterSvePairAggState(AggregateState *state)
{
    fixedInt32PairSveAosStates.emplace_back(state);
    return static_cast<uint32_t>(fixedInt32PairSveAosStates.size() - 1);
}

bool HashAggregationOperator::HasSveAggData() const
{
    return fixedInt32SveAos != nullptr &&
           (fixedInt32SveAos->GetElementsSize() > 0 || hasNullGroupState32);
}

bool HashAggregationOperator::HasSvePairAggData() const
{
    return fixedInt32PairSveAos != nullptr && fixedInt32PairSveAos->GetElementsSize() > 0;
}

bool HashAggregationOperator::IsActiveSveHandle() const
{
    return (fixedInt32SveAos != nullptr && !sveAosFallbackToFixedInt32) ||
           (fixedInt32PairSveAos != nullptr && !svePairFallbackToSerialize);
}

AggregateState *HashAggregationOperator::GetOrCreateNullGroupState32(std::vector<AggregateState *> &newGroupStates)
{
    if (!hasNullGroupState32) {
        nullGroupState32 = AllocateAggState();
        hasNullGroupState32 = true;
        newGroupStates.emplace_back(nullGroupState32);
    }
    return nullGroupState32;
}

void HashAggregationOperator::FallbackSveAggToFixedInt32()
{
    if (fixedInt32SveAos != nullptr) {
        const bool hasAgg = !aggregators.empty();
        constexpr int32_t kMigrationBatchSize = 1024;
        std::vector<uint32_t> keys(kMigrationBatchSize);
        std::vector<uint32_t> handles(kMigrationBatchSize);
        uint64_t slot = 0;
        const uint64_t capacity = fixedInt32SveAos->GetCapacity();

        while (slot < capacity) {
            uint64_t nextSlot = slot;
            int32_t copied = fixedInt32SveAos->CopyGroups(
                slot, kMigrationBatchSize, keys.data(), handles.data(), nextSlot);
            for (int32_t i = 0; i < copied; ++i) {
                fixedInt32->InsertOneValueToHashmap<false>(
                    static_cast<int32_t>(keys[i]), hasAgg ? fixedInt32SveAosStates[handles[i]] : nullptr);
            }
            slot = nextSlot;
        }

        if (hasNullGroupState32) {
            fixedInt32->InsertOneValueToHashmap<true>(0, hasAgg ? nullGroupState32 : nullptr);
        }

        fixedInt32SveAos->Reset();
        fixedInt32SveAosStates.clear();
        nullGroupState32 = nullptr;
        hasNullGroupState32 = false;
        sveAosNullGroupOutput = false;
        sveAosFallbackToFixedInt32 = true;
        return;
    }
    sveAosFallbackToFixedInt32 = true;
}

void HashAggregationOperator::FallbackSvePairAggToSerializeIfEmpty()
{
    if (HasSvePairAggData()) {
        throw omniruntime::exception::OmniException(
            "UNSUPPORTED_ERROR",
            "SVE pair hash aggregation cannot fallback after SVE groups have been created");
    }
    svePairFallbackToSerialize = true;
}

void HashAggregationOperator::ProcessStatesWithNewGroups(
    VectorBatch *vecBatch, std::vector<AggregateState *> &newGroupStates)
{
    const size_t aggNum = aggregators.size();
    if (aggFiltersCount > 0) {
        int32_t filterOffset = vecBatch->GetVectorCount() - aggFiltersCount;
        for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
            auto &aggregator = aggregators[aggIdx];
            if (!newGroupStates.empty()) {
                aggregator->InitStates(newGroupStates);
            }
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
            if (!newGroupStates.empty()) {
                aggregator->InitStates(newGroupStates);
            }
            aggregator->ProcessGroup(rowsAggStates, vecBatch, 0);
        }
    }
}

struct SveInt32KeyLoader {
    BaseVector *vector = nullptr;
    uint32_t *values = nullptr;
    uint32_t *dictValues = nullptr;
    int32_t *dictIds = nullptr;
    uint32_t constValue = 0;
    bool isConst = false;
    bool constIsNull = false;
    bool isDictionary = false;

    ALWAYS_INLINE uint32_t GetValue(int32_t row) const
    {
        if (isConst) {
            return constValue;
        }
        if (isDictionary) {
            return dictValues[dictIds[row]];
        }
        return values[row];
    }

    ALWAYS_INLINE bool IsNull(int32_t row) const
    {
        return isConst ? constIsNull : vector->IsNull(row);
    }
};

static SveInt32KeyLoader MakeSveInt32KeyLoader(BaseVector *vector)
{
    SveInt32KeyLoader loader;
    loader.vector = vector;
    if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
        loader.isConst = true;
        loader.constIsNull = vector->IsNull(0);
        if (!loader.constIsNull) {
            loader.constValue =
                static_cast<uint32_t>(reinterpret_cast<ConstVector<int32_t> *>(vector)->GetConstValue());
        }
        return loader;
    }
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vector);
        loader.isDictionary = true;
        loader.dictValues = reinterpret_cast<uint32_t *>(unsafe::UnsafeDictionaryVector::GetDictionary(dictVector));
        loader.dictIds = unsafe::UnsafeDictionaryVector::GetIds(dictVector);
        return loader;
    }
    loader.values = reinterpret_cast<uint32_t *>(VectorHelper::UnsafeGetValues(vector));
    return loader;
}

static ALWAYS_INLINE bool IsConstVector(BaseVector *vector)
{
    return vector->GetEncoding() == OMNI_ENCODING_CONST;
}

static ALWAYS_INLINE bool IsDictionaryVector(BaseVector *vector)
{
    return vector->GetEncoding() == OMNI_DICTIONARY;
}

void HashAggregationOperator::EmplaceFixedInt32SveAos(VectorBatch *vecBatch, BaseVector *groupVector)
{
    const bool hasAgg = !aggregators.empty();
    const int32_t rowCount = vecBatch->GetRowCount();

    if (groupVector->GetEncoding() == OMNI_ENCODING_CONST) {
        std::vector<AggregateState *> newGroupStates;
        if (groupVector->IsNull(0)) {
            if (!hasAgg) {
                hasNullGroupState32 = true;
                return;
            }
            rowsAggStates.resize(rowCount);
            auto *state = GetOrCreateNullGroupState32(newGroupStates);
            std::fill(rowsAggStates.begin(), rowsAggStates.end(), state);
            ProcessStatesWithNewGroups(vecBatch, newGroupStates);
            return;
        }

        auto key = static_cast<uint32_t>(reinterpret_cast<ConstVector<int32_t> *>(groupVector)->GetConstValue());
        if (key == hashmap::SveAggAosHashTable32::kEmptyKey) {
            FallbackSveAggToFixedInt32();
            BaseVector *groupVectors[] = {groupVector};
            Emplace(fixedInt32, vecBatch, groupVectors, 1);
            return;
        }

        auto ret = fixedInt32SveAos->EmplaceScalar(key);
        if (!hasAgg) {
            return;
        }

        uint32_t handle;
        if (ret.IsInsert()) {
            auto *state = AllocateAggState();
            handle = RegisterSveAggState(state);
            ret.SetValue(handle);
            newGroupStates.emplace_back(state);
        } else {
            handle = ret.GetValue();
        }
        rowsAggStates.resize(rowCount);
        std::fill(rowsAggStates.begin(), rowsAggStates.end(), fixedInt32SveAosStates[handle]);
        ProcessStatesWithNewGroups(vecBatch, newGroupStates);
        return;
    }

    const bool isDictionary = IsDictionaryVector(groupVector);
    auto *values = isDictionary ? nullptr : reinterpret_cast<uint32_t *>(VectorHelper::UnsafeGetValues(groupVector));
    auto loader = isDictionary ? MakeSveInt32KeyLoader(groupVector) : SveInt32KeyLoader {};
    std::vector<uint32_t> compactKeys;
    std::vector<uint32_t> compactRows;
    compactKeys.reserve(rowCount);
    compactRows.reserve(rowCount);
    std::vector<AggregateState *> newGroupStates;
    std::vector<uint32_t> nullRows;
    bool hasNullInBatch = false;
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        if (groupVector->IsNull(rowIdx)) {
            if (hasAgg) {
                if (nullRows.empty()) {
                    nullRows.reserve(16);
                }
                nullRows.emplace_back(static_cast<uint32_t>(rowIdx));
            } else {
                hasNullInBatch = true;
            }
            continue;
        }
        const uint32_t key = isDictionary ? loader.GetValue(rowIdx) : values[rowIdx];
        if (key == hashmap::SveAggAosHashTable32::kEmptyKey) {
            FallbackSveAggToFixedInt32();
            BaseVector *groupVectors[] = {groupVector};
            Emplace(fixedInt32, vecBatch, groupVectors, 1);
            return;
        }
        compactKeys.emplace_back(key);
        compactRows.emplace_back(static_cast<uint32_t>(rowIdx));
    }

    if (hasAgg) {
        rowsAggStates.resize(rowCount);
        for (uint32_t rowIdx : nullRows) {
            rowsAggStates[rowIdx] = GetOrCreateNullGroupState32(newGroupStates);
        }
    } else if (hasNullInBatch) {
        hasNullGroupState32 = true;
    }

    const int32_t compactCount = static_cast<int32_t>(compactKeys.size());
    if (compactCount > 0) {
        std::vector<uint32_t> hitRows(compactCount);
        std::vector<uint32_t> hitHandles(compactCount);
        std::vector<uint32_t> missRows(compactCount);
        std::vector<uint32_t> missKeys(compactCount);
#ifdef SVEHTMISSES
        std::vector<uint32_t> missSlots(compactCount);
        auto counts = fixedInt32SveAos->LookupBatchSVEForInsert(compactKeys.data(), compactRows.data(), compactCount,
            hitRows.data(), hitHandles.data(), missRows.data(), missKeys.data(), missSlots.data());
        const bool useKnownMissInsert = ShouldUseKnownMissInsert(fixedInt32SveAos.get(), counts.missCount,
            [&]() { return EstimateDistinctMisses32(missKeys.data(), counts.missCount); });
#else
        auto counts = fixedInt32SveAos->LookupBatchSVE(compactKeys.data(), compactRows.data(), compactCount,
            hitRows.data(), hitHandles.data(), missRows.data(), missKeys.data());
#endif

        if (!hasAgg) {
#ifdef SVEHTMISSES
            bool knownMissActive = useKnownMissInsert;
#endif
            for (int32_t i = 0; i < counts.missCount; ++i) {
#ifdef SVEHTMISSES
                if (knownMissActive) {
                    const uint64_t capacityBefore = fixedInt32SveAos->GetCapacity();
                    fixedInt32SveAos->EmplaceKnownMiss(missKeys[i], missSlots[i]);
                    knownMissActive = fixedInt32SveAos->GetCapacity() == capacityBefore;
                } else {
                    fixedInt32SveAos->EmplaceScalar(missKeys[i]);
                }
#else
                fixedInt32SveAos->EmplaceScalar(missKeys[i]);
#endif
            }
            return;
        }

        for (int32_t i = 0; i < counts.hitCount; ++i) {
            rowsAggStates[hitRows[i]] = fixedInt32SveAosStates[hitHandles[i]];
        }

#ifdef SVEHTMISSES
        bool knownMissActive = useKnownMissInsert;
#endif
        for (int32_t i = 0; i < counts.missCount; ++i) {
#ifdef SVEHTMISSES
            auto ret = [&]() {
                if (knownMissActive) {
                    const uint64_t capacityBefore = fixedInt32SveAos->GetCapacity();
                    auto knownMissRet = fixedInt32SveAos->EmplaceKnownMiss(missKeys[i], missSlots[i]);
                    knownMissActive = fixedInt32SveAos->GetCapacity() == capacityBefore;
                    return knownMissRet;
                }
                return fixedInt32SveAos->EmplaceScalar(missKeys[i]);
            }();
#else
            auto ret = fixedInt32SveAos->EmplaceScalar(missKeys[i]);
#endif
            uint32_t handle;
            if (ret.IsInsert()) {
                auto *state = AllocateAggState();
                handle = RegisterSveAggState(state);
                ret.SetValue(handle);
                newGroupStates.emplace_back(state);
            } else {
                handle = ret.GetValue();
            }
            rowsAggStates[missRows[i]] = fixedInt32SveAosStates[handle];
        }
    }

    if (hasAgg) {
        ProcessStatesWithNewGroups(vecBatch, newGroupStates);
    }
}

void HashAggregationOperator::EmplaceFixedInt32PairSveAos(
    VectorBatch *vecBatch, BaseVector *groupVector0, BaseVector *groupVector1)
{
    const bool hasAgg = !aggregators.empty();
    const int32_t rowCount = vecBatch->GetRowCount();
    const bool needsKeyLoader = IsConstVector(groupVector0) || IsConstVector(groupVector1) ||
                                IsDictionaryVector(groupVector0) || IsDictionaryVector(groupVector1);

    if (needsKeyLoader) {
        auto loader0 = MakeSveInt32KeyLoader(groupVector0);
        auto loader1 = MakeSveInt32KeyLoader(groupVector1);
        if (loader0.isConst && loader1.isConst) {
            hashmap::SveAggAosHashTable32Pair::Key key;
            if (loader0.constIsNull) {
                key.nullMask |= hashmap::SveAggAosHashTable32Pair::kKey0Null;
            } else {
                key.key0 = loader0.constValue;
            }
            if (loader1.constIsNull) {
                key.nullMask |= hashmap::SveAggAosHashTable32Pair::kKey1Null;
            } else {
                key.key1 = loader1.constValue;
            }

            auto ret = fixedInt32PairSveAos->EmplaceScalar(key);
            if (!hasAgg) {
                return;
            }

            std::vector<AggregateState *> newGroupStates;
            uint32_t handle;
            if (ret.IsInsert()) {
                auto *state = AllocateAggState();
                handle = RegisterSvePairAggState(state);
                ret.SetValue(handle);
                newGroupStates.emplace_back(state);
            } else {
                handle = ret.GetValue();
            }
            rowsAggStates.resize(rowCount);
            std::fill(rowsAggStates.begin(), rowsAggStates.end(), fixedInt32PairSveAosStates[handle]);
            ProcessStatesWithNewGroups(vecBatch, newGroupStates);
            return;
        }

        std::vector<uint32_t> keys0(rowCount);
        std::vector<uint32_t> keys1(rowCount);
        std::vector<uint32_t> nullMasks(rowCount);
        std::vector<uint32_t> rowIds(rowCount);
        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
            uint32_t nullMask = 0;
            if (loader0.IsNull(rowIdx)) {
                nullMask |= hashmap::SveAggAosHashTable32Pair::kKey0Null;
                keys0[rowIdx] = 0;
            } else {
                keys0[rowIdx] = loader0.GetValue(rowIdx);
            }
            if (loader1.IsNull(rowIdx)) {
                nullMask |= hashmap::SveAggAosHashTable32Pair::kKey1Null;
                keys1[rowIdx] = 0;
            } else {
                keys1[rowIdx] = loader1.GetValue(rowIdx);
            }
            nullMasks[rowIdx] = nullMask;
            rowIds[rowIdx] = static_cast<uint32_t>(rowIdx);
        }

        std::vector<uint32_t> hitRows(rowCount);
        std::vector<uint32_t> hitHandles(rowCount);
        std::vector<uint32_t> missRows(rowCount);
        std::vector<hashmap::SveAggAosHashTable32Pair::Key> missKeys(rowCount);
#ifdef SVEHTMISSES
        std::vector<uint32_t> missSlots(rowCount);
        auto counts = fixedInt32PairSveAos->LookupBatchSVEForInsert(keys0.data(), keys1.data(), nullMasks.data(),
            rowIds.data(), rowCount, hitRows.data(), hitHandles.data(), missRows.data(), missKeys.data(),
            missSlots.data());
        const bool useKnownMissInsert = ShouldUseKnownMissInsert(fixedInt32PairSveAos.get(), counts.missCount,
            [&]() { return EstimateDistinctMisses32Pair(missKeys.data(), counts.missCount); });
#else
        auto counts = fixedInt32PairSveAos->LookupBatchSVE(keys0.data(), keys1.data(), nullMasks.data(),
            rowIds.data(), rowCount, hitRows.data(), hitHandles.data(), missRows.data(), missKeys.data());
#endif

        if (!hasAgg) {
#ifdef SVEHTMISSES
            bool knownMissActive = useKnownMissInsert;
#endif
            for (int32_t i = 0; i < counts.missCount; ++i) {
#ifdef SVEHTMISSES
                if (knownMissActive) {
                    const uint64_t capacityBefore = fixedInt32PairSveAos->GetCapacity();
                    fixedInt32PairSveAos->EmplaceKnownMiss(missKeys[i], missSlots[i]);
                    knownMissActive = fixedInt32PairSveAos->GetCapacity() == capacityBefore;
                } else {
                    fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
                }
#else
                fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
#endif
            }
            return;
        }

        rowsAggStates.resize(rowCount);
        std::vector<AggregateState *> newGroupStates;
        for (int32_t i = 0; i < counts.hitCount; ++i) {
            rowsAggStates[hitRows[i]] = fixedInt32PairSveAosStates[hitHandles[i]];
        }

#ifdef SVEHTMISSES
        bool knownMissActive = useKnownMissInsert;
#endif
        for (int32_t i = 0; i < counts.missCount; ++i) {
#ifdef SVEHTMISSES
            auto ret = [&]() {
                if (knownMissActive) {
                    const uint64_t capacityBefore = fixedInt32PairSveAos->GetCapacity();
                    auto knownMissRet = fixedInt32PairSveAos->EmplaceKnownMiss(missKeys[i], missSlots[i]);
                    knownMissActive = fixedInt32PairSveAos->GetCapacity() == capacityBefore;
                    return knownMissRet;
                }
                return fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
            }();
#else
            auto ret = fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
#endif
            uint32_t handle;
            if (ret.IsInsert()) {
                auto *state = AllocateAggState();
                handle = RegisterSvePairAggState(state);
                ret.SetValue(handle);
                newGroupStates.emplace_back(state);
            } else {
                handle = ret.GetValue();
            }
            rowsAggStates[missRows[i]] = fixedInt32PairSveAosStates[handle];
        }

        ProcessStatesWithNewGroups(vecBatch, newGroupStates);
        return;
    }

    auto *values0 = reinterpret_cast<uint32_t *>(VectorHelper::UnsafeGetValues(groupVector0));
    auto *values1 = reinterpret_cast<uint32_t *>(VectorHelper::UnsafeGetValues(groupVector1));

    std::vector<uint32_t> keys0(rowCount);
    std::vector<uint32_t> keys1(rowCount);
    std::vector<uint32_t> nullMasks(rowCount);
    std::vector<uint32_t> rowIds(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        uint32_t nullMask = 0;
        if (groupVector0->IsNull(rowIdx)) {
            nullMask |= hashmap::SveAggAosHashTable32Pair::kKey0Null;
        }
        if (groupVector1->IsNull(rowIdx)) {
            nullMask |= hashmap::SveAggAosHashTable32Pair::kKey1Null;
        }
        keys0[rowIdx] = values0[rowIdx];
        keys1[rowIdx] = values1[rowIdx];
        nullMasks[rowIdx] = nullMask;
        rowIds[rowIdx] = static_cast<uint32_t>(rowIdx);
    }

    std::vector<uint32_t> hitRows(rowCount);
    std::vector<uint32_t> hitHandles(rowCount);
    std::vector<uint32_t> missRows(rowCount);
    std::vector<hashmap::SveAggAosHashTable32Pair::Key> missKeys(rowCount);
#ifdef SVEHTMISSES
    std::vector<uint32_t> missSlots(rowCount);
    auto counts = fixedInt32PairSveAos->LookupBatchSVEForInsert(keys0.data(), keys1.data(), nullMasks.data(),
        rowIds.data(), rowCount, hitRows.data(), hitHandles.data(), missRows.data(), missKeys.data(),
        missSlots.data());
    const bool useKnownMissInsert = ShouldUseKnownMissInsert(fixedInt32PairSveAos.get(), counts.missCount,
        [&]() { return EstimateDistinctMisses32Pair(missKeys.data(), counts.missCount); });
#else
    auto counts = fixedInt32PairSveAos->LookupBatchSVE(keys0.data(), keys1.data(), nullMasks.data(), rowIds.data(),
        rowCount, hitRows.data(), hitHandles.data(), missRows.data(), missKeys.data());
#endif

    if (!hasAgg) {
#ifdef SVEHTMISSES
        bool knownMissActive = useKnownMissInsert;
#endif
        for (int32_t i = 0; i < counts.missCount; ++i) {
#ifdef SVEHTMISSES
            if (knownMissActive) {
                const uint64_t capacityBefore = fixedInt32PairSveAos->GetCapacity();
                fixedInt32PairSveAos->EmplaceKnownMiss(missKeys[i], missSlots[i]);
                knownMissActive = fixedInt32PairSveAos->GetCapacity() == capacityBefore;
            } else {
                fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
            }
#else
            fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
#endif
        }
        return;
    }

    rowsAggStates.resize(rowCount);
    std::vector<AggregateState *> newGroupStates;
    for (int32_t i = 0; i < counts.hitCount; ++i) {
        rowsAggStates[hitRows[i]] = fixedInt32PairSveAosStates[hitHandles[i]];
    }

#ifdef SVEHTMISSES
    bool knownMissActive = useKnownMissInsert;
#endif
    for (int32_t i = 0; i < counts.missCount; ++i) {
#ifdef SVEHTMISSES
        auto ret = [&]() {
            if (knownMissActive) {
                const uint64_t capacityBefore = fixedInt32PairSveAos->GetCapacity();
                auto knownMissRet = fixedInt32PairSveAos->EmplaceKnownMiss(missKeys[i], missSlots[i]);
                knownMissActive = fixedInt32PairSveAos->GetCapacity() == capacityBefore;
                return knownMissRet;
            }
            return fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
        }();
#else
        auto ret = fixedInt32PairSveAos->EmplaceScalar(missKeys[i]);
#endif
        uint32_t handle;
        if (ret.IsInsert()) {
            auto *state = AllocateAggState();
            handle = RegisterSvePairAggState(state);
            ret.SetValue(handle);
            newGroupStates.emplace_back(state);
        } else {
            handle = ret.GetValue();
        }
        rowsAggStates[missRows[i]] = fixedInt32PairSveAosStates[handle];
    }

    ProcessStatesWithNewGroups(vecBatch, newGroupStates);
}
#endif

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

    if (aggregators.empty()) {
        if (groupByColumnsHandleType == HandleType::serialize && serialize != nullptr) {
            serialize->Extract(expectSize, outputState,
                [&](uint8_t *rowPtr, uint8_t *, int32_t idx) {
                    serialize->ParseKeyToCols(rowPtr, groupOutputVectors, groupColNum, idx);
                }, [&](uint8_t *rowPtr, uint8_t *, int32_t idx) {
                    serialize->ParseNull(
                        reinterpret_cast<char *>(rowPtr), groupOutputVectors, groupColNum, idx);
                });
        } else {
            deserializeHashmap->Extract(expectSize, outputState,
                [&](const auto &key, uint8_t *, int32_t idx) {
                    deserializeHashmap->ParseKeyToCols(key, groupOutputVectors, groupColNum, idx);
                }, [&](const auto &key, uint8_t *, int32_t idx) {
                    deserializeHashmap->ParseNull(key, groupOutputVectors, groupColNum, idx);
                });
        }
        return;
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
    aggregationSort->ResizeKvVector(totalSpillCount);

    auto aggregationSortPtr = aggregationSort.get();
    OutputState curOutputState;
    {
        if (serialize != nullptr) {
            serialize->SpillExtract(totalSpillCount, spillOutputState,
            [&](const auto &key, int64_t hashVal, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVectorWithHashVal(key, value, idx, hashVal);
                }, [&](const auto &key, int64_t hashVal, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVectorWithHashVal(key, value, idx, hashVal);
            });
        } else if (fixedInt32 != nullptr) {
            fixedInt32->Extract(totalSpillCount, spillOutputState,
                [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVector(key, value, idx);
                }, [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseNullHashMapToVector(key, value, idx);
            });
        } else if (fixedInt64 != nullptr) {
            fixedInt64->Extract(totalSpillCount, spillOutputState,
                [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVector(key, value, idx);
                }, [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseNullHashMapToVector(key, value, idx);
            });
        } else if (fixedInt16 != nullptr) {
            fixedInt16->Extract(totalSpillCount, spillOutputState,
                [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVector(key, value, idx);
                }, [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseNullHashMapToVector(key, value, idx);
            });
        } else if (packedInt32 != nullptr) {
            packedInt32->Extract(totalSpillCount, spillOutputState,
                [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVectorAsBytes(key, value, idx);
                }, [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseNullHashMapToVector(key, value, idx);
            });
        } else if (packedInt64 != nullptr) {
            packedInt64->Extract(totalSpillCount, spillOutputState,
                [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseHashMapToVectorAsBytes(key, value, idx);
                }, [&](const auto &key, uint8_t *value, int32_t idx) mutable {
                aggregationSortPtr->ParseNullHashMapToVector(key, value, idx);
            });
        }
    }

    aggregationSort->SortKvVector(serialize != nullptr);
    auto rowCount = aggregationSort->GetRowCount();
    LogDebug("Spill data to disk starting in hash aggregation operator, rowCount=%lld\n", rowCount);
    ErrorCode result = spiller->Spill(aggregationSort.get(), this, serialize != nullptr);
    LogDebug("Spill data to disk finished in hash aggregation operator, rowCount=%lld\n", rowCount);
    aggregationSort->ClearVector();
    return result;
}

ErrorCode HashAggregationOperator::SpillHashMap()
{
    if (groupByColumnsHandleType == HandleType::NormalizeKey &&
        (normalizeKey != nullptr || normalizeKeyWithoutAgg != nullptr)) {
        FallbackNormalizeKeyToSerialize(nullptr, static_cast<int32_t>(groupByCols.size()));
    }
    auto rowCount = 0;
    if (serialize != nullptr) {
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
    auto result = SpillToDisk();
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
    if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKeyWithoutAgg != nullptr) {
        return normalizeKeyWithoutAgg->GetElementsSize();
    } else if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKey != nullptr) {
        return normalizeKey->GetElementsSize();
#ifdef OMNI_SVEHT32_HASH_AGG
    } else if (fixedInt32PairSveAos != nullptr && !svePairFallbackToSerialize) {
        return fixedInt32PairSveAos->GetElementsSize();
    } else if (fixedInt32SveAos != nullptr && !sveAosFallbackToFixedInt32) {
        return fixedInt32SveAos->GetElementsSize() + (hasNullGroupState32 ? 1 : 0);
#endif
    } else if (serialize != nullptr) {
        return serialize->GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        return fixedInt32->GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        return fixedInt64->GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        return fixedInt16->GetElementsSize();
    } else if (packedInt32 != nullptr) {
        return packedInt32->GetElementsSize();
    } else if (packedInt64 != nullptr) {
        return packedInt64->GetElementsSize();
    } else if (packedInt128 != nullptr) {
        return packedInt128->GetElementsSize();
    }
    return 0;
}

VectorBatch *HashAggregationOperator::AlignSchema(VectorBatch *inputVecBatch)
{
    // 混存路径：列存 → MixedVectorBatch（COMPLETE_ROW_ONLY）
    // 条件与 GetOutput 中 OutputMixed 的调用条件保持一致
    if (canOutputMixed_) {
        return AlignSchemaMixed(inputVecBatch);
    }

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

VectorBatch *HashAggregationOperator::AlignSchemaMixed(VectorBatch *inputVecBatch)
{
    executionContext->GetArena()->Reset();

    int32_t rowCount = inputVecBatch->GetRowCount();
    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    int32_t numNullBytes = util::NullBits::NumBytes(groupColNum);

    // === Step 1: 解码 group-by 列 ===
    std::vector<BaseVector *> groupVectors(groupColNum);
    for (int32_t i = 0; i < groupColNum; ++i) {
        groupVectors[i] = inputVecBatch->Get(groupByCols[i].idx);
    }
    serialize->DecodeGroupByColumns(groupVectors.data(), groupColNum, rowCount);

    // 预计算列信息
    std::vector<DataTypeId> keyTypeIds;
    keyTypeIds.reserve(groupColNum);
    std::vector<int32_t> fixedKeySizes(groupColNum, 0);
    std::vector<bool> isVarcharCol(groupColNum, false);
    std::vector<bool> isComplexCol(groupColNum, false);
    bool hasComplexTypes = false;
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto typeId = groupByCols[i].input->GetId();
        keyTypeIds.push_back(typeId);
        if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
            isVarcharCol[i] = true;
        } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_ROW) {
            isComplexCol[i] = true;
            hasComplexTypes = true;
        } else {
            fixedKeySizes[i] = OperatorUtil::GetTypeSize(groupByCols[i].input);
        }
    }
    int32_t sumFixedKeySizes = 0;
    for (int32_t i = 0; i < groupColNum; ++i) {
        sumFixedKeySizes += fixedKeySizes[i];
    }
    // 复杂列序列化需要 serializers（ARRAY/ROW 用 complexVectorSerializerCenter）。
    // AddInput 每次调用都会 ResetSerializer，这里确保 AlignSchemaMixed 时已压入。
    if (hasComplexTypes) {
        PrepareSerializeHandlers(groupVectors.data(), groupColNum);
    }

    // === Step 2: 批量计算 agg state ===
    // rowStates[i] 指向第 i 行的 state 区基址。stateBuffer 不含 key/nullBits 前缀，
    // buffer 开头即 state 区，故不再加任何偏移；InitState/ProcessGroupInternal 内部
    // 会各自加 aggregator->aggStateOffset 定位本 aggregator 的 state
    // （见 sum_aggregator.cpp:120/184，collect_list_aggregator.cpp:102/204）。
    // 这与 Emplace 路径不同：Emplace 的行指针指向 RowContainer 行首，需先加
    // serialize->AggStateOffset() 跳过 key/nullBits 前缀才到 state 区基址。
    std::vector<uint8_t> stateBuffer(static_cast<size_t>(rowCount) * totalAggStatesSize);
    std::vector<AggregateState *> rowStates(rowCount);
    for (int32_t i = 0; i < rowCount; ++i) {
        rowStates[i] = stateBuffer.data() + static_cast<size_t>(i) * totalAggStatesSize;
    }
    for (auto &aggregator : aggregators) {
        aggregator->InitStates(rowStates);
    }
    for (auto &aggregator : aggregators) {
        aggregator->ProcessGroup(rowStates, inputVecBatch, 0);
    }

    // === Step 3: 创建 MixedVectorBatch ===
    bool hasMergedVarchar = (serialize->varcharColIndices.size() > 1);
    int32_t mergedSlotOff = 0;
    int32_t fixRowSize = 0;
    int32_t mergedAggStateOffset = 0;
    int32_t mergedKeyLength = 0;
    if (hasMergedVarchar) {
        mergedSlotOff = serialize->aggRows->ColumnAt(serialize->varcharSlotColIdx).Offset();
        fixRowSize = serialize->aggRows->FixedRowSize();
        mergedAggStateOffset = serialize->AggStateOffset();
        mergedKeyLength = mergedAggStateOffset - numNullBytes;
    }
    auto output = std::make_unique<vec::MixedVectorBatch>(rowCount, keyTypeIds);
    output->SetMode(vec::COMPLETE_ROW_ONLY);
    output->SetVarcharSlotOffset(hasMergedVarchar ? mergedSlotOff : -1);
    output->PrepareRowArena(static_cast<int64_t>(rowCount) * (fixRowSize + totalAggStatesSize + 256));

    // === Step 4: 逐行序列化到 RowSegment ===
    if (hasMergedVarchar) {
        // merged varchar 路径：[fixed(含slot元数据/nullBits/AggState)][varchar块]
        // 与 OutputMixed merged path 一致（group_aggregation.cpp:866-893）
        int32_t varcharCount = static_cast<int32_t>(serialize->varcharColIndices.size());
        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
            // 第一遍：计算 varcharTotalSize
            int32_t varcharTotalSize = 0;
            for (int32_t v = 0; v < varcharCount; ++v) {
                int32_t vcIdx = serialize->varcharColIndices[v];
                auto &decoded = serialize->decodedCols[vcIdx];
                if (decoded.IsNull(rowIdx)) {
                    varcharTotalSize += 1;
                } else {
                    auto sv = serialize->GetVarcharFromDecoded(decoded, rowIdx);
                    uint8_t rowLenSize = (sv.size() <= 0xFF) ? 1 : (sv.size() <= 0xFFFF) ? 2 : 4;
                    varcharTotalSize += 1 + rowLenSize + static_cast<int32_t>(sv.size());
                }
            }
            int32_t length = fixRowSize + varcharTotalSize;
            auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(length));
            memset(rowData, 0, fixRowSize);

            // 写固定列（非 varchar，按 RowContainer offset）
            for (int32_t col = 0; col < groupColNum; ++col) {
                if (isVarcharCol[col]) continue;
                auto &decoded = serialize->decodedCols[col];
                int32_t off = serialize->aggRows->ColumnAt(col).Offset();
                switch (keyTypeIds[col]) {
                    case type::OMNI_BYTE: case type::OMNI_BOOLEAN: {
                        auto val = decoded.GetValue<int8_t>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(int8_t)); break;
                    }
                    case type::OMNI_SHORT: {
                        auto val = decoded.GetValue<int16_t>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(int16_t)); break;
                    }
                    case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_TIME32: {
                        auto val = decoded.GetValue<int32_t>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(int32_t)); break;
                    }
                    case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                    case type::OMNI_DATE64: case type::OMNI_TIME64: {
                        auto val = decoded.GetValue<int64_t>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(int64_t)); break;
                    }
                    case type::OMNI_DOUBLE: {
                        auto val = decoded.GetValue<double>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(double)); break;
                    }
                    case type::OMNI_FLOAT: {
                        auto val = decoded.GetValue<float>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(float)); break;
                    }
                    case type::OMNI_DECIMAL128: {
                        auto val = decoded.GetValue<Decimal128>(rowIdx);
                        memcpy(rowData + off, &val, sizeof(Decimal128)); break;
                    }
                    default: break;
                }
            }

            // slot 列位置写 [varcharTotalSize][fixRowSize](slotDataOff=fixRowSize)
            *reinterpret_cast<int32_t *>(rowData + mergedSlotOff) = varcharTotalSize;
            *reinterpret_cast<int32_t *>(rowData + mergedSlotOff + sizeof(int32_t)) = fixRowSize;

            // nullBits
            for (int32_t col = 0; col < groupColNum; ++col) {
                auto &decoded = serialize->decodedCols[col];
                if (decoded.IsNull(rowIdx)) {
                    util::NullBits::SetNull(rowData + mergedKeyLength, col);
                }
            }

            // AggState
            if (totalAggStatesSize > 0) {
                memcpy(rowData + mergedAggStateOffset, rowStates[rowIdx], totalAggStatesSize);
            }

            // varchar 块（各 varchar 连续拼接：null=1字节0，非null=[rowLenSize][len][data]）
            uint8_t *vptr = rowData + fixRowSize;
            for (int32_t v = 0; v < varcharCount; ++v) {
                int32_t vcIdx = serialize->varcharColIndices[v];
                auto &decoded = serialize->decodedCols[vcIdx];
                if (decoded.IsNull(rowIdx)) {
                    *vptr = 0; vptr += 1;
                } else {
                    auto sv = serialize->GetVarcharFromDecoded(decoded, rowIdx);
                    uint8_t rowLenSize = (sv.size() <= 0xFF) ? 1 : (sv.size() <= 0xFFFF) ? 2 : 4;
                    *vptr = rowLenSize;
                    size_t strLen = sv.size();
                    memcpy(vptr + 1, &strLen, rowLenSize);
                    memcpy(vptr + 1 + rowLenSize, sv.data(), sv.size());
                    vptr += 1 + rowLenSize + sv.size();
                }
            }

            output->SetArenaRow(rowIdx, rowData, mergedKeyLength, mergedAggStateOffset, length);
        }
    } else {
        // 非 merged 路径：[keyData][nullBits][AggState]，尾部对齐到 8 字节
        // 与 OutputMixed non-merged 一致
        bool canUseP2 = !hasComplexTypes && serialize->varcharColIndices.empty() &&
            sumFixedKeySizes >= static_cast<int32_t>(sizeof(void*));
        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        // P2 fast path: all fixed-width, no nulls — aligned stateOffset (与 OutputMixed P2 一致)
        if (canUseP2) {
            bool hasNulls = false;
            for (int32_t col = 0; col < groupColNum; ++col) {
                if (serialize->decodedCols[col].IsNull(rowIdx)) { hasNulls = true; break; }
            }
            if (!hasNulls) {
                int32_t keyLength = sumFixedKeySizes;
                int32_t stateOffset = (sumFixedKeySizes + numNullBytes + 7) & ~7;
                int32_t totalLength = (stateOffset + totalAggStatesSize + 7) & ~7;
                auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(totalLength));
                memset(rowData, 0, totalLength);
                uint8_t *current = rowData;
                for (int32_t col = 0; col < groupColNum; ++col) {
                    auto &decoded = serialize->decodedCols[col];
                    auto typeId = keyTypeIds[col];
                    switch (typeId) {
                        case type::OMNI_BYTE: case type::OMNI_BOOLEAN: {
                            auto val = decoded.GetValue<int8_t>(rowIdx);
                            memcpy(current, &val, sizeof(int8_t)); current += sizeof(int8_t); break;
                        }
                        case type::OMNI_SHORT: {
                            auto val = decoded.GetValue<int16_t>(rowIdx);
                            memcpy(current, &val, sizeof(int16_t)); current += sizeof(int16_t); break;
                        }
                        case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_TIME32: {
                            auto val = decoded.GetValue<int32_t>(rowIdx);
                            memcpy(current, &val, sizeof(int32_t)); current += sizeof(int32_t); break;
                        }
                        case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                        case type::OMNI_DATE64: case type::OMNI_TIME64: {
                            auto val = decoded.GetValue<int64_t>(rowIdx);
                            memcpy(current, &val, sizeof(int64_t)); current += sizeof(int64_t); break;
                        }
                        case type::OMNI_DOUBLE: {
                            auto val = decoded.GetValue<double>(rowIdx);
                            memcpy(current, &val, sizeof(double)); current += sizeof(double); break;
                        }
                        case type::OMNI_FLOAT: {
                            auto val = decoded.GetValue<float>(rowIdx);
                            memcpy(current, &val, sizeof(float)); current += sizeof(float); break;
                        }
                        case type::OMNI_DECIMAL128: {
                            auto val = decoded.GetValue<Decimal128>(rowIdx);
                            memcpy(current, &val, sizeof(Decimal128)); current += sizeof(Decimal128); break;
                        }
                        default: break;
                    }
                }
                if (totalAggStatesSize > 0) {
                    memcpy(rowData + stateOffset, rowStates[rowIdx], totalAggStatesSize);
                }
                output->SetArenaRow(rowIdx, rowData, keyLength, stateOffset, totalLength);
                continue;
            }
        }
        // 第一遍: 计算 keySerializedSize
        // 复杂列缓存（串行化到 pool 的临时 StringRef，供第二遍 memcpy）
        constexpr int32_t kMaxStackCols = 128;
        const uint8_t* complexData[kMaxStackCols] = {};
        int32_t complexSize[kMaxStackCols] = {};
        int32_t keySerializedSize = 0;
        for (int32_t col = 0; col < groupColNum; ++col) {
            auto &decoded = serialize->decodedCols[col];
            bool isNull = decoded.IsNull(rowIdx);
            if (isNull) {
                continue;
            }
            if (isVarcharCol[col]) {
                auto sv = serialize->GetVarcharFromDecoded(decoded, rowIdx);
                uint8_t rowLenSize = (sv.size() <= 0xFF) ? 1 : (sv.size() <= 0xFFFF) ? 2 : 4;
                keySerializedSize += 1 + rowLenSize + static_cast<int32_t>(sv.size());
            } else if (isComplexCol[col]) {
                // 复杂类型：序列化到 pool 缓存，段内写 [rowLenSize][size][data]（无指针）
                type::StringRef key = serialize->SerializeComplexCol(col, rowIdx);
                complexData[col] = reinterpret_cast<const uint8_t*>(key.data);
                complexSize[col] = static_cast<int32_t>(key.size);
                uint8_t rowLenSize = (key.size <= 0xFF) ? 1 : (key.size <= 0xFFFF) ? 2 : 4;
                keySerializedSize += 1 + rowLenSize + static_cast<int32_t>(key.size);
            } else {
                keySerializedSize += fixedKeySizes[col];
            }
        }

        int32_t keyLength = keySerializedSize;
        int32_t stateOffset = keySerializedSize + numNullBytes;
        int32_t totalLength = (stateOffset + totalAggStatesSize + 7) & ~7;

        auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(totalLength));

        // 写 keyData (rowData[0..keySerializedSize))
        uint8_t *current = rowData;
        for (int32_t col = 0; col < groupColNum; ++col) {
            auto &decoded = serialize->decodedCols[col];
            bool isNull = decoded.IsNull(rowIdx);
            if (isNull) {
                continue;
            }
            if (isVarcharCol[col]) {
                auto sv = serialize->GetVarcharFromDecoded(decoded, rowIdx);
                uint8_t rowLenSize = (sv.size() <= 0xFF) ? 1 : (sv.size() <= 0xFFFF) ? 2 : 4;
                *current = rowLenSize;
                size_t strLen = sv.size();
                memcpy(current + 1, &strLen, rowLenSize);
                memcpy(current + 1 + rowLenSize, sv.data(), sv.size());
                current += 1 + rowLenSize + sv.size();
            } else if (isComplexCol[col]) {
                // 写 [rowLenSize][size][data]，数据来自第一遍缓存的 pool StringRef
                size_t dataSize = static_cast<size_t>(complexSize[col]);
                uint8_t rowLenSize = (dataSize <= 0xFF) ? 1 : (dataSize <= 0xFFFF) ? 2 : 4;
                *current = rowLenSize; current += 1;
                memcpy(current, &dataSize, rowLenSize); current += rowLenSize;
                if (dataSize > 0) {
                    memcpy(current, complexData[col], dataSize);
                    current += dataSize;
                }
            } else {
                auto typeId = keyTypeIds[col];
                switch (typeId) {
                    case type::OMNI_BYTE: case type::OMNI_BOOLEAN: {
                        auto val = decoded.GetValue<int8_t>(rowIdx);
                        memcpy(current, &val, sizeof(int8_t)); current += sizeof(int8_t); break;
                    }
                    case type::OMNI_SHORT: {
                        auto val = decoded.GetValue<int16_t>(rowIdx);
                        memcpy(current, &val, sizeof(int16_t)); current += sizeof(int16_t); break;
                    }
                    case type::OMNI_INT: case type::OMNI_DATE32: case type::OMNI_TIME32: {
                        auto val = decoded.GetValue<int32_t>(rowIdx);
                        memcpy(current, &val, sizeof(int32_t)); current += sizeof(int32_t); break;
                    }
                    case type::OMNI_LONG: case type::OMNI_TIMESTAMP: case type::OMNI_DECIMAL64:
                    case type::OMNI_DATE64: case type::OMNI_TIME64: {
                        auto val = decoded.GetValue<int64_t>(rowIdx);
                        memcpy(current, &val, sizeof(int64_t)); current += sizeof(int64_t); break;
                    }
                    case type::OMNI_DOUBLE: {
                        auto val = decoded.GetValue<double>(rowIdx);
                        memcpy(current, &val, sizeof(double)); current += sizeof(double); break;
                    }
                    case type::OMNI_FLOAT: {
                        auto val = decoded.GetValue<float>(rowIdx);
                        memcpy(current, &val, sizeof(float)); current += sizeof(float); break;
                    }
                    case type::OMNI_DECIMAL128: {
                        auto val = decoded.GetValue<Decimal128>(rowIdx);
                        memcpy(current, &val, sizeof(Decimal128)); current += sizeof(Decimal128); break;
                    }
                    default: break;
                }
            }
        }

        // 写 nullBits (rowData + keySerializedSize)
        memset(rowData + keySerializedSize, 0, numNullBytes);
        for (int32_t col = 0; col < groupColNum; ++col) {
            auto &decoded = serialize->decodedCols[col];
            if (decoded.IsNull(rowIdx)) {
                util::NullBits::SetNull(rowData + keySerializedSize, col);
            }
        }

        // AggState
        if (totalAggStatesSize > 0) {
            memcpy(rowData + stateOffset, rowStates[rowIdx], totalAggStatesSize);
        }

        output->SetArenaRow(rowIdx, rowData, keyLength, stateOffset, totalLength);
    }
    }  // 非 merged 路径

    VectorHelper::FreeVecBatch(inputVecBatch);
    return output.release();
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
                    auto key = static_cast<int64_t>(std::stoi(keyRef.ToString()));
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
            executionContext->GetArena()->Reset();
            ResetHashmap();
            if (UNLIKELY(result != ErrorCode::SUCCESS)) {
                throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
            }
        }

        spilledBytes = spiller->GetSpilledBytes();
        auto spillFiles = spiller->FinishSpill();
        UpdateSpillFileInfo(spillFiles.size());
        spillMerger = spiller->CreateSpillMerger(spillFiles, spiller->isSpillCompressEnable(), serialize != nullptr);
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

/// 按 canonical 自描述格式计算复杂类型(ARRAY/ROW)序列化字节总长，并推进 pos。
/// ARRAY = [sizeLenSize(1B)][元素数(sizeLenSize 字节)][各元素...]
/// ROW   = [countLenSize(1B)][字段数(countLenSize 字节)][各字段...]
/// 复杂值为 null 时序列化为单字节 0（1 字节）。元素/字段按各自类型递归
/// （定长=裸字节，VARCHAR=[rls][len][data]，复杂=递归），格式与
/// ArrayVectorSerializer/RowVectorSerializer 一致。
static size_t ComputeComplexSerializedSize(const char *&pos, const type::DataTypePtr &dataType)
{
    switch (dataType->GetId()) {
        case type::OMNI_ARRAY: {
            const uint8_t sizeLenSize = *reinterpret_cast<const uint8_t *>(pos);
            pos += sizeof(uint8_t);
            if (sizeLenSize == 0) {
                return sizeof(uint8_t);
            }
            uint64_t count = 0;
            memcpy(&count, pos, sizeLenSize);
            pos += sizeLenSize;
            auto arrayType = std::dynamic_pointer_cast<type::ArrayType>(dataType);
            const auto &elementType = arrayType->ElementType();
            size_t total = sizeof(uint8_t) + sizeLenSize;
            for (uint64_t i = 0; i < count; ++i) {
                total += ComputeComplexSerializedSize(pos, elementType);
            }
            return total;
        }
        case type::OMNI_ROW: {
            const uint8_t countLenSize = *reinterpret_cast<const uint8_t *>(pos);
            pos += sizeof(uint8_t);
            if (countLenSize == 0) {
                return sizeof(uint8_t);
            }
            uint64_t childCount = 0;
            memcpy(&childCount, pos, countLenSize);
            pos += countLenSize;
            auto rowType = std::dynamic_pointer_cast<type::RowType>(dataType);
            size_t total = sizeof(uint8_t) + countLenSize;
            for (uint64_t i = 0; i < childCount; ++i) {
                total += ComputeComplexSerializedSize(pos, rowType->Type(static_cast<int32_t>(i)));
            }
            return total;
        }
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
        case type::OMNI_VARBINARY: {
            const uint8_t rowLenSize = *reinterpret_cast<const uint8_t *>(pos);
            pos += sizeof(uint8_t);
            if (rowLenSize == 0) {
                return sizeof(uint8_t);
            }
            size_t stringLen = 0;
            memcpy(&stringLen, pos, rowLenSize);
            pos += rowLenSize + stringLen;
            return sizeof(uint8_t) + rowLenSize + stringLen;
        }
        case type::OMNI_DECIMAL128: {
            pos += 16;
            return 16;
        }
        default: {
            const size_t colSize = OperatorUtil::GetTypeSize(dataType);
            pos += colSize;
            return colSize;
        }
    }
}

int32_t HashAggregationOperator::OutputMixedFromDisk(VectorBatch **outputVecBatch)
{
    if (spillMerger == nullptr) {
        if (GetElementsSize() > 0) {
            auto result = SpillHashMap();
            executionContext->GetArena()->Reset();
            ResetHashmap();
            if (UNLIKELY(result != ErrorCode::SUCCESS)) {
                throw omniruntime::exception::OmniException(GetErrorCode(result), GetErrorMessage(result));
            }
        }
        spilledBytes = spiller->GetSpilledBytes();
        auto spillFiles = spiller->FinishSpill();
        UpdateSpillFileInfo(spillFiles.size());
        spillMerger = spiller->CreateSpillMerger(spillFiles, spiller->isSpillCompressEnable(), serialize != nullptr);
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
    if (rowCount <= 0) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
        return 0;
    }

    // 归并 + 合并（参考 GetOutputFromDiskWithAgg，但收集 key 字节流而非解析到列存向量）
    auto aggNum = static_cast<int32_t>(aggregators.size());
    auto groupStatesPtr = groupStates.get();
    rowStates.resize(rowCount);

    std::vector<UnspillRowInfo> unspillRows(UNSPILL_ROW_COUNT_ONE_BATCH);
    std::vector<AggregateState *> newGroupStates;
    std::vector<std::string> groupKeys;
    int32_t offset = 0;
    int32_t rowIdx = 0;

    bool isEqual = false;
    bool nextKeyIsNew = true;
    AggregateState *currentGroupStates = nullptr;

    auto flushAndBuild = [&]() {
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
    };

    for (;;) {
        auto currentVecBatch = spillMerger->CurrentBatchWithEqual(isEqual);
        if (currentVecBatch == nullptr) {
            flushAndBuild();
            if (rowIdx > 0) {
                groupKeys.resize(rowIdx);
                std::vector<AggregateState *> states(rowIdx);
                for (int32_t i = 0; i < rowIdx; ++i) {
                    states[i] = rowStates[i];
                }
                *outputVecBatch = BuildMixedBatchFromDiskGroups(rowIdx, groupKeys, states);
                UpdateGetOutputInfo(rowIdx);
                if (spillTotalRowCount == spillRowOffset) {
                    SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
                }
                return 1;
            }
            SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
            return 0;
        }

        bool isLastRow = false;
        auto currentRowIndex = spillMerger->CurrentRowIndex(isLastRow);
        if (nextKeyIsNew) {
            auto keyIndex = serialize != nullptr ? 1 : 0;
            auto keyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(currentVecBatch->Get(keyIndex));
            auto key = keyVector->GetValue(currentRowIndex);
            groupKeys.push_back(std::string(key.data(), key.size()));

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
            flushAndBuild();
            offset = 0;
        }

        nextKeyIsNew = !isEqual;
        spillMerger->Pop();
        spillRowOffset++;
        if (nextKeyIsNew && rowIdx >= rowCount) {
            flushAndBuild();
            groupKeys.resize(rowIdx);
            std::vector<AggregateState *> states(rowIdx);
            for (int32_t i = 0; i < rowIdx; ++i) {
                states[i] = rowStates[i];
            }
            *outputVecBatch = BuildMixedBatchFromDiskGroups(rowIdx, groupKeys, states);
            UpdateGetOutputInfo(rowIdx);
            return 1;
        }
    }
}

VectorBatch *HashAggregationOperator::BuildMixedBatchFromDiskGroups(int32_t rowCount,
    const std::vector<std::string> &groupKeys,
    const std::vector<AggregateState *> &groupStates)
{
    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    int32_t numNullBytes = util::NullBits::NumBytes(groupColNum);
    auto aggStateOffset = serialize->AggStateOffset();
    bool hasMergedVarchar = (serialize->varcharColIndices.size() > 1);
    bool hasComplexTypes = false;

    std::vector<DataTypeId> keyTypeIds(groupColNum);
    std::vector<bool> isVarcharCol(groupColNum, false);
    std::vector<bool> isComplexCol(groupColNum, false);
    std::vector<int32_t> colOffsets(groupColNum, 0);
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto typeId = groupByCols[i].input->GetId();
        keyTypeIds[i] = typeId;
        colOffsets[i] = serialize->aggRows->ColumnAt(i).Offset();
        if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
            isVarcharCol[i] = true;
        } else if (typeId == type::OMNI_ARRAY || typeId == type::OMNI_ROW) {
            isComplexCol[i] = true;
            hasComplexTypes = true;
        }
    }

    int32_t sumFixedKeySizes = 0;
    for (int32_t i = 0; i < groupColNum; ++i) {
        if (!isVarcharCol[i] && !isComplexCol[i]) {
            sumFixedKeySizes += OperatorUtil::GetTypeSize(groupByCols[i].input);
        }
    }

    auto output = std::make_unique<MixedVectorBatch>(rowCount, keyTypeIds);
    output->SetMode(COMPLETE_ROW_ONLY);
    output->PrepareRowArena(static_cast<int64_t>(rowCount) * (totalAggStatesSize + 256));

    struct ParsedCol { bool isNull; const char *data; size_t dataLen; };

    if (hasMergedVarchar && !hasComplexTypes) {
        // merged varchar 布局：[fixed 区][varchar 块]，与 OutputMixed 一致
        int32_t fixRowSize = serialize->aggRows->FixedRowSize();
        int32_t varcharSlotOffset = serialize->aggRows->ColumnAt(serialize->varcharSlotColIdx).Offset();
        output->SetVarcharSlotOffset(varcharSlotOffset);
        int32_t keyLength = aggStateOffset - numNullBytes;

        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
            const auto &key = groupKeys[rowIdx];
            const char *pos = key.data();
            int32_t varcharTotalSize = 0;
            std::vector<ParsedCol> parsed(groupColNum);

            for (int32_t col = 0; col < groupColNum; ++col) {
                uint8_t firstByte = static_cast<uint8_t>(*pos);
                if (firstByte == 0) {
                    parsed[col] = {true, nullptr, 0};
                    pos += 1;
                } else if (isVarcharCol[col]) {
                    uint8_t rowLenSize = firstByte;
                    pos += 1;
                    size_t stringLen = 0;
                    memcpy(&stringLen, pos, rowLenSize);
                    pos += rowLenSize;
                    parsed[col] = {false, pos - rowLenSize - 1, sizeof(uint8_t) + rowLenSize + stringLen};
                    pos += stringLen;
                } else {
                    int32_t colSize = firstByte;
                    pos += 1;
                    parsed[col] = {false, pos, static_cast<size_t>(colSize)};
                    pos += colSize;
                }
                if (isVarcharCol[col]) {
                    varcharTotalSize += parsed[col].isNull ? 1 : static_cast<int32_t>(parsed[col].dataLen);
                }
            }

            int32_t length = fixRowSize + varcharTotalSize;
            auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(length));
            memset(rowData, 0, fixRowSize);

            for (int32_t col = 0; col < groupColNum; ++col) {
                if (isVarcharCol[col]) {
                    continue;
                }
                if (parsed[col].isNull) {
                    util::NullBits::SetNull(rowData + keyLength, col);
                } else {
                    memcpy(rowData + colOffsets[col], parsed[col].data, parsed[col].dataLen);
                }
            }

            if (totalAggStatesSize > 0) {
                memcpy(rowData + aggStateOffset, groupStates[rowIdx], totalAggStatesSize);
            }

            int32_t varcharOffset = fixRowSize;
            for (int32_t col = 0; col < groupColNum; ++col) {
                if (!isVarcharCol[col]) {
                    continue;
                }
                if (parsed[col].isNull) {
                    rowData[varcharOffset++] = 0;
                } else {
                    memcpy(rowData + varcharOffset, parsed[col].data, parsed[col].dataLen);
                    varcharOffset += static_cast<int32_t>(parsed[col].dataLen);
                }
            }

            // varchar slot header: [varcharTotalSize][dataOffset]，与 OutputMixed 一致
            *reinterpret_cast<int32_t *>(rowData + varcharSlotOffset) = varcharTotalSize;
            *reinterpret_cast<int32_t *>(rowData + varcharSlotOffset + sizeof(int32_t)) = fixRowSize;

            output->SetArenaRow(rowIdx, rowData, keyLength, aggStateOffset, length);
        }
    } else {
        // non-merged 布局（含复杂类型）：[key data][null bits][AggState]，与 OutputMixed 一致
        output->SetVarcharSlotOffset(-1);

        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
            const auto &key = groupKeys[rowIdx];
            const char *pos = key.data();
            int32_t keySerializedSize = 0;
            std::vector<ParsedCol> parsed(groupColNum);

            for (int32_t col = 0; col < groupColNum; ++col) {
                uint8_t firstByte = static_cast<uint8_t>(*pos);
                if (firstByte == 0) {
                    parsed[col] = {true, nullptr, 0};
                    pos += 1;
                } else if (isVarcharCol[col]) {
                    uint8_t rowLenSize = firstByte;
                    pos += 1;
                    size_t stringLen = 0;
                    memcpy(&stringLen, pos, rowLenSize);
                    pos += rowLenSize;
                    parsed[col] = {false, pos - rowLenSize - 1, sizeof(uint8_t) + rowLenSize + stringLen};
                    pos += stringLen;
                    keySerializedSize += static_cast<int32_t>(parsed[col].dataLen);
                } else if (isComplexCol[col]) {
                    // 复杂列 spill key 为 canonical 自描述字节，需尺寸遍历确定边界
                    const char *dataPtr = pos;
                    size_t dataSize = ComputeComplexSerializedSize(pos, groupByCols[col].input);
                    uint8_t rowLenSize = (dataSize <= 0xFF) ? 1 : (dataSize <= 0xFFFF) ? 2 : 4;
                    parsed[col] = {false, dataPtr, dataSize};
                    keySerializedSize += static_cast<int32_t>(sizeof(uint8_t) + rowLenSize + dataSize);
                } else {
                    int32_t colSize = firstByte;
                    pos += 1;
                    parsed[col] = {false, pos, static_cast<size_t>(colSize)};
                    pos += colSize;
                    keySerializedSize += colSize;
                }
            }

            // P2 fast path: all fixed-width, no nulls — aligned stateOffset (与 OutputMixed P2 一致)
            bool canUseP2 = !hasComplexTypes && serialize->varcharColIndices.empty() &&
                sumFixedKeySizes >= static_cast<int32_t>(sizeof(void*)) &&
                keySerializedSize == sumFixedKeySizes;
            if (canUseP2) {
                bool hasNulls = false;
                for (int32_t col = 0; col < groupColNum; ++col) {
                    if (parsed[col].isNull) { hasNulls = true; break; }
                }
                if (!hasNulls) {
                    int32_t keyLength = sumFixedKeySizes;
                    int32_t stateOffset = (sumFixedKeySizes + numNullBytes + 7) & ~7;
                    int32_t totalLength = (stateOffset + totalAggStatesSize + 7) & ~7;
                    auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(totalLength));
                    memset(rowData, 0, totalLength);
                    int32_t keyOffset = 0;
                    for (int32_t col = 0; col < groupColNum; ++col) {
                        memcpy(rowData + keyOffset, parsed[col].data, parsed[col].dataLen);
                        keyOffset += static_cast<int32_t>(parsed[col].dataLen);
                    }
                    if (totalAggStatesSize > 0) {
                        memcpy(rowData + stateOffset, groupStates[rowIdx], totalAggStatesSize);
                    }
                    output->SetArenaRow(rowIdx, rowData, keyLength, stateOffset, totalLength);
                    continue;
                }
            }

            int32_t keyLength = keySerializedSize;
            int32_t stateOffset = keySerializedSize + numNullBytes;
            int32_t totalLength = (stateOffset + totalAggStatesSize + 7) & ~7;

            auto *rowData = static_cast<uint8_t *>(output->GetRowArena()->Allocate(totalLength));
            memset(rowData, 0, totalLength);

            int32_t keyOffset = 0;
            for (int32_t col = 0; col < groupColNum; ++col) {
                const auto &parsedCol = parsed[col];
                if (parsedCol.isNull) {
                    util::NullBits::SetNull(rowData + keySerializedSize, col);
                    continue;
                }
                if (isComplexCol[col]) {
                    // 复杂列：写 [rls][size][data]（与 OutputMixed 一致）
                    size_t dataSize = parsedCol.dataLen;
                    uint8_t rowLenSize = (dataSize <= 0xFF) ? 1 : (dataSize <= 0xFFFF) ? 2 : 4;
                    rowData[keyOffset++] = rowLenSize;
                    if (rowLenSize == 1) {
                        rowData[keyOffset++] = static_cast<uint8_t>(dataSize);
                    } else if (rowLenSize == 2) {
                        int16_t sz = static_cast<int16_t>(dataSize);
                        memcpy(rowData + keyOffset, &sz, sizeof(sz));
                        keyOffset += 2;
                    } else {
                        int32_t sz = static_cast<int32_t>(dataSize);
                        memcpy(rowData + keyOffset, &sz, sizeof(sz));
                        keyOffset += 4;
                    }
                    memcpy(rowData + keyOffset, parsedCol.data, dataSize);
                    keyOffset += static_cast<int32_t>(dataSize);
                } else {
                    memcpy(rowData + keyOffset, parsedCol.data, parsedCol.dataLen);
                    keyOffset += static_cast<int32_t>(parsedCol.dataLen);
                }
            }

            if (totalAggStatesSize > 0) {
                memcpy(rowData + stateOffset, groupStates[rowIdx], totalAggStatesSize);
            }
            output->SetArenaRow(rowIdx, rowData, keyLength, stateOffset, totalLength);
        }
    }

    return output.release();
}

void HashAggregationOperator::CalcAndSetStatesSize()
{
    totalAggStatesSize = 0;
    for (auto &agg: aggregators) {
        agg->SetStateOffset(totalAggStatesSize);
        totalAggStatesSize += agg->GetStateSize();
    }
}

#ifdef OMNI_SVEHT32_HASH_AGG
int32_t HashAggregationOperator::OutputFixedInt32SveAos(VectorBatch **outputVecBatch)
{
    usedMemBytes = executionContext->GetArena()->UsedBytes();
    totalMemBytes = executionContext->GetArena()->TotalBytes();

    const bool hasAgg = !aggregators.empty();
    const int32_t totalRowCount = static_cast<int32_t>(fixedInt32SveAos->GetElementsSize()) +
                                  (hasNullGroupState32 ? 1 : 0);
    if (totalRowCount == 0) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
        return 0;
    }

    const int32_t remainingRows = totalRowCount - static_cast<int32_t>(outputState.hasBeenOutputNum);
    const int32_t curRowCount = std::min(rowsPerBatch, remainingRows);
    auto output = std::make_unique<VectorBatch>(curRowCount);
    auto outputPtr = output.get();
    SetVectors(outputPtr, outputTypes, curRowCount);

    auto *groupVector = outputPtr->Get(0);
    std::vector<uint32_t> keys(curRowCount);
    std::vector<uint32_t> handles(curRowCount);
    std::vector<AggregateState *> states;
    if (hasAgg) {
        states.resize(curRowCount);
    }

    uint64_t nextSlot = outputState.outputHashmapPos;
    int32_t rowIdx = fixedInt32SveAos->CopyGroups(outputState.outputHashmapPos, curRowCount, keys.data(),
        handles.data(), nextSlot);
    for (int32_t i = 0; i < rowIdx; ++i) {
        reinterpret_cast<Vector<int32_t> *>(groupVector)->SetValue(i, static_cast<int32_t>(keys[i]));
        if (hasAgg) {
            states[i] = fixedInt32SveAosStates[handles[i]];
        }
    }
    outputState.outputHashmapPos = static_cast<uint32_t>(nextSlot);

    if (rowIdx < curRowCount && hasNullGroupState32 && !sveAosNullGroupOutput) {
        groupVector->SetNull(rowIdx);
        if (hasAgg) {
            states[rowIdx] = nullGroupState32;
        }
        sveAosNullGroupOutput = true;
        ++rowIdx;
    }

    if (hasAgg) {
        auto aggOutputStartIndex = 1;
        for (size_t aggIndex = 0; aggIndex < aggregators.size(); ++aggIndex) {
            auto &aggregator = aggregators[aggIndex];
            const auto oneAggOutputCols = aggOutputTypes[aggIndex].GetSize();
            std::vector<BaseVector *> adaptAggVectors(oneAggOutputCols);
            for (auto j = 0; j < oneAggOutputCols; j++) {
                adaptAggVectors[j] = outputPtr->Get(aggOutputStartIndex + j);
            }
            aggOutputStartIndex += oneAggOutputCols;
            aggregator->ExtractValuesBatch(states, adaptAggVectors, 0, rowIdx);
        }
    }

    outputState.hasBeenOutputNum += rowIdx;
    *outputVecBatch = output.release();
    UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
    if (static_cast<int32_t>(outputState.hasBeenOutputNum) == totalRowCount) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
    }
    return 1;
}

int32_t HashAggregationOperator::OutputFixedInt32PairSveAos(VectorBatch **outputVecBatch)
{
    usedMemBytes = executionContext->GetArena()->UsedBytes();
    totalMemBytes = executionContext->GetArena()->TotalBytes();

    const bool hasAgg = !aggregators.empty();
    const int32_t totalRowCount = static_cast<int32_t>(fixedInt32PairSveAos->GetElementsSize());
    if (totalRowCount == 0) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
        return 0;
    }

    const int32_t remainingRows = totalRowCount - static_cast<int32_t>(outputState.hasBeenOutputNum);
    const int32_t curRowCount = std::min(rowsPerBatch, remainingRows);
    auto output = std::make_unique<VectorBatch>(curRowCount);
    auto outputPtr = output.get();
    SetVectors(outputPtr, outputTypes, curRowCount);

    auto *groupVector0 = outputPtr->Get(0);
    auto *groupVector1 = outputPtr->Get(1);
    std::vector<uint32_t> keys0(curRowCount);
    std::vector<uint32_t> keys1(curRowCount);
    std::vector<uint32_t> nullMasks(curRowCount);
    std::vector<uint32_t> handles(curRowCount);
    std::vector<AggregateState *> states;
    if (hasAgg) {
        states.resize(curRowCount);
    }

    uint64_t nextSlot = outputState.outputHashmapPos;
    int32_t rowIdx = fixedInt32PairSveAos->CopyGroups(outputState.outputHashmapPos, curRowCount, keys0.data(),
        keys1.data(), nullMasks.data(), handles.data(), nextSlot);
    for (int32_t i = 0; i < rowIdx; ++i) {
        if ((nullMasks[i] & hashmap::SveAggAosHashTable32Pair::kKey0Null) != 0) {
            groupVector0->SetNull(i);
        } else {
            reinterpret_cast<Vector<int32_t> *>(groupVector0)->SetValue(i, static_cast<int32_t>(keys0[i]));
        }
        if ((nullMasks[i] & hashmap::SveAggAosHashTable32Pair::kKey1Null) != 0) {
            groupVector1->SetNull(i);
        } else {
            reinterpret_cast<Vector<int32_t> *>(groupVector1)->SetValue(i, static_cast<int32_t>(keys1[i]));
        }
        if (hasAgg) {
            states[i] = fixedInt32PairSveAosStates[handles[i]];
        }
    }
    outputState.outputHashmapPos = static_cast<uint32_t>(nextSlot);

    if (hasAgg) {
        auto aggOutputStartIndex = 2;
        for (size_t aggIndex = 0; aggIndex < aggregators.size(); ++aggIndex) {
            auto &aggregator = aggregators[aggIndex];
            const auto oneAggOutputCols = aggOutputTypes[aggIndex].GetSize();
            std::vector<BaseVector *> adaptAggVectors(oneAggOutputCols);
            for (auto j = 0; j < oneAggOutputCols; j++) {
                adaptAggVectors[j] = outputPtr->Get(aggOutputStartIndex + j);
            }
            aggOutputStartIndex += oneAggOutputCols;
            aggregator->ExtractValuesBatch(states, adaptAggVectors, 0, rowIdx);
        }
    }

    outputState.hasBeenOutputNum += rowIdx;
    *outputVecBatch = output.release();
    UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
    if (static_cast<int32_t>(outputState.hasBeenOutputNum) == totalRowCount) {
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
    }
    return 1;
}
#endif

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
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
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
        SetStatus(OmniStatus::OMNI_STATUS_FINISHED);
    }
    return 1;
}

ALWAYS_INLINE size_t HashAggregationOperator::GetElementsSize()
{
    size_t elementSize = 0;
    if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKeyWithoutAgg != nullptr) {
        elementSize = normalizeKeyWithoutAgg->GetElementsSize();
    } else if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKey != nullptr) {
        elementSize = normalizeKey->GetElementsSize();
#ifdef OMNI_SVEHT32_HASH_AGG
    } else if (fixedInt32PairSveAos != nullptr && !svePairFallbackToSerialize) {
        elementSize = fixedInt32PairSveAos->GetElementsSize();
    } else if (fixedInt32SveAos != nullptr && !sveAosFallbackToFixedInt32) {
        elementSize = fixedInt32SveAos->GetElementsSize() + (hasNullGroupState32 ? 1 : 0);
#endif
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

ALWAYS_INLINE void HashAggregationOperator::ResetHashmap()
{
    if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKeyWithoutAgg != nullptr) {
        normalizeKeyWithoutAgg->ResetHashmap();
    } else if (groupByColumnsHandleType == HandleType::NormalizeKey && normalizeKey != nullptr) {
        normalizeKey->ResetHashmap();
#ifdef OMNI_SVEHT32_HASH_AGG
    } else if (fixedInt32PairSveAos != nullptr && !svePairFallbackToSerialize) {
        fixedInt32PairSveAos->Reset();
        fixedInt32PairSveAosStates.clear();
    } else if (fixedInt32SveAos != nullptr && !sveAosFallbackToFixedInt32) {
        fixedInt32SveAos->Reset();
        fixedInt32SveAosStates.clear();
        nullGroupState32 = nullptr;
        hasNullGroupState32 = false;
        sveAosNullGroupOutput = false;
#endif
    } else if (serialize != nullptr) {
        serialize->ResetHashmap();
    } else if (fixedInt32 != nullptr) {
        fixedInt32->ResetHashmap();
    } else if (fixedInt64 != nullptr) {
        fixedInt64->ResetHashmap();
    } else if (fixedInt16 != nullptr) {
        fixedInt16->ResetHashmap();
    } else if (packedInt32 != nullptr) {
        packedInt32->ResetHashmap();
    } else if (packedInt64 != nullptr) {
        packedInt64->ResetHashmap();
    } else if (packedInt128 != nullptr) {
        packedInt128->ResetHashmap();
    }
}
} // end of namespace op
} // end of namespace omniruntime
