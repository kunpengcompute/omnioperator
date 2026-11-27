/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "common/common.h"
#include "common/vector_util.h"
#include "expression/jsonparser/jsonparser.h"
#include "operator/filter/filter_and_project.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace omniruntime::mem;
using namespace omniruntime::expressions;

namespace om_benchmark {
class FilterAndProject : public BaseOperatorFixture {
protected:
    OperatorFactory *createOperatorFactory(const State &state) override
    {
        auto overflowConfig = new OverflowConfig();
        DataTypes dataTypes(INPUT_TYPES[Query(state)]);
        auto filterExpr = GetExprsFromJson({FILTERS[Query(state)]});
        auto projectionExprs = GetExprsFromJson(PROJECTIONS[Query(state)]);
        auto exprEvaluator =
            std::make_shared<ExpressionEvaluator>(filterExpr.at(0), projectionExprs, dataTypes, overflowConfig);
        return new FilterAndProjectOperatorFactory(move(exprEvaluator));
    }

    std::vector<VectorBatchSupplier> createVecBatch(const State &state) override
    {
        std::vector<VectorBatch *> vvb(totalPositions / PositionsPerPage(state));

        for (int i = 0; i < totalPositions / PositionsPerPage(state); ++i) {
            if (DictionaryBlocks(state)) {
                vvb[i] =
                    CreateSequenceVectorBatchWithDictionaryVector(INPUT_TYPES[Query(state)], PositionsPerPage(state));
            } else {
                vvb[i] = CreateSequenceVectorBatch(INPUT_TYPES[Query(state)], PositionsPerPage(state));
            }
        }

        return VectorBatchToVectorBatchSupplier(vvb);
    }

    BaseFixtureGetOutputStrategy GetOutputStrategy() override { return AFTER_EACH_INPUT_FINISHED; }

    std::vector<omniruntime::expressions::Expr *> GetExprsFromJson(const std::vector<std::string> &exprs)
    {
        auto expressions = std::vector<omniruntime::expressions::Expr *>();
        for (const auto &item : exprs) {
            auto jsonExpression = nlohmann::json::parse(item);
            auto expression = JSONParser::ParseJSON(jsonExpression);
            if (expression == nullptr) {
                omniruntime::expressions::Expr::DeleteExprs(expressions);
                throw omniruntime::exception::OmniException(
                    "EXPRESSION_NOT_SUPPORT", "The expression is not supported yet: " + jsonExpression.dump());
            }
            expressions.push_back(expression);
        }
        return expressions;
    }

private:
    int64_t totalPositions = 1000000;

