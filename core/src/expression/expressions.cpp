/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description:
 */
#include "expressions.h"
#include <string>
#include <algorithm>
#include <utility>
#include <vectorization/functions/ConcatFunction.h>
#include "vectorization/functions/IsNull.h"
#include "vectorization/functions/Cast.h"
#include "vectorization/functions/NamedStruct.h"
#include "vectorization/functions/MapFunction.h"
#include "vectorization/functions/MathDecimalFunctions.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "codegen/func_registry.h"
#include "util/type_util.h"
#include "expr_verifier.h"
#include "expr_printer.h"
#include "arm_neon.h"

using namespace std;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::vec;

namespace omniruntime {
namespace expressions {
// Prevent ExprVerifier from being optimized out by the compiler.
static ExprVerifier globalExprVerifier;
static ExprPrinter globalExprPrinter;
const static int32_t NEON_BYTE_SIZE = 16;

std::vector<BaseVector *> GetConstantInputs(const std::vector<Expr *> &arguments)
{
    std::vector<BaseVector *> constantInputs;
    for (auto arg : arguments) {
        auto literalExpr = dynamic_cast<LiteralExpr *>(arg);
        if (literalExpr && !literalExpr->isNull) {
            auto typeId = arg->dataType->GetId();
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    constantInputs.push_back(new ConstVector(literalExpr->intVal, typeId));
                    break;
                case OMNI_SHORT:
                    constantInputs.push_back(new ConstVector(literalExpr->shortVal, typeId));
                    break;
                case OMNI_BYTE:
                    constantInputs.push_back(new ConstVector(literalExpr->byteVal, typeId));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    constantInputs.push_back(new ConstVector(literalExpr->longVal, typeId));
                    break;
                case OMNI_DOUBLE:
                    constantInputs.push_back(new ConstVector(literalExpr->doubleVal, typeId));
                    break;
                case OMNI_FLOAT:
                    constantInputs.push_back(new ConstVector(literalExpr->floatVal, typeId));
                    break;
                case OMNI_BOOLEAN:
                    constantInputs.push_back(new ConstVector(literalExpr->boolVal, typeId));
                    break;
                case OMNI_DECIMAL128: {
                    Decimal128 decimal128(*literalExpr->stringVal);
                    constantInputs.push_back(new ConstVector<Decimal128>(decimal128, typeId));
                    break;
                }
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                case OMNI_VARBINARY:
                    constantInputs.push_back(new ConstVector(std::string_view(*literalExpr->stringVal), typeId));
                    break;
                default: LogError("Do not support such vector type %d", typeId);
            }
        } else {
            constantInputs.push_back(nullptr);
        }
    }
    return constantInputs;
}

bool IsNullLiteral(const std::string &value)
{
    const std::string loweredNullValue = "null";
    if (value.size() != loweredNullValue.size()) {
        return false;
    }
    for (uint32_t i = 0; i < loweredNullValue.size(); i++) {
        if (tolower(value[i]) != loweredNullValue[i]) {
            return false;
        }
    }
    return true;
}

bool IsComparisonOperator(Operator op)
{
    return op == Operator::GT || op == Operator::GTE || op == Operator::LT || op == Operator::LTE ||
        op == Operator::EQ || op == Operator::NEQ;
}

bool IsLogicalOperator(Operator op)
{
    return op == Operator::AND || op == Operator::OR || op == Operator::NOT;
}

Operator StringToOperator(const std::string &opStr)
{
    auto opItr = OPERATOR_FROM_STRING.find(opStr);
    if (opItr != OPERATOR_FROM_STRING.end()) {
        return opItr->second;
    }
    return Operator::INVALIDOP;
}

ExprType Expr::GetType() const
{
    return ExprType::INVALID_E;
}

DataTypePtr Expr::GetReturnType() const
{
    return dataType;
}

DataTypeId Expr::GetReturnTypeId() const
{
    return dataType->GetId();
}

void Expr::DeleteExprs(const std::vector<Expr *> &exprs)
{
    for (Expr *exp : exprs) {
        delete exp;
    }
}

void Expr::DeleteExprs(const std::vector<std::vector<Expr *>> &exprs)
{
    for (const std::vector<Expr *> &expr : exprs) {
        Expr::DeleteExprs(expr);
    }
}

// Literal Expression methods
LiteralExpr::LiteralExpr() = default;

LiteralExpr::~LiteralExpr()
{
    delete stringVal;
}

ExprType LiteralExpr::GetType() const
{
    return ExprType::LITERAL_E;
}

// Helper constructors for different data types
LiteralExpr::LiteralExpr(bool val, DataTypePtr dt)
{
    dataType = std::move(dt);
    boolVal = val;
}

LiteralExpr::LiteralExpr(int8_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    byteVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(int16_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    shortVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(int32_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    intVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(int64_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    longVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(double val, DataTypePtr dt)
{
    dataType = std::move(dt);
    doubleVal = val;
}

LiteralExpr::LiteralExpr(float val, DataTypePtr dt)
{
    dataType = std::move(dt);
    floatVal = val;
}

LiteralExpr::LiteralExpr(std::string *val, DataTypePtr dt)
{
    dataType = std::move(dt);
    stringVal = val;
}

uint8_t* LiteralExpr::compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    if (dataType->GetId() != OMNI_BOOLEAN) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR", "LiteralExpr::compute Only Support Boolean.");
    }
    auto rowSize = vecBatch->GetRowCount();
    if (boolVal) {
        memset(bitMark, 0xFF, BitUtil::Nbytes(rowSize));
    } else {
        memset(bitMark, 0, BitUtil::Nbytes(rowSize));
    }
    return bitMark;
}

std::string GetBoolValOutput(const LiteralExpr &e)
{
    string output = "Literal:bool:";
    e.boolVal ? output += "true" : output += "false";
    return output;
}

std::string GetIntValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.intVal);
    return output;
}

std::string GetLongValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.longVal);
    return output;
}

