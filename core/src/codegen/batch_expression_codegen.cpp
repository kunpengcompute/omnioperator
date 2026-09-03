/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: batch expression codegen
 */
#include "batch_expression_codegen.h"

namespace omniruntime::codegen {
namespace {
const int BATCH_EXPRFUNC_ROWCNT_INDEX = 3;
const int BATCH_EXPRFUNC_OUT_LENGTH_ARG_INDEX = 5;
const int BATCH_EXPRFUNC_OUT_NULL_INDEX = 8;
const int BATCH_EXPRFUNC_OUT_DATA_INDEX = 9;
}

BatchExpressionCodeGen::BatchExpressionCodeGen(std::string name, const Expr &cpExpr, op::OverflowConfig *overflowConfig)
    : CodegenBase(name, cpExpr, overflowConfig)
{}

bool BatchExpressionCodeGen::InitializeBatchCodegenContext(iterator_range<llvm::Function::arg_iterator> args)
{
    this->batchCodegenContext = std::make_unique<BatchCodegenContext>();
    for (auto &arg : args) {
        auto argName = arg.getName().str();
        if (argName == "data") {
            batchCodegenContext->data = &arg;
        } else if (argName == "nullBitmap") {
            batchCodegenContext->nullBitmap = &arg;
        } else if (argName == "offsets") {
            batchCodegenContext->offsets = &arg;
        } else if (argName == "rowCnt") {
            batchCodegenContext->rowCnt = &arg;
        } else if (argName == "rowIdxArray") {
            batchCodegenContext->rowIdxArray = &arg;
        } else if (argName == "outputLength" || argName == "outputNull" || argName == "outputData") {
            continue;
        } else if (argName == "executionContext") {
            batchCodegenContext->executionContext = &arg;
        } else if (argName == "dictionaryVectors") {
            batchCodegenContext->dictionaryVectors = &arg;
        } else {
            LogWarn("Invalid argument %s", argName.c_str());
            return false;
        }
    }

    return true;
}

llvm::Function *BatchExpressionCodeGen::CreateBatchFunction()
{
    std::vector<Type *> args {
        llvmTypes->I64PtrType(),                                   // data
        llvmTypes->I64PtrType(),                                   // bitmap
        llvmTypes->I64PtrType(),                                   // offsets
        llvmTypes->I32Type(),                                      // rowCnt
        llvmTypes->I32PtrType(),                                   // rowIdxArray
        llvmTypes->I32PtrType(),                                   // outputLength
        llvmTypes->I64Type(),                                      // executionCon
        llvmTypes->I64PtrType(),                                   // dictionaryVe
        llvmTypes->I1PtrType(),                                    // outputNull
        llvmTypes->ToBatchDataPointerType(expr->GetReturnTypeId()) // outputData
    };

    FunctionType *prototype = FunctionType::get(llvmTypes->I32Type(), args, false);
    func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage, funcName, modulePtr);

    std::string argNames[] = {
        "data", "nullBitmap", "offsets", "rowCnt", "rowIdxArray",
        "outputLength", "executionContext", "dictionaryVectors", "outputNull", "outputData"
    };
    int32_t idx = 0;
    for (auto &arg : func->args()) {
        arg.setName(argNames[idx]);
        idx++;
    }

    BasicBlock *body = BasicBlock::Create(*context, "CREATED_BATCH_FUNC_BODY", func);
    builder->SetInsertPoint(body);

    if (!InitializeBatchCodegenContext(func->args())) {
        return nullptr;
    }

    auto result = VisitExpr(*expr);
    if (result->data == nullptr) {
        return nullptr;
    }

    // copy length
    if (result->length != nullptr) {
        CallExternFunction("batch_copy", { OMNI_INT }, OMNI_INT,
            { func->getArg(BATCH_EXPRFUNC_OUT_LENGTH_ARG_INDEX), result->length,
            func->getArg(BATCH_EXPRFUNC_ROWCNT_INDEX) },
            nullptr, "copy_length");
    }
    // copy data
    CallExternFunction("batch_copy", { expr->GetReturnTypeId() }, expr->GetReturnTypeId(),
        { func->getArg(BATCH_EXPRFUNC_OUT_DATA_INDEX), result->data, func->getArg(BATCH_EXPRFUNC_ROWCNT_INDEX) },
        nullptr, "copy_data");

    // copy null
    CallExternFunction("batch_copy", { OMNI_BOOLEAN }, OMNI_BOOLEAN,
        { func->getArg(BATCH_EXPRFUNC_OUT_NULL_INDEX), result->isNull, func->getArg(BATCH_EXPRFUNC_ROWCNT_INDEX) },
        nullptr, "copy_null");

    // Return rowCnt
    builder->CreateRet(func->getArg(BATCH_EXPRFUNC_ROWCNT_INDEX));
    verifyFunction(*func);
    return func;
}

CodeGenValuePtr BatchExpressionCodeGen::VisitExpr(const Expr &e)
{
    e.Accept(*this);
    return this->value;
}

void BatchExpressionCodeGen::Visit(const LiteralExpr &lExpr)
{
    this->value.reset(BatchLiteralExprConstantHelper(lExpr));
}

CodeGenValue *BatchExpressionCodeGen::BatchLiteralExprConstantHelper(const LiteralExpr &lExpr)
{
    bool isNullLiteral = lExpr.isNull;
    Value *isNull = llvmTypes->CreateConstantBool(isNullLiteral);
    AllocaInst *nullArrayPtr =
        builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "IS_NULL_PTR");
    AllocaInst *literalArrayPtr = GetResultArray(lExpr.GetReturnTypeId(), this->batchCodegenContext->rowCnt);
    Value *literalValue = nullptr;
    Value *length = nullptr;
    Value *lengthArrayPtr = nullptr;
    Value *precisionVal = nullptr;
    Value *scaleVal = nullptr;
    switch (lExpr.GetReturnTypeId()) {
        case OMNI_INT:
        case OMNI_DATE32: {
            literalValue = llvmTypes->CreateConstantInt(lExpr.intVal);
            break;
        }
        case OMNI_TIMESTAMP:
        case OMNI_LONG: {
            literalValue = llvmTypes->CreateConstantLong(lExpr.longVal);
            break;
        }
        case OMNI_DOUBLE: {
            literalValue = llvmTypes->CreateConstantDouble(lExpr.doubleVal);
            break;
        }
        case OMNI_FLOAT: {
            literalValue = llvmTypes->CreateConstantFloat(lExpr.floatVal);
            break;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            literalValue = this->CreateConstantString(*(lExpr.stringVal));
            lengthArrayPtr =
                builder->CreateAlloca(llvmTypes->I32Type(), this->batchCodegenContext->rowCnt, "LENGTH_PTR");
            length = llvmTypes->CreateConstantInt(lExpr.stringVal->length());
            break;
        }
        case OMNI_BOOLEAN: {
            literalValue = llvmTypes->CreateConstantBool(lExpr.boolVal);
            break;
        }
        case OMNI_DECIMAL64: {
            precisionVal = llvmTypes->CreateConstantInt(
                static_cast<Decimal64DataType *>(lExpr.GetReturnType().get())->GetPrecision());
            scaleVal =
                llvmTypes->CreateConstantInt(static_cast<Decimal64DataType *>(lExpr.GetReturnType().get())->GetScale());
            literalValue = llvmTypes->CreateConstantLong(lExpr.longVal);
            break;
        }
        case OMNI_DECIMAL128: {
            std::string dec128String = isNullLiteral ? "0" : *lExpr.stringVal;
            __uint128_t dec128 = Decimal128Utils::StrToUint128_t(dec128String.c_str());
            dec128String = Decimal128Utils::Uint128_tToStr(dec128);
            precisionVal = llvmTypes->CreateConstantInt(
                dynamic_cast<Decimal128DataType *>(lExpr.GetReturnType().get())->GetPrecision());
            scaleVal = llvmTypes->CreateConstantInt(
                dynamic_cast<Decimal128DataType *>(lExpr.GetReturnType().get())->GetScale());
            literalValue = llvm::ConstantInt::get(llvm::Type::getInt128Ty(*context), dec128String, 10);
            break;
        }
        default: {
            LogWarn("Unsupported data type in LITERAL Expr %d", lExpr.GetReturnTypeId());
            return new CodeGenValue(nullptr, nullptr);
        }
    }

    std::vector<Value *> funcArgs;
    if (TypeUtil::IsStringType(lExpr.GetReturnTypeId())) {
        funcArgs = { this->batchCodegenContext->executionContext,
            literalArrayPtr,
            nullArrayPtr,
            lengthArrayPtr,
            literalValue,
            isNull,
            length,
            this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_fill_literal", { OMNI_VARCHAR }, OMNI_VARCHAR, funcArgs, nullptr,
            "fill_literal_array");
        return new CodeGenValue(literalArrayPtr, nullArrayPtr, lengthArrayPtr);
    } else if (TypeUtil::IsDecimalType(lExpr.GetReturnTypeId())) {
        funcArgs = { literalArrayPtr, nullArrayPtr, literalValue, isNull, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_fill_literal", { lExpr.GetReturnTypeId() }, lExpr.GetReturnTypeId(), funcArgs,
            nullptr, "fill_literal_array");
        return new DecimalValue(literalArrayPtr, nullArrayPtr, precisionVal, scaleVal);
    } else {
        funcArgs = { literalArrayPtr, nullArrayPtr, literalValue, isNull, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_fill_literal", { lExpr.GetReturnTypeId() }, lExpr.GetReturnTypeId(), funcArgs,
            nullptr, "fill_literal_array");
        return new CodeGenValue(literalArrayPtr, nullArrayPtr);
    }
}

