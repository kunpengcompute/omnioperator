/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * Description: Expression evaluator
 */
#include <iostream>
#include <memory>
#include <sstream>
#include <mutex>
#include "expr_evaluator.h"
#include "vectorization/ExprEval.h"
#include "expression/expr_verifier.h"
#include "vector/array_vector.h"

namespace omniruntime::codegen {
std::mutex mtx;
CacheMap<std::string, intptr_t> Projection::projFuncCache(100);
CacheMap<std::string, std::shared_ptr<ProjectionCodeGen>> Projection::rtCache(100);
CacheMap<std::string, intptr_t> Filter::filterFuncCache(100);
CacheMap<std::string, std::shared_ptr<FilterCodeGen>> Filter::rtCache(100);

int64_t GetRawAddr(const DataTypes &types, int32_t i, BaseVector *colVec)
{
    switch (types.GetIds()[i]) {
        case OMNI_INT:
        case OMNI_DATE32:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<int32_t> *>(colVec)));
        case OMNI_SHORT:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<int16_t> *>(colVec)));
        case OMNI_BYTE:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<int8_t> *>(colVec)));
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<int64_t> *>(colVec)));
        case OMNI_DOUBLE:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<double> *>(colVec)));
        case OMNI_FLOAT:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<float> *>(colVec)));
        case OMNI_BOOLEAN:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<bool> *>(colVec)));
        case OMNI_DECIMAL128:
            return reinterpret_cast<int64_t>(unsafe::UnsafeVector::GetRawValues(
                reinterpret_cast<Vector<Decimal128> *>(colVec)));
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            return reinterpret_cast<int64_t>(unsafe::UnsafeStringVector::GetValues(
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVec)));
        case OMNI_TIMESTAMP_WITHOUT_TIME_ZONE:
            return reinterpret_cast<int64_t>(
                    unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int64_t> *>(colVec)));
        case OMNI_ARRAY:
        case OMNI_MAP:
        case OMNI_ROW:
            // Complex types have no single raw value buffer; layout is offsets + element vector(s). Return 0 so
            // callers that need the column use the vector (e.g. column projection) or offsetAddrs.
            return 0;
        default:
            LogError("Do not support such vector type %d", types.GetIds()[i]);
            return 0;
    }
}

