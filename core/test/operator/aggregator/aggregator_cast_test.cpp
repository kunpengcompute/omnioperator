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

#include "operator/aggregation/aggregator/typed_aggregator.h"

#include "test/util/test_util.h"

#include "aggregator_multi_stage_no_groupby.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;

// helper classes and functions

// test runs for 'NUM_VALUES' different values, so 'inXXX' which is list of values for each type should at least
// have 'NUM_VALUES' entries.
static const int32_t NUM_VALUES = 15;

static std::vector<int32_t> CHANNEL{0};

class AggregatorCastTestClass : public TypedAggregator {
public:
    AggregatorCastTestClass(DataTypes &inTypes, DataTypes &outTypes, bool rawIn, bool partialOut, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_SUM, std::move(inTypes), std::move(outTypes), CHANNEL, rawIn,
                          partialOut, isOverflowAsNull)
    {
    }

    size_t GetStateSize() override { return 0; }

    void InitState(AggregateState *state) override { return; }

    void InitStates(std::vector<AggregateState *> &states) override
    {
        for (auto state : states) {
            InitState(state);
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
                               const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override
    {
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
                              const std::shared_ptr<NullsHelper> nullMap) override
    {
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, const int32_t rowIndex) override
    {
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
                            int32_t rowOffset, int32_t rowCount) override
    {
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
                               const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
    }

    template <typename InType, typename OutType> OutType TestCastWithOverflow(const InType val, bool &overflow)
    {
        return TypedAggregator::CastWithOverflow<InType, OutType>(val, overflow);
    }

    void SetInputType(DataTypes &inType) { inputTypes = inType; }

    void SetOutputType(DataTypes &outType) { outputTypes = outType; }
};

template <typename T> struct R {
    R() : v(T{}), overflow(true) {}

    explicit R(T v_) : v(v_), overflow(false) {}

    const T v;
    const bool overflow;
};

struct Results {
    Results() = delete;
    Results(const std::vector<R<int16_t>> &out16_, const std::vector<R<int32_t>> &out32_,
            const std::vector<R<int64_t>> &out64_, const std::vector<R<double>> &outDouble_,
            const std::vector<R<int64_t>> &outDecimal64_, const std::vector<R<Decimal128>> &outDecimal128_,
            const std::vector<R<Decimal128>> &outVarchar_)
        : out16(out16_), out32(out32_), out64(out64_), outDouble(outDouble_), outDecimal64(outDecimal64_),
          outDecimal128(outDecimal128_), outVarchar(outVarchar_)
    {
    }
    const std::vector<R<int16_t>> out16;
    const std::vector<R<int32_t>> out32;
    const std::vector<R<int64_t>> out64;
    const std::vector<R<double>> outDouble;
    const std::vector<R<int64_t>> outDecimal64;
    const std::vector<R<Decimal128>> outDecimal128;
    const std::vector<R<Decimal128>> outVarchar;
};

static DataTypesPtr CreateType(DataTypeId dataTypeId)
{
    std::vector<DataTypePtr> dataTypeVector(1);
    switch (dataTypeId) {
        case OMNI_SHORT: dataTypeVector[0] = ShortType(); break;
        case OMNI_INT: dataTypeVector[0] = IntType(); break;
        case OMNI_LONG: dataTypeVector[0] = LongType(); break;
        case OMNI_DOUBLE: dataTypeVector[0] = DoubleType(); break;
        case OMNI_DECIMAL64: dataTypeVector[0] = Decimal64Type(); break;
        case OMNI_DECIMAL128: dataTypeVector[0] = Decimal128Type(); break;
        case OMNI_VARCHAR: dataTypeVector[0] = VarcharType(sizeof(DecimalPartialResult)); break;
        default: throw OmniException("Invalid Argument", "Invalid type " + TypeUtil::TypeToStringLog(dataTypeId));
    }

    return DataTypes(dataTypeVector).Instance();
}

static void SetDecimalOutputWithScale(AggregatorCastTestClass &agg, const int32_t scale)
{
    std::vector<DataTypePtr> dataTypeVector(1);
    switch (agg.GetOutputTypes().GetType(0)->GetId()) {
        case OMNI_DECIMAL64: dataTypeVector[0] = Decimal64Type(DECIMAL64_DEFAULT_PRECISION, scale); break;
        case OMNI_DECIMAL128: dataTypeVector[0] = Decimal128Type(DECIMAL128_DEFAULT_PRECISION, scale); break;
        default: throw OmniException("Invalid argument", "Expecting decimal output type");
    }

    agg.SetOutputType(*DataTypes(dataTypeVector).Instance());
}

static void SetDecimalInputWithScale(AggregatorCastTestClass &agg, const int32_t scale)
{
    std::vector<DataTypePtr> dataTypeVector(1);
    switch (agg.GetInputTypes().GetType(0)->GetId()) {
        case OMNI_DECIMAL64: dataTypeVector[0] = Decimal64Type(DECIMAL64_DEFAULT_PRECISION, scale); break;
        case OMNI_DECIMAL128: dataTypeVector[0] = Decimal128Type(DECIMAL128_DEFAULT_PRECISION, scale); break;
        default: throw OmniException("Invalid argument", "Expecting decimal input type");
    }

    agg.SetInputType(*DataTypes(dataTypeVector).Instance());
}

template <typename InType, typename OutType>
static std::vector<R<OutType>> ConvertValues(const std::vector<R<InType>> &in, const int32_t scale = 0)
{
    std::vector<R<OutType>> result;
    result.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        OutType v;
        if constexpr (std::is_same_v<OutType, Decimal128>) {
            Decimal128Wrapper wrapped(static_cast<int64_t>(in[i].v));
            if (scale > 0) {
                wrapped.ReScale(scale).SetScale(0);
            }
            v = wrapped.ToDecimal128();
        } else {
            v = static_cast<OutType>(in[i].v);
            if (scale > 0) {
                v *= static_cast<OutType>(pow(10, scale));
            }
        }
        result.push_back(R<OutType>(v));
    }
    return result;
}

template <typename T>
static std::vector<R<T>> ConcatinateValues(const std::vector<R<T>> &in1, const std::vector<R<T>> &in2)
{
    std::vector<R<T>> result;
    result.reserve(in1.size() + in2.size());
    for (auto v : in1) {
        result.push_back(v);
    }
    for (auto v : in2) {
        result.push_back(v);
    }
    return result;
}

template <typename T> static std::vector<R<T>> RepeatVector(const std::vector<R<T>> &in, const int32_t reps)
{
    std::vector<R<T>> result;
    result.reserve(in.size() * reps);
    for (auto i = 0; i < reps; ++i) {
        for (auto v : in) {
            result.push_back(v);
        }
    }
    return result;
}

// test data
static std::vector<R<int16_t>> in16{R<int16_t>(0),      R<int16_t>(123),   R<int16_t>(-123),   R<int16_t>(12345),
                                    R<int16_t>(-12345), R<int16_t>(32767), R<int16_t>(-32767), R<int16_t>(32766),
                                    R<int16_t>(-32768), R<int16_t>(1363),  R<int16_t>(-1363),  R<int16_t>(1984),
                                    R<int16_t>(-1984),  R<int16_t>(13631), R<int16_t>(-13631)};
static Results in16Results(in16, ConvertValues<int16_t, int32_t>(in16), ConvertValues<int16_t, int64_t>(in16),
                           ConvertValues<int16_t, double>(in16),
                           ConcatinateValues<int64_t>(ConvertValues<int16_t, int64_t>(in16),
                                                      ConvertValues<int16_t, int64_t>(in16, 2)),
                           ConcatinateValues<Decimal128>(ConvertValues<int16_t, Decimal128>(in16),
                                                         ConvertValues<int16_t, Decimal128>(in16, 2)),
                           ConvertValues<int16_t, Decimal128>(in16));

static std::vector<R<int32_t>> in32{R<int32_t>(0),           R<int32_t>(123),        R<int32_t>(-123),
                                    R<int32_t>(12345),       R<int32_t>(-12345),     R<int32_t>(32767),
                                    R<int32_t>(-32767),      R<int32_t>(32768),      R<int32_t>(-32768),
                                    R<int32_t>(13630126),    R<int32_t>(-13630126),  R<int32_t>(2147483647),
                                    R<int32_t>(-2147483647), R<int32_t>(2147483646), R<int32_t>(-2147483648)};
static Results in32Results(std::vector<R<int16_t>>{R<int16_t>(0), R<int16_t>(123), R<int16_t>(-123), R<int16_t>(12345),
                                                   R<int16_t>(-12345), R<int16_t>(32767), R<int16_t>(-32767),
                                                   R<int16_t>(), R<int16_t>(-32768), R<int16_t>(), R<int16_t>(),
                                                   R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>()},
                           in32, ConvertValues<int32_t, int64_t>(in32), ConvertValues<int32_t, double>(in32),
                           ConcatinateValues<int64_t>(ConvertValues<int32_t, int64_t>(in32),
                                                      ConvertValues<int32_t, int64_t>(in32, 2)),
                           ConcatinateValues<Decimal128>(ConvertValues<int32_t, Decimal128>(in32),
                                                         ConvertValues<int32_t, Decimal128>(in32, 2)),
                           ConvertValues<int32_t, Decimal128>(in32));

static std::vector<R<int64_t>> in64{R<int64_t>(0),
                                    R<int64_t>(123),
                                    R<int64_t>(-123),
                                    R<int64_t>(12345),
                                    R<int64_t>(-12345),
                                    R<int64_t>(1234567890123LL),
                                    R<int64_t>(-1234567890123LL),
                                    R<int64_t>(123456789012345678LL),
                                    R<int64_t>(-123456789012345678LL),
                                    R<int64_t>(1363),
                                    R<int64_t>(-1363),
                                    R<int64_t>(1984),
                                    R<int64_t>(-1984),
                                    R<int64_t>(13630126LL),
                                    R<int64_t>(-13630126LL)};
static Results in64Results(
    std::vector<R<int16_t>>{R<int16_t>(0), R<int16_t>(123), R<int16_t>(-123), R<int16_t>(12345), R<int16_t>(-12345),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(1363), R<int16_t>(-1363),
                            R<int16_t>(1984), R<int16_t>(-1984), R<int16_t>(), R<int16_t>()},
    std::vector<R<int32_t>>{R<int32_t>(0), R<int32_t>(123), R<int32_t>(-123), R<int32_t>(12345), R<int32_t>(-12345),
                            R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(1363), R<int32_t>(-1363),
                            R<int32_t>(1984), R<int32_t>(-1984), R<int32_t>(13630126), R<int32_t>(-13630126)},
    in64,
    std::vector<R<double>>{R<double>(0.0), R<double>(123.0), R<double>(-123.0), R<double>(12345.0), R<double>(-12345.0),
                           R<double>(1234567890123.0), R<double>(-1234567890123.0), R<double>(123456789012345678.0),
                           R<double>(-123456789012345678.0), R<double>(1363.0), R<double>(-1363.0), R<double>(1984.0),
                           R<double>(-1984.0), R<double>(13630126.0), R<double>(-13630126.0)},
    std::vector<R<int64_t>>{// scale = 0
                            R<int64_t>(0), R<int64_t>(123), R<int64_t>(-123), R<int64_t>(12345), R<int64_t>(-12345),
                            R<int64_t>(1234567890123LL), R<int64_t>(-1234567890123LL), R<int64_t>(123456789012345678LL),
                            R<int64_t>(-123456789012345678LL), R<int64_t>(1363), R<int64_t>(-1363), R<int64_t>(1984),
                            R<int64_t>(-1984), R<int64_t>(13630126), R<int64_t>(-13630126),
                            // scale = 2
                            R<int64_t>(0), R<int64_t>(12300), R<int64_t>(-12300), R<int64_t>(1234500),
                            R<int64_t>(-1234500), R<int64_t>(123456789012300LL), R<int64_t>(-123456789012300LL),
                            R<int64_t>(), R<int64_t>(), R<int64_t>(136300), R<int64_t>(-136300), R<int64_t>(198400),
                            R<int64_t>(-198400), R<int64_t>(1363012600), R<int64_t>(-1363012600)},
    std::vector<R<Decimal128>>{
        // scale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("123")), R<Decimal128>(Decimal128("-123")),
        R<Decimal128>(Decimal128("12345")), R<Decimal128>(Decimal128("-12345")),
        R<Decimal128>(Decimal128("1234567890123")), R<Decimal128>(Decimal128("-1234567890123")),
        R<Decimal128>(Decimal128("123456789012345678")), R<Decimal128>(Decimal128("-123456789012345678")),
        R<Decimal128>(Decimal128("1363")), R<Decimal128>(Decimal128("-1363")), R<Decimal128>(Decimal128("1984")),
        R<Decimal128>(Decimal128("-1984")), R<Decimal128>(Decimal128("13630126")),
        R<Decimal128>(Decimal128("-13630126")),
        // scale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12300")), R<Decimal128>(Decimal128("-12300")),
        R<Decimal128>(Decimal128("1234500")), R<Decimal128>(Decimal128("-1234500")),
        R<Decimal128>(Decimal128("123456789012300")), R<Decimal128>(Decimal128("-123456789012300")),
        R<Decimal128>(Decimal128("12345678901234567800")), R<Decimal128>(Decimal128("-12345678901234567800")),
        R<Decimal128>(Decimal128("136300")), R<Decimal128>(Decimal128("-136300")), R<Decimal128>(Decimal128("198400")),
        R<Decimal128>(Decimal128("-198400")), R<Decimal128>(Decimal128("1363012600")),
        R<Decimal128>(Decimal128("-1363012600"))},
    std::vector<R<Decimal128>>{
        R<Decimal128>(Decimal128("0")),
        R<Decimal128>(Decimal128("123")),
        R<Decimal128>(Decimal128("-123")),
        R<Decimal128>(Decimal128("12345")),
        R<Decimal128>(Decimal128("-12345")),
        R<Decimal128>(Decimal128("1234567890123")),
        R<Decimal128>(Decimal128("-1234567890123")),
        R<Decimal128>(Decimal128("123456789012345678")),
        R<Decimal128>(Decimal128("-123456789012345678")),
        R<Decimal128>(Decimal128("1363")),
        R<Decimal128>(Decimal128("-1363")),
        R<Decimal128>(Decimal128("1984")),
        R<Decimal128>(Decimal128("-1984")),
        R<Decimal128>(Decimal128("13630126")),
        R<Decimal128>(Decimal128("-13630126")),
    });

static std::vector<R<double>> inDouble{R<double>(0.0),
                                       R<double>(12.3),
                                       R<double>(-12.3),
                                       R<double>(12.345),
                                       R<double>(-12.345),
                                       R<double>(123456.789),
                                       R<double>(-123456.789),
                                       R<double>(123456789012345.678),
                                       R<double>(-123456789012345.678),
                                       R<double>(1234567890123456.789),
                                       R<double>(-1234567890123456.789),
                                       R<double>(12345678901234567.891),
                                       R<double>(-12345678901234567.891),
                                       R<double>(12345678901234567890.123),
                                       R<double>(-12345678901234567890.123)};
static Results inDoubleResults(
    std::vector<R<int16_t>>{R<int16_t>(0), R<int16_t>(12), R<int16_t>(-12), R<int16_t>(12), R<int16_t>(-12),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>()},
    std::vector<R<int32_t>>{R<int32_t>(0), R<int32_t>(12), R<int32_t>(-12), R<int32_t>(12), R<int32_t>(-12),
                            R<int32_t>(123457), R<int32_t>(-123457), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>()},
    std::vector<R<int64_t>>{R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(12), R<int64_t>(-12),
                            R<int64_t>(123457), R<int64_t>(-123457), R<int64_t>(123456789012346),
                            R<int64_t>(-123456789012346), R<int64_t>(1234567890123457), R<int64_t>(-1234567890123457),
                            R<int64_t>(12345678901234568), R<int64_t>(-12345678901234568), R<int64_t>(), R<int64_t>()},
    inDouble,
    std::vector<R<int64_t>>{// scale = 0
                            R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(12), R<int64_t>(-12),
                            R<int64_t>(123457), R<int64_t>(-123457), R<int64_t>(123456789012346),
                            R<int64_t>(-123456789012346), R<int64_t>(1234567890123457), R<int64_t>(-1234567890123457),
                            R<int64_t>(12345678901234568), R<int64_t>(-12345678901234568), R<int64_t>(), R<int64_t>(),
                            // scale = 2
                            R<int64_t>(0), R<int64_t>(1230), R<int64_t>(-1230), R<int64_t>(1235), R<int64_t>(-1235),
                            R<int64_t>(12345679), R<int64_t>(-12345679),
                            // 123456789012345.678 changes to 123456789012345.671875
                            R<int64_t>(12345678901234567), R<int64_t>(-12345678901234567),
                            // 1234567890123456.789 changes to 1234567890123456.750000
                            R<int64_t>(123456789012345675), R<int64_t>(-123456789012345675),
                            // 12345678901234567.891 changes to 12345678901234568.000000
                            R<int64_t>(1234567890123456800), R<int64_t>(-1234567890123456800), R<int64_t>(),
                            R<int64_t>()},
    std::vector<R<Decimal128>>{
        // scale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")),
        R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")), R<Decimal128>(Decimal128("123457")),
        R<Decimal128>(Decimal128("-123457")), R<Decimal128>(Decimal128("123456789012346")),
        R<Decimal128>(Decimal128("-123456789012346")), R<Decimal128>(Decimal128("1234567890123457")),
        R<Decimal128>(Decimal128("-1234567890123457")), R<Decimal128>(Decimal128("12345678901234568")),
        R<Decimal128>(Decimal128("-12345678901234568")),
        // 12345678901234567890.123 changes to 12345678901234567168.000000
        R<Decimal128>(Decimal128("12345678901234567168")), R<Decimal128>(Decimal128("-12345678901234567168")),
        // scale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("1230")), R<Decimal128>(Decimal128("-1230")),
        R<Decimal128>(Decimal128("1235")), R<Decimal128>(Decimal128("-1235")), R<Decimal128>(Decimal128("12345679")),
        R<Decimal128>(Decimal128("-12345679")),
        // 123456789012345.678 changes to 123456789012345.671875
        R<Decimal128>(Decimal128("12345678901234567")), R<Decimal128>(Decimal128("-12345678901234567")),
        // 1234567890123456.789 changes to 1234567890123456.750000
        R<Decimal128>(Decimal128("123456789012345675")), R<Decimal128>(Decimal128("-123456789012345675")),
        // 12345678901234567.891 changes to 12345678901234568.000000
        R<Decimal128>(Decimal128("1234567890123456800")), R<Decimal128>(Decimal128("-1234567890123456800")),
        // 12345678901234567890.123 changes to 12345678901234567168.000000
        R<Decimal128>(Decimal128("1234567890123456716800")), R<Decimal128>(Decimal128("-1234567890123456716800"))},
    std::vector<R<Decimal128>>{
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")),
        R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")), R<Decimal128>(Decimal128("123457")),
        R<Decimal128>(Decimal128("-123457")), R<Decimal128>(Decimal128("123456789012346")),
        R<Decimal128>(Decimal128("-123456789012346")), R<Decimal128>(Decimal128("1234567890123457")),
        R<Decimal128>(Decimal128("-1234567890123457")), R<Decimal128>(Decimal128("12345678901234568")),
        R<Decimal128>(Decimal128("-12345678901234568")), R<Decimal128>(Decimal128("12345678901234567168")),
        R<Decimal128>(Decimal128("-12345678901234567168"))});

static std::vector<R<int64_t>> inDec64{R<int64_t>(0),
                                       R<int64_t>(123),
                                       R<int64_t>(-123),
                                       R<int64_t>(12345),
                                       R<int64_t>(-12345),
                                       R<int64_t>(123456789012345678LL),
                                       R<int64_t>(-123456789012345678LL),
                                       R<int64_t>(32768),
                                       R<int64_t>(-32768),
                                       R<int64_t>(123456789012LL),
                                       R<int64_t>(-123456789012LL),
                                       R<int64_t>(2147483647LL),
                                       R<int64_t>(-2147483647LL),
                                       R<int64_t>(2147483648LL),
                                       R<int64_t>(-2147483648LL)};
static Results inDec64Results(
    std::vector<R<int16_t>>{// scale = 0
                            R<int16_t>(0), R<int16_t>(123), R<int16_t>(-123), R<int16_t>(12345), R<int16_t>(-12345),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(-32768), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            // scale = 1
                            R<int16_t>(0), R<int16_t>(12), R<int16_t>(-12), R<int16_t>(1234), R<int16_t>(-1234),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(3276), R<int16_t>(-3276), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            // scale = 3
                            R<int16_t>(0), R<int16_t>(0), R<int16_t>(0), R<int16_t>(12), R<int16_t>(-12), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(32), R<int16_t>(-32), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>()},
    std::vector<R<int32_t>>{
        // scale = 0
        R<int32_t>(0), R<int32_t>(123), R<int32_t>(-123), R<int32_t>(12345), R<int32_t>(-12345), R<int32_t>(),
        R<int32_t>(), R<int32_t>(32768), R<int32_t>(-32768), R<int32_t>(), R<int32_t>(), R<int32_t>(2147483647),
        R<int32_t>(-2147483647), R<int32_t>(), R<int32_t>(-2147483648),
        // scale = 1
        R<int32_t>(0), R<int32_t>(12), R<int32_t>(-12), R<int32_t>(1234), R<int32_t>(-1234), R<int32_t>(), R<int32_t>(),
        R<int32_t>(3276), R<int32_t>(-3276), R<int32_t>(), R<int32_t>(), R<int32_t>(214748364), R<int32_t>(-214748364),
        R<int32_t>(214748364), R<int32_t>(-214748364),
        // scale = 3
        R<int32_t>(0), R<int32_t>(0), R<int32_t>(0), R<int32_t>(12), R<int32_t>(-12), R<int32_t>(), R<int32_t>(),
        R<int32_t>(32), R<int32_t>(-32), R<int32_t>(123456789), R<int32_t>(-123456789), R<int32_t>(2147483),
        R<int32_t>(-2147483), R<int32_t>(2147483), R<int32_t>(-2147483)},
    std::vector<R<int64_t>>{
        // scale = 0
        R<int64_t>(0), R<int64_t>(123), R<int64_t>(-123), R<int64_t>(12345), R<int64_t>(-12345),
        R<int64_t>(123456789012345678LL), R<int64_t>(-123456789012345678LL), R<int64_t>(32768), R<int64_t>(-32768),
        R<int64_t>(123456789012LL), R<int64_t>(-123456789012LL), R<int64_t>(2147483647LL), R<int64_t>(-2147483647LL),
        R<int64_t>(2147483648LL), R<int64_t>(-2147483648LL),
        // scale = 1
        R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(1234), R<int64_t>(-1234),
        R<int64_t>(12345678901234567LL), R<int64_t>(-12345678901234567LL), R<int64_t>(3276), R<int64_t>(-3276),
        R<int64_t>(12345678901LL), R<int64_t>(-12345678901LL), R<int64_t>(214748364LL), R<int64_t>(-214748364LL),
        R<int64_t>(214748364LL), R<int64_t>(-214748364LL),
        // scale = 3
        R<int64_t>(0), R<int64_t>(0), R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(123456789012345LL),
        R<int64_t>(-123456789012345LL), R<int64_t>(32), R<int64_t>(-32), R<int64_t>(123456789LL),
        R<int64_t>(-123456789LL), R<int64_t>(2147483LL), R<int64_t>(-2147483LL), R<int64_t>(2147483LL),
        R<int64_t>(-2147483LL)},
    std::vector<R<double>>{
        // scale = 0
        R<double>(0.0),
        R<double>(123.0),
        R<double>(-123.0),
        R<double>(12345.0),
        R<double>(-12345.0),
        R<double>(123456789012345678.0),
        R<double>(-123456789012345678.0),
        R<double>(32768.0),
        R<double>(-32768.0),
        R<double>(123456789012.0),
        R<double>(-123456789012.0),
        R<double>(2147483647.0),
        R<double>(-2147483647.0),
        R<double>(2147483648.0),
        R<double>(-2147483648.0),
        // scale = 1
        R<double>(0.0),
        R<double>(12.3),
        R<double>(-12.3),
        R<double>(1234.5),
        R<double>(-1234.5),
        R<double>(12345678901234567.8),
        R<double>(-12345678901234567.8),
        R<double>(3276.8),
        R<double>(-3276.8),
        R<double>(12345678901.2),
        R<double>(-12345678901.2),
        R<double>(214748364.7),
        R<double>(-214748364.7),
        R<double>(214748364.8),
        R<double>(-214748364.8),
        // scale = 3
        R<double>(0.0),
        R<double>(0.123),
        R<double>(-0.123),
        R<double>(12.345),
        R<double>(-12.345),
        R<double>(123456789012345.678),
        R<double>(-123456789012345.678),
        R<double>(32.768),
        R<double>(-32.768),
        R<double>(123456789.012),
        R<double>(-123456789.012),
        R<double>(2147483.647),
        R<double>(-2147483.647),
        R<double>(2147483.648),
        R<double>(-2147483.648),
    },
    std::vector<R<int64_t>>{
        // inScale = 0, outScale = 0
        R<int64_t>(0), R<int64_t>(123), R<int64_t>(-123), R<int64_t>(12345), R<int64_t>(-12345),
        R<int64_t>(123456789012345678LL), R<int64_t>(-123456789012345678LL), R<int64_t>(32768), R<int64_t>(-32768),
        R<int64_t>(123456789012LL), R<int64_t>(-123456789012LL), R<int64_t>(2147483647LL), R<int64_t>(-2147483647LL),
        R<int64_t>(2147483648LL), R<int64_t>(-2147483648LL),
        // inScale = 0, outScale = 2
        R<int64_t>(0), R<int64_t>(12300), R<int64_t>(-12300), R<int64_t>(1234500), R<int64_t>(-1234500), R<int64_t>(),
        R<int64_t>(), R<int64_t>(3276800LL), R<int64_t>(-3276800LL), R<int64_t>(12345678901200LL),
        R<int64_t>(-12345678901200LL), R<int64_t>(214748364700LL), R<int64_t>(-214748364700LL),
        R<int64_t>(214748364800LL), R<int64_t>(-214748364800LL),
        // inScale = 1, outScale = 0
        R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(1234), R<int64_t>(-1234),
        R<int64_t>(12345678901234567LL), R<int64_t>(-12345678901234567LL), R<int64_t>(3276LL), R<int64_t>(-3276LL),
        R<int64_t>(12345678901LL), R<int64_t>(-12345678901LL), R<int64_t>(214748364LL), R<int64_t>(-214748364LL),
        R<int64_t>(214748364LL), R<int64_t>(-214748364LL),
        // inScale = 1, outScale = 2
        R<int64_t>(0), R<int64_t>(1230LL), R<int64_t>(-1230LL), R<int64_t>(123450LL), R<int64_t>(-123450LL),
        R<int64_t>(1234567890123456780LL), R<int64_t>(-1234567890123456780LL), R<int64_t>(327680LL),
        R<int64_t>(-327680LL), R<int64_t>(1234567890120LL), R<int64_t>(-1234567890120LL), R<int64_t>(21474836470LL),
        R<int64_t>(-21474836470LL), R<int64_t>(21474836480LL), R<int64_t>(-21474836480LL),
        // inScale = 3, outScale = 0
        R<int64_t>(0), R<int64_t>(0), R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(123456789012345LL),
        R<int64_t>(-123456789012345LL), R<int64_t>(32), R<int64_t>(-32), R<int64_t>(123456789), R<int64_t>(-123456789),
        R<int64_t>(2147483), R<int64_t>(-2147483), R<int64_t>(2147483), R<int64_t>(-2147483),
        // inScale = 3, outScale = 2
        R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(1234), R<int64_t>(-1234),
        R<int64_t>(12345678901234567LL), R<int64_t>(-12345678901234567LL), R<int64_t>(3276), R<int64_t>(-3276),
        R<int64_t>(12345678901LL), R<int64_t>(-12345678901LL), R<int64_t>(214748364LL), R<int64_t>(-214748364LL),
        R<int64_t>(214748364LL), R<int64_t>(-214748364LL)},
    std::vector<R<Decimal128>>{
        // inScale = 0, outScale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("123")), R<Decimal128>(Decimal128("-123")),
        R<Decimal128>(Decimal128("12345")), R<Decimal128>(Decimal128("-12345")),
        R<Decimal128>(Decimal128("123456789012345678")), R<Decimal128>(Decimal128("-123456789012345678")),
        R<Decimal128>(Decimal128("32768")), R<Decimal128>(Decimal128("-32768")),
        R<Decimal128>(Decimal128("123456789012")), R<Decimal128>(Decimal128("-123456789012")),
        R<Decimal128>(Decimal128("2147483647")), R<Decimal128>(Decimal128("-2147483647")),
        R<Decimal128>(Decimal128("2147483648")), R<Decimal128>(Decimal128("-2147483648")),
        // inScale = 0, outScale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12300")), R<Decimal128>(Decimal128("-12300")),
        R<Decimal128>(Decimal128("1234500")), R<Decimal128>(Decimal128("-1234500")),
        R<Decimal128>(Decimal128("12345678901234567800")), R<Decimal128>(Decimal128("-12345678901234567800")),
        R<Decimal128>(Decimal128("3276800")), R<Decimal128>(Decimal128("-3276800")),
        R<Decimal128>(Decimal128("12345678901200")), R<Decimal128>(Decimal128("-12345678901200")),
        R<Decimal128>(Decimal128("214748364700")), R<Decimal128>(Decimal128("-214748364700")),
        R<Decimal128>(Decimal128("214748364800")), R<Decimal128>(Decimal128("-214748364800")),
        // inScale = 1, outScale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")),
        R<Decimal128>(Decimal128("1235")), R<Decimal128>(Decimal128("-1235")),
        R<Decimal128>(Decimal128("12345678901234568")), R<Decimal128>(Decimal128("-12345678901234568")),
        R<Decimal128>(Decimal128("3277")), R<Decimal128>(Decimal128("-3277")), R<Decimal128>(Decimal128("12345678901")),
        R<Decimal128>(Decimal128("-12345678901")), R<Decimal128>(Decimal128("214748365")),
        R<Decimal128>(Decimal128("-214748365")), R<Decimal128>(Decimal128("214748365")),
        R<Decimal128>(Decimal128("-214748365")),
        // inScale = 1, outScale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("1230")), R<Decimal128>(Decimal128("-1230")),
        R<Decimal128>(Decimal128("123450")), R<Decimal128>(Decimal128("-123450")),
        R<Decimal128>(Decimal128("1234567890123456780")), R<Decimal128>(Decimal128("-1234567890123456780")),
        R<Decimal128>(Decimal128("327680")), R<Decimal128>(Decimal128("-327680")),
        R<Decimal128>(Decimal128("1234567890120")), R<Decimal128>(Decimal128("-1234567890120")),
        R<Decimal128>(Decimal128("21474836470")), R<Decimal128>(Decimal128("-21474836470")),
        R<Decimal128>(Decimal128("21474836480")), R<Decimal128>(Decimal128("-21474836480")),
        // inScale = 3, outScale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("0")),
        R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")), R<Decimal128>(Decimal128("123456789012346")),
        R<Decimal128>(Decimal128("-123456789012346")), R<Decimal128>(Decimal128("33")),
        R<Decimal128>(Decimal128("-33")), R<Decimal128>(Decimal128("123456789")),
        R<Decimal128>(Decimal128("-123456789")), R<Decimal128>(Decimal128("2147484")),
        R<Decimal128>(Decimal128("-2147484")), R<Decimal128>(Decimal128("2147484")),
        R<Decimal128>(Decimal128("-2147484")),
        // inScale = 3, outScale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")),
        R<Decimal128>(Decimal128("1235")), R<Decimal128>(Decimal128("-1235")),
        R<Decimal128>(Decimal128("12345678901234568")), R<Decimal128>(Decimal128("-12345678901234568")),
        R<Decimal128>(Decimal128("3277")), R<Decimal128>(Decimal128("-3277")), R<Decimal128>(Decimal128("12345678901")),
        R<Decimal128>(Decimal128("-12345678901")), R<Decimal128>(Decimal128("214748365")),
        R<Decimal128>(Decimal128("-214748365")), R<Decimal128>(Decimal128("214748365")),
        R<Decimal128>(Decimal128("-214748365"))},
    RepeatVector<Decimal128>(
        std::vector<R<Decimal128>>{R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("123")),
                                   R<Decimal128>(Decimal128("-123")), R<Decimal128>(Decimal128("12345")),
                                   R<Decimal128>(Decimal128("-12345")), R<Decimal128>(Decimal128("123456789012345678")),
                                   R<Decimal128>(Decimal128("-123456789012345678")), R<Decimal128>(Decimal128("32768")),
                                   R<Decimal128>(Decimal128("-32768")), R<Decimal128>(Decimal128("123456789012")),
                                   R<Decimal128>(Decimal128("-123456789012")), R<Decimal128>(Decimal128("2147483647")),
                                   R<Decimal128>(Decimal128("-2147483647")), R<Decimal128>(Decimal128("2147483648")),
                                   R<Decimal128>(Decimal128("-2147483648"))},
        3));

