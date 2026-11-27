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

#include <gtest/gtest-param-test.h>
#include <sstream>
#include <sys/time.h>

#include "aggregator_multi_stage_no_groupby.h"
#include "aggregator_multi_stage_with_groupby.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

static void SetAggregateFactoryParams(const bool hasMask, const FunctionType aggFunc, DataTypeId inId, DataTypeId outId,
                                      const int32_t valueColumnIndex, const int32_t maskColumnIndex,
                                      std::vector<uint32_t> &aggFuncVector, std::vector<DataTypePtr> &inputTypeVector,
                                      std::vector<DataTypePtr> &outputTypeVector,
                                      std::vector<uint32_t> &aggColIdxVector, std::vector<uint32_t> &aggMask)
{
    aggFuncVector.resize(2);
    aggFuncVector[0] = static_cast<uint32_t>(aggFunc);
    aggFuncVector[1] = static_cast<uint32_t>(OMNI_AGGREGATION_TYPE_COUNT_COLUMN);

    aggMask.resize(2);
    if (hasMask) {
        aggMask[0] = maskColumnIndex;
        aggMask[1] = maskColumnIndex;
    } else {
        aggMask[0] = static_cast<uint32_t>(-1);
        aggMask[1] = static_cast<uint32_t>(-1);
    }

    inputTypeVector.resize(2);
    inputTypeVector[0] = GetType(inId);
    inputTypeVector[1] = GetType(inId);

    outputTypeVector.resize(2);
    outputTypeVector[0] = GetType(outId);
    outputTypeVector[1] = LongType();

    aggColIdxVector.resize(2);
    aggColIdxVector[0] = valueColumnIndex;
    aggColIdxVector[1] = valueColumnIndex;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
class AggregatorTesterTemplateSingleState : public AggregatorTesterTemplate<IN_ID, OUT_ID> {
public:
    AggregatorTesterTemplateSingleState(const std::string aggFuncName, const int32_t nullPercent, const bool isDict,
                                        const bool hasMask, const bool nullWhenOverflow)
        : AggregatorTesterTemplate<IN_ID, OUT_ID>(aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow)
    {
    }

    ~AggregatorTesterTemplateSingleState() override = default;

    bool GeneratePartialExpectedResult(VectorBatch **expectedResult, std::vector<VectorBatch *> &vvb) override
    {
        throw OmniException("Not Supported",
                            "GeneratePartialExpectedResult not supported in AggregatorTesterTemplateSingleState");
    }

    std::unique_ptr<OperatorFactory> CreatePartialFactory() override
    {
        throw OmniException("Not Supported",
                            "CreatePartialFactory not supported in AggregatorTesterTemplateSingleState");
    }

    std::unique_ptr<OperatorFactory> CreateFinalFactory() override
    {
        std::vector<uint32_t> aggFuncVector;
        std::vector<DataTypePtr> inputTypeVector;
        std::vector<DataTypePtr> outputTypeVector;
        std::vector<uint32_t> aggColIdxVector;
        std::vector<uint32_t> aggMaskVector;

        SetAggregateFactoryParams(this->hasMask, this->aggFunc, IN_ID, OUT_ID, this->GetValueColumnIndex(),
                                  this->GetMaskColumnIndex(), aggFuncVector, inputTypeVector, outputTypeVector,
                                  aggColIdxVector, aggMaskVector);
        return this->CreateFactory(aggFuncVector, inputTypeVector, outputTypeVector, aggColIdxVector, aggMaskVector,
                                   true, false);
    }

    bool GenerateFinalExpectedResult(VectorBatch **expectedResult, std::vector<VectorBatch *> &vvb) override
    {
        *expectedResult = new VectorBatch(1);
        BaseVector *v = DYNAMIC_TYPE_DISPATCH(VectorHelper::CreateFlatVector, OUT_ID, 1);
        (*expectedResult)->Append(v);
        (*expectedResult)->Append(new Vector<int64_t>(1));

        if constexpr (IN_ID == OMNI_VARCHAR || IN_ID == OMNI_CHAR) {
            AggregatorTesterTemplate<IN_ID, OUT_ID>::GenerateVarcharResult(vvb, *expectedResult, true);
            return false;
        } else if constexpr (OUT_ID == OMNI_VARCHAR || OUT_ID == OMNI_CHAR) {
            throw OmniException("Invalid Arguement", "Invalid aggregation type " +
                                                         std::to_string(as_integer(this->aggFunc)) +
                                                         " for Varchar input");
        } else {
            using T_IN = typename NativeAndVectorType<IN_ID>::type;
            using T_OUT = typename NativeAndVectorType<OUT_ID>::type;
            using V_OUT = typename NativeAndVectorType<OUT_ID>::vector;
            T_OUT result{};
            int64_t count = 0;
            bool overflow;
            const int32_t valueIndex = this->GetValueColumnIndex();
            const int32_t maskIndex = this->GetMaskColumnIndex();

            if (TypeUtil::IsDecimalType(IN_ID) &&
                (this->aggFunc == OMNI_AGGREGATION_TYPE_SUM || this->aggFunc == OMNI_AGGREGATION_TYPE_AVG)) {
                if (this->aggFunc == OMNI_AGGREGATION_TYPE_SUM) {
                    overflow = GenerateExpectedResultNumeric<AllMatchFilter, IN_ID, T_OUT, Decimal128>(
                        vvb, valueIndex, maskIndex, SumFunc<T_IN, Decimal128>, count, result);
                } else {
                    Decimal128 result128{};
                    overflow = GenerateExpectedResultNumeric<AllMatchFilter, IN_ID, Decimal128, Decimal128>(
                        vvb, valueIndex, maskIndex, SumFunc<T_IN, Decimal128>, count, result128);
                    if (!overflow && count > 0) {
                        // generate actual average from some and count
                        Decimal128Wrapper wrapped = Decimal128Wrapper(result128).Divide(Decimal128Wrapper(count), 0);
                        overflow = !DoCast<Decimal128, T_OUT>(result, wrapped.ToDecimal128());
                    }
                }
            } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_SUM) {
                using T_MID = std::conditional_t<std::is_floating_point_v<T_IN>, double, int64_t>;
                overflow = GenerateExpectedResultNumeric<AllMatchFilter, IN_ID, T_OUT, T_MID>(
                    vvb, valueIndex, maskIndex, SumFunc<T_IN, T_MID>, count, result);
            } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_AVG) {
                double resultDouble{};
                overflow = GenerateExpectedResultNumeric<AllMatchFilter, IN_ID, double, double>(
                    vvb, valueIndex, maskIndex, SumFunc<T_IN, double>, count, resultDouble);
                if (!overflow && count > 0) {
                    overflow = !DoCast<double, T_OUT>(result, resultDouble /= count);
                }
            } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_MIN) {
                overflow = GenerateExpectedResultNumeric<AllMatchFilter, IN_ID, T_OUT, T_IN>(
                    vvb, valueIndex, maskIndex, MinFunc<T_IN, T_IN>, count, result);
            } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_MAX) {
                overflow = GenerateExpectedResultNumeric<AllMatchFilter, IN_ID, T_OUT, T_IN>(
                    vvb, valueIndex, maskIndex, MaxFunc<T_IN, T_IN>, count, result);
            } else {
                throw OmniException("Invalid Arguement",
                                    "Invalid aggregation type " + std::to_string(as_integer(this->aggFunc)));
            }

            static_cast<Vector<int64_t> *>((*expectedResult)->Get(1))->SetValue(0, count);
            if (overflow || count == 0) {
                (*expectedResult)->Get(0)->SetNull(0);
            } else {
                static_cast<V_OUT *>((*expectedResult)->Get(0))->SetValue(0, result);
            }

            return overflow;
        }

        throw OmniException("Unreachable code", "Unreachable code");
    }
};

