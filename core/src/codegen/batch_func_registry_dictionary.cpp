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
 * Description: Batch Dictionary Function Registry
 */

#include "batch_func_registry_dictionary.h"
#include "batch_functions/batch_dictionaryfunctions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

std::vector<Function> BatchDictionaryFunctionRegistry::GetFunctions()
{
    std::vector<DataTypeId> paramTypes = {OMNI_LONG};
    std::vector<Function> batchDictionaryFnRegistry = {
        Function(reinterpret_cast<void *>(BatchGetIntFromDictionaryVector), "batch_GetDic", {}, paramTypes, OMNI_INT),
        Function(reinterpret_cast<void *>(BatchGetIntFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_DATE32),
        Function(reinterpret_cast<void *>(BatchGetLongFromDictionaryVector), "batch_GetDic", {}, paramTypes, OMNI_LONG),
        Function(reinterpret_cast<void *>(BatchGetLongFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_DECIMAL64),
        Function(reinterpret_cast<void *>(BatchGetLongFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_TIMESTAMP),
        Function(reinterpret_cast<void *>(BatchGetDoubleFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_DOUBLE),
        Function(reinterpret_cast<void *>(BatchGetBooleanFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_BOOLEAN),
        Function(reinterpret_cast<void *>(BatchGetVarcharFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_VARCHAR),
        Function(reinterpret_cast<void *>(BatchGetVarcharFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_CHAR),
        Function(reinterpret_cast<void *>(BatchGetDecimalFromDictionaryVector), "batch_GetDic", {}, paramTypes,
                 OMNI_DECIMAL128),
        Function(reinterpret_cast<void *>(BatchGetIntFromVector), "batch_GetData", {}, paramTypes, OMNI_INT),
        Function(reinterpret_cast<void *>(BatchGetIntFromVector), "batch_GetData", {}, paramTypes, OMNI_DATE32),
        Function(reinterpret_cast<void *>(BatchGetLongFromVector), "batch_GetData", {}, paramTypes, OMNI_LONG),
        Function(reinterpret_cast<void *>(BatchGetLongFromVector), "batch_GetData", {}, paramTypes, OMNI_DECIMAL64),
        Function(reinterpret_cast<void *>(BatchGetLongFromVector), "batch_GetData", {}, paramTypes, OMNI_TIMESTAMP),
        Function(reinterpret_cast<void *>(BatchGetDoubleFromVector), "batch_GetData", {}, paramTypes, OMNI_DOUBLE),
        Function(reinterpret_cast<void *>(BatchGetBooleanFromVector), "batch_GetData", {}, paramTypes, OMNI_BOOLEAN),
        Function(reinterpret_cast<void *>(BatchGetVarcharFromVector), "batch_GetData", {}, paramTypes, OMNI_VARCHAR),
        Function(reinterpret_cast<void *>(BatchGetVarcharFromVector), "batch_GetData", {}, paramTypes, OMNI_CHAR),
        Function(reinterpret_cast<void *>(BatchGetDecimalFromVector), "batch_GetData", {}, paramTypes,
                 OMNI_DECIMAL128)};
    return batchDictionaryFnRegistry;
}
} // namespace omniruntime::codegen
