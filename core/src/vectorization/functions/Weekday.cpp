/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: weekday(date/timestamp) -> int (0=Monday .. 6=Sunday), vectorized implementation
 */

#include "Weekday.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/debug.h"
#include <ctime>
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
static constexpr int64_t kSecondsPerDay = 86400LL;
static constexpr int64_t kMicrosecondsPerSecond = 1000000LL;

/// weekday(date/timestamp) -> int32_t
/// Returns day of week in Spark/Velox convention: 0 = Monday, 1 = Tuesday, ..., 6 = Sunday.
/// tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday => (tm_wday + 6) % 7.
static bool getWeekdayFromEpochSeconds(int64_t seconds, int32_t &weekday)
{
    std::tm tmValue;
    if (!Timestamp::epochToCalendarUtc(seconds, tmValue)) {
        return false;
    }
    // Spark/Velox: 0=Monday, 1=Tuesday, ..., 6=Sunday
    weekday = static_cast<int32_t>((tmValue.tm_wday + 6) % 7);
    return true;
}

class WeekdayFunction : public VectorFunction {
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
        if (result->GetSize() < size) {
            delete inputArg;
            OMNI_THROW("WeekdayFunction Error:", "Result vector size is less than input size: " +
                std::to_string(result->GetSize()) + " < " + std::to_string(size));
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
                int32_t wd;
                if (getWeekdayFromEpochSeconds(seconds, wd)) {
                    resultRaw[i] = wd;
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        } else if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
            rows.applyToSelected([&](vector_size_t i) {
                int64_t micros = inputRaw[i];
                int64_t seconds = micros / kMicrosecondsPerSecond;
                int32_t wd;
                if (getWeekdayFromEpochSeconds(seconds, wd)) {
                    resultRaw[i] = wd;
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        }
        delete inputArg;
    }
};
} // namespace

void RegisterWeekdayFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_INT,
        std::make_shared<WeekdayFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<WeekdayFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_INT,
        std::make_shared<WeekdayFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_INT,
        std::make_shared<WeekdayFunction>());
}
}
