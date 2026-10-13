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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "operator/pages_index.h"
#include "operator/pdqsort.h"
#include "operator/timsort.h"
#include "operator/timsort_pages_index.h"
#include "operator/varchar_sort_policies.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "type/decimal128.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
std::string MakeLen(char c, size_t n)
{
    return std::string(n, c);
}

std::vector<uint64_t> IdentityAddrs(size_t n)
{
    std::vector<uint64_t> a(n);
    std::iota(a.begin(), a.end(), 0);
    return a;
}

template <typename RawType, int32_t Asc>
void AssertPdqSorted(std::vector<int64_t> &values, std::vector<uint64_t> &addrs)
{
    ASSERT_EQ(values.size(), addrs.size());
    pdqsort::pdqsort<RawType, Asc>(values.data(), addrs.data(), 0, int32_t(values.size()));

    for (size_t i = 1; i < values.size(); ++i) {
        if constexpr (std::is_same_v<RawType, double>) {
            double a, b;
            std::memcpy(&a, &values[i - 1], sizeof(double));
            std::memcpy(&b, &values[i], sizeof(double));
            if (Asc) {
                ASSERT_LE(a, b + 1e-12) << "i=" << i;
            } else {
                ASSERT_GE(a + 1e-12, b) << "i=" << i;
            }
        } else if constexpr (std::is_same_v<RawType, float>) {
            float a, b;
            uint32_t ua = uint32_t(values[i - 1]), ub = uint32_t(values[i]);
            std::memcpy(&a, &ua, sizeof(float));
            std::memcpy(&b, &ub, sizeof(float));
            if (Asc) {
                ASSERT_LE(a, b + 1e-6f) << "i=" << i;
            } else {
                ASSERT_GE(a + 1e-6f, b) << "i=" << i;
            }
        } else if constexpr (std::is_same_v<RawType, Decimal128>) {
            auto *a = reinterpret_cast<Decimal128 *>(values[i - 1]);
            auto *b = reinterpret_cast<Decimal128 *>(values[i]);
            if (Asc) {
                ASSERT_FALSE(*a > *b) << "i=" << i;
            } else {
                ASSERT_FALSE(*a < *b) << "i=" << i;
            }
        } else {
            RawType a = RawType(values[i - 1]), b = RawType(values[i]);
            if (Asc) {
                ASSERT_LE(a, b) << "i=" << i;
            } else {
                ASSERT_GE(a, b) << "i=" << i;
            }
        }
    }
}

void AssertAddressesSortedByString(const int64_t *values, const uint32_t *lens, int32_t from, int32_t to, bool ascending)
{
    for (int32_t i = from + 1; i < to; ++i) {
        std::string_view a(reinterpret_cast<const char *>(values[i - 1]), lens[i - 1]);
        std::string_view b(reinterpret_cast<const char *>(values[i]), lens[i]);
        if (ascending) {
            ASSERT_LE(a, b) << "i=" << i << " a=" << a << " b=" << b;
        } else {
            ASSERT_GE(a, b) << "i=" << i << " a=" << a << " b=" << b;
        }
    }
}

template <typename T>
VectorBatch *MakeTypedBatch(const std::vector<T> &data, const std::vector<bool> &isNull = {})
{
    const int32_t n = int32_t(data.size());
    auto *vb = new VectorBatch(n);
    using VecT = std::conditional_t<std::is_same_v<T, std::string>, LargeStringContainer<std::string_view>, T>;
    auto *vec = new Vector<VecT>(n);
    for (int32_t i = 0; i < n; ++i) {
        if (size_t(i) < isNull.size() && isNull[i]) {
            vec->SetNull(i);
        } else if constexpr (std::is_same_v<T, std::string>) {
            vec->SetValue(i, std::string_view(data[i]));
        } else {
            vec->SetValue(i, data[i]);
        }
    }
    vb->Append(vec);
    return vb;
}

inline VectorBatch *MakeVarcharBatch(const std::vector<std::string> &data, const std::vector<bool> &isNull = {})
{
    return MakeTypedBatch(data, isNull);
}

VectorBatch *MakeVarcharDictBatch(const std::vector<std::string> &dictVals, const std::vector<int32_t> &ids,
    BaseVector **keepAlive, const std::vector<bool> &isNull = {})
{
    const int32_t dictSize = int32_t(dictVals.size()), n = int32_t(ids.size());
    auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(dictSize);
    for (int32_t i = 0; i < dictSize; ++i) {
        dictBase->SetValue(i, std::string_view(dictVals[i]));
    }
    *keepAlive = dictBase;
    auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), n, dictBase);
    for (size_t i = 0; i < isNull.size(); ++i) {
        if (isNull[i]) {
            dictVec->SetNull(int32_t(i));
        }
    }
    auto *vb = new VectorBatch(n);
    vb->Append(dictVec);
    return vb;
}

void RunTipi(PagesIndex &pi, int32_t sortColCount, int32_t asc, int32_t nf)
{
    const int32_t n = int32_t(pi.GetRowCount());
    std::vector<int32_t> cols(sortColCount);
    std::iota(cols.begin(), cols.end(), 0);
    std::vector<int32_t> ascA(sortColCount, asc), nfA(sortColCount, nf);
    pi.Sort(cols.data(), ascA.data(), nfA.data(), sortColCount, 0, n);
}

std::vector<std::string> ReadVarcharSorted(PagesIndex &pagesIndex, int32_t colIdx)
{
    const int32_t rowCount = int32_t(pagesIndex.GetRowCount());
    int32_t outputCols[] = {colIdx};
    const int32_t *sourceTypes = pagesIndex.GetTypes().GetIds();
    auto *outBatch = new VectorBatch(rowCount);
    outBatch->Append(new Vector<LargeStringContainer<std::string_view>>(rowCount));
    pagesIndex.GetOutput(outputCols, 1, outBatch, sourceTypes, 0, rowCount);

    std::vector<std::string> out;
    out.reserve(rowCount);
    auto *vec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(outBatch->Get(0));
    for (int32_t i = 0; i < rowCount; ++i) {
        if (vec->IsNull(i)) {
            out.emplace_back("NULL");
        } else {
            out.emplace_back(vec->GetValue(i));
        }
    }
    VectorHelper::FreeVecBatch(outBatch);
    return out;
}

template <typename T>
std::vector<std::optional<T>> ReadTypedSorted(PagesIndex &pagesIndex, int32_t colIdx)
{
    const int32_t rowCount = int32_t(pagesIndex.GetRowCount());
    int32_t outputCols[] = {colIdx};
    const int32_t *sourceTypes = pagesIndex.GetTypes().GetIds();
    auto *outBatch = new VectorBatch(rowCount);
    outBatch->Append(new Vector<T>(rowCount));
    pagesIndex.GetOutput(outputCols, 1, outBatch, sourceTypes, 0, rowCount);

    std::vector<std::optional<T>> out;
    out.reserve(rowCount);
    auto *vec = dynamic_cast<Vector<T> *>(outBatch->Get(0));
    for (int32_t i = 0; i < rowCount; ++i) {
        if (vec->IsNull(i)) {
            out.push_back(std::nullopt);
        } else {
            out.push_back(vec->GetValue(i));
        }
    }
    VectorHelper::FreeVecBatch(outBatch);
    return out;
}

std::string ReadVarcharAtAddress(PagesIndex &pagesIndex, int32_t colIdx, uint64_t encodedIndex)
{
    auto **columns = pagesIndex.GetColumns()[colIdx];
    const uint32_t batch = uint32_t(encodedIndex >> 32);
    const uint32_t row = uint32_t(encodedIndex);
    auto *vec = columns[batch];
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return std::string(static_cast<ConstVector<std::string_view> *>(vec)->GetConstValue());
    }
    if (vec->IsNull(int32_t(row))) {
        return "NULL";
    }
    if (vec->GetEncoding() == OMNI_DICTIONARY) {
        return std::string(static_cast<Vector<DictionaryContainer<std::string_view>> *>(vec)->GetValue(int32_t(row)));
    }
    return std::string(
        static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec)->GetValue(int32_t(row)));
}

std::vector<std::string> ReadVarcharViaAddresses(PagesIndex &pagesIndex, int32_t colIdx, int32_t from, int32_t to)
{
    auto *addrs = pagesIndex.GetValueAddresses();
    std::vector<std::string> out;
    out.reserve(to - from);
    for (int32_t i = from; i < to; ++i) {
        out.push_back(ReadVarcharAtAddress(pagesIndex, colIdx, addrs[i]));
    }
    return out;
}

