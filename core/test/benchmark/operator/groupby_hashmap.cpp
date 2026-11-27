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

#include <random>
#include <chrono>
#include <unordered_map>
#include "operator/hashmap/base_hash_map.h"
#include "benchmark/benchmark.h"
#include "cstddef"
#include "type/decimal128.h"
namespace om_benchmark {
/*
 * NotPodClass contains a reference integer called copyCounter,
 * reference counting is performed for each copy.
 *
 * NotPodClass will be emplaced into hashmap,
 * and hashmap will release NotPodClass resource correctly
 */
class NotPodSharedClass {
public:
    explicit NotPodSharedClass(size_t i = 0, double j = 0, float k = false) : d(i, j, k) {}

    NotPodSharedClass(const NotPodSharedClass &data)
    {
        copyCounter = data.copyCounter;
        ++(*copyCounter);
        this->d = data.d;
    }

    void SetResource() { copyCounter = new int(1); }

    NotPodSharedClass &operator=(const NotPodSharedClass &data)
    {
        if (this != &data) {
            copyCounter = data.copyCounter;
            ++(*copyCounter);
            this->d = data.d;
        }
        return *this;
    }

    NotPodSharedClass(NotPodSharedClass &&data) noexcept
    {
        if (this != &data) {
            copyCounter = data.copyCounter;
            this->d = data.d;
            data.copyCounter = nullptr;
        }
    };

    NotPodSharedClass &operator=(NotPodSharedClass &&data) noexcept
    {
        if (this != &data) {
            copyCounter = data.copyCounter;
            this->d = data.d;
            data.copyCounter = nullptr;
        }
        return *this;
    }

    ~NotPodSharedClass()
    {
        if (copyCounter != nullptr) {
            --(*copyCounter);
            if (*copyCounter == 0) {
                delete copyCounter;
            }
        }
    }

    size_t Get0() const { return std::get<0>(d); }

    double Get1() const { return std::get<1>(d); }

    bool Get2() const { return std::get<2>(d); }

