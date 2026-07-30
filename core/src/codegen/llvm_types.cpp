/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression code generator
 */
#include "llvm_types.h"
#include <llvm/IR/Constant.h>
#include <vector>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace llvm;

namespace {
const int INT8_VALUE = 8;
const int INT16_VALUE = 16;
const int INT32_VALUE = 32;
const int INT64_VALUE = 64;
const int INT128_VALUE = 128;
}

LLVMTypes::LLVMTypes(llvm::LLVMContext &context) : context(context)
{
    VectorToLLVMTypeMap = { { OMNI_INT, I32Type() },
        { OMNI_LONG, I64Type() },
        { OMNI_DOUBLE, DoubleType() },
        { OMNI_FLOAT, FloatType() },
        { OMNI_BOOLEAN, I1Type() },
        { OMNI_BYTE, I8Type() },
        { OMNI_SHORT, I16Type() },
        { OMNI_DECIMAL64, I64Type() },
        { OMNI_DECIMAL128, I128Type() },
        { OMNI_DATE32, I32Type() },
        { OMNI_DATE64, I64Type() },
        { OMNI_TIMESTAMP, I64Type() },
        { OMNI_INTERVAL_MONTHS, I32Type() },
        { OMNI_INTERVAL_DAY_TIME, I32Type() },
        { OMNI_VARCHAR, I8PtrType() },
        { OMNI_CHAR, I8PtrType() },
        { OMNI_VARBINARY, I8PtrType() },
        { OMNI_ROW, I64Type() }
    };
}

LLVMTypes::~LLVMTypes() = default;

Value *LLVMTypes::CreateConstantBool(bool v)
{
    return ConstantInt::get(context, APInt(1, v));
}

Value *LLVMTypes::CreateConstantByte(int8_t v)
{
    return ConstantInt::get(context, APInt(INT8_VALUE, v, true));
}

Value *LLVMTypes::CreateConstantShort(int16_t v)
{
    return ConstantInt::get(context, APInt(INT16_VALUE, v, true));
}

Value *LLVMTypes::CreateConstantInt(int32_t v)
{
    return ConstantInt::get(context, APInt(INT32_VALUE, v, true));
}

Value *LLVMTypes::CreateConstantLong(int64_t v)
{
    return ConstantInt::get(context, APInt(INT64_VALUE, v, true));
}

Value *LLVMTypes::CreateConstantDouble(double v)
{
    return ConstantFP::get(context, APFloat(v));
}

Value *LLVMTypes::CreateConstantFloat(float v)
{
    return ConstantFP::get(context, APFloat(v));
}

Value *LLVMTypes::CreateConstant128(int64_t v)
{
    return ConstantInt::get(context, APInt(INT128_VALUE, v, true));
}

llvm::Type *LLVMTypes::VoidType()
{
    return llvm::Type::getVoidTy(context);
}

llvm::Type *LLVMTypes::I1Type()
{
    return llvm::Type::getInt1Ty(context);
}

llvm::Type *LLVMTypes::I8Type()
{
    return llvm::Type::getInt8Ty(context);
}

llvm::Type *LLVMTypes::I16Type()
{
    return llvm::Type::getInt16Ty(context);
}

llvm::Type *LLVMTypes::I32Type()
{
    return llvm::Type::getInt32Ty(context);
}

llvm::Type *LLVMTypes::I64Type()
{
    return llvm::Type::getInt64Ty(context);
}

llvm::Type *LLVMTypes::I128Type()
{
    return llvm::Type::getInt128Ty(context);
}

llvm::Type *LLVMTypes::DoubleType()
{
    return llvm::Type::getDoubleTy(context);
}

llvm::Type *LLVMTypes::FloatType()
{
    return llvm::Type::getFloatTy(context);
}

llvm::PointerType *LLVMTypes::PtrType(llvm::Type *type)
{
    return type->getPointerTo();
}

llvm::PointerType *LLVMTypes::I1PtrType()
{
    return PtrType(I1Type());
}

llvm::PointerType *LLVMTypes::I8PtrType()
{
    return PtrType(I8Type());
}

llvm::PointerType *LLVMTypes::I16PtrType()
{
    return PtrType(I16Type());
}

llvm::PointerType *LLVMTypes::I32PtrType()
{
    return PtrType(I32Type());
}

llvm::PointerType *LLVMTypes::I64PtrType()
{
    return PtrType(I64Type());
}

llvm::PointerType *LLVMTypes::I128PtrType()
{
    return PtrType(I128Type());
}

llvm::PointerType *LLVMTypes::DoublePtrType()
{
    return PtrType(DoubleType());
}

llvm::PointerType *LLVMTypes::FloatPtrType()
{
    return PtrType(FloatType());
}

llvm::Type *LLVMTypes::ToLLVMType(DataTypeId id)
{
    auto result = VectorToLLVMTypeMap.find(id);
    return (result == VectorToLLVMTypeMap.end()) ? NULL : result->second;
}

llvm::Type *LLVMTypes::VectorToLLVMType(const DataType &type)
{
    return ToLLVMType(type.GetId());
}

llvm::Type *LLVMTypes::ToPointerType(DataTypeId typeId)
{
    switch (typeId) {
        case OMNI_BOOLEAN:
            return I1PtrType();
        case OMNI_BYTE:
            return I8PtrType();
        case OMNI_SHORT:
            return I16PtrType();
        case OMNI_INT:
        case OMNI_DATE32:
            return I32PtrType();
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return I64PtrType();
        case OMNI_FLOAT:
            return FloatPtrType();
        case OMNI_DOUBLE:
            return DoublePtrType();
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            return I8PtrType();
        case OMNI_DECIMAL128:
            return I128PtrType();
        default:
            LLVM_DEBUG_LOG("Unsupported column data type %d", typeId);
            return I64PtrType();
    }
}

#ifndef EXCLUDE_BATCH_FUNCTIONS
llvm::Type *LLVMTypes::ToBatchDataPointerType(DataTypeId typeId)
{
    switch (typeId) {
        case OMNI_BOOLEAN:
            return I1PtrType();
        case OMNI_BYTE:
            return I8PtrType();
        case OMNI_SHORT:
            return I16PtrType();
        case OMNI_INT:
        case OMNI_DATE32:
            return I32PtrType();
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return I64PtrType();
        case OMNI_DOUBLE:
            return DoublePtrType();
        case OMNI_FLOAT:
            return FloatPtrType();
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            return PtrType(I8PtrType());
        case OMNI_DECIMAL128:
            return I128PtrType();
        default:
            LLVM_DEBUG_LOG("Unsupported column data type %d", typeId);
            return I64PtrType();
    }
}
#endif

llvm::Type *LLVMTypes::GetFunctionReturnType(DataTypeId typeId)
{
    if (TypeUtil::IsStringType(typeId)) {
        return Type::getInt64Ty(context);
    } else {
        return ToLLVMType(typeId);
    }
}
}
