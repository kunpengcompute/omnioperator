/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include <vector>
#include "gtest/gtest.h"
#include "operator/union/union.h"
#include "operator/hashmap/base_hash_map.h"
#include "operator/hashmap/vector_marshaller.h"
#include "memory/allocator.h"
#include "memory/simple_arena_allocator.h"

#define MUST_CARE_RET [[nodiscard]]

namespace HashMapTest {
/**
 * in unit test, ResourceInfoCollector is usually used to count bytes
 * caller call operator+= after malloc or new manually
 * caller call operator-= after free or delete manually
 * NoMemLeak function is used to check memory leak
 */
class ResourceInfoCollector {
public:
    static ResourceInfoCollector &GetInstance()
    {
        static ResourceInfoCollector infoCollection;
        return infoCollection;
    }
    ResourceInfoCollector &operator += (uint32_t s)
    {
        size += s;
        return *this;
    }
    ResourceInfoCollector &operator -= (uint32_t s)
    {
        size -= s;
        return *this;
    }

    MUST_CARE_RET uint32_t GetState() const
    {
        return size;
    }
    // when this function return false, that means memory leak;
    MUST_CARE_RET bool NoMemLeak() const
    {
        return size == 0;
    }

private:
    ResourceInfoCollector() = default;
    uint32_t size = 0;
};

class ValueWithResource {
public:
    ValueWithResource() : curSize(0)
    {
        resPtr = nullptr;
    }
    ValueWithResource(const ValueWithResource &o) = delete;

    ValueWithResource &operator = (const ValueWithResource &o) = delete;

    ValueWithResource(ValueWithResource &&o) noexcept
    {
        this->curSize = o.curSize;
        o.curSize = 0;
        this->resPtr = o.resPtr;
        o.resPtr = nullptr;
    }

    ValueWithResource &operator = (ValueWithResource &&o) noexcept
    {
        this->curSize = o.curSize;
        o.curSize = 0;
        this->resPtr = o.resPtr;
        o.resPtr = nullptr;
        return *this;
    }

    explicit ValueWithResource(uint32_t size) : curSize(size)
    {
        resPtr = static_cast<char *>(malloc(size));
        // malloc a little memory , it is almost impossible for resPtr to be nullptr expect the memory of system is
        // exhausted
        EXPECT_TRUE(resPtr != nullptr);
        ResourceInfoCollector::GetInstance() += (size);
    }
    ~ValueWithResource()
    {
        if (resPtr != nullptr) {
            free(resPtr);
            ResourceInfoCollector::GetInstance() -= (curSize);
        }
    }

private:
    using ResourcePtr = char *;
    ResourcePtr resPtr = nullptr;
    uint32_t curSize = 0;
};
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

    void SetResource()
    {
        copyCounter = new int(1);
        ResourceInfoCollector::GetInstance() += ResourceUsedBytes;
    }

    NotPodSharedClass &operator = (const NotPodSharedClass &data)
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
    }

    NotPodSharedClass &operator = (NotPodSharedClass &&data) noexcept
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
                ResourceInfoCollector::GetInstance() -= ResourceUsedBytes;
                delete copyCounter;
            }
        }
    }

    MUST_CARE_RET size_t Get0() const
    {
        return std::get<0>(d);
    }
    MUST_CARE_RET double Get1() const
    {
        return std::get<1>(d);
    }
    MUST_CARE_RET bool Get2() const
    {
        return std::get<2>(d);
    }
    bool operator == (const NotPodSharedClass &o) const
    {
        return Get0() == o.Get0() && std::abs(Get1() - o.Get1()) < DBL_EPSILON && Get2() == o.Get2();
    }

private:
    static constexpr uint8_t ResourceUsedBytes = sizeof(int);
    int *copyCounter = nullptr;
    std::tuple<size_t, double, bool> d;
};

/**
 * this class is only movable
 */
class MaxState {
public:
    MaxState() = default;

