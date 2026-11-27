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

#include "operator/filter/filter_and_project.h"
#include "operator/sort/sort.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "operator/limit/distinct_limit.h"
#include "operator/union/union.h"
#include "operator/topn/topn.h"
#include "operator/join/sortmergejoin/sort_merge_join.h"
#include "operator/topnsort/topn_sort_expr.h"
#include "operator/window/window.h"
#include "operator/join/sortmergejoin/sort_merge_join_expr_v3.h"
#include "test_util.h"
#include "dt_fuzz_factory_create_util.h"

using namespace omniruntime::type;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace TestUtil;

namespace DtFuzzFactoryCreateUtil {
const int32_t SHORT_DECIMAL_SIZE = 18;
const int32_t LONG_DECIMAL_SIZE = 38;
constexpr int32_t CHAR_SIZE = 10;
OperatorFactory *CreateFilterFactory(omniruntime::type::DataTypes &sourceTypes, BinaryExpr *filterExpr,
                                     std::vector<Expr *> projections, OverflowConfig *overflowConfig)
{
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, sourceTypes, overflowConfig);
    auto operatorFactory = new FilterAndProjectOperatorFactory(move(exprEvaluator));
    return operatorFactory;
}

OperatorFactory *CreateSortFactory(omniruntime::type::DataTypes &sourceTypes)
{
    int32_t sourceTypesSize = sourceTypes.GetSize();
    int32_t outputCols[sourceTypesSize];
    int32_t sortColCount = sourceTypesSize / 5;
    int32_t sortCols[sortColCount];
    int32_t ascendings[sortColCount];
    int32_t nullFirsts[sortColCount];
    for (int32_t i = 0, j = 0; i < sourceTypesSize; i++) {
        outputCols[i] = i;
        if (i % 5 == 0 && j < sortColCount) {
            sortCols[j] = i;
            ascendings[j] = 1;
            nullFirsts[j] = 0;
            j++;
        }
    }

    auto operatorFactory = SortOperatorFactory::CreateSortOperatorFactory(
        sourceTypes, outputCols, sourceTypesSize, sortCols, ascendings, nullFirsts, sortColCount);
    return operatorFactory;
}

OperatorFactory *CreateAggregationFactory(omniruntime::type::DataTypes &sourceTypes)
{
    omniruntime::op::FunctionType aggFunType[] = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
    DataTypes groupTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    DataTypes aggInputTypes(std::vector<DataTypePtr>({LongType(), DoubleType()}));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({LongType(), DoubleType()}));
    uint32_t aggCols[2] = {2, 4};
    std::vector<uint32_t> aggFuncTypeContext =
        std::vector<uint32_t>((uint32_t *)aggFunType, (uint32_t *)aggFunType + 2);
    uint32_t maskCols[2] = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};
    std::vector<uint32_t> maskColsContext = std::vector<uint32_t>((uint32_t *)maskCols, (uint32_t *)maskCols + 2);
    std::vector<uint32_t> aggInputColsContext = std::vector<uint32_t>((uint32_t *)aggCols, (uint32_t *)aggCols + 2);

    auto aggInputColsContextWrap = AggregatorUtil::WrapWithVector(aggInputColsContext);
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawsWrap = std::vector<bool>(aggFuncTypeContext.size(), true);
    auto outputPartialsWrap = std::vector<bool>(aggFuncTypeContext.size(), false);

    auto operatorFactory =
        new AggregationOperatorFactory(sourceTypes, aggFuncTypeContext, aggInputColsContextWrap, maskColsContext,
                                       aggOutputTypesWrap, inputRawsWrap, outputPartialsWrap, true);

    return operatorFactory;
}

