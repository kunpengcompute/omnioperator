/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Description: simple filter code generator
 */

#include "simple_filter_codegen.h"

namespace omniruntime {
namespace codegen {
using namespace llvm;
using namespace llvm::orc;
using namespace omniruntime::expressions;

namespace {
const std::string FUNCTION_NAME = "WRAPPER_FUNC";
const int SIMPLE_FILTER_OUTPUT_LENGTH_INDEX = 4;
const int SIMPLE_FILTER_OUTPUT_IS_NULL_INDEX = 3;
} // namespace

void SimpleFilterCodeGen::Visit(const omniruntime::expressions::FieldExpr &fieldExpr)
{
    Value *data = this->codegenContext->data;
    Value *isNulls = this->codegenContext->nullBitmap;
    Value *lengths = this->codegenContext->offsets;

    Value *colIdx = llvmTypes->CreateConstantInt(fieldExpr.colVal);
    // Find address of this column in the addresses array argument.
    Value *gep = builder->CreateGEP(llvmTypes->I64Type(), data, colIdx);
    // Load the address value.
    Value *elementAddr = builder->CreateLoad(llvmTypes->I64Type(), gep);
    Value *elementPtr = GetPtrTypeFromInt(fieldExpr.GetReturnTypeId(), elementAddr);

    Type *ty = llvmTypes->VectorToLLVMType(*(fieldExpr.GetReturnType()));
    Value *dataValue = nullptr;
    Value *length = nullptr;
    if (TypeUtil::IsStringType(fieldExpr.GetReturnTypeId())) {
        // Get length for varchar/char type
        auto lengthGEP = builder->CreateGEP(llvmTypes->I32Type(), lengths, colIdx);
        length = builder->CreateLoad(llvmTypes->I32Type(), lengthGEP);
        // For varchar, only need to get the pointer
        dataValue = elementPtr;
    } else {
        dataValue = builder->CreateLoad(ty, elementPtr);
    }

    // Get isNull value
    auto isNullGEP = builder->CreateGEP(llvmTypes->I1Type(), isNulls, colIdx);
    Value *isNull = builder->CreateLoad(llvmTypes->I1Type(), isNullGEP);

    if (TypeUtil::IsDecimalType(fieldExpr.GetReturnTypeId())) {
        Value *precision = llvmTypes->CreateConstantInt(
            dynamic_cast<DecimalDataType *>(fieldExpr.GetReturnType().get())->GetPrecision());
        Value *scale =
            llvmTypes->CreateConstantInt(dynamic_cast<DecimalDataType *>(fieldExpr.GetReturnType().get())->GetScale());
        this->value = std::make_shared<DecimalValue>(dataValue, isNull, precision, scale);
    } else {
        this->value = std::make_shared<CodeGenValue>(dataValue, isNull, length);
    }
}

bool SimpleFilterCodeGen::InitCodegenContext(iterator_range<llvm::Function::arg_iterator> args)
{
    this->codegenContext = std::make_unique<CodegenContext>();
    for (auto &arg : args) {
        auto argName = arg.getName().str();
        if (argName == "data") {
            codegenContext->data = &arg;
        } else if (argName == "isNulls") {
            codegenContext->nullBitmap = &arg;
        } else if (argName == "lengths") {
            codegenContext->offsets = &arg;
        } else if (argName == "executionContext") {
            codegenContext->executionContext = &arg;
        } else if (argName == "dataLength" || argName == "isResultNull") {
            continue;
        } else {
            LLVM_DEBUG_LOG("Invalid argument %s", argName.c_str());
            return false;
        }
    }

    codegenContext->print = modulePtr->getOrInsertFunction(
        "printf",
        FunctionType::get(IntegerType::getInt32Ty(*context), PointerType::get(Type::getInt8Ty(*context), 0), true));

    return true;
}

llvm::Function *SimpleFilterCodeGen::CreateFunction()
{
    // The args indicates the type of the function parameter list.
    std::vector<Type *> args{
        llvmTypes->I64PtrType(), // valueArray*
        llvmTypes->I1PtrType(),  // isNullArray*
        llvmTypes->I32PtrType(), // lengthArray*
        llvmTypes->I1PtrType(),  // isResultNull*
        llvmTypes->I32PtrType(), // outputLength*
        llvmTypes->I64Type()     // executionContext
    };

    FunctionType *prototype = FunctionType::get(llvmTypes->GetFunctionReturnType(expr->GetReturnTypeId()), args, false);
    func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage, FUNCTION_NAME, modulePtr);

    std::string argNames[] = {"data", "isNulls", "lengths", "isResultNull", "dataLength", "executionContext"};
    int32_t idx = 0;
    for (auto &arg : func->args()) {
        arg.setName(argNames[idx]);
        idx++;
    }

    RecordMainFunction(func);

    BasicBlock *body = BasicBlock::Create(*context, "FUNC_BODY", func);
    builder->SetInsertPoint(body);

    if (!InitCodegenContext(func->args())) {
        return nullptr;
    }

    // Generate code
    auto result = VisitExpr(*expr);
    if (!result->IsValidValue()) {
        return nullptr;
    }

    // Update final output Length
    if (result->length != nullptr) {
        Argument *outputLength = func->getArg(SIMPLE_FILTER_OUTPUT_LENGTH_INDEX);
        Value *lengthGep = builder->CreateGEP(llvmTypes->I32Type(), outputLength, llvmTypes->CreateConstantInt(0),
                                              "OUTPUT_LENGTH_ADDRESS");
        builder->CreateStore(result->length, lengthGep);
    }

    Argument *isResultNull = this->func->getArg(SIMPLE_FILTER_OUTPUT_IS_NULL_INDEX);
    Value *nullGep =
        builder->CreateGEP(llvmTypes->I1Type(), isResultNull, llvmTypes->CreateConstantInt(0), "OUTPUT_NULL_ADDRESS");
    builder->CreateStore(result->isNull, nullGep);

    // Return value
    builder->CreateRet(result->data);

    OptimizeModule();

    verifyFunction(*func);
    return func;
}

intptr_t SimpleFilterCodeGen::GetFunction()
{
#ifdef DEBUG
    std::cout << "Row Expression: " << std::endl;
    ExprPrinter p;
    expr->Accept(p);
    std::cout << std::endl;
#endif

    auto func = this->CreateFunction();
    if (func == nullptr) {
        return 0;
    }

#ifdef DEBUG_LLVM
    modulePtr->print(errs(), nullptr);
#endif
    return Compile();
}
} // namespace codegen
} // namespace omniruntime
