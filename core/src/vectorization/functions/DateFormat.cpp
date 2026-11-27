/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateFormat function implementation
 * Formats a timestamp value into a string according to the format specifier.
 * Follows Velox Spark SQL behavior: date_format(timestamp, format) -> varchar
 */

#include "DateFormat.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/TimeUtils.h"
#include "util/config/QueryConfig.h"
#include <ctime>
#include <cstring>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

constexpr size_t kMaxFormatBufferSize = 256;

const tz::TimeZone *GetTimeZoneFromConfig(const config::QueryConfig &config)
{
    const auto sessionTzName = config.SessionTimezone();
    if (!sessionTzName.empty()) {
        return tz::locateZone(sessionTzName);
    }
    return nullptr;
}

/// Convert JODA-like format tokens (yyyy, MM, dd, HH, mm, ss) to strftime format.
/// Aligns with OmniOperator codegen layer (datetime_functions.cpp:toOmniTimeFormat).
std::string ToStrftimeFormat(std::string_view format)
{
    std::string result(format);
    const std::pair<std::string, std::string> replacements[] = {
        {"yyyy", "%Y"}, {"MM", "%m"}, {"dd", "%d"},
        {"HH", "%H"},   {"mm", "%M"}, {"ss", "%S"}};
    for (const auto &[from, to] : replacements) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    return result;
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
    }
    return std::string_view();
}

/// DateFormat function
/// date_format(timestamp, format) -> varchar
/// Converts timestamp to string using the given format pattern.
class DateFormatFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("DateFormat error:", "date_format requires exactly 2 arguments");
        }

        const auto formatArg = args.top();
        args.pop();
        const auto timestampArg = args.top();
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
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }

        auto *resultFlatVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);

        const tz::TimeZone *sessionTz = GetTimeZoneFromConfig(context->queryConfig());

        bool tsIsConst = (timestampArg->GetEncoding() == OMNI_ENCODING_CONST);
        int64_t constTsValue = 0;
        const int64_t *tsRaw = nullptr;
        const uint64_t *tsNulls = nullptr;

        if (tsIsConst) {
            if (timestampArg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                delete timestampArg;
                delete formatArg;
                return;
            }
            constTsValue = static_cast<ConstVector<int64_t> *>(timestampArg)->GetConstValue();
        } else {
            auto *tsVector = reinterpret_cast<Vector<int64_t> *>(timestampArg);
            tsRaw = unsafe::UnsafeVector::GetRawValues(tsVector);
            tsNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(timestampArg));
        }

        bool fmtIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        std::string_view constFormatView;
        std::string constStrftimeFormat;
        const uint64_t *fmtNulls = nullptr;

        if (fmtIsConst) {
            if (formatArg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                delete timestampArg;
                delete formatArg;
                return;
            }
            constFormatView = GetStringValueFromVector(formatArg, 0);
            constStrftimeFormat = ToStrftimeFormat(constFormatView);
        } else {
            fmtNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(formatArg));
        }

        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto nullsSize = BitUtil::Nbytes(size);
        if (tsIsConst) {
            memset(resultNulls, 0x00, nullsSize);
        } else {
            memcpy(resultNulls, tsNulls, nullsSize);
        }

        SelectivityVector rows(size);
        if (tsIsConst) {
            rows.setAll();
        } else {
            rows.setFromBitsNegate(tsNulls, size);
        }

        rows.applyToSelected([&](vector_size_t i) {
            if (!fmtIsConst && fmtNulls && BitUtil::IsBitSet(fmtNulls, i)) {
                result->SetNull(i);
                return;
            }

            int64_t micros = tsIsConst ? constTsValue : tsRaw[i];
            Timestamp ts = Timestamp::fromMicros(micros);
            std::tm tmValue = util::GetDateTime(ts, sessionTz);

            std::string strftimeFormat;
            if (fmtIsConst) {
                strftimeFormat = constStrftimeFormat;
            } else {
                std::string_view fmtView = GetStringValueFromVector(formatArg, i);
                strftimeFormat = ToStrftimeFormat(fmtView);
            }

            char buffer[kMaxFormatBufferSize];
            size_t ret = strftime(buffer, sizeof(buffer), strftimeFormat.c_str(), &tmValue);
            if (ret == 0) {
                result->SetNull(i);
                return;
            }

            std::string_view resultView(buffer, ret);
            resultFlatVector->SetValue(i, resultView);
            result->SetNotNull(i);
        });

        delete timestampArg;
        delete formatArg;
    }
};
} // namespace

void RegisterDateFormatFunction(const std::string &name)
{
    auto func = std::make_shared<DateFormatFunction>();

    // (TIMESTAMP, VARCHAR) -> VARCHAR
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_VARCHAR, func);
    // (LONG, VARCHAR) -> VARCHAR (OMNI_TIMESTAMP and OMNI_LONG are equivalent at runtime)
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR, func);
}
}
