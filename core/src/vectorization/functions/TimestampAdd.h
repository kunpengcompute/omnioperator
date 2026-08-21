/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TimestampAdd function for expression system
 * Adds an interval to a timestamp using the specified time unit.
 * Supports: SECOND, MINUTE, HOUR, DAY, MONTH, YEAR
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterTimestampAddFunction(const std::string &name);
}
