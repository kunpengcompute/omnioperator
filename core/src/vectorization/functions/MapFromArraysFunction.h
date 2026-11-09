/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include <vector>
#include <string_view>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class MapFromArraysFunction : public VectorFunction {
    public:
        explicit MapFromArraysFunction () {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            // inputValues_ is the whole expression stack, not a fresh stack per call.
            // Nested map_from_arrays (for example ifnull(col, map(...))) already has the
            // outer arguments below these two.
            if (args.size() < 2) {
                OMNI_THROW("MapFromArrays Error:", "map_from_arrays requires exactly 2 array arguments");
            }

            BaseVector* valueVec = args.top();
            args.pop();
            BaseVector* keyVec = args.top();
            args.pop();

            if (keyVec == nullptr || valueVec == nullptr) {
                OMNI_THROW("MapFromArrays Error:", "map_from_arrays received null vector arg ment");
            }

            auto* keysArray = dynamic_cast<ArrayVector*>(keyVec);
            auto* valuesArray = dynamic_cast<ArrayVector*>(valueVec);
            if (keysArray == nullptr || valuesArray == nullptr) {
                OMNI_THROW("MapFromArrays Error:", "map_from_arrays arguments must be ARRAY type");
            }

            auto keyElements = keysArray->GetElementVector();
            auto valueElements = valuesArray->GetElementVector();

            if (keyElements->HasNull()) {
                OMNI_THROW("MapFromArrays Error:", "keysArray can not be null");
            }

            if ((keyElements->GetSize() != valueElements->GetSize()) && keyElements->GetSize() != 0 &&
                 valueElements->GetSize() != 0) {
                OMNI_THROW("MapFromArrays Error:", "keysArray size not equal valuesArray size");
            }

            int32_t totalRows = keysArray->GetSize();
            result = new MapVector(totalRows);
            auto *dstMapVec = dynamic_cast<MapVector *>(result);

            dstMapVec->SetKeyVector(std::move(keyElements));
            dstMapVec->SetValueVector(std::move(valueElements));

            for (int i = 0; i < totalRows; ++i) {
                dstMapVec->SetOffset(i+1,keysArray->GetOffset(i+1));
                if (keysArray->IsNull(i) || valuesArray->IsNull(i)) {
                    dstMapVec->SetNull(i);
                    continue;
                }
            }
            if (keyVec != nullptr) {
                delete keyVec;
            }
            if (valueVec != nullptr) {
                delete valueVec;
            }
        }
    };
}
