/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */


#include <random>
#include "common/common.h"
#include "operator/join/lookup_join.h"
#include "common/vector_util.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class HashBuilder : public BaseOperatorFixture {
protected:
    int32_t rowsPerPage = 10240;
    int32_t buildRowsNumber = 8000000;
    std::string prefix;
    std::string filterExpression;
    std::map<std::string, std::vector<DataTypePtr>> BUILD_TYPES = {
        { "group1", { LongType(), VarcharType(20) } },
        { "group2",
          { LongType(), IntType(), VarcharType(50), IntType(), IntType(), VarcharType(50), VarcharType(10) } },
        { "group3", { LongType(), VarcharType(10) } },
        { "group4", { LongType(), VarcharType(50), VarcharType(50) } },
        { "group5",
          { LongType(), IntType(), VarcharType(50), IntType(), IntType(), VarcharType(50), VarcharType(10) } },
        { "group6", { LongType(), LongType(), LongType(), LongType(), IntType(), IntType() } },
        { "group7", { LongType() } },
        { "group8", { LongType(), IntType(), IntType() } },
        { "group9", { LongType() } },
        { "group10", { LongType() } },
        { "group11", { LongType(), LongType(), LongType(), LongType(), LongType(), IntType(), IntType() } },
        { "group12", { VarcharType(50), VarcharType(50), LongType(), VarcharType(50), VarcharType(50), LongType() } },
        { "group13", { VarcharType(50), IntType(), LongType() } },
        { "group14", { Decimal64Type(12, 2), LongType(), VarcharType(50), LongType() } }
    };

    std::map<std::string, std::vector<int32_t>> BUILD_OUTPUT_COLS = { { "group1", { 1 } },
                                                                      { "group2", { 1, 2, 3, 4, 5 } },
                                                                      { "group3", { 1 } },
                                                                      { "group4", { 1, 2 } },
                                                                      { "group5", { 1, 2, 3, 4, 5, 6 } },
                                                                      { "group6", { 0, 1, 3, 4, 5 } },
                                                                      { "group7", {} },
                                                                      { "group8", { 1, 2 } },
                                                                      { "group9", {} },
                                                                      { "group10", {} },
                                                                      { "group11", { 0, 2, 3, 4, 5, 6 } },
                                                                      { "group12", { 2 } },
                                                                      { "group13", { 0, 1 } },
                                                                      { "group14", { 0, 2, 3 } } };

    std::map<std::string, std::vector<int32_t>> BUILD_HASH_COLS = {
        { "group1", { 0 } },  { "group2", { 0 } },  { "group3", { 0 } },  { "group4", { 0 } },
        { "group5", { 0 } },  { "group6", { 2 } },  { "group7", { 0 } },  { "group8", { 0 } },
        { "group9", { 0 } },  { "group10", { 0 } }, { "group11", { 1 } }, { "group12", { 3, 0, 4, 1, 5 } },
        { "group13", { 2 } }, { "group14", { 1 } }
    };
    OMNI_BENCHMARK_PARAM(std::string, TestGroup, "group1", "group2", "group3", "group4", "group5", "group6", "group7",
        "group8", "group9", "group10", "group11", "group12", "group13", "group14");
    OMNI_BENCHMARK_PARAM(bool, IsDictionaryBlocks, false, true);
    OMNI_BENCHMARK_PARAM(int32_t, BuildRowsRepetition, 1, 5);

protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        return new HashBuilderOperatorFactory(JoinType::OMNI_JOIN_TYPE_INNER, DataTypes(BUILD_TYPES[TestGroup(state)]),
            BUILD_HASH_COLS[TestGroup(state)].data(), (int32_t)BUILD_HASH_COLS[TestGroup(state)].size(),1);
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<DataTypePtr> buildTypes = BUILD_TYPES[TestGroup(state)];
        std::vector<std::vector<int32_t>> columnValues(buildTypes.size());
        for (unsigned int i = 0; i < buildTypes.size(); i++) {
            columnValues[i] = std::vector<int32_t>();
        }

        int maxValue = buildRowsNumber / BuildRowsRepetition(state) + 40;
        int rows = 0;
        std::vector<VectorBatch *> vvb;
        std::map<int32_t, int32_t> initialValueOffsets = { { OMNI_VARCHAR, 20 },   { OMNI_LONG, 30 },
            { OMNI_INT, 40 },       { OMNI_DOUBLE, 50 },
            { OMNI_DECIMAL64, 60 }, { OMNI_DECIMAL128, 60 } };
        while (rows < buildRowsNumber) {
            int newRows = std::min(buildRowsNumber - rows, rowsPerPage);
            for (unsigned int i = 0; i < buildTypes.size(); i++) {
                DataTypeId type = buildTypes[i]->GetId();
                std::vector<int32_t> values(newRows);

                for (int j = 0; j < newRows; j++) {
                    values[j] = (rows + initialValueOffsets[type]) % maxValue + j;
                }
                columnValues[i] = values;
            }

            if (IsDictionaryBlocks(state)) {
                vvb.push_back(CreateVectorBatch(OMNI_DICTIONARY, buildTypes, prefix, columnValues, newRows));
            } else {
                vvb.push_back(CreateVectorBatch(OMNI_FLAT, buildTypes, prefix, columnValues, newRows));
            }

            rows += newRows;

            for (unsigned int i = 0; i < buildTypes.size(); i++) {
                columnValues[i].clear();
            }
        }

        return VectorBatchToVectorBatchSupplier(vvb);
    }
};

