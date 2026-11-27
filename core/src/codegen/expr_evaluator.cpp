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

/*
 * Description: Expression evaluator
 */

#include "expr_evaluator.h"
#include "codegen/functions/stringfunctions.h"

namespace omniruntime::codegen {
int64_t GetRawAddr(const DataTypes &types, int32_t i, BaseVector *colVec)
{
    switch (types.GetIds()[i]) {
        case OMNI_INT:
        case OMNI_DATE32:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int32_t> *>(colVec)));
        case OMNI_SHORT:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int16_t> *>(colVec)));
        case OMNI_BYTE:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int8_t> *>(colVec)));
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int64_t> *>(colVec)));
        case OMNI_DOUBLE:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<double> *>(colVec)));
        case OMNI_BOOLEAN:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<bool> *>(colVec)));
        case OMNI_DECIMAL128:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<Decimal128> *>(colVec)));
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            return reinterpret_cast<int64_t>(unsafe::UnsafeStringVector::GetValues(
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVec)));
        case OMNI_TIMESTAMP_WITHOUT_TIME_ZONE:
        case OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int64_t> *>(colVec)));
        case OMNI_ARRAY:
            // ARRAY has no single raw value buffer; layout is offsets + element vector. Return 0 so callers
            // that need the column use the vector (e.g. column projection) or offsetAddrs.
        default: LogError("Do not support such vector type %d", types.GetIds()[i]); return 0;
    }
}

void GetAddr(VectorBatch &vecBatch, intptr_t valueAddrs[], intptr_t nullAddrs[], intptr_t offsetAddrs[],
             intptr_t dictionaries[], const DataTypes &types)
{
    intptr_t valuesAddress;
    intptr_t dictVecAddress;
    int32_t vectorCount = vecBatch.GetVectorCount();
    for (int32_t i = 0; i < vectorCount; i++) {
        auto colVec = vecBatch.Get(i);
        dictVecAddress = 0;
        valuesAddress = 0;
        if (colVec->GetEncoding() == OMNI_DICTIONARY) {
            dictVecAddress = reinterpret_cast<intptr_t>(reinterpret_cast<void *>(colVec));
        } else {
            valuesAddress = GetRawAddr(types, i, colVec);
        }

        // data handling
        dictionaries[i] = dictVecAddress;
        valueAddrs[i] = valuesAddress;

        // nulls handling
        nullAddrs[i] = reinterpret_cast<intptr_t>(unsafe::UnsafeBaseVector::GetNulls(colVec));

        // offsets handling
        offsetAddrs[i] = reinterpret_cast<intptr_t>(VectorHelper::UnsafeGetOffsetsAddr(colVec));
    }
}

Filter::Filter(const Expr &expression, const DataTypes &inputDataTypes, OverflowConfig *overflowConfig)
{
#ifdef DEBUG
    std::cout << "String expression in Filter: " << std::endl;
    ExprPrinter printExprTree;
    expression.Accept(printExprTree);
    std::cout << std::endl;
#endif
    intptr_t f;
    if (!ConfigUtil::IsEnableBatchExprEvaluate()) {
        this->codeGen = std::make_unique<FilterCodeGen>("filterFunc", expression, overflowConfig);
        f = this->codeGen->GetFunction(inputDataTypes);
    } else {
        this->batchCodeGen = std::make_unique<BatchFilterCodeGen>("filterFunc", expression, overflowConfig);
        f = this->batchCodeGen->GetFunction();
    }
    if (f == 0) {
        this->isSupported = false;
        this->apply = nullptr;
    } else {
        this->isSupported = true;
        void *function = &f;
        this->apply = *static_cast<FilterFunc *>(function);
    }
}

