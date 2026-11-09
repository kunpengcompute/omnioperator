/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Trunc function implementation
 */

#include "Trunc.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <chrono>
#include <cstring>
#include <string>
#include <string_view>
#include "libboundscheck/include/securec.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
class TruncFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            return;
        }
        
        // Extract arguments from stack: format (VARCHAR), date (DATE32)
        const auto formatArg = args.top();
        args.pop();
        const auto dateArg = args.top();
        args.pop();
        
        const auto size = dateArg->GetSize();
        
        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }
        
        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        
        // Get input type
        const auto dateTypeId = dateArg->GetTypeId();
        
        if (dateTypeId == OMNI_DATE32 || dateTypeId == OMNI_INT) {
            // Extract date values
            auto *dateVector = reinterpret_cast<Vector<int32_t> *>(dateArg);
            const auto *dateRaw = unsafe::UnsafeVector::GetRawValues(dateVector);
            const auto *dateNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(dateArg));
            
            // Get format vector (may be null if const)
            Vector<LargeStringContainer<std::string_view>> *formatVector = nullptr;
            if (formatArg->GetEncoding() != OMNI_ENCODING_CONST) {
                formatVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(formatArg);
            }
            const auto *formatNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(formatArg));
            
            // Copy NULL bits from date input to result (so NULL rows are already set to NULL)
            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, dateNulls, nullsSize);
            
            // Process only non-NULL rows using SelectivityVector
            SelectivityVector rows(size);
            rows.setFromBitsNegate(dateNulls, size);
            
            // Check if format is constant
            bool formatIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
            std::string constFormat;
            DateTruncMode constLevel = DateTruncMode::TRUNC_INVALID;
            
            if (formatIsConst) {
                auto *constFormatVec = reinterpret_cast<ConstVector<std::string_view> *>(formatArg);
                std::string_view formatView = constFormatVec->GetConstValue();
                constFormat = std::string(formatView);
                constLevel = Date32::ParseTruncLevel(constFormat);
            }
            
            rows.applyToSelected([&](vector_size_t i) {
                // Check if format is NULL (for non-const format)
                if (!formatIsConst) {
                    if (formatNulls && BitUtil::IsBitSet(formatNulls, i)) {
                        result->SetNull(i);
                        return;
                    }
                }
                
                // Get format string
                std::string formatStr;
                DateTruncMode level;
                
                if (formatIsConst) {
                    level = constLevel;
                } else {
                    std::string_view formatView = formatVector->GetValue(i);
                    formatStr = std::string(formatView);
                    level = Date32::ParseTruncLevel(formatStr);
                }
                
                // Check if format is invalid
                if (level == DateTruncMode::TRUNC_INVALID) {
                    result->SetNull(i);
                    return;
                }
                
                // Check if level is valid for date truncation (must be >= WEEK)
                if (level < DateTruncMode::TRUNC_TO_WEEK) {
                    result->SetNull(i);
                    return;
                }
                
                // Perform truncation
                int32_t daysSinceEpoch = dateRaw[i];
                int32_t truncatedDays = 0;
                
                if (Date32::TruncDate(daysSinceEpoch, level, truncatedDays) == CONVERT_SUCCESS) {
                    resultRaw[i] = truncatedDays;
                    result->SetNotNull(i);
                } else {
                    // If truncation fails, set to null
                    result->SetNull(i);
                }
            });
        }
        delete formatArg;
        delete dateArg;
    }
    
private:
    // Helper: Get string value from vector with different encodings
    std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row) const {
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
};
} // namespace

void RegisterTruncFunction(const std::string &name)
{
    // Trunc takes DATE32 and VARCHAR(format) and returns DATE32
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_VARCHAR}, OMNI_DATE32,
        std::make_shared<TruncFunction>());
}

namespace {

int64_t FloorDiv(int64_t a, int64_t b)
{
    if (b == 0) {
        return 0;
    }
    int64_t q = a / b;
    int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        q -= 1;
    }
    return q;
}

void TruncateCalendarFields(std::tm &tmValue, DateTruncMode level)
{
    switch (level) {
        case DateTruncMode::TRUNC_TO_DAY:
            tmValue.tm_hour = 0;
            tmValue.tm_min = 0;
            tmValue.tm_sec = 0;
            break;
        case DateTruncMode::TRUNC_TO_WEEK: {
            int32_t isoWeekday = (tmValue.tm_wday == 0) ? 7 : tmValue.tm_wday;
            tmValue.tm_mday -= (isoWeekday - 1);
            tmValue.tm_hour = 0;
            tmValue.tm_min = 0;
            tmValue.tm_sec = 0;
            break;
        }
        case DateTruncMode::TRUNC_TO_MONTH:
            tmValue.tm_mday = 1;
            tmValue.tm_hour = 0;
            tmValue.tm_min = 0;
            tmValue.tm_sec = 0;
            break;
        case DateTruncMode::TRUNC_TO_QUARTER:
            tmValue.tm_mon = (tmValue.tm_mon / 3) * 3;
            tmValue.tm_mday = 1;
            tmValue.tm_hour = 0;
            tmValue.tm_min = 0;
            tmValue.tm_sec = 0;
            break;
        case DateTruncMode::TRUNC_TO_YEAR:
            tmValue.tm_mon = 0;
            tmValue.tm_mday = 1;
            tmValue.tm_hour = 0;
            tmValue.tm_min = 0;
            tmValue.tm_sec = 0;
            break;
        default:
            break;
    }
}

