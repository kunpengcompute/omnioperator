/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/SelectivityVector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// Generic subscript/element_at implementation for both array and map data
/// types.
///
/// Provides four template parameters to configure the behavior:
/// - allowNegativeIndices: if allowed, negative indices accesses elements
/// from
///   last to the first; otherwise, throws.
/// - nullOnNegativeIndices: returns NULL for negative indices instead of the
///   behavior described above.
/// - allowOutOfBound: if allowed, returns NULL for out of bound accesses; if
///   false, throws an exception.
/// - indexStartsAtOne: whether indices start at zero or one.
class SubscriptImpl : public VectorFunction {
public:
    explicit SubscriptImpl() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        std::vector<BaseVector *> inputs;
        inputs.push_back(args.top());
        args.pop();
        inputs.push_back(args.top());
        args.pop();

        auto size = inputs[1]->GetSize();
        auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputs[1]));
        auto rows = SelectivityVector(size);
        rows.setFromBitsNegate(nullBits, size);
        switch (inputs[1]->GetTypeId()) {
            case OMNI_ARRAY:
                result = applyArray(rows, inputs, context);
                break;
            case OMNI_MAP:
                result = applyMap(rows, inputs, context);
                break;
            default: OMNI_THROW("SubscriptImpl error:", "Not support type!");
        }
        for (auto input : inputs) {
            if (input != nullptr) {
                delete input;
            }
        }
    }

    BaseVector *applyArray(const SelectivityVector &rows, std::vector<BaseVector *> &args,
        ExecutionContext *context) const
    {
        auto arrayArg = args[1];
        auto indexArg = args[0];

        switch (indexArg->GetTypeId()) {
            case OMNI_INT:
                return applyArrayTyped<int32_t>(rows, arrayArg, indexArg, context);
            case OMNI_LONG:
                return applyArrayTyped<int64_t>(rows, arrayArg, indexArg, context);
            default: OMNI_THROW("Unsupported type for element_at index {}", std::to_string(indexArg->GetTypeId()));
        }
    }

    BaseVector *applyMap(const SelectivityVector &rows, std::vector<BaseVector *> &args,
        ExecutionContext *context) const;

    template <typename I>
    BaseVector *applyArrayTyped(const SelectivityVector &rows, BaseVector *arrayArg, BaseVector *indexArg,
        ExecutionContext *context) const
    {
        auto rowSize = arrayArg->GetSize();
        int32_t dicIndex[rowSize];
        memset(dicIndex, -1, sizeof(dicIndex));
        auto arrayVector = dynamic_cast<ArrayVector *>(arrayArg);

        auto offset = arrayVector->GetOffsets();
        rows.applyToSelected([&](auto row) {
            I index = VectorHelper::GetValueFromVector<I>(indexArg, row);
            if (index == 0) {
                OMNI_THROW("Runtime Error:", "The index 0 is invalid!. An index shall be either < 0 or > 0 (the first element has index 1)");
            }
            auto rowArraySize = offset[row+1] - offset[row];
            if (rowArraySize <= 0) {
                dicIndex[row] = -1;
            } else {
                int32_t realIndex;
                if (index > 0) {
                    realIndex = index - 1;
                } else if (index < 0) {
                    realIndex = rowArraySize + index;
                }
                dicIndex[row] = rowArraySize <= realIndex ? -1 : offset[row] + realIndex;
            }
        });

        auto elementVector = arrayVector->GetElementVector();
        auto result = VectorHelper::CreateDictionaryVector(dicIndex, rowSize, elementVector.get(),
            elementVector->GetTypeId());
        for (auto i = 0; i < rowSize; i++) {
            if (dicIndex[i] == -1) {
                result->SetNull(i);
            }
        }
        return result;
    }
};

/// Spark get(array, index) / GetArrayItem(failOnError = false).
/// 0-based. NULL array, NULL index, negative index and out-of-bounds index return NULL.
/// Integral index is truncated to int32, matching Spark's `(int) index`.
class GetArrayItemFunction : public VectorFunction {
public:
    explicit GetArrayItemFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;
};
}