bool Projection::SetLiteralValue(const LiteralExpr *literalExpr)
{
    if (literalExpr->isNull) {
        literalVal.isNull = true;
        return true;
    }
    switch (outType->GetId()) {
        case OMNI_INT:
        case OMNI_DATE32: {
            literalVal.value.intVal = literalExpr->intVal;
            break;
        }
        case OMNI_SHORT: {
            literalVal.value.shortVal = literalExpr->shortVal;
            break;
        }
        case OMNI_BYTE: {
            literalVal.value.byteVal = literalExpr->byteVal;
            break;
        }
        case OMNI_LONG:
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP: {
            literalVal.value.longVal = literalExpr->longVal;
            break;
        }
        case OMNI_DOUBLE: {
            literalVal.value.doubleVal = literalExpr->doubleVal;
            break;
        }
        case OMNI_BOOLEAN: {
            literalVal.value.boolVal = literalExpr->boolVal;
            break;
        }
        case OMNI_DECIMAL128: {
            std::string dec128String = *literalExpr->stringVal;
            __uint128_t dec128 = Decimal128Utils::StrToUint128_t(dec128String.c_str());
            literalVal.value.decimal128Val.SetValue(static_cast<int128_t>(dec128));
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR: {
            literalVal.value.stringVal = std::string_view(*(literalExpr->stringVal));
            break;
        }
        default: LogError("Do not support such vector type %d", outType->GetId()); return false;
    }
    return true;
}

bool Projection::Initialize(bool filter, const DataTypes &inputDataTypes, OverflowConfig *overflowConfig)
{
    // short-circuit logic for column projections
    // no need to go through codegen
    if (expr->GetType() == ExprType::FIELD_E) {
        this->isColumnProjection = true;
        this->columnProjectionIndex = static_cast<const FieldExpr *>(expr)->colVal;
        return true;
    }

    // short-circuit logic for literal expression
    if (expr->GetType() == ExprType::LITERAL_E) {
        this->isConstantProjection = true;
        return SetLiteralValue(static_cast<const LiteralExpr *>(expr));
    }

    intptr_t f;
    if (!ConfigUtil::IsEnableBatchExprEvaluate()) {
        this->codeGen = std::make_unique<ProjectionCodeGen>("proj_func", *(this->expr), filter, overflowConfig);
        f = this->codeGen->GetFunction(inputDataTypes);
    } else {
        this->batchCodeGen =
            std::make_unique<BatchProjectionCodeGen>("proj_func", *(this->expr), filter, overflowConfig);
        f = this->batchCodeGen->GetFunction();
    }

    if (f == 0) {
        return false;
    }

    void *function = &f;
    auto cfunction = static_cast<ProjFunc *>(function);
    this->projector = *cfunction;
    return true;
}

Projection::Projection(const Expr &expr, bool filter, DataTypePtr outType, const DataTypes &inputDataTypes,
                       OverflowConfig *overflowConfig)
    : expr(&expr), outType(std::move(outType)), projector(nullptr)
{
#ifdef DEBUG
    std::cout << "Expression in projection:" << std::endl;
    ExprPrinter printExprTree;
    expr.Accept(printExprTree);
    std::cout << std::endl;
#endif
    bool initialized = this->Initialize(filter, inputDataTypes, overflowConfig);
    if (!initialized) {
        this->isSupported = false;
    }
}

/* for supporting cast(null as string) or NULL as col_name */
bool Projection::NullColumnProjection(ExecutionContext *context, BaseVector *outVec)
{
    auto outNulls = unsafe::UnsafeBaseVector::GetNulls(outVec);
    auto outNullsSize = BitUtil::Nbytes(outVec->GetSize());
    auto result = memset_s(outNulls, outNullsSize, -1, outNullsSize);
    if (result != EOK) {
        std::string errorMessage = "Memset failed, ret " + std::to_string(result) + " destMax " +
                                   std::to_string(outNullsSize) + " count " + std::to_string(outNullsSize);
        context->SetError(errorMessage);
        return false;
    }
    return true;
}

template <typename T> void Projection::SetConstantValues(T &value, BaseVector *outVec)
{
    auto rowCount = outVec->GetSize();
    if constexpr (std::is_same_v<T, std::string_view>) {
        auto outputVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(outVec);
        for (int32_t i = 0; i < rowCount; i++) {
            outputVector->SetValue(i, value);
        }
    } else {
        auto outputVector = static_cast<Vector<T> *>(outVec);
        for (int32_t i = 0; i < rowCount; i++) {
            outputVector->SetValue(i, value);
        }
    }
}

bool Projection::ConstantColumnProjection(ExecutionContext *context, BaseVector *outVec)
{
    if (literalVal.isNull) {
        return NullColumnProjection(context, outVec);
    }
    auto outputTypeId = this->outType->GetId();
    switch (outputTypeId) {
        case OMNI_INT:
        case OMNI_DATE32: SetConstantValues<int32_t>(literalVal.value.intVal, outVec); break;
        case OMNI_SHORT: SetConstantValues<int16_t>(literalVal.value.shortVal, outVec); break;
        case OMNI_BYTE: SetConstantValues<int8_t>(literalVal.value.byteVal, outVec); break;
        case OMNI_LONG:
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP: SetConstantValues<int64_t>(literalVal.value.longVal, outVec); break;
        case OMNI_DOUBLE: SetConstantValues<double>(literalVal.value.doubleVal, outVec); break;
        case OMNI_BOOLEAN: SetConstantValues<bool>(literalVal.value.boolVal, outVec); break;
        case OMNI_DECIMAL128: SetConstantValues<Decimal128>(literalVal.value.decimal128Val, outVec); break;
        case OMNI_VARCHAR:
        case OMNI_CHAR: SetConstantValues<std::string_view>(literalVal.value.stringVal, outVec); break;
        default:
            std::string errorMessage = "Do not support such vector type " + std::to_string(outputTypeId);
            context->SetError(errorMessage);
            return false;
    }
    return true;
}

BaseVector *Projection::Project(VectorBatch *vecBatch, int32_t selectedRows[], int32_t numSelectedRows,
                                int64_t *valueAddrs, int64_t *nullAddrs, int64_t *offsetAddrs,
                                ExecutionContext *context, int64_t *dictionaryVectors, const int32_t *typeIds)
{
    if (this->isColumnProjection) {
        return ColumnProjectionProxy(vecBatch, selectedRows, numSelectedRows, typeIds);
    } else if (this->isConstantProjection) {
        BaseVector *outVec = VectorHelper::CreateFlatVector(outType->GetId(), numSelectedRows);
        if (!ConstantColumnProjection(context, outVec)) {
            delete outVec;
            context->GetArena()->Reset();
            return nullptr;
        } else {
            context->GetArena()->Reset();
            return outVec;
        }
    } else {
        DataTypeId outTypeId = outType->GetId();
        BaseVector *outVec = VectorHelper::CreateFlatVector(outTypeId, numSelectedRows);
        if (outTypeId == OMNI_VARCHAR || outTypeId == OMNI_CHAR) {
            ProjectHelperVarWidth(*vecBatch, valueAddrs, nullAddrs, offsetAddrs, outVec, numSelectedRows, selectedRows,
                                  context, dictionaryVectors, outTypeId);
        } else {
            ProjectHelperFixedWidth(*vecBatch, valueAddrs, nullAddrs, offsetAddrs, outVec, numSelectedRows,
                                    selectedRows, context, dictionaryVectors, outTypeId);
        }
        context->GetArena()->Reset();
        return outVec;
    }
}

template <typename T>
BaseVector *Projection::ColumnProjectionFlatVectorCopyPositionsHelper(const int32_t *selectedRows,
                                                                      int32_t numSelectedRows, BaseVector *colVec) const
{
    return reinterpret_cast<Vector<T> *>(colVec)->CopyPositions(selectedRows, 0, numSelectedRows);
}

template <typename T>
BaseVector *Projection::ColumnProjectionDictionaryVectorCopyPositionsHelper(const int32_t *selectedRows,
                                                                            int32_t numSelectedRows,
                                                                            BaseVector *colVec) const
{
    return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->CopyPositions(selectedRows, 0, numSelectedRows);
}

template <typename T>
BaseVector *Projection::ColumnProjectionFlatVectorSliceHelper(int32_t numSelectedRows, BaseVector *colVec) const
{
    return reinterpret_cast<Vector<T> *>(colVec)->Slice(0, numSelectedRows);
}

template <typename T>
BaseVector *Projection::ColumnProjectionDictionaryVectorSliceHelper(int32_t numSelectedRows, BaseVector *colVec) const
{
    return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->Slice(0, numSelectedRows);
}

void Projection::ProjectHelperVarWidth(VectorBatch &vecBatch, int64_t *valueAddrs, int64_t *nullAddrs,
                                       int64_t *offsetAddrs, BaseVector *outVec, int32_t numSelectedRows,
                                       int32_t selectedRows[], ExecutionContext *context, int64_t *dictionaryVectors,
                                       DataTypeId &outTypeId) const
{
    this->projector(valueAddrs, vecBatch.GetRowCount(), reinterpret_cast<int64_t>(outVec), selectedRows,
                    numSelectedRows, nullAddrs, offsetAddrs,
                    reinterpret_cast<int32_t *>(unsafe::UnsafeBaseVector::GetNulls(outVec)), nullptr,
                    reinterpret_cast<int64_t>(context), dictionaryVectors);
}

void Projection::ProjectHelperFixedWidth(VectorBatch &vecBatch, int64_t *valueAddrs, int64_t *nullAddrs,
                                         int64_t *offsetAddrs, BaseVector *outVec, int32_t numSelectedRows,
                                         int32_t selectedRows[], ExecutionContext *context, int64_t *dictionaryVectors,
                                         DataTypeId &outTypeId) const
{
    auto outValueAddr = reinterpret_cast<int64_t>(VectorHelper::UnsafeGetValues(outVec));
    this->projector(valueAddrs, vecBatch.GetRowCount(), outValueAddr, selectedRows, numSelectedRows, nullAddrs,
                    offsetAddrs, reinterpret_cast<int32_t *>(unsafe::UnsafeBaseVector::GetNulls(outVec)), nullptr,
                    reinterpret_cast<int64_t>(context), dictionaryVectors);
}

BaseVector *Projection::Project(VectorBatch *vecBatch, int64_t *valueAddrs, int64_t *nullAddrs, int64_t *offsetAddrs,
                                ExecutionContext *context, int64_t *dictionaryVectors, const int32_t *typeIds)
{
    return this->Project(vecBatch, nullptr, vecBatch->GetRowCount(), valueAddrs, nullAddrs, offsetAddrs, context,
                         dictionaryVectors, typeIds);
}

BaseVector *Projection::ColumnProjectionProxy(VectorBatch *vecBatch, int32_t selectedRows[], int32_t numSelectedRows,
                                              const int32_t *typeIds) const
{
    switch (typeIds[columnProjectionIndex]) {
        case OMNI_INT:
        case OMNI_DATE32: return ColumnProjectionHelper<int32_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_SHORT: return ColumnProjectionHelper<int16_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_BYTE: return ColumnProjectionHelper<int8_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: return ColumnProjectionHelper<int64_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_DOUBLE: return ColumnProjectionHelper<double>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_BOOLEAN: return ColumnProjectionHelper<bool>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_DECIMAL128: return ColumnProjectionHelper<Decimal128>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            return ColumnProjectionVarCharVectorHelper<std::string_view>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_TIMESTAMP_WITHOUT_TIME_ZONE:
        case OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE:
            return ColumnProjectionHelper<int64_t>(vecBatch, selectedRows, numSelectedRows);
        default: LogError("Do not support such vector type %d", typeIds[columnProjectionIndex]); return nullptr;
    }
}

template <typename T>
BaseVector *Projection::ColumnProjectionHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
                                               int32_t numSelectedRows) const
{
    auto colVec = vecBatch->Get(this->columnProjectionIndex);
    auto rowCnt = vecBatch->GetRowCount();
    if (numSelectedRows != 0 && numSelectedRows == rowCnt) {
        if (colVec->GetEncoding() == OMNI_DICTIONARY) {
            return ColumnProjectionDictionaryVectorSliceHelper<T>(numSelectedRows, colVec);
        } else {
            return ColumnProjectionFlatVectorSliceHelper<T>(numSelectedRows, colVec);
        }
    }

    if (selectedRows != nullptr && numSelectedRows != 0) {
        if (colVec->GetEncoding() == OMNI_DICTIONARY) {
            return ColumnProjectionDictionaryVectorCopyPositionsHelper<T>(selectedRows, numSelectedRows, colVec);
        } else {
            return ColumnProjectionFlatVectorCopyPositionsHelper<T>(selectedRows, numSelectedRows, colVec);
        }
    }
    return nullptr;
}

