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
#include <arm_neon.h>
#include <iostream>
#include <functional>
#include <vector>
#include <chrono>
#include <limits>
#include <gtest/gtest.h>
#include <ctime>
#include "simd/func/reduce.h"
#include "src/operator/aggregation/aggregator/state_flag_operation.h"

namespace omniruntime {
using namespace simd;

int32_t SimdNoNull(int32_t *values, int32_t size)
{
    int64_t result = 0;
    int64_t flag = 0;
    ReduceExternal<int32_t, int64_t, int64_t, op::StateCountHandler, ReduceFunc::Sum>(&result, flag, values, size);
    return result;
}

template <typename T> T SimdWithNull(T *values, uint8_t *nulls, int32_t size)
{
    int64_t result = 0;
    int64_t flag = 0;
    ReduceWithNullsExternal<T, int64_t, int64_t, op::StateCountHandler, ReduceFunc::Sum>(&result, flag, values, size,
                                                                                         nulls);
    return result;
}

template <typename T> T GccSumWithNulls(T *values, uint8_t *nulls, int32_t size)
{
    int i = 0;
    int64_t result = 0;
    for (; i < size; i++) {
        if (not nulls[i]) {
            result += values[i];
        }
    }
    return result;
}

template <typename T> T SimdSumWithDict(int32_t size, T *values, int32_t *indices)
{
    int64_t result = 0;
    int64_t flag = 0;
    ReduceWithDicExternal<T, int64_t, int64_t, op::StateCountHandler, ReduceFunc::Sum>(&result, flag, values, size,
                                                                                       indices);
    return result;
}

template <typename T> T SimdWithDictWithNull(const T *values, const uint8_t *nulls, int32_t *indices, int32_t size)
{
    double result = 0;
    int64_t flag = 0;
    ReduceWithDicAndNullsExternal<T, double, int64_t, op::StateCountHandler, ReduceFunc::Sum>(&result, flag, values,
                                                                                              size, nulls, indices);
    return result;
}

template <typename T> T SimdMinWithDictWithNull(const T *values, const uint8_t *nulls, int32_t *indices, int32_t size)
{
    T result = std::numeric_limits<T>::max();
    int64_t flag = 0;
    ReduceWithDicAndNullsExternal<T, T, int64_t, op::StateCountHandler, ReduceFunc::Min>(&result, flag, values, size,
                                                                                         nulls, indices);
    return result;
}

template <typename T> T GccMinWithDictWithNull(const T *values, const uint8_t *nulls, int32_t *indices, int32_t size)
{
    T result = std::numeric_limits<T>::max();
    int64_t flag = 0;
    for (int i = 0; i < size; ++i) {
        if (not nulls[i]) {
            result = std::min(result, (T)(values[indices[i]]));
            ++flag;
        }
    }
    return result;
}

template <typename T> T SimdMaxWithDictWithNull(const T *values, const uint8_t *nulls, int32_t *indices, int32_t size)
{
    T result = 0;
    int64_t flag = 0;
    ReduceWithDicAndNullsExternal<T, T, int64_t, op::StateCountHandler, ReduceFunc::Max>(&result, flag, values, size,
                                                                                         nulls, indices);
    return result;
}

template <typename T> T GccMaxWithDictWithNull(const T *values, const uint8_t *nulls, int32_t *indices, int32_t size)
{
    T result = std::numeric_limits<T>::min();
    int64_t flag = 0;
    for (int i = 0; i < size; ++i) {
        if (not nulls[i]) {
            result = std::max(result, (T)(values[indices[i]]));
            ++flag;
        }
    }
    return result;
}

template <typename T> T GccWithDictWithNull(const T *values, const uint8_t *nulls, int32_t *indices, int32_t size)
{
    double result = 0;
    int64_t flag = 0;
    for (int i = 0; i < size; ++i) {
        if (not nulls[i]) {
            result += static_cast<double>(values[indices[i]]);
            ++flag;
        }
    }
    return result;
}

template <typename T> T GccSumDict(int n, T *data, int *idx)
{
    T total_sum = 0;
    for (int i = 0; i < n; i++) {
        total_sum += data[idx[i]];
    }
    return total_sum;
}

int32_t GccTestNoNull(int32_t *values, int32_t size)
{
    int64_t result = 0;

    for (int i = 0; i < size; ++i) {
        result += values[i];
    }
    return result;
}

template <typename Ret, typename... Args> int32_t Calc(const std::string &name, Ret (*func)(Args...), Args... args)
{
    auto start = std::chrono::high_resolution_clock::now();
    float val = 0;
    for (int i = 0; i < 1000; i++) {
        Ret ret = func(args...);
        val += ret;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << name << " spend : " << duration << "ms,result is " << val << std::endl;
    return val;
}

template <typename T> void TestSimdWithNull(int32_t size)
{
    T values[size];
    uint8_t nulls[size];
    for (int i = 0; i < size; ++i) {
        values[i] = i * 1000;
        nulls[i] = i % 200 == 0;
    }
    auto ret = Calc<T>("SimdWithNull", SimdWithNull, values, nulls, (size));
    auto expect = Calc<T>("GccSumWithNulls", GccSumWithNulls, values, nulls, (size));
    EXPECT_EQ(ret, expect);
}

template <typename T> void TestSimdWithNoNull(int32_t size)
{
    T values[size];
    for (int i = 0; i < size; ++i) {
        values[i] = i * 1000;
    }
    auto ret = Calc<T>("simdNoNull", SimdNoNull, values, (size));
    auto expect = Calc<T>("GccTestNoNull", GccTestNoNull, values, (size));
    EXPECT_EQ(ret, expect);
}

template <typename T> void BenchWithDict(int32_t size)
{
    T values[size];
    int32_t indices[size];
    for (int i = 0; i < size; ++i) {
        values[i] = i;
    }
    for (int i = 0; i < size; ++i) {
        indices[i] = 4;
    }

    auto ret = Calc<T, int, T *, int32_t *>("SimdSumWithDict", SimdSumWithDict, (size), values, indices);
    auto expect = Calc<T, int, T *, int32_t *>("GccSumDict", GccSumDict, (size), values, indices);
    EXPECT_EQ(ret, expect);
}

template <typename T> void TestSimdSumWithDictNull(int32_t size)
{
    int32_t indices[size];
    T values[size];
    uint8_t nulls[size];
    for (int i = 0; i < size; ++i) {
        values[i] = i;
    }
    for (int i = 0; i < size; ++i) {
        indices[i] = 40;
        nulls[i] = (i % 20 == 0);
    }

    auto ret = Calc<T, const T *, const uint8_t *, int32_t *, int32_t>("SimdWithDictWithNull", SimdWithDictWithNull,
                                                                       values, nulls, indices, (size));
    auto expect = Calc<T, const T *, const uint8_t *, int32_t *, int32_t>("GccWithDictWithNull", GccWithDictWithNull,
                                                                          values, nulls, indices, (size));
    EXPECT_EQ(ret, expect);
}

template <typename T> void TestSimdMinWithDictNull(int32_t size)
{
    int32_t indices[size];
    T values[size];
    uint8_t nulls[size];
    for (int i = 0; i < size; ++i) {
        values[i] = i;
    }
    for (int i = 0; i < size; ++i) {
        indices[i] = 40;
        nulls[i] = i % 20 == 0;
    }

    auto ret = Calc<T, const T *, const uint8_t *, int32_t *, int32_t>(
        "SimdMinWithDictWithNull", SimdMinWithDictWithNull, values, nulls, indices, (size));
    auto expect = Calc<T, const T *, const uint8_t *, int32_t *, int32_t>(
        "GccMinWithDictWithNull", GccMinWithDictWithNull, values, nulls, indices, (size));
    EXPECT_EQ(ret, expect);
}

template <typename T> void TestSimdMaxWithDictNull(int32_t size)
{
    int32_t indices[size];
    T values[size];
    uint8_t nulls[size];
    for (int i = 0; i < size; ++i) {
        values[i] = i;
    }
    for (int i = 0; i < size; ++i) {
        indices[i] = 40;
        nulls[i] = i % 20 == 0;
    }

    Calc<T, const T *, const uint8_t *, int32_t *, int32_t>("SimdMaxWithDictWithNull", SimdMaxWithDictWithNull, values,
                                                            nulls, indices, (size));
    Calc<T, const T *, const uint8_t *, int32_t *, int32_t>("GccMaxWithDictWithNull", GccMaxWithDictWithNull, values,
                                                            nulls, indices, (size));
}

void Verify()
{
    int32_t expect = 0;
    srand(time(nullptr));
    int labCount = 1000;
    while (labCount--) {
        int value = random() % 59999;
        int nullNum = random();
        if (nullNum < 0) {
            nullNum = -1 * nullNum;
        }
        nullNum %= value;
        std::vector<int32_t> data;
        data.resize(value);
        uint8_t nulls[value];
        int countNull = nullNum;
        for (int i = 0; i < value; i++) {
            if ((countNull != 0) && (rand() & 0x1)) {
                nulls[i] = true;
                --countNull;
            } else {
                nulls[i] = false;
                data[i] = random() % 100;
            }
        }
        expect = 0;
        for (int i = 0; i < value; i++) {
            if (not nulls[i]) {
                expect += data[i];
            }
        }
        int64_t flag = 0;
        int32_t actual = 0;
        ReduceWithNullsExternal<int32_t, int32_t, int64_t, op::StateCountHandler, ReduceFunc::Sum>(
            &actual, flag, data.data(), value, nulls);
        if (expect != actual) {
            for (int i = 0; i < value; ++i) {
                std::cout << data[i] << std::endl;
            }
            for (int i = 0; i < value; ++i) {
                std::cout << (nulls[i] == true) << std::endl;
            }
            EXPECT_EQ(expect, actual);
            break;
        }
    }
}

TEST(AggregatorNeonTest, simd_neon)
{
    Verify();
    TestSimdWithNull<int64_t>(99999);
    TestSimdWithNoNull<int32_t>(99999);
    BenchWithDict<int32_t>(99999);
    // 5% null value
    TestSimdSumWithDictNull<int32_t>(99999);
    TestSimdMinWithDictNull<int32_t>(99999);
    TestSimdMaxWithDictNull<int32_t>(99999);
}
} // namespace omniruntime
