/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description:
 */
#include <utility>
#include "gtest/gtest.h"
#include "expression/expressions.h"
#include "expression/jsonparser/jsonparser.h"

#include "test/util/test_util.h"
#include "expression/parserhelper.h"

using namespace std;
using namespace omniruntime::expressions;
using namespace omniruntime::vec;

namespace JsonParserTest {
const int32_t INT32_VAL = 1;
const int64_t INT64_VAL = 123456789;
const double DOUBLE_VAL = 2.0;
const bool BOOL_VAL = true;
const int32_t COL_NUM = 1;
const string VARCHAR_VAL = "hello world";
const int32_t PRECISION64 = 8;
const int32_t PRECISION128 = 17;
const int32_t NUM_SCALE = 0;
const int32_t VARCHAR_WIDTH = INT_MAX;

std::stringstream ss;

map<Operator, string> ArithOps = { { Operator::ADD, "ADD" },
                                   { Operator::SUB, "SUBTRACT" },
                                   { Operator::MUL, "MULTIPLY" },
                                   { Operator::DIV, "DIVIDE" },
                                   { Operator::MOD, "MODULUS" } };
map<Operator, string> CmpOps = { { Operator::GT, "GREATER_THAN" }, { Operator::GTE, "GREATER_THAN_OR_EQUAL" },
                                 { Operator::LT, "LESS_THAN" },    { Operator::LTE, "LESS_THAN_OR_EQUAL" },
                                 { Operator::EQ, "EQUAL" },        { Operator::NEQ, "NOT_EQUAL" } };

string GetInt32TestJSON(int32_t val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 1, "isNull": false, "value": )" << val << R"(})";
    return ss.str();
}

string GetDate32TestJson(int32_t val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 8, "isNull": false, "value": )" << val << R"(})";
    return ss.str();
}

string GetVarcharTestJson(const string &val, int32_t width)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 15, "isNull": false, "value": ")" << val << R"(", "width": )" <<
        width << R"(})";
    return ss.str();
}

string GetInt64TestJson(int64_t val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 2, "isNull": false, "value": )" << val << R"(})";
    return ss.str();
}

string GetTimestampTestJson(int64_t val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 12, "isNull": false, "value": )" << val << R"(})";
    return ss.str();
}

string GetDec64TestJson(int64_t val, int32_t precision, int32_t scale)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 6, "isNull": false, "value": )" << val << R"(, "precision": )" <<
        precision << R"(, "scale": )" << scale << R"(})";
    return ss.str();
}

string GetDec128TestJson(const string &val, int32_t precision, int32_t scale)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 7, "isNull": false, "value": ")" << val << R"(", "precision": )" <<
        precision << R"(, "scale": )" << scale << R"(})";
    return ss.str();
}

string GetDoubleTestJson(double val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 3, "isNull": false, "value": )" << val << R"(})";
    return ss.str();
}

string GetBoolTestJson(bool val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 4, "isNull": false, "value": )" << boolalpha << val << R"(})";
    return ss.str();
}

string GetNoneTestJson(const string &val)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": 0, "isNull": false, "value": ")" << val << R"("})";
    return ss.str();
}

string GetNullTestJson(int32_t dt)
{
    ss.str("");
    ss << R"({ "exprType": "LITERAL", "dataType": )" << dt << R"(, "isNull": true})";
    return ss.str();
}

string GetFieldRefTestJson(int32_t dt, int32_t colval)
{
    ss.str("");
    ss << R"({ "exprType": "FIELD_REFERENCE", "dataType": )" << dt << R"(, "colVal": )" << colval << R"(})";
    return ss.str();
}

string GetStringFieldRefTestJson(int32_t dt, int32_t colval, int32_t width)
{
    ss.str("");
    ss << R"({ "exprType": "FIELD_REFERENCE", "dataType": )" << dt << R"(, "colVal": )" << colval <<
        R"(, "width": )" << width << R"(})";
    return ss.str();
}

string GetDecimalFieldRefTestJson(int32_t dt, int32_t colval, int32_t precision, int32_t scale)
{
    ss.str("");
    ss << R"({ "exprType": "FIELD_REFERENCE", "dataType": )" << dt << R"(, "colVal": )" << colval <<
        R"(, "precision": )" << precision << R"(, "scale": )" << scale << R"(})";
    return ss.str();
}


string GetAndTestJson(const string &left, const string &right)
{
    ss.str("");
    ss << R"({"exprType": "BINARY", "returnType": 4, "operator": "AND", "left":)" << left << R"(, "right":)" << right <<
        R"(})";
    return ss.str();
}

string GetOrTestJson(const string &left, const string &right)
{
    ss.str("");
    ss << R"({"exprType": "BINARY", "returnType": 4, "operator": "OR", "left":)" << left << R"(, "right":)" << right <<
        R"(})";
    return ss.str();
}

