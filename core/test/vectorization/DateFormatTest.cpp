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
        auto signature = std::make_shared<FunctionSignature>("DateFormat",
            std::vector<DataTypeId>{timestampTypeId, formatTypeId}, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DateFormat function not found for signature ("
            << static_cast<int>(timestampTypeId) << ", " << static_cast<int>(formatTypeId) << ")";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(timestampVec->GetSize());
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