static std::vector<R<Decimal128>> inDec128{R<Decimal128>(Decimal128("0")),
                                           R<Decimal128>(Decimal128("123")),
                                           R<Decimal128>(Decimal128("-123")),
                                           R<Decimal128>(Decimal128("12345")),
                                           R<Decimal128>(Decimal128("-12345")),
                                           R<Decimal128>(Decimal128("12345678")),
                                           R<Decimal128>(Decimal128("-12345678")),
                                           R<Decimal128>(Decimal128("123456789012345678")),
                                           R<Decimal128>(Decimal128("-123456789012345678")),
                                           R<Decimal128>(Decimal128("1234567890123456789")),
                                           R<Decimal128>(Decimal128("-1234567890123456789")),
                                           R<Decimal128>(Decimal128("12345678901234567891")),
                                           R<Decimal128>(Decimal128("-12345678901234567891")),
                                           R<Decimal128>(Decimal128("12345678901234567890123")),
                                           R<Decimal128>(Decimal128("-12345678901234567890123"))};
static Results inDec128Results(
    std::vector<R<int16_t>>{// scale = 0
                            R<int16_t>(0), R<int16_t>(123), R<int16_t>(-123), R<int16_t>(12345), R<int16_t>(-12345),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            // scale = 1
                            R<int16_t>(0), R<int16_t>(12), R<int16_t>(-12), R<int16_t>(1235), R<int16_t>(-1235),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            // scale = 3
                            R<int16_t>(0), R<int16_t>(0), R<int16_t>(0), R<int16_t>(12), R<int16_t>(-12),
                            R<int16_t>(12346), R<int16_t>(-12346), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>()},
    std::vector<R<int32_t>>{// scale = 0
                            R<int32_t>(0), R<int32_t>(123), R<int32_t>(-123), R<int32_t>(12345), R<int32_t>(-12345),
                            R<int32_t>(12345678), R<int32_t>(-12345678), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            // scale = 1
                            R<int32_t>(0), R<int32_t>(12), R<int32_t>(-12), R<int32_t>(1235), R<int32_t>(-1235),
                            R<int32_t>(1234568), R<int32_t>(-1234568), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            // scale = 3
                            R<int32_t>(0), R<int32_t>(0), R<int32_t>(0), R<int32_t>(12), R<int32_t>(-12),
                            R<int32_t>(12346), R<int32_t>(-12346), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>()},
    std::vector<R<int64_t>>{
        // scale = 0
        R<int64_t>(0), R<int64_t>(123), R<int64_t>(-123), R<int64_t>(12345), R<int64_t>(-12345), R<int64_t>(12345678),
        R<int64_t>(-12345678), R<int64_t>(123456789012345678LL), R<int64_t>(-123456789012345678LL),
        R<int64_t>(1234567890123456789LL), R<int64_t>(-1234567890123456789LL), R<int64_t>(), R<int64_t>(), R<int64_t>(),
        R<int64_t>(),
        // scale = 1
        R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(1235), R<int64_t>(-1235), R<int64_t>(1234568),
        R<int64_t>(-1234568), R<int64_t>(12345678901234568LL), R<int64_t>(-12345678901234568LL),
        R<int64_t>(123456789012345679LL), R<int64_t>(-123456789012345679LL), R<int64_t>(1234567890123456789LL),
        R<int64_t>(-1234567890123456789LL), R<int64_t>(), R<int64_t>(),
        // scale = 3
        R<int64_t>(0), R<int64_t>(0), R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(12346),
        R<int64_t>(-12346), R<int64_t>(123456789012346LL), R<int64_t>(-123456789012346LL),
        R<int64_t>(1234567890123457LL), R<int64_t>(-1234567890123457LL), R<int64_t>(12345678901234568LL),
        R<int64_t>(-12345678901234568LL), R<int64_t>(), R<int64_t>()},
    std::vector<R<double>>{
        // scale = 0
        R<double>(0.0), R<double>(123.0), R<double>(-123.0), R<double>(12345.0), R<double>(-12345.0),
        R<double>(12345678.0), R<double>(-12345678.0), R<double>(123456789012345678.0),
        R<double>(-123456789012345678.0), R<double>(1234567890123456789.0), R<double>(-1234567890123456789.0),
        R<double>(12345678901234567891.0), R<double>(-12345678901234567891.0), R<double>(12345678901234567890123.0),
        R<double>(-12345678901234567890123.0),
        // scale = 1
        R<double>(0.0), R<double>(12.3), R<double>(-12.3), R<double>(1234.5), R<double>(-1234.5), R<double>(1234567.8),
        R<double>(-1234567.8), R<double>(12345678901234567.8), R<double>(-12345678901234567.8),
        R<double>(123456789012345678.9), R<double>(-123456789012345678.9), R<double>(1234567890123456789.1),
        R<double>(-1234567890123456789.1), R<double>(1234567890123456789012.3), R<double>(-1234567890123456789012.3),
        // scale = 3
        R<double>(0.0), R<double>(0.123), R<double>(-0.123), R<double>(12.345), R<double>(-12.345),
        R<double>(12345.678), R<double>(-12345.678), R<double>(123456789012345.678), R<double>(-123456789012345.678),
        R<double>(1234567890123456.789), R<double>(-1234567890123456.789), R<double>(12345678901234567.891),
        R<double>(-12345678901234567.891), R<double>(12345678901234567890.123), R<double>(-12345678901234567890.123)},
    std::vector<R<int64_t>>{
        // inScale = 0, outScale = 0
        R<int64_t>(0), R<int64_t>(123), R<int64_t>(-123), R<int64_t>(12345), R<int64_t>(-12345), R<int64_t>(12345678),
        R<int64_t>(-12345678), R<int64_t>(123456789012345678LL), R<int64_t>(-123456789012345678LL),
        R<int64_t>(1234567890123456789LL), R<int64_t>(-1234567890123456789LL), R<int64_t>(), R<int64_t>(), R<int64_t>(),
        R<int64_t>(),
        // inScale = 0, outScale = 2
        R<int64_t>(0), R<int64_t>(12300), R<int64_t>(-12300), R<int64_t>(1234500), R<int64_t>(-1234500),
        R<int64_t>(1234567800LL), R<int64_t>(-1234567800LL), R<int64_t>(), R<int64_t>(), R<int64_t>(), R<int64_t>(),
        R<int64_t>(), R<int64_t>(), R<int64_t>(), R<int64_t>(),
        // inScale = 1, outScale = 0
        R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(1235), R<int64_t>(-1235), R<int64_t>(1234568),
        R<int64_t>(-1234568), R<int64_t>(12345678901234568LL), R<int64_t>(-12345678901234568LL),
        R<int64_t>(123456789012345679LL), R<int64_t>(-123456789012345679LL), R<int64_t>(1234567890123456789LL),
        R<int64_t>(-1234567890123456789LL), R<int64_t>(), R<int64_t>(),
        // inScale = 1, outScale = 2
        R<int64_t>(0), R<int64_t>(1230), R<int64_t>(-1230), R<int64_t>(123450), R<int64_t>(-123450),
        R<int64_t>(123456780LL), R<int64_t>(-123456780LL), R<int64_t>(1234567890123456780LL),
        R<int64_t>(-1234567890123456780LL), R<int64_t>(), R<int64_t>(), R<int64_t>(), R<int64_t>(), R<int64_t>(),
        R<int64_t>(),
        // inScale = 3, outScale = 0
        R<int64_t>(0), R<int64_t>(0), R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(12346),
        R<int64_t>(-12346), R<int64_t>(123456789012346LL), R<int64_t>(-123456789012346LL),
        R<int64_t>(1234567890123457LL), R<int64_t>(-1234567890123457LL), R<int64_t>(12345678901234568LL),
        R<int64_t>(-12345678901234568LL), R<int64_t>(), R<int64_t>(),
        // inScale = 3, outScale = 2
        R<int64_t>(0), R<int64_t>(12), R<int64_t>(-12), R<int64_t>(1235), R<int64_t>(-1235), R<int64_t>(1234568LL),
        R<int64_t>(-1234568LL), R<int64_t>(12345678901234568LL), R<int64_t>(-12345678901234568LL),
        R<int64_t>(123456789012345679LL), R<int64_t>(-123456789012345679LL), R<int64_t>(1234567890123456789LL),
        R<int64_t>(-1234567890123456789LL), R<int64_t>(), R<int64_t>()},
    std::vector<R<Decimal128>>{
        // inScale = 0, outScale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("123")), R<Decimal128>(Decimal128("-123")),
        R<Decimal128>(Decimal128("12345")), R<Decimal128>(Decimal128("-12345")), R<Decimal128>(Decimal128("12345678")),
        R<Decimal128>(Decimal128("-12345678")), R<Decimal128>(Decimal128("123456789012345678")),
        R<Decimal128>(Decimal128("-123456789012345678")), R<Decimal128>(Decimal128("1234567890123456789")),
        R<Decimal128>(Decimal128("-1234567890123456789")), R<Decimal128>(Decimal128("12345678901234567891")),
        R<Decimal128>(Decimal128("-12345678901234567891")), R<Decimal128>(Decimal128("12345678901234567890123")),
        R<Decimal128>(Decimal128("-12345678901234567890123")),
        // inScale = 0, outScale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12300")), R<Decimal128>(Decimal128("-12300")),
        R<Decimal128>(Decimal128("1234500")), R<Decimal128>(Decimal128("-1234500")),
        R<Decimal128>(Decimal128("1234567800")), R<Decimal128>(Decimal128("-1234567800")),
        R<Decimal128>(Decimal128("12345678901234567800")), R<Decimal128>(Decimal128("-12345678901234567800")),
        R<Decimal128>(Decimal128("123456789012345678900")), R<Decimal128>(Decimal128("-123456789012345678900")),
        R<Decimal128>(Decimal128("1234567890123456789100")), R<Decimal128>(Decimal128("-1234567890123456789100")),
        R<Decimal128>(Decimal128("1234567890123456789012300")), R<Decimal128>(Decimal128("-1234567890123456789012300")),
        // inScale = 1, outScale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")),
        R<Decimal128>(Decimal128("1235")), R<Decimal128>(Decimal128("-1235")), R<Decimal128>(Decimal128("1234568")),
        R<Decimal128>(Decimal128("-1234568")), R<Decimal128>(Decimal128("12345678901234568")),
        R<Decimal128>(Decimal128("-12345678901234568")), R<Decimal128>(Decimal128("123456789012345679")),
        R<Decimal128>(Decimal128("-123456789012345679")), R<Decimal128>(Decimal128("1234567890123456789")),
        R<Decimal128>(Decimal128("-1234567890123456789")), R<Decimal128>(Decimal128("1234567890123456789012")),
        R<Decimal128>(Decimal128("-1234567890123456789012")),
        // inScale = 1, outScale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("1230")), R<Decimal128>(Decimal128("-1230")),
        R<Decimal128>(Decimal128("123450")), R<Decimal128>(Decimal128("-123450")),
        R<Decimal128>(Decimal128("123456780")), R<Decimal128>(Decimal128("-123456780")),
        R<Decimal128>(Decimal128("1234567890123456780")), R<Decimal128>(Decimal128("-1234567890123456780")),
        R<Decimal128>(Decimal128("12345678901234567890")), R<Decimal128>(Decimal128("-12345678901234567890")),
        R<Decimal128>(Decimal128("123456789012345678910")), R<Decimal128>(Decimal128("-123456789012345678910")),
        R<Decimal128>(Decimal128("123456789012345678901230")), R<Decimal128>(Decimal128("-123456789012345678901230")),
        // inScale = 3, outScale = 0
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("0")),
        R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")), R<Decimal128>(Decimal128("12346")),
        R<Decimal128>(Decimal128("-12346")), R<Decimal128>(Decimal128("123456789012346")),
        R<Decimal128>(Decimal128("-123456789012346")), R<Decimal128>(Decimal128("1234567890123457")),
        R<Decimal128>(Decimal128("-1234567890123457")), R<Decimal128>(Decimal128("12345678901234568")),
        R<Decimal128>(Decimal128("-12345678901234568")), R<Decimal128>(Decimal128("12345678901234567890")),
        R<Decimal128>(Decimal128("-12345678901234567890")),
        // inScale = 3, outScale = 2
        R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("12")), R<Decimal128>(Decimal128("-12")),
        R<Decimal128>(Decimal128("1235")), R<Decimal128>(Decimal128("-1235")), R<Decimal128>(Decimal128("1234568")),
        R<Decimal128>(Decimal128("-1234568")), R<Decimal128>(Decimal128("12345678901234568")),
        R<Decimal128>(Decimal128("-12345678901234568")), R<Decimal128>(Decimal128("123456789012345679")),
        R<Decimal128>(Decimal128("-123456789012345679")), R<Decimal128>(Decimal128("1234567890123456789")),
        R<Decimal128>(Decimal128("-1234567890123456789")), R<Decimal128>(Decimal128("1234567890123456789012")),
        R<Decimal128>(Decimal128("-1234567890123456789012"))},
    RepeatVector<Decimal128>(
        std::vector<R<Decimal128>>{
            R<Decimal128>(Decimal128("0")), R<Decimal128>(Decimal128("123")), R<Decimal128>(Decimal128("-123")),
            R<Decimal128>(Decimal128("12345")), R<Decimal128>(Decimal128("-12345")),
            R<Decimal128>(Decimal128("12345678")), R<Decimal128>(Decimal128("-12345678")),
            R<Decimal128>(Decimal128("123456789012345678")), R<Decimal128>(Decimal128("-123456789012345678")),
            R<Decimal128>(Decimal128("1234567890123456789")), R<Decimal128>(Decimal128("-1234567890123456789")),
            R<Decimal128>(Decimal128("12345678901234567891")), R<Decimal128>(Decimal128("-12345678901234567891")),
            R<Decimal128>(Decimal128("12345678901234567890123")),
            R<Decimal128>(Decimal128("-12345678901234567890123"))},
        3));

