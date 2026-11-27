/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "common/common.h"
#include "common/vector_util.h"
#include "operator/window/window.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class Window : public BaseOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        std::vector<omniruntime::op::FunctionType> functions = WINDOW_FUNCTION[TestGroup(state)];
        std::vector<std::vector<int32_t>> sortOrders(SORT_CHANNELS[TestGroup(state)].size());
        for (unsigned int i = 0; i < SORT_CHANNELS[TestGroup(state)].size(); ++i) {
            sortOrders[i] = {1, 0};
        }
        std::vector<int32_t> outputChannels(INPUT_TYPES[TestGroup(state)].size());
        for (unsigned int i = 0; i < INPUT_TYPES[TestGroup(state)].size(); ++i) {
            outputChannels[i] = (int)i;
        }
        std::vector<DataTypePtr> resultTypes = {RESULT_TYPE[TestGroup(state)]};

        std::vector<int32_t> argumentChannels(ARGUMENT_CHANNELS[TestGroup(state)].size());
        for (unsigned int i = 0; i < ARGUMENT_CHANNELS[TestGroup(state)].size(); ++i) {
            argumentChannels[i] = ARGUMENT_CHANNELS[TestGroup(state)][i];
        }

        if (NumberOfPregroupedColumns(state) == 0) {
            return createFactory(INPUT_TYPES[TestGroup(state)], resultTypes, outputChannels, functions,
                                 PARTITION_CHANNELS[TestGroup(state)], {}, SORT_CHANNELS[TestGroup(state)], sortOrders,
                                 argumentChannels, 0);
        } else if (NumberOfPregroupedColumns(state) < numberOfGroupColumns) {
            return createFactory(INPUT_TYPES[TestGroup(state)], resultTypes, outputChannels, functions,
                                 PARTITION_CHANNELS[TestGroup(state)], {1}, SORT_CHANNELS[TestGroup(state)], sortOrders,
                                 argumentChannels, 0);
        } else {
            return createFactory(INPUT_TYPES[TestGroup(state)], resultTypes, outputChannels, functions,
                                 PARTITION_CHANNELS[TestGroup(state)], {0, 1}, SORT_CHANNELS[TestGroup(state)],
                                 sortOrders, argumentChannels,
                                 (NumberOfPregroupedColumns(state) - numberOfGroupColumns));
        }
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<VectorBatch *> vvb(totalPages);

        for (int i = 0; i < totalPages; ++i) {
            if (DictionaryBlocks(state)) {
                vvb[i] = CreateSequenceVectorBatchWithDictionaryVector(INPUT_TYPES[TestGroup(state)], rowsPerPage);
            } else {
                vvb[i] = CreateSequenceVectorBatch(INPUT_TYPES[TestGroup(state)], rowsPerPage);
            }
        }

        return VectorBatchToVectorBatchSupplier(vvb);
    }

