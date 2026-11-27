/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Year function implementation
 * Extracts the year from date or timestamp values.
 * Supports: DATE32, INT (days since epoch), TIMESTAMP, LONG (microseconds since epoch)
 */

#include "Year.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/TimeUtils.h"
#include "util/config/QueryConfig.h"
#include "type/tz/TimeZoneMap.h"
#include <ctime>
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
static constexpr int64_t kSecondsPerDay = 86400LL;
static constexpr int64_t kMicrosecondsPerSecond = 1000000LL;

const tz::TimeZone *getTimeZoneFromConfig(const config::QueryConfig &config)
{
    const auto sessionTzName = config.SessionTimezone();
    if (!sessionTzName.empty()) {
        return tz::locateZone(sessionTzName);
    }
    return nullptr;
}

/// year function
/// year(date) -> int32, year(timestamp) -> int32
/// Extracts the year from a date (days since epoch) or timestamp (microseconds since epoch).
/// For timestamp input, session timezone is applied (following Velox Spark SQL behavior).
/// Returns NULL if the input is NULL.
class YearFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.empty()) {
            return;
        }

        const auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        const auto inputTypeId = inputArg->GetTypeId();

        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy(resultNulls, inputNulls, nullsSize);

        SelectivityVector rows(size);
        rows.setFromBitsNegate(inputNulls, size);

        if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
            auto *inputVector = reinterpret_cast<Vector<int32_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            rows.applyToSelected([&](vector_size_t i) {
                int32_t daysSinceEpoch = inputRaw[i];
                int64_t seconds = static_cast<int64_t>(daysSinceEpoch) * kSecondsPerDay;

                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_year + 1900);
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        } else if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());

            rows.applyToSelected([&](vector_size_t i) {
                int64_t microseconds = inputRaw[i];
                Timestamp ts = Timestamp::fromMicros(microseconds);
                std::tm tmValue = util::GetDateTime(ts, sessionTz);
                resultRaw[i] = static_cast<int32_t>(tmValue.tm_year + 1900);
                result->SetNotNull(i);
            });
        }
        delete inputArg;
    }
};
} // namespace

void RegisterYearFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_INT,
        std::make_shared<YearFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<YearFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_INT,
        std::make_shared<YearFunction>());
}
}
