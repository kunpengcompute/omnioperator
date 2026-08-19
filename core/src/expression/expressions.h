/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef __EXPRESSIONS_H__
#define __EXPRESSIONS_H__

#include <codegen/function.h>
#include <map>
#include <string>
#include <vector>
#include "type/data_type.h"
#include "type/decimal128.h"
#include "expression/arithmetic_spec.h"
#include <locale>
#include <regex>
#include <codecvt>
#include "vector/vector_common.h"
#include "vectorization/VectorFunction.h"
#include "codegen/bloom_filter.h"
#include "memory/aligned_buffer.h"

class ExprVisitor;

namespace omniruntime {
namespace expressions {
using namespace type;
using namespace codegen;
using namespace vectorization;

enum class Operator {
    // Comparison
    EQ,
    NEQ,
    LT,
    LTE,
    GT,
    GTE,
    // Logical
    AND,
    OR,
    NOT,
    NEG,
    POS,
    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    TRY_ADD,
    TRY_SUB,
    TRY_MUL,
    TRY_DIV,
    INVALIDOP
};

enum class OperatorType { COMPARISON, LOGICAL, ARITHMETIC, INVALIDOPTYPE };

enum class ExprType {
    LITERAL_E,
    FIELD_E,
    BINARY_E,
    UNARY_E,
    IN_E,
    BETWEEN_E,
    IF_E,
    SWITCH_E,
    COALESCE_E,
    IS_NULL_E,
    FUNC_E,
    LAMBDA_E,
    PARAM_REF_E,
    INVALID_E,
};

const std::map<std::string, Operator> OPERATOR_FROM_STRING = {{"EQUAL", Operator::EQ}, {"LESS_THAN", Operator::LT},
    {"LESS_THAN_OR_EQUAL", Operator::LTE}, {"GREATER_THAN_OR_EQUAL", Operator::GTE}, {"GREATER_THAN", Operator::GT},
    {"NOT_EQUAL", Operator::NEQ}, {"AND", Operator::AND}, {"OR", Operator::OR}, {"NOT", Operator::NOT},
    {"not", Operator::NOT}, {"POSITIVE", Operator::POS}, {"NEGATION", Operator::NEG}, {"ADD", Operator::ADD},
    {"SUBTRACT", Operator::SUB}, {"MULTIPLY", Operator::MUL}, {"DIVIDE", Operator::DIV}, {"MODULUS", Operator::MOD},
    {"TRY_ADD", Operator::TRY_ADD}, {"TRY_SUBTRACT", Operator::TRY_SUB}, {"TRY_MULTIPLY", Operator::TRY_MUL},
    {"TRY_DIVIDE", Operator::TRY_DIV}};

bool IsNullLiteral(const std::string &value);
bool IsComparisonOperator(Operator op);
bool IsLogicalOperator(Operator op);
Operator StringToOperator(const std::string &opStr);

enum ExprFunctionType { BUILTIN = 0, HIVE_UDF };

class Expr {
public:
    DataTypePtr dataType; // dataType of returned value
    std::shared_ptr<VectorFunction> vectorFunction;
    bool isAllSupportVectorization_ = true;
    bool isRoot = false;

    DataTypePtr GetReturnType() const;
    omniruntime::type::DataTypeId GetReturnTypeId() const;
    virtual ExprType GetType() const;
    virtual ~Expr() = default;
    virtual void Accept(ExprVisitor &visitor) const = 0;
    static void DeleteExprs(const std::vector<Expr *> &exprs);
    static void DeleteExprs(const std::vector<std::vector<Expr *>> &exprs);

    virtual uint8_t *compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark) { return nullptr; }

    virtual bool supportVectorized() const { return false; }

    virtual std::string toString() const
    {
        OMNI_THROW("RUNTIME_ERROR:", "not implementation expr!");
    };
};

class LiteralExpr;

std::string GetBoolValOutput(const LiteralExpr &e);

std::string GetIntValOutput(const LiteralExpr &e);

std::string GetLongValOutput(const LiteralExpr &e);

std::string GetDoubleValOutput(const LiteralExpr &e);

std::string GetCharValOutput(const LiteralExpr &e);

std::string GetDecimal64ValOutput(const LiteralExpr &e);

std::string GetDecimal128ValOutput(const LiteralExpr &e);

std::string GetShortValOutput(const LiteralExpr &e);

std::string GetByteValOutput(const LiteralExpr &e);

std::string GetFloatValOutput(const LiteralExpr &e);

class LiteralExpr : public Expr {
public:
    bool isNull = false;
    bool boolVal = false;
    int8_t byteVal = 0;
    int16_t shortVal = 0;
    int32_t intVal = 0;
    int64_t longVal = 0;
    double doubleVal = 0;
    float floatVal = 0;
    std::string *stringVal = nullptr;

