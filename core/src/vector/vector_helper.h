/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#ifndef OMNI_RUNTIME_VECTOR_HELPER_H
#define OMNI_RUNTIME_VECTOR_HELPER_H

#include "vector.h"
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
    static RowBatch *TransRowBatchFromVectorBatch(VectorBatch *vecBatch);
    static BaseVector *CreateStringDictionary(const int32_t *values, int32_t valueSize,
        Vector<LargeStringContainer<std::string_view>> *vector, bool valueAsNullifyIndices = false);
    static BaseVector *CreateVector(int32_t vectorEncodingId, int32_t dataTypeId, int32_t size,
        int32_t capacityInBytes = INITIAL_STRING_SIZE);
    static void SetVectorDataType(BaseVector* vec, DataType* dataType);
    static BaseVector *CreateComplexVector(DataType* dataType, int32_t size);
    static std::shared_ptr<BaseVector> CreateComplexVectorShared(DataType* dataType, int32_t size);
    static BaseVector *CreateEmptyComplexVector(DataType* dataType, int32_t size);
    static BaseVector *CreateFlatVector(int32_t dataTypeId, int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE);
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
    static BaseVector *CreateDictionaryVector(int *values, int valueSize, BaseVector *vector, int dataTypeId, bool valueAsNullifyIndices = false);
    static void *UnsafeGetValuesDictionary(BaseVector *vector);
    static BaseVector *CastConstVectorToVector(BaseVector *vector);
    static void *UnsafeGetValues(BaseVector *vector);
    static void *UnsafeGetOffsetsAddr(BaseVector *vector);
    static BaseVector *SliceDictionaryVector(BaseVector *vector, int positionOffset, int length);
    static BaseVector *SliceConstVector(BaseVector *vector, int length);
    static BaseVector *SliceVector(BaseVector *vector, int positionOffset, int length);
    static BaseVector *CopyPositionsDictionaryVector(BaseVector *vector, int *positions, int offset, int length);
    static std::unique_ptr<VectorBatch> CopyVectorBatch(VectorBatch *src);
    static BaseVector *CopyPositionsVector(BaseVector *vector, int *positions, int offset, int length);
    static void *UnsafeGetDictionary(BaseVector *vector);
    static void AppendVector(BaseVector *destVector, int32_t offset, BaseVector *srcVector, int32_t length);
    static void ExpandElementVector(BaseVector *elementVec, type::DataTypeId typeId, int32_t newSize);
    static BaseVector *DecodeVarcharDictionaryVector(BaseVector *vector);
    static BaseVector *DecodeDictionaryVector(BaseVector *vector);
    static void CopyFlatVector(BaseVector *destVector, BaseVector *srcVector, int32_t offset, int32_t length);
    static DataTypePtr GetDataType(BaseVector *srcVector);
    static void EmptyArrayProjection(ArrayVector *dstArrVec, DataTypeId typeId);
    static void EmptyMapProjection(MapVector *dstArrVec, DataTypeId keyTypeId, DataTypeId valueTypeId);
    static void CreateNullArrayVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    static void CreateNullMapVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    static void CreateNullRowVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output);
    static std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row);

    template <typename T> static BaseVector *CreateDictionary(int32_t *values, int32_t valueSize, Vector<T> *vector, bool valueAsNullifyIndices = false)
    {
        auto nullsBuffer = std::make_unique<NullsBuffer>(valueSize);
        for (int i = 0; i < valueSize; i++) {
            if (UNLIKELY(vector->IsNull(values[i]) || (valueAsNullifyIndices && values[i] == -1))) {
                nullsBuffer->SetNull(i);
            }
        }

        auto dictionary = std::make_shared<DictionaryContainer<T>>(values, valueSize,
            unsafe::UnsafeVector::GetValues<T>(vector), vector->GetSize(), vector->GetOffset());
        return new Vector<DictionaryContainer<T>>(valueSize, dictionary, nullsBuffer.get(), TYPE_ID<T>);
    }

    template <type::DataTypeId typeId>
    static BaseVector *CreateFlatVector(int32_t size, int32_t capacityInBytes = INITIAL_STRING_SIZE)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            return new Vector<LargeStringContainer<std::string_view>>(size, capacityInBytes, typeId);
        }
        return new Vector<T>(size, typeId);
    }

    template <type::DataTypeId typeId>
    static std::shared_ptr<BaseVector> CreateFlatVectorShared(int32_t size,
        int32_t capacityInBytes = INITIAL_STRING_SIZE)
    {
        using T = typename type::NativeType<typeId>::type;
        if constexpr (std::is_same_v<T, std::string_view>) {
            return std::make_shared<Vector<LargeStringContainer<std::string_view>>>(size, capacityInBytes, typeId);
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

    template <typename T> static BaseVector *DecodeFlatDictionaryVector(BaseVector *vector)
    {
        int size = vector->GetSize();
        auto *flatVector = new Vector<T>(size);
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
    static ALWAYS_INLINE BaseVector *CreateStringVector(uint32_t vectorSize)
    {
        return new Vector<LargeStringContainer<std::string_view>>(vectorSize);
    }

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
}

#endif // OMNI_RUNTIME_VECTOR_HELPER_H
