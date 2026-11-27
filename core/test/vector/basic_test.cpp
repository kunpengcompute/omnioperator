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
#include "vector/vector.h"
#include "vector_test_util.h"
#include "vector/dictionary_container.h"
#include "vector/vector_helper.h"

namespace omniruntime::vec::test {
template <typename T> void vector_get_set_value()
{
    int vecSize = 100;
    auto vector = std::make_unique<Vector<T>>(vecSize);
    EXPECT_EQ(vector->GetTypeId(), TYPE_ID<T>);
    for (int i = 0; i < vecSize; i++) {
        T value = static_cast<T>(i) * 2 / 3;
        vector->SetValue(i, value);
    }

    for (int i = 0; i < vecSize; i++) {
        T value = static_cast<T>(i) * 2 / 3;
        EXPECT_EQ(value, vector->GetValue(i));
    }
}

template <typename T> void vector_has_null()
{
    int vecSize = 100;
    auto vector = std::make_unique<Vector<T>>(vecSize);
    for (int i = 0; i < vecSize; i++) {
        T value = static_cast<T>(i) * 2 / 3;
        vector->SetValue(i, value);
    }

    bool hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, false);

    vector->SetNull(vecSize - 1);
    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);

    for (int i = 0; i < vecSize; i++) {
        if (i % 2 == 0) {
            vector->SetNull(i);
            continue;
        }

        T value = static_cast<T>(i) * 2 / 3;
        vector->SetValue(i, value);
    }

    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);

    for (int i = 0; i < vecSize; i++) {
        vector->SetNull(i);
    }

    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);
}

template <> void vector_has_null<std::string_view>()
{
    int vecSize = 100;
    auto vector = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(vecSize);
    for (int i = 0; i < vecSize; i++) {
        std::string str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.size());
        vector->SetValue(i, value);
    }

    bool hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, false);

    vector->SetNull(vecSize - 1);
    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);

    for (int i = 0; i < vecSize; i++) {
        if (i % 2 == 0) {
            vector->SetNull(i);
            continue;
        }

        std::string str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.size());
        vector->SetValue(i, value);
    }

    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);

    for (int i = 0; i < vecSize; i++) {
        vector->SetNull(i);
    }

    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);
}

template <typename T> void dict_vector_get_value_with_null()
{
    int dicSize = 10;
    int valueSize = 100;
    int *values = new int[valueSize];
    for (int i = 0; i < valueSize; i++) {
        values[i] = i % dicSize;
    }

    auto *dictionary = new Vector<T>(dicSize);
    for (int i = 0; i < dicSize; i++) {
        if (i % 2 == 0) {
            dictionary->SetNull(i);
            continue;
        }
        T value = static_cast<T>(i * 2 / 3);
        dictionary->SetValue(i, value);
    }

    BaseVector *vectorPtr = VectorHelper::CreateDictionary(values, valueSize, dictionary);
    auto *vector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vectorPtr);

    for (int i = 0; i < valueSize; i++) {
        if (values[i] % 2 == 0) {
            EXPECT_EQ(dictionary->IsNull(values[i]), vector->IsNull(i));
            continue;
        }
        T value = vector->GetValue(i);
        EXPECT_EQ(dictionary->GetValue(i % dicSize), value);
    }
    delete[] values;
    delete vector;
    delete dictionary;
}

template <> void dict_vector_get_value_with_null<std::string_view>()
{
    int dicSize = 10;
    int valueSize = 100;
    int *values = new int[valueSize];
    for (int i = 0; i < valueSize; i++) {
        values[i] = i % dicSize;
    }

    auto *dictionary = new Vector<LargeStringContainer<std::string_view>>(dicSize);
    for (int i = 0; i < dicSize; i++) {
        if (i % 2 == 0) {
            dictionary->SetNull(i);
            continue;
        }
        std::string str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.size());
        dictionary->SetValue(i, value);
    }

    BaseVector *vectorPtr = VectorHelper::CreateStringDictionary(values, valueSize, dictionary);
    auto *vector = reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(vectorPtr);

    for (int i = 0; i < valueSize; i++) {
        if (values[i] % 2 == 0) {
            EXPECT_EQ(dictionary->IsNull(values[i]), vector->IsNull(i));
            continue;
        }
        std::string_view value = vector->GetValue(i);
        EXPECT_EQ(dictionary->GetValue(i % dicSize), value);
    }
    delete[] values;
    delete vector;
    delete dictionary;
}

