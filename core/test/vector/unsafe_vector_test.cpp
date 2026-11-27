/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "vector/unsafe_vector.h"
#include "vector_test_util.h"

namespace omniruntime::vec::test {
using namespace omniruntime::vec::unsafe;
using namespace omniruntime::type;

void VectorSetNull(BaseVector *vector)
{
    int size = 100;
    for (int i = 0; i < size; i++) {
        if (i % 2) {
            vector->SetNull(i);
        }
    }
}

template <typename DATA_TYPE> void VectorGetValuesAndNulls()
{
    int size = 100;
    auto vector = CreateVectorAndSetValue<DATA_TYPE>(size);
    VectorSetNull(vector.get());

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(vector.get());

    DATA_TYPE *vectorValues = UnsafeVector::GetRawValues(vector.get());

    for (int i = 0; i < size; i++) {
        if (i % 2) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues[i], vector->GetValue(i));
        }
    }
}

template <typename DATA_TYPE> void VectorSliceGetValuesAndNulls()
{
    int size = 100;
    int offSet = 50;
    int sliceLength = 50;

    auto vector = CreateVectorAndSetValue<DATA_TYPE>(size);
    VectorSetNull(vector.get());

    auto vector2 = (Vector<DATA_TYPE> *)(vector->Slice(offSet, sliceLength));

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(vector2);
    DATA_TYPE *vectorValues = UnsafeVector::GetRawValues(vector2);

    for (int i = 0; i < sliceLength; i++) {
        if (i % 2) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues[i], vector2->GetValue(i));
        }
    }
    delete vector2;
}

template <typename DATA_TYPE> void DictionaryVectorGetValuesAndNulls()
{
    int size = 100;

    auto vector = CreateDictionaryVector<DATA_TYPE>(10, 100);
    VectorSetNull(vector.get());

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(vector.get());
    DATA_TYPE *vectorValues = UnsafeDictionaryVector::GetDictionary(vector.get());
    int *valuesIds = UnsafeDictionaryVector::GetIds(vector.get());

    for (int i = 0; i < size; i++) {
        if (i % 2) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues[valuesIds[i]], vector->GetValue(i));
        }
    }
}

void DictionaryStringVectorGetValuesAndNulls()
{
    int dictionarySize = 10;
    auto vector = VectorHelper::CreateStringVector(dictionarySize);
    auto stringVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);

    for (int i = 0; i < dictionarySize; i++) {
        if (i % 2 == 0) {
            stringVector->SetNull(i);
            continue;
        }
        std::string value = "hello___world" + std::to_string(i);
        std::string_view input(value.data(), value.size());
        stringVector->SetValue(i, input);
    }

    int size = 100;
    int *values = new int[size];
    for (int i = 0; i < size; i++) {
        values[i] = i % dictionarySize;
    }

    BaseVector *vectorPtr = VectorHelper::CreateStringDictionary(values, size, stringVector);
    auto dictVector =
        reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vectorPtr);

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(dictVector);
    char *vectorValues = UnsafeDictionaryVector::GetVarCharDictionary(dictVector);
    int *valuesIds = UnsafeDictionaryVector::GetIds(dictVector);
    auto vectorOffsets = UnsafeDictionaryVector::GetDictionaryOffsets(dictVector);
    for (int i = 0; i < size; i++) {
        if (i % 2 == 0) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues + vectorOffsets[valuesIds[i]], dictVector->GetValue(i).data());
        }
    }
    delete[] values;
    delete dictVector;
    delete stringVector;
}

template <typename DATA_TYPE> void DictionaryVectorSliceGetValuesAndNulls()
{
    int offSet = 50;
    int sliceLength = 50;

    auto vector = CreateDictionaryVector<DATA_TYPE>(10, 100);
    VectorSetNull(vector.get());

    auto vector2 = vector->Slice(offSet, sliceLength);

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(vector2);
    DATA_TYPE *vectorValues = UnsafeDictionaryVector::GetDictionary(vector2);
    int *valuesIds = UnsafeDictionaryVector::GetIds(vector2);

    for (int i = 0; i < sliceLength; i++) {
        if (i % 2) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues[valuesIds[i]], vector2->GetValue(i));
        }
    }
    delete vector2;
}

template <typename CONTAINER> void StringVectorGetValuesAndNulls()
{
    int size = 100;

    auto baseVector = CreateStringTestVector<CONTAINER>(1000);
    VectorSetNull(baseVector);

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(baseVector);
    auto *vector = (Vector<CONTAINER> *)baseVector;
    char *vectorValues = UnsafeStringVector::GetValues(vector);
    auto valueOffsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector));
    for (int i = 0; i < size; i++) {
        if (i % 2) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues + valueOffsets[i], vector->GetValue(i).data());
        }
    }
    delete vector;
}

template <typename CONTAINER> void StringVectorSliceGetValuesAndNulls()
{
    int offSet = 50;
    int sliceLength = 50;

    auto baseVector = CreateStringTestVector<CONTAINER>(1000);
    VectorSetNull(baseVector);

    auto *vector = (Vector<CONTAINER> *)baseVector;
    auto vector2 = (Vector<CONTAINER> *)(vector->Slice(offSet, sliceLength));

    auto vectorNulls = UnsafeBaseVector::GetNullsHelper(vector2);
    char *vectorValues = UnsafeStringVector::GetValues(vector2);
    auto valueOffsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(vector2));
    for (int i = 0; i < sliceLength; i++) {
        if (i % 2) {
            EXPECT_TRUE((*vectorNulls)[i]);
        } else {
            EXPECT_EQ(vectorValues + valueOffsets[i], vector2->GetValue(i).data());
        }
    }
    delete vector2;
    delete vector;
}

