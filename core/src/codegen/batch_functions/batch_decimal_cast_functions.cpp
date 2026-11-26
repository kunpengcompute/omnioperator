/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: batch decimal functions implementation
 */

#include "batch_decimal_cast_functions.h"
#include <cmath>
#include <iomanip>
#include "codegen/context_helper.h"
#include "type/decimal_operations.h"

namespace omniruntime::codegen::function {
using namespace omniruntime::type;

// Round towards "nearest neighbor" unless both neighbors are equidistant, in which case round up.
extern "C" DLLEXPORT void BatchCastDecimal64ToIntHalfUp(int64_t contextPtr, int64_t *x, int32_t precision,
                                                        int32_t scale, const bool *isAnyNull, int32_t *output,
                                                        int32_t rowCnt)
{
    int32_t result = 0;
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        auto status = Decimal128Wrapper(x[i]).SetScale(scale).ToInt(result);
        if (status != type::OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_INT, x[i], OpStatus::SUCCESS, precision, scale));
            continue;
        }
        output[i] = result;
    }
}

// Round towards zero.
extern "C" DLLEXPORT void BatchCastDecimal64ToIntDown(int64_t contextPtr, int64_t *x, int32_t precision, int32_t scale,
                                                      const bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    int64_t scaledValue;
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        scaledValue = Decimal64(x[i]).SetScale(scale).ReScale(0, RoundingMode::ROUND_FLOOR).GetValue();
        if (scaledValue < INT_MIN || scaledValue > INT_MAX) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_INT, x[i], OpStatus::SUCCESS, precision, scale));
            continue;
        }
        output[i] = static_cast<int32_t>(scaledValue);
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToLongDown(int64_t *x, int32_t precision, int32_t scale,
                                                       const bool *isAnyNull, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        output[i] = Decimal64(x[i]).SetScale(scale).ReScale(0, RoundingMode::ROUND_FLOOR).GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToLongHalfUp(int64_t *x, int32_t precision, int32_t scale,
                                                         const bool *isAnyNull, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        output[i] = static_cast<int64_t>(Decimal64(x[i]).SetScale(scale));
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToDoubleDown(const int64_t *x, int32_t precision, int32_t scale,
                                                         const bool *isAnyNull, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        std::string doubleString = Decimal64(x[i]).SetScale(scale).ToString();
        output[i] = stod(doubleString);
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToDoubleHalfUp(const int64_t *x, int32_t precision, int32_t scale,
                                                           const bool *isAnyNull, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        output[i] = static_cast<double>(Decimal64(x[i]).SetScale(scale));
    }
}

extern "C" DLLEXPORT void BatchCastIntToDecimal64(int64_t contextPtr, int32_t *x, const bool *isAnyNull,
                                                  int64_t *output, int32_t precision, int32_t scale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal64 result(x[i]);
        result.ReScale(scale);
        if (result.IsOverflow(precision) != OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_INT, OMNI_DECIMAL64, x[i], OpStatus::SUCCESS, precision, scale));
            continue;
        }
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastLongToDecimal64(int64_t contextPtr, int64_t *x, const bool *isAnyNull,
                                                   int64_t *output, int32_t outPrecision, int32_t outScale,
                                                   int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal64 result(x[i]);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr,
                     CastErrorMessage(OMNI_LONG, OMNI_DECIMAL64, x[i], OpStatus::SUCCESS, outPrecision, outScale));
            continue;
        }
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToDecimal64(int64_t contextPtr, double *x, const bool *isAnyNull,
                                                     int64_t *output, int32_t outPrecision, int32_t outScale,
                                                     int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal64 result(x[i]);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr,
                     CastErrorMessage(OMNI_DOUBLE, OMNI_DECIMAL64, x[i], OpStatus::SUCCESS, outPrecision, outScale));
            continue;
        }
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToInt(int64_t contextPtr, Decimal128 *x, int32_t precision, int32_t scale,
                                                   const bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        int32_t result = 0;
        auto status = Decimal128Wrapper(x[i]).SetScale(scale).ToInt(result);
        if (status != type::OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_INT, x[i].ToInt128(), OpStatus::OP_OVERFLOW,
                                                  precision, scale));
            continue;
        }
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToLong(int64_t contextPtr, Decimal128 *x, int32_t precision, int32_t scale,
                                                    const bool *isAnyNull, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        int64_t result = 0;
        auto status = Decimal128Wrapper(x[i]).SetScale(scale).ToLong(result);
        if (status != type::OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_LONG, x[i].ToInt128(), OpStatus::OP_OVERFLOW,
                                                  precision, scale));
            continue;
        }
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToDoubleDown(Decimal128 *x, int32_t precision, int32_t scale,
                                                          const bool *isAnyNull, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        output[i] = static_cast<double>(Decimal128Wrapper(x[i])) / DOUBLE_10_POW[scale];
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToDoubleHalfUp(Decimal128 *x, int32_t precision, int32_t scale,
                                                            const bool *isAnyNull, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        output[i] = stod(Decimal128Wrapper(x[i]).SetScale(scale).ToString());
    }
}