void GetAddr(VectorBatch &vecBatch, intptr_t valueAddrs[], intptr_t nullAddrs[], intptr_t offsetAddrs[],
    intptr_t dictionaries[], const DataTypes &types)
{
    intptr_t valuesAddress;
    intptr_t dictVecAddress;
    int32_t vectorCount = vecBatch.GetVectorCount();
    int32_t size = std::min(vectorCount, types.GetSize());
    for (int32_t i = 0; i < size; i++) {
        auto colVec = vecBatch.Get(i);
        dictVecAddress = 0;
        valuesAddress = 0;
        if (colVec->GetEncoding() == OMNI_DICTIONARY || colVec->GetEncoding() == OMNI_ENCODING_CONST) {
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
std::lock_guard<std::mutex> lock(mtx);
#ifdef DEBUG
    std::cout << "String expression in Filter: " << std::endl;
    ExprPrinter printExprTree;
    expression.Accept(printExprTree);
    std::cout << std::endl;
#endif
    intptr_t f;
    if (!ConfigUtil::IsEnableBatchExprEvaluate()) {
        auto overflowKey = overflowConfig ? std::to_string(overflowConfig->IsOverflowAsNull()) : "0";
        auto key = expression.toString() + overflowKey;
        auto cacheRtValue = rtCache.Get(key);
        auto cacheValue = filterFuncCache.Get(key);

        if (cacheValue != std::nullopt) {
            f = cacheValue.value();
            this->codeGen = cacheRtValue.value();
        }  else {
            this->codeGen = std::make_shared<FilterCodeGen>("filterFunc", expression, overflowConfig);
            f = this->codeGen->GetFunction(inputDataTypes);
            filterFuncCache.Set(key, f);
            rtCache.Set(key, this->codeGen);
        }
    } else {
#ifndef EXCLUDE_BATCH_FUNCTIONS
        this->batchCodeGen = std::make_unique<BatchFilterCodeGen>("filterFunc", expression, overflowConfig);
        f = this->batchCodeGen->GetFunction();
#else
        auto overflowKey = overflowConfig ? std::to_string(overflowConfig->IsOverflowAsNull()) : "0";
        auto key = expression.toString() + overflowKey;
        auto cacheRtValue = rtCache.Get(key);
        auto cacheValue = filterFuncCache.Get(key);

        if (cacheValue != std::nullopt) {
            f = cacheValue.value();
            this->codeGen = cacheRtValue.value();
        }  else {
            this->codeGen = std::make_shared<FilterCodeGen>("filterFunc", expression, overflowConfig);
            f = this->codeGen->GetFunction(inputDataTypes);
            filterFuncCache.Set(key, f);
            rtCache.Set(key, this->codeGen);
        }
#endif
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
        case OMNI_FLOAT: {
            literalVal.value.floatVal = literalExpr->floatVal;
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
        case OMNI_VARBINARY:
        case OMNI_CHAR: {
            literalVal.value.stringVal = std::string_view(*(literalExpr->stringVal));
            break;
        }
        default:
            LogError("Do not support such vector type %d", outType->GetId());
            return false;
    }
    return true;
}

bool Projection::Initialize(bool filter, const DataTypes &inputDataTypes, OverflowConfig *overflowConfig)
{
    std::lock_guard<std::mutex> lock(mtx);
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

    // OMNI_ARRAY is not supported by codegen (ToLLVMType returns null). Only allow constant-array path.
    if (expr->GetReturnTypeId() == OMNI_ARRAY) {
        const auto *f = dynamic_cast<const FuncExpr *>(expr);
        if (f && f->funcName == "array" && !f->arguments.empty()) {
            constantArrayValues.clear();
            bool allNumericLiteral = true;
            for (const Expr *arg : f->arguments) {
                const auto *lit = dynamic_cast<const LiteralExpr *>(arg);
                if (!lit || lit->isNull) {
                    allNumericLiteral = false;
                    break;
                }
                if (lit->GetReturnTypeId() == OMNI_DOUBLE) {
                    constantArrayValues.push_back(lit->doubleVal);
                } else if (lit->GetReturnTypeId() == OMNI_FLOAT) {
                    constantArrayValues.push_back(static_cast<double>(lit->floatVal));
                } else {
                    allNumericLiteral = false;
                    break;
                }
            }
            if (allNumericLiteral && !constantArrayValues.empty()) {
                this->isConstantArrayProjection = true;
                return true;
            }
        }
        return false;
    }

    intptr_t f;
    if (!ConfigUtil::IsEnableBatchExprEvaluate()) {
        auto overflowKey = overflowConfig ? std::to_string(overflowConfig->IsOverflowAsNull()) : "0";
        auto key = expr->toString() + overflowKey;
        auto cacheRtValue = rtCache.Get(key);
        auto cacheValue = projFuncCache.Get(key);
        if (cacheValue != std::nullopt)  {
           f = cacheValue.value();
           this->codeGen = cacheRtValue.value();
        }  else {
            this->codeGen = std::make_shared<ProjectionCodeGen>("proj_func", *(this->expr), filter, overflowConfig);
            f = this->codeGen->GetFunction(inputDataTypes);
            projFuncCache.Set(key, f);
            rtCache.Set(key, this->codeGen);
        }
    } else {
#ifndef EXCLUDE_BATCH_FUNCTIONS
        this->batchCodeGen = std::make_unique<BatchProjectionCodeGen>("proj_func", *(this->expr), filter,
            overflowConfig);
        f = this->batchCodeGen->GetFunction();
#else
        auto overflowKey = overflowConfig ? std::to_string(overflowConfig->IsOverflowAsNull()) : "0";
        auto key = expr->toString() + overflowKey;
        auto cacheRtValue = rtCache.Get(key);
        auto cacheValue = projFuncCache.Get(key);
        if (cacheValue != std::nullopt)  {
           f = cacheValue.value();
           this->codeGen = cacheRtValue.value();
        }  else {
            this->codeGen = std::make_shared<ProjectionCodeGen>("proj_func", *(this->expr), filter, overflowConfig);
            f = this->codeGen->GetFunction(inputDataTypes);
            projFuncCache.Set(key, f);
            rtCache.Set(key, this->codeGen);
        }
#endif
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
    memset(outNulls, -1, outNullsSize);
    return true;
}

template <typename T>
void Projection::SetConstantValues(T &value, BaseVector *outVec)
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
        case OMNI_DATE32:
            SetConstantValues<int32_t>(literalVal.value.intVal, outVec);
            break;
        case OMNI_SHORT:
            SetConstantValues<int16_t>(literalVal.value.shortVal, outVec);
            break;
        case OMNI_BYTE:
            SetConstantValues<int8_t>(literalVal.value.byteVal, outVec);
            break;
        case OMNI_LONG:
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP:
            SetConstantValues<int64_t>(literalVal.value.longVal, outVec);
            break;
        case OMNI_DOUBLE:
            SetConstantValues<double>(literalVal.value.doubleVal, outVec);
            break;
        case OMNI_FLOAT:
            SetConstantValues<float>(literalVal.value.floatVal, outVec);
            break;
        case OMNI_BOOLEAN:
            SetConstantValues<bool>(literalVal.value.boolVal, outVec);
            break;
        case OMNI_DECIMAL128:
            SetConstantValues<Decimal128>(literalVal.value.decimal128Val, outVec);
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            SetConstantValues<std::string_view>(literalVal.value.stringVal, outVec);
            break;
        default:
            std::string errorMessage = "Do not support such vector type " + std::to_string(outputTypeId);
            context->SetError(errorMessage);
            return false;
    }
    return true;
}

BaseVector *Projection::ConstantArrayProjection(int32_t numSelectedRows) const
{
    const size_t n = constantArrayValues.size();
    if (numSelectedRows <= 0) {
        return new ArrayVector(0);
    }
    if (n == 0) {
        auto *arr = new ArrayVector(numSelectedRows);
        auto *elemVec = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(OMNI_DOUBLE, 0));
        arr->SetElementVectorFromRaw(elemVec);
        for (int32_t i = 0; i < numSelectedRows; i++) {
            arr->SetSize(i, 0);
        }
        return arr;
    }
    const int32_t totalElements = numSelectedRows * static_cast<int32_t>(n);
    auto *elemVec = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(OMNI_DOUBLE, totalElements));
    double *vals = unsafe::UnsafeVector::GetRawValues(elemVec);
    for (int32_t row = 0; row < numSelectedRows; row++) {
        for (size_t j = 0; j < n; j++) {
            vals[row * static_cast<int32_t>(n) + j] = constantArrayValues[j];
        }
    }
    auto *arrVec = new ArrayVector(numSelectedRows);
    arrVec->SetElementVectorFromRaw(elemVec);
    for (int32_t i = 0; i < numSelectedRows; i++) {
        arrVec->SetSize(i, static_cast<int32_t>(n));
    }
    return arrVec;
}

