/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: NextDay function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/NextDay.h"
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

class NextDayTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const next_day_test_env = ::testing::AddGlobalTestEnvironment(new NextDayTestEnvironment);

class NextDayFunctionTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << expectedValue << ", Actual=" << actualValue << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }

    static void ExecuteNextDay(BaseVector* dateVec, BaseVector* dowVec, DataTypeId dateTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("next_day",
            std::vector<DataTypeId>{dateTypeId, OMNI_VARCHAR}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "next_day function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context;
        context.SetResultRowSize(dateVec->GetSize());
        std::stack<BaseVector*> args;
        dateVec->SetIsField(true);  // caller-owned, do not let Apply delete
        dowVec->SetIsField(true);
        args.push(dateVec);
        args.push(dowVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "next_day function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: next_day with Monday abbreviations (Mon, mo, monday)
TEST(NextDayTest, MondayAbbreviations) {
    std::cout << "=== Test: next_day with Monday abbreviations ===" << std::endl;

    // 2015-07-23 is Thursday; next Monday is 2015-07-27
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Mon", "mo", "monday"};

    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 27);
    std::vector<int32_t> expected = {expectedDay, expectedDay, expectedDay};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with Tuesday abbreviations (Tue, tu, tuesday)
TEST(NextDayTest, TuesdayAbbreviations) {
    std::cout << "=== Test: next_day with Tuesday abbreviations ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Tue", "tu", "tuesday"};

    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 28);
    std::vector<int32_t> expected = {expectedDay, expectedDay, expectedDay};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with Wednesday abbreviations (we, wed, wednesday)
TEST(NextDayTest, WednesdayAbbreviations) {
    std::cout << "=== Test: next_day with Wednesday abbreviations ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"we", "wed", "wednesday"};

    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 29);
    std::vector<int32_t> expected = {expectedDay, expectedDay, expectedDay};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with Thursday abbreviations (Thu, TH, thursday)
TEST(NextDayTest, ThursdayAbbreviations) {
    std::cout << "=== Test: next_day with Thursday abbreviations ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Thu", "TH", "thursday"};

    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 30);
    std::vector<int32_t> expected = {expectedDay, expectedDay, expectedDay};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with Friday abbreviations (Fri, fr, friday)
TEST(NextDayTest, FridayAbbreviations) {
    std::cout << "=== Test: next_day with Friday abbreviations ===" << std::endl;

    // 2015-07-23 is Thursday; next Friday is 2015-07-24 (the very next day)
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Fri", "fr", "friday"};

    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 24);
    std::vector<int32_t> expected = {expectedDay, expectedDay, expectedDay};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with Saturday and Sunday
TEST(NextDayTest, SaturdayAndSunday) {
    std::cout << "=== Test: next_day with Saturday and Sunday ===" << std::endl;

    // 2015-07-23 is Thursday; next Saturday is 2015-07-25, next Sunday is 2015-07-26
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"saturday", "sunday"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 25),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 26)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day crossing month boundary
TEST(NextDayTest, CrossMonthBoundary) {
    std::cout << "=== Test: next_day crossing month boundary ===" << std::endl;

    // 2015-07-31 (Friday); next Wednesday is 2015-08-05
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 31)
    };
    std::vector<std::string> dowValues = {"wed"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(2015, 8, 5)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day crossing year boundary
TEST(NextDayTest, CrossYearBoundary) {
    std::cout << "=== Test: next_day crossing year boundary ===" << std::endl;

    // 2015-12-31 (Thursday); next Friday is 2016-01-01
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 12, 31)
    };
    std::vector<std::string> dowValues = {"Fri"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(2016, 1, 1)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with INT input type (equivalent to DATE32 at runtime)
TEST(NextDayTest, IntInputType) {
    std::cout << "=== Test: next_day with INT input type ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Mon", "Fri"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 27),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 24)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateIntVector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_INT, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with invalid day-of-week strings returns NULL
