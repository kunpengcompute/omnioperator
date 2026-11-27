/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: LastDay function implementation
 */

#include "LastDay.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>
#include <limits>


namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

constexpr int64_t kSecondsInDay = 86400LL;
constexpr int64_t kMicrosecondsInSecond = 1000000LL;

/// last_day function
/// last_day(date) -> date
/// Returns the last day of the month which the given date belongs to.
/// For example: last_day('2015-03-27') returns '2015-03-31'.
/// Supports OMNI_DATE32 and OMNI_INT (equivalent types at runtime).
/// Handles leap years correctly (e.g., last_day('2016-02-07') returns '2016-02-29').
/// Returns NULL for NULL input.
inline int32_t GetMaxDayOfMonth(int32_t year, int32_t month)
{
    bool isLeap = Date32::IsLeapYear(year);
    return isLeap ? LEAP_YEAR_OF_DAYS[month] : NORMAL_YEAR_OF_DAYS[month];
}

template <typename T>
void LastDayFunctionImpl(BaseVector *&inputArg, BaseVector *&result, vector_size_t size, int64_t calculateValue, bool isDiv)
{
    if (calculateValue == 0) {
        return;
    }
    auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
    auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

    auto *inputVector = reinterpret_cast<Vector<T> *>(inputArg);
    const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
    const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));

    auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
    auto nullsSize = BitUtil::Nbytes(size);
    memcpy(resultNulls, inputNulls, nullsSize);

    SelectivityVector rows(size);
    rows.setFromBitsNegate(inputNulls, size);

    rows.applyToSelected([&](vector_size_t i) {
        T inputRawValue = inputRaw[i];
        int64_t seconds = isDiv ? static_cast<int64_t>(inputRawValue) / calculateValue : static_cast<int64_t>(inputRawValue) * calculateValue;

        std::tm tmValue;
        if (!Timestamp::epochToCalendarUtc(seconds, tmValue)) {
            result->SetNull(i);
            return;
        }

        int32_t year = tmValue.tm_year + 1900;
        int32_t month = tmValue.tm_mon + 1;
        int32_t lastDay = GetMaxDayOfMonth(year, month);

        int64_t resultDays = 0;
        if (!Date32::DaysSinceEpochFromDate(year, month, lastDay, resultDays)) {
            result->SetNull(i);
            return;
        }

        if (resultDays < std::numeric_limits<int32_t>::min() ||
            resultDays > std::numeric_limits<int32_t>::max()) {
            result->SetNull(i);
            return;
        }

        resultRaw[i] = static_cast<int32_t>(resultDays);
        result->SetNotNull(i);
    }); 
}

class LastDayFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.empty()) {
            return;
        }

        auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }

        const auto inputTypeId = inputArg->GetTypeId();

        if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
            LastDayFunctionImpl<int32_t>(inputArg, result, size, kSecondsInDay, false);
        } else if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            LastDayFunctionImpl<int64_t>(inputArg, result, size, kMicrosecondsInSecond, true);
        } else {
            if (inputArg != nullptr) {
                delete inputArg;
            }
            OMNI_THROW("LastDayFunction Error:", "Invalid input type");
        }
        if (inputArg != nullptr) {
            delete inputArg;
        }
    }
};
} // namespace

void RegisterLastDayFunction(const std::string &name)
{
    auto lastDayFunction = std::make_shared<LastDayFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_DATE32, lastDayFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_DATE32, lastDayFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_DATE32, lastDayFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_DATE32, lastDayFunction);
}
}
