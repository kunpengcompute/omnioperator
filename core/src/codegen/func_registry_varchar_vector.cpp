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
 * Description: Varchar Vector Functions Registry
 */

#include "func_registry_varchar_vector.h"
#include "functions/varcharVectorfunctions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

std::vector<Function> VarcharVectorFunctionRegistry::GetFunctions()
{
    std::vector<DataTypeId> paramTypes = {OMNI_LONG, OMNI_INT, OMNI_VARCHAR};
    std::vector<Function> varcharVectorFnRegistry = {
        Function(reinterpret_cast<void *>(WrapVarcharVector), "WrapVarcharVector", {}, paramTypes, OMNI_INT),
        Function(reinterpret_cast<void *>(WrapSetBitNull), "WrapSetBitNull", {}, {OMNI_INT}, OMNI_BOOLEAN),
        Function(reinterpret_cast<void *>(WrapIsBitNull), "WrapIsBitNull", {}, {OMNI_INT}, OMNI_BOOLEAN)};
    return varcharVectorFnRegistry;
}
} // namespace omniruntime::codegen