    LiteralExpr();
    ~LiteralExpr() override;
    explicit LiteralExpr(bool val, DataTypePtr colType);
    explicit LiteralExpr(int8_t val, DataTypePtr colType, bool isNull = false);
    explicit LiteralExpr(int16_t val, DataTypePtr colType, bool isNull = false);
    explicit LiteralExpr(int32_t val, DataTypePtr colType, bool isNull = false);
    explicit LiteralExpr(int64_t val, DataTypePtr colType, bool isNulls = false);
    explicit LiteralExpr(double val, DataTypePtr colType);
    explicit LiteralExpr(float val, DataTypePtr colType);
    explicit LiteralExpr(std::string *val, DataTypePtr colType);
    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;
    uint8_t *compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark) override;
    bool supportVectorized() const override
    {
        return true;
    }
    LiteralExpr* Copy()
    {
        auto newExpr = new LiteralExpr();
        newExpr->dataType = dataType;
        newExpr->isNull = isNull;
        newExpr->boolVal = boolVal;
        newExpr->shortVal = shortVal;
        newExpr->intVal = intVal;
        newExpr->longVal = longVal;
        newExpr->doubleVal = doubleVal;
        newExpr->stringVal = (stringVal != nullptr) ? new std::string(*stringVal) : nullptr;
        return newExpr;
    };
    std::string toString() const override;
};

class FieldExpr : public Expr {
public:
    bool isNull = false;
    int32_t colVal = 0;
    int32_t oColVal = -1;
    int32_t ordinal = -1;
    Expr* input = nullptr;

    bool operator==(const FieldExpr &other) const
    {
        return isNull == other.isNull && colVal == other.colVal &&
               ordinal == other.ordinal && input == other.input;
    }

    FieldExpr();
    ~FieldExpr() override;
    FieldExpr(int32_t colIdx, DataTypePtr colType);
    FieldExpr(int32_t colIdx, DataTypePtr colType, int32_t oridinal);
    FieldExpr(int32_t colIdx, DataTypePtr colType, int32_t oridinal, Expr* input);
    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    omniruntime::vec::BaseVector *GetFieldVector(omniruntime::vec::VectorBatch *vecBatch);

    bool supportVectorized() const override
    {
        return true;
    }

    bool FieldIsArray();
    bool FieldIsMap();

    std::string toString() const override
    {
        std::string output = "Field:";
        output += TypeUtil::TypeToString(this->GetReturnTypeId());
        if (this->GetReturnTypeId() == OMNI_CHAR) {
            output += '[' + std::to_string(static_cast<CharDataType &>(*(this->GetReturnType())).GetWidth()) + ']';
        } else if (this->GetReturnTypeId() == OMNI_DECIMAL64 || this->GetReturnTypeId() == OMNI_DECIMAL128) {
            output += "(";
            output += std::to_string(static_cast<DecimalDataType *>(this->dataType.get())->GetPrecision());
            output += ", ";
            output += std::to_string(static_cast<DecimalDataType *>(this->dataType.get())->GetScale());
            output += ")";
        }
        output += ":#" + std::to_string(this->colVal);
        return output;
    }
};

class UnaryExpr : public Expr {
public:
    Operator op = Operator::EQ;
    Expr *exp = nullptr;

    UnaryExpr();
    ~UnaryExpr() override;
    UnaryExpr(Operator logOp, Expr *bodyExpr);
    UnaryExpr(Operator uop, Expr *expr, DataTypePtr dt);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    uint8_t* computeNOT(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark);
    uint8_t *compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark) override;

    bool supportVectorized() const override
    {
        return vectorFunction != nullptr && exp->supportVectorized();
    }

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent;
        switch (this->op) {
            case Operator::NOT:
                output += "Unary:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(NOT ";
                break;
            case Operator::NEG:
                output += "Unary:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(NEGATIVE ";
                break;
            case Operator::POS:
                output += "Unary:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(POSITIVE ";
                break;   
            default:
                output += "InvalidUnaryOperator:" + std::to_string(static_cast<int32_t>(this->op)) + "(";
                break;
        }
        output += this->exp->toString();
        output += ")";
        return output;
    }
};

class BinaryExpr : public Expr {
public:
    Operator op = Operator::EQ;
    ArithmeticOp arithmeticOp = ArithmeticOp::INVALID;
    ArithmeticEvalMode evalMode = ArithmeticEvalMode::LEGACY;
    mutable std::shared_ptr<VectorFunction> checkedArithmeticVectorFunction;
    Expr *left = nullptr;
    Expr *right = nullptr;