void FillVarcharSortBuffers(PagesIndex &pagesIndex, int32_t colIdx, int64_t *values, uint32_t *lens, int32_t from,
    int32_t to)
{
    auto *addrs = pagesIndex.GetValueAddresses();
    auto **columns = pagesIndex.GetColumns()[colIdx];
    for (int32_t i = from; i < to; ++i) {
        const uint32_t batch = uint32_t(addrs[i] >> 32);
        const uint32_t row = uint32_t(addrs[i]);
        auto *vec = columns[batch];
        std::string_view sv;
        if (vec->GetEncoding() == OMNI_DICTIONARY) {
            sv = static_cast<Vector<DictionaryContainer<std::string_view>> *>(vec)->GetValue(int32_t(row));
        } else {
            sv = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec)->GetValue(int32_t(row));
        }
        values[i] = reinterpret_cast<int64_t>(const_cast<char *>(sv.data()));
        lens[i] = uint32_t(sv.size());
    }
}

int32_t ExpectedAscRank(std::string_view s)
{
    if (s == "apple") {
        return 0;
    }
    if (s == "banana") {
        return 1;
    }
    return 2;  // pear
}

void RunDictVarcharPipeline(int32_t n, int32_t asc)
{
    DataTypes types({VarcharType(16)});
    BaseVector *keep = nullptr;
    std::vector<int32_t> ids(n);
    for (int i = 0; i < n; ++i) {
        ids[i] = i % 3;
    }
    PagesIndex pagesIndex(types, false, false, true);
    pagesIndex.AddVecBatch(MakeVarcharDictBatch({"pear", "banana", "apple"}, ids, &keep));
    pagesIndex.Prepare();

    std::vector<int64_t> values(n, 0);
    std::vector<uint32_t> lens(n, 0);
    FillVarcharSortBuffers(pagesIndex, 0, values.data(), lens.data(), 0, n);

    int32_t cols[] = {0}, ascA[] = {asc}, nf[] = {1};
    int32_t from = 0, to = n;
    tipi::SortWithTimSort(pagesIndex, values.data(), lens, cols, ascA, nf, 1, 0, from, to);

    auto got = ReadVarcharViaAddresses(pagesIndex, 0, 0, n);
    if (asc) {
        EXPECT_EQ(got.front(), "apple") << "n=" << n;
        EXPECT_EQ(got.back(), "pear") << "n=" << n;
        EXPECT_TRUE(std::is_sorted(got.begin(), got.end())) << "n=" << n;
    } else {
        EXPECT_EQ(got.front(), "pear") << "n=" << n;
        EXPECT_EQ(got.back(), "apple") << "n=" << n;
        EXPECT_TRUE(std::is_sorted(got.begin(), got.end(), std::greater<std::string>{})) << "n=" << n;
    }
    delete keep;
}

}  // namespace

TEST(SortingsCoveragePdqTest, test_types_thresholds_and_partitions)
{
    // empty / single
    {
        std::vector<int64_t> values;
        std::vector<uint64_t> addrs;
        pdqsort::pdqsort<int32_t, 1>(values.data(), addrs.data(), 0, 0);
        values = {42}, addrs = {7};
        pdqsort::pdqsort<int32_t, 1>(values.data(), addrs.data(), 0, 1);
        ASSERT_EQ(values[0], 42);
        ASSERT_EQ(addrs[0], 7u);
    }

    // insertion threshold (<24), int8/int16
    {
        std::vector<int32_t> raw = {9, 1, 8, 2, 7, 3, 6, 4, 5, 0, 11, 10};
        auto values = std::vector<int64_t>(raw.begin(), raw.end());
        auto addrs = IdentityAddrs(values.size());
        AssertPdqSorted<int32_t, 1>(values, addrs);
        values = std::vector<int64_t>(raw.begin(), raw.end());
        addrs = IdentityAddrs(values.size());
        AssertPdqSorted<int32_t, 0>(values, addrs);

        values = {5, -1, 127, -128, 0, 42}, addrs = IdentityAddrs(values.size());
        AssertPdqSorted<int8_t, 1>(values, addrs);
        values = {500, -200, 0, 1, -1, 32000}, addrs = IdentityAddrs(values.size());
        AssertPdqSorted<int16_t, 1>(values, addrs);
    }

    // medium / ninther sizes
    {
        std::mt19937 rng(42);
        for (int n : {32, 64, 127, 128, 200, 512}) {
            std::vector<int64_t> values(n);
            for (int i = 0; i < n; ++i) {
                values[i] = int64_t(rng() % 1000);
            }
            auto addrs = IdentityAddrs(values.size());
            AssertPdqSorted<int64_t, 1>(values, addrs);
            for (int i = 0; i < n; ++i) {
                values[i] = int64_t(rng() % 1000);
            }
            addrs = IdentityAddrs(values.size());
            AssertPdqSorted<int64_t, 0>(values, addrs);
        }
    }

    // sorted / reverse / duplicates / partial insertion
    {
        constexpr int n = 300;
        std::vector<int64_t> values(n);
        std::iota(values.begin(), values.end(), 0);
        auto addrs = IdentityAddrs(n);
        AssertPdqSorted<int64_t, 1>(values, addrs);
        std::reverse(values.begin(), values.end());
        addrs = IdentityAddrs(n);
        AssertPdqSorted<int64_t, 1>(values, addrs);
        std::iota(values.begin(), values.end(), 0);
        addrs = IdentityAddrs(n);
        AssertPdqSorted<int64_t, 0>(values, addrs);

        values.assign(256, 7);
        for (int i = 0; i < 32; ++i) {
            values[i * 8] = i;
        }
        addrs = IdentityAddrs(values.size());
        AssertPdqSorted<int32_t, 1>(values, addrs);

        std::vector<int64_t> almost(256);
        std::iota(almost.begin(), almost.end(), 0);
        std::swap(almost[10], almost[11]);
        std::swap(almost[100], almost[102]);
        addrs = IdentityAddrs(almost.size());
        pdqsort::pdqsort<int64_t, 1>(almost.data(), addrs.data(), 0, int32_t(almost.size()));
        ASSERT_TRUE(std::is_sorted(almost.begin(), almost.end()));
    }

    // float/double epsilon + non-branchless partition paths
    {
        float fa = 1.0f, fb = 1.0f + 0.5f * __FLT_EPSILON__;
        uint32_t ua = 0, ub = 0;
        std::memcpy(&ua, &fa, sizeof(float));
        std::memcpy(&ub, &fb, sizeof(float));
        std::vector<int64_t> values = {int64_t(ub), int64_t(ua),
            int64_t(ub), int64_t(ua)};
        auto addrs = IdentityAddrs(values.size());
        AssertPdqSorted<float, 1>(values, addrs);

        double da = 1.0, db = 1.0 + 0.5 * __DBL_EPSILON__;
        int64_t dva = 0, dvb = 0;
        std::memcpy(&dva, &da, sizeof(double));
        std::memcpy(&dvb, &db, sizeof(double));
        values = {dvb, dva, dvb, dva, 0};
        double d0 = -10.0;
        std::memcpy(&values[4], &d0, sizeof(double));
        addrs = IdentityAddrs(values.size());
        AssertPdqSorted<double, 1>(values, addrs);
        values = {dva, dvb}, addrs = IdentityAddrs(values.size());
        AssertPdqSorted<double, 0>(values, addrs);

        constexpr int fn = 400;
        values.resize(fn);
        for (int i = 0; i < fn; ++i) {
            float f = float(fn - i);
            uint32_t bits = 0;
            std::memcpy(&bits, &f, sizeof(float));
            values[i] = bits;
        }
        addrs = IdentityAddrs(fn);
        pdqsort::pdqsort<float, 1>(values.data(), addrs.data(), 0, fn);

        constexpr int dn = 500;
        values.resize(dn);
        for (int i = 0; i < dn; ++i) {
            double d = double(i % 7) + (i % 2 ? 0.1 : -0.1);
            std::memcpy(&values[i], &d, sizeof(double));
        }
        addrs = IdentityAddrs(dn);
        pdqsort::pdqsort<double, 0>(values.data(), addrs.data(), 0, dn);

        float a = 1.0f, b = 2.0f;
        uint32_t uaa = 0, ubb = 0;
        std::memcpy(&uaa, &a, sizeof(float));
        std::memcpy(&ubb, &b, sizeof(float));
        values = {int64_t(uaa), int64_t(ubb), int64_t(uaa)}, addrs = IdentityAddrs(3);
        pdqsort::pdqsort<float, 0>(values.data(), addrs.data(), 0, 3);
    }

    // decimal128
    {
        std::vector<Decimal128> storage = {
            Decimal128(10), Decimal128(1), Decimal128(5), Decimal128(-3), Decimal128(100), Decimal128(0)};
        std::vector<int64_t> values(storage.size());
        for (size_t i = 0; i < storage.size(); ++i) {
            values[i] = reinterpret_cast<int64_t>(&storage[i]);
        }
        auto addrs = IdentityAddrs(values.size());
        AssertPdqSorted<Decimal128, 1>(values, addrs);
        for (size_t i = 0; i < storage.size(); ++i) {
            values[i] = reinterpret_cast<int64_t>(&storage[i]);
        }
        addrs = IdentityAddrs(values.size());
        AssertPdqSorted<Decimal128, 0>(values, addrs);

        storage.clear();
        storage.reserve(200);
        for (int i = 0; i < 200; ++i) {
            storage.emplace_back(i % 50);
        }
        values.resize(storage.size());
        for (size_t i = 0; i < storage.size(); ++i) {
            values[i] = reinterpret_cast<int64_t>(&storage[i]);
        }
        addrs = IdentityAddrs(values.size());
        pdqsort::pdqsort<Decimal128, 1>(values.data(), addrs.data(), 0, int32_t(values.size()));
    }

    // ninther / unbalanced / heap fallback via public API
    {
        constexpr int n = 2048;
        std::vector<int64_t> values(n, 0);
        const int s2 = n / 2;
        for (int idx : {0, 1, 2, s2 - 1, s2, s2 + 1, n - 3, n - 2, n - 1}) {
            values[idx] = 1'000'000 + idx;
        }
        auto addrs = IdentityAddrs(n);
        pdqsort::pdqsort<int64_t, 1>(values.data(), addrs.data(), 0, n);
        ASSERT_TRUE(std::is_sorted(values.begin(), values.end()));

        values.assign(1024, 1);
        for (int i = 0; i < 8; ++i) {
            values[1024 - 1 - i] = 1000 + i;
        }
        addrs = IdentityAddrs(1024);
        pdqsort::pdqsort<int64_t, 1>(values.data(), addrs.data(), 0, 1024);
        ASSERT_TRUE(std::is_sorted(values.begin(), values.end()));
    }

    // float descending (non-branchless partition)
    {
        constexpr int n = 300;
        std::vector<int64_t> values(n, 0);
        for (int i = 0; i < n; ++i) {
            float f = (i < n - 4) ? 1.0f : float(10 + i);
            uint32_t bits = 0;
            std::memcpy(&bits, &f, sizeof(float));
            values[i] = bits;
        }
        auto addrs = IdentityAddrs(n);
        pdqsort::pdqsort<float, 0>(values.data(), addrs.data(), 0, n);
        for (int i = 1; i < n; ++i) {
            float a = 0, b = 0;
            uint32_t ua = uint32_t(values[i - 1]), ub = uint32_t(values[i]);
            std::memcpy(&a, &ua, sizeof(float));
            std::memcpy(&b, &ub, sizeof(float));
            ASSERT_GE(a, b) << "i=" << i;
        }
    }
}