void BatchExpressionCodeGen::Visit(const FieldExpr &fExpr)
{
    Value *rowCnt = this->batchCodegenContext->rowCnt;
    Value *vecBatch = this->batchCodegenContext->data;
    Value *bitmap = this->batchCodegenContext->nullBitmap;
    Value *offsets = this->batchCodegenContext->offsets;
    Value *dictionaryVectors = this->batchCodegenContext->dictionaryVectors;
    Value *rowIdxArray = this->batchCodegenContext->rowIdxArray;

    Value *colIdx = llvmTypes->CreateConstantInt(fExpr.colVal);
    Value *gep = builder->CreateGEP(llvmTypes->I64Type(), vecBatch, colIdx);
    auto dictionaryVectorGEP = builder->CreateGEP(llvmTypes->I64Type(), dictionaryVectors, colIdx);
    Value *dictionaryVectorPtr = builder->CreateLoad(llvmTypes->I64Type(), dictionaryVectorGEP);
    auto condition = builder->CreateIsNotNull(dictionaryVectorPtr);

    BasicBlock *trueBlock = BasicBlock::Create(*context, "DICTIONARY_NOT_NULL", func);
    BasicBlock *falseBlock = BasicBlock::Create(*context, "DICTIONARY_IS_NULL");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "field_data");
    builder->CreateCondBr(condition, trueBlock, falseBlock);

    builder->SetInsertPoint(trueBlock);
    AllocaInst *dicLengthArray = builder->CreateAlloca(llvmTypes->I32Type(), rowCnt, "dic_varchar_length");
    auto dicArrayPtr = this->GetDictionaryVectorValue(*(fExpr.GetReturnType()), rowIdxArray, rowCnt,
        dictionaryVectorPtr, dicLengthArray);
    builder->CreateBr(mergeBlock);
    trueBlock = builder->GetInsertBlock();

    func->getBasicBlockList().push_back(falseBlock);
    builder->SetInsertPoint(falseBlock);
    Value *elementAddr = builder->CreateLoad(llvmTypes->I64Type(), gep);
    AllocaInst *lengthArray = nullptr;
    Value *dataArrayPtr = nullptr;
    if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        lengthArray = builder->CreateAlloca(llvmTypes->I32Type(), rowCnt, "varchar_length");
        auto offsetsGEP = builder->CreateGEP(llvmTypes->I64Type(), offsets, colIdx);
        Value *offsetPtr = builder->CreateLoad(llvmTypes->I64Type(), offsetsGEP);
        offsetPtr = builder->CreateIntToPtr(offsetPtr, llvmTypes->I32PtrType());
        dataArrayPtr =
            this->GetVectorValue(*(fExpr.GetReturnType()), rowIdxArray, rowCnt, elementAddr, offsetPtr, lengthArray);
    } else {
        dataArrayPtr =
            this->GetVectorValue(*(fExpr.GetReturnType()), rowIdxArray, rowCnt, elementAddr, nullptr, nullptr);
    }
    builder->CreateBr(mergeBlock);
    falseBlock = builder->GetInsertBlock();

    func->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);
    int32_t numReservedValues = 2;
    Type *phiType = llvmTypes->ToBatchDataPointerType(fExpr.GetReturnTypeId());
    PHINode *phiValue = builder->CreatePHI(phiType, numReservedValues, "data");
    phiValue->addIncoming(dicArrayPtr, trueBlock);
    phiValue->addIncoming(dataArrayPtr, falseBlock);

    PHINode *phiLength = nullptr;
    if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        phiLength = builder->CreatePHI(llvmTypes->I32PtrType(), numReservedValues, "length");
        phiLength->addIncoming(dicLengthArray, trueBlock);
        phiLength->addIncoming(lengthArray, falseBlock);
    }

    // Get isNull value
    auto bitmapGEP = builder->CreateGEP(llvmTypes->I64Type(), bitmap, colIdx);
    Value *nullBitsPtr = builder->CreateLoad(llvmTypes->I64Type(), bitmapGEP);
    nullBitsPtr = builder->CreateIntToPtr(nullBitsPtr, llvmTypes->I32PtrType());
    auto dstNullArray = GetResultArray(OMNI_BOOLEAN, rowCnt);
    CallExternFunction("batch_BitsToNullArray", { OMNI_BOOLEAN }, OMNI_BOOLEAN,
        { dstNullArray, nullBitsPtr, rowIdxArray, rowCnt }, nullptr, "copy_null");

    if (TypeUtil::IsDecimalType(fExpr.GetReturnTypeId())) {
        Value *precision =
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision());
        Value *scale =
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale());
        this->value = std::make_shared<DecimalValue>(phiValue, dstNullArray, precision, scale);
    } else if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        this->value = std::make_shared<CodeGenValue>(phiValue, dstNullArray, phiLength);
    } else {
        this->value = std::make_shared<CodeGenValue>(phiValue, dstNullArray);
    }
}

Value *BatchExpressionCodeGen::GetDictionaryVectorValue(const DataType &dataType, Value *rowIdxArray,
    llvm::Value *rowCnt, llvm::Value *dictionaryVectorPtr, AllocaInst *lengthArrayPtr)
{
    std::vector<DataTypeId> paramTypes = { OMNI_LONG };
    DataTypeId retTypeId = dataType.GetId();
    AllocaInst *dataArrayPtr = GetResultArray(retTypeId, rowCnt);
    std::vector<Value *> funcArgs;
    if (TypeUtil::IsStringType(retTypeId)) {
        funcArgs = { batchCodegenContext->executionContext,
            dictionaryVectorPtr,
            rowIdxArray,
            rowCnt,
            dataArrayPtr,
            lengthArrayPtr };
    } else {
        funcArgs = { dictionaryVectorPtr, rowIdxArray, rowCnt, dataArrayPtr };
    }

    CallExternFunction("batch_GetDic", { OMNI_LONG }, retTypeId, funcArgs, nullptr, "get_dictionary_value");
    return dataArrayPtr;
}

Value *BatchExpressionCodeGen::GetVectorValue(const DataType &dataType, Value *rowIdxArray, llvm::Value *rowCnt,
    llvm::Value *dataVectorPtr, Value *offsetArrayPtr, llvm::Value *lengthArrayPtr)
{
    std::vector<DataTypeId> paramTypes = { OMNI_LONG };
    DataTypeId retTypeId = dataType.GetId();
    AllocaInst *dataArrayPtr = GetResultArray(retTypeId, rowCnt);
    std::vector<Value *> funcArgs;
    if (TypeUtil::IsStringType(retTypeId)) {
        funcArgs = { batchCodegenContext->executionContext,
            offsetArrayPtr,
            dataVectorPtr,
            rowIdxArray,
            rowCnt,
            dataArrayPtr,
            lengthArrayPtr };
    } else {
        funcArgs = { dataVectorPtr, rowIdxArray, rowCnt, dataArrayPtr };
    }

    CallExternFunction("batch_GetData", { OMNI_LONG }, retTypeId, funcArgs, nullptr, "get_vector_value");
    return dataArrayPtr;
}

