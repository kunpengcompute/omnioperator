/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ToDate function implementation for vectorized execution
 *              to_date(string, format) -> DATE32
 *
 * Mirrors the vectorized get_timestamp (ToTimestampFunction) parsing path: it reuses the
 * Joda/SimpleDateFormat -> strptime conversion and ParseDateTimeString helper to obtain the
 * parsed UTC micros, then converts the micros to days-since-epoch (DATE32) via
 * type::util::toDate. Flink's TO_DATE parses dates in UTC (DATE has no time component and no
 * timezone), so no session-timezone adjustment is applied, matching DateTimeUtils.parseDate.
 */

#include "ToDate.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "type/TimestampConversion.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

// The helpers below (ConvertJodaToStrptime / ParseMillisFromString / ParseDateTimeString /
// GetStringValueFromVector) are copied verbatim from ToTimestamp.cpp so that to_date shares the
// exact same Java/Joda format parsing semantics as get_timestamp. Keeping local copies avoids
// cross-file coupling in the anonymous namespace.

std::string ConvertJodaToStrptime(const std::string &jodaFormat)
{
    std::string result;
    result.reserve(jodaFormat.size() * 2);
    size_t i = 0;
    size_t len = jodaFormat.size();
    while (i < len) {
        char c = jodaFormat[i];
        if (c == '\'') {
            ++i;
            while (i < len && jodaFormat[i] != '\'') {
                result += jodaFormat[i];
                ++i;
            }
            if (i < len) {
                ++i;
            }
        } else if (c == 'y' || c == 'Y') {
            while (i < len && (jodaFormat[i] == 'y' || jodaFormat[i] == 'Y')) {
                ++i;
            }
            result += "%Y";
        } else if (c == 'M') {
            while (i < len && jodaFormat[i] == 'M') {
                ++i;
            }
            result += "%m";
        } else if (c == 'd') {
            while (i < len && jodaFormat[i] == 'd') {
                ++i;
            }
            result += "%d";
        } else if (c == 'H') {
            while (i < len && jodaFormat[i] == 'H') {
                ++i;
            }
            result += "%H";
        } else if (c == 'h') {
            while (i < len && jodaFormat[i] == 'h') {
                ++i;
            }
            result += "%I";
        } else if (c == 'm') {
            while (i < len && jodaFormat[i] == 'm') {
                ++i;
            }
            result += "%M";
        } else if (c == 's') {
            while (i < len && jodaFormat[i] == 's') {
                ++i;
            }
            result += "%S";
        } else if (c == 'S') {
            while (i < len && jodaFormat[i] == 'S') {
                ++i;
            }
            if (!result.empty() && result.back() == '.') {
                result.pop_back();
            }
        } else if (c == 'a') {
            while (i < len && jodaFormat[i] == 'a') {
                ++i;
            }
            result += "%p";
        } else {
            result += c;
            ++i;
        }
    }
    return result;
}

int32_t ParseMillisFromString(const std::string_view &input, const std::string &jodaFormat)
{
    size_t sssPos = jodaFormat.find('S');
    if (sssPos == std::string::npos) {
        return 0;
    }

    size_t dotInInput = input.rfind('.');
    if (dotInInput == std::string_view::npos) {
        return 0;
    }

    std::string fracStr(input.substr(dotInInput + 1));
    while (fracStr.size() < 3) {
        fracStr += '0';
    }
    if (fracStr.size() > 3) {
        fracStr = fracStr.substr(0, 3);
    }

    int32_t millis = 0;
    for (size_t ci = 0; ci < fracStr.size(); ++ci) {
        if (fracStr[ci] < '0' || fracStr[ci] > '9') {
            return 0;
        }
        millis = millis * 10 + (fracStr[ci] - '0');
    }
    return millis;
}

