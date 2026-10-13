/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: LastDay function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/LastDay.h"
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

class LastDayTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const last_day_test_env =
    ::testing::AddGlobalTestEnvironment(new LastDayTestEnvironment);

class LastDayTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteLastDay(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("last_day",
            std::vector<DataTypeId>{inputTypeId}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "last_day function not found for input type "
                                     << static_cast<int>(inputTypeId);

        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "last_day function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    static int64_t DateToMicros(int year, int month, int day) {
        int32_t d = DateToDays(year, month, day);
        return static_cast<int64_t>(d) * 86400LL * 1000000LL;
    }
};

// ======================================================================
// DATE32 input tests
// ======================================================================

// Test: last_day for all 12 months of 2015 (non-leap year) - matching Velox test
TEST(LastDayTest, Date32AllMonths2015) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2015, 2, 28),   // Feb non-leap
        LastDayTestHelper::DateToDays(2015, 3, 27),
        LastDayTestHelper::DateToDays(2015, 4, 26),
        LastDayTestHelper::DateToDays(2015, 5, 25),
        LastDayTestHelper::DateToDays(2015, 6, 24),
        LastDayTestHelper::DateToDays(2015, 7, 23),
        LastDayTestHelper::DateToDays(2015, 8, 1),
        LastDayTestHelper::DateToDays(2015, 9, 2),
        LastDayTestHelper::DateToDays(2015, 10, 3),
        LastDayTestHelper::DateToDays(2015, 11, 4),
        LastDayTestHelper::DateToDays(2015, 12, 5),
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2015, 2, 28),   // Feb 28 (non-leap)
        LastDayTestHelper::DateToDays(2015, 3, 31),
        LastDayTestHelper::DateToDays(2015, 4, 30),
        LastDayTestHelper::DateToDays(2015, 5, 31),
        LastDayTestHelper::DateToDays(2015, 6, 30),
        LastDayTestHelper::DateToDays(2015, 7, 31),
        LastDayTestHelper::DateToDays(2015, 8, 31),
        LastDayTestHelper::DateToDays(2015, 9, 30),
        LastDayTestHelper::DateToDays(2015, 10, 31),
        LastDayTestHelper::DateToDays(2015, 11, 30),
        LastDayTestHelper::DateToDays(2015, 12, 31),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: last_day for January 2016 and February 2016 (leap year) - matching Velox test
TEST(LastDayTest, Date32LeapYear2016) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2016, 1, 6),
        LastDayTestHelper::DateToDays(2016, 2, 7),
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2016, 1, 31),
        LastDayTestHelper::DateToDays(2016, 2, 29),   // Feb 29 (leap year)
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: last_day with NULL input - matching Velox null propagation
TEST(LastDayTest, Date32WithNull) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2024, 1, 15),
        LastDayTestHelper::DateToDays(2024, 6, 20),
        LastDayTestHelper::DateToDays(2024, 12, 31),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), LastDayTestHelper::DateToDays(2024, 1, 31));
    EXPECT_EQ(resultVecTyped->GetValue(2), LastDayTestHelper::DateToDays(2024, 12, 31));

    delete resultVec;
}

// Test: last_day with all NULL input
TEST(LastDayTest, Date32AllNull) {
    std::vector<int32_t> dateValues = {0, 0, 0};

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete resultVec;
}

// ======================================================================
// INT input tests (OMNI_INT is equivalent to OMNI_DATE32)
// ======================================================================

TEST(LastDayTest, IntBasic) {
    std::vector<int32_t> intValues = {
        LastDayTestHelper::DateToDays(2024, 3, 15),
        LastDayTestHelper::DateToDays(2024, 2, 1),
        LastDayTestHelper::DateToDays(2023, 2, 10),
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2024, 3, 31),
        LastDayTestHelper::DateToDays(2024, 2, 29),   // 2024 is leap year
        LastDayTestHelper::DateToDays(2023, 2, 28),   // 2023 is non-leap year
    };

    BaseVector* inputVec = LastDayTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_INT, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(LastDayTest, IntWithNull) {
    std::vector<int32_t> intValues = {
        LastDayTestHelper::DateToDays(2024, 1, 15),
        LastDayTestHelper::DateToDays(2024, 6, 20),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_INT, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(1), LastDayTestHelper::DateToDays(2024, 6, 30));

    delete resultVec;
}

// ======================================================================
// TIMESTAMP input tests
// ======================================================================

