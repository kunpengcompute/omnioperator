/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: vector  implementation
 */
#ifndef OMNI_RUNTIME_VECTOR_H
#define OMNI_RUNTIME_VECTOR_H
#pragma once

#include <algorithm>
#include <cstdlib>
#include <limits>
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
#include "string_view.h"
#include "memory/aligned_buffer.h"
#include "memory/omni_task_memory_pool.h"
#include "type/data_type.h"
#include "nulls_buffer.h"
#include "libboundscheck/include/securec.h"

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

    BaseVector(int32_t size, Encoding encoding, NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0,
        omniruntime::mem::Allocator *allocator = nullptr)
        : size(size), encoding(encoding), offset(0), isSliced(false)
    {
        this->nullsBuffer = std::make_shared<NullsBuffer>(size, nullsBufferPtr, sliceOffset, allocator);
    }

    BaseVector(int32_t size, Encoding encoding, NullsBuffer *nullsBufferPtr, int32_t sliceOffset,
        omniruntime::mem::Allocator *allocator,
        const OmniTaskMemoryPool::AllocationAccountingScope &scope)
        : size(size), encoding(encoding), offset(0), isSliced(false)
    {
        this->nullsBuffer = std::make_shared<NullsBuffer>(size, nullsBufferPtr, sliceOffset, allocator, scope);
    }

    BaseVector(int32_t size, Encoding encoding, DataTypeId dataTypeId, NullsBuffer *nullsBufferPtr = nullptr,
        int32_t sliceOffset = 0, omniruntime::mem::Allocator *allocator = nullptr)
        : size(size), encoding(encoding), dataTypeId(dataTypeId), offset(0), isSliced(false)
    {
        // Thread the per-task pool allocator into the validity NullsBuffer so the
        // complex-wrapper (Array/Map/Row) top-level nulls are pool-accounted (#15).
        // allocator==nullptr keeps the no-pool bridge for legacy callers.
        this->nullsBuffer = std::make_shared<NullsBuffer>(size, nullsBufferPtr, sliceOffset, allocator);
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

    // Polymorphic per-task pool accessor. Pool-bound vector implementations override this
    // to return their construction-time pool_. No-pool bridge vectors return nullptr.
    // Appended to the vtable tail — existing virtual slot offsets are unchanged (ABI-safe).
    virtual omniruntime::mem::OmniTaskMemoryPool *GetPool() const
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

    // Pool-bound variant. The only heap allocation a ConstVector performs is its
    // validity (nulls) buffer; routing the per-task pool's bound allocator into
    // BaseVector charges that buffer to the pool (symmetric SubMemory on free via
    // the bound allocator). pool may be nullptr (legacy / test callers with no
    // per-task pool context) → falls back to the no-pool NullsBuffer bridge,
    // accounting skipped.
    ConstVector(RAW_DATA_TYPE value, DataTypeId dataTypeId, int32_t size,
        omniruntime::mem::OmniTaskMemoryPool *pool)
        : BaseVector(size, OMNI_ENCODING_CONST, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              (pool != nullptr ? pool->getBoundAllocator() : nullptr)),
          value(value),
          pool_(pool)
    {
        this->dataTypeId = dataTypeId;
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

    omniruntime::mem::OmniTaskMemoryPool *GetPool() const override
    {
        return pool_;
    }

    // Prevent copy/move to avoid dangling string_view after copy
    ConstVector(const ConstVector&) = delete;
    ConstVector& operator=(const ConstVector&) = delete;
    ConstVector(ConstVector&&) = delete;
    ConstVector& operator=(ConstVector&&) = delete;

private:
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
    // Construction-time task pool. nullptr for legacy/no-pool ConstVectors.
    omniruntime::mem::OmniTaskMemoryPool *pool_ = nullptr;
};

/**
 * The partially specialized implementation for primitive types without encoding
 * @tparam RAW_DATA_TYPE
 */
template <typename RAW_DATA_TYPE> class Vector : public BaseVector {
public:
    // No-pool bridge ctor. Required by vector_helper.h DecodeFlatDictionaryVector
    // (helper allocates a result Vector without access to a pool). Do not delete
    // until DecodeFlatDictionaryVector / DecodeVarcharDictionaryVector callers are
    // pool-threaded.
    [[deprecated("no-pool bridge; vector_helper.h DecodeFlatDictionaryVector / DecodeVarcharDictionaryVector callers still need pool-threading")]]
    explicit Vector(int vSize, DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>)
        : BaseVector(vSize), pool_(nullptr)
    {
        static omniruntime::mem::Allocator nullBridge{nullptr};
        this->dataTypeId = dataTypeId;
        valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(vSize, &nullBridge);
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + sizeof(AlignedBuffer<RAW_DATA_TYPE>);
    }

    /* *
     * Pool-bound primary ctor. Mirrors Velox Buffer::pool_ const member invariant:
     * pool_ is set once at construction; ~Vector() always routes SubMemory to this
     * pool, regardless of calling thread's TLS scope.
     *
     * pool is non-owning; pool lifetime (Spark task) outlives operator-owned Vectors.
     * pool must not be null; all call sites supply a real pool from ExecutionContext.
     *
     * @param vSize  number of elements
     * @param pool   owning per-task memory pool (non-null, non-owning)
     * @param dataTypeId  optional override (defaults to TYPE_ID<RAW_DATA_TYPE>)
     */
    Vector(int vSize, omniruntime::mem::OmniTaskMemoryPool *pool,
        DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>)
        // Thread the per-task pool's bound allocator into the NullsBuffer alloc so the
        // validity buffer is pool-accounted instead of going through the [[deprecated]]
        // no-pool bridge. Values buffer is pool-bound below via allocForCtor.
        : BaseVector(vSize, OMNI_FLAT, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              (pool != nullptr ? pool->getBoundAllocator() : nullptr)),
          pool_(pool)
    {
        this->dataTypeId = dataTypeId;
        // nullptr-pool sentinel: VectorHelper helpers may receive pool=nullptr when
        // invoked through the no-pool bridge chain (CreateComplexVector(DataType*, int32_t)
        // → CreateFlatVector(pool=nullptr)). Route allocator through nullBridge;
        // per-task accounting skipped until those callers are pool-threaded.
        static omniruntime::mem::Allocator nullBridgePrimaryCtor{nullptr};
        omniruntime::mem::Allocator *allocForCtor =
            (pool_ != nullptr) ? pool_->getBoundAllocator() : &nullBridgePrimaryCtor;
        valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(vSize, allocForCtor);
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        // vector class capacity, valuesBuffer class capacity and nullsBuffer class capacity
        int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer) + sizeof(AlignedBuffer<uint8_t>) +
            sizeof(AlignedBuffer<RAW_DATA_TYPE>);
        if (pool_ != nullptr) {
            pool_->AddMemory(vectorCapacity);   // direct — no TLS (Velox pool_ invariant, const after ctor)
        }
    }

    Vector(int vSize, const OmniTaskMemoryPool::AllocationAccountingScope &scope,
        DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>)
        // The scope commits each charge; buffers keep their persistent allocator.
        : BaseVector(vSize, OMNI_FLAT, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              scope.pool()->getBoundAllocator(), scope),
          pool_(scope.pool())
    {
        this->dataTypeId = dataTypeId;
        auto *allocForCtor = pool_->getBoundAllocator();
        valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(vSize, allocForCtor, false, scope);
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        // vector class capacity, valuesBuffer class capacity and nullsBuffer class capacity
        int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer) + sizeof(AlignedBuffer<uint8_t>) +
            sizeof(AlignedBuffer<RAW_DATA_TYPE>);
        if (pool_ != nullptr) {
            pool_->AddMemoryWithScope(vectorCapacity, scope);   // direct — no TLS (Velox pool_ invariant, const after ctor)
        }
    }

    virtual ~Vector() override
    {
        if ((encoding == OMNI_FLAT) && (valuesBuffer != nullptr)) {
            int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer);
            if (!isSliced) {
                // vector class, nullsBuffer class and valuesBuffer class
                vectorCapacity += sizeof(AlignedBuffer<uint8_t>) + sizeof(AlignedBuffer<RAW_DATA_TYPE>);
            }
            if (pool_ != nullptr) {
                // Pool-bound path: routes SubMemory to the construction-time pool,
                // never consulting TLS — cross-scope free is structurally impossible.
                pool_->SubMemory(vectorCapacity);
            }
            // pool_==nullptr: no-pool bridge instance — accounting skipped.
        }
    }

    // Legacy encoding-variant no-pool ctor RETIRED.
    // Container / dictionary / varchar subclasses now MUST construct via the pool-bound
    // encoding variant below.

    // Pool-bound variant for container/dictionary/varchar subclasses.
    // Used by ContainerVector and specialization ctors that inherit from Vector<T>.
    Vector(int vSize, Encoding encoding, omniruntime::mem::OmniTaskMemoryPool *pool,
        NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0)
        // Thread the per-task pool's bound allocator into the validity NullsBuffer so the
        // varchar/dictionary vectors' null bitmaps are pool-accounted. When
        // nullsBufferPtr!=nullptr the NullsBuffer shares the parent buffer (allocator
        // unused), so this is inert for the dict-slice path; otherwise it is purely
        // additive — this ctor charges no bitmap bytes itself, so no double-count.
        : BaseVector(vSize, encoding, nullsBufferPtr, sliceOffset,
              (pool != nullptr ? pool->getBoundAllocator() : nullptr)),
          pool_(pool)
    {
        capacity = vSize;
    }

    // Legacy slice ctor (no-pool, TLS-bridge) RETIRED.
    // All slices now MUST inherit the parent's pool via the pool-bound slice ctor below
    // (preserves dtor-side SubMemory routing symmetry).

    // Pool-bound slice ctor. Slice inherits the parent's pool so its SubMemory in
    // the dtor goes to the same pool as the parent's AddMemory.
    //
    // pool MAY be nullptr when parent was constructed through a no-pool bridge path
    // (shuffle reader / JNI input iterator vectors are created Java-side with no
    // per-task pool). The slice dtor already tolerates pool_==nullptr (skips
    // SubMemory); the AddMemory side is symmetric to avoid SIGSEGV at construction.
    // Without this guard, every Projection::Project on a shuffle-deserialized vector
    // crashes at slice time.
    Vector(int vSize, Encoding encoding, NullsBuffer *nullsBufferPtr,
        std::shared_ptr<AlignedBuffer<RAW_DATA_TYPE>> valuesBuffer,
        omniruntime::mem::OmniTaskMemoryPool *pool,
        DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>,
        int32_t sliceOffset = 0) : BaseVector(vSize, encoding, nullsBufferPtr, sliceOffset), pool_(pool)
    {
        this->dataTypeId = dataTypeId;
        this->valuesBuffer = valuesBuffer; // copy the data field shared_ptr
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        // vector class capacity (slice only charges Vector + NullsBuffer; shared buffer not re-charged)
        int64_t vectorCapacity = sizeof(Vector<RAW_DATA_TYPE>) + sizeof(NullsBuffer);
        if (pool_ != nullptr) {
            pool_->AddMemory(vectorCapacity);   // direct — no TLS
        }
        // pool_==nullptr: no-pool bridge / shuffle path — per-task accounting skipped.
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
        // nullptr-pool sentinel: if Vector was constructed via a no-pool bridge ctor
        // or shuffle path (pool_=nullptr), route through nullBridge for symmetric dtor
        // Free until those callers are pool-threaded. Hoisted above the nulls block so
        // the reallocated validity bitmap is pool-accounted too.
        static omniruntime::mem::Allocator nullBridgeReserve{nullptr};
        omniruntime::mem::Allocator *allocForReserve = (pool_ != nullptr) ? pool_->getBoundAllocator() : &nullBridgeReserve;
        if (copyNulls && this->nullsBuffer != nullptr) {
            auto oldNullsBuffer = std::move(nullsBuffer);
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity, nullptr, 0, allocForReserve);
            this->nullsBuffer->SetNulls(0, oldNullsBuffer.get(), size);
        } else {
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity, nullptr, 0, allocForReserve);
        }
        if (copyValues && valuesBuffer != nullptr) {
            auto oldValuesBuffer = std::move(valuesBuffer);
            this->valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(
                suggestedCapacity, allocForReserve);
            memcpy(this->valuesBuffer->GetBuffer(), oldValuesBuffer->GetBuffer(), this->size * sizeof(RAW_DATA_TYPE));
        } else {
            this->valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(
                suggestedCapacity, allocForReserve);
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
        // Inherit parent's pool (pool-bound only path).
        auto vector = new Vector<RAW_DATA_TYPE>(length, pool_);
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
        // Pool-bound only. The legacy TLS-bridge slice ctor was retired; parents now
        // always carry a non-null pool, so the slice inherits it directly for
        // dtor-side accounting symmetry.
        Vector<RAW_DATA_TYPE> *sliced = new Vector<RAW_DATA_TYPE>(length, encoding, nullsBuffer.get(),
            valuesBuffer, pool_, dataTypeId, positionOffset);
        sliced->offset = offset + positionOffset; // update offset
        sliced->isSliced = true;
        return sliced;
    }

    RAW_DATA_TYPE * GetValuesBuffer() {
        return values;
    }

    // Per-task pool this vector was constructed with (may be nullptr for no-pool
    // bridge / shuffle-deserialized vectors). Lets helpers (e.g. VectorHelper::SliceVector
    // container branch) recover the source pool and thread it into derived vectors
    // without a no-pool [[deprecated]] bridge ctor.
    omniruntime::mem::OmniTaskMemoryPool *GetPool() const override {
        return pool_;
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

        // nullptr-pool sentinel for no-pool bridge ctor / shuffle compat.
        static omniruntime::mem::Allocator nullBridgeExpand{nullptr};
        omniruntime::mem::Allocator *allocForExpand = (pool_ != nullptr) ? pool_->getBoundAllocator() : &nullBridgeExpand;
        valuesBuffer = std::make_shared<AlignedBuffer<RAW_DATA_TYPE>>(newCapacity, allocForExpand);
        values = valuesBuffer->GetBuffer();

        if (oldValues != nullptr) {
            memcpy(values, oldValues, oldSize * sizeof(RAW_DATA_TYPE));
        }

        auto oldNullsBuffer = nullsBuffer;
        // Reuse allocForExpand so the reallocated validity bitmap is pool-accounted
        // alongside the values buffer.
        nullsBuffer = std::make_shared<NullsBuffer>(newCapacity, nullptr, 0, allocForExpand);
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

    // Pool-bound memory tracker. Non-owning pointer; set at ctor, never reassigned
    // (Velox const pool_ invariant). Pool lifetime (Spark task) strictly outlives
    // operator-owned Vectors. nullptr for Vectors constructed via the no-pool bridge ctor.
    omniruntime::mem::OmniTaskMemoryPool *pool_;
};

