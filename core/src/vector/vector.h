/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: vector  implementation
 */
#ifndef OMNI_RUNTIME_VECTOR_H
#define OMNI_RUNTIME_VECTOR_H
#pragma once

#include <cstdlib>
#include <functional>
#include <string>
#include <type_traits>
#include <jemalloc/jemalloc.h>
#include <memory>
#include "type_utils.h"
#include "dictionary_container.h"
#include "util/debug.h"
#include "util/bit_map.h"
#include "util/compiler_util.h"
#include "large_string_container.h"
#include "memory/aligned_buffer.h"
#include "type/data_type.h"
#include "nulls_buffer.h"

namespace omniruntime::vec::unsafe {
class UnsafeBaseVector;
class UnsafeVector;
class UnsafeDictionaryVector;
class UnsafeStringVector;
}

namespace omniruntime::vec {
using namespace type;
using namespace mem;
enum Encoding {
    OMNI_FLAT = 0,               // ordinary vector, storing primitive data types, such as int, long, boolean
    OMNI_DICTIONARY = 1,         // dictionary vector, dictionary can be combined with varchar
    OMNI_ENCODING_CONTAINER = 2, // the temporarily added code is mainly used for the agg avg partial, and the vector
                                 // implementation is also placed in the hash agg module
    OMNI_ENCODING_MAP = 3,
    OMNI_ENCODING_ARRAY = 4,
    OMNI_ENCODING_STRUCT = 5,
    OMNI_ENCODING_CONST = 6,
    OMNI_ENCODING_INVALID
};

template <typename T>
using tsan_atomic = T;

class BaseVector;
using VectorPtr = std::shared_ptr<BaseVector>;

class BaseVector {
public:
    BaseVector() = default;
    explicit BaseVector(int32_t size) : BaseVector(size, OMNI_FLAT /* * ordinary vector */) {}

    BaseVector(int32_t size, Encoding encoding, NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0)
        : size(size), encoding(encoding), offset(0), isSliced(false)
    {
        this->nullsBuffer = std::make_shared<NullsBuffer>(size, nullsBufferPtr, sliceOffset);
    }

    BaseVector(int32_t size, Encoding encoding, DataTypeId dataTypeId, NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0)
        : size(size), encoding(encoding), dataTypeId(dataTypeId), offset(0), isSliced(false)
    {
        this->nullsBuffer = std::make_shared<NullsBuffer>(size, nullsBufferPtr, sliceOffset);
    }

    virtual ~BaseVector() = default;

    /* *
     * set the element at the index position to null
     * Attention: String vector has its own SetNull, need call corresponding SetNull when string vector
     * @param index
     */
    void ALWAYS_INLINE SetNull(int32_t index)
    {
        nullsBuffer->SetNull(index, true);
    }

    void ALWAYS_INLINE SetNotNull(int32_t index)
    {
        nullsBuffer->SetNotNull(index);
    }

