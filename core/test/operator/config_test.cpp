/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "util/config/ConfigBase.h"
#include "util/config/QueryConfig.h"
#include "util/format.h"
#include "gtest/gtest.h"

namespace omniruntime::config {
enum TestEnum { ENUM_0 = 0, ENUM_1 = 1, ENUM_2 = 2, UNKNOWN = 3 };

class TestConfig : public ConfigBase {
public:
    template <typename T>
    using Entry = ConfigBase::Entry<T>;

    static Entry<int32_t> kInt32Entry;
    static Entry<uint64_t> kUint64Entry;
    static Entry<bool> kBoolEntry;
    static Entry<std::string> kStringEntry;
    static Entry<TestEnum> kEnumEntry;

    TestConfig(std::unordered_map<std::string, std::string> &&configs, bool _mutable)
        : ConfigBase(std::move(configs), _mutable) {}
};

// Definition needs to be outside of class
TestConfig::Entry<int32_t> TestConfig::kInt32Entry("int32_entry", -32);
TestConfig::Entry<uint64_t> TestConfig::kUint64Entry("uint64_entry", 64);
TestConfig::Entry<bool> TestConfig::kBoolEntry("bool_entry", true);
TestConfig::Entry<std::string> TestConfig::kStringEntry("string_entry", "default.string.value");
TestConfig::Entry<TestEnum> TestConfig::kEnumEntry("enum_entry", TestEnum::ENUM_0, [](const TestEnum &value) {
        if (value == TestEnum::ENUM_0) {
            return "ENUM_0";
        }
        if (value == TestEnum::ENUM_1) {
            return "ENUM_1";
        }
        if (value == TestEnum::ENUM_2) {
            return "ENUM_2";
        }
        return "UNKNOWN";
    }, [](const std::string & /* unused */, const std::string &v) {
        if (v == "ENUM_0") {
            return TestEnum::ENUM_0;
        }
        if (v == "ENUM_1") {
            return TestEnum::ENUM_1;
        }
        if (v == "ENUM_2") {
            return TestEnum::ENUM_2;
        }
        return TestEnum::UNKNOWN;
    });

TEST(ConfigTest, immutableConfig)
{
    // Testing default values
    auto config = std::make_shared<TestConfig>(std::unordered_map<std::string, std::string>(), false);
    ASSERT_EQ(config->Get(TestConfig::kInt32Entry), -32);
    ASSERT_EQ(config->Get(TestConfig::kUint64Entry), 64);
    ASSERT_EQ(config->Get(TestConfig::kBoolEntry), true);
    ASSERT_EQ(config->Get(TestConfig::kStringEntry), "default.string.value");
    ASSERT_EQ(config->Get(TestConfig::kEnumEntry), TestEnum::ENUM_0);

    std::unordered_map<std::string, std::string> rawConfigs{
        {TestConfig::kInt32Entry.key, "-3200"}, {TestConfig::kUint64Entry.key, "6400"},
        {TestConfig::kStringEntry.key, "not.default.string.value"}, {TestConfig::kBoolEntry.key, "false"},
        {TestConfig::kEnumEntry.key, "ENUM_2"}};

    auto expectedRawConfigs = rawConfigs;

    config = std::make_shared<TestConfig>(std::move(rawConfigs), false);

    // Ensure values are unchanged after attempted modifications
    ASSERT_EQ(config->Get(TestConfig::kInt32Entry), -3200);
    ASSERT_EQ(config->Get(TestConfig::kUint64Entry), 6400);
    ASSERT_EQ(config->Get(TestConfig::kBoolEntry), false);
    ASSERT_EQ(config->Get(TestConfig::kStringEntry), "not.default.string.value");
    ASSERT_EQ(config->Get(TestConfig::kEnumEntry), TestEnum::ENUM_2);
    ASSERT_EQ(config->Get(TestConfig::kInt32Entry.key, TestConfig::kInt32Entry.defaultVal), -3200);
    ASSERT_EQ(config->Get(TestConfig::kUint64Entry.key, TestConfig::kUint64Entry.defaultVal), 6400);
    ASSERT_EQ(config->Get(TestConfig::kBoolEntry.key, TestConfig::kBoolEntry.defaultVal), false);
    ASSERT_EQ(config->Get(TestConfig::kStringEntry.key, TestConfig::kStringEntry.defaultVal),
        "not.default.string.value");
    ASSERT_TRUE(config->Get<int32_t>(TestConfig::kInt32Entry.key).has_value());
    ASSERT_EQ(config->Get<int32_t>(TestConfig::kInt32Entry.key).value(), -3200);
    ASSERT_FALSE(config->Get<int32_t>("wrong_int32_key").has_value());

    // Testing value existence
    ASSERT_TRUE(config->ValueExists(TestConfig::kInt32Entry.key));
    ASSERT_FALSE(config->ValueExists("non_existent_entry"));
}

TEST(ConfigTest, mutableConfig)
{
    // Create a mutable configuration with some initial values
    std::unordered_map<std::string, std::string> initialConfigs{
        {TestConfig::kInt32Entry.key, "-3200"}, {TestConfig::kUint64Entry.key, "6400"},
        {TestConfig::kStringEntry.key, "initial.string.value"}, {TestConfig::kBoolEntry.key, "false"},
        {TestConfig::kEnumEntry.key, "ENUM_2"}};

    auto config = std::make_shared<TestConfig>(std::move(initialConfigs), true);

    // Test setting new values
    (*config).Set(TestConfig::kInt32Entry, 123).Set(TestConfig::kStringEntry, std::string("modified.string.value")).
              Set(TestConfig::kBoolEntry.key, "true").Set(TestConfig::kEnumEntry.key, "ENUM_1");

    ASSERT_EQ(config->Get(TestConfig::kInt32Entry), 123);
    ASSERT_EQ(config->Get(TestConfig::kStringEntry), "modified.string.value");
    ASSERT_EQ(config->Get(TestConfig::kBoolEntry), true);
    ASSERT_EQ(config->Get(TestConfig::kEnumEntry), TestEnum::ENUM_1);

    // Test unsetting values
    ASSERT_EQ(config->Get(TestConfig::kUint64Entry), 6400);
    config->Unset(TestConfig::kUint64Entry);
    ASSERT_EQ(config->Get(TestConfig::kUint64Entry), TestConfig::kUint64Entry.defaultVal);

    // Test resetting the configuration
    config->Reset();
    ASSERT_FALSE(config->ValueExists(TestConfig::kUint64Entry.key));
}

TEST(ConfigTest, emptyConfig)
{
    QueryConfig config{};
    ASSERT_FALSE(config.spillEnabled());
}

TEST(ConfigTest, setConfig)
{
    const std::unordered_map<std::string, std::string> configData({{QueryConfig::kSpillEnabled, "true"}});
    const QueryConfig config(configData);

    ASSERT_TRUE(config.spillEnabled());
}

TEST(ConfigTest, maxBatchRowCount)
{
    struct {
        std::optional<int> maxBatchRowCount;;
        int expectedMaxBatchRowCount;
    } testSettings[] = {{std::nullopt, 0}, {2, 2}, {4, 4}, {6, 6}};
    for (const auto &testConfig : testSettings) {
        std::unordered_map<std::string, std::string> configData;
        if (testConfig.maxBatchRowCount.has_value()) {
            configData.emplace(QueryConfig::KMaxBatchRowCount, std::to_string(testConfig.maxBatchRowCount.value()));
        }
        const QueryConfig config(configData);
        ASSERT_EQ(config.maxBatchRowCount(), testConfig.expectedMaxBatchRowCount);
    }
}

TEST(ConfigTest, maxRowCount)
{
    struct {
        std::optional<int> maxRowCount;;
        int expectedMaxRowCount;
    } testSettings[] = {{std::nullopt, 12UL << 20}, {2, 2}, {4, 4}, {6, 6}};
    for (const auto &testConfig : testSettings) {
        std::unordered_map<std::string, std::string> configData;
        if (testConfig.maxRowCount.has_value()) {
            configData.emplace(QueryConfig::KMaxRowCount, std::to_string(testConfig.maxRowCount.value()));
        }
        const QueryConfig config(configData);
        ASSERT_EQ(config.maxRowCount(), testConfig.expectedMaxRowCount);
    }
}
}