std::string GetDoubleValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.doubleVal);
    return output;
}

std::string GetCharValOutput(const LiteralExpr &e)
{
    string output = "Literal:";
    if (e.GetReturnTypeId() == OMNI_CHAR) {
        // meant to look like "%s[%d]:'%s'"
        output += TypeUtil::TypeToString(e.GetReturnTypeId()) + +"[" +
                  to_string(static_cast<CharDataType *>(e.dataType.get())->GetWidth()) + "]" + ":'" + *(e.stringVal) + "'";
    } else {
        // meant to look like "%s:'%s'"
        output += TypeUtil::TypeToString(e.GetReturnTypeId()) + ":'" + *(e.stringVal) + "'";
    }
    return output;
}

std::string GetDecimal64ValOutput(const LiteralExpr &e)
{
    // meant to look like "Literal:%s(%d, %d):%ld"
    string output = "Literal:";
    output += TypeUtil::TypeToString(e.GetReturnTypeId());
    output += "(";
    output += to_string(static_cast<Decimal64DataType *>(e.dataType.get())->GetPrecision());
    output += ", ";
    output += to_string(static_cast<Decimal64DataType *>(e.dataType.get())->GetScale());
    output += "):";
    output += to_string(e.longVal);
    return output;
}

std::string GetDecimal128ValOutput(const LiteralExpr &e)
{
    // meant to look like "%s(%d, %d):'%s'"
    string output = "Literal:";
    output += TypeUtil::TypeToString(e.GetReturnTypeId());
    output += "(";
    output += to_string(static_cast<Decimal128DataType *>(e.dataType.get())->GetPrecision());
    output += ", ";
    output += to_string(static_cast<Decimal128DataType *>(e.dataType.get())->GetScale());
    output += "):";
    output += "'";
    output += *(e.stringVal);
    output += "'";
    return output;
}

std::string GetShortValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.shortVal);
    return output;
}

std::string GetByteValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.byteVal);
    return output;
}

std::string GetFloatValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.floatVal);
    return output;
}


std::string LiteralExpr::toString() const
{
    string output = "";
    switch (this->GetReturnTypeId()) {
        case OMNI_BOOLEAN:
            output += GetBoolValOutput(*this);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            output += GetIntValOutput(*this);
            break;
        case OMNI_TIMESTAMP:
        case OMNI_LONG:
            output += GetLongValOutput(*this);
            break;
        case OMNI_DOUBLE:
            output += GetDoubleValOutput(*this);
            break;
        case OMNI_CHAR:
            output += GetCharValOutput(*this);
            break;
        case OMNI_VARCHAR:
            output += GetCharValOutput(*this);
            break;
        case OMNI_DECIMAL64:
            output += GetDecimal64ValOutput(*this);
            break;
        case OMNI_DECIMAL128:
            output += GetDecimal128ValOutput(*this);
            break;
        default:
            output += "Literal:invalid DataType " + to_string(this->GetReturnTypeId());
    }
    return output;
}

// FieldExpr
FieldExpr::FieldExpr() = default;

FieldExpr::~FieldExpr()
{
    if (input != nullptr) {
        delete input;
        input = nullptr;
    }
}

ExprType FieldExpr::GetType() const
{
    return ExprType::FIELD_E;
}

// Helper constructors
FieldExpr::FieldExpr(int32_t colIdx, DataTypePtr colType)
{
    dataType = std::move(colType);
    colVal = colIdx;
}

omniruntime::vec::BaseVector *FieldExpr::GetFieldVector(omniruntime::vec::VectorBatch *vecBatch)
{
    if (!input) {
        return vecBatch->Get(colVal);
    }
    // has parent
    if (input->dataType->GetId() == OMNI_ROW) {
        auto rowVec = static_cast<FieldExpr *>(input)->GetFieldVector(vecBatch);
        auto resVec = (reinterpret_cast<omniruntime::vec::RowVector *>(rowVec))->ChildAt(this->ordinal);
        return resVec.get();
    } else {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "EQ left Only Support Field.");
    }
}