string GetCoalesceTestJson(int32_t rt, const string &left, const string &right)
{
    ss.str("");
    ss << R"({"exprType": "COALESCE", "returnType": )" << rt << R"(, "value1": )" << left << R"(, "value2": )" <<
        right << R"(})";
    return ss.str();
}

string GetBetweenTestJson(const string &val, const string &val1, const string &val2)
{
    ss.str("");
    ss << R"({"exprType": "BETWEEN", "returnType": 4, "value": )" << val << R"(, "lower_bound": )" << val1 <<
        R"(, "upper_bound": )" << val2 << R"(})";
    return ss.str();
}

string GetIfTestJson(int32_t rt, const string &val, const string &val1, const string &val2)
{
    ss.str("");
    ss << R"({"exprType": "IF", "returnType": )" << rt << R"(, "condition": )" << val << R"(, "if_true": )" << val1 <<
        R"(, "if_false": )" << val2 << R"(})";
    return ss.str();
}

string GetSwitchTestJson(int32_t rt, const string &val, const string &val1, const string &val2)
{
    ss.str("");
    ss << R"({"exprType": "SWITCH", "returnType": )" << rt << R"(, "numOfCases": 1, "input": )" << val <<
        R"(, "Case1": )" << val1 << R"(, "else": )" << val2 << R"(})";
    return ss.str();
}

string GetWhenTestJson(int32_t rt, const string &val, const string &val1)
{
    ss.str("");
    ss << R"({"exprType": "WHEN", "returnType": )" << rt << R"(, "when": )" << val << R"(, "result": )" << val1 <<
        R"(})";
    return ss.str();
}

string GetInTestJson(const vector<string> &args)
{
    ss.str("");
    ss << R"({"exprType": "IN", "returnType": 4, "arguments": [)" << args.at(0);
    for (uint32_t i = 1; i < args.size(); i++) {
        ss << R"(, )" << args.at(i);
    }
    ss << R"(]})";
    return ss.str();
}

string GetBinaryTestJson(int32_t rt, const string &op, const string &left, const string &right)
{
    ss.str("");
    ss << R"({ "exprType": "BINARY", "returnType": )" << rt << R"(, "operator": ")" << op << R"(", "left": )" << left <<
        R"(, "right": )" << right << R"(})";
    return ss.str();
}

string GetDecimalBinaryTestJson(int32_t rt, const string &op, const string &left, const string &right,
    int32_t returnPrecision, int32_t returnScale)
{
    ss.str("");
    ss << R"({ "exprType": "BINARY", "returnType": )" << rt << R"(, "precision": )" << returnPrecision <<
        R"(, "scale": )" << returnScale << R"(, "operator": ")" << op << R"(", "left": )" << left << R"(, "right": )" <<
        right << R"(})";
    return ss.str();
}

string GetUnaryTestJson(const string &op, const string &expr, int32_t returnType = 4)
{
    ss.str("");
    ss << R"({ "exprType": "UNARY", "returnType": )" << returnType << R"(, "operator": ")" << op << R"(", "expr": )" << expr << R"(})";
    return ss.str();
}

string GetFuncTestJson(int32_t rt, const string &func, const vector<string> &args)
{
    ss.str("");
    ss << R"({"exprType": "FUNCTION", "returnType": )" << rt << R"(, "function_name": ")" << func <<
        R"(", "arguments": [)" << args.at(0);
    for (uint32_t i = 1; i < args.size(); i++) {
        ss << R"(, )" << args.at(i);
    }
    ss << R"(]})";
    return ss.str();
}

string GetIsNullTestJson(const string &expr)
{
    ss.str("");
    ss << R"({ "exprType": "IS_NULL", "returnType": 4, "arguments": [)" << expr;
    ss << R"(]})";
    return ss.str();
}

class TestExpr {
public:
    omniruntime::type::DataTypePtr dataType = nullptr;

    virtual bool isEqual(Expr *that) const
    {
        return false;
    };

    virtual ~TestExpr() = default;
};

class TestLiteralExpr : public TestExpr {
    LiteralExpr *expr = nullptr;

public:
    TestLiteralExpr(DataTypePtr dt, int32_t colVal) : expr(new LiteralExpr(colVal, dt))
    {
        dataType = expr->GetReturnType();
    }

    template <typename T> explicit TestLiteralExpr(T val, DataTypePtr dt) : expr(new LiteralExpr(val, dt))
    {
        dataType = expr->GetReturnType();
    }

    // get default expr of null type-dt expression
    explicit TestLiteralExpr(DataTypeId id)
    {
        expr = ParserHelper::GetDefaultValueForType(id);
        expr->isNull = true;
    }

    ~TestLiteralExpr() override
    {
        delete expr;
    }

