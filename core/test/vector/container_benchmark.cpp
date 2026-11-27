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
#include "benchmark/benchmark.h"
#include "vector/vector.h"
#include "vector/dictionary_container.h"
#include "vector/vector_helper.h"

class Vector;

namespace omniruntime::vec::test {
template <typename CONTAINER> void bm_vector_setvalue_string(benchmark::State &state)
{
    int vecSize = 10000;

    std::string valuePrefix;
    valuePrefix = "hello hello hello hello hello: ";

    for (auto _ : state) {
        auto baseVector = VectorHelper::CreateStringVector(vecSize);
        auto *vector = (Vector<CONTAINER> *)baseVector;

        for (int i = 0; i < 1'000'000; i++) {
            std::string value{valuePrefix + std::to_string(i % 100000)};
            std::string_view input(value.data(), value.size());
            vector->SetValue(i % vecSize, input);
        }

        delete vector;
    }
}

template <typename CONTAINER> void bm_vector_getvalue_string(benchmark::State &state)
{
    int vecSize = 10000;

    std::string valuePrefix;
    valuePrefix = "hello hello hello hello hello: ";

    BaseVector *baseVector = VectorHelper::CreateStringVector(vecSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    for (int i = 0; i < vecSize; i++) {
        std::string value{valuePrefix + std::to_string(i % 100000)};
        std::string_view input(value.data(), value.size());
        vector->SetValue(i % vecSize, input);
    }

    for (auto _ : state) {
        for (int i = 0; i < vecSize; i++) {
            std::string_view getValue = vector->GetValue(i);
            std::string output(getValue);
        }
    }

    delete vector;
}

template <typename CONTAINER> void bm_vector_create(benchmark::State &state)
{
    int vecSize = 10000;

    for (auto _ : state) {
        auto baseVector = VectorHelper::CreateStringVector(vecSize);
        auto *vector = (Vector<CONTAINER> *)baseVector;
        delete vector;
    }
}

BENCHMARK_TEMPLATE(bm_vector_setvalue_string, LargeStringContainer<std::string_view>);
BENCHMARK_TEMPLATE(bm_vector_getvalue_string, LargeStringContainer<std::string_view>);
BENCHMARK_TEMPLATE(bm_vector_create, LargeStringContainer<std::string_view>);
} // namespace omniruntime::vec::test