private:
    int64_t totalPages = 100;
    int rowsPerPage = 10000;
    int numberOfGroupColumns = 2;
    std::map<std::string, std::vector<int32_t>> PARTITION_CHANNELS = {
        {"group1", {0, 1}}, {"group2", {0, 1, 2}}, {"group3", {0, 1, 2, 3, 4}}, {"group4", {0, 1, 2, 3}},
        {"group5", {0, 1}}, {"group6", {0, 1}},    {"group7", {0, 1, 3, 4}}};
    std::map<std::string, std::vector<DataTypePtr>> INPUT_TYPES = {
        {"group1", {LongType(), LongType(), LongType(), LongType()}},
        {"group2", {LongType(), LongType(), LongType(), LongType()}},
        {"group3",
         {VarcharType(50), VarcharType(50), VarcharType(50), VarcharType(50), IntType(), IntType(), LongType(),
          LongType()}},
        {"group4",
         {VarcharType(50), VarcharType(50), VarcharType(50), VarcharType(50), IntType(), IntType(), LongType()}},
        {"group5", {LongType(), IntType()}},
        {"group6", {LongType(), IntType()}},
        {"group7",
         {VarcharType(50), VarcharType(50), VarcharType(50), VarcharType(50), VarcharType(50), IntType(), LongType()}}};
    std::map<std::string, std::vector<omniruntime::op::FunctionType>> WINDOW_FUNCTION = {
        {"group1", {OMNI_WINDOW_TYPE_ROW_NUMBER}}, {"group2", {OMNI_AGGREGATION_TYPE_COUNT_COLUMN}},
        {"group3", {OMNI_AGGREGATION_TYPE_AVG}},   {"group4", {OMNI_WINDOW_TYPE_RANK}},
        {"group5", {OMNI_AGGREGATION_TYPE_AVG}},   {"group6", {OMNI_AGGREGATION_TYPE_AVG}},
        {"group7", {OMNI_AGGREGATION_TYPE_AVG}},
    };
    std::map<std::string, std::vector<int32_t>> ARGUMENT_CHANNELS = {{"group1", {}}, {"group2", {2}}, {"group3", {7}},
                                                                     {"group4", {}}, {"group5", {1}}, {"group6", {1}},
                                                                     {"group7", {6}}};
    std::map<std::string, DataTypePtr> RESULT_TYPE = {
        {"group1", LongType()},   {"group2", LongType()},   {"group3", DoubleType()}, {"group4", LongType()},
        {"group5", DoubleType()}, {"group6", DoubleType()}, {"group7", DoubleType()}};

    std::map<std::string, std::vector<int32_t>> SORT_CHANNELS = {{"group1", {3}},    {"group2", {3}}, {"group3", {}},
                                                                 {"group4", {4, 5}}, {"group5", {}},  {"group6", {}},
                                                                 {"group7", {}}};

    OMNI_BENCHMARK_PARAM(std::string, TestGroup, "group1", "group2", "group3", "group4", "group5", "group6", "group7");
    OMNI_BENCHMARK_PARAM(bool, DictionaryBlocks, false, true);
    OMNI_BENCHMARK_PARAM(int32_t, NumberOfPregroupedColumns, 0, 1, 2);

    static OperatorFactory *
    createFactory(const std::vector<DataTypePtr> &sourceTypes, const std::vector<DataTypePtr> &resultTypes,
                  std::vector<int32_t> &outputChannels, std::vector<omniruntime::op::FunctionType> &functions,
                  std::vector<int32_t> &partitionChannels, std::vector<int32_t> preGroupedChannels,
                  std::vector<int32_t> &sortChannels, std::vector<std::vector<int32_t>> &sortOrder,
                  std::vector<int32_t> &argumentChannels, int preSortedChannelPrefix)
    {
        std::vector<int32_t> windowFunctions(functions.size());
        std::vector<int32_t> windowFrameTypesFields(functions.size());
        std::vector<int32_t> windowFrameStartTypesField(functions.size());
        std::vector<int32_t> windowFrameStartChannelsField(functions.size());
        std::vector<int32_t> windowFrameEndTypesField(functions.size());
        std::vector<int32_t> windowFrameEndChannelsField(functions.size());

        for (unsigned int i = 0; i < functions.size(); ++i) {
            windowFunctions[i] = functions[i];
            windowFrameTypesFields[i] = OMNI_FRAME_TYPE_RANGE;
            windowFrameStartTypesField[i] = OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING;
            windowFrameStartChannelsField[i] = -1;
            windowFrameEndTypesField[i] = OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING;
            windowFrameEndChannelsField[i] = -1;
        }
        std::vector<int32_t> asc(sortChannels.size());
        std::vector<int32_t> nullFirst(sortChannels.size());
        for (unsigned int i = 0; i < sortChannels.size(); ++i) {
            asc[i] = sortOrder.at(i)[0];
            nullFirst[i] = sortOrder.at(i)[1];
        }

        std::vector<DataTypePtr> allTypesVec;
        allTypesVec.insert(allTypesVec.end(), sourceTypes.begin(), sourceTypes.end());
        allTypesVec.insert(allTypesVec.end(), resultTypes.begin(), resultTypes.end());
        int32_t expectedPositions = 10;
        auto factory = new WindowOperatorFactory(
            DataTypes(sourceTypes), outputChannels.data(), (int32_t)outputChannels.size(), windowFunctions.data(),
            (int32_t)windowFunctions.size(), partitionChannels.data(), (int32_t)partitionChannels.size(),
            preGroupedChannels.data(), (int32_t)preGroupedChannels.size(), sortChannels.data(), asc.data(),
            nullFirst.data(), (int32_t)sortChannels.size(), preSortedChannelPrefix, expectedPositions,
            DataTypes(allTypesVec), argumentChannels.data(), (int32_t)argumentChannels.size(),
            windowFrameTypesFields.data(), windowFrameStartTypesField.data(), windowFrameStartChannelsField.data(),
            windowFrameEndTypesField.data(), windowFrameEndChannelsField.data(), true);
        factory->Init();
        return factory;
    }
};
OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(Window);
} // namespace om_benchmark
