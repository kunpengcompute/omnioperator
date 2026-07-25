/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * @Description: lookup join operator test implementations
 */
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <ctime>

#include "gtest/gtest.h"
#include "operator/join/hash_builder.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "vector/vector_helper.h"
#include "util/config_util.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace TestUtil;
using std::map;
using std::string;
using std::vector;

namespace JoinTest {
const int32_t DISTINCT_VALUE_COUNT = 4;
const int32_t REPEAT_COUNT = 200;
const int32_t COLUMN_COUNT_4 = 4;
const int32_t VEC_BATCH_COUNT_10 = 10;
const int32_t VEC_BATCH_COUNT_1 = 1;
const int32_t BUILD_POSITION_COUNT = 1000;
const int32_t PROBE_POSITION_COUNT = 1000;
const int32_t TIME_TO_SLEEP = 100;

void DeleteJoinOperatorFactory(HashBuilderOperatorFactory *hashBuilderOperatorFactory,
    LookupJoinOperatorFactory *lookupJoinOperatorFactory,
    LookupOuterJoinOperatorFactory *lookupOuterJoinOperatorFactory)
{
    delete hashBuilderOperatorFactory;
    delete lookupJoinOperatorFactory;
    delete lookupOuterJoinOperatorFactory;
}

void DeleteJoinOperatorFactory(HashBuilderOperatorFactory *hashBuilderOperatorFactory,
    LookupJoinOperatorFactory *lookupJoinOperatorFactory)
{
    DeleteJoinOperatorFactory(hashBuilderOperatorFactory, lookupJoinOperatorFactory, nullptr);
}

VectorBatch *ConstructSimpleBuildData()
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[dataSize] = {1, 2, 1, 2, 3, 4, 5, 6, 7, 1};
    int64_t buildData1[dataSize] = {79, 79, 70, 70, 70, 70, 70, 70, 70, 70};
    return CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
}

VectorBatch **ConstructSimpleBuildData2()
{
    const int32_t dataSize1 = 6;
    const int32_t dataSize2 = 4;

    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData00[dataSize1] = {1, 1, 3, 6, 7, 1};
    int64_t buildData01[dataSize1] = {79, 70, 70, 70, 70, 70};
    VectorBatch *vecBatch0 = CreateVectorBatch(buildTypes, dataSize1, buildData00, buildData01);

    int64_t buildData10[dataSize2] = {2, 2, 4, 5};
    int64_t buildData11[dataSize2] = {79, 70, 70, 70};
    VectorBatch *vecBatch1 = CreateVectorBatch(buildTypes, dataSize2, buildData10, buildData11);

    auto **vectorBatches = new VectorBatch *[2];
    vectorBatches[0] = vecBatch0;
    vectorBatches[1] = vecBatch1;
    return vectorBatches;
}

VectorBatch *ConstructSimpleProbeData()
{
    const int32_t dataSize = 10;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4, 5, 6, 1, 1, 2, 3};
    int64_t probeData1[] = {78, 78, 78, 78, 78, 78, 78, 82, 82, 65};
    return CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
}

VectorBatch *ConstructSimpleExpectedData()
{
    const uint32_t originalDataSize = 10;
    const uint32_t expectedDataSize = 18;

    int64_t expectedData0[originalDataSize] = {78, 78, 78, 78, 78, 78, 78, 82, 82, 65};
    int64_t expectedData1[expectedDataSize] = {79, 70, 70, 79, 70, 70, 70, 70, 70, 79, 70, 70, 79, 70, 70, 79, 70, 70};

    auto inputType = IntType();
    auto expectedVec0 = CreateVector<int64_t>(originalDataSize, expectedData0);
    int32_t ids[expectedDataSize] = {0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9};
    auto expectedDictVec0 =
        VectorHelper::CreateDictionary(ids, expectedDataSize, reinterpret_cast<Vector<int64_t> *>(expectedVec0));
    auto expectedVec1 = CreateVector<int64_t>(expectedDataSize, expectedData1);
    delete expectedVec0;

    auto *vectorBatch = new VectorBatch(expectedDataSize);
    vectorBatch->Append(expectedDictVec0);
    vectorBatch->Append(expectedVec1);
    return vectorBatch;
}

void BuildVectorValues(Vector<int64_t> *vector)
{
    int32_t idx = 0;
    for (int32_t j = 0; j < DISTINCT_VALUE_COUNT; j++) {
        for (int32_t k = 0; k < REPEAT_COUNT; k++) {
            vector->SetValue(idx++, j);
        }
    }
}

void BuildTestData(VectorBatch **vecBatches, int32_t vecBatchCount, int32_t columnCount)
{
    int32_t positionCount = DISTINCT_VALUE_COUNT * REPEAT_COUNT;

    for (int32_t i = 0; i < vecBatchCount; i++) {
        auto *vecBatch = new VectorBatch(positionCount);
        for (int32_t colIdx = 0; colIdx < columnCount; colIdx++) {
            auto vector = std::make_unique<Vector<int64_t>>(positionCount);
            BuildVectorValues(vector.get());
            vecBatch->Append(vector.release());
        }
        vecBatches[i] = vecBatch;
    }
}

HashBuilderOperatorFactory *CreateSimpleBuildFactory(int32_t operatorCount, JoinType joinType)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildTypes,
        buildJoinCols, joinColsCount, operatorCount);
    return hashBuilderFactory;
}

LookupJoinOperatorFactory *CreateSimpleProbeFactory(const HashBuilderOperatorFactory *hashBuilderFactory)
{
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t probeOutputCols[1] = {1};
    int32_t probeOutputColsCount = 1;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t buildOutputCols[1] = {1};
    int32_t buildOutputColsCount = 1;

    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);

    return lookupJoinFactory;
}

TEST(NativeOmniJoinTest, TestComparePerf)
{
    int32_t buildVecBatchCount = 10;
    auto **builderVecBatchesWithoutJit = new VectorBatch *[buildVecBatchCount];
    BuildTestData(builderVecBatchesWithoutJit, buildVecBatchCount, COLUMN_COUNT_4);

    auto hashBuilderFactoryWithoutJit = CreateSimpleBuildFactory(1, OMNI_JOIN_TYPE_INNER);
    auto hashBuilderOperatorWithoutJit =
        dynamic_cast<HashBuilderOperator *>(hashBuilderFactoryWithoutJit->CreateOperator());

    Timer timer;
    timer.SetStart();
    for (int32_t pageIndex = 0; pageIndex < buildVecBatchCount; ++pageIndex) {
        auto errNo = hashBuilderOperatorWithoutJit->AddInput(builderVecBatchesWithoutJit[pageIndex]);
        EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);
    }

    VectorBatch *hashBuilderOutputWithoutJit = nullptr;
    hashBuilderOperatorWithoutJit->GetOutput(&hashBuilderOutputWithoutJit);

    timer.CalculateElapse();
    double wallElapsed = timer.GetWallElapse();
    double cpuElapsed = timer.GetCpuElapse();
    std::cout << "HashBuilder without OmniJit, wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;

    int32_t probeVecBatchCount = 1;
    auto **probeVecBatchesWithoutJit = new VectorBatch *[probeVecBatchCount];
    BuildTestData(probeVecBatchesWithoutJit, probeVecBatchCount, COLUMN_COUNT_4);

    auto lookupJoinFactoryWithoutJit = CreateSimpleProbeFactory(hashBuilderFactoryWithoutJit);
    auto lookupJoinOperatorWithoutJit =
        dynamic_cast<LookupJoinOperator *>(lookupJoinFactoryWithoutJit->CreateOperator());

    timer.Reset();
    auto errNo = lookupJoinOperatorWithoutJit->AddInput(probeVecBatchesWithoutJit[0]);
    EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);

    std::vector<VectorBatch *> lookupJoinOutputWithoutJit;
    while (lookupJoinOperatorWithoutJit->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *result = nullptr;
        lookupJoinOperatorWithoutJit->GetOutput(&result);
        lookupJoinOutputWithoutJit.push_back(result);
    }

    timer.CalculateElapse();
    wallElapsed = timer.GetWallElapse();
    cpuElapsed = timer.GetCpuElapse();
    std::cout << "LookupJoin without OmniJit, wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;

    auto **builderVecBatchesWithJit = new VectorBatch *[buildVecBatchCount];
    BuildTestData(builderVecBatchesWithJit, buildVecBatchCount, COLUMN_COUNT_4);

    auto hashBuilderFactoryWithJit = CreateSimpleBuildFactory(1, OMNI_JOIN_TYPE_INNER);
    auto hashBuilderOperatorWithJit = dynamic_cast<HashBuilderOperator *>(hashBuilderFactoryWithJit->CreateOperator());

    timer.Reset();
    for (int32_t pageIndex = 0; pageIndex < buildVecBatchCount; ++pageIndex) {
        errNo = hashBuilderOperatorWithJit->AddInput(builderVecBatchesWithJit[pageIndex]);
        EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);
    }

    VectorBatch *hashBuilderOutputWithJit = nullptr;
    hashBuilderOperatorWithJit->GetOutput(&hashBuilderOutputWithJit);

    timer.CalculateElapse();
    wallElapsed = timer.GetWallElapse();
    cpuElapsed = timer.GetCpuElapse();
    std::cout << "HashBuilder with OmniJit, wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;

    auto **probeVecBatchesWithJit = new VectorBatch *[probeVecBatchCount];
    BuildTestData(probeVecBatchesWithJit, probeVecBatchCount, COLUMN_COUNT_4);

    auto lookupJoinFactoryWithJit = CreateSimpleProbeFactory(hashBuilderFactoryWithJit);
    auto lookupJoinOperatorWithJit = dynamic_cast<LookupJoinOperator *>(lookupJoinFactoryWithJit->CreateOperator());

    timer.Reset();
    errNo = lookupJoinOperatorWithJit->AddInput(probeVecBatchesWithJit[0]);
    EXPECT_EQ(errNo, OMNI_STATUS_NORMAL);

    std::vector<VectorBatch *> lookupJoinOutputWithJit;
    while (lookupJoinOperatorWithJit->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *result = nullptr;
        lookupJoinOperatorWithJit->GetOutput(&result);
        lookupJoinOutputWithJit.push_back(result);
    }

    timer.CalculateElapse();
    wallElapsed = timer.GetWallElapse();
    cpuElapsed = timer.GetCpuElapse();
    std::cout << "LookupJoin with OmniJit, wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;

    EXPECT_EQ(hashBuilderOutputWithoutJit, nullptr);
    EXPECT_EQ(hashBuilderOutputWithJit, nullptr);

    EXPECT_EQ(lookupJoinOutputWithoutJit.size(), lookupJoinOutputWithJit.size());
    for (uint32_t i = 0; i < lookupJoinOutputWithoutJit.size(); ++i) {
        EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputWithoutJit[i], lookupJoinOutputWithJit[i]));
    }

    VectorHelper::FreeVecBatches(lookupJoinOutputWithoutJit);
    VectorHelper::FreeVecBatches(lookupJoinOutputWithJit);
    delete[] builderVecBatchesWithoutJit;
    delete[] probeVecBatchesWithoutJit;
    delete[] builderVecBatchesWithJit;
    delete[] probeVecBatchesWithJit;

    omniruntime::op::Operator::DeleteOperator(lookupJoinOperatorWithoutJit);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperatorWithJit);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperatorWithoutJit);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperatorWithJit);
    delete lookupJoinFactoryWithoutJit;
    delete lookupJoinFactoryWithJit;
    delete hashBuilderFactoryWithoutJit;
    delete hashBuilderFactoryWithJit;
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithOneBuildOp)
{
    VectorBatch *vecBatch = ConstructSimpleBuildData();
    HashBuilderOperatorFactory *hashBuilderFactory = CreateSimpleBuildFactory(1, OMNI_JOIN_TYPE_INNER);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(vecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorBatch *probeVecBatch = ConstructSimpleProbeData();
    LookupJoinOperatorFactory *lookupJoinFactory = CreateSimpleProbeFactory(hashBuilderFactory);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    VectorBatch *expectVecBatch = ConstructSimpleExpectedData();
    EXPECT_EQ(outputVecBatch->GetRowCount(), expectVecBatch->GetRowCount());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}
// need calculate probe data partitionId
TEST(NativeOmniJoinTest, DISABLED_TestInnerEqualityJoinWithTwoBuildOp)
{
    VectorBatch **vectorBatches = ConstructSimpleBuildData2();
    HashBuilderOperatorFactory *hashBuilderFactory = CreateSimpleBuildFactory(2, OMNI_JOIN_TYPE_INNER);
    auto *hashBuilderOperator0 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator0->AddInput(vectorBatches[0]);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator0->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator1->AddInput(vectorBatches[1]);
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    VectorBatch *probeVecBatch = ConstructSimpleProbeData();
    LookupJoinOperatorFactory *lookupJoinFactory = CreateSimpleProbeFactory(hashBuilderFactory);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    VectorBatch *expectVecBatch = ConstructSimpleExpectedData();
    EXPECT_EQ(outputVecBatch->GetRowCount(), expectVecBatch->GetRowCount());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete[] vectorBatches;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator0);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

struct HashJoinThreadArgs {
    bool isOriginal;
    int64_t hashBuilderFactoryAddr;
    VectorBatch **buildVecBatches;
    int32_t buildVecBatchCount;
    int32_t partitionIndex;
    int64_t lookupJoinFactoryAddr;
    VectorBatch **probeVecBatches;
    int32_t probeVecBatchCount;
    std::mutex lock;
    std::vector<HashBuilderOperator *> buildOperators;
};

VectorBatch **ConstructHashBuilderTestData(int32_t tableCount, int32_t columnCount)
{
    int32_t numbers[] = {1, 2, 3, 4, 6, 7, 8, 9, 12, 13, 75, 27, 28, 38, 36, 32, 20, 50, 37};
    int32_t numberCount = 19;
    auto **vectorBatches = new VectorBatch *[tableCount];
    int32_t positionCount = BUILD_POSITION_COUNT;

    for (int32_t vecBatchIdx = 0; vecBatchIdx < tableCount; vecBatchIdx++) {
        auto *vecBatch = new VectorBatch(columnCount);
        for (int32_t vecIdx = 0; vecIdx < columnCount; vecIdx++) {
            std::unique_ptr<Vector<int64_t>> vector = std::make_unique<Vector<int64_t>>(positionCount);
            for (int32_t position = 0; position < positionCount; position++) {
                int64_t value = numbers[position % numberCount];
                vector->SetValue(position, value);
            }
            vecBatch->Append(vector.release());
        }
        vectorBatches[vecBatchIdx] = vecBatch;
    }

    return vectorBatches;
}

HashBuilderOperatorFactory *PrepareHashBuilder(int32_t operatorCount, bool isOriginal)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int32_t buildHashCols[] = {2, 3};
    int32_t buildHashColsCount = 2;
    HashBuilderOperatorFactory *hashBuilderOperatorFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols,
        buildHashColsCount, operatorCount);
    return hashBuilderOperatorFactory;
}

