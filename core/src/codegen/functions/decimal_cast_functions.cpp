/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: batch decimal functions implementation
 */

#include "decimal_cast_functions.h"

namespace omniruntime::codegen::function {

// Cast Function
extern "C" DLLEXPORT int64_t CastDecimal64To64(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                               bool isNull, int32_t newPrecision, int32_t newScale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(x);
    result.SetScale(scale).ReScale(newScale);
    if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_DECIMAL64, x, OpStatus::SUCCESS, precision, scale,
                                              newPrecision, newScale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT void CastDecimal128To128(int64_t contextPtr, int64_t xHigh, uint64_t xLow, int32_t precision,
                                              int32_t scale, bool isNull, int32_t newPrecision, int32_t newScale,
                                              int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        *outHighPtr = 0;
        *outLowPtr = 0;
        return;
    }
    Decimal128Wrapper result(xHigh, xLow);
    result.SetScale(scale).ReScale(newScale);
    if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_DECIMAL128, ((int128_t(xHigh) << 64) + xLow),
                                              OpStatus::SUCCESS, precision, scale, newPrecision, newScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastDecimal64To128(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                             bool isNull, int32_t newPrecision, int32_t newScale, int64_t *outHighPtr,
                                             uint64_t *outLowPtr)
{
    if (isNull) {
        *outHighPtr = 0;
        *outLowPtr = 0;
        return;
    }
    Decimal128Wrapper result(x);
    result.SetScale(scale).ReScale(newScale);
    if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_DECIMAL128, x, OpStatus::SUCCESS, precision, scale,
                                              newPrecision, newScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT int64_t CastDecimal128To64(int64_t contextPtr, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                int32_t scale, bool isNull, int32_t newPrecision, int32_t newScale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(Decimal128Wrapper(xHigh, xLow).SetScale(scale).ReScale(newScale));
    if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_DECIMAL64, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::SUCCESS, precision, scale, newPrecision, newScale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastIntToDecimal64(int64_t contextPtr, int32_t x, bool isNull, int32_t precision,
                                                int32_t scale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(x);
    result.ReScale(scale);
    if (result.IsOverflow(precision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_INT, OMNI_DECIMAL64, x, OpStatus::SUCCESS, precision, scale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastInt16ToDecimal64(int64_t contextPtr, int16_t x, bool isNull, int32_t precision,
                                                  int32_t scale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(x);
    result.ReScale(scale);
    if (result.IsOverflow(precision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_SHORT, OMNI_DECIMAL64, x, OpStatus::SUCCESS, precision, scale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastInt8ToDecimal64(int64_t contextPtr, int8_t x, bool isNull, int32_t precision,
                                                 int32_t scale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(x);
    result.ReScale(scale);
    if (result.IsOverflow(precision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_BYTE, OMNI_DECIMAL64, x, OpStatus::SUCCESS, precision, scale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastLongToDecimal64(int64_t contextPtr, int64_t x, bool isNull, int32_t outPrecision,
                                                 int32_t outScale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_LONG, OMNI_DECIMAL64, x, OpStatus::SUCCESS, outPrecision, outScale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastDoubleToDecimal64(int64_t contextPtr, double x, bool isNull, int32_t outPrecision,
                                                   int32_t outScale)
{
    if (isNull) {
        return 0;
    }
    Decimal64 result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr,
                 CastErrorMessage(OMNI_DOUBLE, OMNI_DECIMAL64, x, OpStatus::SUCCESS, outPrecision, outScale));
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT void CastIntToDecimal128(int64_t contextPtr, int32_t x, bool isNull, int32_t outPrecision,
                                              int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        *outHighPtr = 0;
        *outLowPtr = 0;
        return;
    }
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr,
                 CastErrorMessage(OMNI_INT, OMNI_DECIMAL128, x, OpStatus::OP_OVERFLOW, outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastInt16ToDecimal128(int64_t contextPtr, int16_t x, bool isNull, int32_t outPrecision,
                                                int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        *outHighPtr = 0;
        *outLowPtr = 0;
        return;
    }
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr,
                 CastErrorMessage(OMNI_SHORT, OMNI_DECIMAL128, x, OpStatus::OP_OVERFLOW, outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastInt8ToDecimal128(int64_t contextPtr, int8_t x, bool isNull, int32_t outPrecision,
                                               int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        *outHighPtr = 0;
        *outLowPtr = 0;
        return;
    }
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr,
                 CastErrorMessage(OMNI_BYTE, OMNI_DECIMAL128, x, OpStatus::OP_OVERFLOW, outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastLongToDecimal128(int64_t contextPtr, int64_t x, bool isNull, int32_t outPrecision,
                                               int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        return;
    }
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr,
                 CastErrorMessage(OMNI_LONG, OMNI_DECIMAL128, x, OpStatus::OP_OVERFLOW, outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastDoubleToDecimal128(int64_t contextPtr, double x, bool isNull, int32_t outPrecision,
                                                 int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        *outHighPtr = 0;
        *outLowPtr = 0;
        return;
    }
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        SetError(contextPtr,
                 CastErrorMessage(OMNI_DOUBLE, OMNI_DECIMAL128, x, OpStatus::OP_OVERFLOW, outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT int32_t CastDecimal64ToIntHalfUp(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                                      bool isNull)
{
    if (isNull) {
        return 0;
    }

    int32_t result;
    try {
        result = static_cast<int32_t>(Decimal64(x).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_INT, x, OpStatus::SUCCESS, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int16_t CastDecimal64ToInt16HalfUp(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                                        bool isNull)
{
    if (isNull) {
        return 0;
    }

    int16_t result;
    try {
        result = static_cast<int16_t>(Decimal64(x).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_SHORT, x, OpStatus::SUCCESS, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int8_t CastDecimal64ToInt8HalfUp(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                                      bool isNull)
{
    if (isNull) {
        return 0;
    }

    int8_t result;
    try {
        result = static_cast<int8_t>(Decimal64(x).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_BYTE, x, OpStatus::SUCCESS, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT double CastDecimal64ToDoubleHalfUp(int64_t x, int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    double result = static_cast<double>(Decimal64(x).SetScale(scale));
    return result;
}

extern "C" DLLEXPORT int32_t CastDecimal128ToIntHalfUp(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                       int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int32_t result;
    try {
        result = static_cast<int32_t>(Decimal128Wrapper(xHigh, xLow).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_INT, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int16_t CastDecimal128ToInt16HalfUp(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                         int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int16_t result;
    try {
        result = static_cast<int16_t>(Decimal128Wrapper(xHigh, xLow).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_SHORT, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int8_t CastDecimal128ToInt8HalfUp(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                       int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int8_t result;
    try {
        result = static_cast<int8_t>(Decimal128Wrapper(xHigh, xLow).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_BYTE, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int64_t CastDecimal128ToLongHalfUp(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                        int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int64_t result;
    try {
        result = static_cast<int64_t>(Decimal128Wrapper(xHigh, xLow).SetScale(scale));
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_LONG, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT double CastDecimal128ToDoubleHalfUp(int64_t high, uint64_t low, int32_t precision, int32_t scale,
                                                         bool isNull)
{
    if (isNull) {
        return 0.0;
    }
    Decimal128Wrapper input(high, low);

    return (double)input.SetScale(scale);
}

extern "C" DLLEXPORT int32_t CastDecimal64ToIntDown(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                                    bool isNull)
{
    if (isNull) {
        return 0;
    }
    return static_cast<int32_t>(Decimal64(x).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
}

extern "C" DLLEXPORT int16_t CastDecimal64ToInt16Down(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                                      bool isNull)
{
    if (isNull) {
        return 0;
    }
    return static_cast<int16_t>(Decimal64(x).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
}

extern "C" DLLEXPORT int8_t CastDecimal64ToInt8Down(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
                                                    bool isNull)
{
    if (isNull) {
        return 0;
    }
    return static_cast<int8_t>(Decimal64(x).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
}

extern "C" DLLEXPORT int64_t CastDecimal64ToLongDown(int64_t x, int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int64_t scaledValue = Decimal64(x).SetScale(scale).ReScale(0, RoundingMode::ROUND_FLOOR).GetValue();
    return scaledValue;
}

extern "C" DLLEXPORT int64_t CastDecimal64ToLongHalfUp(int64_t x, int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int64_t result = static_cast<int64_t>(Decimal64(x).SetScale(scale));
    return result;
}

extern "C" DLLEXPORT double CastDecimal64ToDoubleDown(int64_t x, int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    std::string doubleString = Decimal64(x).SetScale(scale).ToString();
    return stod(doubleString);
}

extern "C" DLLEXPORT int32_t CastDecimal128ToIntDown(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                     int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int32_t result;
    try {
        result =
            static_cast<int32_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_INT, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int16_t CastDecimal128ToInt16Down(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                       int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int16_t result;
    try {
        result =
            static_cast<int16_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_SHORT, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int8_t CastDecimal128ToInt8Down(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                     int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int8_t result;
    try {
        result =
            static_cast<int8_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_BYTE, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int64_t CastDecimal128ToLongDown(int64_t contextPtr, int64_t xHigh, uint64_t xLow,
                                                      int32_t precision, int32_t scale, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int64_t result;
    try {
        result =
            static_cast<int64_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
    } catch (std::overflow_error &e) {
        SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_LONG, (int128_t(xHigh) << 64 | xLow),
                                              OpStatus::OP_OVERFLOW, precision, scale));
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT double CastDecimal128ToDoubleDown(int64_t high, uint64_t low, int32_t precision, int32_t scale,
                                                       bool isNull)
{
    if (isNull) {
        return 0.0;
    }
    Decimal128Wrapper input(high, low);
    return (double)input / DOUBLE_10_POW[scale];
}

// Cast Function
extern "C" DLLEXPORT int64_t CastDecimal64To64RetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale,
                                                      int32_t newPrecision, int32_t newScale)
{
    Decimal64 result(x);
    result.SetScale(scale).ReScale(newScale);
    CHECK_OVERFLOW_RETURN_NULL(result, newPrecision);
    return result.GetValue();
}

extern "C" DLLEXPORT void CastDecimal128To128RetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                     int32_t scale, int32_t newPrecision, int32_t newScale,
                                                     int64_t *outHighPtr, uint64_t *outLowPtr)
{
    Decimal128Wrapper result(xHigh, xLow);
    result.SetScale(scale).ReScale(newScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, newPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastDecimal64To128RetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale,
                                                    int32_t newPrecision, int32_t newScale, int64_t *outHighPtr,
                                                    uint64_t *outLowPtr)
{
    Decimal128Wrapper result(x);
    result.SetScale(scale).ReScale(newScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, newPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT int64_t CastDecimal128To64RetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                       int32_t scale, int32_t newPrecision, int32_t newScale)
{
    Decimal64 result(Decimal128Wrapper(xHigh, xLow).SetScale(scale).ReScale(newScale));
    CHECK_OVERFLOW_RETURN_NULL(result, newPrecision);
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastIntToDecimal64RetNull(bool *isNull, int32_t x, int32_t outPrecision, int32_t outScale)
{
    Decimal64 result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_RETURN_NULL(result, outPrecision);
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastInt16ToDecimal64RetNull(bool *isNull, int16_t x, int32_t outPrecision,
                                                         int32_t outScale)
{
    Decimal64 result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_RETURN_NULL(result, outPrecision);
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastInt8ToDecimal64RetNull(bool *isNull, int8_t x, int32_t outPrecision, int32_t outScale)
{
    Decimal64 result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_RETURN_NULL(result, outPrecision);
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastLongToDecimal64RetNull(bool *isNull, int64_t x, int32_t outPrecision, int32_t outScale)
{
    Decimal64 result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_RETURN_NULL(result, outPrecision);
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastDoubleToDecimal64RetNull(bool *isNull, double x, int32_t outPrecision,
                                                          int32_t outScale)
{
    Decimal64 result(x);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT void CastIntToDecimal128RetNull(bool *isNull, int32_t x, int32_t outPrecision, int32_t outScale,
                                                     int64_t *outHighPtr, uint64_t *outLowPtr)
{
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, outPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastInt16ToDecimal128RetNull(bool *isNull, int16_t x, int32_t outPrecision, int32_t outScale,
                                                       int64_t *outHighPtr, uint64_t *outLowPtr)
{
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, outPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastInt8ToDecimal128RetNull(bool *isNull, int8_t x, int32_t outPrecision, int32_t outScale,
                                                      int64_t *outHighPtr, uint64_t *outLowPtr)
{
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, outPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastLongToDecimal128RetNull(bool *isNull, int64_t x, int32_t outPrecision, int32_t outScale,
                                                      int64_t *outHighPtr, uint64_t *outLowPtr)
{
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, outPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastDoubleToDecimal128RetNull(bool *isNull, double x, int32_t outPrecision, int32_t outScale,
                                                        int64_t *outHighPtr, uint64_t *outLowPtr)
{
    Decimal128Wrapper result(x);
    result.ReScale(outScale);
    CHECK_OVERFLOW_VOID_RETURN_NULL(result, outPrecision);
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT int32_t CastDecimal64ToIntRetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale)
{
    return static_cast<int32_t>(Decimal64(x).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
}

extern "C" DLLEXPORT int16_t CastDecimal64ToInt16RetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale)
{
    return static_cast<int16_t>(Decimal64(x).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
}

extern "C" DLLEXPORT int8_t CastDecimal64ToInt8RetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale)
{
    return static_cast<int8_t>(Decimal64(x).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
}

extern "C" DLLEXPORT int64_t CastDecimal64ToLongRetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale)
{
    Decimal64 result(x);
    result.SetScale(scale).ReScale(0, RoundingMode::ROUND_FLOOR);
    return result.GetValue();
}

extern "C" DLLEXPORT double CastDecimal64ToDoubleRetNull(bool *isNull, int64_t x, int32_t precision, int32_t scale)
{
    std::string doubleString = Decimal64(x).SetScale(scale).ToString();
    double result;
    ConvertStringToDouble(result, doubleString);
    return result;
}

extern "C" DLLEXPORT int32_t CastDecimal128ToIntRetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                        int32_t scale)
{
    return static_cast<int32_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
}

extern "C" DLLEXPORT int16_t CastDecimal128ToInt16RetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                          int32_t scale)
{
    return static_cast<int16_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
}

extern "C" DLLEXPORT int8_t CastDecimal128ToInt8RetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                        int32_t scale)
{
    return static_cast<int8_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
}

extern "C" DLLEXPORT int64_t CastDecimal128ToLongRetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                         int32_t scale)
{
    return static_cast<int64_t>(Decimal128Wrapper(xHigh, xLow).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
}

extern "C" DLLEXPORT double CastDecimal128ToDoubleRetNull(bool *isNull, int64_t xHigh, uint64_t xLow, int32_t precision,
                                                          int32_t scale)
{
    std::string doubleString = Decimal128Wrapper(xHigh, xLow).SetScale(scale).ToString();
    double result;
    ConvertStringToDouble(result, doubleString);
    return result;
}
} // namespace omniruntime::codegen::function
