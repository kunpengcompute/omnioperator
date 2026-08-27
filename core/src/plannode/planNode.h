/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <vector>
#include "vector/vector_batch.h"
#include "expression/expressions.h"
#include "operator/config/operator_config.h"
#include "operator/window/window_function_options.h"
#include "type/data_types.h"
#include "util/config/QueryConfig.h"
#include "connectors/Connector.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {
    enum BuildSide {
        OMNI_BUILD_UNKNOWN = 0,
        OMNI_BUILD_LEFT,
        OMNI_BUILD_RIGHT
    };
}
using namespace type;
using namespace op;
using namespace expressions;
using ExprPtr = Expr *;

using PlanNodeId = std::string;

class SortOrderInfo {
public:
    SortOrderInfo(bool ascending, bool nullsFirst) noexcept : ascending(ascending), nullsFirst(nullsFirst) {}

    bool IsAscending() const { return ascending; }

    bool IsNullsFirst() const { return nullsFirst; }

    bool operator==(const SortOrderInfo &other) const
    {
        return std::tie(ascending, nullsFirst) == std::tie(other.ascending, other.nullsFirst);
    }

    bool operator!=(const SortOrderInfo &other) const { return !(*this == other); }

    std::string ToString() const
    {
        return Format("{} NULLS {}", (ascending ? "ASC" : "DESC"), (nullsFirst ? "FIRST" : "LAST"));
    }

private:
    bool ascending;
    bool nullsFirst;
};

extern const SortOrderInfo K_ASC_NULLS_FIRST;
extern const SortOrderInfo K_ASC_NULLS_LAST;
extern const SortOrderInfo K_DESC_NULLS_FIRST;
extern const SortOrderInfo K_DESC_NULLS_LAST;

class PlanNode {
public:
    explicit PlanNode(const PlanNodeId &id) : id{id} {}

    virtual ~PlanNode() = default;

    virtual const DataTypesPtr &OutputType() const = 0;

    /// The name of the plan node, used in toString.
    virtual std::string_view Name() const = 0;

    const PlanNodeId &Id() const { return id; }

    /// Returns true if this plan node operator is spillable and 'queryConfig' has
    /// enabled it.
    virtual bool CanSpill(const config::QueryConfig &queryConfig) const { return false; }

    virtual const std::vector<std::shared_ptr<const PlanNode>> &Sources() const = 0;

private:
    const std::string id;
};

using PlanNodePtr = std::shared_ptr<const PlanNode>;

class OrderByNode : public PlanNode {
public:
    OrderByNode(const PlanNodeId& id, const std::vector<int32_t>& sortCols, const std::vector<int32_t>& sortAscending,
        const std::vector<int32_t>& sortNullFirsts, const PlanNodePtr& source,
        std::vector<omniruntime::expressions::Expr*>& sortExpressions)
        : PlanNode(id),
          sourceTypes(source->OutputType()),
          sortCols(sortCols),
          sortAscending(sortAscending),
          sortNullFirsts(sortNullFirsts),
          sources({source}),
          sortExpressions(sortExpressions)
    {
        outputCols.reserve(sourceTypes->GetSize());
        for (int i = 0; i < sourceTypes->GetSize(); ++i) {
            outputCols.push_back(i);
        }
    }

    ~OrderByNode() override = default;

    const DataTypesPtr &OutputType() const override { return sources[0]->OutputType(); }

    const std::vector<std::shared_ptr<const PlanNode>> &Sources() const override { return sources; }

    const std::vector<int32_t> &GetSortCols() const { return sortCols; }

    const std::vector<int32_t> &GetOutputCols() const { return outputCols; }
    const std::vector<omniruntime::expressions::Expr*>& GetExpressions() const
    {
        return sortExpressions;
    }

    const DataTypesPtr &GetSourceTypes() const { return sourceTypes; }

    const std::vector<int32_t> &GetSortAscending() const { return sortAscending; }

    const std::vector<int32_t> &GetNullFirsts() const { return sortNullFirsts; }

    bool CanSpill(const config::QueryConfig &queryConfig) const override { return queryConfig.orderBySpillEnabled(); }

    std::string_view Name() const override { return "OrderBy"; }

private:
    const DataTypesPtr sourceTypes;
    std::vector<int32_t> outputCols;
    const std::vector<int32_t> sortCols;
    const std::vector<int32_t> sortAscending;
    const std::vector<int32_t> sortNullFirsts;
    const std::vector<PlanNodePtr> sources;
    const OperatorConfig operatorConfig;
    std::vector<omniruntime::expressions::Expr *> sortExpressions;
};

class FilterNode : public PlanNode {
public:
    FilterNode(const PlanNodeId &id, ExprPtr filter, PlanNodePtr source, const std::vector<ExprPtr> &projectList)
        : PlanNode(id), sources{std::move(source)}, filter(std::move(filter)), projectList(projectList)
    {
        std::vector<DataTypePtr> joinInputTypes;
        if (!projectList.empty()) {
            for (int i = 0; i < projectList.size(); i++) {
                joinInputTypes.push_back(projectList[i]->dataType);
            }
            this->outputType = std::make_shared<DataTypes>(std::move(joinInputTypes));
        } else {
            this->outputType = sources[0]->OutputType();
        }
    }

    ~FilterNode() override = default;

    const DataTypesPtr &OutputType() const override { return outputType; }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    ExprPtr GetFilterExpr() const { return filter; }

    const std::vector<ExprPtr> &ProjectList() const {return projectList; }