static std::vector<R<Decimal128>> inVarchar{R<Decimal128>(Decimal128("0")),
                                            R<Decimal128>(Decimal128("123")),
                                            R<Decimal128>(Decimal128("-123")),
                                            R<Decimal128>(Decimal128("12345")),
                                            R<Decimal128>(Decimal128("-12345")),
                                            R<Decimal128>(Decimal128("12345678")),
                                            R<Decimal128>(Decimal128("-12345678")),
                                            R<Decimal128>(Decimal128("123456789012345678")),
                                            R<Decimal128>(Decimal128("-123456789012345678")),
                                            R<Decimal128>(Decimal128("1234567890123456789")),
                                            R<Decimal128>(Decimal128("-1234567890123456789")),
                                            R<Decimal128>(Decimal128("12345678901234567891")),
                                            R<Decimal128>(Decimal128("-12345678901234567891")),
                                            R<Decimal128>(Decimal128("12345678901234567890123")),
                                            R<Decimal128>(Decimal128("-12345678901234567890123"))};
static Results inVarcharResults(
    std::vector<R<int16_t>>{R<int16_t>(0), R<int16_t>(123), R<int16_t>(-123), R<int16_t>(12345), R<int16_t>(-12345),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>(),
                            R<int16_t>(), R<int16_t>(), R<int16_t>(), R<int16_t>()},
    std::vector<R<int32_t>>{R<int32_t>(0), R<int32_t>(123), R<int32_t>(-123), R<int32_t>(12345), R<int32_t>(-12345),
                            R<int32_t>(12345678), R<int32_t>(-12345678), R<int32_t>(), R<int32_t>(), R<int32_t>(),
                            R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>(), R<int32_t>()},
    std::vector<R<int64_t>>{R<int64_t>(0), R<int64_t>(123), R<int64_t>(-123), R<int64_t>(12345), R<int64_t>(-12345),
                            R<int64_t>(12345678), R<int64_t>(-12345678), R<int64_t>(123456789012345678LL),
                            R<int64_t>(-123456789012345678LL), R<int64_t>(1234567890123456789LL),
                            R<int64_t>(-1234567890123456789LL), R<int64_t>(), R<int64_t>(), R<int64_t>(), R<int64_t>()},
    std::vector<R<double>>{R<double>(0.0), R<double>(123.0), R<double>(-123.0), R<double>(12345.0), R<double>(-12345.0),
                           R<double>(12345678.0), R<double>(-12345678.0), R<double>(123456789012345678.0),
                           R<double>(-123456789012345678.0), R<double>(1234567890123456789.0),
                           R<double>(-1234567890123456789.0), R<double>(12345678901234567891.0),
                           R<double>(-12345678901234567891.0), R<double>(12345678901234567890123.0),
                           R<double>(-12345678901234567890123.0)},
    std::vector<R<int64_t>>{
        // scale = 0
        R<int64_t>(0),
        R<int64_t>(123),
        R<int64_t>(-123),
        R<int64_t>(12345),
        R<int64_t>(-12345),
        R<int64_t>(12345678),
        R<int64_t>(-12345678),
        R<int64_t>(123456789012345678LL),
        R<int64_t>(-123456789012345678LL),
        R<int64_t>(1234567890123456789LL),
        R<int64_t>(-1234567890123456789LL),
        R<int64_t>(),
        R<int64_t>(),
        R<int64_t>(),
        R<int64_t>(),
        // scale = 2
        R<int64_t>(0),
        R<int64_t>(123),
        R<int64_t>(-123),
        R<int64_t>(12345),
        R<int64_t>(-12345),
        R<int64_t>(12345678),
        R<int64_t>(-12345678),
        R<int64_t>(123456789012345678LL),
        R<int64_t>(-123456789012345678LL),
        R<int64_t>(1234567890123456789LL),
        R<int64_t>(-1234567890123456789LL),
        R<int64_t>(),
        R<int64_t>(),
        R<int64_t>(),
        R<int64_t>(),
    },
    ConcatinateValues<Decimal128>(inVarchar, inVarchar), inVarchar);

