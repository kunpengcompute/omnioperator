/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include "aggregator_factory.h"
#include "operator/aggregation/udaf/udaf_registry.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {

// Resolve element type T for CollectSet. Partial: input is usually raw T, or Array<T> when merging; Final: input is Array<T>.
static type::DataTypeId GetCollectElementTypeId(const type::DataTypes &inputTypes, bool inputRaw)
{
    const type::DataTypePtr &inputType = inputTypes.GetType(0);
    type::DataTypeId inputTypeId = inputType->GetId();
    if (inputRaw) {
        // Partial: input is usually raw T (e.g. OMNI_INT); when merging partial results input may be Array<T>
        return inputTypeId;
    }
    // Final: input must be Array<T>
    if (inputTypeId != type::OMNI_ARRAY) {
        std::string omniExceptionInfo =
            "CollectSet final stage expects array input type, got " + std::to_string(inputTypeId);
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }
    return inputType->asArray().ElementType()->GetId();
}

/*
 * e.g(Partial):
 * input: T, output: Array<T>
 * CollectSetAggregator<T, T>::Create
 *
 * e.g(Final):
 * output: array<T>, output: Array<T>
 * CollectSetAggregator<T, T>::Create
 */
std::unique_ptr<Aggregator> CollectSetAggregatorFactory::CreateAggregator(const type::DataTypes &inputTypes,
                                                                          const type::DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
                                                                          bool isOverflowAsNull)
{
    type::DataTypeId elementTypeId = GetCollectElementTypeId(inputTypes, inputRaw);
    switch (elementTypeId) {
        case type::OMNI_BOOLEAN:
            return CollectSetAggregator<type::OMNI_BOOLEAN, type::OMNI_BOOLEAN>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_BYTE:
            return CollectSetAggregator<type::OMNI_BYTE, type::OMNI_BYTE>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_SHORT:
            return CollectSetAggregator<type::OMNI_SHORT, type::OMNI_SHORT>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DATE32:
            return CollectSetAggregator<type::OMNI_DATE32, type::OMNI_DATE32>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_TIME32:
        case type::OMNI_INT:
            return CollectSetAggregator<type::OMNI_INT, type::OMNI_INT>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DATE64:
            return CollectSetAggregator<type::OMNI_DATE64, type::OMNI_DATE64>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_TIMESTAMP:
            return CollectSetAggregator<type::OMNI_TIMESTAMP, type::OMNI_TIMESTAMP>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_LONG:
        case type::OMNI_TIME64:
            return CollectSetAggregator<type::OMNI_LONG, type::OMNI_LONG>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_FLOAT:
            return CollectSetAggregator<type::OMNI_FLOAT, type::OMNI_FLOAT>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DOUBLE:
            return CollectSetAggregator<type::OMNI_DOUBLE, type::OMNI_DOUBLE>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DECIMAL64:
            return CollectSetAggregator<type::OMNI_DECIMAL64, type::OMNI_DECIMAL64>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DECIMAL128:
            return CollectSetAggregator<type::OMNI_DECIMAL128, type::OMNI_DECIMAL128>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
        case type::OMNI_VARBINARY:
            return std::make_unique<CollectSetVarcharAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_ARRAY:
        case type::OMNI_ROW:
            return CollectSetComplexAggregator::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull, elementTypeId);
        case type::OMNI_MAP: {
            std::string omniExceptionInfo = "CollectSet does not support Map type";
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
        default: {
            std::string omniExceptionInfo =
                "CollectSet unsupported element type " + std::to_string(elementTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}


std::unique_ptr<Aggregator> CollectListAggregatorFactory::CreateAggregator(const type::DataTypes &inputTypes,
                                                                          const type::DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
                                                                          bool isOverflowAsNull)
{
    type::DataTypeId elementTypeId = GetCollectElementTypeId(inputTypes, inputRaw);
    switch (elementTypeId) {
        case type::OMNI_BOOLEAN:
            return CollectListAggregator<type::OMNI_BOOLEAN, type::OMNI_BOOLEAN>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_BYTE:
            return CollectListAggregator<type::OMNI_BYTE, type::OMNI_BYTE>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_SHORT:
            return CollectListAggregator<type::OMNI_SHORT, type::OMNI_SHORT>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DATE32:
            return CollectListAggregator<type::OMNI_DATE32, type::OMNI_DATE32>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_TIME32:
        case type::OMNI_INT:
            return CollectListAggregator<type::OMNI_INT, type::OMNI_INT>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DATE64:
            return CollectListAggregator<type::OMNI_DATE64, type::OMNI_DATE64>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_TIMESTAMP:
            return CollectListAggregator<type::OMNI_TIMESTAMP, type::OMNI_TIMESTAMP>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_LONG:
        case type::OMNI_TIME64:
            return CollectListAggregator<type::OMNI_LONG, type::OMNI_LONG>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_FLOAT:
            return CollectListAggregator<type::OMNI_FLOAT, type::OMNI_FLOAT>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DOUBLE:
            return CollectListAggregator<type::OMNI_DOUBLE, type::OMNI_DOUBLE>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DECIMAL64:
            return CollectListAggregator<type::OMNI_DECIMAL64, type::OMNI_DECIMAL64>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_DECIMAL128:
            return CollectListAggregator<type::OMNI_DECIMAL128, type::OMNI_DECIMAL128>::Create(inputTypes, outputTypes,
                channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
        case type::OMNI_VARBINARY:
            return std::make_unique<CollectListVarcharAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
        case type::OMNI_ARRAY:
        case type::OMNI_MAP:
        case type::OMNI_ROW:
            return CollectListComplexAggregator::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull, elementTypeId);
        default: {
            std::string omniExceptionInfo =
                "CollectList unsupported element type " + std::to_string(elementTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}


std::unique_ptr<Aggregator> MinAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetType(0)->GetId();
    if (inputTypeId == OMNI_ARRAY || inputTypeId == OMNI_ROW) {
        return MinComplexAggregator::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial,
            isOverflowAsNull, inputTypeId);
    }
    return typedFactory_.CreateAggregator(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
}

std::unique_ptr<Aggregator> MaxAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetType(0)->GetId();
    if (inputTypeId == OMNI_ARRAY || inputTypeId == OMNI_ROW) {
        return MaxComplexAggregator::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial,
            isOverflowAsNull, inputTypeId);
    }
    return typedFactory_.CreateAggregator(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
}

std::unique_ptr<AggregatorFactory> CreateAggregatorFactory(FunctionType aggType)
{
    switch (aggType) {
        case OMNI_AGGREGATION_TYPE_SUM: {
            if (ConfigUtil::GetSupportContainerVecRule() == SupportContainerVecRule::NOT_SUPPORT) {
                return std::make_unique<SumSparkAggregatorFactory>();
            } else {
                return std::make_unique<SumAggregatorFactory>();
            }
        }
        case OMNI_AGGREGATION_TYPE_AVG: {
            if (ConfigUtil::GetSupportContainerVecRule() == SupportContainerVecRule::NOT_SUPPORT) {
                return std::make_unique<AverageSparkAggregatorFactory>();
            } else {
                return std::make_unique<AverageAggregatorFactory>();
            }
        }
        case OMNI_AGGREGATION_TYPE_SAMP: {
            return std::make_unique<StddevSampSparkAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_STD_POP: {
            return std::make_unique<StddevPopSparkAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_VAR_SAMP: {
            return std::make_unique<VarSampSparkAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_VAR_POP: {
            return std::make_unique<VarPopSparkAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_BIT_AND: {
            return std::make_unique<BitAndAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_BIT_OR: {
            return std::make_unique<BitOrAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_BIT_XOR: {
            return std::make_unique<BitXorAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_CORR: {
            return std::make_unique<CorrAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_COVAR_POP: {
            return std::make_unique<CovarPopAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_COVAR_SAMP: {
            return std::make_unique<CovarSampAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_MIN: {
            return std::make_unique<MinAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_MAX: {
            return std::make_unique<MaxAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_COUNT_COLUMN: {
            return std::make_unique<CountColumnAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_COUNT_ALL: {
            return std::make_unique<CountAllAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL: {
            return std::make_unique<FirstIgnoreNullAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL: {
            return std::make_unique<FirstIncludeNullAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_LAST_IGNORENULL: {
            return std::make_unique<LastIgnoreNullAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL: {
            return std::make_unique<LastIncludeNullAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_MIN_BY: {
            return std::make_unique<MinByAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_MAX_BY: {
            return std::make_unique<MaxByAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT: {
            return std::make_unique<ApproxCountDistinctAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_COLLECT_SET: {
            return std::make_unique<CollectSetAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_COLLECT_LIST: {
            return std::make_unique<CollectListAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_KURTOSIS: {
                    return std::make_unique<KurtosisAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_SKEWNESS: {
                    return std::make_unique<SkewnessAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE: {
            return std::make_unique<ApproxPercentileAggregatorFactory>();
        }
        case OMNI_AGGREGATION_TYPE_REGR_COUNT: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_COUNT);
        }
        case OMNI_AGGREGATION_TYPE_REGR_INTERCEPT: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_INTERCEPT);
        }
        case OMNI_AGGREGATION_TYPE_REGR_R2: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_R2);
        }
        case OMNI_AGGREGATION_TYPE_REGR_SLOPE: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SLOPE);
        }
        case OMNI_AGGREGATION_TYPE_REGR_SXX: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SXX);
        }
        case OMNI_AGGREGATION_TYPE_REGR_SXY: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SXY);
        }
        case OMNI_AGGREGATION_TYPE_REGR_SYY: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SYY);
        }
        case OMNI_AGGREGATION_TYPE_REGR_AVGX: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_AVGX);
        }
        case OMNI_AGGREGATION_TYPE_REGR_AVGY: {
            return std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_AVGY);
        }
        case OMNI_AGGREGATION_TYPE_REGR_REPLACEMENT: {
            return std::make_unique<RegrReplacementAggregatorFactory>();
        }
        default: {
            std::string omniExceptionInfo =
                "In function CreateAggregatorFactory, no such aggregate type " + std::to_string(aggType);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

std::unique_ptr<Aggregator> CorrAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
                                                                    const DataTypes &outputTypes,
                                                                    std::vector<int32_t> &channels, bool inputRaw,
                                                                    bool outputPartial,
                                                                    bool isOverflowAsNull) {
    // Gluten/Spark merge: inputAggBufferAttributes → 6 expressions → Omni gets 6 RAW Double columns only (no container).
    const size_t nInput = inputTypes.GetSize();
    if (nInput == 6) {
        for (size_t k = 0; k < 6; k++) {
            if (inputTypes.GetType(k)->GetId() != OMNI_DOUBLE) {
                throw OmniException("UNSUPPORTED_ERROR",
                    "Corr merge expects 6 DOUBLE columns; type[" + std::to_string(k) + "]=" +
                    std::to_string(static_cast<int32_t>(inputTypes.GetType(k)->GetId())));
            }
        }
        if (outputPartial)
            return CorrAggregator<OMNI_CONTAINER, OMNI_CONTAINER>::Create(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        else
            return CorrAggregator<OMNI_CONTAINER, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
    }
    if (nInput != 2) {
        throw OmniException("UNSUPPORTED_ERROR",
            "Corr requires 2 columns (raw) or 6 DOUBLE (merge). Got size=" + std::to_string(nInput));
    }
    if (!outputPartial && outputTypes.GetType(0)->GetId() != OMNI_DOUBLE) {
        throw OmniException("UNSUPPORTED_ERROR",
                            "Corr aggregator final output type must be DOUBLE");
    }
    auto inputTypeId = inputTypes.GetType(0)->GetId();
    switch (inputTypeId) {
        case OMNI_SHORT:
            return CorrAggregator<OMNI_SHORT, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                   inputRaw, outputPartial, isOverflowAsNull);
        case OMNI_INT:
            return CorrAggregator<OMNI_INT, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                 inputRaw, outputPartial, isOverflowAsNull);
        case OMNI_LONG:
            return CorrAggregator<OMNI_LONG, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                  inputRaw, outputPartial, isOverflowAsNull);
        case OMNI_FLOAT:
            return CorrAggregator<OMNI_FLOAT, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                   inputRaw, outputPartial, isOverflowAsNull);
        case OMNI_DOUBLE:
            return CorrAggregator<OMNI_DOUBLE, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                    inputRaw, outputPartial, isOverflowAsNull);
        case OMNI_DECIMAL64:
            return CorrAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                       inputRaw, outputPartial, isOverflowAsNull);
        case OMNI_DECIMAL128:
            return CorrAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                                                                        inputRaw, outputPartial, isOverflowAsNull);
        default: {
            std::string omniExceptionInfo =
                    "Corr aggregator does not support input type " + std::to_string(inputTypeId);
            throw OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

static std::unique_ptr<Aggregator> CreateCovarianceAggregator(FunctionType aggType, const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull) {
    const bool isPop = (aggType == OMNI_AGGREGATION_TYPE_COVAR_POP);
    const char *aggName = isPop ? "CovarPop" : "CovarSamp";
    // Gluten/Spark merge: inputAggBufferAttributes → 4 expressions → Omni gets 4 RAW Double columns only (no container).
    const size_t nInput = inputTypes.GetSize();
    if (nInput == 4) {
        for (size_t k = 0; k < 4; k++) {
            if (inputTypes.GetType(k)->GetId() != OMNI_DOUBLE) {
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                    std::string(aggName) + " merge expects 4 DOUBLE columns; type[" + std::to_string(k) + "]=" +
                    std::to_string(static_cast<int32_t>(inputTypes.GetType(k)->GetId())));
            }
        }
        if (outputPartial)
            return isPop ? CovarPopAggregator<OMNI_CONTAINER, OMNI_CONTAINER>::Create(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull)
                : CovarSampAggregator<OMNI_CONTAINER, OMNI_CONTAINER>::Create(inputTypes, outputTypes, channels,
                    inputRaw, outputPartial, isOverflowAsNull);
        else
            return isPop ? CovarPopAggregator<OMNI_CONTAINER, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull)
                : CovarSampAggregator<OMNI_CONTAINER, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
                    inputRaw, outputPartial, isOverflowAsNull);
    }
    if (nInput != 2) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            std::string(aggName) + " requires 2 columns (raw) or 4 DOUBLE (merge). Got size=" + std::to_string(nInput));
    }
    if (!outputPartial && outputTypes.GetType(0)->GetId() != OMNI_DOUBLE) {
        if (isPop)
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "CovarPop aggregator final output type must be DOUBLE");
        else
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "CovarSamp aggregator final output type must be DOUBLE");
    }
    if (inputTypes.GetType(0)->GetId() != OMNI_DOUBLE || inputTypes.GetType(1)->GetId() != OMNI_DOUBLE) {
        if (isPop)
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "CovarPop aggregator raw input requires both columns to be DOUBLE");
        else
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "CovarSamp aggregator raw input requires both columns to be DOUBLE");
    }
    return isPop ? CovarPopAggregator<OMNI_DOUBLE, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull)
        : CovarSampAggregator<OMNI_DOUBLE, OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels,
            inputRaw, outputPartial, isOverflowAsNull);
}

