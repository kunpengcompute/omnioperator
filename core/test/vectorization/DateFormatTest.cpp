/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateFormat function unit tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DateFormat.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class DateFormatTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const date_format_test_env =
    ::testing::AddGlobalTestEnvironment(new DateFormatTestEnvironment);

class DateFormatFunctionTestHelper {
public:
    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
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
        auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, std::string_view(values[i]));
        }
        return vec;
    }

    static std::string GetStringValue(BaseVector* vec, int32_t row) {
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        std::string_view view = typedVec->GetValue(row);
        return std::string(view);
    }

    static void ExecuteDateFormat(BaseVector* timestampVec, BaseVector* formatVec,
        DataTypeId timestampTypeId, DataTypeId formatTypeId, BaseVector*& result) {
        ExecuteDateFormatWithTz(timestampVec, formatVec, timestampTypeId, formatTypeId, "", result);
    }

    /// Execute date_format with an explicit session timezone configured into
    /// the ExecutionContext, so the timezone-related tokens (v/z/O/X/x/Z)
    /// can be exercised. Pass empty string to fall back to UTC behavior.
    static void ExecuteDateFormatWithTz(BaseVector* timestampVec, BaseVector* formatVec,
        DataTypeId timestampTypeId, DataTypeId formatTypeId,
        const std::string& sessionTimezone, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("DateFormat",
            std::vector<DataTypeId>{timestampTypeId, formatTypeId}, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DateFormat function not found for signature ("
            << static_cast<int>(timestampTypeId) << ", " << static_cast<int>(formatTypeId) << ")";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        context.SetResultRowSize(timestampVec->GetSize());

        if (!sessionTimezone.empty()) {
            std::unordered_map<std::string, std::string> values{
                {omniruntime::config::QueryConfig::kSessionTimezone, sessionTimezone},
                {omniruntime::config::QueryConfig::kAdjustTimestampToTimezone, "true"}
            };
            omniruntime::config::QueryConfig queryConfig(std::move(values));
            context.SetConfig(queryConfig);
        }

        std::stack<BaseVector*> args;
        args.push(timestampVec);
        args.push(formatVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DateFormat function threw an exception";
    }

    /// Returns microseconds since epoch for a given UTC date and time.
    static int64_t ToMicros(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) {
        std::tm tmValue{};
        tmValue.tm_year = year - 1900;
        tmValue.tm_mon = month - 1;
        tmValue.tm_mday = day;
        tmValue.tm_hour = hour;
        tmValue.tm_min = minute;
        tmValue.tm_sec = second;
        tmValue.tm_isdst = 0;
        // timegm interprets tm as UTC
        time_t seconds = timegm(&tmValue);
        return static_cast<int64_t>(seconds) * 1000000LL;
    }
};

// Test: date_format TIMESTAMP with yyyy-MM-dd format
TEST(DateFormatTest, TimestampYMDFormat) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(1970, 1, 1),
        DateFormatFunctionTestHelper::ToMicros(2020, 12, 31)
    };
    std::vector<std::string> fmtValues = {"yyyy-MM-dd", "yyyy-MM-dd", "yyyy-MM-dd"};
    std::vector<std::string> expected = {"2024-01-29", "1970-01-01", "2020-12-31"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: date_format with full timestamp format yyyy-MM-dd HH:mm:ss
TEST(DateFormatTest, TimestampFullFormat) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29, 11, 45, 30),
        DateFormatFunctionTestHelper::ToMicros(1970, 1, 1, 0, 0, 0),
        DateFormatFunctionTestHelper::ToMicros(2020, 12, 31, 23, 59, 59)
    };
    std::vector<std::string> fmtValues = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd HH:mm:ss"
    };
    std::vector<std::string> expected = {
        "2024-01-29 11:45:30",
        "1970-01-01 00:00:00",
        "2020-12-31 23:59:59"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: date_format with LONG type (OMNI_LONG equivalent to OMNI_TIMESTAMP)
TEST(DateFormatTest, LongYMDFormat) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(2020, 2, 29)
    };
    std::vector<std::string> fmtValues = {"yyyy-MM-dd", "yyyy-MM-dd"};
    std::vector<std::string> expected = {"2024-01-29", "2020-02-29"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateLongVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_LONG, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: date_format with only time portion HH:mm:ss
TEST(DateFormatTest, TimeOnlyFormat) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29, 11, 45, 30),
        DateFormatFunctionTestHelper::ToMicros(1970, 1, 1, 0, 0, 0),
        DateFormatFunctionTestHelper::ToMicros(2020, 6, 15, 23, 59, 59)
    };
    std::vector<std::string> fmtValues = {"HH:mm:ss", "HH:mm:ss", "HH:mm:ss"};
    std::vector<std::string> expected = {"11:45:30", "00:00:00", "23:59:59"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: date_format with year-only format yyyy
TEST(DateFormatTest, YearOnlyFormat) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 1),
        DateFormatFunctionTestHelper::ToMicros(1900, 12, 31),
        DateFormatFunctionTestHelper::ToMicros(2038, 1, 19)
    };
    std::vector<std::string> fmtValues = {"yyyy", "yyyy", "yyyy"};
    std::vector<std::string> expected = {"2024", "1900", "2038"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: epoch timestamp (1970-01-01 00:00:00 UTC)
TEST(DateFormatTest, EpochTimestamp) {
    std::vector<int64_t> tsValues = {0};
    std::vector<std::string> fmtValues = {"yyyy-MM-dd HH:mm:ss"};
    std::vector<std::string> expected = {"1970-01-01 00:00:00"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 0), expected[0]);

    delete resultVec;
}