FieldExpr::FieldExpr(int32_t colIdx, DataTypePtr colType, int32_t oridinal)
{
    dataType = std::move(colType);
    colVal = colIdx;
    this->ordinal = oridinal;
}

int32_t GetColVec2(FieldExpr *expr)
{
    if (expr->input != nullptr) {
        return GetColVec2(reinterpret_cast<FieldExpr *>(expr->input));
    }
    return expr->colVal;
}

FieldExpr::FieldExpr(int32_t colIdx, DataTypePtr colType, int32_t oridinal, Expr *input)
{
    dataType = std::move(colType);
    colVal = colIdx;
    this->ordinal = oridinal;
    this->input = input;
}

bool FieldExpr::FieldIsArray()
{
    return (dataType->GetId() == DataTypeId::OMNI_ARRAY);
}

bool FieldExpr::FieldIsMap()
{
    return (dataType->GetId() == DataTypeId::OMNI_MAP);
}

BinaryExpr::BinaryExpr()
{
    dataType = BooleanType();
}

std::string GetStringOp(Operator op)
{
    switch (op) {
        case Operator::EQ:
            return "equal";
        case Operator::NEQ:
            return "notEqual";
        case Operator::LT:
            return "lessThan";
        case Operator::LTE:
            return "lessThanEqual";
        case Operator::GT:
            return "greaterThan";
        case Operator::GTE:
            return "greaterThanEqual";
        case Operator::AND:
            return "and";
        case Operator::OR:
            return "or";
        case Operator::ADD:
            return "add";
        case Operator::SUB:
            return "subtract";
        case Operator::MUL:
            return "multiply";
        case Operator::DIV:
            return "divide";
        case Operator::MOD:
            return "modulus";
        case Operator::NOT:
            return "not";
        case Operator::NEG:
            return "negative";
        case Operator::POS:
            return "positive";
        default:
            return "Invalid";
    }
}

BinaryExpr::BinaryExpr(Operator bop, Expr *leftExpr, Expr *rightExpr, DataTypePtr dt)
{
    op = bop;
    left = leftExpr;
    right = rightExpr;
    dataType = std::move(dt);
    std::vector<omniruntime::type::DataTypeId> args = {left->dataType->GetId(), right->dataType->GetId()};
    auto signature = std::make_shared<FunctionSignature>(GetStringOp(bop), args, dataType->GetId());
    vectorFunction = VectorFunction::Find(signature);
}

BinaryExpr::~BinaryExpr()
{
    delete left;
    delete right;
}

ExprType BinaryExpr::GetType() const
{
    return ExprType::BINARY_E;
}

uint8_t *BinaryExpr::computeAND(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    auto vectorSize = vecBatch->GetRowCount();

    auto bitMarkBufLeft = std::make_unique<omniruntime::mem::AlignedBuffer<uint8_t>>(BitUtil::Nbytes(vectorSize) + 8,
        true);
    uint8_t *leftResult = left->compute(vecBatch, bitMarkBufLeft->GetBuffer());

    auto bitMarkBufRight = std::make_unique<omniruntime::mem::AlignedBuffer<uint8_t>>(BitUtil::Nbytes(vectorSize) + 8,
        true);
    uint8_t *rightResult = right->compute(vecBatch, bitMarkBufRight->GetBuffer());

    int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
    int32_t byteLen = BitUtil::Nbytes(vectorSize);
    int32_t index = 0;
    for (; index + step <= byteLen; index += step) {
        uint8x16_t leftBatch = vld1q_u8(leftResult + index);
        uint8x16_t rightBatch = vld1q_u8(rightResult + index);
        uint8x16_t result = leftBatch & rightBatch;
        vst1q_u8(bitMark + index, result);
    }
    for (; index < byteLen; index++) {
        bitMark[index] = leftResult[index] & rightResult[index];
    }
    return bitMark;
}

uint8_t *BinaryExpr::computeOR(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    auto vectorSize = vecBatch->GetRowCount();

    auto bitMarkBufLeft = std::make_unique<omniruntime::mem::AlignedBuffer<uint8_t>>(BitUtil::Nbytes(vectorSize) + 8,
        true);
    uint8_t *leftResult = left->compute(vecBatch, bitMarkBufLeft->GetBuffer());

    auto bitMarkBufRight = std::make_unique<omniruntime::mem::AlignedBuffer<uint8_t>>(BitUtil::Nbytes(vectorSize) + 8,
        true);
    uint8_t *rightResult = right->compute(vecBatch, bitMarkBufRight->GetBuffer());

    int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
    int32_t byteLen = BitUtil::Nbytes(vectorSize);
    int32_t index = 0;
    for (; index + step <= byteLen; index += step) {
        uint8x16_t leftBatch = vld1q_u8(leftResult + index);
        uint8x16_t rightBatch = vld1q_u8(rightResult + index);
        uint8x16_t result = leftBatch | rightBatch;
        vst1q_u8(bitMark + index, result);
    }
    for (; index < byteLen; index++) {
        bitMark[index] = leftResult[index] | rightResult[index];
    }
    return bitMark;
}