std::unique_ptr<Aggregator> CovarPopAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull) {
    return CreateCovarianceAggregator(OMNI_AGGREGATION_TYPE_COVAR_POP, inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull);
}

std::unique_ptr<Aggregator> CovarSampAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull) {
    return CreateCovarianceAggregator(OMNI_AGGREGATION_TYPE_COVAR_SAMP, inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull);
}

// Regr merge input type checks (per-case error messages).
static void CheckRegrSxxSyyPartialTypes(const DataTypes &inputTypes)
{
    if (inputTypes.GetSize() != 3) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge with 3 columns expects exactly 3 columns (count Long or Double, avg Double, m2 Double), got " +
                std::to_string(inputTypes.GetSize()));
    }
    auto id0 = inputTypes.GetType(0)->GetId();
    auto id1 = inputTypes.GetType(1)->GetId();
    auto id2 = inputTypes.GetType(2)->GetId();
    if (id0 != OMNI_LONG && id0 != OMNI_DOUBLE) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge 3-column: column 0 expected Long or Double, got " + std::to_string(id0));
    }
    if (id1 != OMNI_DOUBLE || id2 != OMNI_DOUBLE) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge 3-column: columns 1 and 2 must be Double, got " + std::to_string(id1) + ", " + std::to_string(id2));
    }
}

