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
 * Description: MightContain Function Registry
 */

#include "func_registry_might_contain.h"
#include "functions/mightcontain.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace omniruntime::codegen::function;

std::vector<Function> MightContainFunctionRegistry::GetFunctions()
{
    DataTypeId retTypeBoolean = OMNI_BOOLEAN;
    std::string mightContainFnStr = "might_contain";
    std::vector<Function> mightContainRegistry = {// insert native function for might contain function
                                                  Function(reinterpret_cast<void *>(MightContain), mightContainFnStr,
                                                           {}, {OMNI_LONG, OMNI_LONG}, retTypeBoolean, INPUT_DATA)};

    return mightContainRegistry;
}
} // namespace omniruntime::codegen
