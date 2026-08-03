/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Map Size function implementation
 */

#pragma once
#include <vector>
#include "vectorization/VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "vector/map_vector.h"
#include "vectorization/VectorReaders.h"
#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class MapSizeFunction : public VectorFunction {
public:
    explicit MapSizeFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        std::cout << "using vectorization mapsize" << std::endl;
        if (args.empty()) {
            OMNI_THROW("MapSizeFunction Error:", "No arguments provided");
        }

        auto boolArg = args.top();
        args.pop();
        auto mapArg = args.top();
        args.pop();

        result = VectorHelper::CreateFlatVector(OMNI_INT, mapArg->GetSize());
        if (!result) {
            OMNI_THROW("MapSizeFunction Error:", "Result vector is not a FlatVector<int32_t>");
        }

        int32_t rowSize = context->GetResultRowSize();

        MapVectorReader mapReader(mapArg);

        ProcessAllRows(static_cast<Vector<int32_t> *>(result), rowSize, mapReader);
    }

private:
    void ProcessAllRows(Vector<int32_t> *result, int32_t rowSize, const MapVectorReader &mapReader) const
    {
        for (int32_t row = 0; row < rowSize; ++row) {
            if (mapReader.containsNull(row)) {
                // If input map is NULL, output is -1
                result->SetValue(row, -1);
            } else {
                // Get map size and set result
                int64_t mapSize = mapReader.GetSize(row);
                if (mapSize > std::numeric_limits<int32_t>::max()) {
                    OMNI_THROW("MapSizeFunction Error:",
                        "Map size " + std::to_string(mapSize) + " exceeds int32_t range");
                }
                result->SetValue(row, static_cast<int32_t>(mapSize));
            }
        }
    }
};
}
