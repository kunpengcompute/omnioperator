/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: FilterAndProject operator source file
 */
#include "filter_and_project.h"
#include "expression/jsonparser/jsonparser.h"
#include "operator/config/operator_config.h"
#include "util/config/QueryConfig.h"
#include "util/config_util.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace omniruntime::mem;
using namespace std;

SimpleFilter::SimpleFilter(const Expr &expression)
    : codegen(nullptr), expression(&expression), func(nullptr), initialized(false)
{
    resultLength = new int(0);
    isResultNull = new bool(false);
}

SimpleFilter::~SimpleFilter()
{
    delete this->isResultNull;
    delete this->resultLength;
    this->codegen.reset();
}

SimpleFilter::SimpleFilter(const SimpleFilter &simpleFilter)
{
    this->codegen = simpleFilter.codegen;
    this->expression = simpleFilter.expression;
    this->func = simpleFilter.func;
    this->initialized = simpleFilter.initialized;
    this->resultLength = new int(0);
    this->isResultNull = new bool(false);
    this->isColumnFilter = simpleFilter.isColumnFilter;
}

bool SimpleFilter::Initialize(OverflowConfig *overflowConfig)
{
    if (this->expression == nullptr) {
        LogWarn("Unable to parse expression for simple filter");
        return false;
    }

    if (this->expression->GetReturnTypeId() != OMNI_BOOLEAN) {
        LogWarn("Filter expression can only return boolean, current type: %d", this->expression->GetReturnTypeId());
        return false;
    }

    this->codegen = std::make_unique<SimpleFilterCodeGen>("simple_row_expr_eval", *this->expression, overflowConfig);
    if (this->codegen == nullptr) {
        LogWarn("Unable to generate function for simple filter");
        return false;
    }

    int64_t fAddr = this->codegen->GetFunction();
    if (fAddr == 0) {
        LogWarn("Unable to generate function for simple filter");
        return false;
    }

    void *refFunc = &fAddr;
    this->func = *static_cast<SimpleRowExprEvalFunc *>(refFunc);
    isColumnFilter = this->expression->GetType() == ExprType::FIELD_E && GetVectorIndexes().size() == 1;
    this->initialized = true;
    return true;
}

set<int32_t> &SimpleFilter::GetVectorIndexes() { return this->codegen->vectorIndexes; }

bool SimpleFilter::Evaluate(int64_t *values, bool *isNulls, int32_t *lengths, int64_t executionContext)
{
    auto result = this->func(values, isNulls, lengths, this->isResultNull, this->resultLength, executionContext);
    return !*this->isResultNull && result;
}

Operator *FilterAndProjectOperatorFactory::CreateOperator()
{
    return new FilterAndProjectOperator(this->exprEvaluator);
}

OperatorFactory *CreateFilterOperatorFactory(const std::shared_ptr<const FilterNode> filterNode,
                                             const config::QueryConfig &queryConfig)
{
    auto filterExpr = filterNode->GetFilterExpr();
    std::vector<Expr *> projections;
    const auto &sourceTypes = *(filterNode->Sources()[0]->OutputType());
    int32_t idx = 0;
    if (filterNode->ProjectList().empty()) {
        for (const auto &item : sourceTypes.Get()) {
            projections.push_back(new FieldExpr(idx++, item));
        }
    } else {
        projections = filterNode->ProjectList();
    }
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                                 : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, sourceTypes, overflowConfig);
    return new FilterAndProjectOperatorFactory(move(exprEvaluator));
}

int32_t FilterAndProjectOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() > 0) {
        projectedVecs = this->exprEvaluator->Evaluate(vecBatch, executionContext.get(), &selectedRowsBuffer);
    }
    UpdateAddInputInfo(vecBatch->GetRowCount());
    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    return 0;
}

int32_t FilterAndProjectOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (this->projectedVecs == nullptr) {
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
    int rowCount = this->projectedVecs->GetRowCount();
    *outputVecBatch = this->projectedVecs;
    this->projectedVecs = nullptr;
    UpdateGetOutputInfo(rowCount);
    return rowCount;
}

OmniStatus FilterAndProjectOperator::Close()
{
    if (projectedVecs != nullptr) {
        VectorHelper::FreeVecBatch(projectedVecs);
        projectedVecs = nullptr;
    }
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}

