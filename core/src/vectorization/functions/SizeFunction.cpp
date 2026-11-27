/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Size function implementation for Array and Map types
 */

#include "SizeFunction.h"
#include <limits>
#include "vectorization/SelectivityVector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vectorization/VectorReaders.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

void SizeFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("SizeFunction Error:", "No arguments provided");
    }

    // Extract legacySizeOfNull parameter (second argument, boolean)
    // Stack order: top = legacySizeOfNull, bottom = input (array/map)
    BaseVector *boolArg = nullptr;
    bool legacySizeOfNull = false;
    if (args.size() >= 2) {
        boolArg = args.top();
        args.pop();
        // Extract boolean value from the vector
        // Handle both ConstVector and FlatVector for legacySizeOfNull parameter
        if (boolArg != nullptr && boolArg->GetTypeId() == OMNI_BOOLEAN) {
            if (boolArg->GetEncoding() == OMNI_ENCODING_CONST) {
                // ConstVector case: from Gluten LiteralTransformer
                auto *constBoolVec = dynamic_cast<ConstVector<bool> *>(boolArg);
                if (constBoolVec != nullptr) {
                    legacySizeOfNull = constBoolVec->GetConstValue();
                }
            } else {
                // FlatVector case: for unit tests or other scenarios
                auto *boolVec = dynamic_cast<Vector<bool> *>(boolArg);
                if (boolVec != nullptr && boolVec->GetSize() > 0) {
                    legacySizeOfNull = boolVec->GetValue(0);
                }
            }
        }
    }

    // Get input vector (array or map)
    auto inputArg = args.top();
    args.pop();

    if (inputArg == nullptr) {
        OMNI_THROW("SizeFunction Error:", "Input vector is null");
    }

    int32_t rowSize = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_INT, rowSize);
    if (!result) {
        OMNI_THROW("SizeFunction Error:", "Failed to create result vector");
    }

    auto *resultVec = dynamic_cast<Vector<int32_t> *>(result);
    if (!resultVec) {
        OMNI_THROW("SizeFunction Error:", "Result vector is not a FlatVector<int32_t>");
    }

    // Process based on input type
    DataTypeId inputTypeId = inputArg->GetTypeId();
    if (inputTypeId == OMNI_ARRAY) {
        ProcessArraySize(inputArg, resultVec, rowSize, legacySizeOfNull);
    } else if (inputTypeId == OMNI_MAP) {
        ProcessMapSize(inputArg, resultVec, rowSize, legacySizeOfNull);
    } else {
        delete inputArg;
        if (boolArg != nullptr) {
            delete boolArg;
        }
        OMNI_THROW("SizeFunction Error:", "Unsupported input type: " + std::to_string(inputTypeId));
    }

    delete inputArg;
    if (boolArg != nullptr) {
        delete boolArg;
    }
}

void SizeFunction::ProcessArraySize(BaseVector *arrayArg, Vector<int32_t> *resultVec, int32_t rowSize,
    bool legacySizeOfNull) const
{
    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (!arrayVec) {
        OMNI_THROW("SizeFunction Error:", "Input is not an ArrayVector");
    }

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            if (legacySizeOfNull) {
                resultVec->SetValue(row, -1);
            } else {
                resultVec->SetNull(row);
            }
        } else {
            int64_t arraySize = arrayVec->GetSize(row);
            if (arraySize > std::numeric_limits<int32_t>::max()) {
                OMNI_THROW("SizeFunction Error:",
                    "Array size " + std::to_string(arraySize) + " exceeds int32_t range");
            }
            resultVec->SetValue(row, static_cast<int32_t>(arraySize));
        }
    }
}

void SizeFunction::ProcessMapSize(BaseVector *mapArg, Vector<int32_t> *resultVec, int32_t rowSize,
    bool legacySizeOfNull) const
{
    MapVectorReader mapReader(mapArg);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (mapReader.containsNull(row)) {
            if (legacySizeOfNull) {
                resultVec->SetValue(row, -1);
            } else {
                resultVec->SetNull(row);
            }
        } else {
            int64_t mapSize = mapReader.GetSize(row);
            if (mapSize > std::numeric_limits<int32_t>::max()) {
                OMNI_THROW("SizeFunction Error:",
                    "Map size " + std::to_string(mapSize) + " exceeds int32_t range");
            }
            resultVec->SetValue(row, static_cast<int32_t>(mapSize));
        }
    }
}

} // namespace omniruntime::vectorization
