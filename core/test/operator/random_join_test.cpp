/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: random lookup join operator test implementations
 */
#include <vector>
#include <random>
#include <ctime>

#include "gtest/gtest.h"
#include "operator/join/hash_builder.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "vector/vector_helper.h"
#include "util/config_util.h"
#include "util/test_util.h"

namespace RandomJoinTest {
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace TestUtil;

template <typename T> VectorBatch *CreateSingleColumnRandomData(DataTypes &buildTypes, int rowCnt, bool isDic)
{
    std::default_random_engine e(time(nullptr));
    T *data = new T[rowCnt];
    if constexpr (!std::is_same_v<T, Decimal128>) {
        std::uniform_real_distribution<> u(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        for (int i = 0; i < rowCnt; ++i) {
            data[i] = static_cast<T>(u(e));
        }
    } else {
        std::uniform_real_distribution<> u(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
        for (int i = 0; i < rowCnt; ++i) {
            data[i] = Decimal128(static_cast<int64_t>(u(e)), static_cast<uint64_t>(u(e)));
        }
    }

    VectorBatch *buildVecBatch = nullptr;
    if (!isDic) {
        buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data);
    } else {
        auto *ids = new int32_t[rowCnt];
        for (int i = 0; i < rowCnt; ++i) {
            ids[i] = i;
        }
        auto expectedVec0 = CreateVector<T>(rowCnt, data);
        auto expectedDictVec0 =
            VectorHelper::CreateDictionary(ids, rowCnt, reinterpret_cast<Vector<T> *>(expectedVec0));
        buildVecBatch = new VectorBatch(rowCnt);
        buildVecBatch->Append(expectedDictVec0);
        delete expectedVec0;
        delete[] ids;
    }
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }
    delete[] data;
    return buildVecBatch;
}

template <typename T> VectorBatch *CreateTwoColumnRandomData(DataTypes &buildTypes, int rowCnt, bool isDic)
{
    std::default_random_engine e(time(nullptr));
    T *data0 = new T[rowCnt];
    T *data1 = new T[rowCnt];
    if constexpr (!std::is_same_v<T, Decimal128>) {
        std::uniform_real_distribution<> u(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        for (int i = 0; i < rowCnt; ++i) {
            data0[i] = static_cast<T>(u(e));
            data1[i] = static_cast<T>(u(e));
        }
    } else {
        std::uniform_real_distribution<> u(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
        for (int i = 0; i < rowCnt; ++i) {
            data0[i] = Decimal128(static_cast<int64_t>(u(e)), static_cast<uint64_t>(u(e)));
            data1[i] = Decimal128(static_cast<int64_t>(u(e)), static_cast<uint64_t>(u(e)));
        }
    }

    VectorBatch *buildVecBatch = nullptr;
    if (!isDic) {
        buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data1, data0);
    } else {
        auto *ids = new int32_t[rowCnt];
        for (int i = 0; i < rowCnt; ++i) {
            ids[i] = i;
        }
        auto expectedVec0 = CreateVector<T>(rowCnt, data0);
        auto expectedDictVec0 =
            VectorHelper::CreateDictionary(ids, rowCnt, reinterpret_cast<Vector<T> *>(expectedVec0));
        auto expectedVec1 = CreateVector<T>(rowCnt, data1);
        auto expectedDictVec1 =
            VectorHelper::CreateDictionary(ids, rowCnt, reinterpret_cast<Vector<T> *>(expectedVec1));
        buildVecBatch = new VectorBatch(rowCnt);
        buildVecBatch->Append(expectedDictVec0);
        buildVecBatch->Append(expectedDictVec1);
        delete expectedVec0;
        delete expectedVec1;
        delete[] ids;
    }
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }

    delete[] data0;
    delete[] data1;
    return buildVecBatch;
}

VectorBatch *CreateVariableSizeRandomData(DataTypes &buildTypes, int rowCnt, bool isDic)
{
    std::default_random_engine e(time(nullptr));
    auto *data0 = new std::string[rowCnt];
    auto *data1 = new std::string[rowCnt];
    int32_t maxLen = 17;
    std::uniform_real_distribution<> u(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
    for (int i = 0; i < rowCnt; ++i) {
        for (int j = 0; j < maxLen; ++j) {
            data0[i] += std::to_string(u(e));
            data1[i] += std::to_string(u(e));
        }
    }

    VectorBatch *buildVecBatch = nullptr;
    if (!isDic) {
        buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data1, data0);
    } else {
        auto *ids = new int32_t[rowCnt];
        for (int i = 0; i < rowCnt; ++i) {
            ids[i] = i;
        }
        auto expectedVec0 = CreateVarcharVector(data0, rowCnt);
        auto expectedDictVec0 = CreateDictionary<OMNI_VARCHAR>(expectedVec0, ids, rowCnt);
        auto expectedVec1 = CreateVarcharVector(data1, rowCnt);
        auto expectedDictVec1 = CreateDictionary<OMNI_VARCHAR>(expectedVec0, ids, rowCnt);
        buildVecBatch = new VectorBatch(rowCnt);
        buildVecBatch->Append(expectedDictVec0);
        buildVecBatch->Append(expectedDictVec1);
        delete expectedVec0;
        delete expectedVec1;
        delete[] ids;
    }
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }

    delete[] data0;
    delete[] data1;
    return buildVecBatch;
}

template <typename T>
void StressTestSingleHashBuilderForSingleKey(DataTypes buildTypes, JoinType joinType, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnRandomData<T>(buildTypes, dataSize, false);
    VectorBatch *dicVecBatch = CreateSingleColumnRandomData<T>(buildTypes, dataSize, true);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator->AddInput(plainInput);
        hashBuilderOperator->AddInput(dicInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
}

template <typename T>
void StressTestTwoHashBuilderForSingleKey(DataTypes buildTypes, JoinType joinType, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnRandomData<T>(buildTypes, dataSize, false);
    VectorBatch *dicVecBatch = CreateSingleColumnRandomData<T>(buildTypes, dataSize, true);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 2;
    VectorBatch *hashBuildOutput = nullptr;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
        hashBuilderOperator1->AddInput(dicInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
        hashBuilderOperator2->AddInput(dicInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
}

template <typename T>
void StressTestSingleHashBuilderForTwoFixedKey(DataTypes buildTypes, JoinType joinType, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateTwoColumnRandomData<T>(buildTypes, dataSize, false);
    VectorBatch *dicVecBatch = CreateTwoColumnRandomData<T>(buildTypes, dataSize, true);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator->AddInput(plainInput);
        hashBuilderOperator->AddInput(dicInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
}

template <typename T>
void StressTestTwoHashBuilderForTwoFixedKey(DataTypes buildTypes, JoinType joinType, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateTwoColumnRandomData<T>(buildTypes, dataSize, false);
    VectorBatch *dicVecBatch = CreateTwoColumnRandomData<T>(buildTypes, dataSize, true);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 2;
    VectorBatch *hashBuildOutput = nullptr;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
        hashBuilderOperator1->AddInput(dicInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
        hashBuilderOperator2->AddInput(dicInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
}

void StressTestSingleHashBuilderForVariableKey(DataTypes buildTypes, JoinType joinType, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateVariableSizeRandomData(buildTypes, dataSize, false);
    VectorBatch *dicVecBatch = CreateVariableSizeRandomData(buildTypes, dataSize, true);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator->AddInput(plainInput);
        hashBuilderOperator->AddInput(dicInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
}

void StressTestTwoHashBuilderForVariableKey(DataTypes buildTypes, JoinType joinType, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateVariableSizeRandomData(buildTypes, dataSize, false);
    VectorBatch *dicVecBatch = CreateVariableSizeRandomData(buildTypes, dataSize, true);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 2;
    VectorBatch *hashBuildOutput = nullptr;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
        hashBuilderOperator1->AddInput(dicInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
        hashBuilderOperator2->AddInput(dicInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
}

template <typename T> VectorBatch *CreateSingleColumnSequenceDataFromStart(DataTypes &buildTypes, int rowCnt, int start)
{
    T *data = new T[rowCnt];
    for (int i = 0; i < rowCnt; ++i) {
        data[i] = start + i;
    }
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data);
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }

    delete[] data;
    return buildVecBatch;
}

template <typename T>
VectorBatch *CreateSingleColumnSequenceDataFromStartWithStride(DataTypes &buildTypes, int rowCnt, int start, int stride)
{
    T *data = new T[rowCnt];
    for (int i = 0; i < rowCnt; ++i) {
        data[i] = start + i * stride;
    }
    VectorBatch *buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data);
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }

    delete[] data;
    return buildVecBatch;
}

template <typename T>
void StressTestSingleHashBuilderForArrayTable(DataTypes buildTypes, JoinType joinType, int start, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, start);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator->AddInput(plainInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
}

template <typename T>
void StressTestTwoHashBuilderForArrayTable(DataTypes buildTypes, JoinType joinType, int start, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, start);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 2;
    VectorBatch *hashBuildOutput = nullptr;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
}

template <typename T>
void StressTestTwoHashBuilderForArrayTableCornerCase(DataTypes buildTypes, JoinType joinType, int start,
    int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStartWithStride<T>(buildTypes, dataSize, start, 10);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 2;
    VectorBatch *hashBuildOutput = nullptr;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
}

TEST(RandomJoinTest, TestSingleHashBuilderForNormalHashTable)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto time : repeatTimes) {
            StressTestSingleHashBuilderForSingleKey<bool>(DataTypes(std::vector<DataTypePtr>({ BooleanType() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<double>(DataTypes(std::vector<DataTypePtr>({ DoubleType() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Date64Type() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })),
                joinType, time);
            StressTestSingleHashBuilderForSingleKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type() })), joinType, time);

            StressTestSingleHashBuilderForTwoFixedKey<bool>(
                DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<int16_t>(
                DataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ IntType(), IntType() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ Date32Type(), Date32Type() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<double>(
                DataTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ LongType(), LongType() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Date64Type(), Date64Type() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), Decimal64Type() })), joinType, time);
            StressTestSingleHashBuilderForTwoFixedKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type(), Decimal128Type() })), joinType, time);

            StressTestSingleHashBuilderForVariableKey(
                DataTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() })), joinType, time);
        }
    }
}