/**
 * dictionary encoding implementation
 * @tparam RAW_DATA_TYPE
 */
template <typename RAW_DATA_TYPE, template <typename> typename CONTAINER>
class Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> final : public Vector<RAW_DATA_TYPE> {
public:
    // Disambiguates the reduced-charge slice ctor (below) from the full-charge primary
    // ctor: both take an existing container shared_ptr, so their parameter lists would
    // otherwise collide. Mirrors the dedicated LargeString slice ctor.
    struct SliceCtorTag {};

    // Pool-bound primary ctor.
    Vector(int size, std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> container,
        NullsBuffer *nullsBufferPtr, omniruntime::mem::OmniTaskMemoryPool *pool,
        DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>, int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_DICTIONARY, pool, nullsBufferPtr, sliceOffset), container(container)
    {
        this->dataTypeId = dataTypeId;
        // vector class capacity, nullsBuffer class capacity and container total capacity.
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        // nullptr-pool sentinel: dict primary ctor is reached via Slice()/CopyPositions()
        // on parents constructed through the no-pool bridge ctor (OmniColReader.cc:914
        // string-dict reader, VectorHelper::CreateStringDictionary, vector_helper.h:70)
        // which set pool_=nullptr. The dtor already guards (pool_ != nullptr); make
        // AddMemory symmetric. Without this guard, every Project(dict-string-vector) on
        // an ORC-scanned shuffle/scan input SIGSEGVs at the atomic-add in usedBytes_.
        // e.g. TopNWithExprOperator::AddInput → OperatorUtil::ProjectVectors → Slice → here.
        if (this->GetPool() != nullptr) {
            this->GetPool()->AddMemory(vectorCapacity);   // direct — no TLS
        }
        // pool_==nullptr: no-pool bridge path — accounting skipped until callers are pool-threaded.
    }