    bool operator==(const NotPodSharedClass &o) const
    {
        return Get0() == o.Get0() && Get1() - o.Get1() < 0.0000001f && Get2() == o.Get2();
    }

private:
    static constexpr uint8_t RESOURCE_USED_BYTES = sizeof(int);
    int *copyCounter = nullptr;
    std::tuple<size_t, double, bool> d;
};
} // namespace om_benchmark
namespace std {
template <> struct hash<om_benchmark::NotPodSharedClass> {
    size_t operator()(const om_benchmark::NotPodSharedClass &data) const
    {
        auto value = std::hash<int>()(data.Get0());
        value = omniruntime::op::HashUtil::CombineHash(value, std::hash<double>()(data.Get1()));
        value = omniruntime::op::HashUtil::CombineHash(value, std::hash<bool>()(data.Get2()));
        return value;
    }
};
} // namespace std
namespace omniruntime {
namespace op {
template <> struct GroupbyHashCalculator<omniruntime::type::Decimal128> {
    size_t operator()(const omniruntime::type::Decimal128 &val) const
    {
        return omniruntime::op::HashUtil::HashValue(val.LowBits(), val.HighBits());
    }
};
// compare city hash
template <class T> struct GroupbyCityHashCalculator {
    size_t operator()(const T &data) const { return std::hash<T>()(data); }
};

template <> struct GroupbyCityHashCalculator<type::Decimal128> {
    size_t operator()(const type::Decimal128 &data) const
    {
        const uint64_t kMul = 0x9ddfea08eb382d69ULL;
        auto high = data.HighBits();
        auto low = data.LowBits();
        uint64_t a = (low ^ high) * kMul;
        a ^= (a >> 47);
        uint64_t b = (high ^ a) * kMul;
        b ^= (b >> 47);
        b *= kMul;
        return b;
    }
};
} // namespace op
} // namespace omniruntime
namespace om_benchmark {
/**
 * different data type use different random generator
 * use type_traits to implement this function
 * @tparam T
 */
template <typename T> struct TypeRandomDistributor {
    using type = std::uniform_int_distribution<>;
};

template <> struct TypeRandomDistributor<double> {
    using type = std::uniform_real_distribution<>;
};

template <> struct TypeRandomDistributor<float> {
    using type = std::uniform_real_distribution<>;
};

template <typename T, std::enable_if_t<std::is_pod_v<T>> * = nullptr>
void GenerateTypeData(size_t size, size_t range, std::vector<T> &data)
{
    data.resize(size);
    std::default_random_engine re(std::chrono::steady_clock::now().time_since_epoch().count());
    typename TypeRandomDistributor<T>::type randomDis(0, range);
    for (size_t i = 0; i < size; i++) {
        data[i] = randomDis(re);
    }
}

template <typename T, std::enable_if_t<std::is_pod_v<T>> * = nullptr>
static void InsertTypeToUnorderedMap(benchmark::State &state)
{
    std::vector<T> data;
    auto dataSize = state.range(0);
    auto dataRange = state.range(1);
    GenerateTypeData(dataSize, dataRange, data);
    for (auto _ : state) {
        std::unordered_map<T, T> m;
        for (size_t i = 0; i < data.size(); i++) {
            auto value = data[i];
            m.emplace(value, value);
        }
    }
}

template <typename T, std::enable_if_t<std::is_pod_v<T>> * = nullptr>
static void ForEachTypeToUnorderedMap(benchmark::State &state)
{
    std::vector<T> data;
    auto dataSize = state.range(0);
    auto dataRange = state.range(1);
    GenerateTypeData(dataSize, dataRange, data);
    std::unordered_map<T, T> m;
    for (size_t i = 0; i < data.size(); i++) {
        auto value = data[i];
        m.emplace(value, value);
    }
    int i = 0;
    for (auto _ : state) {
        for ([[maybe_unused]] auto &pair : m) {
            ++i;
        }
    }
    std::cout << "UnorderedMap i:" << i << std::endl;
}

template <typename T, std::enable_if_t<std::is_pod_v<T>> * = nullptr>
static void InsertTypeToHashMap(benchmark::State &state)
{
    std::vector<T> data;
    auto dataSize = state.range(0);
    auto dataRange = state.range(1);
    GenerateTypeData(dataSize, dataRange, data);
    for (auto _ : state) {
        omniruntime::op::DefaultHashMap<T, T> m;
        for (size_t i = 0; i < data.size(); i++) {
            auto value = data[i];
            auto ret = m.Emplace(value);
            if (ret.IsInsert()) {
                ret.SetValue(value);
            }
        }
    }
}

template <typename T, std::enable_if_t<std::is_pod_v<T>> * = nullptr>
static void ForEachTypeInHashmap(benchmark::State &state)
{
    // Code inside this loop is measured repeatedly
    std::vector<T> data;
    auto dataSize = state.range(0);
    auto dataRange = state.range(1);
    GenerateTypeData(dataSize, dataRange, data);
    omniruntime::op::DefaultHashMap<T, T> m;
    for (size_t i = 0; i < data.size(); i++) {
        auto value = data[i];
        auto ret = m.Emplace(value);
        if (ret.IsInsert()) {
            ret.SetValue(value);
        }
    }
    int i = 0;
    for (auto _ : state) {
        m.ForEachKV([&i]([[maybe_unused]] const auto &key, const auto &value) { ++i; });
    }
    std::cout << "Hashmap i:" << i << std::endl;
}

static const uint32_t LOW_ELEMENTS = 4096 * 100;
static const uint32_t MIDDLE_ELEMENTS = 4096 * 1000;
static const uint32_t HIGH_ELEMENTS = 4096 * 2000;

static const uint32_t LOW_CARDINALITY = 1024 * 10;
static const uint32_t MIDDLE_CARDINALITY = 1024 * 100;
static const uint32_t HIGH_CARDINALITY = 1024 * 200;

static const uint32_t LOW_CARDINALITY_PER_ONE = 50;     // 50 * 50 * 2
static const uint32_t MIDDLE_CARDINALITY_PER_ONE = 100; // 100 * 100 * 2
static const uint32_t HIGH_CARDINALITY_PER_ONE = 500;   // 500 * 500 * 2

/**
 * this class is only movable
 */
class MaxState {
public:
    MaxState() = default;

