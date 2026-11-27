/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#ifndef OMNI_RUNTIME_VECTOR_HELPER_H
#define OMNI_RUNTIME_VECTOR_HELPER_H

#include "vector.h"
#include "string_view.h"
#include "large_string_container.h"
#include "unsafe_vector.h"
#include "type/data_types.h"
#include "vector_batch.h"
#include "operator/aggregation/container_vector.h"
#include "omni_row.h"
#include "row_vector.h"
#include "map_vector.h"
#include "array_vector.h"
#include "util/type_util.h"

namespace omniruntime::vec {
class VectorHelper {
public:
    // Pool-bound conversion; implemented in vector_helper.cpp.
    static RowBatch *TransRowBatchFromVectorBatch(VectorBatch *vecBatch, mem::Allocator *allocator);
    static BaseVector *CreateStringDictionary(const int32_t *values, int32_t valueSize,
        Vector<LargeStringContainer<std::string_view>> *vector, bool valueAsNullifyIndices = false,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    static BaseVector *CreateVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t vectorEncodingId, int32_t dataTypeId, int32_t size,
        int32_t capacityInBytes = INITIAL_STRING_SIZE);
    // Last in-tree production caller has been migrated; kept for external and test compatibility.
    [[deprecated("last in-tree caller migrated; prefer the pool-bound 5-argument overload")]]
    static BaseVector *CreateVector(int32_t vectorEncodingId, int32_t dataTypeId, int32_t size,
        int32_t capacityInBytes = INITIAL_STRING_SIZE);
    static void SetVectorDataType(BaseVector* vec, DataType* dataType);
    static BaseVector *CreateComplexVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        DataType* dataType, int32_t size);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static BaseVector *CreateComplexVector(DataType* dataType, int32_t size);
    static std::shared_ptr<BaseVector> CreateComplexVectorShared(
        omniruntime::mem::OmniTaskMemoryPool *pool, DataType* dataType, int32_t size);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static std::shared_ptr<BaseVector> CreateComplexVectorShared(DataType* dataType, int32_t size);
    static BaseVector *CreateEmptyComplexVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        DataType* dataType, int32_t size);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static BaseVector *CreateEmptyComplexVector(DataType* dataType, int32_t size);
    static BaseVector *CreateFlatVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t dataTypeId, int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE);
    static std::shared_ptr<BaseVector> CreateFlatVectorShared(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t dataTypeId, int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static BaseVector *CreateFlatVector(int32_t dataTypeId, int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static std::shared_ptr<BaseVector> CreateFlatVectorShared(int32_t dataTypeId, int32_t size,
        int32_t capacityInBytes = INITIAL_STRING_SIZE);
    static void SetValue(vec::BaseVector *vector, int32_t index, void *value);
    static void SetNull(vec::BaseVector *vector, int index);
    static void CopyValue(vec::BaseVector *srcVector, int32_t srcIndex, vec::BaseVector *dstVector, int32_t dstIndex);
    static void PrintVecBatch(VectorBatch *vecBatch);
    static void PrintVec(BaseVector *vector, int32_t rowCount);
    static void PrintVecVector(std::vector<BaseVector*> *vecVector, int32_t rowCount);
    static void PrintStructVectorValue(BaseVector* vector, int32_t rowIndex);
    static void PrintArrayVectorValue(BaseVector* vector, int32_t rowIndex);
    static void PrintArrayVectorOffsetsAndNulls(BaseVector* vector, int32_t rowIndex);
    static void PrintMapVectorValue(MapVector *mapVec, int32_t rowIndex);
    static void PrintVectorValue(BaseVector *vector, int32_t rowIndex);
    static BaseVector *CreateDictionaryVector(int *values, int valueSize, BaseVector *vector, int dataTypeId,
        bool valueAsNullifyIndices = false, omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    static void *UnsafeGetValuesDictionary(BaseVector *vector);
    static BaseVector *CastConstVectorToVector(BaseVector *vector);
    static void *UnsafeGetValues(BaseVector *vector);
    static void *UnsafeGetOffsetsAddr(BaseVector *vector);
    static BaseVector *SliceDictionaryVector(BaseVector *vector, int positionOffset, int length);
    static BaseVector *SliceConstVector(BaseVector *vector, int length);
    static BaseVector *SliceVector(BaseVector *vector, int positionOffset, int length);
    static BaseVector *CopyPositionsDictionaryVector(BaseVector *vector, int *positions, int offset, int length);
    static std::unique_ptr<VectorBatch> CopyVectorBatch(VectorBatch *src);
    // Materialize an independent flat/complex copy whose buffers are owned by `pool`.
    // An explicit nullptr is the process-lifetime Jemalloc compatibility path used by
    // executor-scoped caches whose lifetime is longer than any Spark task pool.
    static std::unique_ptr<VectorBatch> CopyVectorBatchToPool(
        VectorBatch *src, omniruntime::mem::OmniTaskMemoryPool *pool);
    static BaseVector *CopyPositionsVector(BaseVector *vector, int *positions, int offset, int length);
    // Copy the rows at `positions` into a new flat vector whose buffers belong to `pool`. A null
    // pool builds the copy outside any task pool: spill working memory that is written to disk and
    // released before the spill returns must not grow the pool it is reclaiming. Encoded sources
    // (dictionary, constant) are flattened by the copy. A position of -1 copies a null.
    static BaseVector *CopyPositionsVectorToPool(BaseVector *vector, const int *positions, int offset,
        int length, omniruntime::mem::OmniTaskMemoryPool *pool);
    static void *UnsafeGetDictionary(BaseVector *vector);
    static void AppendVector(BaseVector *destVector, int32_t offset, BaseVector *srcVector, int32_t length);
    static void ExpandElementVector(BaseVector *elementVec, type::DataTypeId typeId, int32_t newSize);
    static BaseVector *DecodeVarcharDictionaryVector(BaseVector *vector,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    static BaseVector *DecodeDictionaryVector(BaseVector *vector,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    // A dictionary view keeps its whole dictionary (often an entire upstream batch) alive.
    // True when the view selects fewer than half of the dictionary's rows.
    static bool IsSparseDictionary(BaseVector *vector);
    // Operators that retain input until end of input (hash build, sort) replace sparse
    // dictionary views with flat copies so they pin only the rows they use.
    static void CompactSparseDictionaries(VectorBatch *batch, omniruntime::mem::OmniTaskMemoryPool *pool);
    template <typename T> static BaseVector *DecodeFlatConstVector(BaseVector *vector,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    static BaseVector *DecodeVarcharConstVector(BaseVector *vector,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    static BaseVector *DecodeConstVector(BaseVector *vector,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr);
    static void CopyFlatVector(BaseVector *destVector, BaseVector *srcVector, int32_t offset, int32_t length);
    static DataTypePtr GetDataType(BaseVector *srcVector);
    static void EmptyArrayProjection(omniruntime::mem::OmniTaskMemoryPool *pool,
        ArrayVector *dstArrVec, DataTypeId typeId);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static void EmptyArrayProjection(ArrayVector *dstArrVec, DataTypeId typeId);
    static void EmptyMapProjection(omniruntime::mem::OmniTaskMemoryPool *pool,
        MapVector *dstArrVec, DataTypeId keyTypeId, DataTypeId valueTypeId);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static void EmptyMapProjection(MapVector *dstArrVec, DataTypeId keyTypeId, DataTypeId valueTypeId);
    static void CreateNullArrayVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static void CreateNullArrayVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    static void CreateNullMapVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static void CreateNullMapVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    static void CreateNullRowVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static void CreateNullRowVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    static std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row);

    template <typename T> static BaseVector *CreateDictionary(int32_t *values, int32_t valueSize, Vector<T> *vector,
        bool valueAsNullifyIndices = false, omniruntime::mem::OmniTaskMemoryPool *pool = nullptr)
    {
        auto nullsBuffer = std::make_unique<NullsBuffer>(valueSize, nullptr, 0,
            pool != nullptr ? pool->getBoundAllocator() : nullptr);
        for (int i = 0; i < valueSize; i++) {
            if (UNLIKELY(vector->IsNull(values[i]) || (valueAsNullifyIndices && values[i] == -1))) {
                nullsBuffer->SetNull(i);
            }
        }

        auto dictionary = std::make_shared<DictionaryContainer<T>>(values, valueSize,
            unsafe::UnsafeVector::GetValues<T>(vector), vector->GetSize(), vector->GetOffset());
        if (pool != nullptr) {
            return new Vector<DictionaryContainer<T>>(
                valueSize, dictionary, nullsBuffer.get(), pool, TYPE_ID<T>);
        }
        return new Vector<DictionaryContainer<T>>(valueSize, dictionary, nullsBuffer.get(), TYPE_ID<T>);
    }

    template <type::DataTypeId typeId>
    static BaseVector *CreateFlatVector(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            return new Vector<LargeStringContainer<std::string_view>>(size, pool, capacityInBytes, typeId);
        }
        if constexpr (std::is_same_v<T, StringView>) {
            return new Vector<StringView>(size, pool, capacityInBytes);
        }
        if constexpr (!std::is_same_v<T, std::string_view> && !std::is_same_v<T, StringView>) {
            if (pool != nullptr) {
                auto scope = pool->accountingScope();
                return new Vector<T>(size, scope, typeId);
            }
        }
        return new Vector<T>(size, pool, typeId);
    }

    template <type::DataTypeId typeId>
    static std::shared_ptr<BaseVector> CreateFlatVectorShared(omniruntime::mem::OmniTaskMemoryPool *pool,
        int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            return std::make_shared<Vector<LargeStringContainer<std::string_view>>>(
                size, pool, capacityInBytes, typeId);
        }
        if constexpr (std::is_same_v<T, StringView>) {
            return std::make_shared<Vector<StringView>>(size, pool, capacityInBytes);
        }
        if constexpr (!std::is_same_v<T, std::string_view> && !std::is_same_v<T, StringView>) {
            if (pool != nullptr) {
                auto scope = pool->accountingScope();
                return std::make_shared<Vector<T>>(size, scope, typeId);
            }
        }
        return std::make_shared<Vector<T>>(size, pool, typeId);
    }

    template <type::DataTypeId typeId>
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static BaseVector *CreateFlatVector(int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            return new Vector<LargeStringContainer<std::string_view>>(size, capacityInBytes, typeId);
        }
        if constexpr (std::is_same_v<T, StringView>) {
            return new Vector<StringView>(size, capacityInBytes);
        }
        return new Vector<T>(size, typeId);
    }

    template <type::DataTypeId typeId>
    [[deprecated("pass an OmniTaskMemoryPool*")]]
    static std::shared_ptr<BaseVector> CreateFlatVectorShared(int32_t size,
        int32_t capacityInBytes = INITIAL_STRING_SIZE)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            return std::make_shared<Vector<LargeStringContainer<std::string_view>>>(size, capacityInBytes, typeId);
        }
        if constexpr (std::is_same_v<T, StringView>) {
            return std::make_shared<Vector<StringView>>(size, capacityInBytes);
        }
        return std::make_shared<Vector<T>>(size, typeId);
    }

    template <type::DataTypeId typeId> static void VectorSetValue(vec::BaseVector *vector, int32_t index, void *value)
    {
        using T = typename type::NativeType<typeId>::type;
        if (value == nullptr) {
            if constexpr (std::is_same_v<T, std::string_view>) {
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetNull(index);
            } else {
                vector->SetNull(index);
            }
            return;
        }

        if constexpr (std::is_same_v<T, std::string_view>) {
            std::string_view data = std::string_view(static_cast<std::string *>(value)->data(),
                static_cast<std::string *>(value)->length());
            static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetValue(index, data);
        } else if constexpr (std::is_same_v<T, StringView>) {
            std::string_view sv = *static_cast<std::string_view *>(value);
            static_cast<Vector<StringView> *>(vector)->SetValue(index, StringView(sv));
        } else {
            static_cast<Vector<T> *>(vector)->SetValue(index, *static_cast<T *>(value));
        }
        return;
    }

    template <type::DataTypeId typeId>
    static typename type::NativeType<typeId>::type GetFlatValue(vec::BaseVector *vector, int index)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (typeId == OMNI_VARBINARY || typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                auto *constVector = reinterpret_cast<ConstVector<T> *>(vector);
                return constVector->GetConstValue();
            } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto *rawVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                return rawVector->GetValue(index);
            } else {
                auto *rawVector = reinterpret_cast<Vector<LargeStringContainer<T>> *>(vector);
                return rawVector->GetValue(index);
            }
        } else {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                auto *constVector = reinterpret_cast<ConstVector<T> *>(vector);
                return constVector->GetConstValue();
            } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto *rawVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                return rawVector->GetValue(index);
            } else {
                auto *rawVector = reinterpret_cast<Vector<T> *>(vector);
                return rawVector->GetValue(index);
            }
        }
    }

    // Get the data pointer of the flat vector (including OMNI_FLAT、 OMNI_ENCODING_CONST and OMNI_DICTIONARY)
    template <type::DataTypeId typeId>
    static void *GetFlatValuePtr(vec::BaseVector *vector)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (typeId == OMNI_VARBINARY || typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                auto *constVector = reinterpret_cast<ConstVector<T> *>(vector);
                return const_cast<void *>(reinterpret_cast<const void *>(&constVector->GetConstValueRef()));
            } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto *rawVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                return unsafe::UnsafeDictionaryVector::GetVarCharDictionary(rawVector);
            } else {
                auto *rawVector = reinterpret_cast<Vector<LargeStringContainer<T>> *>(vector);
                return unsafe::UnsafeStringVector::GetValues(rawVector);
            }
        } else {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                auto *constVector = reinterpret_cast<ConstVector<T> *>(vector);
                return const_cast<void *>(reinterpret_cast<const void *>(&constVector->GetConstValueRef()));
            } else if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                return unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
            } else {
                auto *rawVector = reinterpret_cast<Vector<T> *>(vector);
                return unsafe::UnsafeVector::GetRawValues<T>(rawVector);
            }
        }
    }

    template <type::DataTypeId typeId>
    static void SetFlatValue(vec::BaseVector *vector, int index, typename type::NativeType<typeId>::type value)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (typeId == OMNI_VARBINARY || typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
            if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto *rawVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                rawVector->SetValue(index, value);
            } else {
                auto *rawVector = reinterpret_cast<Vector<LargeStringContainer<T>> *>(vector);
                rawVector->SetValue(index, value);
            }
        } else {
            if (vector->GetEncoding() == vec::OMNI_DICTIONARY) {
                auto rawVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                rawVector->SetValue(index, value);
            } else {
                auto rawVector = reinterpret_cast<Vector<T> *>(vector);
                rawVector->SetValue(index, value);
            }
        }
    }

    template <type::DataTypeId typeId>
    static void VectorCopyValue(vec::BaseVector *srcVector, int32_t srcIndex, vec::BaseVector *dstVector, int32_t dstIndex)
    {
        auto value = GetFlatValue<typeId>(srcVector, srcIndex);
        SetFlatValue<typeId>(dstVector, dstIndex, value);
    }

    template <type::DataTypeId typeId> static void PrintArrayElement(BaseVector* elementVec, int32_t index) {
        using namespace omniruntime::type;
        using T = typename NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            std::cout << std::dec << static_cast<Vector<LargeStringContainer<T>> *>(elementVec)->GetValue(index);
        } else {
            std::cout << std::dec << static_cast<Vector<T> *>(elementVec)->GetValue(index);
        }
    }

    template <type::DataTypeId typeId> static void PrintDictionaryVectorValue(BaseVector *vector, int32_t rowIndex)
    {
        using namespace omniruntime::type;
        using T = typename NativeType<typeId>::type;
        using DictionaryVarchar = Vector<DictionaryContainer<std::string_view, LargeStringContainer>>;
        if constexpr (std::is_same_v<T, std::string_view>) {
            std::cout << std::dec << static_cast<DictionaryVarchar *>(vector)->GetValue(rowIndex) << "\t";
        } else {
            std::cout << std::dec << static_cast<Vector<DictionaryContainer<T>> *>(vector)->GetValue(rowIndex) << "\t";
        }
    }

    template <type::DataTypeId typeId> static void PrintFlatVectorValue(BaseVector *vector, int32_t rowIndex)
    {
        if constexpr (typeId == OMNI_CONTAINER) {
            auto vec0 = reinterpret_cast<Vector<double> *>(static_cast<ContainerVector *>(vector)->GetValue(0));
            auto vec1 = reinterpret_cast<Vector<int64_t> *>(static_cast<ContainerVector *>(vector)->GetValue(1));
            std::cout << std::dec << vec0->GetValue(rowIndex) << "/" << vec1->GetValue(rowIndex) << "\t";
            return;
        }
        using namespace omniruntime::type;
        using T = typename NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            std::cout << std::dec << static_cast<Vector<LargeStringContainer<T>> *>(vector)->GetValue(rowIndex) << "\t";
        } else {
            std::cout << std::dec << static_cast<Vector<T> *>(vector)->GetValue(rowIndex) << "\t";
        }
    }

    template <type::DataTypeId typeId> static void PrintConstVectorValue(BaseVector *vector, int32_t rowIndex)
    {
        using namespace omniruntime::type;
        if (vector->IsNull(rowIndex)) {
            std::cout << "NULL"
                      << "\t";
            return;
        }
        using T = typename NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            std::cout << std::dec << static_cast<ConstVector<T> *>(vector)->GetConstValue() << "\t";
        } else if constexpr (typeId == OMNI_DECIMAL128) {
            std::cout << std::dec << static_cast<ConstVector<Decimal128> *>(vector)->GetConstValue() << "\t";
        } else {
            std::cout << std::dec << static_cast<ConstVector<T> *>(vector)->GetConstValue() << "\t";
        }
    }

    template <typename T> static BaseVector *DecodeFlatDictionaryVector(BaseVector *vector,
        omniruntime::mem::OmniTaskMemoryPool *pool = nullptr)
    {
        int size = vector->GetSize();
        auto *flatVector = new Vector<T>(size, pool, static_cast<DataTypeId>(vector->GetTypeId()));
        auto dicVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
        for (int i = 0; i < size; i++) {
            if (dicVector->IsNull(i)) {
                flatVector->SetNull(i);
            } else {
                flatVector->SetValue(i, dicVector->GetValue(i));
            }
        }
        return flatVector;
    }

    template <typename T>
    static void CopyFlatVector(BaseVector *destVector, BaseVector *srcVector, int32_t offset, int32_t length)
    {
        // copy values
        auto destValues = unsafe::UnsafeVector::GetRawValues(static_cast<Vector<T> *>(destVector));
        auto srcValues = unsafe::UnsafeVector::GetRawValues(static_cast<Vector<T> *>(srcVector)) + offset;
        memcpy(destValues, srcValues, length * sizeof(T));
    }

    template <typename T>
    static bool GetValueFromVector(BaseVector *vec, int32_t row, T& result) {
        Encoding encoding = vec->GetEncoding();
        if (encoding == OMNI_ENCODING_CONST) {
            auto *constVec = static_cast<ConstVector<T> *>(vec);
            result = constVec->GetConstValue();
            return true;
        }
        if (encoding == OMNI_FLAT) {
            auto *flatVec = static_cast<Vector<T> *>(vec);
            if (flatVec->IsNull(row)) {
                return false;
            } else {
                result = flatVec->GetValue(row);
                return true;
            }
        }
        OMNI_THROW("GetValueFromVector Failed! ", "Unsupported encoding type");
    }

    template <typename T>
    static T GetValueFromVector(BaseVector *vec, int32_t row) {
        Encoding encoding = vec->GetEncoding();

        if (encoding == OMNI_ENCODING_CONST) {
            auto *constVec = static_cast<ConstVector<T> *>(vec);
            return constVec->GetConstValue();
        } else if (encoding == OMNI_FLAT) {
            auto *flatVec = static_cast<Vector<T> *>(vec);
            return flatVec->GetValue(row);
        } else if (encoding == OMNI_DICTIONARY) {
            auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
            return dictVec->GetValue(row);
        } else {
            OMNI_THROW("OMNI RUNTIME ERROR", "Unsupported encoding type");
        }
    }

    // ---- ALWAYS_INLINE methods (must stay in header) ----
    static ALWAYS_INLINE BaseVector *CreateStringVector(
        omniruntime::mem::OmniTaskMemoryPool *pool, uint32_t vectorSize)
    {
        return new Vector<LargeStringContainer<std::string_view>>(vectorSize, pool);
    }

    [[deprecated("pass a per-task pool via CreateStringVector(pool, size)")]]
    static ALWAYS_INLINE BaseVector *CreateStringVector(uint32_t vectorSize)
    {
        return new Vector<LargeStringContainer<std::string_view>>(vectorSize, nullptr);
    }

    static ALWAYS_INLINE BaseVector *CreateStringViewVector(
        omniruntime::mem::OmniTaskMemoryPool *pool, uint32_t vectorSize)
    {
        return new Vector<StringView>(vectorSize, pool);
    }

    static ALWAYS_INLINE BaseVector *CreateStringViewVector(uint32_t vectorSize)
    {
        return new Vector<StringView>(vectorSize);
    }

    // Pool-bound form: thread the per-task pool through CreateComplexVector(pool, ...)
    // so each output column vector is charged to the task pool. Callers such as
    // SortOperator pass this->pool().
    static ALWAYS_INLINE void AppendVectors(VectorBatch *vectorBatch, const type::DataTypes &sourceTypes,
        int64_t positionCount, omniruntime::mem::OmniTaskMemoryPool *pool)
    {
        const std::vector<omniruntime::type::DataTypePtr> &types = sourceTypes.Get();
        for (int i = 0; i < sourceTypes.GetSize(); i++) {
            vectorBatch->Append(CreateComplexVector(pool, types[i].get(), positionCount));
        }
    }

    [[deprecated("Phase 02.3.1-04: prefer the pool-bound 4-arg overload. Remaining reach: aggregator/window/topn/spill helpers (Plan 07 tail).")]]
    static ALWAYS_INLINE void AppendVectors(VectorBatch *vectorBatch, const type::DataTypes &sourceTypes,
        int64_t positionCount)
    {
        const std::vector<omniruntime::type::DataTypePtr> &types = sourceTypes.Get();
        for (int i = 0; i < sourceTypes.GetSize(); i++) {
            vectorBatch->Append(CreateComplexVector(types[i].get(), positionCount));
        }
    }

    /** Reset varchar/char vectors in the batch so that reuse (e.g. spill row split) starts with clean offsets. */
    static ALWAYS_INLINE void ResetVarcharVectorsForReuse(VectorBatch *vectorBatch)
    {
        if (vectorBatch == nullptr) {
            return;
        }
        int32_t vecCount = vectorBatch->GetVectorCount();
        for (int32_t i = 0; i < vecCount; i++) {
            BaseVector *vec = vectorBatch->Get(i);
            auto typeId = vec->GetTypeId();
            if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR || typeId == type::OMNI_VARBINARY) {
                auto *varcharVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
                varcharVec->ResetForReuse();
            }
        }
    }

    static ALWAYS_INLINE void FreeVecBatches(std::vector<VectorBatch *> &vecBatches)
    {
        // free vectorBatch and vectors inside it.
        for (auto &vecBatch : vecBatches) {
            FreeVecBatch(vecBatch);
        }
    }

    static ALWAYS_INLINE void FreeVecBatch(VectorBatch *vecBatch)
    {
        if (vecBatch == nullptr) {
            return;
        }
        vecBatch->FreeAllVectors();
        delete vecBatch;
    }
};