static void CheckSparkRegrSxyPartialTypes(const DataTypes &inputTypes)
{
    if (inputTypes.GetSize() != 4) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge with 4 columns (Spark regr_sxy partial) expects (n, xAvg, yAvg, ck), got " +
                std::to_string(inputTypes.GetSize()) + " columns");
    }
    auto id0 = inputTypes.GetType(0)->GetId();
    for (size_t j = 1; j < 4; j++) {
        if (inputTypes.GetType(j)->GetId() != OMNI_DOUBLE) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "Regr aggregate merge 4-column: column " + std::to_string(j) + " expected Double, got " +
                    std::to_string(inputTypes.GetType(j)->GetId()));
        }
    }
    if (id0 != OMNI_LONG && id0 != OMNI_DOUBLE) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge 4-column: column 0 expected Long or Double, got " + std::to_string(id0));
    }
}

static void CheckNativePartialTypes(const DataTypes &inputTypes)
{
    if (inputTypes.GetSize() != 6) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge with 6 columns (native partial) expects (count Long/Int/Double, meanX, meanY, c2, m2X, m2Y Double), got " +
                std::to_string(inputTypes.GetSize()) + " columns");
    }
    auto id0 = inputTypes.GetType(0)->GetId();
    if (id0 != OMNI_LONG && id0 != OMNI_INT && id0 != OMNI_DOUBLE) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge 6-column: column 0 expected Long, Int, or Double, got " + std::to_string(id0));
    }
    for (size_t j = 1; j < 6; j++) {
        if (inputTypes.GetType(j)->GetId() != OMNI_DOUBLE) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "Regr aggregate merge 6-column: column " + std::to_string(j) + " expected Double, got " +
                    std::to_string(inputTypes.GetType(j)->GetId()));
        }
    }
}

