/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateFormat function implementation.
 */

#include "DateFormat.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "DateTimeZoneConversion.h"
#include "SparkDateTimeFormat.h"
#include "util/bit_util.h"
#include "util/config/QueryConfig.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "vectorization/SelectivityVector.h"
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

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

datetime::ResolvedTimeZone GetSessionTimeZone(const config::QueryConfig &config)
{
    if (!config.AdjustTimestampToTimezone()) {
        return {};
    }
    return datetime::ResolveTimeZone(config.SessionTimezone());
}

class DateFormatFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("DateFormat function Error", "date_format requires exactly 2 arguments");
        }

        BaseVector *formatArg = args.top();
        args.pop();
        BaseVector *timestampArg = args.top();
        args.pop();

        int32_t size = 0;
        for (const auto *arg : {timestampArg, formatArg}) {
            if (arg->GetEncoding() != OMNI_ENCODING_CONST) {
                size = arg->GetSize();
                break;
            }
        }
        if (size == 0) {
            size = timestampArg->GetSize();
        }
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }
        auto *resultVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);

        const auto resolvedTimeZone = GetSessionTimeZone(context->queryConfig());

        const bool timestampIsConst = timestampArg->GetEncoding() == OMNI_ENCODING_CONST;
        int64_t constTimestamp = 0;
        const int64_t *timestampValues = nullptr;
        const uint64_t *timestampNulls = nullptr;
        if (timestampIsConst) {
            if (timestampArg->IsNull(0)) {
                std::memset(unsafe::UnsafeBaseVector::GetNulls(result), 0xFF, BitUtil::Nbytes(size));
                delete timestampArg;
                delete formatArg;
                return;
            }
            constTimestamp = static_cast<ConstVector<int64_t> *>(timestampArg)->GetConstValue();
        } else {
            auto *timestampVector = reinterpret_cast<Vector<int64_t> *>(timestampArg);
            timestampValues = unsafe::UnsafeVector::GetRawValues(timestampVector);
            timestampNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(timestampArg));
        }

        const bool formatIsConst = formatArg->GetEncoding() == OMNI_ENCODING_CONST;
        datetime::CompiledFormatPattern constFormat;
        bool constFormatValid = true;
        const uint64_t *formatNulls = nullptr;
        if (formatIsConst) {
            if (formatArg->IsNull(0)) {
                std::memset(unsafe::UnsafeBaseVector::GetNulls(result), 0xFF, BitUtil::Nbytes(size));
                delete timestampArg;
                delete formatArg;
                return;
            }
            try {
                constFormat = datetime::CompileFormatPattern(
                    GetStringValueFromVector(formatArg, 0), datetime::FormatterKind::DATE_FORMAT, false);
            } catch (...) {
                constFormatValid = false;
            }
        } else {
            formatNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(formatArg));
        }

        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        if (timestampIsConst) {
            std::memset(resultNulls, 0, BitUtil::Nbytes(size));
        } else {
            std::memcpy(resultNulls, timestampNulls, BitUtil::Nbytes(size));
        }

        SelectivityVector rows(size);
        if (timestampIsConst) {
            rows.setAll();
        } else {
            rows.setFromBitsNegate(timestampNulls, size);
        }

        std::vector<char> formatBuffer(64);
        datetime::UtcToLocalState timeZoneState;
        rows.applyToSelected([&](vector_size_t row) {
            if (!constFormatValid) {
                result->SetNull(row);
                return;
            }
            if (!formatIsConst && formatNulls != nullptr && BitUtil::IsBitSet(formatNulls, row)) {
                result->SetNull(row);
                return;
            }

            const int64_t micros = timestampIsConst ? constTimestamp : timestampValues[row];
            int64_t seconds = micros / Timestamp::kMicrosecondsInSecond;
            int64_t microsOfSecond = micros % Timestamp::kMicrosecondsInSecond;
            if (microsOfSecond < 0) {
                microsOfSecond += Timestamp::kMicrosecondsInSecond;
                --seconds;
            }

            try {
                datetime::CompiledFormatPattern rowFormat;
                const datetime::CompiledFormatPattern *compiledFormat = &constFormat;
                if (!formatIsConst) {
                    rowFormat = datetime::CompileFormatPattern(
                        GetStringValueFromVector(formatArg, row), datetime::FormatterKind::DATE_FORMAT, false);
                    compiledFormat = &rowFormat;
                }

                datetime::CalendarTime calendarTime;
                if (!datetime::ToCalendar(
                    seconds,
                    static_cast<int32_t>(microsOfSecond),
                    resolvedTimeZone,
                    calendarTime,
                    compiledFormat->requiresZoneName,
                    &timeZoneState)) {
                    result->SetNull(row);
                    return;
                }
                const size_t suggestedCapacity = std::max<size_t>(compiledFormat->maxResultSize, 64);
                if (formatBuffer.size() < suggestedCapacity) {
                    formatBuffer.resize(suggestedCapacity);
                }
                const int32_t length = datetime::FormatDateTimeToBuffer(
                    calendarTime,
                    resolvedTimeZone,
                    *compiledFormat,
                    formatBuffer.data(),
                    formatBuffer.size());
                if (length < 0) {
                    const std::string formatted =
                        datetime::FormatDateTime(calendarTime, resolvedTimeZone, *compiledFormat);
                    resultVector->SetValue(row, std::string_view(formatted));
                } else {
                    resultVector->SetValue(
                        row, std::string_view(formatBuffer.data(), static_cast<size_t>(length)));
                }
                result->SetNotNull(row);
            } catch (...) {
                result->SetNull(row);
            }
        });

        delete timestampArg;
        delete formatArg;
    }
};

} // namespace

void RegisterDateFormatFunction(const std::string &name)
{
    auto function = std::make_shared<DateFormatFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_VARCHAR, function);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR, function);
}

} // namespace omniruntime::vectorization