// Test: NULL timestamp input
TEST(DateFormatTest, NullTimestamp) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29)
    };
    std::vector<std::string> fmtValues = {"yyyy-MM-dd", "yyyy-MM-dd", "yyyy-MM-dd"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    tsVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (timestamp is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2));

    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 0), "2024-01-29");
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 2), "2024-01-29");

    delete resultVec;
}

// Test: NULL format string
TEST(DateFormatTest, NullFormat) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29)
    };
    std::vector<std::string> fmtValues = {"yyyy-MM-dd", "yyyy-MM-dd", "yyyy-MM-dd"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    fmtVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (format is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2));

    delete resultVec;
}

// Test: mixed formats across rows (non-constant format)
TEST(DateFormatTest, MixedFormats) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 1, 29, 11, 45, 30);
    std::vector<int64_t> tsValues = {ts, ts, ts, ts};
    std::vector<std::string> fmtValues = {
        "yyyy-MM-dd",
        "yyyy-MM-dd HH:mm:ss",
        "HH:mm:ss",
        "yyyy"
    };
    std::vector<std::string> expected = {
        "2024-01-29",
        "2024-01-29 11:45:30",
        "11:45:30",
        "2024"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: leap year Feb 29
TEST(DateFormatTest, LeapYear) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2020, 2, 29, 12, 0, 0),
        DateFormatFunctionTestHelper::ToMicros(2000, 2, 29, 0, 0, 0)
    };
    std::vector<std::string> fmtValues = {"yyyy-MM-dd", "yyyy-MM-dd"};
    std::vector<std::string> expected = {"2020-02-29", "2000-02-29"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch";
    }

    delete resultVec;
}

// Test: extended Spark/Java DateTimeFormatter tokens (q/Q quarter, K/k hour variants,
//       D day-of-year, E weekday name, MMM/MMMM month name, a AM/PM, h clock-hour)
TEST(DateFormatTest, ExtendedTokens) {
    // 2024-04-15 (Mon, Q2, day-of-year=106, week ~16) at 09:30:45
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 4, 15, 9, 30, 45);
    std::vector<int64_t> tsValues(8, ts);
    std::vector<std::string> fmtValues = {
        "Q",            // quarter as number
        "QQQ",          // quarter padded to 3 digits ("002")
        "MMM",          // short month name
        "MMMM",         // full month name
        "EEE",          // short day-of-week
        "EEEE",         // full day-of-week
        "D",            // day-of-year
        "h:mm a"        // 12-hour clock + AM/PM
    };
    std::vector<std::string> expected = {
        "2",
        "002",
        "Apr",
        "April",
        "Mon",
        "Monday",
        "106",
        "9:30 AM"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " value mismatch (format: " << fmtValues[i] << ")";
    }

    delete resultVec;
}

// Test: hour-of-day variants H/h/K/k at boundary hours 0, 11, 12, 23
TEST(DateFormatTest, HourVariants) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 1, 11, 0, 0),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 1, 12, 0, 0),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 1, 23, 0, 0)
    };
    // H=0-23, h=1-12, K=0-11, k=1-24
    std::vector<std::string> fmtValues = {
        "HH:hh:KK:kk a",
        "HH:hh:KK:kk a",
        "HH:hh:KK:kk a",
        "HH:hh:KK:kk a"
    };
    std::vector<std::string> expected = {
        "00:12:00:24 AM",
        "11:11:11:11 AM",
        "12:12:00:12 PM",
        "23:11:11:23 PM"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " hour variant mismatch";
    }

    delete resultVec;
}