static void CheckSparkCovarianceVarPopPartialTypes(const DataTypes &inputTypes)
{
    if (inputTypes.GetSize() != 7) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge with 7 columns (Spark Covariance+VarPop partial) expects 7 columns, got " +
                std::to_string(inputTypes.GetSize()) + " columns");
    }
    auto id0 = inputTypes.GetType(0)->GetId();
    if (id0 != OMNI_LONG && id0 != OMNI_INT && id0 != OMNI_DOUBLE) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Regr aggregate merge 7-column: column 0 expected Long, Int, or Double, got " + std::to_string(id0));
    }
    for (size_t j = 1; j < 7; j++) {
        if (inputTypes.GetType(j)->GetId() != OMNI_DOUBLE) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "Regr aggregate merge 7-column: column " + std::to_string(j) + " expected Double, got " +
                    std::to_string(inputTypes.GetType(j)->GetId()));
        }
    }
}

std::unique_ptr<Aggregator> RegrAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    if (inputRaw) {
        if (inputTypes.GetSize() != 2) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "Regr aggregate requires exactly two input columns (y, x) for raw input");
        }
        auto id0 = inputTypes.GetType(0)->GetId();
        auto id1 = inputTypes.GetType(1)->GetId();
        auto isRegrNumeric = [](type::DataTypeId id) {
            return id == OMNI_BOOLEAN || id == OMNI_BYTE || id == OMNI_SHORT || id == OMNI_INT ||
                   id == OMNI_LONG || id == OMNI_FLOAT || id == OMNI_DOUBLE;
        };
        if (!isRegrNumeric(id0) || !isRegrNumeric(id1)) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "Regr aggregate (y, x) inputs must be numeric: boolean, byte, short, int, long, float, or double");
        }
    } else {
        size_t n = inputTypes.GetSize();
        bool allow3 = (aggregateType == OMNI_AGGREGATION_TYPE_REGR_SXX || aggregateType == OMNI_AGGREGATION_TYPE_REGR_SYY);
        switch (n) {
            case 3:
                if (!allow3) {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                        "Regr aggregate merge: 3 columns only allowed for regr_sxx/regr_syy (n, avg, m2)");
                }
                CheckRegrSxxSyyPartialTypes(inputTypes);
                break;
            case 4:
                CheckSparkRegrSxyPartialTypes(inputTypes);
                break;
            case 6:
                CheckNativePartialTypes(inputTypes);
                break;
            case 7:
                CheckSparkCovarianceVarPopPartialTypes(inputTypes);
                break;
            default:
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                    "Regr aggregate merge requires 3, 4, 6, or 7 columns, got " + std::to_string(n));
        }
    }
    switch (aggregateType) {
        case OMNI_AGGREGATION_TYPE_REGR_COUNT:
            return std::make_unique<RegrCountAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_INTERCEPT:
            return std::make_unique<RegrInterceptAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_R2:
            return std::make_unique<RegrR2Aggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_SLOPE:
            return std::make_unique<RegrSlopeAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_SXX:
            return std::make_unique<RegrSxxAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_SXY:
            return std::make_unique<RegrSxyAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_SYY:
            return std::make_unique<RegrSyyAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_AVGX:
            return std::make_unique<RegrAvgXAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        case OMNI_AGGREGATION_TYPE_REGR_AVGY:
            return std::make_unique<RegrAvgYAggregator>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        default: {
            std::string msg =
                "RegrAggregatorFactory: unsupported aggregate type " + std::to_string(static_cast<int>(aggregateType));
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", msg);
        }
    }
}

