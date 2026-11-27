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
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/aggregator/all_aggregators.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/util/function_type.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "util/type_util.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

// in this benchmark we examin performance of aggregations inside hash aggregation
// to focus on aggregation performance not hashing performance, here the hash aggregator will have a single
// simple groupby column of type int

namespace om_benchmark {
class AggInHashAggBenchmark : public BaseOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        std::vector<uint32_t> maskChannels(1);
        maskChannels[0] = UseMask(state) ? 2 : static_cast<uint32_t>(-1);

        auto aggFuncType = aggFuncTypes[AggType(state)];

        std::vector<uint32_t> hashChannels{0};
        std::vector<DataTypePtr> hashTypes{IntType()};
        std::vector<uint32_t> aggColVector{1};
        if (aggFuncType[0] == OMNI_AGGREGATION_TYPE_COUNT_ALL) {
            aggColVector.clear();
        }
        std::vector<std::vector<uint32_t>> aggColVectorWrap = AggregatorUtil::WrapWithVector(aggColVector);

        std::string key = GetKey(state);
        std::vector<DataTypes> aggInputTypesWrap = AggregatorUtil::WrapWithVector(DataTypes(inputOutputTypes[key][0]));
        std::vector<DataTypes> aggOutputTypesWrap = AggregatorUtil::WrapWithVector(DataTypes(inputOutputTypes[key][1]));

        HashAggregationOperatorFactory *factory = new HashAggregationOperatorFactory(
            hashChannels, DataTypes(hashTypes), aggColVectorWrap, aggInputTypesWrap, aggOutputTypesWrap, aggFuncType,
            maskChannels, AggregatorUtil::WrapWithVector(InputRaw(state), 1),
            AggregatorUtil::WrapWithVector(OutputPartial(state), 1));
        factory->Init();
        return factory;
    }

    void AppendVector(VectorBatch *vectorBatch, const DataTypePtr &vectorType, uint32_t aggType, const State &state)
    {
        switch (vectorType->GetId()) {
            case OMNI_VARCHAR: {
                if (aggType == OMNI_AGGREGATION_TYPE_AVG) {
                    vectorBatch->Append(createDecimalPartialAverageVector(state));
                } else if (aggType == OMNI_AGGREGATION_TYPE_SUM) {
                    vectorBatch->Append(createDecimalPartialSumVector(state));
                } else {
                    vectorBatch->Append(createVarcharVector(state));
                }
                break;
            }
            case OMNI_DECIMAL64:
            case OMNI_LONG: vectorBatch->Append(createLongVector(state)); break;
            case OMNI_INT: vectorBatch->Append(createIntVector(state)); break;
            case OMNI_DOUBLE: vectorBatch->Append(createDoubleVector(state)); break;
            case OMNI_DECIMAL128: vectorBatch->Append(createDecimal128Vector(state)); break;
            case OMNI_CONTAINER: vectorBatch->Append(createPartialAverageVector(state)); break;
            default:
                throw std::runtime_error("Unsupported vector type" + TypeUtil::TypeToStringLog(vectorType->GetId()));
        }
    }

    void VectorSetNull(VectorBatch *vectorBatch, const State &state)
    {
        srand(time(0));
        for (int32_t c = 0; c < vectorBatch->GetVectorCount(); ++c) {
            auto vector = vectorBatch->Get(c);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                if (rand() % 100 < nullRatio) {
                    vector->SetNull(r);
                }
            }
        }
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<VectorBatch *> vvb(totalPages);
        auto vectorType = inputOutputTypes[GetKey(state)][0][0];
        bool useMask = UseMask(state);
        uint32_t aggType = aggFuncTypes[AggType(state)][0];

        for (int32_t i = 0; i < totalPages; ++i) {
            auto *vectorBatch = new VectorBatch(rowsPerPage);
            vectorBatch->Append(createGroupbyColumn(state));

            if ((aggType == OMNI_AGGREGATION_TYPE_COUNT_COLUMN || aggType == OMNI_AGGREGATION_TYPE_COUNT_ALL) &&
                !InputRaw(state)) {
                vectorBatch->Append(createPartialCounterVector(state));
            } else {
                AppendVector(vectorBatch, vectorType, aggType, state);
            }

            if (useMask) {
                vectorBatch->Append(createMaskVector(state));
            }

            if (HasNull(state)) {
                VectorSetNull(vectorBatch, state);
            }
            vvb[i] = vectorBatch;
        }
        return VectorBatchToVectorBatchSupplier(vvb);
    }

    std::string MessageWhenSkip(const benchmark::State &state) override
    {
        if (!InputRaw(state) && OutputPartial(state)) {
            return "!InputRaw + OutputPartial not supported";
        }

        if (VectorType(state).compare("char") == 0 &&
            (AggType(state).compare("sum") == 0 || AggType(state).compare("avg") == 0)) {
            return VectorType(state) + " " + AggType(state) + " not supported";
        }

        if ((AggType(state).compare("count_all")) == 0 && !InputRaw(state)) {
            return "count_all + !InputRaw  not supported";
        }

        // for aggregators other than sum/avg aggregation logic does not depend on type of input/output
        // so, no need to run redundant tests
        if (!(InputRaw(state) && !OutputPartial(state))) {
            if (AggType(state).compare("sum") != 0 && AggType(state).compare("avg") != 0) {
                return "!InputRaw + !OutputPartial + " + AggType(state) + " not supported";
            }
        }

        return "";
    }

