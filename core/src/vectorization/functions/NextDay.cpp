/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: NextDay function implementation
 */

#include "NextDay.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>
#include <string>
#include <string_view>
#include <algorithm>
#include <unordered_map>
#include <optional>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

/// Day-of-week encoding aligned with Velox convention:
/// 1970-01-01 is Thursday, so Thursday=0, Friday=1, Saturday=2,
/// Sunday=3, Monday=4, Tuesday=5, Wednesday=6.
/// Supports abbreviated (2-char, 3-char) and full day names, case-insensitive.
static const std::unordered_map<std::string, int8_t> kDayOfWeekNames = {
    {"th", 0},        {"fr", 1},      {"sa", 2},        {"su", 3},
    {"mo", 4},        {"tu", 5},      {"we", 6},        {"thu", 0},
    {"fri", 1},       {"sat", 2},     {"sun", 3},       {"mon", 4},
    {"tue", 5},       {"wed", 6},     {"thursday", 0},  {"friday", 1},
    {"saturday", 2},  {"sunday", 3},  {"monday", 4},    {"tuesday", 5},
    {"wednesday", 6}
};

static ALWAYS_INLINE std::optional<int8_t> getDayOfWeekFromString(const std::string_view &dayOfWeek)
{
    std::string lower(dayOfWeek.size(), '\0');
    std::transform(dayOfWeek.begin(), dayOfWeek.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    auto it = kDayOfWeekNames.find(lower);
    if (it != kDayOfWeekNames.end()) {
        return it->second;
    }
    return std::nullopt;
}

/// Computes the next date after startDay that falls on the given dayOfWeek.
/// startDay: days since epoch (1970-01-01).
/// dayOfWeek: 0-6 (Thu=0, Fri=1, ..., Wed=6), aligned with epoch being Thursday.
static ALWAYS_INLINE int64_t getNextDate(int64_t startDay, int8_t dayOfWeek)
{
    return startDay + 1 + ((dayOfWeek - 1 - startDay) % 7 + 7) % 7;
}

/// NextDayFunction - Returns the first date which is later than startDate and named as dayOfWeek
/// next_day(date, dayOfWeek) -> date
/// Returns the first date which is later than startDate and named as dayOfWeek.
/// Returns null if dayOfWeek is invalid.
/// dayOfWeek is case insensitive and must be one of the following:
/// SU, SUN, SUNDAY, MO, MON, MONDAY, TU, TUE, TUESDAY,
/// WE, WED, WEDNESDAY, TH, THU, THURSDAY, FR, FRI, FRIDAY,
/// SA, SAT, SATURDAY.
/// Supports OMNI_DATE32/OMNI_INT (days since epoch) as date input.
class NextDayFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            return;
        }

        const auto dayOfWeekArg = args.top();
        args.pop();
        const auto dateArg = args.top();
        args.pop();

        const auto size = dateArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        const auto dateTypeId = dateArg->GetTypeId();

        if (dateTypeId == OMNI_DATE32 || dateTypeId == OMNI_INT) {
            auto *dateVector = reinterpret_cast<Vector<int32_t> *>(dateArg);
            const auto *dateRaw = unsafe::UnsafeVector::GetRawValues(dateVector);
            const auto *dateNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(dateArg));

            Vector<LargeStringContainer<std::string_view>> *dowFlatVector = nullptr;
            if (dayOfWeekArg->GetEncoding() != OMNI_ENCODING_CONST) {
                dowFlatVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(dayOfWeekArg);
            }
            const auto *dowNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(dayOfWeekArg));

            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, dateNulls, nullsSize);

            SelectivityVector rows(size);
            rows.setFromBitsNegate(dateNulls, size);

            bool dowIsConst = (dayOfWeekArg->GetEncoding() == OMNI_ENCODING_CONST);
            std::optional<int8_t> constWeekDay = std::nullopt;
            bool constInvalidFormat = false;

            if (dowIsConst) {
                auto *constDowVec = reinterpret_cast<ConstVector<std::string_view> *>(dayOfWeekArg);
                std::string_view dowView = constDowVec->GetConstValue();
                constWeekDay = getDayOfWeekFromString(dowView);
                if (!constWeekDay.has_value()) {
                    constInvalidFormat = true;
                }
            }

            rows.applyToSelected([&](vector_size_t i) {
                if (constInvalidFormat) {
                    result->SetNull(i);
                    return;
                }

                if (!dowIsConst) {
                    if (dowNulls && BitUtil::IsBitSet(dowNulls, i)) {
                        result->SetNull(i);
                        return;
                    }
                }

                std::optional<int8_t> weekDay;
                if (dowIsConst) {
                    weekDay = constWeekDay;
                } else {
                    std::string_view dowView = dowFlatVector->GetValue(i);
                    weekDay = getDayOfWeekFromString(dowView);
                }

                if (!weekDay.has_value()) {
                    result->SetNull(i);
                    return;
                }

                int64_t nextDay = getNextDate(static_cast<int64_t>(dateRaw[i]), weekDay.value());
                if (nextDay != static_cast<int64_t>(static_cast<int32_t>(nextDay))) {
                    result->SetNull(i);
                    return;
                }

                resultRaw[i] = static_cast<int32_t>(nextDay);
                result->SetNotNull(i);
            });
        }
        if (!dayOfWeekArg->GetIsField()) {
            delete dayOfWeekArg;
        }
        if (!dateArg->GetIsField()) {
            delete dateArg;
        }
    }
};
} // namespace

void RegisterNextDayFunction(const std::string &name)
{
    auto nextDayFunction = std::make_shared<NextDayFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_VARCHAR}, OMNI_DATE32, nextDayFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_VARCHAR}, OMNI_DATE32, nextDayFunction);
}
}
