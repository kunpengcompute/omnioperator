/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "aggregator_factory.h"

namespace omniruntime {
namespace op {
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
        default: {
            std::string omniExceptionInfo =
                "In function CreateAggregatorFactory, no such aggregate type " + std::to_string(aggType);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
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
        case OMNI_DOUBLE: {
            return CreateFirstAggregatorHelper<double>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_DECIMAL128: {
            return CreateFirstAggregatorHelper<Decimal128>(aggregateType, inputTypes, outputTypes, channels, inputRaw,
                outputPartial, isOverflowAsNull);
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR: {
            return CreateFirstAggregatorHelper<std::string_view>(aggregateType, inputTypes, outputTypes, channels,
                inputRaw, outputPartial, isOverflowAsNull);
        }
        default: {
            std::string omniExceptionInfo =
                "In function FirstAggregatorFactory::CreateAggregator, no such input type " +
                std::to_string(inputTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}
}
}
