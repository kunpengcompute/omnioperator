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
#include "operator/aggregation/group_aggregation.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "util/type_util.h"
#include "operator/util/function_type.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "common/vector_util.h"

namespace om_benchmark {
using namespace benchmark;
using namespace std;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
class HashAggLargeGroupBenchmark : public BaseOperatorFixture {
protected:
    BaseFixtureGetOutputStrategy GetOutputStrategy() override { return AFTER_ALL_INPUT_FINISHED; }

    OperatorFactory *createOperatorFactory(const State &state) override
    {
        std::vector<uint32_t> groupByCols{0};
        DataTypes groupByTypes(std::vector<DataTypePtr>({LongType()}));
        auto aggCols = AggregatorUtil::WrapWithVector(std::vector<uint32_t>{1, 2, 3, 4, 5, 6, 7});
        auto aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>{
            DoubleType(), IntType(), DoubleType(), LongType(), DoubleType(), LongType(), LongType()}));
        auto aggOutputTypes = AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>{
            DoubleType(), DoubleType(), DoubleType(), LongType(), DoubleType(), LongType(), LongType()}));
        int32_t valueNum = 7;
        std::vector<uint32_t> maskCols(valueNum, uint32_t(-1));
        std::vector<bool> inputsRaw(valueNum, true);
        std::vector<bool> outputsPartial(valueNum, false);

        std::vector<uint32_t> aggFuncTypes{OMNI_AGGREGATION_TYPE_AVG,         OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_MIN,         OMNI_AGGREGATION_TYPE_MAX,
                                           OMNI_AGGREGATION_TYPE_SUM,         OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN};

        auto *aggFactory = new omniruntime::op::HashAggregationOperatorFactory(
            groupByCols, groupByTypes, aggCols, aggInputTypes, aggOutputTypes, aggFuncTypes, maskCols, inputsRaw,
            outputsPartial);
        aggFactory->Init();
        return aggFactory;
    }

    vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        const int32_t rowsPerPage = LabNumPages(state);
        const int32_t nPages = LabNumRowsPerPage(state);

        int32_t aggSplit = rowsPerPage / 2;
        int32_t offset;
        std::vector<VectorBatch *> results(nPages);
        for (int32_t p = 0; p < nPages; ++p) {
            auto *input = new VectorBatch(rowsPerPage);
            offset = p * aggSplit;
            auto *aggCol = new Vector<int64_t>(rowsPerPage);
            auto *col1 = new Vector<double>(rowsPerPage);
            auto *col2 = new Vector<int32_t>(rowsPerPage);
            auto *col3 = new Vector<double>(rowsPerPage);
            auto *col4 = new Vector<int64_t>(rowsPerPage);
            auto *col5 = new Vector<double>(rowsPerPage);
            auto *col6 = new Vector<int64_t>(rowsPerPage);
            auto *col7 = new Vector<int64_t>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                aggCol->SetValue(r, (r % aggSplit) + offset);
                const auto v = r % cardinality;
                col1->SetValue(r, v);
                col2->SetValue(r, v);
                col3->SetValue(r, v);
                col4->SetValue(r, v);
                col5->SetValue(r, v);
                col6->SetValue(r, v);
                col7->SetValue(r, v);
            }
            input->Append(aggCol);
            input->Append(col1);
            input->Append(col2);
            input->Append(col3);
            input->Append(col4);
            input->Append(col5);
            input->Append(col6);
            input->Append(col7);

            results[p] = input;
        }
        return VectorBatchToVectorBatchSupplier(results);
    }

private:
    OMNI_BENCHMARK_PARAM(int32_t, LabNumPages, 1000, 3000);
    OMNI_BENCHMARK_PARAM(int32_t, LabNumRowsPerPage, 1000, 3000);

    const int32_t nColumns = 8;
    const int32_t cardinality = 256;
};
OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(HashAggLargeGroupBenchmark);
} // namespace om_benchmark
