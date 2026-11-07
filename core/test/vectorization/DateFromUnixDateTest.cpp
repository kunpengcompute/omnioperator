/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateFromUnixDate function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DateFromUnixDate.h"
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

class DateFromUnixDateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const date_from_unix_date_test_env =
    ::testing::AddGlobalTestEnvironment(new DateFromUnixDateTestEnvironment);

class DateFromUnixDateTestHelper {
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
            std::cout << "Row " << i << ": Expected=" << expectedValue
                      << ", Actual=" << actualValue << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteDateFromUnixDate(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("date_from_unix_date",
            std::vector<DataTypeId>{inputTypeId}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "date_from_unix_date function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "date_from_unix_date function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: date_from_unix_date from INT - basic cases matching Velox test
TEST(DateFromUnixDateTest, IntBasic) {
    std::cout << "=== Test: date_from_unix_date from INT - basic cases ===" << std::endl;

    std::vector<int32_t> inputValues = {0, 1, -1};
    std::vector<int32_t> expected = {
        DateFromUnixDateTestHelper::DateToDays(1970, 1, 1),
        DateFromUnixDateTestHelper::DateToDays(1970, 1, 2),
        DateFromUnixDateTestHelper::DateToDays(1969, 12, 31)
    };

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date from DATE32 - basic cases (OMNI_DATE32 equivalent to OMNI_INT)
TEST(DateFromUnixDateTest, Date32Basic) {
    std::cout << "=== Test: date_from_unix_date from DATE32 - basic cases ===" << std::endl;

    std::vector<int32_t> inputValues = {0, 1, -1};
    std::vector<int32_t> expected = {
        DateFromUnixDateTestHelper::DateToDays(1970, 1, 1),
        DateFromUnixDateTestHelper::DateToDays(1970, 1, 2),
        DateFromUnixDateTestHelper::DateToDays(1969, 12, 31)
    };

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateDate32Vector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_DATE32, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with month boundary dates (matches Velox test cases)
TEST(DateFromUnixDateTest, MonthBoundary) {
    std::cout << "=== Test: date_from_unix_date month boundary dates ===" << std::endl;

    std::vector<int32_t> inputValues = {31, 395, 365};
    std::vector<int32_t> expected = {
        DateFromUnixDateTestHelper::DateToDays(1970, 2, 1),
        DateFromUnixDateTestHelper::DateToDays(1971, 1, 31),
        DateFromUnixDateTestHelper::DateToDays(1971, 1, 1)
    };

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with leap year dates (matches Velox test cases)
TEST(DateFromUnixDateTest, LeapYear) {
    std::cout << "=== Test: date_from_unix_date leap year dates ===" << std::endl;

    std::vector<int32_t> inputValues = {
        365 + 365 + 30 + 29,
        365 + 30 + 28 + 1
    };
    std::vector<int32_t> expected = {
        DateFromUnixDateTestHelper::DateToDays(1972, 2, 29),
        DateFromUnixDateTestHelper::DateToDays(1971, 3, 1)
    };

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with NULL values (INT input)
TEST(DateFromUnixDateTest, IntWithNull) {
    std::cout << "=== Test: date_from_unix_date with NULL values (INT) ===" << std::endl;

    std::vector<int32_t> inputValues = {0, 100, -1};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), DateFromUnixDateTestHelper::DateToDays(1970, 1, 1))
        << "Row 0 should be 1970-01-01";
    EXPECT_EQ(resultVecTyped->GetValue(2), DateFromUnixDateTestHelper::DateToDays(1969, 12, 31))
        << "Row 2 should be 1969-12-31";

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with NULL values (DATE32 input)
TEST(DateFromUnixDateTest, Date32WithNull) {
    std::cout << "=== Test: date_from_unix_date with NULL values (DATE32) ===" << std::endl;

    std::vector<int32_t> inputValues = {0, 100, -1};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateDate32Vector(inputValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), DateFromUnixDateTestHelper::DateToDays(1970, 1, 1))
        << "Row 0 should be 1970-01-01";
    EXPECT_EQ(resultVecTyped->GetValue(2), DateFromUnixDateTestHelper::DateToDays(1969, 12, 31))
        << "Row 2 should be 1969-12-31";

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with all NULL input
TEST(DateFromUnixDateTest, AllNullInput) {
    std::cout << "=== Test: date_from_unix_date with all NULL input ===" << std::endl;

    std::vector<int32_t> inputValues = {0, 0, 0};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with edge values (int32_t max/min - matches Velox kMax/kMin)
TEST(DateFromUnixDateTest, EdgeValues) {
    std::cout << "=== Test: date_from_unix_date with edge values ===" << std::endl;

    const int32_t kMax = std::numeric_limits<int32_t>::max();
    const int32_t kMin = std::numeric_limits<int32_t>::min();

    std::vector<int32_t> inputValues = {kMax, kMin, 0};
    std::vector<int32_t> expected = {kMax, kMin, 0};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date at exact epoch (0 = 1970-01-01)
TEST(DateFromUnixDateTest, EpochDate) {
    std::cout << "=== Test: date_from_unix_date at exact epoch ===" << std::endl;

    std::vector<int32_t> inputValues = {0};
    std::vector<int32_t> expected = {DateFromUnixDateTestHelper::DateToDays(1970, 1, 1)};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with negative days (dates before 1970-01-01)
TEST(DateFromUnixDateTest, NegativeDays) {
    std::cout << "=== Test: date_from_unix_date with negative days (pre-epoch) ===" << std::endl;

    std::vector<int32_t> inputValues = {-1, -365, -730};
    std::vector<int32_t> expected = {-1, -365, -730};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date with different year dates
TEST(DateFromUnixDateTest, DifferentYears) {
    std::cout << "=== Test: date_from_unix_date with different years ===" << std::endl;

    int32_t days2000 = DateFromUnixDateTestHelper::DateToDays(2000, 1, 1);
    int32_t days2024 = DateFromUnixDateTestHelper::DateToDays(2024, 1, 29);
    int32_t days1999 = DateFromUnixDateTestHelper::DateToDays(1999, 12, 25);
    int32_t days2020 = DateFromUnixDateTestHelper::DateToDays(2020, 2, 29);

    std::vector<int32_t> inputValues = {days2000, days2024, days1999, days2020};
    std::vector<int32_t> expected = inputValues;

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultVecTyped, nullptr);

    for (size_t i = 0; i < inputValues.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(resultVecTyped->GetValue(i), expected[i])
            << "Row " << i << ": date_from_unix_date should return the input days as DATE32";
    }

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date single row
TEST(DateFromUnixDateTest, SingleRow) {
    std::cout << "=== Test: date_from_unix_date single row ===" << std::endl;

    int32_t inputDay = DateFromUnixDateTestHelper::DateToDays(2024, 3, 15);
    std::vector<int32_t> inputValues = {inputDay};
    std::vector<int32_t> expected = {inputDay};

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date identity property - output days should equal input
TEST(DateFromUnixDateTest, IdentityProperty) {
    std::cout << "=== Test: date_from_unix_date identity property ===" << std::endl;

    std::vector<int32_t> inputValues = {0, 1, -1, 100, -100, 19000, -19000};
    std::vector<int32_t> expected = inputValues;

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);
    DateFromUnixDateTestHelper::ValidateResult(resultVec, expected, inputValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: date_from_unix_date is inverse of unix_date
TEST(DateFromUnixDateTest, InverseOfUnixDate) {
    std::cout << "=== Test: date_from_unix_date is inverse of unix_date ===" << std::endl;

    std::vector<int32_t> dateValues = {
        DateFromUnixDateTestHelper::DateToDays(1970, 1, 1),
        DateFromUnixDateTestHelper::DateToDays(2000, 6, 15),
        DateFromUnixDateTestHelper::DateToDays(1969, 1, 1),
        DateFromUnixDateTestHelper::DateToDays(2024, 12, 31)
    };

    // unix_date(date) returns int (days since epoch)
    auto unixDateSignature = std::make_shared<FunctionSignature>("unix_date",
        std::vector<DataTypeId>{OMNI_DATE32}, OMNI_INT);
    auto unixDateFunction = VectorFunction::Find(unixDateSignature);
    ASSERT_NE(unixDateFunction, nullptr) << "unix_date function not found";

    BaseVector* dateInputVec = DateFromUnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* unixDateResult = nullptr;

    auto intOutputType = std::make_shared<DataType>(OMNI_INT);
    ExecutionContext context1;
    context1.SetResultRowSize(dateInputVec->GetSize());
    std::stack<BaseVector*> args1;
    dateInputVec->SetIsField(true);  // caller-owned, do not let Apply delete
    args1.push(dateInputVec);
    unixDateFunction->Apply(args1, intOutputType, unixDateResult, &context1);

    // date_from_unix_date(int) returns date
    BaseVector* finalResult = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(unixDateResult, OMNI_INT, finalResult);

    auto* finalVec = dynamic_cast<Vector<int32_t>*>(finalResult);
    ASSERT_NE(finalVec, nullptr);

    for (size_t i = 0; i < dateValues.size(); ++i) {
        EXPECT_FALSE(finalResult->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(finalVec->GetValue(i), dateValues[i])
            << "Row " << i << ": date_from_unix_date(unix_date(d)) should equal d";
    }

    delete dateInputVec;
    delete unixDateResult;
    delete finalResult;
}

// Test: date_from_unix_date large batch
TEST(DateFromUnixDateTest, LargeBatch) {
    std::cout << "=== Test: date_from_unix_date large batch ===" << std::endl;

    const int batchSize = 1000;
    std::vector<int32_t> inputValues(batchSize);
    for (int i = 0; i < batchSize; ++i) {
        inputValues[i] = i - 500;
    }

    BaseVector* inputVec = DateFromUnixDateTestHelper::CreateIntVector(inputValues);
    BaseVector* resultVec = nullptr;
    DateFromUnixDateTestHelper::ExecuteDateFromUnixDate(inputVec, OMNI_INT, resultVec);

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultVecTyped, nullptr);

    for (int i = 0; i < batchSize; ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(resultVecTyped->GetValue(i), inputValues[i])
            << "Row " << i << ": value mismatch in large batch";
    }

    delete inputVec;
    delete resultVec;
}
