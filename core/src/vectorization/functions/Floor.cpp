/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Floor function implementation
 */

#include "Floor.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "type/date32.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>
#include <string>
#include <string_view>
#include <limits>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
constexpr int64_t kSecondsInDay = 86400LL;

class FloorFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            return;
        }

        const auto formatArg = args.top();
        args.pop();
        const auto valueArg = args.top();
        args.pop();

        const auto size = valueArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        const auto valueTypeId = valueArg->GetTypeId();

        const auto *valueNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(valueArg));
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy(resultNulls, valueNulls, nullsSize);

        SelectivityVector rows(size);
        rows.setFromBitsNegate(valueNulls, size);

        bool formatIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        std::string constFormat;
        DateTruncMode constLevel = DateTruncMode::TRUNC_INVALID;

        if (formatIsConst) {
            auto *constFormatVec = reinterpret_cast<ConstVector<std::string_view> *>(formatArg);
            std::string_view formatView = constFormatVec->GetConstValue();
            constFormat = std::string(formatView);
            constLevel = Date32::ParseTruncLevel(constFormat);
        }

        const auto *formatNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(formatArg));
        Vector<LargeStringContainer<std::string_view>> *formatVector = nullptr;
        if (!formatIsConst) {
            formatVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(formatArg);
        }

        if (valueTypeId == OMNI_INT) {
            auto *valueVector = reinterpret_cast<Vector<int32_t> *>(valueArg);
            const auto *valueRaw = unsafe::UnsafeVector::GetRawValues(valueVector);
            auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
            auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

            rows.applyToSelected([&](vector_size_t i) {
                if (!formatIsConst) {
                    if (formatNulls && BitUtil::IsBitSet(formatNulls, i)) {
                        result->SetNull(i);
                        return;
                    }
                }

                DateTruncMode level;
                if (formatIsConst) {
                    level = constLevel;
                } else {
                    std::string_view formatView = formatVector->GetValue(i);
                    std::string formatStr(formatView);
                    level = Date32::ParseTruncLevel(formatStr);
                }

                if (level == DateTruncMode::TRUNC_INVALID) {
                    result->SetNull(i);
                    return;
                }

                int32_t daysSinceEpoch = valueRaw[i];
                Timestamp ts = Timestamp::fromDate(daysSinceEpoch);
                Timestamp truncated;
                if (Timestamp::FloorTime(ts, level, truncated) == CONVERT_SUCCESS) {
                    int64_t resultDays = truncated.getSeconds() / kSecondsInDay;
                    if (resultDays < std::numeric_limits<int32_t>::min() ||
                        resultDays > std::numeric_limits<int32_t>::max()) {
                        result->SetNull(i);
                        return;
                    }
                    resultRaw[i] = static_cast<int32_t>(resultDays);
                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        } else if (valueTypeId == OMNI_LONG) {
            auto *valueVector = reinterpret_cast<Vector<int64_t> *>(valueArg);
            const auto *valueRaw = unsafe::UnsafeVector::GetRawValues(valueVector);
            auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
            auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

            rows.applyToSelected([&](vector_size_t i) {
                if (!formatIsConst) {
                    if (formatNulls && BitUtil::IsBitSet(formatNulls, i)) {
                        result->SetNull(i);
                        return;
                    }
                }

                DateTruncMode level;
                if (formatIsConst) {
                    level = constLevel;
                } else {
                    std::string_view formatView = formatVector->GetValue(i);
                    std::string formatStr(formatView);
                    level = Date32::ParseTruncLevel(formatStr);
                }

                if (level == DateTruncMode::TRUNC_INVALID) {
                    result->SetNull(i);
                    return;
                }

                int64_t millis = valueRaw[i];
                Timestamp ts = Timestamp::fromMillis(millis);
                Timestamp truncated;
                if (Timestamp::FloorTime(ts, level, truncated) == CONVERT_SUCCESS) {
                    resultRaw[i] = truncated.toMillis();

                    result->SetNotNull(i);
                } else {
                    result->SetNull(i);
                }
            });
        }

        delete formatArg;
        delete valueArg;
    }
};
} // namespace

void RegisterFloorFunction(const std::string &name)
{
    auto floorFunction = std::make_shared<FloorFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_LONG, floorFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_VARCHAR}, OMNI_INT, floorFunction);
}
}
