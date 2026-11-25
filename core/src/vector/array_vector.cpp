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
 *
 * Description: arrayVector  implementation
 */

#include "array_vector.h"
#include "vector_helper.h"

namespace omniruntime::vec {

void ArrayVector::SetValue(int index, BaseVector *value)
{
    if (value == nullptr) {
        SetNull(index);
        return;
    }

    int valueSize = value->GetSize();
    if (valueSize > 0) {
        int elementVectorSize = GetOffset(index);
        elements->Expand(elementVectorSize + valueSize);
        VectorHelper::AppendVector(elements.get(), elementVectorSize, value, valueSize);
    }
    SetSize(index, valueSize);
}

BaseVector* ArrayVector::GetValue(int index)
{
    if (UNLIKELY(index < 0 || index >= size)) {
        std::string message("slice vector out of range(needed size:%d, real size:%d).", index,
            size);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    int64_t startOffset = GetOffset(index);
    int64_t arraySize = GetSize(index);

    return GetElementVector()->Slice(startOffset, arraySize, false);
}

/* *
 * Copies the values of the vector at the indicated positions
 * @param positions
 * @param offset
 * @param length
 */
ArrayVector *ArrayVector::CopyPositions(const int *positions, int positionOffset, int length)
{
    if (UNLIKELY((positions == nullptr) || (length < 0))) {
        std::string message("ArrayVector positions is null or the input length is incorrect: %d.", length);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }
    ArrayVector *newArrayVector = new ArrayVector(length);
    auto startPositions = positions + positionOffset;

    std::vector<int> elementPositions;
    int elementLength = 0;
    for (int32_t i = 0; i < length; i++) {
        int position = startPositions[i];
        // position == -1 means that this position in newArrayVector should be set to NULL.
        if (UNLIKELY(position == -1)) {
            newArrayVector->SetNull(i);
            continue;
        }
        if (UNLIKELY(IsNull(position))) {
            newArrayVector->SetNull(i);
            continue;
        }
        int elementIndex = this->GetOffset(position);
        int elementSize = this->GetSize(position);
        newArrayVector->SetSize(i, elementSize);
        elementLength += elementSize;
        updateElementPositions(elementPositions, elementIndex, elementSize);
    }

    auto elementVector = this->GetElementVector();
    if (UNLIKELY(elementLength == 0)) {
        // need create concreate vector, not BaseVector
        auto elementDataType = VectorHelper::GetDataType(elementVector.get());
        newArrayVector->AddElements(VectorHelper::CreateComplexVector(elementDataType.get(), elementLength));
    } else {
        auto newElementVector = elementVector->CopyPositions(elementPositions.data(), 0, elementLength);
        newArrayVector->AddElements(newElementVector);
    }

    return newArrayVector;
}
}