OperatorFactory *CreateHashAggregationFactory(omniruntime::type::DataTypes &sourceTypes)
{
    int32_t SHORT_DECIMAL_SIZE = 18;
    int32_t LONG_DECIMAL_SIZE = 38;
    uint16_t CHAR_SIZE = 10;
    omniruntime::op::FunctionType aggFunType[] = {
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MIN};
    DataTypes groupTypes(std::vector<DataTypePtr>({ShortType(), IntType()}));
    DataTypes aggInputTypes(std::vector<DataTypePtr>(
        {LongType(), BooleanType(), DoubleType(), Date32Type(DAY), Decimal64Type(SHORT_DECIMAL_SIZE, 0),
         Decimal128Type(LONG_DECIMAL_SIZE, 0), VarcharType(CHAR_SIZE), CharType(CHAR_SIZE)}));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>(
        {LongType(), BooleanType(), DoubleType(), Date32Type(DAY), Decimal64Type(SHORT_DECIMAL_SIZE, 0),
         Decimal128Type(LONG_DECIMAL_SIZE, 0), VarcharType(CHAR_SIZE), CharType(CHAR_SIZE)}));
    uint32_t groupCols[2] = {0, 1};
    uint32_t aggCols[8] = {2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint32_t> groupByColContext = {groupCols[0], groupCols[1]};
    std::vector<uint32_t> aggColContext = {aggCols[0], aggCols[1], aggCols[2], aggCols[3],
                                           aggCols[4], aggCols[5], aggCols[6], aggCols[7]};
    std::vector<uint32_t> aggFuncTypeContext = {aggFunType[0], aggFunType[1], aggFunType[2], aggFunType[3],
                                                aggFunType[4], aggFunType[5], aggFunType[6], aggFunType[7]};
    std::vector<uint32_t> maskColsContext = {1, 1, 1, 1, 1, 1, 1, 1};

    auto aggColContextWrap = AggregatorUtil::WrapWithVector(aggColContext);
    auto aggInputTypesWrap = AggregatorUtil::WrapWithVector(aggInputTypes);
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawsWrap = std::vector<bool>(aggFuncTypeContext.size(), true);
    auto outputPartialsWrap = std::vector<bool>(aggFuncTypeContext.size(), false);

    auto operatorFactory = new HashAggregationOperatorFactory(
        groupByColContext, groupTypes, aggColContextWrap, aggInputTypesWrap, aggOutputTypesWrap, aggFuncTypeContext,
        maskColsContext, inputRawsWrap, outputPartialsWrap, OperatorConfig());
    return operatorFactory;
}

Expr *CreateJoinFilterExpr()
{
    std::string filterStr = "substr";
    DataTypePtr retType = VarcharType();

    auto leftSubstrColumn = new FieldExpr(1, VarcharType());
    auto leftSubstrIndex = new LiteralExpr(1, IntType());
    auto leftSubstrLen = new LiteralExpr(5, IntType());
    std::vector<Expr *> leftSubstrArgs;
    leftSubstrArgs.push_back(leftSubstrColumn);
    leftSubstrArgs.push_back(leftSubstrIndex);
    leftSubstrArgs.push_back(leftSubstrLen);
    auto leftSubstrExpr = GetFuncExpr(filterStr, leftSubstrArgs, VarcharType());

    auto rightSubstrColumn = new FieldExpr(3, VarcharType());
    auto rightSubstrIndex = new LiteralExpr(1, IntType());
    auto rightSubstrLen = new LiteralExpr(5, IntType());
    std::vector<Expr *> rightSubstrArgs;
    rightSubstrArgs.push_back(rightSubstrColumn);
    rightSubstrArgs.push_back(rightSubstrIndex);
    rightSubstrArgs.push_back(rightSubstrLen);
    auto rightSubstrExpr = GetFuncExpr(filterStr, rightSubstrArgs, VarcharType());

    auto *notEqualExpr =
        new BinaryExpr(omniruntime::expressions::Operator::NEQ, leftSubstrExpr, rightSubstrExpr, BooleanType());
    return notEqualExpr;
}

std::vector<OperatorFactory *> CreateHashJoinFactory(omniruntime::type::DataTypes &sourceTypes,
                                                     OverflowConfig *overflowConfig, std::string filterExpr,
                                                     int32_t operatorCount)
{
    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    operatorCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, sourceTypes, buildJoinCols, joinColsCount, operatorCount);

    int32_t probeOutputCols[1] = {2};
    int32_t probeOutputColsCount = 1;
    int32_t probeHashCols[1] = {1};
    int32_t probeHashColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType()}));
    int32_t buildOutputCols[1] = {2};
    int32_t buildOutputColsCount = 1;

    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);
    Expr *joinFilterExpr = CreateJoinFilterExpr();
    auto lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
        sourceTypes, probeOutputCols, probeOutputColsCount, probeHashCols, probeHashColsCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, joinFilterExpr, false, overflowConfig);
    std::vector<OperatorFactory *> operatorFactories = {hashBuilderFactory, lookupJoinFactory};
    delete joinFilterExpr;
    return operatorFactories;
}

std::vector<OperatorFactory *> CreateLookupOuterFactory(omniruntime::type::DataTypes &sourceTypes,
                                                        OverflowConfig *overflowConfig, std::string filterExpr,
                                                        int32_t operatorCount)
{
    int32_t buildJoinCols[1] = {1};
    int32_t joinColsCount = 1;
    operatorCount = 1;
    auto hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, sourceTypes, buildJoinCols, joinColsCount, operatorCount);

    int32_t probeOutputCols[1] = {2};
    int32_t probeOutputColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType()}));
    int32_t buildOutputCols[1] = {2};

    auto hashBuilderFactoryAddr = reinterpret_cast<int64_t>(hashBuilderFactory);

    auto lookupOuterJoinOperatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        sourceTypes, probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);

    std::vector<OperatorFactory *> operatorFactories = {hashBuilderFactory, lookupOuterJoinOperatorFactory};
    return operatorFactories;
}