// Test: fraction-of-second token S with various widths (real microsecond values)
TEST(DateFormatTest, FractionOfSecond) {
    // 2024-01-01 00:00:00.123456 UTC -> 123456 microseconds
    int64_t baseMicros = DateFormatFunctionTestHelper::ToMicros(2024, 1, 1, 0, 0, 0) + 123456;
    std::vector<int64_t> tsValues(5, baseMicros);
    std::vector<std::string> fmtValues = {
        "S",          // tenths
        "SS",         // hundredths
        "SSS",        // milliseconds (3 digits)
        "SSSSSS",     // microseconds (6 digits)
        "ss.SSSSSS"   // combined seconds + microseconds
    };
    std::vector<std::string> expected = {
        "1",
        "12",
        "123",
        "123456",
        "00.123456"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " fraction-of-second mismatch (format: " << fmtValues[i] << ")";
    }

    delete resultVec;
}

// Test: literal text escape with single quotes (Java DateTimeFormatter semantics)
TEST(DateFormatTest, LiteralEscape) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 4, 15, 9, 30, 0);
    std::vector<int64_t> tsValues(3, ts);
    std::vector<std::string> fmtValues = {
        "yyyy'-'MM'-'dd",        // ' literally escapes "-" (no token meaning anyway, just exercise escape)
        "yyyy'年'MM'月'dd'日'",   // CJK literal text
        "''yyyy''"                // doubled single-quotes -> single literal '
    };
    std::vector<std::string> expected = {
        "2024-04-15",
        "2024年04月15日",
        "'2024'"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " literal-escape mismatch (format: " << fmtValues[i] << ")";
    }

    delete resultVec;
}

// Test: both timestamp and format NULL at same row
TEST(DateFormatTest, BothNull) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29),
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 29)
    };
    std::vector<std::string> fmtValues = {"yyyy-MM-dd", "yyyy-MM-dd"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    tsVec->SetNull(0);
    fmtVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    DateFormatFunctionTestHelper::ExecuteDateFormat(tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (both NULL)";
    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 1), "2024-01-29");

    delete resultVec;
}

// =============================================================================
// Timezone token regression tests
// Fix verifies that 'v', 'z', 'O', 'X', 'x', 'Z' tokens produce correct results
// (no garbage zone names, no wrong negative offsets) when the session timezone
// is set in QueryConfig. The previous implementation used util::GetDateTime
// which did not populate tm_gmtoff / tm_zone, leading to corrupted output.
// =============================================================================

// Test: zone name token 'z' / 'v' should not be garbage and should reflect
// the configured session timezone.
TEST(DateFormatTest, TimeZoneNameTokens_AsiaShanghai) {
    // 2024-06-15 12:00:00 UTC -> 2024-06-15 20:00:00 Asia/Shanghai
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues = {ts, ts};
    std::vector<std::string> fmtValues = {"z", "v"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    // For Asia/Shanghai, glibc populates tm_zone as "CST".
    // The exact spelling depends on tzdata, but it must be non-empty ASCII
    // and free of control / garbage bytes.
    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i)) << "Row " << i << " should not be NULL";
        std::string actual = DateFormatFunctionTestHelper::GetStringValue(resultVec, i);
        EXPECT_FALSE(actual.empty()) << "Row " << i << " ('" << fmtValues[i]
            << "') zone name should not be empty";
        for (unsigned char ch : actual) {
            EXPECT_TRUE(std::isprint(ch))
                << "Row " << i << " ('" << fmtValues[i] << "') contains non-printable byte: 0x"
                << std::hex << static_cast<int>(ch) << " (output=\"" << actual << "\")";
        }
    }

    delete resultVec;
}

// Test: 'z' / 'v' tokens with GMT-prefixed timezone string should produce
// the GMT-prefixed string itself (per Spark behavior on GMT offsets).
TEST(DateFormatTest, TimeZoneNameTokens_GmtPrefix) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues = {ts, ts};
    std::vector<std::string> fmtValues = {"z", "v"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "GMT+08:00", resultVec);

    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), "GMT+08:00")
            << "Row " << i << " ('" << fmtValues[i] << "') mismatch";
    }

    delete resultVec;
}