    bool operator == (const LiteralExpr &rhs) const
    {
        bool stringIsNull = false;
        if (expr->stringVal == nullptr && rhs.stringVal == nullptr)
            stringIsNull = true;

        return *(expr->GetReturnType()) == *(rhs.GetReturnType()) && expr->isNull == rhs.isNull &&
            expr->boolVal == rhs.boolVal && expr->intVal == rhs.intVal && expr->longVal == rhs.longVal &&
            expr->doubleVal == rhs.doubleVal && (stringIsNull || *(expr->stringVal) == *(rhs.stringVal));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(LiteralExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<LiteralExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestFieldExpr : public TestExpr {
    FieldExpr *expr = nullptr;

public:
    TestFieldExpr(int32_t dt, int32_t colVal) : expr(new FieldExpr(colVal, make_shared<DataType>(dt)))
    {
        dataType = expr->GetReturnType();
    }

    ~TestFieldExpr() override
    {
        delete expr;
    }

    bool operator == (const FieldExpr &rhs) const
    {
        return *(expr->GetReturnType()) == *(rhs.GetReturnType()) && expr->isNull == rhs.isNull &&
            expr->colVal == rhs.colVal;
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(FieldExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<FieldExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestBinaryExpr : public TestExpr {
    Operator op = Operator::EQ;
    TestExpr *left = nullptr;
    TestExpr *right = nullptr;

public:
    TestBinaryExpr(Operator op, TestExpr *left, TestExpr *right, DataTypePtr dt) : op(op), left(left), right(right)
    {
        dataType = std::move(dt);
    }

    ~TestBinaryExpr() override
    {
        delete left;
        delete right;
    }

    bool operator == (const BinaryExpr &rhs) const
    {
        return (op == rhs.op && *dataType == *rhs.GetReturnType() && left->isEqual(rhs.left) &&
            right->isEqual(rhs.right));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(BinaryExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<BinaryExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestUnaryExpr : public TestExpr {
    Operator op = Operator::EQ;
    TestExpr *expr = nullptr;

public:
    TestUnaryExpr(Operator op, TestExpr *expr, DataTypePtr dt = BooleanType()) : op(op), expr(expr)
    {
        dataType = dt;
    }

    ~TestUnaryExpr() override
    {
        delete expr;
    }

    bool operator == (const UnaryExpr &rhs) const
    {
        return (op == rhs.op && *dataType == *rhs.GetReturnType() && expr->isEqual(rhs.exp));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(UnaryExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<UnaryExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestBetweenExpr : public TestExpr {
    TestExpr *value = nullptr;
    TestExpr *lowerBound = nullptr;
    TestExpr *upperBound = nullptr;

public:
    TestBetweenExpr(TestExpr *value, TestExpr *lower, TestExpr *upper)
        : value(value), lowerBound(lower), upperBound(upper)
    {
        dataType = BooleanType();
    }

    ~TestBetweenExpr() override
    {
        delete lowerBound;
        delete upperBound;
        delete value;
    }

    bool operator == (const BetweenExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && value->isEqual(rhs.value) && lowerBound->isEqual(rhs.lowerBound) &&
            upperBound->isEqual(rhs.upperBound));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(BetweenExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<BetweenExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestCoalesceExpr : public TestExpr {
    TestExpr *value1 = nullptr;
    TestExpr *value2 = nullptr;

public:
    TestCoalesceExpr(TestExpr *value1, TestExpr *value2) : value1(value1), value2(value2)
    {
        dataType = std::move(value1->dataType);
    }

    ~TestCoalesceExpr() override
    {
        delete value1;
        delete value2;
    }

    bool operator == (const CoalesceExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && value1->isEqual(rhs.value1) && value2->isEqual(rhs.value2));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(CoalesceExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<CoalesceExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestFuncExpr : public TestExpr {
    string funcName;
    vector<TestExpr *> args;

public:
    TestFuncExpr(omniruntime::type::DataTypePtr rt, string funcName, vector<TestExpr *> args)
        : funcName(std::move(funcName)), args(std::move(args))
    {
        dataType = rt;
    }

    ~TestFuncExpr() override
    {
        for (TestExpr *exp : args) {
            delete exp;
        }
    }

    bool operator == (const FuncExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && funcName == rhs.funcName &&
            std::equal(args.begin(), args.end(), rhs.arguments.begin(),
            [](TestExpr *left, Expr *right) { return left->isEqual(right); }));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(FuncExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<FuncExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestSwitchExpr : public TestExpr {
    vector<pair<TestExpr *, TestExpr *>> whenClause;
    TestExpr *elseExpr = nullptr;

public:
    TestSwitchExpr(vector<pair<TestExpr *, TestExpr *>> whenClause, TestExpr *elseExpr)
        : whenClause(std::move(whenClause)), elseExpr(elseExpr)
    {
        dataType = std::move(elseExpr->dataType);
    }

    ~TestSwitchExpr() override
    {
        for (pair<TestExpr *, TestExpr *> pair : whenClause) {
            delete pair.first;
            delete pair.second;
        }
        delete elseExpr;
    }

    bool operator == (const SwitchExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && elseExpr->isEqual(rhs.falseExpr) &&
            std::equal(whenClause.begin(), whenClause.end(), rhs.whenClause.begin(),
            [](pair<TestExpr *, TestExpr *> left, std::pair<Expr *, Expr *> right) {
                return left.first->isEqual(right.first) && left.second->isEqual(right.second);
            }));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(SwitchExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<SwitchExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestIfExpr : public TestExpr {
    TestExpr *condition = nullptr;
    TestExpr *tExpr = nullptr;
    TestExpr *fExpr = nullptr;

public:
    TestIfExpr(TestBinaryExpr *cond, TestExpr *tExr, TestExpr *fExp) : condition(cond), tExpr(tExr), fExpr(fExp)
    {
        dataType = std::move(tExr->dataType);
    }

    ~TestIfExpr() override
    {
        delete condition;
        delete tExpr;
        delete fExpr;
    }

    bool operator == (const IfExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && condition->isEqual(rhs.condition) &&
            tExpr->isEqual(rhs.trueExpr) && fExpr->isEqual(rhs.falseExpr));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(IfExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<IfExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestInExpr : public TestExpr {
    vector<TestExpr *> args;

public:
    explicit TestInExpr(vector<TestExpr *> args) : args(std::move(args))
    {
        dataType = BooleanType();
    }

    ~TestInExpr() override
    {
        for (TestExpr *exp : args) {
            delete exp;
        }
    }

    bool operator == (const InExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && std::equal(args.begin(), args.end(), rhs.arguments.begin(),
            [](TestExpr *left, Expr *right) { return left->isEqual(right); }));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(InExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<InExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

class TestIsNullExpr : public TestExpr {
public:
    TestExpr *value = nullptr;

    explicit TestIsNullExpr(TestExpr *value) : value(value)
    {
        dataType = BooleanType();
    }

    ~TestIsNullExpr() override
    {
        delete value;
    }

    bool operator == (const IsNullExpr &rhs) const
    {
        return (*dataType == *rhs.GetReturnType() && value->isEqual(rhs.value));
    }

    bool isEqual(Expr *that) const override
    {
        if (typeid(IsNullExpr) != typeid(*that))
            return false;
        auto *rhs = dynamic_cast<IsNullExpr *>(that);
        bool result = (*this == *rhs);
        EXPECT_TRUE(result);
        return result;
    }
};

TEST(JSONParserTest, Literal_Bool)
{
    string unparsedBoolJson = GetBoolTestJson(BOOL_VAL);
    Expr *boolExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedBoolJson));
    TestLiteralExpr expectedExpr(BOOL_VAL, BooleanType());
    expectedExpr.isEqual(boolExpr);
    delete boolExpr;
}

TEST(JSONParserTest, Literal_Integer)
{
    string unparsedIntJson = GetInt32TestJSON(INT32_VAL);
    Expr *intExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedIntJson));
    TestLiteralExpr expectedExpr(INT32_VAL, IntType());
    expectedExpr.isEqual(intExpr);
    delete intExpr;
}

TEST(JSONParserTest, Literal_Date32)
{
    string unparsedDateJson = GetDate32TestJson(INT32_VAL);
    Expr *intExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedDateJson));
    TestLiteralExpr expectedExpr(INT32_VAL, Date32Type());
    expectedExpr.isEqual(intExpr);
    delete intExpr;
}

TEST(JSONParserTest, Literal_Long)
{
    string unparsedLongJson = GetInt64TestJson(INT64_VAL);
    Expr *longExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedLongJson));
    TestLiteralExpr expectedExpr(INT64_VAL, LongType());
    expectedExpr.isEqual(longExpr);
    delete longExpr;
}

TEST(JSONParserTest, Literal_Timestamp)
{
    string unparsedTimestampJson = GetTimestampTestJson(INT64_VAL);
    Expr *timestampExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedTimestampJson));
    TestLiteralExpr expectedExpr(INT64_VAL, TimestampType());
    expectedExpr.isEqual(timestampExpr);
    delete timestampExpr;
}

TEST(JSONParserTest, Literal_Double)
{
    string unparsedDoubleJson = GetDoubleTestJson(DOUBLE_VAL);
    Expr *doubleExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedDoubleJson));
    TestLiteralExpr expectedExpr(DOUBLE_VAL, DoubleType());
    expectedExpr.isEqual(doubleExpr);
    delete doubleExpr;
}

TEST(JSONParserTest, Literal_Decimal64)
{
    string unparsedD64Json = GetDec64TestJson(INT64_VAL, PRECISION64, NUM_SCALE);
    Expr *d64Expr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedD64Json));
    TestLiteralExpr expectedExpr(INT64_VAL, Decimal64Type(8, 0));
    expectedExpr.isEqual(d64Expr);
    delete d64Expr;
}

TEST(JSONParserTest, Literal_Decimal128)
{
    string unparsedD128Json = GetDec128TestJson("12", PRECISION128, NUM_SCALE);
    Expr *d128Expr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedD128Json));
    TestLiteralExpr expectedExpr(new std::string(to_string(12)), Decimal128Type(17, 0));
    expectedExpr.isEqual(d128Expr);
    delete d128Expr;
}

TEST(JSONParserTest, Literal_Varchar)
{
    string unparsedVarcharJson = GetVarcharTestJson(VARCHAR_VAL, VARCHAR_WIDTH);
    Expr *varcharExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedVarcharJson));
    TestLiteralExpr expectedExpr(new string(VARCHAR_VAL), VarcharType());
    expectedExpr.isEqual(varcharExpr);
    delete varcharExpr;
}

TEST(JSONParserTest, Literal_Unknown_Null)
{
    string unparsedNoneWithNull = GetNullTestJson(OMNI_NONE);
    Expr *noneWithNull = JSONParser::ParseJSON(nlohmann::json::parse(unparsedNoneWithNull));
    // None type default DataExpr
    TestLiteralExpr expectedExpr(OMNI_NONE);
    expectedExpr.isEqual(noneWithNull);
    delete noneWithNull;
}

TEST(JSONParserTest, Literal_Bool_Null)
{
    string unparsedNullBool = GetNullTestJson(OMNI_BOOLEAN);
    Expr *noneWithNull = JSONParser::ParseJSON(nlohmann::json::parse(unparsedNullBool));
    // bool type default DataExpr
    TestLiteralExpr expectedExpr(OMNI_BOOLEAN);
    expectedExpr.isEqual(noneWithNull);
    delete noneWithNull;
}

TEST(JSONParserTest, Literal_Int32_Null)
{
    string unparsedNullInt32 = GetNullTestJson(OMNI_INT);
    Expr *nullInt32 = JSONParser::ParseJSON(nlohmann::json::parse(unparsedNullInt32));
    // Int32 default DataExpr
    TestLiteralExpr expectedExpr(OMNI_INT);
    expectedExpr.isEqual(nullInt32);
    delete nullInt32;
}

TEST(JSONParserTest, FieldReference)
{
    string unparsedFieldRefJson = GetFieldRefTestJson(OMNI_LONG, COL_NUM);
    Expr *fieldRefExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFieldRefJson));
    TestFieldExpr expectedExpr(OMNI_LONG, COL_NUM);
    expectedExpr.isEqual(fieldRefExpr);
    delete fieldRefExpr;
}


TEST(JSONParserTest, Decimal128FieldReference)
{
    string unparsedFieldRefJson = GetDecimalFieldRefTestJson(OMNI_DECIMAL128, COL_NUM, 0, 0);
    Expr *fieldRefExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFieldRefJson));
    TestFieldExpr expectedExpr(OMNI_DECIMAL128, COL_NUM);
    expectedExpr.isEqual(fieldRefExpr);
    delete fieldRefExpr;
}

TEST(JSONParserTest, Decimal64FieldReference)
{
    string unparsedFieldRefJson = GetDecimalFieldRefTestJson(OMNI_DECIMAL64, COL_NUM, 0, 0);
    Expr *fieldRefExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFieldRefJson));
    TestFieldExpr expectedExpr(OMNI_DECIMAL64, COL_NUM);
    expectedExpr.isEqual(fieldRefExpr);
    delete fieldRefExpr;
}