    MaxState(const MaxState &) = delete;

    MaxState &operator = (const MaxState &) = delete;

    MaxState(MaxState &&o) noexcept
    {
        maxState = o.maxState;
        o.maxState = nullptr;
    }

    MaxState &operator = (MaxState &&o) noexcept
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
            maxState = reinterpret_cast<uint32_t *>(malloc(StateResourceUsedBytes));
            ResourceInfoCollector::GetInstance() += StateResourceUsedBytes;
            *maxState = std::numeric_limits<uint32_t>::min();
        }
    }

    MUST_CARE_RET uint32_t GetResult() const
    {
        return *maxState;
    }

    ~MaxState()
    {
        if (maxState != nullptr) {
            free(maxState);
            ResourceInfoCollector::GetInstance() -= StateResourceUsedBytes;
        }
    }

private:
    static constexpr uint8_t StateResourceUsedBytes = sizeof(uint32_t);
    uint32_t *maxState = nullptr;
};
}

namespace omniruntime::op {
// in unit test, use combine hash to generate NotPodClass's hash.
template <> struct GroupbyHashCalculator<HashMapTest::NotPodSharedClass> {
    size_t operator () (const HashMapTest::NotPodSharedClass &data) const
    {
        auto value = std::hash<int>()(data.Get0());
        value = omniruntime::op::HashUtil::CombineHash(value, std::hash<double>()(data.Get1()));
        value = omniruntime::op::HashUtil::CombineHash(value, std::hash<bool>()(data.Get2()));
        return value;
    }
};

// in unit test, use combine hash to generate NotPodClass's hash.
template <typename T> struct GroupbyHashCalculator<std::unique_ptr<T>> {
    size_t operator () (const std::unique_ptr<T> &data) const
    {
        auto value = std::hash<T>()(*data);
        return value;
    }
};
}

namespace HashMapTest {
// TU-local stand-in for the retired Allocator singleton (allocator.h `= delete`).
// Null-pool Allocator = the legacy-bridge unaccounted path; same process-wide
// lifetime and no-accounting semantics the old singleton had.
static omniruntime::mem::Allocator *TestAllocator()
{
    static omniruntime::mem::Allocator allocator(nullptr);
    return &allocator;
}

/**
 * test for base hashmap , only key > 0
 * and value is the key of insert time, current insert time is 5;
 * check size of GetElementsSize
 * call ForEachKV to verify all kv
 */
TEST(BaseHashMapTest, TestUIntUIntPairNoNullEmplace)
{
    constexpr uint32_t testTotal = 10000;
    constexpr uint32_t insertTime = 5;

    omniruntime::op::DefaultHashMap<uint32_t, uint32_t> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};
    // the min value is 1 , and null value is 0 , so the hashmap didn't have null key
    for (uint32_t key = 1; key <= testTotal; ++key) {
        auto ret = hashMap.Emplace(key);
        EXPECT_TRUE(ret.IsInsert());
        ret.SetValue(1);
    }

    // update kv by insert again, only insert insertTime - 1 time
    for (uint32_t ins = 0; ins < insertTime - 1; ++ins) {
        for (uint32_t key = 1; key <= testTotal; ++key) {
            auto ret = hashMap.Emplace(key);
            EXPECT_TRUE(not ret.IsInsert());
            auto &value = ret.GetValue();
            ++value;
        }
    }

    // verify total size
    EXPECT_EQ(hashMap.GetElementsSize(), testTotal);

    // verify every cell' value by ForEachKV
    hashMap.ForEachKV([&](const uint32_t &key, const uint32_t &value) { EXPECT_EQ(value, insertTime); });
}

