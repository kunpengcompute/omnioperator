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

#ifndef OMNI_RUNTIME_EXPR_EVALUATOR_H
#define OMNI_RUNTIME_EXPR_EVALUATOR_H

#include "operator/execution_context.h"
#include "codegen/filter_codegen.h"
#ifndef EXCLUDE_BATCH_FUNCTIONS
#include "codegen/batch_filter_codegen.h"
#endif
#include "codegen/projection_codegen.h"
#ifndef EXCLUDE_BATCH_FUNCTIONS
#include "codegen/batch_projection_codegen.h"
#endif
#include "util/config_util.h"
#include "vector/vector_helper.h"
#include "util/cache_map.h"
#include "expression/expressions.h"
#include <vector>

namespace omniruntime::codegen {
using namespace omniruntime::expressions;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace omniruntime::exception;

class ExprSet;

using FilterFunc = int32_t (*)(int64_t *, int32_t, int32_t *, int64_t *, int64_t *, int64_t, int64_t *);
using ProjFunc = int32_t (*)(int64_t const *, int32_t, int64_t, int32_t *, int32_t, int64_t const *, int64_t const *,
    int32_t *, int32_t *, int64_t, int64_t *);

typedef struct LiteralValue {
    bool isNull = false;
    union Value {
        bool boolVal;
        int8_t byteVal;
        int16_t shortVal;
        int32_t intVal;
        int64_t longVal;
        double doubleVal;
        float floatVal;
        Decimal128 decimal128Val;
        std::string_view stringVal;

        Value() {}
    } value;
    LiteralValue() {}
} LiteralValue;

void GetAddr(VectorBatch &vecBatch, intptr_t valueAddrs[], intptr_t nullAddrs[], intptr_t offsetAddrs[],
    intptr_t dictionaries[], const DataTypes &types);

class Filter {
public:
    explicit Filter(const Expr &expression, const DataTypes &inputDataTypes, OverflowConfig *overflowConfig);

    ~Filter() = default;

    FilterFunc GetFilterFunc()
    {
        return apply;
    }

    bool IsSupported()
    {
        return isSupported;
    }

    const Expr *GetExpr() const
    {
        return expr;
    }

private:
    const Expr *expr;
    static CacheMap<std::string, intptr_t> filterFuncCache;
    static CacheMap<std::string, std::shared_ptr<FilterCodeGen>> rtCache;
    std::shared_ptr<FilterCodeGen> codeGen;
#ifndef EXCLUDE_BATCH_FUNCTIONS
    std::unique_ptr<BatchFilterCodeGen> batchCodeGen;
#endif
    bool isSupported;
    FilterFunc apply;
};

class Projection {
public:
    Projection(const Expr &expr, bool filter, DataTypePtr outType, const DataTypes &inputDataTypes,
        OverflowConfig *overflowConfig);

    ~Projection() = default;

    void ProjectHelperFixedWidth(VectorBatch &vecBatch, int64_t *valueAddrs, int64_t *nullAddrs, int64_t *offsetAddrs,
        BaseVector *outVec, int32_t numSelectedRows, int32_t selectedRows[], ExecutionContext *context,
        int64_t *dictionaryVectors, DataTypeId &typeIds) const;

    void ProjectHelperVarWidth(VectorBatch &vecBatch, int64_t *valueAddrs, int64_t *nullAddrs, int64_t *offsetAddrs,
        BaseVector *outVec, int32_t numSelectedRows, int32_t selectedRows[], ExecutionContext *context,
        int64_t *dictionaryVectors, DataTypeId &typeIds) const;

    BaseVector *Project(VectorBatch *vecBatch, int32_t selectedRows[], int32_t numSelectedRows, int64_t *valueAddrs,
        int64_t *nullAddrs, int64_t *offsetAddrs, ExecutionContext *context, int64_t *dictionaryVectors,
        const int32_t *typeIds);

    BaseVector *Project(VectorBatch *vecBatch, int64_t *valueAddrs, int64_t *nullAddrs, int64_t *offsetAddrs,
        ExecutionContext *context, int64_t *dictionaryVectors, const int32_t *typeIds);

    BaseVector *ProjectVec(VectorBatch *vecBatch, ExecutionContext *context);

    omniruntime::type::DataType &GetOutputType() const
    {
        return *(this->outType);
    }

    ProjFunc GetProjector() const
    {
        return projector;
    }

    bool IsSupported()
    {
        return isSupported;
    }

    bool IsColumnProjection() const
    {
        return isColumnProjection;
    }

    int GetColumnProjectionIndex() const
    {
        return columnProjectionIndex;
    }

    const Expr *GetExpr() const
    {
        return expr;
    }

private:
    static CacheMap<std::string, intptr_t> projFuncCache;
    static CacheMap<std::string, std::shared_ptr<ProjectionCodeGen>> rtCache;
    const omniruntime::expressions::Expr *expr;
    std::shared_ptr<ProjectionCodeGen> codeGen { nullptr };
#ifndef EXCLUDE_BATCH_FUNCTIONS
    std::unique_ptr<BatchProjectionCodeGen> batchCodeGen { nullptr };
#endif
    bool isSupported = true;
    bool isColumnProjection = false;
    int columnProjectionIndex = -1;
    bool isConstantProjection = false;
    bool isConstantArrayProjection = false;
    std::vector<double> constantArrayValues;
    LiteralValue literalVal;
    DataTypePtr outType;
    ProjFunc projector;

    bool Initialize(bool filter, const DataTypes &inputDataTypes, OverflowConfig *overflowConfig);
    BaseVector *ColumnProjectionProxy(VectorBatch *vecBatch, int32_t selectedRows[], int32_t numSelectedRows,
        const int32_t *typeIds) const;

