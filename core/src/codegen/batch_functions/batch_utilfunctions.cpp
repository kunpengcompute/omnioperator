/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: batch util functions implementation
 */

#include "batch_utilfunctions.h"
#include "codegen/context_helper.h"
#include "codegen/functions/stringfunctions.h"

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT void FillRowIndexArray(int32_t *dataArray, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = i;
    }
}

extern "C" DLLEXPORT void FillNull(bool *nullArray, bool isNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillBool(int32_t *dataArray, bool *nullArray, bool literal, bool isNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = literal;
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillInt32(int32_t *dataArray, bool *nullArray, int32_t literal, bool isNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = literal;
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillInt64(int64_t *dataArray, bool *nullArray, int64_t literal, bool isNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = literal;
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillDouble(double *dataArray, bool *nullArray, double literal, bool isNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = literal;
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillFloat(float *dataArray, bool *nullArray, float literal, bool isNull, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = literal;
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillDecimal128(Decimal128 *dataArray, bool *nullArray, __int128_t literal, bool isNull,
    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i].SetValue(literal);
        nullArray[i] = isNull;
    }
}

extern "C" DLLEXPORT void FillString(int64_t contextPtr, uint8_t **dataArray, bool *nullArray, int32_t *lengthArray,
    uint8_t *literal, bool isNull, int32_t length, int32_t rowCnt)
{
    char *ret;
    for (int i = 0; i < rowCnt; i++) {
        ret = ArenaAllocatorMalloc(contextPtr, length + 1);
        memcpy(ret, literal, length);
        dataArray[i] = reinterpret_cast<uint8_t *>(ret);
        nullArray[i] = isNull;
        lengthArray[i] = length;
    }
}

extern "C" DLLEXPORT void FillLength(int32_t *offsets, int32_t *rowIdxArray, int32_t *lengthArray, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        lengthArray[i] = offsets[rowIdxArray[i] + 1] - offsets[i];
    }
}

extern "C" DLLEXPORT void FillLengthInFuncExpr(int32_t *lengthArray, int32_t length, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        lengthArray[i] = length;
    }
}

extern "C" DLLEXPORT void CreateNot(bool *val, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        val[i] = !val[i];
    }
}

extern "C" DLLEXPORT void CreateOr(bool *left, bool *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] || right[i];
    }
}

extern "C" DLLEXPORT void CreateAnd(bool *left, bool *right, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        left[i] = left[i] && right[i];
    }
}

extern "C" DLLEXPORT void CreateAndNotBool(bool *dataArray, bool *nullArray, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = dataArray[i] && (!nullArray[i]);
    }
}

extern "C" DLLEXPORT int32_t CreateAndNot(bool *dataArray, bool *nullArray, int32_t *rowIdxArray, int32_t rowCnt)
{
    int selectedCnt = 0;
    for (int i = 0; i < rowCnt; i++) {
        if (dataArray[i] && (!nullArray[i])) {
            rowIdxArray[selectedCnt] = i;
            selectedCnt++;
        }
    }
    return selectedCnt;
}

extern "C" DLLEXPORT void CreateOrExpr(bool *left, bool *leftNull, bool *right, bool *rightNull, int32_t rowCnt)
{
    bool res;
    for (int i = 0; i < rowCnt; ++i) {
        res = left[i] || right[i];
        leftNull[i] = (leftNull[i] && rightNull[i]) || (leftNull[i] && !right[i]) || (rightNull[i] && !left[i]);
        left[i] = res;
    }
}

extern "C" DLLEXPORT void CreateAndExpr(bool *left, bool *leftNull, bool *right, bool *rightNull, int32_t rowCnt)
{
    bool tmpVal;
    for (int i = 0; i < rowCnt; i++) {
        tmpVal = left[i] && right[i];
        leftNull[i] = (leftNull[i] && rightNull[i]) || (leftNull[i] && right[i]) || (left[i] && rightNull[i]);
        left[i] = tmpVal;
    }
}

extern "C" DLLEXPORT void CopyNull(bool *dataArray, bool *output, int32_t *rowIdxArray, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[rowIdxArray[i]];
    }
}