bool ParseDateTimeString(const std::string_view &input, const std::string &jodaFormat,
    int64_t &resultMicros, bool allowTrailingWhitespace = true)
{
    if (input.empty() || jodaFormat.empty()) {
        return false;
    }

    std::string strptimeFormat = ConvertJodaToStrptime(jodaFormat);

    std::string inputStr(input);

    std::string strptimeInput = inputStr;
    size_t dotPos = std::string::npos;
    bool hasFractional = (jodaFormat.find('S') != std::string::npos);
    if (hasFractional) {
        dotPos = inputStr.rfind('.');
        if (dotPos != std::string::npos) {
            strptimeInput = inputStr.substr(0, dotPos);
        }
    }

    struct tm timeInfo = {};
    timeInfo.tm_year = 70;
    timeInfo.tm_mday = 1;
    timeInfo.tm_isdst = -1;

    char *parseEnd = strptime(strptimeInput.c_str(), strptimeFormat.c_str(), &timeInfo);
    if (parseEnd == nullptr) {
        return false;
    }

    if (allowTrailingWhitespace) {
        // LEGACY mode: Allow trailing whitespace (CHAR types are right-padded with spaces)
        const char *remaining = parseEnd;
        while (*remaining == ' ' || *remaining == '\0') {
            if (*remaining == '\0') {
                break;
            }
            ++remaining;
        }
        if (*remaining != '\0') {
            return false;
        }
    } else {
        // CORRECTED mode: Require the entire input to be consumed (JODA behavior)
        if (*parseEnd != '\0') {
            return false;
        }
    }

    int64_t seconds = Timestamp::calendarUtcToEpoch(timeInfo);

    int32_t millis = 0;
    if (hasFractional) {
        millis = ParseMillisFromString(input, jodaFormat);
    }

    resultMicros = seconds * Timestamp::kMicrosecondsInSecond +
        static_cast<int64_t>(millis) * Timestamp::kMicrosecondsInMillisecond;
    return true;
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

// Whether the Joda/SimpleDateFormat pattern encodes any time-of-day component.
// Flink's 1-arg parseDate(String) default format is a pure date pattern ('yyyy-MM-dd'); it also
// truncates the input at the first space so that timestamp strings like '2017-12-12 09:30:00.0'
// are accepted as dates. We mirror that: for pure-date formats, truncate the input at the first
// space before parsing. For formats that already carry a time component, keep the input intact so
// the time portion is consumed by strptime (matches Flink's 2-arg SimpleDateFormat path).
bool FormatHasTimeComponent(const std::string &jodaFormat)
{
    for (char c : jodaFormat) {
        if (c == 'H' || c == 'h' || c == 'm' || c == 's' || c == 'S' || c == 'a' || c == 'K' || c == 'k') {
            return true;
        }
    }
    return false;
}

/// to_date(string, format) -> DATE32
/// Converts a date string to a DATE (days-since-epoch) using the specified Java/SimpleDateFormat
/// format. Returns NULL if parsing fails or input is NULL. The format is the second stack arg.
class ToDateFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("ToDate function Error", "Expected 2 arguments (input, format)");
        }

        auto formatArg = args.top();
        args.pop();
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_INT, size);
        }

        bool formatIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        std::string constFormat;

        if (formatIsConst) {
            auto *constFormatVec = static_cast<ConstVector<std::string_view> *>(formatArg);
            std::string_view formatView = constFormatVec->GetConstValue();
            constFormat = std::string(formatView);
        }

        // to_date does not receive a policy parameter, default to LEGACY (allow trailing whitespace)
        bool isLegacy = true;

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

            std::string format;
            if (formatIsConst) {
                format = constFormat;
            } else {
                std::string_view formatView = GetStringValueFromVector(formatArg, row);
                format = std::string(formatView);
            }

            // Flink 1-arg parseDate(String) truncates the input at the first space so a timestamp
            // string ('2024-01-15 10:30:45') is accepted as a date under the default 'yyyy-MM-dd'
            // format. Mirror that for pure-date formats (no time component) so the default-format
            // path stays byte-for-byte consistent with Flink. Formats that already carry a time
            // component keep the input intact and let strptime consume the time portion.
            std::string_view parseInput = inputStr;
            if (!FormatHasTimeComponent(format)) {
                size_t spacePos = inputStr.find(' ');
                if (spacePos != std::string_view::npos) {
                    parseInput = inputStr.substr(0, spacePos);
                }
            }

            int64_t resultMicros = 0;
            if (ParseDateTimeString(parseInput, format, resultMicros, isLegacy)) {
                // Flink TO_DATE parses dates in UTC (DATE has no timezone); convert the parsed
                // UTC micros to days-since-epoch without any session-timezone adjustment.
                int32_t daysSinceEpoch = type::util::toDate(Timestamp::fromMicros(resultMicros), nullptr);
                auto *resultVec = static_cast<Vector<int32_t> *>(result);
                resultVec->SetValue(row, daysSinceEpoch);
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

} // namespace

void RegisterToDateFunction(const std::string &name)
{
    auto toDateFunction = std::make_shared<ToDateFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, toDateFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR}, OMNI_INT, toDateFunction);
}
}
