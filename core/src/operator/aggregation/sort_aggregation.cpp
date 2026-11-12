/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Sort Aggregation Source File
 */
#include "sort_aggregation.h"
#include "vector/vector_helper.h"
#include "operator/status.h"
#include "operator/util/operator_util.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::type;

OmniStatus SortAggregationOperatorFactory::Init()
{
    for (auto groupByCol : groupByColsVector) {
        groupByColIndices.push_back(groupByCol);
    }
    for (auto aggInputColsVec : aggsInputColsVector) {
        std::vector<int32_t> aggInputCols;
        for (uint32_t i = 0; i < aggInputColsVec.size(); ++i) {
            aggInputCols.push_back(aggInputColsVec[i]);
        }
        aggsInputCols.push_back(aggInputCols);
    }
    auto ret = CreateAggregatorFactories(aggregatorFactories, aggFuncTypesVector, GetMaskColumns());
    return ret;
}

OmniStatus SortAggregationOperatorFactory::Close()
{
    return OMNI_STATUS_NORMAL;
}

Operator *SortAggregationOperatorFactory::CreateOperator()
{
    std::vector<ColumnIndex> groupByIndex(groupByColIndices.size(), ColumnIndex());
    std::vector<std::unique_ptr<Aggregator>> aggs;

    for (uint32_t i = 0; i < this->groupByColIndices.size(); ++i) {
        auto &type = this->groupByTypes.GetType(i);
        groupByIndex[i] = {this->groupByColIndices[i], type, type};
    }

    uint32_t aggInputColsSize = 0;
    uint32_t aggCountAllSkipCnt = 0;
    uint32_t aggregateType = OMNI_AGGREGATION_TYPE_INVALID;
    for (uint32_t i = 0; i < this->aggregatorFactories.size(); i++) {
        std::vector<int32_t> aggInputColIdxVec;
        std::vector<DataTypePtr> inputDataTypesPtr;
        aggregateType = aggFuncTypesVector[i];

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

    auto sortAggOperator = new SortAggregationOperator(groupByIndex, aggsInputCols, aggInputColsSize, aggInputTypes,
        aggOutputTypes, std::move(aggs), inputRaws, outputPartials, hasAggFilters, operatorConfig, aggFuncTypesVector,
        step);
    sortAggOperator->Init();
    return sortAggOperator;
}

OmniStatus SortAggregationOperator::Init()
{
    if (isInited) {
        return OMNI_STATUS_NORMAL;
    }
    isInited = true;

    SetOperatorName("sortAggregation");

    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    groupKeyTypes.resize(groupColNum);
    cachedKeyNulls.resize(groupColNum, false);

    for (int32_t i = 0; i < groupColNum; ++i) {
        groupKeyTypes[i] = groupByCols[i].input;
        outputTypes.push_back(groupByCols[i].input);
    }

    for (auto &aggregator : aggregators) {
        const std::vector<DataTypePtr> &aggOutTypes = aggregator->GetOutputTypes().Get();
        for (auto dataType : aggOutTypes) {
            outputTypes.push_back(dataType);
        }
    }

    totalAggStatesSize = 0;
    for (auto &aggregator : aggregators) {
        aggregator->SetStateOffset(totalAggStatesSize);
        totalAggStatesSize += static_cast<int32_t>(aggregator->GetStateSize());
    }

    if (totalAggStatesSize > 0) {
        currentGroupState = reinterpret_cast<AggregateState *>(
            executionContext->GetArena()->Allocate(totalAggStatesSize));
        for (auto &aggregator : aggregators) {
            aggregator->InitState(currentGroupState);
        }
    }

    int32_t rowSize = 0;
    for (auto &dt : outputTypes) {
        rowSize += OperatorUtil::GetTypeSize(dt);
    }

    return OMNI_STATUS_NORMAL;
}

bool SortAggregationOperator::CompareGroupKey(VectorBatch *vecBatch, int32_t rowIdx)
{
    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    if (!hasGroupStarted) {
        return false;
    }

    size_t offset = 0;
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto *colVector = vecBatch->Get(groupByCols[i].idx);
        bool isNull = colVector->IsNull(rowIdx);
        if (isNull != cachedKeyNulls[i]) {
            return false;
        }
        if (isNull) {
            continue;
        }
        auto typeId = groupKeyTypes[i]->GetId();
        if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
            auto *strVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVector);
            auto val = strVec->GetValue(rowIdx);
            auto cachedStr = std::string_view(
                reinterpret_cast<const char *>(cachedKeyData.data() + offset),
                cachedKeyStringLengths[i]);
            if (val != cachedStr) {
                return false;
            }
            offset += val.size();
        } else {
            int32_t typeSize = OperatorUtil::GetTypeSize(groupKeyTypes[i]);
            auto *rawValues = reinterpret_cast<uint8_t *>(VectorHelper::UnsafeGetValues(colVector));
            uint8_t *rowPtr = rawValues + rowIdx * typeSize;
            if (memcmp(rowPtr, cachedKeyData.data() + offset, typeSize) != 0) {
                return false;
            }
            offset += typeSize;
        }
    }
    return true;
}

