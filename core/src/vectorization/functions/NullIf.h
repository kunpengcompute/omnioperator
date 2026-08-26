/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: NullIf function for conditional expressions
*/

#pragma once
#include "vectorization/VectorFunction.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// nullif(expr1, expr2)->Returns NULL if expr1 equals expr2, otherwise returns expr1.
/// Supports BOOLEAN, BYTE, SHORT, INT, LONG, FLOAT, DOUBLE, VARCHAR, VARBINARY, DATE32, DATE64, TIMESTAMP, DECIMAL64, DECIMAL128.
class NullIfFunction : public VectorFunction {
public:
    explicit NullIfFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    void DispatchNullIf(BaseVector *expr1Vec, BaseVector *expr2Vec,
                        const DataTypePtr &outputType, BaseVector *&result) const;

    template<typename T>
    void NullIfNumeric(BaseVector *expr1Vec, BaseVector *expr2Vec, BaseVector *&result,
                       const DataTypePtr &outputType) const;

    void NullIfString(BaseVector *expr1Vec, BaseVector *expr2Vec, BaseVector *&result,
                      const DataTypePtr &outputType) const;

    template<typename T>
    T GetValueFromVector(BaseVector *vec, int32_t row) const;

    std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row) const;

    template<typename T>
    void SetValueToVector(BaseVector *vec, int32_t row, const T &value) const;

    void SetStringValueToVector(BaseVector *vec, int32_t row, const std::string_view &value) const;
};
}