// Test: localized offset token 'O' / 'OOOO' should produce GMT+8 / GMT+08:00
// when session timezone is Asia/Shanghai (positive offset, no wrong negatives).
TEST(DateFormatTest, LocalizedOffsetToken_O) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues(2, ts);
    std::vector<std::string> fmtValues = {"O", "OOOO"};
    std::vector<std::string> expected = {"GMT+8", "GMT+08:00"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " ('" << fmtValues[i] << "') localized offset mismatch";
    }

    delete resultVec;
}

// Test: ISO offset token 'X' with various widths.
// Asia/Shanghai is +08:00 (positive), so output must start with '+' and never
// produce a wrong negative offset.
TEST(DateFormatTest, IsoOffsetToken_X) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues(3, ts);
    std::vector<std::string> fmtValues = {"X", "XX", "XXX"};
    std::vector<std::string> expected = {"+08", "+0800", "+08:00"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " ('" << fmtValues[i] << "') ISO offset mismatch";
    }

    delete resultVec;
}

// Test: ISO offset token 'X' at UTC offset 0 should output "Z" (per Spark).
TEST(DateFormatTest, IsoOffsetToken_X_Utc) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues = {ts};
    std::vector<std::string> fmtValues = {"X"};
    std::vector<std::string> expected = {"Z"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "UTC", resultVec);

    ASSERT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 0), expected[0]);

    delete resultVec;
}

// Test: lower-case 'x' token never produces "Z" for zero offset (per Spark).
TEST(DateFormatTest, IsoOffsetToken_x_LowerCase) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues = {ts, ts};
    std::vector<std::string> fmtValues = {"x", "xxx"};
    // For UTC: 'x' -> "+00", 'xxx' -> "+00:00".
    std::vector<std::string> expected = {"+00", "+00:00"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "UTC", resultVec);

    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " ('" << fmtValues[i] << "') lower-x mismatch";
    }

    delete resultVec;
}

// Test: 'Z' token: count <=3 outputs RFC offset, count >=4 outputs localized.
TEST(DateFormatTest, OffsetToken_Z) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues(2, ts);
    std::vector<std::string> fmtValues = {"Z", "ZZZZ"};
    std::vector<std::string> expected = {"+0800", "GMT+08:00"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " ('" << fmtValues[i] << "') Z token mismatch";
    }

    delete resultVec;
}

// Test: combined timestamp + timezone token, should produce local-time
// digits plus offset, all consistent.
TEST(DateFormatTest, FullPatternWithTimezoneTokens) {
    // 2024-06-15 12:00:00 UTC -> 2024-06-15 20:00:00 Asia/Shanghai
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues = {ts};
    std::vector<std::string> fmtValues = {"yyyy-MM-dd HH:mm:ss XXX"};
    std::vector<std::string> expected = {"2024-06-15 20:00:00 +08:00"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    ASSERT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 0), expected[0]);

    delete resultVec;
}

// Test: 'V' token outputs the canonical session timezone ID stored in
// ResolvedTimeZone, independently of the timezone abbreviation.
TEST(DateFormatTest, ZoneIdToken_V) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0);
    std::vector<int64_t> tsValues = {ts};
    std::vector<std::string> fmtValues = {"V"};
    std::vector<std::string> expected = {"Asia/Shanghai"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    ASSERT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 0), expected[0]);

    delete resultVec;
}

// =============================================================================
// Code review fix regression tests
//   - GetLegacyDayOfWeekInMonth removed: 'F' token now always uses
//     ((dayOfMonth - 1) % 7) + 1 (Java DateTimeFormatter aligned-week-of-month).
//   - setenv/tzset hoisted out of per-row loop: must remain consistent within
//     a batch even when input rows span multiple distinct days.
//   - Dead isLegacy branches removed: q/Q/V/v/O/x tokens must succeed
//     unconditionally (no spurious ThrowUnsupportedPattern path).
// =============================================================================

// Test: 'F' (aligned-week-of-month) for representative same-weekday days.
// Per Java DateTimeFormatter, F is ordinal of same-weekday occurrence in
// the month: day 1..7 -> 1, day 8..14 -> 2, ... regardless of weekday.
TEST(DateFormatTest, AlignedWeekOfMonth_F) {
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 1, 0, 0, 0),   // 1st of month
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 7, 0, 0, 0),   // 7th
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 8, 0, 0, 0),   // 8th
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 14, 0, 0, 0),  // 14th
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 0, 0, 0),  // 15th
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 30, 0, 0, 0)   // 30th
    };
    std::vector<std::string> fmtValues(tsValues.size(), "F");
    std::vector<std::string> expected = {"1", "1", "2", "2", "3", "5"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormat(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " 'F' token mismatch";
    }

    delete resultVec;
}