class HashJoin : public HashBuilder {
protected:
    HashBuilderOperatorFactory *hashBuilderOperatorFactory = nullptr;
    omniruntime::op::Operator *hashBuilderOperator = nullptr;

    OperatorFactory *createOperatorFactory(const State &state) override
    {
        hashBuilderOperatorFactory =
            dynamic_cast<HashBuilderOperatorFactory *>(HashBuilder::createOperatorFactory(state));
        hashBuilderOperator = hashBuilderOperatorFactory->CreateOperator(nullptr);
        for (const auto &vb : HashBuilder::createVecBatch(state)) {
            hashBuilderOperator->AddInput(vb());
        }
        VectorBatch *hashBuildOutput = nullptr;
        hashBuilderOperator->GetOutput(&hashBuildOutput);

        std::vector<DataTypePtr> buildOutputTypes(BUILD_OUTPUT_COLS[TestGroup(state)].size());
        for (unsigned int i = 0; i < BUILD_OUTPUT_COLS[TestGroup(state)].size(); ++i) {
            buildOutputTypes[i] = BUILD_TYPES[TestGroup(state)][BUILD_OUTPUT_COLS[TestGroup(state)][i]];
        }
        return new LookupJoinOperatorFactory(DataTypes(PROBE_TYPES[TestGroup(state)]),
            PROBE_OUTPUT_COLS[TestGroup(state)].data(), (int32_t)PROBE_OUTPUT_COLS[TestGroup(state)].size(),
            PROBE_HASH_COLS[TestGroup(state)].data(), (int32_t)PROBE_HASH_COLS[TestGroup(state)].size(),
            BUILD_OUTPUT_COLS[TestGroup(state)].data(), (int32_t)BUILD_OUTPUT_COLS[TestGroup(state)].size(),
            DataTypes(buildOutputTypes),
            hashBuilderOperatorFactory->GetHashTablesVariants(), nullptr, new OverflowConfig());
    }

    void TearDown(State &state) override
    {
        BaseOperatorFixture::TearDown(state);
        omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
        delete hashBuilderOperatorFactory;
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<DataTypePtr> probeTypes = PROBE_TYPES[TestGroup(state)];

        std::vector<VectorBatch *> vvb;
        fillVectorBatches(probeTypes, state, vvb);

        return VectorBatchToVectorBatchSupplier(vvb);
    }

    BaseFixtureGetOutputStrategy GetOutputStrategy() override
    {
        return AFTER_EACH_INPUT_FINISHED;
    }