class AggregatorCastTest : public ::testing::TestWithParam<std::tuple<DataTypeId, int32_t, DataTypeId>> {
public:
protected:
    virtual void SetUp()
    {
        ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::NOT_CHECK_RESCALE);
        ConfigUtil::SetEmptySearchStrReplaceRule(EmptySearchStrReplaceRule::REPLACE);
        ConfigUtil::SetCastDecimalToDoubleRule(CastDecimalToDoubleRule::CAST);
        ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
        ConfigUtil::SetRoundingRule(RoundingRule::HALF_UP);
        ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    }
};

// test execution functions
template <typename InType, typename OutType>
static void TestNumericInputNumericOutput(AggregatorCastTestClass &agg, const InType &inValue,
                                          const R<OutType> &expected)
{
    bool overflow = false;
    OutType result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.v);
        } else {
            EXPECT_EQ(result, expected.v);
        }
    }
}

template <typename InType, typename OutType>
static void TestNumericInputDecimalOutput(AggregatorCastTestClass &agg, const InType &inValue, const int32_t valueIndex,
                                          const std::vector<R<OutType>> &expected)
{
    int32_t expectedIndex = valueIndex;
    bool overflow = false;
    OutType result;

    // did not put following repeated tests in a loop to easily identify error based on EXPECT line number
    SetDecimalOutputWithScale(agg, 0);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalOutputWithScale(agg, 2);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }
}

