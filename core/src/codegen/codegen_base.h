/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * Description: Base codegen generator
 */
#ifndef OMNI_RUNTIME_CODEGEN_BASE_H
#define OMNI_RUNTIME_CODEGEN_BASE_H

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include "codegen/llvm_engine.h"
#include "codegen_context.h"
#ifndef EXCLUDE_BATCH_FUNCTIONS
#include "batch_codegen_context.h"
#endif
#include "type/decimal128_utils.h"
#include "util/type_util.h"

namespace omniruntime::codegen {
using CodeGenValuePtr = std::shared_ptr<CodeGenValue>;

// The base class for the code generator
class CodegenBase : public LLVMEngine {
public:
    CodegenBase(std::string name, const omniruntime::expressions::Expr &cpExpr,
        omniruntime::op::OverflowConfig *overflowConfig);

    Value *GetPtrTypeFromInt(omniruntime::type::DataTypeId dataTypeId, Value *elementAddr);

protected:
    void PrintValues(std::string format, const std::vector<Value *> &values);

    std::string DumpCode();

    llvm::Constant *CreateConstantString(std::string s);

    static CodeGenValuePtr CreateInvalidCodeGenValue()
    {
        return std::make_shared<CodeGenValue>(nullptr, nullptr, nullptr);
    }

    std::string funcName;
    const omniruntime::expressions::Expr *expr;
    llvm::Function *func = nullptr;

    int numGlobalValues = 0;
    std::unique_ptr<CodegenContext> codegenContext;
#ifndef EXCLUDE_BATCH_FUNCTIONS
    std::unique_ptr<BatchCodegenContext> batchCodegenContext;
#endif
    CodeGenValuePtr value = nullptr;
    omniruntime::op::OverflowConfig *overflowConfig;
};
}

#endif
