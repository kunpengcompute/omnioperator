/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "topn.h"
#include <vector>
#include "vector/vector_helper.h"
#include "operator/util/operator_util.h"
#include "operator/omni_id_type_vector_traits.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace std;

TopNOperatorFactory::TopNOperatorFactory(const type::DataTypes &sourceTypes, int32_t limit, int32_t offset,
                                         int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
                                         int32_t sortColCount)
    : sourceTypes(sourceTypes), limit(limit), offset(offset), sortColCount(sortColCount)
{
    this->sortCols.insert(this->sortCols.end(), sortCols, sortCols + sortColCount);
    this->sortAscendings.insert(this->sortAscendings.end(), sortAscendings, sortAscendings + sortColCount);
    this->sortNullFirsts.insert(this->sortNullFirsts.end(), sortNullFirsts, sortNullFirsts + sortColCount);
}

TopNOperatorFactory::TopNOperatorFactory(const type::DataTypes &sourceTypes, int32_t limit, int32_t offset,
                                         std::vector<int32_t> sortCols, std::vector<int32_t> sortAscendings,
                                         std::vector<int32_t> sortNullFirsts, int32_t sortColCount)
    : sourceTypes(sourceTypes), limit(limit), offset(offset), sortCols(sortCols), sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts), sortColCount(sortColCount)
{
}

TopNOperatorFactory::~TopNOperatorFactory() = default;

Operator *TopNOperatorFactory::CreateOperator()
{
    return new TopNOperator(sourceTypes, limit, offset, sortCols, sortAscendings, sortNullFirsts, sortColCount);
}

TopNOperator::TopNOperator(const type::DataTypes &sourceTypes, int32_t limit, int32_t offset,
                           std::vector<int32_t> &sortCols, std::vector<int32_t> &sortAscendings,
                           std::vector<int32_t> &sortNullFirsts, int32_t sortColCount)
    : sourceTypes(sourceTypes), sourceTypesCount(this->sourceTypes.GetSize()), sortCols(sortCols), limit(limit),
      offset(offset), sortAscendings(sortAscendings), sortNullFirsts(sortNullFirsts), sortColCount(sortColCount)
{
    int32_t eachRowSize = OperatorUtil::GetRowSize(sourceTypes.Get());
    maxRowCount = OperatorUtil::GetMaxRowCount(eachRowSize);
}

TopNOperator::~TopNOperator()
{
    for (const auto &item : singleRowVectorBatchSet) {
        VectorHelper::FreeVecBatch(item);
    }
    resultVectorBatchList.clear();
}

int CompareVectorBatch(int32_t leftPosition, omniruntime::vec::VectorBatch *left, int32_t rightPosition,
                       omniruntime::vec::VectorBatch *right, int32_t sortColCount, const int32_t *sortCols,
                       const int32_t *sourceTypeIds, const int32_t *sortAscendings, const int32_t *sortNullFirsts)
{
    int compare = 0;

    for (int i = 0; i < sortColCount; ++i) {
        int32_t sortCol = sortCols[i];
        int32_t colTypeId = sourceTypeIds[sortCol];

        BaseVector *leftVector = left->Get(sortCol);
        BaseVector *rightVector = right->Get(sortCol);

        compare = OperatorUtil::CompareNull(leftVector, leftPosition, rightVector, rightPosition, sortNullFirsts[i]);
        if (compare == OperatorUtil::COMPARE_STATUS_GREATER_THAN || compare == OperatorUtil::COMPARE_STATUS_LESS_THAN) {
            break;
        } else if (compare == OperatorUtil::COMPARE_STATUS_EQUAL) {
            continue;
        }

        compare =
            OperatorUtil::CompareVectorAtPosition(colTypeId, leftVector, leftPosition, rightVector, rightPosition);
        if (sortAscendings[i] == 0) {
            compare = -compare;
        }

        if (compare != 0) {
            break;
        }
    }
    return compare;
}

int32_t TopNOperator::AddInput(VectorBatch *vectorBatch)
{
    if (vectorBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vectorBatch);
        ResetInputVecBatch();
        return 0;
    }

    auto typeIds = sourceTypes.GetIds();
    int32_t position = 0;
    for (; (static_cast<int32_t>(pq.size()) < limit) && (position < vectorBatch->GetRowCount()); ++position) {
        VectorBatch *singleRowVecBatch = CreateSingleRowVecBatch(vectorBatch, position);
        pq.emplace(typeIds, sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount,
                   singleRowVecBatch);
        singleRowVectorBatchSet.insert(singleRowVecBatch);
    }
    for (; position < vectorBatch->GetRowCount(); ++position) {
        VectorBatch *top = pq.top().GetVecBatch();
        if (CompareVectorBatch(position, vectorBatch, 0, top, sortColCount, sortCols.data(), typeIds,
                               sortAscendings.data(), sortNullFirsts.data()) < 0) {
            pq.pop();
            UpdateSingleRowVectorBatch(vectorBatch, top, position);
            pq.emplace(typeIds, sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), sortColCount, top);
        }
    }
    VectorHelper::FreeVecBatch(vectorBatch);
    ResetInputVecBatch();
    SetStatus(OMNI_STATUS_NORMAL);
    return 0;
}

