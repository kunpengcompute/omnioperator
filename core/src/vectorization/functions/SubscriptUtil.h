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
        try {
            switch (inputs[1]->GetTypeId()) {
                case OMNI_ARRAY:
                    result = applyArray(rows, inputs, context, outputType);
                    break;
                case OMNI_MAP:
                    result = applyMap(rows, inputs, context, outputType);
                    break;
                default:
                    OMNI_THROW("SubscriptImpl error:", "Not support type!");
            }
        } catch (...) {
            for (auto input : inputs) {
                if (input != nullptr) {
                    delete input;
                }
            }
            throw;
        }
        for (auto input : inputs) {
            if (input != nullptr) {
                delete input;
            }
        }
    }

    BaseVector *applyArray(const SelectivityVector &rows, std::vector<BaseVector *> &args,
        ExecutionContext *context, const DataTypePtr &outputType) const;

    BaseVector *applyMap(const SelectivityVector &rows, std::vector<BaseVector *> &args,
        ExecutionContext *context, const DataTypePtr &outputType) const;

    template <typename I>
    BaseVector *applyArrayTyped(const SelectivityVector &rows, BaseVector *arrayArg, BaseVector *indexArg,
        ExecutionContext *context, const DataTypePtr &outputType) const;
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