private:
    std::vector<DataTypePtr> containerFieldTypes{DoubleType(), LongType()};
    const int32_t totalPages = 1000;
    const int32_t rowsPerPage = 3000;
    const int32_t varcharLength = 128;
    const int32_t maskValidRatio = 50; // percentage of valid rows in mask
    const int32_t nullRatio = 25;      // percentage of null rows
    std::map<std::string, std::vector<std::vector<DataTypePtr>>> inputOutputTypes = {
        {"sum_true_true_int", {{IntType()}, {IntType()}}},
        {"sum_true_true_long", {{LongType()}, {LongType()}}},
        {"sum_true_true_double", {{DoubleType()}, {DoubleType()}}},
        {"sum_true_true_decimal64", {{Decimal64Type()}, {VarcharType(sizeof(DecimalPartialResult))}}},
        {"sum_true_true_decimal128", {{Decimal128Type()}, {VarcharType(sizeof(DecimalPartialResult))}}},
        {"sum_true_false_int", {{IntType()}, {IntType()}}},
        {"sum_true_false_long", {{LongType()}, {LongType()}}},
        {"sum_true_false_double", {{DoubleType()}, {DoubleType()}}},
        {"sum_true_false_decimal64", {{Decimal64Type()}, {Decimal64Type()}}},
        {"sum_true_false_decimal128", {{Decimal128Type()}, {Decimal128Type()}}},
        {"sum_false_false_int", {{IntType()}, {IntType()}}},
        {"sum_false_false_long", {{LongType()}, {LongType()}}},
        {"sum_false_false_double", {{DoubleType()}, {DoubleType()}}},
        {"sum_false_false_decimal64", {{VarcharType(sizeof(DecimalPartialResult))}, {Decimal64Type()}}},
        {"sum_false_false_decimal128", {{VarcharType(sizeof(DecimalPartialResult))}, {Decimal128Type()}}},
        {"avg_true_true_int", {{IntType()}, {ContainerType(std::vector<DataTypePtr>(containerFieldTypes))}}},
        {"avg_true_true_long", {{LongType()}, {ContainerType(std::vector<DataTypePtr>(containerFieldTypes))}}},
        {"avg_true_true_double", {{DoubleType()}, {ContainerType(std::vector<DataTypePtr>(containerFieldTypes))}}},
        {"avg_true_true_decimal64", {{Decimal64Type()}, {VarcharType(sizeof(DecimalPartialResult))}}},
        {"avg_true_true_decimal128", {{Decimal128Type()}, {VarcharType(sizeof(DecimalPartialResult))}}},
        {"avg_true_false_int", {{IntType()}, {DoubleType()}}},
        {"avg_true_false_long", {{LongType()}, {DoubleType()}}},
        {"avg_true_false_double", {{DoubleType()}, {DoubleType()}}},
        {"avg_true_false_decimal64", {{Decimal64Type()}, {Decimal64Type()}}},
        {"avg_true_false_decimal128", {{Decimal128Type()}, {Decimal128Type()}}},
        {"avg_false_false_int", {{ContainerType(std::vector<DataTypePtr>(containerFieldTypes))}, {DoubleType()}}},
        {"avg_false_false_long", {{ContainerType(std::vector<DataTypePtr>(containerFieldTypes))}, {DoubleType()}}},
        {"avg_false_false_double", {{ContainerType(std::vector<DataTypePtr>(containerFieldTypes))}, {DoubleType()}}},
        {"avg_false_false_decimal64", {{VarcharType(sizeof(DecimalPartialResult))}, {Decimal64Type()}}},
        {"avg_false_false_decimal128", {{VarcharType(sizeof(DecimalPartialResult))}, {Decimal128Type()}}},
        {"minMax_true_true_int", {{IntType()}, {IntType()}}},
        {"minMax_true_true_long", {{LongType()}, {LongType()}}},
        {"minMax_true_true_double", {{DoubleType()}, {DoubleType()}}},
        {"minMax_true_true_decimal64", {{Decimal64Type()}, {Decimal64Type()}}},
        {"minMax_true_true_decimal128", {{Decimal128Type()}, {Decimal128Type()}}},
        {"minMax_true_true_char", {{VarcharType(varcharLength)}, {VarcharType(varcharLength)}}},
        {"minMax_true_false_int", {{IntType()}, {IntType()}}},
        {"minMax_true_false_long", {{LongType()}, {LongType()}}},
        {"minMax_true_false_double", {{DoubleType()}, {DoubleType()}}},
        {"minMax_true_false_decimal64", {{Decimal64Type()}, {Decimal64Type()}}},
        {"minMax_true_false_decimal128", {{Decimal128Type()}, {Decimal128Type()}}},
        {"minMax_true_false_char", {{VarcharType(varcharLength)}, {VarcharType(varcharLength)}}},
        {"minMax_false_false_int", {{IntType()}, {IntType()}}},
        {"minMax_false_false_long", {{LongType()}, {LongType()}}},
        {"minMax_false_false_double", {{DoubleType()}, {DoubleType()}}},
        {"minMax_false_false_decimal64", {{Decimal64Type()}, {Decimal64Type()}}},
        {"minMax_false_false_decimal128", {{Decimal128Type()}, {Decimal128Type()}}},
        {"minMax_false_false_char", {{VarcharType(varcharLength)}, {VarcharType(varcharLength)}}},
        {"count_true_true_int", {{IntType()}, {LongType()}}},
        {"count_true_true_long", {{LongType()}, {LongType()}}},
        {"count_true_true_double", {{DoubleType()}, {LongType()}}},
        {"count_true_true_decimal64", {{Decimal64Type()}, {LongType()}}},
        {"count_true_true_decimal128", {{Decimal128Type()}, {LongType()}}},
        {"count_true_true_char", {{VarcharType(varcharLength)}, {LongType()}}},
        {"count_true_false_int", {{IntType()}, {LongType()}}},
        {"count_true_false_long", {{LongType()}, {LongType()}}},
        {"count_true_false_double", {{DoubleType()}, {LongType()}}},
        {"count_true_false_decimal64", {{Decimal64Type()}, {LongType()}}},
        {"count_true_false_decimal128", {{Decimal128Type()}, {LongType()}}},
        {"count_true_false_char", {{VarcharType(varcharLength)}, {LongType()}}},
        {"count_false_false_int", {{LongType()}, {LongType()}}},
        {"count_false_false_long", {{LongType()}, {LongType()}}},
        {"count_false_false_double", {{LongType()}, {LongType()}}},
        {"count_false_false_decimal64", {{LongType()}, {LongType()}}},
        {"count_false_false_decimal128", {{LongType()}, {LongType()}}},
        {"count_false_false_char", {{LongType()}, {LongType()}}}};

    std::map<std::string, std::vector<uint32_t>> aggFuncTypes = {{"sum", {OMNI_AGGREGATION_TYPE_SUM}},
                                                                 {"avg", {OMNI_AGGREGATION_TYPE_AVG}},
                                                                 {"min", {OMNI_AGGREGATION_TYPE_MIN}},
                                                                 {"max", {OMNI_AGGREGATION_TYPE_MAX}},
                                                                 {"count", {OMNI_AGGREGATION_TYPE_COUNT_COLUMN}},
                                                                 {"count_all", {OMNI_AGGREGATION_TYPE_COUNT_ALL}}};

    OMNI_BENCHMARK_PARAM(bool, HasNull, false, true);
    OMNI_BENCHMARK_PARAM(bool, IsDictionary, false, true);
    OMNI_BENCHMARK_PARAM(bool, UseMask, false, true);
    OMNI_BENCHMARK_PARAM(bool, InputRaw, false, true);
    OMNI_BENCHMARK_PARAM(bool, OutputPartial, false, true);
    OMNI_BENCHMARK_PARAM(std::string, AggType, "sum", "avg", "min", "max", "count", "count_all");
    OMNI_BENCHMARK_PARAM(std::string, VectorType, "int", "long", "double", "decimal64", "decimal128", "char");
    OMNI_BENCHMARK_PARAM(int32_t, NumGroups, 1, 10, 100, 1000);

