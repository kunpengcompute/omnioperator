/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: make_timestamp(year, month, day, hour, minute, second) -> TIMESTAMP unit tests
 */

#include <gtest/gtest.h>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class MakeTimestampTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const make_timestamp_test_env =
    ::testing::AddGlobalTestEnvironment(new MakeTimestampTestEnvironment);

class MakeTimestampTestHelper {
public:
    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
            vec->SetNotNull(i);
        }
        return vec;
    }

    static BaseVector* CreateConstInt32Vector(int32_t value, int32_t size) {
        return new ConstVector<int32_t>(value, OMNI_INT, size);
    }

    static BaseVector* CreateDoubleVector(const std::vector<double>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<double>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
            vec->SetNotNull(i);
        }
        return vec;
    }

    static BaseVector* CreateConstDecimal64Vector(int64_t value, int32_t size) {
        return new ConstVector<int64_t>(value, OMNI_DECIMAL64, size);
    }

    static void ExecuteMakeTimestamp(BaseVector* yearVec, BaseVector* monthVec, BaseVector* dayVec,
        BaseVector* hourVec, BaseVector* minVec, BaseVector* secVec, BaseVector*& result) {
        auto sig = std::make_shared<FunctionSignature>("make_timestamp",
            std::vector<DataTypeId>{OMNI_INT, OMNI_INT, OMNI_INT, OMNI_INT, OMNI_INT, secVec->GetTypeId()},
            OMNI_TIMESTAMP);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "make_timestamp function not found";
        auto outputType = std::make_shared<DataType>(OMNI_TIMESTAMP);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(yearVec->GetSize());
        std::stack<BaseVector*> args;
        // Push in call order (year, month, day, hour, min, sec) so stack top = sec, matching Apply pop order
        args.push(yearVec);
        args.push(monthVec);
        args.push(dayVec);
        args.push(hourVec);
        args.push(minVec);
        args.push(secVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static int64_t ExpectedMicros(int32_t year, int month, int day, int hour, int min, double sec) {
        LocalDate d(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        int32_t daysSinceEpoch = d.ToDays();
        int64_t microsSinceMidnight = static_cast<int64_t>(hour) * 3600LL * 1000000LL
            + static_cast<int64_t>(min) * 60LL * 1000000LL
            + static_cast<int64_t>(std::round(sec * 1000000.0));
        return daysSinceEpoch * 86400LL * 1000000LL + microsSinceMidnight;
    }
};

TEST(MakeTimestampTest, EpochTimestamp) {
    std::vector<int32_t> years = {1970}, months = {1}, days = {1};
    std::vector<int32_t> hours = {0}, mins = {0};
    std::vector<double> secs = {0.0};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);

    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(resultVec->GetValue(0), 0) << "1970-01-01 00:00:00 should be 0 micros";

    delete result;
}

TEST(MakeTimestampTest, ConstTimeArguments) {
    std::vector<int32_t> years = {2021, 1};
    std::vector<int32_t> months = {7, 13};
    std::vector<int32_t> days = {11, 1};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateConstInt32Vector(17, 1);
    BaseVector* mi = MakeTimestampTestHelper::CreateConstInt32Vector(56, 1);
    BaseVector* s = MakeTimestampTestHelper::CreateConstDecimal64Vector(20LL * 1000000LL, 1);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);

    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    int64_t exp = MakeTimestampTestHelper::ExpectedMicros(2021, 7, 11, 17, 56, 20.0);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(resultVec->GetValue(0), exp);
    EXPECT_TRUE(result->IsNull(1));

    delete result;
}

TEST(MakeTimestampTest, BasicTimestamp) {
    std::vector<int32_t> years = {2014, 2000};
    std::vector<int32_t> months = {12, 1};
    std::vector<int32_t> days = {28, 1};
    std::vector<int32_t> hours = {6, 0};
    std::vector<int32_t> mins = {30, 0};
    std::vector<double> secs = {45.0, 0.0};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);

    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    int64_t exp0 = MakeTimestampTestHelper::ExpectedMicros(2014, 12, 28, 6, 30, 45.0);
    int64_t exp1 = MakeTimestampTestHelper::ExpectedMicros(2000, 1, 1, 0, 0, 0.0);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_FALSE(result->IsNull(1));
    EXPECT_EQ(resultVec->GetValue(0), exp0);
    EXPECT_EQ(resultVec->GetValue(1), exp1);

    delete result;
}

TEST(MakeTimestampTest, FractionalSeconds) {
    std::vector<int32_t> years = {2014}, months = {12}, days = {28};
    std::vector<int32_t> hours = {6}, mins = {30};
    std::vector<double> secs = {45.887};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);

    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    int64_t exp = MakeTimestampTestHelper::ExpectedMicros(2014, 12, 28, 6, 30, 45.887);
    EXPECT_EQ(resultVec->GetValue(0), exp);

    delete result;
}

TEST(MakeTimestampTest, InvalidTimeReturnsNull) {
    std::vector<int32_t> years = {2014}, months = {12}, days = {28};
    std::vector<int32_t> hours = {25};
    std::vector<int32_t> mins = {0};
    std::vector<double> secs = {0.0};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0)) << "hour=25 should yield NULL";

    delete result;
}

// 24:00:00 is valid (end-of-day); 24:01:00, 24:00:01 etc. are invalid (hour/minute/second joint validation)
TEST(MakeTimestampTest, Time240000Valid) {
    std::vector<int32_t> years = {2024}, months = {6}, days = {15};
    std::vector<int32_t> hours = {24}, mins = {0};
    std::vector<double> secs = {0.0};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0)) << "24:00:00 should be valid";
    int64_t exp = MakeTimestampTestHelper::ExpectedMicros(2024, 6, 15, 24, 0, 0.0);
    EXPECT_EQ(dynamic_cast<Vector<int64_t>*>(result)->GetValue(0), exp);

    delete result;
}

TEST(MakeTimestampTest, Time240100Invalid) {
    std::vector<int32_t> years = {2024}, months = {6}, days = {15};
    std::vector<int32_t> hours = {24}, mins = {1};
    std::vector<double> secs = {0.0};

    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0)) << "24:01:00 should yield NULL (hour 24 only allows 24:00:00)";

    delete result;
}

TEST(MakeTimestampTest, NullPropagation) {
    std::vector<int32_t> years = {2024, 2024}, months = {1, 1}, days = {1, 2};
    std::vector<int32_t> hours = {0, 0}, mins = {0, 0};
    std::vector<double> secs = {0.0, 0.0};
    BaseVector* y = MakeTimestampTestHelper::CreateInt32Vector(years);
    BaseVector* mo = MakeTimestampTestHelper::CreateInt32Vector(months);
    BaseVector* d = MakeTimestampTestHelper::CreateInt32Vector(days);
    BaseVector* h = MakeTimestampTestHelper::CreateInt32Vector(hours);
    BaseVector* mi = MakeTimestampTestHelper::CreateInt32Vector(mins);
    BaseVector* s = MakeTimestampTestHelper::CreateDoubleVector(secs);
    y->SetNull(0);

    BaseVector* result = nullptr;
    MakeTimestampTestHelper::ExecuteMakeTimestamp(y, mo, d, h, mi, s, result);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0));
    EXPECT_FALSE(result->IsNull(1));

    delete result;
}