std::unique_ptr<Aggregator> RegrReplacementAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    if (inputRaw) {
        if (inputTypes.GetSize() != 1) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "RegrReplacement requires exactly one input column (the replacement expression value) for raw input");
        }
        if (inputTypes.GetType(0)->GetId() != OMNI_DOUBLE) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "RegrReplacement supports double type only for raw input");
        }
    } else {
        if (inputTypes.GetSize() != 3) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "RegrReplacement merge requires 3 input columns (n, avg, m2)");
        }
    }
    return std::make_unique<RegrReplacementAggregator>(inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull);
}

template <template <bool, bool, typename...> class T, typename... Args>
std::unique_ptr<Aggregator> CreateAggregatorHelper(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
{
    if (inputRaw) {
        if (outputPartial) {
            return std::make_unique<T<true, true, Args...>>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        } else {
            return std::make_unique<T<true, false, Args...>>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        }
    } else {
        if (outputPartial) {
            return std::make_unique<T<false, true, Args...>>(inputTypes, outputTypes, channels, inputRaw, outputPartial,
                isOverflowAsNull);
        } else {
            return std::make_unique<T<false, false, Args...>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
    }
}

std::unique_ptr<Aggregator> SumSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_BYTE: {
            return std::make_unique<SumFlatIMAggregator<OMNI_BYTE, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_SHORT: {
            return std::make_unique<SumFlatIMAggregator<OMNI_SHORT, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_INT: {
            return std::make_unique<SumFlatIMAggregator<OMNI_INT, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_LONG: {
            return std::make_unique<SumFlatIMAggregator<OMNI_LONG, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_FLOAT: {
            return std::make_unique<SumFlatIMAggregator<OMNI_FLOAT, OMNI_DOUBLE>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<SumFlatIMAggregator<OMNI_DOUBLE, OMNI_DOUBLE>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_DECIMAL64: {
            auto outputTypeId = outputTypes.GetIds()[0];
            if (outputTypeId == OMNI_DECIMAL64) {
                return std::make_unique<SumSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<SumSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
        case OMNI_DECIMAL128: {
            return std::make_unique<SumSparkDecimalAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>>(inputTypes,
                outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
        }
        default: {
            std::string omniExceptionInfo =
                "In function SumSparkAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

std::unique_ptr<Aggregator> TrySumSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_BYTE: {
            return std::make_unique<TrySumFlatIMAggregator<OMNI_BYTE, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_SHORT: {
            return std::make_unique<TrySumFlatIMAggregator<OMNI_SHORT, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_INT: {
            return std::make_unique<TrySumFlatIMAggregator<OMNI_INT, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_LONG: {
            return std::make_unique<TrySumFlatIMAggregator<OMNI_LONG, OMNI_LONG>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_FLOAT: {
            return std::make_unique<SumFlatIMAggregator<OMNI_FLOAT, OMNI_DOUBLE>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<SumFlatIMAggregator<OMNI_DOUBLE, OMNI_DOUBLE>>(inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_DECIMAL64: {
            auto outputTypeId = outputTypes.GetIds()[0];
            if (outputTypeId == OMNI_DECIMAL64) {
                return std::make_unique<SumSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<SumSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
        case OMNI_DECIMAL128: {
            return std::make_unique<SumSparkDecimalAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>>(inputTypes,
                outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
        }
        default: {
            std::string omniExceptionInfo =
                "In function TrySumSparkAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}


std::unique_ptr<Aggregator> AverageSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    // fetch first inputTypes id as aggregator input type and map to type
    // spark rule for average function input type:
    //    timestamp/sting: cast as double
    //    boolean, date, binnary: not support
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_BYTE: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_BYTE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_SHORT: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_SHORT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_INT: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_INT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_LONG: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_LONG>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_FLOAT: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_FLOAT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_DOUBLE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DECIMAL64: {
            auto outputTypeId = outputTypes.GetIds()[0];
            if (outputTypeId == OMNI_DECIMAL64) {
                return std::make_unique<AverageSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<AverageSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
        case OMNI_DECIMAL128: {
            // for calculate, all types of intermedia and input data should be decimal128 ,
            // so all template types are Decimal128
            // but for final result , Decimal128 / n = Decimal64 exist, we will handle result in extractValue function
            return std::make_unique<AverageSparkDecimalAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>>(inputTypes,
                outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
        }
        default: {
            std::string omniExceptionInfo =
                "In function AverageSparkAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

std::unique_ptr<Aggregator> TryAverageSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    // fetch first inputTypes id as aggregator input type and map to type
    // spark rule for average function input type:
    //    timestamp/sting: cast as double
    //    boolean, date, binnary: not support
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_BYTE: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_BYTE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, true);
        }
        case OMNI_SHORT: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_SHORT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, true);
        }
        case OMNI_INT: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_INT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, true);
        }
        case OMNI_LONG: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_LONG>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, true);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<AverageFlatIMAggregator<OMNI_DOUBLE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, true);
        }
        case OMNI_DECIMAL64: {
            auto outputTypeId = outputTypes.GetIds()[0];
            if (outputTypeId == OMNI_DECIMAL64) {
                return std::make_unique<AverageSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, true);
            } else {
                return std::make_unique<AverageSparkDecimalAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>>(inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, true);
            }
        }
        case OMNI_DECIMAL128: {
            // for calculate, all types of intermedia and input data should be decimal128 ,
            // so all template types are Decimal128
            // but for final result , Decimal128 / n = Decimal64 exist, we will handle result in extractValue function
            return std::make_unique<AverageSparkDecimalAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>>(inputTypes,
                outputTypes, channels, inputRaw, outputPartial, true);
        }
        default: {
            std::string omniExceptionInfo =
                "In function TryAverageSparkAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

std::unique_ptr<Aggregator> StddevSampSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_FLOAT: {
            return std::make_unique<StddevSampAggregator<OMNI_FLOAT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<StddevSampAggregator<OMNI_DOUBLE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        default: {
            LogError("Unsupported input type %d for spark stddev_samp aggregate", inputTypeId);
            return nullptr;
        }
    }
}

std::unique_ptr<Aggregator> StddevPopSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_FLOAT: {
            return std::make_unique<StddevPopAggregator<OMNI_FLOAT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<StddevPopAggregator<OMNI_DOUBLE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        default: {
            LogError("Unsupported input type %d for spark stddev_pop aggregate", inputTypeId);
            return nullptr;
        }
    }
}

std::unique_ptr<Aggregator> VarSampSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_FLOAT: {
            return std::make_unique<VarSampAggregator<OMNI_FLOAT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<VarSampAggregator<OMNI_DOUBLE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        default: {
            LogError("Unsupported input type %d for spark var_samp aggregate", inputTypeId);
            return nullptr;
        }
    }
}

std::unique_ptr<Aggregator> VarPopSparkAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_FLOAT: {
            return std::make_unique<VarPopAggregator<OMNI_FLOAT>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return std::make_unique<VarPopAggregator<OMNI_DOUBLE>>(inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        default: {
            LogError("Unsupported input type %d for spark var_pop aggregate", inputTypeId);
            return nullptr;
        }
    }
}

template <typename InputType>
std::unique_ptr<Aggregator> FirstAggregatorFactory::CreateFirstAggregatorHelper(FunctionType aggregateType,
    const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw,
    bool outputPartial, bool isOverflowAsNull)
{
    if (inputRaw) {
        if (outputPartial) {
            if (aggregateType == OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL) {
                return std::make_unique<FirstAggregator<true, true, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<FirstAggregator<true, true, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        } else {
            if (aggregateType == OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL) {
                return std::make_unique<FirstAggregator<true, false, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<FirstAggregator<true, false, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
    } else {
        if (outputPartial) {
            if (aggregateType == OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL) {
                return std::make_unique<FirstAggregator<false, true, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<FirstAggregator<false, true, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        } else {
            if (aggregateType == OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL) {
                return std::make_unique<FirstAggregator<false, false, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<FirstAggregator<false, false, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
    }
}

std::unique_ptr<Aggregator> FirstAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    // fetch first inputTypes id as aggregator input type and map to type
    // spark rule for first function input type:
    //    binnary/sting: run with SortAggregateExec, so current not implemented
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_BOOLEAN: {
            return CreateFirstAggregatorHelper<bool>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_BYTE: {
            return CreateFirstAggregatorHelper<int8_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_SHORT: {
            return CreateFirstAggregatorHelper<int16_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            return CreateFirstAggregatorHelper<int32_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            return CreateFirstAggregatorHelper<int64_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_FLOAT: {
            return CreateFirstAggregatorHelper<float>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return CreateFirstAggregatorHelper<double>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DECIMAL128: {
            return CreateFirstAggregatorHelper<Decimal128>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            return CreateFirstAggregatorHelper<std::string_view>(aggregateType, inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_ARRAY:
        case OMNI_MAP:
        case OMNI_ROW: {
            return std::make_unique<FirstComplexAggregator>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        default: {
            std::string omniExceptionInfo =
                "In function FirstAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

template <typename InputType>
std::unique_ptr<Aggregator> LastAggregatorFactory::CreateLastAggregatorHelper(FunctionType aggregateType,
    const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw,
    bool outputPartial, bool isOverflowAsNull)
{
    if (inputRaw) {
        if (outputPartial) {
            if (aggregateType == OMNI_AGGREGATION_TYPE_LAST_IGNORENULL) {
                return std::make_unique<LastAggregator<true, true, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<LastAggregator<true, true, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        } else {
            if (aggregateType == OMNI_AGGREGATION_TYPE_LAST_IGNORENULL) {
                return std::make_unique<LastAggregator<true, false, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<LastAggregator<true, false, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
    } else {
        if (outputPartial) {
            if (aggregateType == OMNI_AGGREGATION_TYPE_LAST_IGNORENULL) {
                return std::make_unique<LastAggregator<false, true, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<LastAggregator<false, true, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        } else {
            if (aggregateType == OMNI_AGGREGATION_TYPE_LAST_IGNORENULL) {
                return std::make_unique<LastAggregator<false, false, true, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            } else {
                return std::make_unique<LastAggregator<false, false, false, InputType>>(aggregateType, inputTypes,
                    outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            }
        }
    }
}

std::unique_ptr<Aggregator> LastAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetIds()[0];
    switch (inputTypeId) {
        case OMNI_BOOLEAN: {
            return CreateLastAggregatorHelper<bool>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_BYTE: {
            return CreateLastAggregatorHelper<int8_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_SHORT: {
            return CreateLastAggregatorHelper<int16_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            return CreateLastAggregatorHelper<int32_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            return CreateLastAggregatorHelper<int64_t>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_FLOAT: {
            return CreateLastAggregatorHelper<float>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DOUBLE: {
            return CreateLastAggregatorHelper<double>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DECIMAL128: {
            return CreateLastAggregatorHelper<Decimal128>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            return CreateLastAggregatorHelper<std::string_view>(aggregateType, inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        case OMNI_ARRAY:
        case OMNI_MAP:
        case OMNI_ROW: {
            return std::make_unique<LastComplexAggregator>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        default: {
            std::string omniExceptionInfo =
                "In function LastAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

// approx_count_distinct two-phase: Partial operator one aggregate (raw type -> VARBINARY), Final operator one aggregate (VARBINARY -> LONG).
// Distinguished by plan inputRaws/outputPartials: Partial=(true,true), Final=(false,false), single-stage=(true,false).
std::unique_ptr<Aggregator> ApproxCountDistinctAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    auto inputTypeId = inputTypes.GetType(0)->GetId();
    if (inputRaw && outputPartial) {
        // Partial phase: raw column -> serialized HLL/boolean state (VARBINARY); supports bool/tinyint/smallint/integer/bigint/real/double/varchar/varbinary/timestamp/date etc. aligned with Velox
        switch (inputTypeId) {
            case OMNI_BOOLEAN:
                return ApproxCountDistinctAggregator<OMNI_BOOLEAN, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return ApproxCountDistinctAggregator<OMNI_BYTE, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return ApproxCountDistinctAggregator<OMNI_SHORT, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return ApproxCountDistinctAggregator<OMNI_LONG, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_TIME32:
                return ApproxCountDistinctAggregator<OMNI_INT, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return ApproxCountDistinctAggregator<OMNI_FLOAT, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return ApproxCountDistinctAggregator<OMNI_DOUBLE, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return ApproxCountDistinctAggregator<OMNI_VARCHAR, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return ApproxCountDistinctAggregator<OMNI_CHAR, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return ApproxCountDistinctAggregator<OMNI_VARBINARY, OMNI_VARBINARY>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            default: {
                std::string omniExceptionInfo =
                    "ApproxCountDistinctAggregatorFactory: unsupported input type " + std::to_string(inputTypeId);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
            }
        }
    }
    if (inputRaw && !outputPartial) {
        // Single-stage: raw column -> approximate cardinality output (LONG) directly
        switch (inputTypeId) {
            case OMNI_BOOLEAN:
                return ApproxCountDistinctAggregator<OMNI_BOOLEAN, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return ApproxCountDistinctAggregator<OMNI_BYTE, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return ApproxCountDistinctAggregator<OMNI_SHORT, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return ApproxCountDistinctAggregator<OMNI_LONG, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_TIME32:
                return ApproxCountDistinctAggregator<OMNI_INT, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return ApproxCountDistinctAggregator<OMNI_FLOAT, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return ApproxCountDistinctAggregator<OMNI_DOUBLE, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return ApproxCountDistinctAggregator<OMNI_VARCHAR, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return ApproxCountDistinctAggregator<OMNI_CHAR, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return ApproxCountDistinctAggregator<OMNI_VARBINARY, OMNI_LONG>::Create(inputTypes, outputTypes,
                    channels, inputRaw, outputPartial, isOverflowAsNull);
            default: {
                std::string omniExceptionInfo =
                    "ApproxCountDistinctAggregatorFactory: unsupported input type " + std::to_string(inputTypeId);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
            }
        }
    }
    if (!inputRaw && !outputPartial) {
        // Final phase: upstream Partial VARBINARY output -> merge and output approximate cardinality (LONG)
        return ApproxCountDistinctAggregator<OMNI_VARBINARY, OMNI_LONG>::Create(inputTypes, outputTypes,
            channels, inputRaw, outputPartial, isOverflowAsNull);
    }
    std::string omniExceptionInfo =
        "ApproxCountDistinctAggregatorFactory: invalid inputRaw/outputPartial combination";
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
}

std::unique_ptr<Aggregator> DynamicUdafAggregatorFactory::CreateAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull)
{
    return UdafRegistry::getInstance().create(
        udafName_, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
}
}
}
