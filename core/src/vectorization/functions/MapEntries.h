/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MapEntries function for map operations
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "type/data_operations.h"
#include <vector>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    /// map_entries function
    /// map_entries(map(K,V)) -> array(row(K,V))
    /// Returns an array of all entries in the given map.
    /// Each entry is a row containing the key and value.
    class MapEntriesFunction : public VectorFunction {
    public:
        explicit MapEntriesFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *inputVec = args.top();
            args.pop();
            auto *srcMapVec = dynamic_cast<MapVector *>(inputVec);

            int32_t totalRows = inputVec->GetSize();
            result = new ArrayVector(totalRows, context->GetPool());
            auto *dstArrayVec = dynamic_cast<ArrayVector *>(result);

            auto keyElements = srcMapVec->GetKeyVector();
            auto valueElements = srcMapVec->GetValueVector();

            int64_t totalEntries = std::min(keyElements->GetSize(), valueElements->GetSize());
            auto *rowVec = new RowVector(static_cast<int32_t>(totalEntries), context->GetPool());
            rowVec->AddChild(keyElements);
            rowVec->AddChild(valueElements);

            dstArrayVec->SetElementVector(std::shared_ptr<BaseVector>(rowVec));

            for (int i = 0; i < totalRows; ++i) {
                dstArrayVec->SetOffset(i + 1, srcMapVec->GetOffset(i + 1));
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
