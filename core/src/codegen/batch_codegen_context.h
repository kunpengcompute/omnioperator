/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: batch codegen context
 */

#ifndef OMNI_RUNTIME_BATCH_CODEGEN_CONTEXT_H
#define OMNI_RUNTIME_BATCH_CODEGEN_CONTEXT_H

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Value.h"

namespace omniruntime::codegen {
class BatchCodegenContext {
public:
    explicit BatchCodegenContext()
        : data(nullptr),
          nullBitmap(nullptr),
          offsets(nullptr),
          rowCnt(nullptr),
          rowIdxArray(nullptr),
          executionContext(nullptr),
          dictionaryVectors(nullptr),
          subqueryRowCount(nullptr)
    {}

    explicit BatchCodegenContext(llvm::Value *data, llvm::Value *nullBitmap, llvm::Value *offsets, llvm::Value *rowIdx,
        llvm::Value *rowIdxArray, llvm::Value *executionContext, llvm::Value *dictionaryVectors)
        : data(data),
          nullBitmap(nullBitmap),
          offsets(offsets),
          rowCnt(rowIdx),
          rowIdxArray(rowIdxArray),
          executionContext(executionContext),
          dictionaryVectors(dictionaryVectors),
          subqueryRowCount(nullptr)
    {}

    explicit BatchCodegenContext(llvm::Value *data, llvm::Value *nullBitmap, llvm::Value *offsets, llvm::Value *rowIdx,
        llvm::Value *rowIdxArray, llvm::Value *executionContext, llvm::Value *dictionaryVectors,
        llvm::Value *subqueryRowCnt)
        : data(data),
          nullBitmap(nullBitmap),
          offsets(offsets),
          rowCnt(rowIdx),
          rowIdxArray(rowIdxArray),
          executionContext(executionContext),
          dictionaryVectors(dictionaryVectors),
          subqueryRowCount(subqueryRowCnt)
    {}

    ~BatchCodegenContext() = default;

    friend class BatchExpressionCodeGen;

    friend class CodegenBase;

private:
    llvm::Value *data;
    llvm::Value *nullBitmap;
    llvm::Value *offsets;
    llvm::Value *rowCnt;
    llvm::Value *rowIdxArray;
    llvm::Value *executionContext;
    llvm::Value *dictionaryVectors;
    llvm::Value *subqueryRowCount;  // Row count of subquery result for IN (subquery) expressions
};
}
#endif // OMNI_RUNTIME_BATCH_CODEGEN_CONTEXT_H
