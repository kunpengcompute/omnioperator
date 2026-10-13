/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <cmath>
#include <limits>
#include "operator/config/operator_config.h"
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
#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT

TEST(ConfigTest, sortAlgorithmConfigDefaultsDisabled)
{
    const QueryConfig config{};
    ASSERT_FALSE(config.PdqSortEnabled());
    ASSERT_FALSE(config.InplacePdqSortEnabled());
    ASSERT_FALSE(config.TimSortEnabled());
}
#endif

TEST(ConfigTest, sortAlgorithmConfigFromQueryConfig)
{
    const std::unordered_map<std::string, std::string> configData({
        {QueryConfig::kPdqSortEnabled, "true"},
        {QueryConfig::kInplacePdqSortEnabled, "true"},
        {QueryConfig::kTimSortEnabled, "true"}
    });
    const QueryConfig config(configData);
#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT
    // Compile-time switch enabled: the runtime knobs are respected.
    ASSERT_TRUE(config.PdqSortEnabled());
    ASSERT_TRUE(config.InplacePdqSortEnabled());
    ASSERT_TRUE(config.TimSortEnabled());
#else
    // Compile-time switch strips experimental sorts; runtime knobs are ignored.
    ASSERT_FALSE(config.PdqSortEnabled());
    ASSERT_FALSE(config.InplacePdqSortEnabled());
    ASSERT_FALSE(config.TimSortEnabled());
#endif
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

namespace {
class ScopedSpillTestMemoryLimit {
public:
    explicit ScopedSpillTestMemoryLimit(int64_t limit)
        : previous_(mem::GetGlobalMemoryLimit())
    {
        mem::SetGlobalMemoryLimit(limit);
    }
    ~ScopedSpillTestMemoryLimit()
    {
        mem::SetGlobalMemoryLimit(previous_);
    }
private:
    int64_t previous_;
};
}

TEST(ConfigTest, spillMemoryFractionsPreserveDecimalsAndDefaults)
{
    const QueryConfig defaults;
    EXPECT_DOUBLE_EQ(defaults.memFraction(), 0.1);
    EXPECT_DOUBLE_EQ(defaults.SpillMemFraction(), 0.9);
    for (const auto &value : {"0.9", "0.123456789", "1e-7", "1", " 0.9 "}) {
        const QueryConfig config(std::unordered_map<std::string, std::string>{
            {QueryConfig::kMemFraction, value}, {QueryConfig::KColumnarSpillMemThreshold, value}});
        EXPECT_DOUBLE_EQ(config.memFraction(), std::stod(value));
        EXPECT_DOUBLE_EQ(config.SpillMemFraction(), std::stod(value));
    }
}

TEST(ConfigTest, spillMemoryFractionsRejectPercentagesAndMalformedValues)
{
    for (const auto &value : {"90", "0", "-0.1", "1.1", "nan", "inf", "0.9junk", "0.9 junk", ""}) {
        SCOPED_TRACE(value);
        const QueryConfig config(std::unordered_map<std::string, std::string>{
            {QueryConfig::kMemFraction, value}, {QueryConfig::KColumnarSpillMemThreshold, value}});
        EXPECT_THROW(config.memFraction(), exception::OmniException);
        EXPECT_THROW(config.SpillMemFraction(), exception::OmniException);
    }
}

TEST(ConfigTest, sparkSpillThresholdUsesFractionWithoutDividingAgain)
{
    const ScopedSpillTestMemoryLimit memoryLimit(1000000);
    EXPECT_EQ(op::SparkSpillConfig("/tmp", UINT64_MAX, INT32_MAX).GetSpillMemThreshold(), 900000);
    EXPECT_EQ(op::SparkSpillConfig("/tmp", UINT64_MAX, INT32_MAX, 0.125).GetSpillMemThreshold(), 125000);
    EXPECT_EQ(op::SparkSpillConfig("/tmp", UINT64_MAX, INT32_MAX, 1.0).GetSpillMemThreshold(), 1000000);
    for (double fraction : {0.0, -0.1, 90.0, std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW(op::SparkSpillConfig("/tmp", UINT64_MAX, INT32_MAX, fraction), exception::OmniException);
    }
}

TEST(ConfigTest, sparkSpillThresholdPreservesUnlimitedAndMaximumLimits)
{
    {
        const ScopedSpillTestMemoryLimit memoryLimit(mem::GLOBAL_MEM_UNLIMITED);
        EXPECT_EQ(op::SparkSpillConfig("/tmp", UINT64_MAX, INT32_MAX, 0.9).GetSpillMemThreshold(), INT64_MAX);
    }
    {
        const ScopedSpillTestMemoryLimit memoryLimit(INT64_MAX);
        EXPECT_EQ(op::SparkSpillConfig("/tmp", UINT64_MAX, INT32_MAX, 1.0).GetSpillMemThreshold(), INT64_MAX);
        const auto threshold = op::SparkSpillConfig(
            "/tmp", UINT64_MAX, INT32_MAX, std::nextafter(1.0, 0.0)).GetSpillMemThreshold();
        EXPECT_GT(threshold, 0);
        EXPECT_LT(threshold, INT64_MAX);
    }
}

TEST(ConfigTest, sparkSpillJsonPreservesDecimals)
{
    const ScopedSpillTestMemoryLimit memoryLimit(1000000);
    nlohmann::json config = {{"spillConfig", {
        {"spillConfigId", "SPILL_CONFIG_SPARK"}, {"spillEnabled", true}, {"spillPath", "/tmp"},
        {"maxSpillBytes", 1024}, {"writeBufferSize", 0}, {"numElementsForSpillThreshold", INT32_MAX},
        {"memUsageFractionForSpillThreshold", 0.125}}}};
    auto decoded = op::OperatorConfig::DeserializeOperatorConfig(config.dump());
    auto *spill = dynamic_cast<op::SparkSpillConfig *>(decoded.GetSpillConfig());
    ASSERT_NE(spill, nullptr);
    EXPECT_EQ(spill->GetSpillMemThreshold(), 125000);
}

TEST(ConfigTest, JoinSpillEffectiveRequiresGlobalSpill)
{
    const QueryConfig globalOff({
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "true"}});
    EXPECT_TRUE(globalOff.joinSpillEnabled());
    EXPECT_FALSE(globalOff.joinSpillEffective());
    EXPECT_FALSE(globalOff.joinSpillHandoffEffective());

    const QueryConfig globalOn({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "true"}});
    EXPECT_TRUE(globalOn.joinSpillEffective());
    EXPECT_TRUE(globalOn.joinSpillHandoffEffective());
}

