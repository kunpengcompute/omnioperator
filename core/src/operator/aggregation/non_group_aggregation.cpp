/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Aggregation Source File
 */
#include "non_group_aggregation.h"
#include "vector/vector_common.h"
#include "operator/status.h"
#include "util/type_util.h"
#include "vector_getter.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

OmniStatus AggregationOperatorFactory::Init()
{
    auto &types = sourceTypes.Get();
    aggsInputCols.reserve(aggsInputColsVector.size());
    aggsInputTypes.reserve(aggsInputColsVector.size());
    for (auto aggInputCol : aggsInputColsVector) {
        std::vector<DataTypePtr> aggInputTypeVec;
        std::vector<int32_t> aggsInputVec;
        for (uint32_t i = 0; i < aggInputCol.size(); i++) {
            aggInputTypeVec.push_back(types[aggInputCol[i]]);
            aggsInputVec.push_back(aggInputCol[i]);
        }
        aggsInputCols.push_back(aggsInputVec);
        aggsInputTypes.push_back(std::make_unique<DataTypes>(aggInputTypeVec));
    }

    return CreateAggregatorFactories(aggregatorFactories, aggFuncTypesVector, GetMaskColumns(), aggUdafNamesVector);
}

OmniStatus AggregationOperatorFactory::Close()
{
    return OMNI_STATUS_NORMAL;
}

Operator *AggregationOperatorFactory::CreateOperator()
{
    std::vector<std::unique_ptr<Aggregator>> aggs;

    uint32_t aggCountAllSkipCnt = 0;
    for (uint32_t i = 0; i < this->aggsOutputTypes.size(); i++) {
        uint32_t aggregateType = aggFuncTypesVector[i];
        std::vector<int32_t> aggInputColIdxVec;
        std::vector<DataTypePtr> inputDataTypesPtr;

        // for COUNT_ALL aggregator no input(key and columnar index)
        // use aggCountAllSkipCnt to align with aggsInputCols and aggregatorFactories index not same
        if (aggregateType == OMNI_AGGREGATION_TYPE_COUNT_ALL && inputRaws[i]) {
            inputDataTypesPtr.push_back(NoneType());
            aggInputColIdxVec.push_back(-1);
            aggCountAllSkipCnt++;
        } else {
            auto aggInputIdx = i - aggCountAllSkipCnt;
            for (uint32_t j = 0; j < this->aggsInputCols[aggInputIdx].size(); j++) {
                inputDataTypesPtr.push_back(aggsInputTypes[aggInputIdx]->GetType(j));
                aggInputColIdxVec.push_back(aggsInputCols[aggInputIdx][j]);
            }
        }
        auto inputTypes = DataTypes(inputDataTypesPtr).Instance();
        auto outputTypes = aggsOutputTypes[i].Instance();
        auto aggregator = aggregatorFactories[i]->CreateAggregator(*inputTypes, *outputTypes, aggInputColIdxVec,
            inputRaws[i], outputPartials[i], isOverflowAsNull);
        if (UNLIKELY(aggregator == nullptr)) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", "Unable to create aggregator " + std::to_string(i) + " / " +
                std::to_string(this->aggregatorFactories.size()));
        }
        aggregator->SetStatisticalAggregate(isStatisticalAggregate);
        aggs.push_back(std::move(aggregator));
    }

    return new AggregationOperator(std::move(aggs), aggsOutputTypes, inputRaws, outputPartials, hasAggFilters);
}

int32_t AggregationOperator::AddInput(VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }

    size_t aggCount = aggregators.size();
    if (aggFiltersCount > 0) {
        int32_t filterOffset = vecBatch->GetVectorCount() - aggFiltersCount;
        for (size_t aggIdx = 0; aggIdx < aggCount; aggIdx++) {
            if (hasAggFilters[aggIdx] == 1) {
                aggregators[aggIdx]->ProcessGroupFilter(aggsStates.get(), vecBatch, 0, filterOffset);
                filterOffset++;
            } else {
                aggregators[aggIdx]->ProcessGroup(aggsStates.get(), vecBatch, 0, rowCount);
            }
        }
    } else {
        for (size_t aggIdx = 0; aggIdx < aggCount; aggIdx++) {
            aggregators[aggIdx]->ProcessGroup(aggsStates.get(), vecBatch, 0, rowCount);
        }
    }

    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    return 0;
}

static ALWAYS_INLINE void GenerateAggVector(VectorBatch *vectorBatch, std::vector<DataTypes> &aggsOutputTypes, int size)
{
    // one AggregateExec has multi aggregators, every aggregator may have multi output; create one vector per output column.
    for (auto &outputTypes : aggsOutputTypes) {
        // noramally one aggregator only has one output, but for complex agg function, it may have multi output.
        for (int32_t i = 0; i < outputTypes.GetSize(); ++i) {
            auto typeId = outputTypes.GetType(i)->GetId();
            if (typeId == OMNI_ARRAY || typeId == OMNI_MAP || typeId == OMNI_ROW) {
                // OMNI_ARRAY = 30, OMNI_MAP = 31, OMNI_ROW = 32
                auto &newFunc = newUniqueComplexVectorFunctions[typeId - OMNI_ARRAY];
                newFunc(outputTypes.GetType(i), vectorBatch, size);
            } else {
                auto &newFunc = newUniqueVectorFunctions[typeId];
                newFunc(vectorBatch, size);
            }
        }
    }
}

int AggregationOperator::GetOutput(VectorBatch **outputVecBatch)
{
    // always output one row
    int32_t aggsCount = 0;
    std::vector<DataTypeId> aggsOutputDataTypeIds;
    for (auto &aggOutputTypes : aggsOutputTypes) {
        auto aggSize = aggOutputTypes.GetSize();
        aggsCount += aggSize;
        for (int i = 0; i < aggSize; ++i) {
            aggsOutputDataTypeIds.push_back(aggOutputTypes.GetType(i)->GetId());
        }
    }
    auto output = std::make_unique<VectorBatch>(1);
    auto outputPtr = output.get();
    GenerateAggVector(outputPtr, aggsOutputTypes, 1);

    // set result value
    int32_t aggOutputColsStart = 0;
    for (size_t aggIdx = 0; aggIdx < aggregators.size(); ++aggIdx) {
        auto aggregator = aggregators[aggIdx].get();
        auto *state = aggsStates.get();
        std::vector<BaseVector *> extractVectors;
        for (int32_t i = 0; i < aggsOutputTypes[aggIdx].GetSize(); ++i) {
            extractVectors.push_back(outputPtr->Get(aggOutputColsStart + i));
        }
        aggOutputColsStart += aggsOutputTypes[aggIdx].GetSize();

        try {
            aggregator->ExtractValues(state, extractVectors, 0);
        } catch (const OmniException &oneException) {
            // release VectorBatch when aggregator.ExtractValues throw exception
            // when spark hash agg sum/avg decimal overflow, it will throw exception when
            // OverflowConfigId==OVERFLOW_CONFIG_EXCEPTION
            throw oneException;
        }
    }

    *outputVecBatch = output.release();
    SetStatus(OMNI_STATUS_FINISHED);
    return OMNI_STATUS_FINISHED;
}
}
}