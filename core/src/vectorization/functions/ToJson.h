/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: to_json function for JSON operations
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "util/debug.h"
#include <string>
#include <string_view>
#include <sstream>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class ToJsonFunction final : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    void appendToJson(BaseVector *vec, int32_t row, const DataType *type, std::string &out) const;
    void appendToJsonFromSlice(BaseVector *vec, int32_t startIdx, int32_t count, const DataType *type, std::string &out) const;
    void appendArrayToJson(ArrayVector *arrVec, int32_t row, const DataType *elemType, std::string &out) const;
    void appendMapToJson(MapVector *mapVec, int32_t row, const DataType *keyType, const DataType *valType, std::string &out) const;
    void appendRowToJson(RowVector *rowVec, int32_t row, const RowType *rowType, std::string &out) const;
    void escapeJsonString(const std::string_view &s, std::string &out) const;
    template <typename T>
    T getValueFromVector(BaseVector *vec, int32_t row) const;
    std::string_view getStringFromVector(BaseVector *vec, int32_t row) const;
};
}
