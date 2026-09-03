/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression code generator
 */
#include "expression_codegen.h"

#include <chrono>
#include <utility>
#include <memory>
#include <vector>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>

#include "expr_info_extractor.h"
#include "codegen_context.h"
#include "function.h"

namespace omniruntime::codegen {
using namespace llvm;
using namespace llvm::orc;
using namespace omniruntime;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace std;

namespace {
const int INT32_VALUE = 32;
const int INT64_VALUE = 64;
const int EXPRFUNC_OUT_LENGTH_ARG_INDEX = 1;
const int EXPRFUNC_OUT_NULL_INDEX = 3;
}

ExpressionCodeGen::ExpressionCodeGen(std::string name, const Expr &cpExpr, op::OverflowConfig *overflowConfig)
    : CodegenBase(name, cpExpr, overflowConfig)
{}

ExpressionCodeGen::~ExpressionCodeGen()
{
    if (rt) {
        eoe(rt->remove());
    }
}

bool ExpressionCodeGen::InitializeCodegenContext(iterator_range<llvm::Function::arg_iterator> args)
{
    this->codegenContext = std::make_unique<CodegenContext>();
    for (auto &arg : args) {
        auto argName = arg.getName().str();
        if (argName == "data") {
            codegenContext->data = &arg;
        } else if (argName == "nullBitmap") {
            codegenContext->nullBitmap = &arg;
        } else if (argName == "offsets") {
            codegenContext->offsets = &arg;
        } else if (argName == "rowIdx") {
            codegenContext->rowIdx = &arg;
        } else if (argName == "dataLength" || argName == "isNullPtr") {
            continue;
        } else if (argName == "executionContext") {
            codegenContext->executionContext = &arg;
        } else if (argName == "dictionaryVectors") {
            codegenContext->dictionaryVectors = &arg;
        } else if (argName.find("column_") == 0 || argName.find("dic_") == 0 || argName.find("bitmap_") == 0 ||
            argName.find("offset_") == 0) {
            continue;
        } else {
            LogWarn("Invalid argument %s", argName.c_str());
            return false;
        }
    }

    codegenContext->print = modulePtr->getOrInsertFunction("printf",
        FunctionType::get(IntegerType::getInt32Ty(*context), PointerType::get(Type::getInt8Ty(*context), 0), true));

    return true;
}

llvm::Function *ExpressionCodeGen::CreateFunction(const DataTypes &inputDataTypes)
{
    exprFunc = make_shared<ExprFunction>(funcName, *expr, *this, inputDataTypes);
    func = exprFunc->GetFunction();

    // Fill the function body
    BasicBlock *body = BasicBlock::Create(*context, "CREATED_FUNC_BODY", func);
    builder->SetInsertPoint(body);

    if (!InitializeCodegenContext(func->args())) {
        return nullptr;
    }

    auto result = VisitExpr(*expr);
    if (result->data == nullptr) {
        return nullptr;
    }

    // Update final string length of output
    if (result->length != nullptr) {
        Argument *outputLength = func->getArg(EXPRFUNC_OUT_LENGTH_ARG_INDEX);
        Value *lengthGep = builder->CreateGEP(llvmTypes->I32Type(), outputLength, llvmTypes->CreateConstantInt(0),
            "OUTPUT_LENGTH_ADDRESS");
        builder->CreateStore(result->length, lengthGep);
    }

    // Update final isNull of output
    builder->CreateStore(result->isNull, func->getArg(EXPRFUNC_OUT_NULL_INDEX));

    if (expr->GetReturnTypeId() == DataTypeId::OMNI_VARCHAR) {
        result->data = builder->CreatePtrToInt(result->data, llvmTypes->I64Type());
    }
    builder->CreateRet(result->data);
    verifyFunction(*func);
    return func;
}

CodeGenValuePtr ExpressionCodeGen::VisitExpr(const omniruntime::expressions::Expr &e)
{
    e.Accept(*this);
    return this->value;
}

void ExpressionCodeGen::Visit(const LiteralExpr &lExpr)
{
    this->value.reset(LiteralExprConstantHelper(lExpr));
}

void ExpressionCodeGen::Visit(const FieldExpr &fExpr)
{
    Value *rowIdx = this->codegenContext->rowIdx;
    Value *length = nullptr;

    // Get dictionary address of this column
    Value *dictionaryVectorPtr = exprFunc->GetDicArgument(fExpr.colVal);
    Type *dataType = llvmTypes->ToLLVMType(fExpr.GetReturnTypeId());
    auto condition = builder->CreateIsNotNull(dictionaryVectorPtr);

    BasicBlock *trueBlock = BasicBlock::Create(*context, "DICTIONARY_NOT_NULL", func);
    BasicBlock *falseBlock = BasicBlock::Create(*context, "DICTIONARY_IS_NULL");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "ifcont");

    builder->CreateCondBr(condition, trueBlock, falseBlock);

    // If dictionary vector is present, call DictionaryVector methods
    // to get encoded values and length if varchar type
    builder->SetInsertPoint(trueBlock);

    AllocaInst *lengthAllocaInst = nullptr;
    Value *dictionaryValue =
        this->GetDictionaryVectorValue(*(fExpr.GetReturnType()), rowIdx, dictionaryVectorPtr, lengthAllocaInst);
    if (dictionaryValue == nullptr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    Value *dictionaryLength = nullptr;
    if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        dictionaryLength = builder->CreateLoad(llvmTypes->I32Type(), lengthAllocaInst, "varchar_length");
    }

    builder->CreateBr(mergeBlock);
    trueBlock = builder->GetInsertBlock();
    func->getBasicBlockList().push_back(falseBlock);

    // If dictionary vector is not present, get vector values
    // using valuesAddress and length using offsets if varchar type
    builder->SetInsertPoint(falseBlock);
    // Load the address value.

    // Get data address of this column
    Value *columnPtr = exprFunc->GetColumnArgument(fExpr.colVal);
    Value *dataValue = nullptr;
    if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        Value *offsetPtr = exprFunc->GetOffsetArgument(fExpr.colVal);
        auto colOffsetGEP = builder->CreateGEP(llvmTypes->I32Type(), offsetPtr, rowIdx);
        Value *startOffset = builder->CreateLoad(llvmTypes->I32Type(), colOffsetGEP);
        colOffsetGEP = builder->CreateGEP(llvmTypes->I32Type(), offsetPtr,
            builder->CreateAdd(rowIdx, llvmTypes->CreateConstantInt(1)));
        Value *endOffset = builder->CreateLoad(llvmTypes->I32Type(), colOffsetGEP);
        // Get length for varchar
        length = builder->CreateSub(endOffset, startOffset);
        // Find the address of the row to be processed.
        dataValue = builder->CreateGEP(llvmTypes->I8Type(), columnPtr, startOffset);
    } else {
        // Find the address of the row to be processed.
        auto rowValuePtr = builder->CreateGEP(dataType, columnPtr, rowIdx);
        // Value to be processed.
        dataValue = builder->CreateLoad(dataType, rowValuePtr);
    }

    builder->CreateBr(mergeBlock);
    falseBlock = builder->GetInsertBlock();

    // Get merged data value and length
    int32_t numReservedValues = 2;
    func->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);

    PHINode *phiValue = builder->CreatePHI(dataType, numReservedValues, "iftmp");
    phiValue->addIncoming(dictionaryValue, trueBlock);
    phiValue->addIncoming(dataValue, falseBlock);

    // Length is only valid for varchar type
    PHINode *phiLength = nullptr;
    if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        phiLength = builder->CreatePHI(llvmTypes->I32Type(), numReservedValues, "length");
        phiLength->addIncoming(dictionaryLength, trueBlock);
        phiLength->addIncoming(length, falseBlock);
    }

    FunctionSignature isBitNullFuncSignature = FunctionSignature("WrapIsBitNull", { OMNI_INT }, OMNI_BOOLEAN);
    llvm::Function *isBitNullFunc =
        modulePtr->getFunction(FunctionRegistry::LookupFunction(&isBitNullFuncSignature)->GetId());
    // Get bitmap address of this column
    Value *bitmapPtr = exprFunc->GetNullArgument(fExpr.colVal);
    auto isNullRet = builder->CreateCall(isBitNullFunc, { bitmapPtr, rowIdx }, "wrap_is_bit_null");
    InlineFunctionInfo inlineIsNullFuncInfo;
    InlineFunction(*isNullRet, inlineIsNullFuncInfo);

    if (TypeUtil::IsDecimalType(fExpr.GetReturnTypeId())) {
        Value *precision =
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision());
        Value *scale =
            llvmTypes->CreateConstantInt(static_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale());
        this->value = make_shared<DecimalValue>(phiValue, isNullRet, precision, scale);
    } else {
        this->value = make_shared<CodeGenValue>(phiValue, isNullRet, phiLength);
    }
}

void ExpressionCodeGen::Visit(const BinaryExpr &binaryExpr)
{
    auto *bExpr = const_cast<BinaryExpr *>(&binaryExpr);

    CodeGenValuePtr left = VisitExpr(*(bExpr->left));
    if (!left->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    CodeGenValuePtr right = VisitExpr(*(bExpr->right));
    if (!right->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    if (bExpr->op == omniruntime::expressions::Operator::AND) {
        this->value = make_shared<CodeGenValue>(builder->CreateAnd(left->data, right->data, "logical_and"),
            builder->CreateOr(builder->CreateAnd(left->isNull, right->isNull), builder->CreateOr(
            builder->CreateAnd(left->isNull, right->data), builder->CreateAnd(right->isNull, left->data))));
        return;
    }
    if (bExpr->op == omniruntime::expressions::Operator::OR) {
        this->value = make_shared<CodeGenValue>(builder->CreateOr(left->data, right->data, "logical_or"),
            builder->CreateOr(builder->CreateAnd(left->isNull, right->isNull),
            builder->CreateOr(builder->CreateAnd(left->isNull, builder->CreateNot(right->data)),
            builder->CreateAnd(right->isNull, builder->CreateNot(left->data)))));
        return;
    }
    if (bExpr->left->GetReturnTypeId() == OMNI_BYTE) {
        Value *nullFlag = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "null_flag");
        builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), nullFlag);
        this->value = make_shared<CodeGenValue>(
            this->BinaryExprByteHelper(bExpr, left->data, right->data, left->isNull, right->isNull, nullFlag),
            builder->CreateOr(builder->CreateOr(left->isNull, right->isNull),
                              builder->CreateLoad(llvmTypes->I1Type(), nullFlag)));
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_SHORT) {
        Value *nullFlag = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "null_flag");
        builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), nullFlag);
        this->value = make_shared<CodeGenValue>(
            this->BinaryExprShortHelper(bExpr, left->data, right->data, left->isNull, right->isNull, nullFlag),
            builder->CreateOr(builder->CreateOr(left->isNull, right->isNull),
                              builder->CreateLoad(llvmTypes->I1Type(), nullFlag)));
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_INT || bExpr->left->GetReturnTypeId() == OMNI_DATE32) {
        Value *nullFlag = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "null_flag");
        builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), nullFlag);
        this->value = make_shared<CodeGenValue>(
            this->BinaryExprIntHelper(bExpr, left->data, right->data, left->isNull, right->isNull, nullFlag),
            builder->CreateOr(builder->CreateOr(left->isNull, right->isNull),
                              builder->CreateLoad(llvmTypes->I1Type(), nullFlag)));
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_LONG || bExpr->left->GetReturnTypeId() == OMNI_TIMESTAMP) {
        Value *nullFlag = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "null_flag");
        builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), nullFlag);
        this->value = make_shared<CodeGenValue>(
            this->BinaryExprLongHelper(bExpr, left->data, right->data, left->isNull, right->isNull, nullFlag),
            builder->CreateOr(builder->CreateOr(left->isNull, right->isNull),
                              builder->CreateLoad(llvmTypes->I1Type(), nullFlag)));
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_DECIMAL64) {
        auto decimalLeft = dynamic_cast<DecimalValue &>(*left.get());
        auto decimalRight = dynamic_cast<DecimalValue &>(*right.get());
        if  (decimalLeft.GetScale() == decimalRight.GetScale() &&
                (bExpr->op == omniruntime::expressions::Operator::LT ||
                 bExpr->op == omniruntime::expressions::Operator::LTE ||
                 bExpr->op == omniruntime::expressions::Operator::GT ||
                 bExpr->op == omniruntime::expressions::Operator::GTE ||
                 bExpr->op == omniruntime::expressions::Operator::EQ ||
                 bExpr->op == omniruntime::expressions::Operator::NEQ)) {
            auto output = this->BinaryExprLongHelper(bExpr, left->data, right->data, left->isNull, right->isNull);
            this->value =
                BuildDecimalValue(output, *(bExpr->GetReturnType()), builder->CreateOr(left->isNull, right->isNull));
            return;
        }
        this->BinaryExprDecimal64Helper(bExpr, decimalLeft, decimalRight, left->isNull, right->isNull);
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_DOUBLE) {
        Value *nullFlag = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "null_flag");
        builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), nullFlag);
        this->value = make_shared<CodeGenValue>(
                this->BinaryExprDoubleHelper(bExpr, left->data, right->data, left->isNull, right->isNull, nullFlag),
                builder->CreateOr(builder->CreateOr(left->isNull, right->isNull),
                                  builder->CreateLoad(llvmTypes->I1Type(), nullFlag)));
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_FLOAT) {
        Value *nullFlag = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "null_flag");
        builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), nullFlag);
        this->value = make_shared<CodeGenValue>(
                this->BinaryExprFloatHelper(bExpr, left->data, right->data, left->isNull, right->isNull, nullFlag),
                builder->CreateOr(builder->CreateOr(left->isNull, right->isNull),
                                  builder->CreateLoad(llvmTypes->I1Type(), nullFlag)));
        return;
    } else if (TypeUtil::IsStringType(bExpr->left->GetReturnTypeId())) {
        this->value = make_shared<CodeGenValue>(this->BinaryExprStringHelper(bExpr, left->data, left->length,
            right->data, right->length, left->isNull, right->isNull),
            builder->CreateOr(left->isNull, right->isNull));
        return;
    } else if (bExpr->left->GetReturnTypeId() == OMNI_DECIMAL128) {
        this->BinaryExprDecimal128Helper(bExpr, dynamic_cast<DecimalValue &>(*left.get()),
            dynamic_cast<DecimalValue &>(*right.get()), left->isNull, right->isNull);
        return;
    }
    LogWarn("Unsupported binary operator %u", static_cast<uint32_t>(bExpr->op));
    this->value = CreateInvalidCodeGenValue();
}

