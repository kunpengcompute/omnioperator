/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Hour function implementation
 * Extracts the hour of day (0-23) from timestamp values.
 * Supports: TIMESTAMP type (following Velox Spark SQL behavior)
 */

#include "Hour.h"
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

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

const tz::TimeZone *getTimeZoneFromConfig(const config::QueryConfig &config)
{
    const auto sessionTzName = config.SessionTimezone();
    if (!sessionTzName.empty()) {
        return tz::locateZone(sessionTzName);
    }
    return nullptr;
}

/// Hour function
/// hour(timestamp) -> int32
/// Extracts the hour of day (0-23) from a timestamp value.
/// Returns NULL if the input is NULL.
class HourFunction : public VectorFunction {
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
        
        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }
        
        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        
        // Get input type
        const auto inputTypeId = inputArg->GetTypeId();
        
        const tz::TimeZone *sessionTz = getTimeZoneFromConfig(context->queryConfig());
        
        // TIMESTAMP is represented as OMNI_TIMESTAMP or OMNI_LONG (int64_t) at runtime
        if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            // Extract hour from timestamp
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
            const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));
            
            // Copy NULL bits from input to result (so NULL rows are already set to NULL)
            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, inputNulls, nullsSize);
            
            // Process only non-NULL rows using SelectivityVector
            SelectivityVector rows(size);
            rows.setFromBitsNegate(inputNulls, size);
            
            rows.applyToSelected([&](vector_size_t i) {
                int64_t microseconds = inputRaw[i];
                Timestamp ts = Timestamp::fromMicros(microseconds);
                std::tm tmValue = util::GetDateTime(ts, sessionTz);
                resultRaw[i] = static_cast<int32_t>(tmValue.tm_hour);
                result->SetNotNull(i);
            });
        }
        delete inputArg;
    }
};

} // namespace

void RegisterHourFunction(const std::string &name)
{
    // Hour takes TIMESTAMP and returns INT (hour of day, 0-23)
    // Following Velox Spark SQL behavior: only TIMESTAMP is supported
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_INT,
        std::make_shared<HourFunction>());
}
}
