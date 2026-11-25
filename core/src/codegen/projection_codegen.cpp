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
 * Description: project  codegen
 */
#include "projection_codegen.h"

namespace omniruntime {
namespace codegen {
using namespace llvm;
using namespace orc;
using namespace omniruntime::expressions;
using namespace omniruntime::type;

namespace {
const int INPUT_TABLE_INDEX = 0;
const int NUM_ROWS_INDEX = 1;
const int OUTPUT_ADDRESS_INDEX = 2;
const int SELECTED = 3;
const int NUM_SELECTED = 4;
const int BITMAP = 5;
const int OFFSETS_INDEX = 6;
const int NEW_NULL_VALUES_INDEX = 7;
const int OUTPUT_OFFSETS_INDEX = 8;
const int EXECUTION_CONTEXT_IDX = 9;
const int DICTIONARY_VECTORS_IDX = 10;
}

intptr_t ProjectionCodeGen::GetFunction(const DataTypes &inputDataTypes)
{
    llvm::Function *func = CreateFunction(inputDataTypes);
    if (func == nullptr) {
        return 0;
    }
    return CreateWrapper();
}

intptr_t ProjectionCodeGen::CreateWrapper()
{
    // The args indicates the type of the function parameter list.
    std::vector<Type *> args {
        llvmTypes->I64PtrType(), // data address array
        llvmTypes->I32Type(),    // the num of rows
        llvmTypes->I64Type(),    // output array address
        llvmTypes->I32PtrType(), // selected array
        llvmTypes->I32Type(),    // the num of selected rows
        llvmTypes->I64PtrType(), // bitmap address array
        llvmTypes->I64PtrType(), // offset address array
        llvmTypes->I32PtrType(),  // output null values array
        llvmTypes->I32PtrType(), // output offset array
        llvmTypes->I64Type(),    // execution content address
        llvmTypes->I64PtrType()  // dictionary address array
    };

    FunctionType *funcSignature = FunctionType::get(llvmTypes->I32Type(), args, false);
    llvm::Function *funcDecl =
        llvm::Function::Create(funcSignature, llvm::Function::ExternalLinkage, "WRAPPER_FUNC", modulePtr);
    BasicBlock *preLoop = BasicBlock::Create(*context, "PRE_LOOP", funcDecl);
    BasicBlock *loopBody = BasicBlock::Create(*context, "LOOP_BODY", funcDecl);
    BasicBlock *addToOutput = BasicBlock::Create(*context, "ADD_OUTPUT", funcDecl);
    BasicBlock *incrementCounter = BasicBlock::Create(*context, "INCREMENT_COUNTER", funcDecl);
    BasicBlock *endBlock = BasicBlock::Create(*context, "END_BLOCK", funcDecl);
    // preprocessing
    Argument *input = funcDecl->getArg(INPUT_TABLE_INDEX);
    input->setName("INPUT_TABLE");
    Argument *numRows = funcDecl->getArg(NUM_ROWS_INDEX);
    numRows->setName("NUM_ROWS");
    Argument *outputAddress = funcDecl->getArg(OUTPUT_ADDRESS_INDEX);
    outputAddress->setName("OUTPUT_ADDRESS");

    RecordMainFunction(funcDecl);

    // Only use these values if filter enabled
    Argument *selected = nullptr;
    Argument *numSelected = nullptr;
    if (filter) {
        selected = funcDecl->getArg(SELECTED);
        selected->setName("SELECTED_ARRAY");
        numSelected = funcDecl->getArg(NUM_SELECTED);
        numSelected->setName("NUM_SELECTED");
    }

    Argument *bitmap = funcDecl->getArg(BITMAP);
    bitmap->setName("BITMAP");

    Argument *offsets = funcDecl->getArg(OFFSETS_INDEX);
    offsets->setName("OFFSETS");

    Argument *nullValuesAddress = funcDecl->getArg(NEW_NULL_VALUES_INDEX);
    nullValuesAddress->setName("NULL_VALUES_ADDRESS");

    Argument *outputOffsetsAddress = funcDecl->getArg(OUTPUT_OFFSETS_INDEX);
    outputOffsetsAddress->setName("OUTPUT_OFFSETS_ADDRESS");

    Argument *executionContext = funcDecl->getArg(EXECUTION_CONTEXT_IDX);
    executionContext->setName("EXECUTION_CONTEXT_ADDRESS");

    Argument *dictionaryVectors = funcDecl->getArg(DICTIONARY_VECTORS_IDX);
    dictionaryVectors->setName("DICTIONARY_VECTORS");

    Value *zero = llvmTypes->CreateConstantInt(0);
    Value *one = llvmTypes->CreateConstantInt(1);

    // pre loop body
    builder->SetInsertPoint(preLoop);
    // i32* ptrToCounter,Pointer to counter
    AllocaInst *indexStore = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "INDEX_COUNTER");
    // Initialize row index to 0.
    builder->CreateStore(zero, indexStore);
    AllocaInst *offsetStore = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "CURRENT_OFFSET");
    // Initialize offset to 0.
    builder->CreateStore(zero, offsetStore);
    // i32 counter, Counter variable value.
    Value *curIndexVal;
    // i32 rowIndex,Index of row to be processed.
    Value *rowIndexVal;
    // i32 nextCounterValue,Temp value for next row index.
    Value *nextIndexVal;