    std::map<std::string, std::vector<DataTypePtr>> INPUT_TYPES = {
        {"q1", {IntType(), IntType(), IntType(), Date32Type()}},
        {"q2",
         {
             LongType(),
             LongType(),
             IntType(),
             IntType(),
         }},
        {"q3", {VarcharType(200), VarcharType(200), IntType()}},
        {"q4", {VarcharType(200), IntType(), IntType()}},
        {"q5", {CharType(200), IntType(), IntType()}},
        {"q6", {VarcharType(200), LongType(), IntType()}},
        {"q7", {VarcharType(200), IntType()}},
        {"q8", {VarcharType(200), VarcharType(200), LongType(), IntType()}},
        {"q9", {LongType(), IntType(), IntType(), VarcharType(200)}},
        {"q10", {LongType(), IntType(), IntType(), VarcharType(200)}},
        {"q1.1", {LongType(), LongType(), IntType()}},
        {"q1.2", {LongType(), CharType(16), VarcharType(200), Decimal64Type(7, 2), IntType()}},
        {"q1.3", {LongType(), Date32Type(DAY)}},
        {"q2.1",
         {CharType(50), CharType(50), VarcharType(50), VarcharType(50), IntType(), IntType(), Decimal128Type(38, 2),
          LongType()}},
        {"q2.2", {LongType(), IntType(), IntType()}},
        {"q2.3",
         {CharType(50), CharType(50), VarcharType(50), VarcharType(50), IntType(), IntType(), Decimal128Type(38, 2),
          LongType(), Decimal128Type(38, 2)}},
        {"q2.4",
         {CharType(50), CharType(50), VarcharType(50), VarcharType(50), IntType(), Decimal128Type(38, 2),
          Decimal128Type(38, 2), LongType()}},
        {"q3.1", {Decimal128Type(38, 2), IntType(), Decimal128Type(38, 2)}},
        {"q3.2", {LongType(), IntType(), IntType()}},
        {"q3.3", {LongType(), CharType(50), CharType(50), CharType(50), IntType()}},
        {"q3.4", {LongType(), IntType()}},
        {"q4.1", {LongType(), IntType(), IntType(), LongType(), IntType(), CharType(50), IntType()}},
        {"q4.2", {LongType(), IntType(), IntType(), CharType(20)}},
        {"q4.3", {LongType(), IntType(), CharType(50), IntType()}},
        {"q4.4", {LongType()}},
        {"q4.5", {LongType()}},
        {"q5.1", {LongType(), IntType(), IntType()}},
        {"q5.2", {LongType(), VarcharType(60)}},
        {"q6.1", {Decimal128Type(38, 2), IntType(), Decimal128Type(38, 2)}},
        {"q6.2", {LongType(), IntType(), IntType()}},
        {"q6.3", {LongType(), CharType(50), CharType(50), CharType(50), IntType()}},
        {"q7.1", {LongType(), IntType()}},
        {"q8.1",
         {CharType(50), CharType(50), CharType(50), VarcharType(50), VarcharType(50), IntType(), Decimal128Type(38, 2),
          Decimal128Type(38, 2)}},
        {"q8.2", {LongType(), IntType(), IntType()}},
        {"q8.3", {LongType(), CharType(50), CharType(50), CharType(50)}},
        {"q9.1", {LongType(), IntType(), VarcharType(60)}},
        {"q9.2", {LongType(), IntType(), IntType()}},
        {"q9.3", {LongType(), IntType(), IntType()}},
        {"q10.1", {LongType(), IntType(), CharType(50), IntType(), CharType(50), IntType()}},
        {"q10.2", {LongType(), IntType(), IntType()}}};
    std::map<std::string, std::vector<std::string>> PROJECTIONS = {
        {"q1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":8,"colVal":3})"}},
        {"q2",
         {R"({"exprType":"BINARY","returnType":2,"operator":"SUBTRACT","left":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"right":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}})",
          R"({"exprType":"BINARY","returnType":2,"operator":"ADD","left":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1},"right":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3}]})"}},
        {"q3",
         {R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":200})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2}]})"}},
        {"q4",
         {R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})"}},
        {"q5",
         {R"({"exprType":"FUNCTION","returnType":16,"function_name":"concat","arguments":[{"exprType":"FUNCTION","returnType":16,"function_name":"concat","arguments":[{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"foo","width":3},{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":200}],"width":203},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"lish","width":4}],"width":207})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})"}},
        {"q6",
         {R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2}]})"}},
        {"q7",
         {R"({"exprType":"FUNCTION","returnType":15,"function_name":"substr","arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":0},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}],"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})"}},
        {"q8",
         {R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":2})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3}]})"}},
        {"q9",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1}]})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2}]})",
          R"({"exprType":"FUNCTION","returnType":15,"function_name":"substr","arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":200},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":0},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}],"width":200})"}},
        {"q10",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FUNCTION","returnType":2,"function_name":"CAST","arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1}]})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})",
          R"({"exprType":"FUNCTION","returnType":15,"function_name":"substr","arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":200},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":0},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1}],"width":200})"}},
        {"q1.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1})"}},
        {"q1.2",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":16})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":2,"width":200})",
          R"({"exprType":"FIELD_REFERENCE","dataType":6,"colVal":3,"precision":7,"scale":2})"}},
        {"q1.3", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q2.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":2,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":5})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":7})"}},
        {"q2.2",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})"}},
        {"q2.3",
         {R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":2,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":8,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":7})"}},
        {"q2.4",
         {R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":2,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":5,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":7})"}},
        {"q3.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":0,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2})"}},
        {"q3.2",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})"}},
        {"q3.3",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4})"}},
        {"q3.4",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})"}},
        {"q4.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})"}},
        {"q4.2", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q4.3",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50})"}},
        {"q4.4", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q4.5", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q5.1", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q5.2", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q6.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":0,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2})"}},
        {"q6.2",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})"}},
        {"q6.3",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4})"}},
        {"q7.1", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q8.1",
         {R"({"exprType":"BINARY","returnType":7,"operator":"SUBTRACT","precision":38,"scale":2,"left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":7,"precision":38,"scale":2}})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":3,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":4,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":5})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2})",
          R"({"exprType":"FIELD_REFERENCE","dataType":7,"colVal":7,"precision":38,"scale":2})"}},
        {"q8.2",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2})"}},
        {"q8.3",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50})"}},
        {"q9.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":15,"colVal":2,"width":60})"}},
        {"q9.2", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q9.3", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}},
        {"q10.1",
         {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1})",
          R"({"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3})",
          R"({"exprType":"FIELD_REFERENCE","dataType":16,"colVal":4,"width":50})"}},
        {"q10.2", {R"({"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0})"}}};

    std::
        map<std::string, std::string>
            FILTERS =
                {
                    {"q1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":0},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2}]},"right":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},"upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":10}}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":0},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}]}})"},
                    {"q2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"right":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":0}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":10}}},"right":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2}]},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}}})"},
                    {"q3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"4","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"5","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"6","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"7","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"8","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"9","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"10","width":200}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200}]}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":4},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":5}]}})"},
                    {"q4",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200}]},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}})"},
                    {"q5",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":0,"width":200},"right":{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"3","width":200}},"right":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN_OR_EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":10}}},"right":{"exprType":"BINARY","returnType":4,"operator":"LESS_THAN_OR_EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":20}}})"},
                    {"q6",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"4","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"5","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"6","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"7","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"8","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"9","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"10","width":200}]},"right":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":1},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":1},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":10}}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":4},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":5}]}})"},
                    {"q7",
                     R"({"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3},"upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":5}})"},
                    {"q8",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":0,"width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"1","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"2","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"3","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"4","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"5","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"6","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"7","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"8","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"9","width":200},{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"10","width":200}]},"right":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":2},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":3},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":5}}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}})"},
                    {"q9",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":3},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":5}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2}]}})"},
                    {"q10",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":3},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":5}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":3}}})"},
                    {"q1.1",
                     R"({"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":100},"upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":500}})"},
                    {"q1.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":6,"colVal":3,"precision":7,"scale":2},"lower_bound":{"exprType":"LITERAL","dataType":6,"isNull":false,"value":7600,"precision":7,"scale":2},"upper_bound":{"exprType":"LITERAL","dataType":6,"isNull":false,"value":10600,"precision":7,"scale":2}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":16},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":409},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":512},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":677}]}})"},
                    {"q1.3",
                     R"({"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":8,"colVal":1},"lower_bound":{"exprType":"LITERAL","dataType":8,"isNull":false,"value":10406},"upper_bound":{"exprType":"LITERAL","dataType":8,"isNull":false,"value":10467}})"},
                    {"q2.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":4},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2000}})"},
                    {"q2.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2450816},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2452642}},"right":{"exprType":"IS_NULL","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0}]}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1999},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2000},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2001}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2000}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1999}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":12}}}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2001}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1}}}}})"},
                    {"q2.3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":8,"precision":38,"scale":2},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"0","precision":38,"scale":2}})"},
                    {"q2.4",
                     R"({"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"IF","returnType":7,"condition":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"0","precision":38,"scale":2}},"if_true":{"exprType":"BINARY","returnType":7,"operator":"DIVIDE","precision":38,"scale":2,"left":{"exprType":"FUNCTION","returnType":7,"function_name":"abs","arguments":[{"exprType":"BINARY","returnType":7,"operator":"SUBTRACT","precision":38,"scale":2,"left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":5,"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2}}],"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2}},"if_false":{"exprType":"LITERAL","dataType":7,"isNull":true,"precision":38,"scale":2}},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"10","precision":38,"scale":2}})"},
                    {"q3.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"IF","returnType":7,"condition":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"0","precision":38,"scale":2}},"if_true":{"exprType":"BINARY","returnType":7,"operator":"DIVIDE","precision":38,"scale":2,"left":{"exprType":"FUNCTION","returnType":7,"function_name":"abs","arguments":[{"exprType":"BINARY","returnType":7,"operator":"SUBTRACT","precision":38,"scale":2,"left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":0,"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2}}],"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2}},"if_false":{"exprType":"LITERAL","dataType":7,"isNull":true,"precision":38,"scale":2}},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"10","precision":38,"scale":2}})"},
                    {"q3.2",
                     R"({"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1212},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1213},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1214},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1215},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1216},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1217},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1218},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1219},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1220},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1221},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1222},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1223}]})"},
                    {"q3.3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"amalgimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"edu packscholar #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiunivamalg #9","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"importoamalg #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #14","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #7","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #9","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Books","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Children","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Electronics","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Men","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Music","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Women","width":50}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"accessories","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"classical","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"fragrances","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"pants","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"personal","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"portable","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"reference","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"self-help","width":50}]},"right":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Books","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Children","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Electronics","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"personal","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"portable","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"reference","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"self-help","width":50}]}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #14","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #7","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiunivamalg #9","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #9","width":50}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Women","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Music","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Men","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"accessories","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"classical","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"fragrances","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"pants","width":50}]}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"amalgimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"edu packscholar #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"importoamalg #1","width":50}]}}}}})"},
                    {"q3.4",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2450816},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2452642}},"right":{"exprType":"IS_NULL","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0}]}})"},
                    {"q4.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":12}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2001}}})"},
                    {"q4.2",
                     R"({"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":20},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"breakfast","width":20},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"dinner","width":20}]})"},
                    {"q4.3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":3},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1}})"},
                    {"q4.4",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2450815},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2452657}},"right":{"exprType":"IS_NULL","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0}]}})"},
                    {"q4.5",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2450816},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2452642}},"right":{"exprType":"IS_NULL","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0}]}})"},
                    {"q5.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN_OR_EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":32287}},"right":{"exprType":"BINARY","returnType":4,"operator":"LESS_THAN_OR_EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":82287}}})"},
                    {"q5.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":15,"colVal":1,"width":60},"right":{"exprType":"LITERAL","dataType":15,"isNull":false,"value":"Hopewell","width":60}})"},
                    {"q6.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"IF","returnType":7,"condition":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"0","precision":38,"scale":2}},"if_true":{"exprType":"BINARY","returnType":7,"operator":"DIVIDE","precision":38,"scale":2,"left":{"exprType":"FUNCTION","returnType":7,"function_name":"abs","arguments":[{"exprType":"BINARY","returnType":7,"operator":"SUBTRACT","precision":38,"scale":2,"left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":0,"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2}}],"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":2,"precision":38,"scale":2}},"if_false":{"exprType":"LITERAL","dataType":7,"isNull":true,"precision":38,"scale":2}},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"10","precision":38,"scale":2}})"},
                    {"q6.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2452123},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2452487}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1219},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1220},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1221},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1222},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1223},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1224},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1225},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1226},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1227},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1228},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1229},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1230}]}})"},
                    {"q6.3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"amalgimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"edu packscholar #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiunivamalg #9","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"importoamalg #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #14","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #7","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #9","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Books","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Children","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Electronics","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Men","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Music","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Women","width":50}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"accessories","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"classical","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"fragrances","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"pants","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"personal","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"portable","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"reference","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"self-help","width":50}]},"right":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Books","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Children","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Electronics","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"personal","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"portable","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"reference","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"self-help","width":50}]}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #14","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #7","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiunivamalg #9","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"scholaramalgamalg #9","width":50}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Women","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Music","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Men","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"accessories","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"classical","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"fragrances","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"pants","width":50}]}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":1,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"amalgimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"edu packscholar #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"exportiimporto #1","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"importoamalg #1","width":50}]}}}}})"},
                    {"q7.1",
                     R"({"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1202},"upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1213}})"},
                    {"q8.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"IF","returnType":7,"condition":{"exprType":"BINARY","returnType":4,"operator":"NOT_EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":7,"precision":38,"scale":2},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"0","precision":38,"scale":2}},"if_true":{"exprType":"BINARY","returnType":7,"operator":"DIVIDE","precision":38,"scale":2,"left":{"exprType":"FUNCTION","returnType":7,"function_name":"abs","arguments":[{"exprType":"BINARY","returnType":7,"operator":"SUBTRACT","precision":38,"scale":2,"left":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":6,"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":7,"precision":38,"scale":2}}],"precision":38,"scale":2},"right":{"exprType":"FIELD_REFERENCE","dataType":7,"colVal":7,"precision":38,"scale":2}},"if_false":{"exprType":"LITERAL","dataType":7,"isNull":true,"precision":38,"scale":2}},"right":{"exprType":"LITERAL","dataType":7,"isNull":false,"value":"10","precision":38,"scale":2}})"},
                    {"q8.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451545},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451910}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2000}}})"},
                    {"q8.3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Books","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Electronics","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Home","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Jewelry","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Men","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Shoes","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"birdal","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"musical","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"pants","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"parenting","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"wallpaper","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"womens","width":50}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Home","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Books","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Electronics","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"womens","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"birdal","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"pants","width":50}]}},"right":{"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":2,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"wallpaper","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"parenting","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"musical","width":50}]},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":16,"colVal":3,"width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Shoes","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Jewelry","width":50},{"exprType":"LITERAL","dataType":16,"isNull":false,"value":"Men","width":50}]}}}})"}, // codespell:ignore womens
                    {"q9.1",
                     R"({"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"lower_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":200},"upper_bound":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":295}})"},
                    {"q9.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BETWEEN","returnType":4,"value":{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},"lower_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2450819},"upper_bound":{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451904}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1}}},"right":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1998},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1999},{"exprType":"LITERAL","dataType":1,"isNull":false,"value":2000}]}})"},
                    {"q9.3",
                     R"({"exprType":"BINARY","returnType":4,"operator":"OR","left":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":8}},"right":{"exprType":"BINARY","returnType":4,"operator":"GREATER_THAN","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":0}}})"},
                    {"q10.1",
                     R"({"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":5},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":7}})"},
                    {"q10.2",
                     R"({"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"BINARY","returnType":4,"operator":"AND","left":{"exprType":"IN","returnType":4,"arguments":[{"exprType":"FIELD_REFERENCE","dataType":2,"colVal":0},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451484},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451485},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451486},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451487},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451488},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451489},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451490},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451491},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451492},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451493},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451494},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451495},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451496},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451497},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451498},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451499},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451500},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451501},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451502},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451503},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451504},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451505},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451506},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451507},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451508},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451509},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451510},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451511},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451512},{"exprType":"LITERAL","dataType":2,"isNull":false,"value":2451513}]},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":2},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":11}}},"right":{"exprType":"BINARY","returnType":4,"operator":"EQUAL","left":{"exprType":"FIELD_REFERENCE","dataType":1,"colVal":1},"right":{"exprType":"LITERAL","dataType":1,"isNull":false,"value":1999}}})"},
                };
    OMNI_BENCHMARK_PARAM(std::string, Query, "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9", "q10", "q1.1",
                         "q1.2", "q1.3", "q2.1", "q2.2", "q2.3", "q2.4", "q3.1", "q3.2", "q3.3", "q3.4", "q4.1", "q4.2",
                         "q4.3", "q4.4", "q4.5", "q5.1", "q5.2", "q6.1", "q6.2", "q6.3", "q7.1", "q8.1", "q8.2", "q8.3",
                         "q9.1", "q9.2", "q9.3", "q10.1", "q10.2");
    OMNI_BENCHMARK_PARAM(int32_t, PositionsPerPage, 32, 1024);
    OMNI_BENCHMARK_PARAM(bool, DictionaryBlocks, false, true);
};
OMNI_BENCHMARK_DECLARE_OPERATOR_DEFAULT(FilterAndProject);
} // namespace om_benchmark
