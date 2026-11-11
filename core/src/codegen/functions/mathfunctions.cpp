/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: registry math function name
 */
#include "mathfunctions.h"
#include <iostream>
#include <cfloat>
#include "codegen/context_helper.h"
#include "codegen/common_util.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"


#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

const double DOUBLE_NAN = (0.0 / 0.0);
const uint64_t DOUBLE_BIT_MASK = ((static_cast<uint64_t>(1) << (sizeof(double) * 8 - 1)) - 1);

namespace omniruntime::codegen::function {
static constexpr char DIVIDE_ZERO_EROR[] = "Divided by zero error!";

extern "C" DLLEXPORT int16_t CastInt32ToInt16(int32_t x)
{
    return static_cast<int16_t>(x);
}

extern "C" DLLEXPORT int8_t CastInt32ToInt8(int32_t x)
{
    return static_cast<int8_t>(x);
}

extern "C" DLLEXPORT int16_t CastInt64ToInt16(int64_t x)
{
    return static_cast<int16_t>(x);
}

extern "C" DLLEXPORT int8_t CastInt64ToInt8(int64_t x)
{
    return static_cast<int8_t>(x);
}

extern "C" DLLEXPORT int64_t CastInt32ToInt64(int32_t x)
{
    return static_cast<int64_t>(x);
}

extern "C" DLLEXPORT int32_t CastInt64ToInt32(int64_t x)
{
    return static_cast<int32_t>(x);
}


extern "C" DLLEXPORT int32_t CastInt16ToInt32(int16_t x)
{
    return static_cast<int32_t>(x);
}

extern "C" DLLEXPORT int32_t CastInt8ToInt32(int8_t x)
{
    return static_cast<int32_t>(x);
}

extern "C" DLLEXPORT int64_t CastInt16ToInt64(int16_t x)
{
    return static_cast<int64_t>(x);
}

extern "C" DLLEXPORT int64_t CastInt8ToInt64(int8_t x)
{
    return static_cast<int64_t>(x);
}

extern "C" DLLEXPORT int16_t CastInt8ToInt16(int8_t x)
{
    return static_cast<int16_t>(x);
}

extern "C" DLLEXPORT double CastInt16ToDouble(int16_t x)
{
    return static_cast<double>(x);
}

extern "C" DLLEXPORT double CastInt8ToDouble(int8_t x)
{
    return static_cast<double>(x);
}

extern "C" DLLEXPORT double CastInt32ToDouble(int32_t x)
{
    return static_cast<double>(x);
}

extern "C" DLLEXPORT double CastInt64ToDouble(int64_t x)
{
    return static_cast<double>(x);
}

extern "C" DLLEXPORT float CastInt32ToFloat(int32_t x)
{
    return static_cast<float>(x);
}

extern "C" DLLEXPORT float CastInt64ToFloat(int64_t x)
{
    return static_cast<float>(x);
}

extern "C" DLLEXPORT float CastInt16ToFloat(int16_t x)
{
    return static_cast<float>(x);
}

extern "C" DLLEXPORT float CastInt8ToFloat(int8_t x)
{
    return static_cast<float>(x);
}

// Spark Cast(timestamp AS double): epoch seconds as double (internal storage is microseconds).
extern "C" DLLEXPORT double CastTimestampMicrosToDoubleSeconds(int64_t microsSinceEpoch)
{
    return static_cast<double>(microsSinceEpoch) / 1000000.0;
}

extern "C" DLLEXPORT float CastTimestampMicrosToFloatSeconds(int64_t microsSinceEpoch)
{
    return static_cast<float>(microsSinceEpoch) / 1000000.0f;
}

extern "C" DLLEXPORT int64_t CastDate32ToTimestampMicros(int32_t daysSinceEpoch)
{
    auto ts = Timestamp::fromDate(daysSinceEpoch);
    // Keep codegen CAST(date as timestamp) timezone semantics aligned with
    // existing codegen timestamp casts (Asia/Shanghai).
    static const auto *sessionTimezone = tz::locateZone("Asia/Shanghai");
    if (sessionTimezone != nullptr) {
        ts.toGMT(*sessionTimezone);
    }
    return ts.toMicros();
}

extern "C" DLLEXPORT double CastFloatToDouble(float x)
{
    return static_cast<double>(x);
}

extern "C" DLLEXPORT int32_t CastDoubleToInt32Down(double x)
{
    return static_cast<int32_t>(x);
}

extern "C" DLLEXPORT int16_t CastDoubleToInt16Down(double x)
{
    return static_cast<int16_t>(x);
}

extern "C" DLLEXPORT int8_t CastDoubleToInt8Down(double x)
{
    return static_cast<int8_t>(x);
}

extern "C" DLLEXPORT int64_t CastDoubleToInt64Down(double x)
{
    return static_cast<int64_t>(x);
}

extern "C" DLLEXPORT int32_t CastDoubleToInt32HalfUp(double x)
{
    return static_cast<int32_t>(Round(x, 0));
}

extern "C" DLLEXPORT int16_t CastDoubleToInt16HalfUp(double x)
{
    return static_cast<int16_t>(Round(x, 0));
}

extern "C" DLLEXPORT int8_t CastDoubleToInt8HalfUp(double x)
{
    return static_cast<int8_t>(Round(x, 0));
}

extern "C" DLLEXPORT int64_t CastDoubleToInt64HalfUp(double x)
{
    return static_cast<int64_t>(Round(x, 0));
}

// double functions

extern "C" DLLEXPORT double AddDouble(double left, double right)
{
    return left + right;
}

extern "C" DLLEXPORT double SubtractDouble(double left, double right)
{
    return left - right;
}

extern "C" DLLEXPORT double MultiplyDouble(double left, double right)
{
    return left * right;
}

extern "C" DLLEXPORT double DivideDouble(bool *isNull, double divident, double divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident / divisor;
}

extern "C" DLLEXPORT double ModulusDouble(bool *isNull, double divident, double divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return std::fmod(divident, divisor);
}

extern "C" DLLEXPORT bool LessThanDouble(double left, double right)
{
    return left < right;
}

extern "C" DLLEXPORT bool LessThanEqualDouble(double left, double right)
{
    return left <= right;
}

extern "C" DLLEXPORT bool GreaterThanDouble(double left, double right)
{
    return left > right;
}

extern "C" DLLEXPORT bool GreaterThanEqualDouble(double left, double right)
{
    return left >= right;
}

extern "C" DLLEXPORT bool EqualDouble(double left, double right)
{
    return std::fabs(left - right) < DBL_EPSILON;
}

extern "C" DLLEXPORT bool NotEqualDouble(double left, double right)
{
    return std::fabs(left - right) >= DBL_EPSILON;
}

extern "C" DLLEXPORT double NormalizeNaNAndZero(double value)
{
    if (std::isnan(value)) {
        return DOUBLE_NAN;
    }
    union {
        uint64_t l;
        double d;
    } u;
    u.d = value;
    if (u.l & DOUBLE_BIT_MASK) {
        return value;
    }
    return 0.0;
}

extern "C" DLLEXPORT double PowerDouble(double base, double exponent)
{
    return pow(base, exponent);
}

// float functions
    extern "C" DLLEXPORT float AddFloat(float left, float right)
{
    return left + right;
}

