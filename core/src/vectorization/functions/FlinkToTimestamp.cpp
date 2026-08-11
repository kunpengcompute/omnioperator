/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_to_timestamp function implementation
 *
 * flink_to_timestamp(string1[, string2]) -> int64 (OMNI_LONG, millis since epoch)
 *
 * Mirrors Flink's TO_TIMESTAMP(string1[, string2]) semantics:
 *   - 1-arg: parses string1 with default format 'yyyy-MM-dd HH:mm:ss' (lenient:
 *     1-2 digit fields, optional time part, up to 9 fractional-second digits).
 *   - 2-arg: parses string1 with the explicit format string2 (java-time /
 *     Joda pattern: yyyy/MM/dd/HH/mm/ss/S).
 *   - No session timezone is applied — Flink docs say "under the 'UTC+0' time
 *     zone", and DateTimeUtils.parseTimestampData uses
 *     TimestampData.fromLocalDateTime (no tz conversion): the wall-clock fields
 *     are stored verbatim as epoch millis (as if UTC). This DIFFERS from the
 *     existing get_timestamp (ToTimestamp.cpp, Spark semantics) which applies
 *     the session timezone via ConvertLocalMicrosToUtc.
 *   - Returns NULL on parse failure or NULL input (1-arg: exception->null via
 *     codegen wrapTryCatch; 2-arg: internal catch->null).
 *
 * Implementation: reuses datetime::ParseDateTimeString (SparkDateTimeFormat.h),
 * which parses the string and returns epoch MICROSECONDS computed with the
 * wall-clock fields treated as UTC (no timezone shift) — exactly matching
 * Flink's fromLocalDateTime semantics. We then divide by 1000 to get millis
 * (floor, matching Flink TIMESTAMP(3) precision).
 */

#include "FlinkToTimestamp.h"
#include "SparkDateTimeFormat.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

static constexpr const char *kDefaultFormat = "yyyy-MM-dd HH:mm:ss";
static constexpr int64_t kMicrosPerMilli = 1000LL;

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

/// flink_to_timestamp(string1[, string2]) -> int64 (OMNI_LONG, millis)
/// 1-arg: default format 'yyyy-MM-dd HH:mm:ss'.
/// 2-arg: explicit format string2.
/// No session timezone applied — wall-clock stored as UTC millis.
/// Returns NULL on parse failure or NULL input.
class FlinkToTimestampFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.empty()) {
            OMNI_THROW("flink_to_timestamp function Error", "Expected 1 or 2 arguments");
        }

        // Stack top is the last argument: pop right-to-left.
        // 2-arg: (input, format) -> format on top. 1-arg: (input) -> input on top.
        BaseVector *formatArg = nullptr;
        if (args.size() >= 2) {
            formatArg = args.top();
            args.pop();
        }
        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        }
        auto *resultVec = static_cast<Vector<int64_t> *>(result);

        // Determine the format source: explicit formatArg (2-arg) or default (1-arg).
        const bool hasFormatArg = (formatArg != nullptr);
        const bool formatIsConst = hasFormatArg && (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        // get_timestamp/flink_to_timestamp currently follows LEGACY parsing semantics (lenient).
        constexpr bool isLegacy = true;

        // Pre-compile const/default format once.
        datetime::CompiledParseFormat constCompiledFormat;
        if (hasFormatArg) {
            if (formatIsConst) {
                if (!formatArg->IsNull(0)) {
                    std::string_view formatView = GetStringValueFromVector(formatArg, 0);
                    constCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
                }
            }
        } else {
            // 1-arg: default format.
            constCompiledFormat = datetime::CompileParseFormat(kDefaultFormat, isLegacy);
        }

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            if (hasFormatArg && !formatIsConst && formatArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view inputStr = GetStringValueFromVector(inputArg, row);

            datetime::CompiledParseFormat rowCompiledFormat;
            const datetime::CompiledParseFormat *compiledFormat = &constCompiledFormat;
            if (hasFormatArg && !formatIsConst) {
                std::string_view formatView = GetStringValueFromVector(formatArg, row);
                rowCompiledFormat = datetime::CompileParseFormat(formatView, isLegacy);
                compiledFormat = &rowCompiledFormat;
            }

            int64_t resultMicros = 0;
            if (datetime::ParseDateTimeString(inputStr, *compiledFormat, resultMicros)) {
                // No session timezone: resultMicros is already wall-clock-as-UTC epoch micros.
                // Convert to millis (floor, Flink TIMESTAMP(3) precision).
                int64_t resultMillis = resultMicros / kMicrosPerMilli;
                resultVec->SetValue(row, resultMillis);
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

void RegisterFlinkToTimestampFunction(const std::string &name)
{
    auto func = std::make_shared<FlinkToTimestampFunction>();
    // 1-arg: string -> OMNI_LONG (millis), default format yyyy-MM-dd HH:mm:ss.
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR}, OMNI_LONG, func);
    // 2-arg: string + format -> OMNI_LONG (millis).
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_CHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_VARCHAR}, OMNI_LONG, func);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR, OMNI_CHAR}, OMNI_LONG, func);
}

} // namespace omniruntime::vectorization
