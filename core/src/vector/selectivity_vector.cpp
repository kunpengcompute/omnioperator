/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

#include "selectivity_vector.h"
namespace omniruntime::vec {
// only for UT
template void omniruntime::vec::SelectivityVector::GetSelectivityVectorFromVectorBatch<
    (omniruntime::vec::GetSelectivityVectorMethod)0>(omniruntime::vec::VectorBatch *,
    omniruntime::vec::SelectivityVector &);
template void omniruntime::vec::SelectivityVector::GetSelectivityVectorFromVectorBatch<
    (omniruntime::vec::GetSelectivityVectorMethod)0>(omniruntime::vec::VectorBatch *,
    const std::vector<unsigned long, std::allocator<unsigned long>> &, omniruntime::vec::SelectivityVector &);
template void omniruntime::vec::SelectivityVector::GetSelectivityVectorFromVectorBatch<
    (omniruntime::vec::GetSelectivityVectorMethod)1>(omniruntime::vec::VectorBatch *,
    omniruntime::vec::SelectivityVector &);
template void omniruntime::vec::SelectivityVector::GetSelectivityVectorFromVectorBatch<
    (omniruntime::vec::GetSelectivityVectorMethod)1>(omniruntime::vec::VectorBatch *,
    const std::vector<unsigned long, std::allocator<unsigned long>> &, omniruntime::vec::SelectivityVector &);

SelectivityVector SelectivityVector::GetSelectivityVectorFromBaseVector(omniruntime::vec::BaseVector *baseVector)
{
    auto rowNum = baseVector->GetSize();
    SelectivityVector result(rowNum, true);
    if (baseVector->HasNull()) {
        for (int i = 0; i < rowNum; i++) {
            if (baseVector->IsNull(i)) {
                result.SetBit(i, false);
            }
        }
    }
    return result;
}

template <typename FlatVector>
void SetFlatVectorValue(size_t rowCount, BaseVector *baseVector, BaseVector *selectedBaseVector,
    SelectivityVector &selectivityVector)
{
    size_t index = 0;
    for (auto j = 0; j < rowCount; j++) {
        if (selectivityVector.IsValid(j)) {
            if (baseVector->IsNull(j)) {
                static_cast<FlatVector *>(selectedBaseVector)->SetNull(index++);
            } else {
                auto value = static_cast<FlatVector *>(baseVector)->GetValue(j);
                static_cast<FlatVector *>(selectedBaseVector)->SetValue(index++, value);
            }
        }
    }
}

bool SelectivityVector::GetFlatBaseVectorsFromSelectivityVector(std::vector<BaseVector *> &baseVectors,
    SelectivityVector &selectivityVector, std::vector<BaseVector *> &result)
{
    if (selectivityVector.IsAllSelected()) {
        // all selected, filtering is not required.
        return false;
    }
    if (UNLIKELY(baseVectors.empty())) {
        return false;
    }
    size_t resultSize = selectivityVector.CountSelected();
    size_t rowCount = baseVectors[0]->GetSize();
    size_t encodingType = baseVectors[0]->GetEncoding();
    if (UNLIKELY(encodingType == OMNI_DICTIONARY)) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "OMNI_DICTIONARY is unsupported.");
    }
    result.resize(baseVectors.size(), nullptr);
    for (size_t i = 0; i < baseVectors.size(); i++) {
        auto baseVector = baseVectors[i];
        auto dataType = baseVector->GetTypeId();
        auto selectedBaseVector = VectorHelper::CreateVector(OMNI_FLAT, dataType, static_cast<int32_t>(resultSize));
        switch (dataType) {
            case OMNI_INT:
            case OMNI_DATE32: {
                SetFlatVectorValue<Vector<int32_t>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_SHORT: {
                SetFlatVectorValue<Vector<int16_t>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_BYTE: {
                SetFlatVectorValue<Vector<int8_t>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64: {
                SetFlatVectorValue<Vector<int64_t>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_DOUBLE: {
                SetFlatVectorValue<Vector<double>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_BOOLEAN: {
                SetFlatVectorValue<Vector<bool>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_DECIMAL128: {
                SetFlatVectorValue<Vector<Decimal128>>(rowCount, baseVector, selectedBaseVector, selectivityVector);
                break;
            }
            case OMNI_VARCHAR:
            case OMNI_CHAR: {
                SetFlatVectorValue<Vector<LargeStringContainer<std::string_view>>>(rowCount, baseVector,
                    selectedBaseVector, selectivityVector);
                break;
            }
            default: {
                LogError("No such %d type support", dataType);
                throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                    "unsupported selectivity type: " + std::to_string(static_cast<int>(dataType)));
            }
        }
        result[i] = selectedBaseVector;
    }
    return true;
}

template <GetSelectivityVectorMethod type>
void SelectivityVector::GetSelectivityVectorFromVectorBatch(omniruntime::vec::VectorBatch *vectorBatch,
    const std::vector<size_t> &selectedColumns, SelectivityVector &selectivityVector)
{
    auto rowNum = vectorBatch->GetRowCount();
    SelectivityVector singleColumnSelectivityVector(rowNum, true);
    if constexpr (type == GetSelectivityVectorMethod::AND) {
        for (unsigned long selectedColumn : selectedColumns) {
            auto currentColumn = vectorBatch->Get(selectedColumn);
            for (int i = 0; i < rowNum; i++) {
                if (currentColumn->IsNull(i)) {
                    singleColumnSelectivityVector.SetBit(i, false);
                }
            }
            selectivityVector.And(singleColumnSelectivityVector);
            singleColumnSelectivityVector.SetAll();
        }
    } else if constexpr (type == GetSelectivityVectorMethod::OR) {
        for (unsigned long selectedColumn : selectedColumns) {
            auto currentColumn = vectorBatch->Get(selectedColumn);
            for (int i = 0; i < rowNum; i++) {
                if (currentColumn->IsNull(i)) {
                    singleColumnSelectivityVector.SetBit(i, false);
                }
            }
            selectivityVector.Or(singleColumnSelectivityVector);
            singleColumnSelectivityVector.SetAll();
        }
    } else {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
            "unsupported selectivity type: " + std::to_string(static_cast<int>(type)));
    }
}

template <GetSelectivityVectorMethod type>
void SelectivityVector::GetSelectivityVectorFromVectorBatch(omniruntime::vec::VectorBatch *vectorBatch,
    SelectivityVector &selectivityVector)
{
    auto vectorBatchSize = vectorBatch->GetVectorCount();
    auto rowNum = vectorBatch->GetRowCount();
    if constexpr (type == GetSelectivityVectorMethod::AND) {
        SelectivityVector singleColumnSelectivityVector(rowNum, true);
        for (size_t i = 0; i < vectorBatchSize; i++) {
            auto currentColumn = vectorBatch->Get(i);
            for (int j = 0; j < rowNum; j++) {
                if (currentColumn->IsNull(j)) {
                    singleColumnSelectivityVector.SetBit(j, false);
                }
            }
            selectivityVector.And(singleColumnSelectivityVector);
            singleColumnSelectivityVector.SetAll();
        }
    } else if constexpr (type == GetSelectivityVectorMethod::OR) {
        SelectivityVector singleColumnSelectivityVector(rowNum, false);
        for (size_t i = 0; i < vectorBatchSize; i++) {
            auto currentColumn = vectorBatch->Get(i);
            for (int j = 0; j < rowNum; j++) {
                if (currentColumn->IsNull(j)) {
                    singleColumnSelectivityVector.SetBit(j, false);
                }
            }
            selectivityVector.Or(singleColumnSelectivityVector);
            singleColumnSelectivityVector.SetAll();
        }
    } else {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
            "unsupported selectivity type: " + std::to_string(static_cast<int>(type)));
    }
}
}