template <typename T>
BaseVector *Projection::ColumnProjectionVarCharVectorHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
                                                            int32_t numSelectedRows) const
{
    auto colVec = vecBatch->Get(this->columnProjectionIndex);
    auto rowCnt = vecBatch->GetRowCount();
    if (numSelectedRows != 0 && numSelectedRows == rowCnt) {
        if (colVec->GetEncoding() == OMNI_DICTIONARY) {
            return ColumnProjectionDictionaryVectorSliceHelper<T>(numSelectedRows, colVec);
        } else {
            return ColumnProjectionFlatVectorSliceHelper<LargeStringContainer<std::string_view>>(numSelectedRows,
                                                                                                 colVec);
        }
    }

    if (selectedRows != nullptr && numSelectedRows != 0) {
        if (colVec->GetEncoding() == OMNI_DICTIONARY) {
            return ColumnProjectionDictionaryVectorCopyPositionsHelper<T>(selectedRows, numSelectedRows, colVec);
        } else {
            // copyPosition for string vectors is 10 to 30 times slower than creating a dictionary
            // so here rather than calling copyPosition with create a dictionary view of original input vector
            return VectorHelper::CreateStringDictionary(
                selectedRows, numSelectedRows,
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVec));
        }
    }
    return nullptr;
}
ExpressionEvaluator::ExpressionEvaluator(Expr *filterExpression, const std::vector<Expr *> &projectionExprs,
                                         const DataTypes &inputDataTypes, OverflowConfig *ofConfig)
    : inputTypes(const_cast<DataTypes &>(inputDataTypes))
{
    hasFilter = true;
    filterExpr = filterExpression;
    for (auto &projectionExpr : projectionExprs) {
        projExprs.emplace_back(projectionExpr);
    }
    overflowConfig = std::make_unique<OverflowConfig>(*ofConfig);
    projectVecCount = static_cast<int32_t>(projectionExprs.size());

    for (int i = 0; isSupportedExpr && i < projectVecCount; ++i) {
        outputTypes.emplace_back(projExprs[i]->GetReturnType());
    }
}