TEST(JSONParserTest, VarBinaryFieldReferenceWithoutWidth)
{
    string unparsedFieldRefJson = GetFieldRefTestJson(OMNI_VARBINARY, COL_NUM);
    Expr *fieldRefExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFieldRefJson));
    TestFieldExpr expectedExpr(OMNI_VARBINARY, COL_NUM);
    expectedExpr.isEqual(fieldRefExpr);
    delete fieldRefExpr;
}

TEST(JSONParserTest, BinaryExpr_EQ)
{
    string unparsedBinaryJson = GetBinaryTestJson(OMNI_BOOLEAN, CmpOps.at(Operator::EQ), GetInt32TestJSON(INT32_VAL),
        GetInt64TestJson(INT64_VAL));
    Expr *addExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedBinaryJson));
    // When binary operator has different types from left op and right op, a cast will be inserted
    vector<TestExpr *> args = {new TestLiteralExpr(INT32_VAL, IntType())};
    TestFuncExpr* castFuncExpr = new TestFuncExpr(LongType(), "CAST", args);
    TestBinaryExpr expectedExpr(Operator::EQ, castFuncExpr, new TestLiteralExpr(INT64_VAL, LongType()), BooleanType());
    expectedExpr.isEqual(addExpr);
    delete addExpr;
}