bool ALWAYS_INLINE IsEqual(std::string_view src, std::string *target)
{
    return src == *target;
}

uint8_t *BinaryExpr::computeEQ(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    auto field = dynamic_cast<FieldExpr *>(left);
    if (!field) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "EQ left Only Support Field.");
    }

    auto literal = dynamic_cast<LiteralExpr *>(right);
    if (!literal) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "EQ right Only Support literal.");
    }

    auto vector = field->GetFieldVector(vecBatch);
    auto rows = vector->GetSize();
    auto dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            auto target = literal->stringVal;
            if (target == nullptr) {
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                    "EQ Not Support stringVal is null.");
            }
            if (vector->GetEncoding() == vec::OMNI_FLAT) {
                auto varCharVector = reinterpret_cast<vec::Vector<vec::LargeStringContainer<std::string_view>> *>(
                    vector);
                for (int index = 0; index < rows; index++) {
                    bool result = vector->IsNull(index) ? 0 : IsEqual(varCharVector->GetValue(index), target);
                    BitUtil::SetBit(bitMark, index, result);
                }
            } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto dictVarchar = reinterpret_cast<vec::Vector<vec::DictionaryContainer<std::string_view,
                    vec::LargeStringContainer>> *>(vector);
                for (int index = 0; index < rows; index++) {
                    bool result = vector->IsNull(index) ? 0 : IsEqual(dictVarchar->GetValue(index), target);
                    BitUtil::SetBit(bitMark, index, result);
                }
            } else {
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                    "EQ Not Support ENCODING for varchar : " + std::to_string(static_cast<int>(dataTypeId)));
            }
            break;
        }
        default:
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                "EQ Not Support Type : " + std::to_string(static_cast<int>(dataTypeId)));
    }

    return bitMark;
}

uint8_t *BinaryExpr::computeNEQ(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    auto field = dynamic_cast<FieldExpr *>(left);
    if (!field) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "NEQ left Only Support Field.");
    }

    auto literal = dynamic_cast<LiteralExpr *>(right);
    if (!literal) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "NEQ right Only Support literal.");
    }

    auto vector = field->GetFieldVector(vecBatch);
    auto rows = vector->GetSize();
    auto dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            auto target = literal->stringVal;
            if (target == nullptr) {
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                    "NEQ Not Support stringVal is null.");
            }
            if (vector->GetEncoding() == vec::OMNI_FLAT) {
                auto varCharVector = reinterpret_cast<vec::Vector<vec::LargeStringContainer<std::string_view>> *>(
                    vector);
                for (int index = 0; index < rows; index++) {
                    bool result = vector->IsNull(index) ? 0 : IsEqual(varCharVector->GetValue(index), target);
                    BitUtil::SetBit(bitMark, index, !result);
                }
            } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto dictVarchar = reinterpret_cast<vec::Vector<vec::DictionaryContainer<std::string_view,
                    vec::LargeStringContainer>> *>(vector);
                for (int index = 0; index < rows; index++) {
                    bool result = vector->IsNull(index) ? 0 : IsEqual(dictVarchar->GetValue(index), target);
                    BitUtil::SetBit(bitMark, index, !result);
                }
            } else {
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                    "NEQ Not Support ENCODING for varchar : " + std::to_string(static_cast<int>(dataTypeId)));
            }
            break;
        }
        default:
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                "NEQ Not Support Type : " + std::to_string(static_cast<int>(dataTypeId)));
    }

    return bitMark;
}

uint8_t *BinaryExpr::compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    switch (op) {
        case Operator::EQ:
            return computeEQ(vecBatch, bitMark);
        case Operator::AND:
            return computeAND(vecBatch, bitMark);
        case Operator::OR:
            return computeOR(vecBatch, bitMark);
        case Operator::NEQ:
            return computeNEQ(vecBatch, bitMark);
        default:
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                "BinaryExpr Not Support: " + std::to_string(static_cast<int>(op)));
    }
}