template <DataTypeId IN_ID, DataTypeId OUT_ID>
class HashAggregatorTesterTemplateSingleState : public HashAggregatorTesterTemplate<IN_ID, OUT_ID> {
public:
    HashAggregatorTesterTemplateSingleState(const std::string aggFuncName, const int32_t nullPercent, const bool isDict,
                                            const bool hasMask, const bool nullWhenOverflow)
        : HashAggregatorTesterTemplate<IN_ID, OUT_ID>(aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow)
    {
    }

    ~HashAggregatorTesterTemplateSingleState() override = default;

    bool GeneratePartialExpectedResult(VectorBatch **expectedResult, std::vector<VectorBatch *> &vvb) override
    {
        throw OmniException("Not Supported",
                            "GeneratePartialExpectedResult not supported in HashAggregatorTesterTemplateSingleState");
    }

    std::unique_ptr<OperatorFactory> CreatePartialFactory() override
    {
        throw OmniException("Not Supported",
                            "CreatePartialFactory not supported in HashAggregatorTesterTemplateSingleState");
    }

    std::unique_ptr<OperatorFactory> CreateFinalFactory() override
    {
        std::vector<uint32_t> aggFuncVector;
        std::vector<DataTypePtr> inputTypeVector;
        std::vector<DataTypePtr> outputTypeVector;
        std::vector<uint32_t> aggColIdxVector;
        std::vector<uint32_t> aggMaskVector;

        SetAggregateFactoryParams(this->hasMask, this->aggFunc, IN_ID, OUT_ID, this->GetValueColumnIndex(),
                                  this->GetMaskColumnIndex(), aggFuncVector, inputTypeVector, outputTypeVector,
                                  aggColIdxVector, aggMaskVector);
        return this->CreateFactory(aggFuncVector, inputTypeVector, outputTypeVector, aggColIdxVector, aggMaskVector,
                                   true, false);
    }

