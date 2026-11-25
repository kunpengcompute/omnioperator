/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * Description: batch projection expression codegen
 */

#include "batch_projection_codegen.h"

namespace omniruntime::codegen {
using namespace llvm;
using namespace orc;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime;

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
intptr_t BatchProjectionCodeGen::GetFunction()
{
    llvm::Function *func = this->CreateBatchFunction();
    if (func == nullptr) {
        return 0;
    }
    return this->CreateBatchWrapper(*func);
}

intptr_t BatchProjectionCodeGen::CreateBatchWrapper(llvm::Function &projFunc)
{
    llvm::Function *proj = &projFunc;

    // The args indicates the type of the function parameter list.
    std::vector<Type *> args;
    args.push_back(llvmTypes->I64PtrType()); // data address array
    args.push_back(llvmTypes->I32Type());    // the num of rows
    args.push_back(llvmTypes->I64Type());    // output array address
    args.push_back(llvmTypes->I32PtrType()); // selected array
    args.push_back(llvmTypes->I32Type());    // the num of selected rows
    args.push_back(llvmTypes->I64PtrType()); // bitmap address array
    args.push_back(llvmTypes->I64PtrType()); // offset address array
    args.push_back(llvmTypes->I32PtrType());  // output null values array
    args.push_back(llvmTypes->I32PtrType()); // output offset array
    args.push_back(llvmTypes->I64Type());    // execution content address
    args.push_back(llvmTypes->I64PtrType()); // dictionary address array

    FunctionType *funcSignature = FunctionType::get(llvmTypes->I32Type(), args, false);
    llvm::Function *funcDecl =
        llvm::Function::Create(funcSignature, llvm::Function::ExternalLinkage, "WRAPPER_FUNC", modulePtr);
    BasicBlock *projectionMain = BasicBlock::Create(*context, "PROJECTION_MAIN", funcDecl);

    // set args names
    Argument *input = funcDecl->getArg(INPUT_TABLE_INDEX);
    input->setName("INPUT_TABLE");
    Argument *numRows = funcDecl->getArg(NUM_ROWS_INDEX);
    numRows->setName("NUM_ROWS");
    Argument *outputAddress = funcDecl->getArg(OUTPUT_ADDRESS_INDEX);
    outputAddress->setName("OUTPUT_ADDRESS");
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

    builder->SetInsertPoint(projectionMain);
    Type *outPtrType = llvmTypes->ToPointerType(expr->GetReturnTypeId());
    if (outPtrType == nullptr) {
        return 0;
    }
    Value *outColPtr = builder->CreateIntToPtr(outputAddress, outPtrType);

    AllocaInst *rowIdxArray;
    if (filter) {
        rowIdxArray = reinterpret_cast<AllocaInst *>(selected);
        numRows = numSelected;
    } else {
        rowIdxArray = builder->CreateAlloca(llvmTypes->I32Type(), numRows, "ROW_IDX_ARRAY");
        CallExternFunction("fill_rowIdx", { OMNI_INT, OMNI_INT }, OMNI_INT, { rowIdxArray, numRows }, nullptr,
            "fill_rowIdx");
    }
    // generate output array for inner function
    AllocaInst *outputLenPtr = builder->CreateAlloca(llvmTypes->I32Type(), numRows, "OUTPUT_LENGTH");
    auto isNullPtr = builder->CreateAlloca(llvmTypes->I1Type(), numRows, "IS_NULL");
    auto resArray = this->GetResultArray(this->expr->GetReturnTypeId(), numRows);

    std::vector<Value *> projFuncArgs { input,        bitmap,           offsets,           numRows,   rowIdxArray,
        outputLenPtr, executionContext, dictionaryVectors, isNullPtr, resArray };
    builder->CreateCall(proj, projFuncArgs, "INNER_FUNC");

    std::vector<Value *> funcArgs;
    if (TypeUtil::IsStringType(expr->GetReturnTypeId())) {
        std::vector<DataTypeId> paramTypes = { OMNI_LONG, OMNI_VARCHAR, OMNI_INT, OMNI_INT };
        funcArgs = { outColPtr, resArray, outputLenPtr, numRows };
        CallExternFunction("batch_WrapVarcharVector", paramTypes, OMNI_INT, funcArgs, nullptr, "copy_varchar_result");
    } else {
        funcArgs = { outColPtr, resArray, numRows };
        CallExternFunction("batch_copy", { this->expr->GetReturnTypeId() }, this->expr->GetReturnTypeId(), funcArgs,
            nullptr, "copy_result");
    }

    funcArgs = { nullValuesAddress, isNullPtr, numRows };
    CallExternFunction("batch_NullArrayToBits", { OMNI_BOOLEAN }, OMNI_BOOLEAN, funcArgs, nullptr, "copy_null");
    builder->CreateRet(numRows);
    OptimizeFunctionsAndModule();
    return Compile();
}
}