    // No-pool bridge for dictionary variant. Required by vector_helper.h CreateDictionary
    // (helper allocates a dictionary Vector without access to a pool). Do not delete
    // until CreateDictionary callers are pool-threaded.
    [[deprecated("no-pool bridge; vector_helper.h CreateDictionary callers still need pool-threading")]]
    Vector(int size, std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> container,
        NullsBuffer *nullsBufferPtr, DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>,
        int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_DICTIONARY,
              static_cast<omniruntime::mem::OmniTaskMemoryPool *>(nullptr),
              nullsBufferPtr, sliceOffset),
          container(container)
    {
        this->dataTypeId = dataTypeId;
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
    }

    // Pool-bound slice ctor. Shares the parent's container via shared_ptr and charges only
    // {Vector + NullsBuffer} — the container capacity is intentionally NOT charged (the
    // dtor's isSliced branch skips crediting it back). Previously Slice() routed through the
    // full-charge primary ctor, charging container capacity the isSliced dtor never credits
    // back — a persistent per-slice over-count in usedBytes_. SliceCtorTag disambiguates this
    // from the primary ctor (identical arg shape).
    Vector(SliceCtorTag, int size, std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> container,
        NullsBuffer *nullsBufferPtr, omniruntime::mem::OmniTaskMemoryPool *pool,
        DataTypeId dataTypeId = TYPE_ID<RAW_DATA_TYPE>, int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_DICTIONARY, pool, nullsBufferPtr, sliceOffset), container(container)
    {
        this->dataTypeId = dataTypeId;
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer);
        if (this->GetPool() != nullptr) {
            this->GetPool()->AddMemory(vectorCapacity);
        }
    }

    ~Vector() override
    {
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>) + sizeof(NullsBuffer);
        if (!this->isSliced) {
            vectorCapacity += sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        }
        if (this->GetPool() != nullptr) {
            // Pool-bound path: routes SubMemory to construction-time pool.
            this->GetPool()->SubMemory(vectorCapacity);
        }
        // pool_==nullptr: no-pool bridge instance — accounting skipped.
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

        // new nulls — pool-account the copied dictionary validity bitmap.
        auto *pool = this->GetPool();
        std::unique_ptr<NullsBuffer> newNullsBuffer = std::make_unique<NullsBuffer>(length, nullptr, 0,
            pool != nullptr ? pool->getBoundAllocator() : nullptr);
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
        // Pool-bound dictionary ctor only path.
        return new Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>(length, newContainer, newNullsBuffer.get(),
            pool, this->dataTypeId);
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
        // Pool-bound dictionary slice ctor: charges only {Vector + NullsBuffer}. The shared
        // container capacity is intentionally not charged here — the dtor's isSliced branch
        // skips re-crediting it. Symmetric accounting, no per-slice over-count.
        auto sliced = new Vector<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>(SliceCtorTag{}, length,
            this->container, this->nullsBuffer.get(), this->GetPool(), this->dataTypeId, positionOffset);
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
    // Pool-bound primary ctor for LargeString encoding.
    explicit Vector(int size, omniruntime::mem::OmniTaskMemoryPool *pool,
        int capacityInBytes = INITIAL_STRING_SIZE, DataTypeId stringDataTypeId = OMNI_VARCHAR)
        : Vector<RAW_DATA_TYPE>(size, OMNI_FLAT, pool)
    {
        this->dataTypeId = stringDataTypeId;
        // default string_view vector use large string encoding.
        // Thread the per-task pool so the string content buffer (LargeStringBuffer)
        // is charged inside the container. GetContainerCapacity() below charges only the
        // container object; offsets are charged by LargeStringContainer itself,
        // so the content and offsets do not double-count.
        this->container = std::make_shared<LargeStringContainer<std::string_view>>(size, capacityInBytes, this->pool_);
        // vector class capacity, nullsBuffer class capacity and values total capacity
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        // nullptr-pool sentinel for no-pool bridge compat.
        if (this->pool_ != nullptr) {
            this->pool_->AddMemory(vectorCapacity);   // direct — no TLS
        }
    }

    // No-pool bridge for LargeString encoding. Required by vector_helper.h
    // DecodeVarcharDictionaryVector (helper allocates a result VarcharVector without
    // access to a pool). Do not delete until those callers are pool-threaded.
    [[deprecated("no-pool bridge; vector_helper.h DecodeVarcharDictionaryVector callers still need pool-threading")]]
    explicit Vector(int size, int capacityInBytes = INITIAL_STRING_SIZE,
        DataTypeId stringDataTypeId = OMNI_VARCHAR)
        : Vector<RAW_DATA_TYPE>(size, OMNI_FLAT,
              static_cast<omniruntime::mem::OmniTaskMemoryPool *>(nullptr))
    {
        this->dataTypeId = stringDataTypeId;
        this->container = std::make_shared<LargeStringContainer<std::string_view>>(size, capacityInBytes);
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer) +
            sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
    }

    // Pool-bound slice ctor for LargeString encoding. Slice inherits parent's pool;
    // only charges {Vector + NullsBuffer} (container is shared via shared_ptr, the
    // dtor's isSliced path correctly skips re-credit).
    explicit Vector(int size, std::shared_ptr<LargeStringContainer<std::string_view>> container,
        NullsBuffer *nullsBufferPtr, omniruntime::mem::OmniTaskMemoryPool *pool,
        DataTypeId dataTypeId = OMNI_VARCHAR, int32_t sliceOffset = 0)
        : Vector<RAW_DATA_TYPE>(size, OMNI_FLAT, pool, nullsBufferPtr, sliceOffset), container(container)
    {
        this->dataTypeId = dataTypeId;
        // vector class capacity only (slice shares container via shared_ptr).
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer);
        // nullptr-pool sentinel: parent may be shuffle-deserialized (no pool), and the
        // dtor already skips SubMemory when pool_ is null. Symmetric guard avoids SIGSEGV
        // in the slice path.
        if (this->pool_ != nullptr) {
            this->pool_->AddMemory(vectorCapacity);   // direct — no TLS
        }
    }

    ~Vector() override
    {
        // vector class capacity
        int64_t vectorCapacity = sizeof(Vector<LargeStringContainer<RAW_DATA_TYPE>>) + sizeof(NullsBuffer);
        if (!this->isSliced) {
            vectorCapacity += sizeof(AlignedBuffer<uint8_t>) + container->GetContainerCapacity();
        }
        if (this->pool_ != nullptr) {
            // Pool-bound path: routes SubMemory to construction-time pool.
            this->pool_->SubMemory(vectorCapacity);
        }
        // pool_==nullptr: no-pool bridge instance — accounting skipped.
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

        auto startPositions = positions + offset;
        // Size the content buffer to the copied bytes. The default 32KB start (plus doubling)
        // dominates small copies such as spill sub-partitions and selective filters.
        int64_t totalBytes = 0;
        for (int32_t i = 0; i < length; i++) {
            if (!this->IsNull(startPositions[i])) {
                totalBytes += static_cast<int64_t>(GetValue(startPositions[i]).size());
            }
        }
        const int capacityInBytes = static_cast<int>(std::min<int64_t>(std::max<int64_t>(totalBytes, 1),
            std::numeric_limits<int32_t>::max()));
        // Pool-bound LargeString ctor only path.
        auto vector = new Vector<LargeStringContainer<std::string_view>>(length, this->pool_,
            capacityInBytes, this->dataTypeId);
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
        // Pool-bound LargeString slice ctor only path.
        auto sliced = new Vector<LargeStringContainer<std::string_view>>(length, container, this->nullsBuffer.get(),
            this->pool_, this->dataTypeId, positionOffset);
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
        // Pool-account the reallocated varchar validity bitmap.
        this->nullsBuffer = std::make_shared<NullsBuffer>(newCapacity, nullptr, 0,
            this->pool_ != nullptr ? this->pool_->getBoundAllocator() : nullptr);
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
        // Pool-account the varchar validity bitmap on Reserve too.
        omniruntime::mem::Allocator *allocForReserveStr =
            this->pool_ != nullptr ? this->pool_->getBoundAllocator() : nullptr;
        if (copyNulls && this->nullsBuffer != nullptr) {
            auto oldNullsBuffer = std::move(this->nullsBuffer);
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity, nullptr, 0, allocForReserveStr);
            this->nullsBuffer->SetNulls(0, oldNullsBuffer.get(), this->size);
        } else {
            this->nullsBuffer = std::make_shared<NullsBuffer>(suggestedCapacity, nullptr, 0, allocForReserveStr);
        }
        this->capacity = suggestedCapacity;
        container->Expand(suggestedCapacity);
    }

