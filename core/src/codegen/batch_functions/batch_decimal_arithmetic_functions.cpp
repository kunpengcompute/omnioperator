/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
#include "batch_decimal_arithmetic_functions.h"
#include <cmath>
#include <iomanip>
#include "codegen/context_helper.h"
#include "type/decimal_operations.h"

using namespace omniruntime::type;

namespace omniruntime::codegen::function {
const std::string DECIMAL_OVERFLOW { "Decimal overflow" };
const std::string DIVIDE_ZERO { "Division by zero" };

extern "C" DLLEXPORT void BatchDecimal128Compare(Decimal128 *x, int32_t xPrecision, int32_t xScale, Decimal128 *y,
    int32_t yPrecision, int32_t yScale, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = Decimal128Wrapper(x[i]).SetScale(xScale).Compare(Decimal128Wrapper(y[i]).SetScale(yScale));
    }
}

extern "C" DLLEXPORT void BatchLessThanDecimal128(Decimal128 *left, int32_t xPrecision, int32_t xScale,
    Decimal128 *right, int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal128Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] < 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchLessThanEqualDecimal128(Decimal128 *left, int32_t xPrecision, int32_t xScale,
    Decimal128 *right, int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal128Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] <= 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchGreaterThanDecimal128(Decimal128 *left, int32_t xPrecision, int32_t xScale,
    Decimal128 *right, int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal128Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] > 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchGreaterThanEqualDecimal128(Decimal128 *left, int32_t xPrecision, int32_t xScale,
    Decimal128 *right, int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal128Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] >= 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchEqualDecimal128(Decimal128 *left, int32_t xPrecision, int32_t xScale, Decimal128 *right,
    int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal128Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] == 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchNotEqualDecimal128(Decimal128 *left, int32_t xPrecision, int32_t xScale,
    Decimal128 *right, int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal128Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] != 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchAbsDecimal128(Decimal128 *x, int32_t xPrecision, int32_t xScale, bool *isNull,
    Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = Decimal128Wrapper(x[i]).Abs().ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchRoundDecimal128(int64_t contextPtr, Decimal128 *x, int32_t xPrecision, int32_t xScale,
    int32_t *round, bool *isNull, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        Decimal128Wrapper input(x[i]);
        input.SetScale(xScale);
        DecimalOperations::Round(input, outScale, round[i]);
        CHECK_OVERFLOW_CONTINUE(input, outPrecision);
        output[i] = input.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchRoundDecimal64(int64_t contextPtr, int64_t *x, int32_t xPrecision, int32_t xScale,
    int32_t *round, bool *isNull, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        Decimal64 input(x[i]);
        input.SetScale(xScale);
        DecimalOperations::Round(input, outScale, round[i]);
        CHECK_OVERFLOW_CONTINUE(input, outPrecision);
        output[i] = input.GetValue();
    }
}

extern "C" DLLEXPORT void BatchRoundDecimal128WithoutRound(int64_t contextPtr, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, bool *isNull, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        Decimal128Wrapper input(x[i]);
        input.SetScale(xScale);
        DecimalOperations::Round(input, outScale, 0);
        CHECK_OVERFLOW_CONTINUE(input, outPrecision);
        output[i] = input.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchRoundDecimal64WithoutRound(int64_t contextPtr, int64_t *x, int32_t xPrecision,
    int32_t xScale, bool *isNull, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        Decimal64 input(x[i]);
        input.SetScale(xScale);
        DecimalOperations::Round(input, outScale, 0);
        CHECK_OVERFLOW_CONTINUE(input, outPrecision);
        output[i] = input.GetValue();
    }
}

// decimal64 arith functions
extern "C" DLLEXPORT void BatchDecimal64Compare(int64_t *x, int32_t xPrecision, int32_t xScale, int64_t *y,
    int32_t yPrecision, int32_t yScale, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = Decimal64(x[i]).SetScale(xScale).Compare(Decimal64(y[i]).SetScale(yScale));
    }
}