void ExpressionCodeGen::Visit(const UnaryExpr &uExpr)
{
    auto val = VisitExpr(*(uExpr.exp));
    if (!val->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    switch (uExpr.op) {
        case omniruntime::expressions::Operator::NOT: {
            Value *notValue = builder->CreateNot(val->data, "logical_not");
            this->value = make_shared<CodeGenValue>(notValue, val->isNull);
            break;
        }
        default: {
            // Ignore the unary operator if it is invalid
            this->value = CreateInvalidCodeGenValue();
            break;
        }
    }
}

void ExpressionCodeGen::Visit(const SwitchExpr &switchExpr)
{
    Type *switchDataType = llvmTypes->VectorToLLVMType(*(switchExpr.GetReturnType()));
    Expr *elseExpr = switchExpr.falseExpr;
    std::vector<std::pair<Expr *, Expr *>> whenClause = switchExpr.whenClause;
    const size_t size = whenClause.size();

    std::vector<BasicBlock *> condBlockList;
    std::vector<BasicBlock *> trueBlockList;
    BasicBlock *falseBlock = BasicBlock::Create(*context, "FALSE_BLOCK");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "ifcont");
    int32_t numReservedValues = 2;

    AllocaInst *resultValuePtr = builder->CreateAlloca(switchDataType, numReservedValues, nullptr, "temp_result_value");
    AllocaInst *resultNullPtr =
        builder->CreateAlloca(Type::getInt1Ty(*context), numReservedValues, nullptr, "temp_result_null");
    AllocaInst *resultLengthPtr =
        builder->CreateAlloca(Type::getInt32Ty(*context), numReservedValues, nullptr, "temp_result_length");

    AllocaInst *resultPrecisionPtr =
        builder->CreateAlloca(Type::getInt32Ty(*context), numReservedValues, nullptr, "temp_result_precision");

    AllocaInst *resultScalePtr =
        builder->CreateAlloca(Type::getInt32Ty(*context), numReservedValues, nullptr, "temp_result_scale");

    condBlockList.push_back(BasicBlock::Create(*context, "Condition" + std::to_string(0), func));
    trueBlockList.push_back(BasicBlock::Create(*context, "TRUE_BLOCK" + std::to_string(0), func));

    for (size_t i = 1; i < size; i++) { // Generate block lists used in the next loop to evaluate conditions
        condBlockList.push_back(BasicBlock::Create(*context, "Condition" + std::to_string(i)));
        trueBlockList.push_back(BasicBlock::Create(*context, "TRUE_BLOCK" + std::to_string(i), func));
    }
    for (size_t i = 0; i < size; i++) { // Evaluate condition in the whenClause
        Expr *cond = whenClause[i].first;
        Expr *resExpr = whenClause[i].second;

        // If cond evaluates to true, control flow goes to trueBlock, save evTrue to temp value
        // Otherwise goes to next Block in the list and keeps evaluating next cond in the whenClause
        // If last cond evaluates to false, control flow goes to falseBlock and save evFalse to temp value
        if (i == 0) { // Create the entry of the block
            builder->CreateBr(condBlockList[i]);
        }
        if (i > 0) {
            func->getBasicBlockList().push_back(condBlockList[i]);
        }

        auto elseBranch = falseBlock;
        if (i < size - 1) {
            elseBranch = condBlockList[i + 1];
        }
        builder->SetInsertPoint(condBlockList[i]);
        CodeGenValuePtr evCond = VisitExpr(*cond);
        if (!evCond->IsValidValue()) {
            this->value = CreateInvalidCodeGenValue();
            return;
        }
        builder->CreateCondBr(builder->CreateAnd(builder->CreateNot(evCond->isNull), evCond->data), trueBlockList[i],
            elseBranch);

        builder->SetInsertPoint(trueBlockList[i]);
        auto evTrue = VisitExpr(*resExpr);
        if (!evTrue->IsValidValue()) {
            this->value = CreateInvalidCodeGenValue();
            return;
        }
        builder->CreateStore(evTrue->data, resultValuePtr);
        builder->CreateStore(evTrue->isNull, resultNullPtr);
        if (TypeUtil::IsStringType(switchExpr.GetReturnTypeId())) {
            builder->CreateStore(evTrue->length, resultLengthPtr);
        } else if (TypeUtil::IsDecimalType(switchExpr.GetReturnTypeId())) {
            builder->CreateStore(dynamic_cast<DecimalValue *>(evTrue.get())->GetPrecision(), resultPrecisionPtr);
            builder->CreateStore(dynamic_cast<DecimalValue *>(evTrue.get())->GetScale(), resultScalePtr);
        }
        builder->CreateBr(mergeBlock);
    }

    func->getBasicBlockList().push_back(falseBlock);
    builder->SetInsertPoint(falseBlock);
    auto evFalse = VisitExpr(*elseExpr);
    if (!evFalse->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    builder->CreateStore(evFalse->data, resultValuePtr);
    builder->CreateStore(evFalse->isNull, resultNullPtr);
    if (TypeUtil::IsStringType(switchExpr.GetReturnTypeId())) {
        builder->CreateStore(evFalse->length, resultLengthPtr);
    } else if (TypeUtil::IsDecimalType(switchExpr.GetReturnTypeId())) {
        builder->CreateStore(dynamic_cast<DecimalValue *>(evFalse.get())->GetPrecision(), resultPrecisionPtr);
        builder->CreateStore(dynamic_cast<DecimalValue *>(evFalse.get())->GetScale(), resultScalePtr);
    }
    builder->CreateBr(mergeBlock);

    func->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);
    if (TypeUtil::IsStringType(switchExpr.GetReturnTypeId())) {
        this->value = make_shared<CodeGenValue>(builder->CreateLoad(switchDataType, resultValuePtr),
            builder->CreateLoad(llvmTypes->I1Type(), resultNullPtr),
            builder->CreateLoad(llvmTypes->I32Type(), resultLengthPtr));
    } else if (TypeUtil::IsDecimalType(switchExpr.GetReturnTypeId())) {
        this->value = make_shared<DecimalValue>(builder->CreateLoad(switchDataType, resultValuePtr),
            builder->CreateLoad(llvmTypes->I1Type(), resultNullPtr),
            builder->CreateLoad(llvmTypes->I32Type(), resultPrecisionPtr),
            builder->CreateLoad(llvmTypes->I32Type(), resultScalePtr));
    } else {
        this->value = std::make_shared<CodeGenValue>(builder->CreateLoad(switchDataType, resultValuePtr),
            builder->CreateLoad(llvmTypes->I1Type(), resultNullPtr));
    }
}

void ExpressionCodeGen::Visit(const IfExpr &ifExpr)
{
    Expr *cond = ifExpr.condition;
    Expr *ifTrue = ifExpr.trueExpr;
    Expr *ifFalse = ifExpr.falseExpr;

    BasicBlock *trueBlock = BasicBlock::Create(*context, "TRUE_BLOCK", func);
    BasicBlock *falseBlock = BasicBlock::Create(*context, "FALSE_BLOCK");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "ifcont");

    CodeGenValuePtr evCond = VisitExpr(*cond);
    if (!evCond->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    // If cond evaluates to true, control flow goes to trueBlock, returning evTrue
    // Otherwise goes to falseBlock and returns evFalse
    builder->CreateCondBr(builder->CreateAnd(builder->CreateNot(evCond->isNull), evCond->data), trueBlock, falseBlock);
    builder->SetInsertPoint(trueBlock);
    auto evTrue = VisitExpr(*ifTrue);
    if (!evTrue->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    builder->CreateBr(mergeBlock);
    // Codegen of 'true' can change the current block, update trueBlock for the PHI.
    trueBlock = builder->GetInsertBlock();

    func->getBasicBlockList().push_back(falseBlock);
    builder->SetInsertPoint(falseBlock);
    auto evFalse = VisitExpr(*ifFalse);
    if (!evFalse->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    builder->CreateBr(mergeBlock);
    // Codegen of 'false' can change the current block, update falseBlock for the PHI.
    falseBlock = builder->GetInsertBlock();
    int32_t numReservedValues = 2;
    // Emit merge block.
    Type *phiType = llvmTypes->VectorToLLVMType(*(ifExpr.GetReturnType()));
    func->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);
    PHINode *pn = builder->CreatePHI(phiType, numReservedValues, "iftmp");
    PHINode *phiNull = builder->CreatePHI(evTrue->isNull->getType(), numReservedValues, "iftmpNull");

    pn->addIncoming(evTrue->data, trueBlock);
    pn->addIncoming(evFalse->data, falseBlock);
    phiNull->addIncoming(evTrue->isNull, trueBlock);
    phiNull->addIncoming(evFalse->isNull, falseBlock);

    PHINode *lengthPhi = nullptr;
    if (TypeUtil::IsStringType(ifExpr.GetReturnTypeId())) {
        lengthPhi = builder->CreatePHI(Type::getInt32Ty(*context), numReservedValues, "length");
        lengthPhi->addIncoming(evTrue->length, trueBlock);
        lengthPhi->addIncoming(evFalse->length, falseBlock);
    }

    PHINode *precisionPhi = nullptr;
    PHINode *scalePhi = nullptr;
    if (TypeUtil::IsDecimalType(ifExpr.GetReturnTypeId())) {
        precisionPhi = builder->CreatePHI(Type::getInt32Ty(*context), numReservedValues, "precision");
        auto evTruePrecision = (Value *)dynamic_cast<DecimalValue *>(evTrue.get())->GetPrecision();
        auto evFalsePrecision = (Value *)dynamic_cast<DecimalValue *>(evFalse.get())->GetPrecision();
        precisionPhi->addIncoming(evTruePrecision, trueBlock);
        precisionPhi->addIncoming(evFalsePrecision, falseBlock);

        scalePhi = builder->CreatePHI(Type::getInt32Ty(*context), numReservedValues, "scale");
        auto evTrueScale = (Value *)dynamic_cast<DecimalValue *>(evTrue.get())->GetScale();
        auto evFalseScale = (Value *)dynamic_cast<DecimalValue *>(evFalse.get())->GetScale();
        scalePhi->addIncoming(evTrueScale, trueBlock);
        scalePhi->addIncoming(evFalseScale, falseBlock);

        this->value = std::make_shared<DecimalValue>(pn, phiNull, precisionPhi, scalePhi);
        return;
    }

    this->value = std::make_shared<CodeGenValue>(pn, phiNull, lengthPhi);
}

void ExpressionCodeGen::Visit(const InExpr &inExpr)
{
    auto size = inExpr.arguments.size();
    CodeGenValuePtr argiValue;
    auto inArray = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "res");
    builder->CreateStore(llvmTypes->CreateConstantBool(false), inArray);
    auto isNull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "res_null");
    builder->CreateStore(llvmTypes->CreateConstantBool(false), isNull);
    auto hasnull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "has_null");
    builder->CreateStore(llvmTypes->CreateConstantBool(false), hasnull);
    Type *retType = llvmTypes->ToLLVMType(inExpr.GetReturnTypeId());

    std::vector<BasicBlock *> condBlockList;
    BasicBlock *trueBlock = BasicBlock::Create(*context, "TRUE_BLOCK");
    BasicBlock *falseBlock = BasicBlock::Create(*context, "FALSE_BLOCK");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "MERGE_BLOCK");

    condBlockList.push_back(nullptr);
    for (size_t i = 1; i < size; i++) {
        condBlockList.push_back(BasicBlock::Create(*context, "Condition" + std::to_string(i)));
    }

    Expr *toCompare = inExpr.arguments[0];
    auto valueToCompare = VisitExpr(*toCompare);
    builder->CreateStore(valueToCompare->isNull, isNull);
    if (!valueToCompare->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    for (size_t i = 1; i < size; i++) {
        if (AreInvalidDataTypes(toCompare->GetReturnTypeId(), inExpr.arguments[i]->GetReturnTypeId())) {
            LogError("Arg 1 and arg %d have different data types", i + 1);
            this->value = CreateInvalidCodeGenValue();
            return;
        }

        if (i == 1) {
            builder->CreateBr(condBlockList[i]);
        }
        auto elseBranch = falseBlock;
        if (i < size - 1) {
            elseBranch = condBlockList[i + 1];
        }

        func->getBasicBlockList().push_back(condBlockList[i]);
        builder->SetInsertPoint(condBlockList[i]);

        Value *tmpCmpData = llvmTypes->CreateConstantBool(false);
        Value *tmpCmpNull = llvmTypes->CreateConstantBool(false);

        argiValue = VisitExpr(*(inExpr.arguments[i]));
        if (!argiValue->IsValidValue()) {
            this->value = CreateInvalidCodeGenValue();
            return;
        }

        switch (inExpr.arguments[0]->GetReturnTypeId()) {
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_TIMESTAMP:
            case OMNI_LONG: {
                InExprIntegerHelper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull);
                break;
            }
            case OMNI_DECIMAL64: {
                DecimalValue &left = static_cast<DecimalValue &>(*valueToCompare);
                DecimalValue &right = static_cast<DecimalValue &>(*argiValue);
                if (left.GetScale() == right.GetScale()) {
                    InExprIntegerHelper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull);
                } else {
                    InExprDecimal64Helper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull, retType);
                }
                break;
            }
            case OMNI_DOUBLE: {
                InExprDoubleHelper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull);
                break;
            }
            case OMNI_FLOAT: {
                InExprFloatHelper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull);
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                InExprStringHelper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull);
                break;
            }
            case OMNI_DECIMAL128: {
                InExprDecimal128Helper(valueToCompare, argiValue, tmpCmpData, tmpCmpNull, retType);
                break;
            }
            default: {
                LogWarn("Unsupported data type in IN expr %d", inExpr.arguments[0]->GetReturnTypeId());
                this->value = CreateInvalidCodeGenValue();
                return;
            }
        }
        builder->CreateStore(builder->CreateOr(argiValue->isNull, builder->CreateLoad(llvmTypes->I1Type(), hasnull)),
            hasnull);
        builder->CreateCondBr(builder->CreateAnd(builder->CreateNot(tmpCmpNull), tmpCmpData), trueBlock, elseBranch);
    }

    func->getBasicBlockList().push_back(trueBlock);
    builder->SetInsertPoint(trueBlock);
    builder->CreateStore(llvmTypes->CreateConstantBool(true), inArray);
    builder->CreateBr(mergeBlock);

    func->getBasicBlockList().push_back(falseBlock);
    builder->SetInsertPoint(falseBlock);
    builder->CreateStore(builder->CreateOr(builder->CreateLoad(llvmTypes->I1Type(), hasnull),
        builder->CreateLoad(llvmTypes->I1Type(), isNull)), isNull);
    builder->CreateBr(mergeBlock);

    func->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);
    this->value = std::make_shared<CodeGenValue>(builder->CreateLoad(llvmTypes->I1Type(), inArray),
        builder->CreateLoad(llvmTypes->I1Type(), isNull));
}

