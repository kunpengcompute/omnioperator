/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TRY arithmetic vector functions
 */

#pragma once

#include <string>

#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {

enum class TryArithmeticOp {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
};

// The wrapper owns TRY's row-level error policy. Type-specific kernels only
// report arithmetic errors and are shared by all four public functions.
class TryArithmeticFunction final : public VectorFunction {
public:
    explicit TryArithmeticFunction(TryArithmeticOp operation) : operation_(operation) {}

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override;

private:
    TryArithmeticOp operation_;
};

void RegisterTryArithmeticFunctions(const std::string &prefix);

} // namespace omniruntime::vectorization