    std::string_view Name() const override { return "Filter"; }

private:
    const std::vector<PlanNodePtr> sources;
    ExprPtr filter;
    const std::vector<ExprPtr> projectList;
    DataTypesPtr outputType;
};

class ProjectNode : public PlanNode {
public:
    ProjectNode(const PlanNodeId &id, std::vector<ExprPtr> &&inProjections, PlanNodePtr source)
        : PlanNode(id), sources{source}, projections(std::move(inProjections)),
        outputType(MakeOutputType(projections)) {}

    static DataTypesPtr MakeOutputType(const std::vector<ExprPtr> &projections)
    {
        std::vector<DataTypePtr> argTypes;
        for (auto project : projections) {
            argTypes.push_back(project->GetReturnType());
        }
        return std::make_shared<DataTypes>(std::move(argTypes));
    }

    ~ProjectNode() override = default;

    const DataTypesPtr &OutputType() const override
    {
        return outputType;
    }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    const std::vector<ExprPtr> &GetProjections() const { return projections; }

    std::string_view Name() const override { return "Project"; }

protected:
    const std::vector<PlanNodePtr> sources;
    const std::vector<ExprPtr> projections;
    const DataTypesPtr outputType;
};

class AggregationNode : public PlanNode {
public:
    enum class Step {
        // raw input in - partial result out
        K_PARTIAL,
        // partial result in - final result out
        K_FINAL,
        // partial result in - partial result out
        K_INTERMEDIATE,
        // raw input in - final result out
        K_SINGLE
    };

    AggregationNode(const PlanNodeId &id, const std::vector<ExprPtr> &groupByKeys, const uint32_t groupByNum,
        const std::vector<std::vector<ExprPtr>> &aggKeys, const DataTypesPtr sourceDataTypes,
        const std::vector<DataTypes> aggsOutputTypes, const std::vector<uint32_t> &aggFuncTypes,
        const std::vector<ExprPtr> &aggFilters, const std::vector<uint32_t> &maskColumns,
        const std::vector<bool> &inputRaws, const std::vector<bool> &outputPartials, const bool isStatisticalAggregate,
        const DataTypesPtr outputType, PlanNodePtr source, Step step, bool mixedInputExpected = false,
        bool mixedOutputEnabled = false)
        : PlanNode(id), groupByKeys(groupByKeys), groupByNum(groupByNum), aggKeys(aggKeys),
          sourceDataTypes(sourceDataTypes), aggsOutputTypes(aggsOutputTypes), aggFuncTypes(aggFuncTypes),
          aggFilters(aggFilters), maskColumns(maskColumns), inputRaws(inputRaws), outputPartials(outputPartials),
        isStatisticalAggregate(isStatisticalAggregate), outputType(outputType), sources({source}), step(step),
        mixedInputExpected(mixedInputExpected), mixedOutputEnabled(mixedOutputEnabled) {}

    ~AggregationNode() override = default;

    std::string_view Name() const override { return "Aggregation"; }

    const DataTypesPtr &OutputType() const override { return outputType; }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    const std::vector<ExprPtr> &GetGroupByKeys() const { return groupByKeys; }

    const uint32_t GetGroupByNum() const { return groupByNum; }

    const std::vector<std::vector<ExprPtr>> GetAggsKeys() const { return aggKeys; }

    DataTypesPtr GetSourceDataTypes() const { return sourceDataTypes; }

    const std::vector<uint32_t> &GetAggFuncTypes() const { return aggFuncTypes; }

    const std::vector<ExprPtr> GetAggFilters() const { return aggFilters; }

    const std::vector<uint32_t> GetMaskColumns() const { return maskColumns; }

    const std::vector<bool> GetInputRaws() const { return inputRaws; }

    const std::vector<bool> GetOutputPartials() const { return outputPartials; }

    const bool GetIsStatisticalAggregate() const { return isStatisticalAggregate; }

    const std::vector<DataTypes> &GetAggsOutputTypes() const { return aggsOutputTypes; }

    Step GetStep() const { return step;}

    bool IsMixedInputExpected() const { return mixedInputExpected; }

    bool IsMixedOutputEnabled() const { return mixedOutputEnabled; }

private:
    const std::vector<ExprPtr> groupByKeys;
    const uint32_t groupByNum;
    const std::vector<std::vector<ExprPtr>> aggKeys;
    const DataTypesPtr sourceDataTypes;
    const std::vector<DataTypes> aggsOutputTypes; // 要改成这个
    const std::vector<uint32_t> aggFuncTypes;
    const std::vector<ExprPtr> aggFilters;
    const std::vector<uint32_t> maskColumns;
    const std::vector<bool> inputRaws;
    const std::vector<bool> outputPartials;
    bool isStatisticalAggregate;
    const DataTypesPtr outputType;
    const std::vector<PlanNodePtr> sources;
    const Step step;
    const bool mixedInputExpected;
    const bool mixedOutputEnabled;
};

