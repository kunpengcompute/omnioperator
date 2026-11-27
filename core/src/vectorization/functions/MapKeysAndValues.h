/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
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

    template <bool isKeys>
    class MapKeysAndValuesFunction : public VectorFunction {
    public:
        explicit MapKeysAndValuesFunction () {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector* inputVec = args.top();
            args.pop();
            auto* srcMapVec = dynamic_cast<MapVector*>(inputVec);
            auto keyElements = srcMapVec->GetKeyVector();
            auto valueElements = srcMapVec->GetValueVector();

            int32_t totalRows = inputVec->GetSize();
            result = new ArrayVector(totalRows, context->GetPool());
            auto *dstArrayVec = dynamic_cast<ArrayVector *>(result);

            if (isKeys) {
                dstArrayVec->SetElementVector(std::move(keyElements));
            } else {
                dstArrayVec->SetElementVector(std::move(valueElements));
            }
            for (int i = 0; i < totalRows; ++i) {
                dstArrayVec->SetOffset(i+1, srcMapVec->GetOffset(i+1));
                if (srcMapVec->IsNull(i)) {
                    dstArrayVec->SetNull(i);
                    continue;
                }
            }

            if (inputVec != nullptr) {
                delete inputVec;
            }
        }
    };
}