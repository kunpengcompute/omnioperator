/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MonthsBetween function implementation
 */

#include "MonthsBetween.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "type/date_time_utils.h"
#include <ctime>
#include <cstring>
#include <cmath>
#include <string>
#include <string_view>


namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

constexpr int32_t kMonthsInYear = 12;
constexpr int64_t kSecondsInMinute = 60;
constexpr int64_t kSecondsInHour = 3600;
constexpr int64_t kSecondsInDay = 86400;
constexpr int64_t kSecondsInMonth = kSecondsInDay * 31;
constexpr int64_t kRoundingPrecision = 100000000;
constexpr int64_t kMicrosPerSecond = 1000000LL;

inline int32_t GetMaxDayOfMonth(int32_t year, int32_t month)
{
    bool isLeap = Date32::IsLeapYear(year);
    return isLeap ? LEAP_YEAR_OF_DAYS[month] : NORMAL_YEAR_OF_DAYS[month];
}

inline bool IsEndDayOfMonth(const std::tm &tm)
{
    int32_t year = tm.tm_year + 1900;
    int32_t month = tm.tm_mon + 1;
    return tm.tm_mday == GetMaxDayOfMonth(year, month);
}

inline double ComputeMonthsBetween(const std::tm &tm1, const std::tm &tm2, bool roundOff)
{
    const double monthDiff =
        static_cast<double>((tm1.tm_year - tm2.tm_year) * kMonthsInYear + tm1.tm_mon - tm2.tm_mon);

    if (tm1.tm_mday == tm2.tm_mday || (IsEndDayOfMonth(tm1) && IsEndDayOfMonth(tm2))) {
        return monthDiff;
    }

    const int64_t secondsInDay1 =
        static_cast<int64_t>(tm1.tm_hour) * kSecondsInHour +
        static_cast<int64_t>(tm1.tm_min) * kSecondsInMinute +
        static_cast<int64_t>(tm1.tm_sec);
    const int64_t secondsInDay2 =
        static_cast<int64_t>(tm2.tm_hour) * kSecondsInHour +
        static_cast<int64_t>(tm2.tm_min) * kSecondsInMinute +
        static_cast<int64_t>(tm2.tm_sec);
    const int64_t secondsDiff =
        static_cast<int64_t>(tm1.tm_mday - tm2.tm_mday) * kSecondsInDay +
        secondsInDay1 - secondsInDay2;
    const double diff = monthDiff + static_cast<double>(secondsDiff) / static_cast<double>(kSecondsInMonth);

    if (roundOff) {
        return std::round(diff * static_cast<double>(kRoundingPrecision)) / static_cast<double>(kRoundingPrecision);
    }
    return diff;
}

std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row)
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
    } else {
        return std::string_view();
    }
}

/// Converts UTC micros to local-time std::tm using the given timezone.
/// Returns false if the conversion fails.
inline bool MicrosToLocalTm(int64_t micros, const tz::TimeZone *zone, std::tm &out)
{
    Timestamp ts = Timestamp::fromMicros(micros);
    if (zone != nullptr) {
        ts.toTimezone(*zone);
    }
    return Timestamp::epochToCalendarUtc(ts.getSeconds(), out);
}

class MonthsBetweenFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 3) {
            return;
        }

        // Pop timeZoneId if present (4th argument, on top of stack)
        BaseVector *timezoneArg = nullptr;
        if (args.size() >= 4) {
            timezoneArg = args.top();
            args.pop();
        }

        const auto roundOffArg = args.top();
        args.pop();
        const auto timestamp2Arg = args.top();
        args.pop();
        const auto timestamp1Arg = args.top();
        args.pop();

        // Guards cover the const-null early returns below and a throwing tz::locateZone.
        using VectorGuard = std::unique_ptr<BaseVector, void (*)(BaseVector *)>;
        auto deleter = [](BaseVector *v) {
            if (!v->GetIsField()) {
                delete v;
            }
        };
        VectorGuard timezoneGuard(timezoneArg, deleter);
        VectorGuard roundOffGuard(roundOffArg, deleter);
        VectorGuard timestamp2Guard(timestamp2Arg, deleter);
        VectorGuard timestamp1Guard(timestamp1Arg, deleter);

        // Determine batch size from the first non-constant argument,
        // since ConstVector may not carry the correct batch size.
        int32_t size = 0;
        for (const auto *arg : {timestamp1Arg, timestamp2Arg, roundOffArg}) {
            if (arg->GetEncoding() != OMNI_ENCODING_CONST) {
                size = arg->GetSize();
                break;
            }
        }
        if (size == 0) {
            size = timestamp1Arg->GetSize();
        }

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<double> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        // Resolve timezone
        const tz::TimeZone *sessionTimeZone = nullptr;
        if (timezoneArg != nullptr) {
            std::string_view tzView = GetStringValueFromVector(timezoneArg, 0);
            if (!tzView.empty()) {
                sessionTimeZone = tz::locateZone(tzView);
            }
        }

        // Handle roundOff parameter
        bool roundOffIsConst = (roundOffArg->GetEncoding() == OMNI_ENCODING_CONST);
        bool constRoundOff = true;
        const bool *roundOffRaw = nullptr;
        const uint64_t *roundOffNulls = nullptr;

        if (roundOffIsConst) {
            if (roundOffArg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                return;
            }
            constRoundOff = static_cast<ConstVector<bool> *>(roundOffArg)->GetConstValue();
        } else {
            auto *roundOffVector = reinterpret_cast<Vector<bool> *>(roundOffArg);
            roundOffRaw = unsafe::UnsafeVector::GetRawValues(roundOffVector);
            roundOffNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(roundOffArg));
        }

        // Handle timestamp1 (may be constant)
        bool ts1IsConst = (timestamp1Arg->GetEncoding() == OMNI_ENCODING_CONST);
        int64_t constTs1Value = 0;
        const int64_t *ts1Raw = nullptr;
        const uint64_t *ts1Nulls = nullptr;

        if (ts1IsConst) {
            if (timestamp1Arg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                return;
            }
            constTs1Value = static_cast<ConstVector<int64_t> *>(timestamp1Arg)->GetConstValue();
        } else {
            auto *ts1Vector = reinterpret_cast<Vector<int64_t> *>(timestamp1Arg);
            ts1Raw = unsafe::UnsafeVector::GetRawValues(ts1Vector);
            ts1Nulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(timestamp1Arg));
        }

        // Handle timestamp2 (may be constant)
        bool ts2IsConst = (timestamp2Arg->GetEncoding() == OMNI_ENCODING_CONST);
        int64_t constTs2Value = 0;
        const int64_t *ts2Raw = nullptr;
        const uint64_t *ts2Nulls = nullptr;

        if (ts2IsConst) {
            if (timestamp2Arg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                return;
            }
            constTs2Value = static_cast<ConstVector<int64_t> *>(timestamp2Arg)->GetConstValue();
        } else {
            auto *ts2Vector = reinterpret_cast<Vector<int64_t> *>(timestamp2Arg);
            ts2Raw = unsafe::UnsafeVector::GetRawValues(ts2Vector);
            ts2Nulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(timestamp2Arg));
        }

        // Initialize result NULLs
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto nullsSize = BitUtil::Nbytes(size);
        if (ts1IsConst) {
            memset(resultNulls, 0x00, nullsSize);
        } else {
            memcpy(resultNulls, ts1Nulls, nullsSize);
        }

        SelectivityVector rows(size);
        if (ts1IsConst) {
            rows.setAll();
        } else {
            rows.setFromBitsNegate(ts1Nulls, size);
        }

        rows.applyToSelected([&](vector_size_t i) {
            if (!ts2IsConst && ts2Nulls && BitUtil::IsBitSet(ts2Nulls, i)) {
                result->SetNull(i);
                return;
            }
            if (!roundOffIsConst && roundOffNulls && BitUtil::IsBitSet(roundOffNulls, i)) {
                result->SetNull(i);
                return;
            }

            int64_t ts1Micros = ts1IsConst ? constTs1Value : ts1Raw[i];
            int64_t ts2Micros = ts2IsConst ? constTs2Value : ts2Raw[i];
            bool roundOff = roundOffIsConst ? constRoundOff : roundOffRaw[i];

            std::tm tm1;
            std::tm tm2;
            if (!MicrosToLocalTm(ts1Micros, sessionTimeZone, tm1) ||
                !MicrosToLocalTm(ts2Micros, sessionTimeZone, tm2)) {
                result->SetNull(i);
                return;
            }

            resultRaw[i] = ComputeMonthsBetween(tm1, tm2, roundOff);
            result->SetNotNull(i);
        });
    }
};
} // namespace

void RegisterMonthsBetweenFunction(const std::string &name)
{
    auto func = std::make_shared<MonthsBetweenFunction>();

    // 4-parameter signatures: (timestamp1, timestamp2, roundOff, timeZoneId)
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_TIMESTAMP, OMNI_TIMESTAMP, OMNI_BOOLEAN, OMNI_VARCHAR}, OMNI_DOUBLE, func);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_LONG, OMNI_TIMESTAMP, OMNI_BOOLEAN, OMNI_VARCHAR}, OMNI_DOUBLE, func);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_TIMESTAMP, OMNI_LONG, OMNI_BOOLEAN, OMNI_VARCHAR}, OMNI_DOUBLE, func);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_LONG, OMNI_LONG, OMNI_BOOLEAN, OMNI_VARCHAR}, OMNI_DOUBLE, func);

    // 3-parameter signatures: (timestamp1, timestamp2, roundOff) - no timezone, use UTC
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_TIMESTAMP, OMNI_TIMESTAMP, OMNI_BOOLEAN}, OMNI_DOUBLE, func);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_LONG, OMNI_LONG, OMNI_BOOLEAN}, OMNI_DOUBLE, func);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_TIMESTAMP, OMNI_LONG, OMNI_BOOLEAN}, OMNI_DOUBLE, func);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_LONG, OMNI_TIMESTAMP, OMNI_BOOLEAN}, OMNI_DOUBLE, func);
}
}
