/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Fused concat_ws and MD5 vector function
 */

#pragma once

#include <cstddef>

#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {

class FusedMd5ConcatWsFunction final : public VectorFunction {
public:
    explicit FusedMd5ConcatWsFunction(size_t numArgs) : numArgs_(numArgs) {}

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override;

private:
    size_t numArgs_;
};

std::vector<std::shared_ptr<codegen::FunctionSignature>> FusedMd5ConcatWsSignatures(const std::string &name);

std::shared_ptr<VectorFunction> MakeFusedMd5ConcatWsFunction(const std::string &name,
    const std::vector<type::DataTypeId> &inputArgs, const config::QueryConfig &config);

} // namespace omniruntime::vectorization
