/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TimestampAdd function implementation
 *
 * TIMESTAMPADD(timeunit VARCHAR, interval BIGINT, timestamp TIMESTAMP) -> TIMESTAMP
 *
 * Adds the specified interval to the timestamp.
 * Supported time units: SECOND, MINUTE, HOUR, DAY, MONTH, YEAR
 *
 * For SECOND/MINUTE/HOUR/DAY: direct microsecond arithmetic.
 * For MONTH/YEAR: calendar-based manipulation preserving sub-second precision.
 */

#include "TimestampAdd.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

/// Time unit enumeration for TIMESTAMPADD
enum TimeUnitKind {
    UNIT_SECOND,
    UNIT_MINUTE,
    UNIT_HOUR,
    UNIT_DAY,
    UNIT_MONTH,
    UNIT_YEAR,
    UNIT_INVALID
};

/// Case-insensitive string comparison
static bool EqualsIgnoreCase(std::string_view a, const char* b)
{
    size_t bLen = std::strlen(b);
    if (a.size() != bLen) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/// Parse time unit string to enum
static TimeUnitKind ParseTimeUnit(std::string_view unit)
{
    if (EqualsIgnoreCase(unit, "SECOND") || EqualsIgnoreCase(unit, "SQL_TSI_SECOND")) {
        return UNIT_SECOND;
    }
    if (EqualsIgnoreCase(unit, "MINUTE") || EqualsIgnoreCase(unit, "SQL_TSI_MINUTE")) {
        return UNIT_MINUTE;
    }
    if (EqualsIgnoreCase(unit, "HOUR") || EqualsIgnoreCase(unit, "SQL_TSI_HOUR")) {
        return UNIT_HOUR;
    }
    if (EqualsIgnoreCase(unit, "DAY") || EqualsIgnoreCase(unit, "SQL_TSI_DAY")) {
        return UNIT_DAY;
    }
    if (EqualsIgnoreCase(unit, "MONTH") || EqualsIgnoreCase(unit, "SQL_TSI_MONTH")) {
        return UNIT_MONTH;
    }
    if (EqualsIgnoreCase(unit, "YEAR") || EqualsIgnoreCase(unit, "SQL_TSI_YEAR")) {
        return UNIT_YEAR;
    }
    return UNIT_INVALID;
}

/// Microsecond constants
static constexpr int64_t kMicrosPerSecond = 1000000LL;
static constexpr int64_t kMicrosPerMinute = 60LL * kMicrosPerSecond;
static constexpr int64_t kMicrosPerHour = 60LL * kMicrosPerMinute;
static constexpr int64_t kMicrosPerDay = 24LL * kMicrosPerHour;

/// Get maximum day of month (1-indexed month)
static int32_t GetMaxDayOfMonth(int32_t year, int32_t month)
{
    bool isLeap = Date32::IsLeapYear(year);
    return isLeap ? LEAP_YEAR_OF_DAYS[month] : NORMAL_YEAR_OF_DAYS[month];
}

/// Add interval to timestamp for time-based units (SECOND/MINUTE/HOUR/DAY)
/// Returns the result in microseconds since epoch
static int64_t AddTimeUnit(int64_t timestampMicros, int64_t interval, TimeUnitKind unit)
{
    int64_t microsPerUnit;
    switch (unit) {
        case UNIT_SECOND: microsPerUnit = kMicrosPerSecond; break;
        case UNIT_MINUTE: microsPerUnit = kMicrosPerMinute; break;
        case UNIT_HOUR:   microsPerUnit = kMicrosPerHour; break;
        case UNIT_DAY:    microsPerUnit = kMicrosPerDay; break;
        default: return timestampMicros; // Should not reach here
    }
    return timestampMicros + interval * microsPerUnit;
}

/// Add interval to timestamp for calendar-based units (MONTH/YEAR)
/// Returns true on success, false on overflow
static bool AddCalendarUnit(int64_t timestampMicros, int64_t interval, TimeUnitKind unit,
                            int64_t &resultMicros)
{
    // Split into seconds and sub-second parts
    int64_t totalMicros = timestampMicros;
    int64_t subSecondMicros;
    int64_t epochSeconds;

    if (totalMicros >= 0) {
        epochSeconds = totalMicros / kMicrosPerSecond;
        subSecondMicros = totalMicros % kMicrosPerSecond;
    } else {
        // For negative timestamps, ensure subSecondMicros is non-negative
        epochSeconds = totalMicros / kMicrosPerSecond - 1;
        subSecondMicros = totalMicros - epochSeconds * kMicrosPerSecond;
    }

    // Convert to calendar
    std::tm tmValue;
    if (!Timestamp::epochToCalendarUtc(epochSeconds, tmValue)) {
        return false;
    }

    int32_t year = tmValue.tm_year + 1900;
    int32_t month = tmValue.tm_mon + 1; // tm_mon is 0-11
    int32_t day = tmValue.tm_mday;

    if (unit == UNIT_MONTH) {
        // Add months similar to AddMonths logic
        int64_t monthAdded = static_cast<int64_t>(month) - 1 + interval;
        int64_t yearOffset = (monthAdded >= 0 ? monthAdded : monthAdded - 11) / 12;
        int32_t monthResult = static_cast<int32_t>(monthAdded - yearOffset * 12 + 1);
        int64_t yearResult = static_cast<int64_t>(year) + yearOffset;

        if (yearResult < MIN_YEAR || yearResult > MAX_YEAR) {
            return false;
        }

        // Adjust day if it exceeds the max day of the result month
        int32_t lastDayOfMonth = GetMaxDayOfMonth(static_cast<int32_t>(yearResult), monthResult);
        int32_t dayResult = (lastDayOfMonth < day) ? lastDayOfMonth : day;

        // Convert back to epoch seconds
        tmValue.tm_year = static_cast<int32_t>(yearResult) - 1900;
        tmValue.tm_mon = monthResult - 1;
        tmValue.tm_mday = dayResult;
    } else { // UNIT_YEAR
        int64_t yearResult = static_cast<int64_t>(year) + interval;
        if (yearResult < MIN_YEAR || yearResult > MAX_YEAR) {
            return false;
        }

        // Adjust day for leap year (Feb 29 -> Feb 28)
        int32_t lastDayOfMonth = GetMaxDayOfMonth(static_cast<int32_t>(yearResult), month);
        int32_t dayResult = (lastDayOfMonth < day) ? lastDayOfMonth : day;

        tmValue.tm_year = static_cast<int32_t>(yearResult) - 1900;
        tmValue.tm_mday = dayResult;
    }

    int64_t newEpochSeconds = Timestamp::calendarUtcToEpoch(tmValue);
    resultMicros = newEpochSeconds * kMicrosPerSecond + subSecondMicros;
    return true;
}

/// Helper: extract string value from vector with different encodings
static std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row)
{
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    }
    return std::string_view();
}

class TimestampAddFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 3) {
            return;
        }

        // Extract arguments from stack (LIFO order):
        // Stack: unit (top), interval, timestamp (bottom)
        const auto unitArg = args.top();
        args.pop();
        const auto intervalArg = args.top();
        args.pop();
        const auto timestampArg = args.top();
        args.pop();

        const auto size = timestampArg->GetSize();

        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));

        // Get timestamp values
        auto *timestampVector = reinterpret_cast<Vector<int64_t> *>(timestampArg);
        const auto *timestampRaw = unsafe::UnsafeVector::GetRawValues(timestampVector);
        const auto *timestampNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(timestampArg));

        // Check if unit is constant
        bool unitIsConst = (unitArg->GetEncoding() == OMNI_ENCODING_CONST ||
                            unitArg->GetEncoding() == OMNI_DICTIONARY);
        TimeUnitKind constUnit = UNIT_INVALID;
        std::string constUnitStr;

        if (unitIsConst) {
            std::string_view unitView = GetStringValueFromVector(unitArg, 0);
            constUnitStr = std::string(unitView);
            constUnit = ParseTimeUnit(constUnitStr);
        }

        // Check if interval is constant
        bool intervalIsConst = (intervalArg->GetEncoding() == OMNI_ENCODING_CONST);
        int64_t constInterval = 0;
        const int64_t *intervalRaw = nullptr;
        const uint64_t *intervalNulls = nullptr;

        if (intervalIsConst) {
            auto *constIntervalVec = reinterpret_cast<ConstVector<int64_t> *>(intervalArg);
            constInterval = constIntervalVec->GetConstValue();
            if (intervalArg->IsNull(0)) {
                // If constant interval is NULL, set all results to NULL
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                delete unitArg;
                delete intervalArg;
                delete timestampArg;
                return;
            }
        } else {
            auto *intervalVector = reinterpret_cast<Vector<int64_t> *>(intervalArg);
            intervalRaw = unsafe::UnsafeVector::GetRawValues(intervalVector);
            intervalNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(intervalArg));
        }

        // Copy NULL bits from timestamp input to result
        auto nullsSize = BitUtil::Nbytes(size);
        if (timestampNulls != nullptr) {
            memcpy(resultNulls, timestampNulls, nullsSize);
        } else {
            memset(resultNulls, 0, nullsSize);
        }

        // Also merge NULL bits from interval (for non-const case)
        if (!intervalIsConst && intervalNulls != nullptr) {
            for (int32_t i = 0; i < size; ++i) {
                if (BitUtil::IsBitSet(intervalNulls, i)) {
                    BitUtil::SetBit(resultNulls, i);
                }
            }
        }

        // If constant unit is invalid, mark all as null
        if (unitIsConst && constUnit == UNIT_INVALID) {
            // Check if unit itself is NULL
            if (unitArg->IsNull(0)) {
                memset(resultNulls, 0xFF, nullsSize);
            } else {
                memset(resultNulls, 0xFF, nullsSize);
            }
            delete unitArg;
            delete intervalArg;
            delete timestampArg;
            return;
        }

        // Process only non-NULL rows
        SelectivityVector rows(size);
        rows.setFromBitsNegate(timestampNulls, size);

        rows.applyToSelected([&](vector_size_t i) {
            // Check if interval is NULL (for non-const case)
            if (!intervalIsConst) {
                if (intervalNulls && BitUtil::IsBitSet(intervalNulls, i)) {
                    result->SetNull(i);
                    return;
                }
            }

            // Check if unit is NULL (for non-const case)
            if (!unitIsConst) {
                const auto *unitNullsLocal = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(unitArg));
                if (unitNullsLocal && BitUtil::IsBitSet(unitNullsLocal, i)) {
                    result->SetNull(i);
                    return;
                }
            }

            // Get time unit
            TimeUnitKind unit;
            if (unitIsConst) {
                unit = constUnit;
            } else {
                std::string_view unitView = GetStringValueFromVector(unitArg, i);
                std::string unitStr(unitView);
                unit = ParseTimeUnit(unitStr);
            }

            if (unit == UNIT_INVALID) {
                result->SetNull(i);
                return;
            }

            // Get interval
            int64_t interval = intervalIsConst ? constInterval : intervalRaw[i];

            // Get timestamp
            int64_t timestampMicros = timestampRaw[i];

            // Perform the addition
            int64_t resultValue;
            if (unit == UNIT_SECOND || unit == UNIT_MINUTE || unit == UNIT_HOUR || unit == UNIT_DAY) {
                resultValue = AddTimeUnit(timestampMicros, interval, unit);
                resultRaw[i] = resultValue;
                result->SetNotNull(i);
            } else {
                // MONTH or YEAR - calendar-based
                if (AddCalendarUnit(timestampMicros, interval, unit, resultValue)) {
                    resultRaw[i] = resultValue;
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            }
        });

        // Clean up
        delete unitArg;
        delete intervalArg;
        delete timestampArg;
    }
};

} // namespace

void RegisterTimestampAddFunction(const std::string &name)
{
    // timestampadd(unit VARCHAR, interval BIGINT, timestamp TIMESTAMP) -> TIMESTAMP
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_VARCHAR, OMNI_LONG, OMNI_TIMESTAMP}, OMNI_TIMESTAMP,
        std::make_shared<TimestampAddFunction>());
}

} // namespace omniruntime::vectorization
