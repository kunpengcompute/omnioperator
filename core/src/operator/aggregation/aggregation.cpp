/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
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
    const std::vector<int32_t> &maskCols)
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
            case OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL: {
                CreateAggregatorFactory<FirstIgnoreNullAggregatorFactory>(aggregatorFactories, maskCols[i]);
                break;
            }
            case OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL: {
                CreateAggregatorFactory<FirstIncludeNullAggregatorFactory>(aggregatorFactories, maskCols[i]);
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