int64_t LocalMidnightToEpochMicros(int32_t days, const tz::TimeZone *timeZone)
{
    int64_t localEpoch = static_cast<int64_t>(days) * 86400LL;
    if (timeZone == nullptr) {
        return localEpoch * 1000000LL;
    }
    try {
        return static_cast<int64_t>(timeZone->to_sys(std::chrono::seconds(localEpoch)).count()) * 1000000LL;
    } catch (const std::exception &) {
        auto corrected = timeZone->correct_nonexistent_time(std::chrono::seconds(localEpoch));
        return static_cast<int64_t>(timeZone->to_sys(corrected, tz::TimeZone::TChoose::kEarliest).count()) * 1000000LL;
    }
}

int64_t TruncateTimestamp(int64_t timestampMicros, DateTruncMode level, const tz::TimeZone *timeZone)
{
    int64_t utcSeconds = FloorDiv(timestampMicros, 1000000LL);
    std::tm tmValue {};
    int64_t calendarSeconds = utcSeconds;
    if (timeZone != nullptr) {
        calendarSeconds = timeZone->to_local(std::chrono::seconds(utcSeconds)).count();
    }
    if (!Timestamp::epochToCalendarUtc(calendarSeconds, tmValue)) {
        return timestampMicros;
    }
    TruncateCalendarFields(tmValue, level);
    int64_t truncatedLocal = Timestamp::calendarUtcToEpoch(tmValue);
    if (timeZone == nullptr) {
        return truncatedLocal * 1000000LL;
    }
    try {
        return static_cast<int64_t>(timeZone->to_sys(std::chrono::seconds(truncatedLocal)).count()) * 1000000LL;
    } catch (const std::exception &) {
        auto corrected = timeZone->correct_nonexistent_time(std::chrono::seconds(truncatedLocal));
        return static_cast<int64_t>(timeZone->to_sys(corrected, tz::TimeZone::TChoose::kEarliest).count()) * 1000000LL;
    }
}

class DateTruncFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            return;
        }
        // Time zone is the session config. SubstraitToOmniExpr strips the optional
        // third argument so this stays the 2-arg signature used by omni_commercial_1230.
        const auto inputArg = args.top();
        args.pop();
        const auto formatArg = args.top();
        args.pop();
        const auto size = inputArg->GetSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(
                outputType != nullptr ? outputType->GetId() : OMNI_TIMESTAMP, size);
        }
        const tz::TimeZone *timeZone = nullptr;
        if (context != nullptr) {
            const auto sessionTzName = context->queryConfig().SessionTimezone();
            if (!sessionTzName.empty()) {
                timeZone = tz::locateZone(sessionTzName);
            }
        }
        std::string_view format = GetString(formatArg, 0);
        DateTruncMode level = Date32::ParseTruncLevel(std::string(format));
        const auto inputType = inputArg->GetTypeId();
        auto *resultVector = static_cast<Vector<int64_t> *>(result);
        for (int32_t row = 0; row < size; ++row) {
            if (level == DateTruncMode::TRUNC_INVALID || inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            if (inputType == OMNI_DATE32 || inputType == OMNI_INT) {
                int32_t days = VectorHelper::GetValueFromVector<int32_t>(inputArg, row);
                int32_t truncated = days;
                if (level >= DateTruncMode::TRUNC_TO_WEEK) {
                    if (Date32::TruncDate(days, level, truncated) != type::Status::CONVERT_SUCCESS) {
                        result->SetNull(row);
                        continue;
                    }
                }
                resultVector->SetValue(row, LocalMidnightToEpochMicros(truncated, timeZone));
            } else {
                int64_t micros = VectorHelper::GetValueFromVector<int64_t>(inputArg, row);
                resultVector->SetValue(row, TruncateTimestamp(micros, level, timeZone));
            }
        }
        delete formatArg;
        delete inputArg;
    }

private:
    static std::string_view GetString(BaseVector *vec, int32_t row)
    {
        Encoding encoding = vec->GetEncoding();
        if (encoding == OMNI_ENCODING_CONST) {
            return static_cast<ConstVector<std::string_view> *>(vec)->GetConstValue();
        }
        if (encoding == OMNI_FLAT) {
            return static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec)->GetValue(row);
        }
        if (encoding == OMNI_DICTIONARY) {
            return static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec)->GetValue(row);
        }
        return std::string_view();
    }
};

} // namespace

void RegisterDateTruncFunction(const std::string &name)
{
    auto fn = std::make_shared<DateTruncFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_TIMESTAMP}, OMNI_TIMESTAMP, fn);
    // omni_commercial_1230 returns DATE32 for a date input. These Spark cases
    // return timestamp for both date and timestamp inputs.
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_DATE32}, OMNI_TIMESTAMP, fn);
}
}