private:
    friend class unsafe::UnsafeStringVector;
    std::shared_ptr<LargeStringContainer<std::string_view>> container;
};

/**
 * Vector specialization for StringView.
 *
 * Storage:
 *  - valuesBuffer: AlignedBuffer<StringView> — contiguous 16-byte structs (O(1) random access)
 *  - stringBuffer: LargeStringBuffer — external char arena for non-inline strings (size > 12)
 *
 * Inline strings (size <= 12) are fully self-contained in the StringView struct; no arena bytes
 * are consumed.  Non-inline strings copy their payload into stringBuffer and the StringView stores
 * a pointer into that arena.
 *
 * If stringBuffer runs out of space, GrowStringBuffer() reallocates it and fixes up all
 * outstanding non-inline pointers in the values array.
 */
template <>
class Vector<StringView> : public BaseVector {
public:
    Vector(int32_t vSize, omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t capacityInBytes = INITIAL_STRING_SIZE)
        : BaseVector(vSize, OMNI_FLAT, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              AllocatorFor(pool)),
          stringBuffer(MakeStringBuffer(static_cast<size_t>(capacityInBytes), pool)),
          pool_(pool)
    {
        this->dataTypeId = OMNI_STRING_VIEW;
        valuesBuffer = std::make_shared<AlignedBuffer<StringView>>(vSize, AllocatorFor(pool_));
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        AddFlatMetadata(/*sharedBuffers=*/false);
    }