TEST(RandomJoinTest, TestTwoHashBuilderForNormalHashTable)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto time : repeatTimes) {
            StressTestTwoHashBuilderForSingleKey<bool>(DataTypes(std::vector<DataTypePtr>({ BooleanType() })), joinType,
                time);
            StressTestTwoHashBuilderForSingleKey<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                joinType, time);
            StressTestTwoHashBuilderForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType,
                time);
            StressTestTwoHashBuilderForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, time);
            StressTestTwoHashBuilderForSingleKey<double>(DataTypes(std::vector<DataTypePtr>({ DoubleType() })),
                joinType, time);
            StressTestTwoHashBuilderForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType,
                time);
            StressTestTwoHashBuilderForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Date64Type() })),
                joinType, time);
            StressTestTwoHashBuilderForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })),
                joinType, time);
            StressTestTwoHashBuilderForSingleKey<Decimal128>(DataTypes(std::vector<DataTypePtr>({ Decimal128Type() })),
                joinType, time);

            StressTestTwoHashBuilderForTwoFixedKey<bool>(
                DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<int16_t>(
                DataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ IntType(), IntType() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ Date32Type(), Date32Type() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<double>(
                DataTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ LongType(), LongType() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Date64Type(), Date64Type() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), Decimal64Type() })), joinType, time);
            StressTestTwoHashBuilderForTwoFixedKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type(), Decimal128Type() })), joinType, time);

            StressTestTwoHashBuilderForVariableKey(
                DataTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() })), joinType, time);
        }
    }
}

template <typename T>
void StressTestSingleHashBuilderForArrayTableCornerCase(DataTypes buildTypes, JoinType joinType, int start,
    int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStartWithStride<T>(buildTypes, dataSize, start, 10);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator->AddInput(plainInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    VectorHelper::FreeVecBatch(plainVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
}

TEST(RandomJoinTest, TestHashBuilderForArrayTable)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> starts { 0, -300, 300 };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto start : starts) {
            for (auto time : repeatTimes) {
                StressTestSingleHashBuilderForArrayTable<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                    joinType, start, time);
                StressTestSingleHashBuilderForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                    joinType, start, time);
                StressTestSingleHashBuilderForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                    joinType, start, time);
                StressTestSingleHashBuilderForArrayTable<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                    joinType, start, time);
                // ----------------------Two build-------------------------------
                StressTestTwoHashBuilderForArrayTable<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                    joinType, start, time);
                StressTestTwoHashBuilderForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                    joinType, start, time);
                StressTestTwoHashBuilderForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                    joinType, start, time);
                StressTestTwoHashBuilderForArrayTable<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                    joinType, start, time);
            }
        }
    }
}