    bool GenerateFinalExpectedResult(VectorBatch **expectedResult, std::vector<VectorBatch *> &vvb) override
    {
        *expectedResult = this->InitializeExpectedResult(vvb);

        if constexpr (IN_ID == OMNI_VARCHAR || IN_ID == OMNI_CHAR) {
            HashAggregatorTesterTemplate<IN_ID, OUT_ID>::GenerateVarcharResult(vvb, *expectedResult, true);
            return false;
        } else if constexpr (OUT_ID == OMNI_VARCHAR || OUT_ID == OMNI_CHAR) {
            throw OmniException("Invalid Arguement", "Invalid aggregation type " +
                                                         std::to_string(as_integer(this->aggFunc)) +
                                                         " for Varchar input");
        } else {
            using T_IN = typename NativeAndVectorType<IN_ID>::type;
            using T_OUT = typename NativeAndVectorType<OUT_ID>::type;
            using V_OUT = typename NativeAndVectorType<OUT_ID>::vector;
            BaseVector *groups = (*expectedResult)->Get(0);
            bool overalOverflow = false;
            const int32_t valueIndex = this->GetValueColumnIndex();
            const int32_t maskIndex = this->GetMaskColumnIndex();

            for (int32_t i = 0; i < groups->GetSize(); ++i) {
                int32_t filterValue;
                if (groups->GetEncoding() == vec::OMNI_DICTIONARY) {
                    filterValue = static_cast<Vector<DictionaryContainer<int32_t>> *>(groups)->GetValue(i);
                } else {
                    filterValue = static_cast<Vector<int32_t> *>(groups)->GetValue(i);
                }
                int32_t *filterValuePtr = groups->IsNull(i) ? nullptr : &filterValue;
                T_OUT result{};
                int64_t count = 0;
                bool overflow;

                if (TypeUtil::IsDecimalType(IN_ID) &&
                    (this->aggFunc == OMNI_AGGREGATION_TYPE_SUM || this->aggFunc == OMNI_AGGREGATION_TYPE_AVG)) {
                    if (this->aggFunc == OMNI_AGGREGATION_TYPE_SUM) {
                        overflow = GenerateExpectedResultNumeric<GroupByFilter, IN_ID, T_OUT, Decimal128>(
                            vvb, valueIndex, maskIndex, SumFunc<T_IN, Decimal128>, count, result, filterValuePtr, 0);
                    } else {
                        Decimal128 result128{};
                        overflow = GenerateExpectedResultNumeric<GroupByFilter, IN_ID, Decimal128, Decimal128>(
                            vvb, valueIndex, maskIndex, SumFunc<T_IN, Decimal128>, count, result128, filterValuePtr, 0);
                        if (!overflow && count > 0) {
                            // generate actual average from some and count
                            Decimal128Wrapper wrapped =
                                Decimal128Wrapper(result128).Divide(Decimal128Wrapper(count), 0);
                            overflow = !DoCast<Decimal128, T_OUT>(result, wrapped.ToDecimal128());
                        }
                    }
                } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_SUM) {
                    using T_MID = std::conditional_t<std::is_floating_point_v<T_IN>, double, int64_t>;
                    overflow = GenerateExpectedResultNumeric<GroupByFilter, IN_ID, T_OUT, T_MID>(
                        vvb, valueIndex, maskIndex, SumFunc<T_IN, T_MID>, count, result, filterValuePtr, 0);
                } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_AVG) {
                    double resultDouble{};
                    overflow = GenerateExpectedResultNumeric<GroupByFilter, IN_ID, double, double>(
                        vvb, valueIndex, maskIndex, SumFunc<T_IN, double>, count, resultDouble, filterValuePtr, 0);
                    if (!overflow && count > 0) {
                        overflow = !DoCast<double, T_OUT>(result, resultDouble /= count);
                    }
                } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_MIN) {
                    overflow = GenerateExpectedResultNumeric<GroupByFilter, IN_ID, T_OUT, T_IN>(
                        vvb, valueIndex, maskIndex, MinFunc<T_IN, T_IN>, count, result, filterValuePtr, 0);
                } else if (this->aggFunc == OMNI_AGGREGATION_TYPE_MAX) {
                    overflow = GenerateExpectedResultNumeric<GroupByFilter, IN_ID, T_OUT, T_IN>(
                        vvb, valueIndex, maskIndex, MaxFunc<T_IN, T_IN>, count, result, filterValuePtr, 0);
                } else {
                    throw OmniException("Invalid Arguement",
                                        "Invalid aggregation type " + std::to_string(as_integer(this->aggFunc)));
                }