    // No-pool bridge for reader/tests that do not yet carry a task pool.
    explicit Vector(int32_t vSize, int32_t capacityInBytes = INITIAL_STRING_SIZE)
        : Vector(vSize, static_cast<omniruntime::mem::OmniTaskMemoryPool *>(nullptr), capacityInBytes)
    {}

    // Encoding variant used as the base of Vector<DictionaryContainer<StringView>>.
    Vector(int32_t vSize, Encoding encoding, omniruntime::mem::OmniTaskMemoryPool *pool,
        NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0)
        : BaseVector(vSize, encoding, nullsBufferPtr, sliceOffset, AllocatorFor(pool)), pool_(pool)
    {
        this->dataTypeId = OMNI_STRING_VIEW;
        capacity = vSize;
    }

    // Used by Vector<DictionaryContainer<StringView>> — does not allocate values or string buffer.
    Vector(int32_t vSize, Encoding encoding, NullsBuffer *nullsBufferPtr = nullptr, int32_t sliceOffset = 0)
        : Vector(vSize, encoding, static_cast<omniruntime::mem::OmniTaskMemoryPool *>(nullptr),
              nullsBufferPtr, sliceOffset)
    {}

    // Creates a new vector of `vSize` elements that shares `source`'s string buffer.
    // Allows SetNoCopy to safely reference substrings from source's string buffer
    // without copying; the shared_ptr keeps the buffer alive even after source is destroyed.
    explicit Vector(int32_t vSize, Vector<StringView>& source)
        : BaseVector(vSize, OMNI_FLAT, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              AllocatorFor(source.pool_)),
          stringBuffer(source.stringBuffer),
          pool_(source.pool_)
    {
        this->dataTypeId = OMNI_STRING_VIEW;
        sharesStringBuffer_ = true;
        valuesBuffer = std::make_shared<AlignedBuffer<StringView>>(vSize, AllocatorFor(pool_));
        values = valuesBuffer->GetBuffer();
        capacity = vSize;
        AddFlatMetadata(/*sharedBuffers=*/false);
    }