void ExpressionCodeGen::Visit(const InSubqueryExpr &inSubExpr)
{
    // For row-level codegen, InSubqueryExpr is more complex because the subquery result
    // is only known at runtime. We generate a loop that iterates through the subquery result.

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

    // Result allocation
    auto inArray = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "in_subquery_res");
    builder->CreateStore(llvmTypes->CreateConstantBool(false), inArray);
    auto isNull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "in_subquery_null");

    // If probe value is NULL, result is NULL
    builder->CreateStore(probeValue->isNull, isNull);

    // Create loop for iterating through subquery results
    // subqueryRowCount is available from the execution context
    Value *subqueryRowCount = this->codegenContext->subqueryRowCount;

    // Loop variables
    auto loopCounter = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "loop_counter");
    builder->CreateStore(llvmTypes->CreateConstantInt(0), loopCounter);

    auto hasNull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "has_null");
    builder->CreateStore(llvmTypes->CreateConstantBool(false), hasNull);

    // Create basic blocks for the loop
    BasicBlock *loopCond = BasicBlock::Create(*context, "loop_cond");
    BasicBlock *loopBody = BasicBlock::Create(*context, "loop_body");
    BasicBlock *loopExit = BasicBlock::Create(*context, "loop_exit");

    // Branch to loop condition
    builder->CreateBr(loopCond);

    // Loop condition: check if counter < subqueryRowCount and not yet matched
    func->getBasicBlockList().push_back(loopCond);
    builder->SetInsertPoint(loopCond);
    Value *counterVal = builder->CreateLoad(llvmTypes->I32Type(), loopCounter);
    Value *condLess = builder->CreateICmpSLT(counterVal, subqueryRowCount);
    Value *notMatched = builder->CreateNot(builder->CreateLoad(llvmTypes->I1Type(), inArray));
    Value *loopCondVal = builder->CreateAnd(condLess, notMatched);
    builder->CreateCondBr(loopCondVal, loopBody, loopExit);

    // Loop body: compare probe value with subquery value at current index
    func->getBasicBlockList().push_back(loopBody);
    builder->SetInsertPoint(loopBody);

    // Get subquery value at current index
    Value *idx = builder->CreateLoad(llvmTypes->I32Type(), loopCounter);
    Value *subValPtr = builder->CreateGEP(subqueryValue->data->getType()->getPointerElementType(),
        subqueryValue->data, idx);
    Value *subVal = builder->CreateLoad(subValPtr->getType()->getPointerElementType(), subValPtr);

    Value *subNullPtr = builder->CreateGEP(subqueryValue->isNull->getType()->getPointerElementType(),
        subqueryValue->isNull, idx);
    Value *subNull = builder->CreateLoad(llvmTypes->I1Type(), subNullPtr);

    // Track if we've seen any NULL in subquery
    Value *prevHasNull = builder->CreateLoad(llvmTypes->I1Type(), hasNull);
    Value *newHasNull = builder->CreateOr(prevHasNull, subNull);
    builder->CreateStore(newHasNull, hasNull);

    // Compare values (only if subquery value is not NULL)
    Value *compareValid = builder->CreateAnd(builder->CreateNot(subNull), builder->CreateNot(probeValue->isNull));

    // Type-specific comparison
    Value *isEqual = nullptr;
    switch (baseType) {
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_BOOLEAN:
            isEqual = builder->CreateICmpEQ(probeValue->data, subVal);
            break;
        case OMNI_DOUBLE:
        case OMNI_FLOAT:
            isEqual = builder->CreateFCmpOEQ(probeValue->data, subVal);
            break;
        default:
            // For string and decimal types, we would need more complex comparison
            // For now, mark as unsupported
            LogWarn("Unsupported data type in IN_SUBQUERY expr %d", baseType);
            this->value = CreateInvalidCodeGenValue();
            return;
    }

    Value *matchFound = builder->CreateAnd(compareValid, isEqual);

    // If match found, set result to TRUE
    BasicBlock *matchBlock = BasicBlock::Create(*context, "match_block");
    BasicBlock *noMatchBlock = BasicBlock::Create(*context, "no_match_block");
    func->getBasicBlockList().push_back(matchBlock);
    func->getBasicBlockList().push_back(noMatchBlock);

    builder->CreateCondBr(matchFound, matchBlock, noMatchBlock);

    builder->SetInsertPoint(matchBlock);
    builder->CreateStore(llvmTypes->CreateConstantBool(true), inArray);
    builder->CreateBr(loopExit);

    builder->SetInsertPoint(noMatchBlock);
    // Increment counter
    Value *newCounter = builder->CreateAdd(counterVal, llvmTypes->CreateConstantInt(1));
    builder->CreateStore(newCounter, loopCounter);
    builder->CreateBr(loopCond);

    // Loop exit: determine final result
    func->getBasicBlockList().push_back(loopExit);
    builder->SetInsertPoint(loopExit);

    // If not matched and (probe is NULL or subquery has NULL), result is NULL
    Value *matched = builder->CreateLoad(llvmTypes->I1Type(), inArray);
    Value *notMatchedFinal = builder->CreateNot(matched);
    Value *probeIsNull = probeValue->isNull;
    Value *subHasNull = builder->CreateLoad(llvmTypes->I1Type(), hasNull);
    Value *shouldBeNull = builder->CreateAnd(notMatchedFinal,
        builder->CreateOr(probeIsNull, subHasNull));
    builder->CreateStore(shouldBeNull, isNull);

    this->value = std::make_shared<CodeGenValue>(
        builder->CreateLoad(llvmTypes->I1Type(), inArray),
        builder->CreateLoad(llvmTypes->I1Type(), isNull));
}

void ExpressionCodeGen::Visit(const BetweenExpr &btExpr)
{
    auto bExpr = const_cast<BetweenExpr *>(&btExpr);

    auto val = VisitExpr(*(bExpr->value));
    if (!val->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    DataTypeId valueTypeId = bExpr->value->GetReturnTypeId();
    if (AreInvalidDataTypes(valueTypeId, bExpr->lowerBound->GetReturnTypeId()) &&
        AreInvalidDataTypes(valueTypeId, bExpr->upperBound->GetReturnTypeId())) {
        LogError("Value, lower bound, and upper bound must have the same type");
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    auto valNull = val->isNull;
    auto lowerVal = VisitExpr(*(bExpr->lowerBound));
    if (!lowerVal->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    auto lowerValNull = lowerVal->isNull;
    auto upperVal = VisitExpr(*(bExpr->upperBound));
    if (!upperVal->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    auto upperValNull = upperVal->isNull;
    auto isAnyNull = builder->CreateOr(builder->CreateOr(valNull, lowerValNull), upperValNull);
    auto isNeitherNull = builder->CreateNot(isAnyNull);
    Value *cmpLeft, *cmpRight;
    std::pair<llvm::Value **, llvm::Value **> cmpPair = std::make_pair(&cmpLeft, &cmpRight);
    bool supportedType = VisitBetweenExprHelper(*bExpr, val, lowerVal, upperVal, cmpPair);
    if (supportedType) {
        std::vector<Value *> andValues;
        andValues.push_back(isNeitherNull);
        andValues.push_back(cmpLeft);
        andValues.push_back(cmpRight);
        Value *result = builder->CreateAnd(andValues);
        this->value = make_shared<CodeGenValue>(result, isAnyNull);
        return;
    }

    LogError("Unsupported data type for between %d", valueTypeId);
    this->value = CreateInvalidCodeGenValue();
}

void ExpressionCodeGen::Visit(const CoalesceExpr &cExpr)
{
    Expr *value1Expr = cExpr.value1;
    Expr *value2Expr = cExpr.value2;
    CodeGenValuePtr value1 = VisitExpr(*value1Expr);
    if (!value1->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    BasicBlock *isNullBlock = BasicBlock::Create(*context, "coalesceVal1IsNull", func);
    BasicBlock *isNotNullBlock = BasicBlock::Create(*context, "coalesceVal1IsNotNull");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "coalesceCont");

    // If cond evaluates to true, control flow goes to trueBlock, returning evTrue
    // Otherwise goes to falseBlock and returns evFalse
    builder->CreateCondBr(value1->isNull, isNullBlock, isNotNullBlock);

    builder->SetInsertPoint(isNullBlock);
    auto value2 = VisitExpr(*value2Expr);
    if (!value2->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }

    builder->CreateBr(mergeBlock);
    // Codegen of 'true' can change the current block, update trueBlock for the PHI.
    isNullBlock = builder->GetInsertBlock();

    func->getBasicBlockList().push_back(isNotNullBlock);
    builder->SetInsertPoint(isNotNullBlock);

    builder->CreateBr(mergeBlock);
    // Codegen of 'false' can change the current block, update falseBlock for the PHI.
    isNotNullBlock = builder->GetInsertBlock();
    int32_t numReservedValues = 2;

    // Emit merge block.
    Type *phiType = llvmTypes->VectorToLLVMType(*(cExpr.GetReturnType()));
    func->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);
    PHINode *pn = builder->CreatePHI(phiType, numReservedValues, "iftmp");
    PHINode *pnNull = builder->CreatePHI(value1->isNull->getType(), numReservedValues, "iftmp");

    pn->addIncoming(value1->data, isNotNullBlock);
    pn->addIncoming(value2->data, isNullBlock);
    pnNull->addIncoming(value1->isNull, isNotNullBlock);
    pnNull->addIncoming(value2->isNull, isNullBlock);

    PHINode *lengthPhi = nullptr;
    if (TypeUtil::IsStringType(cExpr.GetReturnTypeId())) {
        lengthPhi = builder->CreatePHI(Type::getInt32Ty(*context), numReservedValues, "length");
        lengthPhi->addIncoming(value1->length, isNotNullBlock);
        lengthPhi->addIncoming(value2->length, isNullBlock);
    }

    if (TypeUtil::IsDecimalType(cExpr.GetReturnTypeId())) {
        CoalesceExprDecimalHelper(*value1.get(), *value2.get(), *isNotNullBlock, *isNullBlock, *pn, *pnNull);
        return;
    }

    this->value = make_shared<CodeGenValue>(pn, pnNull, lengthPhi);
}

void ExpressionCodeGen::Visit(const IsNullExpr &isNullExpr)
{
    Expr *valueExpr = isNullExpr.value;
    auto value = VisitExpr(*valueExpr);
    if (!value->IsValidValue()) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *isNullValue = value->isNull;

    Value *result = builder->CreateICmpEQ(isNullValue, llvmTypes->CreateConstantBool(true), "isNullCompare");
    this->value = make_shared<CodeGenValue>(result, llvmTypes->CreateConstantBool(false));
}

template <bool isNeedVerifyResult, bool isNeedVerifyVal>
std::vector<Value *> ExpressionCodeGen::GetDefaultFunctionArgValues(
    const FuncExpr &fExpr, Value **isAnyNull, bool &isInvalidExpr)
{
    std::vector<Value *> argVals;
    CodeGenValuePtr resultPtr;
    auto numArgs = fExpr.arguments.size();
    if (fExpr.function->IsExecutionContextSet()) {
        argVals.push_back(this->codegenContext->executionContext);
    }
    for (size_t i = 0; i < numArgs; i++) {
        Expr *argN = fExpr.arguments[i];
        resultPtr = VisitExpr(*argN);
        if (!resultPtr->IsValidValue()) {
            isInvalidExpr = true;
            return argVals;
        }
        argVals.push_back(resultPtr->data);
        if constexpr (isNeedVerifyResult) {
            *isAnyNull = builder->CreateOr(*isAnyNull, resultPtr->isNull);
        }
        if ((TypeUtil::IsStringType(fExpr.arguments[i]->GetReturnTypeId()))) {
            if (fExpr.arguments[i]->GetReturnTypeId() == OMNI_CHAR) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    dynamic_cast<CharDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetWidth()));
            }
            argVals.push_back(this->value->length);
            if (FuncExpr::IsCastStrStr(fExpr)) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    dynamic_cast<VarcharDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetWidth()));
            }
        }
        if (TypeUtil::IsDecimalType(argN->GetReturnTypeId())) {
            argVals.push_back(llvmTypes->CreateConstantInt(
                dynamic_cast<DecimalDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetPrecision()));
            argVals.push_back(llvmTypes->CreateConstantInt(
                dynamic_cast<DecimalDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetScale()));
        }
        if constexpr (isNeedVerifyVal) {
            argVals.push_back(this->value->isNull);
        }
    }
    return argVals;
}