void BatchExpressionCodeGen::Visit(const UnaryExpr &uExpr)
{
    auto val = VisitExpr(*(uExpr.exp));
    if (!val->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    switch (uExpr.op) {
        case omniruntime::expressions::Operator::NOT: {
            std::vector<Value *> funcArgs { val->data, this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_not", { uExpr.exp->GetReturnTypeId() }, uExpr.GetReturnTypeId(), funcArgs,
                nullptr, "logical_not");
            this->value = std::make_shared<CodeGenValue>(val->data, val->isNull);
            break;
        }
        default: {
            this->value = CreateInvalidCodeGenValue();
            break;
        }
    }
}

void BatchExpressionCodeGen::Visit(const BinaryExpr &binaryExpr)
{
    auto *bExpr = const_cast<BinaryExpr *>(&binaryExpr);

    CodeGenValuePtr left = VisitExpr(*(bExpr->left));
    if (!left->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *leftValue = left->data;
    Value *leftLen = left->length;
    Value *leftNull = left->isNull;

    CodeGenValuePtr right = VisitExpr(*(bExpr->right));
    if (!right->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *rightValue = right->data;
    Value *rightLen = right->length;
    Value *rightNull = right->isNull;

    if (bExpr->op == omniruntime::expressions::Operator::AND) {
        std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
        std::vector<Value *> andFuncParams { leftValue, leftNull, rightValue, rightNull,
            this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_and_expr", boolParams, OMNI_BOOLEAN, andFuncParams, nullptr, "and_expr");
        this->value = std::make_shared<CodeGenValue>(leftValue, leftNull);
        return;
    }

    if (bExpr->op == omniruntime::expressions::Operator::OR) {
        std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
        std::vector<Value *> orFuncParams { leftValue, leftNull, rightValue, rightNull,
            this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_or_expr", boolParams, OMNI_BOOLEAN, orFuncParams, nullptr, "or_expr");
        this->value = std::make_shared<CodeGenValue>(leftValue, leftNull);
        return;
    }

    if (bExpr->left->GetReturnTypeId() == OMNI_INT || bExpr->left->GetReturnTypeId() == OMNI_DATE32 ||
        bExpr->left->GetReturnTypeId() == OMNI_LONG || bExpr->left->GetReturnTypeId() == OMNI_TIMESTAMP) {
        this->BatchBinaryExprIntLongHelper(bExpr, leftValue, rightValue, leftNull, rightNull);
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_DOUBLE) {
        this->BatchBinaryExprDoubleHelper(bExpr, leftValue, rightValue, leftNull, rightNull);
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_FLOAT) {
        this->BatchBinaryExprFloatHelper(bExpr, leftValue, rightValue, leftNull, rightNull);
        return;
    } else if (TypeUtil::IsStringType(bExpr->left->GetReturnTypeId())) {
        this->BatchBinaryExprStringHelper(bExpr, leftValue, leftLen, rightValue, rightLen, leftNull, rightNull);
        return;
    } else if (TypeUtil::IsDecimalType(bExpr->left->GetReturnTypeId())) {
        this->BatchBinaryExprDecimalHelper(bExpr, static_cast<DecimalValue &>(*left.get()),
            static_cast<DecimalValue &>(*right.get()), leftNull, rightNull);
        return;
    }

    LogWarn("Unsupported data type for BINARY expr %d", bExpr->left->GetReturnTypeId());
    this->value = CreateInvalidCodeGenValue();
}

void BatchExpressionCodeGen::Visit(const BetweenExpr &btExpr)
{
    auto bExpr = const_cast<BetweenExpr *>(&btExpr);

    auto val = VisitExpr(*(bExpr->value));
    if (!val->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    auto lowerVal = VisitExpr(*(bExpr->lowerBound));
    if (!lowerVal->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    auto upperVal = VisitExpr(*(bExpr->upperBound));
    if (!upperVal->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    AllocaInst *cmpLeft = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "cmpLeft");
    AllocaInst *cmpRight = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "cmpRight");
    std::pair<AllocaInst **, AllocaInst **> cmpPair = std::make_pair(&cmpLeft, &cmpRight);

    BatchVisitBetweenExprHelper(*bExpr, val, lowerVal, upperVal, cmpPair);
}

void BatchExpressionCodeGen::Visit(const IsNullExpr &isNullExpr)
{
    Expr *valueExpr = isNullExpr.value;
    auto isNullValue = VisitExpr(*valueExpr);
    if (!isNullValue->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    std::vector<Value *> funcArgs { isNullValue->isNull, llvmTypes->CreateConstantBool(true),
        this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_equal", { OMNI_BOOLEAN, OMNI_BOOLEAN }, OMNI_BOOLEAN, funcArgs, nullptr, "is_null");

    AllocaInst *nullArrayPtr =
        builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "IS_NULL_PTR");
    funcArgs = { nullArrayPtr, llvmTypes->CreateConstantBool(false), this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_fill_null", { OMNI_BOOLEAN }, OMNI_BOOLEAN, funcArgs, nullptr, "batch_fill_null");

    this->value = std::make_shared<CodeGenValue>(isNullValue->isNull, nullArrayPtr);
}

llvm::AllocaInst *BatchExpressionCodeGen::GetResultArray(omniruntime::type::DataTypeId dataTypeId, Value *rowCnt)
{
    AllocaInst *resultArray = nullptr;
    switch (dataTypeId) {
        case OMNI_INT:
        case OMNI_DATE32: {
            resultArray = builder->CreateAlloca(llvmTypes->I32Type(), rowCnt, "DATA_PTR");
            break;
        }
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
        case OMNI_LONG: {
            resultArray = builder->CreateAlloca(llvmTypes->I64Type(), rowCnt, "DATA_PTR");
            break;
        }
        case OMNI_DOUBLE: {
            resultArray = builder->CreateAlloca(llvmTypes->DoubleType(), rowCnt, "DATA_PTR");
            break;
        }
        case OMNI_FLOAT: {
            resultArray = builder->CreateAlloca(llvmTypes->FloatType(), rowCnt, "DATA_PTR");
            break;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            resultArray = builder->CreateAlloca(llvmTypes->I8PtrType(), rowCnt, "DATA_PTR");
            break;
        }
        case OMNI_BOOLEAN: {
            resultArray = builder->CreateAlloca(llvmTypes->I1Type(), rowCnt, "DATA_PTR");
            break;
        }
        case OMNI_DECIMAL128: {
            resultArray = builder->CreateAlloca(llvmTypes->I128Type(), rowCnt, "DATA_PTR");
            break;
        }
        default: {
            LogWarn("Unsupported type when creating array %d", dataTypeId);
            break;
        }
    }
    if (resultArray == nullptr) {
        LogWarn("Failed to create result array");
    }
    return resultArray;
}

static std::string ChangeFuncNameToNull(const FuncExpr &fExpr)
{
    auto typeSize = static_cast<int32_t>(fExpr.arguments.size() + 1);
    auto originalFuncName = fExpr.function->GetId();
    auto originalFuncChars = originalFuncName.c_str();
    int32_t separatorIdx = 0;
    auto pos = static_cast<int32_t>(originalFuncName.length() - 1);
    for (; pos >= 0; pos--) {
        if (originalFuncChars[pos] == '_') {
            separatorIdx++;
            if (separatorIdx == typeSize) {
                break;
            }
        }
    }
    return originalFuncName.insert(pos, "_null");
}

void BatchExpressionCodeGen::FuncExprOverflowNullHelper(const FuncExpr &fExpr)
{
    Value *falseValue = llvmTypes->CreateConstantBool(false);
    AllocaInst *isAnyNull =
        builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "IS_NULL_PTR");
    std::vector<Value *> funcArgs { isAnyNull, falseValue, this->batchCodegenContext->rowCnt };
    auto ret =
        CallExternFunction("batch_fill_null", { OMNI_BOOLEAN }, OMNI_BOOLEAN, funcArgs, nullptr, "fill_null_array");
    AllocaInst *overflowNull =
        builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "OVERFLOW_NULL_PTR");
    funcArgs = { overflowNull, falseValue, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_fill_null", { OMNI_BOOLEAN }, OMNI_BOOLEAN, funcArgs, nullptr,
        "fill_overflow_null_array");

    DataTypeId funcRetType = fExpr.GetReturnTypeId();
    bool isInvalidExpr = false;

    auto argVals = GetDataAndOverflowNullArgs(fExpr, isAnyNull, isInvalidExpr, overflowNull);
    if (isInvalidExpr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *isNullArray = PushAndGetNullFlagArray(fExpr, argVals, isAnyNull, false);
    AllocaInst *resultArray = GetResultArray(funcRetType, this->batchCodegenContext->rowCnt);
    argVals.push_back(resultArray);
    AllocaInst *outputLenPtr = nullptr;

    if (TypeUtil::IsStringType(funcRetType)) {
        outputLenPtr = builder->CreateAlloca(llvmTypes->I32Type(), this->batchCodegenContext->rowCnt, "output_len");
        auto defaultLength =
            llvmTypes->CreateConstantInt(static_cast<CharDataType *>(fExpr.GetReturnType().get())->GetWidth());
        funcArgs = { outputLenPtr, defaultLength, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_fill_length_literal", { OMNI_INT }, OMNI_INT, funcArgs, nullptr,
            "fill_literal_array");
        argVals.push_back(outputLenPtr);
        if (FuncExpr::IsCastStrStr(fExpr)) {
            argVals.push_back(
                llvmTypes->CreateConstantInt(static_cast<VarcharDataType *>(fExpr.GetReturnType().get())->GetWidth()));
        }
    } else if (TypeUtil::IsDecimalType(funcRetType)) {
        argVals.push_back(
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision()));
        argVals.push_back(
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale()));
    }
    argVals.push_back(this->batchCodegenContext->rowCnt);

    auto f = modulePtr->getFunction("batch_" + ChangeFuncNameToNull(fExpr));
    if (f) {
        ret = CreateCall(f, argVals, fExpr.function->GetId());
        InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(*((CallInst *)ret), inlineFunctionInfo);
    } else {
        LogWarn("Unable to generate function : %s", fExpr.funcName.c_str());
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    CallExternFunction("batch_or", { OMNI_BOOLEAN, OMNI_BOOLEAN }, OMNI_BOOLEAN,
        { isNullArray, overflowNull, this->batchCodegenContext->rowCnt }, nullptr);

    if (TypeUtil::IsDecimalType(funcRetType)) {
        this->value = std::make_shared<DecimalValue>(resultArray, isNullArray,
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision()),
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale()));
    } else {
        this->value = std::make_shared<CodeGenValue>(resultArray, isNullArray, outputLenPtr);
    }
}

std::vector<llvm::Value *> BatchExpressionCodeGen::GetDataAndOverflowNullArgs(
    const omniruntime::expressions::FuncExpr &fExpr, AllocaInst *isAnyNull, bool &isInvalidExpr,
    AllocaInst *overflowNull)
{
    std::vector<Value *> argVals;
    argVals.push_back(overflowNull);
    CodeGenValuePtr resultPtr;
    int numArgs = fExpr.arguments.size();
    std::vector<Value *> nullFuncParams;

    auto signature = fExpr.function->GetSignatures()[0];
    if (FunctionRegistry::IsNullExecutionContextSet(&signature)) {
        argVals.push_back(this->batchCodegenContext->executionContext);
    }
    for (int i = 0; i < numArgs; i++) {
        Expr *argN = fExpr.arguments[i];
        resultPtr = VisitExpr(*argN);
        if (!resultPtr->IsValidValue()) {
            isInvalidExpr = true;
            return argVals;
        }
        argVals.push_back(resultPtr->data);

        nullFuncParams = { isAnyNull, resultPtr->isNull, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_or", { OMNI_BOOLEAN, OMNI_BOOLEAN }, OMNI_BOOLEAN, nullFuncParams, nullptr,
            "either_null");

        if ((TypeUtil::IsStringType(argN->GetReturnTypeId()))) {
            if (argN->GetReturnTypeId() == OMNI_CHAR) {
                argVals.push_back(
                    llvmTypes->CreateConstantInt(static_cast<CharDataType *>(argN->GetReturnType().get())->GetWidth()));
            }
            if (FuncExpr::IsCastStrStr(fExpr)) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    static_cast<VarcharDataType *>(argN->GetReturnType().get())->GetWidth()));
            }
            argVals.push_back(this->value->length);
        }
        if (TypeUtil::IsDecimalType(argN->GetReturnTypeId())) {
            argVals.push_back(llvmTypes->CreateConstantInt(
                static_cast<DecimalDataType *>(argN->GetReturnType().get())->GetPrecision()));
            argVals.push_back(
                llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(argN->GetReturnType().get())->GetScale()));
        }
        if (fExpr.function->GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL) {
            argVals.push_back(this->value->isNull);
        }
    }
    return argVals;
}

