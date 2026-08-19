/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Shared implementation for date arithmetic functions (date_add and date_sub)
 */

#include "DateArithmetic.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

// Shared implementation namespace for date arithmetic functions
namespace date_arithmetic_impl {
// Helper function to perform date arithmetic (add or subtract days)
// Matches Spark native behavior: no type promotion, no overflow protection
template <typename TNumDays>
void ComputeDateArithmetic(int32_t daysSinceEpoch, TNumDays numDays, int32_t &result, bool isSubtract) {
    // Direct calculation without type promotion or overflow checking, matching Spark behavior
    if (isSubtract) {
        // date_sub: subtract numDays
        __builtin_sub_overflow(daysSinceEpoch, numDays, &result);
    } else {
        // date_add: add numDays
        __builtin_add_overflow(daysSinceEpoch, numDays, &result);
    }
    // Note: __builtin_*_overflow returns overflow flag, but we ignore it to match Spark behavior
}

// Template class for DateArithmetic function supporting different integer types for numDays
// isSubtract: true for date_sub, false for date_add
template <typename TNumDays>
class DateArithmeticFunctionImpl : public VectorFunction {
private:
    bool isSubtract_;
    const char* functionName_;
    
public:
    DateArithmeticFunctionImpl(bool isSubtract, const char* functionName) 
        : isSubtract_(isSubtract), functionName_(functionName) {}
    
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("DateArithmetic error:", "{} requires exactly 2 arguments", functionName_);
        }
        
        // Extract arguments from stack: numDays (TNumDays), date (DATE32)
        const auto numDaysArg = args.top();
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
            // Extract date values. DATE literals are represented as ConstVector,
            // so they cannot be read through the flat Vector layout.
            bool dateIsConst = (dateArg->GetEncoding() == OMNI_ENCODING_CONST);
            bool dateConstIsNull = dateIsConst && dateArg->IsNull(0);
            int32_t constDate = 0;
            const int32_t *dateRaw = nullptr;
            if (dateIsConst) {
                auto *constDateVector = reinterpret_cast<ConstVector<int32_t> *>(dateArg);
                if (!dateConstIsNull) {
                    constDate = constDateVector->GetConstValue();
                }
            } else {
                auto *dateVector = reinterpret_cast<Vector<int32_t> *>(dateArg);
                dateRaw = unsafe::UnsafeVector::GetRawValues(dateVector);
            }
            const auto *dateNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(dateArg));
            
            // Check if numDays is constant
            bool numDaysIsConst = (numDaysArg->GetEncoding() == OMNI_ENCODING_CONST);
            TNumDays constNumDays = 0;
            const TNumDays *numDaysRaw = nullptr;
            const uint64_t *numDaysNulls = nullptr;
            
            if (numDaysIsConst) {
                // Handle constant numDays
                auto *constNumDaysVec = reinterpret_cast<ConstVector<TNumDays> *>(numDaysArg);
                constNumDays = constNumDaysVec->GetConstValue();
                // For const vector, check if it's null
                if (numDaysArg->IsNull(0)) {
                    // If constant is NULL, set all results to NULL
                    result->SetNulls(0, true, size);
                    delete numDaysArg;
                    delete dateArg;
                    return;
                }
            } else {
                // Handle non-const numDays
                auto *numDaysVector = reinterpret_cast<Vector<TNumDays> *>(numDaysArg);
                numDaysRaw = unsafe::UnsafeVector::GetRawValues(numDaysVector);
                numDaysNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(numDaysArg));
            }
            
            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            if (dateIsConst) {
                if (dateConstIsNull) {
                    result->SetNulls(0, true, size);
                    delete numDaysArg;
                    delete dateArg;
                    return;
                }
                memset(resultNulls, 0, nullsSize);
                result->SetNullFlag(false);
            } else {
                // Copy NULL bits from date input to result (so NULL rows are already set to NULL)
                memcpy(resultNulls, dateNulls, nullsSize);
            }
            
            // Process only non-NULL rows using SelectivityVector
            SelectivityVector rows(size);
            if (dateIsConst) {
                rows.setAll();
            } else {
                rows.setFromBitsNegate(dateNulls, size);
            }
            
            rows.applyToSelected([&](vector_size_t i) {
                // Check if numDays is NULL (for non-const case)
                if (!numDaysIsConst) {
                    if (numDaysNulls && BitUtil::IsBitSet(numDaysNulls, i)) {
                        result->SetNull(i);
                        return;
                    }
                }
                
                // Perform date arithmetic operation
                int32_t daysSinceEpoch = dateIsConst ? constDate : dateRaw[i];
                TNumDays numDays = numDaysIsConst ? constNumDays : numDaysRaw[i];
                int32_t resultDays = 0;
                
                // Direct calculation without overflow protection, matching Spark native behavior
                ComputeDateArithmetic(daysSinceEpoch, numDays, resultDays, isSubtract_);
                resultRaw[i] = resultDays;
                result->SetNotNull(i);
            });
        }
        delete numDaysArg;
        delete dateArg;
    }
};
} // namespace date_arithmetic_impl

// Helper function to register date arithmetic functions (date_add or date_sub)
void RegisterDateArithmeticFunction(const std::string &name, bool isSubtract) {
    using namespace date_arithmetic_impl;
    const char* functionName = isSubtract ? "date_sub" : "date_add";
    // DateArithmetic takes DATE32 and BYTE/SHORT/INT(numDays) and returns DATE32
    // Support BYTE (int8_t, equivalent to TINYINT)
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_BYTE}, OMNI_DATE32,
        std::make_shared<DateArithmeticFunctionImpl<int8_t>>(isSubtract, functionName));
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_BYTE}, OMNI_DATE32,
        std::make_shared<DateArithmeticFunctionImpl<int8_t>>(isSubtract, functionName));
    
    // Support SHORT (int16_t, equivalent to SMALLINT)
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_SHORT}, OMNI_DATE32,
        std::make_shared<DateArithmeticFunctionImpl<int16_t>>(isSubtract, functionName));
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_SHORT}, OMNI_DATE32,
        std::make_shared<DateArithmeticFunctionImpl<int16_t>>(isSubtract, functionName));
    
    // Support INT (int32_t)
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_INT}, OMNI_DATE32,
        std::make_shared<DateArithmeticFunctionImpl<int32_t>>(isSubtract, functionName));
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_INT}, OMNI_DATE32,
        std::make_shared<DateArithmeticFunctionImpl<int32_t>>(isSubtract, functionName));
}
}
