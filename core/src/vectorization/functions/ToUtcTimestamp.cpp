/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ToUtcTimestamp and FromUtcTimestamp functions implementation for vectorized execution
 */

#include "ToUtcTimestamp.h"
#include "DateTimeZoneConversion.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

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

/// to_utc_timestamp(timestamp, timezone) -> timestamp
/// Treats the input timestamp as local time in the given timezone and converts it to UTC.
/// DST gaps are corrected by shifting forward to the nearest valid time.
/// Throws if the timezone is unknown.
class ToUtcTimestampVectorFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("ToUtcTimestamp function Error", "Expected 2 arguments (timestamp, timezone)");
        }

        auto timezoneArg = args.top();
        args.pop();
        auto timestampArg = args.top();
        args.pop();

        const auto size = timestampArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);
        }

        bool tzIsConst = (timezoneArg->GetEncoding() == OMNI_ENCODING_CONST);
        const tz::TimeZone *constTimeZone = nullptr;

        if (tzIsConst) {
            std::string_view tzView = GetStringValueFromVector(timezoneArg, 0);
            constTimeZone = tz::locateZone(tzView);
        }

        auto *inputVec = static_cast<Vector<int64_t> *>(timestampArg);
        auto *resultVec = static_cast<Vector<int64_t> *>(result);
        datetime::LocalToUtcState timeZoneState;

        for (int32_t row = 0; row < size; ++row) {
            if (timestampArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            if (!tzIsConst && timezoneArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            const tz::TimeZone *zone = constTimeZone;
            if (!tzIsConst) {
                std::string_view tzView = GetStringValueFromVector(timezoneArg, row);
                zone = tz::locateZone(tzView);
            }

            int64_t micros = inputVec->GetValue(row);
            Timestamp ts = Timestamp::fromMicros(micros);

            auto sysSeconds = datetime::ConvertLocalToUtc(
                std::chrono::seconds(ts.getSeconds()),
                zone,
                tzIsConst ? &timeZoneState : nullptr);
            Timestamp converted(sysSeconds.count(), ts.getNanos());

            resultVec->SetValue(row, converted.toMicros());
            result->SetNotNull(row);
        }

        if (timestampArg != nullptr) {
            delete timestampArg;
        }
        if (timezoneArg != nullptr) {
            delete timezoneArg;
        }
    }
};

/// from_utc_timestamp(timestamp, timezone) -> timestamp
/// Treats the input timestamp as UTC and converts it to local time in the given timezone.
/// Throws if the timezone is unknown.
class FromUtcTimestampVectorFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("FromUtcTimestamp function Error", "Expected 2 arguments (timestamp, timezone)");
        }

        auto timezoneArg = args.top();
        args.pop();
        auto timestampArg = args.top();
        args.pop();

        const auto size = timestampArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);
        }

        bool tzIsConst = (timezoneArg->GetEncoding() == OMNI_ENCODING_CONST);
        const tz::TimeZone *constTimeZone = nullptr;

        if (tzIsConst) {
            std::string_view tzView = GetStringValueFromVector(timezoneArg, 0);
            constTimeZone = tz::locateZone(tzView);
        }

        auto *inputVec = static_cast<Vector<int64_t> *>(timestampArg);
        auto *resultVec = static_cast<Vector<int64_t> *>(result);

        for (int32_t row = 0; row < size; ++row) {
            if (timestampArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            if (!tzIsConst && timezoneArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            const tz::TimeZone *zone = constTimeZone;
            if (!tzIsConst) {
                std::string_view tzView = GetStringValueFromVector(timezoneArg, row);
                zone = tz::locateZone(tzView);
            }

            int64_t micros = inputVec->GetValue(row);
            Timestamp ts = Timestamp::fromMicros(micros);
            ts.toTimezone(*zone);

            resultVec->SetValue(row, ts.toMicros());
            result->SetNotNull(row);
        }

        if (timestampArg != nullptr) {
            delete timestampArg;
        }
        if (timezoneArg != nullptr) {
            delete timezoneArg;
        }
    }
};

} // namespace

void RegisterToUtcTimestampFunction(const std::string &name)
{
    auto func = std::make_shared<ToUtcTimestampVectorFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_TIMESTAMP, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_CHAR}, OMNI_TIMESTAMP, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_TIMESTAMP, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_CHAR}, OMNI_TIMESTAMP, func);
}

void RegisterFromUtcTimestampFunction(const std::string &name)
{
    auto func = std::make_shared<FromUtcTimestampVectorFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_TIMESTAMP, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_CHAR}, OMNI_TIMESTAMP, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_TIMESTAMP, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_CHAR}, OMNI_TIMESTAMP, func);
}

}