LookupJoinOperatorFactory *PrepareLookupJoin(const HashBuilderOperatorFactory *hashBuilderOperatorFactory,
    bool isOriginal)
{
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int32_t probeOutputCols[] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[] = {2, 3};
    int32_t probeHashColsCount = 2;
    int32_t buildOutputCols[] = {0, 1};
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderOperatorFactory);
    auto lookupJoinOperatorFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
        buildOutputTypes.GetSize(), buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    return lookupJoinOperatorFactory;
}

void SetHashJoinThreadArgs(struct HashJoinThreadArgs *hashJoinThreadArgs, bool isOriginal,
    int64_t hashBuilderFactoryAddr, VectorBatch **buildVecBatches, int32_t buildVecBatchCount,
    int64_t lookupJoinFactoryAddr, VectorBatch **probeVecBatches, int32_t probeVecBatchCount)
{
    hashJoinThreadArgs->isOriginal = isOriginal;
    hashJoinThreadArgs->hashBuilderFactoryAddr = hashBuilderFactoryAddr;
    hashJoinThreadArgs->buildVecBatches = buildVecBatches;
    hashJoinThreadArgs->buildVecBatchCount = buildVecBatchCount;
    hashJoinThreadArgs->lookupJoinFactoryAddr = lookupJoinFactoryAddr;
    hashJoinThreadArgs->probeVecBatches = probeVecBatches;
    hashJoinThreadArgs->probeVecBatchCount = probeVecBatchCount;
    hashJoinThreadArgs->partitionIndex = -1;
}

void TestHashBuilder(struct HashJoinThreadArgs *hashJoinThreadArgs)
{
    auto *hashBuilderOperatorFactory =
        reinterpret_cast<HashBuilderOperatorFactory *>(hashJoinThreadArgs->hashBuilderFactoryAddr);
    HashBuilderOperator *hashBuilderOperator = nullptr;
    if (hashJoinThreadArgs->isOriginal) {
        hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderOperatorFactory->CreateOperator());
    } else {
        hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderOperatorFactory));
    }

    if (hashJoinThreadArgs->partitionIndex != -1) {
        auto input = DuplicateVectorBatch(hashJoinThreadArgs->buildVecBatches[hashJoinThreadArgs->partitionIndex]);
        hashBuilderOperator->AddInput(input);
        VectorBatch *buildOutputTable = nullptr;
        hashBuilderOperator->GetOutput(&buildOutputTable);
        hashJoinThreadArgs->lock.lock();
        hashJoinThreadArgs->buildOperators.push_back(hashBuilderOperator);
        hashJoinThreadArgs->lock.unlock();
    } else {
        for (int i = 0; i < hashJoinThreadArgs->buildVecBatchCount; ++i) {
            hashBuilderOperator->AddInput(DuplicateVectorBatch(hashJoinThreadArgs->buildVecBatches[i]));
        }
        VectorBatch *buildOutputTable = nullptr;
        hashBuilderOperator->GetOutput(&buildOutputTable);
        omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    }
}

void TestHashBuilderMultiThread(VectorBatch **buildVecBatches)
{
    HashBuilderOperatorFactory *hashBuilderOperatorFactory = PrepareHashBuilder(1, true);
    struct HashJoinThreadArgs hashBuilderThreadArgs;
    SetHashJoinThreadArgs(&hashBuilderThreadArgs, true, reinterpret_cast<int64_t>(hashBuilderOperatorFactory),
        buildVecBatches, 1, 0, nullptr, 0);
    TestHashBuilder(&hashBuilderThreadArgs);
    DeleteJoinOperatorFactory(hashBuilderOperatorFactory, nullptr);
}

void TestLookupJoin(struct HashJoinThreadArgs *hashJoinThreadArgs)
{
    auto *lookupJoinOperatorFactory =
        reinterpret_cast<LookupJoinOperatorFactory *>(hashJoinThreadArgs->lookupJoinFactoryAddr);
    LookupJoinOperator *lookupJoinOperator = nullptr;
    if (hashJoinThreadArgs->isOriginal) {
        lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinOperatorFactory->CreateOperator());
    } else {
        lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinOperatorFactory));
    }
    const int32_t maxLoopCount = 1000;
    for (int loop = 0; loop < maxLoopCount; loop++) {
        for (int i = 0; i < hashJoinThreadArgs->probeVecBatchCount; ++i) {
            lookupJoinOperator->AddInput(DuplicateVectorBatch(hashJoinThreadArgs->probeVecBatches[i]));
            VectorBatch *outputVecBatch = nullptr;
            lookupJoinOperator->GetOutput(&outputVecBatch);
            if (outputVecBatch != nullptr) {
                VectorHelper::FreeVecBatch(outputVecBatch);
            }
        }
    }
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
}

void TestHashJoin(VectorBatch **buildVecBatches, int index, VectorBatch **probeVecBatches)
{
    HashBuilderOperatorFactory *hashBuilderOperatorFactory = PrepareHashBuilder(1, true);
    struct HashJoinThreadArgs hashBuilderThreadArgs;
    SetHashJoinThreadArgs(&hashBuilderThreadArgs, true, reinterpret_cast<int64_t>(hashBuilderOperatorFactory),
        buildVecBatches, 1, 0, nullptr, 0);

    LookupJoinOperatorFactory *lookupJoinOperatorFactory = PrepareLookupJoin(hashBuilderOperatorFactory, true);
    struct HashJoinThreadArgs lookupJoinThreadArgs;
    SetHashJoinThreadArgs(&lookupJoinThreadArgs, true, 0, nullptr, 0,
        reinterpret_cast<int64_t>(lookupJoinOperatorFactory), probeVecBatches, VEC_BATCH_COUNT_1);

    hashBuilderThreadArgs.partitionIndex = index;
    TestHashBuilder(&hashBuilderThreadArgs);
    TestLookupJoin(&lookupJoinThreadArgs);
    for (uint32_t j = 0; j < hashBuilderThreadArgs.buildOperators.size(); j++) {
        omniruntime::op::Operator::DeleteOperator(hashBuilderThreadArgs.buildOperators[j]);
    }
    DeleteJoinOperatorFactory(hashBuilderOperatorFactory, lookupJoinOperatorFactory);
}

void TestHashJoinMultiThreads(VectorBatch **buildVecBatches, int threadNum, VectorBatch **probeVecBatches,
    double &wallElapsed, double &cpuElapsed)
{
    std::vector<std::thread> vecOfThreads;
    Timer timer;
    timer.SetStart();
    for (int32_t i = 0; i < threadNum; ++i) {
        std::thread th(TestHashJoin, buildVecBatches, i, probeVecBatches);
        vecOfThreads.push_back(std::move(th));
    }
    for (auto &th : vecOfThreads) {
        if (th.joinable()) {
            th.join();
        }
    }
    timer.CalculateElapse();
    wallElapsed = timer.GetWallElapse();
    cpuElapsed = timer.GetCpuElapse();
}

TEST(NativeOmniJoinTest, TestInnerEqualityBuildOriginalMultiThreads)
{
    VectorBatch **buildVecBatches = ConstructHashBuilderTestData(VEC_BATCH_COUNT_10, COLUMN_COUNT_4);
    std::cout << "finish build hash builder data" << std::endl;

    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;
    uint32_t threadNums[] = {1, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(uint32_t); ++i) {
        auto t = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;

        uint32_t threadNum = threadNums[i];

        std::vector<std::thread> vecOfThreads;
        Timer timer;
        timer.SetStart();
        for (uint32_t j = 0; j < threadNum; ++j) {
            std::thread th(TestHashBuilderMultiThread, buildVecBatches);
            vecOfThreads.push_back(std::move(th));
        }
        for (auto &th : vecOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse();
        double cpuElapsed = timer.GetCpuElapse();
        std::cout << "testHashBuilderOriginalMultiThreads " << threadNum << " wallElapsed time: " << wallElapsed <<
            "s" << std::endl;
        std::cout << "testHashBuilderOriginalMultiThreads " << threadNum << " cpuElapsed time: " <<
            cpuElapsed / processorCount * t << "s" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TO_SLEEP));
    }

    FreeVecBatches(buildVecBatches, VEC_BATCH_COUNT_10);
}

TEST(NativeOmniJoinTest, TestInnerEqualityBuildJITMultiThreads)
{
    VectorBatch **buildVecBatches = ConstructHashBuilderTestData(VEC_BATCH_COUNT_10, COLUMN_COUNT_4);
    std::cout << "finish build hash builder data" << std::endl;

    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;
    uint32_t threadNums[] = {1, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(uint32_t); ++i) {
        auto t = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;

        uint32_t threadNum = threadNums[i];

        std::vector<std::thread> vecOfThreads;
        Timer timer;
        timer.SetStart();
        for (uint32_t j = 0; j < threadNum; ++j) {
            std::thread th(TestHashBuilderMultiThread, buildVecBatches);
            vecOfThreads.push_back(std::move(th));
        }
        for (auto &th : vecOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse();
        double cpuElapsed = timer.GetCpuElapse();
        std::cout << "testHashBuilderJITMultiThreads " << threadNum << " wallElapsed time: " << wallElapsed << "s" <<
            std::endl;
        std::cout << "testHashBuilderJITMultiThreads " << threadNum << " cpuElapsed time: " <<
            cpuElapsed / processorCount * t << "s" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TO_SLEEP));
    }
    FreeVecBatches(buildVecBatches, VEC_BATCH_COUNT_10);
}

// this test data is used for testLookupJoin*MultiThreads.
// there is one row for a thread, and the thread i will handle the vector batch i
// the numbers[i] belongs to partition i
VectorBatch **ConstructBuilderTestData(int32_t *numbers, int32_t numberCount)
{
    auto **vectorBatches = new VectorBatch *[numberCount];
    for (int32_t vecBatchIdx = 0; vecBatchIdx < numberCount; vecBatchIdx++) {
        auto *vectorBatch = new VectorBatch(1);
        for (int32_t colIdx = 0; colIdx < COLUMN_COUNT_4; colIdx++) {
            std::unique_ptr<Vector<int64_t>> column = std::make_unique<Vector<int64_t>>(1);
            column->SetValue(0, numbers[vecBatchIdx]);
            vectorBatch->Append(column.release());
        }
        vectorBatches[vecBatchIdx] = vectorBatch;
    }
    return vectorBatches;
}

// this test data is used for testLookupJoin*MultiThreads.
// it will output one vector batches, each vector batches has all data
VectorBatch **ConstructProbeTestData(const int32_t *numbers, int32_t numberCount)
{
    if (numberCount <= 0) {
        return nullptr;
    }
    int32_t positionCount = PROBE_POSITION_COUNT;
    auto **vecBatches = new VectorBatch *[VEC_BATCH_COUNT_1];
    for (int32_t vecBatchIdx = 0; vecBatchIdx < VEC_BATCH_COUNT_1; vecBatchIdx++) {
        auto *vecBatch = new VectorBatch(COLUMN_COUNT_4);
        for (int32_t colIdx = 0; colIdx < COLUMN_COUNT_4; colIdx++) {
            std::unique_ptr<Vector<int64_t>> vector = std::make_unique<Vector<int64_t>>(positionCount);
            for (int32_t posIdx = 0; posIdx < positionCount; posIdx++) {
                int64_t value = numbers[(posIdx % numberCount)];
                vector->SetValue(posIdx, value);
            }
            vecBatch->Append(vector.release());
        }
        vecBatches[vecBatchIdx] = vecBatch;
    }

    return vecBatches;
}