inline std::vector<llvm::Value *> ExpressionCodeGen::GetDataArgs(
    const omniruntime::expressions::FuncExpr &fExpr,
    llvm::Value **isAnyNull,
    bool &isInvalidExpr)
{
    return GetDefaultFunctionArgValues<true, false>(fExpr, isAnyNull, isInvalidExpr);
}

inline std::vector<llvm::Value *> ExpressionCodeGen::GetDataAndNullArgs(
    const omniruntime::expressions::FuncExpr &fExpr,
    llvm::Value **isAnyNull,
    bool &isInvalidExpr)
{
    return GetDefaultFunctionArgValues<false, true>(fExpr, isAnyNull, isInvalidExpr);
}

inline std::vector<llvm::Value *> ExpressionCodeGen::GetDataAndNullArgsAndReturnNull(
    const omniruntime::expressions::FuncExpr &fExpr,
    llvm::Value **isAnyNull,
    bool &isInvalidExpr)
{
    return GetDefaultFunctionArgValues<true, true>(fExpr, isAnyNull, isInvalidExpr);
}

std::vector<llvm::Value *> ExpressionCodeGen::GetFunctionArgValues(const omniruntime::expressions::FuncExpr &fExpr,
    llvm::Value **isAnyNull, bool &isInvalidExpr)
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

Value *ExpressionCodeGen::CreateHiveUdfArgTypes(const FuncExpr &fExpr)
{
    auto elementSize = static_cast<int32_t>(fExpr.arguments.size());
    auto alloca = builder->CreateAlloca(llvmTypes->I32Type(), llvmTypes->CreateConstantInt(elementSize));
    for (int32_t i = 0; i < elementSize; i++) {
        auto ptr = builder->CreateGEP(llvmTypes->I32Type(), alloca, llvmTypes->CreateConstantInt(i));
        builder->CreateStore(llvmTypes->CreateConstantInt(fExpr.arguments[i]->GetReturnTypeId()), ptr);
    }
    return alloca;
}

static bool GetValueOffsets(const FuncExpr &fExpr, std::vector<int32_t> &valueOffsets)
{
    int32_t valueSize = 0;
    for (auto argExpr : fExpr.arguments) {
        valueOffsets.emplace_back(valueSize);

        auto argReturnType = argExpr->GetReturnTypeId();
        switch (argReturnType) {
            case OMNI_INT:
            case OMNI_DATE32:
                valueSize += sizeof(int32_t);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                valueSize += sizeof(int64_t);
                break;
            case OMNI_DOUBLE:
                valueSize += sizeof(double);
                break;
            case OMNI_FLOAT:
                valueSize += sizeof(float);
                break;
            case OMNI_BOOLEAN:
                valueSize += sizeof(bool);
                break;
            case OMNI_SHORT:
                valueSize += sizeof(int16_t);
                break;
            case OMNI_BYTE:
                valueSize = sizeof(int8_t);
            case OMNI_DECIMAL128:
                valueSize += 2 * sizeof(int64_t);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                valueSize += sizeof(uint8_t *);
                break;
            default:
                LogWarn("Unsupported data type in Data Expr %d", argReturnType);
                return false;
        }
    }
    valueOffsets.emplace_back(valueSize);
    return true;
}

std::vector<Value *> ExpressionCodeGen::GetHiveUdfArgValues(const FuncExpr &fExpr, bool &isInvalid)
{
    std::vector<Value *> argVals;
    std::vector<int32_t> valueOffsets;
    if (!GetValueOffsets(fExpr, valueOffsets)) {
        isInvalid = true;
        return argVals;
    }

    // Create array for value, null and length of all arguments
    auto argSize = static_cast<int32_t>(fExpr.arguments.size());
    auto valueArray = builder->CreateAlloca(llvmTypes->I8Type(), llvmTypes->CreateConstantInt(valueOffsets[argSize]));
    auto nullArray = builder->CreateAlloca(llvmTypes->I8Type(), llvmTypes->CreateConstantInt(argSize));
    auto lengthArray = builder->CreateAlloca(llvmTypes->I32Type(), llvmTypes->CreateConstantInt(argSize));

    for (int32_t i = 0; i < argSize; i++) {
        auto argExpr = fExpr.arguments[i];
        auto argExprResult = VisitExpr(*argExpr);
        if (!argExprResult->IsValidValue()) {
            isInvalid = true;
            return argVals;
        }

        // Get pointer for value, null and length
        auto valuePtr =
            builder->CreateGEP(llvmTypes->I8Type(), valueArray, llvmTypes->CreateConstantInt(valueOffsets[i]));
        auto nullPtr = builder->CreateGEP(llvmTypes->I8Type(), nullArray, llvmTypes->CreateConstantInt(i));
        auto lengthPtr = builder->CreateGEP(llvmTypes->I32Type(), lengthArray, llvmTypes->CreateConstantInt(i));

        builder->CreateStore(argExprResult->data, valuePtr);
        builder->CreateStore(argExprResult->isNull, nullPtr);
        if (TypeUtil::IsStringType(argExpr->GetReturnTypeId())) {
            builder->CreateStore(argExprResult->length, lengthPtr);
        } else {
            builder->CreateStore(llvmTypes->CreateConstantInt(0), lengthPtr);
        }
    }

    argVals.emplace_back(valueArray);
    argVals.emplace_back(nullArray);
    argVals.emplace_back(lengthArray);

    return argVals;
}

void ExpressionCodeGen::CallHiveUdfFunction(const FuncExpr &fExpr)
{
    std::vector<Value *> argVals;
    argVals.emplace_back(this->codegenContext->executionContext);
    argVals.emplace_back(CreateConstantString(fExpr.funcName));                  // for udf class name
    argVals.emplace_back(CreateHiveUdfArgTypes(fExpr));                          // for inputTypes
    argVals.emplace_back(llvmTypes->CreateConstantInt(fExpr.GetReturnTypeId())); // for ret type
    argVals.emplace_back(llvmTypes->CreateConstantInt(fExpr.arguments.size()));  // for vec count

    bool isInvalidExpr = false;
    auto inputArgs = GetHiveUdfArgValues(fExpr, isInvalidExpr);
    if (isInvalidExpr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    argVals.insert(argVals.end(), inputArgs.begin(),
        inputArgs.end()); // for inputValues, inputNulls, inputLength

    Value *outputValuePtr;
    Value *outputLenPtr;
    Type *ty = llvmTypes->ToLLVMType(fExpr.GetReturnTypeId());

    if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
        auto valueSize = llvmTypes->CreateConstantInt(200);
        std::vector<DataTypeId> paramsVec = { OMNI_LONG, OMNI_INT };
        outputValuePtr = CallExternFunction("ArenaAllocatorMalloc", paramsVec, OMNI_CHAR,
            { this->codegenContext->executionContext, valueSize }, nullptr);
        outputLenPtr = builder->CreateAlloca(Type::getInt32Ty(*context), nullptr, "outputLength");
        builder->CreateStore(llvmTypes->CreateConstantInt(0), outputLenPtr);
    } else {
        outputValuePtr = builder->CreateAlloca(ty, nullptr, "outputValue");
        outputLenPtr = llvmTypes->CreateConstantLong(0);
    }
    argVals.emplace_back(outputValuePtr);
    auto outputNullPtr = builder->CreateAlloca(Type::getInt8Ty(*context), nullptr, "outputNull");
    argVals.emplace_back(outputNullPtr);
    argVals.emplace_back(outputLenPtr);

    auto signature = FunctionSignature("EvaluateHiveUdfSingle", std::vector<DataTypeId> {}, OMNI_INT);
    auto function = FunctionRegistry::LookupFunction(&signature);
    auto f = modulePtr->getFunction(function->GetId());
    if (f) {
        auto ret = CreateCall(f, argVals, "call_evaluate_hive_udf");
        InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(*((CallInst *)ret), inlineFunctionInfo);
        Value *outputValue = outputValuePtr;
        Value *outputLen = nullptr;
        if (TypeUtil::IsStringType(fExpr.GetReturnTypeId())) {
            outputLen = builder->CreateLoad(llvmTypes->I32Type(), outputLenPtr);
        } else {
            outputValue = builder->CreateLoad(ty, outputValuePtr);
        }
        auto outputNull = builder->CreateLoad(llvmTypes->I1Type(), outputNullPtr);
        this->value = make_shared<CodeGenValue>(outputValue, outputNull, outputLen);
    } else {
        LogWarn("Unable to generate udf function : %s", fExpr.funcName.c_str());
        this->value = CreateInvalidCodeGenValue();
    }
}

// Handles all functions
void ExpressionCodeGen::Visit(const FuncExpr &fExpr)
{
    if (fExpr.functionType == HIVE_UDF) {
        CallHiveUdfFunction(fExpr);
        return;
    }
    if (UNLIKELY(fExpr.function == nullptr || fExpr.function->GetSignatures().empty())) {
        LogWarn("Skip codegen for function '%s': missing codegen signature.", fExpr.funcName.c_str());
        this->value = CreateInvalidCodeGenValue();
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
    Value *isAnyNull = llvmTypes->CreateConstantBool(false);
    auto res = std::find_if(fExpr.arguments.begin(), fExpr.arguments.end(),
        [](Expr *exp) { return exp->GetReturnTypeId() == OMNI_DECIMAL128; });
    bool isDecimalFunction = res != fExpr.arguments.end();
    DataTypeId funcRetType = fExpr.GetReturnTypeId();
    bool isInvalidExpr = false;

    auto argVals = GetFunctionArgValues(fExpr, &isAnyNull, isInvalidExpr);
    if (isInvalidExpr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    Value *isNull = PushAndGetNullFlag(fExpr, argVals, isAnyNull, true);
    Value *ret = nullptr;
    Value *outputLen = nullptr;
    AllocaInst *outputLenPtr = nullptr;
    // Call Decimal IR Generator for decimal functions
    if (TypeUtil::IsDecimalType(funcRetType)) {
        argVals.push_back(
            llvmTypes->CreateConstantInt(dynamic_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision()));
        argVals.push_back(
            llvmTypes->CreateConstantInt(dynamic_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale()));
        auto outputValuePtr = BuildDecimalValue(nullptr, *(fExpr.GetReturnType()));
        ret = CallDecimalFunction(fExpr.function->GetId(), llvmTypes->ToLLVMType(funcRetType), argVals);
        outputValuePtr->data = ret;
        outputValuePtr->isNull = LoadNullFlag(fExpr, isNull);
        outputValuePtr->length = outputLen;
        this->value = std::move(outputValuePtr);
        return;
    } else {
        if (TypeUtil::IsStringType(funcRetType)) {
            if (FuncExpr::IsCastStrStr(fExpr)) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    dynamic_cast<VarcharDataType *>(fExpr.GetReturnType().get())->GetWidth()));
            }
            outputLenPtr = builder->CreateAlloca(Type::getInt32Ty(*context), nullptr, "output_len");
            builder->CreateStore(llvmTypes->CreateConstantInt(0), outputLenPtr);
            argVals.push_back(outputLenPtr);
        }

        auto f = modulePtr->getFunction(fExpr.function->GetId());
        if (f) {
            ret = isDecimalFunction ?
                CallDecimalFunction(fExpr.function->GetId(), llvmTypes->ToLLVMType(funcRetType), argVals) :
                CreateCall(f, argVals, fExpr.function->GetId());
            InlineFunctionInfo inlineFunctionInfo;
            llvm::InlineFunction(*((CallInst *)ret), inlineFunctionInfo);
            outputLen = (outputLenPtr == nullptr) ? nullptr : builder->CreateLoad(llvmTypes->I32Type(), outputLenPtr);
        } else {
            LogWarn("Unable to generate function : %s", fExpr.funcName.c_str());
            this->value = make_shared<CodeGenValue>(nullptr, nullptr, nullptr);
            return;
        }
    }
    this->value = std::make_shared<CodeGenValue>(ret, LoadNullFlag(fExpr, isNull), outputLen);
}

void ExpressionCodeGen::Visit(const ParamRefExpr &Expr) {

}