template <typename InType>
static void TestNumericInput(AggregatorCastTestClass &agg, const int32_t valueIndex, const InType &inValue,
                             const Results &results)
{
    DataTypeId outType = agg.GetOutputTypes().GetType(0)->GetId();

    switch (outType) {
        case OMNI_SHORT:
            TestNumericInputNumericOutput<InType, int16_t>(agg, inValue, results.out16.at(valueIndex));
            break;
        case OMNI_INT:
            TestNumericInputNumericOutput<InType, int32_t>(agg, inValue, results.out32.at(valueIndex));
            break;
        case OMNI_LONG:
            TestNumericInputNumericOutput<InType, int64_t>(agg, inValue, results.out64.at(valueIndex));
            break;
        case OMNI_DOUBLE:
            TestNumericInputNumericOutput<InType, double>(agg, inValue, results.outDouble.at(valueIndex));
            break;
        case OMNI_DECIMAL64:
            TestNumericInputDecimalOutput<InType, int64_t>(agg, inValue, valueIndex, results.outDecimal64);
            break;
        case OMNI_DECIMAL128:
            TestNumericInputDecimalOutput<InType, Decimal128>(agg, inValue, valueIndex, results.outDecimal128);
            break;
        case OMNI_VARCHAR:
            TestNumericInputNumericOutput<InType, Decimal128>(agg, inValue, results.outVarchar.at(valueIndex));
            break;
        default: throw OmniException("Invalid Argument", "Invalid type " + TypeUtil::TypeToStringLog(outType));
    }
}