void SortAggregationOperator::CacheGroupKey(VectorBatch *vecBatch, int32_t rowIdx)
{
    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    cachedKeyData.clear();
    cachedKeyStringLengths.assign(groupColNum, 0);
    for (int32_t i = 0; i < groupColNum; ++i) {
        auto *colVector = vecBatch->Get(groupByCols[i].idx);
        bool isNull = colVector->IsNull(rowIdx);
        cachedKeyNulls[i] = isNull;
        if (isNull) {
            continue;
        }
        auto typeId = groupKeyTypes[i]->GetId();
        if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
            auto *strVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVector);
            auto val = strVec->GetValue(rowIdx);
            const char *strData = val.data();
            int32_t strLen = static_cast<int32_t>(val.size());
            cachedKeyStringLengths[i] = static_cast<uint32_t>(strLen);
            size_t oldSize = cachedKeyData.size();
            cachedKeyData.resize(oldSize + strLen);
            memcpy(cachedKeyData.data() + oldSize, strData, strLen);
        } else {
            int32_t typeSize = OperatorUtil::GetTypeSize(groupKeyTypes[i]);
            auto *rawValues = reinterpret_cast<uint8_t *>(VectorHelper::UnsafeGetValues(colVector));
            uint8_t *rowPtr = rawValues + rowIdx * typeSize;
            size_t oldSize = cachedKeyData.size();
            cachedKeyData.resize(oldSize + typeSize);
            memcpy(cachedKeyData.data() + oldSize, rowPtr, typeSize);
        }
    }
}

void SortAggregationOperator::FinalizeCurrentGroup()
{
    if (!hasGroupStarted) {
        return;
    }
    PendingOutputGroup outputGroup;
    outputGroup.keyData = cachedKeyData;
    outputGroup.keyNulls = cachedKeyNulls;
    outputGroup.stringLengths = cachedKeyStringLengths;
    if (totalAggStatesSize > 0) {
        auto *stateCopy = reinterpret_cast<AggregateState *>(
            executionContext->GetArena()->Allocate(totalAggStatesSize));
        memcpy(stateCopy, currentGroupState, totalAggStatesSize);
        outputGroup.state = stateCopy;
    }
    pendingOutputGroups.emplace_back(std::move(outputGroup));
}

void SortAggregationOperator::ResetAggStates()
{
    if (totalAggStatesSize > 0 && currentGroupState != nullptr) {
        for (auto &aggregator : aggregators) {
            aggregator->InitState(currentGroupState);
        }
    }
}

int32_t SortAggregationOperator::AddInput(VectorBatch *vecBatch)
{
    setInputedData(true);
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }

    UpdateAddInputInfo(rowCount);

    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        if (hasGroupStarted && !CompareGroupKey(vecBatch, rowIdx)) {
            FinalizeCurrentGroup();
            ResetAggStates();
        }

        if (!hasGroupStarted) {
            hasGroupStarted = true;
            CacheGroupKey(vecBatch, rowIdx);
        }

        size_t aggNum = aggregators.size();
        if (aggNum > 0 && totalAggStatesSize > 0) {
            std::vector<AggregateState *> rowStates(1, currentGroupState);
            if (aggFiltersCount > 0) {
                int32_t filterOffset = vecBatch->GetVectorCount() - aggFiltersCount;
                for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
                    auto &aggregator = aggregators[aggIdx];
                    if (aggIdx < hasAggFilters.size() && hasAggFilters[aggIdx] == 1) {
                        aggregator->ProcessGroupFilter(rowStates, aggIdx, vecBatch, filterOffset, rowIdx);
                        filterOffset++;
                    } else {
                        aggregator->ProcessGroup(rowStates, vecBatch, rowIdx);
                    }
                }
            } else {
                for (size_t aggIdx = 0; aggIdx < aggNum; ++aggIdx) {
                    auto &aggregator = aggregators[aggIdx];
                    aggregator->ProcessGroup(rowStates, vecBatch, rowIdx);
                }
            }
        }

        if (rowIdx < rowCount - 1 && !CompareGroupKey(vecBatch, rowIdx + 1)) {
            CacheGroupKey(vecBatch, rowIdx + 1);
        }
    }

    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    return 0;
}