class WindowNode : public PlanNode {
public:
    WindowNode(const PlanNodeId &id,
        const std::vector<int32_t> &windowFunctionTypes,
        const std::vector<int32_t> &partitionCols,
        const std::vector<int32_t> &preGroupedCols,
        const std::vector<int32_t> &sortCols,
        const std::vector<int32_t> &sortAscending,
        const std::vector<int32_t> &sortNullFirsts,
        const int32_t preSortedChannelPrefix,
        const int32_t expectedPositionsCount,
        const DataTypesPtr &windowFunctionReturnTypes,
        const DataTypesPtr &allTypes,
        const std::vector<omniruntime::expressions::Expr *> argumentKeys,
        const std::vector<int32_t> &windowFrameTypes,
        const std::vector<int32_t> &windowFrameStartTypes,
        const std::vector<int32_t> &windowFrameStartChannels,
        const std::vector<int32_t> &windowFrameEndTypes,
        const std::vector<int32_t> &windowFrameEndChannels,
        const std::vector<WindowFunctionOptions> &windowFunctionOptions,
        const PlanNodePtr &source)
        : PlanNode(id),
          windowFunctionTypes(windowFunctionTypes),
          partitionCols(partitionCols),
          preGroupedCols(preGroupedCols),
          sortCols(sortCols),
          sortAscending(sortAscending),
          sortNullFirsts(sortNullFirsts),
          preSortedChannelPrefix(preSortedChannelPrefix),
          expectedPositionsCount(expectedPositionsCount),
          windowFunctionReturnTypes(windowFunctionReturnTypes),
          allTypes(allTypes),
          argumentKeys(argumentKeys),
          windowFrameTypes(windowFrameTypes),
          windowFrameStartTypes(windowFrameStartTypes),
          windowFrameStartChannels(windowFrameStartChannels),
          windowFrameEndTypes(windowFrameEndTypes),
          windowFrameEndChannels(windowFrameEndChannels),
          windowFunctionOptions(windowFunctionOptions),
          sources({source}),
          sourceTypes(source->OutputType())
    {
        outputCols.reserve(sourceTypes->GetSize());
        for (int i = 0; i < sourceTypes->GetSize(); ++i) {
            outputCols.push_back(i);
        }
    }

    ~WindowNode() override = default;

    const DataTypesPtr &GetSourceTypes() const { return sourceTypes; }

    const std::vector<int32_t> &GetOutputCols() const { return outputCols; }

    const std::vector<int32_t> &GetWindowFunctionTypes() const { return windowFunctionTypes; }

    const std::vector<int32_t> &GetPartitionCols() const { return partitionCols; }

    const std::vector<int32_t> &GetPreGroupedCols() const { return preGroupedCols; }

    const std::vector<int32_t> &GetSortCols() const { return sortCols; }

    const std::vector<int32_t> &GetSortAscending() const { return sortAscending; }

    const std::vector<int32_t> &GetNullFirsts() const { return sortNullFirsts; }

    const int32_t GetPreSortedChannelPrefix() const { return preSortedChannelPrefix; }

    const int32_t GetExpectedPositionsCount() const { return expectedPositionsCount; }

    const DataTypesPtr &GetWindowFunctionReturnTypes() const { return windowFunctionReturnTypes; }

    const std::vector<omniruntime::expressions::Expr *> &GetArgumentKeys() const { return argumentKeys; }

    const std::vector<int32_t> &GetWindowFrameTypes() const { return windowFrameTypes; }

    const std::vector<int32_t> &GetWindowFrameStartTypes() const { return windowFrameStartTypes; }

    const std::vector<int32_t> &GetWindowFrameStartChannels() const { return windowFrameStartChannels; }

    const std::vector<int32_t> &GetWindowFrameEndTypes() const { return windowFrameEndTypes; }

    const std::vector<int32_t> &GetWindowFrameEndChannels() const{ return windowFrameEndChannels; }

    const std::vector<WindowFunctionOptions> &GetWindowFunctionOptions() const { return windowFunctionOptions; }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    /// The outputType is the concatenation of the input columns
    /// with the output columns of each window function.
    const DataTypesPtr &OutputType() const override { return allTypes; }

    std::string_view Name() const override { return "Window"; }

    bool CanSpill(const config::QueryConfig &queryConfig) const override {
        return queryConfig.orderBySpillEnabled();
    }

private:
    const std::vector<int32_t> windowFunctionTypes;
    const std::vector<int32_t> partitionCols;
    const std::vector<int32_t> preGroupedCols;
    const std::vector<int32_t> sortCols;
    const std::vector<int32_t> sortAscending;
    const std::vector<int32_t> sortNullFirsts;
    const int32_t preSortedChannelPrefix;
    const int32_t expectedPositionsCount;
    const DataTypesPtr windowFunctionReturnTypes;
    const DataTypesPtr allTypes;
    const std::vector<omniruntime::expressions::Expr *> argumentKeys;
    const std::vector<int32_t> windowFrameTypes;
    const std::vector<int32_t> windowFrameStartTypes;
    const std::vector<int32_t> windowFrameStartChannels;
    const std::vector<int32_t> windowFrameEndTypes;
    const std::vector<int32_t> windowFrameEndChannels;
    const std::vector<WindowFunctionOptions> windowFunctionOptions;
    const std::vector<PlanNodePtr> sources;
    const DataTypesPtr sourceTypes;
    std::vector<int32_t> outputCols;
};