                static_cast<Vector<int64_t> *>((*expectedResult)->Get(2))->SetValue(i, count);

                if (overflow || count == 0) {
                    (*expectedResult)->Get(1)->SetNull(i);
                } else {
                    static_cast<V_OUT *>((*expectedResult)->Get(1))->SetValue(i, result);
                }

                overalOverflow |= overflow;
            }

            return overalOverflow;
        }

        throw OmniException("Unreachable code", "Unreachable code");
    }

private:
    VectorBatch *InitializeExpectedResult(std::vector<VectorBatch *> &vvb)
    {
        int32_t hasNull = 0;
        std::set<int32_t> groups;
        for (VectorBatch *vb : vvb) {
            BaseVector *v = vb->Get(0);
            for (int32_t i = 0; i < v->GetSize(); ++i) {
                if (v->IsNull(i)) {
                    hasNull = 1;
                } else {
                    int32_t val;
                    if (v->GetEncoding() == vec::OMNI_DICTIONARY) {
                        val = static_cast<Vector<DictionaryContainer<int32_t>> *>(v)->GetValue(i);
                    } else {
                        val = static_cast<Vector<int32_t> *>(v)->GetValue(i);
                    }
                    groups.insert(val);
                }
            }
        }

        int32_t nGroups = groups.size() + hasNull;
        VectorBatch *expectedResult = new VectorBatch(nGroups);
        Vector<int32_t> *groupCol = new Vector<int32_t>(nGroups);
        int32_t rowIdx = 0;
        if (hasNull > 0) {
            groupCol->SetNull(rowIdx++);
        }
        for (int32_t v : groups) {
            groupCol->SetValue(rowIdx++, v);
        }

        expectedResult->Append(groupCol);
        BaseVector *v = DYNAMIC_TYPE_DISPATCH(VectorHelper::CreateFlatVector, OUT_ID, nGroups);
        expectedResult->Append(v);
        expectedResult->Append(new Vector<int64_t>(nGroups));

        return expectedResult;
    }
};

class SingleStageCompleteTest : public ::testing::TestWithParam<
                                    std::tuple<std::string, DataTypeId, DataTypeId, int32_t, bool, bool, bool, bool>> {
};

class CreateFacotoryTest : public ::testing::TestWithParam<std::tuple<DataTypeId, DataTypeId>> {};

template <DataTypeId IN_ID, DataTypeId OUT_ID>
static std::unique_ptr<AggregatorTester> CreateKnowInputOutput(const std::string aggFuncName, const int32_t nullPercent,
                                                               const bool isDict, const bool hasMask,
                                                               const bool nullWhenOverflow, const bool groupby)
{
    if (groupby) {
        return std::make_unique<HashAggregatorTesterTemplateSingleState<IN_ID, OUT_ID>>(
            aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow);
    } else {
        return std::make_unique<AggregatorTesterTemplateSingleState<IN_ID, OUT_ID>>(aggFuncName, nullPercent, isDict,
                                                                                    hasMask, nullWhenOverflow);
    }
}