    MaxState(const MaxState &) = delete;

    MaxState &operator=(const MaxState &) = delete;

    MaxState(MaxState &&o) noexcept
    {
        maxState = o.maxState;
        o.maxState = nullptr;
    }

    MaxState &operator=(MaxState &&o) noexcept
    {
        maxState = o.maxState;
        o.maxState = nullptr;
        return *this;
    }

    void InputProcess(uint32_t in)
    {
        if (in > *maxState) {
            *maxState = in;
        }
    }

    void InitState()
    {
        if (maxState == nullptr) {
            maxState = reinterpret_cast<uint32_t *>(malloc(STATE_RESOURCE_USED_BYTES));
            *maxState = std::numeric_limits<uint32_t>::min();
        }
    }

    uint32_t GetResult() const { return *maxState; }

    ~MaxState()
    {
        if (maxState != nullptr) {
            free(maxState);
        }
    };

private:
    static constexpr uint8_t STATE_RESOURCE_USED_BYTES = sizeof(uint32_t);
    uint32_t *maxState = nullptr;
};

/**
 * non pod class inserted
 * @param state
 */
static void InsertNonPodToUnorderedMap(benchmark::State &state)
{
    auto dataSize = state.range(0);
    for (auto _ : state) {
        std::unordered_map<NotPodSharedClass, MaxState> m;
        // generate group by three column i(int),j(float),k(bool)
        for (int64_t i = 0; i < dataSize; ++i) {
            for (int64_t j = 0; j < dataSize; ++j) {
                for (int64_t k = 0; k < 2; ++k) {
                    NotPodSharedClass data(i, static_cast<double>(j), k % 2 == 0);
                    data.SetResource();
                    // the data is referenced by hashmap ,and will release by hashmap's DeconstructAllSlot func
                    auto s = MaxState();
                    s.InitState();
                    m.emplace(data, std::move(s));
                }
            }
        } // for i end
    } // state end
}

static void InsertNonPodToHashMap(benchmark::State &state)
{
    auto dataSize = state.range(0);
    for (auto _ : state) {
        omniruntime::op::DefaultHashMap<NotPodSharedClass, MaxState> hashMap;
        // generate group by three column i(int),j(float),k(bool)
        for (int64_t i = 0; i < dataSize; ++i) {
            for (int64_t j = 0; j < dataSize; ++j) {
                for (int64_t k = 0; k < 2; ++k) {
                    NotPodSharedClass data(i, static_cast<double>(j), k % 2 == 0);
                    data.SetResource();
                    // the data is referenced by hashmap ,and will release by hashmap's DeconstructAllSlot func
                    auto ret = hashMap.Emplace(data);
                    auto s = MaxState();
                    s.InitState();
                    ret.SetValue(std::move(s));
                }
            }
        }
    }
}

template <template <typename> class HashAlgo> static void Calculate128ByHash(benchmark::State &state)
{
    auto dataSize = state.range(0);
    for (auto _ : state) {
        omniruntime::op::BaseHashMap<omniruntime::type::Decimal128, omniruntime::type::Decimal128,
                                     HashAlgo<omniruntime::type::Decimal128>, omniruntime::op::Grower,
                                     omniruntime::op::OmniHashmapAllocator>
            mUseCityHash;
        omniruntime::op::DefaultHashMap<omniruntime::type::Decimal128, omniruntime::type::Decimal128> mDefault;

        // generate group by three column i(int),j(float),k(bool)
        for (int64_t i = 0; i < dataSize; ++i) {
            omniruntime::type::Decimal128 data(i, i + 2);
            auto ret = mUseCityHash.Emplace(data);
            ret.SetValue(std::move(data));
        } // for i end
    } // state end
}
} // namespace om_benchmark