BaseVector *Projection::Project(VectorBatch *vecBatch, int32_t selectedRows[], int32_t numSelectedRows,
    int64_t *valueAddrs, int64_t *nullAddrs, int64_t *offsetAddrs, ExecutionContext *context,
    int64_t *dictionaryVectors, const int32_t *typeIds)
{
    if (!this->isSupported) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
            "Unsupported projection (e.g. array type not supported in codegen)");
    }
    if (this->isColumnProjection) {
        return ColumnProjectionProxy(vecBatch, selectedRows, numSelectedRows, typeIds);
    } else if (this->isConstantProjection) {
        BaseVector *outVec = nullptr;
        if (outType->GetId() == OMNI_ARRAY) {
            VectorHelper::CreateNullArrayVector(numSelectedRows, outType, outVec);
        } else if (outType->GetId() == OMNI_MAP) {
            VectorHelper::CreateNullMapVector(numSelectedRows, outType, outVec);
        } else if (outType->GetId() == OMNI_ROW){
            VectorHelper::CreateNullRowVector(numSelectedRows, outType, outVec);
        } else {
            outVec = VectorHelper::CreateFlatVector(outType->GetId(), numSelectedRows);
        }
        if (!ConstantColumnProjection(context, outVec)) {
            delete outVec;
            context->GetArena()->Reset();
            return nullptr;
        } else {
            context->GetArena()->Reset();
            return outVec;
        }
    } else if (this->isConstantArrayProjection) {
        BaseVector *outVec = ConstantArrayProjection(numSelectedRows);
        context->GetArena()->Reset();
        return outVec;
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
    int32_t numSelectedRows, BaseVector *colVec) const
{
    return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->CopyPositions(selectedRows, 0, numSelectedRows);
}

template <typename T>
BaseVector *Projection::ColumnProjectionFlatVectorSliceHelper(int32_t numSelectedRows, BaseVector *colVec) const
{
    return reinterpret_cast<Vector<T> *>(colVec)->Slice(0, numSelectedRows);
}

