/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: AddMonths function implementation
 */

#include "AddMonths.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "type/date_time_utils.h"
#include <ctime>
#include <cstring>
#include <algorithm>
#include <limits>
#include "libboundscheck/include/securec.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
// Helper function to get maximum day of month
inline int32_t GetMaxDayOfMonth(int32_t year, int32_t month) {
    bool isLeap = Date32::IsLeapYear(year);
    return isLeap ? LEAP_YEAR_OF_DAYS[month] : NORMAL_YEAR_OF_DAYS[month];
}

// Helper function to add months to a date
// Returns true on success, false on overflow
bool AddMonthsToDate(int32_t daysSinceEpoch, int32_t numMonths, int32_t &result) {
    // Convert days to seconds and use Timestamp conversion to get the date components
    int64_t seconds = static_cast<int64_t>(daysSinceEpoch) * 86400LL;
    std::tm tmValue;
    if (!Timestamp::epochToCalendarUtc(seconds, tmValue)) {
        return false;
    }
    
    int32_t year = tmValue.tm_year + 1900;
    int32_t month = tmValue.tm_mon + 1;  // tm_mon is 0-11, convert to 1-12
    int32_t day = tmValue.tm_mday;
    
    // Similar to handling number in base 12. Here, month - 1 makes it in [0, 11] range.
    int64_t monthAdded = static_cast<int64_t>(month) - 1 + numMonths;
    
    // Used to adjust month/year when monthAdded is not in [0, 11] range.
    int64_t yearOffset = (monthAdded >= 0 ? monthAdded : monthAdded - 11) / 12;
    
    // Adjusts monthAdded to natural month number in [1, 12] range.
    int32_t monthResult = static_cast<int32_t>(monthAdded - yearOffset * 12 + 1);
    
    // Adjusts year.
    int64_t yearResult = static_cast<int64_t>(year) + yearOffset;
    
    // Check for overflow
    if (yearResult < MIN_YEAR || yearResult > MAX_YEAR) {
        return false;
    }
    
    // Get maximum day of the result month
    int32_t lastDayOfMonth = GetMaxDayOfMonth(static_cast<int32_t>(yearResult), monthResult);
    
    // Adjusts day to valid one (if day exceeds last day of month, use last day)
    int32_t dayResult = (lastDayOfMonth < day) ? lastDayOfMonth : day;
    
    // Convert back to days since epoch
    int64_t resultDays = 0;
    if (!Date32::DaysSinceEpochFromDate(static_cast<int32_t>(yearResult), monthResult, dayResult, resultDays)) {
        return false;
    }
    
    // Check for int32_t overflow
    if (resultDays < std::numeric_limits<int32_t>::min() || resultDays > std::numeric_limits<int32_t>::max()) {
        return false;
    }
    
    result = static_cast<int32_t>(resultDays);
    return true;
}

class AddMonthsFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            return;
        }
        
        // Extract arguments from stack: numMonths (INT), date (DATE32)
        const auto numMonthsArg = args.top();
        args.pop();
        const auto dateArg = args.top();
        args.pop();
        
        const auto size = dateArg->GetSize();
        
        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }
        
        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        
        // Get input type
        const auto dateTypeId = dateArg->GetTypeId();
        
        if (dateTypeId == OMNI_DATE32 || dateTypeId == OMNI_INT) {
            // Extract date values
            auto *dateVector = reinterpret_cast<Vector<int32_t> *>(dateArg);
            const auto *dateRaw = unsafe::UnsafeVector::GetRawValues(dateVector);
            const auto *dateNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(dateArg));
            
            // Check if numMonths is constant
            bool numMonthsIsConst = (numMonthsArg->GetEncoding() == OMNI_ENCODING_CONST);
            int32_t constNumMonths = 0;
            const int32_t *numMonthsRaw = nullptr;
            const uint64_t *numMonthsNulls = nullptr;
            
            if (numMonthsIsConst) {
                // Handle constant numMonths
                auto *constNumMonthsVec = reinterpret_cast<ConstVector<int32_t> *>(numMonthsArg);
                constNumMonths = constNumMonthsVec->GetConstValue();
                // For const vector, check if it's null
                if (numMonthsArg->IsNull(0)) {
                    // If constant is NULL, set all results to NULL
                    auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                    auto nullsSize = BitUtil::Nbytes(size);
                    memset(resultNulls, 0xFF, nullsSize);
                    delete numMonthsArg;
                    delete dateArg;
                    return;
                }
            } else {
                // Handle non-const numMonths
                auto *numMonthsVector = reinterpret_cast<Vector<int32_t> *>(numMonthsArg);
                numMonthsRaw = unsafe::UnsafeVector::GetRawValues(numMonthsVector);
                numMonthsNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(numMonthsArg));
            }
            
            // Copy NULL bits from date input to result (so NULL rows are already set to NULL)
            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, dateNulls, nullsSize);
            
            // Process only non-NULL rows using SelectivityVector
            SelectivityVector rows(size);
            rows.setFromBitsNegate(dateNulls, size);
            
            rows.applyToSelected([&](vector_size_t i) {
                // Check if numMonths is NULL (for non-const case)
                if (!numMonthsIsConst) {
                    if (numMonthsNulls && BitUtil::IsBitSet(numMonthsNulls, i)) {
                        result->SetNull(i);
                        return;
                    }
                }
                
                // Perform add_months operation
                int32_t daysSinceEpoch = dateRaw[i];
                int32_t numMonths = numMonthsIsConst ? constNumMonths : numMonthsRaw[i];
                int32_t resultDays = 0;
                
                if (AddMonthsToDate(daysSinceEpoch, numMonths, resultDays)) {
                    resultRaw[i] = resultDays;
                    result->SetNotNull(i);
                } else {
                    // If operation fails (overflow), set to null
                    result->SetNull(i);
                }
            });
        }
        delete numMonthsArg;
        delete dateArg;
    }
};
} // namespace

void RegisterAddMonthsFunction(const std::string &name)
{
    // AddMonths takes DATE32 and INT(numMonths) and returns DATE32
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_INT}, OMNI_DATE32,
        std::make_shared<AddMonthsFunction>());
    // Also support INT as date input
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_INT}, OMNI_DATE32,
        std::make_shared<AddMonthsFunction>());
}
}