template <typename CONTAINER> void StringVectorGetStringBuffer()
{
    auto baseVector = CreateStringTestVector<CONTAINER>(1000);
    auto *vector = (Vector<CONTAINER> *)baseVector;
    auto vectorCapacityInBytes =
        UnsafeStringContainer::GetCapacityInBytes(unsafe::UnsafeStringVector::GetContainer(vector).get());

    auto real = UnsafeStringContainer::GetStringBufferAddr(unsafe::UnsafeStringVector::GetContainer(vector).get());

    auto expect = UnsafeStringContainer::GetBufferWithSpace(unsafe::UnsafeStringVector::GetContainer(vector).get(),
                                                            vectorCapacityInBytes);

    EXPECT_EQ(expect, real);

    auto newCapacityInBytes =
        UnsafeStringContainer::GetCapacityInBytes(unsafe::UnsafeStringVector::GetContainer(vector).get());
    EXPECT_EQ(vectorCapacityInBytes, newCapacityInBytes);
    delete vector;
}

template <typename CONTAINER> void StringVectorGetStringExpandBuffer()
{
    int requestSize = INITIAL_STRING_SIZE + 10;
    auto baseVector = CreateStringTestVector<CONTAINER>(1000);
    auto *vector = (Vector<CONTAINER> *)baseVector;
    auto vectorCapacityInBytes =
        UnsafeStringContainer::GetCapacityInBytes(unsafe::UnsafeStringVector::GetContainer(vector).get());

    auto expect = UnsafeStringContainer::GetBufferWithSpace(unsafe::UnsafeStringVector::GetContainer(vector).get(),
                                                            vectorCapacityInBytes + requestSize);

    auto real = UnsafeStringContainer::GetStringBufferAddr(unsafe::UnsafeStringVector::GetContainer(vector).get());
    EXPECT_EQ(expect, real);

    auto newCapacityInBytes =
        UnsafeStringContainer::GetCapacityInBytes(unsafe::UnsafeStringVector::GetContainer(vector).get());
    EXPECT_GE(newCapacityInBytes, vectorCapacityInBytes + requestSize);
    delete vector;
}

template <typename CONTAINER> void StringVectorGetStringFirstBuffer()
{
    int valueSize = 1000;

    auto baseVector = VectorHelper::CreateStringVector(valueSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    int requestSize = 10;
    auto expect =
        UnsafeStringContainer::GetBufferWithSpace(unsafe::UnsafeStringVector::GetContainer(vector).get(), requestSize);

    auto real = UnsafeStringContainer::GetStringBufferAddr(unsafe::UnsafeStringVector::GetContainer(vector).get());
    EXPECT_EQ(expect, real);

    auto newCapacityInBytes =
        UnsafeStringContainer::GetCapacityInBytes(unsafe::UnsafeStringVector::GetContainer(vector).get());
    EXPECT_EQ(newCapacityInBytes, INITIAL_STRING_SIZE);
    delete vector;
}

void GetVarcharDictionaryWithEmptyStrings()
{
    int dictSize = 10;
    auto *stringVector = new Vector<LargeStringContainer<std::string_view>>(dictSize, 0);

    int valueSize = 100;
    int *values = new int[valueSize];
    for (int i = 0; i < valueSize; i++) {
        values[i] = i % dictSize;
    }

    auto vectorPtr = VectorHelper::CreateStringDictionary(values, valueSize, stringVector);
    auto *dictAddr = VectorHelper::UnsafeGetDictionary(vectorPtr);
    EXPECT_TRUE(dictAddr != nullptr);

    delete[] values;
    delete stringVector;
    delete vectorPtr;
}

TEST(unsafe_vector, int_get_values_and_nulls) { VectorGetValuesAndNulls<int32_t>(); }

TEST(unsafe_vector, int_slice_get_values_and_nulls) { VectorSliceGetValuesAndNulls<int32_t>(); }

TEST(unsafe_vector, int_dictionary_get_values_and_nulls) { DictionaryVectorGetValuesAndNulls<int32_t>(); }

TEST(unsafe_vector, string_dictionary_get_values_and_nulls) { DictionaryStringVectorGetValuesAndNulls(); }

TEST(unsafe_vector, int_dictionary_slice_get_values_and_nulls) { DictionaryVectorSliceGetValuesAndNulls<int32_t>(); }

TEST(unsafe_vector, large_string_get_values_and_nulls)
{
    StringVectorGetValuesAndNulls<LargeStringContainer<std::string_view>>();
}

TEST(unsafe_vector, large_string_slice_get_values_and_nulls)
{
    StringVectorSliceGetValuesAndNulls<LargeStringContainer<std::string_view>>();
}

TEST(unsafe_vector, string_vector_get_string_buffer)
{
    StringVectorGetStringBuffer<LargeStringContainer<std::string_view>>();
}

TEST(unsafe_vector, string_vector_get_string_expand_buffer)
{
    StringVectorGetStringExpandBuffer<LargeStringContainer<std::string_view>>();
}

TEST(unsafe_vector, string_vector_get_string_first_buffer)
{
    StringVectorGetStringFirstBuffer<LargeStringContainer<std::string_view>>();
}

TEST(unsafe_vector, get_varchar_dictionary_with_empty_strings) { GetVarcharDictionaryWithEmptyStrings(); }
} // namespace omniruntime::vec::test
