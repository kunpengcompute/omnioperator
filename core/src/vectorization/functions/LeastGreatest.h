/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Least and Greatest functions for vectorized comparison expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/Comparisons.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"
#include <vector>
#include <string_view>
#include <iostream>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

/// Compare mode enum
enum class CompareMode {
    LEAST,
    GREATEST
};

/// Templated LeastGreatest function
/// least/greatest(arg1, arg2, ..., argN) -> T
/// Returns the least/greatest value among all input arguments.
/// NULL handling depends on propagateNull:
///   false (Spark SQL, "Greatest"/"Least"): NULL arguments are ignored; the result is NULL
///          only when every argument is NULL.
///   true  (Flink SQL, "flink_greatest"/"flink_least"): a single NULL argument makes the
///          result NULL, matching ScalarOperatorGens.generateGreatestLeast.
/// For floating point types, NaN is treated as greater than any other value (Spark SQL semantics).
template<CompareMode Mode>
class LeastGreatestFunction : public VectorFunction {
public:
    explicit LeastGreatestFunction(size_t numArgs = 2, bool propagateNull = false)
        : numArgs_(numArgs), propagateNull_(propagateNull)
    {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    /// Arity of the resolved signature: Apply() must pop exactly this many vectors off the
    /// shared argument stack, otherwise the enclosing expression sees stale operands.
    size_t numArgs_;
    bool propagateNull_;

    /// Returns true when row must be NULL because an argument is NULL and NULLs propagate.
    bool HasNullArg(const std::vector<BaseVector *> &argVectors, int32_t row) const;

    template<typename T>
    T GetValueFromVector(BaseVector *vec, int32_t row) const;
    
    std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row) const;
    
    template<typename T>
    void SetValueToVector(BaseVector *vec, int32_t row, const T &value) const;
    
    void SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const;
    
    void DispatchCompare(const std::vector<BaseVector *> &argVectors, const DataTypePtr &outputType,
        BaseVector *&result, ExecutionContext *context) const;
    
    // Template implementations for different types
    template<typename T>
    void CompareNumeric(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
        const DataTypePtr &outputType, ExecutionContext *context) const;
    
    void CompareString(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
        const DataTypePtr &outputType, ExecutionContext *context) const;
    
    void CompareBoolean(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
        const DataTypePtr &outputType, ExecutionContext *context) const;
};

using GreatestFunction = LeastGreatestFunction<CompareMode::GREATEST>;
using LeastFunction = LeastGreatestFunction<CompareMode::LEAST>;

// Factory function declarations
std::shared_ptr<VectorFunction> makeGreatest(const std::string &name, 
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &);

std::shared_ptr<VectorFunction> makeLeast(const std::string &name, 
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &);

// Flink variants: identical comparison logic, but NULL arguments propagate to a NULL result.
std::shared_ptr<VectorFunction> makeFlinkGreatest(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &);

std::shared_ptr<VectorFunction> makeFlinkLeast(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &);

// Signatures for least and greatest functions
std::vector<std::shared_ptr<codegen::FunctionSignature>> GreatestSignatures();
std::vector<std::shared_ptr<codegen::FunctionSignature>> LeastSignatures();
std::vector<std::shared_ptr<codegen::FunctionSignature>> FlinkGreatestSignatures();
std::vector<std::shared_ptr<codegen::FunctionSignature>> FlinkLeastSignatures();

}