    void fillVectorBatches(const std::vector<DataTypePtr> &probeTypes, const State &state,
        std::vector<VectorBatch *> &vvb)
    {
        std::vector<std::vector<int32_t>> columnValues(probeTypes.size());
        for (unsigned int i = 0; i < probeTypes.size(); i++) {
            columnValues[i] = std::vector<int32_t>();
        }

        std::vector<int32_t> initials;
        std::uniform_real_distribution<double> distribution(0, 1);
        int randomSeed = 42;
        std::default_random_engine random(randomSeed); // NOLINT(cert-msc51-cpp)

        int remainingRows = probeRowsNumber;
        int rowsInPage = 0;
        std::map<int32_t, int32_t> initialValueOffsets = { { OMNI_VARCHAR, 20 },   { OMNI_LONG, 30 },
            { OMNI_INT, 40 },       { OMNI_DOUBLE, 50 },
            { OMNI_DECIMAL64, 60 }, { OMNI_DECIMAL128, 60 } };
        while (remainingRows > 0) {
            double roll = distribution(random);

            for (auto &probeType : probeTypes) {
                initials.push_back(initialValueOffsets[probeType->GetId()] + remainingRows);
            }

            int rowsCount = 1;
            if (MatchRate(state) < 1) {
                // each row has matchRate chance to join
                if (roll > MatchRate(state)) {
                    // generate not matched row
                    for (int &initial : initials) {
                        initial = initial * -1;
                    }
                }
            } else if (MatchRate(state) > 1) {
                // each row has will be repeated between one and 2*matchRate times
                roll = roll * 2 * MatchRate(state) + 1;
                // example for matchRate == 2:
                // roll is within [0, 5) range
                // rowsCount is within [0, 4] range, where each value has same probability
                // so expected rowsCount is 2
                rowsCount = floor(roll);
            }

            for (int i = 0; i < rowsCount; i++) {
                if (rowsInPage >= rowsPerPage) {
                    vvb.push_back(IsDictionaryBlocks(state) ?
                        CreateVectorBatch(OMNI_DICTIONARY, probeTypes, prefix, columnValues, rowsInPage) :
                        CreateVectorBatch(OMNI_FLAT, probeTypes, prefix, columnValues, rowsInPage));
                    rowsInPage = 0;

                    for (unsigned int j = 0; j < probeTypes.size(); j++) {
                        columnValues[j].clear();
                    }
                }

                for (unsigned int j = 0; j < probeTypes.size(); j++) {
                    columnValues[j].push_back(initials[j]);
                }
                --remainingRows;
                rowsInPage++;
            }
            initials.clear();
        }
    }

private:
    int32_t probeRowsNumber = 1400000;
    std::map<std::string, std::vector<DataTypePtr>> PROBE_TYPES = {
        { "group1",
          { LongType(), LongType(), LongType(), VarcharType(30), VarcharType(50) } },
        { "group2", { LongType(), VarcharType(10) } },
        { "group3", { LongType(), LongType(), IntType(), VarcharType(50), IntType(), IntType(), VarcharType(50) } },
        { "group4", { LongType(), LongType(), IntType(), IntType(), IntType() } },
        { "group5", { LongType(), IntType() } },
        { "group6", { VarcharType(60), LongType() } },
        { "group7",
          { LongType(), LongType(), LongType(), IntType(), VarcharType(50), IntType(), IntType(), VarcharType(50) } },
        { "group8", { LongType(), LongType(), LongType(), IntType() } },
        { "group9", { LongType(), LongType() } },
        { "group10", { LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), IntType(), IntType() } },
        { "group11", { LongType() } },
        { "group12",
          { VarcharType(50), DoubleType(), VarcharType(50), LongType(), VarcharType(50), LongType(), VarcharType(50),
            LongType(), VarcharType(50), IntType(), IntType(), LongType() } },
        { "group13", { Decimal64Type(12, 2), LongType(), LongType() } },
        { "group14", { IntType(), IntType(), LongType() } }
    };

    std::map<std::string, std::vector<int32_t>> PROBE_OUTPUT_COLS = { { "group1", { 0, 2, 3, 4 } },
                                                                      { "group2", {} },
                                                                      { "group3", { 0, 2, 3, 4, 5, 6 } },
                                                                      { "group4", { 0, 2, 3, 4 } },
                                                                      { "group5", { 1 } },
                                                                      { "group6", { 0 } },
                                                                      { "group7", { 0, 1, 3, 4, 5, 6, 7 } },
                                                                      { "group8", { 0, 2, 3 } },
                                                                      { "group9", { 0 } },
                                                                      { "group10", { 0, 1, 2, 3, 4, 6, 7 } },
                                                                      { "group11", {} },
                                                                      { "group12", { 8, 2, 1, 4, 3, 7 } },
                                                                      { "group13", { 0, 2 } },
                                                                      { "group14", { 0, 1 } } };
    std::map<std::string, std::vector<int32_t>> PROBE_HASH_COLS = {
        { "group1", { 1 } },  { "group2", { 0 } },  { "group3", { 1 } },  { "group4", { 1 } },
        { "group5", { 0 } },  { "group6", { 1 } },  { "group7", { 2 } },  { "group8", { 1 } },
        { "group9", { 1 } },  { "group10", { 5 } }, { "group11", { 0 } }, { "group12", { 2, 4, 6, 0, 5 } },
        { "group13", { 1 } }, { "group14", { 2 } }
    };
    OMNI_BENCHMARK_PARAM(double, MatchRate, 0.1, 1, 2);
};

OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(HashBuilder);
OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(HashJoin);
}
