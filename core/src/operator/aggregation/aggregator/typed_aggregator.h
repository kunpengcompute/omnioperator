/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: Inner supported aggregators header
 */

#pragma once
#include "aggregator.h"
#include "operations_hash_aggregator.h"
#include "operations_aggregator.h"

#include <memory>
#include <cmath>
#include <iomanip> // for setpercision

#include "type/decimal_operations.h"
#include "operator/aggregation/vector_getter.h"

namespace omniruntime {
using namespace mem;
namespace op {
using DecimalPartialResult = struct DecimalPartialResult {
    Decimal128 sum = 0;
    int64_t count = 0;
};

template <type::DataTypeId dataTypeId> struct AggNativeAndVectorType {};

template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_BYTE> {
    using type = int8_t;
    using vector = Vector<type>;
};

template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_BOOLEAN> {
    using type = int8_t;
    using vector = Vector<type>;
};

template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_SHORT> {
    using type = int16_t;
    using vector = Vector<type>;
};

template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_INT> {
    using type = int32_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_DATE32> {
    using type = int32_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_TIME32> {
    using type = int32_t;
    using vector = Vector<type>;
};

template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_LONG> {
    using type = int64_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_DATE64> {
    using type = int64_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_TIME64> {
    using type = int64_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_TIMESTAMP> {
    using type = int64_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_FLOAT> {
    using type = float;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_DOUBLE> {
    using type = double;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_DECIMAL64> {
    using type = int64_t;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_DECIMAL128> {
    using type = Decimal128;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_VARCHAR> {
    using type = DecimalPartialResult;
    using vector = Vector<LargeStringContainer<std::string_view>>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_CHAR> {
    using type = uint8_t;
    using vector = Vector<LargeStringContainer<std::string_view>>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_VARBINARY> {
    using type = DecimalPartialResult;
    using vector = Vector<LargeStringContainer<std::string_view>>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_CONTAINER> {
    using type = double;
    using vector = Vector<type>;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_ARRAY> {
    using type =ArrayType;
    using vector = ArrayVector;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_MAP> {
    using type = MapType;
    using vector = MapVector;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_ROW> {
    using type = RowType;
    using vector = RowVector;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_NONE> {
    using type = void;
    using vector = void;
};
template <> struct AggNativeAndVectorType<type::DataTypeId::OMNI_INVALID> {
    using type = void;
    using vector = void;
};

class TypedMaskColAggregator;

class TypedAggregator : public Aggregator {
    friend class TypedMaskColAggregator;

public:
    ~TypedAggregator() override = default;

    // Sort aggregation processes one row at a time. Keep this path aligned with
    // the existing no-group and hash-aggregation implementations so every
    // typed aggregator can be used by both aggregation operators.
    void ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowIndex) override
    {
        curVectorBatch = vectorBatch;
        std::shared_ptr<NullsHelper> nullMap = nullptr;
        BaseVector *vector = GetVector(vectorBatch, rowIndex, 1, &nullMap);
        ProcessSingleInternal(state + aggStateOffset, vector, rowIndex, 1, nullMap);
    }

    // for no groupby aggregation
    void ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowOffset,
        const int32_t rowCount) override
    {
        curVectorBatch = vectorBatch;
        std::shared_ptr<NullsHelper> nullMap = nullptr;
        BaseVector *vector = GetVector(vectorBatch, rowOffset, rowCount, &nullMap);
        ProcessSingleInternal(state + aggStateOffset, vector, rowOffset, rowCount, nullMap);
    }

    // for no groupby aggregation with filter
    void ProcessGroupFilter(AggregateState *state, VectorBatch *vectorBatch, const int32_t rowOffset,
        const int32_t filterIndex) override
    {
        curVectorBatch = vectorBatch;
        std::shared_ptr<NullsHelper> nullMap = nullptr;

        int32_t rowCount = vectorBatch->GetRowCount();
        BaseVector *vector = GetVector(vectorBatch, rowOffset, rowCount, &nullMap);

        Vector<bool> *booleanVector = static_cast<Vector<bool> *>(vectorBatch->Get(filterIndex));
        bool needFilterJude = DoNeedHandleAggFilter(booleanVector, rowOffset, rowCount);
        if (needFilterJude) {
            auto *filterPtr = unsafe::UnsafeVector::GetRawValues(booleanVector);
            filterPtr += rowOffset;
            // notSatisfiedArray can filter row which no need to aggregate
            // the nullMap: true means null
            // booleanVector: false means one row has been filtered
            auto notSatisfied = std::make_shared<NullsHelper>(std::make_shared<NullsBuffer>(rowCount));
            if (nullMap == nullptr) {
                for (int32_t i = 0; i < rowCount; ++i) {
                    notSatisfied->SetNull(i, not filterPtr[i]);
                }
            } else {
                auto nullmapPtr = *nullMap;
                nullmapPtr += rowOffset;
                for (int32_t i = 0; i < rowCount; ++i) {
                    notSatisfied->SetNull(i, nullmapPtr[i] || not filterPtr[i]);
                }
            }
            ProcessSingleInternal(state + aggStateOffset, vector, rowOffset, rowCount, notSatisfied);
        } else {
            // true/false meaning in nullmap is same with notSatisfiedArray
            // true means one row no need to aggregate
            ProcessSingleInternal(state + aggStateOffset, vector, rowOffset, rowCount, nullMap);
        }
    }

    // for groupby hash aggregation
    void ProcessGroup(std::vector<AggregateState *> &rowStates, VectorBatch *vectorBatch,
        const int32_t rowOffset) override
    {
        curVectorBatch = vectorBatch;
        std::shared_ptr<NullsHelper> nullMap = nullptr;

        BaseVector *vector = GetVector(vectorBatch, rowOffset, rowStates.size(), &nullMap);
        ProcessGroupInternal(rowStates, vector, rowOffset, nullMap);
    }

    // for groupby hash aggregation with Filter
    void ProcessGroupFilter(std::vector<AggregateState *> &rowStates, const size_t aggIdx, VectorBatch *vectorBatch,
        const int32_t filterOffset, const int32_t rowOffset) override
    {
        curVectorBatch = vectorBatch;
        std::shared_ptr<NullsHelper> nullMap = nullptr;

        auto rowCount = static_cast<int32_t>(rowStates.size());
        BaseVector *vector = GetVector(vectorBatch, rowOffset, rowCount, &nullMap);

        Vector<bool> *booleanVector = static_cast<Vector<bool> *>(vectorBatch->Get(filterOffset));
        bool needFilterJude = DoNeedHandleAggFilter(booleanVector, rowOffset, rowCount);
        if (needFilterJude) {
            auto *filterPtr = unsafe::UnsafeVector::GetRawValues(booleanVector);
            filterPtr += rowOffset;
            // notSatisfiedArray can filter row which no need to aggregate
            // the nullMap: true means null
            // booleanVector: false means one row has been filtered
            auto notSatisfied = std::make_shared<NullsHelper>(std::make_shared<NullsBuffer>(rowCount));
            if (nullMap == nullptr) {
                for (int32_t i = 0; i < rowCount; ++i) {
                    notSatisfied->SetNull(i, not filterPtr[i]);
                }
            } else {
                auto nullmapPtr = *nullMap;
                nullmapPtr += rowOffset;
                for (int32_t i = 0; i < rowCount; ++i) {
                    notSatisfied->SetNull(i, nullmapPtr[i] || not filterPtr[i]);
                }
            }
            ProcessGroupInternal(rowStates, vector, rowOffset, notSatisfied);
        } else {
            // true/false meaning in nullmap is same with notSatisfiedArray
            // true means one row no need to aggregate
            ProcessGroupInternal(rowStates, vector, rowOffset, nullMap);
        }
    }

    // adaptive partial aggregation
    void AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch, const int32_t filterIndex) override
    {
        int32_t rowCount = inputVecBatch->GetRowCount();
        std::shared_ptr<NullsHelper> nullMap = nullptr;
        BaseVector *originVector = GetVector(inputVecBatch, 0, rowCount, &nullMap);

        Vector<bool> *booleanVector = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
        bool needFilterJude = DoNeedHandleAggFilter(booleanVector, 0, rowCount);
        if (needFilterJude) {
            auto *filterPtr = unsafe::UnsafeVector::GetRawValues(booleanVector);
            // notSatisfiedArray can filter row which no need to aggregate
            // the nullMap: true means null
            // booleanVector: false means one row has been filtered
            auto notSatisfied = std::make_shared<NullsHelper>(std::make_shared<NullsBuffer>(rowCount));
            if (nullMap == nullptr) {
                for (int32_t i = 0; i < rowCount; ++i) {
                    notSatisfied->SetNull(i, not filterPtr[i]);
                }
            } else {
                auto nullmapPtr = *nullMap;
                for (int32_t i = 0; i < rowCount; ++i) {
                    notSatisfied->SetNull(i, nullmapPtr[i] || not filterPtr[i]);
                }
            }
            ProcessAlignAggSchema(result, originVector, notSatisfied, true);
        } else {
            ProcessAlignAggSchema(result, originVector, nullMap, false);
        }
    }

    // adaptive partial aggregation
    void AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch) override
    {
        int32_t rowCount = inputVecBatch->GetRowCount();
        if (rowCount == 0) {
            // Empty input: avoid GetVector which would call inputVecBatch->Get(channel) when batch may have no vectors
            ProcessAlignAggSchema(result, nullptr, nullptr, false);
            return;
        }
        std::shared_ptr<NullsHelper> nullMap = nullptr;
        BaseVector *originVector = GetVector(inputVecBatch, 0, rowCount, &nullMap);
        ProcessAlignAggSchema(result, originVector, nullMap, false);
    }

    bool IsTypedAggregator() override
    {
        return true;
    }

protected:
    TypedAggregator(const FunctionType aggregateType, const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
        const bool isOverflowAsNull);

    /*
     * The state pointer has offset aggStateOffset bytes.
     */
    virtual void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) = 0;

    virtual void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
        const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) = 0;

    virtual void ProcessAlignAggSchema(VectorBatch *vecBatch, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) = 0;

    // set vector value null or throw exception when overflow
    void SetNullOrThrowException(BaseVector *vector, const int index, const char *errorMsg)
    {
        if (!IsOverflowAsNull()) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMsg);
        }
        vector->SetNull(index);
    }

    virtual BaseVector *GetVector(VectorBatch *vectorBatch, const int32_t rowOffset, const int32_t rowCount,
        std::shared_ptr<NullsHelper> *nullMap, const size_t channelIdx);

    virtual BaseVector *GetVector(VectorBatch *vectorBatch, const int32_t rowOffset, const int32_t rowCount,
        std::shared_ptr<NullsHelper> *nullMap);

    // this is needed in case, we directly create aggregator (using Aggregator::Create) without using AggregatorFactory
    static bool CheckTypes(const std::string &aggName, const DataTypes &inputTypes, const DataTypes &outputTypes,
        const DataTypeId inId, const DataTypeId outId);

    // template<DataTypeId IN_ID, DataTypeId OUT_ID, typename OutType = typename AggNativeAndVectorType<OUT_ID>::type,
    // typename InType = typename AggNativeAndVectorType<IN_ID>::type>
    template <typename InType, typename OutType> OutType CastWithOverflow(const InType &val, bool &overflow)
    {
        if (overflow) {
            return OutType{};
        }

        if constexpr (std::is_same_v<InType, Decimal128>) {
            return CastWithOverflowDecimalInput<OutType>(val, overflow);
        } else if constexpr (std::is_same_v<OutType, Decimal128>) {
            return CastWithOverflowDecimalOutput<InType>(val, overflow);
        } else {
            return CastWithOverflowNonDecimal<InType, OutType>(val, overflow);
        }
    }

    Allocator *allocator = Allocator::GetAllocator();

    static inline bool CheckType(const DataTypeId actual, const DataTypeId expected)
    {
        switch (actual) {
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_INT:
                return expected == OMNI_INT;
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIME64:
            case OMNI_TIMESTAMP:
                return expected == OMNI_LONG;
            default:
                return expected == actual;
        }
    }

protected:
    VectorBatch *curVectorBatch = nullptr;

private:
    template <typename OutType> OutType CastWithOverflowDecimalInput(const Decimal128 &val, bool &overflow)
    {
        if constexpr (std::is_same_v<OutType, Decimal128>) {
            return this->CastWithOverflowDecimalToDecimal(val, overflow);
        } else if constexpr (std::is_same_v<OutType, int64_t>) {
            return this->CastWithOverflowDecimalToLong(val, overflow);
        } else if constexpr (std::is_floating_point_v<OutType>) {
            return this->CastWithOverflowDecimalToFloatingPoint<OutType>(val, overflow);
        } else {
            auto &inputType = inputTypes.GetType(0);
            OutType result{};
            Decimal128Wrapper val128(val);

            // so we shoud add decimal part separately if input is acutally decimal not varchar
            if (inputType->GetId() == OMNI_DECIMAL128) {
                int32_t scale = -static_cast<DecimalDataType *>(inputType.get())->GetScale();
                val128.ReScale(scale).SetScale(0);
                if (val128.IsOverflow() != OpStatus::SUCCESS) {
                    overflow = true;
                    return result;
                }
            }

            int64_t result64;
            try {
                result64 = static_cast<int64_t>(val128);
            } catch (std::overflow_error &e) {
                overflow = true;
                return result;
            }

            result = static_cast<OutType>(result64);
            overflow = (static_cast<int64_t>(result) != result64);
            return result;
        }
    }

    template <typename InType> Decimal128 CastWithOverflowDecimalOutput(const InType &val, bool &overflow)
    {
        if constexpr (std::is_same_v<InType, Decimal128>) {
            throw OmniException("Invalid Arguement", "Unexpected decimal input for 'CastWithOverflowDecimalOutput'");
        } else if constexpr (std::is_same_v<InType, int64_t>) {
            return this->CastWithOverflowLongToDecimal(val, overflow);
        } else if constexpr (std::is_floating_point_v<InType>) {
            return this->CastWithOverflowFloatingPointDecimal<InType>(val, overflow);
        } else {
            Decimal128Wrapper result(static_cast<int64_t>(val));
            auto &outputType = outputTypes.GetType(0);
            if (outputType->GetId() == OMNI_DECIMAL128) {
                int32_t scale = static_cast<DecimalDataType *>(outputType.get())->GetScale();
                result.ReScale(scale).SetScale(0);
                overflow = (result.IsOverflow() != OpStatus::SUCCESS);
            }

            return result.ToDecimal128();
        }
    }

    Decimal128 CastWithOverflowDecimalToDecimal(const Decimal128 &val, bool &overflow)
    {
        auto &inputType = inputTypes.GetType(0);
        auto &outputType = outputTypes.GetType(0);
        Decimal128Wrapper result(val);
        // following if condition makes sure that both input and output are acutally decimal,
        // not varchare (which has no scale)
        if ((inputType->GetId() == OMNI_DECIMAL128 || inputType->GetId() == OMNI_DECIMAL64) &&
            (outputType->GetId() == OMNI_DECIMAL64 || outputType->GetId() == OMNI_DECIMAL128)) {
            int32_t scale = static_cast<DecimalDataType *>(outputType.get())->GetScale() -
                static_cast<DecimalDataType *>(inputType.get())->GetScale();
            if (scale != 0) {
                result.ReScale(scale).SetScale(0);
                overflow = (result.IsOverflow() != OpStatus::SUCCESS);
            }
        }

        return result.ToDecimal128();
    }

    int64_t CastWithOverflowDecimalToLong(const Decimal128 &val, bool &overflow)
    {
        auto &inputType = inputTypes.GetType(0);
        auto &outputType = outputTypes.GetType(0);
        int64_t result{};
        Decimal128Wrapper resultDec(val);
        int32_t scale = 0;
        if (outputType->GetId() == OMNI_DECIMAL64) {
            // following if condition makes sure that input is acutally decimal,
            // not varchare (which has no scale)
            if (inputType->GetId() == OMNI_DECIMAL128 || inputType->GetId() == OMNI_DECIMAL64) {
                scale = static_cast<DecimalDataType *>(outputType.get())->GetScale() -
                    static_cast<DecimalDataType *>(inputType.get())->GetScale();
            }
        } else {
            // regular long output. so we shoud remove decimal part if input is acutally decimal not varchar
            if (inputType->GetId() == OMNI_DECIMAL128 || inputType->GetId() == OMNI_DECIMAL64) {
                scale = -static_cast<DecimalDataType *>(inputType.get())->GetScale();
            }
        }

        if (scale != 0) {
            resultDec.ReScale(scale).SetScale(0);
            if (resultDec.IsOverflow() != OpStatus::SUCCESS) {
                overflow = true;
                return result;
            }
        } else {
            resultDec = val;
        }
        try {
            result = static_cast<int64_t>(resultDec.SetScale(0));
        } catch (std::overflow_error &e) {
            overflow = true;
        }
        return result;
    }

    template <typename OutType> OutType CastWithOverflowDecimalToFloatingPoint(const Decimal128 &val, bool &overflow)
    {
        // so we shoud add decimal part separately if input is acutally decimal not varchar
        auto &inputType = inputTypes.GetType(0);
        int32_t scale =
            (inputType->GetId() == OMNI_DECIMAL128) ? static_cast<DecimalDataType *>(inputType.get())->GetScale() : 0;
        // higher performance if we convert directly rather than going through string
        std::string doubleString = ToStringWithScale(val.ToString(), scale);
        return static_cast<OutType>(stod(doubleString));
    }

    Decimal128 CastWithOverflowLongToDecimal(const int64_t &val, bool &overflow)
    {
        auto &inputType = inputTypes.GetType(0);
        auto &outputType = outputTypes.GetType(0);
        Decimal128Wrapper result(val);
        int32_t scale = 0;
        if (inputType->GetId() == OMNI_DECIMAL64) {
            // following if condition makes sure that output is acutally decimal,
            // not varchare (which has no scale)
            if (outputType->GetId() == OMNI_DECIMAL128 || outputType->GetId() == OMNI_DECIMAL64) {
                scale = static_cast<DecimalDataType *>(outputType.get())->GetScale() -
                    static_cast<DecimalDataType *>(inputType.get())->GetScale();
            }
        } else {
            // regular long output. so we shoud remove decimal part if input is acutally decimal not varchar
            if (outputType->GetId() == OMNI_DECIMAL128 || outputType->GetId() == OMNI_DECIMAL64) {
                scale = static_cast<DecimalDataType *>(outputType.get())->GetScale();
            }
        }

        if (scale != 0) {
            result.ReScale(scale).SetScale(0);
            if (result.IsOverflow() != OpStatus::SUCCESS) {
                overflow = true;
            }
        }

        return result.ToDecimal128();
    }

    template <typename InType> Decimal128 CastWithOverflowFloatingPointDecimal(const InType &val, bool &overflow)
    {
        int32_t scale;
        auto &outputType = outputTypes.GetType(0);
        if (outputType->GetId() == OMNI_DECIMAL128 &&
            (scale = static_cast<DecimalDataType *>(outputType.get())->GetScale()) > 0) {
            InType integral;
            InType fractional = std::modf(val, &integral);

            std::stringstream ss;
            ss << std::fixed << std::setprecision(0) << integral;
            std::string s;
            ss >> s;

            int128_t res128;
            int32_t scaleNotUsed = 0;
            int32_t precisionNotUsed = 0;
            overflow = DecimalFromString(s, res128, scaleNotUsed, precisionNotUsed) != OpStatus::SUCCESS;
            Decimal128Wrapper result(res128);
            result.ReScale(scale).SetScale(0);
            overflow |= result.IsOverflow() != OpStatus::SUCCESS;

            fractional *= static_cast<InType>(pow(10, scale));
            integral = round(fractional);
            result = result.Add(Decimal128Wrapper(static_cast<int64_t>(integral)));
            overflow = result.IsOverflow() != OpStatus::SUCCESS;
            return result.ToDecimal128();
        } else {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(0) << round(val);
            std::string s;
            ss >> s;

            int128_t res128;
            int32_t resScale = 0;
            int32_t resPrecision = 0;
            overflow = DecimalFromString(s, res128, resScale, resPrecision) != OpStatus::SUCCESS;
            return Decimal128Wrapper(res128).ToDecimal128();
        }
    }

    template <typename InType, typename OutType> OutType CastWithOverflowNonDecimal(const InType &val, bool &overflow)
    {
        OutType res = static_cast<OutType>(val);
        auto &inputType = inputTypes.GetType(0);
        auto &outputType = outputTypes.GetType(0);
        int32_t scale;

        if constexpr (std::is_floating_point_v<OutType>) {
            if (inputType->GetId() == OMNI_DECIMAL64 &&
                (scale = static_cast<DecimalDataType *>(inputType.get())->GetScale()) > 0) {
                res /= pow(10.0, scale);
            }
        } else if constexpr (std::is_floating_point_v<InType>) {
            if (outputType->GetId() == OMNI_DECIMAL64 &&
                (scale = static_cast<DecimalDataType *>(outputType.get())->GetScale()) > 0) {
                InType integral;
                InType fractional = std::modf(val, &integral);

                res = static_cast<OutType>(integral);
                overflow = static_cast<InType>(res) != integral;
                overflow |= __builtin_mul_overflow(res, static_cast<OutType>(pow(10, scale)), &res);

                fractional *= static_cast<InType>(pow(10, scale));
                integral = round(fractional);
                overflow |= __builtin_add_overflow(res, static_cast<OutType>(integral), &res);
            } else {
                InType integral = round(val);
                res = static_cast<OutType>(integral);
                overflow = static_cast<InType>(res) != integral;
            }
        } else {
            // at this point input and output are either integral or deciaml64
            scale = outputType->GetId() == OMNI_DECIMAL64 ?
                static_cast<DecimalDataType *>(outputType.get())->GetScale() :
                0;
            scale -=
                inputType->GetId() == OMNI_DECIMAL64 ? static_cast<DecimalDataType *>(inputType.get())->GetScale() : 0;
            if (scale == 0) {
                overflow = static_cast<InType>(res) != val;
            } else {
                const OutType scaleFactor = static_cast<OutType>(pow(10, abs(scale)));
                if (scale > 0) {
                    overflow = (static_cast<InType>(res) != val) || __builtin_mul_overflow(res, scaleFactor, &res);
                } else {
                    const InType scaledDown = val / scaleFactor;
                    res = static_cast<OutType>(scaledDown);
                    overflow = static_cast<InType>(res) != scaledDown;
                }
            }
        }
        return res;
    }
};
} // end of namespace op
} // end of namespace omniruntime
