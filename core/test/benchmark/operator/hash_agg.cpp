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
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/util/function_type.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "util/type_util.h"
#include "common/vector_util.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class HashAgg : public BaseOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        std::vector<uint32_t> maskChannels(aggFuncTypes[SqlId(state)].size());
        for (auto &item : maskChannels) {
            item = -1;
        }
        std::vector<std::vector<uint32_t>> aggColVectorWrap = AggregatorUtil::WrapWithVector(aggChannels[SqlId(state)]);
        std::vector<DataTypes> aggInputTypesWrap =
            AggregatorUtil::WrapWithVector(DataTypes(aggInputTypes[SqlId(state)]));
        std::vector<DataTypes> aggOutputTypesWrap =
            AggregatorUtil::WrapWithVector(DataTypes(aggOutputTypes[SqlId(state)]));
        auto factory = new HashAggregationOperatorFactory(
            hashChannels[SqlId(state)], DataTypes(hashTypes[SqlId(state)]), aggColVectorWrap, aggInputTypesWrap,
            aggOutputTypesWrap, aggFuncTypes[SqlId(state)], maskChannels,
            AggregatorUtil::WrapWithVector(true, (int)aggFuncTypes[SqlId(state)].size()),
            AggregatorUtil::WrapWithVector(false, (int)aggFuncTypes[SqlId(state)].size()), OverflowAsNull(state));
        factory->Init();
        return factory;
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<VectorBatch *> vvb(totalPages);

        auto types = allTypes[SqlId(state)];

        int groupsPerPage = rowsPerPage / RowsPerGroup(state);

        for (int i = 0; i < totalPages; i++) {
            auto *vectorBatch = new VectorBatch(groupsPerPage);

            for (auto &type : types) {
                switch (type->GetId()) {
                    case OMNI_VARCHAR: vectorBatch->Append(createVarcharVector(state, i, groupsPerPage)); break;
                    case OMNI_LONG: vectorBatch->Append(createBigIntVector(state, i, groupsPerPage)); break;
                    case OMNI_INT: vectorBatch->Append(createIntegerVector(state, i, groupsPerPage)); break;
                    default: return {};
                }
            }

            vvb[i] = vectorBatch;
        }
        return VectorBatchToVectorBatchSupplier(vvb);
    }

private:
    std::map<std::string, std::vector<DataTypePtr>> allTypes = {
        {"sql2",
         {VarcharType(200), VarcharType(200), VarcharType(200), VarcharType(200), IntType(), IntType(), LongType()}},
        {"sql4", {VarcharType(200), IntType(), IntType(), IntType(), LongType()}},
        {"sql6", {IntType(), IntType(), LongType()}},
        {"sql7",
         {VarcharType(200), VarcharType(200), VarcharType(200), LongType(), LongType(), LongType(), LongType(),
          LongType()}},
        {"sql9", {LongType(), LongType(), LongType(), VarcharType(200), LongType(), LongType()}}};

    std::map<std::string, std::vector<uint32_t>> hashChannels = {{"sql2", {0, 1, 2, 3, 4, 5}},
                                                                 {"sql4", {0, 1, 2, 3}},
                                                                 {"sql6", {0, 1}},
                                                                 {"sql7", {0, 1, 2}},
                                                                 {"sql9", {0, 1, 2, 3}}};

    std::map<std::string, std::vector<DataTypePtr>> hashTypes = {
        {"sql2", {VarcharType(200), VarcharType(200), VarcharType(200), VarcharType(200), IntType(), IntType()}},
        {"sql4", {VarcharType(200), IntType(), IntType(), IntType()}},
        {"sql6", {IntType(), IntType()}},
        {"sql7", {VarcharType(200), VarcharType(200), VarcharType(200)}},
        {"sql9", {LongType(), LongType(), LongType(), VarcharType(200)}},
    };
    std::map<std::string, std::vector<uint32_t>> aggChannels = {
        {"sql2", {6}}, {"sql4", {4}}, {"sql6", {2}}, {"sql7", {3, 4, 5, 6, 7}}, {"sql9", {4, 5}}};
    std::map<std::string, std::vector<DataTypePtr>> aggInputTypes = {
        {"sql2", {LongType()}},
        {"sql4", {LongType()}},
        {"sql6", {LongType()}},
        {"sql7", {LongType(), LongType(), LongType(), LongType(), LongType()}},
        {"sql9", {LongType(), LongType()}}};
    std::map<std::string, std::vector<DataTypePtr>> aggOutputTypes = {
        {"sql2", {LongType()}},
        {"sql4", {LongType()}},
        {"sql6", {LongType()}},
        {"sql7", {LongType(), LongType(), LongType(), LongType(), LongType()}},
        {"sql9", {DoubleType(), LongType()}}};
    std::map<std::string, std::vector<uint32_t>> aggFuncTypes = {
        {"sql2", {OMNI_AGGREGATION_TYPE_SUM}},
        {"sql4", {OMNI_AGGREGATION_TYPE_MAX}},
        {"sql6", {OMNI_AGGREGATION_TYPE_COUNT_COLUMN}},
        {"sql7",
         {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
          OMNI_AGGREGATION_TYPE_SUM}},
        {"sql9", {OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MIN}}};

    int totalPages = 140;
    int rowsPerPage = 10000;
    std::string prefixBase = "A";
    OMNI_BENCHMARK_PARAM(int32_t, RowsPerGroup, 100, 1000, 10000);
    OMNI_BENCHMARK_PARAM(bool, IsDictionary, false, true);
    OMNI_BENCHMARK_PARAM(bool, OverflowAsNull, false, true);
    OMNI_BENCHMARK_PARAM(int32_t, PrefixLength, 0, 50, 150);
    OMNI_BENCHMARK_PARAM(std::string, SqlId, "sql2", "sql4", "sql6", "sql7", "sql9");

