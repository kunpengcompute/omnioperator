/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Binary arithmetic execution specification
 */

#pragma once

namespace omniruntime::expressions {

enum class ArithmeticOp {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    INVALID,
};

enum class ArithmeticEvalMode {
    LEGACY,
    TRY,
    ANSI,
};

} // namespace omniruntime::expressions
