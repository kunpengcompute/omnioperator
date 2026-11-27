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
#include "vector/dictionary_container.h"
#include "vector_test_util.h"
//
// Created by ken on 2022-09-19.
//

namespace omniruntime::vec::test {
template <typename T> void dict_container_get_set_value()
{
    int dictionarySize = 10;
    int valueSize = 100;
    int *values = new int[valueSize];
    for (int i = 0; i < valueSize; i++) {
        values[i] = i % dictionarySize;
    }
    using DictType = std::conditional_t<is_container_v<T>, LargeStringContainer<T>, AlignedBuffer<T>>;
    std::shared_ptr<DictType> dictionary = nullptr;
    if constexpr (std::is_same_v<std::string_view, T>) {
        dictionary = CreateStringDictionary<T, LargeStringContainer>(dictionarySize);
    } else {
        dictionary = CreateDictionary<T>(dictionarySize);
    }

    omniruntime::vec::DictionaryContainer<T> container(values, valueSize, dictionary, dictionarySize, 0);

    for (int i = 0; i < valueSize; i++) {
        T value = dictionary->GetValue((i + 1) % dictionarySize);
        container.SetValue(i, value);
        EXPECT_EQ(value, container.GetValue(i));
    }
    delete[] values;
}

TEST(vector2, dict_container_get_set_value_int32) { dict_container_get_set_value<int32_t>(); }

TEST(vector2, dict_container_get_set_value_int64) { dict_container_get_set_value<int64_t>(); }

TEST(vector2, dict_container_get_set_value_double) { dict_container_get_set_value<double>(); }

TEST(vector2, dict_container_get_set_value_boost_dec128) { dict_container_get_set_value<boost_dec128>(); }

TEST(vector2, dict_container_get_set_value_string_view) { dict_container_get_set_value<std::string_view>(); }
} // namespace omniruntime::vec::test