TEST(NextDayTest, InvalidDayOfWeek) {
    std::cout << "=== Test: next_day with invalid day-of-week strings ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"xx", "\"quote", ""};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (invalid 'xx')";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (invalid '\"quote')";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL (empty string)";

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with NULL date values
TEST(NextDayTest, NullDateValues) {
    std::cout << "=== Test: next_day with NULL date values ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Mon", "Mon", "Mon"};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    dateVec->SetNull(1);

    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (date is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 27);
    EXPECT_EQ(resultVecTyped->GetValue(0), expectedDay) << "Row 0 should be 2015-07-27";
    EXPECT_EQ(resultVecTyped->GetValue(2), expectedDay) << "Row 2 should be 2015-07-27";

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with NULL dayOfWeek values
TEST(NextDayTest, NullDayOfWeekValues) {
    std::cout << "=== Test: next_day with NULL dayOfWeek values ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Mon", "Mon", "Mon"};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    dowVec->SetNull(1);

    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (dayOfWeek is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day all NULL input
TEST(NextDayTest, AllNullInput) {
    std::cout << "=== Test: next_day all NULL input ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Mon", "Mon"};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    dateVec->SetNull(0);
    dateVec->SetNull(1);

    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with epoch date (1970-01-01 is Thursday)
TEST(NextDayTest, EpochDate) {
    std::cout << "=== Test: next_day with epoch date ===" << std::endl;

    // 1970-01-01 is Thursday (day 0)
    // next Friday is 1970-01-02 (day 1)
    // next Monday is 1970-01-05 (day 4)
    // next Thursday is 1970-01-08 (day 7)
    std::vector<int32_t> dateValues = {0, 0, 0};
    std::vector<std::string> dowValues = {"Fri", "Mon", "Thu"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(1970, 1, 2),
        NextDayFunctionTestHelper::DateToDays(1970, 1, 5),
        NextDayFunctionTestHelper::DateToDays(1970, 1, 8)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with negative days (before epoch)
TEST(NextDayTest, BeforeEpoch) {
    std::cout << "=== Test: next_day with negative days (before epoch) ===" << std::endl;

    // 1969-12-31 is Wednesday (day -1)
    // next Thursday is 1970-01-01 (day 0)
    std::vector<int32_t> dateValues = {-1};
    std::vector<std::string> dowValues = {"Thu"};

    std::vector<int32_t> expected = {0};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day with leap year
TEST(NextDayTest, LeapYear) {
    std::cout << "=== Test: next_day with leap year ===" << std::endl;

    // 2020-02-28 (Friday); next Saturday is 2020-02-29 (leap day!)
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2020, 2, 28)
    };
    std::vector<std::string> dowValues = {"Sat"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(2020, 2, 29)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day case insensitivity
TEST(NextDayTest, CaseInsensitivity) {
    std::cout << "=== Test: next_day case insensitivity ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"MONDAY", "Monday", "mOnDaY", "MO"};

    int32_t expectedDay = NextDayFunctionTestHelper::DateToDays(2015, 7, 27);
    std::vector<int32_t> expected = {expectedDay, expectedDay, expectedDay, expectedDay};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: next_day when target is same day of week as input
TEST(NextDayTest, SameDayOfWeek) {
    std::cout << "=== Test: next_day when target is same day of week ===" << std::endl;

    // 2015-07-23 is Thursday; next_day(Thursday) should be 2015-07-30 (not 2015-07-23 itself)
    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"thursday"};

    std::vector<int32_t> expected = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 30)
    };

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);
    NextDayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete dateVec;
    delete dowVec;
    delete resultVec;
}

// Test: comprehensive mixed valid and invalid inputs
TEST(NextDayTest, MixedValidAndInvalid) {
    std::cout << "=== Test: mixed valid and invalid inputs ===" << std::endl;

    std::vector<int32_t> dateValues = {
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23),
        NextDayFunctionTestHelper::DateToDays(2015, 7, 23)
    };
    std::vector<std::string> dowValues = {"Mon", "invalid", "Fri", ""};

    BaseVector* dateVec = NextDayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* dowVec = NextDayFunctionTestHelper::CreateStringVector(dowValues);
    BaseVector* resultVec = nullptr;

    NextDayFunctionTestHelper::ExecuteNextDay(dateVec, dowVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL (valid 'Mon')";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (invalid 'invalid')";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL (valid 'Fri')";
    EXPECT_TRUE(resultVec->IsNull(3)) << "Row 3 should be NULL (empty string)";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), NextDayFunctionTestHelper::DateToDays(2015, 7, 27));
    EXPECT_EQ(resultVecTyped->GetValue(2), NextDayFunctionTestHelper::DateToDays(2015, 7, 24));

    delete dateVec;
    delete dowVec;
    delete resultVec;
}
