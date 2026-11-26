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
 *
 * Description: batch math functions implementation
 */

#include "batch_mathfunctions.h"
#include <iostream>
#include <cfloat>
#include "codegen/context_helper.h"
#include "codegen/functions/mathfunctions.h"
#include "util/config_util.h"
#include "codegen/common_util.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

const double DOUBLE_NAN = (0.0 / 0.0);
const uint64_t DOUBLE_BIT_MASK = ((static_cast<uint64_t>(1) << (sizeof(double) * 8 - 1)) - 1);

namespace omniruntime::codegen::function {
static constexpr char DIVIDE_ZERO_EROR[] = "Divided by zero error!";

extern "C" DLLEXPORT void BatchCastInt32ToInt64(int32_t *x, bool *resIsNull, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<int64_t>(x[i]);
    }
}

extern "C" DLLEXPORT void BatchCastInt64ToInt32(int64_t *x, bool *resIsNull, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<int32_t>(x[i]);
    }
}

extern "C" DLLEXPORT void BatchCastInt32ToDouble(int32_t *x, bool *resIsNull, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<double>(x[i]);
    }
}

extern "C" DLLEXPORT void BatchCastInt64ToDouble(int64_t *x, bool *resIsNull, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<double>(x[i]);
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToInt32HalfUp(double *x, bool *resIsNull, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<int32_t>(Round(x[i], 0));
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToInt64HalfUp(double *x, bool *resIsNull, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<int64_t>(Round(x[i], 0));
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToInt32Down(double *x, bool *resIsNull, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<int32_t>(x[i]);
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToInt64Down(double *x, bool *resIsNull, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = static_cast<int64_t>(x[i]);
    }
}

extern "C" DLLEXPORT void BatchAddDouble(double *left, double *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] + right[i];
    }
}

extern "C" DLLEXPORT void BatchSubtractDouble(double *left, double *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] - right[i];
    }
}

extern "C" DLLEXPORT void BatchMultiplyDouble(double *left, double *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] * right[i];
    }
}

extern "C" DLLEXPORT void BatchDivideDouble(double *left, double *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] / right[i];
    }
}

extern "C" DLLEXPORT void BatchModulusDouble(double *left, double *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = std::fmod(left[i], right[i]);
    }
}

extern "C" DLLEXPORT void BatchLessThanDouble(double *left, double *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (left[i] < right[i]);
    }
}

extern "C" DLLEXPORT void BatchLessThanEqualDouble(double *left, double *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (left[i] <= right[i]);
    }
}

extern "C" DLLEXPORT void BatchGreaterThanDouble(double *left, double *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (left[i] > right[i]);
    }
}

extern "C" DLLEXPORT void BatchGreaterThanEqualDouble(double *left, double *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (left[i] >= right[i]);
    }
}

extern "C" DLLEXPORT void BatchEqualDouble(double *left, double *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = (std::fabs(left[i] - right[i]) < DBL_EPSILON);
    }
}

extern "C" DLLEXPORT void BatchNotEqualDouble(double *left, double *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = std::fabs(left[i] - right[i]) >= DBL_EPSILON;
    }
}

extern "C" DLLEXPORT void BatchNormalizeNaNAndZero(double *input, bool *isAnyNull, double *output, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        auto value = input[i];
        if (std::isnan(value)) {
            output[i] = DOUBLE_NAN;
            continue;
        }
        union {
            uint64_t l;
            double d;
        } u;
        u.d = value;
        if (u.l & DOUBLE_BIT_MASK) {
            output[i] = value;
        } else {
            output[i] = 0.0;
        }
    }
}

extern "C" DLLEXPORT void BatchPowerDouble(double *base, double *exponent, double *output, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        output[i] = pow(base[i], exponent[i]);
    }
}

extern "C" DLLEXPORT void BatchAddInt64(int64_t *left, int64_t *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] + right[i];
    }
}

extern "C" DLLEXPORT void BatchSubtractInt64(int64_t *left, int64_t *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] - right[i];
    }
}

extern "C" DLLEXPORT void BatchMultiplyInt64(int64_t *left, int64_t *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] * right[i];
    }
}