BaseVector *Projection::ColumnProjectionStructVectorSliceHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
    int32_t numSelectedRows) const
{
    auto colVec = vecBatch->Get(this->columnProjectionIndex);
    auto rowCnt = vecBatch->GetRowCount();
    auto rowVector = dynamic_cast<RowVector *>(colVec);
    if (rowVector == nullptr) {
        return nullptr;
    }
    if (numSelectedRows != 0 && numSelectedRows == rowCnt) {
        return rowVector->Slice(0, numSelectedRows);
    }

    if (selectedRows != nullptr && numSelectedRows != 0) {
        return rowVector->CopyPositions(selectedRows, 0, numSelectedRows);
    }
    return nullptr;
}

BaseVector *Projection::ColumnProjectionMapVectorSliceHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
    int32_t numSelectedRows) const
{
    auto colVec = vecBatch->Get(this->columnProjectionIndex);
    auto rowCnt = vecBatch->GetRowCount();
    auto mapVector = dynamic_cast<MapVector *>(colVec);
    if (numSelectedRows != 0 && numSelectedRows == rowCnt) {
        return mapVector->Slice(0, numSelectedRows);
    }

    if (selectedRows != nullptr && numSelectedRows != 0) {
        return mapVector->CopyPositions(selectedRows, 0, numSelectedRows);
    }
    return nullptr;
}

BaseVector *Projection::ColumnProjectionArrayVectorSliceHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
    int32_t numSelectedRows) const
{
    auto colVec = vecBatch->Get(this->columnProjectionIndex);
    auto rowCnt = vecBatch->GetRowCount();
    auto arrayVector = dynamic_cast<ArrayVector *>(colVec);
    if (numSelectedRows != 0 && numSelectedRows == rowCnt) {
        return arrayVector->Slice(0, numSelectedRows);
    }

    if (selectedRows != nullptr && numSelectedRows != 0) {
        return arrayVector->CopyPositions(selectedRows, 0, numSelectedRows);
    }
    return nullptr;
}

template <typename T>
BaseVector *Projection::ColumnProjectionDictionaryVectorSliceHelper(int32_t numSelectedRows, BaseVector *colVec) const
{
    return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->Slice(0, numSelectedRows);
}

void Projection::ProjectHelperVarWidth(VectorBatch &vecBatch, int64_t *valueAddrs, int64_t *nullAddrs,
    int64_t *offsetAddrs, BaseVector *outVec, int32_t numSelectedRows, int32_t selectedRows[],
    ExecutionContext *context, int64_t *dictionaryVectors, DataTypeId &outTypeId) const
{
    this->projector(valueAddrs, vecBatch.GetRowCount(), reinterpret_cast<int64_t>(outVec), selectedRows,
        numSelectedRows, nullAddrs, offsetAddrs,
        reinterpret_cast<int32_t *>(unsafe::UnsafeBaseVector::GetNulls(outVec)), nullptr,
        reinterpret_cast<int64_t>(context), dictionaryVectors);
}

void Projection::ProjectHelperFixedWidth(VectorBatch &vecBatch, int64_t *valueAddrs, int64_t *nullAddrs,
    int64_t *offsetAddrs, BaseVector *outVec, int32_t numSelectedRows, int32_t selectedRows[],
    ExecutionContext *context, int64_t *dictionaryVectors, DataTypeId &outTypeId) const
{
    auto outValueAddr = reinterpret_cast<int64_t>(VectorHelper::UnsafeGetValues(outVec));
    this->projector(valueAddrs, vecBatch.GetRowCount(), outValueAddr, selectedRows, numSelectedRows, nullAddrs,
        offsetAddrs, reinterpret_cast<int32_t *>(unsafe::UnsafeBaseVector::GetNulls(outVec)), nullptr,
        reinterpret_cast<int64_t>(context), dictionaryVectors);
}