string BinaryExprPrinterHelper(const Operator &op, const DataType &type)
{
    string typeStr = TypeUtil::TypeToString(type.GetId());
    if (TypeUtil::IsDecimalType(type.GetId())) {
        typeStr += "(";
        typeStr += to_string(static_cast<const DecimalDataType &>(type).GetPrecision());
        typeStr += ", ";
        typeStr += to_string(static_cast<const DecimalDataType &>(type).GetScale());
        typeStr += ")";
    }

    switch (op) {
        case Operator::EQ:
            return "Cmp:" + typeStr + "(EQ ";
        case Operator::NEQ:
            return "Cmp:" + typeStr + "(NEQ ";
        case Operator::LT:
            return "Cmp:" + typeStr + "(LT ";
        case Operator::LTE:
            return "Cmp:" + typeStr + "(LTE ";
        case Operator::GT:
            return "Cmp:" + typeStr + "(GT ";
        case Operator::GTE:
            return "Cmp:" + typeStr + "(GTE ";
        case Operator::AND:
            return "Bin:" + typeStr + "(AND ";
        case Operator::OR:
            return "Bin:" + typeStr + "(OR ";
        case Operator::ADD:
            return "Arith:" + typeStr + "(ADD ";
        case Operator::SUB:
            return "Arith:" + typeStr + "(SUB ";
        case Operator::MUL:
            return "Arith:" + typeStr + "(MUL ";
        case Operator::DIV:
            return "Arith:" + typeStr + "(DIV ";
        case Operator::MOD:
            return "Arith:" + typeStr + "(MOD ";
        default:
            return "Invalid";
    }
}

std::string BinaryExpr::toString() const
{
    std::string indent = "";
    std::string message = BinaryExprPrinterHelper(this->op, *(this->GetReturnType()));
    if (message == "Invalid") {
        message = "InvalidBinaryOperator:" + std::to_string(static_cast<int32_t>(this->op)) + "(";
    }
    message = indent + message;

    message += this->left->toString();

    message += this->right->toString();
    message += indent + ")";
    return message;
}

UnaryExpr::UnaryExpr()
{
    dataType = BooleanType();
}

UnaryExpr::UnaryExpr(Operator logOp, Expr *bodyExpr) : op(logOp), exp(bodyExpr) {}

UnaryExpr::UnaryExpr(Operator uop, Expr *expr, DataTypePtr dt) : op(uop), exp(expr)
{
    dataType = std::move(dt);
    std::vector<omniruntime::type::DataTypeId> args = {exp->dataType->GetId()};
    auto signature = std::make_shared<FunctionSignature>(GetStringOp(op), args, dataType->GetId());
    vectorFunction = VectorFunction::Find(signature);
}

UnaryExpr::~UnaryExpr()
{
    delete exp;
}

ExprType UnaryExpr::GetType() const
{
    return ExprType::UNARY_E;
}

uint8_t *UnaryExpr::computeNOT(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    auto childExpr = dynamic_cast<IsNullExpr *>(this->exp);
    if (!childExpr) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "Not Only Support isNullExpr.");
    }
    auto vectorSize = vecBatch->GetRowCount();
    uint8_t *childResult = childExpr->compute(vecBatch, bitMark);
    int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
    int32_t byteLen = BitUtil::Nbytes(vectorSize);
    int32_t index = 0;
    for (; index + step <= byteLen; index += step) {
        uint8x16_t valuesBatch = vld1q_u8(childResult + index);
        uint8x16_t result = ~valuesBatch;
        vst1q_u8(bitMark + index, result);
    }
    for (; index < byteLen; index++) {
        bitMark[index] = ~childResult[index];
    }
    return bitMark;
}

uint8_t *UnaryExpr::compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    switch (op) {
        case Operator::NOT:
            return computeNOT(vecBatch, bitMark);
        default:
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                "UnaryExpr Not Support: " + std::to_string(static_cast<int>(op)));
    }
}

InExpr::InExpr()
{
    dataType = BooleanType();
}

InExpr::~InExpr()
{
    DeleteExprs(arguments);
    for (auto *vec : constantInputs){
        if(vec != nullptr){
            delete vec;
        }
    }
    constantInputs.clear();
}

InExpr::InExpr(std::vector<Expr *> args)
{
    dataType = BooleanType();
    arguments = std::move(args);

    auto inputDataType = arguments[0]->dataType;
    auto vector = VectorHelper::CreateFlatVectorShared(inputDataType->GetId(), arguments.size()-1);
    for (int i = 1; i < arguments.size(); i++)
    {
        auto literalExpr = dynamic_cast<LiteralExpr *>(arguments[i]);
        if (literalExpr && !literalExpr->isNull) {
            auto typeId = arguments[i]->dataType->GetId();
            switch (typeId) {
                case OMNI_BOOLEAN:
                    dynamic_cast<Vector<bool> *>(vector.get())->SetValue(i-1, literalExpr->boolVal);
                    break;
                case OMNI_BYTE:
                    dynamic_cast<Vector<int8_t> *>(vector.get())->SetValue(i-1, literalExpr->byteVal);
                    break;
                case OMNI_SHORT:
                    dynamic_cast<Vector<int16_t> *>(vector.get())->SetValue(i-1, literalExpr->shortVal);
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                    dynamic_cast<Vector<int32_t> *>(vector.get())->SetValue(i-1, literalExpr->intVal);
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    dynamic_cast<Vector<int64_t> *>(vector.get())->SetValue(i-1, literalExpr->longVal);
                    break;
                case OMNI_FLOAT:
                    dynamic_cast<Vector<float> *>(vector.get())->SetValue(i-1, literalExpr->floatVal);
                    break;
                case OMNI_DOUBLE:
                    dynamic_cast<Vector<double> *>(vector.get())->SetValue(i-1, literalExpr->doubleVal);
                    break;
                case OMNI_VARCHAR:
                    dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(vector.get())->SetValue(i-1, *literalExpr->stringVal);
                    break;
                case OMNI_DECIMAL128:
                    dynamic_cast<Vector<Decimal128> *>(vector.get())->SetValue(i-1, Decimal128(*literalExpr->stringVal));
                    break;
                default: LogError("Do not support such vector type %d", typeId);
            }
        } else {
            vector-> SetNull(i-1);
        }
    }
    ArrayType arrayType(inputDataType);
    auto arrayVector = VectorHelper::CreateComplexVector(&arrayType, 1);
    dynamic_cast<ArrayVector *>(arrayVector)->SetValue(0, vector.get());
    std::vector<DataTypeId> argTypes = {inputDataType->GetId(), OMNI_ARRAY};
    auto signature = std::make_shared<FunctionSignature>("in", argTypes, dataType->GetId());
    constantInputs = {nullptr, arrayVector};
    vectorFunction = VectorFunction::Find(signature, constantInputs);
    if (vectorFunction == nullptr) {
        vectorFunction = VectorFunction::Find(signature);
    }
}

