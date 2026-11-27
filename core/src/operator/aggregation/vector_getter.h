/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */
#ifndef OMNI_RUNTIME_VECTOR_GETTER_H
#define OMNI_RUNTIME_VECTOR_GETTER_H

#include <cstddef>
#include "vector/vector_batch.h"
#include "vector/dictionary_container.h"
#include "memory/simple_arena_allocator.h"
#include "type/string_ref.h"
#include "type/data_type.h"
#include "vector/unsafe_vector.h"
#include "operator/aggregation/container_vector.h"
#include "vector/vector_helper.h"
#include "definitions.h"
#include "util/type_util.h"

namespace omniruntime {
using namespace type;
namespace op {
template <type::DataTypeId OmniId> void *GetValuesFromVector(BaseVector *vector)
{
    using T = typename NativeType<OmniId>::type;
    if constexpr (std::is_same_v<std::string_view, T>) {
        // note: need offsets for varChar, only return values
        auto largeStringVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
        return unsafe::UnsafeStringVector::GetValues(largeStringVector);
    } else {
        auto rawVector = reinterpret_cast<Vector<T> *>(vector);
        return unsafe::UnsafeVector::GetRawValues<T>(rawVector);
    }
}

template <type::DataTypeId OmniId> void *GetValuesFromDict(BaseVector *vector)
{
    using T = typename NativeType<OmniId>::type;
    if constexpr (std::is_same_v<std::string_view, T>) {
        // note: need offsets for varChar, only return values
        auto *stringDictVector = reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(vector);
        return unsafe::UnsafeDictionaryVector::GetVarCharDictionary(stringDictVector);
    } else {
        auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
        return unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
    }
}

template <type::DataTypeId OmniId> const int32_t *GetIdsFromDict(BaseVector *vector)
{
    using T = typename NativeType<OmniId>::type;
    auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
    return unsafe::UnsafeDictionaryVector::GetIds(dictVector);
}

// The trailing per-task pool is threaded from AggregationOperator::GetOutput so the
// freshly-built output vectors are charged to the operator's pool (pool-bound ctors,
// never the [[deprecated]] no-pool bridges).
using NewUniqueVectorFunction = std::function<void(VectorBatch *, int, omniruntime::mem::OmniTaskMemoryPool *)>;

using NewUniqueComplexVectorFunction =
    std::function<void(DataTypePtr, VectorBatch *, int, omniruntime::mem::OmniTaskMemoryPool *)>;

template <type::DataTypeId OmniId>
ALWAYS_INLINE void NewUniqueVector(VectorBatch *vectorBatch, int size, omniruntime::mem::OmniTaskMemoryPool *pool)
{
    if (OmniId == OMNI_CONTAINER) {
        auto doubleVector = new Vector<double>(size, pool);
        auto longVector = new Vector<int64_t>(size, pool);
        // container is used in average final stage , the inputs only include doubleVector and longVector
        std::vector<int64_t> vectorAddresses(AVG_VECTOR_COUNT);
        vectorAddresses[0] = reinterpret_cast<int64_t>(doubleVector);
        vectorAddresses[1] = reinterpret_cast<int64_t>(longVector);
        std::vector<DataTypePtr> dataTypes{ DoubleType(), LongType() };
        auto containerVector = std::make_unique<ContainerVector>(size, vectorAddresses, dataTypes, pool);
        vectorBatch->Append(containerVector.release());
    } else {
        auto vector = VectorHelper::CreateVector(pool, OMNI_FLAT, OmniId, size);
        vectorBatch->Append(vector);
    }
}

template <type::DataTypeId OmniId>
ALWAYS_INLINE void NewUniqueComplexVector(DataTypePtr datatType, VectorBatch *vectorBatch, int size,
    omniruntime::mem::OmniTaskMemoryPool *pool)
{
    // COMPLEX TYPE: ARRAY, MAP, ROW
    auto vector = VectorHelper::CreateComplexVector(pool, datatType.get(), size);
    vectorBatch->Append(vector);
}

const std::vector<NewUniqueComplexVectorFunction> newUniqueComplexVectorFunctions {
    NewUniqueComplexVector<OMNI_ARRAY>,      // OMNI_ARRAY
    NewUniqueComplexVector<OMNI_MAP>,        // OMNI_MAP
    NewUniqueComplexVector<OMNI_ROW>,        // OMNI_ROW
};

const std::vector<NewUniqueVectorFunction> newUniqueVectorFunctions {
    nullptr,                          // OMNI_NONE
    NewUniqueVector<OMNI_INT>,        // OMNI_INT
    NewUniqueVector<OMNI_LONG>,       // OMNI_LONG
    NewUniqueVector<OMNI_DOUBLE>,     // OMNI_DOUBLE
    NewUniqueVector<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    NewUniqueVector<OMNI_SHORT>,      // OMNI_SHORT
    NewUniqueVector<OMNI_DECIMAL64>,  // OMNI_DECIMAL64
    NewUniqueVector<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    NewUniqueVector<OMNI_DATE32>,     // OMNI_DATE32
    NewUniqueVector<OMNI_DATE64>,     // OMNI_DATE64
    NewUniqueVector<OMNI_TIME32>,     // OMNI_TIME32
    NewUniqueVector<OMNI_TIME64>,     // OMNI_TIME64
    NewUniqueVector<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                          // OMNI_INTERVAL_MONTHS
    nullptr,                          // OMNI_INTERVAL_DAY_TIME
    NewUniqueVector<OMNI_VARCHAR>,    // OMNI_VARCHAR
    NewUniqueVector<OMNI_CHAR>,       // OMNI_CHAR
    NewUniqueVector<OMNI_CONTAINER>,  // OMNI_CONTAINER
    NewUniqueVector<OMNI_BYTE>,       // OMNI_BYTE
    NewUniqueVector<OMNI_FLOAT>,      // OMNI_FLOAT
    NewUniqueVector<OMNI_VARBINARY>,  // OMNI_VARBINARY
    nullptr,                          // OMNI_ARRAY
    nullptr,                          // OMNI_MAP
    nullptr,                          // OMNI_ROW
    nullptr,                          // OMNI_UNKNOWN
    nullptr,                          // OMNI_FUNCTION
    nullptr,                          // OMNI_OPAQUE
    nullptr                           // OMNI_INVALID
};

static ALWAYS_INLINE void GetDecimalValue(BaseVector *vector, const int32_t &dataTypeId, const int32_t &rowIndex,
    int128_t &decimalValue)
{
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        if (dataTypeId == OMNI_DECIMAL64) {
            decimalValue = reinterpret_cast<Vector<DictionaryContainer<long>> *>(vector)->GetValue(rowIndex);
        } else if (dataTypeId == OMNI_DECIMAL128) {
            decimalValue =
                reinterpret_cast<Vector<DictionaryContainer<Decimal128>> *>(vector)->GetValue(rowIndex).ToInt128();
        }
    } else {
        if (dataTypeId == OMNI_DECIMAL64) {
            decimalValue = reinterpret_cast<Vector<int64_t> *>(vector)->GetValue(rowIndex);
        } else if (dataTypeId == OMNI_DECIMAL128) {
            decimalValue = reinterpret_cast<Vector<Decimal128> *>(vector)->GetValue(rowIndex).ToInt128();
        }
    }
}
}
}
#endif // OMNI_RUNTIME_VECTOR_GETTER_H