TEST(RandomJoinTest, TestHashBuilderForArrayTableCornerCase)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> starts { 0, -300, 300 };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto start : starts) {
            for (auto time : repeatTimes) {
                StressTestSingleHashBuilderForArrayTableCornerCase<int16_t>(
                    DataTypes(std::vector<DataTypePtr>({ ShortType() })), joinType, start, time);
                StressTestSingleHashBuilderForArrayTableCornerCase<int32_t>(
                    DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType, start, time);
                StressTestSingleHashBuilderForArrayTableCornerCase<int32_t>(
                    DataTypes(std::vector<DataTypePtr>({ Date32Type() })), joinType, start, time);
                StressTestSingleHashBuilderForArrayTableCornerCase<int64_t>(
                    DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType, start, time);
                // ----------------------Two build-------------------------------
                StressTestTwoHashBuilderForArrayTableCornerCase<int16_t>(
                    DataTypes(std::vector<DataTypePtr>({ ShortType() })), joinType, start, time);
                StressTestTwoHashBuilderForArrayTableCornerCase<int32_t>(
                    DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType, start, time);
                StressTestTwoHashBuilderForArrayTableCornerCase<int32_t>(
                    DataTypes(std::vector<DataTypePtr>({ Date32Type() })), joinType, start, time);
                StressTestTwoHashBuilderForArrayTableCornerCase<int64_t>(
                    DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType, start, time);
            }
        }
    }
}

Expr *GetFilter(DataTypes &buildTypes)
{
    Expr *left = nullptr;
    Expr *right = nullptr;
    switch (buildTypes.GetIds()[0]) {
        case OMNI_BOOLEAN:
            left = new LiteralExpr(100, IntType());
            right = new LiteralExpr(300, IntType());
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            left = new FieldExpr(0, IntType());
            right = new LiteralExpr(300, IntType());
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
        case OMNI_DATE64:
        case OMNI_DOUBLE:
            left = new FieldExpr(0, LongType());
            right = new LiteralExpr(300L, LongType());
            break;
        case OMNI_DECIMAL128:
            left = new FieldExpr(0, Decimal128Type());
            right = new LiteralExpr(new std::string("300"), Decimal128Type());
            break;
        default:
            break;
    }
    return new BinaryExpr(omniruntime::expressions::Operator::NEQ, left, right, BooleanType());
}

template <typename T>
void StressTestSingleLookupJoinForArrayTable(DataTypes buildTypes, JoinType joinType, bool enableFilter, int start)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, start);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    int repeatTimes = 10;
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator->AddInput(plainInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetFilter(buildTypes);
    }
    VectorBatch *probeVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, start + 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

template <typename T>
void StressTestTwoLookupJoinForArrayTable(DataTypes buildTypes, JoinType joinType, bool enableFilter, int start)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, start);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 2;
    int repeatTimes = 10;
    VectorBatch *hashBuildOutput = nullptr;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetFilter(buildTypes);
    }
    VectorBatch *probeVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, start + 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

TEST(RandomJoinTest, TestLookupJoinForArrayTable)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> starts { 0, -300, 300 };
    for (auto &joinType : joinTypes) {
        for (auto start : starts) {
            StressTestSingleLookupJoinForArrayTable<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                joinType, false, start);
            StressTestSingleLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                joinType, false, start);
            StressTestSingleLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, false, start);
            StressTestSingleLookupJoinForArrayTable<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                joinType, false, start);
            // ----------------------Two build-------------------------------
            StressTestTwoLookupJoinForArrayTable<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                joinType, false, start);
            StressTestTwoLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType,
                false, start);
            StressTestTwoLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, false, start);
            StressTestTwoLookupJoinForArrayTable<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType,
                false, start);
        }
    }
}

TEST(RandomJoinTest, TestLookupJoinForArrayTableWithFilter)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> starts { 0, -300, 300 };
    for (auto &joinType : joinTypes) {
        for (auto start : starts) {
            StressTestSingleLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                joinType, true, start);
            StressTestSingleLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, true, start);
            StressTestSingleLookupJoinForArrayTable<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                joinType, true, start);
            // ----------------------Two build-------------------------------
            StressTestTwoLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType,
                true, start);
            StressTestTwoLookupJoinForArrayTable<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, true, start);
            StressTestTwoLookupJoinForArrayTable<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType,
                true, start);
        }
    }
}