class WindowGroupLimitNode : public PlanNode {
public:
    WindowGroupLimitNode(
        const PlanNodeId& id,
        const PlanNodePtr& source,
        int32_t n,
        const std::string& funcName,
        const std::vector<omniruntime::expressions::Expr *>& partitionKeys,
        const std::vector<omniruntime::expressions::Expr *>& sortKeys,
        const std::vector<int32_t>& sortAscendings,
        const std::vector<int32_t>& sortNullFirsts,
        const DataTypesPtr& outputTypes
    )
    : PlanNode(id),
      sources({source}),
      sourceTypes(source->OutputType()),
      n(n),
      funcName(funcName),
      partitionKeys(partitionKeys),
      sortKeys(sortKeys),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts),
      outputTypes(outputTypes) { }

    ~WindowGroupLimitNode() override {
        for (auto *expr : partitionKeys){
            if(expr != nullptr){
                delete expr;
            }
        }

        for (auto *expr : sortKeys){
            if(expr != nullptr){
                delete expr;
            }
        }
    }

    const DataTypesPtr &GetSourceType() const { return sourceTypes; }

    int32_t GetN() const { return n; }

    const std::string& GetFuncName() const { return funcName; }

    const std::vector<omniruntime::expressions::Expr *>& GetPartitionKeys() const { return partitionKeys; }

    const std::vector<omniruntime::expressions::Expr *>& GetSortKeys() const { return sortKeys; }

    const std::vector<int32_t>& GetSortAscendings() const { return sortAscendings; }

    const std::vector<int32_t>& GetSortNullFirsts() const { return sortNullFirsts; }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    const DataTypesPtr &OutputType() const override { return outputTypes; }

    std::string_view Name() const override { return "WindowGroupLimit"; }

private:
    const DataTypesPtr sourceTypes;
    const int32_t n;
    const std::string funcName;
    const std::vector<omniruntime::expressions::Expr *> partitionKeys;
    const std::vector<omniruntime::expressions::Expr *> sortKeys;
    const std::vector<int32_t> sortAscendings;
    const std::vector<int32_t> sortNullFirsts;
    const std::vector<PlanNodePtr> sources;
    const DataTypesPtr outputTypes;
};

enum JoinType {
    OMNI_JOIN_TYPE_INNER = 0,
    OMNI_JOIN_TYPE_LEFT,
    OMNI_JOIN_TYPE_RIGHT,
    OMNI_JOIN_TYPE_FULL,
    OMNI_JOIN_TYPE_LEFT_SEMI,
    OMNI_JOIN_TYPE_LEFT_ANTI,
    OMNI_JOIN_TYPE_EXISTENCE
};

/// Abstract class representing inner/outer/semi/anti joins. Used as a base
/// class for specific join implementations, e.g. hash and merge joins.
class AbstractJoinNode : public PlanNode {
public:
    AbstractJoinNode(const PlanNodeId &id, JoinType joinType_, BuildSide buildSide_, const std::vector<ExprPtr> &leftKeys_, const std::vector<ExprPtr> &rightKeys_,
        ExprPtr filter_, PlanNodePtr left_, PlanNodePtr right_, DataTypesPtr leftOutputType_, DataTypesPtr rightOutputType_, const std::vector<ExprPtr> &partitionKeys_)
        : PlanNode(id), joinType(joinType_), buildSide(buildSide_), leftKeys(leftKeys_), rightKeys(rightKeys_), filter(std::move(filter_)), sources({std::move(left_),
        std::move(right_)}), leftOutputType(std::move(leftOutputType_)), rightOutputType(std::move(rightOutputType_)), partitionKeys(partitionKeys_)
    {
        std::vector<DataTypePtr> joinInputTypes;
        if (!partitionKeys.empty()) {
            for (int i = 0; i < partitionKeys.size(); i++) {
                joinInputTypes.push_back(partitionKeys[i]->dataType);
            }
            this->outputType = std::make_shared<DataTypes>(std::move(joinInputTypes));
        } else {
            this->outputType = GetOutputType();
        }
    }

    ~AbstractJoinNode()
    {
        if (!partitionKeys.empty()) {
            for (auto expr: partitionKeys) {
                delete expr;
            }
        }
    }

    const std::vector<PlanNodePtr> &Sources() const override
    {
        return sources;
    }

    const DataTypesPtr &OutputType() const override
    {
        return outputType;
    }

    const DataTypesPtr &LeftOutputType() const
    {
        return leftOutputType;
    }

    const DataTypesPtr &RightOutputType() const
    {
        return rightOutputType;
    }

    JoinType GetJoinType() const
    {
        return joinType;
    }

    BuildSide GetBuildSide() const
    {
        return buildSide;
    }

    bool IsInnerJoin() const
    {
        return joinType == OMNI_JOIN_TYPE_INNER;
    }

    bool IsLeftJoin() const
    {
        return joinType == JoinType::OMNI_JOIN_TYPE_LEFT;
    }

    bool IsRightJoin() const
    {
        return joinType == JoinType::OMNI_JOIN_TYPE_RIGHT;
    }

    bool IsFullJoin() const
    {
        return joinType == JoinType::OMNI_JOIN_TYPE_FULL;
    }

    bool IsLeftSemi() const
    {
        return joinType == JoinType::OMNI_JOIN_TYPE_LEFT_SEMI;
    }

    bool IsLeftAnti() const
    {
        return joinType == JoinType::OMNI_JOIN_TYPE_LEFT_ANTI;
    }

    bool IsExistence() const
    {
        return joinType == JoinType::OMNI_JOIN_TYPE_EXISTENCE;
    }

    bool IsBuildLeft() const
    {
        return buildSide == BuildSide::OMNI_BUILD_LEFT;
    }

    bool IsBuildRight() const
    {
        return buildSide == BuildSide::OMNI_BUILD_RIGHT;
    }

    const std::vector<ExprPtr> &LeftKeys() const
    {
        return leftKeys;
    }

    const std::vector<ExprPtr> &RightKeys() const
    {
        return rightKeys;
    }

    const ExprPtr Filter() const
    {
        return filter;
    }

