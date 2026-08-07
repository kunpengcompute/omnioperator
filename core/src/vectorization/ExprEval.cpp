/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#include "ExprEval.h"
#include <string>
#include "codegen/expr_evaluator.h"
#include "type/data_type.h"

namespace omniruntime::vectorization {
using namespace omniruntime::expressions;
using namespace omniruntime::mem;
using namespace omniruntime::vec;
using namespace omniruntime::op;

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
    }
    return reinterpret_cast<Vector<T> *>(colVec)->Slice(0, numSelectedRows);
}

template <typename T>
BaseVector *ColumnProjectionCopyPositionsHelper(BaseVector *colVec, int32_t *selectedRows, int32_t numSelectedRows)
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
        return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->CopyPositions(selectedRows, 0,
            numSelectedRows);
    }
    return reinterpret_cast<Vector<T> *>(colVec)->CopyPositions(selectedRows, 0, numSelectedRows);
}

template <typename T>
BaseVector *ColumnProjectionVarCharVectorHelper(BaseVector *colVec, int32_t numSelectedRows)
{
    if (colVec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = reinterpret_cast<ConstVector<T> *>(colVec);
        auto sv = constVec->GetConstValue();
        auto *newConst = new ConstVector<T>(sv, colVec->GetTypeId(), numSelectedRows);
        if (constVec->HasNull() && constVec->IsNull(0)) {
            newConst->SetNulls(0, true, numSelectedRows);
        }
        return newConst;
    }
    auto rowCnt = colVec->GetSize();
    if (numSelectedRows != 0 && numSelectedRows == rowCnt) {
        if (colVec->GetEncoding() == OMNI_DICTIONARY) {
            return reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec)->Slice(0, numSelectedRows);
        } else {
            return reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVec)->
                Slice(0, numSelectedRows);
        }
    }
    return nullptr;
}

template <typename T>
BaseVector *ColumnProjectionVarCharCopyPositionsHelper(BaseVector *colVec, const int32_t *selectedRows,
    int32_t numSelectedRows)
{
    if (colVec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = reinterpret_cast<ConstVector<T> *>(colVec);
        auto sv = constVec->GetConstValue();
        auto *newConst = new ConstVector<T>(sv, colVec->GetTypeId(), numSelectedRows);
        if (constVec->HasNull() && constVec->IsNull(0)) {
            newConst->SetNulls(0, true, numSelectedRows);
        }
        return newConst;
    }
    if (colVec->GetEncoding() == OMNI_FLAT) {
        auto flatVec = reinterpret_cast<Vector<LargeStringContainer<T>> *>(colVec);
        return flatVec->CopyPositions(selectedRows, 0, numSelectedRows);
    } else if (colVec->GetEncoding() == OMNI_DICTIONARY) {
        auto dictVec = reinterpret_cast<Vector<DictionaryContainer<T>> *>(colVec);
        return dictVec->CopyPositions(selectedRows, 0, numSelectedRows);
    } else {
        OMNI_THROW("String Projection Error", "Unsupported encoding for varchar vector");
    }
}

template <typename T>
void SetConstantValues(T value, BaseVector *outVec)
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

void NullColumnProjection(ExecutionContext *context, BaseVector *outVec)
{
    auto outNulls = unsafe::UnsafeBaseVector::GetNulls(outVec);
    auto outNullsSize = BitUtil::Nbytes(outVec->GetSize());
    memset(outNulls, -1, outNullsSize);
}

