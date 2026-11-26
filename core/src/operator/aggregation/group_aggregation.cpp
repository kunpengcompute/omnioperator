/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: Hash Aggregation Source File
 */

#include "group_aggregation.h"
#include <cmath>
#include "vector/vector_helper.h"
#include "operator/status.h"
#include "operator/util/operator_util.h"
#include "util/type_util.h"
#include "util/debug.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"

#if defined(DEBUG_OPERATOR) && defined(TRACE)
#include <sstream>
#endif
namespace omniruntime {
namespace op {
using namespace omniruntime::type;

static constexpr int32_t UNSPILL_ROW_COUNT_ONE_BATCH = 128;

using SetVector = void (*)(VectorBatch *vecBatch, int32_t rowCount);
template <typename V> void SetVectorImpl(VectorBatch *vecBatch, int32_t rowCount) { vecBatch->Append(new V(rowCount)); }

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
    std::vector<DataTypePtr> dataTypes{DoubleType(), LongType()};
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
    nullptr,
    nullptr,
};

OmniStatus HashAggregationOperatorFactory::Init()
{
    for (auto groupByCol : groupByColsVector) {
        groupByColIndices.push_back(groupByCol);
    }
    for (auto aggInputColsVector : aggsInputColsVector) {
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

OmniStatus HashAggregationOperatorFactory::Close() { return OMNI_STATUS_NORMAL; }

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

    auto groupByOperator =
        new HashAggregationOperator(groupByIndex, aggsInputCols, aggInputColsSize, aggInputTypes, aggOutputTypes,
                                    std::move(aggs), inputRaws, outputPartials, hasAggFilters, operatorConfig);
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
    handleType = HandleType::serialize;
}

void HashAggregationOperator::SetGroupByColumnsHandleType(HandleType t) { groupByColumnsHandleType = t; }

OmniStatus HashAggregationOperator::Init()
{
    // 1. avoid init more than once
    if (isInited) {
        return OMNI_STATUS_NORMAL;
    }
    isInited = true;

    // 2. set op name for metrics
    SetOperatorName(metricsNameHashAgg);

    // 3. check group by handle methcd
    // put at beginning so that we do not allocate memory if there is error
    if (groupByColumnsHandleType == HandleType::serialize) {
        serialize = std::make_unique<decltype(serialize)::element_type>();
        serialize->InitSize(groupByCols.size());
    } else if (groupByColumnsHandleType == HandleType::fixedInt32) {
        fixedInt32 = std::make_unique<decltype(fixedInt32)::element_type>();
    } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
        fixedInt64 = std::make_unique<decltype(fixedInt64)::element_type>();
    } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
        fixedInt16 = std::make_unique<decltype(fixedInt16)::element_type>();
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
    for (const auto &c : groupByCols) {
        sourceTypes[c.idx] = static_cast<int32_t>(c.input->GetId());
        memoryChunkSize += OperatorUtil::GetTypeSize(c.input);
    }

    // agg source types
    for (size_t i = 0; i < aggInputCols.size(); ++i) {
        for (size_t j = 0; j < aggInputCols[i].size(); ++j) {
            sourceTypes[aggInputCols[i][j]] = aggInputTypes[i].GetType(j)->GetId();
        }
    }

    for (auto &aggregator : aggregators) {
        const std::vector<DataTypePtr> &aggTypes = aggregator->GetOutputTypes().Get();
        for (auto dataType : aggTypes) {
            memoryChunkSize += OperatorUtil::GetTypeSize(dataType);
        }
    }
    memoryChunkSize += static_cast<int64_t>(aggregators.size() * sizeof(AggregateState));
    executionContext->GetArena()->SetMinChunkSize(memoryChunkSize * 8);

    // 5 init max row when getoutput
    int32_t rowByteSize = InitMaxRowCountAndOutputTypes();
    rowsPerBatch = OperatorUtil::GetMaxRowCount(rowByteSize);

    // 6 calculate every aggregator's size and set offset of aggregator
    CalcAndSetStatesSize();

    // 7 vector analyzer
    vectorAnalyzer = new VectorAnalyzer(groupByCols);

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
        res1 = memcpy_sp(newSlots + 1 + offSet, (newTableSize - 1) * sizeof(void *), oldSlots + 1,
                         (oldTableSize - 1) * sizeof(void *));
    }
    errno_t res2 = memcpy_sp(newAssigned + 1 + offSet, (newTableSize - 1) * sizeof(bool), oldAssigned + 1,
                             (oldTableSize - 1) * sizeof(bool));
    newArrayTable->AddElementsSize(arrayTable->GetElementsSize());
    if (UNLIKELY(res1 != EOK || res2 != EOK)) {
        std::string omniExceptionInfo = "In adjust hashagg array table, memcpy failed, ret1 is：" +
                                        std::to_string(res1) + " , ret2 is：" + std::to_string(res2) +
                                        ", reason is: " + std::string(strerror(errno));
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", omniExceptionInfo);
    }
    arrayTable.reset(newArrayTable.release());
    resizeArrayMapCnt++;
}