    BinaryExpr();
    ~BinaryExpr() override;
    BinaryExpr(Operator bop, Expr *leftExpr, Expr *rightExpr, DataTypePtr dt);
    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;
    bool SupportsCheckedArithmeticVectorization() const;

    uint8_t* computeEQ(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark);
    uint8_t* computeAND(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark);
    uint8_t* computeOR(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark);
    uint8_t* computeNEQ(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark);
    uint8_t *compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark) override;

    bool supportVectorized() const override
    {
        return (vectorFunction != nullptr || SupportsCheckedArithmeticVectorization()) &&
            left->supportVectorized() && right->supportVectorized();
    }

    std::string toString() const override;
};

class InExpr : public Expr {
public:
    // first element of arguments is the value to be compared to every other argument
    std::vector<Expr *> arguments;
    std::shared_ptr<vec::RowVector> constantRowVec;
    std::vector<vec::BaseVector *> constantInputs;

    InExpr();
    ~InExpr() override;
    explicit InExpr(std::vector<Expr *> args);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    bool supportVectorized() const override
    {
        return vectorFunction != nullptr && !arguments.empty() && arguments[0]->supportVectorized();
    }

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent + "In:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(";
        for (uint32_t i = 0; i < this->arguments.size(); i++) {
            output += (this->arguments[i])->toString();
        }
        output += ")";
        return output;
    }
};

class BetweenExpr : public Expr {
public:
    Expr *value = nullptr;
    Expr *lowerBound = nullptr;
    Expr *upperBound = nullptr;

    BetweenExpr();
    ~BetweenExpr() override;
    BetweenExpr(Expr *val, Expr *lowBound, Expr *upBound);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent + "Between:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(";
        output += this->value->toString();

        output += this->lowerBound->toString();

        output += this->upperBound->toString();
        output += ")";
        return output;
    }
};

class SwitchExpr : public Expr {
public:
    std::vector<std::pair<Expr *, Expr *>> whenClause;
    Expr *falseExpr = nullptr;
    SwitchExpr();
    ~SwitchExpr() override;
    SwitchExpr(const std::vector<std::pair<Expr *, Expr *>> &whens, Expr *fexp);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent + "Switch:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(";
        for (const auto &i : this->whenClause) {
            output += i.first->toString();
            output += i.second->toString();
        }
        output += this->falseExpr->toString();
        output += ")";
        return output;
    }
};

class IfExpr : public Expr {
public:
    Expr *condition = nullptr;
    Expr *trueExpr = nullptr;
    Expr *falseExpr = nullptr;
    std::vector<Expr *> arguments;

    IfExpr();
    ~IfExpr() override;
    IfExpr(Expr *cond, Expr *texp, Expr *fexp);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    bool supportVectorized() const override
    {
        return vectorFunction != nullptr && condition->supportVectorized() &&
            trueExpr->supportVectorized() && falseExpr->supportVectorized();
    }

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent + "If:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(";
        output += this->condition->toString();

        output += this->trueExpr->toString();

        output += this->falseExpr->toString();

        output += ")";
        return output;
    }
};

class CoalesceExpr : public Expr {
public:
    Expr *value1 = nullptr;
    Expr *value2 = nullptr;
    std::vector<Expr *> arguments;
    CoalesceExpr();
    ~CoalesceExpr() override;
    CoalesceExpr(Expr *val1, Expr *val2);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    bool supportVectorized() const override
    {
        return vectorFunction != nullptr && value1->supportVectorized() && value2->supportVectorized();
    }

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent + "Coalesce:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(";
        output += this->value1->toString();
        output += this->value2->toString();
        output += ")";
        return output;
    }
};

class IsNullExpr : public Expr {
public:
    Expr *value = nullptr;
    IsNullExpr();
    ~IsNullExpr() override;
    explicit IsNullExpr(Expr *value);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    uint8_t *compute(omniruntime::vec::VectorBatch *vecBatch, uint8_t *bitMark) override;

    bool supportVectorized() const override
    {
        return vectorFunction != nullptr && value->supportVectorized();
    }

    std::string toString() const override
    {
        std::string indent = "";
        std::string output = indent + "IsNull:" + TypeUtil::TypeToString(this->GetReturnTypeId()) + "(";
        output += this->value->toString();
        output += ")";
        return output;
    }
};

class FuncExpr : public Expr {
public:
    std::string funcName;
    std::vector<Expr *> arguments;
    const Function *function;
    ExprFunctionType functionType;
    std::vector<vec::BaseVector *> constantInputs;