ExpressionEvaluator::ExpressionEvaluator(const std::vector<Expr *> &projectionExprs, const DataTypes &inputDataTypes,
                                         OverflowConfig *ofConfig)
    : inputTypes(const_cast<DataTypes &>(inputDataTypes))
{
    hasFilter = false;
    for (auto &projectionExpr : projectionExprs) {
        projExprs.emplace_back(projectionExpr);
    }
    overflowConfig = std::make_unique<OverflowConfig>(*ofConfig);
    projectVecCount = static_cast<int32_t>(projectionExprs.size());

    for (int i = 0; isSupportedExpr && i < projectVecCount; ++i) {
        outputTypes.emplace_back(projExprs[i]->GetReturnType());
    }
}

bool ExpressionEvaluator::IsSupportedExpr() const { return isSupportedExpr; }

VectorBatch *ExpressionEvaluator::Evaluate(VectorBatch *vecBatch, ExecutionContext *context,
                                           AlignedBuffer<int32_t> *selectedRowsBuffer)
{
    const int vectorCount = vecBatch->GetVectorCount();
    intptr_t valueAddrs[vectorCount];
    intptr_t nullAddrs[vectorCount];
    intptr_t offsetAddrs[vectorCount];
    intptr_t dictionaries[vectorCount];
    GetAddr(*vecBatch, valueAddrs, nullAddrs, offsetAddrs, dictionaries, this->inputTypes);
    if (hasFilter) {
        return ProcessFilterAndProject(vecBatch, context, selectedRowsBuffer, valueAddrs, nullAddrs, offsetAddrs,
                                       dictionaries);
    } else {
        return ProcessProject(vecBatch, context, valueAddrs, nullAddrs, offsetAddrs, dictionaries);
    }
}