    // i64 selectedAddress,Only use if filter enabled
    Value *selectedAddress;

    // set bits null func
    FunctionSignature setBitNullFuncSignature = FunctionSignature("WrapSetBitNull", { OMNI_INT }, OMNI_BOOLEAN);
    llvm::Function *setBitNullFunc =
        modulePtr->getFunction(FunctionRegistry::LookupFunction(&setBitNullFuncSignature)->GetId());

    // Type of output column
    llvm::Function *varcharVectorFunc = nullptr;
    if (expr->GetReturnTypeId() == OMNI_CHAR || expr->GetReturnTypeId() == OMNI_VARCHAR) {
        std::vector<DataTypeId> paramTypes = { OMNI_LONG, OMNI_INT, OMNI_VARCHAR };
        FunctionSignature varcharVectorFuncSignature = FunctionSignature("WrapVarcharVector", paramTypes, OMNI_INT);
        varcharVectorFunc =
            modulePtr->getFunction(FunctionRegistry::LookupFunction(&varcharVectorFuncSignature)->GetId());
    }
    Type *outPtrType = llvmTypes->ToPointerType(expr->GetReturnTypeId());
    if (outPtrType == nullptr) {
        return 0;
    }
    Value *outColPtr = builder->CreateIntToPtr(outputAddress, outPtrType);
    // Create a integer pointer to store output length value
    AllocaInst *outputLenPtr = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "OUTPUT_LENGTH");
    auto isNullPtr = builder->CreateAlloca(llvmTypes->I1Type(), nullptr, "IS_NULL");

    auto columnArgs = exprFunc->ToColumnArgs(input);
    auto dicArgs = exprFunc->ToDicArgs(dictionaryVectors);
    auto nullArgs = exprFunc->ToNullArgs(bitmap);
    auto offsetArgs = exprFunc->ToOffsetArgs(offsets);

    builder->CreateBr(loopBody);
    // loop body
    builder->SetInsertPoint(loopBody);
    // i32 counter = *ptrToCounter, Get the value of the current row index to process.
    curIndexVal = builder->CreateLoad(llvmTypes->I32Type(), indexStore, "CUR_INDEX");
    if (filter) {
        // i32* selectedAddress = gep i32* selected, i32 counter, Get address of selected index.
        selectedAddress = builder->CreateGEP(llvmTypes->I32Type(), selected, curIndexVal, "SELECTED_ADDRESS");
        // i32 rowIndexVal = *selectedAddress
        rowIndexVal = builder->CreateLoad(llvmTypes->I32Type(), selectedAddress);
    } else {
        // i32 rowIndexVal = counter
        rowIndexVal = curIndexVal;
    }

    builder->CreateStore(llvmTypes->CreateConstantBool(false), isNullPtr);

    // projFuncArgs contains the values of the arguments to the projection function
    std::vector<Value *> projFuncArgs;
    int32_t argsSize = exprFunc->GetArgumentCount() + exprFunc->GetInputColumnCount() * 4;
    projFuncArgs.reserve(argsSize);

    projFuncArgs.push_back(rowIndexVal);
    projFuncArgs.push_back(outputLenPtr);
    projFuncArgs.push_back(executionContext);
    projFuncArgs.push_back(isNullPtr);

    projFuncArgs.insert(projFuncArgs.end(), columnArgs.begin(), columnArgs.end());
    projFuncArgs.insert(projFuncArgs.end(), dicArgs.begin(), dicArgs.end());
    projFuncArgs.insert(projFuncArgs.end(), nullArgs.begin(), nullArgs.end());
    projFuncArgs.insert(projFuncArgs.end(), offsetArgs.begin(), offsetArgs.end());

    // Get the boolean response for this row from the filter function.
    // ret = column value after applying projection
    CallInst *ret = builder->CreateCall(func, projFuncArgs, "ROW_PROCESS");

    // Add the processed value to output column.
    builder->CreateBr(addToOutput);
    // Add row index to results array
    builder->SetInsertPoint(addToOutput);

    Value *gep;
    Type *ty = llvmTypes->VectorToLLVMType(*(expr->GetReturnType()));
    if (TypeUtil::IsStringType(expr->GetReturnTypeId())) {
        auto outputLen = builder->CreateLoad(llvmTypes->I32Type(), outputLenPtr, "OUTPUT_LENGTH");
        auto stringPtr = builder->CreateIntToPtr(ret, Type::getInt8PtrTy(*context));
        // call wrap_varchar_vector function
        std::vector<Value *> argVals { outColPtr, curIndexVal, stringPtr, outputLen };
        auto call = builder->CreateCall(varcharVectorFunc, argVals, "wrap_varchar_vector");
        InlineFunctionInfo inlineFunctionInfo;
        InlineFunction(*call, inlineFunctionInfo);
    } else {
        // x* gep = gep x* outColPtr, i32 counter
        gep = builder->CreateGEP(ty, outColPtr, curIndexVal, "OUTPUT_ADDRESS");
        // *gep = ret
        builder->CreateStore(ret, gep);
    }

    auto setNullRet = builder->CreateCall(setBitNullFunc,
        { nullValuesAddress, curIndexVal, builder->CreateLoad(llvmTypes->I1Type(), isNullPtr) }, "wrap_set_bit_null");
    InlineFunctionInfo inlineSetNullFuncInfo;
    InlineFunction(*setNullRet, inlineSetNullFuncInfo);

    builder->CreateBr(incrementCounter);
    // Increment loop counter
    builder->SetInsertPoint(incrementCounter);
    // Increment counter.
    nextIndexVal = builder->CreateAdd(curIndexVal, one, "NEXT_INDEX");
    builder->CreateStore(nextIndexVal, indexStore);
    // If there are rows remaining, repeat, otherwise, exit.
    Value *sentinel;
    if (filter) {
        sentinel = numSelected;
    } else {
        sentinel = numRows;
    }
    Value *cond = builder->CreateICmpSLT(nextIndexVal, sentinel, "END_LOOP_COND");
    builder->CreateCondBr(cond, loopBody, endBlock);

    // Return results
    builder->SetInsertPoint(endBlock);
    builder->CreateRet(nextIndexVal);
    OptimizeFunctionsAndModule();

    return Compile();
}
}
}
