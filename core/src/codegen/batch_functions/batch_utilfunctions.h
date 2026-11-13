/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: batch util functions implementation
 */

#ifndef OMNI_RUNTIME_BATCH_UTILFUNCTIONS_H
#define OMNI_RUNTIME_BATCH_UTILFUNCTIONS_H
#include <iostream>
#include <cmath>
#include <vector>
#include <libboundscheck/include/securec.h>
#include "type/decimal128.h"

using namespace omniruntime::type;

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

namespace omniruntime::codegen::function {
// make an array like {0, 1, 2, 3, ..., rowCnt - 1}
extern "C" DLLEXPORT void FillRowIndexArray(int32_t *dataArray, int32_t rowCnt);

// convert literal to an array
extern "C" DLLEXPORT void FillNull(bool *nullArray, bool isNull, int32_t rowCnt);

extern "C" DLLEXPORT void FillBool(bool *dataArray, bool *nullArray, bool literal, bool isNull, int32_t rowCnt);

extern "C" DLLEXPORT void FillInt32(int32_t *dataArray, bool *nullArray, int32_t literal, bool isNull, int32_t rowCnt);

extern "C" DLLEXPORT void FillInt64(int64_t *dataArray, bool *nullArray, int64_t literal, bool isNull, int32_t rowCnt);

extern "C" DLLEXPORT void FillDouble(double *dataArray, bool *nullArray, double literal, bool isNull, int32_t rowCnt);

extern "C" DLLEXPORT void FillDecimal128(Decimal128 *dataArray, bool *nullArray, __int128_t literal, bool isNull,
                                         int32_t rowCnt);

extern "C" DLLEXPORT void FillString(int64_t contextPtr, uint8_t **dataArray, bool *nullArray, int32_t *lengthArray,
                                     uint8_t *literal, bool isNull, int32_t length, int32_t rowCnt);

// fill varchar length array according to offset array
extern "C" DLLEXPORT void FillLength(int32_t *offsets, int32_t *rowIdxArray, int32_t *lengthArray, int32_t rowCnt);

extern "C" DLLEXPORT void FillLengthInFuncExpr(int32_t *lengthArray, int32_t length, int32_t rowCnt);

// logical operations for boolean
extern "C" DLLEXPORT int32_t CreateAndNot(bool *dataArray, bool *nullArray, int32_t *rowIdxArray, int32_t rowCnt);

extern "C" DLLEXPORT void CreateNot(bool *val, int32_t rowCnt);

extern "C" DLLEXPORT void CreateOr(bool *left, bool *right, int32_t rowCnt);

extern "C" DLLEXPORT void CreateAnd(bool *left, bool *right, int32_t rowCnt);

extern "C" DLLEXPORT void CreateAndNotBool(bool *dataArray, bool *nullArray, int32_t rowCnt);

// process AND/OR expression
extern "C" DLLEXPORT void CreateOrExpr(bool *left, bool *leftNull, bool *right, bool *rightNull, int32_t rowCnt);

extern "C" DLLEXPORT void CreateAndExpr(bool *left, bool *leftNull, bool *right, bool *rightNull, int32_t rowCnt);

// copy result to output vector
extern "C" DLLEXPORT void CopyNull(bool *dataArray, bool *output, int32_t *rowIdxArray, int32_t rowCnt);

extern "C" DLLEXPORT void CopyBoolean(bool *dataArray, bool *output, int32_t rowCnt);

extern "C" DLLEXPORT void CopyInt32(int32_t *dataArray, int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void CopyInt64(int64_t *dataArray, int64_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void CopyDouble(double *dataArray, double *output, int32_t rowCnt);

extern "C" DLLEXPORT void CopyDecimal128(Decimal128 *dataArray, Decimal128 *output, int32_t rowCnt);

extern "C" DLLEXPORT void CopyString(uint8_t **dataArray, uint8_t **output, int32_t rowCnt);

template <typename T> extern DLLEXPORT void Coalesce(T *lArray, bool *lIsNull, T *rArray, bool *rIsNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (lIsNull[i]) {
            lArray[i] = rArray[i];
            lIsNull[i] = rIsNull[i];
        }
    }
}

extern "C" DLLEXPORT void CoalesceString(uint8_t **lArray, bool *lIsNull, int32_t *lLength, uint8_t **rArray,
                                         bool *rIsNull, int32_t *rLength, int32_t rowCnt);

template <typename T>
extern DLLEXPORT void IfExpr(bool *ifCond, bool *ifNull, T *trueValue, bool *trueNull, T *falseValue, bool *falseNull,
                             int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (!(ifCond[i] && !ifNull[i])) {
            trueValue[i] = falseValue[i];
            trueNull[i] = falseNull[i];
        }
    }
}

extern "C" DLLEXPORT void IfExprString(bool *ifCond, bool *ifNull, uint8_t **trueValue, bool *trueNull,
                                       int32_t *trueLength, uint8_t **falseValue, bool *falseNull, int32_t *falseLength,
                                       int32_t rowCnt);

template <typename T>
extern DLLEXPORT void SwitchExpr(int32_t whenCnt, int64_t *whenClauses, int64_t *whenBools, int64_t *resultValues,
                                 int64_t *resultNulls, T *elseValue, bool *elseNull, T *finalResult, bool *finalNull,
                                 int32_t rowCnt)
{
    std::vector<bool *> whenValues;
    std::vector<bool *> whenNulls;
    std::vector<T *> resValues;
    std::vector<bool *> resNulls;

    for (int i = 0; i < whenCnt; ++i) {
        whenValues.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(whenClauses[i])));
        whenNulls.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(whenBools[i])));
        resValues.push_back(reinterpret_cast<T *>(reinterpret_cast<void *>(resultValues[i])));
        resNulls.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(resultNulls[i])));
    }

    for (int i = 0; i < rowCnt; ++i) {
        bool hasSet = false;
        for (int j = 0; j < whenCnt; ++j) {
            if (whenValues[j][i] && !whenNulls[j][i]) {
                finalResult[i] = resValues[j][i];
                finalNull[i] = resNulls[j][i];
                hasSet = true;
                break;
            }
        }
        if (!hasSet) {
            finalResult[i] = elseValue[i];
            finalNull[i] = elseNull[i];
        }
    }
}