TEST(SortingsCoverageTimSortTest, test_runs_merge_gallop_and_stack)
{
    // basic stable / reverse / mixed / empty / timmerge
    {
        std::vector<std::pair<int, int>> items = {{2, 0}, {1, 1}, {2, 2}, {1, 3}, {3, 4}, {2, 5}};
        timsort::timsort(items.begin(), items.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
        ASSERT_EQ(items[0].second, 1);
        ASSERT_EQ(items[2].second, 0);
        ASSERT_EQ(items[3].second, 2);

        std::vector<int> desc = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
        timsort::timsort(desc);
        ASSERT_TRUE(std::is_sorted(desc.begin(), desc.end()));

        std::vector<int> mixed;
        mixed.reserve(200);
        for (int i = 0; i < 50; ++i) {
            mixed.push_back(i);
        }
        for (int i = 100; i >= 50; --i) {
            mixed.push_back(i);
        }
        for (int i = 101; i < 200; ++i) {
            mixed.push_back(i % 17);
        }
        timsort::timsort(mixed.begin(), mixed.end());
        ASSERT_TRUE(std::is_sorted(mixed.begin(), mixed.end()));

        std::vector<int> a = {1, 3, 5, 0, 2, 4};
        timsort::timmerge(a.begin(), a.begin() + 3, a.end());
        ASSERT_TRUE(std::is_sorted(a.begin(), a.end()));

        std::vector<int> empty;
        timsort::timsort(empty.begin(), empty.end());
        ASSERT_TRUE(empty.empty());

        std::mt19937 rng(7);
        std::vector<int> data(1000);
        for (auto &v : data) {
            v = int(rng() % 500);
        }
        timsort::timsort(data);
        ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));
    }

    // gallop-heavy merges
    {
        std::vector<int> data;
        for (int i = 0; i < 200; ++i) {
            data.push_back(i);
        }
        for (int i = 400; i >= 150; --i) {
            data.push_back(i);
        }
        for (int i = 0; i < 300; ++i) {
            data.push_back((i * 13) % 97);
        }
        timsort::timsort(data.begin(), data.end(), std::less<int>{});
        ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));

        std::vector<int> left = {1, 3, 5, 7, 9, 0, 2, 4, 6, 8};
        timsort::timsort(left.begin(), left.end(), std::greater<int>{});
        ASSERT_TRUE(std::is_sorted(left.begin(), left.end(), std::greater<int>{}));
    }

    // rotate / single-element runs / tiny / descending blocks
    {
        std::vector<int> data;
        for (int i = 0; i < 64; ++i) {
            data.push_back((i % 2 == 0) ? i : -i);
        }
        timsort::timsort(data);
        ASSERT_TRUE(std::is_sorted(data.begin(), data.end()));

        std::vector<int> halves = {100};
        for (int i = 0; i < 40; ++i) {
            halves.push_back(i);
        }
        timsort::timmerge(halves.begin(), halves.begin() + 1, halves.end());
        ASSERT_TRUE(std::is_sorted(halves.begin(), halves.end()));

        std::vector<int> halves2;
        for (int i = 0; i < 40; ++i) {
            halves2.push_back(i);
        }
        halves2.push_back(-1);
        timsort::timmerge(halves2.begin(), halves2.end() - 1, halves2.end());
        ASSERT_TRUE(std::is_sorted(halves2.begin(), halves2.end()));

        std::vector<int> tiny = {3, 1, 2};
        timsort::timsort(tiny);
        ASSERT_EQ(tiny, (std::vector<int>{1, 2, 3}));

        std::vector<int> desc;
        for (int block = 0; block < 10; ++block) {
            for (int i = 30; i >= 0; --i) {
                desc.push_back(block * 100 + i);
            }
        }
        timsort::timsort(desc.begin(), desc.end(), std::greater<int>{});
    }

    // merge early-return / trailing run / stack collapse / gallop exits
    {
        std::vector<int> v = {1, 2, 3, 4};
        timsort::timmerge(v.begin(), v.begin(), v.end());
        timsort::timmerge(v.begin(), v.end(), v.end());
        ASSERT_EQ(v, (std::vector<int>{1, 2, 3, 4}));

        v.clear();
        for (int i = 0; i < 65; ++i) {
            v.push_back((i * 7) % 65);
        }
        timsort::timsort(v);
        ASSERT_TRUE(std::is_sorted(v.begin(), v.end()));

        v.clear();
        for (int run = 0; run < 40; ++run) {
            const int base = run * 50, len = 16 + (run % 5);
            for (int i = 0; i < len; ++i) {
                v.push_back(base + i);
            }
        }
        for (size_t i = 1; i + 1 < v.size(); i += 17) {
            std::swap(v[i], v[i + 1]);
        }
        timsort::timsort(v);
        ASSERT_TRUE(std::is_sorted(v.begin(), v.end()));

        auto mergeHalves = [](std::vector<int> &left, std::vector<int> &right) {
            std::vector<int> merged;
            merged.insert(merged.end(), left.begin(), left.end());
            merged.insert(merged.end(), right.begin(), right.end());
            timsort::timmerge(merged.begin(), merged.begin() + int64_t(left.size()), merged.end());
            return merged;
        };
        {
            std::vector<int> left;
            std::vector<int> right;
            for (int i = 0; i < 200; ++i) {
                left.push_back(i * 2);
            }
            for (int i = 0; i < 200; ++i) {
                right.push_back(i * 2 + 1);
            }
            auto merged = mergeHalves(left, right);
            ASSERT_TRUE(std::is_sorted(merged.begin(), merged.end()));
        }
        {
            std::vector<int> left;
            std::vector<int> right;
            for (int i = 0; i < 80; ++i) {
                left.push_back(1000 + i);
            }
            for (int i = 0; i < 80; ++i) {
                right.push_back(i);
            }
            auto merged = mergeHalves(left, right);
            ASSERT_TRUE(std::is_sorted(merged.begin(), merged.end()));
        }
        {
            std::vector<int> left;
            std::vector<int> right;
            for (int i = 0; i < 80; ++i) {
                left.push_back(i);
            }
            for (int i = 0; i < 80; ++i) {
                right.push_back(1000 + i);
            }
            auto merged = mergeHalves(left, right);
            ASSERT_TRUE(std::is_sorted(merged.begin(), merged.end()));
        }
    }

    // stack collapse motifs (40/50/80 descending runs) + greater<> + asymmetric gallop
    {
        auto appendDescRun = [](std::vector<int> &v, int base, int len) {
            for (int i = len - 1; i >= 0; --i) {
                v.push_back(base + i);
            }
        };
        std::vector<int> v;
        for (int rep = 0; rep < 6; ++rep) {
            appendDescRun(v, rep * 10000 + 0, 40);
            appendDescRun(v, rep * 10000 + 1000, 50);
            appendDescRun(v, rep * 10000 + 2000, 80);
        }
        timsort::timsort(v);
        ASSERT_TRUE(std::is_sorted(v.begin(), v.end()));

        v.clear();
        for (int rep = 0; rep < 4; ++rep) {
            for (int i = 0; i < 40; ++i) {
                v.push_back(rep * 10000 + i);
            }
            for (int i = 0; i < 50; ++i) {
                v.push_back(rep * 10000 + 1000 + i);
            }
            for (int i = 0; i < 80; ++i) {
                v.push_back(rep * 10000 + 2000 + i);
            }
        }
        timsort::timsort(v.begin(), v.end(), std::greater<int>{});
        ASSERT_TRUE(std::is_sorted(v.begin(), v.end(), std::greater<int>{}));

        v.clear();
        for (int i = 0; i < 100; ++i) {
            v.push_back(10 + i);
        }
        for (int i = 0; i < 5; ++i) {
            v.push_back(i);
        }
        timsort::timmerge(v.begin(), v.begin() + 100, v.end());

        v.clear();
        for (int i = 0; i < 150; ++i) {
            v.push_back(i);
        }
        for (int i = 0; i < 4; ++i) {
            v.push_back(1000 + i);
        }
        timsort::timmerge(v.begin(), v.begin() + 150, v.end());

        v.clear();
        for (int i = 31; i >= 0; --i) {
            v.push_back(1000 + i);
        }
        v.push_back(0);
        timsort::timsort(v);
        ASSERT_TRUE(std::is_sorted(v.begin(), v.end()));
    }
}