template <typename T> T GetTestValue(int32_t index)
{
    T value;
    if constexpr (std::is_same_v<std::string_view, T>) {
        std::string str = "string " + std::to_string(index);
        value = std::string_view(str.data(), str.size());
    } else {
        value = static_cast<T>(index) * 2 / 3;
    }
    return value;
}

template <typename T> void vector_append_value()
{
    int vecSize = 5;
    Vector<T> v1{vecSize};
    std::vector<T> expected;
    for (int32_t i = 0; i < vecSize; i++) {
        T value = GetTestValue<T>(i);
        v1.SetValue(i, value);
        expected.template emplace_back(value);
    }

    int32_t appendedVecSize = 15;
    Vector<T> appended{appendedVecSize};
    appended.Append(&v1, 0, vecSize);

    Vector<T> v2WithNull{vecSize};
    for (int32_t i = 0; i < vecSize; i++) {
        if (i % 2 == 0) {
            v2WithNull.SetNull(i);
            continue;
        }
        v2WithNull.SetValue(i, expected[i]);
    }
    appended.Append(&v2WithNull, 5, 5);

    Vector<T> v3Emtpy{0};
    EXPECT_ANY_THROW(appended.Append(&v3Emtpy, 10, 0));

    Vector<T> v4OverBounds{vecSize};
    for (int32_t i = 0; i < vecSize; i++) {
        v4OverBounds.SetValue(i, expected[i]);
    }
    EXPECT_ANY_THROW(appended.Append(&v4OverBounds, 10, vecSize + 1));

    std::vector<bool> expectedNull{false, false, false, false, false, true, false, true, false, true};
    for (int32_t i = 0; i < appendedVecSize; i++) {
        // append empty vector or beyond the bound
        if (i >= 10) {
            EXPECT_FALSE(appended.IsNull(i));
            // for number it is random value
            if constexpr (std::is_same_v<std::string_view, T>) {
                EXPECT_EQ("", appended.GetValue(i));
            }
            continue;
        }
        // append success for value check
        if (appended.IsNull(i)) {
            EXPECT_EQ(expectedNull[i], appended.IsNull(i));
            continue;
        }
        EXPECT_EQ(expected[i % 5], appended.GetValue(i));
    }
}

template <typename T> void vector_copy_positions_value()
{
    int vecSize = 10;
    Vector<T> vector{vecSize};
    std::vector<T> expected;
    for (int32_t i = 0; i < vecSize; i++) {
        T value = GetTestValue<T>(i);
        expected.template emplace_back(value);
        if (i % 2 == 0) {
            vector.SetNull(i);
            continue;
        }
        vector.SetValue(i, value);
    }

    int index[] = {2, 3, 4, 5, 6, 7};
    int offset1 = 0;
    int offset2 = 1;
    int copySize = 4;
    auto v1OffsetZero = (Vector<T> *)(vector.CopyPositions(index, offset1, copySize));
    auto v2OffsetNotZero = (Vector<T> *)(vector.CopyPositions(index, offset2, copySize));

    for (int32_t i = 0; i < copySize; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(v1OffsetZero->IsNull(i), true);
            EXPECT_EQ(v2OffsetNotZero->GetValue(i), expected[index[i + offset2]]);
            continue;
        }
        EXPECT_EQ(v2OffsetNotZero->IsNull(i), true);
        EXPECT_EQ(v1OffsetZero->GetValue(i), expected[index[i + offset1]]);
    }

    auto v3Empty = (Vector<T> *)(vector.CopyPositions(index, offset2, 0));
    EXPECT_EQ(v3Empty->GetSize(), 0);
    EXPECT_ANY_THROW(vector.CopyPositions(index, offset1, -1));
    delete v1OffsetZero;
    delete v2OffsetNotZero;
    delete v3Empty;
}

