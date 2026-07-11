/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression code generator
 */
#ifndef OMNI_RUNTIME_LLVM_TYPES_H
#define OMNI_RUNTIME_LLVM_TYPES_H

#include <cstdint>
#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include "util/type_util.h"
#include "type/data_type.h"

namespace omniruntime::codegen {
class LLVMTypes {
public:
    explicit LLVMTypes(llvm::LLVMContext &context);

    LLVMTypes();

    llvm::Type *VoidType();

    llvm::Type *I1Type();

    llvm::Type *I8Type();

    llvm::Type *I16Type();

    llvm::Type *I32Type();

    llvm::Type *I64Type();

    llvm::Type *I128Type();

    llvm::Type *DoubleType();

    llvm::Type *FloatType();

    llvm::PointerType *PtrType(llvm::Type *type);

    llvm::PointerType *I1PtrType();

    llvm::PointerType *I8PtrType();

    llvm::PointerType *I16PtrType();

    llvm::PointerType *I32PtrType();

    llvm::PointerType *I64PtrType();

    llvm::PointerType *DoublePtrType();

    llvm::PointerType *FloatPtrType();

    llvm::PointerType *I128PtrType();

    llvm::Value *CreateConstantBool(bool n);

    llvm::Value *CreateConstantByte(int8_t n);

    llvm::Value *CreateConstantShort(int16_t n);

    llvm::Value *CreateConstantInt(int32_t n);

    llvm::Value *CreateConstantLong(int64_t n);

    llvm::Value *CreateConstantDouble(double n);

    llvm::Value *CreateConstantFloat(float n);

    llvm::Value *CreateConstant128(int64_t v);

    // / For a given Vector type, find the corresponding ir type.
    llvm::Type *ToLLVMType(omniruntime::type::DataTypeId id);

    llvm::Type *VectorToLLVMType(const omniruntime::type::DataType &type);

    llvm::Type *ToPointerType(omniruntime::type::DataTypeId typeId);

#ifndef EXCLUDE_BATCH_FUNCTIONS
    llvm::Type *ToBatchDataPointerType(omniruntime::type::DataTypeId typeId);
#endif

    llvm::Type *GetFunctionReturnType(omniruntime::type::DataTypeId typeId);

    virtual ~LLVMTypes();

private:
    std::map<omniruntime::type::DataTypeId, llvm::Type *> VectorToLLVMTypeMap;
    llvm::LLVMContext &context;
};
}

#endif // OMNI_RUNTIME_LLVM_TYPES_H