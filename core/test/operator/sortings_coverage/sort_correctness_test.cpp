/**
 * Copyright (C) 2023-2026. Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "operator/inplace_pdqsort.h"
#include "operator/pages_index.h"
#include "operator/pdqsort.h"
#include "operator/timsort.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
std::vector<uint64_t> IdentityAddrs(size_t n)
{
    std::vector<uint64_t> a(n);
    std::iota(a.begin(), a.end(), 0);
    return a;
}

VectorBatch *MakeVarcharBatch(const std::vector<std::string> &data, const std::vector<bool> &isNull = {})
{
    const int32_t n = int32_t(data.size());
    auto *vb = new VectorBatch(n);
    auto *col = new Vector<LargeStringContainer<std::string_view>>(n);
    for (int32_t i = 0; i < n; ++i) {
        if (size_t(i) < isNull.size() && isNull[i]) {
            col->SetNull(i);
        } else {
            col->SetValue(i, std::string_view(data[i]));
        }
    }
    vb->Append(col);
    return vb;
}

std::vector<std::string> ReadSortedVarchar(PagesIndex &pi, int32_t col = 0)
{
    std::vector<std::string> out;
    auto *addrs = pi.GetValueAddresses();
    const int32_t n = int32_t(pi.GetRowCount());
    auto **columns = pi.GetColumns()[col];
    out.reserve(n);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t batch = uint32_t(addrs[i] >> 32), row = uint32_t(addrs[i]);
        auto *v = columns[batch];
        if (v->IsNull(row)) {
            out.emplace_back("NULL");
        } else {
            out.emplace_back(static_cast<Vector<LargeStringContainer<std::string_view>> *>(v)->GetValue(row));
        }
    }
    return out;
}

std::vector<int32_t> ReadSortedInt(PagesIndex &pi, int32_t col = 0)
{
    std::vector<int32_t> out;
    auto *addrs = pi.GetValueAddresses();
    const int32_t n = int32_t(pi.GetRowCount());
    auto **columns = pi.GetColumns()[col];
    out.reserve(n);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t batch = uint32_t(addrs[i] >> 32), row = uint32_t(addrs[i]);
        out.push_back(static_cast<Vector<int32_t> *>(columns[batch])->GetValue(row));
    }
    return out;
}
}  // namespace

TEST(SortCorrectnessPdqTest, test_random_asc_desc)
{
    std::mt19937 rng(123);
    for (int n : {0, 1, 2, 23, 24, 128, 513}) {
        std::vector<int64_t> values(n);
        for (int i = 0; i < n; ++i) {
            values[i] = int64_t(rng() % 10000) - 5000;
        }
        auto addrs = IdentityAddrs(values.size());
        auto expected = values;
        std::sort(expected.begin(), expected.end());
        pdqsort::pdqsort<int64_t, 1>(values.data(), addrs.data(), 0, n);
        ASSERT_EQ(values, expected) << "n=" << n << " ascending";

        values = expected;
        std::shuffle(values.begin(), values.end(), rng);
        addrs = IdentityAddrs(values.size());
        auto expectedDesc = values;
        std::sort(expectedDesc.begin(), expectedDesc.end(), std::greater<int64_t>{});
        pdqsort::pdqsort<int64_t, 0>(values.data(), addrs.data(), 0, n);
        ASSERT_EQ(values, expectedDesc) << "n=" << n << " descending";
    }
}

TEST(SortCorrectnessPdqTest, test_addresses_permute_with_keys)
{
    std::vector<int64_t> values = {30, 10, 20, 40};
    std::vector<uint64_t> addrs = {3, 1, 2, 4};
    pdqsort::pdqsort<int64_t, 1>(values.data(), addrs.data(), 0, 4);
    ASSERT_EQ(values, (std::vector<int64_t>{10, 20, 30, 40}));
    ASSERT_EQ(addrs, (std::vector<uint64_t>{1, 2, 3, 4}));
}

// inplace_pdqsort is what PagesIndex::SortInplace calls (single fixed-width column).
TEST(SortCorrectnessInplacePdqTest, test_asc_desc_and_duplicates)
{
    {
        std::vector<int64_t> values = {5, 1, 4, 2, 3, 9, 0, 8, 7, 6};
        auto expected = values;
        std::sort(expected.begin(), expected.end());
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_EQ(values, expected);
    }
    {
        std::vector<int32_t> values = {1, 5, 2, 5, 3, 5, 0};
        auto expected = values;
        std::sort(expected.begin(), expected.end(), std::greater<int32_t>{});
        inplace_pdqsort::pdqsort(values.begin(), values.end(), std::greater<int32_t>{});
        ASSERT_EQ(values, expected);
    }
    {
        constexpr int n = 512;
        std::vector<int64_t> values(n, 1);
        for (int i = 0; i < 16; ++i) {
            values[n - 1 - i] = 10000 + i;
        }
        auto expected = values;
        std::sort(expected.begin(), expected.end());
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_EQ(values, expected);
    }
}

TEST(SortCorrectnessInplacePdqTest, test_patterns_for_internal_branches)
{
    {
        std::vector<int> empty;
        inplace_pdqsort::pdqsort(empty.begin(), empty.end());
        ASSERT_TRUE(empty.empty());
        std::vector<int> one = {42};
        inplace_pdqsort::pdqsort(one.begin(), one.end());
        ASSERT_EQ(one[0], 42);
        std::vector<int> tiny = {3, 1, 2};
        inplace_pdqsort::pdqsort(tiny.begin(), tiny.end());
        ASSERT_EQ(tiny, (std::vector<int>{1, 2, 3}));
    }

    {
        std::vector<int64_t> values(200);
        std::iota(values.begin(), values.end(), 0);
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_TRUE(std::is_sorted(values.begin(), values.end()));
    }

    {
        std::vector<int64_t> values(300);
        for (int i = 0; i < 300; ++i) {
            values[i] = 299 - i;
        }
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_TRUE(std::is_sorted(values.begin(), values.end()));
    }

    {
        std::vector<int64_t> values(512);
        for (int i = 0; i < 512; ++i) {
            values[i] = (i * 37 + 11) % 512;
        }
        auto expected = values;
        std::sort(expected.begin(), expected.end());
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_EQ(values, expected);
    }

    {
        std::vector<int64_t> values(256);
        std::iota(values.begin(), values.end(), 0);
        std::swap(values[10], values[11]);
        std::swap(values[100], values[102]);
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_TRUE(std::is_sorted(values.begin(), values.end()));
    }

    {
        std::vector<int64_t> values(180, 7);
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_TRUE(std::all_of(values.begin(), values.end(), [](int64_t x) { return x == 7; }));
    }

    {
        std::vector<double> values = {3.5, -1.0, 2.25, 0.0, 2.25, -0.5};
        auto expected = values;
        std::sort(expected.begin(), expected.end());
        inplace_pdqsort::pdqsort(values.begin(), values.end());
        ASSERT_EQ(values, expected);
    }
}

TEST(SortCorrectnessTimSortTest, test_stable_and_random)
{
    std::vector<std::pair<int, int>> items = {{2, 0}, {1, 1}, {2, 2}, {1, 3}};
    timsort::timsort(items.begin(), items.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });
    ASSERT_EQ(items, (std::vector<std::pair<int, int>>{{1, 1}, {1, 3}, {2, 0}, {2, 2}}));

    std::mt19937 rng(99);
    std::vector<int> data(800);
    for (auto &v : data) {
        v = int(rng() % 200);
    }
    auto expected = data;
    std::stable_sort(expected.begin(), expected.end());
    timsort::timsort(data);
    ASSERT_EQ(data, expected);
}

TEST(SortCorrectnessPagesIndexTest, test_varchar_and_int_order)
{
    {
        DataTypes types({VarcharType(16)});
        PagesIndex pi(types);
        pi.AddVecBatch(MakeVarcharBatch({"delta", "alpha", "charlie", "bravo"}, {}));
        pi.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        pi.Sort(cols, asc, nf, 1, 0, 4);
        auto got = ReadSortedVarchar(pi);
        ASSERT_EQ(got, (std::vector<std::string>{"alpha", "bravo", "charlie", "delta"}));
    }
    {
        DataTypes types({IntType()});
        PagesIndex pi(types);
        auto *vb = new VectorBatch(5);
        auto *col = new Vector<int32_t>(5);
        for (int i = 0; i < 5; ++i) {
            col->SetValue(i, 5 - i);
        }
        vb->Append(col);
        pi.AddVecBatch(vb);
        pi.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        pi.Sort(cols, asc, nf, 1, 0, 5);
        ASSERT_EQ(ReadSortedInt(pi), (std::vector<int32_t>{1, 2, 3, 4, 5}));
    }
    {
        DataTypes types({VarcharType(8)});
        PagesIndex pi(types);
        pi.AddVecBatch(MakeVarcharBatch({"c", "a", "b"}, {false, true, false}));
        pi.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};  // nulls first
        pi.Sort(cols, asc, nf, 1, 0, 3);
        auto got = ReadSortedVarchar(pi);
        ASSERT_EQ(got[0], "NULL");
        ASSERT_EQ(got[1], "b");
        ASSERT_EQ(got[2], "c");
    }
}
#endif
