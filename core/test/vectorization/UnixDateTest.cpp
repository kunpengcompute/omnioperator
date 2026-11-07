/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixDate function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/UnixDate.h"
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

class UnixDateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const unix_date_test_env =
    ::testing::AddGlobalTestEnvironment(new UnixDateTestEnvironment);

class UnixDateTestHelper {
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

    static void ExecuteUnixDate(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("unix_date",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "unix_date function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "unix_date function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: unix_date from DATE32 - basic cases matching Velox test
TEST(UnixDateTest, Date32Basic) {
    std::cout << "=== Test: unix_date from DATE32 - basic cases ===" << std::endl;

    std::vector<int32_t> dateValues = {
        UnixDateTestHelper::DateToDays(1970, 1, 1),
        UnixDateTestHelper::DateToDays(1970, 1, 2),
        UnixDateTestHelper::DateToDays(1969, 12, 31)
    };
    std::vector<int32_t> expected = {0, 1, -1};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date from INT - basic cases (OMNI_INT is equivalent to OMNI_DATE32)
TEST(UnixDateTest, IntBasic) {
    std::cout << "=== Test: unix_date from INT - basic cases ===" << std::endl;

    std::vector<int32_t> intValues = {
        UnixDateTestHelper::DateToDays(1970, 1, 1),
        UnixDateTestHelper::DateToDays(1970, 1, 2),
        UnixDateTestHelper::DateToDays(1969, 12, 31)
    };
    std::vector<int32_t> expected = {0, 1, -1};

    BaseVector* inputVec = UnixDateTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_INT, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with month boundary dates (Velox test cases)
TEST(UnixDateTest, MonthBoundary) {
    std::cout << "=== Test: unix_date month boundary dates ===" << std::endl;

    std::vector<int32_t> dateValues = {
        UnixDateTestHelper::DateToDays(1970, 2, 1),
        UnixDateTestHelper::DateToDays(1971, 1, 31),
        UnixDateTestHelper::DateToDays(1971, 1, 1)
    };
    std::vector<int32_t> expected = {31, 395, 365};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with leap year dates (Velox test cases)
TEST(UnixDateTest, LeapYear) {
    std::cout << "=== Test: unix_date leap year dates ===" << std::endl;

    std::vector<int32_t> dateValues = {
        UnixDateTestHelper::DateToDays(1972, 2, 29),
        UnixDateTestHelper::DateToDays(1971, 3, 1)
    };
    // 1972-02-29: 365 (1970) + 365 (1971) + 31 (Jan 1972) + 29 (Feb 1-29) = 790
    // 1971-03-01: 365 (1970) + 31 (Jan 1971) + 28 (Feb 1971) + 1 (Mar 1) = 425
    std::vector<int32_t> expected = {365 + 365 + 30 + 29, 365 + 30 + 28 + 1};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with NULL values (DATE32 input)
TEST(UnixDateTest, Date32WithNull) {
    std::cout << "=== Test: unix_date with NULL values (DATE32) ===" << std::endl;

    std::vector<int32_t> dateValues = {
        UnixDateTestHelper::DateToDays(1970, 1, 1),
        UnixDateTestHelper::DateToDays(2024, 6, 15),
        UnixDateTestHelper::DateToDays(1969, 12, 31)
    };

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 0) << "Row 0 should be 0 (epoch)";
    EXPECT_EQ(resultVecTyped->GetValue(2), -1) << "Row 2 should be -1 (day before epoch)";

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with NULL values (INT input)
TEST(UnixDateTest, IntWithNull) {
    std::cout << "=== Test: unix_date with NULL values (INT) ===" << std::endl;

    std::vector<int32_t> intValues = {
        UnixDateTestHelper::DateToDays(1970, 1, 1),
        UnixDateTestHelper::DateToDays(2024, 6, 15),
        UnixDateTestHelper::DateToDays(1969, 12, 31)
    };

    BaseVector* inputVec = UnixDateTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 0) << "Row 0 should be 0 (epoch)";
    EXPECT_EQ(resultVecTyped->GetValue(2), -1) << "Row 2 should be -1 (day before epoch)";

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with all NULL input
TEST(UnixDateTest, AllNullInput) {
    std::cout << "=== Test: unix_date with all NULL input ===" << std::endl;

    std::vector<int32_t> dateValues = {0, 0, 0};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with edge values (int32_t max/min - matches Velox kMax/kMin)
TEST(UnixDateTest, EdgeValues) {
    std::cout << "=== Test: unix_date with edge values ===" << std::endl;

    const int32_t kMax = std::numeric_limits<int32_t>::max();
    const int32_t kMin = std::numeric_limits<int32_t>::min();

    std::vector<int32_t> dateValues = {kMax, kMin, 0};
    std::vector<int32_t> expected = {kMax, kMin, 0};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with epoch date (0 days since epoch = 1970-01-01)
TEST(UnixDateTest, EpochDate) {
    std::cout << "=== Test: unix_date at exact epoch ===" << std::endl;

    std::vector<int32_t> dateValues = {0};
    std::vector<int32_t> expected = {0};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with negative days (dates before 1970-01-01)
TEST(UnixDateTest, NegativeDays) {
    std::cout << "=== Test: unix_date with negative days (pre-epoch) ===" << std::endl;

    std::vector<int32_t> dateValues = {-1, -365, -730};
    std::vector<int32_t> expected = {-1, -365, -730};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date with different years
TEST(UnixDateTest, DifferentYears) {
    std::cout << "=== Test: unix_date with different years ===" << std::endl;

    std::vector<int32_t> dateValues = {
        UnixDateTestHelper::DateToDays(2000, 1, 1),
        UnixDateTestHelper::DateToDays(2024, 1, 29),
        UnixDateTestHelper::DateToDays(1999, 12, 25),
        UnixDateTestHelper::DateToDays(2020, 2, 29)
    };

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultVecTyped, nullptr);

    for (size_t i = 0; i < dateValues.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(resultVecTyped->GetValue(i), dateValues[i])
            << "Row " << i << ": unix_date should return the input days since epoch directly";
    }

    delete inputVec;
    delete resultVec;
}

// Test: unix_date single row
TEST(UnixDateTest, SingleRow) {
    std::cout << "=== Test: unix_date single row ===" << std::endl;

    std::vector<int32_t> dateValues = {
        UnixDateTestHelper::DateToDays(2024, 3, 15)
    };
    std::vector<int32_t> expected = {dateValues[0]};

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}

// Test: unix_date identity property - input days should equal output
TEST(UnixDateTest, IdentityProperty) {
    std::cout << "=== Test: unix_date identity property ===" << std::endl;

    std::vector<int32_t> dateValues = {0, 1, -1, 100, -100, 19000, -19000};
    std::vector<int32_t> expected = dateValues;

    BaseVector* inputVec = UnixDateTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    UnixDateTestHelper::ExecuteUnixDate(inputVec, OMNI_DATE32, resultVec);
    UnixDateTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete inputVec;
    delete resultVec;
}