void TestHashBuilderMultiThreads(struct HashJoinThreadArgs *hashBuilderThreadArgs, int32_t threadNum)
{
    std::vector<std::thread> vecOfThreads;
    for (int32_t i = 0; i < threadNum; ++i) {
        hashBuilderThreadArgs->partitionIndex = i;
        std::thread th(TestHashBuilder, hashBuilderThreadArgs);
        vecOfThreads.push_back(std::move(th));
    }
    for (auto &th : vecOfThreads) {
        if (th.joinable()) {
            th.join();
        }
    }
}

LookupJoinOperatorFactory *TestLookupJoinMultiThreads(VectorBatch ***probeVecBatches, int32_t threadNum, int32_t index,
    bool isOriginal, const HashBuilderOperatorFactory *hashBuilderOperatorFactory, double &wallElapsed,
    double &cpuElapsed)
{
    LookupJoinOperatorFactory *lookupJoinOperatorFactory = PrepareLookupJoin(hashBuilderOperatorFactory, isOriginal);
    struct HashJoinThreadArgs lookupJoinThreadArgs;
    SetHashJoinThreadArgs(&lookupJoinThreadArgs, isOriginal, 0, nullptr, 0,
        reinterpret_cast<int64_t>(lookupJoinOperatorFactory), probeVecBatches[index], VEC_BATCH_COUNT_1);

    std::vector<std::thread> vecOfThreads;
    Timer timer;
    timer.SetStart();
    for (int32_t i = 0; i < threadNum; ++i) {
        std::thread t(TestLookupJoin, &lookupJoinThreadArgs);
        vecOfThreads.push_back(std::move(t));
    }
    for (auto &th : vecOfThreads) {
        if (th.joinable()) {
            th.join();
        }
    }
    timer.CalculateElapse();
    wallElapsed = timer.GetWallElapse();
    cpuElapsed = timer.GetCpuElapse();
    return lookupJoinOperatorFactory;
}

TEST(NativeOmniJoinTest, TestInnerEqualityProbeOriginalMultiThreads)
{
    int32_t numbers[3][16] = {
        {6},
        {6, 13, 7, 4, 1, 9, 3, 2},
        {8, 13, 75, 12, 27, 28, 3, 2, 6, 38, 36, 32, 1, 20, 50, 37}
        };
    uint32_t threadNums[] = {1, 8, 16};
    uint32_t groupCount = sizeof(threadNums) / sizeof(uint32_t);
    auto ***buildVecBatches = new VectorBatch **[groupCount];
    for (uint32_t i = 0; i < groupCount; i++) {
        buildVecBatches[i] = ConstructBuilderTestData(numbers[i], threadNums[i]);
    }
    auto ***probeVecBatches = new VectorBatch **[groupCount];
    for (uint32_t i = 0; i < groupCount; i++) {
        probeVecBatches[i] = ConstructProbeTestData(numbers[i], threadNums[i]);
    }

    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;
    for (uint32_t i = 0; i < groupCount; ++i) {
        auto t = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;
        uint32_t threadNum = threadNums[i];

        double wallElapsed = 0;
        double cpuElapsed = 0;
        TestHashJoinMultiThreads(buildVecBatches[i], threadNum, probeVecBatches[i], wallElapsed, cpuElapsed);

        std::cout << "testLookupJoinOriginalMultiThreads " << threadNum << " wallElapsed time: " << wallElapsed <<
            "s" << std::endl;
        std::cout << "testLookupJoinOriginalMultiThreads " << threadNum << " cpuElapsed time: " <<
            cpuElapsed / processorCount * t << "s" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TO_SLEEP));
    }

    for (uint32_t i = 0; i < groupCount; i++) {
        FreeVecBatches(buildVecBatches[i], threadNums[i]);
        FreeVecBatches(probeVecBatches[i], VEC_BATCH_COUNT_1);
    }
    delete[] buildVecBatches;
    delete[] probeVecBatches;
}

TEST(NativeOmniJoinTest, TestInnerEqualityProbeJITMultiThreads)
{
    int32_t numbers[3][16] = {
            {6},
            {6, 13, 7, 4, 1, 9, 3, 2},
            {8, 13, 75, 12, 27, 28, 3, 2, 6, 38, 36, 32, 1, 20, 50, 37}
    };
    uint32_t threadNums[] = {1, 8, 16};
    uint32_t groupCount = sizeof(threadNums) / sizeof(uint32_t);
    auto ***buildVecBatches = new VectorBatch **[groupCount];
    for (uint32_t i = 0; i < groupCount; i++) {
        buildVecBatches[i] = ConstructBuilderTestData(numbers[i], threadNums[i]);
    }
    auto ***probeVecBatches = new VectorBatch **[groupCount];
    for (uint32_t i = 0; i < groupCount; i++) {
        probeVecBatches[i] = ConstructProbeTestData(numbers[i], threadNums[i]);
    }

    const auto processorCount = std::thread::hardware_concurrency();
    std::cout << "core number: " << processorCount << std::endl;
    for (uint32_t i = 0; i < groupCount; ++i) {
        auto t = threadNums[i] < processorCount ? processorCount / threadNums[i] : 1;
        uint32_t threadNum = threadNums[i];

        double wallElapsed = 0;
        double cpuElapsed = 0;
        TestHashJoinMultiThreads(buildVecBatches[i], threadNum, probeVecBatches[i], wallElapsed, cpuElapsed);

        std::cout << "testLookupJoinOriginalMultiThreads " << threadNum << " wallElapsed time: " << wallElapsed <<
            "s" << std::endl;
        std::cout << "testLookupJoinOriginalMultiThreads " << threadNum << " cpuElapsed time: " <<
            cpuElapsed / processorCount * t << "s" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(TIME_TO_SLEEP));
    }

    for (uint32_t i = 0; i < groupCount; i++) {
        FreeVecBatches(buildVecBatches[i], threadNums[i]);
        FreeVecBatches(probeVecBatches[i], VEC_BATCH_COUNT_1);
    }
    delete[] buildVecBatches;
    delete[] probeVecBatches;
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoin)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[4][expectedDataSize] = {
            {1, 2, 3, 4},
            {11, 22, 33, 44},
            {2, 0, 4, 0},
            {11, 0, 33, 0}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinAndBuildLeft)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);
	BaseVector **pVector = outputVecBatch->GetVectors();
	std::rotate(pVector, pVector + 2, pVector + outputVecBatch->GetVectorCount());
    const int32_t expectedDataSize = 2;
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int64_t expectedDatas[4][expectedDataSize] = {
            {1, 3},
            {11, 33},
            {2, 4},
            {11, 33}};
    AssertVecBatchEquals(outputVecBatch, probeTypes.GetSize() + buildOutputColsCount, expectedDataSize,
        expectedDatas[0], expectedDatas[1]);

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedData0[expectedDatasSize] = {0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0};
    int64_t expectedData2[expectedDatasSize] = {3, 1};
    int64_t expectedData3[expectedDatasSize] = {333, 111};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinAndBuildLeftWithArray)
{
    // construct input data
    const int32_t dataSize = 5;
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t buildData0[] = {1, 2, 3, 4, 5};
    int64_t buildData1[] = {10, 11, 14, 15, 12};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t probeData0[] = {1, 2, 3, 4, 5};
    int64_t probeData1[] = {11, 22, 33, 12, 10};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr, nullptr, false,
                                                                       nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory =
            LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
                                                                                 probeOutputCols,
                                                                                 probeOutputColsCount,
                                                                                 buildOutputCols,
                                                                                 buildOutputTypes,
                                                                                 hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 3;
    DataTypes expectedTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType(), LongType()}));
    int64_t expectedDatas[4][expectedDataSize] = {
            {1,  4,  5},
            {11, 12, 10},
            {2,  5,  1},
            {11, 12, 10}};
    auto *expectedVecbatch = CreateVectorBatch(expectedTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
                                               expectedDatas[2], expectedDatas[3]);
    BaseVector **pVector = outputVecBatch->GetVectors();
    std::rotate(pVector, pVector + 2, pVector + outputVecBatch->GetVectorCount());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedData0[expectedDatasSize] = {0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0};
    int64_t expectedData2[expectedDatasSize] = {3, 4};
    int64_t expectedData3[expectedDatasSize] = {14, 15};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinChar)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    std::string probeData1[dataSize] = {"11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    std::string expectedData1[expectedDataSize] = {"11", "22", "33", "44"};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    std::string expectedData3[expectedDataSize] = {"11", "", "33", ""};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3), LongType(), VarcharType(2) }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinDate32)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    int32_t buildData1[dataSize] = {123, 11, 321, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    int32_t probeData1[dataSize] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    int32_t expectedData1[expectedDataSize] = {11, 22, 33, 44};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    int32_t expectedData3[expectedDataSize] = {11, -1, 33, -1};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinShort)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), ShortType() }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    int16_t buildData1[dataSize] = {123, 11, 321, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), ShortType() }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    int16_t probeData1[dataSize] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), ShortType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    int16_t expectedData1[expectedDataSize] = {11, 22, 33, 44};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    int16_t expectedData3[expectedDataSize] = {11, -1, 33, -1};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), ShortType(), LongType(), ShortType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinDecimal64)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), Decimal64Type(3, 0) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    int64_t buildData1[dataSize] = {123, 11, 321, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), Decimal64Type(2, 0) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    int64_t probeData1[dataSize] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), Decimal64Type(3, 0) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    int64_t expectedData1[expectedDataSize] = {11, 22, 33, 44};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    int64_t expectedData3[expectedDataSize] = {11, -1, 33, -1};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinDecimal128)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), Decimal128Type(3, 0) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    Decimal128 buildData1[dataSize] = {Decimal128(123, 0), Decimal128(11, 0), Decimal128(321, 0), Decimal128(33, 0)};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), Decimal128Type(2, 0) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    Decimal128 probeData1[dataSize] = {Decimal128(11, 0), Decimal128(22, 0), Decimal128(33, 0), Decimal128(44, 0)};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), Decimal128Type(3, 0) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    Decimal128 expectedData1[expectedDataSize] = {Decimal128(11, 0), Decimal128(22, 0), Decimal128(33, 0),
                                                  Decimal128(44, 0)};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    Decimal128 expectedData3[expectedDataSize] = {Decimal128(11, 0), Decimal128(0, 0), Decimal128(33, 0),
                                                  Decimal128(0, 0)};
    DataTypes expectedDataTypes(
        std::vector<DataTypePtr>({ LongType(), Decimal128Type(), LongType(), Decimal128Type() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinDictionary)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector<int64_t>(dataSize, buildData0));
    const DataTypePtr &dataType = buildTypes.GetType(1);
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*dataType, dataSize, ids, dataSize, buildData1));

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector<int64_t>(dataSize, probeData0));
    const DataTypePtr &probeDataType = probeTypes.GetType(1);
    probeVecBatch->Append(CreateDictionaryVector(*probeDataType, dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 2;
    int64_t expectedDatas[4][expectedDataSize] = {
            {1, 3},
            {11, 33},
            {2, 4},
            {11, 33}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinHasOutputNulls)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 0, 3, 0};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(1);
    buildVecBatch->Get(0)->SetNull(3);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    int64_t probeData0[dataSize] = {0, 2, 0, 4};
    std::string probeData1[dataSize] = {"11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    probeVecBatch->Get(0)->SetNull(0);
    probeVecBatch->Get(0)->SetNull(2);
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    LookupJoinOperatorFactory *lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 2;
    int64_t expectedData2[expectedDataSize] = {0, 0};
    std::string expectedData3[expectedDataSize] = {"11", "33"};
    auto expectedVector2 = CreateVector<int64_t>(expectedDataSize, expectedData2);
    expectedVector2->SetNull(0);
    expectedVector2->SetNull(1);
    auto expectedVector3 = CreateVarcharVector(expectedData3, expectedDataSize);

    int32_t ids[2] = {0, 2};
    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    DataTypePtr dataType = LongType();
    expectedVecBatch->Append(CreateDictionary<OMNI_LONG>(expectedProbeVec0, ids, expectedDataSize));
    expectedVecBatch->Append(CreateDictionary<OMNI_VARCHAR>(expectedProbeVec1, ids, expectedDataSize));
    expectedVecBatch->Append(expectedVector2);
    expectedVecBatch->Append(expectedVector3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinHasOutputNullsChar)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), CharType(3) }));
    int64_t buildData0[dataSize] = {1, 0, 3, 0};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(1);
    buildVecBatch->Get(0)->SetNull(3);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), CharType(2) }));
    int64_t probeData0[dataSize] = {0, 2, 0, 4};
    std::string probeData1[dataSize] = {"11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    probeVecBatch->Get(0)->SetNull(0);
    probeVecBatch->Get(0)->SetNull(2);
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), CharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);


    const int32_t expectedDataSize = 2;
    int64_t expectedData2[expectedDataSize] = {0, 0};
    std::string expectedData3[expectedDataSize] = {"11", "33"};
    auto expectedVector2 = CreateVector<int64_t>(expectedDataSize, expectedData2);
    expectedVector2->SetNull(0);
    expectedVector2->SetNull(1);
    auto expectedVector3 = CreateVarcharVector(expectedData3, expectedDataSize);

    auto expectedVecBatch = new VectorBatch(expectedDataSize);
    int32_t ids[2] = {0, 2};
    expectedVecBatch->Append(CreateDictionary<OMNI_LONG>(expectedProbeVec0, ids, expectedDataSize));
    expectedVecBatch->Append(CreateDictionary<OMNI_CHAR>(expectedProbeVec1, ids, expectedDataSize));
    expectedVecBatch->Append(expectedVector2);
    expectedVecBatch->Append(expectedVector3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    delete expectedProbeVec0;
    delete expectedProbeVec1;

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithIntFilter)
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), IntType() }));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    int32_t buildData1[dataSize] = {35709, 31904, 35709, 31904, 35709, 31904, 35709, 31904, 35709, 31904};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;

    // create the expression for the filter
    auto *notEqualLeft = new FieldExpr(1, IntType());
    auto *notEqualRight = new FieldExpr(3, IntType());
    auto *notEqualExpr =
        new BinaryExpr(omniruntime::expressions::Operator::NEQ, notEqualLeft, notEqualRight, BooleanType());

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
        buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), IntType() }));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7};
    int32_t probeData1[dataSize] = {35709, 35709, 31904, 12477, 31904, 38721, 90419, 35709, 88371, 35709};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            probeVecBatch->Get(1)->SetNull(i);
        } else {
            static_cast<Vector<int32_t> *>(probeVecBatch->Get(1))->SetValue(i, probeData1[i]);
        }
    }
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), IntType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, notEqualExpr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 3;
    int32_t ids[expectDataSize] = {0, 1, 7};
    int32_t expectData2[expectDataSize] = {20, 16, 19};
    int32_t expectData3[expectDataSize] = {31904, 31904, 31904};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData3));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ notEqualExpr });

    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithIntFilterWithOddDataSize)
{
    const int32_t dataSize = 11;
    DataTypes buildTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16, 15};
    int32_t buildData1[dataSize] = {35709, 31904, 35709, 31904, 35709, 31904, 35709, 31904, 35709, 31904, 31904};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;

    // create the expression for the filter
    auto *notEqualLeft = new FieldExpr(1, IntType());
    auto *notEqualRight = new FieldExpr(3, IntType());
    auto *notEqualExpr =
            new BinaryExpr(omniruntime::expressions::Operator::NEQ, notEqualLeft, notEqualRight, BooleanType());

    auto hashBuilderFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
                                                                         buildTypes, buildJoinCols,
                                                                         joinColsCount,
                                                                         operatorCount);
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7, 11};
    int32_t probeData1[dataSize] = {35709, 35709, 31904, 12477, 31904, 38721, 90419, 35709, 88371, 35709, 35709};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            probeVecBatch->Get(1)->SetNull(i);
        } else {
            static_cast<Vector<int32_t> *>(probeVecBatch->Get(1))->SetValue(i, probeData1[i]);
        }
    }
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr,
                                                                       notEqualExpr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 3;
    int32_t ids[expectDataSize] = {0, 1, 7};
    int32_t expectData2[expectDataSize] = {20, 16, 19};
    int32_t expectData3[expectDataSize] = {31904, 31904, 31904};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData3));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({notEqualExpr});

    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithIntFilterWithoutArray)
{
    const int32_t dataSize = 11;
    DataTypes buildTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16, 99999};
    int32_t buildData1[dataSize] = {35709, 31904, 35709, 31904, 35709, 31904, 35709, 31904, 35709, 31904, 31904};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;

    // create the expression for the filter
    auto *notEqualLeft = new FieldExpr(1, IntType());
    auto *notEqualRight = new FieldExpr(3, IntType());
    auto *notEqualExpr =
            new BinaryExpr(omniruntime::expressions::Operator::NEQ, notEqualLeft, notEqualRight, BooleanType());

    auto hashBuilderFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
                                                                         buildTypes, buildJoinCols,
                                                                         joinColsCount,
                                                                         operatorCount);
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7, 11};
    int32_t probeData1[dataSize] = {35709, 35709, 31904, 12477, 31904, 38721, 90419, 35709, 88371, 35709, 35709};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            probeVecBatch->Get(1)->SetNull(i);
        } else {
            static_cast<Vector<int32_t> *>(probeVecBatch->Get(1))->SetValue(i, probeData1[i]);
        }
    }
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr,
                                                                       notEqualExpr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 3;
    int32_t ids[expectDataSize] = {0, 1, 7};
    int32_t expectData2[expectDataSize] = {20, 16, 19};
    int32_t expectData3[expectDataSize] = {31904, 31904, 31904};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData3));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({notEqualExpr});

    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

