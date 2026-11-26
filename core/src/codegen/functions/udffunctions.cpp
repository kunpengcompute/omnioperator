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
 * Description: udf functions.
 */

#include <mutex>
#include "codegen/context_helper.h"
#include "udf/cplusplus/java_udf_functions.h"
#include "udffunctions.h"

using namespace omniruntime::udf;

namespace omniruntime::codegen::function {
namespace {
std::once_flag init_udf_flag;
bool g_isUdfInited;
const std::string INIT_UDF_FAILED = "Init UDF failed";
} // namespace

static void InitHiveUdf()
{
    auto ret = InitUdf();
    if (ret != omniruntime::op::ErrorCode::SUCCESS) {
        g_isUdfInited = false;
    } else {
        g_isUdfInited = true;
    }
}

extern DLLEXPORT void EvaluateHiveUdfSingle(int64_t contextPtr, const char *udfClass, int32_t *inputTypes,
                                            int32_t retType, int32_t vecCount, int64_t inputValue, int64_t inputNull,
                                            int64_t inputLength, int64_t outputValue, int64_t outputNull,
                                            int64_t outputLength)
{
    std::call_once(init_udf_flag, InitHiveUdf);
    if (!g_isUdfInited) {
        SetError(contextPtr, INIT_UDF_FAILED);
        return;
    }
    ExecuteHiveUdfSingle(contextPtr, udfClass, inputTypes, retType, vecCount, inputValue, inputNull, inputLength,
                         outputValue, outputNull, outputLength);
}

extern DLLEXPORT void EvaluateHiveUdfBatch(int64_t contextPtr, const char *udfClass, int32_t *inputTypes,
                                           int32_t retType, int32_t vecCount, int32_t rowCount, int64_t *inputValues,
                                           int64_t *inputNulls, int64_t *inputLengths, int64_t outputValue,
                                           int64_t outputNull, int64_t outputLength)
{
    std::call_once(init_udf_flag, InitHiveUdf);
    if (!g_isUdfInited) {
        SetError(contextPtr, INIT_UDF_FAILED);
        return;
    }
    ExecuteHiveUdfBatch(contextPtr, udfClass, inputTypes, retType, vecCount, rowCount, inputValues, inputNulls,
                        inputLengths, outputValue, outputNull, outputLength);
}
} // namespace omniruntime::codegen::function