template <bool isNeedVerifyResult, bool isNeedVerifyVal>
std::vector<llvm::Value *> BatchExpressionCodeGen::GetDefaultFunctionArgValues(
    const FuncExpr &fExpr,
    AllocaInst *isAnyNull,
    bool &isInvalidExpr)
{
    std::vector<Value *> argVals;
    CodeGenValuePtr resultPtr;
    int numArgs = fExpr.arguments.size();
    std::vector<Value *> nullFuncParams;

    if (fExpr.function->IsExecutionContextSet()) {
        argVals.push_back(this->batchCodegenContext->executionContext);
    }
    for (int i = 0; i < numArgs; i++) {
        Expr *argN = fExpr.arguments[i];
        resultPtr = VisitExpr(*argN);
        if (!resultPtr->IsValidValue()) {
            isInvalidExpr = true;
            return argVals;
        }
        argVals.push_back(resultPtr->data);
        if constexpr (isNeedVerifyResult) {
            nullFuncParams = { isAnyNull, resultPtr->isNull, this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_or", { OMNI_BOOLEAN, OMNI_BOOLEAN }, OMNI_BOOLEAN, nullFuncParams, nullptr,
                               "either_null");
        }
        if ((TypeUtil::IsStringType(argN->GetReturnTypeId()))) {
            if (argN->GetReturnTypeId() == OMNI_CHAR) {
                argVals.push_back(
                    llvmTypes->CreateConstantInt(
                        static_cast<CharDataType *>(argN->GetReturnType().get())->GetWidth()));
            }
            if (FuncExpr::IsCastStrStr(fExpr)) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    static_cast<VarcharDataType *>(argN->GetReturnType().get())->GetWidth()));
            }
            argVals.push_back(this->value->length);
        }
        if (TypeUtil::IsDecimalType(argN->GetReturnTypeId())) {
            argVals.push_back(llvmTypes->CreateConstantInt(
                static_cast<DecimalDataType *>(argN->GetReturnType().get())->GetPrecision()));
            argVals.push_back(
                llvmTypes->CreateConstantInt(
                    static_cast<DecimalDataType *>(argN->GetReturnType().get())->GetScale()));
        }
        if constexpr (isNeedVerifyVal) {
            argVals.push_back(this->value->isNull);
        }
    }
    return argVals;
}

inline std::vector<llvm::Value *> BatchExpressionCodeGen::GetDataArgs(const FuncExpr &fExpr, AllocaInst *isAnyNull,
    bool &isInvalidExpr)
{
    return GetDefaultFunctionArgValues<true, false>(fExpr, isAnyNull, isInvalidExpr);
}

inline std::vector<llvm::Value *> BatchExpressionCodeGen::GetDataAndNullArgs(const FuncExpr &fExpr,
    AllocaInst *isAnyNull, bool &isInvalidExpr)
{
    return GetDefaultFunctionArgValues<false, true>(fExpr, isAnyNull, isInvalidExpr);
}

inline std::vector<llvm::Value *> BatchExpressionCodeGen::GetDataAndNullArgsAndReturnNull(const FuncExpr &fExpr,
    AllocaInst *isAnyNull, bool &isInvalidExpr)
{
    return GetDefaultFunctionArgValues<true, true>(fExpr, isAnyNull, isInvalidExpr);
}

std::vector<llvm::Value *> BatchExpressionCodeGen::GetFunctionArgValues(const omniruntime::expressions::FuncExpr &fExpr,
    AllocaInst *isAnyNull, bool &isInvalidExpr)
{
    switch (fExpr.function->GetNullableResultType()) {
        case INPUT_DATA:
            return GetDataArgs(fExpr, isAnyNull, isInvalidExpr);
        case INPUT_DATA_AND_NULL:
            return GetDataAndNullArgs(fExpr, isAnyNull, isInvalidExpr);
        case INPUT_DATA_AND_NULL_AND_RETURN_NULL:
            return GetDataAndNullArgsAndReturnNull(fExpr, isAnyNull, isInvalidExpr);
        default:
            return GetDataArgs(fExpr, isAnyNull, isInvalidExpr);
    }
}

Value *BatchExpressionCodeGen::ArenaAlloc(Value *sizeInBytes)
{
    return CallExternFunction("ArenaAllocatorMalloc", { OMNI_LONG, OMNI_INT }, OMNI_CHAR,
        { batchCodegenContext->executionContext, sizeInBytes }, nullptr);
}

Value *BatchExpressionCodeGen::GetTypeSize(DataTypeId dataTypeId)
{
    int32_t typeSize = 0;
    switch (dataTypeId) {
        case OMNI_INT:
        case OMNI_DATE32:
            typeSize = sizeof(int32_t);
            break;
        case OMNI_TIMESTAMP:
        case OMNI_LONG:
        case OMNI_DECIMAL64:
            typeSize = sizeof(int64_t);
            break;
        case OMNI_DOUBLE:
            typeSize = sizeof(double);
            break;
        case OMNI_FLOAT:
            typeSize = sizeof(float);
            break;
        case OMNI_BOOLEAN:
            typeSize = sizeof(bool);
            break;
        case OMNI_SHORT:
            typeSize = sizeof(int16_t);
            break;
        case OMNI_DECIMAL128:
            typeSize = 2 * sizeof(int64_t);
            break;
        case OMNI_CHAR:
        case OMNI_VARCHAR:
            typeSize = sizeof(int64_t); // for pointer
            break;
        default:
            LogWarn("Unsupported data type in UDF funcExpr %d", dataTypeId);
            return nullptr;
    }
    return llvmTypes->CreateConstantInt(typeSize);
}

std::vector<llvm::Value *> BatchExpressionCodeGen::GetHiveUdfArgValues(const FuncExpr &fExpr, bool &isInvalidExpr)
{
    auto argSize = static_cast<int32_t>(fExpr.arguments.size());
    auto size = llvmTypes->CreateConstantInt(argSize);
    auto valueAddrArray = builder->CreateAlloca(llvmTypes->I64Type(), size);
    auto nullAddrArray = builder->CreateAlloca(llvmTypes->I64Type(), size);
    auto lengthAddrArray = builder->CreateAlloca(llvmTypes->I64Type(), size);

    std::vector<Value *> argVals;
    for (int32_t i = 0; i < argSize; i++) {
        auto argExpr = fExpr.arguments[i];
        auto argExprResult = VisitExpr(*argExpr);
        if (!argExprResult->IsValidValue()) {
            isInvalidExpr = true;
            return argVals;
        }

        auto valuePtr = builder->CreateGEP(llvmTypes->I64Type(), valueAddrArray, llvmTypes->CreateConstantInt(i));
        auto nullPtr = builder->CreateGEP(llvmTypes->I64Type(), nullAddrArray, llvmTypes->CreateConstantInt(i));
        auto lengthPtr = builder->CreateGEP(llvmTypes->I64Type(), lengthAddrArray, llvmTypes->CreateConstantInt(i));
        builder->CreateStore(argExprResult->data, valuePtr);
        builder->CreateStore(argExprResult->isNull, nullPtr);
        auto length = TypeUtil::IsStringType(argExpr->GetReturnTypeId()) ? argExprResult->length :
                                                                           llvmTypes->CreateConstantLong(0);
        builder->CreateStore(length, lengthPtr);
    }

    argVals.push_back(valueAddrArray);
    argVals.push_back(nullAddrArray);
    argVals.push_back(lengthAddrArray);
    return argVals;
}

Value *BatchExpressionCodeGen::CreateHiveUdfArgTypes(const FuncExpr &fExpr)
{
    auto elementSize = static_cast<int32_t>(fExpr.arguments.size());
    auto alloca = builder->CreateAlloca(llvmTypes->I32Type(), llvmTypes->CreateConstantInt(elementSize));
    for (int32_t i = 0; i < elementSize; i++) {
        auto ptr = builder->CreateGEP(llvmTypes->I32Type(), alloca, llvmTypes->CreateConstantInt(i));
        builder->CreateStore(llvmTypes->CreateConstantInt(fExpr.arguments[i]->GetReturnTypeId()), ptr);
    }
    return alloca;
}