extern "C" DLLEXPORT void BatchDivideInt64(int64_t contextPtr, int64_t *left, int64_t *right, int32_t rowCnt,
                                           bool *isNull)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        if (right[i] == 0) {
            SetError(contextPtr, DIVIDE_ZERO_EROR);
            return;
        }
        left[i] = left[i] / right[i];
    }
}

extern "C" DLLEXPORT void BatchModulusInt64(int64_t contextPtr, int64_t *left, int64_t *right, int32_t rowCnt,
                                            bool *isNull)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        if (right[i] == 0) {
            SetError(contextPtr, DIVIDE_ZERO_EROR);
            return;
        }
        left[i] = std::fmod(left[i], right[i]);
    }
}

extern "C" DLLEXPORT void BatchLessThanInt64(int64_t *left, int64_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] < right[i];
    }
}

extern "C" DLLEXPORT void BatchLessThanEqualInt64(int64_t *left, int64_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] <= right[i];
    }
}

extern "C" DLLEXPORT void BatchGreaterThanInt64(int64_t *left, int64_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] > right[i];
    }
}

extern "C" DLLEXPORT void BatchGreaterThanEqualInt64(int64_t *left, int64_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] >= right[i];
    }
}

extern "C" DLLEXPORT void BatchEqualInt64(int64_t *left, int64_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] == right[i];
    }
}

extern "C" DLLEXPORT void BatchNotEqualInt64(int64_t *left, int64_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] != right[i];
    }
}

extern "C" DLLEXPORT void BatchAddInt32(int32_t *left, int32_t *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] + right[i];
    }
}

extern "C" DLLEXPORT void BatchSubtractInt32(int32_t *left, int32_t *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] - right[i];
    }
}

extern "C" DLLEXPORT void BatchMultiplyInt32(int32_t *left, int32_t *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] * right[i];
    }
}

extern "C" DLLEXPORT void BatchDivideInt32(int64_t contextPtr, int32_t *left, int32_t *right, int32_t rowCnt,
                                           bool *isNull)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        if (right[i] == 0) {
            SetError(contextPtr, DIVIDE_ZERO_EROR);
            return;
        }
        left[i] = left[i] / right[i];
    }
}

extern "C" DLLEXPORT void BatchModulusInt32(int64_t contextPtr, int32_t *left, int32_t *right, int32_t rowCnt,
                                            bool *isNull)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isNull[i]) {
            continue;
        }
        if (right[i] == 0) {
            SetError(contextPtr, DIVIDE_ZERO_EROR);
            return;
        }
        left[i] = std::fmod(left[i], right[i]);
    }
}

extern "C" DLLEXPORT void BatchLessThanInt32(int32_t *left, int32_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] < right[i];
    }
}

extern "C" DLLEXPORT void BatchLessThanEqualInt32(int32_t *left, int32_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] <= right[i];
    }
}

extern "C" DLLEXPORT void BatchGreaterThanInt32(int32_t *left, int32_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] > right[i];
    }
}

extern "C" DLLEXPORT void BatchGreaterThanEqualInt32(int32_t *left, int32_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] >= right[i];
    }
}

extern "C" DLLEXPORT void BatchEqualInt32(int32_t *left, int32_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] == right[i];
    }
}

extern "C" DLLEXPORT void BatchNotEqualInt32(int32_t *left, int32_t *right, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        output[i] = left[i] != right[i];
    }
}

extern "C" DLLEXPORT void BatchEqualBool(bool *left, bool toCmp, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = (left[i] == toCmp);
    }
}

extern "C" DLLEXPORT void BatchPmod(int32_t *x, int32_t *y, bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    int32_t r;
    for (int i = 0; i < rowCnt; i++) {
        if (y[i] == 0) {
            output[i] = 0;
            continue;
        }
        r = x[i] % y[i];
        if (r < 0) {
            output[i] = (r + y[i]) % y[i];
        } else {
            output[i] = r;
        }
    }
}

extern "C" DLLEXPORT void BatchRoundLong(int64_t *num, int32_t *decimals, bool *isAnyNull, int64_t *output,
                                         int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = RoundOperator(num[i], decimals[i]);
    }
}
} // namespace omniruntime::codegen::function
