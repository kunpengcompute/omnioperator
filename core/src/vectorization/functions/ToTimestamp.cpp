/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ToTimestamp and ToUnixTimestamp functions implementation for vectorized execution
 */

#include "ToTimestamp.h"
#include "DateTimeZoneConversion.h"
#include "SparkDateTimeFormat.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/config/QueryConfig.h"
#include <ctime>
#include <cstring>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

const tz::TimeZone *getTimeZoneFromConfig(const config::QueryConfig &config)
{
    if (config.AdjustTimestampToTimezone()) {
        const auto sessionTzName = config.SessionTimezone();
        if (!sessionTzName.empty()) {
            return tz::locateZone(sessionTzName);
        }
    }
    return nullptr;
}

int64_t ConvertLocalMicrosToUtc(
    int64_t localMicros,
    const tz::TimeZone *timeZone,
    datetime::LocalToUtcState &state)
{
    if (timeZone == nullptr) {
        return localMicros;
    }
    const Timestamp timestamp = Timestamp::fromMicros(localMicros);
    const auto utcSeconds = datetime::ConvertLocalToUtc(
        std::chrono::seconds(timestamp.getSeconds()), timeZone, &state);
    return utcSeconds.count() * Timestamp::kMicrosecondsInSecond +
        (localMicros % Timestamp::kMicrosecondsInSecond);
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

/// get_timestamp(string, format) -> timestamp
/// Converts a date string to Timestamp type using the specified format.
/// Returns NULL if parsing fails or input is NULL.
class ToTimestampFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("ToTimestamp function Error", "Expected 2 arguments (input, format)");
        }

        auto formatArg = args.top();
        args.pop();
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);
        }

        bool formatIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        datetime::CompiledParseFormat constCompiledFormat;
        // get_timestamp has no policy argument and currently follows LEGACY parsing semantics.
        constexpr bool isLegacy = true;

        if (formatIsConst) {
            auto *constFormatVec = static_cast<ConstVector<std::string_view> *>(formatArg);
            std::string_view formatView = constFormatVec->GetConstValue();
            constCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
        }

        const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());
        datetime::LocalToUtcState timeZoneState;

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            if (!formatIsConst && formatArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view inputStr = GetStringValueFromVector(inputArg, row);

            datetime::CompiledParseFormat rowCompiledFormat;
            const datetime::CompiledParseFormat *compiledFormat = &constCompiledFormat;
            if (!formatIsConst) {
                std::string_view formatView = GetStringValueFromVector(formatArg, row);
                rowCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
                compiledFormat = &rowCompiledFormat;
            }

            int64_t resultMicros = 0;
            if (datetime::ParseDateTimeString(inputStr, *compiledFormat, resultMicros)) {
                resultMicros =
                    ConvertLocalMicrosToUtc(resultMicros, sessionTz, timeZoneState);
                auto *resultVec = static_cast<Vector<int64_t> *>(result);
                resultVec->SetValue(row, resultMicros);
                result->SetNotNull(row);
            } else {
                result->SetNull(row);
            }
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
        if (formatArg != nullptr) {
            delete formatArg;
        }
    }
};