extern "C" DLLEXPORT void BatchCastIntToDecimal128(int64_t contextPtr, int32_t *x, const bool *isAnyNull,
                                                   Decimal128 *output, int32_t outPrecision, int32_t outScale,
                                                   int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal128Wrapper result(x[i]);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr,
                     CastErrorMessage(OMNI_INT, OMNI_DECIMAL128, x[i], OpStatus::OP_OVERFLOW, outPrecision, outScale));
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastLongToDecimal128(int64_t contextPtr, int64_t *x, const bool *isAnyNull,
                                                    Decimal128 *output, int32_t outPrecision, int32_t outScale,
                                                    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal128Wrapper result(x[i]);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr,
                     CastErrorMessage(OMNI_LONG, OMNI_DECIMAL128, x[i], OpStatus::OP_OVERFLOW, outPrecision, outScale));
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToDecimal128(int64_t contextPtr, double *x, bool *isAnyNull,
                                                      Decimal128 *output, int32_t outPrecision, int32_t outScale,
                                                      int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal128Wrapper result(x[i]);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr,
                     CastErrorMessage(OMNI_BOOLEAN, OMNI_DECIMAL128, *x, OpStatus::SUCCESS, outPrecision, outScale));
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64To64(int64_t contextPtr, int64_t *x, int32_t precision, int32_t scale,
                                                 const bool *isAnyNull, int64_t *output, int32_t newPrecision,
                                                 int32_t newScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal64 result(x[i]);
        result.SetScale(scale).ReScale(newScale);
        if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_DECIMAL64, x[i], OpStatus::SUCCESS, precision,
                                                  scale, newPrecision, newScale));
            continue;
        }
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128To128(int64_t contextPtr, Decimal128 *x, int32_t precision, int32_t scale,
                                                   const bool *isAnyNull, Decimal128 *output, int32_t newPrecision,
                                                   int32_t newScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal128Wrapper result = Decimal128Wrapper(x[i]).SetScale(scale).ReScale(newScale);
        if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_DECIMAL128, x[i].ToInt128(), OpStatus::SUCCESS,
                                                  precision, scale, newPrecision, newScale));
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64To128(int64_t contextPtr, int64_t *x, int32_t precision, int32_t scale,
                                                  const bool *isAnyNull, Decimal128 *output, int32_t newPrecision,
                                                  int32_t newScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal128Wrapper result(x[i]);
        result.SetScale(scale).ReScale(newScale);
        if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL64, OMNI_DECIMAL128, x[i], OpStatus::SUCCESS, precision,
                                                  scale, newPrecision, newScale));
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128To64(int64_t contextPtr, Decimal128 *x, int32_t precision, int32_t scale,
                                                  const bool *isAnyNull, int64_t *output, int32_t newPrecision,
                                                  int32_t newScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            continue;
        }
        Decimal128Wrapper decimal128 = Decimal128Wrapper(x[i]).SetScale(scale).ReScale(newScale);
        Decimal64 result(decimal128);
        if (result.IsOverflow(newPrecision) != OpStatus::SUCCESS) {
            SetError(contextPtr, CastErrorMessage(OMNI_DECIMAL128, OMNI_DECIMAL64, x[i].ToInt128(), OpStatus::SUCCESS,
                                                  precision, scale, newPrecision, newScale));
            continue;
        }
        output[i] = result.GetValue();
    }
}