omniruntime::expressions::Expr *CreateJoinFilterExprWithChar()
{
    // create the filter expression
    std::string funcStr = "substr";
    DataTypePtr retType = VarcharType();

    auto leftSubstrColumn = new FieldExpr(1, VarcharType());
    auto leftSubstrIndex = new LiteralExpr(1, IntType());
    auto leftSubstrLen = new LiteralExpr(5, IntType());
    std::vector<Expr *> leftSubstrArgs;
    leftSubstrArgs.push_back(leftSubstrColumn);
    leftSubstrArgs.push_back(leftSubstrIndex);
    leftSubstrArgs.push_back(leftSubstrLen);
    auto leftSubstrExpr = GetFuncExpr(funcStr, leftSubstrArgs, VarcharType());

    auto rightSubstrColumn = new FieldExpr(3, VarcharType());
    auto rightSubstrIndex = new LiteralExpr(1, IntType());
    auto rightSubstrLen = new LiteralExpr(5, IntType());
    std::vector<Expr *> rightSubstrArgs;
    rightSubstrArgs.push_back(rightSubstrColumn);
    rightSubstrArgs.push_back(rightSubstrIndex);
    rightSubstrArgs.push_back(rightSubstrLen);
    auto rightSubstrExpr = GetFuncExpr(funcStr, rightSubstrArgs, VarcharType());

    auto *notEqualExpr =
        new BinaryExpr(omniruntime::expressions::Operator::NEQ, leftSubstrExpr, rightSubstrExpr, BooleanType());
    return notEqualExpr;
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithCharFilter)
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "35709", "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
        buildTypes, buildJoinCols, joinColsCount, operatorCount);
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7};
    std::string probeData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371", "35709"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            static_cast<VarcharVector *>(probeVecBatch->Get(1))->SetNull(i);
        } else {
            std::string_view value(probeData1[i].data(), 5);
            static_cast<VarcharVector *>(probeVecBatch->Get(1))->SetValue(i, value);
        }
    }
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 3;
    int32_t expectData2[expectDataSize] = {20, 16, 19};
    std::string expectData3[expectDataSize] = {"31904", "31904", "31904"};
    int32_t ids[expectDataSize] = {0, 1, 7};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_VARCHAR>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVarcharVector(expectData3, expectDataSize));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithCharFilterWithOddDataSize)
{
    const int32_t dataSize = 9;
    DataTypes buildTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 16};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
                                                                         buildTypes, buildJoinCols,
                                                                         joinColsCount,
                                                                         operatorCount);
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8};
    std::string probeData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            static_cast<VarcharVector *>(probeVecBatch->Get(1))->SetNull(i);
        } else {
            std::string_view value(probeData1[i].data(), 5);
            static_cast<VarcharVector *>(probeVecBatch->Get(1))->SetValue(i, value);
        }
    }
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr,
                                                                       joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 3;
    int32_t expectData2[expectDataSize] = {20, 16, 19};
    std::string expectData3[expectDataSize] = {"31904", "31904", "31904"};
    int32_t ids[expectDataSize] = {0, 1, 7};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_VARCHAR>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVarcharVector(expectData3, expectDataSize));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({joinFilter});
    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithCharFilter2)
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t buildData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7};
    std::string buildData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371", "35709"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            buildVecBatch->Get(1)->SetNull(i);
        } else {
            std::string_view value(buildData1[i].data(), 5);
            static_cast<Vector<LargeStringContainer<std::string_view>> *>(buildVecBatch->Get(1))->SetValue(i, value);
        }
    }

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
        buildTypes, buildJoinCols, joinColsCount, operatorCount);
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t probeData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    std::string probeData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "35709", "31904"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 3;
    int32_t expectData2[expectDataSize] = {19, 20, 16};
    std::string expectData3[expectDataSize] = {"35709", "35709", "35709"};
    int32_t ids[expectDataSize] = {3, 5, 9};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_VARCHAR>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVarcharVector(expectData3, expectDataSize));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinWithCharFilter2WithOddDataSize)
{
    const int32_t dataSize = 9;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t buildData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8};
    std::string buildData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            buildVecBatch->Get(1)->SetNull(i);
        } else {
            std::string_view value(buildData1[i].data(), 5);
            static_cast<Vector<LargeStringContainer<std::string_view>> *>(buildVecBatch->Get(1))->SetValue(i, value);
        }
    }

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
                                                                         buildTypes, buildJoinCols, joinColsCount,
                                                                         operatorCount);
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t probeData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20};
    std::string probeData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "35709"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    auto expectedProbeVec0 = SliceVector(probeVecBatch->Get(0), 0, dataSize);
    auto expectedProbeVec1 = SliceVector(probeVecBatch->Get(1), 0, dataSize);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount, probeHashCols,
                                                                       probeHashColsCount, buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes, hashBuilderFactoryAddr,
                                                                       joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 2;
    int32_t expectData2[expectDataSize] = {19, 20};
    std::string expectData3[expectDataSize] = {"35709", "35709"};
    int32_t ids[expectDataSize] = {3, 5};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateDictionary<OMNI_INT>(expectedProbeVec0, ids, expectDataSize));
    expectVecBatch->Append(CreateDictionary<OMNI_VARCHAR>(expectedProbeVec1, ids, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVarcharVector(expectData3, expectDataSize));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({joinFilter});
    delete expectedProbeVec0;
    delete expectedProbeVec1;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

// left join with filter
TEST(NativeOmniJoinTest, TestLeftEqualityJoinWithCharFilter)
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "35709", "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_LEFT,
                                                                         buildTypes, buildJoinCols,
                                                                         joinColsCount,
                                                                         operatorCount);
    // create filter expression object
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7};
    std::string probeData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371", "35709"};
    auto probeVec0 = CreateVector<int32_t>(dataSize, probeData0);
    auto probeVec1 = new Vector<LargeStringContainer<std::string_view>>(dataSize);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            probeVec1->SetNull(i);
        } else {
            std::string_view value(probeData1[i].data(), probeData1[i].length());
            probeVec1->SetValue(i, value);
        }
    }
    auto probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(probeVec0);
    probeVecBatch->Append(probeVec1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr,
                                                                       joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 10;
    int32_t expectData0[expectDataSize] = {20, 16, 13, 20, 19, 7, 4, 4, 22, 8};
    std::string expectData1[expectDataSize] = {"35709", "35709", "31904", "", "35709", "", "12477", "38721",
                                               "90419", "88371"};
    auto expectedProbeVec0 = CreateVector<int32_t>(dataSize, expectData0);
    auto expectedProbeVec1 = new Vector<LargeStringContainer<std::string_view>>(dataSize);
    for (int32_t i = 0; i < expectDataSize; i++) {
        if (i == 3 || i == 5) {
            expectedProbeVec1->SetNull(i);
        } else {
            std::string_view value(expectData1[i].data(), expectData1[i].length());
            expectedProbeVec1->SetValue(i, value);
        }
    }
    int32_t expectData2[expectDataSize] = {20, 16, -1, -1, 19, -1, -1, -1, -1, -1};
    std::string expectData3[expectDataSize] = {"31904", "31904", "", "", "31904", "", "", "", "", ""};
    auto expectVec2 = new Vector<int32_t>(expectDataSize);
    auto expectVec3 = new Vector<LargeStringContainer<std::string_view>>(expectDataSize);
    for (int32_t i = 0; i < expectDataSize; i++) {
        if (i == 0 || i == 1 || i == 4) {
            expectVec2->SetValue(i, expectData2[i]);
            std::string_view value(expectData3[i].data(), expectData3[i].length());
            expectVec3->SetValue(i, value);
        } else {
            expectVec2->SetNull(i);
            expectVec3->SetNull(i);
        }
    }

    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(expectedProbeVec0);
    expectVecBatch->Append(expectedProbeVec1);
    expectVecBatch->Append(expectVec2);
    expectVecBatch->Append(expectVec3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({joinFilter});
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftEqualityJoinWithCharFilterWithOddDataSize)
{
    const int32_t dataSize = 11;
    DataTypes buildTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16, 16};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "35709", "31904", "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_LEFT,
                                                                         buildTypes, buildJoinCols,
                                                                         joinColsCount,
                                                                         operatorCount);
    // create filter expression object
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 8, 7, 7};
    std::string probeData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371", "35709", "35709"};
    auto probeVec0 = CreateVector<int32_t>(dataSize, probeData0);
    auto probeVec1 = new Vector<LargeStringContainer<std::string_view>>(dataSize);
    for (int32_t i = 0; i < dataSize; i++) {
        if (i % 5 == 4) {
            probeVec1->SetNull(i);
        } else {
            std::string_view value(probeData1[i].data(), probeData1[i].length());
            probeVec1->SetValue(i, value);
        }
    }
    auto probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(probeVec0);
    probeVecBatch->Append(probeVec1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({IntType(), VarcharType(5)}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr,
                                                                       joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 12;
    int32_t expectedData0[expectDataSize] = {20, 16, 16, 13, 20, 19, 7, 4, 4, 22, 8, 7};
    std::string expectedData1[expectDataSize] = {"35709", "35709", "35709", "31904", "", "35709", "", "12477", "38721",
                                                 "90419",
                                                 "88371", "35709"};
    int32_t expectData2[expectDataSize] = {20, 16, 16, -1, -1, 19, -1, -1, -1, -1, -1, -1};
    std::string expectData3[expectDataSize] = {"31904", "31904", "31904", "", "", "31904", "", "", "", "", "", ""};
    auto expectedProbeVec0 = CreateVector<int32_t>(expectDataSize, expectedData0);
    auto expectedProbeVec1 = new Vector<LargeStringContainer<std::string_view>>(expectDataSize);
    for (int32_t i = 0; i < expectDataSize; i++) {
        if (i == 4 || i == 6) {
            expectedProbeVec1->SetNull(i);
        } else {
            std::string_view value(expectedData1[i].data(), expectedData1[i].length());
            expectedProbeVec1->SetValue(i, value);
        }
    }
    auto expectVec2 = new Vector<int32_t>(expectDataSize);
    auto expectVec3 = new Vector<LargeStringContainer<std::string_view>>(expectDataSize);
    for (int32_t i = 0; i < expectDataSize; i++) {
        if (i == 0 || i == 1 || i == 2 || i == 5) {
            expectVec2->SetValue(i, expectData2[i]);
            std::string_view value(expectData3[i].data(), expectData3[i].length());
            expectVec3->SetValue(i, value);
        } else {
            expectVec2->SetNull(i);
            expectVec3->SetNull(i);
        }
    }

    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(expectedProbeVec0);
    expectVecBatch->Append(expectedProbeVec1);
    expectVecBatch->Append(expectVec2);
    expectVecBatch->Append(expectVec3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

VectorBatch *CreateBuildInputForAllTypes(DataTypes &buildTypes, void **buildDatas, int32_t dataSize, bool isDictionary)
{
    int32_t buildTypesSize = buildTypes.GetSize();
    auto *buildTypeIds = const_cast<int32_t *>(buildTypes.GetIds());
    BaseVector *buildVectors[buildTypesSize];
    for (int32_t i = 0; i < buildTypesSize; i++) {
        buildVectors[i] = VectorHelper::CreateVector(OMNI_FLAT, buildTypeIds[i], dataSize);
        SetValue(buildVectors[i], 0, buildDatas[i]);
    }
    for (int32_t i = 1; i < dataSize; i++) {
        for (int32_t j = 0; j < buildTypesSize; j++) {
            if (i == j + 1) {
                buildVectors[j]->SetNull(i);
            } else {
                SetValue(buildVectors[j], i, buildDatas[j]);
            }
        }
    }

    if (isDictionary) {
        int32_t ids[dataSize];
        // todo::increase the scenario where the dict ids is twice the dataSize
        for (int32_t i = 0; i < dataSize; i++) {
            ids[i] = i;
        }
        for (int32_t i = 0; i < buildTypesSize; i++) {
            auto buildVector = buildVectors[i];
            buildVectors[i] = DYNAMIC_TYPE_DISPATCH(CreateDictionary, buildTypeIds[i], buildVector, ids, dataSize);
            delete buildVector;
        }
    }

    auto buildVecBatch = new VectorBatch(dataSize);
    for (int32_t i = 0; i < buildTypesSize; i++) {
        buildVecBatch->Append(buildVectors[i]);
    }
    return buildVecBatch;
}

VectorBatch *CreateProbeInputForAllTypes(DataTypes &probeTypes, void **probeDatas, int32_t dataSize, bool isDictionary)
{
    int32_t probeTypesSize = probeTypes.GetSize();
    auto *probeTypeIds = const_cast<int32_t *>(probeTypes.GetIds());
    BaseVector *probeVectors[probeTypesSize];
    for (int32_t i = 0; i < probeTypesSize; i++) {
        probeVectors[i] = VectorHelper::CreateVector(OMNI_FLAT, probeTypeIds[i], dataSize);
    }
    for (int32_t i = 0; i < dataSize - 1; i++) {
        for (int32_t j = 0; j < probeTypesSize; j++) {
            if (i == j) {
                probeVectors[j]->SetNull(i);
            } else {
                SetValue(probeVectors[j], i, probeDatas[j]);
            }
        }
    }
    for (int32_t j = 0; j < probeTypesSize; j++) {
        SetValue(probeVectors[j], probeTypesSize, probeDatas[j]);
    }
    if (isDictionary) {
        int32_t ids[dataSize];
        for (int32_t i = 0; i < dataSize; i++) {
            ids[i] = i;
        }
        for (int32_t i = 0; i < probeTypesSize; i++) {
            auto probeVector = probeVectors[i];
            probeVectors[i] = DYNAMIC_TYPE_DISPATCH(CreateDictionary, probeTypeIds[i], probeVector, ids, dataSize);
            delete probeVector;
        }
    }
    auto probeVecBatch = new VectorBatch(dataSize);
    for (int32_t j = 0; j < probeTypesSize; j++) {
        probeVecBatch->Append(probeVectors[j]);
    }
    return probeVecBatch;
}

VectorBatch *CreateExpectVecBatchForAllTypes(DataTypes &probeTypes, VectorBatch *probeVecBatch,
    VectorBatch *buildVecBatch)
{
    int32_t probeVecCount = probeVecBatch->GetVectorCount();
    int32_t buildVecCount = buildVecBatch->GetVectorCount();
    // 20	20	1	20	20	20	0x00000000000000140000000000000000	20	20
    // 20	20	1	20	20	20	0x00000000000000140000000000000000	20  20
    const int32_t expectDataSize = 1;
    int32_t index = 0;
    auto expectVecBatch = new VectorBatch(expectDataSize);
    for (int32_t i = 0; i < probeVecCount; i++) {
        BaseVector *col = probeVecBatch->Get(i);
        expectVecBatch->Append(SliceVector(col, probeVecCount, 1));
        index++;
    }
    for (int32_t i = 0; i < buildVecCount; i++) {
        auto buildVector = buildVecBatch->Get(i);
        expectVecBatch->Append(SliceVector(buildVector, 0, 1));
        index++;
    }
    return expectVecBatch;
}

// join on keys like all types with nulls
TEST(NativeOmniJoinTest, TestInnerEqualityJoinOnAllTypesWithNulls)
{
    // all types: int, long, boolean, double, date32, decimal, decimal128, varchar, char, short
    int32_t intValue = 20;
    int64_t longValue = 20;
    bool boolValue = true;
    double doubleValue = 20.0;
    Decimal128 decimal128(20, 0);
    std::string stringValue("20");
    int16_t shortValue = 20;
    const int32_t dataSize = 11;
    void *joinDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue, &decimal128,
                                 &stringValue, &stringValue, &shortValue};
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
        Decimal64Type(2, 0), Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    auto buildVecBatch = CreateBuildInputForAllTypes(joinTypes, joinDatas, dataSize, false);
    auto probeVecBatch = CreateProbeInputForAllTypes(joinTypes, joinDatas, dataSize, false);
    auto expectVecBatch = CreateExpectVecBatchForAllTypes(joinTypes, probeVecBatch, buildVecBatch);

    int32_t operatorCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
        joinTypes, joinColumns, joinTypesSize, operatorCount);
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    int32_t buildOutputColsCount = joinTypes.GetSize();
    auto lookupJoinFactory =
        LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(joinTypes, joinColumns, joinTypesSize, joinColumns,
        joinTypesSize, joinColumns, buildOutputColsCount, joinTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

// join on keys like dictionary vector with all types with nulls
TEST(NativeOmniJoinTest, TestInnerEqualityJoinOnDictionaryWithNulls)
{
    // all types: int, long, boolean, double, date32, decimal, decimal128, varchar, char, short
    int32_t intValue = 20;
    int64_t longValue = 20;
    bool boolValue = true;
    double doubleValue = 20.0;
    Decimal128 decimal128(20, 0);
    std::string stringValue("20");
    int16_t shortValue = 20;
    const int32_t dataSize = 11;
    void *joinDatas[dataSize] = {&intValue, &longValue, &boolValue, &doubleValue, &intValue, &longValue, &decimal128,
        &stringValue, &stringValue, &shortValue};
    DataTypes joinTypes(std::vector<DataTypePtr>({ IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
        Decimal64Type(2, 0), Decimal128Type(2, 0), VarcharType(2), CharType(2), ShortType() }));
    int32_t joinTypesSize = joinTypes.GetSize();
    int32_t joinColumns[joinTypesSize];
    for (int32_t i = 0; i < joinTypesSize; i++) {
        joinColumns[i] = i;
    }
    auto buildVecBatch = CreateBuildInputForAllTypes(joinTypes, joinDatas, dataSize, true);
    auto probeVecBatch = CreateProbeInputForAllTypes(joinTypes, joinDatas, dataSize, true);
    auto expectVecBatch = CreateExpectVecBatchForAllTypes(joinTypes, probeVecBatch, buildVecBatch);

    int32_t operatorCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER,
        joinTypes, joinColumns, joinTypesSize, operatorCount);
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    int32_t buildOutputColsCount = joinTypes.GetSize();
    auto lookupJoinFactory =
        LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(joinTypes, joinColumns, joinTypesSize, joinColumns,
        joinTypesSize, joinColumns, buildOutputColsCount, joinTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

LookupOuterJoinOperatorFactory *CreateSimpleLookupOuterJoinFactory(const HashBuilderOperatorFactory *hashBuilderFactory)
{
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t probeOutputCols[1] = {1};
    int32_t probeOutputColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t buildOutputCols[1] = {1};

    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);

    return lookupOuterJoinFactory;
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinWithOneBuildOp)
{
    VectorBatch *vecBatch = ConstructSimpleBuildData();
    HashBuilderOperatorFactory *hashBuilderFactory = CreateSimpleBuildFactory(1, OMNI_JOIN_TYPE_FULL);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(vecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorBatch *probeVecBatch = ConstructSimpleProbeData();
    LookupJoinOperatorFactory *lookupJoinFactory = CreateSimpleProbeFactory(hashBuilderFactory);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = CreateSimpleLookupOuterJoinFactory(hashBuilderFactory);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    VectorBatch *expectVecBatch = ConstructSimpleExpectedData();
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    EXPECT_EQ(outputVecBatch->GetRowCount(), expectVecBatch->GetRowCount());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    int64_t expectedData0[1] = {0};
    int64_t expectedData1[1] = {70};
    auto expectedVec0 = CreateVector(1, expectedData0);
    auto expectedVec1 = CreateVector(1, expectedData1);
    auto vectorBatch = new VectorBatch(1);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Get(0)->SetNull(0);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, DISABLED_TestFullEqualityJoinWithTwoBuildOp)
{
    VectorBatch **vectorBatches = ConstructSimpleBuildData2();
    HashBuilderOperatorFactory *hashBuilderFactory = CreateSimpleBuildFactory(2, OMNI_JOIN_TYPE_FULL);
    auto *hashBuilderOperator0 = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator0->AddInput(vectorBatches[0]);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator0->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator1->AddInput(vectorBatches[1]);
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    VectorBatch *probeVecBatch = ConstructSimpleProbeData();
    LookupJoinOperatorFactory *lookupJoinFactory = CreateSimpleProbeFactory(hashBuilderFactory);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = CreateSimpleLookupOuterJoinFactory(hashBuilderFactory);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    VectorBatch *expectVecBatch = ConstructSimpleExpectedData();
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    EXPECT_EQ(outputVecBatch->GetRowCount(), expectVecBatch->GetRowCount());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    int64_t expectedData0[1] = {0};
    int64_t expectedData1[1] = {70};
    auto expectedVec0 = CreateVector(1, expectedData0);
    auto expectedVec1 = CreateVector(1, expectedData1);
    auto vectorBatch = new VectorBatch(1);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Get(0)->SetNull(0);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    delete[] vectorBatches;
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator0);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinWithArray)
{
    // construct input data
    const int32_t dataSize = 5;
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t buildData0[] = {1, 2, 3, 4, 5};
    int64_t buildData1[] = {1, 2, 3, 4, 5};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t probeData0[] = {1, 2, 3, 4, 7};
    int64_t probeData1[] = {1, 5, 7, 9, 11};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr, nullptr, false,
                                                                       nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory =
            LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
                                                                                 probeOutputCols,
                                                                                 probeOutputColsCount,
                                                                                 buildOutputCols,
                                                                                 buildOutputTypes,
                                                                                 hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 5;
    DataTypes expectedTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType(), LongType()}));
    int64_t expectedDatas[4][expectedDataSize] = {
            {1, 2, 3,  4,  7},
            {1, 5, 7,  9,  11},
            {1, 5, -1, -1, -1},
            {1, 5, -1, -1, -1}};
    auto *expectedVecbatch = CreateVectorBatch(expectedTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
                                               expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(2);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(2)->SetNull(4);
    expectedVecbatch->Get(3)->SetNull(2);
    expectedVecbatch->Get(3)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 3;
    int64_t expectedData0[expectedDatasSize] = {0, 0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0, 0};
    int64_t expectedData2[expectedDatasSize] = {2, 3, 4};
    int64_t expectedData3[expectedDatasSize] = {2, 3, 4};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(0)->SetNull(2);
    vectorBatch->Get(1)->SetNull(2);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoin)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int64_t expectedDatas[4][expectedDataSize] = {
        {1, 2, 3, 4},
        {11, 22, 33, 44},
        {2, 0, 4, 0},
        {11, 0, 33, 0}};
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedData0[expectedDatasSize] = {0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0};
    int64_t expectedData2[expectedDatasSize] = {3, 1};
    int64_t expectedData3[expectedDatasSize] = {333, 111};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestOneKeyFullEqualityJoinWithBuildNull)
{
    const int32_t dataSize = 6;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4, 5, 6};
    int64_t buildData1[] = {111, 11, 333, 33, 11, 11};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(1)->SetNull(0);
    buildVecBatch->Get(1)->SetNull(2);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    const int32_t probeDataSize = 4;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 6;
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int64_t expectedDatas[4][expectedDataSize] = {
                {1, 1, 1, 2, 3, 4},
                {11, 11, 11, 22, 33, 44},
                {2, 5, 6, 0, 4, 0},
                {11, 11, 11, 0, 33, 0}};
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(2)->SetNull(5);
    expectedVecbatch->Get(3)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(5);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedData0[expectedDatasSize] = {0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0};
    int64_t expectedData2[expectedDatasSize] = {1, 3};
    int64_t expectedData3[expectedDatasSize] = {111, 333};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    vectorBatch->Get(3)->SetNull(0);
    vectorBatch->Get(3)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestFixedKeysFullEqualityJoinWithBuildNull)
{
    const int32_t dataSize = 6;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 1, 3, 3, 5, 6};
    int64_t buildData1[] = {111, 11, 333, 33, 11, 11};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(1)->SetNull(0);
    buildVecBatch->Get(1)->SetNull(2);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    const int32_t probeDataSize = 4;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[2] = {0, 1};
    int32_t probeHashColsCount = 2;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int64_t expectedDatas[4][expectedDataSize] = {
                {1, 2, 3, 4},
                {11, 22, 33, 44},
                {1, 0, 3, 0},
                {11, 0, 33, 0}};
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 4;
    int64_t expectedData0[expectedDatasSize] = {0, 0, 0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0, 0, 0};
    int64_t expectedData2[expectedDatasSize] = {1, 3, 5, 6};
    int64_t expectedData3[expectedDatasSize] = {111, 333, 11, 11};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(0)->SetNull(2);
    vectorBatch->Get(0)->SetNull(3);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    vectorBatch->Get(1)->SetNull(2);
    vectorBatch->Get(1)->SetNull(3);
    vectorBatch->Get(3)->SetNull(0);
    vectorBatch->Get(3)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestVariableKeysFullEqualityJoinWithBuildNull)
{
    const int32_t dataSize = 8;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t buildData0[dataSize] = {20, 14, 7, 19, 1, 20, 20, 13};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "35709", "35709", "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(1);
    buildVecBatch->Get(0)->SetNull(2);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 1;

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_FULL,
        buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    const int32_t probeDataSize = 6;
    int32_t probeData0[probeDataSize] = {20, 16, 13, 4, 20, 4};
    std::string probeData1[probeDataSize] = {"35709", "35709", "31904", "12477", "31904", "38721"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[2] = {0, 1};
    int32_t probeHashColsCount = 2;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 8;
    int32_t expectData0[expectDataSize] = {20, 20, 20, 16, 13, 4, 20, 4};
    std::string expectData1[expectDataSize] = {"35709", "35709", "35709", "35709", "31904", "12477", "31904", "38721"};
    int32_t expectData2[expectDataSize] = {20, 20, 20, 0, 13, 0, 0, 0};
    std::string expectData3[expectDataSize] = {"35709", "35709", "35709", "", "31904", "", "", ""};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData0));
    expectVecBatch->Append(CreateVarcharVector(expectData1, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVarcharVector(expectData3, expectDataSize));
    expectVecBatch->Get(2)->SetNull(3);
    expectVecBatch->Get(2)->SetNull(5);
    expectVecBatch->Get(2)->SetNull(6);
    expectVecBatch->Get(2)->SetNull(7);
    expectVecBatch->Get(3)->SetNull(3);
    expectVecBatch->Get(3)->SetNull(5);
    expectVecBatch->Get(3)->SetNull(6);
    expectVecBatch->Get(3)->SetNull(7);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);

    const int32_t expectedDatasSize = 4;
    int32_t expectedData0[expectedDatasSize] = {0, 0, 0, 0};
    std::string expectedData1[expectedDatasSize] = {"", "", "", ""};
    int32_t expectedData2[expectedDatasSize] = {0, 0, 1, 19};
    std::string expectedData3[expectedDatasSize] = {"31904", "35709", "35709", "31904"};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVarcharVector(expectedData1, expectedDatasSize);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVarcharVector(expectedData3, expectedDatasSize);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(0)->SetNull(2);
    vectorBatch->Get(0)->SetNull(3);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    vectorBatch->Get(1)->SetNull(2);
    vectorBatch->Get(1)->SetNull(3);
    vectorBatch->Get(2)->SetNull(0);
    vectorBatch->Get(2)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(vectorBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinChar)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    std::string probeData1[dataSize] = {"11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    std::string expectedData1[expectedDataSize] = {"11", "22", "33", "44"};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    std::string expectedData3[expectedDataSize] = {"11", "", "33", ""};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2), LongType(), VarcharType(3) }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    std::string expectedDatas1[expectedDatasSize] = {"", ""};
    int64_t expectedDatas2[expectedDatasSize] = {3, 1};
    std::string expectedDatas3[expectedDatasSize] = {"ccc", "aaa"};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVarcharVector(expectedDatas1, expectedDatasSize);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVarcharVector(expectedDatas3, expectedDatasSize);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinOperatorFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinDate32)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    int32_t buildData1[dataSize] = {123, 11, 321, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    int32_t probeData1[dataSize] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();

    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    int32_t expectedData1[expectedDataSize] = {11, 22, 33, 44};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    int32_t expectedData3[expectedDataSize] = {11, -1, 33, -1};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), Date32Type(DAY), LongType(), Date32Type(DAY) }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    int32_t expectedDatas1[expectedDatasSize] = {0, 0};
    int64_t expectedDatas2[expectedDatasSize] = {3, 1};
    int32_t expectedDatas3[expectedDatasSize] = {321, 123};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedDatas1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedDatas3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinOperatorFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinDecimal64)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), Decimal64Type(3, 0) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    int64_t buildData1[dataSize] = {123, 11, 321, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), Decimal64Type(2, 0) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    int64_t probeData1[dataSize] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), Decimal64Type(3, 0) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);

    auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    int64_t expectedData1[expectedDataSize] = {11, 22, 33, 44};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    int64_t expectedData3[expectedDataSize] = {11, -1, 33, -1};
    DataTypes expectedDataTypes(
        std::vector<DataTypePtr>({ LongType(), Decimal64Type(2, 0), LongType(), Decimal64Type(3, 0) }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    int64_t expectedDatas1[expectedDatasSize] = {0, 0};
    int64_t expectedDatas2[expectedDatasSize] = {3, 1};
    int64_t expectedDatas3[expectedDatasSize] = {321, 123};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedDatas1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedDatas3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinOperatorFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinDecimal128)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), Decimal128Type(3, 0) }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    Decimal128 buildData1[dataSize] = {Decimal128(123, 0), Decimal128(11, 0), Decimal128(321, 0), Decimal128(33, 0)};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), Decimal128Type(2, 0) }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    Decimal128 probeData1[dataSize] = {Decimal128(11, 0), Decimal128(22, 0), Decimal128(33, 0), Decimal128(44, 0)};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), Decimal128Type(3, 0) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();

    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    Decimal128 expectedData1[expectedDataSize] = {Decimal128(11, 0), Decimal128(22, 0), Decimal128(33, 0),
                                                  Decimal128(44, 0)};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    Decimal128 expectedData3[expectedDataSize] = {Decimal128(11, 0), Decimal128(0, 0), Decimal128(33, 0),
                                                  Decimal128(0, 0)};
    DataTypes expectedDataTypes(
        std::vector<DataTypePtr>({ LongType(), Decimal128Type(2, 0), LongType(), Decimal128Type(3, 0) }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    Decimal128 expectedDatas1[expectedDatasSize] = {0, 0};
    int64_t expectedDatas2[expectedDatasSize] = {1, 3};
    Decimal128 expectedDatas3[expectedDatasSize] = {Decimal128(123, 0), Decimal128(321, 0)};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedDatas1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedDatas3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinOperatorFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinShort)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), ShortType() }));
    int64_t buildData0[dataSize] = {1, 2, 3, 4};
    int16_t buildData1[dataSize] = {123, 11, 321, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), ShortType() }));
    int64_t probeData0[dataSize] = {1, 2, 3, 4};
    int16_t probeData1[dataSize] = {11, 22, 33, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), ShortType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedData0[expectedDataSize] = {1, 2, 3, 4};
    int16_t expectedData1[expectedDataSize] = {11, 22, 33, 44};
    int64_t expectedData2[expectedDataSize] = {2, 0, 4, 0};
    int16_t expectedData3[expectedDataSize] = {11, -1, 33, -1};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), ShortType(), LongType(), ShortType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData0, expectedData1,
        expectedData2, expectedData3);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    int16_t expectedDatas1[expectedDatasSize] = {0, 0};
    int64_t expectedDatas2[expectedDatasSize] = {3, 1};
    int16_t expectedDatas3[expectedDatasSize] = {321, 123};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedDatas1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedDatas3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinOperatorFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinDictionary)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    const DataTypePtr &dataType = buildTypes.GetType(1);
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*dataType, dataSize, ids, dataSize, buildData1));

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    const DataTypePtr &probeDataType = probeTypes.GetType(1);
    probeVecBatch->Append(CreateDictionaryVector(*probeDataType, dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[4][expectedDataSize] = {
        {1, 2, 3, 4},
        {11, 22, 33, 44},
        {2, 0, 4, 0},
        {11, 0, 33, 0}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    int64_t expectedDatas1[expectedDatasSize] = {0, 0};
    int64_t expectedDatas2[expectedDatasSize] = {3, 1};
    int64_t expectedDatas3[expectedDatasSize] = {333, 111};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedDatas1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedDatas3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinHasOutputNulls)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 0, 3, 0};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(1);
    buildVecBatch->Get(0)->SetNull(3);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    int64_t probeData0[dataSize] = {0, 2, 0, 4};
    std::string probeData1[dataSize] = {"11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    probeVecBatch->Get(0)->SetNull(0);
    probeVecBatch->Get(0)->SetNull(2);
    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    LookupJoinOperatorFactory *lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int32_t ids[4] = {0, 1, 2, 3};
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2), LongType(), VarcharType(3) }));
    int64_t expectedData0[expectedDataSize] = {0, 2, 0, 4};
    std::string expectedData1[expectedDataSize] = {"11", "22", "33", "44"};
    int64_t expectedData2[expectedDataSize] = {0, 0, 0, 0};
    std::string expectedData3[expectedDataSize] = {"11", "a", "33", "a"};
    auto expectedVector2 = CreateVector(expectedDataSize, expectedData2);
    expectedVector2->SetNull(0);
    expectedVector2->SetNull(1);
    expectedVector2->SetNull(2);
    expectedVector2->SetNull(3);
    auto expectedVector3 = CreateVarcharVector(expectedData3, expectedDataSize);
    auto expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(
        CreateDictionaryVector(*expectedTypes.GetType(0), expectedDataSize, ids, expectedDataSize, expectedData0));
    expectedVecBatch->Append(
        CreateDictionaryVector(*expectedTypes.GetType(1), expectedDataSize, ids, expectedDataSize, expectedData1));
    expectedVecBatch->Append(expectedVector2);
    expectedVecBatch->Append(expectedVector3);
    expectedVecBatch->Get(0)->SetNull(0);
    expectedVecBatch->Get(0)->SetNull(2);
    expectedVecBatch->Get(3)->SetNull(1);
    expectedVecBatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);
    const int32_t expectedDatasSize = 2;
    int64_t expectedDatas0[expectedDatasSize] = {0, 0};
    std::string expectedDatas1[expectedDatasSize] = {"", ""};
    int64_t expectedDatas2[expectedDatasSize] = {3, 1};
    std::string expectedDatas3[expectedDatasSize] = {"ccc", "aaa"};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedDatas0);
    auto expectedVec1 = CreateVarcharVector(expectedDatas1, expectedDatasSize);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedDatas2);
    auto expectedVec3 = CreateVarcharVector(expectedDatas3, expectedDatasSize);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftSemiEqualityJoin)
{
    // construct input data
    const int32_t dataSize = 8;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 0, 3, 0, 1, 0, 2, 2};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33", "aaa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT_SEMI, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    int64_t probeData0[dataSize] = {0, 2, 0, 1, 0, 2, 0, 4};
    std::string probeData1[dataSize] = {"11", "22", "33", "44", "11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    probeVecBatch->Get(0)->SetNull(2);
    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    LookupJoinOperatorFactory *lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));

    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 6;
    int64_t expectedData0[expectedDataSize] = {0, 2, 1, 0, 2, 0};
    std::string expectedData1[expectedDataSize] = {"11", "22", "44", "11", "22", "33"};
    auto expectedVector0 = CreateVector<int64_t>(expectedDataSize, expectedData0);
    auto expectedVector1 = CreateVarcharVector(expectedData1, expectedDataSize);
    auto expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(expectedVector0);
    expectedVecBatch->Append(expectedVector1);
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftSemiEqualityJoinWithOddDataSize)
{
    // construct input data
    const int32_t dataSize = 7;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 0, 3, 0, 1, 0, 2};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33", "aaa", "11", "ccc"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_LEFT_SEMI, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    int64_t probeData0[dataSize] = {0, 2, 0, 1, 0, 2, 0};
    std::string probeData1[dataSize] = {"11", "22", "33", "44", "11", "22", "33"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    probeVecBatch->Get(0)->SetNull(2);
    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    LookupJoinOperatorFactory *lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
            probeTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
            buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));

    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 6;
    int64_t expectedData0[expectedDataSize] = {0, 2, 1, 0, 2, 0};
    std::string expectedData1[expectedDataSize] = {"11", "22", "44", "11", "22", "33"};
    auto expectedVector0 = CreateVector<int64_t>(expectedDataSize, expectedData0);
    auto expectedVector1 = CreateVarcharVector(expectedData1, expectedDataSize);
    auto expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(expectedVector0);
    expectedVecBatch->Append(expectedVector1);
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftSemiEqualityJoinWithCharFilter)
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                            "35709", "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_LEFT_SEMI,
        buildTypes, buildJoinCols, joinColsCount, operatorCount);
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 20, 7};
    std::string probeData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                            "88371", "35709"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    static_cast<Vector<std::string_view> *>(probeVecBatch->Get(1))->SetNull(dataSize - 1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 5;
    int32_t expectData0[expectDataSize] = {20, 16, 20, 19, 20};
    std::string expectData1[expectDataSize] = {"35709", "35709", "31904", "35709", "88371"};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData0));
    expectVecBatch->Append(CreateVarcharVector(expectData1, expectDataSize));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestRightEqualityJoin)
{
    const int32_t dataSize = 8;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    int64_t buildData0[dataSize] = {1, 0, 3, 0, 1, 0, 2, 2};
    std::string buildData1[dataSize] = {"aaa", "11", "ccc", "33", "aa", "11", "ccc", "33"};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_RIGHT, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2) }));
    const int32_t probeDataSize = 4;
    int64_t probeData0[probeDataSize] = {0, 2, 0, 1};
    std::string probeData1[probeDataSize] = {"11", "22", "33", "44"};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);
    probeVecBatch->Get(0)->SetNull(2);
    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(3) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    LookupJoinOperatorFactory *lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));

    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 8;
    int64_t expectedData0[expectedDataSize] = {0, 0, 0, 2, 2, 1, 1, 0};
    std::string expectedData1[expectedDataSize] = {"11", "11", "11", "22", "22", "44", "44", "33"};
    int64_t expectedData2[expectedDataSize] = {0, 0, 0, 2, 2, 1, 1, 0};
    std::string expectedData3[expectedDataSize] = {"11", "33", "11", "ccc", "33", "aaa", "aa", ""};
    auto expectedVector0 = CreateVector<int64_t>(expectedDataSize, expectedData0);
    expectedVector0->SetNull(7);
    auto expectedVector1 = CreateVarcharVector(expectedData1, expectedDataSize);
    auto expectedVector2 = CreateVector<int64_t>(expectedDataSize, expectedData2);
    expectedVector2->SetNull(7);
    auto expectedVector3 = CreateVarcharVector(expectedData3, expectedDataSize);
    expectedVector3->SetNull(7);
    auto expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(expectedVector0);
    expectedVecBatch->Append(expectedVector1);
    expectedVecBatch->Append(expectedVector2);
    expectedVecBatch->Append(expectedVector3);
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(2), LongType(), VarcharType(3) }));
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}