template <typename T>
VectorBatch *CreateSingleColumnSequenceDataFromStart(DataTypes &buildTypes, int rowCnt, bool isDic, int start)
{
    T *data = new T[rowCnt];
    if constexpr (!std::is_same_v<T, Decimal128>) {
        for (int i = 0; i < rowCnt; ++i) {
            data[i] = static_cast<T>(i + start);
        }
    } else {
        for (int i = 0; i < rowCnt; ++i) {
            data[i] = Decimal128(i + start, i + start);
        }
    }

    VectorBatch *buildVecBatch = nullptr;
    if (!isDic) {
        buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data);
    } else {
        auto *ids = new int32_t[rowCnt];
        for (int i = 0; i < rowCnt; ++i) {
            ids[i] = i;
        }
        auto expectedVec0 = CreateVector<T>(rowCnt, data);
        auto expectedDictVec0 =
            VectorHelper::CreateDictionary(ids, rowCnt, reinterpret_cast<Vector<T> *>(expectedVec0));
        buildVecBatch = new VectorBatch(rowCnt);
        buildVecBatch->Append(expectedDictVec0);
        delete expectedVec0;
        delete[] ids;
    }
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }
    delete[] data;
    return buildVecBatch;
}

template <typename T>
void StressTestSingleLookupJoinForSingleKey(DataTypes buildTypes, JoinType joinType, bool enableFilter, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 0);
    VectorBatch *dicVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 0);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator->AddInput(plainInput);
        hashBuilderOperator->AddInput(dicInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetFilter(buildTypes);
    }
    VectorBatch *probeVecBatch1 = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 100);
    VectorBatch *probeVecBatch2 = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch1);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    lookupJoinOperator->AddInput(probeVecBatch2);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

VectorBatch *CreateVariableSizeSequenceData(DataTypes &buildTypes, int rowCnt, bool isDic, int start)
{
    auto *data0 = new std::string[rowCnt];
    auto *data1 = new std::string[rowCnt];
    int32_t maxLen = 17;
    for (int i = 0; i < rowCnt; ++i) {
        for (int j = 0; j < maxLen; ++j) {
            data0[i] += std::to_string(i + start);
            data1[i] += std::to_string(i + start);
        }
    }

    VectorBatch *buildVecBatch = nullptr;
    if (!isDic) {
        buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data1, data0);
    } else {
        auto *ids = new int32_t[rowCnt];
        for (int i = 0; i < rowCnt; ++i) {
            ids[i] = i;
        }
        auto expectedVec0 = CreateVarcharVector(data0, rowCnt);
        auto expectedDictVec0 = CreateDictionary<OMNI_VARCHAR>(expectedVec0, ids, rowCnt);
        auto expectedVec1 = CreateVarcharVector(data1, rowCnt);
        auto expectedDictVec1 = CreateDictionary<OMNI_VARCHAR>(expectedVec0, ids, rowCnt);
        buildVecBatch = new VectorBatch(rowCnt);
        buildVecBatch->Append(expectedDictVec0);
        buildVecBatch->Append(expectedDictVec1);
        delete expectedVec0;
        delete expectedVec1;
        delete[] ids;
    }
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }

    delete[] data0;
    delete[] data1;
    return buildVecBatch;
}

Expr *GetStringFilter()
{
    Expr *left = new FieldExpr(0, VarcharType());
    Expr *right = new LiteralExpr(new std::string("300"), VarcharType());
    return new BinaryExpr(omniruntime::expressions::Operator::NEQ, left, right, BooleanType());
}

