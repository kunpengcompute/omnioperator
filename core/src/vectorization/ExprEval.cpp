/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include "ExprEval.h"
#include <iostream>
#include <string>
#include "codegen/expr_evaluator.h"
#include "type/data_type.h"
#include "util/bit_util.h"
#include "vector/unsafe_vector.h"
#include "vectorization/ConjunctCost.h"
#include "vectorization/functions/If.h"
#include "vectorization/functions/Md5ConcatWsFusion.h"

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
BaseVector *ColumnProjectionCopyPositionsHelper(BaseVector *colVec, const int32_t *selectedRows, int32_t numSelectedRows)
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
    rowSize = vectorBatch->GetRowCount();
}

ExprEval::ExprEval(ExecutionContext *context): context(context), rowSize(0)
{}

void ExprEval::Reset(VectorBatch *vectorBatch, ExecutionContext *ctx)
{
    context = ctx;
    int32_t vecCount = vectorBatch->GetVectorCount();
    vecBatch_.clear();
    vecBatch_.reserve(vecCount);
    for (int i = 0; i < vecCount; i++) {
        vecBatch_.push_back(vectorBatch->Get(i));
    }
    typeIds.clear();
    typeIds.reserve(vecCount);
    for (int i = 0; i < vecCount; i++) {
        typeIds.push_back(vectorBatch->Get(i)->GetTypeId());
    }
    std::stack<vec::BaseVector *>().swap(inputValues_);
    activeSel_ = nullptr;
    activePos_.clear();
    rowSize = vectorBatch->GetRowCount();
}

