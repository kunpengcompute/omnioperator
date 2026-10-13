/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: YearOfWeek function unit tests
 */

#include <gtest/gtest.h>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/YearOfWeek.h"
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

class YearOfWeekTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const yearofweek_test_env =
    ::testing::AddGlobalTestEnvironment(new YearOfWeekTestEnvironment);

class YearOfWeekTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, size_t rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (size_t i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            EXPECT_EQ(actualValue, expected[i]) << "Row " << i << " value mismatch";
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

    static void ExecuteYearOfWeek(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("year_of_week",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "year_of_week function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "year_of_week function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Basic test: dates where ISO year equals calendar year
TEST(YearOfWeekTest, Date32BasicSameYear) {
    std::vector<int32_t> dateValues = {
        YearOfWeekTestHelper::DateToDays(2024, 6, 15),
        YearOfWeekTestHelper::DateToDays(2023, 3, 10),
        YearOfWeekTestHelper::DateToDays(2025, 9, 20)
    };
    std::vector<int32_t> expected = {2024, 2023, 2025};

    BaseVector* inputVec = YearOfWeekTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_DATE32, resultVec);
    YearOfWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Dec 31 that belongs to next year's ISO week 1 (leap year ending on Thursday)
TEST(YearOfWeekTest, DecemberBelongsToNextYear) {
    // 2020-12-31 is a Thursday; ISO week 53 of 2020
    // 2015-12-31 is a Thursday; ISO week 53 of 2015
    // But 2021-01-01 is a Friday, so Dec 31 2020 is still 2020
    // 2009-12-31 is a Thursday -> ISO week 53 of 2009
    std::vector<int32_t> dateValues = {
        YearOfWeekTestHelper::DateToDays(2020, 12, 31),
        YearOfWeekTestHelper::DateToDays(2015, 12, 31)
    };
    std::vector<int32_t> expected = {2020, 2015};

    BaseVector* inputVec = YearOfWeekTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_DATE32, resultVec);
    YearOfWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Jan 1 that belongs to previous year's last ISO week
TEST(YearOfWeekTest, JanuaryBelongsToPreviousYear) {
    // 2021-01-01 is a Friday -> ISO week 53 of 2020 (belongs to 2020)
    // 2005-01-01 is a Saturday -> ISO week 53 of 2004 (belongs to 2004)
    std::vector<int32_t> dateValues = {
        YearOfWeekTestHelper::DateToDays(2021, 1, 1),
        YearOfWeekTestHelper::DateToDays(2005, 1, 1)
    };
    std::vector<int32_t> expected = {2020, 2004};

    BaseVector* inputVec = YearOfWeekTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_DATE32, resultVec);
    YearOfWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: INT input type
TEST(YearOfWeekTest, IntInput) {
    std::vector<int32_t> intValues = {
        YearOfWeekTestHelper::DateToDays(2024, 6, 15),
        YearOfWeekTestHelper::DateToDays(2021, 1, 1)
    };
    std::vector<int32_t> expected = {2024, 2020};

    BaseVector* inputVec = YearOfWeekTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_INT, resultVec);
    YearOfWeekTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

// Test: NULL values
TEST(YearOfWeekTest, WithNullValues) {
    std::vector<int32_t> dateValues = {
        YearOfWeekTestHelper::DateToDays(2024, 6, 15),
        YearOfWeekTestHelper::DateToDays(2023, 3, 10),
        YearOfWeekTestHelper::DateToDays(2025, 9, 20)
    };

    BaseVector* inputVec = YearOfWeekTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2024);
    EXPECT_EQ(resultVecTyped->GetValue(2), 2025);

    delete resultVec;
}

// Test: all NULLs
TEST(YearOfWeekTest, AllNulls) {
    std::vector<int32_t> dateValues = {0, 0, 0};

    BaseVector* inputVec = YearOfWeekTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_DATE32, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    EXPECT_TRUE(resultVec->IsNull(2));

    delete resultVec;
}

// Test: epoch date (1970-01-01, Thursday -> ISO week 1 of 1970)
TEST(YearOfWeekTest, EpochDate) {
    std::vector<int32_t> dateValues = {
        YearOfWeekTestHelper::DateToDays(1970, 1, 1)
    };
    std::vector<int32_t> expected = {1970};

    BaseVector* inputVec = YearOfWeekTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    YearOfWeekTestHelper::ExecuteYearOfWeek(inputVec, OMNI_DATE32, resultVec);
    YearOfWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}
