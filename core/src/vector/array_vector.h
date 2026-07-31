/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: arrayVector  implementation
 */

#ifndef OMNI_RUNTIME_ARRAY_VECTOR_H
#define OMNI_RUNTIME_ARRAY_VECTOR_H

#include <vector>
#include <memory>
#include "vector.h"

namespace omniruntime::vec {
class ArrayVector : public BaseVector {
public:
    ArrayVector(int64_t size, std::shared_ptr<BaseVector> elementVector)
        : BaseVector(size, OMNI_ENCODING_ARRAY, OMNI_ARRAY),
          elements(std::move(elementVector)), capacity(static_cast<int32_t>(size))
    {
        offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1, true);
        offsets = offsetsBuffer->GetBuffer();
    }

    ArrayVector(int64_t size)
        : BaseVector(size, OMNI_ENCODING_ARRAY, OMNI_ARRAY), capacity(static_cast<int32_t>(size))
    {
        offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1, true);
        offsets = offsetsBuffer->GetBuffer();
    }

    const std::shared_ptr<AlignedBuffer<int64_t>>& GetOffsetsBuffer() const
    {
        return offsetsBuffer;
    }

    int64_t* GetOffsets()
    {
        return offsets;
    }

    int64_t GetOffset(int64_t index)
    {
        return offsets[index];
    }

    // Return the number of elements in the array at the index-th position
    int64_t GetSize(int64_t index)
    {
        return offsets[index + 1] - offsets[index];
    }

    using BaseVector::GetSize;

    BaseVector* GetValue(int index);

    std::string GetValueToString(int index, const std::string_view &separator) const;

    const std::shared_ptr<BaseVector> GetElementVector() const
    {
        return elements;
    }

    void SetOffset(int32_t index, int32_t offset)
    {
        offsets[index] = offset;
    }

    void SetSize(int32_t index, int32_t size)
    {
        offsets[index + 1] = offsets[index] + size;
    }

    void SetElementVector(std::shared_ptr<BaseVector> elementVector)
    {
        elements = std::move(elementVector);
    }

    void SetElementVectorFromRaw(BaseVector* addedElements)
    {
        elements = std::shared_ptr<BaseVector>(addedElements);
    }

    void ALWAYS_INLINE SetNull(int64_t index)
    {
        BaseVector::SetNull(index);
        SetSize(index, 0);
    }

    void SetValue(int index, BaseVector* elements);

    /* *
     * Get the array at the specified index with optional copying.
     *
     * @param index The index of the array to retrieve
     * @param copy If true, return a copy instead of a view
     * @return std::shared_ptr<BaseVector> GetArrayAt(int64_t index, bool copy)
     */
    std::shared_ptr<BaseVector> GetArrayAt(int64_t index, bool copy = false);

    /* *
     * Create a new vector based on a slice of the vector. The returned Vector is
     * a read-only vector which shares data memory with the original vector,
     * if the vector data is modified, the original vector data is also modified.
     *
     * @param positionOffset
     * @param length
     * @param isCopy reserved parameters
     */
    ArrayVector *Slice(int positionOffset, int length, bool isCopy = false) override;

    /* *
     * Copies the values of the vector at the indicated positions
     * @param positions
     * @param offset
     * @param length
     */
    ArrayVector *CopyPositions(const int *positions, int positionOffset, int length);

    static void updateElementPositions(std::vector<int> &elementPositions, int index, int size)
    {
        for (int i = 0; i < size; ++i) {
            elementPositions.push_back(index++);
        }
    }

    void Expand(int32_t needCapacity) override;

    /* *
     * Append another arrayVector to the current arrayVector starting at a specified offset
     *
     * @param other Source ArrayVector to copy from
     * @param positionOffset Starting index in this vector where data will be written
     * @param length Number of array entries to copy from source ArrayVector
     */
    void Append(BaseVector *other, int positionOffset, int length);

protected:
    int64_t* offsets;
    std::shared_ptr<AlignedBuffer<int64_t>> offsetsBuffer;
    std::shared_ptr<BaseVector> elements;
    int32_t capacity;
};
}

#endif // OMNI_RUNTIME_ARRAY_VECTOR_H