/// to_unix_timestamp(string) -> int64
/// Parses a date string with default format "yyyy-MM-dd HH:mm:ss" and returns unix seconds.
/// to_unix_timestamp(string, format) -> int64
/// Parses a date string with specified format and returns unix seconds.
/// to_unix_timestamp(string, format, timezone, policy) -> int64
/// Parses a date string with format/timezone/policy from Gluten and returns unix seconds.
/// to_unix_timestamp(timestamp) -> int64
/// Extracts seconds from a timestamp value.
/// to_unix_timestamp(date) -> int64
/// Converts a date value to unix seconds.
class ToUnixTimestampFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.empty()) {
            OMNI_THROW("ToUnixTimestamp function Error", "No input arguments");
        }

        size_t argCount = args.size();
        
        if (argCount >= 4) {
            // Gluten path: (timeStr, format, timezone, policy)
            // Args are pushed in order, so stack top is the last arg (policy).
            auto policyArg = args.top();
            args.pop();
            auto tzArg = args.top();
            args.pop();
            auto formatArg = args.top();
            args.pop();
            auto inputArg = args.top();

            DataTypeId inputTypeId = inputArg->GetTypeId();

            const tz::TimeZone *sessionTz = ExtractTimezone(tzArg, context);
            bool isLegacy = ExtractLegacyPolicy(policyArg);

            if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
                delete policyArg;
                delete tzArg;
                delete formatArg;
                ApplyTimestamp(args, outputType, result);
            } else if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
                delete policyArg;
                delete formatArg;
                ApplyDate(args, outputType, result, sessionTz);
                delete tzArg;
            } else {
                // String path with explicit format and timezone
                args.push(formatArg);
                ApplyStringWithFormat(args, outputType, result, context, sessionTz, isLegacy);
                delete policyArg;
                delete tzArg;
            }
        } else if (argCount >= 2) {
            auto formatArg = args.top();
            args.pop();
            auto inputArg = args.top();
            DataTypeId inputTypeId = inputArg->GetTypeId();

            if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
                delete formatArg;
                ApplyTimestamp(args, outputType, result);
            } else if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
                delete formatArg;
                const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());
                ApplyDate(args, outputType, result, sessionTz);
            } else {
                args.push(formatArg);
                ApplyStringWithFormat(args, outputType, result, context);
            }
        } else if (argCount >= 1) {
            auto inputArg = args.top();
            DataTypeId inputTypeId = inputArg->GetTypeId();

            if (inputTypeId == OMNI_VARCHAR || inputTypeId == OMNI_CHAR) {
                ApplyStringDefault(args, outputType, result, context);
            } else if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
                ApplyTimestamp(args, outputType, result);
            } else if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
                const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());
                ApplyDate(args, outputType, result, sessionTz);
            } else {
                args.pop();
                OMNI_THROW("ToUnixTimestamp function Error",
                    "Unsupported input type: " + TypeUtil::TypeToString(inputTypeId));
            }
        } else {
            OMNI_THROW("ToUnixTimestamp function Error",
                "Unexpected number of arguments: " + std::to_string(argCount));
        }
    }

