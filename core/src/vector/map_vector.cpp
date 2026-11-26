/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: MapVector  implementation
 */

#include "map_vector.h"
#include "vector_helper.h"

namespace omniruntime::vec {
/* *
 * Copies the values of the vector at the indicated positions
 * @param positions
 * @param offset
 * @param length
 */
MapVector *MapVector::CopyPositions(const int *positions, int positionOffset, int length)
{
    if ((positions == nullptr) || (length < 0)) {
        std::string message("MapVector positions is null or the input length is incorrect: %d.", length);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    MapVector *newMapVector = new MapVector(length);
    auto startPositions = positions + positionOffset;

    std::vector<int> keyPositions;
    int keyLength = 0;
    for (int32_t i = 0; i < length; i++) {
        int position = startPositions[i];
        if (UNLIKELY(IsNull(position))) {
            newMapVector->SetNull(i);
        }
        int keyIndex = this->GetOffset(position);
        int keySize = this->GetSize(position);

        newMapVector->SetOffset(i, keyLength);
        keyLength += keySize;

        UpdateKeyPositions(keyPositions, keyIndex, keySize);
    }
    newMapVector->SetOffset(length, keyLength);

    auto keyVector = this->GetKeyVector();
    if (UNLIKELY(keyLength == 0)) {
        auto keyDataType = VectorHelper::GetDataType(keyVector.get());
        newMapVector->AddKeys(VectorHelper::CreateComplexVector(keyDataType.get(), length));
    } else {
        auto newKeyVector = keyVector->CopyPositions(keyPositions.data(), 0, keyLength);
        newMapVector->AddKeys(newKeyVector);
    }

    auto valueVector = this->GetValueVector();
    if (UNLIKELY(keyLength == 0)) {
        // need create concreate vector, not BaseVector
        auto valueDataType = VectorHelper::GetDataType(valueVector.get());
        newMapVector->AddValues(VectorHelper::CreateComplexVector(valueDataType.get(), keyLength));
    } else {
        auto newValueVector = valueVector->CopyPositions(keyPositions.data(), 0, keyLength);
        newMapVector->AddValues(newValueVector);
    }
    return newMapVector;
}
} // namespace omniruntime::vec