ExprType InExpr::GetType() const
{
    return ExprType::IN_E;
}

BetweenExpr::BetweenExpr()
{
    dataType = BooleanType();
}

BetweenExpr::~BetweenExpr()
{
    delete value;
    delete lowerBound;
    delete upperBound;
}

BetweenExpr::BetweenExpr(Expr *val, Expr *lowBound, Expr *upBound)
{
    dataType = BooleanType();
    value = val;
    lowerBound = lowBound;
    upperBound = upBound;
}

ExprType BetweenExpr::GetType() const
{
    return ExprType::BETWEEN_E;
}

SwitchExpr::SwitchExpr() : whenClause(), falseExpr() {}

SwitchExpr::~SwitchExpr()
{
    for (std::pair<Expr *, Expr *> &vec : whenClause) {
        delete vec.first;
        delete vec.second;
    }
    delete falseExpr;
}

SwitchExpr::SwitchExpr(const std::vector<std::pair<Expr *, Expr *>> &whens, Expr *fexp)
{
    dataType = fexp->GetReturnType();
    whenClause = whens;
    falseExpr = fexp;
}

ExprType SwitchExpr::GetType() const
{
    return ExprType::SWITCH_E;
}

IfExpr::IfExpr() : condition(), trueExpr(), falseExpr() {}

IfExpr::~IfExpr()
{
    delete condition;
    delete trueExpr;
    delete falseExpr;
}

IfExpr::IfExpr(Expr *cond, Expr *texp, Expr *fexp)
{
    dataType = texp->GetReturnType();
    condition = cond;
    trueExpr = texp;
    falseExpr = fexp;
    arguments = {cond, texp, fexp};
    std::vector<omniruntime::type::DataTypeId> args = {condition->dataType->GetId(),
        trueExpr->dataType->GetId(), falseExpr->dataType->GetId()};
    auto signature = std::make_shared<FunctionSignature>("if", args, dataType->GetId());
    vectorFunction = VectorFunction::Find(signature);
}

ExprType IfExpr::GetType() const
{
    return ExprType::IF_E;
}

CoalesceExpr::CoalesceExpr() : value1(), value2() {}

CoalesceExpr::~CoalesceExpr()
{
    delete value1;
    delete value2;
}

CoalesceExpr::CoalesceExpr(Expr *val1, Expr *val2)
{
    dataType = val1->GetReturnType();
    value1 = val1;
    value2 = val2;
    arguments = {val1, val2};
    std::vector<DataTypeId> argTypes(arguments.size());
    std::transform(arguments.begin(), arguments.end(), argTypes.begin(), [](Expr *expr) -> DataTypeId {
        return expr->GetReturnTypeId();
    });
    auto signature = std::make_shared<FunctionSignature>("coalesce", argTypes, dataType->GetId());
    vectorFunction = VectorFunction::Find(signature);
}

ExprType CoalesceExpr::GetType() const
{
    return ExprType::COALESCE_E;
}

IsNullExpr::IsNullExpr() : value() {}

IsNullExpr::~IsNullExpr()
{
    delete value;
}

IsNullExpr::IsNullExpr(Expr *value)
{
    dataType = BooleanType();
    std::vector<omniruntime::type::DataTypeId> args = {value->dataType->GetId()};
    auto signature = std::make_shared<FunctionSignature>("isnull", args, dataType->GetId());
    vectorFunction = VectorFunction::Find(signature);
    this->value = value;
}

