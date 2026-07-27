/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Time function for expression system
 * Parses a string in HH:mm:ss format and returns SQL time as microseconds since midnight.
 */

#pragma once
#include <string>
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {
void RegisterTimeFunction(const std::string &name);
}
