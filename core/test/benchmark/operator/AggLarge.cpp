/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include <algorithm>    // std::random_shuffle
#include <string_view>

#include "common/common.h"
#include "vector/vector_helper.h"
#include "vector/vector_common.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/aggregation/aggregator/aggregator_util.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class AggLarge : public BaseOperatorLargeFixture {
public:
    void SetUp(benchmark::State &state) override
    {
        srand(0);
        BaseOperatorLargeFixture::SetUp(state);
        rowsProcessed = 0;

        // create vector batch. same vector batch will be passed to operator at each iteration
        std::vector<int32_t> dictRowIds(ROWS_PER_PAGE);
        for (size_t i = 0; i < ROWS_PER_PAGE; ++i) {
            dictRowIds[i] = static_cast<int32_t>(i);
        }

        std::vector<int32_t> rowValues(ROWS_PER_PAGE);
        for (size_t i = 0; i < ROWS_PER_PAGE; ++i) {
            rowValues[i] = i;
        }

        int32_t nullRatio = NullRatio(state);
        bool isDict = DictionaryBlocks(state);
        int32_t currentNullRatio = nullRatio;
        bool currentIsDict = !isDict;
        std::vector<DataTypePtr> sourceTypesVec(INPUT_TYPES[InputTypes(state)]);
        const size_t nCols = sourceTypesVec.size();
        vecBatch = new VectorBatch(ROWS_PER_PAGE);

        for (size_t j = 0; j < nCols; ++j) {
            auto colTypeId = sourceTypesVec[j]->GetId();
            if (j % 2 == 0) {
                currentIsDict = !currentIsDict;
            }

            BaseVector *col = nullptr;
            if (colTypeId == OMNI_INT) {
                col = createColumn<OMNI_INT>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else if (colTypeId == OMNI_LONG) {
                col = createColumn<OMNI_LONG>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else if (colTypeId == OMNI_DOUBLE) {
                col = createColumn<OMNI_DOUBLE>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else if (colTypeId == OMNI_DECIMAL64) {
                col = createColumn<OMNI_DECIMAL64>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else if (colTypeId == OMNI_DECIMAL128) {
                col = createColumn<OMNI_DECIMAL128>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else if (colTypeId == OMNI_VARCHAR) {
                col = createColumn<OMNI_VARCHAR>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else if (colTypeId == OMNI_CHAR) {
                col = createColumn<OMNI_CHAR>(currentNullRatio, currentIsDict, rowValues, dictRowIds);
            } else {
                throw omniruntime::exception::OmniException(
                    "InvalidArgument", "Unsupported column type for AggLarge Bench Test");
            }
            vecBatch->Append(col);

            currentNullRatio = currentNullRatio == 0 ? nullRatio : 0;
        }

        if (WithMask(state)) {
            AddaMaskColumn(state, vecBatch);
        }
    }

    void TearDown(benchmark::State &state) override
    {
        // validate for long input
        std::vector<DataTypePtr> sourceTypesVec(INPUT_TYPES[InputTypes(state)]);
        if (sourceTypesVec.size() == 1 && sourceTypesVec[0]->GetId() == OMNI_LONG) {
            Vector<bool> *mask = nullptr;
            if (WithMask(state)) {
                assert(vecBatch->GetVectorCount() == 2);
                mask = static_cast<Vector<bool> *>(vecBatch->Get(1));
            }
            long sum = 0;
            long count = 0;
            for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
                if (mask != nullptr && (mask->IsNull(i) || !mask->GetValue(i))) {
                    continue;
                }
                if (!vecBatch->Get(0)->IsNull(i)) {
                    ++count;
                    if (DictionaryBlocks(state)) {
                        sum += static_cast<Vector<DictionaryContainer<int64_t>> *>(vecBatch->Get(0))->GetValue(i);
                    } else {
                        sum += static_cast<Vector<int64_t> *>(vecBatch->Get(0))->GetValue(i);
                    }
                }
            }

            Operator *op = operatorFactory->CreateOperator(nullptr);
            rowsProcessed = 0;
            VectorBatch *vb = createSingleVecBatch(state);
            int32_t nIters = 0;
            while (vb != nullptr) {
                op->AddInput(vb);
                ++nIters;
                vb = createSingleVecBatch(state);
            }
            sum *= nIters;
            count *= nIters;

            assert(op->GetStatus() != OMNI_STATUS_FINISHED);
            VectorBatch *outputVecBatch = nullptr;
            op->GetOutput(&outputVecBatch);
            assert(outputVecBatch != nullptr);
            assert(outputVecBatch->GetRowCount() == 1);
            assert(outputVecBatch->GetVectorCount() == 1);
            double actualValue = static_cast<Vector<double> *>(outputVecBatch->Get(0))->GetValue(0);
            double expectedValue = sum;
            expectedValue /= count;
            assert(fabs(actualValue - expectedValue) < 1.0);
            VectorHelper::FreeVecBatch(outputVecBatch);
            Operator::DeleteOperator(op);
        }

        VectorHelper::FreeVecBatch(vecBatch);
        vecBatch = nullptr;
        rowsProcessed = 0;
        BaseOperatorLargeFixture::TearDown(state);
    }

protected:
    static constexpr size_t ROWS_PER_PAGE = 1024;
    static constexpr int32_t NULL_RATIO = 10;
    static constexpr int32_t MAX_VARCHAR_LEN = 60;
    static constexpr int32_t LIMIT = 102400;
    static constexpr size_t TOTAL_ROWS = 1024000;
    static constexpr int32_t PERCENT = 100;

    size_t rowsProcessed = 0;
    char buffer[MAX_VARCHAR_LEN + 1];
    VectorBatch *vecBatch = nullptr;

    OperatorFactory *createOperatorFactory(const State &state) override
    {
        // setup group by
        std::vector<DataTypePtr> sourceTypesVec = INPUT_TYPES[InputTypes(state)];
        DataTypes sourceTypes(sourceTypesVec);
        const int32_t nAggs = sourceTypesVec.size();

        std::vector<DataTypePtr> aggOutputTypesVec;
        std::vector<uint32_t> aggColVector;
        std::vector<uint32_t> aggFuncTypeVector;
        std::vector<uint32_t> maskColsVector;
        std::vector<bool> inputRawWraps;
        std::vector<bool> outputPartialWraps;
        aggOutputTypesVec.reserve(nAggs);
        aggColVector.reserve(nAggs);
        aggFuncTypeVector.reserve(nAggs);
        maskColsVector.reserve(nAggs);
        inputRawWraps.reserve(nAggs);
        outputPartialWraps.reserve(nAggs);
        for (int32_t i = 0; i < nAggs; ++i) {
            aggOutputTypesVec.push_back(DoubleType());
            aggColVector.push_back(i);
            aggFuncTypeVector.push_back(omniruntime::op::FunctionType::OMNI_AGGREGATION_TYPE_AVG);
            if (WithMask(state)) {
                maskColsVector.push_back(i + nAggs);
            } else {
                maskColsVector.push_back(-1);
            }
            inputRawWraps.push_back(true);
            outputPartialWraps.push_back(false);
        }
        auto aggColVectorWrap = AggregatorUtil::WrapWithVector(aggColVector);
        auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(DataTypes(aggOutputTypesVec));

        AggregationOperatorFactory *factory = new AggregationOperatorFactory(sourceTypes, aggFuncTypeVector,
            aggColVectorWrap, maskColsVector, aggOutputTypesWrap, inputRawWraps, outputPartialWraps, false);
        factory->Init();
        return factory;
    }

    VectorBatch *createSingleVecBatch(const State &state) override
    {
        if (rowsProcessed >= TOTAL_ROWS) {
            rowsProcessed = 0;
            return nullptr;
        }

        // return duplicate of original vector batch
        auto vecCount = vecBatch->GetVectorCount();
        auto rowCount = vecBatch->GetRowCount();
        auto duplication = new VectorBatch(rowCount);
        for (int32_t i = 0; i < vecCount; ++i) {
            duplication->Append(VectorHelper::SliceVector(vecBatch->Get(i), 0, rowCount));
        }
        rowsProcessed += rowCount;
        return duplication;
    }

private:
    int32_t GenerateStrings(const int32_t maxLen, char *buf)
    {
        const int32_t curLen = rand() % maxLen;
        for (int32_t j = 0; j < curLen; ++j) {
            buf[j] = static_cast<char>((rand() % (127 - 32)) + 32);
        }
        buf[curLen] = 0;
        return curLen;
    }

    template <DataTypeId typeId>
    BaseVector *createColumn(const int32_t nullRatio, const bool isDict, std::vector<int32_t> &rowValues,
        std::vector<int32_t> dictRowIds)
    {
        using T = typename omniruntime::type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            auto *vector = new Vector<LargeStringContainer<std::string_view>>(ROWS_PER_PAGE);
            for (size_t i = 0; i < ROWS_PER_PAGE; ++i) {
                if (rand() % PERCENT < nullRatio) {
                    vector->SetNull(i);
                } else {
                    const int32_t strLen = GenerateStrings(MAX_VARCHAR_LEN, buffer);
                    std::string_view strV(buffer, strLen);
                    vector->SetValue(i, strV);
                }
            }
            if (isDict) {
                std::random_shuffle(dictRowIds.begin(), dictRowIds.end());
                auto *dictVec = VectorHelper::CreateStringDictionary(dictRowIds.data(), ROWS_PER_PAGE, vector);
                delete vector;
                return dictVec;
            }
            return vector;
        } else {
            std::random_shuffle(rowValues.begin(), rowValues.end());
            auto *vector = new Vector<T>(ROWS_PER_PAGE, typeId);
            for (size_t i = 0; i < ROWS_PER_PAGE; ++i) {
                if (rand() % PERCENT < nullRatio) {
                    vector->SetNull(i);
                } else {
                    vector->SetValue(i, static_cast<T>(rowValues[i]));
                }
            }
            if (isDict) {
                std::random_shuffle(dictRowIds.begin(), dictRowIds.end());
                auto *dictVec =  VectorHelper::CreateDictionary(dictRowIds.data(), ROWS_PER_PAGE, vector);
                delete vector;
                return dictVec;
            }
            return vector;
        }
    }

    void AddaMaskColumn(const State &state, VectorBatch *vecBatch)
    {
        int32_t nullRatio = NullRatio(state);
        bool isDict = DictionaryBlocks(state);
        int32_t currentNullRatio = nullRatio;
        bool currentIsDict = !isDict;
        const int32_t nCols = vecBatch->GetVectorCount();

        for (int32_t j = 0; j < nCols; ++j) {
            if (j % 2 == 0) {
                currentIsDict = !currentIsDict;
            }
            auto *col = new Vector<bool>(ROWS_PER_PAGE, OMNI_BOOLEAN);
            for (size_t i = 0; i < ROWS_PER_PAGE; ++i) {
                if (rand() % PERCENT < nullRatio) {
                    col->SetNull(i);
                } else {
                    col->SetValue(i, rand() % 5 == 0 ? false : true);
                }
            }
            vecBatch->Append(col);
            currentNullRatio = currentNullRatio == 0 ? nullRatio : 0;
        }
    }

    std::vector<std::vector<DataTypePtr>> INPUT_TYPES = {
        { IntType() },
        { LongType() },
        { DoubleType() },
        { Decimal64Type() },
        { Decimal128Type() }
    };
    OMNI_BENCHMARK_PARAM(int32_t, InputTypes, 0, 1, 2, 3, 4);
    OMNI_BENCHMARK_PARAM(int32_t, NullRatio, 0, 10);
    OMNI_BENCHMARK_PARAM(bool, DictionaryBlocks, false, true);
    OMNI_BENCHMARK_PARAM(bool, WithMask, false, true);
};
OMNI_BENCHMARK_DECLARE_OPERATOR_LARGE(AggLarge);
}