extern "C" DLLEXPORT void BatchAbsDecimal64(int64_t *x, int32_t xPrecision, int32_t xScale, bool *isNull,
    int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = std::abs(x[i]);
    }
}

extern "C" DLLEXPORT void BatchLessThanDecimal64(int64_t *left, int32_t xPrecision, int32_t xScale, int64_t *right,
    int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal64Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] < 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchLessThanEqualDecimal64(int64_t *left, int32_t xPrecision, int32_t xScale, int64_t *right,
    int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal64Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] <= 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchGreaterThanDecimal64(int64_t *left, int32_t xPrecision, int32_t xScale, int64_t *right,
    int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal64Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] > 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchGreaterThanEqualDecimal64(int64_t *left, int32_t xPrecision, int32_t xScale,
    int64_t *right, int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal64Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] >= 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchEqualDecimal64(int64_t *left, int32_t xPrecision, int32_t xScale, int64_t *right,
    int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal64Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] == 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchNotEqualDecimal64(int64_t *left, int32_t xPrecision, int32_t xScale, int64_t *right,
    int32_t yPrecision, int32_t yScale, bool *output, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchDecimal64Compare(left, xPrecision, xScale, right, yPrecision, yScale, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (tmp[i] != 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchUnscaledValue64(int64_t *x, int32_t precision, int32_t scale, bool *isAnyNull,
    int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = x[i];
    }
}

extern "C" DLLEXPORT void BatchMakeDecimal64(int64_t contextPtr, int64_t *x, bool *isAnyNull, int64_t *output,
    int32_t precision, int32_t scale, int32_t rowCnt)
{
    std::ostringstream errorMessage;
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 1;
            continue;
        }
        if (DecimalOperations::IsUnscaledLongOverflow(x[i], precision, scale) && !HasError(contextPtr)) {
            errorMessage << "Unscaled value " << x << " out of Decimal(" << precision << ", " << scale << ") range";
            SetError(contextPtr, errorMessage.str());
            output[i] = 1;
            continue;
        }
        output[i] = x[i];
    }
}

extern "C" DLLEXPORT void BatchMakeDecimal64RetNull(bool *isNull, int64_t *x, int64_t *output, int32_t precision,
    int32_t scale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (DecimalOperations::IsUnscaledLongOverflow(x[i], precision, scale)) {
            isNull[i] = true;
            output[i] = 1;
            continue;
        }
        output[i] = x[i];
    }
}

// Decimal Add Operator ReScale
extern "C" DLLEXPORT void BatchAddDec64Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec64Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec128Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec64Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec128Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *y,
    int32_t yPrecision, int32_t yScale, int64_t *x, int32_t xPrecision, int32_t xScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

// Decimal SubOperator ReScale
extern "C" DLLEXPORT void BatchSubDec64Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec64Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec128Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec64Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec128Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

// Decimal MulOperator ReScale
extern "C" DLLEXPORT void BatchMulDec64Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec64Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec128Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec64Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        int32_t rescaleFactor = xScale + yScale - outScale;
        if (rescaleFactor >= 0 && rescaleFactor <= MAX_SCALE) {
            result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), rescaleFactor);
        } else {
            DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
                Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
            result.ReScale(outScale);
        }
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec128Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        int32_t rescaleFactor = xScale + yScale - outScale;
        if (rescaleFactor >= 0 && rescaleFactor <= MAX_SCALE) {
            result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), rescaleFactor);
        } else {
            DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
                Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
            result.ReScale(outScale);
        }
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

// Decimal DivOperation ReScale
extern "C" DLLEXPORT void BatchDivDec64Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec128Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

// Decimal Mod Operation ReScale
extern "C" DLLEXPORT void BatchModDec64Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec64Dec128Dec64ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec64Dec64ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec64Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec128Dec64ReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int64_t *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec64Dec128Dec128ReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.ToDecimal128();
    }
}