    ~Vector() override
    {
        if (encoding != OMNI_FLAT || valuesBuffer == nullptr || pool_ == nullptr) {
            return;
        }
        pool_->SubMemory(FlatMetadataBytes(/*sharedBuffers=*/isSliced));
    }

    /* *
     * Set the value at the indicated index.
     * Inline values are stored directly in the struct; non-inline values are copied into
     * the string buffer and the StringView is updated to point into it.
     */
    void ALWAYS_INLINE SetValue(int32_t index, StringView value)
    {
        if (value.isInline()) {
            values[index] = value;
        } else {
            const char *dest = AppendToStringBuffer(value.data(), value.size());
            values[index] = StringView(dest, static_cast<int32_t>(value.size()));
        }
    }

    /* *
     * Stores the StringView at the given index without copying data into the string buffer.
     * The caller is responsible for ensuring the pointed-to data outlives this vector;
     * using the shared-buffer constructor guarantees this automatically.
     */
    void ALWAYS_INLINE SetNoCopy(int32_t index, StringView value)
    {
        values[index] = value;
    }

    /* *
     * Gets the value of the vector at the indicated index.
     */
    ALWAYS_INLINE StringView GetValue(int32_t index) const
    {
        return values[index + offset];
    }

    /* *
     * Returns a reference (offset-aware, no copy) to the StringView at the indicated index.
     * Needed when converting to std::string_view: a by-value StringView copy whose payload is
     * inline (<=12B, stored in the 16B struct) would dangle once the copy dies. The buffer slot
     * referenced here outlives the access. (StringView::operator std::string_view() const&& is
     * deleted, so converting an rvalue copy is a compile error — use this ref accessor instead.)
     */
    ALWAYS_INLINE const StringView &GetValueRef(int32_t index) const
    {
        return values[index + offset];
    }