BaseVector *Projection::ProjectVec(VectorBatch *vecBatch, ExecutionContext *context)
{
    context->SetResultRowSize(vecBatch->GetRowCount());
    ExprEval e(vecBatch, context);
    e.VisitExpr(*expr);
    auto outCol = e.GetResult();
    return outCol;
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
        case OMNI_DATE32:
            return ColumnProjectionHelper<int32_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_SHORT:
            return ColumnProjectionHelper<int16_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_BYTE:
            return ColumnProjectionHelper<int8_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return ColumnProjectionHelper<int64_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_DOUBLE:
            return ColumnProjectionHelper<double>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_FLOAT:
            return ColumnProjectionHelper<float>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_BOOLEAN:
            return ColumnProjectionHelper<bool>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_DECIMAL128:
            return ColumnProjectionHelper<Decimal128>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            return ColumnProjectionVarCharVectorHelper<std::string_view>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_MAP:
            return ColumnProjectionMapVectorSliceHelper(vecBatch, selectedRows, numSelectedRows);
        case OMNI_ROW:
            return ColumnProjectionStructVectorSliceHelper(vecBatch, selectedRows, numSelectedRows);
        case OMNI_TIMESTAMP_WITHOUT_TIME_ZONE:
            return ColumnProjectionHelper<int64_t>(vecBatch, selectedRows, numSelectedRows);
        case OMNI_ARRAY:
            return ColumnProjectionArrayVectorSliceHelper(vecBatch, selectedRows, numSelectedRows);
        default: LogError("Do not support such vector type %d", typeIds[columnProjectionIndex]);
            return nullptr;
    }
}

template <typename T>
BaseVector *Projection::ColumnProjectionHelper(VectorBatch *vecBatch, const int32_t *selectedRows,
    int32_t numSelectedRows) const
{
    auto colVec = vecBatch->Get(this->columnProjectionIndex);
    auto rowCnt = vecBatch->GetRowCount();
    if (colVec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = reinterpret_cast<ConstVector<T> *>(colVec);
        T val = constVec->GetConstValue();
        auto *newConst = new ConstVector<T>(val, colVec->GetTypeId(), numSelectedRows);
        if (constVec->HasNull() && constVec->IsNull(0)) {
            newConst->SetNulls(0, true, numSelectedRows);
        }
        return newConst;
    }
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
    if (colVec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = reinterpret_cast<ConstVector<std::string_view> *>(colVec);
        auto sv = constVec->GetConstValue();
        auto *newConst = new ConstVector<std::string_view>(sv, colVec->GetTypeId(), numSelectedRows);
        if (constVec->HasNull() && constVec->IsNull(0)) {
            newConst->SetNulls(0, true, numSelectedRows);
        }
        return newConst;
    }
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
            return VectorHelper::CreateStringDictionary(selectedRows, numSelectedRows,
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVec));
        }
    }
    return nullptr;
}

ExpressionEvaluator::ExpressionEvaluator(Expr *filterExpression, const std::vector<Expr *> &projectionExprs,
    const DataTypes &inputDataTypes, const config::QueryConfig &queryConfig)
    : inputTypes(const_cast<DataTypes &>(inputDataTypes)), queryConfig_(queryConfig)
{
    auto ofConfig = queryConfig.IsOverFlowASNull() == true
                    ? std::make_unique<OverflowConfig>(OVERFLOW_CONFIG_NULL)
                    : std::make_unique<OverflowConfig>(OVERFLOW_CONFIG_EXCEPTION);
    preferVectorization = queryConfig.PreferVectorizationExpression();
    hasFilter = true;
    filterExpr = filterExpression;
    for (auto &projectionExpr : projectionExprs) {
        projectionExpr->isRoot = true;
        projExprs.emplace_back(projectionExpr);
    }
    overflowConfig = std::make_unique<OverflowConfig>(*ofConfig);
    projectVecCount = static_cast<int32_t>(projectionExprs.size());

    for (int i = 0; isSupportedExpr && i < projectVecCount; ++i) {
        outputTypes.emplace_back(projExprs[i]->GetReturnType());
    }

    ExprVerifier verifier;
    verifier.VisitExpr(*filterExpr);
    for (auto &projExpr : projExprs) {
        verifier.VisitExpr(*projExpr);
    }

    isSupportVectorization = verifier.IsSupportVectorization();
    isSupportCodegen = verifier.IsSupportCodegen();
    useCodegen = !(preferVectorization && isSupportVectorization) && isSupportCodegen;
}

