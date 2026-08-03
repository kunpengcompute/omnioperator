/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: MapVector  implementation
 */

#include "map_vector.h"
#include "vector_helper.h"

namespace omniruntime::vec {
    MapVector *MapVector::Slice(int positionOffset, int length, bool isCopy)
    {
        if (UNLIKELY(positionOffset + length > size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto sliced = new MapVector(length);
        sliced->isSliced = true;
        int32_t startOffset = GetOffset(positionOffset);
        for (int i = 0; i < length; ++i) {
            sliced->SetOffset(i + 1, GetOffset(positionOffset + 1 + i) - startOffset);
        }
        for (int i = 0; i < length; ++i) {
            if (IsNull(positionOffset + i)) {
                sliced->SetNull(i);
            }
        }
        sliced->SetKeyVector(
            std::shared_ptr<BaseVector>(GetKeyVector()->Slice(startOffset, sliced->GetOffset(length), isCopy)));
        sliced->SetValueVector(
            std::shared_ptr<BaseVector>(GetValueVector()->Slice(startOffset, sliced->GetOffset(length), isCopy)));
        return sliced;
    }

    /* *
     * Copies the values of the vector at the indicated positions
     * @param positions
     * @param offset
     * @param length
     */
    MapVector *MapVector::CopyPositions(const int *positions, int positionOffset, int length)
    {
        if ((positions == nullptr) || (length < 0)) {
            std::string message = "MapVector positions is null or the input length is incorrect: " + std::to_string(length) + ".";
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

    void MapVector::Expand(int32_t needCapacity)
    {
        if (needCapacity <= size) {
            return;
        }

        if (needCapacity <= capacity) {
            size = needCapacity;
            return;
        }

        int32_t newCapacity = std::max(capacity * 2, needCapacity);
        int32_t oldSize = size;

        auto oldOffsetsBuffer = offsetsBuffer;
        offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(newCapacity + 1, true);
        offsets = offsetsBuffer->GetBuffer();

        if (oldOffsetsBuffer != nullptr) {
            memcpy(
                offsets,
                oldOffsetsBuffer->GetBuffer(),
                (oldSize + 1) * sizeof(int64_t)
            );
        }

        auto oldNullsBuffer = nullsBuffer;
        nullsBuffer = std::make_shared<NullsBuffer>(newCapacity);
        if (oldNullsBuffer != nullptr) {
            nullsBuffer->SetNulls(0, oldNullsBuffer.get(), oldSize);
        } else {
            nullsBuffer->SetNulls(0, false, newCapacity);
        }

        capacity = newCapacity;
        size = needCapacity;
    }

    void MapVector::Append(MapVector* other, int32_t offset)
    {
        if (other == nullptr) {
            return;
        }

        if (keys->GetTypeId() != other->keys->GetTypeId() ||
            values->GetTypeId() != other->values->GetTypeId()) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", "MapVector type mismatch during Append.");
        }

        int64_t keyLength = other->GetSize(0);
        if (keyLength == 0) {
            return;
        }
        VectorHelper::ExpandElementVector(keys.get(), keys->GetTypeId(), offset + keyLength);
        VectorHelper::AppendVector(keys.get(), offset,other->keys.get(), static_cast<int32_t>(keyLength));

        VectorHelper::ExpandElementVector(values.get(), values->GetTypeId(), offset + keyLength);
        VectorHelper::AppendVector(values.get(), offset,other->values.get(), static_cast<int32_t>(keyLength));
    }

    /* *
     * Append another mapVector to the current mapVector starting at a specified offset
     *
     * @param other Source MapVector to copy from
     * @param positionOffset Starting index in this vector where data will be written
     * @param length Number of map entries to copy from source MapVector
     */
    void MapVector::Append(BaseVector *other, int positionOffset, int length)
    {
        auto *otherMapVector = dynamic_cast<MapVector *>(other);
        if (otherMapVector == nullptr) {
            std::string message = "Invalid vector type: expected MapVector for append operation.";
            throw OmniException("MAPVECTOR_TYPE_MISMATCH", message);
        }

        if (length <= 0) {
            return;
        }
        if (positionOffset < 0) {
            std::string message = "Invalid append position";
            throw OmniException("MAPVECTOR_APPEND_ERROR", message);
        }

        int32_t newSize = positionOffset + length;
        Expand(newSize);

        // calculate the total number of key-value pairs that need to be added
        int64_t totalKeyValuePairs = 0;
        for (int i = 0; i < length; i++) {
            int destIndex = positionOffset + i;
            if (!otherMapVector->IsNull(i)) {
                totalKeyValuePairs += otherMapVector->GetSize(i);
            }
        }

        // obtain the size of the current keys/values
        int64_t keysSize = GetOffset(positionOffset);

        // if there are any key-value pairs that need to be added
        if (totalKeyValuePairs > 0) {
            int64_t newKeysSize = keysSize + totalKeyValuePairs;

            // expand the capacity all at once, and then add the data
            keys->Expand(newKeysSize);
            values->Expand(newKeysSize);

            // record the writing position of the current keys/values
            int64_t currentKeysWritePos = keysSize;

            // traverse each entry of the map
            for (int i = 0; i < length; i++) {
                int newIndex = positionOffset + i;

                if (otherMapVector->IsNull(i)) {
                    SetNull(newIndex);
                } else {
                    // obtain the size and offset of the source entry
                    int64_t sourceSize = otherMapVector->GetSize(i);
                    int64_t sourceOffset = otherMapVector->GetOffset(i);

                    // set the size and offset of the current map entry
                    SetSize(newIndex, sourceSize);

                    // add the specific entries to the keys and values
                    if (sourceSize > 0) {
                        // obtain the slices of keys and values of the source map
                        auto sourceKeys = otherMapVector->GetKeyVector();
                        auto sourceKeysSlice = sourceKeys->Slice(sourceOffset, sourceSize, false);
                        auto sourceValues = otherMapVector->GetValueVector();
                        auto sourceValuesSlice = sourceValues->Slice(sourceOffset, sourceSize, false);

                        // append the slices to the current keys and values
                        VectorHelper::AppendVector(keys.get(), currentKeysWritePos, sourceKeysSlice, sourceSize);
                        VectorHelper::AppendVector(values.get(), currentKeysWritePos, sourceValuesSlice, sourceSize);

                        // update write position
                        currentKeysWritePos += sourceSize;

                        delete sourceKeysSlice;
                        delete sourceValuesSlice;
                    }
                }
            }
        } else {
            // there is no key-value pair, so just set null or an empty entry.
            for (int i = 0; i < length; i++) {
                int newIndex = positionOffset + i;
                if (otherMapVector->IsNull(i)) {
                    SetNull(newIndex);
                } else {
                    SetSize(newIndex, 0);
                }
            }
        }
    }

    void MapVector::SetValue(int index, MapVector* value)
    {
        if (value == nullptr) {
            SetNull(index);
            return;
        }
        if (keys->GetTypeId() != value->keys->GetTypeId() ||
            values->GetTypeId() != value->values->GetTypeId()) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", "MapVector type mismatch during SetValue.");
            }
        int64_t offset = GetOffset(index);
        int64_t keyLength = value->GetSize(0);
        if (keyLength == 0) {
            SetSize(index, 0);
            return;
        }
        VectorHelper::ExpandElementVector(keys.get(), keys->GetTypeId(), static_cast<int32_t>(offset + keyLength));
        VectorHelper::AppendVector(keys.get(), static_cast<int32_t>(offset), value->GetKeyVector().get(),
            static_cast<int32_t>(keyLength));
        VectorHelper::ExpandElementVector(values.get(), values->GetTypeId(), static_cast<int32_t>(offset + keyLength));
        VectorHelper::AppendVector(values.get(), static_cast<int32_t>(offset), value->GetValueVector().get(),
            static_cast<int32_t>(keyLength));
        SetSize(index, static_cast<int32_t>(keyLength));
    }

    std::pair<BaseVector*, BaseVector*> MapVector::GetValue(int index)
    {
        if (UNLIKELY(index < 0 || index >= size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", index,
                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
 
        int64_t startOffset = GetOffset(index);
        int64_t size = GetSize(index);
 
        BaseVector* k = GetKeyVector()->Slice(startOffset, size, false);
        BaseVector* v = GetValueVector()->Slice(startOffset, size, false);
        return std::make_pair(k, v);
    }
}