void HashAggregationOperator::MoveEntryArrayTableToHashMap(int64_t minValue)
{
    bool hasAgg = aggregators.size() > 0;
    arrayTable->ForEachValue([&](const auto &value, const auto &index) {
        if (index != 0) {
            if (groupByColumnsHandleType == HandleType::fixedInt32) {
                auto ret = fixedInt32->InsertOneValueToHashmap<false>(static_cast<int32_t>(index + minValue - 1));
                if (hasAgg) {
                    ret.SetValue(reinterpret_cast<AggregateState *>(value));
                }
            } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
                auto ret = fixedInt64->InsertOneValueToHashmap<false>(static_cast<int64_t>(index + minValue - 1));
                if (hasAgg) {
                    ret.SetValue(reinterpret_cast<AggregateState *>(value));
                }
            } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
                auto ret = fixedInt16->InsertOneValueToHashmap<false>(static_cast<int16_t>(index + minValue - 1));
                if (hasAgg) {
                    ret.SetValue(reinterpret_cast<AggregateState *>(value));
                }
            }
            return;
        }
        if (groupByColumnsHandleType == HandleType::fixedInt32) {
            auto ret = fixedInt32->InsertOneValueToHashmap<true>(0);
            if (hasAgg) {
                ret.SetValue(reinterpret_cast<AggregateState *>(value));
            }
        } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
            auto ret = fixedInt64->InsertOneValueToHashmap<true>(0);
            if (hasAgg) {
                ret.SetValue(reinterpret_cast<AggregateState *>(value));
            }
        } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
            auto ret = fixedInt16->InsertOneValueToHashmap<true>(0);
            if (hasAgg) {
                ret.SetValue(reinterpret_cast<AggregateState *>(value));
            }
        }
    });
    arrayTable.reset();
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
                serialize->PushBackSerializer(dicVectorSerializerCenter[omniId]);
            } else {
                serialize->PushBackSerializer(vectorSerializerCenter[omniId]);
            }
            serialize->PushBackDeSerializer(vectorDeSerializerCenter[omniId]);
        }
        groupVectors[i] = curVector;
    }

    if (LIKELY(groupByColumnsHandleType == HandleType::serialize)) {
        Emplace(serialize, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::fixedInt32) {
        Emplace(fixedInt32, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::fixedInt64) {
        Emplace(fixedInt64, vecBatch, groupVectors, groupColNum);
    } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
        Emplace(fixedInt16, vecBatch, groupVectors, groupColNum);
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

/**
 * @param types
 * @return rowSize
 * All the output data types are determined in this function. Following allocation for output vectors and filling
 * value should use the 'types' parameter instead of using input vector types.
 */
int32_t HashAggregationOperator::InitMaxRowCountAndOutputTypes()
{
    int32_t rowSize = 0;
    for (auto &i : groupByCols) {
        outputTypes.push_back(i.input);
        rowSize += OperatorUtil::GetTypeSize(i.input);
    }
    for (auto &aggregator : aggregators) {
        const std::vector<DataTypePtr> &aggTypes = aggregator->GetOutputTypes().Get();
        for (auto dataType : aggTypes) {
            outputTypes.push_back(dataType);
            rowSize += OperatorUtil::GetTypeSize(dataType);
        }
    }
    return rowSize;
}

void HashAggregationOperator::InitSpillInfos()
{
    spillTypes.push_back(VarcharType());
    for (auto &aggregator : aggregators) {
        auto currentSpillType = aggregator->GetSpillType();
        aggTypes.insert(aggTypes.end(), currentSpillType.begin(), currentSpillType.end());
        spillTypes.insert(spillTypes.end(), currentSpillType.begin(), currentSpillType.end());
    }
    SortOrder sortOrder;
    sortOrders.resize(1, sortOrder);
    groupByCloIdx.resize(1, 0);
    aggregationSort = std::make_unique<AggregationSort>(aggregators);
}

void HashAggregationOperator::SetVectors(VectorBatch *output, const std::vector<DataTypePtr> &types, int32_t rowCount)
{
    auto colSize = types.size();
    for (size_t colIndex = 0; colIndex < colSize; ++colIndex) {
        const DataTypePtr &type = types[colIndex];
        GROUP_AGG_FUNCTIONS[type->GetId()](output, rowCount);
    }
}

int32_t HashAggregationOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!hasInputedData()) {
        return 0;
    }
    int32_t expectedBatchSize = 0;
    if (LIKELY(groupByColumnsHandleType == HandleType::serialize)) {
        expectedBatchSize = Output(serialize, outputVecBatch);
    } else if (LIKELY(groupByColumnsHandleType == HandleType::fixedInt32)) {
        expectedBatchSize = Output(fixedInt32, outputVecBatch);
    } else if (LIKELY(groupByColumnsHandleType == HandleType::fixedInt64)) {
        expectedBatchSize = Output(fixedInt64, outputVecBatch);
    } else if (groupByColumnsHandleType == HandleType::fixedInt16) {
        expectedBatchSize = Output(fixedInt16, outputVecBatch);
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

template <typename T, typename GroupMap>
void HashAggregationOperator::InsertValueToArrayMap(GroupMap &arrayMap, BaseVector *groupVector, int32_t rowIdx)
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
        uint64x2x4_t vNull = vld4q_u64(reinterpret_cast<uint64_t *>(nulls));
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
        uint32x4x4_t vNull = vld4q_u32(reinterpret_cast<uint32_t *>(nulls));
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
        uint16x8x4_t vNull = vld4q_u16(reinterpret_cast<uint16_t *>(nulls));
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
    int64_t *matchSlotsData = reinterpret_cast<int64_t *>(rowsAggStates.data());
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

template <typename T, bool hasNull>
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

template <typename Serialize>
void HashAggregationOperator::Emplace(Serialize &emplaceKey, VectorBatch *vecBatch, BaseVector **groupVectors,
                                      int32_t groupColNum)
{
    int32_t rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContext->GetArena());
    size_t aggNum = aggregators.size();
    auto *curVector = groupVectors[0];
    bool isDictEncoded = curVector->GetEncoding();

    if (aggNum == 0) {
        // no aggregator, so just perform groupby
        if (isDictEncoded) {
            for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                emplaceKey->InsertDictValueToHashmap(groupVectors, groupColNum, rowIdx, arenaAllocator);
            }
        } else {
            for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                emplaceKey->InsertValueToHashmap(groupVectors, groupColNum, rowIdx, arenaAllocator);
            }
        }
        return;
    }
    // aggNum > 0
    std::vector<AggregateState *> currentRowStates(rowCount);
    AggregateState *currentGroupStates = nullptr;
    std::vector<AggregateState *> newGroupStates;

    if (isDictEncoded) {
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto ret = emplaceKey->InsertDictValueToHashmap(groupVectors, groupColNum, rowIdx, arenaAllocator);
            if (ret.IsInsert()) {
                currentGroupStates = reinterpret_cast<AggregateState *>(arenaAllocator.Allocate(totalAggStatesSize));
                ret.SetValue(currentGroupStates);
                newGroupStates.emplace_back(currentGroupStates);
            } else {
                currentGroupStates = ret.GetValue();
                arenaAllocator.RollBackContinualMem();
            }
            currentRowStates[rowIdx] = currentGroupStates;
        }
    } else {
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto ret = emplaceKey->InsertValueToHashmap(groupVectors, groupColNum, rowIdx, arenaAllocator);
            if (ret.IsInsert()) {
                currentGroupStates = reinterpret_cast<AggregateState *>(arenaAllocator.Allocate(totalAggStatesSize));
                ret.SetValue(currentGroupStates);
                newGroupStates.emplace_back(currentGroupStates);
            } else {
                currentGroupStates = ret.GetValue();
                arenaAllocator.RollBackContinualMem();
            }
            currentRowStates[rowIdx] = currentGroupStates;
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
}