template <typename InType, typename OutType>
static void TestDecimalInputNumericOutput(AggregatorCastTestClass &agg, const InType &inValue, const int32_t valueIndex,
                                          const std::vector<R<OutType>> &expected)
{
    int32_t expectedIndex = valueIndex;
    bool overflow = false;
    OutType result;

    // did not put following repeated tests in a loop to easily identify error based on EXPECT line number
    SetDecimalInputWithScale(agg, 0);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 1);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 3);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }
}

template <typename InType, typename OutType>
static void TestDecimalInputDecimalOutput(AggregatorCastTestClass &agg, const InType &inValue, const int32_t valueIndex,
                                          const std::vector<R<OutType>> &expected)
{
    int32_t expectedIndex = valueIndex;
    bool overflow = false;
    OutType result;

    // did not put following repeated tests in a loop to easily identify error based on EXPECT line number
    SetDecimalInputWithScale(agg, 0);
    SetDecimalOutputWithScale(agg, 0);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 0);
    SetDecimalOutputWithScale(agg, 2);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 1);
    SetDecimalOutputWithScale(agg, 0);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 1);
    SetDecimalOutputWithScale(agg, 2);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 3);
    SetDecimalOutputWithScale(agg, 0);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }

    expectedIndex += NUM_VALUES;
    overflow = false;
    SetDecimalInputWithScale(agg, 3);
    SetDecimalOutputWithScale(agg, 2);
    result = agg.TestCastWithOverflow<InType, OutType>(inValue, overflow);
    EXPECT_EQ(overflow, expected.at(expectedIndex).overflow);
    if (!overflow) {
        if constexpr (std::is_same_v<OutType, double>) {
            EXPECT_DOUBLE_EQ(result, expected.at(expectedIndex).v);
        } else {
            EXPECT_EQ(result, expected.at(expectedIndex).v);
        }
    }
}

