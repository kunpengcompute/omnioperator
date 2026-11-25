/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
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
 * Description: Murmur3 Hash Functions Registry
 */

#include "func_registry_hash.h"
#include "functions/murmur3_hash.h"
#include "functions/xxhash64_hash.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

std::vector<Function> HashFunctionRegistry::GetFunctions()
{
    DataTypeId retTypeInt = OMNI_INT;
    DataTypeId retTypeLong = OMNI_LONG;
    std::string mm3FnStr = "mm3hash";
    std::string xxH64FnStr = "xxhash64";
    std::vector<Function> hashRegistry = { // insert native function for combine hash math function
        Function(reinterpret_cast<void *>(CombineHash), "combine_hash", {}, { OMNI_LONG, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Int32), mm3FnStr, {}, { OMNI_INT, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Int32), mm3FnStr, {}, { OMNI_DATE32, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Int64), mm3FnStr, {}, { OMNI_LONG, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Int64), mm3FnStr, {}, { OMNI_TIMESTAMP, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Double), mm3FnStr, {}, { OMNI_DOUBLE, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3String), mm3FnStr, {}, { OMNI_VARCHAR, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Decimal64), mm3FnStr, {}, { OMNI_DECIMAL64, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Decimal128), mm3FnStr, {}, { OMNI_DECIMAL128, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(Mm3Boolean), mm3FnStr, {}, { OMNI_BOOLEAN, OMNI_INT }, retTypeInt,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Int8), xxH64FnStr, {}, { OMNI_BYTE, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Int16), xxH64FnStr, {}, { OMNI_SHORT, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Int32), xxH64FnStr, {}, { OMNI_INT, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Int32), xxH64FnStr, {}, { OMNI_DATE32, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Int64), xxH64FnStr, {}, { OMNI_LONG, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Int64), xxH64FnStr, {}, { OMNI_TIMESTAMP, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Double), xxH64FnStr, {}, { OMNI_DOUBLE, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64String), xxH64FnStr, {}, { OMNI_VARCHAR, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Decimal64), xxH64FnStr, {}, { OMNI_DECIMAL64, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Decimal128), xxH64FnStr, {}, { OMNI_DECIMAL128, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL),
        Function(reinterpret_cast<void *>(XxH64Boolean), xxH64FnStr, {}, { OMNI_BOOLEAN, OMNI_LONG }, retTypeLong,
            INPUT_DATA_AND_NULL)
    };

    return hashRegistry;
}
}
