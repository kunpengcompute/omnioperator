/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: Aggregate factories
 */
#ifndef OMNI_RUNTIME_AGGREGATOR_FACTORY_H
#define OMNI_RUNTIME_AGGREGATOR_FACTORY_H

#include "all_aggregators.h"
#include "first_complex_aggregator.h"
#include "last_complex_aggregator.h"
#include "maxby_complex_aggregator.h"
#include "maxby_complex_varchar_aggregator.h"
#include "minby_complex_aggregator.h"
#include "minby_complex_varchar_aggregator.h"
#include "min_complex_aggregator.h"
#include "max_complex_aggregator.h"
#include "util/config_util.h"
#include "operator/util/function_type.h"

namespace omniruntime {
namespace op {
template <template <DataTypeId, DataTypeId> class T> class TypedAggregatorFactory : public AggregatorFactory {
public:
    TypedAggregatorFactory() = default;
    ~TypedAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override
    {
        return CreateAggregatorInternal(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
    }

protected:
    std::unique_ptr<Aggregator> CreateAggregatorInternal(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    {
        auto outputTypeId = outputTypes.GetType(0)->GetId();
        switch (outputTypeId) {
            case OMNI_BOOLEAN:
                return FromKnownOutput<OMNI_BOOLEAN>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return FromKnownOutput<OMNI_BYTE>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return FromKnownOutput<OMNI_SHORT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return FromKnownOutput<OMNI_INT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return FromKnownOutput<OMNI_LONG>(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return FromKnownOutput<OMNI_FLOAT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return FromKnownOutput<OMNI_DOUBLE>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL64:
                return FromKnownOutput<OMNI_DECIMAL64>(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL128:
                return FromKnownOutput<OMNI_DECIMAL128>(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CONTAINER:
                return FromKnownOutput<OMNI_CONTAINER>(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return FromKnownOutput<OMNI_VARCHAR>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return FromKnownOutput<OMNI_CHAR>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return FromKnownOutput<OMNI_VARBINARY>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_ARRAY:
                return FromKnownOutput<OMNI_ARRAY>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                                                  outputPartial, isOverflowAsNull);
            case OMNI_MAP:
                return FromKnownOutput<OMNI_MAP>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_ROW:
                return FromKnownOutput<OMNI_ROW>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            default:
                std::string omniExceptionInfo =
                    "In function CreateAggregatorInternal, no such input type " + std::to_string(outputTypeId);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }

    template <DataTypeId OUT_ID>
    std::unique_ptr<Aggregator> FromKnownOutput(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    {
        auto inputTypeId = inputTypes.GetType(0)->GetId();
        switch (inputTypeId) {
            case OMNI_BOOLEAN:
                return T<OMNI_BOOLEAN, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return T<OMNI_BYTE, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return T<OMNI_SHORT, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return T<OMNI_INT, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return T<OMNI_LONG, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return T<OMNI_FLOAT, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return T<OMNI_DOUBLE, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL64:
                return T<OMNI_DECIMAL64, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL128:
                return T<OMNI_DECIMAL128, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CONTAINER:
                return T<OMNI_CONTAINER, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return T<OMNI_VARCHAR, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return T<OMNI_CHAR, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return T<OMNI_VARBINARY, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_ARRAY:
                return T<OMNI_ARRAY, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_MAP:
                return T<OMNI_MAP, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_ROW:
                return T<OMNI_ROW, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_NONE:
                return T<OMNI_NONE, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw,
                    outputPartial, isOverflowAsNull);
            case OMNI_INVALID:
                return T<OMNI_INVALID, OUT_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels,
                    inputRaw, outputPartial, isOverflowAsNull);
            default:
                std::string omniExceptionInfo =
                    "In function FromKnownOutput, no such input type " + std::to_string(inputTypeId);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
};

template <template <bool, bool, typename...> class T, typename... Args>
std::unique_ptr<Aggregator> CreateAggregatorHelper(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false, bool isOverflowAsNull = true);

// Implementation of Aggregator factories
class SumSparkAggregatorFactory : public AggregatorFactory {
public:
    SumSparkAggregatorFactory() = default;
    ~SumSparkAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class TrySumSparkAggregatorFactory : public AggregatorFactory {
public:
    TrySumSparkAggregatorFactory() = default;
    ~TrySumSparkAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
         std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class AverageSparkAggregatorFactory : public AggregatorFactory {
public:
    AverageSparkAggregatorFactory() = default;
    ~AverageSparkAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class TryAverageSparkAggregatorFactory : public AggregatorFactory {
public:
    TryAverageSparkAggregatorFactory() = default;
    ~TryAverageSparkAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
         std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
         bool isOverflowAsNull = true) override;
};


class StddevSampSparkAggregatorFactory : public AggregatorFactory {
public:
    StddevSampSparkAggregatorFactory() = default;
    ~StddevSampSparkAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class StddevPopSparkAggregatorFactory : public AggregatorFactory {
public:
    StddevPopSparkAggregatorFactory() = default;
    ~StddevPopSparkAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class VarSampSparkAggregatorFactory : public AggregatorFactory {
public:
    VarSampSparkAggregatorFactory() = default;
    ~VarSampSparkAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class VarPopSparkAggregatorFactory : public AggregatorFactory {
public:
    VarPopSparkAggregatorFactory() = default;
    ~VarPopSparkAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class FirstAggregatorFactory : public AggregatorFactory {
public:
    explicit FirstAggregatorFactory(FunctionType aggregateType) : aggregateType(aggregateType) {}
    ~FirstAggregatorFactory() override = default;
    template <typename InputType>
    std::unique_ptr<Aggregator> CreateFirstAggregatorHelper(FunctionType aggregateType, const DataTypes &inputTypes,
        const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true);

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = false) override;

private:
    FunctionType aggregateType;
};

class FirstIgnoreNullAggregatorFactory : public FirstAggregatorFactory {
public:
    explicit FirstIgnoreNullAggregatorFactory() : FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL) {}
    ~FirstIgnoreNullAggregatorFactory() override = default;
};

class FirstIncludeNullAggregatorFactory : public FirstAggregatorFactory {
public:
    explicit FirstIncludeNullAggregatorFactory() : FirstAggregatorFactory(OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL) {}
    ~FirstIncludeNullAggregatorFactory() override = default;
};

class LastAggregatorFactory : public AggregatorFactory {
public:
    explicit LastAggregatorFactory(FunctionType aggregateType) : aggregateType(aggregateType) {}
    ~LastAggregatorFactory() override = default;
    template <typename InputType>
    std::unique_ptr<Aggregator> CreateLastAggregatorHelper(FunctionType aggregateType, const DataTypes &inputTypes,
        const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true);

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = false) override;

private:
    FunctionType aggregateType;
};

class LastIgnoreNullAggregatorFactory : public LastAggregatorFactory {
public:
    explicit LastIgnoreNullAggregatorFactory() : LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL) {}
    ~LastIgnoreNullAggregatorFactory() override = default;
};

class LastIncludeNullAggregatorFactory : public LastAggregatorFactory {
public:
    explicit LastIncludeNullAggregatorFactory() : LastAggregatorFactory(OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL) {}
    ~LastIncludeNullAggregatorFactory() override = default;
};

class AverageAggregatorFactory : public TypedAggregatorFactory<AverageAggregator> {
public:
    AverageAggregatorFactory() : TypedAggregatorFactory<AverageAggregator>() {}
    ~AverageAggregatorFactory() override = default;
};

class SumAggregatorFactory : public TypedAggregatorFactory<SumAggregator> {
public:
    SumAggregatorFactory() : TypedAggregatorFactory<SumAggregator>() {}
    ~SumAggregatorFactory() override = default;
};

class BitAndAggregatorFactory : public TypedAggregatorFactory<BitAndAggregator> {
public:
    BitAndAggregatorFactory() : TypedAggregatorFactory<BitAndAggregator>() {}
    ~BitAndAggregatorFactory() override = default;
};

class BitOrAggregatorFactory : public TypedAggregatorFactory<BitOrAggregator> {
public:
    BitOrAggregatorFactory() : TypedAggregatorFactory<BitOrAggregator>() {}
    ~BitOrAggregatorFactory() override = default;
};

class BitXorAggregatorFactory : public TypedAggregatorFactory<BitXorAggregator> {
public:
    BitXorAggregatorFactory() : TypedAggregatorFactory<BitXorAggregator>() {}
    ~BitXorAggregatorFactory() override = default;
};

class CorrAggregatorFactory : public AggregatorFactory {
public:
    CorrAggregatorFactory() = default;
    ~CorrAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override;
};

class CovarPopAggregatorFactory : public AggregatorFactory {
public:
    CovarPopAggregatorFactory() = default;
    ~CovarPopAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override;
};

class CovarSampAggregatorFactory : public AggregatorFactory {
public:
    CovarSampAggregatorFactory() = default;
    ~CovarSampAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override;
};

class RegrAggregatorFactory : public AggregatorFactory {
public:
    explicit RegrAggregatorFactory(FunctionType aggregateType) : aggregateType(aggregateType) {}
    ~RegrAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;

private:
    FunctionType aggregateType;
};

class RegrReplacementAggregatorFactory : public AggregatorFactory {
public:
    RegrReplacementAggregatorFactory() = default;
    ~RegrReplacementAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class MinAggregatorFactory : public AggregatorFactory {
public:
    MinAggregatorFactory() : AggregatorFactory() {}
    ~MinAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
private:
    TypedAggregatorFactory<MinAggregator> typedFactory_;
};

class MaxAggregatorFactory : public AggregatorFactory {
public:
    MaxAggregatorFactory() : AggregatorFactory() {}
    ~MaxAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
private:
    TypedAggregatorFactory<MaxAggregator> typedFactory_;
};

// CollectSet: always use element type T as template param, create CollectSetAggregator<T, T> for both partial and final.
// Partial: input T -> CollectSetAggregator<T, T>; Final: input Array<T> -> element type T -> CollectSetAggregator<T, T>.
// String types (OMNI_CHAR, OMNI_VARCHAR) and nested array (OMNI_ARRAY) are passed through for custom handling.
class CollectSetAggregatorFactory : public AggregatorFactory {
public:
    CollectSetAggregatorFactory() = default;
    ~CollectSetAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override;
};

class CollectListAggregatorFactory : public AggregatorFactory {
public:
    CollectListAggregatorFactory() = default;
    ~CollectListAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override;
};

class MinByAggregatorFactory : public AggregatorFactory {
public:
    MinByAggregatorFactory() : AggregatorFactory() {}
    ~MinByAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override
    {
        if (inputTypes.GetSize() != 2) {
            throw std::runtime_error("Input data types must have exactly two inputs");
        }
        return CreateAggregatorInternal(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
    }

protected:
    std::unique_ptr<Aggregator> CreateAggregatorInternal(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    {
        auto col2Id = inputTypes.GetType(1)->GetId();
        switch (col2Id) {
            case OMNI_BOOLEAN:
                return FromKnownOutput<OMNI_BOOLEAN>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return FromKnownOutput<OMNI_BYTE>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return FromKnownOutput<OMNI_SHORT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return FromKnownOutput<OMNI_INT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return FromKnownOutput<OMNI_LONG>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return FromKnownOutput<OMNI_FLOAT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return FromKnownOutput<OMNI_DOUBLE>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL64:
                return FromKnownOutput<OMNI_DECIMAL64>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL128:
                return FromKnownOutput<OMNI_DECIMAL128>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return FromKnownOutput<OMNI_VARCHAR>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return FromKnownOutput<OMNI_CHAR>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return FromKnownOutput<OMNI_VARBINARY>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            default:
                std::string omniExceptionInfo = "In MinByAggretator function FromKnownOutput, no such input col2 type " + std::to_string(col2Id);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }

    template <DataTypeId COL2_ID>
    std::unique_ptr<Aggregator> FromKnownOutput(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    {
        auto col1Id = inputTypes.GetType(0)->GetId();
        switch (col1Id) {
            case OMNI_BOOLEAN:
                return MinByAggregator<OMNI_BOOLEAN, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return MinByAggregator<OMNI_BYTE, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return MinByAggregator<OMNI_SHORT, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return MinByAggregator<OMNI_INT, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return MinByAggregator<OMNI_LONG, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return MinByAggregator<OMNI_FLOAT, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return MinByAggregator<OMNI_DOUBLE, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL64:
                return MinByAggregator<OMNI_DECIMAL64, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL128:
                return MinByAggregator<OMNI_DECIMAL128, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CONTAINER:
                return MinByAggregator<OMNI_CONTAINER, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return MinByAggregator<OMNI_VARCHAR, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return MinByAggregator<OMNI_CHAR, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return MinByAggregator<OMNI_VARBINARY, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_ARRAY:
            case OMNI_MAP:
            case OMNI_ROW:
                if constexpr (COL2_ID == OMNI_VARCHAR || COL2_ID == OMNI_CHAR || COL2_ID == OMNI_VARBINARY) {
                    return MinByComplexVarcharAggregator<COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes),
                        channels, inputRaw, outputPartial, isOverflowAsNull, col1Id);
                } else {
                    return MinByComplexAggregator<COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes),
                        channels, inputRaw, outputPartial, isOverflowAsNull, col1Id);
                }
            case OMNI_NONE:
                return MinByAggregator<OMNI_NONE, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            default:
                std::string omniExceptionInfo = "In MinByAggretator function FromKnownOutput, no such input col1 type " + std::to_string(col1Id);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
};

class MaxByAggregatorFactory : public AggregatorFactory {
public:
    MaxByAggregatorFactory() : AggregatorFactory() {}
    ~MaxByAggregatorFactory() override = default;
    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override
    {
        if (inputTypes.GetSize() != 2) {
            throw std::runtime_error("Input data types must have exactly two inputs");
        }
        return CreateAggregatorInternal(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
    }

protected:
    std::unique_ptr<Aggregator> CreateAggregatorInternal(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    {
        auto col2Id = inputTypes.GetType(1)->GetId();
        switch (col2Id) {
            case OMNI_BOOLEAN:
                return FromKnownOutput<OMNI_BOOLEAN>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return FromKnownOutput<OMNI_BYTE>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return FromKnownOutput<OMNI_SHORT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return FromKnownOutput<OMNI_INT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return FromKnownOutput<OMNI_LONG>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return FromKnownOutput<OMNI_FLOAT>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return FromKnownOutput<OMNI_DOUBLE>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL64:
                return FromKnownOutput<OMNI_DECIMAL64>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL128:
                return FromKnownOutput<OMNI_DECIMAL128>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return FromKnownOutput<OMNI_VARCHAR>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return FromKnownOutput<OMNI_CHAR>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return FromKnownOutput<OMNI_VARBINARY>(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            default:
                std::string omniExceptionInfo = "In MaxByAggretator function FromKnownOutput, no such input col2 type " + std::to_string(col2Id);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }

    template <DataTypeId COL2_ID>
    std::unique_ptr<Aggregator> FromKnownOutput(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    {
        auto col1Id = inputTypes.GetType(0)->GetId();
        switch (col1Id) {
            case OMNI_BOOLEAN:
                return MaxByAggregator<OMNI_BOOLEAN, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_BYTE:
                return MaxByAggregator<OMNI_BYTE, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_SHORT:
                return MaxByAggregator<OMNI_SHORT, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return MaxByAggregator<OMNI_INT, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return MaxByAggregator<OMNI_LONG, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_FLOAT:
                return MaxByAggregator<OMNI_FLOAT, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DOUBLE:
                return MaxByAggregator<OMNI_DOUBLE, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL64:
                return MaxByAggregator<OMNI_DECIMAL64, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_DECIMAL128:
                return MaxByAggregator<OMNI_DECIMAL128, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CONTAINER:
                return MaxByAggregator<OMNI_CONTAINER, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARCHAR:
                return MaxByAggregator<OMNI_VARCHAR, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_CHAR:
                return MaxByAggregator<OMNI_CHAR, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_VARBINARY:
                return MaxByAggregator<OMNI_VARBINARY, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            case OMNI_ARRAY:
            case OMNI_MAP:
            case OMNI_ROW:
                if constexpr (COL2_ID == OMNI_VARCHAR || COL2_ID == OMNI_CHAR || COL2_ID == OMNI_VARBINARY) {
                    return MaxByComplexVarcharAggregator<COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes),
                        channels, inputRaw, outputPartial, isOverflowAsNull, col1Id);
                } else {
                    return MaxByComplexAggregator<COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes),
                        channels, inputRaw, outputPartial, isOverflowAsNull, col1Id);
                }
            case OMNI_NONE:
                return MaxByAggregator<OMNI_NONE, COL2_ID>::Create(std::move(inputTypes), std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
            default:
                std::string omniExceptionInfo = "In MaxByAggretator function FromKnownOutput, no such input col1 type " + std::to_string(col1Id);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
};

class CountColumnAggregatorFactory : public TypedAggregatorFactory<CountColumnAggregator> {
public:
    CountColumnAggregatorFactory() : TypedAggregatorFactory<CountColumnAggregator>() {}
    ~CountColumnAggregatorFactory() override = default;
};

class CountAllAggregatorFactory : public TypedAggregatorFactory<CountAllAggregator> {
public:
    CountAllAggregatorFactory() : TypedAggregatorFactory<CountAllAggregator>() {}
    ~CountAllAggregatorFactory() override = default;
};

class BloomFilterAggregatorFactory : public TypedAggregatorFactory<BloomFilterAggregator> {
public:
    BloomFilterAggregatorFactory() : TypedAggregatorFactory<BloomFilterAggregator>() {}
    ~BloomFilterAggregatorFactory() override = default;
};

class ApproxCountDistinctAggregatorFactory : public AggregatorFactory {
  public:
    ApproxCountDistinctAggregatorFactory() = default;
    ~ApproxCountDistinctAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class ApproxPercentileAggregatorFactory : public AggregatorFactory {
public:
    ApproxPercentileAggregatorFactory() = default;
    ~ApproxPercentileAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw = true, bool outputPartial = false,
        bool isOverflowAsNull = true) override;
};

class KurtosisAggregatorFactory : public TypedAggregatorFactory<KurtosisAggregator> {
public:
    KurtosisAggregatorFactory() : TypedAggregatorFactory<KurtosisAggregator>() {}
    ~KurtosisAggregatorFactory() override = default;
};

class SkewnessAggregatorFactory : public TypedAggregatorFactory<SkewnessAggregator> {
public:
    SkewnessAggregatorFactory() : TypedAggregatorFactory<SkewnessAggregator>() {}
    ~SkewnessAggregatorFactory() override = default;
};

template <class T> class MaskAggregatorFactory : public AggregatorFactory {
public:
    explicit MaskAggregatorFactory(int32_t maskCol) : maskColumnId(maskCol), realFactory(std::make_unique<T>()) {}
    ~MaskAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override
    {
        std::unique_ptr<Aggregator> realAggregator = realFactory->CreateAggregator(std::move(inputTypes),
            std::move(outputTypes), channels, inputRaw, outputPartial, isOverflowAsNull);
        if (realAggregator == nullptr) {
            LogError("Error in mask aggregate: Real aggregator is null");
            return nullptr;
        }
        if (realAggregator->IsTypedAggregator()) {
            return TypedMaskColAggregator::Create(maskColumnId, std::move(realAggregator));
        } else {
            return std::make_unique<MaskColAggregator>(maskColumnId, std::move(realAggregator));
        }
    }

private:
    int maskColumnId;
    std::unique_ptr<AggregatorFactory> realFactory;
};

class DynamicUdafAggregatorFactory : public AggregatorFactory {
public:
    explicit DynamicUdafAggregatorFactory(std::string udafName) : udafName_(std::move(udafName)) {}
    ~DynamicUdafAggregatorFactory() override = default;

    std::unique_ptr<Aggregator> CreateAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull) override;

private:
    std::string udafName_;
};

// for window aggregation call
std::unique_ptr<AggregatorFactory> CreateAggregatorFactory(FunctionType aggType);
}
}
#endif // OMNI_RUNTIME_AGGREGATOR_FACTORY_H