TEST(JSONParserTest, BinaryExpr_ADD)
{
    string unparsedBinaryJson = GetBinaryTestJson(OMNI_LONG, ArithOps.at(Operator::ADD), GetInt32TestJSON(INT32_VAL),
        GetInt64TestJson(INT64_VAL));
    Expr *addExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedBinaryJson));
    // When binary operator has different types from left op and right op, a cast will be inserted
    vector<TestExpr *> args = {new TestLiteralExpr(INT32_VAL, IntType())};
    TestFuncExpr* castFuncExpr = new TestFuncExpr(LongType(), "CAST", args);
    TestBinaryExpr expectedExpr(Operator::ADD, castFuncExpr, new TestLiteralExpr(INT64_VAL, LongType()), LongType());
    expectedExpr.isEqual(addExpr);
    delete addExpr;
}

TEST(JSONParserTest, BinaryExpr_ADD_DECIMAL64)
{
    string unparsedBinaryJson = GetDecimalBinaryTestJson(OMNI_DECIMAL64, ArithOps.at(Operator::ADD),
        GetDec64TestJson((int64_t)65, 6, 2), GetInt64TestJson(INT64_VAL), 10, 3);
    Expr *addExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedBinaryJson));
    vector<TestExpr *> argsRight = {new TestLiteralExpr(INT64_VAL, LongType())};
    TestFuncExpr* castFuncExprRight =  new TestFuncExpr(Decimal64Type(10, 3), "CAST", argsRight);
    TestBinaryExpr expectedExpr(Operator::ADD, new TestLiteralExpr((int64_t)65, Decimal64Type(6, 2)),
                                castFuncExprRight, Decimal64Type(10, 3));
    expectedExpr.isEqual(addExpr);
    delete addExpr;
}