    template <typename T>
    BaseVector *ColumnProjectionHelper(VectorBatch *vecBatch, const int32_t selectedRows[],
        int32_t numSelectedRows) const;

    template <typename T>
    BaseVector *ColumnProjectionVarCharVectorHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
        int32_t numSelectedRows) const;

    template <typename T>
    BaseVector *ColumnProjectionFlatVectorSliceHelper(int32_t numSelectedRows, BaseVector *colVec) const;

    template <typename T>
    BaseVector *ColumnProjectionDictionaryVectorSliceHelper(int32_t numSelectedRows, BaseVector *colVec) const;

    template <typename T>
    BaseVector *ColumnProjectionDictionaryVectorCopyPositionsHelper(const int32_t *selectedRows,
        int32_t numSelectedRows, BaseVector *colVec) const;

    template <typename T>
    BaseVector *ColumnProjectionFlatVectorCopyPositionsHelper(const int32_t *selectedRows, int32_t numSelectedRows,
        BaseVector *colVec) const;

    BaseVector *ColumnProjectionStructVectorSliceHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
        int32_t numSelectedRows) const;

    BaseVector *ColumnProjectionMapVectorSliceHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
        int32_t numSelectedRows) const;

    BaseVector *ColumnProjectionArrayVectorSliceHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
        int32_t numSelectedRows) const;

    bool SetLiteralValue(const LiteralExpr *literalExpr);

    bool NullColumnProjection(ExecutionContext *context, BaseVector *outVec);

    bool ConstantColumnProjection(ExecutionContext *context, BaseVector *outVec);

    BaseVector *ConstantArrayProjection(int32_t numSelectedRows) const;

    template <typename T> void SetConstantValues(T &value, BaseVector *outVec);
};

class ExpressionEvaluator {
public:
    ExpressionEvaluator(Expr *filterExpression, const std::vector<Expr *> &projectionExprs,
        const DataTypes &inputDataTypes, const config::QueryConfig &queryConfig);

    ExpressionEvaluator(const std::vector<Expr *> &projectionExprs, const DataTypes &inputDataTypes,
        const config::QueryConfig &queryConfig);

    ExpressionEvaluator(Expr *filterExpression, const std::vector<Expr *> &projectionExprs,
        const DataTypes &inputDataTypes, OverflowConfig *ofConfig, bool preferVectorization = false);

    ExpressionEvaluator(const std::vector<Expr *> &projectionExprs, const DataTypes &inputDataTypes,
        OverflowConfig *ofConfig, bool preferVectorization = false);

    ~ExpressionEvaluator()
    {
        if (filterExpr) {
            delete filterExpr;
        }
        for (size_t i = 0; i < projExprs.size(); ++i) {
            delete projExprs[i];
        }
        projExprs.clear();
    }

    DataTypes &GetInputDataTypes()
    {
        return inputTypes;
    }

    std::vector<type::DataTypePtr> &GetOutputDataTypes()
    {
        return outputTypes;
    }

    Expr *GetFilterExpression()
    {
        return filterExpr;
    }

    FilterFunc GetFilterFunc()
    {
        return filter->GetFilterFunc();
    }

    std::vector<std::unique_ptr<Projection>> &GetProjections()
    {
        return projections;
    }

    int32_t GetProjectVecCount()
    {
        return projectVecCount;
    }

    VectorBatch *Evaluate(VectorBatch *vecBatch, ExecutionContext *context,
        AlignedBuffer<int32_t> *selectedRowsBuffer = nullptr);

    bool IsSupportedExpr() const;

    void FilterFuncGeneration();

    void ProjectFuncGeneration();

    void SetSupportCodegen(const bool isSupport)
    {
        isSupportCodegen = isSupport;
    }

    bool IsSupportCodegen() const
    {
        return isSupportCodegen;
    }

private:
    config::QueryConfig queryConfig_;
    bool isSupportCodegen = true;
    bool isSupportVectorization = true;
    bool preferVectorization = false;
    bool useCodegen = true;
    Expr *filterExpr = nullptr;
    std::vector<Expr *> projExprs;
    int32_t projectVecCount = 0;
    std::unique_ptr<OverflowConfig> overflowConfig;
    bool isSupportedExpr = true;
    bool hasFilter = false;
    DataTypes inputTypes;
    std::vector<type::DataTypePtr> outputTypes;
    std::vector<std::vector<FieldExpr *>> fieldExprMap;

    std::unique_ptr<Filter> filter;
    std::vector<std::unique_ptr<Projection>> projections;

    VectorBatch *ProcessFilterAndProject(VectorBatch *vecBatch, ExecutionContext *context,
        AlignedBuffer<int32_t> *selectedRowsBuffer, intptr_t *valueAddrs, intptr_t *nullAddrs, intptr_t *offsetAddrs,
        intptr_t *dictionaries);

    VectorBatch *ProcessFilterAndProject(VectorBatch *vecBatch, ExecutionContext *context);

    VectorBatch *ProcessProject(VectorBatch *vecBatch, ExecutionContext *context, intptr_t *valueAddrs,
        intptr_t *nullAddrs, intptr_t *offsetAddrs, intptr_t *dictionaries);
};

class ExprSet {
public:
    explicit ExprSet(
            const std::vector<TypedExprPtrNew>& source,
            bool enableConstantFolding = true);

    virtual ~ExprSet();

    void clear();

    const std::vector<std::shared_ptr<Expr>>& exprs() const
    {
        return exprs_;
    }

    const std::shared_ptr<Expr>& expr(int32_t index) const
    {
        return exprs_[index];
    }

protected:
    std::vector<std::shared_ptr<Expr>> exprs_;
};

}
#endif // OMNI_RUNTIME_EXPR_EVALUATOR_H
