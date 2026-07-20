/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: arrayVector  implementation
 */

#include "array_vector.h"
#include "vector_helper.h"

namespace omniruntime::vec {

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

/**
 * Array<String> => String
 * @param index
 * @param separator separator is not nullptr
 * @return
 */
std::string ArrayVector::GetValueToString(int index, const std::string_view &separator) const {
    auto vector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(elements.get());
    std::string result = "";
    bool first = false;
    for (int32_t i = offsets[index]; i < offsets[index + 1]; ++i) {
        if (first) {
            if (!vector->IsNull(i)) {
                result += separator;
                result += vector->GetValue(i);
            }
        } else {
            if (!vector->IsNull(i)) {
                result = vector->GetValue(i);
                first = true;
            }
        }
    }
    return result;
}

void ArrayVector::SetValue(int index, BaseVector *value)
{
    if (value == nullptr) {
        SetNull(index);
        return;
    }

    int valueSize = value->GetSize();
    if (valueSize > 0) {
        int elementVectorSize = GetOffset(index);
        if (elements.get() == nullptr) {
            std::string message("elementVector of ArrayVector is nullptr, please SetElementVector first.");
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        elements->Expand(elementVectorSize + valueSize);
        VectorHelper::AppendVector(elements.get(), elementVectorSize, value, valueSize);
    }
    SetSize(index, valueSize);
}

std::shared_ptr<BaseVector> ArrayVector::GetArrayAt(int64_t index, bool copy)
{
    if (UNLIKELY(index < 0 || index >= size)) {
        std::string message("index out of range(needed size:%d, real size:%d).", index,
            size);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    if (IsNull(index)) {
        return nullptr;
    }

    int64_t startOffset = GetOffset(index);
    int64_t arraySize = GetSize(index);

    return std::shared_ptr<BaseVector>(GetElementVector()->Slice(startOffset, arraySize, false));
}

ArrayVector *ArrayVector::Slice(int positionOffset, int length, bool isCopy)
{
    if (UNLIKELY(positionOffset + length > size)) {
        std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
            size);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }
    auto sliced = new ArrayVector(length);
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
    sliced->SetElementVector(std::shared_ptr<BaseVector>(GetElementVector()->Slice(startOffset, sliced->GetOffset(length), isCopy)));
    return sliced;
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
        std::string message = "ArrayVector positions is null or the input length is incorrect: " + std::to_string(length) + ".";
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
        newArrayVector->SetElementVectorFromRaw(VectorHelper::CreateComplexVector(elementDataType.get(), elementLength));
    } else {
        auto newElementVector = elementVector->CopyPositions(elementPositions.data(), 0, elementLength);
        newArrayVector->SetElementVectorFromRaw(newElementVector);
    }

    return newArrayVector;
}

void ArrayVector::Expand(int32_t needCapacity)
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

void ArrayVector::Append(BaseVector *other, int positionOffset, int length)
{
    auto *otherArrayVector = static_cast<ArrayVector *>(other);

    if (length <= 0) {
        return;
    }
    if (positionOffset < 0) {
        std::string message = "Invalid append position";
        throw OmniException("ARRAYVECTOR_APPEND_ERROR", message);
    }

    int32_t newSize = positionOffset + length;
    Expand(newSize);

    int newIndex = positionOffset;
    for (int i = 0; i < length; i++) {
        if (otherArrayVector->IsNull(i)) {
            SetNull(newIndex);
        } else {
            auto tmp = otherArrayVector->GetValue(i);
            SetValue(newIndex, tmp);
            delete tmp;
        }
        newIndex++;
    }
}
}