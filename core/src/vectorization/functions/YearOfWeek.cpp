/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: YearOfWeek function implementation
 * Returns the ISO year that a given date belongs to according to ISO 8601 week numbering.
 * For most dates this equals the calendar year. However, the last few days of December
 * may belong to the next year's first ISO week, and the first few days of January may
 * belong to the previous year's last ISO week.
 * Supports: DATE32, INT (days since epoch)
 */

#include "YearOfWeek.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
static constexpr int64_t kSecondsPerDay = 86400LL;

/// year_of_week function
/// year_of_week(date) -> int32
/// Returns the ISO 8601 week-numbering year that the given date belongs to.
/// The last few days in December may belong to ISO week 1 of the next year,
/// and the first few days in January may belong to the last ISO week of the
/// previous year.
class YearOfWeekFunction : public VectorFunction {
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
                if (!Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    result->SetNull(i);
                    return;
                }

                int isoWeekDay = (tmValue.tm_wday == 0) ? 7 : tmValue.tm_wday;

                // The last few days in December may belong to the next year if they are
                // in the same week as the next January 1 and this January 1 is a Thursday
                // or before.
                if (tmValue.tm_mon == 11 && tmValue.tm_mday >= 29 &&
                    tmValue.tm_mday - isoWeekDay >= 31 - 3) {
                    resultRaw[i] = static_cast<int32_t>(1900 + tmValue.tm_year + 1);
                    result->SetNotNull(i);
                    return;
                }

                // The first few days in January may belong to the last year if they are
                // in the same week as January 1 and January 1 is a Friday or after.
                if (tmValue.tm_mon == 0 && tmValue.tm_mday <= 3 &&
                    isoWeekDay - (tmValue.tm_mday - 1) >= 5) {
                    resultRaw[i] = static_cast<int32_t>(1900 + tmValue.tm_year - 1);
                    result->SetNotNull(i);
                    return;
                }

                resultRaw[i] = static_cast<int32_t>(1900 + tmValue.tm_year);
                result->SetNotNull(i);
            });
        }
        delete inputArg;
    }
};
} // namespace

void RegisterYearOfWeekFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_INT,
        std::make_shared<YearOfWeekFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<YearOfWeekFunction>());
}
}
