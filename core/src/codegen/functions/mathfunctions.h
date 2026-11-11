/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: registry math function name
 */
#ifndef __MATHFUNCTIONS_H__
#define __MATHFUNCTIONS_H__

#include <iostream>
#include <cmath>

// All extern functions go here temporarily
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

namespace omniruntime::codegen::function {
// Absolute value
template <typename T> extern DLLEXPORT T Abs(T x)
{
    return std::abs(x);
}
extern "C" DLLEXPORT int16_t CastInt32ToInt16(int32_t x);

extern "C" DLLEXPORT int8_t CastInt32ToInt8(int32_t x);

extern "C" DLLEXPORT int16_t CastInt64ToInt16(int64_t x);

extern "C" DLLEXPORT int8_t CastInt64ToInt8(int64_t x);

extern "C" DLLEXPORT double CastInt32ToDouble(int32_t x);

extern "C" DLLEXPORT double CastInt64ToDouble(int64_t x);

extern "C" DLLEXPORT float CastInt32ToFloat(int32_t x);

extern "C" DLLEXPORT float CastInt64ToFloat(int64_t x);

extern "C" DLLEXPORT float CastInt16ToFloat(int16_t x);

extern "C" DLLEXPORT float CastInt8ToFloat(int8_t x);

extern "C" DLLEXPORT double CastTimestampMicrosToDoubleSeconds(int64_t microsSinceEpoch);

extern "C" DLLEXPORT float CastTimestampMicrosToFloatSeconds(int64_t microsSinceEpoch);

extern "C" DLLEXPORT int64_t CastDate32ToTimestampMicros(int32_t daysSinceEpoch);

extern "C" DLLEXPORT double CastFloatToDouble(float x);

extern "C" DLLEXPORT int64_t CastInt32ToInt64(int32_t x);

extern "C" DLLEXPORT int32_t CastInt64ToInt32(int64_t x);

extern "C" DLLEXPORT int32_t CastInt16ToInt32(int16_t x);

extern "C" DLLEXPORT int32_t CastInt8ToInt32(int8_t x);

extern "C" DLLEXPORT int16_t CastInt8ToInt16(int8_t x);

extern "C" DLLEXPORT int64_t CastInt16ToInt64(int16_t x);

extern "C" DLLEXPORT int64_t CastInt8ToInt64(int8_t x);

extern "C" DLLEXPORT int32_t CastDoubleToInt32HalfUp(double x);

extern "C" DLLEXPORT double CastInt16ToDouble(int16_t x);

extern "C" DLLEXPORT double CastInt8ToDouble(int8_t x);

extern "C" DLLEXPORT int32_t CastInt16ToInt32(int16_t x);

extern "C" DLLEXPORT int32_t CastInt8ToInt32(int8_t x);

extern "C" DLLEXPORT int64_t CastInt16ToInt64(int16_t x);

extern "C" DLLEXPORT int64_t CastInt8ToInt64(int8_t x);

extern "C" DLLEXPORT int16_t CastDoubleToInt16HalfUp(double x);

extern "C" DLLEXPORT int8_t CastDoubleToInt8HalfUp(double x);

extern "C" DLLEXPORT int64_t CastDoubleToInt64HalfUp(double x);

extern "C" DLLEXPORT int32_t CastDoubleToInt32Down(double x);

extern "C" DLLEXPORT int16_t CastDoubleToInt16Down(double x);

extern "C" DLLEXPORT int8_t CastDoubleToInt8Down(double x);

extern "C" DLLEXPORT int64_t CastDoubleToInt64Down(double x);

extern "C" DLLEXPORT double CastInt16ToDouble(int16_t x);

extern "C" DLLEXPORT double CastInt8ToDouble(int8_t x);

extern "C" DLLEXPORT int32_t CastInt16ToInt32(int16_t x);

extern "C" DLLEXPORT int32_t CastInt8ToInt32(int8_t x);

extern "C" DLLEXPORT int64_t CastInt16ToInt64(int16_t x);

extern "C" DLLEXPORT int64_t CastInt8ToInt64(int8_t x);

extern "C" DLLEXPORT int32_t CastDoubleToInt32HalfUp(double x);

extern "C" DLLEXPORT int16_t CastDoubleToInt16HalfUp(double x);

extern "C" DLLEXPORT int8_t CastDoubleToInt8HalfUp(double x);

extern "C" DLLEXPORT int64_t CastDoubleToInt64HalfUp(double x);

extern "C" DLLEXPORT int32_t CastDoubleToInt32Down(double x);

extern "C" DLLEXPORT int16_t CastDoubleToInt16Down(double x);

extern "C" DLLEXPORT int8_t CastDoubleToInt8Down(double x);

extern "C" DLLEXPORT int64_t CastDoubleToInt64Down(double x);

extern "C" DLLEXPORT double CastInt16ToDouble(int16_t x);

extern "C" DLLEXPORT double CastInt8ToDouble(int8_t x);

extern "C" DLLEXPORT int32_t CastInt16ToInt32(int16_t x);

extern "C" DLLEXPORT int32_t CastInt8ToInt32(int8_t x);

extern "C" DLLEXPORT int64_t CastInt16ToInt64(int16_t x);

extern "C" DLLEXPORT int64_t CastInt8ToInt64(int8_t x);

extern "C" DLLEXPORT int32_t CastDoubleToInt32HalfUp(double x);

extern "C" DLLEXPORT int16_t CastDoubleToInt16HalfUp(double x);

extern "C" DLLEXPORT int8_t CastDoubleToInt8HalfUp(double x);

extern "C" DLLEXPORT int64_t CastDoubleToInt64HalfUp(double x);

extern "C" DLLEXPORT int32_t CastDoubleToInt32Down(double x);

extern "C" DLLEXPORT int16_t CastDoubleToInt16Down(double x);

extern "C" DLLEXPORT int8_t CastDoubleToInt8Down(double x);

extern "C" DLLEXPORT int64_t CastDoubleToInt64Down(double x);

// double binary operations
extern "C" DLLEXPORT double AddDouble(double left, double right);

extern "C" DLLEXPORT double SubtractDouble(double left, double right);

extern "C" DLLEXPORT double MultiplyDouble(double left, double right);

extern "C" DLLEXPORT double DivideDouble(bool *isNull, double divident, double divisor);

extern "C" DLLEXPORT double ModulusDouble(bool *isNull, double divident, double divisor);

extern "C" DLLEXPORT bool LessThanDouble(double left, double right);

extern "C" DLLEXPORT bool LessThanEqualDouble(double left, double right);

extern "C" DLLEXPORT bool GreaterThanDouble(double left, double right);

extern "C" DLLEXPORT bool GreaterThanEqualDouble(double left, double right);

extern "C" DLLEXPORT bool EqualDouble(double left, double right);

extern "C" DLLEXPORT bool NotEqualDouble(double left, double right);

extern "C" DLLEXPORT double NormalizeNaNAndZero(double value);

extern "C" DLLEXPORT double PowerDouble(double base, double exponent);

// float binary operations
extern "C" DLLEXPORT float AddFloat(float left, float right);

extern "C" DLLEXPORT float SubtractFloat(float left, float right);

extern "C" DLLEXPORT float MultiplyFloat(float left, float right);

extern "C" DLLEXPORT float DivideFloat(bool *isNull, float divident, float divisor);

extern "C" DLLEXPORT float ModulusFloat(bool *isNull, float divident, float divisor);

extern "C" DLLEXPORT bool LessThanFloat(float left, float right);

extern "C" DLLEXPORT bool LessThanEqualFloat(float left, float right);

extern "C" DLLEXPORT bool GreaterThanFloat(float left, float right);

extern "C" DLLEXPORT bool GreaterThanEqualFloat(float left, float right);

extern "C" DLLEXPORT bool EqualFloat(float left, float right);

extern "C" DLLEXPORT bool NotEqualFloat(float left, float right);

extern "C" DLLEXPORT float AddFloatRetNull(bool *isNull, float left, float right);

extern "C" DLLEXPORT float SubtractFloatRetNull(bool *isNull, float left, float right);

extern "C" DLLEXPORT float MultiplyFloatRetNull(bool *isNull, float left, float right);

// long binary operations
extern "C" DLLEXPORT int64_t AddInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT int64_t SubtractInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT int64_t MultiplyInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT int64_t DivideInt64(bool *isNull, int64_t divident, int64_t divisor);

extern "C" DLLEXPORT int64_t DivInt64(bool *isNull, int64_t divident, int64_t divisor);

extern "C" DLLEXPORT int64_t ModulusInt64(bool *isNull, int64_t divident, int64_t divisor);

extern "C" DLLEXPORT int64_t AddInt64RetNull(bool *isNull, int64_t left, int64_t right);

extern "C" DLLEXPORT int64_t SubtractInt64RetNull(bool *isNull, int64_t left, int64_t right);

extern "C" DLLEXPORT int64_t MultiplyInt64RetNull(bool *isNull, int64_t left, int64_t right);

extern "C" DLLEXPORT bool LessThanInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT bool LessThanEqualInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT bool GreaterThanInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT bool GreaterThanEqualInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT bool EqualInt64(int64_t left, int64_t right);

extern "C" DLLEXPORT bool NotEqualInt64(int64_t left, int64_t right);

// int binary operations
extern "C" DLLEXPORT int32_t AddInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT int32_t SubtractInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT int32_t MultiplyInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT int32_t DivideInt32(bool *isNull, int32_t divident, int32_t divisor);

extern "C" DLLEXPORT int32_t ModulusInt32(bool *isNull, int32_t divident, int32_t divisor);

extern "C" DLLEXPORT int32_t AddInt32RetNull(bool *isNull, int32_t left, int32_t right);

extern "C" DLLEXPORT int32_t SubtractInt32RetNull(bool *isNull, int32_t left, int32_t right);

extern "C" DLLEXPORT int32_t MultiplyInt32RetNull(bool *isNull, int32_t left, int32_t right);

extern "C" DLLEXPORT bool LessThanInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT bool LessThanEqualInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT bool GreaterThanInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT bool GreaterThanEqualInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT bool EqualInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT bool NotEqualInt32(int32_t left, int32_t right);

extern "C" DLLEXPORT int64_t RoundLong(int64_t num, int32_t decimals);

// short binary operations
extern "C" DLLEXPORT int16_t AddInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT int16_t SubtractInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT int16_t MultiplyInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT int16_t DivideInt16(bool *isNull, int16_t divident, int16_t divisor);

extern "C" DLLEXPORT int16_t ModulusInt16(bool *isNull, int16_t divident, int16_t divisor);

extern "C" DLLEXPORT int16_t AddInt16RetNull(bool *isNull, int16_t left, int16_t right);

extern "C" DLLEXPORT int16_t SubtractInt16RetNull(bool *isNull, int16_t left, int16_t right);

extern "C" DLLEXPORT int16_t MultiplyInt16RetNull(bool *isNull, int16_t left, int16_t right);

extern "C" DLLEXPORT bool LessThanInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT bool LessThanEqualInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT bool GreaterThanInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT bool GreaterThanEqualInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT bool EqualInt16(int16_t left, int16_t right);

extern "C" DLLEXPORT bool NotEqualInt16(int16_t left, int16_t right);

// byte binary operations
extern "C" DLLEXPORT int8_t AddInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT int8_t SubtractInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT int8_t MultiplyInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT int8_t DivideInt8(bool *isNull, int8_t divident, int8_t divisor);

extern "C" DLLEXPORT int8_t ModulusInt8(bool *isNull, int8_t divident, int8_t divisor);

extern "C" DLLEXPORT int8_t AddInt8RetNull(bool *isNull, int8_t left, int8_t right);

extern "C" DLLEXPORT int8_t SubtractInt8RetNull(bool *isNull, int8_t left, int8_t right);

extern "C" DLLEXPORT int8_t MultiplyInt8RetNull(bool *isNull, int8_t left, int8_t right);

extern "C" DLLEXPORT bool LessThanInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT bool LessThanEqualInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT bool GreaterThanInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT bool GreaterThanEqualInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT bool EqualInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT bool NotEqualInt8(int8_t left, int8_t right);

extern "C" DLLEXPORT bool ExpFunction(double a);

extern "C" DLLEXPORT double Expm1Function(double a);

template <typename T> extern DLLEXPORT T Pmod(T x, T y)
{
    if (y == 0) {
        return 0;
    }
    T r = x % y;
    if (r < 0) {
        return (r + y) % y;
    } else {
        return r;
    }
}

template <typename T> extern DLLEXPORT T Round(T num, int32_t decimals)
{
    if (std::isnan(num) || std::isinf(num)) {
        return num;
    }
    int32_t tenthPower = 10;
    double factor = std::pow(tenthPower, decimals);
    if (num < 0) {
        return -(std::round(-num * factor) / factor);
    }

    return std::round(num * factor) / factor;
}

template <typename T> extern DLLEXPORT T Greatest(T lValue, bool lIsNull, T rValue, bool rIsNull, bool *retIsNull)
{
    if (lIsNull && rIsNull) {
        *retIsNull = true;
        return lValue;
    }
    if (lIsNull || (!rIsNull && rValue > lValue)) {
        return rValue;
    }
    return lValue;
}

template <typename T> extern DLLEXPORT int64_t Floor(T x)
{
    if constexpr (std::is_same_v<T, int64_t>) {
        return x;
    } else {
        return std::floor(x);
    }
}

// nanvl(expr1, expr2): returns expr1 if not NaN, otherwise returns expr2
// Note: NULL propagation is handled by the codegen framework (INPUT_DATA mode).
// The actual vectorized execution uses NanvlFunction (VectorFunction) which handles NULL correctly.
template <typename T> extern DLLEXPORT T Nanvl(T expr1, T expr2)
{
    return std::isnan(expr1) ? expr2 : expr1;
}

template <typename T> extern DLLEXPORT T BitwiseAndFunction(T a, T b)
{
    return a & b;
}

template <typename T> extern DLLEXPORT T BitwiseOrFunction(T a, T b)
{
    return a | b;
}

template <typename T1, typename T2> T1 ShiftRight(T1 a, T2 b)
{
    if constexpr (std::is_same_v<T1, int32_t>) {
        if (b < 0) {
            b = b % 32 + 32;
        }
        if (b >= 32) {
            b = b % 32;
        }
    }
    if constexpr (std::is_same_v<T1, int64_t>) {
        if (b < 0) {
            b = b % 64 + 64;
        }
        if (b >= 64) {
            b = b % 64;
        }
    }
    return a >> b;
}

template <typename T1, typename T2> T1 ShiftLeft(T1 a, T2 b)
{
    if constexpr (std::is_same_v<T1, int32_t>) {
        if (b < 0) {
            b = b % 32 + 32;
        }
        if (b >= 32) {
            b = b % 32;
        }
    }
    if constexpr (std::is_same_v<T1, int64_t>) {
        if (b < 0) {
            b = b % 64 + 64;
        }
        if (b >= 64) {
            b = b % 64;
        }
    }
    return a << b;
}

template <typename T> extern DLLEXPORT T Negative(T a) {
    T result;
    if constexpr (std::is_integral_v<T>) {
        // Avoid undefined integer overflow.
        result = a == std::numeric_limits<T>::min() ? a : -a;
    } else {
        result = -a;
    }
    return result;
}
}

#endif