ExpressionEvaluator::ExpressionEvaluator(const std::vector<Expr *> &projectionExprs, const DataTypes &inputDataTypes,
    const config::QueryConfig &queryConfig)
    : inputTypes(const_cast<DataTypes &>(inputDataTypes)), queryConfig_(queryConfig)
{
    overflowConfig = queryConfig.IsOverFlowASNull() == true
                    ? std::make_unique<OverflowConfig>(OVERFLOW_CONFIG_NULL)
                    : std::make_unique<OverflowConfig>(OVERFLOW_CONFIG_EXCEPTION);
    preferVectorization = queryConfig.PreferVectorizationExpression();
    hasFilter = false;
    for (auto &projectionExpr : projectionExprs) {
        projectionExpr->isRoot = true;
        projExprs.emplace_back(projectionExpr);
    }
    projectVecCount = static_cast<int32_t>(projectionExprs.size());

    for (int i = 0; isSupportedExpr && i < projectVecCount; ++i) {
        outputTypes.emplace_back(projExprs[i]->GetReturnType());
    }

    ExprVerifier verifier;
    for (auto &projExpr : projExprs) {
        verifier.VisitExpr(*projExpr);
    }

    isSupportVectorization = verifier.IsSupportVectorization();
    isSupportCodegen = verifier.IsSupportCodegen();
    useCodegen = !(preferVectorization && isSupportVectorization) && isSupportCodegen;
}

ExpressionEvaluator::ExpressionEvaluator(Expr *filterExpression, const std::vector<Expr *> &projectionExprs,
    const DataTypes &inputDataTypes, OverflowConfig *ofConfig, bool preferVectorization)
    : inputTypes(const_cast<DataTypes &>(inputDataTypes)), preferVectorization(preferVectorization)
{
    hasFilter = true;
    filterExpr = filterExpression;
    for (auto &projectionExpr : projectionExprs) {
        projectionExpr->isRoot = true;
        projExprs.emplace_back(projectionExpr);
    }
    overflowConfig = std::make_unique<OverflowConfig>(*ofConfig);
    projectVecCount = static_cast<int32_t>(projectionExprs.size());

    for (int i = 0; isSupportedExpr && i < projectVecCount; ++i) {
        outputTypes.emplace_back(projExprs[i]->GetReturnType());
    }

    ExprVerifier verifier;
    verifier.VisitExpr(*filterExpr);
    for (auto &projExpr : projExprs) {
        verifier.VisitExpr(*projExpr);
    }

    isSupportVectorization = verifier.IsSupportVectorization();
    isSupportCodegen = verifier.IsSupportCodegen();
    useCodegen = !(preferVectorization && isSupportVectorization) && isSupportCodegen;
}

ExpressionEvaluator::ExpressionEvaluator(const std::vector<Expr *> &projectionExprs, const DataTypes &inputDataTypes,
    OverflowConfig *ofConfig, bool preferVectorization)
    : inputTypes(const_cast<DataTypes &>(inputDataTypes)), preferVectorization(preferVectorization)
{
    hasFilter = false;
    for (auto &projectionExpr : projectionExprs) {
        projectionExpr->isRoot = true;
        projExprs.emplace_back(projectionExpr);
    }
    overflowConfig = std::make_unique<OverflowConfig>(*ofConfig);
    projectVecCount = static_cast<int32_t>(projectionExprs.size());

    for (int i = 0; isSupportedExpr && i < projectVecCount; ++i) {
        outputTypes.emplace_back(projExprs[i]->GetReturnType());
    }

    ExprVerifier verifier;
    for (auto &projExpr : projExprs) {
        verifier.VisitExpr(*projExpr);
    }

    isSupportVectorization = verifier.IsSupportVectorization();
    isSupportCodegen = verifier.IsSupportCodegen();
    useCodegen = !(preferVectorization && isSupportVectorization) && isSupportCodegen;
}

bool ExpressionEvaluator::IsSupportedExpr() const
{
    return isSupportedExpr;
}

VectorBatch *ExpressionEvaluator::Evaluate(VectorBatch *vecBatch, ExecutionContext *context,
    AlignedBuffer<int32_t> *selectedRowsBuffer)
{
    context->SetConfig(queryConfig_);
    context->SetThrowOnError(!overflowConfig->IsOverflowAsNull());
    const int32_t vectorCount = vecBatch->GetVectorCount();
    intptr_t valueAddrs[vectorCount];
    intptr_t nullAddrs[vectorCount];
    intptr_t offsetAddrs[vectorCount];
    intptr_t dictionaries[vectorCount];

    if (useCodegen) {
        GetAddr(*vecBatch, valueAddrs, nullAddrs, offsetAddrs, dictionaries, this->inputTypes);
    }
    if (hasFilter) {
        return ProcessFilterAndProject(vecBatch, context, selectedRowsBuffer, valueAddrs, nullAddrs, offsetAddrs,
            dictionaries);
    } else {
        return ProcessProject(vecBatch, context, valueAddrs, nullAddrs, offsetAddrs, dictionaries);
    }
}