TEST(NativeOmniJoinTest, TestRightEqualityJoinWithCharFilter)
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t buildData0[dataSize] = {19, 14, 7, 19, 1, 20, 10, 13, 20, 16};
    std::string buildData1[dataSize] = {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904",
                                        "35709", "31904"};
    auto buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    string filterExpression = "$operator$NOT_EQUAL:4(substr:15(#1, 1:1, 5:1), substr:15(#3, 1:1, 5:1))";

    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_RIGHT,
        buildTypes, buildJoinCols, joinColsCount, operatorCount);
    // create the filter expression
    omniruntime::expressions::Expr *joinFilter = CreateJoinFilterExprWithChar();
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    int32_t probeData0[dataSize] = {20, 16, 13, 4, 20, 4, 22, 19, 20, 7};
    std::string probeData1[dataSize] = {"35709", "35709", "31904", "12477", "31904", "38721", "90419", "35709",
                                        "88371", "35709"};
    auto probeVecBatch = CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
    static_cast<Vector<std::string_view> *>(probeVecBatch->Get(1))->SetNull(dataSize - 1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ IntType(), VarcharType(5) }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, joinFilter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 11;
    int32_t expectData0[expectDataSize] = {20, 16, 13, 20, 19, 20, 20, 7, 4, 4, 22};
    std::string expectData1[expectDataSize] = {"35709", "35709", "31904", "31904", "35709",
                                               "88371", "88371", "", "12477", "38721", "90419"};
    int32_t expectData2[expectDataSize] = {20, 16, 0, 20, 19, 20, 20, 0, 0, 0, 0};
    std::string expectData3[expectDataSize] = {"31904", "31904", "", "35709", "31904", "31904", "35709", "", "", "",
                                               ""};
    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData0));
    expectVecBatch->Append(CreateVarcharVector(expectData1, expectDataSize));
    expectVecBatch->Append(CreateVector<int32_t>(expectDataSize, expectData2));
    expectVecBatch->Append(CreateVarcharVector(expectData3, expectDataSize));
    static_cast<Vector<std::string_view> *>(expectVecBatch->Get(1))->SetNull(7);
    auto nullIndex = {2, 7, 8, 9, 10};
    for (auto idx : nullIndex) {
        expectVecBatch->Get(2)->SetNull(idx);
        static_cast<Vector<std::string_view> *>(expectVecBatch->Get(3))->SetNull(idx);
    }
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs({ joinFilter });
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftAntiEqualityJoin)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT_ANTI, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    HashBuilderOperator *hashBuilderOperator =
        dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    int32_t probeDataSize = 6;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4, 3, 6};
    int64_t probeData1[] = {11, 22, 33, 44, 22, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[2][expectedDataSize] = {{2, 4, 3, 6}, {22, 44, 22, 44}};
    AssertVecBatchEquals(outputVecBatch, probeTypes.GetSize(), expectedDataSize, expectedDatas[0], expectedDatas[1]);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftAntiEqualityJoinWithOddProbeDataSize)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_LEFT_ANTI, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    HashBuilderOperator *hashBuilderOperator =
            dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    int32_t probeDataSize = 7;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4, 3, 6, 7};
    int64_t probeData1[] = {11, 22, 33, 44, 22, 44, 77};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount, probeHashCols,
                                                                       probeHashColsCount, buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes, hashBuilderFactoryAddr,
                                                                       nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 5;
    int64_t expectedDatas[2][expectedDataSize] = {{2, 4, 3, 6, 7}, {22, 44, 22, 44, 77}};
    AssertVecBatchEquals(outputVecBatch, probeTypes.GetSize(), expectedDataSize, expectedDatas[0], expectedDatas[1]);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestInnerEqualityJoinOfArrayTable)
{
    const int32_t dataSize = 6;
    DataTypes buildTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() }));
    int16_t buildData0[] = {1, 2, 3, 4, 1, 2};
    int16_t buildData1[] = {111, 11, 333, 33, 20, 16};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() }));
    int16_t probeData0[] = {1, 2, 3, 4};
    int16_t probeData1[] = {11, 22, 33, 44};
    const int32_t probeDataSize = 4;
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 6;
    int16_t expectedDatas[4][expectedDataSize] = {
                {1, 1, 2, 2, 3, 4},
                {11, 11, 22, 22, 33, 44},
                {1, 1, 2, 2, 3, 4},
                {111, 20, 11, 16, 333, 33}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType(), ShortType(), ShortType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestLeftAntiEqualityJoinWithArrayProbe)
{
    // construct input data
    const int32_t dataSize = 7;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4, 6, 7, 8};
    int64_t buildData1[] = {2, 11, 7, 3, 4, 5, 5, 6};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_LEFT_ANTI, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    HashBuilderOperator *hashBuilderOperator =
            dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    int32_t probeDataSize = 7;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4, 3, 6, 7};
    int64_t probeData1[] = {28, 2, 3, 10, 0, 15, 8};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount, probeHashCols,
                                                                       probeHashColsCount, buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes, hashBuilderFactoryAddr,
                                                                       nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 5;
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t expectedDatas[2][expectedDataSize] = {{1, 4, 3, 6, 7}, {28, 10, 0, 15, 8}};
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1]);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinOfArrayTable)
{
    const int32_t dataSize = 6;
    DataTypes buildTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() }));
    int16_t buildData0[] = {1, 2, 3, 4, 1, 2};
    int16_t buildData1[] = {111, 11, 333, 33, 20, 16};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(4);
    buildVecBatch->Get(0)->SetNull(5);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() }));
    int16_t probeData0[] = {1, 2, 3, 4};
    int16_t probeData1[] = {11, 22, 33, 44};
    const int32_t probeDataSize = 4;
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    int16_t expectedDatas[4][expectedDataSize] = {
                {1, 2, 3, 4},
                {11, 22, 33, 44},
                {1, 2, 3, 4},
                {111, 11, 333, 33}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType(), ShortType(), ShortType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
                                               expectedDatas[2], expectedDatas[3]);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);

    const int32_t remainSlotSize = 2;
    int16_t expectedData0[remainSlotSize] = {0, 0};
    int16_t expectedData1[remainSlotSize] = {0, 0};
    int16_t expectedData2[remainSlotSize] = {0, 0};
    int16_t expectedData3[remainSlotSize] = {20, 16};
    auto expectedVec0 = CreateVector(remainSlotSize, expectedData0);
    auto expectedVec1 = CreateVector(remainSlotSize, expectedData1);
    auto expectedVec2 = CreateVector(remainSlotSize, expectedData2);
    auto expectedVec3 = CreateVector(remainSlotSize, expectedData3);
    auto vectorBatch = new VectorBatch(remainSlotSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    vectorBatch->Get(2)->SetNull(0);
    vectorBatch->Get(2)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestFullEqualityJoinOfArrayTableWithOddDataSize)
{
    const int32_t dataSize = 7;
    DataTypes buildTypes(std::vector<DataTypePtr>({ShortType(), ShortType()}));
    int16_t buildData0[] = {1, 2, 3, 4, 1, 2, 8};
    int16_t buildData1[] = {111, 11, 333, 33, 20, 16, 88};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
    buildVecBatch->Get(0)->SetNull(4);
    buildVecBatch->Get(0)->SetNull(5);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ShortType(), ShortType()}));
    int16_t probeData0[] = {1, 2, 3, 4, 5, 6, 7};
    int16_t probeData1[] = {11, 22, 33, 44, 55, 66, 77};
    const int32_t probeDataSize = 7;
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ShortType(), ShortType()}));
    auto hashBuilderFactoryAddr = (int64_t) hashBuilderFactory;
    auto lookupJoinFactory =
            LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
                                                                       probeOutputColsCount,
                                                                       probeHashCols,
                                                                       probeHashColsCount,
                                                                       buildOutputCols,
                                                                       buildOutputColsCount,
                                                                       buildOutputTypes,
                                                                       hashBuilderFactoryAddr, nullptr, false,
                                                                       nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 7;
    int16_t expectedDatas[4][expectedDataSize] = {
            {1,   2,  3,   4,  5,  6,  7},
            {11,  22, 33,  44, 55, 66, 77},
            {1,   2,  3,   4,  -1, -1, -1},
            {111, 11, 333, 33, -1, -1, -1}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType(), ShortType(), ShortType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
                                               expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(4);
    expectedVecbatch->Get(2)->SetNull(5);
    expectedVecbatch->Get(2)->SetNull(6);
    expectedVecbatch->Get(3)->SetNull(4);
    expectedVecbatch->Get(3)->SetNull(5);
    expectedVecbatch->Get(3)->SetNull(6);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    auto lookupOuterJoinFactory =
            LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
                                                                                 probeOutputCols,
                                                                                 probeOutputColsCount,
                                                                                 buildOutputCols,
                                                                                 buildOutputTypes,
                                                                                 hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    VectorBatch *appendOutput;
    lookupOuterJoinOperator->GetOutput(&appendOutput);

    const int32_t remainSlotSize = 3;
    int16_t expectedData0[remainSlotSize] = {0, 0, 0};
    int16_t expectedData1[remainSlotSize] = {0, 0, 0};
    int16_t expectedData2[remainSlotSize] = {8, 0, 0};
    int16_t expectedData3[remainSlotSize] = {88, 20, 16};
    auto expectedVec0 = CreateVector(remainSlotSize, expectedData0);
    auto expectedVec1 = CreateVector(remainSlotSize, expectedData1);
    auto expectedVec2 = CreateVector(remainSlotSize, expectedData2);
    auto expectedVec3 = CreateVector(remainSlotSize, expectedData3);
    auto vectorBatch = new VectorBatch(remainSlotSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(0)->SetNull(2);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    vectorBatch->Get(1)->SetNull(2);
    vectorBatch->Get(2)->SetNull(1);
    vectorBatch->Get(2)->SetNull(2);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(vectorBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestExistenceEqualityJoin)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);

    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_EXISTENCE, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    HashBuilderOperator *hashBuilderOperator =
            dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    int32_t probeDataSize = 6;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4, 3, 6};
    int64_t probeData1[] = {11, 22, 33, 44, 22, 44};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    int32_t probeOutputCols[0] = {};
    int32_t probeOutputColsCount = 0;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    int32_t buildOutputCols[0] = {};
    int32_t buildOutputColsCount = 0;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
            probeTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount,
            buildOutputCols, buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr,
            nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 6;
    bool expectedDatas[1][expectedDataSize] = {{true, false, true, false, false, false}};
    AssertVecBatchEquals(outputVecBatch, 1, expectedDataSize, expectedDatas[0]);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