    std::shared_ptr<DataTypes> GetOutputType()
    {
        bool outputMayIncludeLeftColumns = !((IsLeftSemi() || IsExistence()) && IsBuildLeft());
        bool outputMayIncludeRightColumns = !(((IsLeftSemi() || IsExistence()) && IsBuildRight()) || IsLeftAnti());
        if (outputMayIncludeLeftColumns && outputMayIncludeRightColumns) {
            auto outputSize = leftOutputType->GetSize() + rightOutputType->GetSize();
            std::vector<DataTypePtr> joinInputTypes;
            joinInputTypes.reserve(outputSize);
            joinInputTypes.insert(joinInputTypes.end(), leftOutputType->Get().begin(), leftOutputType->Get().end());
            joinInputTypes.insert(joinInputTypes.end(), rightOutputType->Get().begin(),
                                  rightOutputType->Get().end());
            if (buildSide == OMNI_BUILD_LEFT) {
                std::rotate(joinInputTypes.begin(), joinInputTypes.begin() + leftOutputType->Get().size(),
                            joinInputTypes.end());
            }
            return std::make_shared<DataTypes>(std::move(joinInputTypes));
        } else if (outputMayIncludeLeftColumns) {
            int extraCnt = IsExistence() ? 1 : 0;
            auto outputSize = leftOutputType->GetSize() + extraCnt;
            std::vector<DataTypePtr> joinInputTypes;
            joinInputTypes.reserve(outputSize);
            joinInputTypes.insert(joinInputTypes.end(), leftOutputType->Get().begin(), leftOutputType->Get().end());
            if (extraCnt > 0) {
                joinInputTypes.emplace_back(BooleanDataType::Instance());
            }
            if (buildSide == OMNI_BUILD_LEFT) {
                std::rotate(joinInputTypes.begin(), joinInputTypes.begin() + leftOutputType->Get().size(),
                            joinInputTypes.end());
            }
            return std::make_shared<DataTypes>(std::move(joinInputTypes));
        } else if (outputMayIncludeRightColumns) {
            int extraCnt = IsExistence() ? 1 : 0;
            auto outputSize = rightOutputType->GetSize() + extraCnt;
            std::vector<DataTypePtr> joinInputTypes;
            joinInputTypes.reserve(outputSize);
            joinInputTypes.insert(joinInputTypes.end(), rightOutputType->Get().begin(),
                                  rightOutputType->Get().end());
            if (extraCnt > 0) {
                joinInputTypes.emplace_back(BooleanDataType::Instance());
            }
            if (buildSide == OMNI_BUILD_LEFT) {
                std::rotate(joinInputTypes.begin(), joinInputTypes.begin() + rightOutputType->Get().size(),
                            joinInputTypes.end());
            }
            return std::make_shared<DataTypes>(std::move(joinInputTypes));
        }
    }

protected:
    const JoinType joinType;
    const BuildSide buildSide;
    const std::vector<ExprPtr> leftKeys;
    const std::vector<ExprPtr> rightKeys;
    // Optional join filter, nullptr if absent. This is applied to
    // join hits and if this is false, the hit turns into a miss, which
    // has a special meaning for outer joins. For inner joins, this is
    // equivalent to a Filter above the join.
    const ExprPtr filter;
    const std::vector<PlanNodePtr> sources;
    const DataTypesPtr leftOutputType;
    const DataTypesPtr rightOutputType;
    DataTypesPtr outputType;
    const std::vector<ExprPtr> partitionKeys;
};

/// Represents inner/outer/semi/anti hash joins. Translates to an
/// exec::HashBuild and exec::HashProbe. A separate pipeline is produced for
/// the build side when generating exec::Operators.
///
/// 'nullAware' boolean applies to semi and anti joins. When true, the join
/// semantic is IN / NOT IN. When false, the join semantic is EXISTS / NOT
/// EXISTS.
class HashJoinNode : public AbstractJoinNode {
public:
    HashJoinNode(const PlanNodeId &id, JoinType joinType, BuildSide buildSide, bool nullAware, bool isShuffle, const std::vector<ExprPtr> &leftKeys,
        const std::vector<ExprPtr> &rightKeys, ExprPtr filter, PlanNodePtr left, PlanNodePtr right, DataTypesPtr leftOutputType,
        DataTypesPtr rightOutputType, const std::vector<omniruntime::expressions::Expr*>& partitionKeys)
        : AbstractJoinNode(id, joinType, buildSide, leftKeys, rightKeys, std::move(filter), std::move(left), std::move(right), std::move(leftOutputType), std::move(rightOutputType), partitionKeys),
        nullAware{nullAware}, isShuffle{isShuffle} {}

    std::string_view Name() const override
    {
        return "HashJoin";
    }

    bool IsNullAware() const
    {
        return nullAware;
    }

    bool IsShuffle() const
    {
        return isShuffle;
    }

    std::vector<omniruntime::expressions::Expr*> PartitionKeys() const
    {
        return partitionKeys;
    }

private:
    const bool nullAware;
    const bool isShuffle;
};

/// Represents inner/outer/semi/anti merge joins. Translates to an
/// exec::MergeJoin operator. Assumes that both left and right input data is
/// sorted on the join keys. A separate pipeline that puts its output into
/// exec::MergeJoinSource is produced for the right side when generating
/// exec::Operators.
class MergeJoinNode : public AbstractJoinNode {
public:
    MergeJoinNode(const PlanNodeId &id, JoinType joinType, BuildSide buildSide, const std::vector<ExprPtr> &leftKeys,
        const std::vector<ExprPtr> &rightKeys, ExprPtr filter, PlanNodePtr left, PlanNodePtr right, DataTypesPtr leftOutputType, DataTypesPtr rightOutputType, const std::vector<ExprPtr>& partitionKeys)
        : AbstractJoinNode(id, joinType, buildSide, leftKeys, rightKeys, std::move(filter), std::move(left), std::move(right),
        std::move(leftOutputType), std::move(rightOutputType), partitionKeys) {}

