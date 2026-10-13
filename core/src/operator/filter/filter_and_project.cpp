/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: FilterAndProject operator source file
 */
#include "filter_and_project.h"
#include <algorithm>
#include <iostream>
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

namespace {
bool IsStringViewComparisonCandidate(const BinaryExpr *binaryExpr)
{
    if (binaryExpr->op != expressions::Operator::EQ &&
        binaryExpr->op != expressions::Operator::NEQ) {
        return false;
    }
    return binaryExpr->left->GetReturnTypeId() == OMNI_STRING_VIEW ||
        binaryExpr->right->GetReturnTypeId() == OMNI_STRING_VIEW;
}

void CollectStringViewComparisonFields(const Expr *expr, const DataTypes &sourceTypes,
    std::vector<int32_t> &fieldIndexes, bool &foundComparison)
{
    if (expr == nullptr) {
        return;
    }
    switch (expr->GetType()) {
        case ExprType::BINARY_E: {
            const auto *binaryExpr = static_cast<const BinaryExpr *>(expr);
            if (IsStringViewComparisonCandidate(binaryExpr)) {
                const Expr *fieldSide = nullptr;
                const Expr *literalSide = nullptr;
                if (binaryExpr->left->GetType() == ExprType::FIELD_E &&
                    binaryExpr->right->GetType() == ExprType::LITERAL_E) {
                    fieldSide = binaryExpr->left;
                    literalSide = binaryExpr->right;
                } else if (binaryExpr->right->GetType() == ExprType::FIELD_E &&
                    binaryExpr->left->GetType() == ExprType::LITERAL_E) {
                    fieldSide = binaryExpr->right;
                    literalSide = binaryExpr->left;
                } else {
                    OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                        "StringView EQ/NEQ must compare a field with a literal");
                }

                const auto *fieldExpr = static_cast<const FieldExpr *>(fieldSide);
                if (fieldSide->GetReturnTypeId() != OMNI_STRING_VIEW) {
                    OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                        "StringView field type mismatch: expected {}, actual {}",
                        static_cast<int32_t>(OMNI_STRING_VIEW),
                        static_cast<int32_t>(fieldSide->GetReturnTypeId()));
                }
                if (literalSide->GetReturnTypeId() != OMNI_STRING_VIEW) {
                    OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                        "StringView literal type mismatch: expected {}, actual {}",
                        static_cast<int32_t>(OMNI_STRING_VIEW),
                        static_cast<int32_t>(literalSide->GetReturnTypeId()));
                }
                if (fieldExpr->colVal < 0 || fieldExpr->colVal >= sourceTypes.GetSize()) {
                    OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                        "StringView field index {} is outside source type range {}",
                        fieldExpr->colVal, sourceTypes.GetSize());
                }
                if (sourceTypes.GetIds()[fieldExpr->colVal] != OMNI_STRING_VIEW) {
                    OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                        "StringView source type mismatch at field {}: expected {}, actual {}",
                        fieldExpr->colVal, static_cast<int32_t>(OMNI_STRING_VIEW),
                        static_cast<int32_t>(sourceTypes.GetIds()[fieldExpr->colVal]));
                }
                fieldIndexes.push_back(fieldExpr->colVal);
                foundComparison = true;
            }
            CollectStringViewComparisonFields(binaryExpr->left, sourceTypes, fieldIndexes, foundComparison);
            CollectStringViewComparisonFields(binaryExpr->right, sourceTypes, fieldIndexes, foundComparison);
            return;
        }
        case ExprType::UNARY_E:
            CollectStringViewComparisonFields(
                static_cast<const UnaryExpr *>(expr)->exp, sourceTypes, fieldIndexes, foundComparison);
            return;
        case ExprType::IS_NULL_E:
            CollectStringViewComparisonFields(
                static_cast<const IsNullExpr *>(expr)->value, sourceTypes, fieldIndexes, foundComparison);
            return;
        default:
            return;
    }
}
} // namespace

StringViewFilterValidationInfo ValidateStringViewFilterForRuntime(
    const Expr *filterExpr, const DataTypes &sourceTypes)
{
    if (filterExpr == nullptr) {
        OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION", "Filter expression is null");
    }
    if (!filterExpr->supportVectorized()) {
        OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
            "StringView filter expression does not support ExprEval vectorization");
    }
    StringViewFilterValidationInfo info;
    bool foundComparison = false;
    CollectStringViewComparisonFields(filterExpr, sourceTypes, info.fieldIndexes, foundComparison);
    if (!foundComparison) {
        OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
            "No OMNI_STRING_VIEW EQ/NEQ field-literal predicate found");
    }
    std::sort(info.fieldIndexes.begin(), info.fieldIndexes.end());
    info.fieldIndexes.erase(
        std::unique(info.fieldIndexes.begin(), info.fieldIndexes.end()), info.fieldIndexes.end());
    return info;
}

void ValidateStringViewInputBatch(VectorBatch *vecBatch, const std::vector<int32_t> &fieldIndexes)
{
    if (vecBatch == nullptr) {
        OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION", "Filter input batch is null");
    }
    for (const auto fieldIndex : fieldIndexes) {
        if (fieldIndex < 0 || fieldIndex >= vecBatch->GetVectorCount()) {
            OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                "StringView input field index {} is outside vector range {}",
                fieldIndex, vecBatch->GetVectorCount());
        }
        const auto *vector = vecBatch->Get(fieldIndex);
        if (vector == nullptr || vector->GetTypeId() != OMNI_STRING_VIEW) {
            OMNI_THROW("STRING_VIEW_RUNTIME_VALIDATION",
                "StringView filter input mismatch at field {}: expected {}, actual {}",
                fieldIndex, static_cast<int32_t>(OMNI_STRING_VIEW),
                vector == nullptr ? -1 : static_cast<int32_t>(vector->GetTypeId()));
        }
    }
}

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

Operator *FilterAndProjectOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    return new FilterAndProjectOperator(pool, this->exprEvaluator, this->stringViewValidationFields);
}

OperatorFactory *CreateFilterOperatorFactory(
    const std::shared_ptr<const FilterNode> filterNode, const config::QueryConfig &queryConfig)
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
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(filterExpr, projections, sourceTypes, queryConfig);
    std::vector<int32_t> stringViewValidationFields;
    if (queryConfig.StringViewRuntimeValidationEnabled()) {
        stringViewValidationFields = ValidateStringViewFilterForRuntime(filterExpr, sourceTypes).fieldIndexes;
    }
    return new FilterAndProjectOperatorFactory(move(exprEvaluator), move(stringViewValidationFields));
}

int32_t FilterAndProjectOperator::AddInput(VectorBatch *vecBatch)
{
    if (!stringViewValidationFields.empty()) {
        ValidateStringViewInputBatch(vecBatch, stringViewValidationFields);
        if (!stringViewValidationLogged) {
            std::cout << "SV_E2E_FILTER fieldType=OMNI_STRING_VIEW "
                      << "literalType=OMNI_STRING_VIEW inputType=OMNI_STRING_VIEW "
                      << "vectorized=true useCodegen=false" << std::endl;
            stringViewValidationLogged = true;
        }
    }
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
bool FilterAndProjectOperator::ProcessRow(
    int64_t valueAddrs[], const int32_t inputLens[], int64_t outValueAddrs[], int32_t outLens[])
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
    int32_t numSelectedRows = exprEvaluator->GetFilterFunc()(valueAddrs, rowCount, selectedRows, nullsAddrs,
        offsetsAddrs, reinterpret_cast<int64_t>(executionContext.get()), dictsAddrs);

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