TEST(BaseHashMapTest, OnlyNullEmplace)
{
    omniruntime::op::DefaultHashMap<uint32_t, uint32_t> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};

    constexpr uint32_t testTotal = 1000;
    uint32_t nullInt = 0;
    {
        auto ret = hashMap.EmplaceNullValue(nullInt);
        EXPECT_TRUE(ret.IsInsert());
        EXPECT_TRUE(hashMap.HasNullCell());
        ret.SetValue(1);
    }
    // the hashmap only contains null key
    for (size_t key = 0; key < testTotal; ++key) {
        auto ret = hashMap.EmplaceNullValue(nullInt);
        EXPECT_TRUE(not ret.IsInsert());
        auto v = ret.GetValue();
        EXPECT_EQ(v, 1);
    }

    // verify the size
    EXPECT_EQ(hashMap.GetElementsSize(), 1);
}

TEST(BaseHashMapTest, TestUIntUIntPairWithNullEmplace)
{
    omniruntime::op::DefaultHashMap<uint32_t, uint32_t> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};

    constexpr uint32_t testTotal = 1000;
    constexpr uint32_t insertTime = 5;

    // the min value is 0, and null value is 0 , so the hashmap contains null key
    auto ret = hashMap.EmplaceNullValue(0);
    EXPECT_TRUE(ret.IsInsert());
    ret.SetValue(1);

    for (uint32_t key = 1; key < testTotal; ++key) {
        auto ret = hashMap.Emplace(key);
        EXPECT_TRUE(ret.IsInsert());
        ret.SetValue(1);
    }

    // update kv by insert again, only insert insertTime - 1 time
    for (uint32_t ins = 0; ins < insertTime - 1; ++ins) {
        auto ret = hashMap.EmplaceNullValue(0);
        EXPECT_TRUE(not ret.IsInsert());
        auto &value = ret.GetValue();
        ++value;
        for (uint32_t key = 1; key < testTotal; ++key) {
            auto ret = hashMap.Emplace(key);
            EXPECT_TRUE(not ret.IsInsert());
            auto &value = ret.GetValue();
            ++value;
        }
    }

    // verify total size and null cell
    EXPECT_EQ(hashMap.GetElementsSize(), testTotal);
    EXPECT_TRUE(hashMap.HasNullCell());

    // verify every cell' value by ForEachKV

    hashMap.ForEachKV([&insertTime](const uint32_t &key, const uint32_t &value) { EXPECT_EQ(value, insertTime); });
}

/**
 * simulate Aggregate(max) group by (int,double, bool)
 * but use non-POD structure not pointer to implement
 * hashmap will release all memory
 */
TEST(BaseHashMapTest, TestValueWithResourceEmplace)
{
    {
        // hashmap will release all memory
        omniruntime::op::DefaultHashMap<NotPodSharedClass, MaxState *> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};

        constexpr uint32_t total = 100;
        auto currentBytes = 0;
        // generate group by three column i(int),j(float),k(bool)
        for (uint32_t i = 0; i < total; ++i) {
            for (uint32_t j = 0; j < total; ++j) {
                for (uint32_t k = 0; k < 2; ++k) {
                    NotPodSharedClass data(i, static_cast<double>(j), k % 2 == 0);
                    data.SetResource();
                    // the data is referenced by hashmap ,and will release by hashmap's DeconstructAllSlot func
                    auto ret = hashMap.Emplace(data);
                    EXPECT_TRUE(ret.IsInsert());
                    auto s = new MaxState();
                    s->InitState();
                    ret.SetValue(s);
                }
                currentBytes += 8 * 2;
                EXPECT_EQ(ResourceInfoCollector::GetInstance().GetState(), currentBytes);
            }
        }

        // check total size
        EXPECT_EQ(hashMap.GetElementsSize(), total * total * 2);

        // the min value of all maxValue is 10 , every cell's value equal to key.i * key.j
        auto minMaxValue = 10;
        for (uint32_t i = 0; i < total; ++i) {
            for (uint32_t j = 0; j < total; ++j) {
                for (uint32_t k = 0; k < 2; ++k) {
                    NotPodSharedClass data(i, static_cast<double>(j), k % 2 == 0);
                    auto ret = hashMap.Emplace(data);
                    EXPECT_TRUE(not ret.IsInsert());
                    auto &s = ret.GetValue();
                    auto maxIndex = i * j + minMaxValue;
                    // simulate ProcessGroup interface
                    for (uint32_t t = maxIndex; t > maxIndex - minMaxValue; --t) {
                        s->InputProcess(t);
                    }
                }
            }
        }

        // verify every cell's maxValue;
        EXPECT_EQ(hashMap.GetElementsSize(), total * total * 2);
        hashMap.ForEachKV([minMaxValue](const NotPodSharedClass &key, MaxState *state) {
            EXPECT_EQ(state->GetResult(), key.Get0() * key.Get1() + minMaxValue);
            delete state;
        });
    }

    // the resource has been released by hashmap , so there is no memory leak as expected.
    EXPECT_TRUE(ResourceInfoCollector::GetInstance().NoMemLeak());
}