TEST(SortingsCoverageVarcharPolicyTest, test_insertion_timsort_dict_and_simd)
{
    // length edges via TimSort
    {
        const std::vector<size_t> lengths = {0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 64};
        std::vector<std::string> data;
        for (size_t len : lengths) {
            data.push_back(MakeLen('a', len));
            if (len > 0) {
                auto other = MakeLen('a', len);
                other.back() = 'b';
                data.push_back(std::move(other));
            }
        }
        DataTypes types({VarcharType(128)});
        PagesIndex pagesIndex(types, false, false, true);
        pagesIndex.AddVecBatch(MakeVarcharBatch(data, {}));
        pagesIndex.Prepare();
        const int32_t n = int32_t(pagesIndex.GetRowCount());
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        pagesIndex.Sort(cols, asc, nf, 1, 0, n);
        auto got = ReadVarcharSorted(pagesIndex, 0);
        ASSERT_TRUE(std::is_sorted(got.begin(), got.end()));
    }

    // InsertionSortVarChar + TimSortVarChar asc/desc + equal long strings
    {
        std::vector<std::string> storage = {
            "apple", "banana", "apricot", "a", "", "zzzzzzzz", "zzzzzzzzy", MakeLen('x', 20),
            MakeLen('x', 20) + "y", MakeLen('m', 7), MakeLen('m', 8), MakeLen('m', 9),
            MakeLen('x', 20), MakeLen('x', 20), MakeLen('x', 24), MakeLen('y', 20), MakeLen('x', 16), "",
            MakeLen('p', 30), MakeLen('p', 30) + "z"};
        std::vector<int64_t> values(storage.size());
        std::vector<uint32_t> lens(storage.size());
        std::vector<uint64_t> addrs(storage.size());
        for (size_t i = 0; i < storage.size(); ++i) {
            values[i] = reinterpret_cast<int64_t>(storage[i].data());
            lens[i] = uint32_t(storage[i].size());
            addrs[i] = i;
        }
        tipi::InsertionSortVarChar<1>(values.data(), lens.data(), addrs.data(), 0,
            int32_t(storage.size()));
        AssertAddressesSortedByString(values.data(), lens.data(), 0,
            int32_t(storage.size()), true);
        for (size_t i = 0; i < storage.size(); ++i) {
            addrs[i] = i;
        }
        tipi::InsertionSortVarChar<0>(values.data(), lens.data(), addrs.data(), 0,
            int32_t(storage.size()));
        AssertAddressesSortedByString(values.data(), lens.data(), 0,
            int32_t(storage.size()), false);

        storage.clear();
        for (int i = 0; i < 160; ++i) {
            storage.push_back("s" + std::to_string(160 - i) + MakeLen('p', size_t(i % 24)));
        }
        values.resize(storage.size());
        lens.resize(storage.size());
        addrs.resize(storage.size());
        for (size_t i = 0; i < storage.size(); ++i) {
            values[i] = reinterpret_cast<int64_t>(storage[i].data());
            lens[i] = uint32_t(storage[i].size());
            addrs[i] = i;
        }
        tipi::TimSortVarChar<1>(values.data(), lens.data(), addrs.data(), 0, int32_t(storage.size()));
        AssertAddressesSortedByString(values.data(), lens.data(), 0,
            int32_t(storage.size()), true);
        for (size_t i = 0; i < storage.size(); ++i) {
            addrs[i] = i;
        }
        tipi::TimSortVarChar<0>(values.data(), lens.data(), addrs.data(), 0, int32_t(storage.size()));
        AssertAddressesSortedByString(values.data(), lens.data(), 0,
            int32_t(storage.size()), false);

        tipi::TimSortVarChar<1>(values.data(), lens.data(), addrs.data(), 0, 1);
        tipi::InsertionSortVarChar<1>(values.data(), lens.data(), addrs.data(), 0, 1);
        ASSERT_EQ((tipi::CompareVarChar<1>(values[0], lens[0], values[0], lens[0])), 0);
        ASSERT_EQ((tipi::CompareVarChar<0>(values[0], lens[0], values[0], lens[0])), 0);

        std::string s1 = "abcdefgh", s2 = "abcdefghij", longA(20, 'p'), longB = longA + "Z";
        storage = {s2, s1, s2, s1, longB, longA};
        values.resize(storage.size());
        lens.resize(storage.size());
        addrs.resize(storage.size());
        for (size_t i = 0; i < storage.size(); ++i) {
            values[i] = reinterpret_cast<int64_t>(storage[i].data());
            lens[i] = uint32_t(storage[i].size());
            addrs[i] = i;
        }
        tipi::TimSortVarChar<1>(values.data(), lens.data(), addrs.data(), 0, 4);
        tipi::TimSortVarChar<0>(values.data(), lens.data(), addrs.data(), 0, 4);
        tipi::TimSortVarChar<1>(values.data(), lens.data(), addrs.data(), 4, 6);
        tipi::TimSortVarChar<0>(values.data(), lens.data(), addrs.data(), 4, 6);
    }

    // CountingSortDictRanks + SortNullAndGetDictRanks variants
    {
        std::vector<int32_t> ranks = {2, 0, 1, 2, 0, 1};
        std::vector<uint64_t> addrs = {10, 11, 12, 13, 14, 15};
        tipi::CountingSortDictRanks<1>(ranks.data(), addrs.data(), 0, 6, 2);
        ASSERT_EQ(addrs[0], 11u);
        ASSERT_EQ(addrs[1], 14u);
        ranks = {2, 0, 1, 2, 0, 1}, addrs = {10, 11, 12, 13, 14, 15};
        tipi::CountingSortDictRanks<0>(ranks.data(), addrs.data(), 0, 6, 2);
        ASSERT_EQ(addrs[0], 10u);
        tipi::CountingSortDictRanks<1>(ranks.data(), addrs.data(), 0, 1, 0);

        BaseVector *cols[1] = {nullptr};
        int32_t rankBuf[1] = {0};
        uint64_t addrBuf[1] = {0};
        int32_t from = 0, to = 0, maxRank = -1;
        tipi::SortNullAndGetDictRanks<false, true, true>(cols, rankBuf, addrBuf, from, to, maxRank);
        ASSERT_EQ(maxRank, 0);

        DataTypes types({VarcharType(8)});
        BaseVector *keep = nullptr;
        std::vector<int32_t> ids(130);
        for (int i = 0; i < 130; ++i) {
            ids[i] = i % 3;
        }
        std::vector<bool> isNull(130, false);
        isNull[0] = true;
        isNull[50] = true;
        PagesIndex pagesIndex(types);
        pagesIndex.AddVecBatch(MakeVarcharDictBatch({"c", "a", "b"}, ids, &keep, isNull));
        pagesIndex.Prepare();
        auto **col = pagesIndex.GetColumns()[0];
        auto *piAddrs = pagesIndex.GetValueAddresses();
        std::vector<int32_t> dictRanks(130, 0);
        from = 0, to = 130, maxRank = 0;
        tipi::SortNullAndGetDictRanks<true, false, false>(col, dictRanks.data(), piAddrs, from, to, maxRank);
        ASSERT_GE(maxRank, 0);
        delete keep;

        PagesIndex flatPi(types);
        flatPi.AddVecBatch(MakeVarcharBatch({"c", "a", "b", "a", "c"}, {false, false, true, false, false}));
        flatPi.Prepare();
        col = flatPi.GetColumns()[0], piAddrs = flatPi.GetValueAddresses();
        std::vector<int32_t> flatRanks(5, 0);
        maxRank = 0;
        from = 0, to = 5;
        tipi::SortNullAndGetDictRanks<true, true, false>(col, flatRanks.data(), piAddrs, from, to, maxRank);
        tipi::SortNullAndGetDictRanks<true, false, true>(col, flatRanks.data(), piAddrs, from, to, maxRank);

        static std::string kConst = "zzz";
        PagesIndex constPi(types);
        auto *cvb = new VectorBatch(40);
        cvb->Append(new ConstVector<std::string_view>(std::string_view(kConst), OMNI_VARCHAR, 40));
        constPi.AddVecBatch(cvb);
        constPi.Prepare();
        col = constPi.GetColumns()[0], piAddrs = constPi.GetValueAddresses();
        std::vector<int32_t> constRanks(40, 0);
        maxRank = 0;
        from = 0, to = 40;
        tipi::SortNullAndGetDictRanks<false, true, true>(col, constRanks.data(), piAddrs, from, to, maxRank);
        ASSERT_GE(maxRank, 0);
    }

    // SIMD diff via TimSort (deep byte difference)
    {
        auto a = MakeLen('a', 32), b = a;
        b[20] = 'b';
        auto c = MakeLen('q', 8), d = c;
        d[7] = 'z';
        DataTypes types({VarcharType(64)});
        PagesIndex pagesIndex(types, false, false, true);
        pagesIndex.AddVecBatch(MakeVarcharBatch({a, b, c, d, "abcde", "abxde"}, {}));
        pagesIndex.Prepare();
        const int32_t n = int32_t(pagesIndex.GetRowCount());
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        pagesIndex.Sort(cols, asc, nf, 1, 0, n);
        auto got = ReadVarcharSorted(pagesIndex, 0);
        ASSERT_TRUE(std::is_sorted(got.begin(), got.end()));
    }
}