template <typename Deserialize>
void HashAggregationOperator::TraverseHashmapToGetOneResult(Deserialize &deserializeHashmap, VectorBatch *output)
{
    const int32_t expectSize = output->GetRowCount();
    int32_t groupColNum = static_cast<int32_t>(this->groupByCols.size());
    std::vector<BaseVector *> groupOutputVectors(groupColNum);
    for (int32_t i = 0; i < groupColNum; i++) {
        groupOutputVectors[i] = output->Get(i);
    }

    int32_t lambdaRowIndex = 0;
    OutputState curOutputState;
    auto &hashmap = deserializeHashmap->hashmap;
    {
        auto statefulMachine = hashmap.GetOutputMachine(outputState.outputHashmapPos, outputState.hasBeenOutputNum);

        if constexpr (std::remove_reference_t<decltype(deserializeHashmap)>::element_type::HasSpecialNullFunc) {
            curOutputState = statefulMachine.HandleElements(
                expectSize,
                [&](const auto &key, auto &mapped) mutable {
                    deserializeHashmap->ParseKeyToCols(key, groupOutputVectors, groupColNum, lambdaRowIndex);
                    ++lambdaRowIndex;
                },
                [&](const auto &key, auto &mapped) mutable {
                    deserializeHashmap->ParseNull(key, groupOutputVectors, groupColNum, lambdaRowIndex);
                    ++lambdaRowIndex;
                });
        } else {
            curOutputState = statefulMachine.HandleElements(
                expectSize,
                [&](const auto &key, auto &mapped) mutable {
                    deserializeHashmap->ParseKeyToCols(key, groupOutputVectors, groupColNum, lambdaRowIndex);
                    ++lambdaRowIndex;
                },
                [&](const auto &key, auto &mapped) mutable {
                    deserializeHashmap->ParseKeyToCols(key, groupOutputVectors, groupColNum, lambdaRowIndex);
                    ++lambdaRowIndex;
                });
        }
    }

    const size_t aggNum = this->aggregators.size();
    if (aggNum > 0) {
        std::vector<AggregateState *> groupStates(expectSize);
        int32_t lambdaRowIndex = 0;
        {
            auto statefulMachine = hashmap.GetOutputMachine(outputState.outputHashmapPos, outputState.hasBeenOutputNum);
            statefulMachine.HandleElements(
                expectSize,
                [&](const auto &key, auto &mapped) mutable {
                    groupStates[lambdaRowIndex] = mapped;
                    lambdaRowIndex++;
                },
                [&](const auto &key, auto &mapped) mutable {
                    groupStates[lambdaRowIndex] = mapped;
                    lambdaRowIndex++;
                });
        }
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

    outputState.UpdateState(curOutputState);
}

template <bool hasAgg, typename T>
void HashAggregationOperator::TraverseArrayMapGetOutput(BaseVector *groupVector, std::vector<AggregateState *> *states,
                                                        int64_t minValue)
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

template <bool hasAgg>
void HashAggregationOperator::TraverseArrayMap(BaseVector *groupVector, std::vector<AggregateState *> *states)
{
    int32_t lambdaRowIndex = 0;
    auto typeId = groupVector->GetTypeId();
    auto minValue = vectorAnalyzer->MinValue();
    int32_t outputRows = groupVector->GetSize();
    switch (typeId) {
        case OMNI_INT:
        case OMNI_DATE32: TraverseArrayMapGetOutput<hasAgg, int32_t>(groupVector, states, minValue); break;
        case OMNI_SHORT: TraverseArrayMapGetOutput<hasAgg, int16_t>(groupVector, states, minValue); break;
        case OMNI_BYTE: TraverseArrayMapGetOutput<hasAgg, int8_t>(groupVector, states, minValue); break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: TraverseArrayMapGetOutput<hasAgg, int64_t>(groupVector, states, minValue); break;
        default:
            std::string omniExceptionInfo = std::to_string(typeId) + "should not call TraverseArrayMapToGetOneResult";
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
        totalSpillCount = serialize->hashmap.GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        totalSpillCount = fixedInt32->hashmap.GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        totalSpillCount = fixedInt64->hashmap.GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        totalSpillCount = fixedInt16->hashmap.GetElementsSize();
    }
    aggregationSort->ResizeKvVector(totalSpillCount);

    auto aggregationSortPtr = aggregationSort.get();
    size_t lambdaRowIndex = 0;
    OutputState curOutputState;
    {
        if (serialize != nullptr) {
            auto statefulMachine = serialize->hashmap.GetOutputMachine(spillOutputState.outputHashmapPos,
                                                                       spillOutputState.hasBeenOutputNum);
            curOutputState = statefulMachine.HandleElements(
                totalSpillCount,
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                },
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                });
        } else if (fixedInt32 != nullptr) {
            auto statefulMachine = fixedInt32->hashmap.GetOutputMachine(spillOutputState.outputHashmapPos,
                                                                        spillOutputState.hasBeenOutputNum);
            curOutputState = statefulMachine.HandleElements(
                totalSpillCount,
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                },
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseNullHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                });
        } else if (fixedInt64 != nullptr) {
            auto statefulMachine = fixedInt64->hashmap.GetOutputMachine(spillOutputState.outputHashmapPos,
                                                                        spillOutputState.hasBeenOutputNum);
            curOutputState = statefulMachine.HandleElements(
                totalSpillCount,
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                },
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseNullHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                });
        } else if (fixedInt16 != nullptr) {
            auto statefulMachine = fixedInt16->hashmap.GetOutputMachine(spillOutputState.outputHashmapPos,
                                                                        spillOutputState.hasBeenOutputNum);
            curOutputState = statefulMachine.HandleElements(
                totalSpillCount,
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                },
                [&](const auto &key, auto &value) mutable {
                    aggregationSortPtr->ParseNullHashMapToVector(key, value, lambdaRowIndex);
                    ++lambdaRowIndex;
                });
        }
    }
    spillOutputState.UpdateState(curOutputState);

    aggregationSort->SortKvVector();
    auto rowCount = aggregationSort->GetRowCount();
    LogDebug("Spill data to disk starting in hash aggregation operator, rowCount=%lld\n", rowCount);
    ErrorCode result = spiller->Spill(aggregationSort.get());
    LogDebug("Spill data to disk finished in hash aggregation operator, rowCount=%lld\n", rowCount);
    aggregationSort->ClearVector();
    return result;
}