template <typename T> void dict_copy_positions_value()
{
    int dicSize = 10;
    int valueSize = 7;
    auto *dictionary = new Vector<T>(dicSize);
    for (int i = 0; i < dicSize; i++) {
        if (i % 2 == 0) {
            dictionary->SetNull(i);
            continue;
        }
        T value = static_cast<T>(i);
        dictionary->SetValue(i, value);
    }

    int32_t values[] = {2, 3, 4, 5, 6, 8, 9};
    BaseVector *vectorPtr = VectorHelper::CreateDictionary(values, valueSize, dictionary);
    auto vector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vectorPtr);

    int32_t positions[] = {1, 3, 5, 6};
    int32_t offset = 1;
    int32_t newValueSize = 3;
    auto copyPositions = (Vector<DictionaryContainer<T>> *)(vector->CopyPositions(positions, offset, newValueSize));

    for (int i = 0; i < newValueSize; i++) {
        if (values[positions[i + offset]] % 2 == 0) {
            EXPECT_EQ(dictionary->IsNull(values[positions[i + offset]]), copyPositions->IsNull(i));
            continue;
        }
        T expectValue = copyPositions->GetValue(i);
        EXPECT_EQ(dictionary->GetValue(values[positions[i + offset]]), expectValue);
    }

    auto v3Empty = (Vector<DictionaryContainer<T>> *)(vector->CopyPositions(positions, offset, 0));
    EXPECT_EQ(v3Empty->GetSize(), 0);
    EXPECT_ANY_THROW(vector->CopyPositions(positions, offset, -1));
    delete copyPositions;
    delete v3Empty;
    delete vector;
    delete dictionary;
}

template <> void dict_copy_positions_value<std::string_view>()
{
    int dicSize = 10;
    int valueSize = 7;
    auto *dictionary = new Vector<LargeStringContainer<std::string_view>>(dicSize);

    for (int i = 0; i < dicSize; i++) {
        if (i % 2 == 0) {
            dictionary->SetNull(i);
            continue;
        }
        std::string str = std::to_string(i);
        std::string_view value(str.data(), str.size());
        dictionary->SetValue(i, value);
    }

    int32_t values[] = {2, 3, 4, 5, 6, 8, 9};
    BaseVector *vectorPtr = VectorHelper::CreateStringDictionary(values, valueSize, dictionary);
    auto vector = reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vectorPtr);

    int32_t positions[] = {1, 3, 5, 6};
    int32_t offset = 1;
    int32_t newValueSize = 3;
    auto copyPositions = (Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *)(vector->CopyPositions(
        positions, offset, newValueSize));

    for (int i = 0; i < newValueSize; i++) {
        if (values[positions[i + offset]] % 2 == 0) {
            EXPECT_EQ(dictionary->IsNull(values[positions[i + offset]]), copyPositions->IsNull(i));
            continue;
        }
        std::string_view expectValue = copyPositions->GetValue(i);
        EXPECT_EQ(dictionary->GetValue(values[positions[i + offset]]), expectValue);
    }

    auto v3Empty = (Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *)(vector->CopyPositions(
        positions, offset, 0));
    EXPECT_EQ(v3Empty->GetSize(), 0);
    EXPECT_ANY_THROW(vector->CopyPositions(positions, offset, -1));
    delete copyPositions;
    delete v3Empty;
    delete vector;
    delete dictionary;
}