void ExpressionEvaluator::FilterFuncGeneration()
{
    if (!useCodegen) {
        return;
    }

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
    if (!useCodegen) {
        return;
    }

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
    auto projectedVecs = std::make_unique<VectorBatch>(rowCount);

    if (!useCodegen) {
        for (int32_t i = 0; i < projectVecCount; i++) {
            context->SetResultRowSize(vecBatch->GetRowCount());
            ExprEval e(vecBatch, context);
            e.VisitExpr(*projExprs[i]);
            auto outCol = e.GetResult();
            if (context->HasError()) {
                context->GetArena()->Reset();
                std::string errorMessage = context->GetError();
                throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
            }
            projectedVecs->Append(outCol);
        }
    } else {
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
    }
    context->GetArena()->Reset();
    return projectedVecs.release();
}

template <typename T>
BaseVector *ColumnProjectionHelper(BaseVector *colVec, int32_t numSelectedRows)
{
    if (colVec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = reinterpret_cast<ConstVector<T> *>(colVec);
        T val = constVec->GetConstValue();
        auto *newConst = new ConstVector<T>(val, colVec->GetTypeId(), numSelectedRows);
        if (constVec->HasNull() && constVec->IsNull(0)) {
            newConst->SetNulls(0, true, numSelectedRows);
        }
        return newConst;
    }
    if (colVec->GetEncoding() == OMNI_DICTIONARY) {
        return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->Slice(0, numSelectedRows);
    } else {
        return reinterpret_cast<Vector<T> *>(colVec)->Slice(0, numSelectedRows);
    }
}

BaseVector *ColumnProjectionStructVectorSliceHelper(BaseVector *colVec, int32_t numSelectedRows)
{
    auto rowVector = dynamic_cast<RowVector *>(colVec);
    if (rowVector == nullptr) {
        return nullptr;
    }
    return rowVector->Slice(0, numSelectedRows);
}

BaseVector *ColumnProjectionMapVectorSliceHelper(BaseVector *colVec, int32_t numSelectedRows)
{
    auto rowVector = dynamic_cast<MapVector *>(colVec);
    return rowVector->Slice(0, numSelectedRows);
}

BaseVector *ColumnProjectionArrayVectorSliceHelper(BaseVector *colVec, int32_t numSelectedRows)
{
    auto rowVector = dynamic_cast<ArrayVector *>(colVec);
    return rowVector->Slice(0, numSelectedRows);
}

template <typename T>
BaseVector *ColumnProjectionVarCharVectorHelper(BaseVector *colVec, int32_t numSelectedRows)
{
    if (colVec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = reinterpret_cast<ConstVector<std::string_view> *>(colVec);
        auto sv = constVec->GetConstValue();
        auto *newConst = new ConstVector<std::string_view>(sv, colVec->GetTypeId(), numSelectedRows);
        if (constVec->HasNull() && constVec->IsNull(0)) {
            newConst->SetNulls(0, true, numSelectedRows);
        }
        return newConst;
    }
    if (colVec->GetEncoding() == OMNI_DICTIONARY) {
        return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->Slice(0, numSelectedRows);
    } else {
        return reinterpret_cast<Vector<T> *>(colVec)->Slice(0, numSelectedRows);
    }
}

BaseVector *ColumnProjectionProxy(BaseVector *colVec, int32_t numSelectedRows, int32_t typeId)
{
    switch (typeId) {
        case OMNI_INT:
        case OMNI_DATE32:
            return ColumnProjectionHelper<int32_t>(colVec, numSelectedRows);
        case OMNI_SHORT:
            return ColumnProjectionHelper<int16_t>(colVec, numSelectedRows);
        case OMNI_BYTE:
            return ColumnProjectionHelper<int8_t>(colVec, numSelectedRows);
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return ColumnProjectionHelper<int64_t>(colVec, numSelectedRows);
        case OMNI_DOUBLE:
            return ColumnProjectionHelper<double>(colVec, numSelectedRows);
        case OMNI_FLOAT:
            return ColumnProjectionHelper<float>(colVec, numSelectedRows);
        case OMNI_BOOLEAN:
            return ColumnProjectionHelper<bool>(colVec, numSelectedRows);
        case OMNI_DECIMAL128:
            return ColumnProjectionHelper<Decimal128>(colVec, numSelectedRows);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            return ColumnProjectionVarCharVectorHelper<std::string_view>(colVec, numSelectedRows);
        case OMNI_MAP:
            return ColumnProjectionMapVectorSliceHelper(colVec, numSelectedRows);
        case OMNI_ROW:
            return ColumnProjectionStructVectorSliceHelper(colVec, numSelectedRows);
        case OMNI_ARRAY:
            return ColumnProjectionArrayVectorSliceHelper(colVec, numSelectedRows);
        default: LogError("Do not support such vector type %d", typeId);
            return nullptr;
    }
}

