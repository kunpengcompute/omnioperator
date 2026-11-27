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
 * Description: String Function Registry
 */

#include "json_functions.h"
using json = nlohmann::json;

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT const char *GetJsonObject(int64_t contextPtr, const char *jsonStr, int32_t jsonLen,
                                               bool jsonIsNull, const char *pathStr, int32_t pathLen, bool pathIsNull,
                                               bool *retIsNull, int32_t *outLen)
{
    *retIsNull = true;
    *outLen = 0;
    if (jsonIsNull || pathIsNull || jsonStr == nullptr || pathStr == nullptr || jsonLen <= 0 || pathLen <= 0) {
        return nullptr;
    }
    std::string_view json_sv(jsonStr, jsonLen);
    std::string_view path_sv(pathStr, pathLen);
    auto tok = std::make_unique<JsonPathTokenizer>();
    if (!tok->reset(path_sv)) {
        return nullptr;
    }

    std::optional<std::string> resultOpt;
    try {
        resultOpt = traverse_and_extract(json_sv, *tok);
        if (!resultOpt.has_value()) {
            return nullptr;
        }
    } catch (const json::exception &e) {
        // WARN: Here return null if parsing failed, as the Spark does.
        // std::ostringstream errorMessage;
        // errorMessage << "ERROR ! nlohmann json exception id : " << e.id << " for " << e.what();
        // SetError(contextPtr, errorMessage.str());
        return nullptr;
    }

    const std::string &result = resultOpt.value();
    *retIsNull = false;
    *outLen = static_cast<int32_t>(result.size());

    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
    if (memcpy_s(ret, *outLen + 1, result.c_str(), result.size()) != EOK) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    static_cast<char *>(ret)[*outLen] = '\0';
    return ret;
}
} // namespace omniruntime::codegen::function