template <typename T> void vec_set_values(int32_t valueSize = 100)
{
    int vecSize = 100;
    auto vector = std::make_unique<Vector<T>>(vecSize);
    auto *data = new T[valueSize];

    for (int i = 0; i < vecSize; i++) {
        T value = static_cast<T>(i);
        vector->SetValue(i, value);
    }

    for (int i = 0; i < valueSize; i++) {
        data[i] = static_cast<T>(i) * 2 / 3;
    }
    if (valueSize > 100) {
        EXPECT_ANY_THROW(vector->SetValues(0, data, valueSize));
    } else {
        vector->SetValues(0, data, valueSize);
    }

    if (vecSize >= valueSize) {
        for (int i = 0; i < vecSize; i++) {
            T value = static_cast<T>(i) * 2 / 3;
            EXPECT_EQ(value, vector->GetValue(i));
        }
    } else {
        for (int i = 0; i < vecSize; i++) {
            T value = static_cast<T>(i);
            EXPECT_EQ(value, vector->GetValue(i));
        }
    }
    delete[] data;
}

template <typename T> void vec_set_nulls(int32_t nullSize = 100)
{
    int vecSize = 100;
    auto vector = std::make_unique<Vector<T>>(vecSize);
    auto nulls = std::make_shared<NullsBuffer>(nullSize);

    for (int i = 0; i < vecSize; i++) {
        if (i % 2 != 0) {
            vector->SetNull(i);
        } else {
            vector->SetNotNull(i);
        }
    }

    for (int i = 0; i < nullSize; i++) {
        if (i % 2 == 0) {
            nulls->SetNull(i, true);
        } else {
            nulls->SetNull(i, false);
        }
    }
    if (nullSize > 100) {
        EXPECT_ANY_THROW(vector->SetNulls(0, nulls.get(), nullSize));
    } else {
        vector->SetNulls(0, nulls.get(), nullSize);
    }

    if (vecSize >= nullSize) {
        for (int i = 0; i < nullSize; i++) {
            EXPECT_EQ(nulls->IsNull(i), vector->IsNull(i));
        }
    } else {
        for (int i = 0; i < vecSize; i++) {
            bool value = static_cast<bool>(i % 2);
            EXPECT_EQ(value, vector->IsNull(i));
        }
    }
}

TEST(vector, vector_get_set_value_int32) { vector_get_set_value<int32_t>(); }

TEST(vector, vector_get_set_value_int64) { vector_get_set_value<int64_t>(); }

TEST(vector, vector_get_set_value_int16) { vector_get_set_value<int16_t>(); }

TEST(vector, vector_get_set_value_double) { vector_get_set_value<double>(); }

TEST(vector, vector_get_set_value_bool) { vector_get_set_value<bool>(); }

TEST(vector, vector_get_set_value_dec128) { vector_get_set_value<int128_t>(); }

TEST(vector, vector_has_null_int32) { vector_has_null<int32_t>(); }

TEST(vector, vector_has_null_int64) { vector_has_null<int64_t>(); }

TEST(vector, vector_has_null_int16) { vector_has_null<int16_t>(); }

TEST(vector, vector_has_null_double) { vector_has_null<double>(); }

TEST(vector, vector_has_null_bool) { vector_has_null<bool>(); }

TEST(vector, vector_has_nulldec128) { vector_has_null<int128_t>(); }

TEST(vector, vector_has_null_string) { vector_has_null<std::string_view>(); }

TEST(vector, odd_vector_has_null)
{
    int vecSize = 25;
    bool hasNull;
    auto vector = std::make_shared<Vector<int32_t>>(vecSize);
    vector->SetNull(vecSize / 2);
    hasNull = vector->HasNull();
    EXPECT_EQ(hasNull, true);
}

TEST(vector, SliceVector)
{
    int vecSize = 25;
    auto vector = std::make_shared<Vector<int32_t>>(vecSize);
    for (int i = 0; i < vecSize; i++) {
        vector->SetValue(i, i);
    }
    auto sliceVector = reinterpret_cast<Vector<int32_t> *>(VectorHelper::SliceVector(vector.get(), 3, 5));
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(sliceVector->GetValue(i), i + 3);
    }
    delete sliceVector;
}

