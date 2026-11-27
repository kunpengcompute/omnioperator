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
#include "operator/topn/topn.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class TopN : public BaseOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        std::vector<int32_t> asc(SORT_CHANNELS[TestGroup(state)].size());
        std::vector<int32_t> nullFirst(SORT_CHANNELS[TestGroup(state)].size());
        for (unsigned int i = 0; i < SORT_CHANNELS[TestGroup(state)].size(); ++i) {
            asc[i] = 1;
            nullFirst[i] = 0;
        }
        return new TopNOperatorFactory(DataTypes(INPUT_TYPES[TestGroup(state)]), TopNNum(state),
                                       SORT_CHANNELS[TestGroup(state)].data(), asc.data(), nullFirst.data(),
                                       (int32_t)SORT_CHANNELS[TestGroup(state)].size());
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<VectorBatch *> vvb(totalPages);

        for (int i = 0; i < totalPages; ++i) {
            if (DictionaryBlocks(state)) {
                vvb[i] =
                    CreateSequenceVectorBatchWithDictionaryVector(INPUT_TYPES[TestGroup(state)], RowsPerPage(state));
            } else {
                vvb[i] = CreateSequenceVectorBatch(INPUT_TYPES[TestGroup(state)], RowsPerPage(state));
            }
        }

        return VectorBatchToVectorBatchSupplier(vvb);
    }

private:
    int totalPages = 1000;
    std::map<std::string, std::vector<DataTypePtr>> INPUT_TYPES = {
        {"group1", {VarcharType(16)}},
        {"group2", {IntType(), IntType()}},
        {"group3", {IntType(), IntType(), IntType()}},
        {"group4", {LongType()}},
        {"group5", {DoubleType()}},
        {"group6", {IntType(), DoubleType(), LongType()}},
        {"group7", {VarcharType(20), VarcharType(30), VarcharType(50)}},
        {"group8", {Decimal128Type(38, 0)}},
        {"group9", {IntType(), VarcharType(60), VarcharType(20), VarcharType(30)}},
        {"group10", {IntType(), VarcharType(50), IntType(), DoubleType(), VarcharType(50)}},
        {"group11", {LongType(), DoubleType()}},
        {"group12", {LongType(), Decimal128Type(38, 0)}}};

    std::map<std::string, std::vector<int32_t>> SORT_CHANNELS = {
        {"group1", {0}},          {"group2", {0, 1}},           {"group3", {0, 1, 2}}, {"group4", {0}},
        {"group5", {0}},          {"group6", {0, 1, 2}},        {"group7", {0, 1, 2}}, {"group8", {0}},
        {"group9", {0, 1, 2, 3}}, {"group10", {0, 1, 2, 3, 4}}, {"group11", {0, 1}},   {"group12", {0, 1}}};

    OMNI_BENCHMARK_PARAM(int32_t, TopNNum, 1, 10, 100, 1000, 10000);
    OMNI_BENCHMARK_PARAM(std::string, TestGroup, "group1", "group2", "group3", "group4", "group5", "group6", "group7",
                         "group8", "group9", "group10", "group11", "group12");
    OMNI_BENCHMARK_PARAM(bool, DictionaryBlocks, false, true);
    OMNI_BENCHMARK_PARAM(int32_t, RowsPerPage, 32, 1024);
};
OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(TopN);
} // namespace om_benchmark