/*
 * the key value is std::string maxState p;
 * add null (empty string means null value)
 * hashmap will release all memory
 */
TEST(BaseHashMapTest, TestStringMaxStateNoNullHashMap)
{
    {
        // hashmap will release all memory of MaxState in this closure
        omniruntime::op::DefaultHashMap<std::string, MaxState *> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};

        constexpr uint32_t total = 1000;

        for (size_t i = 0; i < total; ++i) {
            // the data is referenced by hashmap ,and will release by hashmap's DeconstructAllSlot func
            auto ret = hashMap.Emplace(std::to_string(i));
            EXPECT_TRUE(ret.IsInsert());
            auto s = new MaxState();
            s->InitState();
            // s is only movable
            ret.SetValue(s);
        }

        // check total size
        EXPECT_EQ(hashMap.GetElementsSize(), total);

        // the min value of all maxValue is 10 , every cell's value equal to key.i * key.j
        uint32_t minMaxValue = 10;
        for (size_t i = 0; i < total; ++i) {
            auto ret = hashMap.Emplace(std::to_string(i));
            EXPECT_TRUE(not ret.IsInsert());
            auto &s = ret.GetValue();
            auto maxIndex = i + minMaxValue;
            // simulate ProcessGroup interface
            for (uint32_t t = maxIndex; t > maxIndex - minMaxValue; --t) {
                s->InputProcess(t);
            }
        }

        // verify every cell's maxValue;
        EXPECT_EQ(hashMap.GetElementsSize(), total);
        hashMap.ForEachKV([minMaxValue](const std::string &key, MaxState *state) {
            EXPECT_TRUE(state->GetResult() == minMaxValue + std::stoul(key));
            delete state;
        });
    }

    // the resource has been released by hashmap , so there is no memory leak as expected.
    EXPECT_TRUE(ResourceInfoCollector::GetInstance().NoMemLeak());
}

/*
 * the key value is std::string std::unique_ptr<int> p;
 * add null (empty string means null value
 */
TEST(BaseHashMapTest, TestStringUniquePtrHashMap)
{
    constexpr uint32_t testTotal = 10000;

    omniruntime::op::DefaultHashMap<std::string, long *> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};
    {
        std::string str;
        // emplace empty string
        auto ret = hashMap.EmplaceNullValue(str);
        EXPECT_TRUE(ret.IsInsert());
        EXPECT_TRUE(hashMap.HasNullCell());
        ret.SetValue(nullptr);
    }

    {
        for (uint32_t ins = 1; ins <= testTotal; ++ins) {
            auto curKey = std::to_string(ins);
            auto ret = hashMap.Emplace(curKey);
            EXPECT_TRUE(ret.IsInsert());
            auto x = new long(ins);
            ret.SetValue(x);
        }
    }

    // verify total size
    EXPECT_EQ(hashMap.GetElementsSize(), testTotal + 1);

    // verify every cell' value by ForEachKV
    hashMap.ForEachKV([&](const std::string &key, long *value) {
        if (key.empty()) {
            EXPECT_TRUE(value == nullptr);
            return;
        }
        EXPECT_EQ(std::stol(key), (*value));
        delete value;
    });
}
/*
 * the key value is std::unique_ptr<int> std::unique_ptr<int> p;
 * add null (empty string means null value
 */