    extern "C" DLLEXPORT float SubtractFloat(float left, float right)
{
    return left - right;
}

    extern "C" DLLEXPORT float MultiplyFloat(float left, float right)
{
    return left * right;
}

    extern "C" DLLEXPORT float DivideFloat(bool *isNull, float divident, float divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident / divisor;
}

    extern "C" DLLEXPORT float ModulusFloat(bool *isNull, float divident, float divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return std::fmod(divident, divisor);
}

    extern "C" DLLEXPORT bool LessThanFloat(float left, float right)
{
    return left < right;
}

    extern "C" DLLEXPORT bool LessThanEqualFloat(float left, float right)
{
    return left <= right;
}

    extern "C" DLLEXPORT bool GreaterThanFloat(float left, float right)
{
    return left > right;
}

    extern "C" DLLEXPORT bool GreaterThanEqualFloat(float left, float right)
{
    return left >= right;
}

    extern "C" DLLEXPORT bool EqualFloat(float left, float right)
{
    return std::fabs(left - right) < FLT_EPSILON;
}

    extern "C" DLLEXPORT bool NotEqualFloat(float left, float right)
{
    return std::fabs(left - right) >= FLT_EPSILON;
}

// long functions

extern "C" DLLEXPORT int64_t AddInt64(int64_t left, int64_t right)
{
    return left + right;
}

extern "C" DLLEXPORT int64_t SubtractInt64(int64_t left, int64_t right)
{
    return left - right;
}

extern "C" DLLEXPORT int64_t MultiplyInt64(int64_t left, int64_t right)
{
    return left * right;
}

extern "C" DLLEXPORT int64_t DivideInt64(bool *isNull, int64_t divident, int64_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident / divisor;
}

extern "C" DLLEXPORT int64_t DivInt64(bool *isNull, int64_t divident, int64_t divisor)
{
    // Integer division (truncates toward zero), returns NULL if divisor is 0
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    // Handle overflow case: Long.MIN_VALUE / -1
    if (divident == std::numeric_limits<int64_t>::min() && divisor == -1) {
        return divident;  // Overflow wraps to Long.MIN_VALUE (Java semantics)
    }
    return divident / divisor;
}

extern "C" DLLEXPORT int64_t ModulusInt64(bool *isNull, int64_t divident, int64_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident % divisor;
}

extern "C" DLLEXPORT int64_t AddInt64RetNull(bool *isNull, int64_t left, int64_t right)
{
    int64_t result;
    *isNull = __builtin_add_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int64_t SubtractInt64RetNull(bool *isNull, int64_t left, int64_t right)
{
    int64_t result;
    *isNull = __builtin_sub_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int64_t MultiplyInt64RetNull(bool *isNull, int64_t left, int64_t right)
{
    int64_t result;
    *isNull = __builtin_mul_overflow(left, right, &result);
    return result;
}


extern "C" DLLEXPORT bool LessThanInt64(int64_t left, int64_t right)
{
    return left < right;
}

extern "C" DLLEXPORT bool LessThanEqualInt64(int64_t left, int64_t right)
{
    return left <= right;
}

extern "C" DLLEXPORT bool GreaterThanInt64(int64_t left, int64_t right)
{
    return left > right;
}

extern "C" DLLEXPORT bool GreaterThanEqualInt64(int64_t left, int64_t right)
{
    return left >= right;
}

extern "C" DLLEXPORT bool EqualInt64(int64_t left, int64_t right)
{
    return left == right;
}

extern "C" DLLEXPORT bool NotEqualInt64(int64_t left, int64_t right)
{
    return left != right;
}

// int functions

extern "C" DLLEXPORT int32_t AddInt32(int32_t left, int32_t right)
{
    return left + right;
}

extern "C" DLLEXPORT int32_t SubtractInt32(int32_t left, int32_t right)
{
    return left - right;
}

extern "C" DLLEXPORT int32_t MultiplyInt32(int32_t left, int32_t right)
{
    return left * right;
}

extern "C" DLLEXPORT int32_t DivideInt32(bool *isNull, int32_t divident, int32_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident / divisor;
}

extern "C" DLLEXPORT int32_t ModulusInt32(bool *isNull, int32_t divident, int32_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident % divisor;
}

extern "C" DLLEXPORT int32_t AddInt32RetNull(bool *isNull, int32_t left, int32_t right)
{
    int32_t result;
    *isNull = __builtin_add_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int32_t SubtractInt32RetNull(bool *isNull, int32_t left, int32_t right)
{
    int32_t result;
    *isNull = __builtin_sub_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int32_t MultiplyInt32RetNull(bool *isNull, int32_t left, int32_t right)
{
    int32_t result;
    *isNull = __builtin_mul_overflow(left, right, &result);
    return result;
}


extern "C" DLLEXPORT bool LessThanInt32(int32_t left, int32_t right)
{
    return left < right;
}

extern "C" DLLEXPORT bool LessThanEqualInt32(int32_t left, int32_t right)
{
    return left <= right;
}

extern "C" DLLEXPORT bool GreaterThanInt32(int32_t left, int32_t right)
{
    return left > right;
}

extern "C" DLLEXPORT bool GreaterThanEqualInt32(int32_t left, int32_t right)
{
    return left >= right;
}

extern "C" DLLEXPORT bool EqualInt32(int32_t left, int32_t right)
{
    return left == right;
}

extern "C" DLLEXPORT bool NotEqualInt32(int32_t left, int32_t right)
{
    return left != right;
}

extern "C" DLLEXPORT int64_t RoundLong(int64_t num, int32_t decimals)
{
    return RoundOperator(num, decimals);
}
}

// short functions

extern "C" DLLEXPORT int16_t AddInt16(int16_t left, int16_t right)
{
    return left + right;
}

extern "C" DLLEXPORT int16_t SubtractInt16(int16_t left, int16_t right)
{
    return left - right;
}

extern "C" DLLEXPORT int16_t MultiplyInt16(int16_t left, int16_t right)
{
    return left * right;
}

extern "C" DLLEXPORT int16_t DivideInt16(bool *isNull, int16_t divident, int16_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident / divisor;
}

extern "C" DLLEXPORT int16_t ModulusInt16(bool *isNull, int16_t divident, int16_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident % divisor;
}

extern "C" DLLEXPORT int16_t AddInt16RetNull(bool *isNull, int16_t left, int16_t right)
{
    int16_t result;
    *isNull = __builtin_add_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int16_t SubtractInt16RetNull(bool *isNull, int16_t left, int16_t right)
{
    int16_t result;
    *isNull = __builtin_sub_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int16_t MultiplyInt16RetNull(bool *isNull, int16_t left, int16_t right)
{
    int16_t result;
    *isNull = __builtin_mul_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT bool LessThanInt16(int16_t left, int16_t right)
{
    return left < right;
}

extern "C" DLLEXPORT bool LessThanEqualInt16(int16_t left, int16_t right)
{
    return left <= right;
}

extern "C" DLLEXPORT bool GreaterThanInt16(int16_t left, int16_t right)
{
    return left > right;
}

extern "C" DLLEXPORT bool GreaterThanEqualInt16(int16_t left, int16_t right)
{
    return left >= right;
}

extern "C" DLLEXPORT bool EqualInt16(int16_t left, int16_t right)
{
    return left == right;
}

extern "C" DLLEXPORT bool NotEqualInt16(int16_t left, int16_t right)
{
    return left != right;
}

// byte functions

extern "C" DLLEXPORT int8_t AddInt8(int8_t left, int8_t right)
{
    return left + right;
}

extern "C" DLLEXPORT int8_t SubtractInt8(int8_t left, int8_t right)
{
    return left - right;
}

extern "C" DLLEXPORT int8_t MultiplyInt8(int8_t left, int8_t right)
{
    return left * right;
}

extern "C" DLLEXPORT int8_t DivideInt8(bool *isNull, int8_t divident, int8_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident / divisor;
}

extern "C" DLLEXPORT int8_t ModulusInt8(bool *isNull, int8_t divident, int8_t divisor)
{
    if (divisor == 0) {
        *isNull = true;
        return 0;
    }
    return divident % divisor;
}

extern "C" DLLEXPORT int8_t AddInt8RetNull(bool *isNull, int8_t left, int8_t right)
{
    int8_t result;
    *isNull = __builtin_add_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int8_t SubtractInt8RetNull(bool *isNull, int8_t left, int8_t right)
{
    int8_t result;
    *isNull = __builtin_sub_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT int8_t MultiplyInt8RetNull(bool *isNull, int8_t left, int8_t right)
{
    int8_t result;
    *isNull = __builtin_mul_overflow(left, right, &result);
    return result;
}

extern "C" DLLEXPORT bool LessThanInt8(int8_t left, int8_t right)
{
    return left < right;
}

extern "C" DLLEXPORT bool LessThanEqualInt8(int8_t left, int8_t right)
{
    return left <= right;
}

extern "C" DLLEXPORT bool GreaterThanInt8(int8_t left, int8_t right)
{
    return left > right;
}

extern "C" DLLEXPORT bool GreaterThanEqualInt8(int8_t left, int8_t right)
{
    return left >= right;
}

extern "C" DLLEXPORT bool EqualInt8(int8_t left, int8_t right)
{
    return left == right;
}

extern "C" DLLEXPORT bool NotEqualInt8(int8_t left, int8_t right)
{
    return left != right;
}

extern "C" DLLEXPORT bool ExpFunction(double a)
{
    return std::exp(a);
}

extern "C" DLLEXPORT double Expm1Function(double a)
{
    return std::expm1(a);
}