TEST(LastDayTest, TimestampBasic) {
    std::vector<int64_t> tsValues = {
        LastDayTestHelper::DateToMicros(2015, 3, 27),
        LastDayTestHelper::DateToMicros(2016, 2, 7),
        LastDayTestHelper::DateToMicros(2024, 12, 5),
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2015, 3, 31),
        LastDayTestHelper::DateToDays(2016, 2, 29),
        LastDayTestHelper::DateToDays(2024, 12, 31),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateTimestampVector(tsValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_TIMESTAMP, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(LastDayTest, TimestampWithNull) {
    std::vector<int64_t> tsValues = {
        LastDayTestHelper::DateToMicros(2024, 1, 15),
        LastDayTestHelper::DateToMicros(2024, 6, 20),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateTimestampVector(tsValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), LastDayTestHelper::DateToDays(2024, 1, 31));

    delete resultVec;
}

// ======================================================================
// LONG input tests (OMNI_LONG is equivalent to OMNI_TIMESTAMP)
// ======================================================================

TEST(LastDayTest, LongAsTimestamp) {
    std::vector<int64_t> longValues = {
        LastDayTestHelper::DateToMicros(2015, 2, 28),
        LastDayTestHelper::DateToMicros(2016, 2, 7),
        LastDayTestHelper::DateToMicros(1970, 1, 1),
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2015, 2, 28),   // non-leap Feb
        LastDayTestHelper::DateToDays(2016, 2, 29),   // leap Feb
        LastDayTestHelper::DateToDays(1970, 1, 31),   // Jan epoch year
    };

    BaseVector* inputVec = LastDayTestHelper::CreateLongVector(longValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_LONG, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, longValues.size());

    delete resultVec;
}

TEST(LastDayTest, LongWithNull) {
    std::vector<int64_t> longValues = {
        LastDayTestHelper::DateToMicros(2024, 4, 15),
        LastDayTestHelper::DateToMicros(2024, 9, 20),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateLongVector(longValues);
    inputVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_LONG, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(1), LastDayTestHelper::DateToDays(2024, 9, 30));

    delete resultVec;
}

// ======================================================================
// Edge cases and boundary tests
// ======================================================================

// Test: Epoch date (1970-01-01)
TEST(LastDayTest, EpochDate) {
    std::vector<int32_t> dateValues = {0};
    std::vector<int32_t> expected = {LastDayTestHelper::DateToDays(1970, 1, 31)};

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Pre-epoch date (1969-12-31)
TEST(LastDayTest, PreEpochDate) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(1969, 12, 31),
        LastDayTestHelper::DateToDays(1969, 1, 1),
    };
    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(1969, 12, 31),
        LastDayTestHelper::DateToDays(1969, 1, 31),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Century leap year rules (2000 is leap, 1900 is not, 2100 is not)
TEST(LastDayTest, CenturyLeapYears) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2000, 2, 15),   // 2000 is a leap year (divisible by 400)
        LastDayTestHelper::DateToDays(1900, 2, 15),   // 1900 is NOT a leap year (divisible by 100 but not 400)
        LastDayTestHelper::DateToDays(2100, 2, 15),   // 2100 is NOT a leap year
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2000, 2, 29),
        LastDayTestHelper::DateToDays(1900, 2, 28),
        LastDayTestHelper::DateToDays(2100, 2, 28),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Last day of each month for various dates
TEST(LastDayTest, AllMonthEndDays) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2023, 1, 1),
        LastDayTestHelper::DateToDays(2023, 2, 1),
        LastDayTestHelper::DateToDays(2023, 3, 1),
        LastDayTestHelper::DateToDays(2023, 4, 1),
        LastDayTestHelper::DateToDays(2023, 5, 1),
        LastDayTestHelper::DateToDays(2023, 6, 1),
        LastDayTestHelper::DateToDays(2023, 7, 1),
        LastDayTestHelper::DateToDays(2023, 8, 1),
        LastDayTestHelper::DateToDays(2023, 9, 1),
        LastDayTestHelper::DateToDays(2023, 10, 1),
        LastDayTestHelper::DateToDays(2023, 11, 1),
        LastDayTestHelper::DateToDays(2023, 12, 1),
    };

    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2023, 1, 31),
        LastDayTestHelper::DateToDays(2023, 2, 28),
        LastDayTestHelper::DateToDays(2023, 3, 31),
        LastDayTestHelper::DateToDays(2023, 4, 30),
        LastDayTestHelper::DateToDays(2023, 5, 31),
        LastDayTestHelper::DateToDays(2023, 6, 30),
        LastDayTestHelper::DateToDays(2023, 7, 31),
        LastDayTestHelper::DateToDays(2023, 8, 31),
        LastDayTestHelper::DateToDays(2023, 9, 30),
        LastDayTestHelper::DateToDays(2023, 10, 31),
        LastDayTestHelper::DateToDays(2023, 11, 30),
        LastDayTestHelper::DateToDays(2023, 12, 31),
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: When input is already the last day of the month
TEST(LastDayTest, InputIsLastDay) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2024, 1, 31),
        LastDayTestHelper::DateToDays(2024, 2, 29),
        LastDayTestHelper::DateToDays(2024, 3, 31),
        LastDayTestHelper::DateToDays(2024, 4, 30),
    };

    std::vector<int32_t> expected = dateValues;

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Single row
TEST(LastDayTest, SingleRow) {
    std::vector<int32_t> dateValues = {
        LastDayTestHelper::DateToDays(2024, 6, 15)
    };
    std::vector<int32_t> expected = {
        LastDayTestHelper::DateToDays(2024, 6, 30)
    };

    BaseVector* inputVec = LastDayTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_DATE32, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Timestamp at epoch (1970-01-01 00:00:00)
TEST(LastDayTest, TimestampEpoch) {
    std::vector<int64_t> tsValues = {0};
    std::vector<int32_t> expected = {LastDayTestHelper::DateToDays(1970, 1, 31)};

    BaseVector* inputVec = LastDayTestHelper::CreateTimestampVector(tsValues);
    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_TIMESTAMP, resultVec);
    LastDayTestHelper::ValidateResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

// Test: Timestamp all NULL
TEST(LastDayTest, TimestampAllNull) {
    std::vector<int64_t> tsValues = {0, 0};

    BaseVector* inputVec = LastDayTestHelper::CreateTimestampVector(tsValues);
    inputVec->SetNull(0);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    LastDayTestHelper::ExecuteLastDay(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    delete resultVec;
}