template <DataTypeId IN_ID>
static std::unique_ptr<AggregatorTester>
CreateKnowInput(const DataTypeId outId, const std::string aggFuncName, const int32_t nullPercent, const bool isDict,
                const bool hasMask, const bool nullWhenOverflow, const bool groupby)
{
    switch (outId) {
        case OMNI_BOOLEAN:
            return CreateKnowInputOutput<IN_ID, OMNI_BOOLEAN>(aggFuncName, nullPercent, isDict, hasMask,
                                                              nullWhenOverflow, groupby);
        case OMNI_SHORT:
            return CreateKnowInputOutput<IN_ID, OMNI_SHORT>(aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                            groupby);
        case OMNI_INT:
            return CreateKnowInputOutput<IN_ID, OMNI_INT>(aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                          groupby);
        case OMNI_LONG:
            return CreateKnowInputOutput<IN_ID, OMNI_LONG>(aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                           groupby);
        case OMNI_DOUBLE:
            return CreateKnowInputOutput<IN_ID, OMNI_DOUBLE>(aggFuncName, nullPercent, isDict, hasMask,
                                                             nullWhenOverflow, groupby);
        case OMNI_DECIMAL64:
            return CreateKnowInputOutput<IN_ID, OMNI_DECIMAL64>(aggFuncName, nullPercent, isDict, hasMask,
                                                                nullWhenOverflow, groupby);
        case OMNI_DECIMAL128:
            return CreateKnowInputOutput<IN_ID, OMNI_DECIMAL128>(aggFuncName, nullPercent, isDict, hasMask,
                                                                 nullWhenOverflow, groupby);
        case OMNI_VARCHAR:
            return CreateKnowInputOutput<IN_ID, OMNI_VARCHAR>(aggFuncName, nullPercent, isDict, hasMask,
                                                              nullWhenOverflow, groupby);
        case OMNI_CHAR:
            return CreateKnowInputOutput<IN_ID, OMNI_CHAR>(aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                           groupby);
        default: throw OmniException("Invalid Argument", "Invalid type " + TypeUtil::TypeToStringLog(outId));
    }
}

static std::unique_ptr<AggregatorTester> CreateAggregatorTester(const std::string aggFuncName, const DataTypeId inId,
                                                                const DataTypeId outId, const int32_t nullPercent,
                                                                const bool isDict, const bool hasMask,
                                                                const bool nullWhenOverflow, const bool groupby)
{
    switch (inId) {
        case OMNI_BOOLEAN:
            return CreateKnowInput<OMNI_BOOLEAN>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                 groupby);
        case OMNI_SHORT:
            return CreateKnowInput<OMNI_SHORT>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                               groupby);
        case OMNI_INT:
            return CreateKnowInput<OMNI_INT>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                             groupby);
        case OMNI_LONG:
            return CreateKnowInput<OMNI_LONG>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                              groupby);
        case OMNI_DOUBLE:
            return CreateKnowInput<OMNI_DOUBLE>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                groupby);
        case OMNI_DECIMAL64:
            return CreateKnowInput<OMNI_DECIMAL64>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                   groupby);
        case OMNI_DECIMAL128:
            return CreateKnowInput<OMNI_DECIMAL128>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                    groupby);
        case OMNI_VARCHAR:
            return CreateKnowInput<OMNI_VARCHAR>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                                 groupby);
        case OMNI_CHAR:
            return CreateKnowInput<OMNI_CHAR>(outId, aggFuncName, nullPercent, isDict, hasMask, nullWhenOverflow,
                                              groupby);
        default: throw OmniException("Invalid Argument", "Invalid type " + TypeUtil::TypeToStringLog(inId));
    }
}