template <typename InType>
static void TestDecimalInput(AggregatorCastTestClass &agg, const int32_t valueIndex, const InType &inValue,
                             const Results &results)
{
    DataTypeId outType = agg.GetOutputTypes().GetType(0)->GetId();

    switch (outType) {
        case OMNI_SHORT: TestDecimalInputNumericOutput<InType, int16_t>(agg, inValue, valueIndex, results.out16); break;
        case OMNI_INT: TestDecimalInputNumericOutput<InType, int32_t>(agg, inValue, valueIndex, results.out32); break;
        case OMNI_LONG: TestDecimalInputNumericOutput<InType, int64_t>(agg, inValue, valueIndex, results.out64); break;
        case OMNI_DOUBLE:
            TestDecimalInputNumericOutput<InType, double>(agg, inValue, valueIndex, results.outDouble);
            break;
        case OMNI_DECIMAL64:
            TestDecimalInputDecimalOutput<InType, int64_t>(agg, inValue, valueIndex, results.outDecimal64);
            break;
        case OMNI_DECIMAL128:
            TestDecimalInputDecimalOutput<InType, Decimal128>(agg, inValue, valueIndex, results.outDecimal128);
            break;
        case OMNI_VARCHAR:
            TestDecimalInputNumericOutput<InType, Decimal128>(agg, inValue, valueIndex, results.outVarchar);
            break;
        default: throw OmniException("Invalid Argument", "Invalid type " + TypeUtil::TypeToStringLog(outType));
    }
}