void BatchExpressionCodeGen::CallHiveUdfFunction(const FuncExpr &fExpr)
{
    auto returnTypeId = fExpr.GetReturnTypeId();
    std::vector<Value *> argVals;
    argVals.emplace_back(batchCodegenContext->executionContext);
    argVals.emplace_back(CreateConstantString(fExpr.funcName));                 // for udf class name
    argVals.emplace_back(CreateHiveUdfArgTypes(fExpr));                         // for inputTypes
    argVals.emplace_back(llvmTypes->CreateConstantInt(returnTypeId));           // for return type
    argVals.emplace_back(llvmTypes->CreateConstantInt(fExpr.arguments.size())); // for vec count
    argVals.emplace_back(batchCodegenContext->rowCnt);                          // for row count

    bool isInvalidExpr = false;
    auto inputArgs = GetHiveUdfArgValues(fExpr, isInvalidExpr);
    if (isInvalidExpr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    argVals.insert(argVals.end(), inputArgs.begin(),
        inputArgs.end()); // for inputValues, inputNulls, inputLengths

    // for output value, output null, output length
    auto returnTypeSize = GetTypeSize(returnTypeId);
    if (returnTypeSize == nullptr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    auto arraySize = batchCodegenContext->rowCnt;
    auto outputValuePtr = ArenaAlloc(builder->CreateMul(returnTypeSize, arraySize));
    auto outputNullPtr = ArenaAlloc(arraySize);
    auto outputLengthPtr = TypeUtil::IsStringType(returnTypeId) ?
        ArenaAlloc(builder->CreateMul(GetTypeSize(OMNI_INT), arraySize)) :
        llvmTypes->CreateConstantLong(0);
    argVals.emplace_back(outputValuePtr);
    argVals.emplace_back(outputNullPtr);
    argVals.emplace_back(outputLengthPtr);

    auto signature = FunctionSignature("EvaluateHiveUdfBatch", std::vector<DataTypeId> {}, OMNI_INT);
    auto function = FunctionRegistry::LookupFunction(&signature);
    auto f = modulePtr->getFunction(function->GetId());
    if (f) {
        auto ret = CreateCall(f, argVals, "call_evaluate_hive_udf");
        InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(*((CallInst *)ret), inlineFunctionInfo);
        this->value = std::make_shared<CodeGenValue>(outputValuePtr, outputNullPtr,
            TypeUtil::IsStringType(returnTypeId) ? outputLengthPtr : nullptr);
    } else {
        LogWarn("Unable to generate udf function : %s", fExpr.funcName.c_str());
        this->value = CreateInvalidCodeGenValue();
    }
}

void BatchExpressionCodeGen::Visit(const FuncExpr &fExpr)
{
    if (fExpr.functionType == HIVE_UDF) {
        CallHiveUdfFunction(fExpr);
        return;
    }

    if (this->overflowConfig != nullptr &&
        this->overflowConfig->GetOverflowConfigId() == omniruntime::op::OVERFLOW_CONFIG_NULL) {
        auto signature = fExpr.function->GetSignatures()[0];
        if (FunctionRegistry::LookupNullFunction(&signature)) {
            FuncExprOverflowNullHelper(fExpr);
            return;
        }
    }

    Value *falseValue = llvmTypes->CreateConstantBool(false);
    AllocaInst *isAnyNull =
        builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "IS_NULL_PTR");
    std::vector<Value *> funcArgs { isAnyNull, falseValue, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_fill_null", { OMNI_BOOLEAN }, OMNI_BOOLEAN, funcArgs, nullptr, "fill_null_array");

    DataTypeId funcRetType = fExpr.GetReturnTypeId();
    bool isInvalidExpr = false;

    auto argVals = GetFunctionArgValues(fExpr, isAnyNull, isInvalidExpr);
    if (isInvalidExpr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    AllocaInst *resultArray = GetResultArray(funcRetType, this->batchCodegenContext->rowCnt);
    Value *isNullArray = PushAndGetNullFlagArray(fExpr, argVals, isAnyNull, true);
    argVals.push_back(resultArray);
    AllocaInst *outputLenPtr = nullptr;

    if (TypeUtil::IsStringType(funcRetType)) {
        outputLenPtr = builder->CreateAlloca(llvmTypes->I32Type(), this->batchCodegenContext->rowCnt, "output_len");
        auto defaultLength =
            llvmTypes->CreateConstantInt(static_cast<CharDataType *>(fExpr.GetReturnType().get())->GetWidth());
        funcArgs = { outputLenPtr, defaultLength, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_fill_length_literal", { OMNI_INT }, OMNI_INT, funcArgs, nullptr,
            "fill_literal_array");
        argVals.push_back(outputLenPtr);
        if (FuncExpr::IsCastStrStr(fExpr)) {
            argVals.push_back(
                llvmTypes->CreateConstantInt(static_cast<VarcharDataType *>(fExpr.GetReturnType().get())->GetWidth()));
        }
    } else if (TypeUtil::IsDecimalType(funcRetType)) {
        argVals.push_back(
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision()));
        argVals.push_back(
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale()));
    }
    argVals.push_back(this->batchCodegenContext->rowCnt);

    auto f = modulePtr->getFunction("batch_" + fExpr.function->GetId());
    if (f) {
        auto ret = CreateCall(f, argVals, fExpr.function->GetId());
        InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(*((CallInst *)ret), inlineFunctionInfo);
    } else {
        LogWarn("Unable to generate function : %s", fExpr.funcName.c_str());
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    if (TypeUtil::IsDecimalType(funcRetType)) {
        this->value = std::make_shared<DecimalValue>(resultArray, isNullArray,
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision()),
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale()));
    } else {
        this->value = std::make_shared<CodeGenValue>(resultArray, isNullArray, outputLenPtr);
    }
}