OperatorFactory *CreateTopNSortFactory(omniruntime::type::DataTypes &sourceTypes,
                                       std::vector<omniruntime::expressions::Expr *> partitionKeys,
                                       std::vector<omniruntime::expressions::Expr *> sortKeys, int32_t dataSize)
{
    std::vector<int32_t> sortAscendings = {0};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();

    auto operatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, dataSize, false, partitionKeys, sortKeys,
                                                               sortAscendings, sortNullFirsts, overflowConfig);
    delete overflowConfig;
    return operatorFactory;
}

OperatorFactory *CreateDistinctLimitFactory(omniruntime::type::DataTypes &sourceTypes, int32_t loopCount)
{
    const int64_t limitCount = loopCount / 100;
    int32_t distinctCols[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto operatorFactory =
        DistinctLimitOperatorFactory::CreateDistinctLimitOperatorFactory(sourceTypes, distinctCols, 10, -1, limitCount);
    return operatorFactory;
}

OperatorFactory *CreateProjectFactory(omniruntime::type::DataTypes &sourceTypes, std::vector<Expr *> exprs,
                                      OverflowConfig *overflowConfig)
{
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, sourceTypes, overflowConfig);
    auto operatorFactory = new ProjectionOperatorFactory(move(exprEvaluator));
    return operatorFactory;
}

OperatorFactory *CreateUnionFactory(omniruntime::type::DataTypes &sourceTypes)
{
    auto operatorFactory = UnionOperatorFactory::CreateUnionOperatorFactory(sourceTypes, sourceTypes.GetSize(), false);
    return operatorFactory;
}

omniruntime::op::Operator *CreateSortMergeJoinOperator(omniruntime::type::DataTypes &sourceTypes,
                                                       OverflowConfig *overflowConfig)
{
    std::string blank = "";
    auto smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, blank);
    DataTypes streamedTblTypes(sourceTypes);
    std::vector<int32_t> streamedKeysCols;
    streamedKeysCols.push_back(1);
    std::vector<int32_t> streamedOutputCols = {0, 2, 3, 4, 5, 6, 7, 8, 9};
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, sourceTypes.GetSize());

    DataTypes bufferedTblTypes(sourceTypes);
    std::vector<int32_t> bufferedKeysCols;
    bufferedKeysCols.push_back(1);
    std::vector<int32_t> bufferedOutputCols = {0, 2, 3, 4, 5, 6, 7, 8, 9};
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, sourceTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(overflowConfig);
    return smjOp;
}

OperatorFactory *CreateTopNFactory(omniruntime::type::DataTypes &sourceTypes)
{
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};

    auto operatorFactory = new TopNOperatorFactory(sourceTypes, 5, 0, sortCols, ascendings, nullFirsts, 1);
    return operatorFactory;
}

OperatorFactory *CreateWindowFactory(omniruntime::type::DataTypes &sourceTypes)
{
    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {2};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_AGGREGATION_TYPE_AVG};
    int32_t partitionCols[1] = {1};
    int32_t preGroupedCols[0] = {};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;
    int32_t argumentChannels[1] = {2};
    DataTypes allTypes(
        std::vector<DataTypePtr>({ShortType(), IntType(), LongType(), BooleanType(), DoubleType(), Date32Type(DAY),
                                  Decimal64Type(SHORT_DECIMAL_SIZE, 0), Decimal128Type(LONG_DECIMAL_SIZE, 0),
                                  VarcharType(CHAR_SIZE), CharType(CHAR_SIZE), DoubleType()}));

    OverflowConfig overflowConfig(OVERFLOW_CONFIG_NULL);
    OperatorConfig operatorConfig(overflowConfig);
    // dealing data with the operator
    auto operatorFactory = WindowOperatorFactory::CreateWindowOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, allTypes, argumentChannels, 1, windowFrameTypes,
        windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels, operatorConfig);
    return operatorFactory;
}

OperatorFactory *CreateStreamedWithExprOperatorFactory()
{
    std::string filterJsonStr = "{\"exprType\":\"BINARY\","
                                "\"returnType\":4,"
                                "\"operator\":\"GREATER_THAN\","
                                "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                                "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":2}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    std::vector<int32_t> streamedOutputCols = {1};
    OperatorConfig operatorConfig;
    auto operatorFactory = StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
        streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filterJsonStr,
        operatorConfig);
    Expr::DeleteExprs(streamedEqualKeyExprs);
    return operatorFactory;
}

OperatorFactory *CreateSortMergeJoinV3Factory(StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory)
{
    OperatorConfig operatorConfig;
    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    auto operatorFactory = BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
        bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory, operatorConfig);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    return operatorFactory;
}
} // namespace DtFuzzFactoryCreateUtil
