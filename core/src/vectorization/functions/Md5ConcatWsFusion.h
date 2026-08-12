/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Matcher for MD5 and concat_ws expression fusion
 */

#pragma once

#include <optional>
#include <string>

#include "expression/expressions.h"

namespace omniruntime::vectorization {

struct Md5ConcatWsFusionPlan {
    const expressions::FuncExpr *concatWs = nullptr;
    std::string fusedFunctionName;
};

class Md5ConcatWsFusion final {
public:
    static std::optional<Md5ConcatWsFusionPlan> Match(const expressions::FuncExpr &expression);
};

} // namespace omniruntime::vectorization
