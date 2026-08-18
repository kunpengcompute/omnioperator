/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Binary arithmetic vector function
 */

#pragma once

#include "expression/arithmetic_spec.h"
#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {

class BinaryArithmeticFunction final : public VectorFunction {
public:
    BinaryArithmeticFunction(expressions::ArithmeticOp operation, expressions::ArithmeticEvalMode evalMode,
        const type::DataTypePtr &leftType, const type::DataTypePtr &rightType,
        const type::DataTypePtr &resultType);

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override;

private:
    expressions::ArithmeticOp operation_;
    expressions::ArithmeticEvalMode evalMode_;
    type::DataTypePtr leftType_;
    type::DataTypePtr rightType_;
    type::DataTypePtr resultType_;
};

std::shared_ptr<VectorFunction> CreateBinaryArithmeticFunction(expressions::ArithmeticOp operation,
    expressions::ArithmeticEvalMode evalMode, const type::DataTypePtr &leftType,
    const type::DataTypePtr &rightType, const type::DataTypePtr &resultType);

} // namespace omniruntime::vectorization