extern "C" DLLEXPORT void SwitchExprString(int32_t whenCnt, int64_t *whenClauses, int64_t *whenBools,
                                           int64_t *resultValues, int64_t *resultNulls, int64_t *resultLengths,
                                           uint8_t **elseValue, bool *elseNull, int32_t *elseLength,
                                           uint8_t **finalResult, bool *finalNull, int32_t *finalLength,
                                           int32_t rowCnt);

template <typename T>
extern DLLEXPORT void InExpr(int32_t cmpCnt, int64_t *cmpValues, int64_t *cmpBools, T *toCmpValue, bool *toCmpBool,
                             bool *finalResult, bool *finalNull, int32_t rowCnt)
{
    std::vector<T *> cmpValuesList;
    std::vector<bool *> cmpNullsList;

    for (int i = 0; i < cmpCnt; ++i) {
        cmpValuesList.push_back(reinterpret_cast<T *>(reinterpret_cast<void *>(cmpValues[i])));
        cmpNullsList.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(cmpBools[i])));
    }

    for (int i = 0; i < rowCnt; ++i) {
        finalResult[i] = false;
        finalNull[i] = false;
        for (int j = 0; j < cmpCnt; ++j) {
            if (!toCmpBool[i] && !cmpNullsList[j][i] && toCmpValue[i] == cmpValuesList[j][i]) {
                finalResult[i] = true;
                break;
            }
        }
    }
}

extern "C" DLLEXPORT void InExprString(int32_t cmpCnt, int64_t *cmpValues, int64_t *cmpBools, int64_t *cmpLengths,
                                       uint8_t **toCmpValue, bool *toCmpBool, int32_t *toCmpLength, bool *finalResult,
                                       bool *finalNull, int32_t rowCnt);
} // namespace omniruntime::codegen::function

#endif // OMNI_RUNTIME_BATCH_UTILFUNCTIONS_H
