/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: Aggregation Base Class
 */

#include "aggregation.h"
#include "aggregator/aggregator_factory.h"
namespace omniruntime {
namespace op {
template <class T>
void AggregationCommonOperatorFactory::CreateAggregatorFactory(
    std::vector<std::unique_ptr<AggregatorFactory>> &aggregatorFactories, int32_t maskCol)
{
    if (maskCol == Aggregator::INVALID_MASK_COL) {
        aggregatorFactories.push_back(std::make_unique<T>());
    } else {
        aggregatorFactories.push_back(std::make_unique<MaskAggregatorFactory<T>>(maskCol));
    }
}

OmniStatus AggregationCommonOperatorFactory::CreateAggregatorFactories(
    std::vector<std::unique_ptr<AggregatorFactory>> &aggregatorFactories, const std::vector<uint32_t> &funcTypesContext,
    const std::vector<int32_t> &maskCols, const std::vector<std::string> &udafNames)
{
    OmniStatus ret = OMNI_STATUS_NORMAL;

    for (uint32_t i = 0; i < funcTypesContext.size(); ++i) {
        switch (funcTypesContext[i]) {
            case OMNI_AGGREGATION_TYPE_SUM: {
                if (ConfigUtil::GetSupportContainerVecRule() == SupportContainerVecRule::NOT_SUPPORT) {
                    CreateAggregatorFactory<SumSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                } else {
                    CreateAggregatorFactory<SumAggregatorFactory>(aggregatorFactories, maskCols[i]);
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_COUNT_COLUMN: {
                CreateAggregatorFactory<CountColumnAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_COUNT_ALL: {
                CreateAggregatorFactory<CountAllAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_MAX: {
                CreateAggregatorFactory<MaxAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_MIN: {
                CreateAggregatorFactory<MinAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_MIN_BY: {
                CreateAggregatorFactory<MinByAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_MAX_BY: {
                CreateAggregatorFactory<MaxByAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_BIT_AND: {
                CreateAggregatorFactory<BitAndAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_BIT_OR: {
                CreateAggregatorFactory<BitOrAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_BIT_XOR: {
                CreateAggregatorFactory<BitXorAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_AVG: {
                if (ConfigUtil::GetSupportContainerVecRule() == SupportContainerVecRule::NOT_SUPPORT) {
                    CreateAggregatorFactory<AverageSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                } else {
                    CreateAggregatorFactory<AverageAggregatorFactory>(aggregatorFactories, maskCols[i]);
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_SAMP: {
                CreateAggregatorFactory<StddevSampSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_STD_POP: {
                CreateAggregatorFactory<StddevPopSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_VAR_SAMP: {
                CreateAggregatorFactory<VarSampSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_VAR_POP: {
                CreateAggregatorFactory<VarPopSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL: {
                CreateAggregatorFactory<FirstIgnoreNullAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL: {
                CreateAggregatorFactory<FirstIncludeNullAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_LAST_IGNORENULL: {
                CreateAggregatorFactory<LastIgnoreNullAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL: {
                CreateAggregatorFactory<LastIncludeNullAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_TRY_SUM: {
                CreateAggregatorFactory<TrySumSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_TRY_AVG: {
                CreateAggregatorFactory<TryAverageSparkAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_BLOOM_FILTER: {
                CreateAggregatorFactory<BloomFilterAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT: {
                CreateAggregatorFactory<ApproxCountDistinctAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_CORR: {
                CreateAggregatorFactory<CorrAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_COVAR_POP: {
                CreateAggregatorFactory<CovarPopAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_COVAR_SAMP: {
                CreateAggregatorFactory<CovarSampAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_COLLECT_SET: {
                CreateAggregatorFactory<CollectSetAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_COLLECT_LIST: {
                CreateAggregatorFactory<CollectListAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_KURTOSIS: {
                CreateAggregatorFactory<KurtosisAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_SKEWNESS: {
                CreateAggregatorFactory<SkewnessAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE: {
                CreateAggregatorFactory<ApproxPercentileAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_COUNT: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_COUNT));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_INTERCEPT: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_INTERCEPT));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_R2: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_R2));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_SLOPE: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SLOPE));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_SXX: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SXX));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_SXY: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SXY));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_SYY: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_SYY));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_AVGX: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_AVGX));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_AVGY: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrAggregatorFactory>(OMNI_AGGREGATION_TYPE_REGR_AVGY));
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Regr aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_REGR_REPLACEMENT: {
                if (maskCols[i] == Aggregator::INVALID_MASK_COL) {
                    aggregatorFactories.push_back(std::make_unique<RegrReplacementAggregatorFactory>());
                } else {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "RegrReplacement aggregate does not support mask column");
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_UDAF: {
                if (i >= udafNames.size() || udafNames[i].empty()) {
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Missing UDAF name for dynamic aggregate");
                }
                aggregatorFactories.push_back(std::make_unique<DynamicUdafAggregatorFactory>(udafNames[i]));
                break;
            }
            default: {
                std::string omniExceptionInfo = "In function CreateAggregatorFactories, No such agg func type " +
                    std::to_string(funcTypesContext[i]);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
            }
        }
    }

    return ret;
}
} // end of namespace op
} // end of namespace omniruntime