private:
    std::string GenVarcharPrefix(const State &state)
    {
        std::string prefix;
        for (int i = 0; i < PrefixLength(state); ++i) {
            prefix += prefixBase;
        }
        return prefix;
    }

    BaseVector *createVarcharVector(const State &state, int pageId, int groupsPerPage)
    {
        std::string prefix = GenVarcharPrefix(state);
        if (!IsDictionary(state)) {
            auto *varcharVector = VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, rowsPerPage, 200 * rowsPerPage);
            int vecIndex = 0;
            for (int k = 0; k < groupsPerPage; k++) {
                std::string groupKey = prefix + std::to_string(pageId * groupsPerPage + k);
                for (int i = 0; i < RowsPerGroup(state); ++i) {
                    VectorHelper::SetValue(varcharVector, vecIndex++, &groupKey);
                }
            }
            return varcharVector;
        } else {
            auto *varcharVector = VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, groupsPerPage, 200 * rowsPerPage);

            for (int k = 0; k < groupsPerPage; k++) {
                std::string groupKey = prefix + std::to_string(pageId * groupsPerPage + k);
                VectorHelper::SetValue(varcharVector, k, &groupKey);
            }
            std::vector<int> ids(rowsPerPage);
            for (int k = 0; k < rowsPerPage; k++) {
                ids[k] = k % groupsPerPage;
            }
            auto vec =
                VectorHelper::CreateDictionaryVector(ids.data(), (int32_t)ids.size(), varcharVector, OMNI_VARCHAR);
            delete varcharVector;
            return vec;
        }
    }

    BaseVector *createBigIntVector(const State &state, int pageId, int groupsPerPage)
    {
        if (!IsDictionary(state)) {
            auto *longVector = new Vector<int64_t>(rowsPerPage);
            int vecIndex = 0;
            for (int k = 0; k < groupsPerPage; k++) {
                auto groupKey = pageId * groupsPerPage + k;
                for (int i = 0; i < RowsPerGroup(state); ++i) {
                    longVector->SetValue(vecIndex++, groupKey);
                }
            }
            return longVector;
        } else {
            auto *longVector = new Vector<int64_t>(groupsPerPage);
            for (int k = 0; k < groupsPerPage; k++) {
                long groupKey = pageId * groupsPerPage + k;
                longVector->SetValue(k, groupKey);
            }
            std::vector<int> ids(rowsPerPage);
            for (int k = 0; k < rowsPerPage; k++) {
                ids[k] = k % groupsPerPage;
            }
            auto vec = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), longVector);
            delete longVector;
            return vec;
        }
    }

    BaseVector *createIntegerVector(const State &state, int pageId, int groupsPerPage)
    {
        if (!IsDictionary(state)) {
            auto *intVector = new Vector<int32_t>(rowsPerPage);
            int vecIndex = 0;
            for (int k = 0; k < groupsPerPage; k++) {
                int groupKey = pageId * groupsPerPage + k;
                for (int i = 0; i < RowsPerGroup(state); ++i) {
                    intVector->SetValue(vecIndex++, groupKey);
                }
            }
            return intVector;
        } else {
            auto *intVector = new Vector<int32_t>(groupsPerPage);
            for (int k = 0; k < groupsPerPage; k++) {
                long groupKey = pageId * groupsPerPage + k;
                intVector->SetValue(k, (int)groupKey);
            }
            std::vector<int> ids(rowsPerPage);
            for (int k = 0; k < rowsPerPage; k++) {
                ids[k] = k % groupsPerPage;
            }
            auto vec = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), intVector);
            delete intVector;
            return vec;
        }
    }
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(HashAgg);
} // namespace om_benchmark
