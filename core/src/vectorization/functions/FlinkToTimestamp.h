/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_to_timestamp function for expression system
 *              Converts a date time string to a TIMESTAMP (OMNI_LONG = millis
 *              since epoch). Mirrors Flink's TO_TIMESTAMP(string1[, string2])
 *              semantics: 1-arg uses default format 'yyyy-MM-dd HH:mm:ss';
 *              2-arg uses the given format. No session timezone is applied
 *              (Flink "under the 'UTC+0' time zone" — wall-clock stored as-is).
 *              Returns NULL on parse failure or NULL input.
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterFlinkToTimestampFunction(const std::string &name);
}
