/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: FromUnixTime function unit tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <ctime>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FromUnixTime.h"
#include "vectorization/functions/DateTimeZoneConversion.h"
#include "vectorization/functions/SparkDateTimeFormat.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/Timestamp.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class FromUnixTimeTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const from_unix_time_test_env =
    ::testing::AddGlobalTestEnvironment(new FromUnixTimeTestEnvironment);

class FromUnixTimeTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<std::string>& expected,
        const std::vector<bool>& expectNull, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (expectNull[i]) {
                EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
                continue;
            }
            EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
            if (!result->IsNull(i)) {
                std::string_view actualValue = resultVec->GetValue(i);
                EXPECT_EQ(std::string(actualValue), expected[i])
                    << "Row " << i << " value mismatch: expected '" << expected[i]
                    << "', got '" << std::string(actualValue) << "'";
            }
        }
    }

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& microValues) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, microValues.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < microValues.size(); ++i) {
            typedVec->SetValue(i, microValues[i]);
        }
        return vec;
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, values.size());
        auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, std::string_view(values[i]));
        }
        return vec;
    }

    static BaseVector* CreateConstStringVector(const std::string& value, int32_t size) {
        return new ConstVector<std::string_view>(std::string_view(value), OMNI_VARCHAR, size);
    }

    static void ExecuteFromUnixTime(BaseVector* inputVec, BaseVector* formatVec,
        DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("from_unixtime",
            std::vector<DataTypeId>{inputTypeId, OMNI_VARCHAR}, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "from_unixtime function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(formatVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "from_unixtime function threw an exception";
    }

    static void ExecuteFromUnixTimeWithTz(BaseVector* inputVec, BaseVector* formatVec,
        BaseVector* tzVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("from_unixtime",
            std::vector<DataTypeId>{inputTypeId, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "from_unixtime function not found for 3-arg signature";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(formatVec);
        args.push(tzVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "from_unixtime function threw an exception";
    }

    static void ExecuteFromUnixTimeWithPolicy(BaseVector* inputVec, BaseVector* formatVec,
        BaseVector* tzVec, BaseVector* policyVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("from_unixtime",
            std::vector<DataTypeId>{inputTypeId, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "from_unixtime function not found for 4-arg signature";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(formatVec);
        args.push(tzVec);
        args.push(policyVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "from_unixtime function threw an exception";
    }
};

// Test: Basic from_unixtime with default format (2-arg, UTC)
TEST(FromUnixTimeTest, BasicWithDefaultFormat) {
    // 2-arg form uses UTC (gmtime_r) when no timezone is specified
    std::vector<int64_t> unixSeconds = {0, 100, 86400};
    std::vector<std::string> expected = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:01:40",
        "1970-01-02 00:00:00"
    };
    std::vector<bool> expectNull = {false, false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    // Note: Apply() already deletes inputVec and formatVec
    delete resultVec;
}

// Test: from_unixtime with date-only format
TEST(FromUnixTimeTest, DateOnlyFormat) {
    std::vector<int64_t> unixSeconds = {0, 100};
    std::vector<std::string> expected = {"1970-01-01", "1970-01-01"};
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: from_unixtime with year-only format
TEST(FromUnixTimeTest, YearOnlyFormat) {
    std::vector<int64_t> unixSeconds = {0, 3600};
    std::vector<std::string> expected = {"1970", "1970"};
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: from_unixtime with negative unix seconds (before epoch, UTC)
TEST(FromUnixTimeTest, NegativeUnixSeconds) {
    std::vector<int64_t> unixSeconds = {-59, -3600};
    std::vector<std::string> expected = {
        "1969-12-31 23:59:01",
        "1969-12-31 23:00:00"
    };
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: from_unixtime with hour-minute format
TEST(FromUnixTimeTest, HourMinuteFormat) {
    std::vector<int64_t> unixSeconds = {120};
    std::vector<std::string> expected = {"1970-01-01 00:02"};
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: NULL input handling
TEST(FromUnixTimeTest, NullInput) {
    std::vector<int64_t> unixSeconds = {0, 100, 86400};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    inputVec->SetNull(1); // Set row 1 to NULL

    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVec);
    EXPECT_EQ(std::string(resultVecTyped->GetValue(0)), "1970-01-01 00:00:00");
    EXPECT_EQ(std::string(resultVecTyped->GetValue(2)), "1970-01-02 00:00:00");

    delete resultVec;
}

// Test: NULL format handling
TEST(FromUnixTimeTest, NullFormat) {
    std::vector<int64_t> unixSeconds = {0};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    formatVec->SetNull(0); // Set format to NULL

    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL when format is NULL";

    delete resultVec;
}

// Test: An empty format produces an empty formatted string, which the current
// from_unixtime contract converts to NULL.
TEST(FromUnixTimeTest, EmptyFormatReturnsNull) {
    std::vector<int64_t> unixSeconds = {0};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector("", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0));

    delete resultVec;
}

// Test: All NULL input
TEST(FromUnixTimeTest, AllNullInput) {
    std::vector<int64_t> unixSeconds = {0, 0, 0};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete resultVec;
}

// Test: Variable format strings (non-constant)
TEST(FromUnixTimeTest, VariableFormat) {
    std::vector<int64_t> unixSeconds = {0, 100, 86400};
    std::vector<std::string> formats = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd",
        "yyyy"
    };
    std::vector<std::string> expected = {
        "1970-01-01 00:00:00",
        "1970-01-01",
        "1970"
    };
    std::vector<bool> expectNull = {false, false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateStringVector(formats);
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: Single row
TEST(FromUnixTimeTest, SingleRow) {
    std::vector<int64_t> unixSeconds = {0};
    std::vector<std::string> expected = {"1970-01-01 00:00:00"};
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", 1);
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, 1);

    delete resultVec;
}

// Test: Large timestamp values
TEST(FromUnixTimeTest, LargeTimestampValues) {
    // 2020-06-30 12:49:59 UTC = 1593521399 seconds
    // 2000-01-01 00:00:00 UTC = 946684800 seconds
    std::vector<int64_t> unixSeconds = {1593521399LL, 946684800LL};
    std::vector<std::string> expected = {
        "2020-06-30 12:49:59",
        "2000-01-01 00:00:00"
    };
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: Spark-compatible signed long overflow when seconds are converted to microseconds.
TEST(FromUnixTimeTest, HugeUnixSecondsMatchesSparkOverflow) {
    std::vector<int64_t> unixSeconds = {
        -3229830308112475844LL,
        2849223427288149722LL,
        3313326846908992582LL,
        -6310053917026126810LL,
        7654871665388838779LL
    };
    std::vector<std::string> expected = {
        "-262156-08-29",
        "-166681-03-20",
        "-110102-07-08",
        "+172503-09-12",
        "+250104-10-14"
    };
    std::vector<bool> expectNull = {false, false, false, false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(
        std::string_view("Asia/Shanghai"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithTz(inputVec, formatVec, tzVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

TEST(FromUnixTimeTest, SafeUnixSecondsBoundariesAndOverflowFallback) {
    constexpr int64_t microsPerSecond = 1'000'000;
    constexpr int64_t minSafe = std::numeric_limits<int64_t>::min() / microsPerSecond;
    constexpr int64_t maxSafe = std::numeric_limits<int64_t>::max() / microsPerSecond;
    const std::vector<int64_t> unixSeconds = {
        minSafe - 1, minSafe, maxSafe, maxSafe + 1
    };

    const auto utc = datetime::ResolveTimeZone("");
    const auto format = datetime::CompileFormatPattern(
        "yyyy-MM-dd", datetime::FormatterKind::FROM_UNIXTIME, false);
    std::vector<std::string> expected;
    for (const int64_t value : unixSeconds) {
        constexpr __int128 modulus = static_cast<__int128>(1) << 64;
        __int128 wrapped = static_cast<__int128>(value) * microsPerSecond % modulus;
        if (wrapped < 0) {
            wrapped += modulus;
        }
        if (wrapped >= (static_cast<__int128>(1) << 63)) {
            wrapped -= modulus;
        }
        const int64_t micros = static_cast<int64_t>(wrapped);
        int64_t seconds = micros / microsPerSecond;
        if (micros % microsPerSecond < 0) {
            --seconds;
        }
        datetime::CalendarTime calendarTime;
        ASSERT_TRUE(datetime::ToCalendar(seconds, 0, utc, calendarTime));
        expected.push_back(datetime::FormatDateTime(calendarTime, utc, format));
    }

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(
        resultVec, expected, std::vector<bool>(unixSeconds.size(), false), unixSeconds.size());
    delete resultVec;
}

TEST(FromUnixTimeTest, DictionaryInputAndFormatEncodings) {
    auto* flatInput = static_cast<Vector<int64_t>*>(
        FromUnixTimeTestHelper::CreateLongVector({0, 86'400}));
    int32_t inputIndices[] = {1, 0, 1};
    BaseVector* inputVec = VectorHelper::CreateDictionary(inputIndices, 3, flatInput);

    auto* flatFormat = static_cast<Vector<LargeStringContainer<std::string_view>>*>(
        FromUnixTimeTestHelper::CreateStringVector({"yyyy-MM-dd", "yyyy"}));
    const int32_t formatIndices[] = {0, 1, 0};
    BaseVector* formatVec =
        VectorHelper::CreateStringDictionary(formatIndices, 3, flatFormat);

    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(
        resultVec,
        {"1970-01-02", "1970", "1970-01-02"},
        {false, false, false},
        3);

    delete flatInput;
    delete flatFormat;
    delete resultVec;
}

TEST(FromUnixTimeTest, ConstInputEncoding) {
    BaseVector* inputVec = new ConstVector<int64_t>(0, OMNI_LONG, 2);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", 2);
    BaseVector* resultVec = nullptr;

    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(
        resultVec,
        {"1970-01-01 00:00:00", "1970-01-01 00:00:00"},
        {false, false},
        2);
    delete resultVec;
}

// Test: OMNI_TIMESTAMP input type (equivalent to OMNI_LONG)
TEST(FromUnixTimeTest, TimestampInputType) {
    std::vector<int64_t> unixSeconds = {0, 100};
    std::vector<std::string> expected = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:01:40"
    };
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateTimestampVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_TIMESTAMP, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: Velox-compatible test cases
TEST(FromUnixTimeTest, VeloxCompatible) {
    std::vector<int64_t> unixSeconds = {0, 100, 120, 100, -59, 3600};
    std::vector<std::string> formats = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd",
        "yyyy-MM-dd HH:mm",
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd HH:mm:ss",
        "yyyy"
    };
    std::vector<std::string> expected = {
        "1970-01-01 00:00:00",
        "1970-01-01",
        "1970-01-01 00:02",
        "1970-01-01 00:01:40",
        "1969-12-31 23:59:01",
        "1970"
    };
    std::vector<bool> expectNull = {false, false, false, false, false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateStringVector(formats);
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTime(inputVec, formatVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: 3-arg signature with timezone (Gluten calling convention)
TEST(FromUnixTimeTest, ThreeArgWithTimezone) {
    // from_unixtime(0, "yyyy-MM-dd HH:mm:ss", "UTC") -> "1970-01-01 00:00:00"
    // from_unixtime(100, "yyyy-MM-dd HH:mm:ss", "UTC") -> "1970-01-01 00:01:40"
    std::vector<int64_t> unixSeconds = {0, 100};
    std::vector<std::string> expected = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:01:40"
    };
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(std::string_view("UTC"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithTz(inputVec, formatVec, tzVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    // Note: Apply() already deletes inputVec, formatVec, and tzVec
    delete resultVec;
}

// Test: 3-arg signature with empty timezone (falls back to UTC)
TEST(FromUnixTimeTest, ThreeArgWithEmptyTimezone) {
    // Empty timezone falls back to UTC (gmtime_r)
    std::vector<int64_t> unixSeconds = {0};
    std::vector<std::string> expected = {"1970-01-01 00:00:00"};
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", 1);
    // Empty timezone string
    BaseVector* tzVec = new ConstVector<std::string_view>(std::string_view(""), OMNI_VARCHAR, 1);
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithTz(inputVec, formatVec, tzVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, 1);

    delete resultVec;
}

// Test: 3-arg signature with GMT+08:00 timezone
TEST(FromUnixTimeTest, ThreeArgWithGMT8Timezone) {
    // from_unixtime(0, "yyyy-MM-dd HH:mm:ss", "GMT+08:00") -> "1970-01-01 08:00:00"
    // from_unixtime(100, "yyyy-MM-dd HH:mm:ss", "GMT+08:00") -> "1970-01-01 08:01:40"
    std::vector<int64_t> unixSeconds = {0, 100};
    std::vector<std::string> expected = {
        "1970-01-01 08:00:00",
        "1970-01-01 08:01:40"
    };
    std::vector<bool> expectNull = {false, false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(std::string_view("GMT+08:00"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithTz(inputVec, formatVec, tzVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: Spark CORRECTED DateTimeFormatter tokens used by from_unixtime.
TEST(FromUnixTimeTest, SparkCorrectedExtendedFormatTokens) {
    std::vector<int64_t> unixSeconds = {0};
    std::vector<std::string> expected = {
        "1970-01-01 1 Thu 1 AM 08 08 08  00 Asia/Shanghai CST GMT+8 +08 +08 +0800"
    };
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd q E F a hh KK kk  SS VV zz O X x Z", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(
        std::string_view("Asia/Shanghai"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithTz(inputVec, formatVec, tzVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: 4-arg signature carries spark.sql.legacy.timeParserPolicy.
TEST(FromUnixTimeTest, FourArgCorrectedPolicy) {
    std::vector<int64_t> unixSeconds = {0};
    std::vector<std::string> expected = {
        "1970-01-01 1 Thu 1 AM 08 08 08  00 Asia/Shanghai CST GMT+8 +08 +08 +0800"
    };
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd q E F a hh KK kk  SS VV zz O X x Z", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(
        std::string_view("Asia/Shanghai"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* policyVec = new ConstVector<std::string_view>(
        std::string_view("CORRECTED"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithPolicy(
        inputVec, formatVec, tzVec, policyVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: CORRECTED z/zz keeps the GMT offset zone name instead of POSIX +08.
TEST(FromUnixTimeTest, FourArgCorrectedGMT8ZoneName) {
    std::vector<int64_t> unixSeconds = {0};
    std::vector<std::string> expected = {
        "1970-01-01 1 Thu 1 AM 08 08 08  00 GMT+08:00 GMT+08:00 GMT+8 +08 +08 +0800"
    };
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd q E F a hh KK kk  SS VV zz O X x Z", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(
        std::string_view("GMT+08:00"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* policyVec = new ConstVector<std::string_view>(
        std::string_view("CORRECTED"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithPolicy(
        inputVec, formatVec, tzVec, policyVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

// Test: LEGACY uses SimpleDateFormat-style F (day-of-week-in-month).
TEST(FromUnixTimeTest, FourArgLegacyPolicy) {
    std::vector<int64_t> unixSeconds = {604800};
    std::vector<std::string> expected = {"1970-01-08 2"};
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd F", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(
        std::string_view("UTC"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* policyVec = new ConstVector<std::string_view>(
        std::string_view("LEGACY"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithPolicy(
        inputVec, formatVec, tzVec, policyVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}

TEST(FromUnixTimeTest, DynamicPolicyAndTimezone) {
    const std::vector<int64_t> unixSeconds = {604'800, 604'800};
    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH F", unixSeconds.size());
    BaseVector* tzVec = FromUnixTimeTestHelper::CreateStringVector(
        {"UTC", "Asia/Shanghai"});
    BaseVector* policyVec = FromUnixTimeTestHelper::CreateStringVector(
        {"CORRECTED", "LEGACY"});
    BaseVector* resultVec = nullptr;

    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithPolicy(
        inputVec, formatVec, tzVec, policyVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(
        resultVec,
        {"1970-01-08 00 1", "1970-01-08 08 2"},
        {false, false},
        unixSeconds.size());
    delete resultVec;
}

// Test: q/v/x are not SimpleDateFormat pattern characters in LEGACY mode.
TEST(FromUnixTimeTest, FourArgLegacyUnsupportedPatternTokens) {
    for (char token : std::vector<char>{'q', 'v', 'x'}) {
        SCOPED_TRACE(std::string("token=") + token);
        std::vector<int64_t> unixSeconds = {0};

        BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
        BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
            std::string("yyyy-MM-dd ") + token, unixSeconds.size());
        BaseVector* tzVec = new ConstVector<std::string_view>(
            std::string_view("UTC"), OMNI_VARCHAR, unixSeconds.size());
        BaseVector* policyVec = new ConstVector<std::string_view>(
            std::string_view("LEGACY"), OMNI_VARCHAR, unixSeconds.size());

        auto signature = std::make_shared<FunctionSignature>("from_unixtime",
            std::vector<DataTypeId>{OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "from_unixtime function not found for 4-arg signature";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(formatVec);
        args.push(tzVec);
        args.push(policyVec);
        BaseVector* resultVec = nullptr;

        EXPECT_ANY_THROW(function->Apply(args, outputType, resultVec, &context));

        delete inputVec;
        delete formatVec;
        delete tzVec;
        delete policyVec;
        delete resultVec;
    }
}

// Test: EXCEPTION formats with the corrected formatter path.
TEST(FromUnixTimeTest, FourArgExceptionPolicy) {
    std::vector<int64_t> unixSeconds = {604800};
    std::vector<std::string> expected = {"1970-01-08 1"};
    std::vector<bool> expectNull = {false};

    BaseVector* inputVec = FromUnixTimeTestHelper::CreateLongVector(unixSeconds);
    BaseVector* formatVec = FromUnixTimeTestHelper::CreateConstStringVector(
        "yyyy-MM-dd F", unixSeconds.size());
    BaseVector* tzVec = new ConstVector<std::string_view>(
        std::string_view("UTC"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* policyVec = new ConstVector<std::string_view>(
        std::string_view("EXCEPTION"), OMNI_VARCHAR, unixSeconds.size());
    BaseVector* resultVec = nullptr;
    FromUnixTimeTestHelper::ExecuteFromUnixTimeWithPolicy(
        inputVec, formatVec, tzVec, policyVec, OMNI_LONG, resultVec);
    FromUnixTimeTestHelper::ValidateResult(resultVec, expected, expectNull, unixSeconds.size());

    delete resultVec;
}