VectorBatch *ExpressionEvaluator::ProcessFilterAndProject(VectorBatch *vecBatch, ExecutionContext *context)
{
    context->hasFilter = false;
    const int rowCount = vecBatch->GetRowCount();
    int32_t numSelectedRows = 0;
    context->SetResultRowSize(rowCount);
    ExprEval e(vecBatch, context);
    e.VisitExpr(*filterExpr);

    auto selectVector = e.GetResult();
    bool *selectAddr = nullptr;
    std::unique_ptr<bool[]> selectMaskBuffer;
    if (selectVector->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constBoolSelectVec = dynamic_cast<vec::ConstVector<bool> *>(selectVector);
        if (constBoolSelectVec == nullptr) {
            throw OmniException("OPERATOR_RUNTIME_ERROR",
                "Const filter result must be a boolean const vector.");
        }

        selectMaskBuffer = std::make_unique<bool[]>(rowCount);
        const bool isNull = constBoolSelectVec->HasNull() && constBoolSelectVec->IsNull(0);
        const bool constantValue = !isNull && constBoolSelectVec->GetConstValue();
        for (int i = 0; i < rowCount; ++i) {
            selectMaskBuffer[i] = constantValue;
        }
        selectAddr = selectMaskBuffer.get();
        numSelectedRows = constantValue ? rowCount : 0;
    } else {
        selectAddr = static_cast<bool *>(VectorHelper::UnsafeGetValues(selectVector));
        // FIXME: The null value check here is a temporary workaround.
        // The In expression relies on SimpleFunction.h which skips null value processing internally.
        // This check ensures nulls are properly filtered, but can be removed once the null handling logic in
        // SimpleFunction is updated to handle null values natively for In expressions.
        auto *boolSelectVec = static_cast<vec::Vector<bool> *>(selectVector);
        for (int i = 0; i < rowCount; i++) {
            if (boolSelectVec->IsNull(i)) {
                selectAddr[i] = false;
            } else {
                if (selectAddr[i]) {
                    ++numSelectedRows;
                }
            }
        }
    }
    if (numSelectedRows == 0) {
        return nullptr;
    }
    auto projectedVecs = std::make_unique<VectorBatch>(numSelectedRows);

    context->hasFilter = true;
    context->SetIsSelectRow(selectAddr);
    for (int32_t i = 0; i < projectVecCount; i++) {
        if (projExprs[i]->GetType() == ExprType::FIELD_E && numSelectedRows != 0 && numSelectedRows == rowCount &&
            dynamic_cast<FieldExpr *>(projExprs[i])->input == nullptr) {
            auto colVal = dynamic_cast<FieldExpr *>(projExprs[i])->colVal;
            auto typeId = GetInputDataTypes().GetIds()[colVal];
            auto colVec = vecBatch->Get(colVal);
            projectedVecs->Append(ColumnProjectionProxy(colVec, numSelectedRows, typeId));
            continue;
        }
        context->SetResultRowSize(vecBatch->GetRowCount());
        ExprEval e2(vecBatch, context);
        e2.VisitExpr(*projExprs[i]);
        auto outCol = e2.GetResult();
        if (context->HasError()) {
            context->GetArena()->Reset();
            std::string errorMessage = context->GetError();
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
        projectedVecs->Append(outCol);
    }
    context->hasFilter = false;
    context->SetIsSelectRow(nullptr);
    return projectedVecs.release();
}

VectorBatch *ExpressionEvaluator::ProcessFilterAndProject(VectorBatch *vecBatch, ExecutionContext *context,
    AlignedBuffer<int32_t> *selectedRowsBuffer, intptr_t *valueAddrs, intptr_t *nullAddrs, intptr_t *offsetAddrs,
    intptr_t *dictionaries)
{
    if (!useCodegen) {
        return ProcessFilterAndProject(vecBatch, context);
    }
    const int rowCount = vecBatch->GetRowCount();
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
}
