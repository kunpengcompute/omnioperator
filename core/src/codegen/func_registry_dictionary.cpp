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
 * Description: Dictionary Functions Registry
 */

#include "func_registry_dictionary.h"
#include "functions/dictionaryfunctions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace omniruntime::codegen::function;

std::vector<Function> DictionaryFunctionRegistry::GetFunctions()
{
    std::vector<DataTypeId> paramTypes = {OMNI_LONG, OMNI_INT};
    std::vector<DataTypeId> getStringParamTypes = {OMNI_LONG, OMNI_INT, OMNI_INT};
    std::vector<Function> dictionaryFnRegistry = {
        Function(reinterpret_cast<void *>(GetIntFromDictionaryVector), "DictionaryGetInt", {}, paramTypes, OMNI_INT),
        Function(reinterpret_cast<void *>(GetByteFromDictionaryVector), "DictionaryGetByte", {}, paramTypes, OMNI_BYTE),
        Function(reinterpret_cast<void *>(GetShortFromDictionaryVector), "DictionaryGetShort", {}, paramTypes,
                 OMNI_SHORT),
        Function(reinterpret_cast<void *>(GetLongFromDictionaryVector), "DictionaryGetLong", {}, paramTypes, OMNI_LONG),
        Function(reinterpret_cast<void *>(GetDoubleFromDictionaryVector), "DictionaryGetDouble", {}, paramTypes,
                 OMNI_DOUBLE),
        Function(reinterpret_cast<void *>(GetBooleanFromDictionaryVector), "DictionaryGetBoolean", {}, paramTypes,
                 OMNI_BOOLEAN),
        Function(reinterpret_cast<void *>(GetVarcharFromDictionaryVector), "DictionaryGetVarchar", {}, paramTypes,
                 OMNI_VARCHAR),
        Function(reinterpret_cast<void *>(GetDecimalFromDictionaryVector), "DictionaryGetDecimal", {}, paramTypes,
                 OMNI_DECIMAL128)};
    return dictionaryFnRegistry;
}
} // namespace omniruntime::codegen
