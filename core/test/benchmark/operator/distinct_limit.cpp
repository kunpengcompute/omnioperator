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
#include "operator/limit/distinct_limit.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class DistinctLimit : public BaseOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        return DistinctLimitOperatorFactory::CreateDistinctLimitOperatorFactory(
            DataTypes(INPUT_TYPES[TestGroup(state)]), DISTINCT_CHANNELS[TestGroup(state)].data(),
            (int32_t)DISTINCT_CHANNELS[TestGroup(state)].size(), -1, Limit(state));
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
    int64_t totalPages = 1000;
    std::map<std::string, std::vector<DataTypePtr>> INPUT_TYPES = {
        {"group1", {IntType()}},
        {"group2", {VarcharType(16)}},
        {"group3", {DoubleType()}},
        {"group4", {Decimal128Type(38, 0)}},
        {"group5", {IntType(), VarcharType(16)}},
        {"group6", {IntType(), LongType(), Decimal128Type(38, 0), DoubleType()}},
        {"group7", {VarcharType(20), VarcharType(30), VarcharType(50)}},
        {"group8", {IntType(), VarcharType(30), LongType(), Decimal128Type(38, 0), VarcharType(50)}}};

    std::map<std::string, std::vector<int32_t>> DISTINCT_CHANNELS = {
        {"group1", {0}},    {"group2", {0}},          {"group3", {0}},       {"group4", {0}},
        {"group5", {0, 1}}, {"group6", {0, 1, 2, 3}}, {"group7", {0, 1, 2}}, {"group8", {0, 1, 2, 3, 4}}};
    OMNI_BENCHMARK_PARAM(int64_t, Limit, 100, 10000, 100000);
    OMNI_BENCHMARK_PARAM(std::string, TestGroup, "group1", "group2", "group3", "group4", "group5", "group6", "group7",
                         "group8");
    OMNI_BENCHMARK_PARAM(bool, DictionaryBlocks, false, true);
    OMNI_BENCHMARK_PARAM(int32_t, RowsPerPage, 32, 1024);
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(DistinctLimit);
} // namespace om_benchmark