private:
    static constexpr const char *kDefaultFormat = "yyyy-MM-dd HH:mm:ss";

    static const tz::TimeZone *ExtractTimezone(BaseVector *tzArg, op::ExecutionContext *context)
    {
        if (tzArg == nullptr || tzArg->IsNull(0)) {
            return getTimeZoneFromConfig(context->queryConfig());
        }
        std::string_view tzStr = GetStringValueFromVector(tzArg, 0);
        if (tzStr.empty()) {
            return getTimeZoneFromConfig(context->queryConfig());
        }
        std::string tzName(tzStr);
        // Normalize common timezone format: "GMT+08:00" -> "Etc/GMT-8"
        // Note: POSIX and IANA use inverted sign convention
        if (tzName.find("GMT") == 0 || tzName.find("UTC") == 0) {
            const tz::TimeZone *tz = tz::locateZone(tzName, false);
            if (tz != nullptr) {
                return tz;
            }
        }
        const tz::TimeZone *tz = tz::locateZone(tzName, false);
        if (tz != nullptr) {
            return tz;
        }
        return getTimeZoneFromConfig(context->queryConfig());
    }

    // Extract the timeParserPolicy from the policy argument.
    // Returns true if policy is "LEGACY" (allow trailing whitespace),
    // false if policy is "CORRECTED" or "EXCEPTION" (strict parsing, no trailing whitespace).
    //
    // Policy behavior:
    // - LEGACY: Uses legacy formatter (SimpleDateFormat), allows trailing whitespace.
    // - CORRECTED: Uses new formatter (Iso8601), requires entire input consumed.
    //             Returns NULL on parse failure.
    // - EXCEPTION: Uses new formatter (Iso8601), requires entire input consumed.
    //             On parse failure, tries legacy formatter. If legacy succeeds, throws
    //             SparkUpgradeException; if legacy also fails, throws original exception.
    //
    // For EXCEPTION policy, full support requires two-phase parsing (try new, then legacy).
    // Current implementation treats EXCEPTION same as CORRECTED for the first phase.
    static bool ExtractLegacyPolicy(BaseVector *policyArg)
    {
        if (policyArg == nullptr || policyArg->IsNull(0)) {
            return false;  // Default to CORRECTED/EXCEPTION (strict)
        }
        std::string_view policyStr = GetStringValueFromVector(policyArg, 0);
        // Only LEGACY policy allows trailing whitespace.
        // Both CORRECTED and EXCEPTION use strict parsing (no trailing whitespace).
        return (policyStr == "LEGACY");
    }

    void ApplyStringDefault(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result, op::ExecutionContext *context,
        bool isLegacy = true) const
    {
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        }

        static const auto kLegacyDefaultFormat = datetime::CompileParseFormat(kDefaultFormat, true);
        static const auto kCorrectedDefaultFormat = datetime::CompileParseFormat(kDefaultFormat, false);
        const auto &defaultFormat = isLegacy ? kLegacyDefaultFormat : kCorrectedDefaultFormat;
        const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());
        datetime::LocalToUtcState timeZoneState;

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view inputStr = GetStringValueFromVector(inputArg, row);

            int64_t resultMicros = 0;
            if (datetime::ParseDateTimeString(inputStr, defaultFormat, resultMicros)) {
                resultMicros =
                    ConvertLocalMicrosToUtc(resultMicros, sessionTz, timeZoneState);
                int64_t seconds = resultMicros / Timestamp::kMicrosecondsInSecond;
                auto *resultVec = static_cast<Vector<int64_t> *>(result);
                resultVec->SetValue(row, seconds);
                result->SetNotNull(row);
            } else {
                result->SetNull(row);
            }
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
    }

    void ApplyStringWithFormat(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result, op::ExecutionContext *context,
        const tz::TimeZone *explicitTz = nullptr,
        bool isLegacy = true) const
    {
        auto formatArg = args.top();
        args.pop();
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        }

        bool formatIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        datetime::CompiledParseFormat constCompiledFormat;

        if (formatIsConst) {
            auto *constFormatVec = static_cast<ConstVector<std::string_view> *>(formatArg);
            std::string_view formatView = constFormatVec->GetConstValue();
            constCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
        }

        const tz::TimeZone *sessionTz = (explicitTz != nullptr)
            ? explicitTz : getTimeZoneFromConfig(context->queryConfig());
        datetime::LocalToUtcState timeZoneState;

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            if (!formatIsConst && formatArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view inputStr = GetStringValueFromVector(inputArg, row);

            datetime::CompiledParseFormat rowCompiledFormat;
            const datetime::CompiledParseFormat *compiledFormat = &constCompiledFormat;
            if (!formatIsConst) {
                std::string_view formatView = GetStringValueFromVector(formatArg, row);
                rowCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
                compiledFormat = &rowCompiledFormat;
            }

            int64_t resultMicros = 0;
            if (datetime::ParseDateTimeString(inputStr, *compiledFormat, resultMicros)) {
                resultMicros =
                    ConvertLocalMicrosToUtc(resultMicros, sessionTz, timeZoneState);
                int64_t seconds = resultMicros / Timestamp::kMicrosecondsInSecond;
                auto *resultVec = static_cast<Vector<int64_t> *>(result);
                resultVec->SetValue(row, seconds);
                result->SetNotNull(row);
            } else {
                result->SetNull(row);
            }
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
        if (formatArg != nullptr) {
            delete formatArg;
        }
    }

    void ApplyTimestamp(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result) const
    {
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        }

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            auto *inputVec = static_cast<Vector<int64_t> *>(inputArg);
            int64_t micros = inputVec->GetValue(row);
            int64_t seconds = micros / Timestamp::kMicrosecondsInSecond;

            auto *resultVec = static_cast<Vector<int64_t> *>(result);
            resultVec->SetValue(row, seconds);
            result->SetNotNull(row);
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
    }

    void ApplyDate(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result, const tz::TimeZone *sessionTz = nullptr) const
    {
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        }

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            auto *inputVec = static_cast<Vector<int32_t> *>(inputArg);
            int32_t daysSinceEpoch = inputVec->GetValue(row);
            
            // Create timestamp from date and apply timezone correction if needed
            Timestamp ts = Timestamp::fromDate(daysSinceEpoch);
            if (sessionTz != nullptr) {
                // Convert from local timezone to GMT (similar to Velox's toGMTWithGapCorrection)
                // This handles timezone offset and potential gap/ambiguous time issues
                ts.toGMT(*sessionTz);
            }
            
            int64_t seconds = ts.getSeconds();

            auto *resultVec = static_cast<Vector<int64_t> *>(result);
            resultVec->SetValue(row, seconds);
            result->SetNotNull(row);
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
    }
};

} // namespace

void RegisterToTimestampFunction(const std::string &name)
{
    auto toTimestampFunction = std::make_shared<ToTimestampFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP,
        toTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP,
        toTimestampFunction);
}

void RegisterToUnixTimestampFunction(const std::string &name)
{
    auto toUnixTimestampFunction = std::make_shared<ToUnixTimestampFunction>();
    // string input with default format
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    // string input with custom format (2-arg)
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    // string input with format + timezone + policy from Gluten (4-arg)
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_CHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    // timestamp input (with optional format arg from Spark)
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    // timestamp/long input with format + timezone + policy from Gluten (4-arg)
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_TIMESTAMP, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    // date input (with optional format arg from Spark)
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    // date input with format + timezone + policy from Gluten (4-arg)
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_DATE32, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_INT, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
        toUnixTimestampFunction);
}

}