TEST(SortingsCoverageTipiTest, test_flag_matrices_and_postprocess)
{
    DataTypes types({VarcharType(16)});

    // flat: all asc/nullFirst combos, small + large
    for (int asc : {0, 1}) {
        for (int nf : {0, 1}) {
            PagesIndex small(types, false, false, true);
            small.AddVecBatch(MakeVarcharBatch({"d", "b", "c", "a"}, {}));
            small.Prepare();
            RunTipi(small, 1, asc, nf);

            PagesIndex large(types, false, false, true);
            std::vector<std::string> data;
            for (int i = 0; i < 140; ++i) {
                data.push_back(MakeLen('a' + (i % 26), 4) + std::to_string(i));
            }
            large.AddVecBatch(MakeVarcharBatch(data, {}));
            large.Prepare();
            RunTipi(large, 1, asc, nf);

            PagesIndex withNulls(types, false, false, true);
            withNulls.AddVecBatch(MakeVarcharBatch({"c", "a", "b", "x"}, {false, true, false, true}));
            withNulls.Prepare();
            RunTipi(withNulls, 1, asc, nf);
        }
    }

    // dictionary large: all asc/nullFirst, with/without nulls
    {
        const std::vector<std::string> dictVals = {"delta", "alpha", "charlie", "bravo"};
        auto runDict = [&](int asc, int nf, bool withNulls) {
            BaseVector *keep = nullptr;
            std::vector<int32_t> ids(160);
            for (int i = 0; i < 160; ++i) {
                ids[i] = i % 4;
            }
            PagesIndex pagesIndex(types, false, false, true);
            if (withNulls) {
                std::vector<bool> isNull(160, false);
                for (int i = 0; i < 160; i += 17) {
                    isNull[i] = true;
                }
                pagesIndex.AddVecBatch(MakeVarcharDictBatch(dictVals, ids, &keep, isNull));
            } else {
                pagesIndex.AddVecBatch(MakeVarcharDictBatch(dictVals, ids, &keep));
            }
            pagesIndex.Prepare();
            RunTipi(pagesIndex, 1, asc, nf);
            delete keep;
        };
        for (int asc : {0, 1}) {
            for (int nf : {0, 1}) {
                runDict(asc, nf, false);
                runDict(asc, nf, true);
            }
        }
    }

    // small dict fallback (<128)
    {
        DataTypes smallTypes({VarcharType(8)});
        BaseVector *keep = nullptr;
        std::vector<int32_t> ids(40);
        for (int i = 0; i < 40; ++i) {
            ids[i] = i % 3;
        }
        PagesIndex pagesIndex(smallTypes, false, false, true);
        pagesIndex.AddVecBatch(MakeVarcharDictBatch({"c", "a", "b"}, ids, &keep));
        pagesIndex.Prepare();
        RunTipi(pagesIndex, 1, 1, 1);
        RunTipi(pagesIndex, 1, 0, 0);
        delete keep;
    }

    // small dict (<128) with null/asc matrix via shared dictBase
    {
        DataTypes smallTypes({VarcharType(8)});
        auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(2);
        dictBase->SetValue(0, std::string_view("b"));
        dictBase->SetValue(1, std::string_view("a"));
        for (int withNulls : {0, 1}) {
            for (int asc : {0, 1}) {
                for (int nf : {0, 1}) {
                    PagesIndex pi(smallTypes, false, false, true);
                    std::vector<int32_t> ids(60);
                    for (int i = 0; i < 60; ++i) {
                        ids[i] = i % 2;
                    }
                    auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), 60, dictBase);
                    if (withNulls) {
                        dictVec->SetNull(3);
                        dictVec->SetNull(17);
                    }
                    auto *vb = new VectorBatch(60);
                    vb->Append(dictVec);
                    pi.AddVecBatch(vb);
                    pi.Prepare();
                    RunTipi(pi, 1, asc, nf);
                }
            }
        }
        delete dictBase;
    }

    // multi-column dictionary postprocess
    {
        DataTypes mcTypes({VarcharType(16), IntType()});
        PagesIndex pagesIndex(mcTypes, false, false, true);
        const int32_t n = 160;
        BaseVector *keep = nullptr;
        std::vector<std::string> dictVals = {"zz", "aa", "mm", "bb"};
        std::vector<int32_t> ids(n);
        for (int i = 0; i < n; ++i) {
            ids[i] = i % 4;
        }
        auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(4);
        for (int i = 0; i < 4; ++i) {
            dictBase->SetValue(i, std::string_view(dictVals[i]));
        }
        keep = dictBase;
        auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), n, dictBase);
        auto *intVec = new Vector<int32_t>(n);
        for (int i = 0; i < n; ++i) {
            intVec->SetValue(i, n - i);
        }
        auto *vb = new VectorBatch(n);
        vb->Append(dictVec);
        vb->Append(intVec);
        pagesIndex.AddVecBatch(vb);
        pagesIndex.Prepare();
        RunTipi(pagesIndex, 2, 1, 1);
        delete keep;
    }

    // postprocess: dict + const + flat multi-batch
    {
        DataTypes mcTypes({VarcharType(16), IntType()});
        PagesIndex pagesIndex(mcTypes, false, false, true);
        const int32_t n1 = 80;
        auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(3);
        dictBase->SetValue(0, std::string_view("cc"));
        dictBase->SetValue(1, std::string_view("aa"));
        dictBase->SetValue(2, std::string_view("bb"));
        std::vector<int32_t> ids(n1);
        for (int i = 0; i < n1; ++i) {
            ids[i] = i % 3;
        }
        {
            auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), n1, dictBase);
            auto *int1 = new Vector<int32_t>(n1);
            for (int i = 0; i < n1; ++i) {
                int1->SetValue(i, i);
            }
            auto *vb1 = new VectorBatch(n1);
            vb1->Append(dictVec);
            vb1->Append(int1);
            pagesIndex.AddVecBatch(vb1);
        }
        static std::string kConst = "aa";
        {
            const int32_t n2 = 80;
            auto *vb2 = new VectorBatch(n2);
            vb2->Append(new ConstVector<std::string_view>(std::string_view(kConst), OMNI_VARCHAR, n2));
            auto *int2 = new Vector<int32_t>(n2);
            for (int i = 0; i < n2; ++i) {
                int2->SetValue(i, i);
            }
            vb2->Append(int2);
            pagesIndex.AddVecBatch(vb2);
        }
        pagesIndex.Prepare();
        ASSERT_GE(pagesIndex.GetRowCount(), 160u);
        RunTipi(pagesIndex, 2, 1, 1);

        // two dict batches + null column slot + flat batch
        PagesIndex pagesIndex2(mcTypes, false, false, true);
        auto *dictBase2 = new Vector<LargeStringContainer<std::string_view>>(2);
        dictBase2->SetValue(0, std::string_view("bb"));
        dictBase2->SetValue(1, std::string_view("aa"));
        const int32_t nDict = 100;
        std::vector<int32_t> ids2(nDict);
        for (int i = 0; i < nDict; ++i) {
            ids2[i] = i % 2;
        }
        {
            auto *vb = new VectorBatch(nDict);
            vb->Append(VectorHelper::CreateStringDictionary(ids2.data(), nDict, dictBase2));
            auto *iv = new Vector<int32_t>(nDict);
            for (int i = 0; i < nDict; ++i) {
                iv->SetValue(i, i);
            }
            vb->Append(iv);
            pagesIndex2.AddVecBatch(vb);
        }
        {
            const int32_t n = 50;
            auto *vb = new VectorBatch(n);
            vb->Append(new ConstVector<std::string_view>(std::string_view(kConst), OMNI_VARCHAR, n));
            auto *iv = new Vector<int32_t>(n);
            for (int i = 0; i < n; ++i) {
                iv->SetValue(i, i);
            }
            vb->Append(iv);
            pagesIndex2.AddVecBatch(vb);
        }
        {
            const int32_t n = 50;
            auto *vb = new VectorBatch(n);
            auto *sv = new Vector<LargeStringContainer<std::string_view>>(n);
            for (int i = 0; i < n; ++i) {
                sv->SetValue(i, (i % 2 == 0) ? std::string_view("aa") : std::string_view("bb"));
            }
            vb->Append(sv);
            auto *iv = new Vector<int32_t>(n);
            for (int i = 0; i < n; ++i) {
                iv->SetValue(i, i);
            }
            vb->Append(iv);
            pagesIndex2.AddVecBatch(vb);
        }
        pagesIndex2.Prepare();
        RunTipi(pagesIndex2, 2, 1, 1);

        BaseVector *colsArr[2] = {nullptr, pagesIndex2.GetColumns()[0][0]};
        std::vector<int32_t> ranks(8, 0);
        std::vector<uint64_t> addrs(8);
        for (int i = 0; i < 8; ++i) {
            addrs[i] = (uint64_t(1) << 32) | uint32_t(i % 50);
        }
        int32_t from = 0, to = 8, maxRank = 0;
        tipi::SortNullAndGetDictRanks<false, true, true>(
            colsArr, ranks.data(), addrs.data(), from, to, maxRank);

        delete dictBase;
        delete dictBase2;
    }

    // two dict batches postprocess (desc, nulls last)
    {
        DataTypes mcTypes({VarcharType(8), IntType()});
        PagesIndex pi(mcTypes, false, false, true);
        auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(2);
        dictBase->SetValue(0, std::string_view("z"));
        dictBase->SetValue(1, std::string_view("a"));
        for (int b = 0; b < 2; ++b) {
            const int32_t n = 90;
            std::vector<int32_t> ids(n);
            for (int i = 0; i < n; ++i) {
                ids[i] = (i + b) % 2;
            }
            auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), n, dictBase);
            auto *iv = new Vector<int32_t>(n);
            for (int i = 0; i < n; ++i) {
                iv->SetValue(i, i);
            }
            auto *vb = new VectorBatch(n);
            vb->Append(dictVec);
            vb->Append(iv);
            pi.AddVecBatch(vb);
        }
        pi.Prepare();
        RunTipi(pi, 2, 0, 0);
        delete dictBase;
    }
}