void ConstantColumnProjection(ExecutionContext *context, BaseVector *outVec, const LiteralExpr &literalVal)
{
    if (literalVal.isNull) {
        return NullColumnProjection(context, outVec);
    }
    switch (literalVal.GetReturnTypeId()) {
        case OMNI_INT:
        case OMNI_DATE32:
            SetConstantValues<int32_t>(literalVal.intVal, outVec);
            break;
        case OMNI_SHORT:
            SetConstantValues<int16_t>(literalVal.shortVal, outVec);
            break;
        case OMNI_BYTE:
            SetConstantValues<int8_t>(literalVal.byteVal, outVec);
            break;
        case OMNI_LONG:
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP:
            SetConstantValues<int64_t>(literalVal.longVal, outVec);
            break;
        case OMNI_DOUBLE:
            SetConstantValues<double>(literalVal.doubleVal, outVec);
            break;
        case OMNI_FLOAT:
            SetConstantValues<float>(literalVal.floatVal, outVec);
            break;
        case OMNI_BOOLEAN:
            SetConstantValues<bool>(literalVal.boolVal, outVec);
            break;
        case OMNI_DECIMAL128: {
            std::string dec128String = *literalVal.stringVal;
            Decimal128 decimal128(dec128String);
            SetConstantValues<Decimal128>(decimal128, outVec);
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            SetConstantValues<std::string_view>(*literalVal.stringVal, outVec);
            break;
        default:
            std::string errorMessage = "Do not support such vector type " +
                std::to_string(literalVal.GetReturnTypeId());
            OMNI_THROW("Runtime error", "NullColumnProjection:" + errorMessage);
    }
}

ExprEval::ExprEval(VectorBatch *vectorBatch, ExecutionContext *context): context(context)
{
    for (int i = 0; i < vectorBatch->GetVectorCount(); i++) {
        vecBatch_.push_back(vectorBatch->Get(i));
        typeIds.push_back(vectorBatch->Get(i)->GetTypeId());
    }
    inputRowSize = vectorBatch->GetRowCount();
    rowSize = context->hasFilter ? context->GetResultRowSize() : inputRowSize;
}

ExprEval::ExprEval(ExecutionContext *context): context(context)
{}

void ExprEval::Visit(const LiteralExpr &e)
{
    if (!e.isRoot) {
        auto typeId = e.dataType->GetId();
        BaseVector *constVec = nullptr;
        switch (e.dataType->GetId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                constVec = new ConstVector(e.intVal, typeId, rowSize);
                break;
            case OMNI_SHORT:
                constVec = new ConstVector(e.shortVal, typeId, rowSize);
                break;
            case OMNI_BYTE:
                constVec = new ConstVector(e.byteVal, typeId, rowSize);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                constVec = new ConstVector(e.longVal, typeId, rowSize);
                break;
            case OMNI_DOUBLE:
                constVec = new ConstVector(e.doubleVal, typeId, rowSize);
                break;
            case OMNI_FLOAT:
                constVec = new ConstVector(e.floatVal, typeId, rowSize);
                break;
            case OMNI_BOOLEAN:
                constVec = new ConstVector(e.boolVal, typeId, rowSize);
                break;
            case OMNI_DECIMAL128: {
                Decimal128 decimal128(*e.stringVal);
                constVec = new ConstVector<Decimal128>(decimal128, typeId, rowSize);
                break;
            }
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY:
                constVec = new ConstVector(std::string_view(*e.stringVal), typeId, rowSize);
                break;
            case OMNI_MAP: {
                // Non-root null map literals (e.g. stack padding: array(col, null)) must match
                // batch row count; size 1 would overrun in variadic functions like array()/named_struct().
                if (e.isNull) {
                    VectorHelper::CreateNullMapVector(rowSize, e.dataType, constVec);
                } else {
                    constVec = VectorHelper::CreateComplexVector(e.dataType.get(), rowSize);
                }
                break;
            }
            case OMNI_ARRAY: {
                if (e.isNull) {
                    VectorHelper::CreateNullArrayVector(rowSize, e.dataType, constVec);
                } else {
                    constVec = VectorHelper::CreateComplexVector(e.dataType.get(), rowSize);
                }
                break;
            }
            case OMNI_ROW: {
                if (e.isNull) {
                    VectorHelper::CreateNullRowVector(rowSize, e.dataType, constVec);
                } else {
                    constVec = VectorHelper::CreateComplexVector(e.dataType.get(), rowSize);
                }
                break;
            }
            case OMNI_NONE: {
                auto *nullArrayVec = new ArrayVector(rowSize);
                auto emptyElements = std::shared_ptr<BaseVector>(
                    VectorHelper::CreateFlatVector(static_cast<int32_t>(OMNI_INT), 0));
                nullArrayVec->SetElementVector(emptyElements);
                for (int32_t i = 0; i < rowSize; ++i) {
                    nullArrayVec->SetNull(i);
                }
                constVec = nullArrayVec;
                break;
            }
            default: LogError("Do not support such vector type %d", typeIds[e.dataType->GetId()]);
        }
        if (constVec != nullptr && e.isNull) {
            constVec->SetNulls(0, true, constVec->GetSize());
        }
        inputValues_.push(constVec);
        return;
    }
    auto typeId = e.dataType->GetId();
    if (typeId == OMNI_NONE) {
        auto *arrayVec = new ArrayVector(rowSize);
        auto emptyElements = std::shared_ptr<BaseVector>(
            VectorHelper::CreateFlatVector(static_cast<int32_t>(OMNI_INT), 0));
        arrayVec->SetElementVector(emptyElements);
        for (int32_t i = 0; i < rowSize; ++i) {
            arrayVec->SetNull(i);
        }
        inputValues_.push(arrayVec);
        return;
    }
    if (typeId == OMNI_ARRAY) {
        BaseVector *arrayVec = nullptr;
        VectorHelper::CreateNullArrayVector(rowSize, e.dataType, arrayVec);
        inputValues_.push(arrayVec);
        return;
    }
    if (typeId == OMNI_MAP) {
        BaseVector *mapVec = nullptr;
        VectorHelper::CreateNullMapVector(rowSize, e.dataType, mapVec);
        inputValues_.push(mapVec);
        return;
    }
    if (typeId == OMNI_ROW) {
        BaseVector *rowVector = nullptr;
        VectorHelper::CreateNullRowVector(rowSize, e.dataType, rowVector);
        inputValues_.push(rowVector);
        return;
    }
    BaseVector *outVec = VectorHelper::CreateFlatVector(typeId, rowSize);
    ConstantColumnProjection(context, outVec, e);
    inputValues_.push(outVec);
}

BaseVector *GetStructField(const Expr *e, const std::vector<BaseVector *> &inputVecBatch)
{
    auto fieldExpr = dynamic_cast<const FieldExpr *>(e);
    if (fieldExpr != nullptr) {
        if (fieldExpr->input == nullptr) {
            return inputVecBatch[fieldExpr->colVal];
        }
        auto tmpVec = dynamic_cast<RowVector *>(GetStructField(fieldExpr->input, inputVecBatch));
        if (tmpVec != nullptr) {
            return tmpVec->ChildAt(fieldExpr->colVal).get();
        }
        OMNI_THROW("Express Error:", "GetStructField error!!");
    }
    OMNI_THROW("Express Error:", "GetStructField error!");
}

void ExprEval::Visit(const FieldExpr &e)
{
    auto colVec = GetStructField(&e, vecBatch_);
    auto typeId = colVec->GetTypeId();
    // For nested field access (e.g., col_structtype.name), e.colVal is the child field
    // index within the struct, not the top-level column index in vecBatch_.
    // We need to traverse the FieldExpr input chain to find the root FieldExpr
    // and use its colVal as the correct top-level column index.
    int32_t topLevelColVal = e.colVal;
    if (e.input != nullptr) {
        const FieldExpr* current = &e;
        while (current->input != nullptr) {
            current = dynamic_cast<const FieldExpr*>(current->input);
        }
        topLevelColVal = current->colVal;
    }
    if (topLevelColVal >= 0 && topLevelColVal < static_cast<int32_t>(vecBatch_.size()) && vecBatch_[topLevelColVal] != nullptr) {
        vecBatch_[topLevelColVal]->SetIsField(true);
    }
    if (context->hasFilter) {
        auto isSelect = context->GetIsSelectRow();
        int selectRow[inputRowSize] = {-1};
        int selectSize = 0;
        for (int i = 0; i < inputRowSize; i++) {
            if (isSelect[i]) {
                selectRow[selectSize] = i;
                ++selectSize;
            }
        }
        switch (typeId) {
            case OMNI_INT:
            case OMNI_DATE32:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<int32_t>(colVec, selectRow, selectSize));
                break;
            case OMNI_SHORT:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<int16_t>(colVec, selectRow, selectSize));
                break;
            case OMNI_BYTE:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<int8_t>(colVec, selectRow, selectSize));
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<int64_t>(colVec, selectRow, selectSize));
                break;
            case OMNI_DOUBLE:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<double>(colVec, selectRow, selectSize));
                break;
            case OMNI_FLOAT:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<float>(colVec, selectRow, selectSize));
                break;
            case OMNI_BOOLEAN:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<bool>(colVec, selectRow, selectSize));
                break;
            case OMNI_DECIMAL128:
                inputValues_.push(ColumnProjectionCopyPositionsHelper<Decimal128>(colVec, selectRow, selectSize));
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY:
                inputValues_.push(
                    ColumnProjectionVarCharCopyPositionsHelper<std::string_view>(colVec, selectRow, selectSize));
                break;
            case OMNI_ARRAY:
                inputValues_.push(reinterpret_cast<ArrayVector *>(colVec)->CopyPositions(selectRow, 0, selectSize));
                break;
            case OMNI_MAP:
                inputValues_.push(reinterpret_cast<MapVector *>(colVec)->CopyPositions(selectRow, 0, selectSize));
                break;
            case OMNI_ROW:
                inputValues_.push(reinterpret_cast<RowVector *>(colVec)->CopyPositions(selectRow, 0, selectSize));
                break;
            default: LogError("Do not support such vector type %d", typeIds[e.colVal]);
        }
        return;
    }
    switch (typeId) {
        case OMNI_INT:
        case OMNI_DATE32:
            inputValues_.push(ColumnProjectionHelper<int32_t>(colVec, rowSize));
            break;
        case OMNI_SHORT:
            inputValues_.push(ColumnProjectionHelper<int16_t>(colVec, rowSize));
            break;
        case OMNI_BYTE:
            inputValues_.push(ColumnProjectionHelper<int8_t>(colVec, rowSize));
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            inputValues_.push(ColumnProjectionHelper<int64_t>(colVec, rowSize));
            break;
        case OMNI_DOUBLE:
            inputValues_.push(ColumnProjectionHelper<double>(colVec, rowSize));
            break;
        case OMNI_FLOAT:
            inputValues_.push(ColumnProjectionHelper<float>(colVec, rowSize));
            break;
        case OMNI_BOOLEAN:
            inputValues_.push(ColumnProjectionHelper<bool>(colVec, rowSize));
            break;
        case OMNI_DECIMAL128:
            inputValues_.push(ColumnProjectionHelper<Decimal128>(colVec, rowSize));
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            inputValues_.push(ColumnProjectionVarCharVectorHelper<std::string_view>(colVec, rowSize));
            break;
        case OMNI_ARRAY:
            inputValues_.push(reinterpret_cast<ArrayVector *>(colVec)->Slice(0, rowSize));
            break;
        case OMNI_MAP:
            inputValues_.push(reinterpret_cast<MapVector *>(colVec)->Slice(0, rowSize));
            break;
        case OMNI_ROW:
            inputValues_.push(reinterpret_cast<RowVector *>(colVec)->Slice(0, rowSize));
            break;
        default: LogError("Do not support such vector type %d", typeIds[e.colVal]);
    }
}

