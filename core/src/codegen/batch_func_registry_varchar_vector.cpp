/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Description: Batch varchar vector Function Registry
 */

#include "batch_func_registry_varchar_vector.h"
#include "batch_functions/batch_varcharVectorfunctions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

std::vector<Function> BatchVarcharVectorFunctionRegistry::GetFunctions()
{
    std::vector<DataTypeId> paramTypes = { OMNI_LONG, OMNI_VARCHAR, OMNI_INT, OMNI_INT };
    std::vector<Function> batchVarcharVectorFnRegistry = { Function(reinterpret_cast<void *>(BatchWrapVarcharVector),
        "batch_WrapVarcharVector", {}, paramTypes, OMNI_INT),
        Function(reinterpret_cast<void *>(BatchNullArrayToBits), "batch_NullArrayToBits", {}, { OMNI_BOOLEAN },
            OMNI_BOOLEAN),
        Function(reinterpret_cast<void *>(BatchBitsToNullArray), "batch_BitsToNullArray", {}, { OMNI_BOOLEAN },
            OMNI_BOOLEAN) };
    return batchVarcharVectorFnRegistry;
}
}
