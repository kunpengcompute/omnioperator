/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CONVERT_TZ function for expression system
 *              Converts a datetime string (default ISO format 'yyyy-MM-dd HH:mm:ss')
 *              from one time zone to another. Mirrors Flink's
 *              DateTimeUtils.convertTz(String, String, String).
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterConvertTzFunction(const std::string &name);
}