void ExprEval::Visit(const UnaryExpr &e)
{
    e.exp->Accept(*this);
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for unary expression");
    }
    auto result = VectorHelper::CreateFlatVector(e.dataType->GetId(), rowSize);
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const BinaryExpr &e)
{
    e.left->Accept(*this);
    e.right->Accept(*this);
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for binary expression");
    }

    BaseVector *result = nullptr;
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const InExpr &e)
{
    e.arguments[0]->Accept(*this);
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for in expression");
    }
    BaseVector *result = nullptr;
    const auto inputDataType = e.arguments[0]->dataType;
    ArrayType arrayType(inputDataType);
    auto searchArray = VectorHelper::CreateComplexVectorShared(&arrayType, 1);
    auto vector = VectorHelper::CreateFlatVectorShared(inputDataType->GetId(), e.arguments.size() - 1);
    dynamic_cast<ArrayVector *>(searchArray.get())->SetValue(0, vector.get());
    inputValues_.push(searchArray.get());
    auto temp = inputValues_;
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const BetweenExpr &e) {}

void ExprEval::Visit(const IfExpr &e)
{
    for (auto arg : e.arguments) {
        arg->Accept(*this);
    }
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for if expression");
    }

    BaseVector *result = nullptr;
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const CoalesceExpr &e)
{
    for (auto arg : e.arguments) {
        arg->Accept(*this);
    }
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for coalesce expression");
    }

    BaseVector *result = nullptr;
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const IsNullExpr &e)
{
    e.value->Accept(*this);
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for isnull expression");
    }
    auto result = VectorHelper::CreateFlatVector(e.dataType->GetId(), rowSize);
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const FuncExpr &e)
{
    for (auto arg : e.arguments) {
        arg->Accept(*this);
    }
    if (e.funcName == "array") {
        context->setInputParamsNUms(e.arguments.size());
    }
    BaseVector *result = nullptr;
    auto resolved = e.vectorFunction;
    bool needQueryConfig = (e.funcName == "spark_partition_id" || e.funcName == "uuid" || e.funcName == "rand");
    if (resolved == nullptr || needQueryConfig) {
        std::vector<DataTypeId> argTypes(e.arguments.size());
        std::transform(e.arguments.begin(), e.arguments.end(), argTypes.begin(),
            [](Expr *expr) -> DataTypeId { return expr->GetReturnTypeId(); });
        auto signature = std::make_shared<codegen::FunctionSignature>(e.funcName, argTypes, e.dataType->GetId());
        resolved = VectorFunction::Find(signature, e.constantInputs, context->queryConfigRef());
        if (resolved == nullptr) {
            resolved = VectorFunction::Find(signature, context->queryConfigRef());
        }
        if (resolved != nullptr) {
            const_cast<FuncExpr &>(e).vectorFunction = resolved;
        } else if (e.vectorFunction != nullptr && !needQueryConfig) {
            resolved = e.vectorFunction;
        } else {
            OMNI_THROW("Vectorization Error:", "Vector function not found for function: " + e.funcName);
        }
    }
    
    // to_json needs the input argument's DataType (carrying struct field names) so it can
    // emit real field names instead of field0/field1. The Apply interface only passes the
    // output type, so stash the input type on the context for ToJson to read.
    if (e.funcName == "to_json" && !e.arguments.empty()) {
        context->SetToJsonInputType(e.arguments[0]->dataType.get());
    }

    resolved->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const SwitchExpr &e) {}

void ExprEval::Visit(const ParamRefExpr &e) {
    int32_t paramIdx = paramNameToIdxMap[e.paramName_];
    vec::BaseVector *paramVec = lambdaParams_.at(paramIdx);
    vec::BaseVector *toPush = paramVec->Slice(0, paramVec->GetSize());
    inputValues_.push(toPush != nullptr ? toPush : paramVec);
}

void ExprEval::Visit(const LambdaExpr &e) {
    context->SetCurrentLambda(&e);
}

void ExprEval::VisitExpr(const Expr &e)
{
    e.Accept(*this);
}

BaseVector *ExprEval::GetResult()
{
    auto result = inputValues_.top();
    inputValues_.pop();
    return result;
}

int32_t ExprEval::GetRowCount() const
{
    return rowSize;
}
}