TEST(NativeOmniJoinTest, TestMultiKeysFullEqualityJoinInteger)
{
    const int32_t dataSize = 6;
    DataTypes buildTypes(std::vector<DataTypePtr>({ ShortType(), IntType(), LongType() }));
    int16_t buildData0[] = {1, 2, 3, 4, 5, 6};
    int32_t buildData1[] = {11, 22, 33, 44, 55, 66};
    int64_t buildData2[] = {111, 222, 333, 444, 555, 666};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1, buildData2);

    int32_t buildJoinCols[3] = {0, 1, 2};
    int32_t joinColsCount = 3;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    const int32_t probeDataSize = 4;
    DataTypes probeTypes(std::vector<DataTypePtr>({ ShortType(), IntType(), LongType() }));
    int16_t probeData0[] = {1, 3, 5, 7};
    int32_t probeData1[] = {11, 33, 55, 77};
    int64_t probeData2[] = {111, 333, 555, 777};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1, probeData2);

    int32_t probeOutputCols[3] = {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    int32_t probeHashCols[3] = {0, 1, 2};
    int32_t probeHashColsCount = 3;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ ShortType(), IntType(), LongType() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    DataTypes expectedDataTypes(std::vector<DataTypePtr>(
        { ShortType(), IntType(), LongType(), ShortType(), IntType(), LongType() }));
    int16_t expectedDatas0[expectedDataSize] = {1, 3, 5, 7};
    int32_t expectedDatas1[expectedDataSize] = {11, 33, 55, 77};
    int64_t expectedDatas2[expectedDataSize] = {111, 333, 555, 777};
    int16_t expectedDatas3[expectedDataSize] = {1, 3, 5, 0};
    int32_t expectedDatas4[expectedDataSize] = {11, 33, 55, 0};
    int64_t expectedDatas5[expectedDataSize] = {111, 333, 555, 0};
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas0, expectedDatas1,
        expectedDatas2, expectedDatas3, expectedDatas4, expectedDatas5);
    expectedVecbatch->Get(3)->SetNull(3);
    expectedVecbatch->Get(4)->SetNull(3);
    expectedVecbatch->Get(5)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