uint8_t *IsNullExpr::compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    auto expr = dynamic_cast<FieldExpr *>(value);
    if (!expr) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "IsNullExpr Only Support Field.");
    }
    auto vector = expr->GetFieldVector(vecBatch);
    auto rowSize = vecBatch->GetRowCount();
    if (vector->HasNull()) {
        memcpy(bitMark, vec::unsafe::UnsafeBaseVector::GetNulls(vector), BitUtil::Nbytes(rowSize));
    } else {
        memset(bitMark, 0, BitUtil::Nbytes(rowSize));
    }
    return bitMark;
}

ExprType IsNullExpr::GetType() const
{
    return ExprType::IS_NULL_E;
}

FuncExpr::FuncExpr() : function(nullptr) {}

FuncExpr::~FuncExpr()
{
    DeleteExprs(arguments);
    for (auto *vec : constantInputs){
        if(vec != nullptr){
            delete vec;
        }
    }
    constantInputs.clear();
}

FuncExpr::FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType)
    : funcName(fnName), arguments(args), functionType(BUILTIN)
{
    dataType = std::move(returnType);
    std::vector<DataTypeId> argTypes(arguments.size());
    std::transform(arguments.begin(), arguments.end(), argTypes.begin(), [](Expr *expr) -> DataTypeId {
        return expr->GetReturnTypeId();
    });
    auto signature = std::make_shared<FunctionSignature>(funcName, argTypes, dataType->GetId());
    this->function = FunctionRegistry::LookupFunction(signature.get());
    constantInputs = GetConstantInputs(arguments);
    vectorFunction = VectorFunction::Find(signature, constantInputs);
    if (vectorFunction == nullptr) {
        vectorFunction = VectorFunction::Find(signature);
    }
    if (funcName == "named_struct") {
        vectorFunction = std::make_shared<NamedStructFunction>(GetInputDataTypes());
    }
    if (funcName == "map") {
        vectorFunction = std::make_shared<MapFunction>(GetInputDataTypes());
    }
    if (funcName == "concat_ws") {
        vectorFunction = std::make_shared<ConcatWsFunction>(GetInputDataTypes());
    }
    if (funcName == "CAST") {
        vectorFunction = std::make_shared<CastFunction>(args[0]->dataType, dataType);
    }
    if (!arguments.empty() && arguments[0]->dataType != nullptr &&
        vectorization::IsDecimalTypeId(arguments[0]->dataType->GetId())) {
        std::vector<DataTypePtr> decArgTypes;
        decArgTypes.reserve(arguments.size());
        for (auto *a : arguments) {
            decArgTypes.push_back(a->dataType);
        }
        if (auto decMathFn = vectorization::TryCreateDecimalMathFunction(funcName, decArgTypes)) {
            vectorFunction = decMathFn;
        } else if (auto decToDecFn = vectorization::TryCreateDecimalToDecimalFunction(funcName, decArgTypes)) {
            // Scale-preserving DECIMAL -> DECIMAL (sign): output type equals the operand type.
            vectorFunction = decToDecFn;
        }
    }
}

FuncExpr::FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType,
    const Function *function)
    : funcName(fnName), arguments(args), function(function), functionType(BUILTIN)
{
    dataType = std::move(returnType);
    std::vector<DataTypeId> argTypes(arguments.size());
    std::transform(arguments.begin(), arguments.end(), argTypes.begin(), [](Expr *expr) -> DataTypeId {
        return expr->GetReturnTypeId();
    });
    auto signature = std::make_shared<FunctionSignature>(funcName, argTypes, dataType->GetId());
    if (this->function == nullptr) {
        this->function = FunctionRegistry::LookupFunction(signature.get());
    }
    vectorFunction = VectorFunction::Find(signature);
    if (funcName == "named_struct") {
        vectorFunction = std::make_shared<NamedStructFunction>(GetInputDataTypes());
    }
    if (funcName == "map") {
        vectorFunction = std::make_shared<MapFunction>(GetInputDataTypes());
    }
    if (funcName == "concat_ws") {
        vectorFunction = std::make_shared<ConcatWsFunction>(GetInputDataTypes());
    }
    if (funcName == "CAST") {
        vectorFunction = std::make_shared<CastFunction>(args[0]->dataType, dataType);
    }
    if (!arguments.empty() && arguments[0]->dataType != nullptr &&
        vectorization::IsDecimalTypeId(arguments[0]->dataType->GetId())) {
        std::vector<DataTypePtr> decArgTypes;
        decArgTypes.reserve(arguments.size());
        for (auto *a : arguments) {
            decArgTypes.push_back(a->dataType);
        }
        if (auto decMathFn = vectorization::TryCreateDecimalMathFunction(funcName, decArgTypes)) {
            vectorFunction = decMathFn;
        } else if (auto decToDecFn = vectorization::TryCreateDecimalToDecimalFunction(funcName, decArgTypes)) {
            // Scale-preserving DECIMAL -> DECIMAL (sign): output type equals the operand type.
            vectorFunction = decToDecFn;
        }
    }
}