    FuncExpr();
    ~FuncExpr() override;
    FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType);
    FuncExpr(
        const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType, const Function *function);
    FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType,
        ExprFunctionType functionType);

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;
    static inline bool IsCastStrStr(const omniruntime::expressions::FuncExpr &e)
    {
        return (e.funcName == "CAST" || e.funcName == "CAST_null") &&
            e.arguments[0]->GetReturnTypeId() == omniruntime::type::OMNI_VARCHAR &&
            e.GetReturnTypeId() == omniruntime::type::OMNI_VARCHAR;
    }

    uint8_t *compute(omniruntime::vec::VectorBatch *vecBatc, uint8_t *bitMark) override;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;

    std::vector<DataTypePtr> GetInputDataTypes() const {
        std::vector<DataTypePtr> inputDataTypes;
        inputDataTypes.reserve(arguments.size());
        for (const auto &arg : arguments) {
            inputDataTypes.push_back(arg->dataType);
        }
        return inputDataTypes;
    }
    bool supportVectorized() const override
    {
        if (vectorFunction == nullptr) {
            return false;
        }
        for (auto &arg : arguments) {
            if (!arg->supportVectorized()) {
                return false;
            }
        }
        return true;
    }

    std::string toString() const override
    {
        std::string indent = "";
        std::string typeStr = TypeUtil::TypeToString(this->GetReturnTypeId());
        if (TypeUtil::IsDecimalType(this->GetReturnTypeId())) {
            auto decimalDataType = static_cast<DecimalDataType *>(this->GetReturnType().get());
            typeStr += "(";
            typeStr += std::to_string(decimalDataType->GetPrecision());
            typeStr += ", ";
            typeStr += std::to_string(decimalDataType->GetScale());
            typeStr += ")";
        } else if (TypeUtil::IsStringType(this->GetReturnTypeId())) {
            typeStr += "[";
            typeStr += std::to_string(static_cast<VarcharDataType *>(this->GetReturnType().get())->GetWidth());
            typeStr += "]";
        }

        std::string output = indent + "Function:" + ":" + this->funcName + ":" + typeStr + "(";
        for (uint32_t i = 0; i < this->arguments.size(); i++) {
            output += this->arguments[i]->toString();
            if (i == this->arguments.size() - 1) {
                output += ")";
            }
        }
        return output;
    }
};

class BloomFilterFuncExpr : public FuncExpr
{
public:
    BloomFilterFuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType,
        std::unique_ptr<op::BloomFilter> bloomFilter)
     : FuncExpr(fnName, args, returnType),
       bloomFilter_(std::move(bloomFilter)) {}
   ~BloomFilterFuncExpr() = default;
   BloomFilterFuncExpr(const BloomFilterFuncExpr &) = delete;
   BloomFilterFuncExpr &operator=(const BloomFilterFuncExpr &) = delete;
   BloomFilterFuncExpr(BloomFilterFuncExpr &&) = delete;
   BloomFilterFuncExpr &operator=(BloomFilterFuncExpr &&) = delete;
private:
    std::unique_ptr<op::BloomFilter> bloomFilter_;
};

class ITypedExpr;

using TypedExprPtrNew = std::shared_ptr<const ITypedExpr>;

class ITypedExpr {
public:
    explicit ITypedExpr(DataTypePtr type) : type_{std::move(type)}, inputs_{} {}

    ITypedExpr(DataTypePtr type, std::vector<TypedExprPtrNew> inputs)
        : type_{std::move(type)}, inputs_{std::move(inputs)} {}

    const DataTypePtr& type() const
    {
        return type_;
    }

    virtual ~ITypedExpr() = default;

    const std::vector<TypedExprPtrNew>& inputs() const
    {
        return inputs_;
    }

    virtual bool operator==(const ITypedExpr& other) const = 0;

private:
    DataTypePtr type_;
    std::vector<TypedExprPtrNew> inputs_;
};


class ParamRefExpr : public Expr {
public:

    std::string paramName_;

    ParamRefExpr();

    ParamRefExpr(std::string paramName, DataTypePtr dt);

    ~ParamRefExpr() override = default;

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

};


class LambdaExpr : public Expr {
public:
    Expr *body_;
    std::vector<type::DataTypePtr> paramTypes_;
    std::unordered_map<std::string, int32_t> paramNameToIdxMap_;

    LambdaExpr();

    LambdaExpr(Expr *body, std::vector<DataTypePtr> paramTypes, const std::unordered_map<std::string, int32_t>& map, DataTypePtr dt);

    ~LambdaExpr() override;

    void Accept(ExprVisitor &visitor) const override;
    ExprType GetType() const override;

    size_t GetParamNum() const { return paramTypes_.size(); }

    Expr *GetBody() const { return body_; }

};

} // namespace expressions
} // namespace omniruntime
#endif