// --- CreateFlatVector<typeId> ---
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_BOOLEAN>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_BYTE>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_SHORT>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_INT>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_DATE32>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_LONG>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_DATE64>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_TIMESTAMP>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_FLOAT>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_DOUBLE>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_DECIMAL64>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_DECIMAL128>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_CHAR>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_VARCHAR>(int32_t, int32_t);
extern template BaseVector *VectorHelper::CreateFlatVector<OMNI_VARBINARY>(int32_t, int32_t);

// --- GetFlatValue<typeId> ---
extern template bool VectorHelper::GetFlatValue<OMNI_BOOLEAN>(vec::BaseVector *, int);
extern template int8_t VectorHelper::GetFlatValue<OMNI_BYTE>(vec::BaseVector *, int);
extern template int16_t VectorHelper::GetFlatValue<OMNI_SHORT>(vec::BaseVector *, int);
extern template int32_t VectorHelper::GetFlatValue<OMNI_INT>(vec::BaseVector *, int);
extern template int32_t VectorHelper::GetFlatValue<OMNI_DATE32>(vec::BaseVector *, int);
extern template int64_t VectorHelper::GetFlatValue<OMNI_LONG>(vec::BaseVector *, int);
extern template int64_t VectorHelper::GetFlatValue<OMNI_DATE64>(vec::BaseVector *, int);
extern template int64_t VectorHelper::GetFlatValue<OMNI_TIMESTAMP>(vec::BaseVector *, int);
extern template float VectorHelper::GetFlatValue<OMNI_FLOAT>(vec::BaseVector *, int);
extern template double VectorHelper::GetFlatValue<OMNI_DOUBLE>(vec::BaseVector *, int);
extern template int64_t VectorHelper::GetFlatValue<OMNI_DECIMAL64>(vec::BaseVector *, int);
extern template Decimal128 VectorHelper::GetFlatValue<OMNI_DECIMAL128>(vec::BaseVector *, int);
extern template std::string_view VectorHelper::GetFlatValue<OMNI_CHAR>(vec::BaseVector *, int);
extern template std::string_view VectorHelper::GetFlatValue<OMNI_VARCHAR>(vec::BaseVector *, int);
extern template std::string_view VectorHelper::GetFlatValue<OMNI_VARBINARY>(vec::BaseVector *, int);