FuncExpr::FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType,
    ExprFunctionType functionType)
    : funcName(fnName), arguments(args), function(nullptr), functionType(functionType)
{
    dataType = std::move(returnType);
    std::vector<DataTypeId> argTypes(arguments.size());
    std::transform(arguments.begin(), arguments.end(), argTypes.begin(), [](Expr *expr) -> DataTypeId {
        return expr->GetReturnTypeId();
    });
    auto signature = std::make_shared<FunctionSignature>(funcName, argTypes, dataType->GetId());
    this->function = FunctionRegistry::LookupFunction(signature.get());
    vectorFunction = VectorFunction::Find(signature);
    if (funcName == "named_struct") {
        vectorFunction = std::make_shared<NamedStructFunction>(GetInputDataTypes());
    }
    if (funcName == "map") {
        vectorFunction = std::make_shared<MapFunction>(GetInputDataTypes());
    }
    if (funcName == "concat_ws") {
        vectorFunction = std::make_shared<ConcatWsFunction>(GetInputDataTypes());
    }
    if (funcName == "CAST") {
        vectorFunction = std::make_shared<CastFunction>(args[0]->dataType, dataType);
    }
    if (!arguments.empty() && arguments[0]->dataType != nullptr &&
        vectorization::IsDecimalTypeId(arguments[0]->dataType->GetId())) {
        std::vector<DataTypePtr> decArgTypes;
        decArgTypes.reserve(arguments.size());
        for (auto *a : arguments) {
            decArgTypes.push_back(a->dataType);
        }
        if (auto decMathFn = vectorization::TryCreateDecimalMathFunction(funcName, decArgTypes)) {
            vectorFunction = decMathFn;
        } else if (auto decToDecFn = vectorization::TryCreateDecimalToDecimalFunction(funcName, decArgTypes)) {
            // Scale-preserving DECIMAL -> DECIMAL (sign): output type equals the operand type.
            vectorFunction = decToDecFn;
        }
    }
}

ExprType FuncExpr::GetType() const
{
    return ExprType::FUNC_E;
}

bool ALWAYS_INLINE RLikeStr(std::string_view src, std::wregex re)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return std::regex_search(convert.from_bytes(std::string(src)), re);
}

uint8_t *FuncExpr::compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark)
{
    if (funcName == "RLike") {
        auto expr = dynamic_cast<FieldExpr *>(arguments[0]);
        auto pattern = dynamic_cast<LiteralExpr *>(arguments[1]);
        if (!expr || !pattern || !pattern->stringVal) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "RLike args error");
        }
        auto vector = expr->GetFieldVector(vecBatch);
        auto rows = vecBatch->GetRowCount();

        auto dataTypeId = vector->GetTypeId();
        switch (dataTypeId) {
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                auto target = pattern->stringVal;
                std::wregex re(convert.from_bytes(*target));
                if (vector->GetEncoding() == vec::OMNI_FLAT) {
                    auto varCharVector = reinterpret_cast<vec::Vector<vec::LargeStringContainer<std::string_view>> *>(
                        vector);
                    for (int index = 0; index < rows; index++) {
                        bool result = vector->IsNull(index) ? 0 : RLikeStr(varCharVector->GetValue(index), re);
                        BitUtil::SetBit(bitMark, index, result);
                    }
                } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                    auto dictVarchar = reinterpret_cast<vec::Vector<vec::DictionaryContainer<std::string_view,
                        vec::LargeStringContainer>> *>(vector);
                    for (int index = 0; index < rows; index++) {
                        bool result = vector->IsNull(index) ? 0 : RLikeStr(dictVarchar->GetValue(index), re);
                        BitUtil::SetBit(bitMark, index, result);
                    }
                } else {
                    throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                        "RLike Not Support ENCODING for varchar : " + std::to_string(static_cast<int>(dataTypeId)));
                }
                break;
            }
            default:
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                    "RLike Not Support Type : " + std::to_string(static_cast<int>(dataTypeId)));
        }
        return bitMark;
    } else {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "FuncExpr Not Support: " + funcName);
    }
}

ParamRefExpr::ParamRefExpr() {}

ParamRefExpr::ParamRefExpr(std::string paramName, DataTypePtr dt) {
    paramName_ = std::move(paramName);
    dataType = std::move(dt);
}

ExprType ParamRefExpr::GetType() const
{
    return ExprType::PARAM_REF_E;
}

LambdaExpr::LambdaExpr() {}

LambdaExpr::LambdaExpr(omniruntime::expressions::Expr *body, std::vector<DataTypePtr> paramTypes,
                       const std::unordered_map<std::string, int32_t>& map, DataTypePtr dt) {
    dataType = std::move(dt);
    body_ = body;
    paramTypes_ = std::move(paramTypes);
    paramNameToIdxMap_ = map;
}

LambdaExpr::~LambdaExpr() {
    delete body_;
}

ExprType LambdaExpr::GetType() const
{
    return ExprType::LAMBDA_E;
}
}
}