template <type::DataTypeId typeId>
static void ALWAYS_INLINE SetValueForSingleRowVecBatch(VectorBatch *singleRowVecBatch, int32_t colIndex,
                                                       BaseVector *vector, int32_t position)
{
    using Type = typename NativeAndVectorType<typeId>::type;
    using Vector = typename NativeAndVectorType<typeId>::vector;
    using DictVector = typename NativeAndVectorType<typeId>::dictVector;

    auto resultVector = static_cast<Vector *>(singleRowVecBatch->Get(colIndex));
    if (vector->IsNull(position)) {
        resultVector->SetNull(0);
        return;
    }

    Type value;
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        value = (static_cast<DictVector *>(vector))->GetValue(position);
    } else {
        value = (static_cast<Vector *>(vector))->GetValue(position);
    }
    resultVector->SetNotNull(0);
    resultVector->SetValue(0, value);
}

static void ALWAYS_INLINE SetVarCharForSingleRowVecBatch(VectorBatch *singleRowVecBatch, int32_t colIndex,
                                                         BaseVector *vector, int32_t position)
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    using VarcharDictVector = Vector<DictionaryContainer<std::string_view>>;

    auto resultVector = static_cast<VarcharVector *>(singleRowVecBatch->Get(colIndex));
    // we just need to set value null
    if (vector->IsNull(position)) {
        resultVector->SetNull(0);
        return;
    }

    std::string_view value;
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        value = static_cast<VarcharDictVector *>(vector)->GetValue(position);
    } else {
        value = static_cast<VarcharVector *>(vector)->GetValue(position);
    }
    resultVector->SetNotNull(0);
    resultVector->SetValue(0, value);
}

void TopNOperator::UpdateSingleRowVectorBatch(VectorBatch *vectorBatch, VectorBatch *singleRowVecBatch,
                                              int32_t position) const
{
    auto typeIds = sourceTypes.GetIds();
    for (int i = 0; i < sourceTypesCount; ++i) {
        BaseVector *vector = vectorBatch->Get(i);
        switch (typeIds[i]) {
            case OMNI_BOOLEAN:
                SetValueForSingleRowVecBatch<OMNI_BOOLEAN>(singleRowVecBatch, i, vector, position);
                break;
            case OMNI_INT:
            case OMNI_DATE32: SetValueForSingleRowVecBatch<OMNI_INT>(singleRowVecBatch, i, vector, position); break;
            case OMNI_SHORT: SetValueForSingleRowVecBatch<OMNI_SHORT>(singleRowVecBatch, i, vector, position); break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64: SetValueForSingleRowVecBatch<OMNI_LONG>(singleRowVecBatch, i, vector, position); break;
            case OMNI_DOUBLE: SetValueForSingleRowVecBatch<OMNI_DOUBLE>(singleRowVecBatch, i, vector, position); break;
            case OMNI_VARCHAR:
            case OMNI_CHAR: {
                SetVarCharForSingleRowVecBatch(singleRowVecBatch, i, vector, position);
                break;
            }
            case OMNI_DECIMAL128:
                SetValueForSingleRowVecBatch<OMNI_DECIMAL128>(singleRowVecBatch, i, vector, position);
                break;
            case OMNI_BYTE: SetValueForSingleRowVecBatch<OMNI_BYTE>(singleRowVecBatch, i, vector, position); break;
            default: break;
        }
    }
}

template <type::DataTypeId typeId>
static void ALWAYS_INLINE SetVectorForSingleRowVecBatch(omniruntime::vec::VectorBatch *singleRowVecBatch,
                                                        BaseVector *vector, int32_t position)
{
    using Type = typename NativeAndVectorType<typeId>::type;
    using Vector = typename NativeAndVectorType<typeId>::vector;
    using DictVector = typename NativeAndVectorType<typeId>::dictVector;

    auto flatVector = VectorHelper::CreateFlatVector<typeId>(1);
    if (vector->IsNull(position)) {
        (static_cast<Vector *>(flatVector))->SetNull(0);
    } else {
        Type value;
        if (vector->GetEncoding() == OMNI_DICTIONARY) {
            value = (static_cast<DictVector *>(vector))->GetValue(position);
        } else {
            value = (static_cast<Vector *>(vector))->GetValue(position);
        }
        (static_cast<Vector *>(flatVector))->SetValue(0, value);
    }
    singleRowVecBatch->Append(flatVector);
}

VectorBatch *TopNOperator::CreateSingleRowVecBatch(VectorBatch *vectorBatch, int32_t position) const
{
    auto typeIds = sourceTypes.GetIds();
    auto singleRowVecBatch = std::make_unique<VectorBatch>(1);
    auto singleRowVecBatchPtr = singleRowVecBatch.get();
    for (int i = 0; i < sourceTypesCount; ++i) {
        BaseVector *vector = vectorBatch->Get(i);
        DYNAMIC_TYPE_DISPATCH(SetVectorForSingleRowVecBatch, typeIds[i], singleRowVecBatchPtr, vector, position);
    }

    return singleRowVecBatch.release();
}

