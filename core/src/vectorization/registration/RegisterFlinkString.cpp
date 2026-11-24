/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: registration of string functions whose Flink semantics differ from Spark's
 *
 * Kept apart from RegisterString.cpp so that adapting a Flink expression never touches the
 * Spark registrations. Names are prefixed with "flink_" and are emitted by RexNodeUtil's
 * simpleFunctionNameMap; overloads that only differ in their return type reuse the Spark name.
 * Flink-only functions that Spark does not provide keep their original names.
 */

#include <string>
#include "../functions/String.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterSplitIndex(const std::string &prefix);
void RegisterCountChar(const std::string &prefix);

void RegisterFlinkStringFunctions(const std::string &prefix)
{
    // Flink FROM_BASE64(string) -> STRING: decoded bytes are interpreted as a UTF-8 string.
    // A VARCHAR-returning overload makes the result marshallable to RowData and matches the
    // signature RexNodeUtil emits (returnType=VARCHAR). Same UnBase64Function body as the
    // VARBINARY-returning Spark overload.
    RegisterFunction<UnBase64Function, std::string, std::string_view>(
        prefix + "unbase64", {OMNI_VARCHAR}, OMNI_VARCHAR);

    // Flink TO_BASE64 / BASE64(string) -> varchar
    // Encodes to Base64 WITHOUT MIME line wrapping (single continuous line, no CRLF),
    // aligned with Flink SQL TO_BASE64 (RFC 4648). Spark base64 emits CRLF every 76 chars.
    RegisterFunction<FlinkBase64Function, std::string, std::string_view>(
        prefix + "flink_base64", {OMNI_VARCHAR}, OMNI_VARCHAR);

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

    // Flink LOCATE(substring, string, start) -> integer
    // Differs from Spark locate only in that start == 0 is treated as start == 1
    // (Spark returns 0 for start < 1). Same body as LocateFunction otherwise.
    // Support all combinations of VARCHAR/CHAR string types with INT32 integer type.
    RegisterFunction<FlinkLocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "flink_locate", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<FlinkLocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "flink_locate", {OMNI_VARCHAR, OMNI_CHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<FlinkLocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "flink_locate", {OMNI_CHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<FlinkLocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "flink_locate", {OMNI_CHAR, OMNI_CHAR, OMNI_INT}, OMNI_INT);

    // Flink INITCAP(string) -> string
    // Capitalizes the first letter of each word; lowercases the rest.
    // Word boundaries are any non-alphanumeric characters (Flink SQL semantics),
    // where Spark initcap only treats whitespace as a word boundary.
    RegisterFunction<FlinkInitCapFunction, std::string, std::string_view>(
        prefix + "flink_initcap", {OMNI_VARCHAR}, OMNI_VARCHAR);

    // Flink LPAD (SqlFunctionUtils.lpad): a negative length or empty pad string
    // yields NULL, where Spark's "lpad" yields an empty string.
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_CHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_CHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_VARCHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_CHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkLPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_lpad", {OMNI_CHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);

    // Flink RPAD (SqlFunctionUtils.rpad): same NULL boundaries as flink_lpad.
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_CHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_CHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_VARCHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_CHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<FlinkRPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "flink_rpad", {OMNI_CHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);

    // Spark has neither function. Keep the names RexNodeUtil already emits.
    RegisterSplitIndex(prefix);
    RegisterCountChar(prefix);
}
}