ErrorCode HashAggregationOperator::SpillHashMap()
{
    auto rowCount = 0;
    if (serialize != nullptr) {
        rowCount = serialize->GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        rowCount = fixedInt32->GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        rowCount = fixedInt64->GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        rowCount = fixedInt16->GetElementsSize();
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
                              spillConfig->GetWriteBufferSize());
        hasSpill = true;
    }
    UpdateSpillTimesInfo();
    auto result = SpillToDisk();
    spillOutputState.hasBeenOutputNum = 0;
    spillOutputState.outputHashmapPos = 0;
    return result;
}

uint64_t HashAggregationOperator::GetSpilledBytes() { return spilledBytes; }

uint64_t HashAggregationOperator::GetUsedMemBytes() { return usedMemBytes; }

uint64_t HashAggregationOperator::GetTotalMemBytes() { return totalMemBytes; }

std::vector<uint64_t> HashAggregationOperator::GetSpecialMetricsInfo()
{
    int arrayLength = 2; // 根据返回元素个数修改长度
    std::vector<uint64_t> specialMetricsInfoArray(arrayLength);
    specialMetricsInfoArray[0] = GetUsedMemBytes();
    specialMetricsInfoArray[1] = GetTotalMemBytes();

    return specialMetricsInfoArray;
}