static void RunAggregatorTest(std::unique_ptr<AggregatorTester> tester, const bool isSupported,
                              std::vector<DataTypePtr> expectTypes, const double error)
{
    const std::string expectedExceptionMessage = tester->GetExpectedExceptionMessage();

    auto factory = tester->CreateFinalFactory();
    EXPECT_TRUE(factory != nullptr);

    op::Operator *agg;
    try {
        agg = factory->CreateOperator();
    } catch (OmniException &e) {
        if (!isSupported) {
            return;
        }
        throw e;
    }
    EXPECT_TRUE(agg != nullptr);

    std::vector<VectorBatch *> inputs = tester->BuildAggInput(VEC_BATCH_NUM, ROW_SIZE);
    EXPECT_TRUE(inputs.size() > 0);
    VectorBatch *expectedResult = nullptr;
    bool overflow = tester->GenerateFinalExpectedResult(&expectedResult, inputs);

    agg->Init();

    for (uint32_t i = 0; i < inputs.size(); ++i) {
        agg->AddInput(inputs[i]);
    }

    VectorBatch *outputVecBatch = nullptr;
    try {
        int32_t vecBatchCount = agg->GetOutput(&outputVecBatch);
        EXPECT_EQ(vecBatchCount, 1);
        EXPECT_EQ(outputVecBatch->GetVectorCount(), expectedResult->GetVectorCount());
        EXPECT_EQ(outputVecBatch->GetRowCount(), expectedResult->GetRowCount());
    } catch (OmniException &e) {
        op::Operator::DeleteOperator(agg);
        VectorHelper::FreeVecBatch(expectedResult);

        if (expectedExceptionMessage.length() == 0 || std::string(e.what()).find(expectedExceptionMessage, 0) < 0) {
            throw e;
        }
        return;
    }
    if (overflow) {
        EXPECT_EQ(expectedExceptionMessage.length(), 0);
        EXPECT_TRUE(ValidateOverflow("Final", tester->GetValueColumnIndex(), expectedResult, outputVecBatch));
    }
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectedResult, error));

    op::Operator::DeleteOperator(agg);
    VectorHelper::FreeVecBatch(expectedResult);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST_P(SingleStageCompleteTest, verify_correctness)
{
    const std::string aggFuncName = std::get<0>(GetParam());
    const DataTypeId inId = std::get<1>(GetParam());
    const DataTypeId outId = std::get<2>(GetParam());
    const int32_t nullPercent = std::get<3>(GetParam());
    const bool isDict = std::get<4>(GetParam());
    const bool hasMask = std::get<5>(GetParam());
    const bool nullWhenOverflow = std::get<6>(GetParam());
    const bool groupby = std::get<7>(GetParam());

    // aggregation on double input uses SIMD vectorization
    // but calculation of expected result does not use SIMD vectorization.
    // Therefore, actual result can be slightly different from expected result.
    // we set error = 0.0001 to handle this situation
    // When outout is not double (it is numeric) expected result could be off by +1/-1
    // for example, actual result could be 23.500123 (which when converted to numeric value will be 24)
    //  expected result, however, could be 23.499923 (which when converted to numeric value will be 23))
    double error = inId == OMNI_DOUBLE ? (outId == OMNI_DOUBLE ? 0.001 : 1) : 0;

    char *randSeedStr = std::getenv("TEST_RAND_SEED");
    unsigned int randSeed;
    if (randSeedStr != nullptr && strlen(randSeedStr) > 0) {
        randSeed = static_cast<unsigned int>(atoi(randSeedStr));
    } else {
        struct timeval time;
        gettimeofday(&time, nullptr);
        randSeed = static_cast<unsigned int>((time.tv_sec * 1000) + (time.tv_usec / 1000));
    }
    printf("Random seed: %d\n", randSeed);
    srand(randSeed);

    std::vector<DataTypePtr> expectTypes;
    if (groupby) {
        expectTypes.resize(3);
        expectTypes[0] = IntType();
        expectTypes[1] = GetType(outId);
        expectTypes[2] = LongType();
    } else {
        expectTypes.resize(2);
        expectTypes[0] = GetType(outId);
        expectTypes[1] = LongType();
    }

    RunAggregatorTest(std::move(CreateAggregatorTester(aggFuncName, inId, outId, nullPercent, isDict, hasMask,
                                                       nullWhenOverflow, groupby)),
                      CheckSupported(aggFuncName, inId, outId), expectTypes, error);
}