template <type::DataTypeId typeId>
static void ALWAYS_INLINE SetValueForVector(BaseVector *pqVector, BaseVector *tmpVector, int64_t index)
{
    using Vector = typename NativeAndVectorType<typeId>::vector;

    auto value = (static_cast<Vector *>(pqVector))->GetValue(0);
    (static_cast<Vector *>(tmpVector))->SetValue(index, value);
}

void TopNOperator::FillResultVectorBatchList()
{
    totalRowCount = static_cast<int64_t>(pq.size()) - offset;
    if (totalRowCount < 0) {
        return;
    }
    resultVectorBatchList.resize(totalRowCount);
    for (int64_t i = totalRowCount; i > 0; i--) {
        resultVectorBatchList[i - 1] = pq.top().GetVecBatch();
        pq.pop();
    }
}

int32_t TopNOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    if (isFinished()) {
        return 0;
    }
    if (!hasFilledResult) {
        if (pq.empty()) {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        FillResultVectorBatchList();
        if (resultVectorBatchList.empty()) {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        hasFilledResult = true;
    }
    int64_t rowCount = std::min(maxRowCount, totalRowCount - outputtedRowCount);
    auto resultVecBatch = std::make_unique<VectorBatch>(rowCount);
    auto resultVecBatchPtr = resultVecBatch.get();
    omniruntime::vec::VectorHelper::AppendVectors(resultVecBatchPtr, sourceTypes, rowCount);
    auto typeIds = sourceTypes.GetIds();
    for (int64_t i = 0; i < rowCount; i++) {
        VectorBatch *singleVecBatch = resultVectorBatchList[i + outputtedRowCount];
        for (int j = 0; j < sourceTypesCount; ++j) {
            BaseVector *singleVector = singleVecBatch->Get(j);
            BaseVector *resultVector = resultVecBatchPtr->Get(j);
            if (typeIds[j] == OMNI_VARCHAR || typeIds[j] == OMNI_CHAR) {
                SetVarcharValueForVectorBatch(i, singleVector, resultVector);
            } else {
                SetValueForVectorBatch(typeIds[j], i, singleVector, resultVector);
            }
        }
        VectorHelper::FreeVecBatch(singleVecBatch);
        singleRowVectorBatchSet.erase(singleVecBatch);
    }
    outputtedRowCount += rowCount;
    *outputVecBatch = resultVecBatch.release();
    if (!HasNext()) {
        SetStatus(OMNI_STATUS_FINISHED);
    }
    return 0;
}

void TopNOperator::SetValueForVectorBatch(int32_t typeId, int64_t index, BaseVector *pqVector,
                                          BaseVector *tmpVector) const
{
    if (pqVector->IsNull(0)) {
        tmpVector->SetNull(static_cast<int32_t>(index));
        return;
    }

    DYNAMIC_TYPE_DISPATCH(SetValueForVector, typeId, pqVector, tmpVector, index);
}

void TopNOperator::SetVarcharValueForVectorBatch(int64_t rowNum, BaseVector *pqVector, BaseVector *tmpVector) const
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    if (pqVector->IsNull(0)) {
        static_cast<VarcharVector *>(tmpVector)->SetNull(static_cast<int32_t>(rowNum));
        return;
    }
    auto value = static_cast<VarcharVector *>(pqVector)->GetValue(0);
    static_cast<VarcharVector *>(tmpVector)->SetValue(static_cast<int32_t>(rowNum), value);
}

OmniStatus TopNOperator::Close()
{
    if (inputVecBatch != nullptr) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        inputVecBatch = nullptr;
    }

    return OMNI_STATUS_NORMAL;
}

RowComparator::~RowComparator() = default;

const int32_t *RowComparator::GetSourceTypes() const { return sourceTypes; }

int32_t *RowComparator::GetSortAscendings() const { return sortAscendings; }

int32_t *RowComparator::GetSortNullFirsts() const { return sortNullFirsts; }

int32_t RowComparator::GetSortColCount() const { return sortColCount; }

VectorBatch *RowComparator::GetVecBatch() const { return vectorBatch; }

int32_t *RowComparator::GetSortCols() const { return sortCols; }

bool operator<(const RowComparator &left, const RowComparator &right)
{
    int compare =
        CompareVectorBatch(0, left.GetVecBatch(), 0, right.GetVecBatch(), left.GetSortColCount(), left.GetSortCols(),
                           left.GetSourceTypes(), left.GetSortAscendings(), left.GetSortNullFirsts());
    // priority_queue is desc, return 1 means left smaller than right,so
    // priority_queue will swap. suppose output is asc,compare>0 means left bigger
    // than right so pq shouldn't swap,so return 0
    if (compare >= 0) {
        return false;
    } else {
        return true;
    }
}
} // namespace op
} // namespace omniruntime