void StressTestSingleLookupJoinForVariableKey(DataTypes buildTypes, JoinType joinType, bool enableFilter,
    int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateVariableSizeSequenceData(buildTypes, dataSize, false, 0);
    VectorBatch *dicVecBatch = CreateVariableSizeSequenceData(buildTypes, dataSize, true, 0);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator->AddInput(plainInput);
        hashBuilderOperator->AddInput(dicInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetStringFilter();
    }
    VectorBatch *probeVecBatch1 = CreateVariableSizeSequenceData(buildTypes, dataSize, false, 100);
    VectorBatch *probeVecBatch2 = CreateVariableSizeSequenceData(buildTypes, dataSize, true, 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch1);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    lookupJoinOperator->AddInput(probeVecBatch2);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

void StressTestTwoLookupJoinForVariableKey(DataTypes buildTypes, JoinType joinType, bool enableFilter, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateVariableSizeSequenceData(buildTypes, dataSize, false, 0);
    VectorBatch *dicVecBatch = CreateVariableSizeSequenceData(buildTypes, dataSize, true, 0);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 2;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    VectorBatch *hashBuildOutput = nullptr;

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
        hashBuilderOperator1->AddInput(dicInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
        hashBuilderOperator2->AddInput(dicInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetStringFilter();
    }
    VectorBatch *probeVecBatch1 = CreateVariableSizeSequenceData(buildTypes, dataSize, false, 100);
    VectorBatch *probeVecBatch2 = CreateVariableSizeSequenceData(buildTypes, dataSize, true, 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->setNoMoreInput(false);
    lookupJoinOperator->AddInput(probeVecBatch1);
    while (!lookupJoinOperator->needsInput() && lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    lookupJoinOperator->AddInput(probeVecBatch2);
    lookupJoinOperator->noMoreInput();
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

template <typename T>
void StressTestTwoLookupJoinForSingleKey(DataTypes buildTypes, JoinType joinType, bool enableFilter, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 0);
    VectorBatch *dicVecBatch = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 0);

    int32_t buildJoinCols[1] = {0};
    int32_t joinColsCount = 1;
    int32_t operatorCount = 2;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    VectorBatch *hashBuildOutput = nullptr;

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
        hashBuilderOperator1->AddInput(dicInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
        hashBuilderOperator2->AddInput(dicInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetFilter(buildTypes);
    }
    VectorBatch *probeVecBatch1 = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 100);
    VectorBatch *probeVecBatch2 = CreateSingleColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->setNoMoreInput(false);
    lookupJoinOperator->AddInput(probeVecBatch1);
    while (!lookupJoinOperator->needsInput() && lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    lookupJoinOperator->AddInput(probeVecBatch2);
    lookupJoinOperator->noMoreInput();
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

template <typename T>
VectorBatch *CreateTwoColumnSequenceDataFromStart(DataTypes &buildTypes, int rowCnt, bool isDic, int start)
{
    T *data1 = new T[rowCnt];
    T *data2 = new T[rowCnt];
    if constexpr (!std::is_same_v<T, Decimal128>) {
        for (int i = 0; i < rowCnt; ++i) {
            data1[i] = static_cast<T>(i + start);
            data2[i] = static_cast<T>(i + start);
        }
    } else {
        for (int i = 0; i < rowCnt; ++i) {
            data1[i] = Decimal128(i + start, i + start);
            data2[i] = Decimal128(i + start, i + start);
        }
    }

    VectorBatch *buildVecBatch = nullptr;
    if (!isDic) {
        buildVecBatch = CreateVectorBatch(buildTypes, rowCnt, data1, data2);
    } else {
        auto *ids = new int32_t[rowCnt];
        for (int i = 0; i < rowCnt; ++i) {
            ids[i] = i;
        }
        auto expectedVec0 = CreateVector<T>(rowCnt, data1);
        auto expectedDictVec0 =
            VectorHelper::CreateDictionary(ids, rowCnt, reinterpret_cast<Vector<T> *>(expectedVec0));
        auto expectedVec1 = CreateVector<T>(rowCnt, data1);
        auto expectedDictVec1 =
            VectorHelper::CreateDictionary(ids, rowCnt, reinterpret_cast<Vector<T> *>(expectedVec1));
        buildVecBatch = new VectorBatch(rowCnt);
        buildVecBatch->Append(expectedDictVec0);
        buildVecBatch->Append(expectedDictVec1);
        delete expectedVec0;
        delete expectedVec1;
        delete[] ids;
    }
    int nullStride = 5;
    for (int i = 0; i < rowCnt; i += nullStride) {
        buildVecBatch->Get(0)->SetNull(i);
    }
    delete[] data1;
    delete[] data2;
    return buildVecBatch;
}

template <typename T>
void StressTestSingleLookupJoinForTwoFixedKey(DataTypes buildTypes, JoinType joinType, bool enableFilter,
    int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 0);
    VectorBatch *dicVecBatch = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 0);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 1;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator->AddInput(plainInput);
        hashBuilderOperator->AddInput(dicInput);
    }
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetFilter(buildTypes);
    }
    VectorBatch *probeVecBatch1 = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 100);
    VectorBatch *probeVecBatch2 = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->AddInput(probeVecBatch1);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    lookupJoinOperator->AddInput(probeVecBatch2);
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

template <typename T>
void StressTestTwoLookupJoinForTwoFixedKey(DataTypes buildTypes, JoinType joinType, bool enableFilter, int repeatTimes)
{
    const int32_t dataSize = 1000;
    VectorBatch *plainVecBatch = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 0);
    VectorBatch *dicVecBatch = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 0);

    int32_t buildJoinCols[2] = {0, 1};
    int32_t joinColsCount = 2;
    int32_t operatorCount = 2;
    HashBuilderOperatorFactory *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildTypes, buildJoinCols, joinColsCount, operatorCount);
    VectorBatch *hashBuildOutput = nullptr;

    auto *hashBuilderOperator1 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator1->AddInput(plainInput);
        hashBuilderOperator1->AddInput(dicInput);
    }
    hashBuilderOperator1->GetOutput(&hashBuildOutput);

    auto *hashBuilderOperator2 = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    for (int i = 0; i < repeatTimes; ++i) {
        auto plainInput = DuplicateVectorBatch(plainVecBatch);
        auto dicInput = DuplicateVectorBatch(dicVecBatch);
        hashBuilderOperator2->AddInput(plainInput);
        hashBuilderOperator2->AddInput(dicInput);
    }
    hashBuilderOperator2->GetOutput(&hashBuildOutput);

    Expr *filter = nullptr;
    if (enableFilter) {
        filter = GetFilter(buildTypes);
    }
    VectorBatch *probeVecBatch1 = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, false, 100);
    VectorBatch *probeVecBatch2 = CreateTwoColumnSequenceDataFromStart<T>(buildTypes, dataSize, true, 100);
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderFactory;
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(buildTypes, buildJoinCols,
        joinColsCount, buildJoinCols, joinColsCount, buildJoinCols, joinColsCount, buildTypes, hashBuilderFactoryAddr,
        filter, false, nullptr);
    auto lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator());
    lookupJoinOperator->setNoMoreInput(false);
    lookupJoinOperator->AddInput(probeVecBatch1);
    while (!lookupJoinOperator->needsInput() && lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }
    lookupJoinOperator->AddInput(probeVecBatch2);
    lookupJoinOperator->noMoreInput();
    while (lookupJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinOperator->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);
    }

    if (joinType == OMNI_JOIN_TYPE_FULL) {
        auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            buildTypes, buildJoinCols, joinColsCount, buildJoinCols, buildTypes, hashBuilderFactoryAddr);
        auto lookupOuterJoinOperator = lookupOuterJoinOperatorFactory->CreateOperator();
        while (lookupOuterJoinOperator->GetStatus() != OMNI_STATUS_FINISHED) {
            VectorBatch *appendOutput = nullptr;
            lookupOuterJoinOperator->GetOutput(&appendOutput);
            VectorHelper::FreeVecBatch(appendOutput);
        }
        omniruntime::op::Operator::DeleteOperator(lookupOuterJoinOperator);
        delete lookupOuterJoinOperatorFactory;
    }

    Expr::DeleteExprs({ filter });
    VectorHelper::FreeVecBatch(plainVecBatch);
    VectorHelper::FreeVecBatch(dicVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator2);
    delete hashBuilderFactory;
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    delete lookupJoinFactory;
}