void ExprEval::Visit(const LiteralExpr &e)
{
    int32_t rowCount = CurrentRowCount();
    if (!e.isRoot) {
        auto typeId = e.dataType->GetId();
        BaseVector *constVec = nullptr;
        switch (e.dataType->GetId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                constVec = new ConstVector(e.intVal, typeId, rowCount);
                break;
            case OMNI_SHORT:
                constVec = new ConstVector(e.shortVal, typeId, rowCount);
                break;
            case OMNI_BYTE:
                constVec = new ConstVector(e.byteVal, typeId, rowCount);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                constVec = new ConstVector(e.longVal, typeId, rowCount);
                break;
            case OMNI_DOUBLE:
                constVec = new ConstVector(e.doubleVal, typeId, rowCount);
                break;
            case OMNI_FLOAT:
                constVec = new ConstVector(e.floatVal, typeId, rowCount);
                break;
            case OMNI_BOOLEAN:
                constVec = new ConstVector(e.boolVal, typeId, rowCount);
                break;
            case OMNI_DECIMAL128: {
                Decimal128 decimal128(*e.stringVal);
                constVec = new ConstVector<Decimal128>(decimal128, typeId, rowCount);
                break;
            }
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY:
                constVec = new ConstVector(std::string_view(*e.stringVal), typeId, rowCount);
                break;
            case OMNI_STRING_VIEW:
#ifdef STRINGVIEW_ENABLE
                // StringView is a 16-byte fixed-width struct. For non-inline values its data pointer
                // refers to e.stringVal's buffer, which LiteralExpr owns throughout evaluation.
                // Construct it from (const char*, len) to avoid the deleted std::string_view rvalue
                // overload and its resulting ambiguity.
                constVec = new ConstVector<StringView>(
                    StringView(e.stringVal->data(), static_cast<int32_t>(e.stringVal->size())), typeId, rowCount);
                break;
#else
                OMNI_THROW("STRING_VIEW_DISABLED", "StringView expression was requested but this native build was configured with STRINGVIEW_ENABLE=OFF");
#endif
            case OMNI_MAP: {
                // Non-root null map literals (e.g. stack padding: array(col, null)) must match
                // batch row count; size 1 would overrun in variadic functions like array()/named_struct().
                if (e.isNull) {
                    VectorHelper::CreateNullMapVector(rowCount, e.dataType, constVec);
                } else {
                    constVec = VectorHelper::CreateComplexVector(e.dataType.get(), rowCount);
                }
                break;
            }
            case OMNI_ARRAY: {
                if (e.isNull) {
                    VectorHelper::CreateNullArrayVector(rowCount, e.dataType, constVec);
                } else {
                    constVec = VectorHelper::CreateComplexVector(e.dataType.get(), rowCount);
                }
                break;
            }
            case OMNI_ROW: {
                if (e.isNull) {
                    VectorHelper::CreateNullRowVector(rowCount, e.dataType, constVec);
                } else {
                    constVec = VectorHelper::CreateComplexVector(e.dataType.get(), rowCount);
                }
                break;
            }
            case OMNI_NONE: {
                auto *nullArrayVec = new ArrayVector(rowCount);
                auto emptyElements = std::shared_ptr<BaseVector>(
                    VectorHelper::CreateFlatVector(static_cast<int32_t>(OMNI_INT), 0));
                nullArrayVec->SetElementVector(emptyElements);
                for (int32_t i = 0; i < rowCount; ++i) {
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
        auto *arrayVec = new ArrayVector(rowCount);
        auto emptyElements = std::shared_ptr<BaseVector>(
            VectorHelper::CreateFlatVector(static_cast<int32_t>(OMNI_INT), 0));
        arrayVec->SetElementVector(emptyElements);
        for (int32_t i = 0; i < rowCount; ++i) {
            arrayVec->SetNull(i);
        }
        inputValues_.push(arrayVec);
        return;
    }
    if (typeId == OMNI_ARRAY) {
        BaseVector *arrayVec = nullptr;
        VectorHelper::CreateNullArrayVector(rowCount, e.dataType, arrayVec);
        inputValues_.push(arrayVec);
        return;
    }
    if (typeId == OMNI_MAP) {
        BaseVector *mapVec = nullptr;
        VectorHelper::CreateNullMapVector(rowCount, e.dataType, mapVec);
        inputValues_.push(mapVec);
        return;
    }
    if (typeId == OMNI_ROW) {
        BaseVector *rowVector = nullptr;
        VectorHelper::CreateNullRowVector(rowCount, e.dataType, rowVector);
        inputValues_.push(rowVector);
        return;
    }
    BaseVector *outVec = VectorHelper::CreateFlatVector(typeId, rowCount);
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

void ExprEval::PushProjectedColumn(BaseVector *colVec, DataTypeId typeId, const int32_t *selectRow, int32_t selectSize)
{
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
        case OMNI_DATE64:
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
        case OMNI_STRING_VIEW:
            inputValues_.push(ColumnProjectionCopyPositionsHelper<StringView>(colVec, selectRow, selectSize));
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
        default: LogError("Do not support such vector type %d", typeId);
    }
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
    if (activeSel_ != nullptr) {
        // Compacted evaluation mode: project only the active rows, in full-batch order.
        PushProjectedColumn(colVec, typeId, activePos_.data(), static_cast<int32_t>(activePos_.size()));
        return;
    }
    if (context->hasFilter) {
        auto isSelect = context->GetIsSelectRow();
        int selectRow[context->GetResultRowSize()] = {-1};
        int selectSize = 0;
        for (int i = 0; i < context->GetResultRowSize(); i++) {
            if (isSelect[i]) {
                selectRow[selectSize] = i;
                ++selectSize;
            }
        }
        PushProjectedColumn(colVec, typeId, selectRow, selectSize);
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
        case OMNI_DATE64:
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
        case OMNI_STRING_VIEW:
            inputValues_.push(ColumnProjectionHelper<StringView>(colVec, rowSize));
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
    auto result = VectorHelper::CreateFlatVector(e.dataType->GetId(), CurrentRowCount());
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const BinaryExpr &e)
{
    // Lazy AND/OR: skip when throw-on-error is enabled, because a skipped or
    // reordered conjunct must not change which rows surface errors (see
    // lazy_conjunct_optimization.md section 5.3).
    if ((e.op == expressions::Operator::AND || e.op == expressions::Operator::OR) && !context->ThrowOnError()) {
        VisitLazyLogical(e);
        return;
    }
    e.left->Accept(*this);
    e.right->Accept(*this);
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for binary expression");
    }

    BaseVector *result = nullptr;
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

bool ExprEval::IsFlatBoolColumnOperand(const Expr *operand) const
{
    if (operand->GetType() != ExprType::FIELD_E) {
        return false;
    }
    // Resolve the top-level column index by walking the field access chain;
    // an empty batch (n = 0) has no columns, so bounds-check before touching
    // vecBatch_ (GetStructField would read out of bounds).
    int32_t col = static_cast<const FieldExpr *>(operand)->colVal;
    for (const FieldExpr *cur = static_cast<const FieldExpr *>(operand); cur->input != nullptr;) {
        cur = dynamic_cast<const FieldExpr *>(cur->input);
        if (cur == nullptr) {
            break;
        }
        col = cur->colVal;
    }
    if (col < 0 || col >= static_cast<int32_t>(vecBatch_.size()) || vecBatch_[col] == nullptr) {
        return false;
    }
    return vecBatch_[col]->GetEncoding() == OMNI_FLAT;
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

int32_t ExprEval::CurrentRowCount() const
{
    // In compacted mode the space size is the number of active rows; otherwise it is
    // the context row count, which callers keep in sync with the space being
    // evaluated (batch row count for regular evaluation, element count for lambda
    // evaluation).
    return activeSel_ != nullptr ? static_cast<int32_t>(activePos_.size()) : context->GetResultRowSize();
}

BaseVector *ExprEval::EvaluateBranch(const Expr *branch, const SelectivityVector &innerSel, bool inPlace)
{
    int32_t n = CurrentRowCount();
    int32_t selCount = innerSel.countSelected();
    if (selCount == 0 && n > 0) {
        OMNI_THROW("If expr Error", "Internal error: empty branch selection");
    }
    if (inPlace || selCount == n) {
        // The branch covers the whole (possibly empty) current space; evaluate it in
        // place.
        branch->Accept(*this);
    } else {
        // Evaluate only the selected rows: map the selection into full-batch
        // coordinates so column projections gather the right rows, and evaluate in
        // compacted mode. The compacted space's own size is the bound of those
        // coordinates (activePos_ positions live in the space the guard covers),
        // not the batch row count, which can differ under lambda evaluation. When
        // no compacted space is active the mapping is the identity and the current
        // space size bounds all positions (e.g. lambda evaluation has no batch
        // columns).
        int32_t fullSpace = activeSel_ != nullptr ? activeSel_->size() : n;
        SelectivityVector branchSel(fullSpace, false);
        std::vector<int32_t> branchPos;
        branchPos.reserve(selCount);
        innerSel.applyToSelected([&](vector_size_t i) {
            int32_t fullPos = activeSel_ != nullptr ? activePos_[i] : i;
            if (fullPos >= fullSpace) {
                OMNI_THROW("Vectorization Error:", "Internal error: branch position exceeds full-space bounds");
            }
            branchSel.setValid(fullPos, true);
            branchPos.push_back(fullPos);
        });
        branchSel.updateBounds();
        ScopedCompactEval guard(*this, branchSel, branchPos);
        branch->Accept(*this);
    }
    BaseVector *result = inputValues_.top();
    inputValues_.pop();
    return result;
}

namespace {
/// A branch whose leaves cannot throw and whose per-row evaluation is cheaper than
/// gathering + scattering its inputs: evaluating it over the whole current space and
/// merging afterwards beats the compacted path.
bool IsCheapIfBranch(const Expr *e)
{
    switch (e->GetType()) {
        case ExprType::FIELD_E:
        case ExprType::LITERAL_E:
        case ExprType::PARAM_REF_E:
            return true;
        default:
            return false;
    }
}

/// Merges the two (in-place or compacted) branch vectors into result in ascending
/// row order. String and array containers append values per row and rely on fill
/// order, so the merge must visit result rows strictly in order.
template <typename T>
void MergeIfTyped(BaseVector *trueVec, BaseVector *falseVec, const std::vector<int32_t> &truePos,
    const std::vector<int32_t> &falsePos, bool trueCheap, bool falseCheap, BaseVector *result, int32_t n)
{
    auto *resVec = static_cast<Vector<T> *>(result);
    bool trueConst = trueVec->GetEncoding() == OMNI_ENCODING_CONST;
    bool falseConst = falseVec->GetEncoding() == OMNI_ENCODING_CONST;
    bool trueHasNull = trueVec->HasNull();
    bool falseHasNull = falseVec->HasNull();
    size_t trueIdx = 0;
    size_t falseIdx = 0;
    for (int32_t dst = 0; dst < n; ++dst) {
        if (trueIdx < truePos.size() && truePos[trueIdx] == dst) {
            int32_t srcRow = trueConst ? 0 : (trueCheap ? dst : static_cast<int32_t>(trueIdx));
            if (trueHasNull && trueVec->IsNull(srcRow)) {
                resVec->SetNull(dst);
            } else {
                resVec->SetValue(dst, VectorHelper::GetValueFromVector<T>(trueVec, srcRow));
            }
            ++trueIdx;
        } else {
            int32_t srcRow = falseConst ? 0 : (falseCheap ? dst : static_cast<int32_t>(falseIdx));
            if (falseHasNull && falseVec->IsNull(srcRow)) {
                resVec->SetNull(dst);
            } else {
                resVec->SetValue(dst, VectorHelper::GetValueFromVector<T>(falseVec, srcRow));
            }
            ++falseIdx;
        }
    }
}

void MergeIfString(BaseVector *trueVec, BaseVector *falseVec, const std::vector<int32_t> &truePos,
    const std::vector<int32_t> &falsePos, bool trueCheap, bool falseCheap, BaseVector *result, int32_t n)
{
    auto *resVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    bool trueConst = trueVec->GetEncoding() == OMNI_ENCODING_CONST;
    bool falseConst = falseVec->GetEncoding() == OMNI_ENCODING_CONST;
    bool trueHasNull = trueVec->HasNull();
    bool falseHasNull = falseVec->HasNull();
    size_t trueIdx = 0;
    size_t falseIdx = 0;
    for (int32_t dst = 0; dst < n; ++dst) {
        if (trueIdx < truePos.size() && truePos[trueIdx] == dst) {
            int32_t srcRow = trueConst ? 0 : (trueCheap ? dst : static_cast<int32_t>(trueIdx));
            if (trueHasNull && trueVec->IsNull(srcRow)) {
                resVec->SetNull(dst);
            } else {
                resVec->SetValue(dst, VectorHelper::GetStringValueFromVector(trueVec, srcRow));
            }
            ++trueIdx;
        } else {
            int32_t srcRow = falseConst ? 0 : (falseCheap ? dst : static_cast<int32_t>(falseIdx));
            if (falseHasNull && falseVec->IsNull(srcRow)) {
                resVec->SetNull(dst);
            } else {
                resVec->SetValue(dst, VectorHelper::GetStringValueFromVector(falseVec, srcRow));
            }
            ++falseIdx;
        }
    }
}

void MergeIfComplex(BaseVector *trueVec, BaseVector *falseVec, const std::vector<int32_t> &truePos,
    const std::vector<int32_t> &falsePos, bool trueCheap, bool falseCheap, BaseVector *result, int32_t n,
    DataTypeId typeId)
{
    size_t trueIdx = 0;
    size_t falseIdx = 0;
    for (int32_t dst = 0; dst < n; ++dst) {
        BaseVector *src = nullptr;
        int32_t srcRow = 0;
        if (trueIdx < truePos.size() && truePos[trueIdx] == dst) {
            src = trueVec;
            srcRow = trueCheap ? dst : static_cast<int32_t>(trueIdx);
            ++trueIdx;
        } else {
            src = falseVec;
            srcRow = falseCheap ? dst : static_cast<int32_t>(falseIdx);
            ++falseIdx;
        }
        if (src->IsNull(srcRow)) {
            if (typeId == OMNI_ARRAY) {
                static_cast<ArrayVector *>(result)->SetNull(dst);
            } else if (typeId == OMNI_MAP) {
                static_cast<MapVector *>(result)->SetNull(dst);
            } else {
                result->SetNull(dst);
            }
            continue;
        }
        if (typeId == OMNI_ARRAY) {
            IfFunction::CopyArrayValue(src, result, srcRow, dst);
        } else if (typeId == OMNI_MAP) {
            IfFunction::CopyMapValue(src, result, srcRow, dst);
        } else {
            IfFunction::CopyRowValue(src, result, srcRow, dst);
        }
    }
}

/// Scatters the branch values into result at the selected positions, visiting result
/// rows in ascending order. trueCheap/falseCheap tell whether the corresponding
/// branch vector covers the whole space (src row == dst row) or is compacted (src row
/// == compacted index).
void MergeIfBranches(BaseVector *trueVec, BaseVector *falseVec, const std::vector<int32_t> &truePos,
    const std::vector<int32_t> &falsePos, bool trueCheap, bool falseCheap, BaseVector *result, int32_t n,
    DataTypeId typeId)
{
    switch (typeId) {
        case OMNI_BOOLEAN:
            MergeIfTyped<bool>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_BYTE:
            MergeIfTyped<int8_t>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_SHORT:
            MergeIfTyped<int16_t>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            MergeIfTyped<int32_t>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_LONG:
        case OMNI_DATE64:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            MergeIfTyped<int64_t>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_FLOAT:
            MergeIfTyped<float>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_DOUBLE:
            MergeIfTyped<double>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_DECIMAL128:
            MergeIfTyped<Decimal128>(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            MergeIfString(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n);
            break;
        case OMNI_ARRAY:
        case OMNI_MAP:
        case OMNI_ROW:
            MergeIfComplex(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n, typeId);
            break;
        default:
            OMNI_THROW("If expr Error", "Unsupported output type Id:" + std::to_string(typeId));
    }
}

// ---- Switch (CASE) expression helpers -------------------------------------
//
// Port of Velox's lazy SwitchExpr evaluation (velox/expression/SwitchExpr.cpp)
// onto the compactor-based evaluator: conditions run only on rows no earlier
// case has matched, results only on the rows their condition selected, and a
// null-propagation fast path skips both on rows where the shared result column
// is null.

/// Follows the FieldExpr input chain to the top-level column index.
int32_t RootFieldCol(const FieldExpr *field)
{
    int32_t rootCol = field->colVal;
    for (const FieldExpr *cur = field; cur->input != nullptr;) {
        cur = dynamic_cast<const FieldExpr *>(cur->input);
        if (cur == nullptr) {
            break;
        }
        rootCol = cur->colVal;
    }
    return rootCol;
}

/// Collects the top-level column indices referenced anywhere in the expression tree.
void CollectFieldCols(const Expr *expr, std::vector<int32_t> &cols)
{
    if (expr == nullptr) {
        return;
    }
    switch (expr->GetType()) {
        case ExprType::FIELD_E:
            cols.push_back(RootFieldCol(static_cast<const FieldExpr *>(expr)));
            return;
        case ExprType::LITERAL_E:
        case ExprType::PARAM_REF_E:
            return;
        case ExprType::UNARY_E:
            CollectFieldCols(static_cast<const UnaryExpr *>(expr)->exp, cols);
            return;
        case ExprType::BINARY_E: {
            const auto *binary = static_cast<const BinaryExpr *>(expr);
            CollectFieldCols(binary->left, cols);
            CollectFieldCols(binary->right, cols);
            return;
        }
        case ExprType::IN_E:
            for (const Expr *arg : static_cast<const InExpr *>(expr)->arguments) {
                CollectFieldCols(arg, cols);
            }
            return;
        case ExprType::BETWEEN_E: {
            const auto *between = static_cast<const BetweenExpr *>(expr);
            CollectFieldCols(between->value, cols);
            CollectFieldCols(between->lowerBound, cols);
            CollectFieldCols(between->upperBound, cols);
            return;
        }
        case ExprType::IF_E: {
            const auto *ifExpr = static_cast<const IfExpr *>(expr);
            CollectFieldCols(ifExpr->condition, cols);
            CollectFieldCols(ifExpr->trueExpr, cols);
            CollectFieldCols(ifExpr->falseExpr, cols);
            return;
        }
        case ExprType::SWITCH_E: {
            const auto *switchExpr = static_cast<const SwitchExpr *>(expr);
            for (const auto &when : switchExpr->whenClause) {
                CollectFieldCols(when.first, cols);
                CollectFieldCols(when.second, cols);
            }
            CollectFieldCols(switchExpr->falseExpr, cols);
            return;
        }
        case ExprType::COALESCE_E:
            for (const Expr *arg : static_cast<const CoalesceExpr *>(expr)->arguments) {
                CollectFieldCols(arg, cols);
            }
            return;
        case ExprType::IS_NULL_E:
            CollectFieldCols(static_cast<const IsNullExpr *>(expr)->value, cols);
            return;
        case ExprType::FUNC_E:
            for (const Expr *arg : static_cast<const FuncExpr *>(expr)->arguments) {
                CollectFieldCols(arg, cols);
            }
            return;
        case ExprType::LAMBDA_E:
            CollectFieldCols(static_cast<const LambdaExpr *>(expr)->body_, cols);
            return;
        default:
            return;
    }
}

/// Collects the field access path from leaf to root (top-level column index
/// last). Null propagation requires all results to be the SAME field path, not
/// just the same root column: two fields of one struct (s.a and s.b) share a
/// root but have independent nulls per row, so sealing rows where s.a is null
/// would be wrong when another branch reads s.b.
void FieldAccessPath(const Expr *expr, std::vector<int32_t> &path)
{
    for (const FieldExpr *cur = static_cast<const FieldExpr *>(expr); cur != nullptr;
         cur = dynamic_cast<const FieldExpr *>(cur->input)) {
        path.push_back(cur->colVal);
    }
}

/// Returns true if expr is a pure field reference (possibly nested) over the
/// shared field; on the first call path is set to the referenced access path.
bool IsSingleFieldExpr(const Expr *expr, std::vector<int32_t> &path)
{
    if (expr == nullptr || expr->GetType() != ExprType::FIELD_E) {
        return false;
    }
    std::vector<int32_t> candidate;
    FieldAccessPath(expr, candidate);
    if (path.empty()) {
        path = candidate;
    }
    return path == candidate;
}

/// Types the EQ(field, literal) fast path can compare with a plain scalar
/// equality (see RunEqFieldConstCase). Complex types and decimal128 keep the
/// general vectorized path.
bool IsSupportedEqFieldConstType(DataTypeId typeId)
{
    switch (typeId) {
        case OMNI_BOOLEAN:
        case OMNI_BYTE:
        case OMNI_SHORT:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_DATE64:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
        case OMNI_FLOAT:
        case OMNI_DOUBLE:
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            return true;
        default:
            return false;
    }
}

/// Cached-on-the-AST analyses for switch evaluation (computed once per
/// expression, not once per batch):
///  - null propagation (Velox's SwitchExpr::computePropagatesNulls): every
///    result and the else clause must be the same single field (identical
///    access path) and every condition must reference only that field's
///    top-level column;
///  - EQ(field, literal) fast path: every condition must compare the same
///    single field to a literal of matching type, so the conditions can run
///    as allocation-free scalar passes over the remaining rows instead of
///    compacted vector evaluations.
void AnalyzeSwitchPatterns(const SwitchExpr &e)
{
    if (e.switchAnalysisDone_) {
        return;
    }
    e.switchAnalysisDone_ = true;
    if (e.whenClause.empty()) {
        return;
    }

    // Null-propagation analysis.
    std::vector<int32_t> path;
    bool nullProp = true;
    for (const auto &when : e.whenClause) {
        if (!IsSingleFieldExpr(when.second, path)) {
            nullProp = false;
            break;
        }
    }
    if (nullProp && !IsSingleFieldExpr(e.falseExpr, path)) {
        nullProp = false;
    }
    if (nullProp) {
        // Conditions may touch any part of the shared field's top-level column:
        // all results are the identical path, so the outcome of a sealed row is
        // fixed regardless of what the conditions say.
        int32_t col = path.back();
        for (const auto &when : e.whenClause) {
            std::vector<int32_t> cols;
            CollectFieldCols(when.first, cols);
            for (int32_t c : cols) {
                if (c != col) {
                    nullProp = false;
                    break;
                }
            }
            if (!nullProp) {
                break;
            }
        }
    }
    if (nullProp) {
        e.propagatesNulls_ = true;
        e.nullFieldCol_ = path.back();
    }

    // EQ(field, literal) fast-path analysis.
    std::vector<int32_t> eqPath;
    bool eqFieldConst = true;
    for (const auto &when : e.whenClause) {
        if (when.first->GetType() != ExprType::BINARY_E) {
            eqFieldConst = false;
            break;
        }
        const auto *binary = static_cast<const BinaryExpr *>(when.first);
        if (binary->op != expressions::Operator::EQ || binary->right == nullptr ||
            binary->right->GetType() != ExprType::LITERAL_E ||
            !IsSingleFieldExpr(binary->left, eqPath) ||
            binary->right->GetReturnTypeId() != binary->left->GetReturnTypeId() ||
            !IsSupportedEqFieldConstType(binary->left->GetReturnTypeId())) {
            eqFieldConst = false;
            break;
        }
    }
    if (eqFieldConst) {
        e.eqFieldConst_ = true;
    }
}

enum class CondMix { kAllTrue, kAllFalse, kMix };

/// Classifies the boolean values of condVec over [0, size). The vector covers
/// exactly the rows the current case condition was evaluated on, in ascending
/// order. For kMix fills trueBits (bit set = condition true, nulls behave as
/// false).
CondMix ClassifyBoolCond(BaseVector *condVec, int32_t size, std::vector<uint64_t> &trueBits)
{
    if (condVec->GetEncoding() == OMNI_ENCODING_CONST) {
        bool isNull = condVec->HasNull() && condVec->IsNull(0);
        if (isNull || !static_cast<ConstVector<bool> *>(condVec)->GetConstValue()) {
            return CondMix::kAllFalse;
        }
        return CondMix::kAllTrue;
    }
    if (condVec->GetEncoding() == OMNI_FLAT && !condVec->HasNull()) {
        bool *values = static_cast<Vector<bool> *>(condVec)->GetValuesBuffer();
        bool allTrue = true;
        bool anyTrue = false;
        for (int32_t i = 0; i < size; ++i) {
            if (values[i]) {
                anyTrue = true;
            } else {
                allTrue = false;
            }
        }
        if (allTrue) {
            return CondMix::kAllTrue;
        }
        if (!anyTrue) {
            return CondMix::kAllFalse;
        }
        trueBits.assign(BitUtil::Nwords(size), 0);
        for (int32_t i = 0; i < size; ++i) {
            if (values[i]) {
                BitUtil::SetBit(trueBits.data(), i);
            }
        }
        return CondMix::kMix;
    }
    // Other encodings or nulls: per-row scan.
    trueBits.assign(BitUtil::Nwords(size), 0);
    bool anyTrue = false;
    for (int32_t i = 0; i < size; ++i) {
        if (!condVec->IsNull(i) && VectorHelper::GetValueFromVector<bool>(condVec, i)) {
            BitUtil::SetBit(trueBits.data(), i);
            anyTrue = true;
        }
    }
    return anyTrue ? CondMix::kMix : CondMix::kAllFalse;
}

/// One EQ(field, literal) fast-path condition: marks the rows of remaining
/// whose field value equals the constant. No vector evaluation, no
/// compaction, no data movement - the field vector is the zero-copy slice
/// evaluated once before the case loop. Nulls never match (SQL semantics).
template <typename T>
void EqFieldConstCase(BaseVector *fieldVec, T constVal, const SelectivityVector &remaining,
    SelectivityVector &thenSel, std::vector<int32_t> &thenPos)
{
    remaining.applyToSelected([&](vector_size_t row) {
        if (fieldVec->IsNull(row)) {
            return;
        }
        T value;
        if constexpr (std::is_same_v<T, std::string_view>) {
            value = VectorHelper::GetStringValueFromVector(fieldVec, row);
        } else {
            value = VectorHelper::GetValueFromVector<T>(fieldVec, row);
        }
        if (value == constVal) {
            thenSel.setValid(row, true);
            thenPos.push_back(row);
        }
    });
    thenSel.updateBounds();
}

/// Dispatches the EQ(field, literal) scalar pass for the field type, reading
/// the constant from the literal expression.
void RunEqFieldConstCase(BaseVector *fieldVec, const LiteralExpr &lit, DataTypeId typeId,
    const SelectivityVector &remaining, SelectivityVector &thenSel, std::vector<int32_t> &thenPos)
{
    switch (typeId) {
        case OMNI_BOOLEAN:
            EqFieldConstCase<bool>(fieldVec, lit.boolVal, remaining, thenSel, thenPos);
            break;
        case OMNI_BYTE:
            EqFieldConstCase<int8_t>(fieldVec, lit.byteVal, remaining, thenSel, thenPos);
            break;
        case OMNI_SHORT:
            EqFieldConstCase<int16_t>(fieldVec, lit.shortVal, remaining, thenSel, thenPos);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            EqFieldConstCase<int32_t>(fieldVec, lit.intVal, remaining, thenSel, thenPos);
            break;
        case OMNI_LONG:
        case OMNI_DATE64:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            EqFieldConstCase<int64_t>(fieldVec, lit.longVal, remaining, thenSel, thenPos);
            break;
        case OMNI_FLOAT:
            EqFieldConstCase<float>(fieldVec, lit.floatVal, remaining, thenSel, thenPos);
            break;
        case OMNI_DOUBLE:
            EqFieldConstCase<double>(fieldVec, lit.doubleVal, remaining, thenSel, thenPos);
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            EqFieldConstCase<std::string_view>(fieldVec, std::string_view(*lit.stringVal), remaining,
                thenSel, thenPos);
            break;
        default:
            OMNI_THROW("Switch expr Error",
                "Unsupported EQ(field, literal) type " + std::to_string(typeId));
    }
}

/// One evaluated result branch of a switch: its vector, the rows it covers in
/// the result space (ascending), and whether the vector covers the whole result
/// space (src row == dst row; the in-place and const evaluation modes).
struct SwitchBranch {
    BaseVector *vec = nullptr;
    std::vector<int32_t> pos;
    bool coversAll = false;
};

/// K-way ordered merge of the case branches into result. The branches partition
/// the result rows (rows outside all branches keep their pre-filled nulls) and
/// every position list is ascending, so a single left-to-right pass with
/// per-branch cursors preserves the ascending fill order that string and array
/// containers rely on.
template <typename T>
void MergeSwitchTyped(const std::vector<SwitchBranch> &branches, BaseVector *result, int32_t n)
{
    auto *resVec = static_cast<Vector<T> *>(result);
    std::vector<int32_t> cursor(branches.size(), 0);
    for (int32_t dst = 0; dst < n; ++dst) {
        int32_t branchIdx = -1;
        for (size_t b = 0; b < branches.size(); ++b) {
            if (cursor[b] < static_cast<int32_t>(branches[b].pos.size()) && branches[b].pos[cursor[b]] == dst) {
                branchIdx = static_cast<int32_t>(b);
                break;
            }
        }
        if (branchIdx < 0) {
            continue;
        }
        const SwitchBranch &branch = branches[branchIdx];
        bool isConst = branch.vec->GetEncoding() == OMNI_ENCODING_CONST;
        int32_t srcRow = isConst ? 0 : (branch.coversAll ? dst : cursor[branchIdx]);
        if (branch.vec->IsNull(srcRow)) {
            resVec->SetNull(dst);
        } else {
            resVec->SetValue(dst, VectorHelper::GetValueFromVector<T>(branch.vec, srcRow));
        }
        ++cursor[branchIdx];
    }
}

void MergeSwitchString(const std::vector<SwitchBranch> &branches, BaseVector *result, int32_t n)
{
    auto *resVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    std::vector<int32_t> cursor(branches.size(), 0);
    for (int32_t dst = 0; dst < n; ++dst) {
        int32_t branchIdx = -1;
        for (size_t b = 0; b < branches.size(); ++b) {
            if (cursor[b] < static_cast<int32_t>(branches[b].pos.size()) && branches[b].pos[cursor[b]] == dst) {
                branchIdx = static_cast<int32_t>(b);
                break;
            }
        }
        if (branchIdx < 0) {
            continue;
        }
        const SwitchBranch &branch = branches[branchIdx];
        bool isConst = branch.vec->GetEncoding() == OMNI_ENCODING_CONST;
        int32_t srcRow = isConst ? 0 : (branch.coversAll ? dst : cursor[branchIdx]);
        if (branch.vec->IsNull(srcRow)) {
            resVec->SetNull(dst);
        } else {
            resVec->SetValue(dst, VectorHelper::GetStringValueFromVector(branch.vec, srcRow));
        }
        ++cursor[branchIdx];
    }
}

void MergeSwitchComplex(const std::vector<SwitchBranch> &branches, BaseVector *result, int32_t n, DataTypeId typeId)
{
    std::vector<int32_t> cursor(branches.size(), 0);
    for (int32_t dst = 0; dst < n; ++dst) {
        int32_t branchIdx = -1;
        for (size_t b = 0; b < branches.size(); ++b) {
            if (cursor[b] < static_cast<int32_t>(branches[b].pos.size()) && branches[b].pos[cursor[b]] == dst) {
                branchIdx = static_cast<int32_t>(b);
                break;
            }
        }
        if (branchIdx < 0) {
            continue;
        }
        const SwitchBranch &branch = branches[branchIdx];
        bool isConst = branch.vec->GetEncoding() == OMNI_ENCODING_CONST;
        int32_t srcRow = isConst ? 0 : (branch.coversAll ? dst : cursor[branchIdx]);
        if (branch.vec->IsNull(srcRow)) {
            if (typeId == OMNI_ARRAY) {
                static_cast<ArrayVector *>(result)->SetNull(dst);
            } else if (typeId == OMNI_MAP) {
                static_cast<MapVector *>(result)->SetNull(dst);
            } else {
                result->SetNull(dst);
            }
            ++cursor[branchIdx];
            continue;
        }
        if (typeId == OMNI_ARRAY) {
            IfFunction::CopyArrayValue(branch.vec, result, srcRow, dst);
        } else if (typeId == OMNI_MAP) {
            IfFunction::CopyMapValue(branch.vec, result, srcRow, dst);
        } else {
            IfFunction::CopyRowValue(branch.vec, result, srcRow, dst);
        }
        ++cursor[branchIdx];
    }
}

void MergeSwitchBranches(const std::vector<SwitchBranch> &branches, BaseVector *result, int32_t n, DataTypeId typeId)
{
    switch (typeId) {
        case OMNI_BOOLEAN:
            MergeSwitchTyped<bool>(branches, result, n);
            break;
        case OMNI_BYTE:
            MergeSwitchTyped<int8_t>(branches, result, n);
            break;
        case OMNI_SHORT:
            MergeSwitchTyped<int16_t>(branches, result, n);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            MergeSwitchTyped<int32_t>(branches, result, n);
            break;
        case OMNI_LONG:
        case OMNI_DATE64:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            MergeSwitchTyped<int64_t>(branches, result, n);
            break;
        case OMNI_FLOAT:
            MergeSwitchTyped<float>(branches, result, n);
            break;
        case OMNI_DOUBLE:
            MergeSwitchTyped<double>(branches, result, n);
            break;
        case OMNI_DECIMAL128:
            MergeSwitchTyped<Decimal128>(branches, result, n);
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            MergeSwitchString(branches, result, n);
            break;
        case OMNI_ARRAY:
        case OMNI_MAP:
        case OMNI_ROW:
            MergeSwitchComplex(branches, result, n, typeId);
            break;
        default:
            OMNI_THROW("Switch expr Error", "Unsupported output type Id:" + std::to_string(typeId));
    }
}

// ---- Lazy AND/OR (conjunct) evaluation helpers ------------------------------
//
// Port of Velox's lazy ConjunctExpr evaluation onto the compactor-based
// evaluator: each conjunct runs only on the rows no earlier conjunct has
// decided, AND drops rows on FALSE, OR drops rows on TRUE, and NULLs taint a
// row without deciding it (three-valued logic, identical to the eager
// AndFunction/OrFunction in Comparisons.h). Operands are ordered by estimated
// cost so cheap, decisive conjuncts shrink the space before expensive ones
// run. See lazy_conjunct_optimization.md.

enum class ConjMix { kAllTrue, kAllFalse, kAllNull, kMix };

/// Classifies the boolean values of conjVec over [0, size) in ascending
/// order. Unlike ClassifyBoolCond, NULLs are distinguished from FALSE: a NULL
/// conjunct taints a row without deciding it, while FALSE (AND) / TRUE (OR)
/// decides it. For kMix fills trueBits (bit set = value true).
ConjMix ClassifyConjunct(BaseVector *conjVec, int32_t size, std::vector<uint64_t> &trueBits)
{
    if (conjVec->GetEncoding() == OMNI_ENCODING_CONST) {
        bool isNull = conjVec->HasNull() && conjVec->IsNull(0);
        if (isNull) {
            return ConjMix::kAllNull;
        }
        return static_cast<ConstVector<bool> *>(conjVec)->GetConstValue() ? ConjMix::kAllTrue : ConjMix::kAllFalse;
    }
    if (conjVec->GetEncoding() == OMNI_FLAT && !conjVec->HasNull()) {
        // GetValuesBuffer returns the raw buffer start; sliced vectors carry
        // their start in the offset, so index by logical row + offset.
        bool *values = static_cast<Vector<bool> *>(conjVec)->GetValuesBuffer() + conjVec->GetOffset();
        bool allTrue = true;
        bool allFalse = true;
        for (int32_t i = 0; i < size; ++i) {
            if (values[i]) {
                allFalse = false;
            } else {
                allTrue = false;
            }
        }
        if (allTrue) {
            return ConjMix::kAllTrue;
        }
        if (allFalse) {
            return ConjMix::kAllFalse;
        }
        trueBits.assign(BitUtil::Nwords(size), 0);
        for (int32_t i = 0; i < size; ++i) {
            if (values[i]) {
                BitUtil::SetBit(trueBits.data(), i);
            }
        }
        return ConjMix::kMix;
    }
    // Other encodings or nulls: per-row scan resolving through the vector.
    trueBits.assign(BitUtil::Nwords(size), 0);
    bool anyTrue = false;
    bool anyFalse = false;
    bool anyNull = false;
    for (int32_t i = 0; i < size; ++i) {
        if (conjVec->IsNull(i)) {
            anyNull = true;
            continue;
        }
        if (VectorHelper::GetValueFromVector<bool>(conjVec, i)) {
            BitUtil::SetBit(trueBits.data(), i);
            anyTrue = true;
        } else {
            anyFalse = true;
        }
    }
    // The uniform classes must be pure: a false+null mix is kMix, because the
    // NULL rows still need tainting (FALSE OR NULL = NULL, NULL AND FALSE
    // differs from FALSE when a later conjunct decides).
    if (!anyTrue && anyFalse && !anyNull) {
        return ConjMix::kAllFalse;
    }
    if (anyTrue && !anyFalse && !anyNull) {
        return ConjMix::kAllTrue;
    }
    if (!anyTrue && !anyFalse && anyNull) {
        return ConjMix::kAllNull;
    }
    return ConjMix::kMix;
}

/// One operand of a flattened AND/OR chain, with its ordering metadata.
struct ConjunctOperand {
    const Expr *expr = nullptr;
    uint64_t cost = 0;
    enum class Kind {
        kNormal,         // evaluate + merge per row
        kDecidingConst,  // const-FALSE for AND / const-TRUE for OR: decides the whole chain
        kNullConst       // const NULL: taints every active row, never decides
    } kind = Kind::kNormal;
};

/// Appends one operand, folding boolean constants immediately. An identity
/// constant (TRUE for AND / FALSE for OR) is dropped entirely.
void AppendConjunct(const Expr *expr, omniruntime::expressions::Operator chainOp, std::vector<ConjunctOperand> &out)
{
    if (expr->GetType() == ExprType::LITERAL_E) {
        const auto *lit = static_cast<const LiteralExpr *>(expr);
        if (lit->isNull) {
            out.push_back({expr, 0, ConjunctOperand::Kind::kNullConst});
            return;
        }
        bool decides = (chainOp == omniruntime::expressions::Operator::AND) ? !lit->boolVal : lit->boolVal;
        if (decides) {
            out.push_back({expr, 0, ConjunctOperand::Kind::kDecidingConst});
        }
        return;
    }
    out.push_back({expr, EstimateConjunctCost(expr), ConjunctOperand::Kind::kNormal});
}

/// Flattens a same-op chain into its operand list in left-to-right order.
/// Children with a different operator (or any non-binary node) are single
/// operands and are left untouched; they run their own lazy chain when the
/// loop evaluates them.
void CollectConjuncts(const BinaryExpr &e, omniruntime::expressions::Operator chainOp,
    std::vector<ConjunctOperand> &out)
{
    if (e.left->GetType() == ExprType::BINARY_E && static_cast<const BinaryExpr *>(e.left)->op == chainOp) {
        CollectConjuncts(*static_cast<const BinaryExpr *>(e.left), chainOp, out);
    } else {
        AppendConjunct(e.left, chainOp, out);
    }
    if (e.right->GetType() == ExprType::BINARY_E && static_cast<const BinaryExpr *>(e.right)->op == chainOp) {
        CollectConjuncts(*static_cast<const BinaryExpr *>(e.right), chainOp, out);
    } else {
        AppendConjunct(e.right, chainOp, out);
    }
}

/// Deciding constants first (they end the chain before any evaluation), then
/// ascending estimated cost with a stable sort (equal cost keeps plan order,
/// so evaluation stays deterministic), null constants last (they only taint).
void OrderConjuncts(std::vector<ConjunctOperand> &ops)
{
    std::stable_sort(ops.begin(), ops.end(), [](const ConjunctOperand &a, const ConjunctOperand &b) {
        auto rank = [](const ConjunctOperand &o) {
            return o.kind == ConjunctOperand::Kind::kDecidingConst ? 0 :
                (o.kind == ConjunctOperand::Kind::kNullConst ? 2 : 1);
        };
        if (rank(a) != rank(b)) {
            return rank(a) < rank(b);
        }
        return a.kind == ConjunctOperand::Kind::kNormal && b.kind == ConjunctOperand::Kind::kNormal &&
            a.cost < b.cost;
    });
}

/// Operands cheaper than this (comparisons cost 1-5, arithmetic 2-3, casts 3,
/// bare reads 0 — see EstimateConjunctCost) leave the ordering nothing to
/// defer, so the lazy machinery cannot pay for itself. Regex/like/func
/// operands cost 10-100 and keep the lazy path.
constexpr uint64_t kCheapConjunctBailoutCost = 5;

bool AllConjunctsCheap(const std::vector<ConjunctOperand> &ops)
{
    for (const ConjunctOperand &op : ops) {
        if (op.cost > kCheapConjunctBailoutCost) {
            return false;
        }
    }
    return true;
}

/// Projects the given rows of a vector into a new compacted vector. The same
/// per-type dispatch as PushProjectedColumn, but the result is returned
/// instead of pushed (used by the lazy coalesce to align branch vectors with
/// their position lists).
BaseVector *ProjectRows(BaseVector *srcVec, DataTypeId typeId, const int32_t *rows, int32_t count)
{
    switch (typeId) {
        case OMNI_INT:
        case OMNI_DATE32:
            return ColumnProjectionCopyPositionsHelper<int32_t>(srcVec, rows, count);
        case OMNI_SHORT:
            return ColumnProjectionCopyPositionsHelper<int16_t>(srcVec, rows, count);
        case OMNI_BYTE:
            return ColumnProjectionCopyPositionsHelper<int8_t>(srcVec, rows, count);
        case OMNI_LONG:
        case OMNI_DATE64:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return ColumnProjectionCopyPositionsHelper<int64_t>(srcVec, rows, count);
        case OMNI_DOUBLE:
            return ColumnProjectionCopyPositionsHelper<double>(srcVec, rows, count);
        case OMNI_FLOAT:
            return ColumnProjectionCopyPositionsHelper<float>(srcVec, rows, count);
        case OMNI_BOOLEAN:
            return ColumnProjectionCopyPositionsHelper<bool>(srcVec, rows, count);
        case OMNI_DECIMAL128:
            return ColumnProjectionCopyPositionsHelper<Decimal128>(srcVec, rows, count);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            return ColumnProjectionVarCharCopyPositionsHelper<std::string_view>(srcVec, rows, count);
        case OMNI_STRING_VIEW:
            return ColumnProjectionCopyPositionsHelper<StringView>(srcVec, rows, count);
        case OMNI_ARRAY:
            return reinterpret_cast<ArrayVector *>(srcVec)->CopyPositions(rows, 0, count);
        case OMNI_MAP:
            return reinterpret_cast<MapVector *>(srcVec)->CopyPositions(rows, 0, count);
        case OMNI_ROW:
            return reinterpret_cast<RowVector *>(srcVec)->CopyPositions(rows, 0, count);
        default:
            OMNI_THROW("Coalesce expr Error", "Unsupported coalesce type Id:" + std::to_string(typeId));
    }
}
} // namespace

void ExprEval::VisitLazyLogical(const BinaryExpr &e)
{
    const bool isAnd = e.op == expressions::Operator::AND;

    // Zero-overhead case: two bare flat boolean columns and no outer filter —
    // the original eager path through AndFunction/OrFunction. The eager merge
    // is only safe for flat inputs: it reads values through the 3-argument
    // GetValueFromVector, which ignores nulls on constants and throws on
    // dictionary encodings, and literals are folded by the lazy loop instead
    // (a NULL literal must taint, not evaluate as FALSE). Under an outer
    // filter the eager merge would size itself off the full row count while
    // the leaves project only the surviving rows, so Act 0's compacted space
    // is required there.
    if (e.vectorFunction != nullptr && IsFlatBoolColumnOperand(e.left) && IsFlatBoolColumnOperand(e.right) &&
        !(activeSel_ == nullptr && context->hasFilter)) {
        e.left->Accept(*this);
        e.right->Accept(*this);
        BaseVector *eagerResult = nullptr;
        e.vectorFunction->Apply(inputValues_, e.dataType, eagerResult, context);
        inputValues_.push(eagerResult);
        return;
    }

    // Act 0: under an outer filter the current rows are the filter-selected
    // rows scattered over the full batch; convert them into an explicit
    // compacted space first (same as Visit(IfExpr) / Visit(SwitchExpr)).
    SelectivityVector outerSel;
    std::vector<int32_t> outerPos;
    std::unique_ptr<ScopedCompactEval> outerGuard;
    if (activeSel_ == nullptr && context->hasFilter && context->GetResultRowSize() == rowSize) {
        auto *mask = context->GetIsSelectRow();
        int32_t fullSize = context->GetResultRowSize();
        outerSel = SelectivityVector(fullSize, false);
        outerPos.reserve(fullSize);
        for (int32_t i = 0; i < fullSize; ++i) {
            if (mask[i]) {
                outerSel.setValid(i, true);
                outerPos.push_back(i);
            }
        }
        outerSel.updateBounds();
        outerGuard = std::make_unique<ScopedCompactEval>(*this, outerSel, outerPos);
    }

    int32_t n = CurrentRowCount();

    // Acts 1+2: flatten the same-op chain and order the operands.
    std::vector<ConjunctOperand> operands;
    CollectConjuncts(e, e.op, operands);

    // Cheap-chain bail-out: when every operand is cheap there is nothing the
    // cost ordering can meaningfully defer, so the shrinking-row machinery
    // only adds per-conjunct classification scans and selectivity churn.
    // Evaluate every operand once over the whole current space — the exact
    // eager cost — and fold the chain with one Kleene merge (FALSE decides
    // an AND row, TRUE decides an OR row, NULL taints without deciding).
    // Bit-identical to the lazy loop below and to AndFunction/OrFunction;
    // safe in both error modes because nothing is skipped. Motivated by q74
    // in the weekend1 TPCDS run: a 9-way AND of join-key equalities (all
    // cost 1-5) regressed +15.3s vs the eager switch build
    // (tpcds_dip_investigation.md).
    if (AllConjunctsCheap(operands)) {
        BaseVector *eagerResult = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, n);
        auto *eagerVec = static_cast<Vector<bool> *>(eagerResult);
        std::vector<uint64_t> decided(BitUtil::Nwords(n), 0);
        std::vector<uint64_t> taint(BitUtil::Nwords(n), 0);
        if (n > 0) {
            memset(unsafe::UnsafeVector::GetRawValues(eagerVec), isAnd ? 1 : 0, static_cast<size_t>(n));
        }
        SelectivityVector allSel(n, n > 0);
        if (n > 0) {
            for (const ConjunctOperand &operand : operands) {
                BaseVector *conjVec = EvaluateBranch(operand.expr, allSel, true);
                if (conjVec->GetEncoding() == OMNI_ENCODING_CONST) {
                    bool isNull = conjVec->HasNull() && conjVec->IsNull(0);
                    if (!isNull && static_cast<ConstVector<bool> *>(conjVec)->GetConstValue() != isAnd) {
                        // Deciding constant: every row takes the deciding value, no nulls.
                        memset(unsafe::UnsafeVector::GetRawValues(eagerVec), isAnd ? 0 : 1, static_cast<size_t>(n));
                        memset(decided.data(), 0xFF, BitUtil::Nwords(n) * sizeof(uint64_t));
                        delete conjVec;
                        break;
                    }
                    if (isNull) {
                        memset(taint.data(), 0xFF, BitUtil::Nwords(n) * sizeof(uint64_t));
                    }
                    delete conjVec;
                    continue;
                }
                if (conjVec->GetEncoding() == OMNI_FLAT) {
                    bool *values = static_cast<Vector<bool> *>(conjVec)->GetValuesBuffer() + conjVec->GetOffset();
                    for (int32_t i = 0; i < n; ++i) {
                        if (conjVec->IsNull(i)) {
                            BitUtil::SetBit(taint.data(), i);
                        } else if (values[i] != isAnd) {
                            eagerVec->SetValue(i, !isAnd);
                            BitUtil::SetBit(decided.data(), i);
                        }
                    }
                } else {
                    for (int32_t i = 0; i < n; ++i) {
                        if (conjVec->IsNull(i)) {
                            BitUtil::SetBit(taint.data(), i);
                        } else if (VectorHelper::GetValueFromVector<bool>(conjVec, i) != isAnd) {
                            eagerVec->SetValue(i, !isAnd);
                            BitUtil::SetBit(decided.data(), i);
                        }
                    }
                }
                delete conjVec;
            }
        }
        // Rows that stayed undecided but were tainted by a NULL conjunct are
        // NULL (FALSE/TRUE would have decided them).
        for (int32_t i = 0; i < n; ++i) {
            if (!BitUtil::IsBitSet(decided.data(), i) && BitUtil::IsBitSet(taint.data(), i)) {
                eagerResult->SetNull(i);
            }
        }
        inputValues_.push(eagerResult);
        return;
    }

    OrderConjuncts(operands);

    // Result starts at the operator's identity value with no nulls: all TRUE
    // for AND, all FALSE for OR. Rows only change when a conjunct decides
    // them or taints them (Act 4). The values buffer is not zero-initialized,
    // so the identity must be filled explicitly.
    BaseVector *result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, n);
    auto *resVec = static_cast<Vector<bool> *>(result);
    if (n > 0) {
        memset(unsafe::UnsafeVector::GetRawValues(resVec), isAnd ? 1 : 0, static_cast<size_t>(n));
    }

    // Act 3: run each conjunct only on the rows no earlier conjunct decided.
    SelectivityVector active(n, n > 0);
    std::vector<uint64_t> taint(BitUtil::Nwords(n), 0);   // active rows hit by a NULL conjunct
    std::vector<uint64_t> trueBits;

    for (const ConjunctOperand &operand : operands) {
        if (!active.hasSelections()) {
            break;
        }
        switch (operand.kind) {
            case ConjunctOperand::Kind::kDecidingConst:
                // const-FALSE for AND / const-TRUE for OR decides every active row.
                active.applyToSelected([&](vector_size_t row) { resVec->SetValue(row, !isAnd); });
                active.clearAll();
                continue;
            case ConjunctOperand::Kind::kNullConst:
                active.applyToSelected([&](vector_size_t row) { BitUtil::SetBit(taint.data(), row); });
                continue;
            default:
                break;
        }

        int32_t selCount = active.countSelected();
        bool cheap = IsCheapIfBranch(operand.expr);
        BaseVector *conjVec = EvaluateBranch(operand.expr, active, cheap);
        bool coversAll = cheap || selCount == n;
        ConjMix mix = ClassifyConjunct(conjVec, coversAll ? n : selCount, trueBits);

        switch (mix) {
            case ConjMix::kAllTrue:
                if (!isAnd) {
                    active.applyToSelected([&](vector_size_t row) { resVec->SetValue(row, true); });
                    active.clearAll();
                }
                break;
            case ConjMix::kAllFalse:
                if (isAnd) {
                    active.applyToSelected([&](vector_size_t row) { resVec->SetValue(row, false); });
                    active.clearAll();
                }
                break;
            case ConjMix::kAllNull:
                active.applyToSelected([&](vector_size_t row) { BitUtil::SetBit(taint.data(), row); });
                break;
            case ConjMix::kMix: {
                SelectivityVector dropSel(n, false);
                bool anyDrop = false;
                int32_t j = 0;
                active.applyToSelected([&](vector_size_t row) {
                    int32_t src = coversAll ? row : j;
                    ++j;
                    if (conjVec->IsNull(src)) {
                        BitUtil::SetBit(taint.data(), row);
                    } else if (BitUtil::IsBitSet(trueBits.data(), src) != isAnd) {
                        // FALSE decides an AND row, TRUE decides an OR row.
                        dropSel.setValid(row, true);
                        anyDrop = true;
                    }
                });
                if (anyDrop) {
                    dropSel.updateBounds();
                    dropSel.applyToSelected([&](vector_size_t row) { resVec->SetValue(row, !isAnd); });
                    active.deselect(dropSel);
                }
                break;
            }
        }
        delete conjVec;
    }

    // Act 4: rows that stayed undecided but were tainted by a NULL conjunct
    // are NULL (FALSE/TRUE would have decided them).
    active.applyToSelected([&](vector_size_t row) {
        if (BitUtil::IsBitSet(taint.data(), row)) {
            result->SetNull(row);
        }
    });

    inputValues_.push(result);
}

void ExprEval::Visit(const IfExpr &e)
{
    if (e.vectorFunction == nullptr) {
        OMNI_THROW("Vectorization Error:", "Vector function not found for if expression");
    }

    // Lazy evaluation: the condition is evaluated for every row, then each branch is
    // evaluated only for the rows that select it. Rows with a NULL condition take the
    // false branch (same semantics as IfFunction::Apply).
    //
    // Under an outer filter the current rows are the filter-selected rows scattered
    // over the full batch; convert them into an explicit compacted space first so the
    // condition and both branches agree on sizes and positions. The filter mask is
    // defined over the batch row space only: when the current space differs (lambda
    // evaluation keeps an inherited hasFilter but sizes the space by element count),
    // it must not be applied.
    SelectivityVector outerSel;
    std::vector<int32_t> outerPos;
    std::unique_ptr<ScopedCompactEval> outerGuard;
    if (activeSel_ == nullptr && context->hasFilter && context->GetResultRowSize() == rowSize) {
        auto *mask = context->GetIsSelectRow();
        int32_t fullSize = context->GetResultRowSize();
        outerSel = SelectivityVector(fullSize, false);
        outerPos.reserve(fullSize);
        for (int32_t i = 0; i < fullSize; ++i) {
            if (mask[i]) {
                outerSel.setValid(i, true);
                outerPos.push_back(i);
            }
        }
        outerSel.updateBounds();
        outerGuard = std::make_unique<ScopedCompactEval>(*this, outerSel, outerPos);
    }

    int32_t n = CurrentRowCount();

    // 1. Evaluate only the condition.
    e.condition->Accept(*this);
    BaseVector *condVec = inputValues_.top();
    inputValues_.pop();

    // 2. Build the selectivity vectors of the true and false branches.
    SelectivityVector trueSel(n, false);
    SelectivityVector falseSel(n, false);
    if (condVec->GetEncoding() == OMNI_ENCODING_CONST) {
        bool condIsNull = condVec->HasNull() && condVec->IsNull(0);
        bool condValue = !condIsNull && static_cast<ConstVector<bool> *>(condVec)->GetConstValue();
        (condValue ? trueSel : falseSel).setAll();
    } else {
        for (int32_t i = 0; i < n; ++i) {
            if (condVec->IsNull(i) || !VectorHelper::GetValueFromVector<bool>(condVec, i)) {
                falseSel.setValid(i, true);
            } else {
                trueSel.setValid(i, true);
            }
        }
        trueSel.updateBounds();
        falseSel.updateBounds();
    }

    // 3. Evaluate each branch only for the rows that select it and merge the results.
    BaseVector *result = nullptr;
    if (!trueSel.hasSelections()) {
        result = EvaluateBranch(e.falseExpr, falseSel, false);
    } else if (!falseSel.hasSelections()) {
        result = EvaluateBranch(e.trueExpr, trueSel, false);
    } else {
        // Cheap (leaf-only) branches are evaluated over the whole current space:
        // their evaluation is zero-copy (Slice) and beats gathering the selected
        // rows. Expensive branches are compacted so they run only on selected rows.
        bool trueCheap = IsCheapIfBranch(e.trueExpr);
        bool falseCheap = IsCheapIfBranch(e.falseExpr);
        BaseVector *trueVec = EvaluateBranch(e.trueExpr, trueSel, trueCheap);
        BaseVector *falseVec = EvaluateBranch(e.falseExpr, falseSel, falseCheap);

        if (trueCheap && falseCheap && condVec->GetEncoding() == OMNI_FLAT) {
            // Leaf-only branches: merge through IfFunction, which avoids the
            // position materialization and has a tight loop for flat null-free
            // inputs. It consumes and frees its three arguments.
            inputValues_.push(condVec);
            inputValues_.push(trueVec);
            inputValues_.push(falseVec);
            e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
            inputValues_.push(result);
            return;
        }

        IfFunction::CreateResultVector(e.dataType->GetId(), n, trueVec, result);
        std::vector<int32_t> truePos;
        std::vector<int32_t> falsePos;
        trueSel.applyToSelected([&](vector_size_t i) { truePos.push_back(i); });
        falseSel.applyToSelected([&](vector_size_t i) { falsePos.push_back(i); });
        MergeIfBranches(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n,
            e.dataType->GetId());
        delete trueVec;
        delete falseVec;
    }
    delete condVec;
    inputValues_.push(result);
}

void ExprEval::Visit(const CoalesceExpr &e)
{
    // Lazy evaluation (Velox's CoalesceExpr::evalSpecialForm): each argument
    // runs only on the rows where every earlier argument was NULL. A non-null
    // value decides the row (first non-null wins), rows still NULL after all
    // arguments stay NULL. Gated off in throw-on-error mode, where skipping
    // arguments for decided rows could change which rows surface errors —
    // the same contract as lazy AND/OR.
    if (!context->ThrowOnError() && !e.arguments.empty()) {
        // Act 0: under an outer filter, materialize the surviving rows into an
        // explicit compacted space first (same as Visit(IfExpr)/Visit(SwitchExpr)).
        SelectivityVector outerSel;
        std::vector<int32_t> outerPos;
        std::unique_ptr<ScopedCompactEval> outerGuard;
        if (activeSel_ == nullptr && context->hasFilter && context->GetResultRowSize() == rowSize) {
            auto *mask = context->GetIsSelectRow();
            int32_t fullSize = context->GetResultRowSize();
            outerSel = SelectivityVector(fullSize, false);
            outerPos.reserve(fullSize);
            for (int32_t i = 0; i < fullSize; ++i) {
                if (mask[i]) {
                    outerSel.setValid(i, true);
                    outerPos.push_back(i);
                }
            }
            outerSel.updateBounds();
            outerGuard = std::make_unique<ScopedCompactEval>(*this, outerSel, outerPos);
        }

        int32_t n = CurrentRowCount();
        SelectivityVector remaining(n, n > 0);
        std::vector<SwitchBranch> branches;
        BaseVector *result = nullptr;

        for (const Expr *arg : e.arguments) {
            if (!remaining.hasSelections()) {
                break;
            }
            int32_t selCount = remaining.countSelected();
            bool cheap = IsCheapIfBranch(arg);
            BaseVector *argVec = EvaluateBranch(arg, remaining, cheap);
            bool coversAll = cheap || selCount == n;

            // Rows where this argument is non-null are decided by it. Collect
            // both the decided rows (current-space) and their source rows in
            // argVec (row itself when the vector covers the whole space, the
            // compacted index otherwise) so the decided values can be
            // extracted into a branch vector aligned with the position list.
            SelectivityVector decidedSel(n, false);
            std::vector<int32_t> decidedPos;
            std::vector<int32_t> decidedSrc;
            if (coversAll) {
                remaining.applyToSelected([&](vector_size_t row) {
                    if (!argVec->IsNull(row)) {
                        decidedSel.setValid(row, true);
                        decidedPos.push_back(row);
                        decidedSrc.push_back(row);
                    }
                });
            } else {
                int32_t j = 0;
                remaining.applyToSelected([&](vector_size_t row) {
                    if (!argVec->IsNull(j)) {
                        decidedSel.setValid(row, true);
                        decidedPos.push_back(row);
                        decidedSrc.push_back(j);
                    }
                    ++j;
                });
            }
            if (!decidedPos.empty()) {
                decidedSel.updateBounds();
                // The merge helpers read a branch vector at its own position
                // cursor. argVec is already cursor-aligned when it covers the
                // whole space (coversAll: src = dst row) or when every
                // evaluated row was decided (the branch positions are exactly
                // the evaluation set in ascending order). Only a proper
                // subset needs extraction into a compacted branch vector —
                // the gather is skipped for the common full-decide shapes
                // (e.g. coalesce(col, 0) over almost-non-null columns).
                BaseVector *branchVec = argVec;
                bool branchCoversAll = coversAll;
                if (!coversAll && static_cast<int32_t>(decidedPos.size()) < selCount) {
                    branchVec = ProjectRows(argVec, e.dataType->GetId(), decidedSrc.data(),
                        static_cast<int32_t>(decidedSrc.size()));
                    delete argVec;
                }
                if (result == nullptr) {
                    IfFunction::CreateResultVector(e.dataType->GetId(), n, branchVec, result);
                }
                branches.push_back({branchVec, std::move(decidedPos), branchCoversAll});
                remaining.deselect(decidedSel);
            } else {
                delete argVec;
            }
        }

        if (result == nullptr) {
            // Every argument was NULL for every row: create a typed all-null
            // result from the first argument's shape.
            SelectivityVector allSel(n, n > 0);
            BaseVector *sample = EvaluateBranch(e.arguments[0], allSel, false);
            IfFunction::CreateResultVector(e.dataType->GetId(), n, sample, result);
            delete sample;
        }
        // The k-way merge visits rows ascending and takes the first branch
        // covering each row — first non-null argument wins, exactly coalesce.
        MergeSwitchBranches(branches, result, n, e.dataType->GetId());
        remaining.applyToSelected([&](vector_size_t row) { result->SetNull(row); });
        for (const SwitchBranch &branch : branches) {
            delete branch.vec;
        }
        inputValues_.push(result);
        return;
    }

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
    auto result = VectorHelper::CreateFlatVector(e.dataType->GetId(), CurrentRowCount());
    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

bool ExprEval::TryEvaluateMd5ConcatWsFusion(const FuncExpr &e)
{
    const auto fusionPlan = Md5ConcatWsFusion::Match(e);
    if (fusionPlan.has_value()) {
        std::vector<DataTypeId> fusedArgTypes(fusionPlan->concatWs->arguments.size());
        std::transform(fusionPlan->concatWs->arguments.begin(), fusionPlan->concatWs->arguments.end(),
            fusedArgTypes.begin(), [](Expr *expr) -> DataTypeId { return expr->GetReturnTypeId(); });
        auto fusedSignature = std::make_shared<codegen::FunctionSignature>(
            fusionPlan->fusedFunctionName, fusedArgTypes, e.dataType->GetId());
        auto fusedFunction = VectorFunction::Find(fusedSignature);
        if (fusedFunction != nullptr) {
            for (Expr *argument : fusionPlan->concatWs->arguments) {
                argument->Accept(*this);
            }
            BaseVector *fusedResult = nullptr;
            fusedFunction->Apply(inputValues_, e.dataType, fusedResult, context);
            inputValues_.push(fusedResult);
            return true;
        }
    }
    return false;
}

void ExprEval::Visit(const FuncExpr &e)
{
    if (TryEvaluateMd5ConcatWsFusion(e)) {
        return;
    }

    for (auto arg : e.arguments) {
        arg->Accept(*this);
    }
    if (e.funcName == "array" || e.funcName == "name_struct") {
        // name_struct must consume only this call's arguments. Parent comparisons
        // leave the other operand on the same stack.
        context->setInputParamsNUms(static_cast<int32_t>(e.arguments.size()));
    }
    BaseVector *result = nullptr;
    if (e.vectorFunction == nullptr) {
        // vectorFunction may be null when VectorFunction registry was not yet initialized
        // at FuncExpr construction time. Try a lazy lookup now.
        std::vector<DataTypeId> argTypes(e.arguments.size());
        std::transform(e.arguments.begin(), e.arguments.end(), argTypes.begin(),
            [](Expr *expr) -> DataTypeId { return expr->GetReturnTypeId(); });
        auto signature = std::make_shared<codegen::FunctionSignature>(e.funcName, argTypes, e.dataType->GetId());
        auto resolved = VectorFunction::Find(signature);
        if (resolved != nullptr) {
            const_cast<FuncExpr &>(e).vectorFunction = resolved;
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

    e.vectorFunction->Apply(inputValues_, e.dataType, result, context);
    inputValues_.push(result);
}

void ExprEval::Visit(const SwitchExpr &e)
{
    // Lazy evaluation of CASE (Velox-style SwitchExpr::evalSpecialForm): each
    // condition runs only on the rows no earlier case has matched, and each
    // result only on the rows its condition selected. Rows with a NULL
    // condition do not match, so they keep falling through to later cases and
    // finally the else clause.

    // Under an outer filter the current rows are the filter-selected rows
    // scattered over the full batch; convert them into an explicit compacted
    // space first (same as Visit(IfExpr)). The mask is defined over the batch
    // row space only; see Visit(IfExpr) for why it is skipped otherwise.
    SelectivityVector outerSel;
    std::vector<int32_t> outerPos;
    std::unique_ptr<ScopedCompactEval> outerGuard;
    if (activeSel_ == nullptr && context->hasFilter && context->GetResultRowSize() == rowSize) {
        auto *mask = context->GetIsSelectRow();
        int32_t fullSize = context->GetResultRowSize();
        outerSel = SelectivityVector(fullSize, false);
        outerPos.reserve(fullSize);
        for (int32_t i = 0; i < fullSize; ++i) {
            if (mask[i]) {
                outerSel.setValid(i, true);
                outerPos.push_back(i);
            }
        }
        outerSel.updateBounds();
        outerGuard = std::make_unique<ScopedCompactEval>(*this, outerSel, outerPos);
    }

    int32_t n = CurrentRowCount();

    // Rows not matched by any case yet, in current-space coordinates.
    SelectivityVector remaining(n, n > 0);

    AnalyzeSwitchPatterns(e);

    BaseVector *result = nullptr;
    if (e.propagatesNulls_ && n > 0) {
        // Null-propagation fast path: every result and the else clause are the
        // shared column, so rows where it is null are null in the result and no
        // condition or result ever needs to run on them.
        SelectivityVector allSel(n, true);
        BaseVector *fieldVec = EvaluateBranch(e.whenClause[0].second, allSel, true);
        IfFunction::CreateResultVector(e.dataType->GetId(), n, fieldVec, result);
        if (fieldVec->HasNull()) {
            auto *nulls = reinterpret_cast<const uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(fieldVec));
            remaining.deselectNonNulls(nulls, 0, n);
            for (int32_t i = 0; i < n; ++i) {
                if (fieldVec->IsNull(i)) {
                    result->SetNull(i);
                }
            }
        }
        delete fieldVec;
    }

    std::vector<SwitchBranch> branches;
    branches.reserve(e.whenClause.size() + 1);
    std::vector<uint64_t> trueBits;

    // Single-case fast path: a 1-branch CASE is exactly one IF, so route it
    // through the two-way merge machinery (same shape as Visit(IfExpr))
    // instead of the K-way structures, which are pure overhead for one case.
    // Skipped when the null-propagation pre-pass ran, because its sealed rows
    // are handled by the general path's merge bookkeeping.
    if (e.whenClause.size() == 1 && !e.propagatesNulls_ && e.falseExpr != nullptr) {
        const Expr *cond = e.whenClause[0].first;
        const Expr *thenExpr = e.whenClause[0].second;
        const Expr *elseExpr = e.falseExpr;

        SelectivityVector trueSel(n, false);
        SelectivityVector falseSel(n, false);
        if (e.eqFieldConst_ && n > 0) {
            const auto *binary = static_cast<const BinaryExpr *>(cond);
            const auto *lit = static_cast<const LiteralExpr *>(binary->right);
            SelectivityVector allSel(n, true);
            BaseVector *fieldVec = EvaluateBranch(binary->left, allSel, true);
            if (lit->isNull) {
                falseSel = remaining;   // `field = NULL` never matches
            } else {
                std::vector<int32_t> truePos;
                RunEqFieldConstCase(fieldVec, *lit, fieldVec->GetTypeId(), remaining, trueSel, truePos);
                falseSel = remaining;
                falseSel.deselect(trueSel);
            }
            delete fieldVec;
        } else {
            BaseVector *condVec = EvaluateBranch(cond, remaining, false);
            int32_t condSize = static_cast<int32_t>(remaining.countSelected());
            CondMix mix = ClassifyBoolCond(condVec, condSize, trueBits);
            delete condVec;
            if (mix == CondMix::kAllTrue) {
                trueSel = remaining;
            } else if (mix == CondMix::kMix) {
                int32_t j = 0;
                remaining.applyToSelected([&](vector_size_t row) {
                    if (BitUtil::IsBitSet(trueBits.data(), j)) {
                        trueSel.setValid(row, true);
                    }
                    ++j;
                });
                trueSel.updateBounds();
                falseSel = remaining;
                falseSel.deselect(trueSel);
            } else {
                falseSel = remaining;
            }
        }

        if (!trueSel.hasSelections()) {
            result = EvaluateBranch(elseExpr, falseSel, false);
        } else if (!falseSel.hasSelections()) {
            result = EvaluateBranch(thenExpr, trueSel, false);
        } else {
            bool trueCheap = IsCheapIfBranch(thenExpr);
            bool falseCheap = IsCheapIfBranch(elseExpr);
            BaseVector *trueVec = EvaluateBranch(thenExpr, trueSel, trueCheap);
            BaseVector *falseVec = EvaluateBranch(elseExpr, falseSel, falseCheap);
            IfFunction::CreateResultVector(e.dataType->GetId(), n, trueVec, result);
            std::vector<int32_t> truePos;
            std::vector<int32_t> falsePos;
            trueSel.applyToSelected([&](vector_size_t row) { truePos.push_back(row); });
            falseSel.applyToSelected([&](vector_size_t row) { falsePos.push_back(row); });
            MergeIfBranches(trueVec, falseVec, truePos, falsePos, trueCheap, falseCheap, result, n,
                e.dataType->GetId());
            delete trueVec;
            delete falseVec;
        }
        inputValues_.push(result);
        return;
    }

    // EQ(field, literal) fast path: the shared field is evaluated once (a
    // zero-copy slice); each condition then runs as a scalar pass over the
    // remaining rows, skipping the compacted-evaluation machinery entirely.
    BaseVector *eqFieldVec = nullptr;
    if (e.eqFieldConst_ && n > 0) {
        SelectivityVector allSel(n, true);
        eqFieldVec = EvaluateBranch(static_cast<const BinaryExpr *>(e.whenClause[0].first)->left, allSel, true);
    }

    for (const auto &when : e.whenClause) {
        if (!remaining.hasSelections()) {
            break;
        }

        SelectivityVector thenSel(n, false);
        std::vector<int32_t> thenPos;
        if (e.eqFieldConst_) {
            const auto *binary = static_cast<const BinaryExpr *>(when.first);
            const auto *lit = static_cast<const LiteralExpr *>(binary->right);
            if (lit->isNull) {
                continue;   // `field = NULL` never matches
            }
            thenPos.reserve(remaining.countSelected());
            RunEqFieldConstCase(eqFieldVec, *lit, eqFieldVec->GetTypeId(), remaining, thenSel, thenPos);
            remaining.deselect(thenSel);
            if (!thenSel.hasSelections()) {
                continue;
            }
        } else {
            // 1. Evaluate the condition on the remaining rows only.
            BaseVector *condVec = EvaluateBranch(when.first, remaining, false);
            int32_t condSize = static_cast<int32_t>(remaining.countSelected());

            // 2. Classify the condition and build the selection of the rows the
            //    result runs on.
            CondMix mix = ClassifyBoolCond(condVec, condSize, trueBits);
            delete condVec;
            if (mix == CondMix::kAllFalse) {
                continue;
            }
            if (mix == CondMix::kAllTrue) {
                thenSel = remaining;
                thenPos.reserve(remaining.countSelected());
                remaining.applyToSelected([&](vector_size_t row) { thenPos.push_back(row); });
            } else {
                thenPos.reserve(remaining.countSelected());
                int32_t j = 0;
                remaining.applyToSelected([&](vector_size_t row) {
                    if (BitUtil::IsBitSet(trueBits.data(), j)) {
                        thenSel.setValid(row, true);
                        thenPos.push_back(row);
                    }
                    ++j;
                });
                thenSel.updateBounds();
            }
            remaining.deselect(thenSel);
            if (!thenSel.hasSelections()) {
                continue;
            }
        }

        // 3. Evaluate the result on its rows. Cheap (leaf-only) results run in
        //    place over the whole space; expensive ones are compacted.
        bool cheap = IsCheapIfBranch(when.second);
        int32_t selCount = thenSel.countSelected();
        BaseVector *thenVec = EvaluateBranch(when.second, thenSel, cheap);
        if (result == nullptr) {
            IfFunction::CreateResultVector(e.dataType->GetId(), n, thenVec, result);
        }
        branches.push_back({thenVec, std::move(thenPos), cheap || selCount == n});
    }
    delete eqFieldVec;

    // The else clause covers the rows no case matched.
    if (remaining.hasSelections()) {
        if (e.falseExpr != nullptr) {
            bool cheap = IsCheapIfBranch(e.falseExpr);
            int32_t selCount = remaining.countSelected();
            BaseVector *elseVec = EvaluateBranch(e.falseExpr, remaining, cheap);
            if (result == nullptr) {
                IfFunction::CreateResultVector(e.dataType->GetId(), n, elseVec, result);
            }
            SwitchBranch elseBranch;
            elseBranch.vec = elseVec;
            elseBranch.coversAll = cheap || selCount == n;
            elseBranch.pos.reserve(selCount);
            remaining.applyToSelected([&](vector_size_t row) { elseBranch.pos.push_back(row); });
            branches.push_back(std::move(elseBranch));
        } else {
            // No ELSE clause: unmatched rows are NULL. Defensive; the parser
            // always materializes an else clause.
            if (result == nullptr) {
                if (e.whenClause.empty()) {
                    OMNI_THROW("Switch expr Error", "Switch expression without cases and else clause");
                }
                SelectivityVector allSel(n, true);
                BaseVector *srcVec = EvaluateBranch(e.whenClause[0].second, allSel, true);
                IfFunction::CreateResultVector(e.dataType->GetId(), n, srcVec, result);
                delete srcVec;
            }
            remaining.applyToSelected([&](vector_size_t row) { result->SetNull(row); });
        }
    }

    if (result == nullptr) {
        // No branch produced a result (empty input): evaluate a fallback
        // branch in place to produce the correctly-typed empty vector.
        const Expr *fallback =
            e.falseExpr != nullptr ? e.falseExpr : (e.whenClause.empty() ? nullptr : e.whenClause[0].second);
        if (fallback == nullptr) {
            OMNI_THROW("Switch expr Error", "Switch expression without cases and else clause");
        }
        result = EvaluateBranch(fallback, remaining, false);
    }

    MergeSwitchBranches(branches, result, n, e.dataType->GetId());
    for (const SwitchBranch &branch : branches) {
        delete branch.vec;
    }
    inputValues_.push(result);
}

void ExprEval::Visit(const ParamRefExpr &e) {
    int32_t paramIdx = paramNameToIdxMap[e.paramName_];
    vec::BaseVector *paramVec = lambdaParams_.at(paramIdx);
    if (activeSel_ != nullptr) {
        // Compacted evaluation mode: project only the active rows so the result
        // stays aligned with the compacted space.
        PushProjectedColumn(paramVec, paramVec->GetTypeId(), activePos_.data(),
            static_cast<int32_t>(activePos_.size()));
        return;
    }
    vec::BaseVector *toPush = paramVec->Slice(0, paramVec->GetSize());
    inputValues_.push(toPush != nullptr ? toPush : paramVec);
}

void ExprEval::Visit(const LambdaExpr &e) {
    context->SetCurrentLambda(&e);
}

void ExprEval::VisitExpr(const Expr &e)
{
    try {
        e.Accept(*this);
    } catch (...) {
        // Free the partially evaluated operand stack — the dtor doesn't.
        while (!inputValues_.empty()) {
            delete inputValues_.top();
            inputValues_.pop();
        }
        throw;
    }
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
