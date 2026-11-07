/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once

#include <memory>
#include <stack>
#include "vector/vector.h"
#include "operator/execution_context.h"
#include "vector/vector_helper.h"
#include "codegen/func_signature.h"
#include "type/data_type.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;

namespace omniruntime::vectorization {
using FunctionSignaturePtr = std::shared_ptr<codegen::FunctionSignature>;

struct Hash {
    std::size_t operator ()(const FunctionSignaturePtr &signature) const
    {
        return signature->HashCode();
    }
};

struct Equals {
    bool operator ()(FunctionSignaturePtr s1, const FunctionSignaturePtr s2) const
    {
        return *s1 == *s2;
    }
};

class VectorFunction;

class SimpleFunctionAdapterFactory {
public:
    virtual std::unique_ptr<VectorFunction> createVectorFunction(const std::vector<type::DataTypeId> &inputTypes,
        const config::QueryConfig &config, const std::vector<vec::BaseVector *> &constantInputs) const = 0;

    virtual ~SimpleFunctionAdapterFactory() = default;
};

using AdapterFunction = SimpleFunctionAdapterFactory;
using FunctionFactory = std::function<std::unique_ptr<AdapterFunction>()>;

using FunctionMap = std::unordered_map<FunctionSignaturePtr, std::shared_ptr<VectorFunction>, Hash, Equals>;

using VectorFunctionFactory = std::function<std::shared_ptr<VectorFunction>(const std::string &name,
    const std::vector<type::DataTypeId> &inputArgs, const config::QueryConfig &config)>;

using FunctionFactoryMap = std::unordered_map<FunctionSignaturePtr, VectorFunctionFactory, Hash, Equals>;

using SimpleFunctionFactory = std::function<std::shared_ptr<VectorFunction>(
    const std::vector<type::DataTypeId> &inputArgs, const config::QueryConfig &config,
    const std::vector<vec::BaseVector *> &constantInputs)>;

using SimpleFunctionFactoryMap = std::unordered_map<FunctionSignaturePtr, FunctionFactory, Hash, Equals>;

using VectorPtr = vec::BaseVector*;

// Owns a BaseVector popped from the Apply args stack. Captureless deleter so the
// unique_ptr type is the same in every TU. unique_ptr skips the deleter for nullptr.
using VectorArgGuard = std::unique_ptr<vec::BaseVector, void (*)(vec::BaseVector *)>;

inline VectorArgGuard MakeVectorArgGuard(vec::BaseVector *p)
{
    return VectorArgGuard(p, [](vec::BaseVector *v) { delete v; });
}

class VectorFunction {
public:
    virtual ~VectorFunction() = default;

    /// Returns true if (1) supports evaluation on all constant inputs of size >
    /// 1; (2) returns flat or constant result when inputs are all flat, all
    /// constant or a mix of flat and constant; (3) guarantees that if all inputs
    /// are not null, the result is also not null.
    virtual bool SupportsFlatNoNullsFastPath() const
    {
        return false;
    }

    virtual void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const = 0;

    /// Registers stateless VectorFunction. The same instance will be used for all
    /// expressions.
    /// Returns true iff an new function is inserted
    static bool RegisterVectorFunction(const std::string &name, const std::vector<type::DataTypeId> &paramsType,
        type::DataTypeId returnType, const std::shared_ptr<VectorFunction> &func);

    static bool RegisterVectorFunctionFactory(const std::string &name, const std::vector<type::DataTypeId> &paramsType,
        type::DataTypeId returnType, const VectorFunctionFactory &factory);

    static bool RegisterVectorFunctionFactory(
        std::vector<std::shared_ptr<codegen::FunctionSignature>> functionSignatures,
        const VectorFunctionFactory &factory);

    static std::shared_ptr<VectorFunction> Find(const FunctionSignaturePtr &signature,
        const config::QueryConfig &config = config::QueryConfig())
    {
        auto it = functionMap_.find(signature);
        if (it != functionMap_.end()) {
            return it->second;
        }
        auto factory = functionFactoryMap_.find(signature);
        if (factory != functionFactoryMap_.end()) {
            return factory->second(signature->GetName(), signature->GetParams(), config);
        }
        auto simpleFactory = simpleFunctionFactoryMap_.find(signature);
        if (simpleFactory != simpleFunctionFactoryMap_.end()) {
            return simpleFactory->second()->createVectorFunction(signature->GetParams(), config, {});
        }
        return nullptr;
    }

    static std::shared_ptr<VectorFunction> Find(const FunctionSignaturePtr &signature,
        const std::vector<vec::BaseVector *> &constantInputs, const config::QueryConfig &config = config::QueryConfig())
    {
        auto simpleFactory = simpleFunctionFactoryMap_.find(signature);
        if (simpleFactory != simpleFunctionFactoryMap_.end()) {
            return simpleFactory->second()->createVectorFunction(signature->GetParams(), config, constantInputs);
        }
        return nullptr;
    }

    static inline FunctionMap functionMap_;
    static inline FunctionFactoryMap functionFactoryMap_;
    static inline SimpleFunctionFactoryMap simpleFunctionFactoryMap_;
};
}
