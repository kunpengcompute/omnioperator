/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Description: String Function Registry
 */
#include "func_registry_json.h"
#include "functions/json_functions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

const std::string GetJsonObjectFnStr()
{
    const std::string getJsonFnStr = "GetJsonObject";
    return getJsonFnStr;
}

std::vector<Function> JsonFunctionRegistry::GetFunctions()
{
    std::vector<Function> jsonFnRegistry = {Function(reinterpret_cast<void *>(GetJsonObject), GetJsonObjectFnStr(), {},
                                                     {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR,
                                                     INPUT_DATA_AND_NULL_AND_RETURN_NULL, true)};
    return jsonFnRegistry;
}
} // namespace omniruntime::codegen