// test different hash algorithm
BENCHMARK(om_benchmark::Calculate128ByHash<omniruntime::op::GroupbyCityHashCalculator>)
    ->Args({om_benchmark::LOW_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::Calculate128ByHash<omniruntime::op::GroupbyHashCalculator>)
    ->Args({om_benchmark::LOW_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::Calculate128ByHash<omniruntime::op::GroupbyCityHashCalculator>)
    ->Args({om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::Calculate128ByHash<omniruntime::op::GroupbyHashCalculator>)
    ->Args({om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);

// test insert no pod
BENCHMARK(om_benchmark::InsertNonPodToUnorderedMap)->Args({om_benchmark::LOW_CARDINALITY_PER_ONE})->Iterations(3);

BENCHMARK(om_benchmark::InsertNonPodToHashMap)->Args({om_benchmark::LOW_CARDINALITY_PER_ONE})->Iterations(3);

BENCHMARK(om_benchmark::InsertNonPodToUnorderedMap)->Args({om_benchmark::MIDDLE_CARDINALITY_PER_ONE})->Iterations(3);

BENCHMARK(om_benchmark::InsertNonPodToHashMap)->Args({om_benchmark::MIDDLE_CARDINALITY_PER_ONE})->Iterations(3);

BENCHMARK(om_benchmark::InsertNonPodToUnorderedMap)->Args({om_benchmark::HIGH_CARDINALITY_PER_ONE})->Iterations(3);

BENCHMARK(om_benchmark::InsertNonPodToHashMap)->Args({om_benchmark::HIGH_CARDINALITY_PER_ONE})->Iterations(3);

// test insert MinElement times, and MinCardinality
BENCHMARK(om_benchmark::InsertTypeToUnorderedMap<double>)
    ->Args({om_benchmark::LOW_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(20);
BENCHMARK(om_benchmark::InsertTypeToHashMap<double>)
    ->Args({om_benchmark::LOW_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(20);

BENCHMARK(om_benchmark::InsertTypeToUnorderedMap<int32_t>)
    ->Args({om_benchmark::MIDDLE_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(10);
BENCHMARK(om_benchmark::InsertTypeToHashMap<int32_t>)
    ->Args({om_benchmark::MIDDLE_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(10);

BENCHMARK(om_benchmark::InsertTypeToUnorderedMap<double>)
    ->Args({om_benchmark::MIDDLE_ELEMENTS, om_benchmark::MIDDLE_CARDINALITY})
    ->Iterations(10);
BENCHMARK(om_benchmark::InsertTypeToHashMap<double>)
    ->Args({om_benchmark::MIDDLE_ELEMENTS, om_benchmark::MIDDLE_CARDINALITY})
    ->Iterations(10);

BENCHMARK(om_benchmark::InsertTypeToUnorderedMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::InsertTypeToHashMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::InsertTypeToUnorderedMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::MIDDLE_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::InsertTypeToHashMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::MIDDLE_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::InsertTypeToUnorderedMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::InsertTypeToHashMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::ForEachTypeToUnorderedMap<int64_t>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::ForEachTypeInHashmap<int64_t>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::LOW_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::ForEachTypeToUnorderedMap<float>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::MIDDLE_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::ForEachTypeInHashmap<float>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::MIDDLE_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::ForEachTypeToUnorderedMap<int64_t>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::ForEachTypeInHashmap<int64_t>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);

BENCHMARK(om_benchmark::ForEachTypeToUnorderedMap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);
BENCHMARK(om_benchmark::ForEachTypeInHashmap<double>)
    ->Args({om_benchmark::HIGH_ELEMENTS, om_benchmark::HIGH_CARDINALITY})
    ->Iterations(3);

BENCHMARK_MAIN();
