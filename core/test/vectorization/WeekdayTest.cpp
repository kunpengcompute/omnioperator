/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: weekday function unit tests (0=Monday .. 6=Sunday, Spark/Velox convention)
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Weekday.h"
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

class WeekdayTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment *const weekday_test_env =
    ::testing::AddGlobalTestEnvironment(new WeekdayTestEnvironment);

class WeekdayTestHelper {
public:
    static void ValidateResult(BaseVector *result, const std::vector<int32_t> &expected, size_t rowSize) {
        auto *resultVec = dynamic_cast<Vector<int32_t> *>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (size_t i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " weekday mismatch";
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

    static void ExecuteWeekday(BaseVector *inputVec, DataTypeId inputTypeId, BaseVector *&result) {
        auto signature = std::make_shared<FunctionSignature>("weekday",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "weekday function not found";
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

    static int64_t DateToMicros(int year, int month, int day) {
        int32_t d = DateToDays(year, month, day);
        return static_cast<int64_t>(d) * 86400LL * 1000000LL;
    }
};

// Spark/Velox: 0=Monday, 1=Tuesday, ..., 6=Sunday
// 1970-01-01 = Thursday = 3; 2023-08-21 = Monday = 0; 2023-08-20 = Sunday = 6
TEST(WeekdayTest, Date32Basic) {
    std::vector<int32_t> dateValues = {
        WeekdayTestHelper::DateToDays(1970, 1, 1),   // Thursday -> 3
        WeekdayTestHelper::DateToDays(2023, 8, 21), // Monday -> 0
        WeekdayTestHelper::DateToDays(2023, 8, 22), // Tuesday -> 1
        WeekdayTestHelper::DateToDays(2023, 8, 23), // Wednesday -> 2
        WeekdayTestHelper::DateToDays(2023, 8, 24), // Thursday -> 3
        WeekdayTestHelper::DateToDays(2023, 8, 25), // Friday -> 4
        WeekdayTestHelper::DateToDays(2023, 8, 26), // Saturday -> 5
        WeekdayTestHelper::DateToDays(2023, 8, 27), // Sunday -> 6
        WeekdayTestHelper::DateToDays(2023, 8, 20), // Sunday -> 6
        WeekdayTestHelper::DateToDays(2009, 7, 30), // Thursday -> 3
        WeekdayTestHelper::DateToDays(2017, 5, 27), // Saturday -> 5
        WeekdayTestHelper::DateToDays(2015, 4, 8),  // Wednesday -> 2
        WeekdayTestHelper::DateToDays(2013, 11, 8), // Friday -> 4
    };
    std::vector<int32_t> expected = {3, 0, 1, 2, 3, 4, 5, 6, 6, 3, 5, 2, 4};

    BaseVector *inputVec = WeekdayTestHelper::CreateDate32Vector(dateValues);
    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_DATE32, resultVec);
    WeekdayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Velox test alignment: epoch 0 -> Thursday -> 3; -1 day -> Wednesday -> 2; -40 -> Friday -> 5
TEST(WeekdayTest, Date32EpochAndNegative) {
    std::vector<int32_t> dateValues = {
        0,    // 1970-01-01 Thursday -> 3
        -1,   // 1969-12-31 Wednesday -> 2
        -40,  // 1969-11-22 Saturday -> 5 (Velox expects 5 for -40)
    };
    std::vector<int32_t> expected = {3, 2, 5};

    BaseVector *inputVec = WeekdayTestHelper::CreateDate32Vector(dateValues);
    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_DATE32, resultVec);
    WeekdayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(WeekdayTest, Date32YearBoundary) {
    std::vector<int32_t> dateValues = {
        WeekdayTestHelper::DateToDays(2024, 1, 1),   // Monday -> 0
        WeekdayTestHelper::DateToDays(2024, 12, 31), // Tuesday -> 1
        WeekdayTestHelper::DateToDays(2011, 5, 6),   // Friday -> 4
        WeekdayTestHelper::DateToDays(1582, 10, 15), // Friday -> 4 (Velox test)
    };
    std::vector<int32_t> expected = {0, 1, 4, 4};

    BaseVector *inputVec = WeekdayTestHelper::CreateDate32Vector(dateValues);
    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_DATE32, resultVec);
    WeekdayTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(WeekdayTest, IntBasic) {
    std::vector<int32_t> intValues = {
        WeekdayTestHelper::DateToDays(1970, 1, 1),
        WeekdayTestHelper::DateToDays(2013, 4, 8),  // Monday -> 0
    };
    std::vector<int32_t> expected = {3, 0};

    BaseVector *inputVec = WeekdayTestHelper::CreateIntVector(intValues);
    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_INT, resultVec);
    WeekdayTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(WeekdayTest, TimestampBasic) {
    std::vector<int64_t> tsValues = {
        WeekdayTestHelper::DateToMicros(1970, 1, 1),
        WeekdayTestHelper::DateToMicros(2023, 8, 21),
        WeekdayTestHelper::DateToMicros(2005, 1, 1),  // Saturday -> 5
    };
    std::vector<int32_t> expected = {3, 0, 5};

    BaseVector *inputVec = WeekdayTestHelper::CreateTimestampVector(tsValues);
    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_TIMESTAMP, resultVec);
    WeekdayTestHelper::ValidateResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(WeekdayTest, LongAsTimestamp) {
    std::vector<int64_t> longValues = {
        WeekdayTestHelper::DateToMicros(1970, 1, 1),
        WeekdayTestHelper::DateToMicros(2021, 1, 1),  // Friday -> 4
    };
    std::vector<int32_t> expected = {3, 4};

    BaseVector *inputVec = WeekdayTestHelper::CreateLongVector(longValues);
    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_LONG, resultVec);
    WeekdayTestHelper::ValidateResult(resultVec, expected, longValues.size());

    delete resultVec;
}

TEST(WeekdayTest, Date32WithNull) {
    std::vector<int32_t> dateValues = {
        WeekdayTestHelper::DateToDays(2024, 1, 1),
        WeekdayTestHelper::DateToDays(2024, 6, 15),
        WeekdayTestHelper::DateToDays(2024, 12, 31),
    };
    BaseVector *inputVec = WeekdayTestHelper::CreateDate32Vector(dateValues);
    inputVec->SetNull(1);

    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));
    auto *typed = dynamic_cast<Vector<int32_t> *>(resultVec);
    EXPECT_EQ(typed->GetValue(0), 0);  // 2024-01-01 Monday
    EXPECT_EQ(typed->GetValue(2), 1);  // 2024-12-31 Tuesday

    delete resultVec;
}

TEST(WeekdayTest, TimestampWithNull) {
    std::vector<int64_t> tsValues = {
        WeekdayTestHelper::DateToMicros(2024, 1, 1),
        WeekdayTestHelper::DateToMicros(2024, 7, 1),
    };
    BaseVector *inputVec = WeekdayTestHelper::CreateTimestampVector(tsValues);
    inputVec->SetNull(1);

    BaseVector *resultVec = nullptr;
    WeekdayTestHelper::ExecuteWeekday(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    auto *typed = dynamic_cast<Vector<int32_t> *>(resultVec);
    EXPECT_EQ(typed->GetValue(0), 0);  // 2024-01-01 Monday

    delete resultVec;
}
