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
 * Description: Batch Date Time Function Registry
 */

#include "batch_func_registry_datetime.h"
#include "batch_functions/batch_datetime_functions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace omniruntime::codegen::function;

std::vector<Function> BatchDateTimeFunctionRegistry::GetFunctions()
{
    static std::vector<Function> batchDateTimeFunctions = {
        Function(reinterpret_cast<void *>(BatchUnixTimestampFromStr), "batch_unix_timestamp", {},
                 {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG,
                 INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(BatchUnixTimestampFromDate), "batch_unix_timestamp", {},
                 {OMNI_DATE32, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchFromUnixTime), "batch_from_unixtime", {},
                 {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchFromUnixTimeRetNull), "batch_from_unixtime_null", {},
                 {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true)};

    return batchDateTimeFunctions;
}
} // namespace omniruntime::codegen
