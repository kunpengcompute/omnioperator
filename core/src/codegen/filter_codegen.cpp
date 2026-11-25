/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description:filter code generation methods
 */
#include "filter_codegen.h"

namespace omniruntime::codegen {
using namespace llvm;
using namespace orc;
using namespace omniruntime::expressions;

namespace {
const int ARGS_ARRAY_INDEX = 0;
const int NUM_ROWS_INDEX = 1;
const int RESULTS_INDEX = 2;
const int BITMAP_INDEX = 3;
const int OFFSETS_INDEX = 4;
const int EXECUTION_CONTEXT_IDX = 5;
const int DICTIONARY_VECTORS_IDX = 6;
} // namespace

intptr_t FilterCodeGen::GetFunction(const DataTypes &inputDataTypes)
{
    llvm::Function *func = CreateFunction(inputDataTypes);
    if (func == nullptr) {
        return 0;
    }
    return CreateWrapper();
}

intptr_t FilterCodeGen::CreateWrapper()
{
    // The args indicates the type of the function parameter list.
    std::vector<Type *> args{
        llvmTypes->I64PtrType(), // data address array
        llvmTypes->I32Type(),    // the num of rows
        llvmTypes->I32PtrType(), // output array
        llvmTypes->I64PtrType(), // bitmap address array
        llvmTypes->I64PtrType(), // offset address array
        llvmTypes->I64Type(),    // execution content address
        llvmTypes->I64PtrType()  // dictionary address array
    };

    FunctionType *funcSignature = FunctionType::get(llvmTypes->I32Type(), args, false);
    llvm::Function *funcDecl =
        llvm::Function::Create(funcSignature, llvm::Function::ExternalLinkage, "WRAPPER_FUNC", modulePtr);
    BasicBlock *preLoop = BasicBlock::Create(*context, "PRE_LOOP", funcDecl);
    BasicBlock *loopBody = BasicBlock::Create(*context, "LOOP_BODY", funcDecl);
    BasicBlock *filterPassed = BasicBlock::Create(*context, "FILTER_PASSED", funcDecl);
    BasicBlock *incrementCounter = BasicBlock::Create(*context, "INCREMENT_COUNTER", funcDecl);
    BasicBlock *endBlock = BasicBlock::Create(*context, "END_BLOCK", funcDecl);
    // preprocessing
    Argument *data = funcDecl->getArg(ARGS_ARRAY_INDEX);
    data->setName("ARGS_ARRAY");

    Argument *numRows = funcDecl->getArg(NUM_ROWS_INDEX);
    numRows->setName("NUM_ROWS");

    Argument *resultsArray = funcDecl->getArg(RESULTS_INDEX);
    resultsArray->setName("RESULTS");

    Argument *bitmap = funcDecl->getArg(BITMAP_INDEX);
    bitmap->setName("BITMAP");

    Argument *offsets = funcDecl->getArg(OFFSETS_INDEX);
    offsets->setName("OFFSETS");

    Argument *executionContext = funcDecl->getArg(EXECUTION_CONTEXT_IDX);
    executionContext->setName("EXECUTION_CONTEXT_ADDRESS");

    Argument *dictionaryVectors = funcDecl->getArg(DICTIONARY_VECTORS_IDX);
    dictionaryVectors->setName("DICTIONARY_VECTORS");

    RecordMainFunction(funcDecl);

    Value *zero = llvmTypes->CreateConstantInt(0);
    Value *one = llvmTypes->CreateConstantInt(1);

    // filterFuncArgs contains the values of the arguments to the filter function
    std::vector<Value *> filterFuncArgs;
    int32_t argsSize = exprFunc->GetArgumentCount() + exprFunc->GetInputColumnCount() * 4;
    filterFuncArgs.reserve(argsSize);

    // pre loop body
    builder->SetInsertPoint(preLoop);
    // Pointer to the current row index to be processed.
    AllocaInst *indexStore = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "INDEX_COUNTER");
    // Initialize row index to 0.
    builder->CreateStore(zero, indexStore);
    // Value of the current row index to be processed.
    Value *curIndexVal;
    // Temp value for next row index.
    Value *nextIndexVal;
    // Pointer to the index of the selected positions array to be filled next.
    AllocaInst *selectedIndexStore = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "SELECTED_INDEX_PTR");
    // Initialize index to 0.
    builder->CreateStore(zero, selectedIndexStore);
    // Value of the selected positions index.
    Value *selectedIndexVal;
    // Address of the selected index for writing.
    Value *selectedAddress;
    // Temp value for next selected index.
    Value *nextSelectedIndexVal;

    // Create a int pointer to store data length
    AllocaInst *lengthAllocaInst = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "DATA_LENGTH");
    auto isNullPtr = builder->CreateAlloca(llvmTypes->I1Type(), nullptr, "IS_NULL_PTR");

    auto columnArgs = exprFunc->ToColumnArgs(data);
    auto dicArgs = exprFunc->ToDicArgs(dictionaryVectors);
    auto nullArgs = exprFunc->ToNullArgs(bitmap);
    auto offsetArgs = exprFunc->ToOffsetArgs(offsets);

    builder->CreateBr(loopBody);
    // loop body
    builder->SetInsertPoint(loopBody);
    // Get the value of the current row index to process.
    curIndexVal = builder->CreateLoad(llvmTypes->I32Type(), indexStore, "CUR_INDEX");

    // initialize data_length to 0;
    builder->CreateStore(llvmTypes->CreateConstantInt(0), lengthAllocaInst);

    // initialize isNullPtr to false
    builder->CreateStore(llvmTypes->CreateConstantBool(false), isNullPtr);

    filterFuncArgs.push_back(curIndexVal);
    filterFuncArgs.push_back(lengthAllocaInst);
    filterFuncArgs.push_back(executionContext);
    filterFuncArgs.push_back(isNullPtr);

    filterFuncArgs.insert(filterFuncArgs.end(), columnArgs.begin(), columnArgs.end());
    filterFuncArgs.insert(filterFuncArgs.end(), dicArgs.begin(), dicArgs.end());
    filterFuncArgs.insert(filterFuncArgs.end(), nullArgs.begin(), nullArgs.end());
    filterFuncArgs.insert(filterFuncArgs.end(), offsetArgs.begin(), offsetArgs.end());

    // Get the boolean response for this row from the filter function.
    CallInst *ret = builder->CreateCall(func, filterFuncArgs, "ROW_EVAL");

    ret = static_cast<CallInst *>(
        builder->CreateAnd(builder->CreateNot(builder->CreateLoad(llvmTypes->I1Type(), isNullPtr)), ret));
    // If true, add row index to selected array, otherwise, process next row.
    builder->CreateCondBr(ret, filterPassed, incrementCounter);

    // Add row index to results array
    builder->SetInsertPoint(filterPassed);
    // Get value of selected index.
    selectedIndexVal = builder->CreateLoad(llvmTypes->I32Type(), selectedIndexStore, "SELECTED_INDEX");
    // Get address of selected index.
    selectedAddress = builder->CreateGEP(llvmTypes->I32Type(), resultsArray, selectedIndexVal, "SELECTED_ADDRESS");
    // Set the selected value to the current row index.
    builder->CreateStore(curIndexVal, selectedAddress);
    // Increment the selected index.
    nextSelectedIndexVal = builder->CreateAdd(selectedIndexVal, one, "NEXT_SELECTED_INDEX");
    builder->CreateStore(nextSelectedIndexVal, selectedIndexStore);

    // Increment counter and process next row.
    builder->CreateBr(incrementCounter);
    // Increment loop counter
    builder->SetInsertPoint(incrementCounter);
    // Increment counter.
    nextIndexVal = builder->CreateAdd(curIndexVal, one, "NEXT_INDEX");
    builder->CreateStore(nextIndexVal, indexStore);
    // If there are rows remaining, repeat, otherwise, exit.
    Value *cond = builder->CreateICmpSLT(nextIndexVal, numRows, "END_LOOP_COND");
    builder->CreateCondBr(cond, loopBody, endBlock);

    // Return results
    builder->SetInsertPoint(endBlock);
    nextSelectedIndexVal = builder->CreateLoad(llvmTypes->I32Type(), selectedIndexStore);
    builder->CreateRet(nextSelectedIndexVal);
    OptimizeFunctionsAndModule();

    return Compile();
}
} // namespace omniruntime::codegen
