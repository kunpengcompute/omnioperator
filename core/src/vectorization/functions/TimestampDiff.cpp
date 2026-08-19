/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TimestampDiff function implementation
 *
 * TIMESTAMPDIFF(timeunit VARCHAR, timestamp1 TIMESTAMP, timestamp2 TIMESTAMP) -> BIGINT
 *
 * Returns the difference between timestamp1 and timestamp2 (timestamp1 - timestamp2).
 * Supported time units: SECOND, MINUTE, HOUR, DAY, MONTH, YEAR
 *
 * For SECOND/MINUTE/HOUR/DAY: integer division of microsecond difference.
 * For MONTH/YEAR: calendar-based difference.
 */

#include "TimestampDiff.h"
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

/// Time unit enumeration for TIMESTAMPDIFF
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

/// Compute difference for time-based units (SECOND/MINUTE/HOUR/DAY)
/// Returns (ts1 - ts2) in the specified unit, truncated toward zero
static int64_t ComputeTimeDiff(int64_t ts1Micros, int64_t ts2Micros, TimeUnitKind unit)
{
    int64_t diffMicros = ts1Micros - ts2Micros;
    int64_t microsPerUnit;
    switch (unit) {
        case UNIT_SECOND: microsPerUnit = kMicrosPerSecond; break;
        case UNIT_MINUTE: microsPerUnit = kMicrosPerMinute; break;
        case UNIT_HOUR:   microsPerUnit = kMicrosPerHour; break;
        case UNIT_DAY:    microsPerUnit = kMicrosPerDay; break;
        default: return 0; // Should not reach here
    }
    return diffMicros / microsPerUnit;
}

/// Compute difference for calendar-based units (MONTH/YEAR)
/// Returns true on success, false on error
static bool ComputeCalendarDiff(int64_t ts1Micros, int64_t ts2Micros, TimeUnitKind unit,
                                int64_t &result)
{
    // Convert ts1 to calendar
    int64_t epochSeconds1;
    if (ts1Micros >= 0) {
        epochSeconds1 = ts1Micros / kMicrosPerSecond;
    } else {
        epochSeconds1 = ts1Micros / kMicrosPerSecond - 1;
    }
    std::tm tm1;
    if (!Timestamp::epochToCalendarUtc(epochSeconds1, tm1)) {
        return false;
    }

    // Convert ts2 to calendar
    int64_t epochSeconds2;
    if (ts2Micros >= 0) {
        epochSeconds2 = ts2Micros / kMicrosPerSecond;
    } else {
        epochSeconds2 = ts2Micros / kMicrosPerSecond - 1;
    }
    std::tm tm2;
    if (!Timestamp::epochToCalendarUtc(epochSeconds2, tm2)) {
        return false;
    }

    int32_t year1 = tm1.tm_year + 1900;
    int32_t month1 = tm1.tm_mon + 1; // tm_mon is 0-11
    int32_t year2 = tm2.tm_year + 1900;
    int32_t month2 = tm2.tm_mon + 1;

    if (unit == UNIT_MONTH) {
        // Month difference: (year1 - year2) * 12 + (month1 - month2)
        result = static_cast<int64_t>(year1 - year2) * 12 + (month1 - month2);
    } else { // UNIT_YEAR
        // Year difference
        result = static_cast<int64_t>(year1 - year2);
    }
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

class TimestampDiffFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 3) {
            return;
        }

        // Extract arguments from stack (LIFO order):
        // Stack: unit (top), ts1, ts2 (bottom)
        const auto unitArg = args.top();
        args.pop();
        const auto ts1Arg = args.top();
        args.pop();
        const auto ts2Arg = args.top();
        args.pop();

        const auto size = ts1Arg->GetSize();

        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));

        // Get timestamp1 values
        auto *ts1Vector = reinterpret_cast<Vector<int64_t> *>(ts1Arg);
        const auto *ts1Raw = unsafe::UnsafeVector::GetRawValues(ts1Vector);
        const auto *ts1Nulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(ts1Arg));

        // Get timestamp2 values
        auto *ts2Vector = reinterpret_cast<Vector<int64_t> *>(ts2Arg);
        const auto *ts2Raw = unsafe::UnsafeVector::GetRawValues(ts2Vector);
        const auto *ts2Nulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(ts2Arg));

        // Check if unit is constant
        bool unitIsConst = (unitArg->GetEncoding() == OMNI_ENCODING_CONST ||
                            unitArg->GetEncoding() == OMNI_DICTIONARY);
        TimeUnitKind constUnit = UNIT_INVALID;

        if (unitIsConst) {
            std::string_view unitView = GetStringValueFromVector(unitArg, 0);
            constUnit = ParseTimeUnit(unitView);
        }

        // Copy NULL bits from ts1 input to result
        auto nullsSize = BitUtil::Nbytes(size);
        if (ts1Nulls != nullptr) {
            memcpy(resultNulls, ts1Nulls, nullsSize);
        } else {
            memset(resultNulls, 0, nullsSize);
        }

        // Merge NULL bits from ts2
        if (ts2Nulls != nullptr) {
            for (int32_t i = 0; i < size; ++i) {
                if (BitUtil::IsBitSet(ts2Nulls, i)) {
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
            delete ts1Arg;
            delete ts2Arg;
            return;
        }

        // Process only non-NULL rows
        SelectivityVector rows(size);
        rows.setFromBitsNegate(resultNulls, size);

        rows.applyToSelected([&](vector_size_t i) {
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

            // Get timestamps
            int64_t ts1Micros = ts1Raw[i];
            int64_t ts2Micros = ts2Raw[i];

            // Perform the difference calculation
            int64_t resultValue;
            if (unit == UNIT_SECOND || unit == UNIT_MINUTE || unit == UNIT_HOUR || unit == UNIT_DAY) {
                resultValue = ComputeTimeDiff(ts1Micros, ts2Micros, unit);
                resultRaw[i] = resultValue;
                result->SetNotNull(i);
            } else {
                // MONTH or YEAR - calendar-based
                if (ComputeCalendarDiff(ts1Micros, ts2Micros, unit, resultValue)) {
                    resultRaw[i] = resultValue;
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            }
        });

        // Clean up
        delete unitArg;
        delete ts1Arg;
        delete ts2Arg;
    }
};

} // namespace

void RegisterTimestampDiffFunction(const std::string &name)
{
    // timestampdiff(unit VARCHAR, ts1 TIMESTAMP, ts2 TIMESTAMP) -> BIGINT
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_VARCHAR, OMNI_TIMESTAMP, OMNI_TIMESTAMP}, OMNI_LONG,
        std::make_shared<TimestampDiffFunction>());
}

} // namespace omniruntime::vectorization
