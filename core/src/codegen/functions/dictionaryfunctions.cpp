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
 * Description: registry dictionary functions
 */

#include "dictionaryfunctions.h"
#include "vector/vector.h"
#include "codegen/context_helper.h"

using namespace omniruntime::vec;
using namespace std;

namespace omniruntime::codegen::function {
extern DLLEXPORT int32_t GetIntFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index)
{
    auto dictionaryVectorPtr = reinterpret_cast<vec::Vector<vec::DictionaryContainer<int32_t>> *>(dictionaryVectorAddr);
    return dictionaryVectorPtr->GetValue(index);
}

extern DLLEXPORT int8_t GetByteFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index)
{
    auto dictionaryVectorPtr = reinterpret_cast<vec::Vector<vec::DictionaryContainer<int8_t>> *>(dictionaryVectorAddr);
    return dictionaryVectorPtr->GetValue(index);
}

extern DLLEXPORT int16_t GetShortFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index)
{
    auto dictionaryVectorPtr = reinterpret_cast<vec::Vector<vec::DictionaryContainer<int16_t>> *>(dictionaryVectorAddr);
    return dictionaryVectorPtr->GetValue(index);
}

extern DLLEXPORT int64_t GetLongFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index)
{
    auto dictionaryVectorPtr = reinterpret_cast<vec::Vector<vec::DictionaryContainer<int64_t>> *>(dictionaryVectorAddr);
    return dictionaryVectorPtr->GetValue(index);
}

extern DLLEXPORT double GetDoubleFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index)
{
    auto dictionaryVectorPtr = reinterpret_cast<vec::Vector<vec::DictionaryContainer<double>> *>(dictionaryVectorAddr);
    return dictionaryVectorPtr->GetValue(index);
}

extern DLLEXPORT bool GetBooleanFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index)
{
    auto dictionaryVectorPtr = reinterpret_cast<vec::Vector<vec::DictionaryContainer<bool>> *>(dictionaryVectorAddr);
    return dictionaryVectorPtr->GetValue(index);
}

extern DLLEXPORT uint8_t *GetVarcharFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index,
                                                         int32_t *lengthPtr)
{
    auto dictionaryVectorPtr =
        reinterpret_cast<vec::Vector<vec::DictionaryContainer<std::string_view>> *>(dictionaryVectorAddr);
    auto stringView = dictionaryVectorPtr->GetValue(index);
    int32_t length = stringView.length();
    *lengthPtr = length;
    return (uint8_t *)stringView.data();
}

extern DLLEXPORT void GetDecimalFromDictionaryVector(int64_t dictionaryVectorAddr, int32_t index, int32_t outPrecision,
                                                     int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    auto dictionaryVectorPtr =
        reinterpret_cast<vec::Vector<vec::DictionaryContainer<type::Decimal128>> *>(dictionaryVectorAddr);
    auto value = dictionaryVectorPtr->GetValue(index);
    *outLowPtr = value.LowBits();
    *outHighPtr = value.HighBits();
}
} // namespace omniruntime::codegen::function