TEST(SortingsCoveragePagesIndexTest, test_varchar_primitives_and_multibatch)
{
    // direct tipi + nulls last desc
    {
        DataTypes types({VarcharType(16)});
        PagesIndex pagesIndex(types, false, false, true);
        pagesIndex.AddVecBatch(MakeVarcharBatch({"zz", "aa", "mm", "bb"}, {}));
        pagesIndex.Prepare();
        RunTipi(pagesIndex, 1, 1, 1);

        PagesIndex pagesIndex2(types, false, false, true);
        pagesIndex2.AddVecBatch(MakeVarcharBatch({"c", "a", "b", "x"}, {false, false, false, true}));
        pagesIndex2.Prepare();
        RunTipi(pagesIndex2, 1, 0, 0);
    }

    // varchar nulls asc/desc via PagesIndex::Sort
    {
        DataTypes types({VarcharType(32)});
        PagesIndex pagesIndex(types);
        pagesIndex.AddVecBatch(MakeVarcharBatch({"world", "x", "omni", "apple", "y"},
            {false, true, false, false, true}));
        pagesIndex.Prepare();
        int32_t sortCols[] = {0};
        int32_t sortAsc[] = {1};
        int32_t sortNullsFirst[] = {1};
        pagesIndex.Sort(sortCols, sortAsc, sortNullsFirst, 1, 0, int32_t(pagesIndex.GetRowCount()));
        auto got = ReadVarcharSorted(pagesIndex, 0);
        ASSERT_EQ(got[0], "NULL");
        ASSERT_EQ(got[2], "apple");

        PagesIndex pagesIndex2(types);
        pagesIndex2.AddVecBatch(MakeVarcharBatch({"b", "a", "c"}, {}));
        pagesIndex2.Prepare();
        int32_t sortDesc[] = {0};
        int32_t nullsLast[] = {0};
        pagesIndex2.Sort(sortCols, sortDesc, nullsLast, 1, 0, 3);
        got = ReadVarcharSorted(pagesIndex2, 0);
        ASSERT_EQ(got[0], "c");
        ASSERT_EQ(got[2], "a");
    }

    // small insertion + large timsort varchar paths
    {
        DataTypes types({VarcharType(64)});
        PagesIndex small(types, false, false, true);
        std::vector<std::string> data;
        for (int i = 0; i < 40; ++i) {
            data.push_back("v" + std::to_string(40 - i));
        }
        small.AddVecBatch(MakeVarcharBatch(data, {}));
        small.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        small.Sort(cols, asc, nf, 1, 0, 40);
        auto got = ReadVarcharSorted(small, 0);
        ASSERT_TRUE(std::is_sorted(got.begin(), got.end()));

        PagesIndex large(types, false, false, true);
        data.clear();
        for (int i = 0; i < 200; ++i) {
            data.push_back(MakeLen('a' + (i % 26), 8 + (i % 5)) + std::to_string(i));
        }
        large.AddVecBatch(MakeVarcharBatch(data, {}));
        large.Prepare();
        large.Sort(cols, asc, nf, 1, 0, 200);
        got = ReadVarcharSorted(large, 0);
        ASSERT_TRUE(std::is_sorted(got.begin(), got.end()));
    }

    // dictionary large via PagesIndex::Sort + tipi
    {
        DataTypes types({VarcharType(16)});
        const int32_t n = 160;
        BaseVector *keep = nullptr;
        std::vector<int32_t> ids(n);
        for (int i = 0; i < n; ++i) {
            ids[i] = i % 4;
        }
        PagesIndex pagesIndex(types, false, false, true);
        pagesIndex.AddVecBatch(MakeVarcharDictBatch({"delta", "alpha", "charlie", "bravo"}, ids, &keep));
        pagesIndex.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        pagesIndex.Sort(cols, asc, nf, 1, 0, n);
        auto got = ReadVarcharSorted(pagesIndex, 0);
        ASSERT_EQ(got.front(), "alpha");
        RunTipi(pagesIndex, 1, 1, 1);
        delete keep;
    }

    // multi-column tie break
    {
        DataTypes types({VarcharType(8), IntType()});
        PagesIndex pagesIndex(types);
        const int32_t n = 6;
        auto *vb = new VectorBatch(n);
        auto *v0 = new Vector<LargeStringContainer<std::string_view>>(n);
        auto *v1 = new Vector<int32_t>(n);
        const char *keys[] = {"b", "a", "b", "a", "c", "b"};
        int32_t vals[] = {3, 9, 1, 2, 5, 2};
        for (int i = 0; i < n; ++i) {
            v0->SetValue(i, std::string_view(keys[i]));
            v1->SetValue(i, vals[i]);
        }
        vb->Append(v0);
        vb->Append(v1);
        pagesIndex.AddVecBatch(vb);
        pagesIndex.Prepare();
        int32_t cols[] = {0, 1}, asc[] = {1, 1}, nf[] = {1, 1};
        pagesIndex.Sort(cols, asc, nf, 2, 0, n);
        auto gotStr = ReadVarcharSorted(pagesIndex, 0);
        ASSERT_EQ(gotStr[0], "a");
        ASSERT_EQ(gotStr[5], "c");
        auto gotInt = ReadTypedSorted<int32_t>(pagesIndex, 1);
        ASSERT_EQ(gotInt[0], 2);
        ASSERT_EQ(gotInt[5], 5);
    }

    // primitives: int nulls, desc, long/double/float/bool/short/byte, decimal, dict int
    {
        DataTypes intTypes({IntType()});
        PagesIndex intPi(intTypes);
        intPi.AddVecBatch(MakeTypedBatch<int32_t>({5, 1, 3, 2, 4}, {false, true, false, false, true}));
        intPi.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        intPi.Sort(cols, asc, nf, 1, 0, 5);
        auto gotInt = ReadTypedSorted<int32_t>(intPi, 0);
        ASSERT_FALSE(gotInt[0].has_value());
        ASSERT_EQ(gotInt[2], 2);

        PagesIndex intDesc(intTypes);
        intDesc.AddVecBatch(MakeTypedBatch<int32_t>({5, 1, 3}, {}));
        intDesc.Prepare();
        int32_t desc[] = {0};
        int32_t nullsLast[] = {0};
        intDesc.Sort(cols, desc, nullsLast, 1, 0, 3);
        gotInt = ReadTypedSorted<int32_t>(intDesc, 0);
        ASSERT_EQ(gotInt[0], 5);

        {
            DataTypes longTypes({LongType()});
            PagesIndex pi(longTypes);
            pi.AddVecBatch(MakeTypedBatch<int64_t>({9, -1, 4, 0}, {}));
            pi.Prepare();
            pi.Sort(cols, asc, nf, 1, 0, 4);
        }
        {
            DataTypes doubleTypes({DoubleType()});
            PagesIndex pi(doubleTypes);
            pi.AddVecBatch(MakeTypedBatch<double>({2.5, -1.0, 0.0, 2.5}, {}));
            pi.Prepare();
            pi.Sort(cols, asc, nf, 1, 0, 4);
        }
        {
            DataTypes floatTypes({FloatType()});
            PagesIndex pi(floatTypes);
            pi.AddVecBatch(MakeTypedBatch<float>({2.5f, -1.0f, 0.0f}, {}));
            pi.Prepare();
            pi.Sort(cols, asc, nf, 1, 0, 3);
        }
        {
            DataTypes boolTypes({BooleanType()});
            PagesIndex pi(boolTypes);
            pi.AddVecBatch(MakeTypedBatch<bool>({true, false, true, false}, {}));
            pi.Prepare();
            pi.Sort(cols, asc, nf, 1, 0, 4);
        }
        {
            DataTypes shortTypes({ShortType()});
            PagesIndex pi(shortTypes);
            pi.AddVecBatch(MakeTypedBatch<int16_t>({5, -2, 0}, {}));
            pi.Prepare();
            pi.Sort(cols, asc, nf, 1, 0, 3);
        }
        {
            DataTypes byteTypes({ByteType()});
            PagesIndex pi(byteTypes);
            pi.AddVecBatch(MakeTypedBatch<int8_t>({5, -2, 0}, {}));
            pi.Prepare();
            pi.Sort(cols, asc, nf, 1, 0, 3);
        }

        DataTypes decTypes({Decimal128Type(38, 0)});
        PagesIndex decPi(decTypes);
        decPi.AddVecBatch(MakeTypedBatch<Decimal128>({Decimal128(10), Decimal128(-1), Decimal128(5)}, {}));
        decPi.Prepare();
        decPi.Sort(cols, asc, nf, 1, 0, 3);
        auto gotDec = ReadTypedSorted<Decimal128>(decPi, 0);
        ASSERT_EQ(*gotDec[0], Decimal128(-1));

        auto *dictBase = new Vector<int32_t>(3);
        dictBase->SetValue(0, 30);
        dictBase->SetValue(1, 10);
        dictBase->SetValue(2, 20);
        int32_t dictIds[] = {0, 1, 2, 1, 0};
        PagesIndex dictPi(intTypes);
        auto *dictVec = VectorHelper::CreateDictionary(dictIds, 5, dictBase);
        auto *vb = new VectorBatch(5);
        vb->Append(dictVec);
        dictPi.AddVecBatch(vb);
        dictPi.Prepare();
        dictPi.Sort(cols, asc, nf, 1, 0, 5);
        gotInt = ReadTypedSorted<int32_t>(dictPi, 0);
        ASSERT_EQ(*gotInt[0], 10);
        delete dictBase;
    }

    // multi-batch int sort
    {
        DataTypes types({IntType()});
        PagesIndex pagesIndex(types);
        pagesIndex.AddVecBatch(MakeTypedBatch<int32_t>({5, 1}, {}));
        pagesIndex.AddVecBatch(MakeTypedBatch<int32_t>({4, 2}, {}));
        pagesIndex.AddVecBatch(MakeTypedBatch<int32_t>({3}, {}));
        pagesIndex.Prepare();
        int32_t cols[] = {0}, asc[] = {1}, nf[] = {1};
        pagesIndex.Sort(cols, asc, nf, 1, 0, int32_t(pagesIndex.GetRowCount()));
        auto got = ReadTypedSorted<int32_t>(pagesIndex, 0);
        for (size_t i = 0; i < got.size(); ++i) {
            ASSERT_EQ(*got[i], int32_t(i + 1));
        }
    }
}

