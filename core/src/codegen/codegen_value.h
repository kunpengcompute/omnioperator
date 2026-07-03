/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Value object contains reference to data, isNull and varchar length
 */
#ifndef OMNI_RUNTIME_CODEGEN_VALUE_H
#define OMNI_RUNTIME_CODEGEN_VALUE_H

#include <llvm/IR/Value.h>

namespace omniruntime::codegen {
class CodeGenValue {
public:
    explicit CodeGenValue(llvm::Value *data, llvm::Value *isNull, llvm::Value *length = nullptr)
        : data(data), isNull(isNull), length(length)
    {}

    virtual ~CodeGenValue() = default;

    bool IsValidValue()
    {
        return this->data != nullptr;
    }

    friend class ExpressionCodeGen;

    friend class SimpleFilterCodeGen;

#ifndef EXCLUDE_BATCH_FUNCTIONS
    friend class BatchExpressionCodeGen;
#endif

    friend class CodegenBase;

private:
    llvm::Value *data;
    llvm::Value *isNull;
    llvm::Value *length;
};

class DecimalValue : public CodeGenValue {
public:
    explicit DecimalValue(llvm::Value *data, llvm::Value *isNull, llvm::Value *precision, llvm::Value *scale)
        : CodeGenValue(data, isNull), precision(precision), scale(scale)
    {}

    virtual ~DecimalValue() = default;

    llvm::Value *GetPrecision() const
    {
        return precision;
    }

    llvm::Value *GetScale() const
    {
        return scale;
    }

private:
    llvm::Value *precision;
    llvm::Value *scale;
};

class DecimalSplitValue : public DecimalValue {
public:
    explicit DecimalSplitValue(llvm::Value *high, llvm::Value *low, llvm::Value *isNull = nullptr,
        llvm::Value *precision = nullptr, llvm::Value *scale = nullptr)
        : DecimalValue(nullptr, isNull, precision, scale), high(high), low(low)
    {}
    virtual ~DecimalSplitValue() = default;
    const llvm::Value *GetHigh()
    {
        return high;
    }
    const llvm::Value *GetLow()
    {
        return low;
    }

private:
    llvm::Value *high;
    llvm::Value *low;
};
}
#endif // OMNI_RUNTIME_CODEGEN_VALUE_H