TEST(JSONParserTest, BinaryExpr_ADD_DECIMAL128)
{
    string unparsedBinaryJson = GetDecimalBinaryTestJson(OMNI_DECIMAL128, ArithOps.at(Operator::DIV),
        GetDec128TestJson("123456", 32, 2), GetInt32TestJSON(INT32_VAL), 35, 4);
    auto divExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedBinaryJson));
    vector<TestExpr *> argsRight = {new TestLiteralExpr(INT32_VAL, IntType())};
    TestFuncExpr* castFuncExprRight =  new TestFuncExpr(Decimal128Type(35, 4), "CAST", argsRight);
    TestBinaryExpr expectedExpr(Operator::DIV, new TestLiteralExpr(new std::string("123456"), Decimal128Type(32, 2)),
                                castFuncExprRight, Decimal128Type(35, 4));
    expectedExpr.isEqual(divExpr);
    delete divExpr;
}

TEST(JSONParserTest, UnaryExpr_NOT)
{
    string unparsedUnaryJson =
        GetUnaryTestJson("NOT", GetIsNullTestJson(GetDecimalFieldRefTestJson(OMNI_DECIMAL64, COL_NUM, 0, 0)));
    Expr *unaryExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedUnaryJson));
    TestUnaryExpr expectedExpr(Operator::NOT, new TestIsNullExpr(new TestFieldExpr(OMNI_DECIMAL64, COL_NUM)));
    expectedExpr.isEqual(unaryExpr);
    delete unaryExpr;
}

TEST(JSONParserTest, UnaryExpr_NEGATIVE_INT)
{
    string unparsedUnaryJson = GetUnaryTestJson("NEGATION", GetInt32TestJSON(INT32_VAL), 1);
    Expr *unaryExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedUnaryJson));
    TestUnaryExpr expectedExpr(Operator::NEG, new TestLiteralExpr(INT32_VAL, IntType()), IntType());
    expectedExpr.isEqual(unaryExpr);
    delete unaryExpr;
}

TEST(JSONParserTest, UnaryExpr_POSITIVE_INT)
{
    string unparsedUnaryJson = GetUnaryTestJson("POSITIVE", GetInt32TestJSON(INT32_VAL), 1);
    Expr *unaryExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedUnaryJson));
    TestUnaryExpr expectedExpr(Operator::POS, new TestLiteralExpr(INT32_VAL, IntType()), IntType());
    expectedExpr.isEqual(unaryExpr);
    delete unaryExpr;
}