/**
 * Process one row for fusion operator
 * @param valueAddrs contains value address of each column.
 * @param inputLens contains null or length of each column. inputLens[i] == -1 means i-th value is null; inputLens[i] >=
 * 0 represents the i-th values's length.
 * @param outValueAddrs contains output value address of each projection.
 * @param outLens contains null or length of each projection.
 * @return true(filter pass) or false(filter fail).
 */
bool FilterAndProjectOperator::ProcessRow(int64_t valueAddrs[], const int32_t inputLens[], int64_t outValueAddrs[],
                                          int32_t outLens[])
{
    auto vecCount = exprEvaluator->GetInputDataTypes().GetSize();
    auto dictsAddrs = new int64_t[vecCount];
    auto offsetsAddrs = new int64_t[vecCount];
    auto nullsAddrs = new int64_t[vecCount];
    for (int i = 0; i < vecCount; ++i) {
        dictsAddrs[i] = 0; // Spark's TableScan will not produce dictionary.
        auto null = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        nullsAddrs[i] = reinterpret_cast<int64_t>(null);
        auto offset = new int32_t[2]; // offset[1] - offset[0] = length
        offsetsAddrs[i] = reinterpret_cast<int64_t>(offset);
    }

    // Construct nullsAddrs and offsetsAddrs from inputLens
    for (int i = 0; i < vecCount; ++i) {
        if (inputLens[i] == -1) {
            BitUtil::SetBit(reinterpret_cast<uint8_t *>(nullsAddrs[i]), 0, true);
            reinterpret_cast<int32_t *>(offsetsAddrs[i])[0] = 0;
            reinterpret_cast<int32_t *>(offsetsAddrs[i])[1] = 0;
        } else {
            BitUtil::SetBit(reinterpret_cast<uint8_t *>(nullsAddrs[i]), 0, false);
            reinterpret_cast<int32_t *>(offsetsAddrs[i])[0] = 0;
            reinterpret_cast<int32_t *>(offsetsAddrs[i])[1] = inputLens[i];
        }
    }

    const int rowCount = 1;
    int32_t selectedRows[rowCount];
    int32_t numSelectedRows =
        exprEvaluator->GetFilterFunc()(valueAddrs, rowCount, selectedRows, nullsAddrs, offsetsAddrs,
                                       reinterpret_cast<int64_t>(executionContext.get()), dictsAddrs);

    if (executionContext->HasError()) {
        executionContext->GetArena()->Reset();
        for (int i = 0; i < vecCount; ++i) {
            delete[] reinterpret_cast<uint8_t *>(nullsAddrs[i]);
            delete[] reinterpret_cast<int32_t *>(offsetsAddrs[i]);
        }
        delete[] dictsAddrs;
        delete[] nullsAddrs;
        delete[] offsetsAddrs;
        string errorMessage = executionContext->GetError();
        throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
    }

    if (numSelectedRows <= 0) {
        executionContext->GetArena()->Reset();
        for (int i = 0; i < vecCount; ++i) {
            delete[] reinterpret_cast<uint8_t *>(nullsAddrs[i]);
            delete[] reinterpret_cast<int32_t *>(offsetsAddrs[i]);
        }
        delete[] dictsAddrs;
        delete[] nullsAddrs;
        delete[] offsetsAddrs;
        return false;
    }

    for (int32_t i = 0; i < exprEvaluator->GetProjectVecCount(); i++) {
        auto &projections = exprEvaluator->GetProjections();
        if (projections[i]->IsColumnProjection()) {
            outValueAddrs[i] = valueAddrs[projections[i]->GetColumnProjectionIndex()];
            outLens[i] = inputLens[projections[i]->GetColumnProjectionIndex()];
        } else {
            executionContext->GetArena()->Reset();
            for (int j = 0; j < vecCount; ++j) {
                delete[] reinterpret_cast<uint8_t *>(nullsAddrs[j]);
                delete[] reinterpret_cast<int32_t *>(offsetsAddrs[j]);
            }
            delete[] dictsAddrs;
            delete[] nullsAddrs;
            delete[] offsetsAddrs;
            string errorMessage = "Fusion filter only supports raw column projection!";
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
    }

    executionContext->GetArena()->Reset();
    for (int i = 0; i < vecCount; ++i) {
        delete[] reinterpret_cast<uint8_t *>(nullsAddrs[i]);
        delete[] reinterpret_cast<int32_t *>(offsetsAddrs[i]);
    }
    delete[] dictsAddrs;
    delete[] nullsAddrs;
    delete[] offsetsAddrs;
    return true;
}
} // namespace op
} // namespace omniruntime
