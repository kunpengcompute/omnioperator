/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: batch expression codegen
 */

#ifndef OMNI_RUNTIME_BATCH_EXPRESSION_CODEGEN_H
#define OMNI_RUNTIME_BATCH_EXPRESSION_CODEGEN_H

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Error.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

#include "codegen_value.h"
#include "batch_codegen_context.h"
#include "expression/expressions.h"
#include "expression/parser/parser.h"
#include "expression/expr_printer.h"
#include "util/debug.h"
#include "llvm_types.h"
#include "llvm_engine.h"
#include "operator/config/operator_config.h"
#include "type/data_type.h"
#include "vector/vector_batch.h"
#include "codegen_base.h"

namespace omniruntime::codegen {
using namespace llvm;
using namespace llvm::orc;
using namespace omniruntime;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using CodeGenValuePtr = std::shared_ptr<CodeGenValue>;

class BatchExpressionCodeGen : public ExprVisitor, public CodegenBase {
public:
    BatchExpressionCodeGen(std::string name, const omniruntime::expressions::Expr &cpExpr,
        op::OverflowConfig *ofConfig);

    ~BatchExpressionCodeGen() override
    {
        if (rt) {
            eoe(rt->remove());
        }
    }

    virtual intptr_t GetFunction() = 0;

    void Visit(const LiteralExpr &e) override;

    void Visit(const FieldExpr &e) override;

    void Visit(const UnaryExpr &e) override;

    void Visit(const BinaryExpr &e) override;

    void Visit(const InExpr &e) override;

    void Visit(const InSubqueryExpr &e) override;

    void Visit(const BetweenExpr &e) override;

    void Visit(const IfExpr &e) override;

    void Visit(const CoalesceExpr &e) override;

    void Visit(const IsNullExpr &e) override;

    void Visit(const FuncExpr &e) override;

    void Visit(const SwitchExpr &e) override;

    void Visit(const ParamRefExpr &e) override;

    void Visit(const LambdaExpr &e) override;

    CodeGenValuePtr VisitExpr(const Expr &e);

    std::vector<llvm::Value *> GetFunctionArgValues(const FuncExpr &fExpr, llvm::AllocaInst *isAnyNull,
        bool &isInvalidExpr);

protected:
    AllocaInst *GetResultArray(DataTypeId dataTypeId, Value *rowCnt);

    virtual llvm::Function *CreateBatchFunction();

private:
    bool InitializeBatchCodegenContext(llvm::iterator_range<llvm::Function::arg_iterator> args);

    Value *GetDictionaryVectorValue(const DataType &dataType, llvm::Value *rowIdxArray, Value *rowCnt,
        Value *dictionaryVectorPtr, AllocaInst *lengthArrayPtr);

    Value *GetVectorValue(const DataType &dataType, Value *rowIdxArray, Value *rowCnt, Value *dataVectorPtr,
        Value *offsetArray, Value *lengthArrayPtr);

    CodeGenValue *BatchLiteralExprConstantHelper(const LiteralExpr &lExpr);

    void BatchBinaryExprIntLongHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull);

    void BatchBinaryExprDoubleHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull);

    void BatchBinaryExprFloatHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull);

    void BatchBinaryExprStringHelper(const BinaryExpr *binaryExpr, Value *left, Value *leftLen, Value *right,
        Value *rightLen, Value *leftIsNull, Value *rightIsNull);

    void BatchBinaryExprDecimalHelper(const BinaryExpr *binaryExpr, DecimalValue &left, DecimalValue &right,
        Value *leftIsNull, Value *rightIsNull);

    void BatchVisitBetweenExprHelper(BetweenExpr &bExpr, const std::shared_ptr<CodeGenValue> &val,
        const std::shared_ptr<CodeGenValue> &lowerVal, const std::shared_ptr<CodeGenValue> &upperVal,
        std::pair<llvm::AllocaInst **, AllocaInst **> cmpPair);

    template <bool isNeedVerifyResult, bool isNeedVerifyVal>
    std::vector<llvm::Value *> GetDefaultFunctionArgValues(const FuncExpr &fExpr, AllocaInst *isAnyNull,
        bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataArgs(const FuncExpr &fExpr, AllocaInst *isAnyNull, bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataAndNullArgs(const FuncExpr &fExpr, AllocaInst *isAnyNull, bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataAndNullArgsAndReturnNull(const FuncExpr &fExpr, AllocaInst *isAnyNull,
        bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataAndOverflowNullArgs(const FuncExpr &fExpr, AllocaInst *isAnyNull,
        bool &isInvalidExpr, AllocaInst *overflowNull);

    void FuncExprOverflowNullHelper(const FuncExpr &e);

    Value *ArenaAlloc(Value *sizeInBytes);

    Value *GetTypeSize(DataTypeId dataTypeId);

    std::vector<Value *> GetHiveUdfArgValues(const FuncExpr &fExpr, bool &isInvalidExpr);

    llvm::Value *CreateHiveUdfArgTypes(const FuncExpr &fExpr);

    void CallHiveUdfFunction(const FuncExpr &fExpr);

    Value *PushAndGetNullFlagArray(const FuncExpr &fExpr, std::vector<llvm::Value *> &argVals, Value *nullFlagArray,
        bool needAdd);
};
}
#endif // OMNI_RUNTIME_BATCH_EXPRESSION_CODEGEN_H
