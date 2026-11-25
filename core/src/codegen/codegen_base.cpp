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
 * Description: Base codegen generator
 */

#include "codegen_base.h"

namespace omniruntime::codegen {
CodegenBase::CodegenBase(std::string name, const omniruntime::expressions::Expr &cpExpr,
                         omniruntime::op::OverflowConfig *overflowConfig)
    : funcName(std::move(name)), expr(&cpExpr), overflowConfig(overflowConfig)
{
}

/**
 * Usage example: std::vector<Value *> values;
 * values.push_back(value1);
 * values.push_back(value2);
 * PrintValues("LLVM DEBUG: %d, %d\n", values);
 */
void CodegenBase::PrintValues(std::string format, const std::vector<Value *> &values)
{
    // Return a cast to an i8*
    auto formatPtr = CreateConstantString(std::move(format));
    std::vector<Value *> args;
    args.push_back(formatPtr);
    for (auto v : values) {
        args.push_back(v);
    }

    builder->CreateCall(codegenContext->print, args, "printfCall");
}

std::string CodegenBase::DumpCode()
{
    std::string ir;
    llvm::raw_string_ostream stream(ir);
    modulePtr->print(stream, nullptr);
    std::cout << " Generated code::" << ir;
    return ir;
}

Value *CodegenBase::GetPtrTypeFromInt(omniruntime::type::DataTypeId dataTypeId, Value *elementAddr)
{
    Value *elementPtr = nullptr;
    // Convert the column address to array of proper datatype.
    switch (dataTypeId) {
        case OMNI_BOOLEAN: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I1PtrType()); break;
        case OMNI_BYTE: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I8PtrType()); break;
        case OMNI_SHORT: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I16PtrType()); break;
        case OMNI_INT:
        case OMNI_DATE32: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I32PtrType()); break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I64PtrType()); break;
        case OMNI_DOUBLE: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->DoublePtrType()); break;
        case OMNI_CHAR:
        case OMNI_VARCHAR: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I8PtrType()); break;
        case OMNI_DECIMAL128: elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I128PtrType()); break;
        default:
            LLVM_DEBUG_LOG("Unsupported column data type %d", dataTypeId);
            elementPtr = builder->CreateIntToPtr(elementAddr, llvmTypes->I64PtrType());
            break;
    }
    return elementPtr;
}

llvm::Constant *CodegenBase::CreateConstantString(std::string s)
{
    auto charType = Type::getInt8Ty(*context);
    std::vector<llvm::Constant *> chars(s.size());
    for (unsigned int i = 0; i < s.size(); i++) {
        chars[i] = ConstantInt::get(charType, s[i]);
    }
    chars.push_back(llvm::ConstantInt::get(charType, 0));
    auto stringType = llvm::ArrayType::get(charType, chars.size());

    this->numGlobalValues++;
    auto globalDeclaration = static_cast<llvm::GlobalVariable *>(
        modulePtr->getOrInsertGlobal("string" + std::to_string(this->numGlobalValues), stringType));
    globalDeclaration->setInitializer(llvm::ConstantArray::get(stringType, chars));
    globalDeclaration->setConstant(true);
    globalDeclaration->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
    globalDeclaration->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

    auto stringPtr = llvm::ConstantExpr::getBitCast(globalDeclaration, charType->getPointerTo());
    return stringPtr;
}
} // namespace omniruntime::codegen
