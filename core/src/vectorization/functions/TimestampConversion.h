/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Timestamp conversion functions for vectorized execution
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/type_util.h"
#include <vector>
#include <cmath>
#include <limits>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class TimestampMicrosFunction : public VectorFunction {
    public:
        explicit TimestampMicrosFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override;

    private:
        template<typename T>
        void ProcessIntegral(omniruntime::mem::OmniTaskMemoryPool *pool, BaseVector *input, BaseVector *&result) const;
    };

    class TimestampMillisFunction : public VectorFunction {
    public:
        explicit TimestampMillisFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override;

    private:
        template<typename T>
        void ProcessIntegral(omniruntime::mem::OmniTaskMemoryPool *pool, BaseVector *input, BaseVector *&result) const;
    };

    class TimestampSecondsFunction : public VectorFunction {
    public:
        explicit TimestampSecondsFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override;

    private:
        template<typename T>
        void ProcessIntegral(omniruntime::mem::OmniTaskMemoryPool *pool, BaseVector *input, BaseVector *&result) const;

        template<typename T>
        void ProcessFloatingPoint(omniruntime::mem::OmniTaskMemoryPool *pool, BaseVector *input, BaseVector *&result) const;
    };

    // Registration functions
    void RegisterTimestampMicrosFunction(const std::string &name);
    void RegisterTimestampMillisFunction(const std::string &name);
    void RegisterTimestampSecondsFunction(const std::string &name);
}