INSTANTIATE_TEST_CASE_P(AggregatorTest, SingleStageCompleteTest,
                        ::testing::Combine(::testing::Values("sum", "min", "max", "avg"),
                                           ::testing::Values(OMNI_BOOLEAN, OMNI_INT, OMNI_LONG, OMNI_DOUBLE,
                                                             OMNI_DECIMAL64, OMNI_DECIMAL128, OMNI_VARCHAR),
                                           ::testing::Values(OMNI_BOOLEAN, OMNI_INT, OMNI_LONG, OMNI_DOUBLE,
                                                             OMNI_DECIMAL64, OMNI_DECIMAL128, OMNI_VARCHAR),
                                           ::testing::Values(0, 25), // nullPercent
                                           ::testing::Bool(),        // isDict
                                           ::testing::Bool(),        // hasMask
                                           ::testing::Values(true),  // nullWhenOverflow
                                           ::testing::Bool()         // groupby
                                           ),
                        [](const testing::TestParamInfo<SingleStageCompleteTest::ParamType> &info) {
                            return std::get<0>(info.param) + "_" + TypeUtil::TypeToStringLog(std::get<1>(info.param)) +
                                   "_" + TypeUtil::TypeToStringLog(std::get<2>(info.param)) + "_" +
                                   std::to_string(std::get<3>(info.param)) + "_" +
                                   (std::get<4>(info.param) ? "dict_" : "flat_") +
                                   (std::get<5>(info.param) ? "withMask_" : "noMask_") +
                                   (std::get<6>(info.param) ? "overflowNull_" : "overflowExcep_") +
                                   (std::get<7>(info.param) ? "withGroupBy" : "noGroupBy");
                        });

TEST_P(CreateFacotoryTest, verify_branch)
{
    DataTypeId inId = std::get<0>(GetParam());
    DataTypeId outId = std::get<1>(GetParam());

    auto maxFactory = new MaxAggregatorFactory();
    auto minFactory = new MinAggregatorFactory();
    auto sumFactory = new SumAggregatorFactory();
    auto avgFactory = new AverageAggregatorFactory();
    auto countColumnFactory = new CountColumnAggregatorFactory();
    auto countAllFactory = new CountAllAggregatorFactory();
    std::vector<int32_t> channal0 = {0};

    auto max = maxFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(GetType(inId)).get()),
                                            *(AggregatorUtil::WrapWithDataTypes(GetType(outId)).get()), channal0, true,
                                            true, false);
    auto min = minFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(GetType(inId)).get()),
                                            *(AggregatorUtil::WrapWithDataTypes(GetType(outId)).get()), channal0, true,
                                            true, false);
    auto sum = sumFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(GetType(inId)).get()),
                                            *(AggregatorUtil::WrapWithDataTypes(GetType(outId)).get()), channal0, true,
                                            true, false);
    auto avg = avgFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(GetType(inId)).get()),
                                            *(AggregatorUtil::WrapWithDataTypes(GetType(outId)).get()), channal0, true,
                                            true, false);
    auto count1 = countColumnFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(GetType(inId)).get()),
                                                       *(AggregatorUtil::WrapWithDataTypes(GetType(outId)).get()),
                                                       channal0, true, true, false);
    auto count2 = countAllFactory->CreateAggregator(*(AggregatorUtil::WrapWithDataTypes(GetType(inId)).get()),
                                                    *(AggregatorUtil::WrapWithDataTypes(GetType(outId)).get()),
                                                    channal0, true, true, false);

    delete maxFactory;
    delete minFactory;
    delete sumFactory;
    delete avgFactory;
    delete countColumnFactory;
    delete countAllFactory;
}

INSTANTIATE_TEST_CASE_P(
    AggregatorTest, CreateFacotoryTest,
    ::testing::Combine(::testing::Values(OMNI_BOOLEAN, OMNI_SHORT, OMNI_INT, OMNI_DATE32, OMNI_TIME32, OMNI_LONG,
                                         OMNI_DATE64, OMNI_TIME64, OMNI_DOUBLE, OMNI_DECIMAL64, OMNI_DECIMAL128,
                                         OMNI_VARCHAR, OMNI_CHAR),
                       ::testing::Values(OMNI_BOOLEAN, OMNI_SHORT, OMNI_INT, OMNI_DATE32, OMNI_TIME32, OMNI_LONG,
                                         OMNI_DATE64, OMNI_TIME64, OMNI_DOUBLE, OMNI_DECIMAL64, OMNI_DECIMAL128,
                                         OMNI_VARCHAR, OMNI_CHAR)),
    [](const testing::TestParamInfo<CreateFacotoryTest::ParamType> &info) {
        return "CreateFactory_" + TypeUtil::TypeToString(std::get<0>(info.param)) + "_" +
               TypeUtil::TypeToString(std::get<1>(info.param));
    });
} // namespace omniruntime
