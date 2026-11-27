/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 */
#pragma once

#include <stdexcept>
#include <string>

namespace omniruntime::mem {
// Only a clean Spark reservation denial is advisory. Reclaim/I/O failures and
// cancellation must propagate even when they happen during a speculative borrow.
class ReservationDenied final : public std::runtime_error {
public:
    explicit ReservationDenied(const std::string& message) : std::runtime_error(message) {}
};
} // namespace omniruntime::mem