    std::string_view Name() const override
    {
        return "MergeJoin";
    }
};

/// Represents inner/outer nested loop joins. Translates to an
/// exec::NestedLoopJoinProbe and exec::NestedLoopJoinBuild. A separate
/// pipeline is produced for the build side when generating exec::Operators.
///
/// Nested loop join (NLJ) supports both equal and non-equal joins.
/// Expressions specified in joinCondition are evaluated on every combination
/// of left/right tuple, to emit result. Results are emitted following the
/// same input order of probe rows for inner and left joins, for each thread
/// of execution.
///
/// To create Cartesian product of the left/right's output, use the
/// constructor without `joinType` and `joinCondition` parameter.
class NestedLoopJoinNode : public PlanNode {
public:
    NestedLoopJoinNode(const PlanNodeId &id, JoinType joinType_, ExprPtr filter_, PlanNodePtr left_, PlanNodePtr right_,
        DataTypesPtr leftOutputType_, DataTypesPtr rightOutputType_)
        : PlanNode(id), joinType(joinType_), filter(std::move(filter_)), sources({std::move(left_), std::move(right_)}),
        leftOutputType(std::move(leftOutputType_)), rightOutputType(std::move(rightOutputType_))
    {
        auto outputSize = leftOutputType->GetSize() + rightOutputType->GetSize();
        std::vector<DataTypePtr> joinInputTypes;
        joinInputTypes.reserve(outputSize);

        joinInputTypes.insert(joinInputTypes.end(), leftOutputType->Get().begin(), leftOutputType->Get().end());
        joinInputTypes.insert(joinInputTypes.end(), rightOutputType->Get().begin(), rightOutputType->Get().end());
        this->outputType = std::make_shared<DataTypes>(std::move(joinInputTypes));
    }

    const std::vector<PlanNodePtr> &Sources() const override
    {
        return sources;
    }

    const DataTypesPtr &OutputType() const override
    {
        return outputType;
    }

    std::string_view Name() const override
    {
        return "NestedLoopJoin";
    }

    const ExprPtr Filter() const
    {
        return filter;
    }

    const JoinType GetJoinType() const
    {
        return joinType;
    }

    const DataTypesPtr &LeftOutputType() const
    {
        return leftOutputType;
    }

    const DataTypesPtr &RightOutputType() const
    {
        return rightOutputType;
    }

    /// If nested loop join supports this join type.
    static bool IsSupported(JoinType joinTypeParam)
    {
        switch (joinTypeParam) {
            case OMNI_JOIN_TYPE_INNER:
            case OMNI_JOIN_TYPE_LEFT:
            case OMNI_JOIN_TYPE_RIGHT:
            case OMNI_JOIN_TYPE_FULL:
                return true;
            default:
                return false;
        }
    }

private:
    const JoinType joinType;
    const ExprPtr filter;
    const std::vector<PlanNodePtr> sources;
    const DataTypesPtr leftOutputType;
    const DataTypesPtr rightOutputType;
    DataTypesPtr outputType;
};

class TopNNode : public PlanNode {
public:
    TopNNode(const PlanNodeId &id, const std::vector<omniruntime::expressions::Expr *> &sortCols,
        const std::vector<int32_t> &sortAscending, const std::vector<int32_t> &sortNullFirsts,
        int32_t count, const PlanNodePtr &source)
        : PlanNode(id), sourceTypes(source->OutputType()), sortCols(sortCols), sortAscending(sortAscending),
          sortNullFirsts(sortNullFirsts), count(count), sources({source})
    {}

    const std::vector<omniruntime::expressions::Expr *> &GetSortCols() const { return sortCols; }

    const std::vector<int32_t> &GetSortAscending() const { return sortAscending; }

    const std::vector<int32_t> &GetNullFirsts() const { return sortNullFirsts; }

    const DataTypesPtr &OutputType() const override { return sources[0]->OutputType(); }

    const DataTypesPtr &GetSourceTypes() const { return sourceTypes; }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    int32_t Count() const { return count; }

    std::string_view Name() const override { return "TopN"; }

private:
    const DataTypesPtr sourceTypes;
    const std::vector<omniruntime::expressions::Expr *> sortCols;
    const std::vector<int32_t> sortAscending;
    const std::vector<int32_t> sortNullFirsts;
    const int32_t count;
    const std::vector<PlanNodePtr> sources;
};

class TopNSortNode : public PlanNode {
public:
    TopNSortNode(const PlanNodeId &id, const std::vector<omniruntime::expressions::Expr*>& partitionKeys,
        const std::vector<omniruntime::expressions::Expr*>& sortKeys, const std::vector<int32_t>& sortAscendings,
        const std::vector<int32_t>& sortNullFirsts, int32_t n, bool isStrictTopN, const PlanNodePtr &source)
        : PlanNode(id), sourceTypes(source->OutputType()), n(n), isStrictTopN(isStrictTopN), partitionKeys(partitionKeys),
        sortKeys(sortKeys), sortAscendings(sortAscendings), sortNullFirsts(sortNullFirsts), sources({source})
    {}

