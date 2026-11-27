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

namespace omniruntime::vec::test {
int g_vecSize = 100;
int g_offset = 50;
int g_len = 10;

template <typename T> void v2_slice_get_set_value()
{
    auto parent = new Vector<T>(g_vecSize);
    for (int i = 0; i < g_vecSize; i++) {
        T value = T(i * 2 / 3);
        parent->SetValue(i, value);
    }

    auto vector = (Vector<T> *)(parent->Slice(g_offset, g_len, false));
    EXPECT_EQ(vector->GetTypeId(), parent->GetTypeId());
    delete parent;

    for (int i = 0; i < g_len; i++) {
        T value = T((i + g_offset) * 2 / 3);
        EXPECT_EQ(value, vector->GetValue(i));
    }
    delete vector;
}

template <typename T> void v2_slice_get_set_value_exception()
{
    auto parent = new Vector<T>(g_vecSize);
    for (int i = 0; i < g_vecSize; i++) {
        T value = T(i * 2 / 3);
        parent->SetValue(i, value);
    }

    EXPECT_ANY_THROW(parent->Slice(1000, g_len, false));
    delete parent;
}

template <> void v2_slice_get_set_value<std::string_view>()
{
    auto parent = new Vector<LargeStringContainer<std::string_view>>(g_vecSize);
    for (int i = 0; i < g_vecSize; i++) {
        std::string str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.size());
        parent->SetValue(i, value);
    }

    auto vector = (Vector<LargeStringContainer<std::string_view>> *)(parent->Slice(g_offset, g_len, false));
    EXPECT_EQ(vector->GetTypeId(), parent->GetTypeId());
    delete parent;

    for (int i = 0; i < g_len; i++) {
        std::string str = "string " + std::to_string(i + g_offset);
        std::string_view value(str.data(), str.size());
        EXPECT_EQ(value, vector->GetValue(i));
    }
    delete vector;
}

TEST(vector2, v2_slice_get_set_value_int32) { v2_slice_get_set_value<int32_t>(); }
TEST(vector2, v2_slice_get_set_value_int64) { v2_slice_get_set_value<int64_t>(); }
TEST(vector2, v2_slice_get_set_value_double) { v2_slice_get_set_value<double>(); }
TEST(vector2, v2_slice_get_set_value_string) { v2_slice_get_set_value<std::string_view>(); }

TEST(vector2, v2_slice_get_set_value_dec128) { v2_slice_get_set_value<int128_t>(); }

template <typename T> void v2_slice_container()
{
    int dictionary_size = 10;
    int value_size = 100;
    int *values = new int[value_size];
    std::unique_ptr<NullsBuffer> nullsBuffer = std::make_unique<NullsBuffer>(value_size);
    for (int i = 0; i < value_size; i++) {
        values[i] = i % dictionary_size;
        nullsBuffer->SetNull(i, false);
    }

    using DICTIONARY_DATA_TYPE = typename TYPE_UTIL<T>::DICTIONARY_TYPE;

    auto dictionary = CreateDictionary<DICTIONARY_DATA_TYPE>(dictionary_size);
    auto container = std::make_shared<DictionaryContainer<T>>(values, value_size, dictionary, dictionary_size, 0);
    auto parent = new Vector<DictionaryContainer<T>>(value_size, container, nullsBuffer.get(), TYPE_ID<T>);

    auto vector = parent->Slice(g_offset, g_len);
    delete parent; // purposely deleting parent;

    T value;
    for (int i = 0; i < g_len; i++) {
        value = vector->GetValue(i);
        EXPECT_EQ(dictionary->GetValue(i % dictionary_size), value);
    }

    delete[] values;
    delete vector;
}

TEST(vector2, slice_container_int32) { v2_slice_container<int32_t>(); }

TEST(vector2, slice_container_int64) { v2_slice_container<int64_t>(); }

TEST(vector2, slice_container_double) { v2_slice_container<double>(); }
} // namespace omniruntime::vec::test
