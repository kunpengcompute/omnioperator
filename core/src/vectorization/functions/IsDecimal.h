/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS_DECIMAL function implementation.
 *   IS_DECIMAL(string) -> boolean.
 *   Returns true if string can be parsed to a valid numeric, otherwise false.
 *   NULL input returns false (output is NOT null); empty string -> false.
 *   Aligned with Flink SqlFunctionUtils.isDecimal, which reduces to whether the
 *   string is accepted by Java Double.parseDouble (covers Integer/Long/Double).
 */

#pragma once
#include <string>

namespace omniruntime::vectorization {

/// Registers IS_DECIMAL for VARCHAR/CHAR (Java Double.parseDouble grammar) and
/// numeric input types (always true for non-null, matching Flink instanceof-numeric).
/// Registration name: "is_decimal" (lowercase, no prefix — Path B convention).
void RegisterIsDecimalFunction(const std::string &name);

}  // namespace omniruntime::vectorization