TEST(SortingsCoverageVarcharExtractTest, test_sort_null_and_get_varchar_value)
{
    auto sortAndRead = [](PagesIndex &pagesIndex, int32_t asc, int32_t nf) {
        int32_t cols[] = {0};
        int32_t ascA[] = {asc};
        int32_t nfA[] = {nf};
        pagesIndex.Sort(cols, ascA, nfA, 1, 0, int32_t(pagesIndex.GetRowCount()));
        return ReadVarcharSorted(pagesIndex, 0);
    };

    // varchar nulls last / first (flat) via QuickSort and TimSort
    {
        DataTypes types({VarcharType(8)});
        auto run = [&](bool timSort) {
            PagesIndex nullsLast(types, false, false, timSort);
            nullsLast.AddVecBatch(MakeVarcharBatch({"b", "a", "c"}, {false, true, false}));
            nullsLast.Prepare();
            auto got = sortAndRead(nullsLast, 1, 0);
            ASSERT_EQ(got.back(), "NULL");
            ASSERT_EQ(got.size(), 3u);

            PagesIndex nullsFirst(types, false, false, timSort);
            nullsFirst.AddVecBatch(MakeVarcharBatch({"b", "a", "c"}, {false, true, false}));
            nullsFirst.Prepare();
            got = sortAndRead(nullsFirst, 1, 1);
            ASSERT_EQ(got.front(), "NULL");
        };
        run(false);
        run(true);
    }

    // const varchar + dictionary varchar
    {
        DataTypes types({VarcharType(8)});
        static std::string kConst = "constv";
        PagesIndex pagesIndex(types, false, false, true);
        const int32_t n = 6;
        auto *vb = new VectorBatch(n);
        vb->Append(new ConstVector<std::string_view>(std::string_view(kConst), OMNI_VARCHAR, n));
        pagesIndex.AddVecBatch(vb);
        pagesIndex.Prepare();
        auto got = sortAndRead(pagesIndex, 1, 1);
        ASSERT_EQ(got.size(), 6u);
        for (const auto &s : got) {
            ASSERT_EQ(s, kConst);
        }

        BaseVector *keep = nullptr;
        PagesIndex dictPi(types, false, false, true);
        dictPi.AddVecBatch(MakeVarcharDictBatch({"bb", "aa"}, {0, 1, 0, 1, 0}, &keep));
        dictPi.Prepare();
        got = sortAndRead(dictPi, 1, 0);
        ASSERT_EQ(got.front(), "aa");
        ASSERT_EQ(got.back(), "bb");
        delete keep;
    }

    // dictionary varchar with nulls
    {
        DataTypes types({VarcharType(8)});
        BaseVector *keep = nullptr;
        PagesIndex dictPi(types, false, false, true);
        dictPi.AddVecBatch(
            MakeVarcharDictBatch({"bb", "aa"}, {0, 1, 0, 1, 0}, &keep, {false, true, false, false, true}));
        dictPi.Prepare();
        auto got = sortAndRead(dictPi, 1, 1);
        ASSERT_EQ(got[0], "NULL");
        ASSERT_EQ(got[1], "NULL");
        delete keep;
    }
}