// Test: timezone is applied consistently to ALL rows in a batch even when
// they span different dates. Regression for the case where setenv/tzset
// was previously called per-row (now hoisted out of the row loop).
TEST(DateFormatTest, TimezoneConsistentAcrossBatch) {
    // Multiple distinct timestamps spanning different days; all should be
    // formatted in Asia/Shanghai (UTC+8) consistently.
    std::vector<int64_t> tsValues = {
        DateFormatFunctionTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),    // -> 08:00 next day
        DateFormatFunctionTestHelper::ToMicros(2024, 6, 15, 12, 0, 0),  // -> 20:00 same day
        DateFormatFunctionTestHelper::ToMicros(2024, 12, 31, 16, 0, 0), // -> 00:00 next day
        DateFormatFunctionTestHelper::ToMicros(2024, 3, 10, 23, 30, 0)  // -> 07:30 next day
    };
    std::vector<std::string> fmtValues(tsValues.size(), "yyyy-MM-dd HH:mm XXX");
    std::vector<std::string> expected = {
        "2024-01-01 08:00 +08:00",
        "2024-06-15 20:00 +08:00",
        "2025-01-01 00:00 +08:00",
        "2024-03-11 07:30 +08:00"
    };

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    for (size_t i = 0; i < expected.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i));
        EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, i), expected[i])
            << "Row " << i << " batch-timezone consistency mismatch";
    }

    delete resultVec;
}

// Test: tokens that previously had an isLegacy-gated ThrowUnsupportedPattern
// branch (q/Q/V/v/O/x) must all succeed under default CORRECTED policy.
TEST(DateFormatTest, NoLegacyThrow_FormerlyGatedTokens) {
    int64_t ts = DateFormatFunctionTestHelper::ToMicros(2024, 4, 15, 12, 0, 0);
    std::vector<int64_t> tsValues(6, ts);
    std::vector<std::string> fmtValues = {"q", "Q", "V", "v", "O", "x"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    DateFormatFunctionTestHelper::ExecuteDateFormatWithTz(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, "Asia/Shanghai", resultVec);

    for (size_t i = 0; i < fmtValues.size(); ++i) {
        ASSERT_FALSE(resultVec->IsNull(i))
            << "Token '" << fmtValues[i] << "' must not throw / become NULL under CORRECTED policy";
        std::string actual = DateFormatFunctionTestHelper::GetStringValue(resultVec, i);
        EXPECT_FALSE(actual.empty())
            << "Token '" << fmtValues[i] << "' produced empty output";
    }

    delete resultVec;
}

// Test: pre-epoch (negative microseconds) - floor-semantics split must
// correctly produce non-negative sub-second microseconds and a
// floor-rounded seconds count. Without floor handling, the sub-second
// fraction or the seconds field would be off by one.
// Choose: 1969-12-31 23:59:59.123456 UTC -> -876544 microseconds.
TEST(DateFormatTest, NegativeTimestampFloorSplit) {
    int64_t epochZero = DateFormatFunctionTestHelper::ToMicros(1970, 1, 1, 0, 0, 0);
    int64_t ts = epochZero - 876544;  // 1969-12-31 23:59:59.123456 UTC
    std::vector<int64_t> tsValues = {ts};
    std::vector<std::string> fmtValues = {"yyyy-MM-dd HH:mm:ss.SSSSSS"};
    std::vector<std::string> expected = {"1969-12-31 23:59:59.123456"};

    BaseVector* tsVec = DateFormatFunctionTestHelper::CreateTimestampVector(tsValues);
    BaseVector* fmtVec = DateFormatFunctionTestHelper::CreateStringVector(fmtValues);
    BaseVector* resultVec = nullptr;

    // No timezone -> UTC path.
    DateFormatFunctionTestHelper::ExecuteDateFormat(
        tsVec, fmtVec, OMNI_TIMESTAMP, OMNI_VARCHAR, resultVec);

    ASSERT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(DateFormatFunctionTestHelper::GetStringValue(resultVec, 0), expected[0]);

    delete resultVec;
}