// Decimal Add Operator NotReScale
extern "C" DLLEXPORT void BatchAddDec64Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAdd(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec64Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAdd(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec128Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAdd(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec64Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAdd(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec128Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *y,
    int32_t yPrecision, int32_t yScale, int64_t *x, int32_t xPrecision, int32_t xScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAdd(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

// Decimal SubOperator NotReScale
extern "C" DLLEXPORT void BatchSubDec64Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtract(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec64Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtract(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec128Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtract(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec64Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtract(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec128Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtract(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

// Decimal MulOperator NotReScale
extern "C" DLLEXPORT void BatchMulDec64Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec64Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec128Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec64Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        int32_t rescaleFactor = xScale + yScale - outScale;
        if (rescaleFactor >= 0 && rescaleFactor <= MAX_SCALE) {
            result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), rescaleFactor);
        } else {
            DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
                Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        }
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec128Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        int32_t rescaleFactor = xScale + yScale - outScale;
        if (rescaleFactor >= 0 && rescaleFactor <= MAX_SCALE) {
            result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), rescaleFactor);
        } else {
            DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
                Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        }
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
    }
}

// Decimal DivOperation NotReScale
extern "C" DLLEXPORT void BatchDivDec64Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec128Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

// Decimal Mod Operation NotReScale
extern "C" DLLEXPORT void BatchModDec64Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec64Dec128Dec64NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec64Dec64NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec64Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec128Dec64NotReScale(int64_t contextPtr, bool *isNull, Decimal128 *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int64_t *output,
    int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec64Dec128Dec128NotReScale(int64_t contextPtr, bool *isNull, int64_t *x,
    int32_t xPrecision, int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            x[i] = 1;
            continue;
        }
        CHECK_DIVIDE_BY_ZERO_CONTINUE(y[i]);
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        CHECK_OVERFLOW_CONTINUE(result, outPrecision);
        y[i] = result.ToDecimal128();
    }
}


// add return null
extern "C" DLLEXPORT void BatchAddDec64Dec64Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec64Dec64Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec128Dec128Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec64Dec128Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchAddDec128Dec64Dec128RetNull(bool *isNull, Decimal128 *y, int32_t yPrecision,
    int32_t yScale, int64_t *x, int32_t xPrecision, int32_t xScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalAddWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

// sub ret null
extern "C" DLLEXPORT void BatchSubDec64Dec64Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec64Dec64Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec128Dec128Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec64Dec128Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchSubDec128Dec64Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalSubtractWithResultScale(Decimal128Wrapper(x[i]).SetScale(xScale), xScale,
            xPrecision, Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, outScale, result);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

// mul ret null
extern "C" DLLEXPORT void BatchMulDec64Dec64Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        x[i] = result.GetValue();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec64Dec64Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        output[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec128Dec128Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), xScale + yScale - outScale);
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec64Dec128Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        int32_t rescaleFactor = xScale + yScale - outScale;
        if (rescaleFactor >= 0 && rescaleFactor <= MAX_SCALE) {
            result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), rescaleFactor);
        } else {
            DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
                Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
            result.ReScale(outScale);
        }
        y[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

extern "C" DLLEXPORT void BatchMulDec128Dec64Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        int32_t rescaleFactor = xScale + yScale - outScale;
        if (rescaleFactor >= 0 && rescaleFactor <= MAX_SCALE) {
            result = Decimal128Wrapper(x[i]).MultiplyRoundUp(Decimal128Wrapper(y[i]), rescaleFactor);
        } else {
            DecimalOperations::InternalDecimalMultiply(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
                Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
            result.ReScale(outScale);
        }
        x[i] = result.ToDecimal128();
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
    }
}

// div ret null
extern "C" DLLEXPORT void BatchDivDec64Dec64Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec128Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec64Dec64RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        y[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec64Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, Decimal128 *output, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec128Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec64Dec128Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        y[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchDivDec128Dec64Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        DecimalOperations::InternalDecimalDivide(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result, outScale);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

// mod ret null
extern "C" DLLEXPORT void BatchModDec64Dec64Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal64(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal64(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec64Dec128Dec64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec64Dec64RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        y[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec64Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, int64_t *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec128Dec128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale,
    int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        x[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchModDec128Dec128Dec64RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision,
    int32_t xScale, Decimal128 *y, int32_t yPrecision, int32_t yScale, int64_t *output, int32_t outPrecision,
    int32_t outScale, int32_t rowCnt)
{
    Decimal64 result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchModDec64Dec128Dec128RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    Decimal128 *y, int32_t yPrecision, int32_t yScale, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    Decimal128Wrapper result;
    for (int i = 0; i < rowCnt; ++i) {
        if (isNull[i]) {
            continue;
        }
        DecimalOperations::InternalDecimalMod(Decimal128Wrapper(x[i]).SetScale(xScale), xScale, xPrecision,
            Decimal128Wrapper(y[i]).SetScale(yScale), yScale, yPrecision, result);
        result.ReScale(outScale);
        CHECK_OVERFLOW_CONTINUE_NULL(result, outPrecision);
        y[i] = result.ToDecimal128();
    }
}


extern "C" DLLEXPORT void BatchRoundDecimal128RetNull(bool *isNull, Decimal128 *x, int32_t xPrecision, int32_t xScale,
    int32_t *round, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        Decimal128Wrapper input(x[i]);
        input.SetScale(xScale);
        DecimalOperations::Round(input, outScale, round[i]);
        CHECK_OVERFLOW_CONTINUE_NULL(input, outPrecision);
        output[i] = input.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchRoundDecimal64RetNull(bool *isNull, int64_t *x, int32_t xPrecision, int32_t xScale,
    int32_t *round, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        Decimal64 input(x[i]);
        input.SetScale(xScale);
        DecimalOperations::Round(input, outScale, round[i]);
        CHECK_OVERFLOW_CONTINUE_NULL(input, outPrecision);
        output[i] = input.GetValue();
    }
}

extern "C" DLLEXPORT void BatchGreatestDecimal64(int64_t contextPtr, int64_t *xValue, int32_t xPrecision,
    int32_t xScale, bool *xIsNull, int64_t *yValue, int32_t yPrecision, int32_t yScale, bool *yIsNull, bool *retIsNull,
    int64_t *output, int32_t newPrecision, int32_t newScale, int32_t rowCnt)
{
    if (xPrecision == yPrecision && xScale == yScale) {
        for (int i = 0; i < rowCnt; i++) {
            if (xIsNull[i] && yIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            auto x = xValue[i];
            auto y = yValue[i];
            if (xIsNull[i] || (!yIsNull[i] && x < y)) {
                output[i] = y;
                continue;
            }
            output[i] = x;
        }
    } else {
        for (int i = 0; i < rowCnt; i++) {
            if (xIsNull[i] && yIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            Decimal64 x(xValue[i]);
            x.SetScale(xScale);
            Decimal64 y(yValue[i]);
            y.SetScale(yScale);
            if (xIsNull[i] || (!yIsNull[i] && x.Compare(y) < 0)) {
                y.ReScale(newScale);
                CHECK_OVERFLOW_CONTINUE(y, newPrecision);
                output[i] = y.GetValue();
                continue;
            }
            x.ReScale(newScale);
            CHECK_OVERFLOW_CONTINUE(x, newPrecision);
            output[i] = x.GetValue();
        }
    }
}

extern "C" DLLEXPORT void BatchGreatestDecimal128(int64_t contextPtr, type::Decimal128 *xValue, int32_t xPrecision,
    int32_t xScale, bool *xIsNull, type::Decimal128 *yValue, int32_t yPrecision, int32_t yScale, bool *yIsNull,
    bool *retIsNull, type::Decimal128 *output, int32_t newPrecision, int32_t newScale, int32_t rowCnt)
{
    if (xPrecision == yPrecision && xScale == yScale) {
        for (int i = 0; i < rowCnt; i++) {
            if (xIsNull[i] && yIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            if (xIsNull[i] || (!yIsNull[i] && xValue[i] < yValue[i])) {
                output[i] = yValue[i];
                continue;
            }
            output[i] = xValue[i];
        }
    } else {
        for (int i = 0; i < rowCnt; i++) {
            if (xIsNull[i] && yIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            Decimal128Wrapper x(xValue[i]);
            x.SetScale(xScale);
            Decimal128Wrapper y(yValue[i]);
            y.SetScale(yScale);
            if (xIsNull[i] || (!yIsNull[i] && x.Compare(y) < 0)) {
                y.ReScale(newScale);
                CHECK_OVERFLOW_CONTINUE(y, newPrecision);
                output[i] = y.ToDecimal128();
                continue;
            }
            x.ReScale(newScale);
            CHECK_OVERFLOW_CONTINUE(x, newPrecision);
            output[i] = x.ToDecimal128();
        }
    }
}

extern "C" DLLEXPORT void BatchGreatestDecimal64RetNull(bool *isNull, int64_t *xValue, int32_t xPrecision,
    int32_t xScale, bool *xIsNull, int64_t *yValue, int32_t yPrecision, int32_t yScale, bool *yIsNull, bool *retIsNull,
    int64_t *output, int32_t newPrecision, int32_t newScale, int32_t rowCnt)
{
    if (xPrecision == yPrecision && xScale == yScale) {
        for (int i = 0; i < rowCnt; i++) {
            if (yIsNull[i] && xIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            auto x = xValue[i];
            auto y = yValue[i];
            if (xIsNull[i] || (!yIsNull[i] && x < y)) {
                output[i] = y;
                continue;
            }
            output[i] = x;
        }
    } else {
        for (int i = 0; i < rowCnt; i++) {
            if (yIsNull[i] && xIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            Decimal64 x(xValue[i]);
            x.SetScale(xScale);
            Decimal64 y(yValue[i]);
            y.SetScale(yScale);
            if (xIsNull[i] || (!yIsNull[i] && x.Compare(y) < 0)) {
                y.ReScale(newScale);
                CHECK_OVERFLOW_CONTINUE_NULL(y, newPrecision);
                output[i] = y.GetValue();
                continue;
            }
            x.ReScale(newScale);
            CHECK_OVERFLOW_CONTINUE_NULL(x, newPrecision);
            output[i] = x.GetValue();
        }
    }
}

extern "C" DLLEXPORT void BatchGreatestDecimal128RetNull(bool *isNull, type::Decimal128 *xValue, int32_t xPrecision,
    int32_t xScale, bool *xIsNull, type::Decimal128 *yValue, int32_t yPrecision, int32_t yScale, bool *yIsNull,
    bool *retIsNull, type::Decimal128 *output, int32_t newPrecision, int32_t newScale, int32_t rowCnt)
{
    if (xPrecision == yPrecision && xScale == yScale) {
        for (int i = 0; i < rowCnt; i++) {
            if (yIsNull[i] && xIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            if (xIsNull[i] || (!yIsNull[i] && xValue[i] < yValue[i])) {
                output[i] = yValue[i];
                continue;
            }
            output[i] = xValue[i];
        }
    } else {
        for (int i = 0; i < rowCnt; i++) {
            if (yIsNull[i] && xIsNull[i]) {
                retIsNull[i] = true;
                continue;
            }
            Decimal128Wrapper x(xValue[i]);
            x.SetScale(xScale);
            Decimal128Wrapper y(yValue[i]);
            y.SetScale(yScale);
            if (xIsNull[i] || (!yIsNull[i] && x.Compare(y) < 0)) {
                y.ReScale(newScale);
                CHECK_OVERFLOW_CONTINUE_NULL(y, newPrecision);
                output[i] = y.ToDecimal128();
                continue;
            }
            x.ReScale(newScale);
            CHECK_OVERFLOW_CONTINUE_NULL(x, newPrecision);
            output[i] = x.ToDecimal128();
        }
    }
}
}