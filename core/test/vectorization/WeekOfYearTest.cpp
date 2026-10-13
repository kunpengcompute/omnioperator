/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: week_of_year function unit tests (ISO week 1-53)
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/WeekOfYear.h"
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

class WeekOfYearTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment *const weekofyear_test_env =
    ::testing::AddGlobalTestEnvironment(new WeekOfYearTestEnvironment);

class WeekOfYearTestHelper {
public:
    static void ValidateResult(BaseVector *result, const std::vector<int32_t> &expected, int rowSize) {
        auto *resultVec = dynamic_cast<Vector<int32_t> *>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " week_of_year mismatch";
        }
    }

    static BaseVector *CreateDate32Vector(const std::vector<int32_t> &values) {
        BaseVector *vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, values.size());
        auto *typedVec = static_cast<Vector<int32_t> *>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector *CreateIntVector(const std::vector<int32_t> &values) {
        BaseVector *vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        auto *typedVec = static_cast<Vector<int32_t> *>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector *CreateTimestampVector(const std::vector<int64_t> &values) {
        BaseVector *vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto *typedVec = static_cast<Vector<int64_t> *>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector *CreateLongVector(const std::vector<int64_t> &values) {
        BaseVector *vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        auto *typedVec = static_cast<Vector<int64_t> *>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteWeekOfYear(BaseVector *inputVec, DataTypeId inputTypeId, BaseVector *&result) {
        auto signature = std::make_shared<FunctionSignature>("week_of_year",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "week_of_year function not found";
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector *> args;
        args.push(inputVec);
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context));
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    /// Timestamp at midnight UTC for the given date (microseconds)
    static int64_t DateToMicros(int year, int month, int day) {
        int32_t d = DateToDays(year, month, day);
        return static_cast<int64_t>(d) * 86400LL * 1000000LL;
    }
};

// Spark/Velox alignment: 1920-01-04 is week 1, 1920-01-05 is week 2
TEST(WeekOfYearTest, Date32Basic) {
    std::vector<int32_t> dateValues = {
        WeekOfYearTestHelper::DateToDays(1920, 1, 1),   // week 1
        WeekOfYearTestHelper::DateToDays(1920, 1, 4),   // week 1
        WeekOfYearTestHelper::DateToDays(1920, 1, 5),   // week 2
        WeekOfYearTestHelper::DateToDays(1970, 1, 1),   // week 1
        WeekOfYearTestHelper::DateToDays(2008, 2, 20),  // week 8
        WeekOfYearTestHelper::DateToDays(2015, 4, 8),   // week 15
    };
    std::vector<int32_t> expected = {1, 1, 2, 1, 8, 15};

    BaseVector *inputVec = WeekOfYearTestHelper::CreateDate32Vector(dateValues);
    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_DATE32, resultVec);
    WeekOfYearTestHelper::ValidateResult(resultVec, expected, static_cast<int>(dateValues.size()));

    delete resultVec;
}

// Year boundary: last/first week of year
TEST(WeekOfYearTest, Date32YearBoundary) {
    std::vector<int32_t> dateValues = {
        WeekOfYearTestHelper::DateToDays(1960, 1, 1),   // week 53 (prev year's week 1 extends)
        WeekOfYearTestHelper::DateToDays(1960, 1, 3),   // week 53
        WeekOfYearTestHelper::DateToDays(1960, 1, 4),   // week 1
        WeekOfYearTestHelper::DateToDays(1969, 12, 31), // week 1
        WeekOfYearTestHelper::DateToDays(2021, 1, 1),   // week 53
        WeekOfYearTestHelper::DateToDays(2016, 1, 1),   // week 53
        WeekOfYearTestHelper::DateToDays(2017, 1, 1),   // week 52
        WeekOfYearTestHelper::DateToDays(2022, 1, 1),   // week 52
        WeekOfYearTestHelper::DateToDays(2023, 1, 1),   // week 52
    };
    std::vector<int32_t> expected = {53, 53, 1, 1, 53, 53, 52, 52, 52};

    BaseVector *inputVec = WeekOfYearTestHelper::CreateDate32Vector(dateValues);
    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_DATE32, resultVec);
    WeekOfYearTestHelper::ValidateResult(resultVec, expected, static_cast<int>(dateValues.size()));

    delete resultVec;
}