TEST(JSONParserTest, BetweenExpr)
{
    string unparsedBetweenJson =
        GetBetweenTestJson(GetInt32TestJSON(INT32_VAL), GetInt32TestJSON(INT32_VAL), GetInt32TestJSON(INT32_VAL));
    Expr *betweenExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedBetweenJson));
    TestBetweenExpr expectedExpr(new TestLiteralExpr(INT32_VAL, IntType()), new TestLiteralExpr(INT32_VAL, IntType()),
        new TestLiteralExpr(INT32_VAL, IntType()));
    expectedExpr.isEqual(betweenExpr);
    delete betweenExpr;
}

TEST(JSONParserTest, CoalesceExpr)
{
    string unparsedCoalesceJson =
        GetCoalesceTestJson(OMNI_VARCHAR, GetVarcharTestJson(VARCHAR_VAL, VARCHAR_WIDTH), GetVarcharTestJson("", 0));
    Expr *coalesceExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedCoalesceJson));
    TestCoalesceExpr expectedExpr(new TestLiteralExpr(new string(VARCHAR_VAL), VarcharType()),
        new TestLiteralExpr(new string(""), VarcharType(0)));
    expectedExpr.isEqual(coalesceExpr);
    delete coalesceExpr;
}

TEST(JSONParserTest, FuncExpr_substr)
{
    // substr(string, int)
    vector<string> argsJson = { GetVarcharTestJson(VARCHAR_VAL, VARCHAR_WIDTH), GetInt32TestJSON(INT32_VAL) };
    string unparsedFuncJson = GetFuncTestJson(OMNI_VARCHAR, "substr", argsJson);
    Expr *funcExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFuncJson));
    vector<TestExpr *> args = { new TestLiteralExpr(new string(VARCHAR_VAL), VarcharType()),
        new TestLiteralExpr(INT32_VAL, IntType()) };
    TestFuncExpr expectedExpr(VarcharType(), "substr", args);
    expectedExpr.isEqual(funcExpr);
    delete funcExpr;
}

TEST(JSONParserTest, FuncExpr_jsonSplitCharInput)
{
    vector<string> argsJson = { GetStringFieldRefTestJson(OMNI_CHAR, COL_NUM, 8) };
    string unparsedFuncJson = GetFuncTestJson(OMNI_VARCHAR, "json_split", argsJson);
    Expr *funcExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFuncJson));

    ASSERT_NE(funcExpr, nullptr);
    EXPECT_EQ(funcExpr->GetType(), ExprType::FUNC_E);
    EXPECT_EQ(funcExpr->GetReturnTypeId(), OMNI_VARCHAR);

    auto typedFuncExpr = dynamic_cast<FuncExpr *>(funcExpr);
    ASSERT_NE(typedFuncExpr, nullptr);
    ASSERT_EQ(typedFuncExpr->arguments.size(), 1);
    EXPECT_EQ(typedFuncExpr->arguments[0]->GetReturnTypeId(), OMNI_CHAR);

    delete funcExpr;
}

TEST(JSONParserTest, FieldReference_StringWithoutWidth)
{
    string unparsedFieldRefJson = GetFieldRefTestJson(OMNI_CHAR, COL_NUM);
    Expr *fieldRefExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFieldRefJson));

    EXPECT_EQ(fieldRefExpr, nullptr);
}

TEST(JSONParserTest, FuncExpr_jsonValueWithBehaviors)
{
    string unparsedFuncJson = R"({
        "exprType": "FUNCTION",
        "returnType": 15,
        "function_name": "json_value",
        "arguments": [
            { "exprType": "FIELD_REFERENCE", "dataType": 15, "colVal": 0, "width": 32 },
            { "exprType": "LITERAL", "dataType": 16, "isNull": false, "value": "$.age", "width": 5 }
        ],
        "emptyBehavior": {
            "type": "DEFAULT",
            "defaultValue": { "exprType": "LITERAL", "dataType": 15, "isNull": false, "value": "unknown", "width": 7 }
        },
        "errorBehavior": {
            "type": "ERROR"
        },
        "width": 32
    })";
    Expr *funcExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedFuncJson));

    ASSERT_NE(funcExpr, nullptr);
    auto typedFuncExpr = dynamic_cast<FuncExpr *>(funcExpr);
    ASSERT_NE(typedFuncExpr, nullptr);
    ASSERT_EQ(typedFuncExpr->arguments.size(), 6);
    EXPECT_EQ(typedFuncExpr->arguments[2]->GetReturnTypeId(), OMNI_INT);
    EXPECT_EQ(typedFuncExpr->arguments[3]->GetReturnTypeId(), OMNI_VARCHAR);
    EXPECT_EQ(typedFuncExpr->arguments[4]->GetReturnTypeId(), OMNI_INT);
    EXPECT_EQ(typedFuncExpr->arguments[5]->GetReturnTypeId(), OMNI_VARCHAR);

    auto emptyBehaviorExpr = dynamic_cast<LiteralExpr *>(typedFuncExpr->arguments[2]);
    auto errorBehaviorExpr = dynamic_cast<LiteralExpr *>(typedFuncExpr->arguments[4]);
    auto defaultOnEmptyExpr = dynamic_cast<LiteralExpr *>(typedFuncExpr->arguments[3]);
    ASSERT_NE(emptyBehaviorExpr, nullptr);
    ASSERT_NE(errorBehaviorExpr, nullptr);
    ASSERT_NE(defaultOnEmptyExpr, nullptr);
    EXPECT_EQ(emptyBehaviorExpr->intVal, 2);
    EXPECT_EQ(errorBehaviorExpr->intVal, 1);
    ASSERT_NE(defaultOnEmptyExpr->stringVal, nullptr);
    EXPECT_EQ(*defaultOnEmptyExpr->stringVal, "unknown");

    delete funcExpr;
}