TEST_P(AggregatorCastTest, verify_cast)
{
    const DataTypeId inType = std::get<0>(GetParam());
    const int32_t valueIndex = std::get<1>(GetParam());
    AggregatorCastTestClass agg(*CreateType(inType), *CreateType(std::get<2>(GetParam())), true, false, false);

    switch (inType) {
        case OMNI_SHORT: TestNumericInput<int16_t>(agg, valueIndex, in16.at(valueIndex).v, in16Results); break;
        case OMNI_INT: TestNumericInput<int32_t>(agg, valueIndex, in32.at(valueIndex).v, in32Results); break;
        case OMNI_LONG: TestNumericInput<int64_t>(agg, valueIndex, in64.at(valueIndex).v, in64Results); break;
        case OMNI_DOUBLE: TestNumericInput<double>(agg, valueIndex, inDouble.at(valueIndex).v, inDoubleResults); break;
        case OMNI_DECIMAL64:
            TestDecimalInput<int64_t>(agg, valueIndex, inDec64.at(valueIndex).v, inDec64Results);
            break;
        case OMNI_DECIMAL128:
            TestDecimalInput<Decimal128>(agg, valueIndex, inDec128.at(valueIndex).v, inDec128Results);
            break;
        case OMNI_VARCHAR:
            TestNumericInput<Decimal128>(agg, valueIndex, inVarchar.at(valueIndex).v, inVarcharResults);
            break;
        default: throw OmniException("Invalid Argument", "Invalid type " + TypeUtil::TypeToStringLog(inType));
    }
}

static std::vector<DataTypeId> testTypes{OMNI_SHORT,     OMNI_INT,        OMNI_LONG,   OMNI_DOUBLE,
                                         OMNI_DECIMAL64, OMNI_DECIMAL128, OMNI_VARCHAR};

INSTANTIATE_TEST_CASE_P(AggregatorTest, AggregatorCastTest,
                        ::testing::Combine(::testing::ValuesIn(testTypes), ::testing::Range(0, NUM_VALUES),
                                           ::testing::ValuesIn(testTypes)),
                        [](const testing::TestParamInfo<AggregatorCastTest::ParamType> &info) {
                            return TypeUtil::TypeToStringLog(std::get<0>(info.param)) + "_" +
                                   std::to_string(std::get<1>(info.param)) + "_" +
                                   TypeUtil::TypeToStringLog(std::get<2>(info.param));
                        });
} // namespace omniruntime