TEST(ConfigTest, ForceBuildGroupSpillRequiresEffectiveHandoff)
{
    const QueryConfig handoffOff({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "false"},
        {QueryConfig::kForceBuildGroupSpillAfterNoMoreInput, "true"}});
    EXPECT_TRUE(handoffOff.forceBuildGroupSpillAfterNoMoreInput());
    EXPECT_FALSE(handoffOff.forceBuildGroupSpillEffective());

    const QueryConfig handoffOn({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "true"},
        {QueryConfig::kForceBuildGroupSpillAfterNoMoreInput, "true"}});
    EXPECT_TRUE(handoffOn.forceBuildGroupSpillEffective());
}

TEST(ConfigTest, PassiveReclaimIsOnByDefault)
{
    const QueryConfig defaults({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kForceBuildGroupSpillAfterNoMoreInput, "false"}});
    EXPECT_TRUE(defaults.passiveReclaimEnabled());
    EXPECT_TRUE(defaults.passiveReclaimEffective());
    EXPECT_FALSE(defaults.forceBuildGroupSpillEffective());
}

TEST(ConfigTest, PassiveReclaimAndForceEobAreIndependent)
{
    const QueryConfig passiveOnly({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "true"},
        {QueryConfig::kPassiveReclaimEnabled, "true"},
        {QueryConfig::kForceBuildGroupSpillAfterNoMoreInput, "false"}});
    EXPECT_TRUE(passiveOnly.passiveReclaimEffective());
    EXPECT_FALSE(passiveOnly.forceBuildGroupSpillEffective());

    const QueryConfig forceOnly({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "true"},
        {QueryConfig::kPassiveReclaimEnabled, "false"},
        {QueryConfig::kForceBuildGroupSpillAfterNoMoreInput, "true"}});
    EXPECT_FALSE(forceOnly.passiveReclaimEffective());
    EXPECT_TRUE(forceOnly.forceBuildGroupSpillEffective());

    const QueryConfig passiveWithoutHandoff({
        {QueryConfig::kSpillEnabled, "true"},
        {QueryConfig::kJoinSpillEnabled, "true"},
        {QueryConfig::kJoinSpillHandoffEnabled, "false"},
        {QueryConfig::kPassiveReclaimEnabled, "true"}});
    EXPECT_TRUE(passiveWithoutHandoff.passiveReclaimEnabled());
    EXPECT_FALSE(passiveWithoutHandoff.passiveReclaimEffective());
}
}
