/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DayOfWeek function implementation
 */

#include "DayOfWeek.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <ctime>
#include <cstring>
#include "libboundscheck/include/securec.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
/// DayOfWeekFunction - Extract day of week from a date value
/// dayofweek(date) -> int
/// Returns the day of week (1-7) from a date.
/// Spark SQL convention: 1 = Sunday, 2 = Monday, ..., 7 = Saturday
/// Supports DATE32 (days since epoch) and INT (treated as days since epoch).
/// For example: dayofweek('2024-03-15') returns 6 (Friday).
class DayOfWeekFunction : public VectorFunction {
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
        
        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }
        
        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        
        // Get input type
        const auto inputTypeId = inputArg->GetTypeId();
        
        if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
            // Extract day of week from date
            auto *inputVector = reinterpret_cast<Vector<int32_t> *>(inputArg);
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
                // Convert date (days since epoch) to seconds
                int32_t daysSinceEpoch = inputRaw[i];
                int64_t seconds = static_cast<int64_t>(daysSinceEpoch) * 86400LL;
                
                // Extract day of week using Timestamp::epochToCalendarUtc (static method)
                std::tm tmValue;
                if (Timestamp::epochToCalendarUtc(seconds, tmValue)) {
                    // tm_wday is 0-6 (Sunday=0, Monday=1, ..., Saturday=6)
                    // Spark SQL convention: 1 = Sunday, 2 = Monday, ..., 7 = Saturday
                    // So we add 1 to convert from 0-based to 1-based
                    resultRaw[i] = static_cast<int32_t>(tmValue.tm_wday + 1);
                    result->SetNotNull(i);
                } else {
                    // If conversion fails, set to null
                    result->SetNull(i);
                }
            });
        }
        delete inputArg;
    }
};
} // namespace

void RegisterDayOfWeekFunction(const std::string &name)
{
    // DayOfWeek takes DATE32 or INT and returns INT (day of week, 1-7)
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_INT,
        std::make_shared<DayOfWeekFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT,
        std::make_shared<DayOfWeekFunction>());
}
}