void ExpressionCodeGen::Visit(const LambdaExpr &Expr) {

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

std::vector<llvm::Value *> ExpressionCodeGen::GetDataAndOverflowNullArgs(
    const omniruntime::expressions::FuncExpr &fExpr, llvm::Value **isAnyNull, bool &isInvalidExpr,
    llvm::Value *overflowNull)
{
    std::vector<Value *> argVals;
    auto signature = fExpr.function->GetSignatures()[0];
    if (FunctionRegistry::IsNullExecutionContextSet(&signature)) {
        argVals.push_back(this->codegenContext->executionContext);
    }
    argVals.push_back(overflowNull);
    CodeGenValuePtr resultPtr;
    auto numArgs = fExpr.arguments.size();

    for (size_t i = 0; i < numArgs; i++) {
        Expr *argN = fExpr.arguments[i];
        resultPtr = VisitExpr(*argN);
        if (!resultPtr->IsValidValue()) {
            isInvalidExpr = true;
            return argVals;
        }
        argVals.push_back(resultPtr->data);
        *isAnyNull = builder->CreateOr(*isAnyNull, resultPtr->isNull);
        if ((TypeUtil::IsStringType(fExpr.arguments[i]->GetReturnTypeId()))) {
            if (fExpr.arguments[i]->GetReturnTypeId() == OMNI_CHAR) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    dynamic_cast<CharDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetWidth()));
            }
            argVals.push_back(this->value->length);
            if (FuncExpr::IsCastStrStr(fExpr)) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    dynamic_cast<VarcharDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetWidth()));
            }
        }
        if (TypeUtil::IsDecimalType(argN->GetReturnTypeId())) {
            argVals.push_back(llvmTypes->CreateConstantInt(
                dynamic_cast<DecimalDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetPrecision()));
            argVals.push_back(llvmTypes->CreateConstantInt(
                dynamic_cast<DecimalDataType *>(fExpr.arguments[i]->GetReturnType().get())->GetScale()));
        }
        if (fExpr.function->GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL) {
            argVals.push_back(this->value->isNull);
        }
    }
    return argVals;
}

void ExpressionCodeGen::FuncExprOverflowNullHelper(const FuncExpr &fExpr)
{
    Value *isAnyNull = llvmTypes->CreateConstantBool(false);
    auto res = std::find_if(fExpr.arguments.begin(), fExpr.arguments.end(),
        [](Expr *exp) { return exp->GetReturnTypeId() == OMNI_DECIMAL128; });
    bool isDecimalFunction = res != fExpr.arguments.end();
    DataTypeId funcRetType = fExpr.GetReturnTypeId();
    bool isInvalidExpr = false;

    AllocaInst *overflowNull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "overflow_null");
    builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), overflowNull);
    auto argVals = GetDataAndOverflowNullArgs(fExpr, &isAnyNull, isInvalidExpr, overflowNull);
    if (isInvalidExpr) {
        this->value = CreateInvalidCodeGenValue();
        return;
    }
    auto isNull = PushAndGetNullFlag(fExpr, argVals, isAnyNull, false);
    Value *ret = nullptr;
    Value *outputLen = nullptr;
    AllocaInst *outputLenPtr = nullptr;
    std::string functionName = ChangeFuncNameToNull(fExpr);

    // Call Decimal IR Generator for decimal functions
    if (TypeUtil::IsDecimalType(funcRetType)) {
        argVals.push_back(
            llvmTypes->CreateConstantInt(dynamic_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetPrecision()));
        argVals.push_back(
            llvmTypes->CreateConstantInt(dynamic_cast<DecimalDataType *>(fExpr.GetReturnType().get())->GetScale()));

        auto outputValuePtr = BuildDecimalValue(nullptr, *(fExpr.GetReturnType()));
        ret = CallDecimalFunction(functionName, llvmTypes->ToLLVMType(funcRetType), argVals);
        outputValuePtr->data = ret;
        outputValuePtr->isNull =
            builder->CreateOr(LoadNullFlag(fExpr, isNull), builder->CreateLoad(llvmTypes->I1Type(), overflowNull));
        outputValuePtr->length = outputLen;
        this->value = std::move(outputValuePtr);
        return;
    } else {
        if (TypeUtil::IsStringType(funcRetType)) {
            if (FuncExpr::IsCastStrStr(fExpr)) {
                argVals.push_back(llvmTypes->CreateConstantInt(
                    dynamic_cast<VarcharDataType *>(fExpr.GetReturnType().get())->GetWidth()));
            }
            outputLenPtr = builder->CreateAlloca(Type::getInt32Ty(*context), nullptr, "output_len");
            builder->CreateStore(llvmTypes->CreateConstantInt(0), outputLenPtr);
            argVals.push_back(outputLenPtr);
        }
        auto f = modulePtr->getFunction(functionName);
        if (f) {
            ret = isDecimalFunction ? CallDecimalFunction(functionName, llvmTypes->ToLLVMType(funcRetType), argVals) :
                                      CreateCall(f, argVals, functionName);
            InlineFunctionInfo inlineFunctionInfo;
            llvm::InlineFunction(*((CallInst *)ret), inlineFunctionInfo);
            outputLen = (outputLenPtr == nullptr) ? nullptr : builder->CreateLoad(llvmTypes->I32Type(), outputLenPtr);
            Value *finalNull =
                builder->CreateOr(LoadNullFlag(fExpr, isNull), builder->CreateLoad(llvmTypes->I1Type(), overflowNull));
            this->value = std::make_shared<CodeGenValue>(ret, finalNull, outputLen);
            return;
        } else {
            LogError("Unable to generate function : %s", fExpr.funcName.c_str());
            this->value = std::make_shared<CodeGenValue>(nullptr, nullptr, nullptr);
            return;
        }
    }
}

void ExpressionCodeGen::ExtractVectorIndexes()
{
    ExprInfoExtractor exprInfoExtractor;
    this->expr->Accept(exprInfoExtractor);
    this->vectorIndexes = exprInfoExtractor.GetVectorIndexes();
}