TEST(SortingsCoverageTipiTest, test_dictionary_varchar_desc_ranks_are_lexicographic_asc)
{
    DataTypes types({VarcharType(16)});
    const int32_t n = 48;
    BaseVector *keep = nullptr;
    std::vector<int32_t> ids(n);
    for (int i = 0; i < n; ++i) {
        ids[i] = i % 3;
    }

    PagesIndex pagesIndex(types, false, false, true);
    pagesIndex.AddVecBatch(MakeVarcharDictBatch({"pear", "banana", "apple"}, ids, &keep));
    pagesIndex.Prepare();

    auto **col = pagesIndex.GetColumns()[0];
    auto *addrs = pagesIndex.GetValueAddresses();
    std::vector<int32_t> ranksDesc(n, 0);
    std::vector<int32_t> ranksAsc(n, 0);
    int32_t from = 0, to = n, maxRankDesc = 0, maxRankAsc = 0;

    tipi::SortNullAndGetDictRanks<false, true, false>(col, ranksDesc.data(), addrs, from, to, maxRankDesc);
    from = 0;
    to = n;
    tipi::SortNullAndGetDictRanks<false, true, true>(col, ranksAsc.data(), addrs, from, to, maxRankAsc);

    ASSERT_EQ(maxRankDesc, 2);
    ASSERT_EQ(maxRankAsc, 2);
    ASSERT_EQ(ranksDesc, ranksAsc);
    for (int32_t i = 0; i < n; ++i) {
        ASSERT_EQ(ranksDesc[i], ExpectedAscRank(ReadVarcharAtAddress(pagesIndex, 0, addrs[i]))) << "i=" << i;
    }

    tipi::CountingSortDictRanks<0>(ranksDesc.data(), addrs, 0, n, maxRankDesc);
    auto gotDesc = ReadVarcharViaAddresses(pagesIndex, 0, 0, n);
    ASSERT_EQ(gotDesc.front(), "pear");
    ASSERT_EQ(gotDesc.back(), "apple");
    ASSERT_TRUE(std::is_sorted(gotDesc.begin(), gotDesc.end(), std::greater<std::string>{}));

    delete keep;
}

TEST(SortingsCoverageTipiTest, test_dictionary_varchar_desc_pipeline_and_mixed_groups)
{
    // n=8: insertion path (below kSmallSize). n=48: dictionary counting sort.
    RunDictVarcharPipeline(8, 0);
    RunDictVarcharPipeline(8, 1);
    RunDictVarcharPipeline(48, 0);
    RunDictVarcharPipeline(48, 1);

    // First key is dictionary VARCHAR DESC; second key is unused here except to
    // force PostProcessDictionaryData after counting sort.
    {
        DataTypes types({VarcharType(16), IntType()});
        const int32_t n = 48;
        auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(3);
        dictBase->SetValue(0, std::string_view("pear"));
        dictBase->SetValue(1, std::string_view("banana"));
        dictBase->SetValue(2, std::string_view("apple"));
        std::vector<int32_t> ids(n);
        auto *intVec = new Vector<int32_t>(n);
        for (int i = 0; i < n; ++i) {
            ids[i] = i % 3;
            intVec->SetValue(i, i);
        }
        auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), n, dictBase);
        auto *vb = new VectorBatch(n);
        vb->Append(dictVec);
        vb->Append(intVec);

        PagesIndex pagesIndex(types, false, false, true);
        pagesIndex.AddVecBatch(vb);
        pagesIndex.Prepare();

        std::vector<int64_t> values(n, 0);
        std::vector<uint32_t> lens(n, 0);
        int32_t cols[] = {0, 1}, ascA[] = {0, 1}, nf[] = {1, 1};
        int32_t from = 0, to = n;
        tipi::SortWithTimSort(pagesIndex, values.data(), lens, cols, ascA, nf, 2, 0, from, to);

        auto got = ReadVarcharViaAddresses(pagesIndex, 0, 0, n);
        ASSERT_EQ(got.front(), "pear");
        ASSERT_EQ(got.back(), "apple");
        ASSERT_TRUE(std::is_sorted(got.begin(), got.end(), std::greater<std::string>{}));
        delete dictBase;
    }

    // Same query, mixed group sizes: small group uses insertion sort, large group
    // uses CountingSortDictRanks. Both must be city DESC.
    {
        DataTypes types({IntType(), VarcharType(16)});
        const int32_t smallN = 8;
        const int32_t largeN = 24;
        const int32_t n = smallN + largeN;
        auto *dictBase = new Vector<LargeStringContainer<std::string_view>>(3);
        dictBase->SetValue(0, std::string_view("pear"));
        dictBase->SetValue(1, std::string_view("banana"));
        dictBase->SetValue(2, std::string_view("apple"));
        std::vector<int32_t> ids(n);
        auto *intVec = new Vector<int32_t>(n);
        for (int i = 0; i < n; ++i) {
            ids[i] = i % 3;
            intVec->SetValue(i, i < smallN ? 0 : 1);
        }
        auto *dictVec = VectorHelper::CreateStringDictionary(ids.data(), n, dictBase);
        auto *vb = new VectorBatch(n);
        vb->Append(intVec);
        vb->Append(dictVec);

        PagesIndex pagesIndex(types, false, false, true);
        pagesIndex.AddVecBatch(vb);
        pagesIndex.Prepare();

        std::vector<int64_t> values(n, 0);
        std::vector<uint32_t> lens(n, 0);
        int32_t cols[] = {1}, ascA[] = {0}, nf[] = {1};

        FillVarcharSortBuffers(pagesIndex, 1, values.data(), lens.data(), 0, smallN);
        int32_t from = 0, to = smallN;
        tipi::SortWithTimSort(pagesIndex, values.data(), lens, cols, ascA, nf, 1, 0, from, to);

        from = smallN;
        to = n;
        tipi::SortWithTimSort(pagesIndex, values.data(), lens, cols, ascA, nf, 1, 0, from, to);

        auto smallGot = ReadVarcharViaAddresses(pagesIndex, 1, 0, smallN);
        auto largeGot = ReadVarcharViaAddresses(pagesIndex, 1, smallN, n);
        ASSERT_TRUE(std::is_sorted(smallGot.begin(), smallGot.end(), std::greater<std::string>{}));
        ASSERT_TRUE(std::is_sorted(largeGot.begin(), largeGot.end(), std::greater<std::string>{}));
        ASSERT_EQ(smallGot.front(), "pear");
        ASSERT_EQ(largeGot.front(), "pear");
        ASSERT_EQ(largeGot.back(), "apple");

        delete dictBase;
    }
}
#endif