TEST(NativeOmniJoinTest, TestMultiKeysFullEqualityJoinDecimal64)
{
    const int32_t dataSize = 6;
    DataTypes buildTypes(std::vector<DataTypePtr>({ ShortType(), IntType(), Decimal64Type() }));
    int16_t buildData0[] = {1, 2, 3, 4, 5, 6};
    int32_t buildData1[] = {11, 22, 33, 44, 55, 66};
    double buildData2[] = {1.11, 2.22, 3.33, 4.44, 5.55, 6.66};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1, buildData2);

    int32_t buildJoinCols[3] = {0, 1, 2};
    int32_t joinColsCount = 3;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(CreateTestOperator(hashBuilderFactory));
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    const int32_t probeDataSize = 4;
    DataTypes probeTypes(std::vector<DataTypePtr>({ ShortType(), IntType(), Decimal64Type() }));
    int16_t probeData0[] = {1, 3, 5, 7};
    int32_t probeData1[] = {11, 33, 55, 77};
    double probeData2[] = {1.11, 3.33, 5.55, 7.77};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1, probeData2);

    int32_t probeOutputCols[3] = {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    int32_t probeHashCols[3] = {0, 1, 2};
    int32_t probeHashColsCount = 3;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ ShortType(), IntType(), Decimal64Type() }));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(CreateTestOperator(lookupJoinFactory));
    auto lookupOuterJoinFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    const int32_t expectedDataSize = 4;
    DataTypes expectedDataTypes(std::vector<DataTypePtr>(
        { ShortType(), IntType(), Decimal64Type(), ShortType(), IntType(), Decimal64Type() }));
    int16_t expectedDatas0[expectedDataSize] = {1, 3, 5, 7};
    int32_t expectedDatas1[expectedDataSize] = {11, 33, 55, 77};
    double expectedDatas2[expectedDataSize] = {1.11, 3.33, 5.55, 7.77};
    int16_t expectedDatas3[expectedDataSize] = {1, 3, 5, 0};
    int32_t expectedDatas4[expectedDataSize] = {11, 33, 55, 0};
    double expectedDatas5[expectedDataSize] = {1.11, 3.33, 5.55, 0};
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas0, expectedDatas1,
        expectedDatas2, expectedDatas3, expectedDatas4, expectedDatas5);
    expectedVecbatch->Get(3)->SetNull(3);
    expectedVecbatch->Get(4)->SetNull(3);
    expectedVecbatch->Get(5)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecbatch));

    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory, lookupOuterJoinFactory);
}

