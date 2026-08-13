/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Vectorized MD5 function with optional ISA-L Crypto backend
 */

#pragma once

#include "vectorization/VectorFunction.h"

namespace omniruntime::vectorization {

class Md5VectorFunction final : public VectorFunction {
public:
    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override;
};

std::shared_ptr<VectorFunction> MakeMd5VectorFunction(const std::string &name,
    const std::vector<type::DataTypeId> &inputArgs, const config::QueryConfig &config);

} // namespace omniruntime::vectorization