uint64_t HashAggregationOperator::GetHashMapUniqueKeys()
{
    if (serialize != nullptr) {
        return serialize->hashmap.GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        return fixedInt32->hashmap.GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        return fixedInt64->hashmap.GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        return fixedInt16->hashmap.GetElementsSize();
    }
    return 0;
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
            auto keyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(currentVecBatch->Get(0));
            auto key = keyVector->GetValue(currentRowIndex);
            StringRef keyRef(const_cast<char *>(key.data()), key.size());
            if (serialize != nullptr) {
                serialize->ParseKeyToCols(keyRef, groupOutputVectors, groupColNum, rowIdx);
            } else if (fixedInt32 != nullptr) {
                auto key = static_cast<int32_t>(std::stoi(keyRef.ToString()));
                fixedInt32->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
            } else if (fixedInt64 != nullptr) {
                auto key = static_cast<int64_t>(std::stoi(keyRef.ToString()));
                fixedInt64->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
            } else if (fixedInt16 != nullptr) {
                auto key = static_cast<int16_t>(std::stoi(keyRef.ToString()));
                fixedInt16->ParseKeyToCols(key, groupOutputVectors, groupColNum, rowIdx);
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
                int32_t vectorIndex = 1;
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
            auto keyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(currentVecBatch->Get(0));
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
            int32_t vectorIndex = 1;
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
                int32_t vectorIndex = 1;
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
        spillMerger = spiller->CreateSpillMerger(spillFiles);
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
    for (auto &agg : aggregators) {
        agg->SetStateOffset(totalAggStatesSize);
        totalAggStatesSize += agg->GetStateSize();
    }
}

template <typename Deserialize>
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
        auto &hashmap = deserializeHashmap->hashmap;
        totalRowCount = hashmap.GetElementsSize();
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
    if (serialize != nullptr) {
        elementSize = serialize->GetElementsSize();
    } else if (fixedInt32 != nullptr) {
        elementSize = fixedInt32->GetElementsSize();
    } else if (fixedInt64 != nullptr) {
        elementSize = fixedInt64->GetElementsSize();
    } else if (fixedInt16 != nullptr) {
        elementSize = fixedInt16->GetElementsSize();
    }
    return elementSize;
}

ALWAYS_INLINE void HashAggregationOperator::ResetHashmap()
{
    if (serialize != nullptr) {
        serialize->ResetHashmap();
    } else if (fixedInt32 != nullptr) {
        fixedInt32->ResetHashmap();
    } else if (fixedInt64 != nullptr) {
        fixedInt64->ResetHashmap();
    } else if (fixedInt16 != nullptr) {
        fixedInt16->ResetHashmap();
    }
}
} // end of namespace op
} // end of namespace omniruntime
