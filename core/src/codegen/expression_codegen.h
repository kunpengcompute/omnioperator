/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression code generator
 */
#ifndef __EXPRESSION_CODEGEN_H__
#define __EXPRESSION_CODEGEN_H__

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>

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
#include "codegen_context.h"
#include "expression/expressions.h"
#include "expression/parser/parser.h"
#include "expression/expr_printer.h"
#include "util/debug.h"
#include "llvm_types.h"
#include "llvm_engine.h"
#include "codegen_base.h"
#include "vector/vector_batch.h"
#include "expr_function.h"

namespace omniruntime::codegen {
using CodeGenValuePtr = std::shared_ptr<CodeGenValue>;

class ExpressionCodeGen : public ExprVisitor, public CodegenBase {
public:
    /**
     * Method to initialize a ExpressionCodeGen instance
     * @param name ExpressionCodeGen module name
     * @param cpExpr the expression to code generation
     * @param ofConfig config of overflow
     */
    ExpressionCodeGen(std::string name, const Expr &cpExpr, op::OverflowConfig *ofConfig);

    ~ExpressionCodeGen() override;

    /**
     * Method to get function of processing expression
     * @param inputDataTypes is used to provide data type when preload data
     * @return the address of function
     */
    virtual intptr_t GetFunction(const DataTypes &inputDataTypes)
    {
        return 0;
    }

    std::set<int32_t> vectorIndexes{};

protected:
    /**
     * Method to get function of processing expression for a single line
     * @param inputDataTypes is used to provide data type when preload data
     * @return the address of function
     */
    virtual llvm::Function *CreateFunction(const DataTypes &inputDataTypes);

    /**
     * Visitor methods
     * @param e expression to visit
     */
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

    /**
     * Method to get LLVM value ptr of expression
     * @param e expression to visit
     * @return llvm value ptr of expression
     */
    CodeGenValuePtr VisitExpr(const Expr &e);

    void ExtractVectorIndexes();

    std::vector<Value *> GetFunctionArgValues(const FuncExpr &fExpr, Value **isAnyNull, bool &isInvalidExpr);

    bool InitializeCodegenContext(iterator_range<llvm::Function::arg_iterator> args);

    Value *GetDictionaryVectorValue(const omniruntime::type::DataType &dataType, Value *rowIdx,
        Value *dictionaryVectorPtr, AllocaInst *&lengthAllocaInst);

    // Represents the generated expression function
    std::shared_ptr<ExprFunction> exprFunc;

private:
    template <bool isNeedVerifyResult, bool isNeedVerifyVal>
    std::vector<Value *> GetDefaultFunctionArgValues(const FuncExpr &fExpr, Value **isAnyNull, bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataArgs(const omniruntime::expressions::FuncExpr &fExpr, llvm::Value **isAnyNull,
        bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataAndNullArgs(const omniruntime::expressions::FuncExpr &fExpr,
        llvm::Value **isAnyNull, bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataAndNullArgsAndReturnNull(const omniruntime::expressions::FuncExpr &fExpr,
        llvm::Value **isAnyNull, bool &isInvalidExpr);

    std::vector<llvm::Value *> GetDataAndOverflowNullArgs(const omniruntime::expressions::FuncExpr &fExpr,
        llvm::Value **isAnyNull, bool &isInvalidExpr, llvm::Value *overflowNull);

    llvm::Value *CreateHiveUdfArgTypes(const omniruntime::expressions::FuncExpr &fExpr);

    std::vector<llvm::Value *> GetHiveUdfArgValues(const omniruntime::expressions::FuncExpr &fExpr, bool &isInvalid);

    void CallHiveUdfFunction(const omniruntime::expressions::FuncExpr &fExpr);

    void FuncExprOverflowNullHelper(const omniruntime::expressions::FuncExpr &e);

    Value *StringCmp(Value *lhs, Value *lLen, Value *rhs, Value *rLen);

    Value *StringEqual(Value *lhs, Value *lLen, Value *rhs, Value *rLen, Value *isNull);

    void BinaryExprNullHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull, PHINode **leftPhi, PHINode **rightPhi);
    
    llvm::Value *BinaryExprByteHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull, Value *nullFlag = nullptr);

    llvm::Value *BinaryExprShortHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull, Value *nullFlag = nullptr);
        
    llvm::Value *BinaryExprIntHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull, Value *nullFlag = nullptr);

    Value *BinaryExprLongHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull, Value *nullFlag = nullptr);

    void BinaryExprDecimal64Helper(const BinaryExpr *binaryExpr, DecimalValue &left, DecimalValue &right,
        Value *leftIsNull, Value *rightIsNull);

    Value *BinaryExprDoubleHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
        Value *rightIsNull, Value *nullFlag = nullptr);

    Value *BinaryExprFloatHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
                                  Value *rightIsNull, Value *nullFlag = nullptr);

    Value *BinaryExprStringHelper(const BinaryExpr *binaryExpr, Value *leftVal, Value *leftLen, Value *rightVal,
        Value *rightLen, Value *leftIsNull, Value *rightIsNull);

    void BinaryExprDecimal128Helper(const BinaryExpr *binaryExpr, DecimalValue &left, DecimalValue &right,
        Value *leftIsNull, Value *rightIsNull);

    CodeGenValue *LiteralExprConstantHelper(const LiteralExpr &lExpr);

    bool AreInvalidDataTypes(DataTypeId type1, DataTypeId type2);

    void InExprIntegerHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue, Value *&tmpCmpData,
        Value *&tmpCmpNull);

    void InExprDecimal64Helper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue, Value *&tmpCmpData,
        Value *&tmpCmpNull, Type *retType);

    void InExprDoubleHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue, Value *&tmpCmpData,
        Value *&tmpCmpNull);

    void InExprFloatHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue, Value *&tmpCmpData,
        Value *&tmpCmpNull);

    void InExprStringHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue, Value *&tmpCmpData,
        Value *&tmpCmpNull);

    void InExprDecimal128Helper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue, Value *&tmpCmpData,
        Value *&tmpCmpNull, llvm::Type *retType);

    bool VisitBetweenExprHelper(BetweenExpr &bExpr, const std::shared_ptr<CodeGenValue> &val,
        const std::shared_ptr<CodeGenValue> &lowerVal, const std::shared_ptr<CodeGenValue> &upperVal,
        std::pair<Value **, Value **> cmpPair);

    void CoalesceExprDecimalHelper(CodeGenValue &v1, CodeGenValue &v2, BasicBlock &isNotNullBlock,
        BasicBlock &isNullBlock, PHINode &pn, PHINode &pnNull);

    Value *PushAndGetNullFlag(const FuncExpr &fExpr, std::vector<llvm::Value *> &argVals, Value *nullFlag,
        bool needAdd);

    Value *LoadNullFlag(const FuncExpr &fExpr, Value *nullFlag);
};
}

#endif