extern "C" DLLEXPORT void CopyBoolean(bool *dataArray, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void CopyInt32(int32_t *dataArray, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void CopyInt64(int64_t *dataArray, int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void CopyDouble(double *dataArray, double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void CopyFloat(float *dataArray, float *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void CopyDecimal128(Decimal128 *dataArray, Decimal128 *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void CopyString(uint8_t **dataArray, uint8_t **output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        dataArray[i] = output[i];
    }
}

extern "C" DLLEXPORT void IfExprString(bool *ifCond, bool *ifNull, uint8_t **trueValue, bool *trueNull,
    int32_t *trueLength, uint8_t **falseValue, bool *falseNull, int32_t *falseLength, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (!(ifCond[i] && !ifNull[i])) {
            trueValue[i] = falseValue[i];
            trueNull[i] = falseNull[i];
            trueLength[i] = falseLength[i];
        }
    }
}

extern "C" DLLEXPORT void SwitchExprString(int32_t whenCnt, int64_t *whenClauses, int64_t *whenBools,
    int64_t *resultValues, int64_t *resultNulls, int64_t *resultLengths, uint8_t **elseValue, bool *elseNull,
    int32_t *elseLength, uint8_t **finalResult, bool *finalNull, int32_t *finalLength, int32_t rowCnt)
{
    std::vector<bool *> whenValues;
    std::vector<bool *> whenNulls;

    std::vector<uint8_t **> resValues;
    std::vector<bool *> resNulls;
    std::vector<int32_t *> resLengths;

    for (int i = 0; i < whenCnt; ++i) {
        whenValues.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(whenClauses[i])));
        whenNulls.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(whenBools[i])));
        resValues.push_back(reinterpret_cast<uint8_t **>(reinterpret_cast<void *>(resultValues[i])));
        resNulls.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(resultNulls[i])));
        resLengths.push_back(reinterpret_cast<int32_t *>(reinterpret_cast<void *>(resultLengths[i])));
    }

    for (int i = 0; i < rowCnt; ++i) {
        bool hasSet = false;
        for (int j = 0; j < whenCnt; ++j) {
            if (whenValues[j][i] && !whenNulls[j][i]) {
                finalResult[i] = resValues[j][i];
                finalNull[i] = resNulls[j][i];
                finalLength[i] = resLengths[j][i];
                hasSet = true;
                break;
            }
        }
        if (!hasSet) {
            finalResult[i] = elseValue[i];
            finalNull[i] = elseNull[i];
            finalLength[i] = elseLength[i];
        }
    }
}

extern "C" DLLEXPORT void CoalesceString(uint8_t **lArray, bool *lIsNull, int32_t *lLength, uint8_t **rArray,
    bool *rIsNull, int32_t *rLength, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (lIsNull[i]) {
            lArray[i] = rArray[i];
            lIsNull[i] = rIsNull[i];
            lLength[i] = rLength[i];
        }
    }
}

extern "C" DLLEXPORT void InExprString(int32_t cmpCnt, int64_t *cmpValues, int64_t *cmpBools, int64_t *cmpLengths,
    uint8_t **toCmpValue, bool *toCmpBool, int32_t *toCmpLength, bool *finalResult, bool *finalNull, int32_t rowCnt)
{
    std::vector<uint8_t **> cmpValuesList;
    std::vector<bool *> cmpNullsList;
    std::vector<int32_t *> cmpLengthsList;

    for (int32_t i = 0; i < cmpCnt; ++i) {
        cmpValuesList.push_back(reinterpret_cast<uint8_t **>(reinterpret_cast<void *>(cmpValues[i])));
        cmpNullsList.push_back(reinterpret_cast<bool *>(reinterpret_cast<void *>(cmpBools[i])));
        cmpLengthsList.push_back(reinterpret_cast<int32_t *>(reinterpret_cast<void *>(cmpLengths[i])));
    }

    for (int32_t i = 0; i < rowCnt; ++i) {
        finalResult[i] = false;
        finalNull[i] = false;
        bool hasNull = false;
        for (int32_t j = 0; j < cmpCnt; ++j) {
            if (!toCmpBool[i] && !cmpNullsList[j][i]) {
                if (StrCompare(reinterpret_cast<char *>(toCmpValue[i]), toCmpLength[i],
                    reinterpret_cast<char *>(cmpValuesList[j][i]), cmpLengthsList[j][i]) == 0) {
                    finalResult[i] = true;
                    break;
                }
            }
            if (cmpNullsList[j][i]) {
                hasNull = true;
            }
        }
        // Three-valued NULL logic: if no match and (value is NULL or any comparison value is NULL), result is NULL
        if (!finalResult[i]) {
            finalNull[i] = toCmpBool[i] || hasNull;
        }
    }
}

extern "C" DLLEXPORT void InSubqueryExprString(const uint8_t **subqueryValues, const bool *subqueryNulls,
    const int32_t *subqueryLengths, int32_t subqueryRowCount, uint8_t **probeValue, const bool *probeNull,
    const int32_t *probeLength, bool *finalResult, bool *finalNull, int32_t rowCnt)
{
    // Check if subquery contains any NULL values
    bool subqueryHasNull = false;
    for (int32_t i = 0; i < subqueryRowCount; ++i) {
        if (subqueryNulls[i]) {
            subqueryHasNull = true;
            break;
        }
    }

    for (int32_t i = 0; i < rowCnt; ++i) {
        finalResult[i] = false;
        finalNull[i] = false;

        // If probe value is NULL, result is NULL
        if (probeNull[i]) {
            finalNull[i] = true;
            continue;
        }

        // Linear scan through subquery results
        bool matched = false;
        for (int32_t j = 0; j < subqueryRowCount; ++j) {
            if (!subqueryNulls[j]) {
                if (StrCompare(reinterpret_cast<char *>(probeValue[i]), probeLength[i],
                    reinterpret_cast<char *>(subqueryValues[j]), subqueryLengths[j]) == 0) {
                    finalResult[i] = true;
                    matched = true;
                    break;
                }
            }
        }

        // Three-valued NULL logic: no match + subquery has NULL -> NULL
        if (!matched && subqueryHasNull) {
            finalNull[i] = true;
        }
    }
}
}