    /* *
     * Appends `length` elements from `other` starting at `positionOffset` into this vector.
     */
    void Append(BaseVector *other, int positionOffset, int length)
    {
        if (UNLIKELY(positionOffset + length > size)) {
            std::string message("append vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto src = reinterpret_cast<Vector<StringView> *>(other);
        for (int32_t i = 0; i < length; i++) {
            int32_t srcIdx = i;
            int32_t dstIdx = i + positionOffset;
            if (src->IsNull(srcIdx)) {
                SetNull(dstIdx);
            } else {
                SetValue(dstIdx, src->GetValue(srcIdx));
            }
        }
    }

    /* *
     * Copies the values at the indicated positions into a new vector.
     * Zero-copy materialization: the new vector shares this vector's string buffer
     * (refcounted) and only the 16-byte views are copied; non-inline views keep pointing
     * into the shared buffer, so no payload bytes move (same model as Slice / Velox
     * acquireSharedStringBuffers). Trade-off: a sparse selection pins the whole source
     * buffer until the output dies — compaction heuristic is future work (KT.1).
     */
    BaseVector *CopyPositions(const int *positions, int positionOffset, int length) override
    {
        if (UNLIKELY((positions == nullptr) || (length < 0))) {
            std::string message("positions is null or the input length is incorrect: %d.", length);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto vector = new Vector<StringView>(length, *this);
        auto startPositions = positions + positionOffset;
        for (int32_t i = 0; i < length; i++) {
            int position = startPositions[i];
            if (UNLIKELY(position == -1)) {
                vector->SetNull(i);
                continue;
            }
            if (UNLIKELY(IsNull(position))) {
                vector->SetNull(i);
            } else {
                vector->SetNoCopy(i, GetValue(position));
            }
        }
        return vector;
    }

    /* *
     * Creates a read-only slice that shares the values and string buffers with the parent.
     */
    BaseVector *Slice(int positionOffset, int length, bool isCopy = false) override
    {
        if (UNLIKELY(positionOffset + length > size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto sliced = new Vector<StringView>(
            length, valuesBuffer, stringBuffer, nullsBuffer.get(), pool_, positionOffset);
        sliced->SetOffset(offset + positionOffset);
        sliced->SetSliced(true);
        return sliced;
    }

    /* *
     * Expands the vector to accommodate needCapacity elements.
     * Only the values array and nulls buffer are grown; the string buffer is unchanged
     * because existing StringView pointers remain valid.
     */
    void Expand(int32_t needCapacity) override
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

        auto oldValuesBuffer = valuesBuffer;
        auto oldValues = values;
        valuesBuffer = std::make_shared<AlignedBuffer<StringView>>(newCapacity, AllocatorFor(pool_));
        values = valuesBuffer->GetBuffer();
        if (oldValues != nullptr) {
            error_t res = memcpy_s(values, newCapacity * sizeof(StringView),
                                   oldValues, oldSize * sizeof(StringView));
            if (res != EOK) {
                throw OmniException("ERROR: Vector<StringView> Expand memcpy_s failed", std::to_string(res));
            }
        }

        auto oldNullsBuffer = nullsBuffer;
        nullsBuffer = std::make_shared<NullsBuffer>(newCapacity, nullptr, 0, AllocatorFor(pool_));
        if (oldNullsBuffer != nullptr) {
            nullsBuffer->SetNulls(0, oldNullsBuffer.get(), oldSize);
        }
        capacity = newCapacity;
        size = needCapacity;
    }

    StringView *GetValuesBuffer()
    {
        return values;
    }

    omniruntime::mem::OmniTaskMemoryPool *GetPool() const override
    {
        return pool_;
    }

protected:
    friend class unsafe::UnsafeVector;
    std::shared_ptr<AlignedBuffer<StringView>> valuesBuffer;
    StringView *values = nullptr;
    int32_t capacity = 0;
    std::shared_ptr<LargeStringBuffer> stringBuffer;
    bool sharesStringBuffer_ = false;

private:
    static omniruntime::mem::Allocator *AllocatorFor(omniruntime::mem::OmniTaskMemoryPool *pool)
    {
        static omniruntime::mem::Allocator nullBridge{nullptr};
        return pool == nullptr ? &nullBridge : pool->getBoundAllocator();
    }

    static std::shared_ptr<LargeStringBuffer> MakeStringBuffer(
        size_t capacityInBytes, omniruntime::mem::OmniTaskMemoryPool *pool)
    {
        if (pool != nullptr) {
            return std::make_shared<LargeStringBuffer>(capacityInBytes, pool);
        }
        return std::make_shared<LargeStringBuffer>(capacityInBytes);
    }

    static constexpr int64_t FlatMetadataBytes(bool sharedBuffers)
    {
        int64_t bytes = sizeof(Vector<StringView>) + sizeof(NullsBuffer);
        if (!sharedBuffers) {
            bytes += sizeof(AlignedBuffer<uint8_t>) + sizeof(AlignedBuffer<StringView>);
        }
        return bytes;
    }

    void AddFlatMetadata(bool sharedBuffers)
    {
        if (pool_ != nullptr) {
            pool_->AddMemory(FlatMetadataBytes(sharedBuffers));
        }
    }

    /* Shared constructor used by Slice — shares both buffers with the parent. */
    Vector(int32_t vSize, std::shared_ptr<AlignedBuffer<StringView>> valuesBuffer,
        std::shared_ptr<LargeStringBuffer> stringBuffer, NullsBuffer *nullsBufferPtr,
        omniruntime::mem::OmniTaskMemoryPool *pool, int32_t sliceOffset = 0)
        : BaseVector(vSize, OMNI_FLAT, nullsBufferPtr, sliceOffset, AllocatorFor(pool)),
          valuesBuffer(valuesBuffer),
          stringBuffer(stringBuffer),
          pool_(pool)
    {
        this->dataTypeId = OMNI_STRING_VIEW;
        this->values = this->valuesBuffer->GetBuffer();
        this->capacity = vSize;
        AddFlatMetadata(/*sharedBuffers=*/true);
    }

    /* Appends `len` bytes to the string buffer, growing it if necessary, and returns
     * a stable pointer to the copied bytes. */
    const char *AppendToStringBuffer(const char *data, size_t len)
    {
        uint64_t usedSize = stringBuffer->Size();
        if (usedSize + len > stringBuffer->Capacity()) {
            GrowStringBuffer(usedSize + len);
            usedSize = stringBuffer->Size();
        }
        char *dest = stringBuffer->Data() + usedSize;
        error_t res = memcpy_s(dest, stringBuffer->Capacity() - usedSize, data, len);
        if (UNLIKELY(res != EOK)) {
            throw OmniException("ERROR: Vector<StringView> string buffer memcpy_s failed", std::to_string(res));
        }
        stringBuffer->SetSize(usedSize + len);
        return dest;
    }

    /* Reallocates the string buffer to at least `neededBytes`, copies all existing string data,
     * and fixes up all non-inline StringView pointers in the values array. */
    void GrowStringBuffer(uint64_t neededBytes)
    {
        uint64_t newCapacity = std::max(stringBuffer->Capacity() * 2, neededBytes);
        const char *oldData = stringBuffer->Data();

        auto newBuffer = MakeStringBuffer(newCapacity, pool_);
        newBuffer->SetSize(stringBuffer->Size());
        error_t res = memcpy_s(newBuffer->Data(), newCapacity, oldData, stringBuffer->Size());
        if (UNLIKELY(res != EOK)) {
            throw OmniException("ERROR: Vector<StringView> GrowStringBuffer memcpy_s failed", std::to_string(res));
        }

        // Fix up all non-inline StringView pointers to point into the new buffer.
        // Only fix up pointers that actually fall within the old buffer's used range —
        // uninitialized entries may contain garbage pointers that must not be dereferenced.
        const char *newData = newBuffer->Data();
        const uint64_t oldUsedSize = stringBuffer->Size();
        for (int32_t i = 0; i < size; i++) {
            if (!IsNull(i) && !values[i].isInline()) {
                ptrdiff_t off = values[i].data() - oldData;
                if (off >= 0 && static_cast<uint64_t>(off) < oldUsedSize) {
                    values[i] = StringView(newData + off, static_cast<int32_t>(values[i].size()));
                }
            }
        }

        // LargeStringBuffer's pool-bound ctor/dtor account the new/old capacities
        // symmetrically when the shared_ptr is replaced.
        stringBuffer = std::move(newBuffer);
    }

    omniruntime::mem::OmniTaskMemoryPool *pool_ = nullptr;
};
}

#endif // OMNI_RUNTIME_VECTOR_H