TEST(RandomJoinTest, DISABLED_TestLookupJoinWithSingleHashBuilderForNormalHashTable)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto time : repeatTimes) {
            StressTestSingleLookupJoinForSingleKey<bool>(DataTypes(std::vector<DataTypePtr>({ BooleanType() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<double>(DataTypes(std::vector<DataTypePtr>({ DoubleType() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Date64Type() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })),
                joinType, false, time);
            StressTestSingleLookupJoinForSingleKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type() })), joinType, false, time);

            StressTestSingleLookupJoinForTwoFixedKey<bool>(
                DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<int16_t>(
                DataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ IntType(), IntType() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ Date32Type(), Date32Type() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<double>(
                DataTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ LongType(), LongType() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Date64Type(), Date64Type() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), Decimal64Type() })), joinType, false, time);
            StressTestSingleLookupJoinForTwoFixedKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type(), Decimal128Type() })), joinType, false, time);
            StressTestSingleLookupJoinForVariableKey(
                DataTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() })), joinType, false, time);
        }
    }
}

TEST(RandomJoinTest, TestLookupJoinWithTwoHashBuilderForNormalHashTable)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto time : repeatTimes) {
            StressTestTwoLookupJoinForSingleKey<bool>(DataTypes(std::vector<DataTypePtr>({ BooleanType() })), joinType,
                false, time);
            StressTestTwoLookupJoinForSingleKey<int16_t>(DataTypes(std::vector<DataTypePtr>({ ShortType() })), joinType,
                false, time);
            StressTestTwoLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType,
                false, time);
            StressTestTwoLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, false, time);
            StressTestTwoLookupJoinForSingleKey<double>(DataTypes(std::vector<DataTypePtr>({ DoubleType() })), joinType,
                false, time);
            StressTestTwoLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType,
                false, time);
            StressTestTwoLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Date64Type() })),
                joinType, false, time);
            StressTestTwoLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })),
                joinType, false, time);
            StressTestTwoLookupJoinForSingleKey<Decimal128>(DataTypes(std::vector<DataTypePtr>({ Decimal128Type() })),
                joinType, false, time);

            StressTestTwoLookupJoinForTwoFixedKey<bool>(
                DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<int16_t>(
                DataTypes(std::vector<DataTypePtr>({ ShortType(), ShortType() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ IntType(), IntType() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ Date32Type(), Date32Type() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<double>(
                DataTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ LongType(), LongType() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Date64Type(), Date64Type() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), Decimal64Type() })), joinType, false, time);
            StressTestTwoLookupJoinForTwoFixedKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type(), Decimal128Type() })), joinType, false, time);

            StressTestTwoLookupJoinForVariableKey(DataTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() })),
                joinType, false, time);
        }
    }
}