// Inner join with ConstVector as probe data column
TEST(NativeOmniJoinTest, TestInnerJoinWithProbeConstVector)
{
    // Build side: regular vectors
    const int32_t buildDataSize = 5;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[buildDataSize] = {1, 2, 3, 4, 5};
    int64_t buildData1[buildDataSize] = {10, 20, 30, 40, 50};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    // Probe side: col0 (regular join key), col1 (ConstVector data)
    const int32_t probeDataSize = 4;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto *probeCol0 = new Vector<int64_t>(probeDataSize);
    int64_t probeData0[] = {1, 3, 5, 7};
    for (int32_t i = 0; i < probeDataSize; i++) {
        probeCol0->SetValue(i, probeData0[i]);
    }
    auto *probeCol1 = new ConstVector<int64_t>(99L, OMNI_LONG, probeDataSize);
    auto *probeVecBatch = new VectorBatch(probeDataSize);
    probeVecBatch->Append(probeCol0);
    probeVecBatch->Append(probeCol1);

    // Setup hash build
    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, 1);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    // Setup probe
    int32_t probeOutputCols[1] = {1};
    int32_t probeOutputColsCount = 1;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t buildOutputCols[1] = {1};
    int32_t buildOutputColsCount = 1;
    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    // Expected: probe(1)->build(1,10), probe(3)->build(3,30), probe(5)->build(5,50), probe(7) no match
    // Output: probe_col1(99), build_col1(10,30,50) => 3 rows
    const int32_t expectedDataSize = 3;
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t expectedData0[expectedDataSize] = {99, 99, 99};
    int64_t expectedData1[expectedDataSize] = {10, 30, 50};
    auto *expectedVecBatch = CreateVectorBatch(expectedTypes, expectedDataSize, expectedData0, expectedData1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

// Inner join with ConstVector as build data column
TEST(NativeOmniJoinTest, TestInnerJoinWithBuildConstVector)
{
    // Build side: col0 (regular join key), col1 (ConstVector data)
    const int32_t buildDataSize = 5;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto *buildCol0 = new Vector<int64_t>(buildDataSize);
    int64_t buildData0[] = {1, 2, 3, 4, 5};
    for (int32_t i = 0; i < buildDataSize; i++) {
        buildCol0->SetValue(i, buildData0[i]);
    }
    auto *buildCol1 = new ConstVector<int64_t>(88L, OMNI_LONG, buildDataSize);
    auto *buildVecBatch = new VectorBatch(buildDataSize);
    buildVecBatch->Append(buildCol0);
    buildVecBatch->Append(buildCol1);

    // Probe side: regular vectors
    const int32_t probeDataSize = 3;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[probeDataSize] = {1, 3, 5};
    int64_t probeData1[probeDataSize] = {100, 200, 300};
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeDataSize, probeData0, probeData1);

    // Setup hash build
    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, 1);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    // Setup probe
    int32_t probeOutputCols[1] = {1};
    int32_t probeOutputColsCount = 1;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t buildOutputCols[1] = {1};
    int32_t buildOutputColsCount = 1;
    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);

    // Expected: 3 matches - output probe_col1, build_col1
    // (100, 88), (200, 88), (300, 88)
    const int32_t expectedDataSize = 3;
    DataTypes expectedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t expectedData0[expectedDataSize] = {100, 200, 300};
    int64_t expectedData1[expectedDataSize] = {88, 88, 88};
    auto *expectedVecBatch = CreateVectorBatch(expectedTypes, expectedDataSize, expectedData0, expectedData1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedVecBatch));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

// Inner join with ConstVector as probe join key (all rows match same build rows)
TEST(NativeOmniJoinTest, TestInnerJoinWithConstVectorAsProbeJoinKey)
{
    // Build side: regular vectors
    const int32_t buildDataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[buildDataSize] = {5, 5, 3, 7};
    int64_t buildData1[buildDataSize] = {10, 20, 30, 40};
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, buildDataSize, buildData0, buildData1);

    // Probe side: col0 (ConstVector join key, all 5), col1 (regular data)
    const int32_t probeDataSize = 3;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto *probeCol0 = new ConstVector<int64_t>(5L, OMNI_LONG, probeDataSize);
    auto *probeCol1 = new Vector<int64_t>(probeDataSize);
    int64_t probeData1[] = {100, 200, 300};
    for (int32_t i = 0; i < probeDataSize; i++) {
        probeCol1->SetValue(i, probeData1[i]);
    }
    auto *probeVecBatch = new VectorBatch(probeDataSize);
    probeVecBatch->Append(probeCol0);
    probeVecBatch->Append(probeCol1);

    // Setup hash build
    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildJoinCols, joinColsCount, 1);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilderOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    // Setup probe
    int32_t probeOutputCols[1] = {1};
    int32_t probeOutputColsCount = 1;
    int32_t probeHashCols[1] = {0};
    int32_t probeHashColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t buildOutputCols[1] = {1};
    int32_t buildOutputColsCount = 1;
    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(probeTypes, probeOutputCols,
        probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols, buildOutputColsCount,
        buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);

    // Collect all output batches
    std::vector<VectorBatch *> outputs;
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *result = nullptr;
        lookupJoinOperator->GetOutput(&result);
        if (result != nullptr) {
            outputs.push_back(result);
        }
    }

    // Each probe row (all join key=5) matches 2 build rows (build[0] and build[1] have key=5)
    // 3 probe rows * 2 matches = 6 result rows
    int32_t totalRows = 0;
    for (auto *batch : outputs) {
        totalRows += batch->GetRowCount();
    }
    EXPECT_EQ(totalRows, 6);

    VectorHelper::FreeVecBatches(outputs);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    DeleteJoinOperatorFactory(hashBuilderFactory, lookupJoinFactory);
}

}