    void SetNulls(int startIndex, NullsBuffer *nullsPtr, int length)
    {
        if (UNLIKELY(startIndex + length > size)) {
            std::string message("vector is out of range(needed size:%d, real size:%d).", startIndex + length, size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        nullsBuffer->SetNulls(startIndex, nullsPtr, length);
    }

    void SetNulls(int startIndex, bool null, int length)
    {
        if (UNLIKELY(startIndex + length > size)) {
            std::string message("vector is out of range(needed size:%d, real size:%d).", startIndex + length, size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        nullsBuffer->SetNulls(startIndex, null, length);
    }

    /* *
     * determine is the element at the index position is null
     * @param index
     * @return true is null, otherwise non-null
     */
    bool ALWAYS_INLINE IsNull(int32_t index)
    {
        return nullsBuffer->IsNull(index);
    }

    Encoding ALWAYS_INLINE GetEncoding()
    {
        return encoding;
    }

    int32_t ALWAYS_INLINE GetSize()
    {
        return size;
    }

    void ALWAYS_INLINE SetNullFlag(bool newHasNull)
    {
        nullsBuffer->SetNullFlag(newHasNull);
    }

    bool ALWAYS_INLINE HasNull()
    {
        return nullsBuffer->HasNull();
    }

    int32_t GetNullCount()
    {
        return nullsBuffer->GetNullCount();
    }

    int32_t ALWAYS_INLINE GetOffset()
    {
        return offset;
    }

    virtual std::vector<DataTypeId> ALWAYS_INLINE GetTypeIds() const
    {
        OMNI_THROW("Vector Error:", "1");
    }

    DataTypeId ALWAYS_INLINE GetTypeId() const
    {
        return dataTypeId;
    }

    const std::shared_ptr<DataType>& GetDataType() const
    {
        return dataType_;
    }

    void SetDataType(std::shared_ptr<DataType> dt)
    {
        dataType_ = std::move(dt);
    }

    virtual BaseVector *CopyPositions(const int *positions, int positionOffset, int length)
    {
        return nullptr;
    }

    /**
     * using caller suggestedCapacity to reverse enough values buffer
    */
    virtual void Reserve(int32_t suggestedCapacity, bool copyNulls, bool copyValues)
    {
    }

    virtual BaseVector *Slice(int positionOffset, int length, bool isCopy = false)
    {
        return nullptr;
    }

    void SetOffset(int32_t offset)
    {
        this->offset = offset;
    }

    void SetSliced(bool isSliced)
    {
        this->isSliced = isSliced;
    }

    void SetIsField(const bool isField)
    {
        isField_ = isField;
    }

    bool GetIsField() const
    {
        return isField_;
    }

    virtual void Expand(int32_t needCapacity)
    {
        OMNI_THROW("Vector Error: ", "Expand method not implemented for this vector type");
    }

    NullsBuffer* GetNullsBuffer() const
    {
        return nullsBuffer.get();
    }

    /// @return the number of bytes required to hold this vector in memory
    int32_t inMemoryBytes() const
    {
        return inMemoryBytes_;
    }

    template <typename T>
    T* asUnchecked()
    {
        static_assert(std::is_base_of_v<BaseVector, T>);
        return static_cast<T*>(this);
    }

    template <typename T = BaseVector>
    static std::shared_ptr<T> create(
        const RowTypePtr& type,
        int32_t size,
        mem::MemoryPool* pool)
    {
        return std::static_pointer_cast<T>(createInternal(type, size, pool));
    }

    /// @return the number of rows of data in this vector
    int32_t size_() const
    {
        return length_;
    }

protected:
    bool isField_ = false;
    friend class unsafe::UnsafeBaseVector;
    int32_t size;
    Encoding encoding; // vector encoding, such as flat, dictionary
    int32_t offset = 0;
    std::shared_ptr<NullsBuffer> nullsBuffer;
    bool isSliced;
    DataTypeId dataTypeId;
    std::shared_ptr<DataType> dataType_ = nullptr;
    int32_t inMemoryBytes_ = 0;
    tsan_atomic<int32_t> length_{0};

private:
    static VectorPtr createInternal(
            const RowTypePtr& type,
            int32_t size,
            mem::MemoryPool* pool);
};

template <typename RAW_DATA_TYPE>
class ConstVector final : public BaseVector {
public:
    ConstVector(RAW_DATA_TYPE value, DataTypeId dataTypeId): value(value)
    {
        this->dataTypeId = dataTypeId;
        this->encoding = OMNI_ENCODING_CONST;
        this->nullsBuffer = std::make_shared<NullsBuffer>(1, nullptr, 0);
        MakeOwnedCopy();
    }

    ConstVector(RAW_DATA_TYPE value, DataTypeId dataTypeId, int32_t size)
        : BaseVector(size, OMNI_ENCODING_CONST, dataTypeId), value(value)
    {
        MakeOwnedCopy();
    }

    RAW_DATA_TYPE GetConstValue() const
    {
        return value;
    }

    const RAW_DATA_TYPE& GetConstValueRef() const
    {
        return value;
    }

    // ArrayVector::CopyPositions / Slice call these. The base implementations return nullptr.
    BaseVector *CopyPositions(const int *positions, int positionOffset, int length) override;
    BaseVector *Slice(int positionOffset, int length, bool isCopy = false) override;

    // Prevent copy/move to avoid dangling string_view after copy
    ConstVector(const ConstVector&) = delete;
    ConstVector& operator=(const ConstVector&) = delete;
    ConstVector(ConstVector&&) = delete;
    ConstVector& operator=(ConstVector&&) = delete;

private:
    bool ConstPositionIsNull(int position)
    {
        if (position >= 0 && position < this->GetSize()) {
            return this->IsNull(position);
        }
        return this->IsNull(0);
    }

    /**
     * For string_view types: copy the referenced string data into ownedData_ so that
     * the ConstVector owns its data and does not depend on external memory lifetime
     * (e.g., hiveSplit_->partitionKeys which may be freed between splits).
     * For all other types: this is a no-op (compiled away by if constexpr).
     */
    void MakeOwnedCopy()
    {
        if constexpr (std::is_same_v<RAW_DATA_TYPE, std::string_view>) {
            ownedData_ = std::string(value);
            value = std::string_view(ownedData_);
        }
    }

    RAW_DATA_TYPE value;
    std::string ownedData_;  // Only meaningful when RAW_DATA_TYPE is std::string_view
};

/**
 * The partially specialized implementation for primitive types without encoding
 * @tparam RAW_DATA_TYPE
 */
template <typename RAW_DATA_TYPE> class Vector : public BaseVector {
public:
    /* *
     * Constructor for data types of arithmetic types without encoding
     * @param vSize: size of array in variables nulls and values
     * @param dataTypeId: the dataTypeId of vector
     */
    Vector(int vSize, DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>) : BaseVector(vSize)
    {
        this->dataTypeId = dataTypeId;
        valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(vSize);
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        // vector class capacity, valuesBuffer class capacity and nullsBuffer class capacity
        int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer) + sizeof(AlignedBuffer<uint8_t>) +
            sizeof(AlignedBuffer<RAW_DATA_TYPE>);
        omniruntime::mem::ThreadMemoryManager::ReportMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    virtual ~Vector() override
    {
        if ((encoding == OMNI_FLAT) && (valuesBuffer != nullptr)) {
            int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer);
            if (!isSliced) {
                // vector class, nullsBuffer class and valuesBuffer class
                vectorCapacity += sizeof(AlignedBuffer<uint8_t>) + sizeof(AlignedBuffer<RAW_DATA_TYPE>);
            }
            omniruntime::mem::ThreadMemoryManager::ReclaimMemory(vectorCapacity);
            omniruntime::mem::MemoryTrace::SubVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
        }
    }

    // used for container vector, dictionary vector and varchar vector.
    Vector(int vSize, Encoding encoding, NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0)
        : BaseVector(vSize, encoding, nullsBufferPtr, sliceOffset)
    {
        capacity = vSize;
    }

    // used for vector slice, sliced vector use same values as parent vector
    Vector(int vSize, Encoding encoding, NullsBuffer *nullsBufferPtr,
        std::shared_ptr<AlignedBuffer<RAW_DATA_TYPE>> valuesBuffer, DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>,
        int32_t sliceOffset = 0) : BaseVector(vSize, encoding, nullsBufferPtr, sliceOffset)
    {
        this->dataTypeId = dataTypeId;
        this->valuesBuffer = valuesBuffer; // copy the data field shared_ptr
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer);
        omniruntime::mem::ThreadMemoryManager::ReportMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    /* *
     * Set the value at the indicated index
     * @param index
     * @param value
     */
    void ALWAYS_INLINE SetValue(int index, typename PARAM_TYPE<RAW_DATA_TYPE>::type value)
    {
        values[index + offset] = value;
    }

    /* *
     * Set the values from start index
     * @param startIndex
     * @param values
     * @param length
     */
    void ALWAYS_INLINE SetValues(int startIndex, const void *values, int length)
    {
        if (UNLIKELY(startIndex + length > size)) {
            std::string message("vector is out of range(needed size:%d, real size:%d).", startIndex + length, size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        RAW_DATA_TYPE *startAddr = reinterpret_cast<RAW_DATA_TYPE *>(this->values);
        memcpy(startAddr + startIndex, values, length * sizeof(RAW_DATA_TYPE));
    }

    /* *
     * Gets the value of the vector at the indicated index
     * @param index
     * @return
     */
    ALWAYS_INLINE typename PARAM_TYPE<RAW_DATA_TYPE>::type GetValue(int index) const
    {
        return values[index + offset];
    }

    /* *
     * append data to the current vector
     *
     * @param other the dst data from
     * @param positionOffset element position
     * @param length number of elements
     */
    void Append(BaseVector *other, int positionOffset, int length)
    {
        if (UNLIKELY(positionOffset + length > size)) {
            std::string message("append vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        if (other->GetEncoding() == OMNI_FLAT) {
            auto src = reinterpret_cast<Vector<RAW_DATA_TYPE> *>(other);
            SetNulls(positionOffset, src->nullsBuffer.get(), length);
            SetValues(positionOffset, src->values + src->offset, length);
        } else { // for dictionary
            auto src = reinterpret_cast<Vector<DictionaryContainer<RAW_DATA_TYPE>> *>(other);
            for (int32_t i = 0; i < length; i++) {
                auto index = i + positionOffset;
                nullsBuffer->SetNull(index, src->IsNull(i));
                SetValue(index, src->GetValue(i));
            }
        }
    }

    void Reserve(int32_t suggestedCapacity, bool copyNulls, bool copyValues) override
    {
        if(suggestedCapacity <= this->capacity || this->offset != 0) {
            return;
        }
        if (copyNulls && this->nullsBuffer != nullptr) {
            auto oldNullsBuffer = std::move(nullsBuffer);
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity);
            this->nullsBuffer->SetNulls(0, oldNullsBuffer.get(), size);
        } else {
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity);
        }
        if (copyValues && valuesBuffer != nullptr) {
            auto oldValuesBuffer = std::move(valuesBuffer);
            this->valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(suggestedCapacity);
            memcpy(this->valuesBuffer->GetBuffer(), oldValuesBuffer->GetBuffer(), this->size * sizeof(RAW_DATA_TYPE));
        } else {
            this->valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(suggestedCapacity);
        }
        values = valuesBuffer->GetBuffer();
        this->capacity = suggestedCapacity;
    }

    /* *
     * Copies the values of the vector at the indicated positions
     * @param positions
     * @param offset
     * @param length
     */
    BaseVector* CopyPositions(const int *positions, int positionOffset, int length) override
    {
        if (UNLIKELY((positions == nullptr) || (length < 0))) {
            std::string message("positions is null or the input length is incorrect: %d.", length);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto vector = new Vector<RAW_DATA_TYPE>(length, this->dataTypeId);
        auto startPositions = positions + positionOffset;
        for (int32_t i = 0; i < length; i++) {
            int position = startPositions[i];
            // when position = -1
            if (UNLIKELY(position == -1)) {
                vector->SetNull(i);
                continue;
            }

            if (UNLIKELY(IsNull(position))) {
                vector->nullsBuffer->SetNull(i);
            }
            vector->SetValue(i, GetValue(position));
        }
        return vector;
    }

    /* *
     * Create a new vector based on a slice of the vector. The returned Vector is
     * a read-only vector which shares data memory with the original vector,
     * if the vector data is modified, the original vector data is also modified.
     *
     * @param positionOffset
     * @param length
     * @param isCopy reserved parameters
     */
    BaseVector* Slice(int positionOffset, int length, bool isCopy = false) override
    {
        if (UNLIKELY(positionOffset + length > size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto sliced = new Vector<RAW_DATA_TYPE>(length, encoding, nullsBuffer.get(), valuesBuffer, dataTypeId,
            positionOffset);
        sliced->offset = offset + positionOffset; // update offset
        sliced->isSliced = true;
        return sliced;
    }

    RAW_DATA_TYPE * GetValuesBuffer() {
        return values;
    }

    ALWAYS_INLINE void Expand(int32_t needCapacity)
    {
        if(needCapacity <= size){
            return;
        }

        if (needCapacity <= capacity) {
            size = needCapacity;
            return;
        }

        int32_t newCapacity = std::max(capacity * 2, needCapacity);
        int32_t oldSize = size;
        auto oldValuesBuffer = valuesBuffer;
        auto oldValues = values;

        valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(newCapacity);
        values = valuesBuffer->GetBuffer();

        if (oldValues != nullptr) {
            memcpy(values, oldValues, oldSize * sizeof(RAW_DATA_TYPE));
        }

        auto oldNullsBuffer = nullsBuffer;
        nullsBuffer = std::make_shared<NullsBuffer>(newCapacity);
        if (oldNullsBuffer != nullptr) {
            nullsBuffer->SetNulls(0, oldNullsBuffer.get(), oldSize);
        }
        capacity = newCapacity;
        size = needCapacity;
    }

protected:
    friend class unsafe::UnsafeVector;
    std::shared_ptr<AlignedBuffer<RAW_DATA_TYPE>> valuesBuffer; // manage values memory and it's metadata
    RAW_DATA_TYPE *values; // valuesBuffer->GetBuffer(), for primitive types without encoding
    int32_t capacity = 0;
};

/**
 * dictionary encoding implementation
 * @tparam RAW_DATA_TYPE
 */
template <typename RAW_DATA_TYPE, template <typename> typename CONTAINER>
class Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> final : public Vector<RAW_DATA_TYPE> {
public:
    Vector(int size, std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> container,
        NullsBuffer *nullsBufferPtr, DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>, int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_DICTIONARY, nullsBufferPtr, sliceOffset), container(container)
    {
        this->dataTypeId = dataTypeId;
        // vector class capacity, nullsBuffer class capacity and container total capacity.
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        omniruntime::mem::ThreadMemoryManager::ReportMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    // used for vector slice, sliced vector use same container as parent vector
    Vector(int size, std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> container,
        NullsBuffer *nullsBufferPtr, bool isSliced, DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>,
        int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_DICTIONARY, nullsBufferPtr, sliceOffset), container(container)
    {
        this->dataTypeId = dataTypeId;
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer);
        omniruntime::mem::ThreadMemoryManager::ReportMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    ~Vector() override
    {
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer);
        if (!this->isSliced) {
            vectorCapacity += sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        }
        omniruntime::mem::ThreadMemoryManager::ReclaimMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::SubVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    /* *
     * Set the value at the indicated index
     * @param index
     * @param value
     */
    void ALWAYS_INLINE SetValue(int index, typename PARAM_TYPE<RAW_DATA_TYPE>::type value)
    {
        container->SetValue(index, value);
    }

    /* *
     * Gets the value of the vector at the indicated index
     * @param index
     * @return
     */
    ALWAYS_INLINE typename PARAM_TYPE<RAW_DATA_TYPE>::type GetValue(int index)
    {
        return container->GetValue(index + this->offset);
    }

    /* *
     * Copies the values of the vector at the indicated positions
     * @param positions
     * @param offset
     * @param length
     */
    Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> *CopyPositions(const int *positions, int positionOffset,
        int length)
    {
        if (UNLIKELY((positions == nullptr) || (length < 0))) {
            std::string message("positions is null or the input length is incorrect: %d.", length);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        // new nulls
        std::unique_ptr<NullsBuffer> newNullsBuffer = std::make_unique<NullsBuffer>(length);
        // new positions
        std::vector<int32_t> newPositions(length);
        auto startPositions = positions + positionOffset;
        for (int32_t i = 0; i < length; i++) {
            auto position = startPositions[i];
            if (UNLIKELY(this->IsNull(position))) {
                newNullsBuffer->SetNull(i);
            }
            newPositions[i] = position + this->offset;
        }
        // new container
        std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> newContainer =
            container->CopyPositions(newPositions.data(), length);
        return new Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>(length, newContainer, newNullsBuffer.get(),
                                                                         this->dataTypeId);
    }

    /* *
     * Create a new vector based on a slice of the vector. The returned Vector is
     * a read-only vector which shares data memory with the original vector,
     * if the vector data is modified, the original vector data is also modified.
     *
     * @param positionOffset
     * @param length
     * @param isCopy
     */
    Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> *Slice(int positionOffset, int length, bool isCopy = false)
    {
        if (UNLIKELY(positionOffset + length > this->size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                this->size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        // copy the data field shared_ptr
        auto sliced = new Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>(length, this->container,
            this->nullsBuffer.get(), true, this->dataTypeId, positionOffset);
        sliced->offset = this->offset + positionOffset; // update offset
        sliced->isSliced = true;
        return sliced;
    }

private:
    friend class unsafe::UnsafeDictionaryVector;
    std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> container;
};

/**
 * The master template for the vector class supporting string encoding
 * @tparam string_view : meta of string
 */
template <typename RAW_DATA_TYPE>
class Vector<LargeStringContainer<RAW_DATA_TYPE>> final : public Vector<RAW_DATA_TYPE> {
public:
    explicit Vector(int size, int capacityInBytes = INITIAL_STRING_SIZE,
        DataTypeId stringDataTypeId = OMNI_VARCHAR)
        : Vector<RAW_DATA_TYPE>(size, OMNI_FLAT)
    {
        this->dataTypeId = stringDataTypeId;
        // default string_view vector use large string encoding
        this->container = std::make_shared<LargeStringContainer<std::string_view>>(size, capacityInBytes);
        // vector class capacity, nullsBuffer class capacity and values total capacity
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        omniruntime::mem::ThreadMemoryManager::ReportMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    // used for vector slice, sliced vector use same container as parent vector
    explicit Vector(int size, std::shared_ptr<LargeStringContainer<std::string_view>> container,
        NullsBuffer *nullsBufferPtr, DataTypeId dataTypeId = OMNI_VARCHAR, int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_FLAT, nullsBufferPtr, sliceOffset), container(container)
    {
        this->dataTypeId = dataTypeId;
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer);
        omniruntime::mem::ThreadMemoryManager::ReportMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    ~Vector() override
    {
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer);
        if (!this->isSliced) {
            vectorCapacity += sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        }
        omniruntime::mem::ThreadMemoryManager::ReclaimMemory(vectorCapacity);
        omniruntime::mem::MemoryTrace::SubVectorMemory(reinterpret_cast<uintptr_t>(this), vectorCapacity);
    }

    /* *
     * set the element at the index position to null
     * @param index
     */
    void ALWAYS_INLINE SetNull(int32_t index)
    {
        BaseVector::SetNull(index);
        container->SetNull(index);
    }

    /* *
     * Set the value at the indicated index
     * @param index
     * @param value
     */
    void ALWAYS_INLINE SetValue(int index, const std::string_view &value)
    {
        container->SetValue(index, value);
    }

    /* *
     * Gets the value of the vector at the indicated index
     * @param index
     * @return
     */
    ALWAYS_INLINE std::string_view GetValue(int index) const
    {
        return container->GetValue(index + this->offset);
    }

    /** Reset string content so next SetValue starts from offset 0. Used when reusing vector for spill. */
    void ResetForReuse()
    {
        if (container && !this->isSliced) {
            container->ResetForReuse(this->size);
        }
    }

    /* *
     * append data to the current vector
     *
     * @param other the dst data from
     * @param positionOffset element position
     * @param length number of elements
     */
    void Append(BaseVector *other, int positionOffset, int length)
    {
        if (UNLIKELY(positionOffset + length > this->size)) {
            std::string message("append vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                this->size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        if (other->GetEncoding() == OMNI_FLAT) {
            auto src = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(other);
            for (int32_t i = 0; i < length; i++) {
                bool isNull = src->IsNull(i);
                auto index = i + positionOffset;
                if (!isNull) {
                    auto value = src->GetValue(i);
                    SetValue(index, value);
                } else {
                    SetNull(index);
                }
            }
        } else { // for dictionary
            auto src = reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(other);
            for (int32_t i = 0; i < length; i++) {
                bool isNull = src->IsNull(i);
                auto index = i + positionOffset;
                if (!isNull) {
                    auto value = src->GetValue(i);
                    SetValue(index, value);
                } else {
                    SetNull(index);
                }
            }
        }
    }

    /* *
     * Copies the values of the vector at the indicated positions
     * @param positions
     * @param offset
     * @param length
     */
    BaseVector* CopyPositions(const int *positions, int offset, int length) override
    {
        if (UNLIKELY((positions == nullptr) || (length < 0))) {
            std::string message("positions is null or the input length is incorrect: %d.", length);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        auto vector = new Vector<LargeStringContainer<std::string_view>>(length, INITIAL_STRING_SIZE, this->dataTypeId);
        auto startPositions = positions + offset;
        for (int32_t i = 0; i < length; i++) {
            auto position = startPositions[i];
            if (this->IsNull(position)) {
                vector->SetNull(i);
            } else {
                auto value = GetValue(position);
                vector->SetValue(i, value);
            }
        }
        return vector;
    }

    /* *
     * Create a new vector based on a slice of the vector. The returned Vector is
     * a read-only vector which shares data memory with the original vector,
     * if the vector data is modified, the original vector data is also modified.
     *
     * @param positionOffset
     * @param length
     */
    BaseVector* Slice(int positionOffset, int length, bool isCopy = false) override
    {
        if (UNLIKELY(positionOffset + length > this->size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                this->size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        // copy the data field shared_ptr
        auto sliced = new Vector<LargeStringContainer<std::string_view>>(length, container, this->nullsBuffer.get(),
            this->dataTypeId, positionOffset);
        sliced->SetOffset(this->offset + positionOffset); // update offset
        sliced->SetSliced(true);
        return std::move(sliced);
    }

    ALWAYS_INLINE void Expand(int32_t needCapacity)
    {
        if(needCapacity <= this->size){
            return;
        }

        if (needCapacity <= this->capacity) {
            this->size = needCapacity;
            container->Expand(needCapacity);
            return;
        }

        int32_t newCapacity = std::max(this->capacity * 2, needCapacity);
        int32_t oldSize = this->size;

        auto oldNullsBuffer = this->nullsBuffer;
        this->nullsBuffer = std::make_shared<NullsBuffer>(newCapacity);
        if (oldNullsBuffer != nullptr) {
            this->nullsBuffer->SetNulls(0, oldNullsBuffer.get(), oldSize);
        }
        this->capacity = newCapacity;
        this->size = needCapacity;
        container->Expand(needCapacity);
    }

    void Reserve(int32_t suggestedCapacity, bool copyNulls, bool copyValues) override
    {
        if(suggestedCapacity <= this->capacity || this->offset != 0) {
            return;
        }
        if (copyNulls && this->nullsBuffer != nullptr) {
            auto oldNullsBuffer = std::move(this->nullsBuffer);
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity);
            this->nullsBuffer->SetNulls(0, oldNullsBuffer.get(), this->size);
        } else {
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity);
        }
        this->capacity = suggestedCapacity;
        container->Expand(suggestedCapacity);
    }

private:
    friend class unsafe::UnsafeStringVector;
    std::shared_ptr<LargeStringContainer<std::string_view>> container;
};

template <typename RAW_DATA_TYPE>
BaseVector *ConstVector<RAW_DATA_TYPE>::CopyPositions(const int *positions, int positionOffset, int length)
{
    if (UNLIKELY((positions == nullptr) || (length < 0))) {
        std::string message("positions is null or the input length is incorrect: %d.", length);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }
    auto *startPositions = positions + positionOffset;
    if constexpr (std::is_same_v<RAW_DATA_TYPE, std::string_view>) {
        auto *vector = new Vector<LargeStringContainer<std::string_view>>(length, INITIAL_STRING_SIZE, this->dataTypeId);
        for (int32_t i = 0; i < length; ++i) {
            int position = startPositions[i];
            if (UNLIKELY(position == -1) || ConstPositionIsNull(position)) {
                vector->SetNull(i);
                continue;
            }
            vector->SetValue(i, value);
        }
        return vector;
    } else {
        auto *vector = new Vector<RAW_DATA_TYPE>(length, this->dataTypeId);
        for (int32_t i = 0; i < length; ++i) {
            int position = startPositions[i];
            if (UNLIKELY(position == -1) || ConstPositionIsNull(position)) {
                vector->SetNull(i);
                continue;
            }
            vector->SetValue(i, value);
        }
        return vector;
    }
}

template <typename RAW_DATA_TYPE>
BaseVector *ConstVector<RAW_DATA_TYPE>::Slice(int positionOffset, int length, bool /*isCopy*/)
{
    if (UNLIKELY(positionOffset < 0 || length < 0 ||
        (this->GetSize() > 0 && positionOffset + length > this->GetSize()))) {
        std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
            this->GetSize());
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }
    if constexpr (std::is_same_v<RAW_DATA_TYPE, std::string_view>) {
        auto *vector = new Vector<LargeStringContainer<std::string_view>>(length, INITIAL_STRING_SIZE, this->dataTypeId);
        for (int32_t i = 0; i < length; ++i) {
            int position = positionOffset + i;
            if (ConstPositionIsNull(position)) {
                vector->SetNull(i);
                continue;
            }
            vector->SetValue(i, value);
        }
        return vector;
    } else {
        auto *vector = new Vector<RAW_DATA_TYPE>(length, this->dataTypeId);
        for (int32_t i = 0; i < length; ++i) {
            int position = positionOffset + i;
            if (ConstPositionIsNull(position)) {
                vector->SetNull(i);
                continue;
            }
            vector->SetValue(i, value);
        }
        return vector;
    }
}
}

#endif // OMNI_RUNTIME_VECTOR_H