TEST(BaseHashMapTest, TestUniqueUniquePtrHashMap)
{
    constexpr uint32_t testTotal = 10000;

    omniruntime::op::DefaultHashMap<std::unique_ptr<uint32_t>, long *> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};
    {
        auto ret = hashMap.EmplaceNullValue(std::make_unique<uint32_t>(0));
        EXPECT_TRUE(ret.IsInsert());
        EXPECT_TRUE(hashMap.HasNullCell());
        ret.SetValue(nullptr);
    }

    {
        for (uint32_t ins = 1; ins <= testTotal; ++ins) {
            auto curKey = std::make_unique<uint32_t>(ins);
            auto ret = hashMap.Emplace(std::move(curKey));
            EXPECT_TRUE(ret.IsInsert());
            auto x = new long(ins);
            ret.SetValue(x);
        }
    }

    // verify total size
    EXPECT_EQ(hashMap.GetElementsSize(), testTotal + 1);

    // verify every cell' value by ForEachKV
    hashMap.ForEachKV([&](const std::unique_ptr<uint32_t> &key, long *value) {
        if (*key == 0) {
            EXPECT_TRUE(value == nullptr);
            return;
        }
        EXPECT_EQ(*key, *value);
        delete value;
    });
}
/**
 * the resource is not owned by hashmap
 * so the memory must be released by caller
 */
TEST(BaseHashMapTest, TestUserDefinedRelease)
{
    {
        omniruntime::op::DefaultHashMap<uint32_t, ValueWithResource *> hashMap{omniruntime::op::OmniHashmapAllocator(TestAllocator())};
        constexpr uint32_t total = 1000;
        constexpr uint32_t totalBytes = 1000 * 100;
        std::vector<ValueWithResource *> p(total, nullptr);
        for (uint32_t i = 0; i < total; ++i) {
            auto ret = hashMap.Emplace(i);
            EXPECT_TRUE(ret.IsInsert());
            auto *ptr = new ValueWithResource(100);
            ret.SetValue(ptr);
            p[i] = ptr;
        }
        EXPECT_EQ(ResourceInfoCollector::GetInstance().GetState(), totalBytes);
        // resource must release by caller
        for (auto iter : p) {
            delete iter;
        }
        EXPECT_EQ(ResourceInfoCollector::GetInstance().GetState(), 0);
    }
    // resource has been released by caller
    EXPECT_TRUE(ResourceInfoCollector::GetInstance().NoMemLeak());
}

TEST(BaseHashMapTest, TestRehash)
{
    constexpr uint32_t testTotal = 10000;
    constexpr uint32_t insertTime = 4;

    omniruntime::op::DefaultHashMap<uint32_t, uint32_t> hashMap(omniruntime::op::OmniHashmapAllocator(TestAllocator()), 4);
    for (uint32_t key = 0; key < testTotal; ++key) {
        auto ret = hashMap.Emplace(key);
        EXPECT_TRUE(ret.IsInsert());
        ret.SetValue(key);
    }

    for (uint32_t ins = 0; ins < insertTime; ++ins) {
        for (uint32_t key = 0; key < testTotal; ++key) {
            auto ret = hashMap.Emplace(key);
            EXPECT_FALSE(ret.IsInsert());
            auto &value = ret.GetValue();
            ++value;
        }
    }

    EXPECT_EQ(hashMap.GetElementsSize(), testTotal);
}