TEST(JSONParserTest, InExpr)
{
    vector<string> argsJson = { GetInt32TestJSON(INT32_VAL), GetInt32TestJSON(0), GetInt32TestJSON(2) };
    string unparsedInJson = GetInTestJson(argsJson);
    Expr *inExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedInJson));
    vector<TestExpr *> args = { new TestLiteralExpr(INT32_VAL, IntType()), new TestLiteralExpr(0, IntType()),
        new TestLiteralExpr(2, IntType()) };
    TestInExpr expectedExpr(args);
    expectedExpr.isEqual(inExpr);
    delete inExpr;
}

TEST(JSONParserTest, SwitchExpr)
{
    string whenJson = GetWhenTestJson(OMNI_INT, GetInt64TestJson(INT64_VAL), GetInt32TestJSON(INT32_VAL));
    string unparsedSwitchJson = GetSwitchTestJson(OMNI_INT, GetInt32TestJSON(INT32_VAL), whenJson, GetInt32TestJSON(4));
    Expr *switchExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedSwitchJson));
    auto condition = new TestBinaryExpr(Operator::EQ, new TestLiteralExpr(INT32_VAL, IntType()),
        new TestLiteralExpr(INT64_VAL, LongType()), BooleanType());
    vector<pair<TestExpr *, TestExpr *>> whenClause = { { condition, new TestLiteralExpr(INT32_VAL, IntType()) } };
    TestSwitchExpr expectedExpr(whenClause, new TestLiteralExpr(4, IntType()));
    expectedExpr.isEqual(switchExpr);
    delete switchExpr;
}

TEST(JSONParserTest, IfExpr)
{
    string conditionJson = GetBinaryTestJson(OMNI_BOOLEAN, CmpOps.at(Operator::EQ), GetInt32TestJSON(INT32_VAL),
        GetInt64TestJson(INT64_VAL));
    string unparsedIfJson = GetIfTestJson(OMNI_INT, conditionJson, GetInt32TestJSON(INT32_VAL), GetInt32TestJSON(4));
    Expr *ifExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedIfJson));
    vector<TestExpr *> args = {new TestLiteralExpr(INT32_VAL, IntType())};
    TestFuncExpr* castFuncExpr =  new TestFuncExpr(LongType(), "CAST", args);
    TestBinaryExpr* condition = new TestBinaryExpr(Operator::EQ, castFuncExpr,
        new TestLiteralExpr(INT64_VAL, LongType()), BooleanType());
    TestIfExpr expectedExpr(condition, new TestLiteralExpr(INT32_VAL, IntType()), new TestLiteralExpr(4, IntType()));
    expectedExpr.isEqual(ifExpr);
    delete ifExpr;
}

// This ut mimics tpcds-q87
TEST(JSONParserTest, UnsupportedFunctionExpr)
{
    vector<string> argsJson1 = { GetNullTestJson(OMNI_INT) };
    string conditionJson = GetFuncTestJson(OMNI_BOOLEAN, "not", argsJson1);
    string unparsedIfJson = GetIfTestJson(OMNI_INT, conditionJson, GetInt32TestJSON(1), GetInt32TestJSON(0));
    vector<string> argsJson2 = { unparsedIfJson };
    string unparsedCastJson = GetFuncTestJson(OMNI_LONG, "CAST", argsJson2);

    Expr *castExpr = JSONParser::ParseJSON(nlohmann::json::parse(unparsedCastJson));
    EXPECT_EQ(castExpr, nullptr);
}

TEST(JSONParserTest, UnsupportedFunctionExprs)
{
    vector<string> argsJson = { GetNullTestJson(OMNI_INT), GetNullTestJson(OMNI_LONG) };
    string castJson = GetFuncTestJson(OMNI_LONG, "CAST", argsJson);
    nlohmann::json expr = nlohmann::json::parse(castJson);
    nlohmann::json exprs[] = {expr, expr};

    vector<Expr *> res = JSONParser::ParseJSON(exprs, 2);
    EXPECT_EQ(res.size(), 0);
}
}