TEST(RandomJoinTest, DISABLED_TestLookupJoinWithSingleHashBuilderForNormalHashTableWithFilter)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto time : repeatTimes) {
            StressTestSingleLookupJoinForSingleKey<bool>(DataTypes(std::vector<DataTypePtr>({ BooleanType() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<double>(DataTypes(std::vector<DataTypePtr>({ DoubleType() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Date64Type() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })),
                joinType, true, time);
            StressTestSingleLookupJoinForSingleKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type() })), joinType, true, time);

            StressTestSingleLookupJoinForTwoFixedKey<bool>(
                DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ IntType(), IntType() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ Date32Type(), Date32Type() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<double>(
                DataTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ LongType(), LongType() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Date64Type(), Date64Type() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), Decimal64Type() })), joinType, true, time);
            StressTestSingleLookupJoinForTwoFixedKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type(), Decimal128Type() })), joinType, true, time);

            StressTestSingleLookupJoinForVariableKey(
                DataTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() })), joinType, true, time);
        }
    }
}

TEST(RandomJoinTest, TestLookupJoinWithTwoHashBuilderForNormalHashTableWithFilter)
{
    std::vector<JoinType> joinTypes { OMNI_JOIN_TYPE_INNER, OMNI_JOIN_TYPE_LEFT,      OMNI_JOIN_TYPE_RIGHT,
        OMNI_JOIN_TYPE_FULL,  OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_JOIN_TYPE_LEFT_ANTI };
    std::vector<int> repeatTimes { 1, 10 };
    for (auto &joinType : joinTypes) {
        for (auto time : repeatTimes) {
            StressTestTwoLookupJoinForSingleKey<bool>(DataTypes(std::vector<DataTypePtr>({ BooleanType() })), joinType,
                true, time);
            StressTestTwoLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ IntType() })), joinType,
                true, time);
            StressTestTwoLookupJoinForSingleKey<int32_t>(DataTypes(std::vector<DataTypePtr>({ Date32Type() })),
                joinType, true, time);
            StressTestTwoLookupJoinForSingleKey<double>(DataTypes(std::vector<DataTypePtr>({ DoubleType() })), joinType,
                true, time);
            StressTestTwoLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ LongType() })), joinType,
                true, time);
            StressTestTwoLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Date64Type() })),
                joinType, true, time);
            StressTestTwoLookupJoinForSingleKey<int64_t>(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })),
                joinType, true, time);
            StressTestTwoLookupJoinForSingleKey<Decimal128>(DataTypes(std::vector<DataTypePtr>({ Decimal128Type() })),
                joinType, true, time);

            StressTestTwoLookupJoinForTwoFixedKey<bool>(
                DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ IntType(), IntType() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<int32_t>(
                DataTypes(std::vector<DataTypePtr>({ Date32Type(), Date32Type() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<double>(
                DataTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ LongType(), LongType() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Date64Type(), Date64Type() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<int64_t>(
                DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), Decimal64Type() })), joinType, true, time);
            StressTestTwoLookupJoinForTwoFixedKey<Decimal128>(
                DataTypes(std::vector<DataTypePtr>({ Decimal128Type(), Decimal128Type() })), joinType, true, time);

            StressTestTwoLookupJoinForVariableKey(DataTypes(std::vector<DataTypePtr>({ VarcharType(), VarcharType() })),
                joinType, true, time);
        }
    }
}
}