TEST(vector, string_vec_any_size)
{
    for (int i = 1; i < 100; i++) {
        auto vec = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(i);
        int idx = rand() % i;
        std::string str = "hello";
        std::string_view value(str.data(), str.size());
        vec->SetValue(idx, value);
        EXPECT_EQ(value, vec->GetValue(idx));
    }
}

TEST(vector, string_vec_size_0) { auto vec = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(0); }

TEST(vector, append_int32) { vector_append_value<int32_t>(); }

TEST(vector, append_int64) { vector_append_value<int64_t>(); }

TEST(vector, append_int16) { vector_append_value<int16_t>(); }

TEST(vector, append_double) { vector_append_value<double>(); }

TEST(vector, append_bool) { vector_append_value<bool>(); }

TEST(vector, append_boost_dec128) { vector_append_value<int128_t>(); }

TEST(vector, copy_positions_int32) { vector_copy_positions_value<int32_t>(); }

TEST(vector, copy_positions_int64) { vector_copy_positions_value<int64_t>(); }

TEST(vector, copy_positions_int16) { vector_copy_positions_value<int16_t>(); }

TEST(vector, copy_positions_double) { vector_copy_positions_value<double>(); }

TEST(vector, copy_positions_bool) { vector_copy_positions_value<bool>(); }

TEST(vector, copy_positions_dec128) { vector_copy_positions_value<int128_t>(); }

TEST(vector, dict_get_value_with_null_int32) { dict_vector_get_value_with_null<int32_t>(); }

TEST(vector, dict_get_value_with_null_int64) { dict_vector_get_value_with_null<int64_t>(); }

TEST(vector, dict_get_value_with_null_int16) { dict_vector_get_value_with_null<int16_t>(); }

TEST(vector, dict_get_value_with_null_double) { dict_vector_get_value_with_null<double>(); }

TEST(vector, dict_get_value_with_null_bool) { dict_vector_get_value_with_null<bool>(); }

TEST(vector, dict_get_value_with_null_dec128) { dict_vector_get_value_with_null<int128_t>(); }

TEST(vector, dict_get_value_with_null_string) { dict_vector_get_value_with_null<std::string_view>(); }

TEST(vector, dict_copy_position_with_null_int32) { dict_copy_positions_value<int32_t>(); }

TEST(vector, dict_copy_position_with_null_int64) { dict_copy_positions_value<int64_t>(); }

TEST(vector, dict_copy_position_with_null_int16) { dict_copy_positions_value<int16_t>(); }

TEST(vector, dict_copy_position_with_null_double) { dict_copy_positions_value<double>(); }

TEST(vector, dict_copy_position_with_null_bool) { dict_copy_positions_value<bool>(); }

TEST(vector, dict_copy_position_with_dec128) { dict_copy_positions_value<int128_t>(); }

TEST(vector, dict_copy_position_with_null_string) { dict_copy_positions_value<std::string_view>(); }

TEST(vector, vec_set_values_int16) { vec_set_values<int16_t>(); }

TEST(vector, vec_set_values_int32) { vec_set_values<int32_t>(); }

TEST(vector, vec_set_values_int64) { vec_set_values<int64_t>(); }

TEST(vector, vec_set_values_double) { vec_set_values<double>(); }

TEST(vector, vec_set_values_dec128) { vec_set_values<int128_t>(); }

TEST(vector, vec_set_values_int32_out_of_range) { vec_set_values<int32_t>(101); }

TEST(vector, vec_set_nulls_int32) { vec_set_nulls<int32_t>(); }

TEST(vector, vec_set_nulls_int32_out_of_range) { vec_set_nulls<int32_t>(101); }
} // namespace omniruntime::vec::test
