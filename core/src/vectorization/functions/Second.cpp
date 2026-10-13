/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Second function implementation
 */

#include "Second.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/TimeUtils.h"
#include "util/config/QueryConfig.h"
#include "type/tz/TimeZoneMap.h"
#include <ctime>
#include <cstring>
#include "libboundscheck/include/securec.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

static constexpr int64_t kMicrosPerSecond = 1000000LL;

const tz::TimeZone *getTimeZoneFromConfig(const config::QueryConfig &config)
{
    const auto sessionTzName = config.SessionTimezone();
    if (!sessionTzName.empty()) {
        return tz::locateZone(sessionTzName);
    }
    return nullptr;
}

class SecondFunction : public VectorFunction {
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

        if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
            const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));

            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, inputNulls, nullsSize);

            SelectivityVector rows(size);
            rows.setFromBitsNegate(inputNulls, size);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t microseconds = inputRaw[i];
                int64_t seconds = microseconds / kMicrosPerSecond;

                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_sec);
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        }
        delete inputArg;
    }
};

// SecondWithFraction: extract(SECOND FROM timestamp/date) → Decimal(8,6)
// Returns seconds with microsecond fraction as Decimal(8,6).
// The underlying int64_t value = second_of_minute * 1000000 + microsecond_fraction.
// E.g., 30.123456s is stored as 30123456.
// For DATE input (cast to TIMESTAMP at midnight), the result is always 0.000000.
class SecondWithFractionFunction : public VectorFunction {
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

        auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        const auto inputTypeId = inputArg->GetTypeId();

        const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy(resultNulls, inputNulls, nullsSize);

        SelectivityVector rows(size);
        rows.setFromBitsNegate(inputNulls, size);

        if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);

            const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());

            rows.applyToSelected([&](vector_size_t i) {
                int64_t micros = inputRaw[i];
                Timestamp ts = Timestamp::fromMicros(micros);
                std::tm tmValue = util::GetDateTime(ts, sessionTz);
                int32_t sec = static_cast<int32_t>(tmValue.tm_sec);
                // Microsecond fraction within the current second
                int64_t microFraction = micros % kMicrosPerSecond;
                if (microFraction < 0) {
                    microFraction += kMicrosPerSecond;
                }
                resultRaw[i] = static_cast<int64_t>(sec) * kMicrosPerSecond + microFraction;
                result->SetNotNull(i);
            });
        }
        delete inputArg;
    }
};

} // namespace

void RegisterSecondFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_INT,
        std::make_shared<SecondFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_DECIMAL64,
        std::make_shared<SecondWithFractionFunction>());
}
}
