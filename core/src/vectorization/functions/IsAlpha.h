/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS_ALPHA function implementation.
 *   IS_ALPHA(string) -> boolean.
 *   Returns true if the string is non-empty and every character is a Unicode
 *   letter (general categories Lu/Ll/Lt/Lm/Lo, matching Java Character.isLetter);
 *   otherwise false. NULL input returns false (output is NOT null).
 *   Aligned with Flink SqlFunctionUtils.isAlpha / Apache Commons Lang3 StringUtils.isAlpha.
 */

#pragma once
#include <string>

namespace omniruntime::vectorization {

/// Registers IS_ALPHA for VARCHAR/CHAR (per-character Unicode-letter check) and
/// numeric input types (always false, matching Flink `!(obj instanceof String)`).
/// Registration name: "is_alpha" (lowercase, no prefix — Path B convention).
void RegisterIsAlphaFunction(const std::string &name);

}  // namespace omniruntime::vectorization
