/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Ceil_time function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Ceil.h"
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

class CeilTimeTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const ceil_time_test_env = ::testing::AddGlobalTestEnvironment(new CeilTimeTestEnvironment);

class CeilTimeFunctionTestHelper {
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
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
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
    
    static void ExecuteCeilTime(BaseVector* valueVec, BaseVector* formatVec, BaseVector*& result, DataTypeId valueTypeId) {
        DataTypeId outputTypeId = valueTypeId;
        auto signature = std::make_shared<FunctionSignature>("flink_ceil_time", 
            std::vector<DataTypeId>{valueTypeId, OMNI_VARCHAR}, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Ceil_time function not found for signature";
        
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context;
        context.SetResultRowSize(valueVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(valueVec);
        args.push(formatVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Ceil_time function threw an exception";
    }
    
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    static int64_t DateTimeToMillis(int year, int month, int day, int hour, int minute, int second, int millis) {
        int32_t days = DateToDays(year, month, day);
        return static_cast<int64_t>(days) * 86400LL * 1000LL
             + static_cast<int64_t>(hour) * 3600LL * 1000LL
             + static_cast<int64_t>(minute) * 60LL * 1000LL
             + static_cast<int64_t>(second) * 1000LL
             + static_cast<int64_t>(millis);
    }
};

TEST(CeilTimeTest, CeilDate32ToYear) {
    std::cout << "=== Test: Ceil DATE32 to YEAR ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2026, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "YEAR", "YEAR"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32ToMonth) {
    std::cout << "=== Test: Ceil DATE32 to MONTH ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 7, 1),
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2025, 4, 1)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32ToQuarter) {
    std::cout << "=== Test: Ceil DATE32 to QUARTER ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 7, 1),
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2025, 4, 1)
    };
    
    std::vector<std::string> formatValues = {"QUARTER", "QUARTER", "QUARTER"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32ToWeek) {
    std::cout << "=== Test: Ceil DATE32 to WEEK ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 16),
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 17)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 16),
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 16),
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 23)
    };
    
    std::vector<std::string> formatValues = {"WEEK", "WEEK", "WEEK"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32ToDay) {
    std::cout << "=== Test: Ceil DATE32 to DAY ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"DAY", "DAY", "DAY"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToHour) {
    std::cout << "=== Test: Ceil TIMESTAMP to HOUR ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 15, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToMinute) {
    std::cout << "=== Test: Ceil TIMESTAMP to MINUTE ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 31, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"MINUTE", "MINUTE", "MINUTE"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToSecond) {
    std::cout << "=== Test: Ceil TIMESTAMP to SECOND ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 46, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"SECOND", "SECOND", "SECOND"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToDay) {
    std::cout << "=== Test: Ceil TIMESTAMP to DAY ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"DAY", "DAY", "DAY"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToWeek) {
    std::cout << "=== Test: Ceil TIMESTAMP to WEEK ===" << std::endl;

    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 1, 1, 1, 1),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 10, 20, 30, 456)
    };

    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 0, 0, 0, 0)
    };

    std::vector<std::string> formatValues = {"WEEK", "WEEK", "WEEK"};

    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;

    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToMonth) {
    std::cout << "=== Test: Ceil TIMESTAMP to MONTH ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 1, 1, 1, 1, 1),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 7, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToQuarter) {
    std::cout << "=== Test: Ceil TIMESTAMP to QUARTER ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 1, 1, 1, 1, 1, 1),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 7, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 1, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"QUARTER", "QUARTER", "QUARTER"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampToYear) {
    std::cout << "=== Test: Ceil TIMESTAMP to YEAR ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 1, 1, 1, 1, 1, 1),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 1, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "YEAR", "YEAR"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilWithNullDate) {
    std::cout << "=== Test: Ceil with NULL date values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    dateVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(CeilTimeTest, CeilWithNullFormat) {
    std::cout << "=== Test: Ceil with NULL format values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31),
        CeilTimeFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    formatVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (format is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampWithNullValue) {
    std::cout << "=== Test: Ceil TIMESTAMP with NULL value ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 10, 20, 30, 456),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    tsVec->SetNull(0);
    tsVec->SetNull(2);
    
    BaseVector* resultVec = nullptr;
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampWithNullFormat) {
    std::cout << "=== Test: Ceil TIMESTAMP with NULL format ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 10, 20, 30, 456),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 12, 31, 23, 59, 59, 999)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    
    formatVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (format is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(CeilTimeTest, CeilWithInvalidFormat) {
    std::cout << "=== Test: Ceil with invalid format ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 15),
        CeilTimeFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    
    std::vector<std::string> formatValues = {"INVALID", "BADFORMAT"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (invalid format)";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (invalid format)";

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32BoundaryEpoch) {
    std::cout << "=== Test: Ceil DATE32 boundary - epoch ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(1970, 1, 1)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(1970, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(1970, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "DAY"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32BoundaryLeapYear) {
    std::cout << "=== Test: Ceil DATE32 boundary - leap year ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 2, 29),
        CeilTimeFunctionTestHelper::DateToDays(2024, 2, 29),
        CeilTimeFunctionTestHelper::DateToDays(2024, 2, 29)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2025, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2024, 3, 1),
        CeilTimeFunctionTestHelper::DateToDays(2024, 2, 29)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "DAY"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilDate32AlreadyOnBoundary) {
    std::cout << "=== Test: Ceil DATE32 already on boundary ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 1),
        CeilTimeFunctionTestHelper::DateToDays(2024, 4, 1)
    };
    
    std::vector<int32_t> expected = {
        CeilTimeFunctionTestHelper::DateToDays(2024, 1, 1),
        CeilTimeFunctionTestHelper::DateToDays(2024, 6, 1),
        CeilTimeFunctionTestHelper::DateToDays(2024, 4, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "QUARTER"};
    
    BaseVector* dateVec = CeilTimeFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(dateVec, formatVec, resultVec, OMNI_INT);
    CeilTimeFunctionTestHelper::ValidateDate32Result(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampBoundaryExactHour) {
    std::cout << "=== Test: Ceil TIMESTAMP boundary - exact hour ===" << std::endl;
    
    std::vector<int64_t> tsValues = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 0, 0, 1),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 59, 59, 999)
    };
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 15, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 15, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"HOUR", "HOUR", "HOUR"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}

TEST(CeilTimeTest, CeilTimestampWithDifferentFormats) {
    std::cout << "=== Test: Ceil TIMESTAMP with different format strings ===" << std::endl;
    
    int64_t ts = CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 15, 14, 30, 45, 123);
    std::vector<int64_t> tsValues = {ts, ts, ts};
    
    std::vector<int64_t> expected = {
        CeilTimeFunctionTestHelper::DateTimeToMillis(2025, 1, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 7, 1, 0, 0, 0, 0),
        CeilTimeFunctionTestHelper::DateTimeToMillis(2024, 6, 16, 0, 0, 0, 0)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "MONTH", "DAY"};
    
    BaseVector* tsVec = CeilTimeFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* formatVec = CeilTimeFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    CeilTimeFunctionTestHelper::ExecuteCeilTime(tsVec, formatVec, resultVec, OMNI_LONG);
    CeilTimeFunctionTestHelper::ValidateLongResult(resultVec, expected, tsValues.size());

    delete resultVec;
}