TEST(WeekOfYearTest, Date32EpochAndExtreme) {
    std::vector<int32_t> dateValues = {
        WeekOfYearTestHelper::DateToDays(1919, 12, 31), // week 1
        WeekOfYearTestHelper::DateToDays(1, 1, 1),      // week 1 (year 1)
        WeekOfYearTestHelper::DateToDays(9999, 12, 31), // week 52
    };
    std::vector<int32_t> expected = {1, 1, 52};

    BaseVector *inputVec = WeekOfYearTestHelper::CreateDate32Vector(dateValues);
    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_DATE32, resultVec);
    WeekOfYearTestHelper::ValidateResult(resultVec, expected, static_cast<int>(dateValues.size()));

    delete resultVec;
}

TEST(WeekOfYearTest, IntBasic) {
    std::vector<int32_t> intValues = {
        WeekOfYearTestHelper::DateToDays(1970, 1, 1),
        WeekOfYearTestHelper::DateToDays(2013, 4, 8),  // week 15
    };
    std::vector<int32_t> expected = {1, 15};

    BaseVector *inputVec = WeekOfYearTestHelper::CreateIntVector(intValues);
    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_INT, resultVec);
    WeekOfYearTestHelper::ValidateResult(resultVec, expected, static_cast<int>(intValues.size()));

    delete resultVec;
}

TEST(WeekOfYearTest, TimestampBasic) {
    std::vector<int64_t> tsValues = {
        WeekOfYearTestHelper::DateToMicros(1970, 1, 1),
        WeekOfYearTestHelper::DateToMicros(2008, 2, 20),
        WeekOfYearTestHelper::DateToMicros(2005, 1, 1),  // week 53
    };
    std::vector<int32_t> expected = {1, 8, 53};

    BaseVector *inputVec = WeekOfYearTestHelper::CreateTimestampVector(tsValues);
    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_TIMESTAMP, resultVec);
    WeekOfYearTestHelper::ValidateResult(resultVec, expected, static_cast<int>(tsValues.size()));

    delete resultVec;
}

TEST(WeekOfYearTest, LongAsTimestamp) {
    std::vector<int64_t> longValues = {
        WeekOfYearTestHelper::DateToMicros(1970, 1, 1),
        WeekOfYearTestHelper::DateToMicros(2021, 1, 1),  // week 53
    };
    std::vector<int32_t> expected = {1, 53};

    BaseVector *inputVec = WeekOfYearTestHelper::CreateLongVector(longValues);
    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_LONG, resultVec);
    WeekOfYearTestHelper::ValidateResult(resultVec, expected, static_cast<int>(longValues.size()));

    delete resultVec;
}

TEST(WeekOfYearTest, Date32WithNull) {
    std::vector<int32_t> dateValues = {
        WeekOfYearTestHelper::DateToDays(2024, 1, 1),
        WeekOfYearTestHelper::DateToDays(2024, 6, 15),
        WeekOfYearTestHelper::DateToDays(2024, 12, 29),
    };
    BaseVector *inputVec = WeekOfYearTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(1);

    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));
    auto *typed = dynamic_cast<Vector<int32_t> *>(resultVec);
    EXPECT_EQ(typed->GetValue(0), 1);
    EXPECT_EQ(typed->GetValue(2), 52);

    delete resultVec;
}

TEST(WeekOfYearTest, TimestampWithNull) {
    std::vector<int64_t> tsValues = {
        WeekOfYearTestHelper::DateToMicros(2024, 1, 1),
        WeekOfYearTestHelper::DateToMicros(2024, 7, 1),
    };
    BaseVector *inputVec = WeekOfYearTestHelper::CreateTimestampVector(tsValues);
    inputVec->SetNull(1);

    BaseVector *resultVec = nullptr;
    WeekOfYearTestHelper::ExecuteWeekOfYear(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    auto *typed = dynamic_cast<Vector<int32_t> *>(resultVec);
    EXPECT_EQ(typed->GetValue(0), 1);

    delete resultVec;
}