void ExpressionEvaluator::FilterFuncGeneration()
{
    filter = std::make_unique<Filter>(*filterExpr, GetInputDataTypes(), overflowConfig.get());
    if (!this->filter->IsSupported()) {
        this->isSupportedExpr = false;
    }
    for (auto &projExpr : projExprs) {
        auto projection = std::make_unique<Projection>(*projExpr, true, projExpr->GetReturnType(), GetInputDataTypes(),
                                                       overflowConfig.get());
        if (!projection->IsSupported()) {
            this->isSupportedExpr = false;
            break;
        }
        projections.emplace_back(move(projection));
    }
}

void ExpressionEvaluator::ProjectFuncGeneration()
{
    for (auto &projExpr : projExprs) {
        auto projection = std::make_unique<Projection>(*projExpr, false, projExpr->GetReturnType(), GetInputDataTypes(),
                                                       overflowConfig.get());
        if (!projection->IsSupported()) {
            this->isSupportedExpr = false;
            break;
        }
        projections.emplace_back(move(projection));
    }
}

VectorBatch *ExpressionEvaluator::ProcessProject(VectorBatch *vecBatch, ExecutionContext *context, intptr_t *valueAddrs,
                                                 intptr_t *nullAddrs, intptr_t *offsetAddrs, intptr_t *dictionaries)
{
    auto rowCount = vecBatch->GetRowCount();
    [[maybe_unused]] function::JsonRowDocumentCacheScope rowDocumentScope(reinterpret_cast<int64_t>(context));
    auto projectedVecs = std::make_unique<VectorBatch>(rowCount);
    for (int32_t i = 0; i < projectVecCount; i++) {
        BaseVector *outCol = projections[i]->Project(vecBatch, valueAddrs, nullAddrs, offsetAddrs, context,
                                                     dictionaries, GetInputDataTypes().GetIds());
        if (context->HasError()) {
            context->GetArena()->Reset();
            std::string errorMessage = context->GetError();
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
        projectedVecs->Append(outCol);
    }
    context->GetArena()->Reset();
    return projectedVecs.release();
}

VectorBatch *ExpressionEvaluator::ProcessFilterAndProject(VectorBatch *vecBatch, ExecutionContext *context,
                                                          AlignedBuffer<int32_t> *selectedRowsBuffer,
                                                          intptr_t *valueAddrs, intptr_t *nullAddrs,
                                                          intptr_t *offsetAddrs, intptr_t *dictionaries)
{
    const int rowCount = vecBatch->GetRowCount();
    [[maybe_unused]] function::JsonRowDocumentCacheScope rowDocumentScope(reinterpret_cast<int64_t>(context));
    auto selectedRows = selectedRowsBuffer->AllocateReuse(rowCount, false);
    int32_t numSelectedRows = filter->GetFilterFunc()(valueAddrs, rowCount, selectedRows, nullAddrs, offsetAddrs,
                                                      reinterpret_cast<int64_t>(context), dictionaries);
    if (context->HasError()) {
        context->GetArena()->Reset();
        std::string errorMessage = context->GetError();
        throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
    }
    if (numSelectedRows <= 0) {
        context->GetArena()->Reset();
        return nullptr;
    }

    auto projectedVecs = std::make_unique<VectorBatch>(numSelectedRows);
    for (int32_t i = 0; i < projectVecCount; i++) {
        BaseVector *col = projections[i]->Project(vecBatch, selectedRows, numSelectedRows, valueAddrs, nullAddrs,
                                                  offsetAddrs, context, dictionaries, GetInputDataTypes().GetIds());
        if (context->HasError()) {
            context->GetArena()->Reset();
            std::string errorMessage = context->GetError();
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
        projectedVecs->Append(col);
    }

    context->GetArena()->Reset();
    return projectedVecs.release();
}
} // namespace omniruntime::codegen