// return null
extern "C" DLLEXPORT void BatchCastDecimal64ToIntRetNull(bool *isNull, int64_t *x, int32_t precision, int32_t scale,
                                                         int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = static_cast<int32_t>(Decimal64(x[i]).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToLongRetNull(bool *isNull, int64_t *x, int32_t precision, int32_t scale,
                                                          int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = static_cast<int64_t>(Decimal64(x[i]).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToDoubleRetNull(bool *isNull, const int64_t *x, int32_t precision,
                                                            int32_t scale, double *output, int32_t rowCnt)
{
    double result;
    for (int i = 0; i < rowCnt; ++i) {
        std::string doubleString = Decimal64(x[i]).SetScale(scale).ToString();
        ConvertStringToDouble(result, doubleString);
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchCastIntToDecimal64RetNull(bool *isNull, int32_t *x, int64_t *output, int32_t precision,
                                                         int32_t scale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal64 result(x[i]);
        result.ReScale(scale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, precision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastLongToDecimal64RetNull(bool *isNull, int64_t *x, int64_t *output,
                                                          int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal64 result(x[i]);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToDecimal64RetNull(bool *isNull, double *x, int64_t *output,
                                                            int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal64 result(x[i]);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToIntRetNull(bool *isNull, Decimal128 *x, int32_t precision, int32_t scale,
                                                          int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = static_cast<int32_t>(Decimal128Wrapper(x[i]).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToLongRetNull(bool *isNull, Decimal128 *x, int32_t precision,
                                                           int32_t scale, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = static_cast<int64_t>(Decimal128Wrapper(x[i]).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToDoubleRetNull(bool *isNull, Decimal128 *x, int32_t precision,
                                                             int32_t scale, double *output, int32_t rowCnt)
{
    double result;
    std::string doubleString;
    for (int i = 0; i < rowCnt; ++i) {
        doubleString = Decimal128Wrapper(x[i]).SetScale(scale).ToString();
        ConvertStringToDouble(result, doubleString);
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchCastIntToDecimal128RetNull(bool *isNull, int32_t *x, Decimal128 *output,
                                                          int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal128Wrapper result(x[i]);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastLongToDecimal128RetNull(bool *isNull, int64_t *x, Decimal128 *output,
                                                           int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal128Wrapper result(x[i]);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToDecimal128RetNull(bool *isNull, double *x, Decimal128 *output,
                                                             int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal128Wrapper result(x[i]);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64To64RetNull(bool *isNull, int64_t *x, int32_t precision, int32_t scale,
                                                        int64_t *output, int32_t newPrecision, int32_t newScale,
                                                        int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal64 result(x[i]);
        result.SetScale(scale).ReScale(newScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, newPrecision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128To128RetNull(bool *isNull, Decimal128 *x, int32_t precision, int32_t scale,
                                                          Decimal128 *output, int32_t newPrecision, int32_t newScale,
                                                          int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto result = Decimal128Wrapper(x[i]).SetScale(scale).ReScale(newScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, newPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64To128RetNull(bool *isNull, int64_t *x, int32_t precision, int32_t scale,
                                                         Decimal128 *output, int32_t newPrecision, int32_t newScale,
                                                         int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        Decimal128Wrapper result(x[i]);
        result.SetScale(scale).ReScale(newScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, newPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128To64RetNull(bool *isNull, Decimal128 *x, int32_t precision, int32_t scale,
                                                         int64_t *output, int32_t newPrecision, int32_t newScale,
                                                         int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto decimal128 = Decimal128Wrapper(x[i]).SetScale(scale).ReScale(newScale);
        Decimal64 result(decimal128);
        CHECK_OVERFLOW_CONTINUE_NULL(result, newPrecision);
        output[i] = result.GetValue();
    }
}
} // namespace omniruntime::codegen::function
