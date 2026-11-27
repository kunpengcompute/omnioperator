/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * Description: batch dictionary functions implementation
 */

#include "batch_dictionaryfunctions.h"
#include "vector/vector.h"
#include "codegen/context_helper.h"

using namespace omniruntime::vec;

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT void BatchGetIntFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t *rowIdxArray,
                                                          int32_t rowCnt, int32_t *output)
{
    auto dictionaryVectorPtr = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(dictionaryVectorAddr);
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = dictionaryVectorPtr->GetValue(rowIdxArray[i]);
    }
}

extern "C" DLLEXPORT void BatchGetLongFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t *rowIdxArray,
                                                           int32_t rowCnt, int64_t *output)
{
    auto dictionaryVectorPtr = reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(dictionaryVectorAddr);
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = dictionaryVectorPtr->GetValue(rowIdxArray[i]);
    }
}

extern "C" DLLEXPORT void BatchGetDoubleFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t *rowIdxArray,
                                                             int32_t rowCnt, double *output)
{
    auto dictionaryVectorPtr = reinterpret_cast<Vector<DictionaryContainer<double>> *>(dictionaryVectorAddr);
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = dictionaryVectorPtr->GetValue(rowIdxArray[i]);
    }
}

extern "C" DLLEXPORT void BatchGetBooleanFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t *rowIdxArray,
                                                              int32_t rowCnt, bool *output)
{
    auto dictionaryVectorPtr = reinterpret_cast<Vector<DictionaryContainer<bool>> *>(dictionaryVectorAddr);
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = dictionaryVectorPtr->GetValue(rowIdxArray[i]);
    }
}

extern "C" DLLEXPORT void BatchGetVarcharFromDictionaryVector(int64_t contextPtr, int64_t dictionaryVectorAddr,
                                                              int32_t *rowIdxArray, int32_t rowCnt, uint8_t **str,
                                                              int32_t *length)
{
    auto dictionaryVectorPtr = reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(dictionaryVectorAddr);
    for (int i = 0; i < rowCnt; ++i) {
        auto stringView = dictionaryVectorPtr->GetValue(rowIdxArray[i]);
        length[i] = stringView.length();
        str[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(stringView.data()));
    }
}

extern "C" DLLEXPORT void BatchGetDecimalFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t *rowIdxArray,
                                                              int32_t rowCnt, Decimal128 *output)
{
    auto dictionaryVectorPtr = reinterpret_cast<Vector<DictionaryContainer<type::Decimal128>> *>(dictionaryVectorAddr);
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = dictionaryVectorPtr->GetValue(rowIdxArray[i]);
    }
}

extern "C" DLLEXPORT void BatchGetIntFromVector(int32_t *vector, int32_t *rowIdxArray, int32_t rowCnt, int32_t *output)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = vector[rowIdxArray[i]];
    }
}

extern "C" DLLEXPORT void BatchGetLongFromVector(int64_t *vector, int32_t *rowIdxArray, int32_t rowCnt, int64_t *output)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = vector[rowIdxArray[i]];
    }
}

extern "C" DLLEXPORT void BatchGetDoubleFromVector(double *vector, int32_t *rowIdxArray, int32_t rowCnt, double *output)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = vector[rowIdxArray[i]];
    }
}

extern "C" DLLEXPORT void BatchGetBooleanFromVector(bool *vector, int32_t *rowIdxArray, int32_t rowCnt, bool *output)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = vector[rowIdxArray[i]];
    }
}

extern "C" DLLEXPORT void BatchGetVarcharFromVector(int64_t contextPtr, int32_t *offsetArray, const char *vector,
                                                    int32_t *rowIdxArray, int32_t rowCnt, uint8_t **str,
                                                    int32_t *length)
{
    errno_t err;
    char *ret;
    for (int i = 0; i < rowCnt; ++i) {
        length[i] = offsetArray[rowIdxArray[i] + 1] - offsetArray[rowIdxArray[i]];
        if (length[i] == 0) {
            str[i] = (uint8_t *)"";
            continue;
        }
        ret = ArenaAllocatorMalloc(contextPtr, length[i]);
        err = memcpy_s(ret, length[i], vector + offsetArray[rowIdxArray[i]], length[i]);
        if (err != EOK) {
            SetError(contextPtr, "Get string from vector failed");
            str[i] = nullptr;
            continue;
        }
        str[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchGetDecimalFromVector(Decimal128 *vector, int32_t *rowIdxArray, int32_t rowCnt,
                                                    Decimal128 *output)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = vector[rowIdxArray[i]];
    }
}
} // namespace omniruntime::codegen::function
