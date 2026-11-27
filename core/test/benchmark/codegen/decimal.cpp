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
#include <array>
#include "gtest/gtest.h"
#include "benchmark/benchmark.h"
#include "codegen/functions/decimal_arithmetic_functions.h"
#include "operator/execution_context.h"

using namespace std;
using namespace omniruntime::op;
using namespace omniruntime::codegen;
using namespace omniruntime::codegen::function;
namespace om_benchmark {

const static int ROW_SIZE = 8;
const static array<int64_t, ROW_SIZE> HIGH_BITS = {0, 27, 0, 998, 0, 1L << 63, 0, 0};
const static array<uint64_t, ROW_SIZE> LOW_BITS = {0, 0, 865, 0, 66, UINT64_MAX, 99999, 0};

const static array<int64_t, ROW_SIZE> HIGH_BITS2 = {0, 27, 0, 998, 0, 1L << 63, 0, 0};
const static array<uint64_t, ROW_SIZE> LOW_BITS2 = {1, 0, 865, 0, 66, UINT64_MAX, 99999, 1};

const static array<int64_t, ROW_SIZE - 1> HIGH_BITS_RESULT = {270, 0, 9980, 0, -9223372036854775799L, 0, 0};
const static array<uint64_t, ROW_SIZE - 1> LOW_BITS_RESULT = {0, 8650, 0, 660, 18446744073709551606UL, 999990, 0};

static void Decimal128Operation(benchmark::State &state)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high;
    uint64_t low;
    array<int64_t, ROW_SIZE - 1> highResult{};
    array<uint64_t, ROW_SIZE - 1> lowResult{};
    for (auto _ : state) {
        for (int i = 0; i < ROW_SIZE - 1; i++) {
            MulDec128Dec128Dec128ReScale(contextPtr, HIGH_BITS[i], LOW_BITS[i], 22, 1, HIGH_BITS[i + 1],
                                         LOW_BITS[i + 1], 22, 1, 38, 2, &high, &low);
            AddDec128Dec128Dec128ReScale(contextPtr, high, low, 22, 1, HIGH_BITS[i + 1], LOW_BITS[i + 1], 22, 1, 38, 2,
                                         &high, &low);
            ModDec128Dec128Dec128ReScale(contextPtr, high, low, 22, 1, HIGH_BITS2[i + 1], LOW_BITS2[i + 1], 22, 1, 38,
                                         2, &high, &low);
            SubDec128Dec128Dec128ReScale(contextPtr, high, low, 22, 1, HIGH_BITS[i + 1], LOW_BITS[i + 1], 22, 1, 38, 2,
                                         &high, &low);
            ModDec128Dec128Dec128ReScale(contextPtr, high, low, 22, 1, HIGH_BITS2[i + 1], LOW_BITS2[i + 1], 22, 1, 38,
                                         2, &high, &low);
            AddDec128Dec128Dec128ReScale(contextPtr, high, low, 22, 1, HIGH_BITS[i + 1], LOW_BITS[i + 1], 22, 1, 38, 2,
                                         &high, &low);
            highResult[i] = high;
            lowResult[i] = low;
        }
    }

    for (int i = 0; i < ROW_SIZE - 1; i++) {
        EXPECT_EQ(highResult[i], HIGH_BITS_RESULT[i]);
        EXPECT_EQ(lowResult[i], LOW_BITS_RESULT[i]);
    }
}

static void Decimal128MulOperation(benchmark::State &state)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high;
    uint64_t low;
    array<int64_t, ROW_SIZE - 1> highResult{};
    array<uint64_t, ROW_SIZE - 1> lowResult{};
    for (auto _ : state) {
        for (int i = 0; i < ROW_SIZE - 1; i++) {
            MulDec128Dec128Dec128ReScale(contextPtr, HIGH_BITS[i], LOW_BITS[i], 22, 1, HIGH_BITS[i + 1],
                                         LOW_BITS[i + 1], 22, 1, 38, 2, &high, &low);
            highResult[i] = high;
            lowResult[i] = low;
        }
    }
}

BENCHMARK(Decimal128Operation);
BENCHMARK(Decimal128MulOperation);

BENCHMARK_MAIN();
} // namespace om_benchmark