TEST(BaseHashMapTest, TestNotNullEmplaceRehash)
{
    constexpr uint32_t testTotal = 10000;
    omniruntime::op::DefaultHashMap<uint32_t, uint32_t> direct(
        omniruntime::op::OmniHashmapAllocator(TestAllocator()), 4);
    omniruntime::op::DefaultHashMap<uint32_t, uint32_t> prehashed(
        omniruntime::op::OmniHashmapAllocator(TestAllocator()), 4);

    // Adaptive-builder contract: the growth-enabled insert keeps a small seed
    // correct; plain EmplaceNotNullKey is reserved for pre-sized tables.
    for (uint32_t key = 0; key < testTotal; ++key) {
        auto directResult = direct.EmplaceNotNullKeyGrowing(key);
        ASSERT_TRUE(directResult.IsInsert());
        directResult.SetValue(key);

        size_t hash = prehashed.CalculateHash(key);
        auto prehashedResult = prehashed.EmplaceNotNullKeyGrowing(key, hash);
        ASSERT_TRUE(prehashedResult.IsInsert());
        prehashedResult.SetValue(key);
    }

    for (uint32_t key = 0; key < testTotal; ++key) {
        EXPECT_FALSE(direct.EmplaceNotNullKey(key).IsInsert());
        size_t hash = prehashed.CalculateHash(key);
        EXPECT_FALSE(prehashed.EmplaceNotNullKey(key, hash).IsInsert());
    }
    EXPECT_EQ(direct.GetElementsSize(), testTotal);
    EXPECT_EQ(prehashed.GetElementsSize(), testTotal);
}

TEST(BaseHashMapTest, TestMemoryUsageOfSerializing)
{
    using namespace omniruntime::vec;
    omniruntime::mem::SimpleArenaAllocator arenaAllocator(4096, TestAllocator());
    std::vector<BaseVector *> groupVectors;
    std::vector<omniruntime::op::VectorSerializer> serializers;
    int rowSize = 10000;

    BaseVector *vector1 = new Vector<int>(rowSize);
    BaseVector *vector2 = new Vector<long>(rowSize);
    BaseVector *vector3 = new Vector<LargeStringContainer<std::string_view *>>(rowSize);
    for (int i = 0; i < rowSize; ++i) {
        // value is null.
        vector1->SetNull(i);
        vector2->SetNull(i);
        (reinterpret_cast<Vector<LargeStringContainer<std::string_view *>> *>(vector3))->SetNull(i);
    }
    groupVectors.push_back(vector1);
    groupVectors.push_back(vector2);
    groupVectors.push_back(vector3);

    serializers.push_back(omniruntime::op::vectorSerializerCenter[omniruntime::type::OMNI_INT]);
    serializers.push_back(omniruntime::op::vectorSerializerCenter[omniruntime::type::OMNI_LONG]);
    serializers.push_back(omniruntime::op::vectorSerializerCenter[omniruntime::type::OMNI_VARCHAR]);

    EXPECT_EQ(arenaAllocator.TotalBytes(), 0);
    for (int rowIdx = 0; rowIdx < rowSize; ++rowIdx) {
        omniruntime::type::StringRef key;
        for (int32_t groupColIdx = 0; groupColIdx < static_cast<int32_t >(groupVectors.size()); ++groupColIdx) {
            auto curVector = groupVectors[groupColIdx];
            auto &curFunc = serializers[groupColIdx];
            curFunc(curVector, rowIdx, arenaAllocator, key);
        }
    }

    int64_t usedBytes = arenaAllocator.UsedBytes();
    // 30000 = vector1(10000) + vector2(10000) + vector3(10000)
    EXPECT_EQ(usedBytes, 30000);

    delete vector1;
    delete vector2;
    delete vector3;
}