// --- GetFlatValuePtr<typeId> ---
extern template void *VectorHelper::GetFlatValuePtr<OMNI_BOOLEAN>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_BYTE>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_SHORT>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_INT>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_DATE32>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_LONG>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_DATE64>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_TIMESTAMP>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_FLOAT>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_DECIMAL64>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_DECIMAL128>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_CHAR>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_VARCHAR>(vec::BaseVector *);
extern template void *VectorHelper::GetFlatValuePtr<OMNI_VARBINARY>(vec::BaseVector *);

// --- GetValueFromVector<T> (返回 T) ---
extern template bool VectorHelper::GetValueFromVector<bool>(BaseVector *, int32_t);
extern template int8_t VectorHelper::GetValueFromVector<int8_t>(BaseVector *, int32_t);
extern template int16_t VectorHelper::GetValueFromVector<int16_t>(BaseVector *, int32_t);
extern template int32_t VectorHelper::GetValueFromVector<int32_t>(BaseVector *, int32_t);
extern template int64_t VectorHelper::GetValueFromVector<int64_t>(BaseVector *, int32_t);
extern template float VectorHelper::GetValueFromVector<float>(BaseVector *, int32_t);
extern template double VectorHelper::GetValueFromVector<double>(BaseVector *, int32_t);
extern template Decimal128 VectorHelper::GetValueFromVector<Decimal128>(BaseVector *, int32_t);

// --- GetValueFromVector<T> (返回 bool, 3 参数) ---
extern template bool VectorHelper::GetValueFromVector<bool>(BaseVector *, int32_t, bool &);
extern template bool VectorHelper::GetValueFromVector<int64_t>(BaseVector *, int32_t, int64_t &);
}

#endif // OMNI_RUNTIME_VECTOR_HELPER_H
