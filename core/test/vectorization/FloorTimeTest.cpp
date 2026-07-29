/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Floor_time function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Floor.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"
#include "type/Timestamp.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class FloorTimeTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const floor_time_test_env = ::testing::AddGlobalTestEnvironment(new FloorTimeTestEnvironment);

class FloorTimeFunctionTestHelper {
public:
    static void ValidateDate32Result(BaseVector* result, const std::vector<int32_t>& expected, int rowSize) {
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

    static void ValidateLongResult(BaseVector* result, const std::vector<int64_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            int64_t actualValue = resultVec->GetValue(i);
            int64_t expectedValue = expected[i];
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

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
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
    
    static void ExecuteFloorTime(BaseVector* valueVec, BaseVector* formatVec, BaseVector*& result, DataTypeId valueTypeId) {
        DataTypeId outputTypeId = valueTypeId;
        auto signature = std::make_shared<FunctionSignature>("floor_time", 
            std::vector<DataTypeId>{valueTypeId, OMNI_VARCHAR}, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Floor_time function not found for signature";
        
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context;
        context.SetResultRowSize(valueVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(valueVec);
        args.push(formatVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Floor_time function threw an exception";
    }
    
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    static int64_t DateTimeToMicros(int year, int month, int day, int hour, int minute, int second, int micros) {
        int32_t days = DateToDays(year, month, day);
        return static_cast<int64_t>(days) * 86400LL * 1000000LL
             + static_cast<int64_t>(hour) * 3600LL * 1000000LL
             + static_cast<int64_t>(minute) * 60LL * 1000000LL
             + static_cast<int64_t>(second) * 1000000LL
             + static_cast<int64_t>(micros);
    }
};

TEST(FloorTimeTest, FloorDate32ToYear) {
    std::cout << "=== Test: Floor DATE32 to YEAR ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "YEAR", "YEAR"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32ToMonth) {
    std::cout << "=== Test: Floor DATE32 to MONTH ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 1),
        FloorTimeFunctionTestHelper::DateToDays(2025, 3, 1)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32ToQuarter) {
    std::cout << "=== Test: Floor DATE32 to QUARTER ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 4, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 10, 1),
        FloorTimeFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"QUARTER", "QUARTER", "QUARTER"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32ToWeek) {
    std::cout << "=== Test: Floor DATE32 to WEEK ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 17),
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 20)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 9),
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 16),
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 16)
    };
    
    std::vector<std::string> formatValues = {"WEEK", "WEEK", "WEEK"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32ToDay) {
    std::cout << "=== Test: Floor DATE32 to DAY ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"DAY", "DAY", "DAY"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToHour) {
    std::cout << "=== Test: Floor TIMESTAMP to HOUR ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToMinute) {
    std::cout << "=== Test: Floor TIMESTAMP to MINUTE ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"MINUTE", "MINUTE", "MINUTE"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToSecond) {
    std::cout << "=== Test: Floor TIMESTAMP to SECOND ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 0)
    };
    
    std::vector<std::string> formatValues = {"SECOND", "SECOND", "SECOND"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToDay) {
    std::cout << "=== Test: Floor TIMESTAMP to DAY ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"DAY", "DAY", "DAY"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToMonth) {
    std::cout << "=== Test: Floor TIMESTAMP to MONTH ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToQuarter) {
    std::cout << "=== Test: Floor TIMESTAMP to QUARTER ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 4, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 10, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"QUARTER", "QUARTER", "QUARTER"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampToYear) {
    std::cout << "=== Test: Floor TIMESTAMP to YEAR ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "YEAR", "YEAR"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorWithNullDate) {
    std::cout << "=== Test: Floor with NULL date values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    dateVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(FloorTimeTest, FloorWithNullFormat) {
    std::cout << "=== Test: Floor with NULL format values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        FloorTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    formatVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (format is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampWithNullValue) {
    std::cout << "=== Test: Floor TIMESTAMP with NULL value ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 10, 20, 30, 456),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    tsVec->SetNull(0);
    tsVec->SetNull(2);
    
    BaseVector* resultVec = nullptr;
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete resultVec;
}

TEST(FloorTimeTest, FloorWithInvalidFormat) {
    std::cout << "=== Test: Floor with invalid format ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        FloorTimeFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    
    std::vector<std::string> formatValues = {"INVALID", "BADFORMAT"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (invalid format)";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (invalid format)";

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32BoundaryEpoch) {
    std::cout << "=== Test: Floor DATE32 boundary - epoch ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(1970, 1, 1)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(1970, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "DAY"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32BoundaryLeapYear) {
    std::cout << "=== Test: Floor DATE32 boundary - leap year ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 2, 29),
        FloorTimeFunctionTestHelper::DateToDays(2024, 2, 29),
        FloorTimeFunctionTestHelper::DateToDays(2024, 2, 29)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 2, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 2, 29)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "DAY"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorDate32AlreadyOnBoundary) {
    std::cout << "=== Test: Floor DATE32 already on boundary ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 4, 1)
    };
    
    std::vector<int32_t> expected = {
        FloorTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 6, 1),
        FloorTimeFunctionTestHelper::DateToDays(2024, 4, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "QUARTER"};
    
    BaseVector* dateVec = FloorTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(dateVec, formatVec, resultVec, OMNI_DATE32);
    FloorTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampBoundaryExactHour) {
    std::cout << "=== Test: Floor TIMESTAMP boundary - exact hour ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 1),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(FloorTimeTest, FloorTimestampWithDifferentFormats) {
    std::cout << "=== Test: Floor TIMESTAMP with different format strings ===" << std::endl;
    
    int64_t ts = FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 14, 30, 45, 123);
    std::vector<int64_t> tsValues = {ts, ts, ts};
    
    std::vector<int64_t> expected = {
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 1, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 1, 0, 0, 0, 0),
        FloorTimeFunctionTestHelper::DateTimeToMicros(2024, 6, 15, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "DAY"};
    
    BaseVector* tsVec = FloorTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = FloorTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    FloorTimeFunctionTestHelper::ExecuteFloorTime(tsVec, formatVec, resultVec, OMNI_LONG);
    FloorTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}
