/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: registration of string functions whose Flink semantics differ from Spark's
 *
 * Kept apart from RegisterString.cpp so that adapting a Flink expression never touches the
 * Spark registrations. Names are prefixed with "flink_" and are emitted by RexNodeUtil's
 * simpleFunctionNameMap; overloads that only differ in their return type reuse the Spark name.
 */

#include <string>
#include "../functions/String.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterFlinkStringFunctions(const std::string &prefix)
{
    // Flink FROM_BASE64(string) -> STRING: decoded bytes are interpreted as a UTF-8 string.
    // A VARCHAR-returning overload makes the result marshallable to RowData and matches the
    // signature RexNodeUtil emits (returnType=VARCHAR). Same UnBase64Function body as the
    // VARBINARY-returning Spark overload.
    RegisterFunction<UnBase64Function, std::string, std::string_view>(
        prefix + "unbase64", {OMNI_VARCHAR}, OMNI_VARCHAR);

    // Flink REPLACE (SqlFunctionUtils.replace -> Java String.replace): an empty search string
    // inserts the replacement around every character, where Spark returns the input unchanged.
    RegisterFunction<FlinkReplaceFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "flink_replace", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);

    // Flink SUBSTRING/SUBSTR (BinaryStringDataUtil.substringSQL): a negative length yields
    // NULL and an out-of-range negative position yields an empty string, where Spark yields
    // an empty string and a prefix respectively.
    RegisterFunction<FlinkSubstrFunction, std::string, std::string_view, int32_t>(
        prefix + "flink_substr", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<FlinkSubstrFunction, std::string, std::string_view, int32_t, int32_t>(
        prefix + "flink_substr", {OMNI_VARCHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<FlinkSubstrFunction, std::string, std::string_view, int32_t>(
        prefix + "flink_substr", {OMNI_CHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<FlinkSubstrFunction, std::string, std::string_view, int32_t, int32_t>(
        prefix + "flink_substr", {OMNI_CHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);
}
}