int32_t SortAggregationOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!hasInputedData()) {
        return 0;
    }

    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }

    if (hasGroupStarted) {
        FinalizeCurrentGroup();
        hasGroupStarted = false;
    }

    if (pendingOutputGroups.empty() || outputCursor >= static_cast<int32_t>(pendingOutputGroups.size())) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }

    int32_t outputRowCount = static_cast<int32_t>(pendingOutputGroups.size()) - outputCursor;
    auto *output = new VectorBatch(outputRowCount);
    for (auto &dt : outputTypes) {
        auto typeId = dt->GetId();
        if (typeId == OMNI_ARRAY || typeId == OMNI_MAP || typeId == OMNI_ROW) {
            output->Append(VectorHelper::CreateComplexVector(dt.get(), outputRowCount));
            continue;
        }
        if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
            output->Append(new Vector<LargeStringContainer<std::string_view>>(outputRowCount));
        } else {
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                case OMNI_TIME32:
                    output->Append(new Vector<int32_t>(outputRowCount));
                    break;
                case OMNI_LONG:
                case OMNI_DATE64:
                case OMNI_TIME64:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    output->Append(new Vector<int64_t>(outputRowCount));
                    break;
                case OMNI_DOUBLE:
                    output->Append(new Vector<double>(outputRowCount));
                    break;
                case OMNI_FLOAT:
                    output->Append(new Vector<float>(outputRowCount));
                    break;
                case OMNI_BOOLEAN:
                    output->Append(new Vector<bool>(outputRowCount));
                    break;
                case OMNI_SHORT:
                    output->Append(new Vector<int16_t>(outputRowCount));
                    break;
                case OMNI_BYTE:
                    output->Append(new Vector<int8_t>(outputRowCount));
                    break;
                case OMNI_DECIMAL128:
                    output->Append(new Vector<Decimal128>(outputRowCount));
                    break;
                default:
                    output->Append(new Vector<int64_t>(outputRowCount));
                    break;
            }
        }
    }

    int32_t groupColNum = static_cast<int32_t>(groupByCols.size());
    size_t aggNum = aggregators.size();

    for (int32_t outRow = 0; outputCursor < static_cast<int32_t>(pendingOutputGroups.size());
         ++outputCursor, ++outRow) {
        auto &group = pendingOutputGroups[outputCursor];
        size_t keyOffset = 0;

        for (int32_t i = 0; i < groupColNum; ++i) {
            auto *outputVec = output->Get(i);
            if (group.keyNulls[i]) {
                outputVec->SetNull(outRow);
            } else {
                auto typeId = groupKeyTypes[i]->GetId();
                if (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
                    auto *strVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVec);
                    auto val = std::string_view(
                        reinterpret_cast<const char *>(group.keyData.data() + keyOffset),
                        group.stringLengths[i]);
                    strVec->SetValue(outRow, val);
                    keyOffset += group.stringLengths[i];
                } else {
                    int32_t typeSize = OperatorUtil::GetTypeSize(groupKeyTypes[i]);
                    auto *rawOutput = reinterpret_cast<uint8_t *>(VectorHelper::UnsafeGetValues(outputVec));
                    memcpy(rawOutput + outRow * typeSize, group.keyData.data() + keyOffset, typeSize);
                    keyOffset += typeSize;
                }
            }
        }
    }

    if (aggNum > 0 && totalAggStatesSize > 0) {
        std::vector<AggregateState *> allStates(outputRowCount);
        for (int32_t row = 0; row < outputRowCount; ++row) {
            allStates[row] = pendingOutputGroups[outputCursor - outputRowCount + row].state;
        }
        int32_t aggOutputStartIndex = groupColNum;
        for (size_t aggIndex = 0; aggIndex < aggNum; ++aggIndex) {
            auto &aggregator = aggregators[aggIndex];
            const auto oneAggOutputCols = aggOutputTypes[aggIndex].GetSize();
            std::vector<BaseVector *> adaptAggVectors(oneAggOutputCols);
            for (auto j = 0; j < oneAggOutputCols; j++) {
                adaptAggVectors[j] = output->Get(aggOutputStartIndex + j);
            }
            aggOutputStartIndex += oneAggOutputCols;
            aggregator->ExtractValuesBatch(allStates, adaptAggVectors, 0, outputRowCount);
        }
    }

    pendingOutputGroups.clear();
    outputCursor = 0;
    *outputVecBatch = output;
    SetStatus(OMNI_STATUS_FINISHED);
    return outputRowCount;
}

OmniStatus SortAggregationOperator::Close()
{
    executionContext->GetArena()->Reset();
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}
} // namespace op
} // namespace omniruntime
