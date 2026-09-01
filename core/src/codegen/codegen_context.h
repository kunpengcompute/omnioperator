/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression code generator
 */
#ifndef OMNI_RUNTIME_CODEGEN_CONTEXT_H
#define OMNI_RUNTIME_CODEGEN_CONTEXT_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Value.h"

namespace omniruntime::codegen {
class CodegenContext {
public:
    explicit CodegenContext()
        : data(nullptr),
          nullBitmap(nullptr),
          offsets(nullptr),
          rowIdx(nullptr),
          executionContext(nullptr),
          dictionaryVectors(nullptr),
          print(nullptr),
          subqueryRowCount(nullptr)
    {}

    explicit CodegenContext(llvm::Value *data, llvm::Value *nullBitmap, llvm::Value *offsets, llvm::Value *rowIdx,
        llvm::Value *executionContext, llvm::Value *dictionaryVectors)
        : data(data),
          nullBitmap(nullBitmap),
          offsets(offsets),
          rowIdx(rowIdx),
          executionContext(executionContext),
          dictionaryVectors(dictionaryVectors),
          print(nullptr),
          subqueryRowCount(nullptr)
    {}

    explicit CodegenContext(llvm::Value *data, llvm::Value *nullBitmap, llvm::Value *offsets, llvm::Value *rowIdx,
        llvm::Value *executionContext, llvm::Value *dictionaryVectors, llvm::Value *subqueryRowCnt)
        : data(data),
          nullBitmap(nullBitmap),
          offsets(offsets),
          rowIdx(rowIdx),
          executionContext(executionContext),
          dictionaryVectors(dictionaryVectors),
          print(nullptr),
          subqueryRowCount(subqueryRowCnt)
    {}

    ~CodegenContext() = default;

    friend class ExpressionCodeGen;

    friend class SimpleFilterCodeGen;

    friend class CodegenBase;

private:
    llvm::Value *data;
    llvm::Value *nullBitmap;
    llvm::Value *offsets;
    llvm::Value *rowIdx;
    llvm::Value *executionContext;
    llvm::Value *dictionaryVectors;
    llvm::FunctionCallee print;
    llvm::Value *subqueryRowCount;  // Row count of subquery result for IN (subquery) expressions
};
}

#endif // OMNI_RUNTIME_CODEGEN_CONTEXT_H
