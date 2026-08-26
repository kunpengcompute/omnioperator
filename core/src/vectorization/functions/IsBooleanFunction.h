/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unified IS [NOT] {TRUE|FALSE} function implementation. 
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "type/data_operations.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class IsBooleanFunction : public VectorFunction {
public:
    explicit IsBooleanFunction(bool nullResult, bool negateValue)
        : nullResult_(nullResult), negateValue_(negateValue) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    /// Get bool value from vector (flat / const / dictionary)
    static bool GetValueFromVector(BaseVector *vec, int32_t row);

    bool nullResult_; // value returned when input is NULL (UNKNOWN).
    bool negateValue_; //if true, the non-NULL input value is negated before output.
// Mapping to SQL semantics:    IS NOT TRUE  : nullResult=true,  negateValue=true
// IS FALSE     : nullResult=false, negateValue=true
// IS NOT FALSE : nullResult=true,  negateValue=false
}; 
} 
