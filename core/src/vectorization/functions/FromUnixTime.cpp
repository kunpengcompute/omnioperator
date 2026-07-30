/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: FromUnixTime function implementation for vectorized execution.
 */

#include "FromUnixTime.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "DateTimeZoneConversion.h"
#include "SparkDateTimeFormat.h"
#include "util/bit_util.h"
#include "util/config/QueryConfig.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

constexpr int64_t kMicrosPerSecond = 1'000'000;

struct SparkTimestampParts {
    int64_t seconds;
    int32_t microsOfSecond;
};

int64_t WrapToSparkLong(__int128_t value)
{
    constexpr __int128_t kUint64Modulus =
        static_cast<__int128_t>(std::numeric_limits<uint64_t>::max()) + 1;
    constexpr __int128_t kInt64SignBit = static_cast<__int128_t>(1) << 63;
    __int128_t wrapped = value % kUint64Modulus;
    if (wrapped < 0) {
        wrapped += kUint64Modulus;
    }
    if (wrapped >= kInt64SignBit) {
        wrapped -= kUint64Modulus;
    }
    return static_cast<int64_t>(wrapped);
}

int64_t FloorDiv(int64_t dividend, int64_t divisor)
{
    int64_t quotient = dividend / divisor;
    const int64_t remainder = dividend % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

SparkTimestampParts ConvertUnixSecondsToSparkTimestamp(int64_t unixSeconds)
{
    constexpr int64_t kMinSafeUnixSeconds =
        std::numeric_limits<int64_t>::min() / kMicrosPerSecond;
    constexpr int64_t kMaxSafeUnixSeconds =
        std::numeric_limits<int64_t>::max() / kMicrosPerSecond;
    if (unixSeconds >= kMinSafeUnixSeconds && unixSeconds <= kMaxSafeUnixSeconds) {
        return {unixSeconds, 0};
    }
    const int64_t micros = WrapToSparkLong(static_cast<__int128_t>(unixSeconds) * kMicrosPerSecond);
    const int64_t seconds = FloorDiv(micros, kMicrosPerSecond);
    return {seconds, static_cast<int32_t>(micros - seconds * kMicrosPerSecond)};
}

int64_t GetLongValueFromVector(BaseVector *vector, int32_t row)
{
    switch (vector->GetEncoding()) {
        case OMNI_ENCODING_CONST:
            return static_cast<ConstVector<int64_t> *>(vector)->GetConstValue();
        case OMNI_FLAT:
            return static_cast<Vector<int64_t> *>(vector)->GetValue(row);
        case OMNI_DICTIONARY:
            return static_cast<Vector<DictionaryContainer<int64_t>> *>(vector)->GetValue(row);
        default:
            return 0;
    }
}

std::string ToUpperAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

bool IsLegacyPolicy(std::string_view policy)
{
    return ToUpperAscii(std::string(policy)) == "LEGACY";
}

std::string_view GetStringValueFromVector(BaseVector *vector, int32_t row)
{
    switch (vector->GetEncoding()) {
        case OMNI_ENCODING_CONST:
            return static_cast<ConstVector<std::string_view> *>(vector)->GetConstValue();
        case OMNI_FLAT:
            return static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->GetValue(row);
        case OMNI_DICTIONARY:
            return static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector)->
                GetValue(row);
        default:
            return {};
    }
}

std::string GetSessionTimeZoneName(const config::QueryConfig &config)
{
    return config.AdjustTimestampToTimezone() ? config.SessionTimezone() : std::string{};
}

class FromUnixTimeFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        const size_t argCount = args.size();
        if (argCount < 2) {
            OMNI_THROW("FromUnixTime function Error", "Expected at least 2 arguments (unixtime, format)");
        }

        BaseVector *policyArg = nullptr;
        BaseVector *timeZoneArg = nullptr;
        BaseVector *formatArg = nullptr;
        BaseVector *inputArg = nullptr;
        if (argCount == 4) {
            policyArg = args.top();
            args.pop();
            timeZoneArg = args.top();
            args.pop();
            formatArg = args.top();
            args.pop();
            inputArg = args.top();
            args.pop();
        } else if (argCount == 3) {
            timeZoneArg = args.top();
            args.pop();
            formatArg = args.top();
            args.pop();
            inputArg = args.top();
            args.pop();
        } else if (argCount == 2) {
            formatArg = args.top();
            args.pop();
            inputArg = args.top();
            args.pop();
        } else {
            OMNI_THROW("FromUnixTime function Error", "Expected 2, 3, or 4 arguments");
        }

        const int32_t size = inputArg->GetSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
        }
        auto *resultVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
        const auto inputType = inputArg->GetTypeId();
        const auto inputEncoding = inputArg->GetEncoding();
        const bool inputSupported =
            (inputType == OMNI_LONG || inputType == OMNI_TIMESTAMP) &&
            (inputEncoding == OMNI_ENCODING_CONST ||
             inputEncoding == OMNI_FLAT ||
             inputEncoding == OMNI_DICTIONARY);

        const bool formatIsConst = formatArg->GetEncoding() == OMNI_ENCODING_CONST;
        bool constFormatValid = true;
        std::string constFormatString;
        if (formatIsConst) {
            if (formatArg->IsNull(0)) {
                constFormatValid = false;
            } else {
                constFormatString = std::string(GetStringValueFromVector(formatArg, 0));
            }
        }

        const bool policyIsConst = policyArg == nullptr || policyArg->GetEncoding() == OMNI_ENCODING_CONST;
        std::string constPolicy = "CORRECTED";
        if (policyArg != nullptr && policyIsConst && !policyArg->IsNull(0)) {
            constPolicy = ToUpperAscii(std::string(GetStringValueFromVector(policyArg, 0)));
        }
        const bool constLegacyPolicy = constPolicy == "LEGACY";

        datetime::CompiledFormatPattern constCorrectedFormat;
        datetime::CompiledFormatPattern constLegacyFormat;
        if (formatIsConst && constFormatValid) {
            if (!policyIsConst || !constLegacyPolicy) {
                constCorrectedFormat = datetime::CompileFormatPattern(
                    constFormatString, datetime::FormatterKind::FROM_UNIXTIME, false);
            }
            if (!policyIsConst || constLegacyPolicy) {
                constLegacyFormat = datetime::CompileFormatPattern(
                    constFormatString, datetime::FormatterKind::FROM_UNIXTIME, true);
            }
        }

        const std::string sessionTimeZone = GetSessionTimeZoneName(context->queryConfig());
        const bool timeZoneIsConst = timeZoneArg == nullptr || timeZoneArg->GetEncoding() == OMNI_ENCODING_CONST;
        std::string constTimeZoneName;
        if (timeZoneArg != nullptr && timeZoneIsConst && !timeZoneArg->IsNull(0)) {
            constTimeZoneName = std::string(GetStringValueFromVector(timeZoneArg, 0));
        }
        if (constTimeZoneName.empty()) {
            constTimeZoneName = sessionTimeZone;
        }
        const auto constResolvedTimeZone = datetime::ResolveTimeZone(constTimeZoneName, false);

        std::vector<char> formatBuffer(64);
        datetime::UtcToLocalState timeZoneState;
        auto formatRow = [&](int32_t row,
                             int64_t unixSeconds,
                             const datetime::CompiledFormatPattern &compiledFormat,
                             const datetime::ResolvedTimeZone &resolvedTimeZone) {
            const SparkTimestampParts timestampParts = ConvertUnixSecondsToSparkTimestamp(unixSeconds);
            datetime::CalendarTime calendarTime;
            if (!datetime::ToCalendar(
                timestampParts.seconds,
                timestampParts.microsOfSecond,
                resolvedTimeZone,
                calendarTime,
                compiledFormat.requiresZoneName,
                &timeZoneState)) {
                result->SetNull(row);
                return;
            }

            const size_t suggestedCapacity = std::max<size_t>(compiledFormat.maxResultSize, 64);
            if (formatBuffer.size() < suggestedCapacity) {
                formatBuffer.resize(suggestedCapacity);
            }
            const int32_t length = datetime::FormatDateTimeToBuffer(
                calendarTime,
                resolvedTimeZone,
                compiledFormat,
                formatBuffer.data(),
                formatBuffer.size());
            if (length < 0) {
                const std::string formatted =
                    datetime::FormatDateTime(calendarTime, resolvedTimeZone, compiledFormat);
                if (formatted.empty()) {
                    result->SetNull(row);
                    return;
                }
                resultVector->SetValue(row, std::string_view(formatted));
                result->SetNotNull(row);
                return;
            }
            if (length == 0) {
                result->SetNull(row);
                return;
            }
            resultVector->SetValue(
                row, std::string_view(formatBuffer.data(), static_cast<size_t>(length)));
            result->SetNotNull(row);
        };

        const bool useFlatConstHotPath =
            inputSupported &&
            inputEncoding == OMNI_FLAT &&
            formatIsConst &&
            constFormatValid &&
            policyIsConst &&
            timeZoneIsConst;
        if (useFlatConstHotPath) {
            const auto &compiledFormat =
                constLegacyPolicy ? constLegacyFormat : constCorrectedFormat;
            const size_t suggestedCapacity = std::max<size_t>(compiledFormat.maxResultSize, 64);
            if (formatBuffer.size() < suggestedCapacity) {
                formatBuffer.resize(suggestedCapacity);
            }
            auto *inputVector = static_cast<Vector<int64_t> *>(inputArg);
            const int64_t *inputValues = unsafe::UnsafeVector::GetRawValues(inputVector);
            const uint64_t *inputNulls =
                reinterpret_cast<const uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));
            const bool inputHasNull = inputArg->HasNull();
            for (int32_t row = 0; row < size; ++row) {
                if (inputHasNull && BitUtil::IsBitSet(inputNulls, row)) {
                    result->SetNull(row);
                    continue;
                }
                formatRow(row, inputValues[row], compiledFormat, constResolvedTimeZone);
            }

            delete inputArg;
            delete formatArg;
            if (timeZoneArg != nullptr) {
                delete timeZoneArg;
            }
            if (policyArg != nullptr) {
                delete policyArg;
            }
            return;
        }

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            if (!formatIsConst && formatArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            if (formatIsConst && !constFormatValid) {
                result->SetNull(row);
                continue;
            }

            if (!inputSupported) {
                result->SetNull(row);
                continue;
            }
            const int64_t unixSeconds = GetLongValueFromVector(inputArg, row);

            bool legacyPolicy = constLegacyPolicy;
            if (policyArg != nullptr && !policyIsConst) {
                legacyPolicy = !policyArg->IsNull(row) && IsLegacyPolicy(GetStringValueFromVector(policyArg, row));
            }

            datetime::CompiledFormatPattern rowFormat;
            const datetime::CompiledFormatPattern *compiledFormat = nullptr;
            if (formatIsConst) {
                compiledFormat = legacyPolicy ? &constLegacyFormat : &constCorrectedFormat;
            } else {
                rowFormat = datetime::CompileFormatPattern(
                    GetStringValueFromVector(formatArg, row),
                    datetime::FormatterKind::FROM_UNIXTIME,
                    legacyPolicy);
                compiledFormat = &rowFormat;
            }

            datetime::ResolvedTimeZone dynamicTimeZone;
            const datetime::ResolvedTimeZone *rowTimeZone = &constResolvedTimeZone;
            if (!timeZoneIsConst) {
                std::string rowTimeZoneName;
                if (!timeZoneArg->IsNull(row)) {
                    rowTimeZoneName = std::string(GetStringValueFromVector(timeZoneArg, row));
                }
                if (rowTimeZoneName.empty()) {
                    rowTimeZoneName = sessionTimeZone;
                }
                dynamicTimeZone = datetime::ResolveTimeZone(rowTimeZoneName, false);
                rowTimeZone = &dynamicTimeZone;
            }

            formatRow(row, unixSeconds, *compiledFormat, *rowTimeZone);
        }

        delete inputArg;
        delete formatArg;
        if (timeZoneArg != nullptr) {
            delete timeZoneArg;
        }
        if (policyArg != nullptr) {
            delete policyArg;
        }
    }
};

} // namespace

void RegisterFromUnixTimeFunction(const std::string &name)
{
    auto function = std::make_shared<FromUnixTimeFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR, function);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_VARCHAR, function);
    VectorFunction::RegisterVectorFunction(
        name, {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, function);
    VectorFunction::RegisterVectorFunction(
        name, {OMNI_TIMESTAMP, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, function);
    VectorFunction::RegisterVectorFunction(
        name, {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, function);
    VectorFunction::RegisterVectorFunction(
        name, {OMNI_TIMESTAMP, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, function);
}

} // namespace omniruntime::vectorization