    DataTypesPtr getSourceTypes() const { return sourceTypes; }
    int32_t getN() const { return n; }
    bool getIsStrictTopN() const { return isStrictTopN; }
    const std::vector<omniruntime::expressions::Expr*>& getPartitionKeys() const { return partitionKeys; }
    const std::vector<omniruntime::expressions::Expr*>& getSortKeys() const { return sortKeys; }
    const std::vector<int32_t>& getSortAscendings() const { return sortAscendings; }
    const std::vector<int32_t>& getSortNullFirsts() const { return sortNullFirsts; }

    std::string_view Name() const override { return "TopNSort"; }
    const DataTypesPtr &OutputType() const override { return sources[0]->OutputType(); }
    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

private:
    const DataTypesPtr sourceTypes;
    const int32_t n;
    const bool isStrictTopN;
    const std::vector<omniruntime::expressions::Expr *> partitionKeys;
    const std::vector<omniruntime::expressions::Expr *> sortKeys;
    const std::vector<int32_t> sortAscendings;
    const std::vector<int32_t> sortNullFirsts;
    const std::vector<PlanNodePtr> sources;
};

class LimitNode : public PlanNode {
public:
    // @param isPartial Boolean indicating whether Limit node generates partial
    // results on local workers or finalizes the partial results from `PARTIAL`
    // nodes.
    LimitNode(const PlanNodeId &id, int32_t offset, int32_t count, bool isPartial, const PlanNodePtr &source)
        : PlanNode(id), offset(offset), count(count), isPartial(isPartial), sources{source}
    {}

    const DataTypesPtr &OutputType() const override { return sources[0]->OutputType(); }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    int32_t Offset() const { return offset; }

    int32_t Count() const { return count; }

    bool IsPartial() const { return isPartial; }

    std::string_view Name() const override { return "Limit"; }

private:
    const int32_t offset;
    const int32_t count;
    const bool isPartial;
    const std::vector<PlanNodePtr> sources;
};

class UnionNode : public PlanNode {
public:
    UnionNode(const PlanNodeId &id, std::vector<PlanNodePtr> sources, bool isDistinct)
        : PlanNode(id), isDistinct(isDistinct), sources{std::move(sources)}
    {}

    const DataTypesPtr &OutputType() const override { return sources[0]->OutputType(); }

    const std::vector<PlanNodePtr> &Sources() const override { return sources; }

    std::string_view Name() const override { return "Union"; }

    const DataTypesPtr &GetSourceTypes() const { return sources[0]->OutputType(); }

    const bool &IsDistinct() const { return isDistinct; }

private:
    const bool isDistinct;
    const std::vector<PlanNodePtr> sources;
};

class TableScanNode : public PlanNode {
public:
    TableScanNode(
        const PlanNodeId &id,
        std::vector <std::shared_ptr<DataType>> types,
        std::vector <std::string> names,
        const std::shared_ptr <connector::ConnectorTableHandle> tableHandle,
        const std::unordered_map <
        std::string,
        std::shared_ptr<connector::ColumnHandle>> assignments)
        : PlanNode(id),
          tableHandle_(tableHandle),
          assignments_(assignments),
          types_(types),
          names_(names)
    {
        outputTypes_ = std::make_shared<DataTypes>(types_);
    }

    bool supportsBarrier()
    {
        return true;
    }

    const std::vector <PlanNodePtr> &Sources() const override
    {
        return sources;
    }

    const DataTypesPtr &OutputType() const override
    {
        return outputTypes_;
    }

    RowTypePtr getRowTypePtr() const
    {
        return std::make_shared<const RowType>(types_, names_);
    }

    const std::shared_ptr <connector::ConnectorTableHandle> tableHandle() const
    {
        return tableHandle_;
    }

    const std::
    unordered_map <std::string, std::shared_ptr<connector::ColumnHandle>>
    assignments() const
    {
        return assignments_;
    }

    std::string_view Name() const override
    {
        return "TableScan";
    }

private:
    std::vector <std::shared_ptr<DataType>> types_;
    std::vector <std::string> names_;
    const std::shared_ptr <connector::ConnectorTableHandle> tableHandle_;
    const std::
    unordered_map <std::string, std::shared_ptr<connector::ColumnHandle>>
            assignments_;
    const std::vector <PlanNodePtr> sources;
    DataTypesPtr outputTypes_;
};

class ExpandNode : public PlanNode {
public:
    ExpandNode(const PlanNodeId &id, std::vector<std::vector<ExprPtr>> &&projections, PlanNodePtr source)
        : PlanNode(id), sources{source}, projections(std::move(projections))
    {
        std::vector<DataTypePtr> types;
        if (this->projections.size() > 0) {
            types.reserve(this->projections[0].size());
            for (const auto &projection: this->projections[0]) {
                types.push_back(projection->GetReturnType());
            }
        }
        this->outputType = std::make_shared<DataTypes>(std::move(types));
    }

    const DataTypesPtr &OutputType() const override
    {
        return outputType;
    }

    const DataTypesPtr& InputType() const
    {
        return sources[0]->OutputType();
    }

    const std::vector<PlanNodePtr>& Sources() const override
    {
        return sources;
    }

    const std::vector<std::vector<ExprPtr>>& GetProjections() const
    {
        return projections;
    }

    std::string_view Name() const override
    {
        return "Expand";
    }

private:
    const std::vector<PlanNodePtr> sources;
    const std::vector<std::vector<ExprPtr>> projections;
    DataTypesPtr outputType;
};

