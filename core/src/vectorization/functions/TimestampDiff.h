/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TimestampDiff function for expression system
 * Returns the difference between two timestamps in the specified time unit.
 * Supports: SECOND, MINUTE, HOUR, DAY, MONTH, YEAR
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterTimestampDiffFunction(const std::string &name);
}
