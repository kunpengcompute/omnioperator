/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
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
 * Description: Generated Expression Function
 */

#include "expr_function.h"

#include <utility>

namespace omniruntime::codegen {
ExprFunction::ExprFunction(std::string funcName, const Expr &e, CodegenBase &codegen, const DataTypes &inputDataTypes)
    : funcName(std::move(funcName)),
      expr(const_cast<Expr &>(e)),
      codegen(codegen),
      columnTypes(const_cast<DataTypes &>(inputDataTypes))
{
    CreateFunction();
}

Argument *ExprFunction::GetColumnArgument(int i)
{
    return llvmFunc->getArg(arguments.size() + i);
}

Argument *ExprFunction::GetDicArgument(int i)
{
    return llvmFunc->getArg(arguments.size() + columnTypes.GetSize() + i);
}

Argument *ExprFunction::GetNullArgument(int i)
{
    return llvmFunc->getArg(arguments.size() + columnTypes.GetSize() * 2 + i);
}

Argument *ExprFunction::GetOffsetArgument(int i)
{
    return llvmFunc->getArg(arguments.size() + columnTypes.GetSize() * 3 + i);
}

int32_t ExprFunction::GetInputColumnCount()
{
    return columnTypes.GetSize();
}

size_t ExprFunction::GetArgumentCount()
{
    return arguments.size();
}

std::vector<Type *> ExprFunction::GetArguments()
{
    std::vector<Type *> args;
    for (const auto &arg : arguments) {
        args.push_back(arg.type);
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        args.push_back(codegen.GetTypes()->ToPointerType(columnTypes.GetType(i)->GetId()));
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        args.push_back(codegen.GetTypes()->I64Type());
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        args.push_back(codegen.GetTypes()->I1PtrType());
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        args.push_back(codegen.GetTypes()->I32PtrType());
    }

    return args;
}

Type *ExprFunction::GetReturnType()
{
    return codegen.GetTypes()->GetFunctionReturnType(expr.GetReturnTypeId());
}

llvm::Function *ExprFunction::GetFunction()
{
    return llvmFunc;
}

std::vector<Value *> ExprFunction::ToColumnArgs(Value *data)
{
    std::vector<Value *> result;
    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        auto colAddr = codegen.GetIRBuilder()->CreateGEP(codegen.GetTypes()->I64Type(), data,
            codegen.GetTypes()->CreateConstantInt(i), "column_addr_" + itostr(i));
        std::string colName = "column_";
        auto col =
            codegen.GetIRBuilder()->CreateLoad(codegen.GetTypes()->I64Type(), colAddr, colName.append(itostr(i)));
        auto columnPtr = codegen.GetPtrTypeFromInt(columnTypes.GetType(i)->GetId(), col);
        result.push_back(columnPtr);
    }
    return result;
}

std::vector<Value *> ExprFunction::ToDicArgs(Value *dictionary)
{
    std::vector<Value *> result;
    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        auto dicAddr = codegen.GetIRBuilder()->CreateGEP(codegen.GetTypes()->I64Type(), dictionary,
            codegen.GetTypes()->CreateConstantInt(i), "dic_addr_" + itostr(i));
        std::string dicName = "dic_";
        auto dic =
            codegen.GetIRBuilder()->CreateLoad(codegen.GetTypes()->I64Type(), dicAddr, dicName.append(itostr(i)));
        auto dicPtr = codegen.GetIRBuilder()->CreateIntToPtr(dic, codegen.GetTypes()->I64Type());
        result.push_back(dicPtr);
    }
    return result;
}

std::vector<Value *> ExprFunction::ToNullArgs(Value *bitmap)
{
    std::vector<Value *> result;
    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        auto bitmapAddr = codegen.GetIRBuilder()->CreateGEP(codegen.GetTypes()->I64Type(), bitmap,
            codegen.GetTypes()->CreateConstantInt(i), "bitmap_addr_" + itostr(i));
        std::string bitmapName = "bitmap_";
        auto bitmapValue =
            codegen.GetIRBuilder()->CreateLoad(codegen.GetTypes()->I64Type(), bitmapAddr, bitmapName.append(itostr(i)));
        auto bitmapPtr = codegen.GetIRBuilder()->CreateIntToPtr(bitmapValue, codegen.GetTypes()->I32PtrType());
        result.push_back(bitmapPtr);
    }
    return result;
}

std::vector<Value *> ExprFunction::ToOffsetArgs(Value *offset)
{
    std::vector<Value *> result;
    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        auto offsetAddr = codegen.GetIRBuilder()->CreateGEP(codegen.GetTypes()->I64Type(), offset,
            codegen.GetTypes()->CreateConstantInt(i), "offset_addr_" + itostr(i));
        std::string offsetName = "offset_";
        auto offsetValue =
            codegen.GetIRBuilder()->CreateLoad(codegen.GetTypes()->I64Type(), offsetAddr, offsetName.append(itostr(i)));
        auto offsetPtr = codegen.GetIRBuilder()->CreateIntToPtr(offsetValue, codegen.GetTypes()->I32PtrType());
        result.push_back(offsetPtr);
    }
    return result;
}

void ExprFunction::CreateFunction()
{
    FunctionType *prototype = FunctionType::get(GetReturnType(), GetArguments(), false);

    llvmFunc = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage, funcName, codegen.GetModule());

    for (size_t i = 0; i < arguments.size(); i++) {
        auto arg = llvmFunc->getArg(i);
        arg->setName(arguments.at(i).name);
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        std::string colName = "column_";
        size_t idx = arguments.size() + i;
        auto arg = llvmFunc->getArg(idx);
        arg->setName(colName.append(itostr(i)));
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        std::string dicName = "dic_";
        size_t idx = arguments.size() + columnTypes.GetSize() + i;
        auto arg = llvmFunc->getArg(idx);
        arg->setName(dicName.append(itostr(i)));
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        std::string bitmapName = "bitmap_";
        size_t idx = arguments.size() + columnTypes.GetSize() * 2 + i;
        auto arg = llvmFunc->getArg(idx);
        arg->setName(bitmapName.append(itostr(i)));
    }

    for (int32_t i = 0; i < columnTypes.GetSize(); i++) {
        std::string offsetName = "offset_";
        size_t idx = arguments.size() + columnTypes.GetSize() * 3 + i;
        auto arg = llvmFunc->getArg(idx);
        arg->setName(offsetName.append(itostr(i)));
    }
}
}