void BatchExpressionCodeGen::Visit(const IfExpr &ifExpr)
{
    Expr *cond = ifExpr.condition;
    Expr *ifTrue = ifExpr.trueExpr;
    Expr *ifFalse = ifExpr.falseExpr;

    auto baseType = ifExpr.GetReturnTypeId();

    CodeGenValuePtr evCond = VisitExpr(*cond);
    if (!evCond->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    auto evTrue = VisitExpr(*ifTrue);
    if (!evTrue->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *evTrueValue = evTrue->data;
    Value *evTrueLength = evTrue->length;
    Value *evTrueNull = evTrue->isNull;

    auto evFalse = VisitExpr(*ifFalse);
    if (!evFalse->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *evFalseValue = evFalse->data;
    Value *evFalseLength = evFalse->length;
    Value *evFalseNull = evFalse->isNull;

    switch (baseType) {
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_FLOAT:
        case OMNI_BOOLEAN: {
            CallExternFunction("batch_if", { baseType }, baseType,
                { evCond->data, evCond->isNull, evTrueValue, evTrueNull, evFalseValue, evFalseNull,
                this->batchCodegenContext->rowCnt },
                nullptr);
            this->value = std::make_shared<CodeGenValue>(evTrueValue, evTrueNull);
            return;
        }
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128: {
            auto &left = static_cast<DecimalValue &>(*evTrue);
            auto &right = static_cast<DecimalValue &>(*evFalse);

            std::vector<Value *> argValsCmp { evCond->data,
                evCond->isNull,
                left.data,
                left.isNull,
                right.data,
                right.isNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_if", { baseType }, baseType, argValsCmp, nullptr);
            this->value = std::make_shared<DecimalValue>(left.data, left.isNull,
                const_cast<Value *>(left.GetPrecision()), const_cast<Value *>(left.GetScale()));
            return;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            CallExternFunction("batch_if", { baseType }, baseType,
                { evCond->data, evCond->isNull, evTrueValue, evTrueNull, evTrueLength, evFalseValue, evFalseNull,
                evFalseLength, this->batchCodegenContext->rowCnt },
                nullptr);
            this->value = std::make_shared<CodeGenValue>(evTrueValue, evTrueNull, evTrueLength);
            return;
        }
        default: {
            LogWarn("Unsupported data type in IF expr %d", baseType);
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
}

void BatchExpressionCodeGen::Visit(const CoalesceExpr &cExpr)
{
    Expr *value1Expr = cExpr.value1;
    Expr *value2Expr = cExpr.value2;
    CodeGenValuePtr value1 = VisitExpr(*value1Expr);
    if (!value1->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    auto value2 = VisitExpr(*value2Expr);
    if (!value2->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    if (cExpr.GetReturnTypeId() == OMNI_BOOLEAN || cExpr.GetReturnTypeId() == OMNI_INT ||
        cExpr.GetReturnTypeId() == OMNI_LONG || cExpr.GetReturnTypeId() == OMNI_DOUBLE ||
        cExpr.GetReturnTypeId() == OMNI_DATE32 || cExpr.GetReturnTypeId() == OMNI_TIMESTAMP) {
        CallExternFunction("batch_coalesce", { cExpr.GetReturnTypeId(), cExpr.GetReturnTypeId() },
            cExpr.GetReturnTypeId(),
            { value1->data, value1->isNull, value2->data, value2->isNull, this->batchCodegenContext->rowCnt }, nullptr);
        this->value = std::make_shared<CodeGenValue>(value1->data, value1->isNull);
    } else if (TypeUtil::IsStringType(cExpr.GetReturnTypeId())) {
        CallExternFunction("batch_coalesce", { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_VARCHAR,
            { value1->data, value1->isNull, value1->length, value2->data, value2->isNull, value2->length,
            this->batchCodegenContext->rowCnt },
            nullptr);
        this->value = std::make_shared<CodeGenValue>(value1->data, value1->isNull, value1->length);
    } else if (TypeUtil::IsDecimalType(cExpr.GetReturnTypeId())) {
        auto value1Precision = (Value *)static_cast<DecimalValue &>(*value1.get()).GetPrecision();
        auto value1Scale = (Value *)static_cast<DecimalValue &>(*value1.get()).GetScale();

        CallExternFunction("batch_coalesce", { cExpr.GetReturnTypeId(), cExpr.GetReturnTypeId() },
            cExpr.GetReturnTypeId(),
            { value1->data, value1->isNull, value2->data, value2->isNull, this->batchCodegenContext->rowCnt }, nullptr);
        this->value = std::make_shared<DecimalValue>(value1->data, value1->isNull, value1Precision, value1Scale);
    } else {
        LogWarn("Unsupported data type in COALESCE expr %d", cExpr.GetReturnTypeId());
        this->value = CreateInvalidCodeGenValue();
        return;
    }
}

void BatchExpressionCodeGen::Visit(const InExpr &inExpr)
{
    auto iExpr = const_cast<InExpr *>(&inExpr);
    Expr *toCompare = iExpr->arguments[0];
    auto baseType = iExpr->arguments[0]->GetReturnTypeId();
    int32_t size = iExpr->arguments.size();

    auto valueToCompare = VisitExpr(*toCompare);
    if (!valueToCompare->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    Value *inArray = GetResultArray(OMNI_BOOLEAN, this->batchCodegenContext->rowCnt);
    Value *isNull = GetResultArray(OMNI_BOOLEAN, this->batchCodegenContext->rowCnt);

    std::vector<CodeGenValuePtr> cmps(size - 1);
    for (int i = 1; i < size; ++i) {
        Expr *cmp = iExpr->arguments[i];
        cmps[i - 1] = VisitExpr(*cmp);
        if (!cmps[i - 1]->IsValidValue()) {
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }

    auto cmpValues = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size - 1));
    auto cmpBools = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size - 1));
    for (int i = 0; i < size - 1; ++i) {
        Value *gep =
            builder->CreateGEP(llvmTypes->I64Type(), cmpValues, llvmTypes->CreateConstantInt(i), "cmp_value_address");
        builder->CreateStore(cmps[i]->data, gep);

        gep = builder->CreateGEP(llvmTypes->I64Type(), cmpBools, llvmTypes->CreateConstantInt(i), "cmp_null_address");
        builder->CreateStore(cmps[i]->isNull, gep);
    }

    std::vector<Value *> args;
    switch (baseType) {
        case OMNI_BOOLEAN:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_FLOAT:
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128: {
            args = { llvmTypes->CreateConstantInt(size - 1),
                cmpValues,
                cmpBools,
                valueToCompare->data,
                valueToCompare->isNull,
                inArray,
                isNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_in", { baseType }, OMNI_BOOLEAN, args, nullptr);
            break;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            auto cmpLengths = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size - 1));
            for (int i = 0; i < size - 1; ++i) {
                Value *gep = builder->CreateGEP(llvmTypes->I64Type(), cmpLengths, llvmTypes->CreateConstantInt(i),
                    "cmp_length_address");
                builder->CreateStore(cmps[i]->length, gep);
            }
            args = { llvmTypes->CreateConstantInt(size - 1),
                cmpValues,
                cmpBools,
                cmpLengths,
                valueToCompare->data,
                valueToCompare->isNull,
                valueToCompare->length,
                inArray,
                isNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_in", { baseType }, OMNI_BOOLEAN, args, nullptr);
            break;
        }
        default: {
            LogWarn("Unsupported data type in IN expr %d", baseType);
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }

    this->value = std::make_shared<CodeGenValue>(inArray, isNull);
}

void BatchExpressionCodeGen::Visit(const InSubqueryExpr &inSubExpr)
{
    auto baseType = inSubExpr.value->GetReturnTypeId();

    // Visit the probe value expression
    auto probeValue = VisitExpr(*(inSubExpr.value));
    if (!probeValue->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    // Visit the subquery result expression
    auto subqueryValue = VisitExpr(*(inSubExpr.subqueryResult));
    if (!subqueryValue->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    // Result arrays
    Value *inArray = GetResultArray(OMNI_BOOLEAN, this->batchCodegenContext->rowCnt);
    Value *isNull = GetResultArray(OMNI_BOOLEAN, this->batchCodegenContext->rowCnt);

    // Get subquery row count from the subquery result
    // The subquery result is expected to be a single-column vector
    // We use the batchCodegenContext->subqueryRowCount which should be set before execution
    Value *subqueryRowCount = this->batchCodegenContext->subqueryRowCount;

    std::vector<Value *> args;
    switch (baseType) {
        case OMNI_BOOLEAN:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_FLOAT:
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128: {
            args = { subqueryValue->data,
                subqueryValue->isNull,
                subqueryRowCount,
                probeValue->data,
                probeValue->isNull,
                inArray,
                isNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_in_subquery", { baseType }, OMNI_BOOLEAN, args, nullptr);
            break;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            args = { subqueryValue->data,
                subqueryValue->isNull,
                subqueryValue->length,
                subqueryRowCount,
                probeValue->data,
                probeValue->isNull,
                probeValue->length,
                inArray,
                isNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_in_subquery", { baseType }, OMNI_BOOLEAN, args, nullptr);
            break;
        }
        default: {
            LogWarn("Unsupported data type in IN_SUBQUERY expr %d", baseType);
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }

    this->value = std::make_shared<CodeGenValue>(inArray, isNull);
}

void BatchExpressionCodeGen::Visit(const SwitchExpr &switchExpr)
{
    auto switchDataType = switchExpr.GetReturnTypeId();
    Expr *elseExpr = switchExpr.falseExpr;
    std::vector<std::pair<Expr *, Expr *>> whenClause = switchExpr.whenClause;
    const int size = whenClause.size();

    AllocaInst *finalValue = GetResultArray(switchDataType, this->batchCodegenContext->rowCnt);
    AllocaInst *finalNull = GetResultArray(OMNI_BOOLEAN, this->batchCodegenContext->rowCnt);

    std::vector<CodeGenValuePtr> conditions(size);
    std::vector<CodeGenValuePtr> results(size);
    for (int i = 0; i < size; ++i) {
        Expr *cond = whenClause[i].first;
        Expr *resExpr = whenClause[i].second;
        conditions[i] = VisitExpr(*cond);
        if (!conditions[i]->IsValidValue()) {
            this->value = CreateInvalidCodeGenValue();
            return;
        }
        results[i] = VisitExpr(*resExpr);
        if (!results[i]->IsValidValue()) {
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
    auto whenClauses = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size));
    auto whenBools = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size));
    auto resultValues = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size));
    auto resultNulls = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size));

    for (int i = 0; i < size; ++i) {
        Value *gep = builder->CreateGEP(llvmTypes->I64Type(), whenClauses, llvmTypes->CreateConstantInt(i),
            "when_value_address");
        builder->CreateStore(conditions[i]->data, gep);

        gep = builder->CreateGEP(llvmTypes->I64Type(), whenBools, llvmTypes->CreateConstantInt(i), "when_null_address");
        builder->CreateStore(conditions[i]->isNull, gep);

        gep = builder->CreateGEP(llvmTypes->I64Type(), resultValues, llvmTypes->CreateConstantInt(i),
            "result_value_address");
        builder->CreateStore(results[i]->data, gep);

        gep = builder->CreateGEP(llvmTypes->I64Type(), resultNulls, llvmTypes->CreateConstantInt(i),
            "result_null_address");
        builder->CreateStore(results[i]->isNull, gep);
    }

    auto evFalse = VisitExpr(*elseExpr);
    if (!evFalse->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    std::vector<Value *> args;
    switch (switchDataType) {
        case OMNI_INT:
        case OMNI_BOOLEAN:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_FLOAT:
        case OMNI_DOUBLE: {
            args = { llvmTypes->CreateConstantInt(size),
                whenClauses,
                whenBools,
                resultValues,
                resultNulls,
                evFalse->data,
                evFalse->isNull,
                finalValue,
                finalNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_switch", { switchDataType }, switchDataType, args, nullptr);
            this->value = std::make_shared<CodeGenValue>(finalValue, finalNull);
            return;
        }
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128: {
            auto returnDecimalValue = BuildDecimalValue(nullptr, *switchExpr.GetReturnType(), nullptr);
            args = { llvmTypes->CreateConstantInt(size),
                whenClauses,
                whenBools,
                resultValues,
                resultNulls,
                evFalse->data,
                evFalse->isNull,
                finalValue,
                finalNull,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_switch", { switchDataType }, switchDataType, args, nullptr);
            this->value = std::make_shared<DecimalValue>(finalValue, finalNull,
                const_cast<Value *>(returnDecimalValue->GetPrecision()),
                const_cast<Value *>(returnDecimalValue->GetScale()));
            return;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            auto resultLengths = GetResultArray(OMNI_LONG, llvmTypes->CreateConstantInt(size));
            for (int i = 0; i < size; ++i) {
                Value *gep = builder->CreateGEP(llvmTypes->I64Type(), resultLengths, llvmTypes->CreateConstantInt(i),
                    "result_length_address");
                builder->CreateStore(results[i]->length, gep);
            }
            AllocaInst *finalLength = GetResultArray(OMNI_INT, this->batchCodegenContext->rowCnt);
            args = { llvmTypes->CreateConstantInt(size),
                whenClauses,
                whenBools,
                resultValues,
                resultNulls,
                resultLengths,
                evFalse->data,
                evFalse->isNull,
                evFalse->length,
                finalValue,
                finalNull,
                finalLength,
                this->batchCodegenContext->rowCnt };
            CallExternFunction("batch_switch", { switchDataType }, switchDataType, args, nullptr);
            this->value = std::make_shared<CodeGenValue>(finalValue, finalNull, finalLength);
            return;
        }
        default: {
            LogWarn("Unsupported data type in SWITCH expr %d", switchDataType);
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
}

void BatchExpressionCodeGen::Visit(const ParamRefExpr &paramRefExpr) {

}

void BatchExpressionCodeGen::Visit(const LambdaExpr &lambdaExpr) {

}
void BatchExpressionCodeGen::BatchBinaryExprIntLongHelper(const omniruntime::expressions::BinaryExpr *binaryExpr,
    llvm::Value *left, llvm::Value *right, llvm::Value *leftIsNull, llvm::Value *rightIsNull)
{
    DataTypeId returnTypeId = binaryExpr->GetReturnTypeId();
    std::vector<omniruntime::type::DataTypeId> typeParams;
    if (binaryExpr->left->GetReturnTypeId() == OMNI_INT || binaryExpr->left->GetReturnTypeId() == OMNI_DATE32) {
        typeParams = { OMNI_INT, OMNI_INT };
    } else {
        typeParams = { OMNI_LONG, OMNI_LONG };
    }
    std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
    AllocaInst *logicalArrayPtr = nullptr;
    std::vector<Value *> logicalFuncParams;
    if (returnTypeId == OMNI_BOOLEAN) {
        logicalArrayPtr = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "LOGICAL_PTR");
        logicalFuncParams = { left, right, logicalArrayPtr, this->batchCodegenContext->rowCnt };
    }
    std::vector<Value *> arithFuncParams { left, right, this->batchCodegenContext->rowCnt };

    std::vector<Value *> nullFuncParams { leftIsNull, rightIsNull, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");

    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            CallExternFunction("batch_lessThan", typeParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_lt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GT:
            CallExternFunction("batch_greaterThan", typeParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_gt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::LTE:
            CallExternFunction("batch_lessThanEqual", typeParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_le");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GTE:
            CallExternFunction("batch_greaterThanEqual", typeParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_ge");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::EQ:
            CallExternFunction("batch_equal", typeParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_eq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::NEQ:
            CallExternFunction("batch_notEqual", typeParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_neq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::ADD:
            CallExternFunction("batch_add", typeParams, returnTypeId, arithFuncParams, nullptr, "arithmetic_add");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::SUB:
            CallExternFunction("batch_subtract", typeParams, returnTypeId, arithFuncParams, nullptr, "arithmetic_sub");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::MUL:
            CallExternFunction("batch_multiply", typeParams, returnTypeId, arithFuncParams, nullptr, "arithmetic_mul");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::DIV:
            arithFuncParams.push_back(leftIsNull);
            CallExternFunction("batch_divide", typeParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_div");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::MOD:
            arithFuncParams.push_back(leftIsNull);
            CallExternFunction("batch_modulus", typeParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_mod");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        default: {
            LogError("Unsupported int or long binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
}

void BatchExpressionCodeGen::BatchBinaryExprDoubleHelper(const omniruntime::expressions::BinaryExpr *binaryExpr,
    llvm::Value *left, llvm::Value *right, llvm::Value *leftIsNull, llvm::Value *rightIsNull)
{
    std::vector<omniruntime::type::DataTypeId> doubleParams { OMNI_DOUBLE, OMNI_DOUBLE };
    std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
    DataTypeId returnTypeId = binaryExpr->GetReturnTypeId();
    AllocaInst *logicalArrayPtr = nullptr;
    std::vector<Value *> logicalFuncParams;
    if (returnTypeId == OMNI_BOOLEAN) {
        logicalArrayPtr = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "LOGICAL_PTR");
        logicalFuncParams = { left, right, logicalArrayPtr, this->batchCodegenContext->rowCnt };
    }
    std::vector<Value *> arithFuncParams { left, right, this->batchCodegenContext->rowCnt };

    std::vector<Value *> nullFuncParams { leftIsNull, rightIsNull, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");

    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            CallExternFunction("batch_lessThan", doubleParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_lt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GT:
            CallExternFunction("batch_greaterThan", doubleParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_gt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::LTE:
            CallExternFunction("batch_lessThanEqual", doubleParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_le");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GTE:
            CallExternFunction("batch_greaterThanEqual", doubleParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_ge");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::EQ:
            CallExternFunction("batch_equal", doubleParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_eq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::NEQ:
            CallExternFunction("batch_notEqual", doubleParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_neq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::ADD:
            CallExternFunction("batch_add", doubleParams, OMNI_DOUBLE, arithFuncParams, nullptr, "arithmetic_add");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::SUB:
            CallExternFunction("batch_subtract", doubleParams, OMNI_DOUBLE, arithFuncParams, nullptr, "arithmetic_sub");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::MUL:
            CallExternFunction("batch_multiply", doubleParams, OMNI_DOUBLE, arithFuncParams, nullptr, "arithmetic_mul");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::DIV:
            CallExternFunction("batch_divide", doubleParams, OMNI_DOUBLE, arithFuncParams, nullptr, "arithmetic_div");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::MOD:
            CallExternFunction("batch_modulus", doubleParams, OMNI_DOUBLE, arithFuncParams, nullptr, "arithmetic_mod");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        default: {
            LogError("Unsupported double binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
}

void BatchExpressionCodeGen::BatchBinaryExprFloatHelper(const omniruntime::expressions::BinaryExpr *binaryExpr,
    llvm::Value *left, llvm::Value *right, llvm::Value *leftIsNull, llvm::Value *rightIsNull)
{
    std::vector<omniruntime::type::DataTypeId> floatParams { OMNI_FLOAT, OMNI_FLOAT };
    std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
    DataTypeId returnTypeId = binaryExpr->GetReturnTypeId();
    AllocaInst *logicalArrayPtr = nullptr;
    std::vector<Value *> logicalFuncParams;
    if (returnTypeId == OMNI_BOOLEAN) {
        logicalArrayPtr = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "LOGICAL_PTR");
        logicalFuncParams = { left, right, logicalArrayPtr, this->batchCodegenContext->rowCnt };
    }
    std::vector<Value *> arithFuncParams { left, right, this->batchCodegenContext->rowCnt };

    std::vector<Value *> nullFuncParams { leftIsNull, rightIsNull, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");

    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            CallExternFunction("batch_lessThan", floatParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_lt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GT:
            CallExternFunction("batch_greaterThan", floatParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_gt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::LTE:
            CallExternFunction("batch_lessThanEqual", floatParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_le");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GTE:
            CallExternFunction("batch_greaterThanEqual", floatParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_ge");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::EQ:
            CallExternFunction("batch_equal", floatParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_eq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::NEQ:
            CallExternFunction("batch_notEqual", floatParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_neq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::ADD:
            CallExternFunction("batch_add", floatParams, OMNI_FLOAT, arithFuncParams, nullptr, "arithmetic_add");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::SUB:
            CallExternFunction("batch_subtract", floatParams, OMNI_FLOAT, arithFuncParams, nullptr, "arithmetic_sub");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::MUL:
            CallExternFunction("batch_multiply", floatParams, OMNI_FLOAT, arithFuncParams, nullptr, "arithmetic_mul");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::DIV:
            CallExternFunction("batch_divide", floatParams, OMNI_FLOAT, arithFuncParams, nullptr, "arithmetic_div");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        case omniruntime::expressions::Operator::MOD:
            CallExternFunction("batch_modulus", floatParams, OMNI_FLOAT, arithFuncParams, nullptr, "arithmetic_mod");
            this->value = std::make_shared<CodeGenValue>(left, leftIsNull);
            return;
        default: {
            LogError("Unsupported float binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
}

void BatchExpressionCodeGen::BatchBinaryExprDecimalHelper(const omniruntime::expressions::BinaryExpr *binaryExpr,
    DecimalValue &left, DecimalValue &right, llvm::Value *leftIsNull, llvm::Value *rightIsNull)
{
    std::vector<DataTypeId> decimalParams { binaryExpr->left->GetReturnTypeId(), binaryExpr->right->GetReturnTypeId() };
    std::vector<DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
    DataTypeId returnTypeId = binaryExpr->GetReturnTypeId();
    std::shared_ptr<DecimalValue> returnDecimalValue = nullptr;
    AllocaInst *logicalArrayPtr = nullptr;
    AllocaInst *arithArrayPtr = nullptr;
    std::vector<Value *> logicalFuncParams;
    std::vector<Value *> arithFuncParams;

    if (returnTypeId == OMNI_BOOLEAN) {
        logicalArrayPtr = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "LOGICAL_PTR");
        logicalFuncParams = {
            left.data,       const_cast<Value *>(left.GetPrecision()),  const_cast<Value *>(left.GetScale()),
            right.data,      const_cast<Value *>(right.GetPrecision()), const_cast<Value *>(right.GetScale()),
            logicalArrayPtr, this->batchCodegenContext->rowCnt
        };
    } else if (TypeUtil::IsDecimalType(returnTypeId)) {
        returnDecimalValue = BuildDecimalValue(nullptr, *binaryExpr->GetReturnType(), nullptr);
        arithFuncParams = { left.data,
            const_cast<Value *>(left.GetPrecision()),
            const_cast<Value *>(left.GetScale()),
            right.data,
            const_cast<Value *>(right.GetPrecision()),
            const_cast<Value *>(right.GetScale()),
            const_cast<Value *>(returnDecimalValue->GetPrecision()),
            const_cast<Value *>(returnDecimalValue->GetScale()),
            this->batchCodegenContext->rowCnt };
        if (decimalParams == std::vector<DataTypeId> { OMNI_DECIMAL128, OMNI_DECIMAL128 } &&
            returnTypeId == OMNI_DECIMAL64) {
            arithArrayPtr = builder->CreateAlloca(llvmTypes->I64Type(), this->batchCodegenContext->rowCnt, "ARITH_PTR");
            arithFuncParams.insert(std::begin(arithFuncParams) + 6, arithArrayPtr);
        } else if (decimalParams == std::vector<DataTypeId> { OMNI_DECIMAL64, OMNI_DECIMAL64 } &&
            returnTypeId == OMNI_DECIMAL128) {
            arithArrayPtr =
                builder->CreateAlloca(llvmTypes->I128Type(), this->batchCodegenContext->rowCnt, "ARITH_PTR");
            arithFuncParams.insert(std::begin(arithFuncParams) + 6, arithArrayPtr);
        }
    }

    std::vector<Value *> nullFuncParams { leftIsNull, rightIsNull, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");
    // when val->isNull = true, val->data is a random number, may cause false exception.
    // so push leftIsNull into args.
    // for functions throwing exception, if leftIsNull == true, do nothing.
    if (!overflowConfig || overflowConfig->GetOverflowConfigId() != omniruntime::op::OVERFLOW_CONFIG_NULL) {
        arithFuncParams.insert(arithFuncParams.begin(), leftIsNull);
    }

    Value *falseValue = llvmTypes->CreateConstantBool(false);
    AllocaInst *overflowNull =
        builder->CreateAlloca(Type::getInt1Ty(*context), this->batchCodegenContext->rowCnt, "OVERFLOW_NULL_PTR");
    std::vector<Value *> funcArgs { overflowNull, falseValue, this->batchCodegenContext->rowCnt };
    std::vector<DataTypeId> paramsVec = { OMNI_BOOLEAN };
    CallExternFunction("batch_fill_null", paramsVec, OMNI_BOOLEAN, funcArgs, nullptr, "fill_null_array");
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            CallExternFunction("batch_lessThan", decimalParams, returnTypeId, logicalFuncParams, nullptr,
                "relational_lt");
            break;
        case omniruntime::expressions::Operator::GT:
            CallExternFunction("batch_greaterThan", decimalParams, returnTypeId, logicalFuncParams, nullptr,
                "relational_gt");
            break;
        case omniruntime::expressions::Operator::LTE:
            CallExternFunction("batch_lessThanEqual", decimalParams, returnTypeId, logicalFuncParams, nullptr,
                "relational_le");
            break;
        case omniruntime::expressions::Operator::GTE:
            CallExternFunction("batch_greaterThanEqual", decimalParams, returnTypeId, logicalFuncParams, nullptr,
                "relational_ge");
            break;
        case omniruntime::expressions::Operator::EQ:
            CallExternFunction("batch_equal", decimalParams, returnTypeId, logicalFuncParams, nullptr, "relational_eq");
            break;
        case omniruntime::expressions::Operator::NEQ:
            CallExternFunction("batch_notEqual", decimalParams, returnTypeId, logicalFuncParams, nullptr,
                "relational_neq");
            break;
        case omniruntime::expressions::Operator::ADD:
            CallExternFunction("batch_add", decimalParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_add", this->overflowConfig, overflowNull);
            break;
        case omniruntime::expressions::Operator::SUB:
            CallExternFunction("batch_subtract", decimalParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_sub", this->overflowConfig, overflowNull);
            break;
        case omniruntime::expressions::Operator::MUL:
            CallExternFunction("batch_multiply", decimalParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_mul", this->overflowConfig, overflowNull);
            break;
        case omniruntime::expressions::Operator::DIV:
            CallExternFunction("batch_divide", decimalParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_div", this->overflowConfig, overflowNull);
            break;
        case omniruntime::expressions::Operator::MOD:
            CallExternFunction("batch_modulus", decimalParams, returnTypeId, arithFuncParams,
                batchCodegenContext->executionContext, "arithmetic_mod", this->overflowConfig, overflowNull);
            break;
        default: {
            LogError("Unsupported decimal binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }

    if (TypeUtil::IsDecimalType(returnTypeId)) {
        std::vector<Value *> isAnyNullParams { leftIsNull, overflowNull, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, isAnyNullParams, nullptr, "either_null");
        llvm::Value *dataValue = nullptr;
        if (returnTypeId == omniruntime::type::OMNI_DECIMAL128) {
            if (decimalParams[0] == OMNI_DECIMAL128) {
                dataValue = left.data;
            } else if (decimalParams[1] == OMNI_DECIMAL128) {
                dataValue = right.data;
            } else {
                dataValue = arithArrayPtr;
            }
            this->value = std::make_shared<DecimalValue>(dataValue, leftIsNull,
                const_cast<Value *>(returnDecimalValue->GetPrecision()),
                const_cast<Value *>(returnDecimalValue->GetScale()));
        } else if (returnTypeId == omniruntime::type::OMNI_DECIMAL64) {
            if (decimalParams[0] == OMNI_DECIMAL64) {
                dataValue = left.data;
            } else if (decimalParams[1] == OMNI_DECIMAL64) {
                dataValue = right.data;
            } else {
                dataValue = arithArrayPtr;
            }
            this->value = std::make_shared<DecimalValue>(dataValue, leftIsNull,
                const_cast<Value *>(returnDecimalValue->GetPrecision()),
                const_cast<Value *>(returnDecimalValue->GetScale()));
        }
    } else {
        this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
    }
}

void BatchExpressionCodeGen::BatchBinaryExprStringHelper(const omniruntime::expressions::BinaryExpr *binaryExpr,
    llvm::Value *left, llvm::Value *leftLen, llvm::Value *right, llvm::Value *rightLen, llvm::Value *leftIsNull,
    llvm::Value *rightIsNull)
{
    std::vector<omniruntime::type::DataTypeId> strParams { OMNI_VARCHAR, OMNI_VARCHAR };
    std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
    auto logicalArrayPtr = builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "LOGICAL_PTR");
    std::vector<Value *> logicalFuncParams { left,     leftLen,         right,
        rightLen, logicalArrayPtr, this->batchCodegenContext->rowCnt };

    std::vector<Value *> nullFuncParams { leftIsNull, rightIsNull, this->batchCodegenContext->rowCnt };
    CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");

    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            CallExternFunction("batch_lessThan", strParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_lt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GT:
            CallExternFunction("batch_greaterThan", strParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_gt");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::LTE:
            CallExternFunction("batch_lessThanEqual", strParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_le");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::GTE:
            CallExternFunction("batch_greaterThanEqual", strParams, OMNI_BOOLEAN, logicalFuncParams, nullptr,
                "relational_ge");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::EQ:
            CallExternFunction("batch_equal", strParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_eq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        case omniruntime::expressions::Operator::NEQ:
            CallExternFunction("batch_notEqual", strParams, OMNI_BOOLEAN, logicalFuncParams, nullptr, "relational_neq");
            this->value = std::make_shared<CodeGenValue>(logicalArrayPtr, leftIsNull);
            return;
        default: {
            LogError("Unsupported string binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            this->value = CreateInvalidCodeGenValue();
            return;
        }
    }
}

void BatchExpressionCodeGen::BatchVisitBetweenExprHelper(BetweenExpr &bExpr, const std::shared_ptr<CodeGenValue> &val,
    const std::shared_ptr<CodeGenValue> &lowerVal, const std::shared_ptr<CodeGenValue> &upperVal,
    std::pair<AllocaInst **, AllocaInst **> cmpPair)
{
    auto cmpLeft = cmpPair.first;
    auto cmpRight = cmpPair.second;
    std::vector<omniruntime::type::DataTypeId> params;
    if (TypeUtil::IsStringType(bExpr.value->GetReturnTypeId())) {
        params = { OMNI_VARCHAR, OMNI_VARCHAR };
    } else if (bExpr.value->GetReturnTypeId() == type::OMNI_DATE32) {
        params = { OMNI_INT, OMNI_INT };
    } else if (bExpr.value->GetReturnTypeId() == type::OMNI_TIMESTAMP) {
        params = { OMNI_LONG, OMNI_LONG };
    } else {
        params = { bExpr.value->GetReturnTypeId(), bExpr.value->GetReturnTypeId() };
    }
    std::vector<Value *> logicalFuncParams1;
    std::vector<Value *> logicalFuncParams2;
    bool isSupportedType = false;

    if (bExpr.value->GetReturnTypeId() == OMNI_INT || bExpr.value->GetReturnTypeId() == OMNI_LONG ||
        bExpr.value->GetReturnTypeId() == OMNI_DATE32 || bExpr.value->GetReturnTypeId() == OMNI_DOUBLE ||
        bExpr.value->GetReturnTypeId() == type::OMNI_TIMESTAMP) {
        logicalFuncParams1 = { lowerVal->data, val->data, *cmpLeft, this->batchCodegenContext->rowCnt };
        logicalFuncParams2 = { val->data, upperVal->data, *cmpRight, this->batchCodegenContext->rowCnt };
        isSupportedType = true;
    } else if (TypeUtil::IsStringType(bExpr.value->GetReturnTypeId())) {
        logicalFuncParams1 = { lowerVal->data, lowerVal->length, val->data,
            val->length,    *cmpLeft,         this->batchCodegenContext->rowCnt };
        logicalFuncParams2 = { val->data,        val->length, upperVal->data,
            upperVal->length, *cmpRight,   this->batchCodegenContext->rowCnt };
        isSupportedType = true;
    } else if (TypeUtil::IsDecimalType(bExpr.value->GetReturnTypeId())) {
        logicalFuncParams1 = { lowerVal->data,
            const_cast<Value *>(static_cast<DecimalValue &>(*lowerVal).GetPrecision()),
            const_cast<Value *>(static_cast<DecimalValue &>(*lowerVal).GetScale()),
            val->data,
            const_cast<Value *>(static_cast<DecimalValue &>(*val).GetPrecision()),
            const_cast<Value *>(static_cast<DecimalValue &>(*val).GetScale()),
            *cmpLeft,
            this->batchCodegenContext->rowCnt };
        logicalFuncParams2 = { val->data,
            const_cast<Value *>(static_cast<DecimalValue &>(*val).GetPrecision()),
            const_cast<Value *>(static_cast<DecimalValue &>(*val).GetScale()),
            upperVal->data,
            const_cast<Value *>(static_cast<DecimalValue &>(*upperVal).GetPrecision()),
            const_cast<Value *>(static_cast<DecimalValue &>(*upperVal).GetScale()),
            *cmpRight,
            this->batchCodegenContext->rowCnt };
        isSupportedType = true;
    }
    if (isSupportedType) {
        CallExternFunction("batch_lessThanEqual", params, OMNI_BOOLEAN, logicalFuncParams1, nullptr, "relational_le");
        CallExternFunction("batch_lessThanEqual", params, OMNI_BOOLEAN, logicalFuncParams2, nullptr, "relational_le");

        std::vector<omniruntime::type::DataTypeId> boolParams { OMNI_BOOLEAN, OMNI_BOOLEAN };
        std::vector<Value *> nullFuncParams { lowerVal->isNull, val->isNull, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");
        nullFuncParams = { lowerVal->isNull, upperVal->isNull, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_or", boolParams, OMNI_BOOLEAN, nullFuncParams, nullptr, "either_null");

        std::vector<Value *> betweenFuncParams = { *cmpLeft, *cmpRight, this->batchCodegenContext->rowCnt };
        CallExternFunction("batch_and", boolParams, OMNI_BOOLEAN, betweenFuncParams, nullptr, "between_pass");
        this->value = std::make_shared<CodeGenValue>(*cmpLeft, lowerVal->isNull);
        return;
    }

    LogError("Unsupported data type for BETWEEN expr %d", bExpr.value->GetReturnTypeId());
    this->value = CreateInvalidCodeGenValue();
}

Value *BatchExpressionCodeGen::PushAndGetNullFlagArray(const FuncExpr &fExpr, std::vector<llvm::Value *> &argVals,
    Value *nullFlagArray, bool needAdd)
{
    if (fExpr.function->GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL) {
        AllocaInst *isNullArrPtr =
            builder->CreateAlloca(llvmTypes->I1Type(), this->batchCodegenContext->rowCnt, "IS_RET_NULL_PTR");
        CallExternFunction("batch_fill_null", { OMNI_BOOLEAN }, OMNI_BOOLEAN,
            { isNullArrPtr, llvmTypes->CreateConstantBool(false), this->batchCodegenContext->rowCnt }, nullptr,
            "fill_ret_null_array");
        argVals.push_back(isNullArrPtr);
        return isNullArrPtr;
    }
    if (needAdd) {
        argVals.push_back(nullFlagArray);
    }
    return nullFlagArray;
}
}