Value *ExpressionCodeGen::StringEqual(Value *lhs, Value *lLen, Value *rhs, Value *rLen, Value *isNull)
{
    BasicBlock *lenEqualBlock;
    BasicBlock *lenNotEqualBlock;
    BasicBlock *mergeBlock;
    Value *lenCond = builder->CreateAnd(builder->CreateICmpEQ(lLen, rLen), builder->CreateNot(isNull));
    lenEqualBlock = BasicBlock::Create(*context, "lenEqualBlock", builder->GetInsertBlock()->getParent());
    lenNotEqualBlock = BasicBlock::Create(*context, "lenNotEqualBlock", builder->GetInsertBlock()->getParent());
    mergeBlock = BasicBlock::Create(*context, "ifcont", builder->GetInsertBlock()->getParent());
    builder->CreateCondBr(lenCond, lenEqualBlock, lenNotEqualBlock);
    builder->SetInsertPoint(lenEqualBlock);

    std::vector<Value *> argVals { lhs, lLen, rhs, rLen };
    auto signature =
        FunctionSignature(strEqualStr, std::vector<DataTypeId> { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_BOOLEAN);
    auto f = modulePtr->getFunction(FunctionRegistry::LookupFunction(&signature)->GetId());
    auto ret = CreateCall(f, argVals, "call_str_eq");
    InlineFunctionInfo inlineFunctionInfo;
    llvm::InlineFunction(*ret, inlineFunctionInfo);

    builder->CreateBr(mergeBlock);

    builder->SetInsertPoint(lenNotEqualBlock);
    builder->CreateBr(mergeBlock);

    builder->SetInsertPoint(mergeBlock);

    PHINode *phiValue = builder->CreatePHI(llvmTypes->I1Type(), 2, "ifequal");
    phiValue->addIncoming(ret, lenEqualBlock);
    phiValue->addIncoming(lenCond, lenNotEqualBlock);
    return phiValue;
}

// Other operations which require externed functions
Value *ExpressionCodeGen::StringCmp(Value *lhs, Value *lLen, Value *rhs, Value *rLen)
{
    // call function
    std::vector<Value *> argVals { lhs, lLen, rhs, rLen };
    auto signature = FunctionSignature(strCompareStr, std::vector<DataTypeId> { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_INT);
    auto f = modulePtr->getFunction(FunctionRegistry::LookupFunction(&signature)->GetId());
    auto ret = CreateCall(f, argVals, "call_str_cmp");
    InlineFunctionInfo inlineFunctionInfo;
    llvm::InlineFunction(*ret, inlineFunctionInfo);
    return ret;
}

void ExpressionCodeGen::BinaryExprNullHelper(const BinaryExpr *binaryExpr, Value *left, Value *right, Value *leftIsNull,
    Value *rightIsNull, PHINode **leftPhi, PHINode **rightPhi)
{
    BasicBlock *incomingBlock;
    BasicBlock *nullBlock;
    BasicBlock *nextInst;
    Value *nullCond;
    Value *leftZero;
    Value *rightOne;
    auto op = binaryExpr->op;

    if (op == omniruntime::expressions::Operator::ADD || op == omniruntime::expressions::Operator::SUB ||
        op == omniruntime::expressions::Operator::MUL || op == omniruntime::expressions::Operator::DIV ||
        op == omniruntime::expressions::Operator::MOD || op == omniruntime::expressions::Operator::TRY_ADD ||
        op == omniruntime::expressions::Operator::TRY_SUB || op == omniruntime::expressions::Operator::TRY_MUL ||
        op == omniruntime::expressions::Operator::TRY_DIV) {
        incomingBlock = builder->GetInsertBlock();
        nullBlock = BasicBlock::Create(*context, "nullBlock", builder->GetInsertBlock()->getParent());
        nextInst = BasicBlock::Create(*context, "nextInst", builder->GetInsertBlock()->getParent());
        nullCond = builder->CreateOr(leftIsNull, rightIsNull);
        builder->CreateCondBr(nullCond, nullBlock, nextInst);
        builder->SetInsertPoint(nullBlock);
        switch (binaryExpr->left->GetReturnType()->GetId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                leftZero = llvmTypes->CreateConstantInt(0);
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
                leftZero = llvmTypes->CreateConstantLong(0);
                break;
            case OMNI_DOUBLE:
                leftZero = llvmTypes->CreateConstantDouble(0);
                break;
            case OMNI_FLOAT:
                leftZero = llvmTypes->CreateConstantFloat(0);
                break;
            case OMNI_DECIMAL128:
                leftZero = llvmTypes->CreateConstant128(0);
                break;
            default:
                // Unsupported data-types left as-is
                leftZero = left;
                break;
        }
        switch (binaryExpr->right->GetReturnType()->GetId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                rightOne = llvmTypes->CreateConstantInt(1);
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
                rightOne = llvmTypes->CreateConstantLong(1);
                break;
            case OMNI_DOUBLE:
                rightOne = llvmTypes->CreateConstantDouble(1);
                break;
            case OMNI_FLOAT:
                rightOne = llvmTypes->CreateConstantFloat(1);
                break;
            case OMNI_DECIMAL128:
                rightOne = llvmTypes->CreateConstant128(1);
                break;
            default:
                // Unsupported data-types left as-is
                rightOne = right;
                break;
        }
        builder->CreateBr(nextInst);
        builder->SetInsertPoint(nextInst);
        int numberOfPaths = 2;
        *leftPhi = builder->CreatePHI(left->getType(), numberOfPaths, "iftmp");
        *rightPhi = builder->CreatePHI(right->getType(), numberOfPaths, "iftmp");
        (*leftPhi)->addIncoming(leftZero, nullBlock);
        (*leftPhi)->addIncoming(left, incomingBlock);
        (*rightPhi)->addIncoming(rightOne, nullBlock);
        (*rightPhi)->addIncoming(right, incomingBlock);
    }
}

// Helper methods to parse binary expressions
llvm::Value *ExpressionCodeGen::BinaryExprByteHelper(const BinaryExpr *binaryExpr, Value *left, Value *right,
    Value *leftIsNull, Value *rightIsNull, Value *nullFlag)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    std::vector<omniruntime::type::DataTypeId> byteParams = { OMNI_BYTE, OMNI_BYTE };
    BinaryExprNullHelper(binaryExpr, left, right, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("lessThan", byteParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_lt"));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("greaterThan", byteParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_gt"));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("lessThanEqual", byteParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "relational_le"));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThanEqual", byteParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "relational_ge"));
        case omniruntime::expressions::Operator::EQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("equal", byteParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_eq"));
        case omniruntime::expressions::Operator::NEQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("notEqual", byteParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_neq"));
        case omniruntime::expressions::Operator::ADD:
            return CallExternFunction("add", byteParams, OMNI_BYTE, { leftPhi, rightPhi }, nullptr, "arithmetic_add");
        case omniruntime::expressions::Operator::SUB:
            return CallExternFunction("subtract", byteParams, OMNI_BYTE, { leftPhi, rightPhi }, nullptr,
                "arithmetic_sub");
        case omniruntime::expressions::Operator::MUL:
            return CallExternFunction("multiply", byteParams, OMNI_BYTE, { leftPhi, rightPhi }, nullptr,
                "arithmetic_mul");
        case omniruntime::expressions::Operator::DIV:
            return CallExternFunction("divide", byteParams, OMNI_BYTE, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_div");
        case omniruntime::expressions::Operator::MOD:
            return CallExternFunction("modulus", byteParams, OMNI_BYTE, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_mod");
        case omniruntime::expressions::Operator::TRY_ADD:
            return CallExternFunction("try_add", byteParams, OMNI_BYTE, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_add");
        case omniruntime::expressions::Operator::TRY_SUB:
            return CallExternFunction("try_subtract", byteParams, OMNI_BYTE, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_sub");
        case omniruntime::expressions::Operator::TRY_MUL:
            return CallExternFunction("try_multiply", byteParams, OMNI_BYTE, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_mul");
        case omniruntime::expressions::Operator::TRY_DIV:
            return CallExternFunction("divide", byteParams, OMNI_BYTE, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_div");
        default: {
            LogError("Unsupported byte binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

llvm::Value *ExpressionCodeGen::BinaryExprShortHelper(const BinaryExpr *binaryExpr, Value *left, Value *right,
    Value *leftIsNull, Value *rightIsNull, Value *nullFlag)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    std::vector<omniruntime::type::DataTypeId> shortParams = { OMNI_SHORT, OMNI_SHORT };
    BinaryExprNullHelper(binaryExpr, left, right, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("lessThan", shortParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_lt"));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("greaterThan", shortParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_gt"));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("lessThanEqual", shortParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "relational_le"));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThanEqual", shortParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "relational_ge"));
        case omniruntime::expressions::Operator::EQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("equal", shortParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_eq"));
        case omniruntime::expressions::Operator::NEQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("notEqual", shortParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_neq"));
        case omniruntime::expressions::Operator::ADD:
            return CallExternFunction("add", shortParams, OMNI_SHORT, { leftPhi, rightPhi }, nullptr, "arithmetic_add");
        case omniruntime::expressions::Operator::SUB:
            return CallExternFunction("subtract", shortParams, OMNI_SHORT, { leftPhi, rightPhi }, nullptr,
                "arithmetic_sub");
        case omniruntime::expressions::Operator::MUL:
            return CallExternFunction("multiply", shortParams, OMNI_SHORT, { leftPhi, rightPhi }, nullptr,
                "arithmetic_mul");
        case omniruntime::expressions::Operator::DIV:
            return CallExternFunction("divide", shortParams, OMNI_SHORT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_div");
        case omniruntime::expressions::Operator::MOD:
            return CallExternFunction("modulus", shortParams, OMNI_SHORT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_mod");
        case omniruntime::expressions::Operator::TRY_ADD:
            return CallExternFunction("try_add", shortParams, OMNI_SHORT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_add");
        case omniruntime::expressions::Operator::TRY_SUB:
            return CallExternFunction("try_subtract", shortParams, OMNI_SHORT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_sub");
        case omniruntime::expressions::Operator::TRY_MUL:
            return CallExternFunction("try_multiply", shortParams, OMNI_SHORT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_mul");
        case omniruntime::expressions::Operator::TRY_DIV:
            return CallExternFunction("divide", shortParams, OMNI_SHORT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_div");
        default: {
            LogError("Unsupported short binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

llvm::Value *ExpressionCodeGen::BinaryExprIntHelper(const BinaryExpr *binaryExpr, Value *left, Value *right,
    Value *leftIsNull, Value *rightIsNull, Value *nullFlag)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    std::vector<omniruntime::type::DataTypeId> intParams = { OMNI_INT, OMNI_INT };
    BinaryExprNullHelper(binaryExpr, left, right, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("lessThan", intParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_lt"));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("greaterThan", intParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_gt"));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("lessThanEqual", intParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "relational_le"));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThanEqual", intParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "relational_ge"));
        case omniruntime::expressions::Operator::EQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("equal", intParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_eq"));
        case omniruntime::expressions::Operator::NEQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("notEqual", intParams, OMNI_BOOLEAN, { left, right }, nullptr, "relational_neq"));
        case omniruntime::expressions::Operator::ADD:
            return CallExternFunction("add", intParams, OMNI_INT, { leftPhi, rightPhi }, nullptr, "arithmetic_add");
        case omniruntime::expressions::Operator::SUB:
            return CallExternFunction("subtract", intParams, OMNI_INT, { leftPhi, rightPhi }, nullptr,
                "arithmetic_sub");
        case omniruntime::expressions::Operator::MUL:
            return CallExternFunction("multiply", intParams, OMNI_INT, { leftPhi, rightPhi }, nullptr,
                "arithmetic_mul");
        case omniruntime::expressions::Operator::DIV:
            return CallExternFunction("divide", intParams, OMNI_INT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_div");
        case omniruntime::expressions::Operator::MOD:
            return CallExternFunction("modulus", intParams, OMNI_INT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_mod");
        case omniruntime::expressions::Operator::TRY_ADD:
            return CallExternFunction("try_add", intParams, OMNI_INT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_add");
        case omniruntime::expressions::Operator::TRY_SUB:
            return CallExternFunction("try_subtract", intParams, OMNI_INT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_sub");
        case omniruntime::expressions::Operator::TRY_MUL:
            return CallExternFunction("try_multiply", intParams, OMNI_INT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_mul");
        case omniruntime::expressions::Operator::TRY_DIV:
            return CallExternFunction("divide", intParams, OMNI_INT, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "arithmetic_try_div");
        default: {
            LogError("Unsupported int binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

Value *ExpressionCodeGen::BinaryExprLongHelper(const BinaryExpr *binaryExpr, Value *left, Value *right,
    Value *leftIsNull, Value *rightIsNull, Value *nullFlag)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    std::vector<omniruntime::type::DataTypeId> longParams = { OMNI_LONG, OMNI_LONG };
    BinaryExprNullHelper(binaryExpr, left, right, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("lessThan", longParams, OMNI_BOOLEAN, { left, right }, nullptr, "lrelational_lt"));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("lessThanEqual", longParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "lrelational_le"));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThan", longParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "lrelational_gt"));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThanEqual", longParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "lrelational_ge"));
        case omniruntime::expressions::Operator::EQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("equal", longParams, OMNI_BOOLEAN, { left, right }, nullptr, "larithmetic_eq"));
        case omniruntime::expressions::Operator::NEQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("notEqual", longParams, OMNI_BOOLEAN, { left, right }, nullptr, "larithmetic_neq"));
        case omniruntime::expressions::Operator::ADD:
            return CallExternFunction("add", longParams, OMNI_LONG, { leftPhi, rightPhi }, nullptr, "larithmetic_add");
        case omniruntime::expressions::Operator::SUB:
            return CallExternFunction("subtract", longParams, OMNI_LONG, { leftPhi, rightPhi }, nullptr,
                "larithmetic_sub");
        case omniruntime::expressions::Operator::MUL:
            return CallExternFunction("multiply", longParams, OMNI_LONG, { leftPhi, rightPhi }, nullptr,
                "larithmetic_mul");
        case omniruntime::expressions::Operator::DIV:
            return CallExternFunction("divide", longParams, OMNI_LONG, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "larithmetic_divide");
        case omniruntime::expressions::Operator::MOD:
            return CallExternFunction("modulus", longParams, OMNI_LONG, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "larithmetic_mod");
        case omniruntime::expressions::Operator::TRY_ADD:
            return CallExternFunction("try_add", longParams, OMNI_LONG, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "larithmetic_try_add");
        case omniruntime::expressions::Operator::TRY_SUB:
            return CallExternFunction("try_subtract", longParams, OMNI_LONG, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "larithmetic_try_sub");
        case omniruntime::expressions::Operator::TRY_MUL:
            return CallExternFunction("try_multiply", longParams, OMNI_LONG, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "larithmetic_try_mul");
        case omniruntime::expressions::Operator::TRY_DIV:
            return CallExternFunction("divide", longParams, OMNI_LONG, {nullFlag, leftPhi, rightPhi },
                                      nullptr, "larithmetic_try_divide");
        default: {
            LogWarn("Unsupported long binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

void ExpressionCodeGen::BinaryExprDecimal64Helper(const BinaryExpr *binaryExpr, DecimalValue &left, DecimalValue &right,
    Value *leftIsNull, Value *rightIsNull)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    Value *output = nullptr;
    auto leftType = binaryExpr->left->GetReturnType();
    auto rightType = binaryExpr->right->GetReturnType();
    auto binaryReturnType = binaryExpr->GetReturnType();
    BinaryExprNullHelper(binaryExpr, left.data, right.data, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    std::vector<DataTypeId> params { leftType->GetId(), rightType->GetId() };
    std::shared_ptr<DecimalValue> returnDecimalValue = BuildDecimalValue(nullptr, *binaryReturnType, nullptr);
    std::vector<Value *> argVals { leftPhi,
        const_cast<Value *>(left.GetPrecision()),
        const_cast<Value *>(left.GetScale()),
        rightPhi,
        const_cast<Value *>(right.GetPrecision()),
        const_cast<Value *>(right.GetScale()),
        const_cast<Value *>(returnDecimalValue->GetPrecision()),
        const_cast<Value *>(returnDecimalValue->GetScale()) };
    std::vector<Value *> argValsCmp {
        left.data,  const_cast<Value *>(left.GetPrecision()),  const_cast<Value *>(left.GetScale()),
        right.data, const_cast<Value *>(right.GetPrecision()), const_cast<Value *>(right.GetScale())
    };

    llvm::Type *returnType = llvmTypes->ToLLVMType(binaryExpr->GetReturnTypeId());
    DataTypeId returnTypeId = binaryExpr->GetReturnTypeId();
    std::string decimal64CmpFuncId = FunctionSignature(decimal64CompareStr, params, OMNI_INT).ToString();
    AllocaInst *overflowNull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "overflow_null");
    builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), overflowNull);

    bool isTryExpr = false;
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSLT(
                CallDecimalFunction(decimal64CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::GT:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSGT(
                CallDecimalFunction(decimal64CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::LTE:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSLE(
                CallDecimalFunction(decimal64CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::GTE:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSGE(
                CallDecimalFunction(decimal64CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::EQ: {
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpEQ(
                CallDecimalFunction(decimal64CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        }
        case omniruntime::expressions::Operator::NEQ:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpNE(
                CallDecimalFunction(decimal64CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::ADD: {
            std::string funcId = FunctionSignature(addDec64Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::SUB: {
            std::string funcId = FunctionSignature(subDec64Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::MUL: {
            std::string funcId = FunctionSignature(mulDec64Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::DIV: {
            std::string funcId = FunctionSignature(divDec64Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::MOD: {
            std::string funcId = FunctionSignature(modDec64Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_ADD:{
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(tryAddDecimal64FnStr, params, returnTypeId).ToString();
            output= CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_SUB: {
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(trySubDecimal64FnStr, params, returnTypeId).ToString();

            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_MUL: {
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(tryMulDecimal64FnStr, params, returnTypeId).ToString();
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_DIV: {
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(tryDivDecimal64FnStr, params, returnTypeId).ToString();
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        default: {
            LogWarn("Unsupported decimal64 binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            output = nullptr;
            break;
        }
    }
    CodeGenValuePtr valuePtr = nullptr;
    if (TypeUtil::IsDecimalType(binaryExpr->GetReturnTypeId())) {
        valuePtr =
            BuildDecimalValue(output, *(binaryExpr->GetReturnType()), builder->CreateOr(leftIsNull, rightIsNull));
    } else {
        valuePtr = std::make_shared<CodeGenValue>(output, builder->CreateOr(leftIsNull, rightIsNull));
    }

    if (isTryExpr || (overflowConfig != nullptr &&
    overflowConfig->GetOverflowConfigId() == omniruntime::op::OVERFLOW_CONFIG_NULL)) {
        valuePtr->isNull = builder->CreateOr(valuePtr->isNull, builder->CreateLoad(llvmTypes->I1Type(), overflowNull));
    }
    this->value = valuePtr;
}

Value *ExpressionCodeGen::BinaryExprDoubleHelper(const BinaryExpr *binaryExpr, Value *left, Value *right,
    Value *leftIsNull, Value *rightIsNull, Value *nullFlag)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    std::vector<omniruntime::type::DataTypeId> doubleParams = { OMNI_DOUBLE, OMNI_DOUBLE };
    BinaryExprNullHelper(binaryExpr, left, right, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("lessThan", doubleParams, OMNI_BOOLEAN, { left, right }, nullptr, "frelational_lt"));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("lessThanEqual", doubleParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "frelational_le"));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThan", doubleParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "frelational_gt"));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThanEqual", doubleParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "frelational_ge"));
        case omniruntime::expressions::Operator::EQ:
            return builder->CreateAnd(isNeitherNull,
                CallExternFunction("equal", doubleParams, OMNI_BOOLEAN, { left, right }, nullptr, "farithmetic_eq"));
        case omniruntime::expressions::Operator::NEQ:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("notEqual", doubleParams, OMNI_BOOLEAN,
                { left, right }, nullptr, "farithmetic_neq"));
        case omniruntime::expressions::Operator::ADD:
            return CallExternFunction("add", doubleParams, OMNI_DOUBLE, { leftPhi, rightPhi }, nullptr,
                "farithmetic_add");
        case omniruntime::expressions::Operator::SUB:
            return CallExternFunction("subtract", doubleParams, OMNI_DOUBLE, { leftPhi, rightPhi }, nullptr,
                "farithmetic_sub");
        case omniruntime::expressions::Operator::MUL:
            return CallExternFunction("multiply", doubleParams, OMNI_DOUBLE, { leftPhi, rightPhi }, nullptr,
                "farithmetic_mul");
        case omniruntime::expressions::Operator::DIV:
            return CallExternFunction("divide", doubleParams, OMNI_DOUBLE, { nullFlag, leftPhi, rightPhi }, nullptr,
                "farithmetic_divide");
        case omniruntime::expressions::Operator::MOD:
            return CallExternFunction("modulus", doubleParams, OMNI_DOUBLE, { nullFlag, leftPhi, rightPhi }, nullptr,
                "farithmetic_mod");
        case omniruntime::expressions::Operator::TRY_ADD:
            return CallExternFunction("add", doubleParams, OMNI_DOUBLE, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_add");
        case omniruntime::expressions::Operator::TRY_SUB:
            return CallExternFunction("subtract", doubleParams, OMNI_DOUBLE, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_sub");
        case omniruntime::expressions::Operator::TRY_MUL:
            return CallExternFunction("multiply", doubleParams, OMNI_DOUBLE, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_mul");
        case omniruntime::expressions::Operator::TRY_DIV:
            return CallExternFunction("divide", doubleParams, OMNI_DOUBLE, { nullFlag, leftPhi, rightPhi }, nullptr,
                                      "farithmetic_divide");
        default: {
            LogWarn("Unsupported double binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

Value *ExpressionCodeGen::BinaryExprFloatHelper(const BinaryExpr *binaryExpr, Value *left, Value *right,
                                                 Value *leftIsNull, Value *rightIsNull, Value *nullFlag)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    std::vector<omniruntime::type::DataTypeId> floatParams = { OMNI_FLOAT, OMNI_FLOAT };
    BinaryExprNullHelper(binaryExpr, left, right, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(isNeitherNull,
                                      CallExternFunction("lessThan", floatParams, OMNI_BOOLEAN, { left, right }, nullptr, "frelational_lt"));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("lessThanEqual", floatParams, OMNI_BOOLEAN,
                                                                        { left, right }, nullptr, "frelational_le"));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThan", floatParams, OMNI_BOOLEAN,
                                                                        { left, right }, nullptr, "frelational_gt"));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("greaterThanEqual", floatParams, OMNI_BOOLEAN,
                                                                        { left, right }, nullptr, "frelational_ge"));
        case omniruntime::expressions::Operator::EQ:
            return builder->CreateAnd(isNeitherNull,
                                      CallExternFunction("equal", floatParams, OMNI_BOOLEAN, { left, right }, nullptr, "farithmetic_eq"));
        case omniruntime::expressions::Operator::NEQ:
            return builder->CreateAnd(isNeitherNull, CallExternFunction("notEqual", floatParams, OMNI_BOOLEAN,
                                                                        { left, right }, nullptr, "farithmetic_neq"));
        case omniruntime::expressions::Operator::ADD:
            return CallExternFunction("add", floatParams, OMNI_FLOAT, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_add");
        case omniruntime::expressions::Operator::SUB:
            return CallExternFunction("subtract", floatParams, OMNI_FLOAT, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_sub");
        case omniruntime::expressions::Operator::MUL:
            return CallExternFunction("multiply", floatParams, OMNI_FLOAT, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_mul");
        case omniruntime::expressions::Operator::DIV:
            return CallExternFunction("divide", floatParams, OMNI_FLOAT, { nullFlag, leftPhi, rightPhi }, nullptr,
                                      "farithmetic_divide");
        case omniruntime::expressions::Operator::MOD:
            return CallExternFunction("modulus", floatParams, OMNI_FLOAT, { nullFlag, leftPhi, rightPhi }, nullptr,
                                      "farithmetic_mod");
        case omniruntime::expressions::Operator::TRY_ADD:
            return CallExternFunction("add", floatParams, OMNI_FLOAT, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_add");
        case omniruntime::expressions::Operator::TRY_SUB:
            return CallExternFunction("subtract", floatParams, OMNI_FLOAT, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_sub");
        case omniruntime::expressions::Operator::TRY_MUL:
            return CallExternFunction("multiply", floatParams, OMNI_FLOAT, { leftPhi, rightPhi }, nullptr,
                                      "farithmetic_mul");
        case omniruntime::expressions::Operator::TRY_DIV:
            return CallExternFunction("divide", floatParams, OMNI_FLOAT, { nullFlag, leftPhi, rightPhi }, nullptr,
                                      "farithmetic_divide");
        default: {
            LogWarn("Unsupported float binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

Value *ExpressionCodeGen::BinaryExprStringHelper(const BinaryExpr *binaryExpr, Value *leftVal, Value *leftLen,
    Value *rightVal, Value *rightLen, Value *leftIsNull, Value *rightIsNull)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNull = builder->CreateOr(leftIsNull, rightIsNull);
    BinaryExprNullHelper(binaryExpr, leftVal, rightVal, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            return builder->CreateAnd(builder->CreateNot(isNull), builder->CreateICmpSLT(
                this->StringCmp(leftVal, leftLen, rightVal, rightLen), llvmTypes->CreateConstantInt(0)));
        case omniruntime::expressions::Operator::GT:
            return builder->CreateAnd(builder->CreateNot(isNull), builder->CreateICmpSGT(
                this->StringCmp(leftVal, leftLen, rightVal, rightLen), llvmTypes->CreateConstantInt(0)));
        case omniruntime::expressions::Operator::LTE:
            return builder->CreateAnd(builder->CreateNot(isNull), builder->CreateICmpSLE(
                this->StringCmp(leftVal, leftLen, rightVal, rightLen), llvmTypes->CreateConstantInt(0)));
        case omniruntime::expressions::Operator::GTE:
            return builder->CreateAnd(builder->CreateNot(isNull), builder->CreateICmpSGE(
                this->StringCmp(leftVal, leftLen, rightVal, rightLen), llvmTypes->CreateConstantInt(0)));
        case omniruntime::expressions::Operator::EQ: {
            return this->StringEqual(leftVal, leftLen, rightVal, rightLen, isNull);
        }
        case omniruntime::expressions::Operator::NEQ: {
            return builder->CreateNot(this->StringEqual(leftVal, leftLen, rightVal, rightLen, isNull));
        }

        default: {
            LogWarn("Unsupported string binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            return nullptr;
        }
    }
}

void ExpressionCodeGen::BinaryExprDecimal128Helper(const BinaryExpr *binaryExpr, DecimalValue &left,
    DecimalValue &right, Value *leftIsNull, Value *rightIsNull)
{
    PHINode *leftPhi;
    PHINode *rightPhi;
    Value *isNeitherNull = builder->CreateNot(builder->CreateOr(leftIsNull, rightIsNull));
    Value *output = nullptr;
    auto leftType = binaryExpr->left->GetReturnType();
    auto rightType = binaryExpr->right->GetReturnType();
    auto binaryReturnType = binaryExpr->GetReturnType();
    BinaryExprNullHelper(binaryExpr, left.data, right.data, leftIsNull, rightIsNull, &leftPhi, &rightPhi);
    std::vector<DataTypeId> params { leftType->GetId(), rightType->GetId() };
    std::shared_ptr<DecimalValue> returnDecimalValue = BuildDecimalValue(nullptr, *binaryReturnType, nullptr);
    std::vector<Value *> argVals { leftPhi,
        const_cast<Value *>(left.GetPrecision()),
        const_cast<Value *>(left.GetScale()),
        rightPhi,
        const_cast<Value *>(right.GetPrecision()),
        const_cast<Value *>(right.GetScale()),
        const_cast<Value *>(returnDecimalValue->GetPrecision()),
        const_cast<Value *>(returnDecimalValue->GetScale()) };
    std::vector<Value *> argValsCmp {
        left.data,  const_cast<Value *>(left.GetPrecision()),  const_cast<Value *>(left.GetScale()),
        right.data, const_cast<Value *>(right.GetPrecision()), const_cast<Value *>(right.GetScale())
    };
    DataTypeId returnTypeId = binaryExpr->GetReturnTypeId();
    Type *returnType = llvmTypes->ToLLVMType(binaryExpr->GetReturnTypeId());
    std::string decimal128CmpFuncId = FunctionSignature(decimal128CompareStr, params, OMNI_INT).ToString();
    AllocaInst *overflowNull = builder->CreateAlloca(Type::getInt1Ty(*context), nullptr, "overflow_null");
    builder->CreateStore(ConstantInt::get(IntegerType::getInt1Ty(*context), 0), overflowNull);

    bool isTryExpr = false;
    switch (binaryExpr->op) {
        case omniruntime::expressions::Operator::LT:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSLT(
                CallDecimalFunction(decimal128CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::GT:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSGT(
                CallDecimalFunction(decimal128CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::LTE:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSLE(
                CallDecimalFunction(decimal128CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::GTE:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpSGE(
                CallDecimalFunction(decimal128CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::EQ: {
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpEQ(
                CallDecimalFunction(decimal128CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        }
        case omniruntime::expressions::Operator::NEQ:
            output = builder->CreateAnd(isNeitherNull, builder->CreateICmpNE(
                CallDecimalFunction(decimal128CmpFuncId, returnType, argValsCmp), llvmTypes->CreateConstantInt(0)));
            break;
        case omniruntime::expressions::Operator::ADD: {
            std::string funcId = FunctionSignature(addDec128Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::SUB: {
            std::string funcId = FunctionSignature(subDec128Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::MUL: {
            std::string funcId = FunctionSignature(mulDec128Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::DIV: {
            std::string funcId = FunctionSignature(divDec128Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::MOD: {
            std::string funcId = FunctionSignature(modDec128Str, params, returnTypeId).ToString(this->overflowConfig);
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,
                this->overflowConfig, overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_ADD:{
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(tryAddDecimal128FnStr, params, returnTypeId).ToString();
            output= CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_SUB: {
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(trySubDecimal128FnStr, params, returnTypeId).ToString();
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext,ptr.get(), overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_MUL: {
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(tryMulDecimal128FnStr, params, returnTypeId).ToString();
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        case omniruntime::expressions::Operator::TRY_DIV: {
            isTryExpr = true;
            auto ptr = std::make_unique<omniruntime::op::OverflowConfig>(omniruntime::op::OverflowConfigId::OVERFLOW_CONFIG_NULL);
            std::string funcId = FunctionSignature(tryDivDecimal128FnStr, params, returnTypeId).ToString();
            output = CallDecimalFunction(funcId, returnType, argVals, codegenContext->executionContext, ptr.get(), overflowNull);
            break;
        }
        default: {
            LogWarn("Unsupported decimal128 binary operator %u", static_cast<uint32_t>(binaryExpr->op));
            output = nullptr;
            break;
        }
    }
    CodeGenValuePtr valuePtr = nullptr;
    if (TypeUtil::IsDecimalType(binaryExpr->GetReturnTypeId())) {
        valuePtr =
            BuildDecimalValue(output, *(binaryExpr->GetReturnType()), builder->CreateOr(leftIsNull, rightIsNull));
    } else {
        valuePtr = std::make_shared<CodeGenValue>(output, builder->CreateOr(leftIsNull, rightIsNull));
    }

    if (isTryExpr || (overflowConfig != nullptr &&
    overflowConfig->GetOverflowConfigId() == omniruntime::op::OVERFLOW_CONFIG_NULL)) {
        valuePtr->isNull = builder->CreateOr(valuePtr->isNull, builder->CreateLoad(llvmTypes->I1Type(), overflowNull));
    }
    this->value = valuePtr;
}

CodeGenValue *ExpressionCodeGen::LiteralExprConstantHelper(const LiteralExpr &lExpr)
{
    CodeGenValue *codeGenValue = nullptr;
    bool isNullLiteral = lExpr.isNull;
    switch (lExpr.GetReturnTypeId()) {
        case OMNI_INT:
        case OMNI_DATE32: {
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantInt(lExpr.intVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_BYTE:{
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantByte(lExpr.byteVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_SHORT:{
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantShort(lExpr.shortVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_TIMESTAMP:
        case OMNI_LONG: {
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantLong(lExpr.longVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_DOUBLE: {
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantDouble(lExpr.doubleVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_FLOAT: {
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantFloat(lExpr.floatVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            Constant *strValConst = CreateConstantString(*(lExpr.stringVal));
            Constant *strLenConst =
                ConstantInt::get(*context, APInt(INT32_VALUE, static_cast<int32_t>(lExpr.stringVal->length())));
            codeGenValue = new CodeGenValue(strValConst, llvmTypes->CreateConstantBool(isNullLiteral), strLenConst);
            break;
        }
        case OMNI_BOOLEAN: {
            codeGenValue = new CodeGenValue(llvmTypes->CreateConstantBool(lExpr.boolVal),
                llvmTypes->CreateConstantBool(isNullLiteral));
            break;
        }
        case OMNI_DECIMAL64: {
            Value *precision = llvmTypes->CreateConstantInt(
                static_cast<Decimal64DataType *>(lExpr.GetReturnType().get())->GetPrecision());
            Value *scale =
                llvmTypes->CreateConstantInt(static_cast<Decimal64DataType *>(lExpr.GetReturnType().get())->GetScale());
            codeGenValue = new DecimalValue(llvmTypes->CreateConstantLong(lExpr.longVal),
                llvmTypes->CreateConstantBool(isNullLiteral), precision, scale);
            break;
        }
        case OMNI_DECIMAL128: {
            std::string dec128String = isNullLiteral ? "0" : *lExpr.stringVal;
            __uint128_t dec128 = Decimal128Utils::StrToUint128_t(dec128String.c_str());
            dec128String = Decimal128Utils::Uint128_tToStr(dec128);
            Value *precision = llvmTypes->CreateConstantInt(
                static_cast<Decimal128DataType *>(lExpr.GetReturnType().get())->GetPrecision());
            Value *scale = llvmTypes->CreateConstantInt(
                static_cast<Decimal128DataType *>(lExpr.GetReturnType().get())->GetScale());
            auto const128Val = llvm::ConstantInt::get(llvm::Type::getInt128Ty(*context), dec128String, 10);
            codeGenValue =
                new DecimalValue(const128Val, llvmTypes->CreateConstantBool(isNullLiteral), precision, scale);
            break;
        }
        case OMNI_NONE: {
            codeGenValue =
                new CodeGenValue(llvmTypes->CreateConstantInt(lExpr.intVal), llvmTypes->CreateConstantBool(true));
            break;
        }
        default: {
            LogWarn("Unsupported data type in Data Expr %d", lExpr.GetReturnTypeId());
            codeGenValue =
                new CodeGenValue(llvmTypes->CreateConstantBool(lExpr.boolVal), llvmTypes->CreateConstantBool(false));
            break;
        }
    }
    return codeGenValue;
}

bool ExpressionCodeGen::AreInvalidDataTypes(DataTypeId type1, DataTypeId type2)
{
    return type1 != type2 && !(TypeUtil::IsStringType(type1) && TypeUtil::IsStringType(type2));
}

void ExpressionCodeGen::InExprIntegerHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue,
    Value *&tmpCmpData, Value *&tmpCmpNull)
{
    tmpCmpData = builder->CreateICmpEQ(valueToCompare->data, argiValue->data);
    tmpCmpNull = builder->CreateOr(valueToCompare->isNull, argiValue->isNull);
}

void ExpressionCodeGen::InExprDecimal64Helper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue,
    Value *&tmpCmpData, Value *&tmpCmpNull, llvm::Type *retType)
{
    std::vector<DataTypeId> params { OMNI_DECIMAL64, OMNI_DECIMAL64 };
    std::string funcId = FunctionSignature(decimal64CompareStr, params, OMNI_INT).ToString();
    DecimalValue &left = static_cast<DecimalValue &>(*valueToCompare);
    DecimalValue &right = static_cast<DecimalValue &>(*argiValue);
    std::vector<Value *> argValsCmp {
        left.data,  const_cast<Value *>(left.GetPrecision()),  const_cast<Value *>(left.GetScale()),
        right.data, const_cast<Value *>(right.GetPrecision()), const_cast<Value *>(right.GetScale())
    };
    tmpCmpData =
        builder->CreateICmpEQ(CallDecimalFunction(funcId, retType, argValsCmp), llvmTypes->CreateConstantInt(0));

    tmpCmpNull = builder->CreateOr(valueToCompare->isNull, argiValue->isNull);
}

void ExpressionCodeGen::InExprDecimal128Helper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue,
    Value *&tmpCmpData, Value *&tmpCmpNull, llvm::Type *retType)
{
    std::vector<DataTypeId> params { OMNI_DECIMAL128, OMNI_DECIMAL128 };
    std::string funcId = FunctionSignature(decimal128CompareStr, params, OMNI_INT).ToString();
    DecimalValue &left = static_cast<DecimalValue &>(*valueToCompare);
    DecimalValue &right = static_cast<DecimalValue &>(*argiValue);
    std::vector<Value *> argValsCmp {
        left.data,  const_cast<Value *>(left.GetPrecision()),  const_cast<Value *>(left.GetScale()),
        right.data, const_cast<Value *>(right.GetPrecision()), const_cast<Value *>(right.GetScale())
    };
    tmpCmpData =
        builder->CreateICmpEQ(CallDecimalFunction(funcId, retType, argValsCmp), llvmTypes->CreateConstantInt(0));

    tmpCmpNull = builder->CreateOr(valueToCompare->isNull, argiValue->isNull);
}

void ExpressionCodeGen::InExprStringHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue,
    Value *&tmpCmpData, Value *&tmpCmpNull)
{
    tmpCmpNull = builder->CreateOr(valueToCompare->isNull, argiValue->isNull);
    tmpCmpData = StringEqual(valueToCompare->data, valueToCompare->length, argiValue->data, value->length, tmpCmpNull);
}

void ExpressionCodeGen::InExprDoubleHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue,
    Value *&tmpCmpData, Value *&tmpCmpNull)
{
    tmpCmpData = builder->CreateFCmpOEQ(valueToCompare->data, argiValue->data);
    tmpCmpNull = builder->CreateOr(valueToCompare->isNull, argiValue->isNull);
}

void ExpressionCodeGen::InExprFloatHelper(CodeGenValuePtr &valueToCompare, CodeGenValuePtr &argiValue,
    Value *&tmpCmpData, Value *&tmpCmpNull)
{
    tmpCmpData = builder->CreateFCmpOEQ(valueToCompare->data, argiValue->data);
    tmpCmpNull = builder->CreateOr(valueToCompare->isNull, argiValue->isNull);
}

bool ExpressionCodeGen::VisitBetweenExprHelper(BetweenExpr &bExpr, const std::shared_ptr<CodeGenValue> &val,
    const std::shared_ptr<CodeGenValue> &lowerVal, const std::shared_ptr<CodeGenValue> &upperVal,
    std::pair<Value **, Value **> cmpPair)
{
    llvm::Type *retType = llvmTypes->ToLLVMType(bExpr.GetReturnTypeId());
    auto cmpLeft = cmpPair.first;
    auto cmpRight = cmpPair.second;
    if (bExpr.value->GetReturnTypeId() == OMNI_INT || bExpr.value->GetReturnTypeId() == OMNI_LONG ||
        bExpr.value->GetReturnTypeId() == OMNI_DATE32 || bExpr.value->GetReturnTypeId() == OMNI_TIMESTAMP) {
        *cmpLeft = builder->CreateICmpSLE(lowerVal->data, val->data, "between_cmpleft");
        *cmpRight = builder->CreateICmpSLE(val->data, upperVal->data, "between_cmpright");
        return true;
    } else if (bExpr.value->GetReturnTypeId() == OMNI_DOUBLE || bExpr.value->GetReturnTypeId() == OMNI_FLOAT) {
        *cmpLeft = builder->CreateFCmpULE(lowerVal->data, val->data, "between_cmpleft");
        *cmpRight = builder->CreateFCmpULE(val->data, upperVal->data, "between_cmpright");
        return true;
    } else if (TypeUtil::IsStringType(bExpr.value->GetReturnTypeId())) {
        *cmpLeft = builder->CreateICmpSLE(this->StringCmp(lowerVal->data, lowerVal->length, val->data, val->length),
            llvmTypes->CreateConstantInt(0));
        *cmpRight = builder->CreateICmpSLE(this->StringCmp(val->data, val->length, upperVal->data, upperVal->length),
            llvmTypes->CreateConstantInt(0));
        return true;
    } else if (TypeUtil::IsDecimalType(bExpr.value->GetReturnTypeId())) {
        auto retTypeId = bExpr.value->GetReturnTypeId();
        if (retTypeId == OMNI_DECIMAL64) {
            std::vector<DataTypeId> params { OMNI_DECIMAL64, OMNI_DECIMAL64 };
            auto &cmpLower = static_cast<DecimalValue &>(*lowerVal);
            auto &cmpVal = static_cast<DecimalValue &>(*val);
            auto &cmpUpper = static_cast<DecimalValue &>(*upperVal);
            if (cmpVal.GetScale() == cmpLower.GetScale() && cmpVal.GetScale() == cmpUpper.GetScale()) {
                *cmpLeft = builder->CreateICmpSLE(lowerVal->data, val->data, "between_cmpleft");
                *cmpRight = builder->CreateICmpSLE(val->data, upperVal->data, "between_cmpright");
                return true;
            }

            std::vector<Value *> argValsCmpLeft {
                cmpLower.data, const_cast<Value *>(cmpLower.GetPrecision()), const_cast<Value *>(cmpLower.GetScale()),
                cmpVal.data,   const_cast<Value *>(cmpVal.GetPrecision()),   const_cast<Value *>(cmpVal.GetScale())
            };
            std::vector<Value *> argValsCmpRight {
                cmpVal.data,   const_cast<Value *>(cmpVal.GetPrecision()),   const_cast<Value *>(cmpVal.GetScale()),
                cmpUpper.data, const_cast<Value *>(cmpUpper.GetPrecision()), const_cast<Value *>(cmpUpper.GetScale())
            };
            std::string funcId = FunctionSignature(decimal64CompareStr, params, OMNI_INT).ToString();

            *cmpLeft = builder->CreateICmpSLE(CallDecimalFunction(funcId, retType, argValsCmpLeft),
                llvmTypes->CreateConstantInt(0));
            *cmpRight = builder->CreateICmpSLE(CallDecimalFunction(funcId, retType, argValsCmpRight),
                llvmTypes->CreateConstantInt(0));
        } else if (retTypeId == OMNI_DECIMAL128) {
            std::vector<DataTypeId> params { OMNI_DECIMAL128, OMNI_DECIMAL128 };
            auto &cmpLower = static_cast<DecimalValue &>(*lowerVal);
            auto &cmpVal = static_cast<DecimalValue &>(*val);
            auto &cmpUpper = static_cast<DecimalValue &>(*upperVal);
            std::vector<Value *> argValsCmpLeft {
                cmpLower.data, const_cast<Value *>(cmpLower.GetPrecision()), const_cast<Value *>(cmpLower.GetScale()),
                cmpVal.data,   const_cast<Value *>(cmpVal.GetPrecision()),   const_cast<Value *>(cmpVal.GetScale())
            };
            std::vector<Value *> argValsCmpRight {
                cmpVal.data,   const_cast<Value *>(cmpVal.GetPrecision()),   const_cast<Value *>(cmpVal.GetScale()),
                cmpUpper.data, const_cast<Value *>(cmpUpper.GetPrecision()), const_cast<Value *>(cmpUpper.GetScale())
            };
            std::string funcId = FunctionSignature(decimal128CompareStr, params, OMNI_INT).ToString();

            *cmpLeft = builder->CreateICmpSLE(CallDecimalFunction(funcId, retType, argValsCmpLeft),
                llvmTypes->CreateConstantInt(0));
            *cmpRight = builder->CreateICmpSLE(CallDecimalFunction(funcId, retType, argValsCmpRight),
                llvmTypes->CreateConstantInt(0));
        }
        return true;
    }
    return false;
}

Value *ExpressionCodeGen::GetDictionaryVectorValue(const omniruntime::type::DataType &dataType, Value *rowIdx,
    Value *dictionaryVectorPtr, AllocaInst *&lengthAllocaInst)
{
    std::vector<DataTypeId> paramTypes = { OMNI_LONG, OMNI_INT };
    DataTypeId typeId = dataType.GetId();
    FunctionSignature dictionaryFuncSignature;
    switch (typeId) {
        case OMNI_BYTE:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetByteStr, paramTypes, OMNI_BYTE);
            break;
        case OMNI_SHORT:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetShortStr, paramTypes, OMNI_SHORT);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetIntStr, paramTypes, OMNI_INT);
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetLongStr, paramTypes, OMNI_LONG);
            break;
        case OMNI_DECIMAL128:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetDecimalStr, paramTypes, OMNI_DECIMAL128);
            break;
        case OMNI_DOUBLE:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetDoubleStr, paramTypes, OMNI_DOUBLE);
            break;
        case OMNI_FLOAT:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetFloatStr, paramTypes, OMNI_FLOAT);
            break;
        case OMNI_BOOLEAN:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetBooleanStr, paramTypes, OMNI_BOOLEAN);
            break;
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetVarcharStr, paramTypes, OMNI_VARCHAR);
            break;
        case OMNI_ROW:
            dictionaryFuncSignature = FunctionSignature(dictionaryGetLongStr, paramTypes, OMNI_LONG);
            break;
        default:
            LogWarn("Unsupported dictionary value type: %d", typeId);
            return nullptr;
    }
    auto dictionaryFunc = modulePtr->getFunction(FunctionRegistry::LookupFunction(&dictionaryFuncSignature)->GetId());
    std::vector<Value *> funcArgs;
    funcArgs.push_back(dictionaryVectorPtr);
    funcArgs.push_back(rowIdx);
    if (TypeUtil::IsStringType(typeId)) {
        lengthAllocaInst = builder->CreateAlloca(llvmTypes->I32Type(), nullptr, "varchar_length");
        builder->CreateStore(llvmTypes->CreateConstantInt(0), lengthAllocaInst);
        funcArgs.push_back(lengthAllocaInst);
    }
    Value *result = nullptr;
    if (typeId == OMNI_DECIMAL128) {
        funcArgs.push_back(
            llvmTypes->CreateConstantInt(static_cast<const Decimal128DataType &>(dataType).GetPrecision()));
        funcArgs.push_back(llvmTypes->CreateConstantInt(static_cast<const Decimal128DataType &>(dataType).GetScale()));
        result = CallDecimalFunction(FunctionRegistry::LookupFunction(&dictionaryFuncSignature)->GetId(),
            llvmTypes->ToLLVMType(typeId), funcArgs);
    } else {
        result = CreateCall(dictionaryFunc, funcArgs, "get_dictionary_value");
        InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(*((CallInst *)result), inlineFunctionInfo);
    }
    return result;
}

void ExpressionCodeGen::CoalesceExprDecimalHelper(CodeGenValue &v1, CodeGenValue &v2, BasicBlock &isNotNullBlock,
    BasicBlock &isNullBlock, PHINode &pn, PHINode &pnNull)
{
    int32_t numReservedValues = 2;
    auto precisionPhi = builder->CreatePHI(Type::getInt32Ty(*context), numReservedValues, "precision");
    auto value1Precision = (Value *)static_cast<DecimalValue &>(v1).GetPrecision();
    auto value2Precision = (Value *)static_cast<DecimalValue &>(v2).GetPrecision();
    precisionPhi->addIncoming(value1Precision, &isNotNullBlock);
    precisionPhi->addIncoming(value2Precision, &isNullBlock);

    auto scalePhi = builder->CreatePHI(Type::getInt32Ty(*context), numReservedValues, "scale");
    auto value1Scale = (Value *)static_cast<DecimalValue &>(v1).GetScale();
    auto value2Scale = (Value *)static_cast<DecimalValue &>(v2).GetScale();
    scalePhi->addIncoming(value1Scale, &isNotNullBlock);
    scalePhi->addIncoming(value2Scale, &isNullBlock);

    this->value = std::make_shared<DecimalValue>(&pn, &pnNull, precisionPhi, scalePhi);
}

Value *ExpressionCodeGen::PushAndGetNullFlag(const FuncExpr &fExpr, std::vector<llvm::Value *> &argVals,
    Value *nullFlag, bool needAdd)
{
    if (fExpr.function->GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL) {
        AllocaInst *isNullPtr = builder->CreateAlloca(builder->getInt1Ty(), nullptr, "is_null");
        builder->CreateStore(llvmTypes->CreateConstantBool(false), isNullPtr);
        argVals.push_back(isNullPtr);
        return isNullPtr;
    }
    if (needAdd) {
        argVals.push_back(nullFlag);
    }
    return nullFlag;
}

Value *ExpressionCodeGen::LoadNullFlag(const FuncExpr &fExpr, Value *nullFlag)
{
    if (fExpr.function->GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL) {
        return builder->CreateLoad(builder->getInt1Ty(), nullFlag);
    }
    return nullFlag;
}
}