class GroupingNode : public PlanNode {
public:
    GroupingNode(const PlanNodeId &id, const std::shared_ptr<const ExpandNode> &expandPlanNode,
        const std::shared_ptr<const AggregationNode> &aggregationNode)
        : PlanNode(id), expandPlanNode_(expandPlanNode), aggregationNode_(aggregationNode),
        sources_(expandPlanNode_->Sources()), outputType_(aggregationNode_->OutputType()) {}

    std::shared_ptr<const ExpandNode> GetExpandPlanNode() const
    {
        return expandPlanNode_;
    }

    std::shared_ptr<const AggregationNode> GetAggregationNode() const
    {
        return aggregationNode_;
    }

    const DataTypesPtr &OutputType() const override
    {
        return outputType_;
    }

    const std::vector<PlanNodePtr> &Sources() const override
    {
        return sources_;
    }

    std::string_view Name() const override
    {
        return "Grouping";
    }

private:
    const std::shared_ptr<const ExpandNode> expandPlanNode_;
    const std::shared_ptr<const AggregationNode> aggregationNode_;
    const std::vector<PlanNodePtr> sources_;
    DataTypesPtr outputType_;
};

/// Expands arrays and maps into separate columns. Arrays are expanded into a
/// single column, and maps are expanded into two columns (key, value). Can be
/// used to expand multiple columns. In this case will produce as many rows as
/// the highest cardinality array or map (the other columns are padded with
/// nulls). Optionally can produce an ordinality column that specifies the row
/// number starting with 1.
class UnnestNode : public PlanNode {
public:
    /// @param replicateVariables Inputs that are projected as is
    /// @param unnestVariables Inputs that are unnested. Must be of type ARRAY
    /// or MAP.
    UnnestNode(const PlanNodeId& id, std::vector<ExprPtr> replicateVariables,
        std::vector<ExprPtr> unnestVariables, const PlanNodePtr& source, bool withOrdinality = false, bool outer = false)
        : PlanNode(id), replicateVariables_(std::move(replicateVariables)),
        unnestVariables_(std::move(unnestVariables)), sources_{source}, withOrdinality_(withOrdinality), outer_(outer),
        outputType_(MakeOutputType(replicateVariables_, unnestVariables_, withOrdinality_, outer_)) {}

    /// The order of columns in the output is: replicated columns (in the order
    /// specified), unnested columns (in the order specified, for maps: key
    /// comes before value), optional ordinality column.
    static DataTypesPtr MakeOutputType(const std::vector<ExprPtr>& replicateVariables,
                                       const std::vector<ExprPtr>& unnestVariables, bool withOrdinality = false, bool outer = false)
    {
        std::vector<DataTypePtr> dataTypes;
        for (const auto &variable : replicateVariables) {
            dataTypes.push_back(variable->dataType);
        }
        for (const auto &variable : unnestVariables) {
            auto fieldExpr = dynamic_cast<FieldExpr *>(variable);
            if (fieldExpr->FieldIsArray()) {
                auto arrayType = dynamic_cast<ArrayType*>(variable->dataType.get());
                dataTypes.push_back(arrayType->ElementType());
            } else if (fieldExpr->FieldIsMap()) {
                auto mapType = dynamic_cast<MapType*>(variable->dataType.get());
                dataTypes.push_back(mapType->Key());
                dataTypes.push_back(mapType->Value());
            } else {
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Unnest operator supports only ARRAY and MAP types");
            }
        }

        if (withOrdinality) {
            dataTypes.push_back(LongType());
        }
        // Note: outer parameter doesn't affect output type, but we keep it for consistency
        return std::make_shared<DataTypes>(std::move(dataTypes));
    }

    const DataTypesPtr &OutputType() const override
    {
        return outputType_;
    }

    const std::vector<PlanNodePtr>& Sources() const override
    {
        return sources_;
    }

    const std::vector<ExprPtr>& replicateVariables() const
    {
        return replicateVariables_;
    }

    const std::vector<ExprPtr>& unnestVariables() const
    {
        return unnestVariables_;
    }

    bool withOrdinality() const
    {
        return withOrdinality_;
    }

    bool outer() const
    {
        return outer_;
    }

    std::string_view Name() const override
    {
        return "Unnest";
    }

private:
    const std::vector<ExprPtr> replicateVariables_;
    const std::vector<ExprPtr> unnestVariables_;
    const bool withOrdinality_;
    const bool outer_;
    const std::vector<PlanNodePtr> sources_;
    const DataTypesPtr outputType_;
};

/// Calculates partition number for each row of the specified vector.
class PartitionFunction {
public:
    virtual ~PartitionFunction() = default;

    /// @param input VectorBatch to split into partitions.
    /// @param [out] partitions Computed partition numbers for each row in
    /// 'input'.
    /// @return Returns partition number in case all rows of 'input' are
    /// assigned to the same partition. In this case 'partitions' vector is left
    /// unchanged. Used to optimize round-robin partitioning in local exchange.
    virtual std::optional<uint32_t> partition(
        const vec::VectorBatch& input,                                                                   //todo  RowVector
        std::vector<uint32_t>& partitions) = 0;
};

/// Factory class for creating PartitionFunction instances.
class PartitionFunctionSpec {
public:
    /// If 'localExchange' is true, the partition function is used for local
    /// exchange within a velox task.
    virtual std::unique_ptr<PartitionFunction> create(
        int numPartitions,
        bool localExchange = false) const = 0;

    virtual ~PartitionFunctionSpec() = default;

    virtual std::string toString() const = 0;
};

} // namespace omniruntime