TEST(BaseHashMapTest, TestSerializedAndDeserialized)
{
    using namespace omniruntime;
    using namespace omniruntime::vec;

    mem::SimpleArenaAllocator arenaAllocator(4096, TestAllocator());
    std::vector<vec::BaseVector *> groupVectors;
    std::vector<op::VectorSerializer> serializers;

    std::vector<vec::BaseVector *> groupOutputVectors;
    std::vector<op::VectorDeSerializer> deserializers;

    int rowSize = 10000;
    BaseVector *vector1 = new Vector<int>(rowSize);
    BaseVector *vector2 = new Vector<long>(rowSize);
    BaseVector *vector3 = new Vector<LargeStringContainer<std::string_view *>>(rowSize);
    for (int i = 0; i < rowSize; ++i) {
        if (i % 2 == 0) {
            // set null
            vector1->SetNull(i);
            vector2->SetNull(i);
            (dynamic_cast<Vector<LargeStringContainer<std::string_view *>> *>(vector3))->SetNull(i);
        } else {
            // set not null
            (dynamic_cast<Vector<int> *>(vector1))->SetValue(i, 1);
            (dynamic_cast<Vector<long> *>(vector2))->SetValue(i, 2);
            std::string_view s("hello");
            (dynamic_cast<Vector<LargeStringContainer<std::string_view *>> *>(vector3))->SetValue(i, s);
        }
    }
    groupVectors.push_back(vector1);
    groupVectors.push_back(vector2);
    groupVectors.push_back(vector3);

    serializers.push_back(omniruntime::op::vectorSerializerCenter[omniruntime::type::OMNI_INT]);
    serializers.push_back(omniruntime::op::vectorSerializerCenter[omniruntime::type::OMNI_LONG]);
    serializers.push_back(omniruntime::op::vectorSerializerCenter[omniruntime::type::OMNI_VARCHAR]);

    BaseVector *outputVector1 = new Vector<int>(rowSize);
    BaseVector *outputVector2 = new Vector<long>(rowSize);
    BaseVector *outputVector3 = new Vector<LargeStringContainer<std::string_view *>>(rowSize);
    groupOutputVectors.push_back(outputVector1);
    groupOutputVectors.push_back(outputVector2);
    groupOutputVectors.push_back(outputVector3);

    deserializers.push_back(omniruntime::op::vectorDeSerializerCenter[omniruntime::type::OMNI_INT]);
    deserializers.push_back(omniruntime::op::vectorDeSerializerCenter[omniruntime::type::OMNI_LONG]);
    deserializers.push_back(omniruntime::op::vectorDeSerializerCenter[omniruntime::type::OMNI_VARCHAR]);

    for (int rowIdx = 0; rowIdx < rowSize; ++rowIdx) {
        type::StringRef key;
        // serialize
        for (int groupColIdx = 0; groupColIdx < static_cast<int32_t >(groupVectors.size()); ++groupColIdx) {
            auto curVector = groupVectors[groupColIdx];
            auto &curFunc = serializers[groupColIdx];
            curFunc(curVector, rowIdx, arenaAllocator, key);
        }
        // deserialize
        auto *pos = key.data;
        for (int groupColIdx = 0; groupColIdx < static_cast<int32_t >(groupVectors.size()); ++groupColIdx) {
            auto curVectorPtr = groupOutputVectors[groupColIdx];
            auto deserializeFunc = deserializers[groupColIdx];
            pos = deserializeFunc(curVectorPtr, rowIdx, pos);
        }
    }

    for (int i = 0; i < rowSize; ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(outputVector1->IsNull(i), true);
            EXPECT_EQ(outputVector2->IsNull(i), true);
            EXPECT_EQ(outputVector3->IsNull(i), true);
        } else {
            EXPECT_EQ((dynamic_cast<Vector<int> *>(outputVector1))->GetValue(i), 1);
            EXPECT_EQ((dynamic_cast<Vector<long> *>(outputVector2))->GetValue(i), 2);
            std::string_view s("hello");
            EXPECT_EQ((dynamic_cast<Vector<LargeStringContainer<std::string_view*>> *>(outputVector3))->GetValue(i), s);
        }
    }

    delete vector1;
    delete vector2;
    delete vector3;
    delete outputVector1;
    delete outputVector2;
    delete outputVector3;
}
} // end test