private:
    std::string to_string(const bool v) { return v ? "true" : "false"; }

    std::string GetKey(const State &state)
    {
        auto aggFuncType = aggFuncTypes[AggType(state)];
        std::string key;
        if (aggFuncType[0] == OMNI_AGGREGATION_TYPE_MIN || aggFuncType[0] == OMNI_AGGREGATION_TYPE_MAX) {
            key = "minMax_";
        } else if (aggFuncType[0] == OMNI_AGGREGATION_TYPE_COUNT_ALL ||
                   aggFuncType[0] == OMNI_AGGREGATION_TYPE_COUNT_COLUMN) {
            key = "count_";
        } else {
            key = AggType(state) + "_";
        }
        key += to_string(InputRaw(state)) + "_" + to_string(OutputPartial(state)) + "_" + VectorType(state);
        return key;
    }

    BaseVector *createVarcharVector(const State &state)
    {
        srand(time(0));
        uint8_t charBuffer[varcharLength];
        if (!IsDictionary(state)) {
            auto *vector =
                VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, rowsPerPage, varcharLength * rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                const auto valLen = (rand() % (varcharLength - 1)) + 1;
                for (int32_t i = 0; i < valLen; ++i) {
                    charBuffer[i] = rand() % 95 + 32;
                }
                auto val = std::string(reinterpret_cast<char *>(&charBuffer), valLen);
                VectorHelper::SetValue(vector, r, &val);
            }
            return vector;
        } else {
            auto *vector = VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, 256, varcharLength * 256);
            for (int32_t k = 0; k < 256; ++k) {
                const auto valLen = (rand() % (varcharLength - 1)) + 1;
                for (int32_t i = 0; i < valLen; ++i) {
                    charBuffer[i] = rand() % 95 + 32;
                }
                auto val = std::string(reinterpret_cast<char *>(&charBuffer), valLen);
                VectorHelper::SetValue(vector, k, &val);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector =
                VectorHelper::CreateDictionaryVector(ids.data(), (int32_t)ids.size(), vector, OMNI_VARCHAR);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createLongVector(const State &state)
    {
        srand(time(0));
        if (!IsDictionary(state)) {
            auto *vector = new Vector<int64_t>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, rand() % 256 - 128);
            }
            return vector;
        } else {
            auto *vector = new Vector<int64_t>(256);
            for (int32_t k = 0; k < 256; ++k) {
                vector->SetValue(k, k - 128);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createIntVector(const State &state)
    {
        srand(time(0));
        if (!IsDictionary(state)) {
            auto *vector = new Vector<int32_t>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, rand() % 256 - 128);
            }
            return vector;
        } else {
            auto *vector = new Vector<int32_t>(256);
            for (int32_t k = 0; k < 256; ++k) {
                vector->SetValue(k, k - 128);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createDoubleVector(const State &state)
    {
        srand(time(0));
        if (!IsDictionary(state)) {
            auto *vector = new Vector<double>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, rand() % 256 - 128 + ((rand() % 100) / 100.0));
            }
            return vector;
        } else {
            auto *vector = new Vector<double>(256);
            for (int32_t k = 0; k < 256; ++k) {
                vector->SetValue(k, k - 128 + ((rand() % 100) / 100.0));
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createDecimal128Vector(const State &state)
    {
        srand(time(0));
        if (!IsDictionary(state)) {
            auto *vector = new Vector<Decimal128>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, Decimal128(static_cast<int64_t>(rand() % 256 - 128)));
            }
            return vector;
        } else {
            auto *vector = new Vector<Decimal128>(256);
            for (int32_t k = 0; k < 256; ++k) {
                vector->SetValue(k, Decimal128(static_cast<int64_t>(k - 128)));
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createDecimalPartialAverageVector(const State &state)
    {
        srand(time(0));
        DecimalPartialResult v;
        if (!IsDictionary(state)) {
            auto *vector = VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, rowsPerPage,
                                                      sizeof(DecimalPartialResult) * rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                v.sum = Decimal128(static_cast<int64_t>(rand() % 256 - 128));
                v.count = rand() % 1024 + 1;
                std::string_view value(reinterpret_cast<char *>(&v), sizeof(DecimalPartialResult));
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetValue(r, value);
            }
            return vector;
        } else {
            auto *vector =
                VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, rowsPerPage, sizeof(DecimalPartialResult) * 256);
            for (int32_t k = 0; k < 256; ++k) {
                v.sum = Decimal128(static_cast<int64_t>(rand() % 256 - 128));
                v.count = rand() % 1024 + 1;
                std::string_view value(reinterpret_cast<char *>(&v), sizeof(DecimalPartialResult));
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetValue(k, value);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector =
                VectorHelper::CreateDictionaryVector(ids.data(), (int32_t)ids.size(), vector, OMNI_VARCHAR);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createDecimalPartialSumVector(const State &state)
    {
        srand(time(0));
        DecimalPartialResult v;
        if (!IsDictionary(state)) {
            auto *vector = VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, rowsPerPage,
                                                      sizeof(DecimalPartialResult) * rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                v.sum = Decimal128(static_cast<int64_t>(rand() % 256 - 128));
                v.count = 1;
                std::string_view value(reinterpret_cast<char *>(&v), sizeof(DecimalPartialResult));
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetValue(r, value);
            }
            return vector;
        } else {
            auto *vector =
                VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, rowsPerPage, sizeof(DecimalPartialResult) * 256);
            for (int32_t k = 0; k < 256; ++k) {
                v.sum = Decimal128(static_cast<int64_t>(rand() % 256 - 128));
                v.count = 1;
                std::string_view value(reinterpret_cast<char *>(&v), sizeof(DecimalPartialResult));
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetValue(k, value);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 256;
            }
            auto dictVector =
                VectorHelper::CreateDictionaryVector(ids.data(), (int32_t)ids.size(), vector, OMNI_VARCHAR);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createPartialAverageVector(const State &state)
    {
        srand(time(0));
        auto doubleVector = new Vector<double>(rowsPerPage);
        auto longVector = new Vector<int64_t>(rowsPerPage);
        for (int32_t r = 0; r < rowsPerPage; ++r) {
            doubleVector->SetValue(r, rand() % 256 - 128 + ((rand() % 100) / 100.0));
            longVector->SetValue(r, rand() % 1024 + 1);
        }
        std::vector<int64_t> vectorAddresses(2);
        vectorAddresses[0] = reinterpret_cast<int64_t>(doubleVector);
        vectorAddresses[1] = reinterpret_cast<int64_t>(longVector);
        std::vector<DataTypePtr> dataTypes{DoubleType(), LongType()};
        return new ContainerVector(rowsPerPage, vectorAddresses, dataTypes);
    }

    BaseVector *createGroupbyColumn(const State &state)
    {
        srand(time(0));
        int32_t nGroups = NumGroups(state);
        if (!IsDictionary(state)) {
            auto *vector = new Vector<int32_t>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, rand() % nGroups);
            }
            return vector;
        } else {
            auto *vector = new Vector<int32_t>(nGroups);
            for (int32_t k = 0; k < nGroups; ++k) {
                vector->SetValue(k, k);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % nGroups;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createPartialCounterVector(const State &state)
    {
        srand(time(0));
        if (!IsDictionary(state)) {
            auto *vector = new Vector<int64_t>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, rand() % 1024);
            }
            return vector;
        } else {
            auto *vector = new Vector<int64_t>(256);
            for (int32_t k = 0; k < 256; ++k) {
                vector->SetValue(k, k - 128);
            }
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = rand() % 1024;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }

    BaseVector *createMaskVector(const State &state)
    {
        srand(time(0));
        if (!IsDictionary(state)) {
            auto *vector = new Vector<bool>(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                vector->SetValue(r, ((rand() % 100) < maskValidRatio));
            }
            return vector;
        } else {
            auto *vector = new Vector<bool>(2);
            vector->SetValue(0, false);
            vector->SetValue(1, true);
            std::vector<int32_t> ids(rowsPerPage);
            for (int32_t r = 0; r < rowsPerPage; ++r) {
                ids[r] = ((rand() % 100) < maskValidRatio) ? 1 : 0;
            }
            auto dictVector = VectorHelper::CreateDictionary(ids.data(), (int32_t)ids.size(), vector);
            delete vector;
            return dictVector;
        }
    }
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(AggInHashAggBenchmark);
} // namespace om_benchmark
