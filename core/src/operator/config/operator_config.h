/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef OMNI_RUNTIME_OPERATOR_CONFIG_H
#define OMNI_RUNTIME_OPERATOR_CONFIG_H

#include <cstdint>
#include <climits>
#include <string>
#include <nlohmann/json.hpp>
#include "operator/memory_builder.h"
#include "memory/global_mem_tracker.h"   // process-global accounting for the self-spill threshold
#include "util/omni_exception.h"

namespace omniruntime {
namespace op {
enum SpillConfigId {
    SPILL_CONFIG_NONE = 0,
    SPILL_CONFIG_OLK = 1,
    SPILL_CONFIG_SPARK = 2,
    SPILL_CONFIG_INVALID
};

NLOHMANN_JSON_SERIALIZE_ENUM(SpillConfigId, { { SPILL_CONFIG_NONE, "SPILL_CONFIG_NONE" },
    { SPILL_CONFIG_OLK, "SPILL_CONFIG_OLK" },
    { SPILL_CONFIG_SPARK, "SPILL_CONFIG_SPARK" },
    { SPILL_CONFIG_INVALID, "SPILL_CONFIG_INVALID" } })

class SpillConfig {
public:
    SpillConfig() : SpillConfig(SPILL_CONFIG_NONE, false, "", DEFAULT_MAX_SPILL_BYTES, DEFAULT_WRITE_BUFFER_SIZE, false) {}

    SpillConfig(SpillConfigId id, bool enabled, const std::string &spillPath, uint64_t maxSpillBytes)
        : SpillConfig(id, enabled, spillPath, maxSpillBytes, DEFAULT_WRITE_BUFFER_SIZE, false)
    {}

    SpillConfig(SpillConfigId id, bool enabled, const std::string &spillPath, uint64_t maxSpillBytes,
        uint64_t writeBufferSize, bool isCompressEnabled)
        : spillConfigId(id),
          spillEnabled(enabled),
          spillPath(spillPath),
          maxSpillBytes(maxSpillBytes),
          writeBufferSize(writeBufferSize),
          spillCompressEnabled(isCompressEnabled)
    {}

    SpillConfig(const SpillConfig &spillConfig)
        : SpillConfig(spillConfig.spillConfigId, spillConfig.spillEnabled, spillConfig.spillPath,
        spillConfig.maxSpillBytes, spillConfig.writeBufferSize, spillConfig.spillCompressEnabled)
    {}

    virtual ~SpillConfig() = default;

    SpillConfigId GetSpillConfigId()
    {
        return spillConfigId;
    }

    bool IsSpillEnabled() const
    {
        return spillEnabled;
    }

    std::string &GetSpillPath()
    {
        return spillPath;
    }

    uint64_t GetMaxSpillBytes() const
    {
        return maxSpillBytes;
    }

    void SetMaxSpillBytes(uint64_t maxSpillSize)
    {
        this->maxSpillBytes = maxSpillSize;
    }

    uint64_t GetWriteBufferSize() const
    {
        return writeBufferSize;
    }

    bool IsSpillCompressEnabled() const
    {
        return spillCompressEnabled;
    }

protected:
    static constexpr uint64_t DEFAULT_MAX_SPILL_BYTES = 100UL * (1 << 30);
    static constexpr uint64_t DEFAULT_WRITE_BUFFER_SIZE = 4 * (1 << 20);
    SpillConfigId spillConfigId;
    bool spillEnabled;
    std::string spillPath;
    uint64_t maxSpillBytes;
    uint64_t writeBufferSize;
    bool spillCompressEnabled;
};

class OLKSpillConfig : public SpillConfig {
public:
    OLKSpillConfig() : SpillConfig(SPILL_CONFIG_OLK, false, "", UINT64_MAX) {}

    OLKSpillConfig(bool spillEnabled, const std::string &spillPath, uint64_t maxSpillBytes)
        : SpillConfig(SPILL_CONFIG_OLK, spillEnabled, spillPath, maxSpillBytes)
    {}

    OLKSpillConfig(OLKSpillConfig &olkSpillConfig) : SpillConfig(static_cast<SpillConfig &>(olkSpillConfig)) {}

    ~OLKSpillConfig() override = default;
};

class SparkSpillConfig : public SpillConfig {
public:
    SparkSpillConfig(bool enabled, const std::string &spillPath, uint64_t maxSpillBytes, int32_t numElementsThreshold,
        double memUsageFraction, uint64_t writeBufferSize, bool isCompressEnabled)
        : SpillConfig(SPILL_CONFIG_SPARK, enabled, spillPath, maxSpillBytes, writeBufferSize, isCompressEnabled),
          numElementsForSpillThreshold(numElementsThreshold)
    {
        OMNI_CHECK(memUsageFraction > 0.0 && memUsageFraction <= 1.0,
            "Spill memory fraction must be in (0, 1]");
        // Self-spill threshold = global limit * pct. The global limit is the executor
        // offheap budget Gluten passes at init via
        // nova.hetu.omniruntime.memory.MemoryManager.setGlobalMemoryLimit(offHeapSize).
        // UNLIMITED until set → INT64_MAX → never triggers, so the
        // trigger stays dormant in standalone/test paths that never set the limit.
        // Observe-only: this is the self-spill DECISION threshold, NOT an allocation cap
        // (Spark still owns per-task MEM_CAP enforcement via the AllocationListener).
        int64_t globalLimit = mem::GetGlobalMemoryLimit();
        memUsageForSpillThreshold = (globalLimit == mem::GLOBAL_MEM_UNLIMITED)
            ? INT64_MAX
            // INT64_MAX rounds to 2^63 as a double. Preserve the integral
            // limit at 100% rather than converting that out-of-range value.
            : (memUsageFraction == 1.0 ? globalLimit
                : static_cast<int64_t>(static_cast<double>(globalLimit) * memUsageFraction));
    }

    SparkSpillConfig(const std::string &spillPath, uint64_t maxSpillBytes, int32_t numElementsThreshold,
        double memUsageFraction = 0.9, uint64_t writeBufferSize = 0, bool isCompressEnabled = false)
        : SparkSpillConfig(true, spillPath, maxSpillBytes, numElementsThreshold, memUsageFraction, writeBufferSize, isCompressEnabled)
    {}

    SparkSpillConfig(const std::string &spillPath, int32_t numElementsThreshold)
        : SparkSpillConfig(spillPath, DEFAULT_MAX_SPILL_BYTES, numElementsThreshold)
    {}

    SparkSpillConfig(const SparkSpillConfig &spillConfig)
        : SpillConfig((SpillConfig &)spillConfig),
          numElementsForSpillThreshold(spillConfig.numElementsForSpillThreshold),
          memUsageForSpillThreshold(spillConfig.memUsageForSpillThreshold)
    {}

    ~SparkSpillConfig() override = default;

    int32_t GetSpillRowThreshold() const
    {
        return numElementsForSpillThreshold;
    }

    int64_t GetSpillMemThreshold() const
    {
        return memUsageForSpillThreshold;
    }

private:
    int32_t numElementsForSpillThreshold;
    int64_t memUsageForSpillThreshold;
};


enum OverflowConfigId {
    OVERFLOW_CONFIG_EXCEPTION = 0,
    OVERFLOW_CONFIG_NULL = 1
};

NLOHMANN_JSON_SERIALIZE_ENUM(OverflowConfigId,
    { { OVERFLOW_CONFIG_EXCEPTION, "OVERFLOW_CONFIG_EXCEPTION" }, { OVERFLOW_CONFIG_NULL, "OVERFLOW_CONFIG_NULL" } })
class OverflowConfig {
public:
    OverflowConfig() : OverflowConfig(OVERFLOW_CONFIG_EXCEPTION) {}

    OverflowConfig(OverflowConfigId overflowConfigId) : overflowConfigId(overflowConfigId) {}

    OverflowConfigId GetOverflowConfigId() const
    {
        return overflowConfigId;
    }

    bool IsOverflowAsNull() const
    {
        return overflowConfigId == OVERFLOW_CONFIG_NULL;
    }

private:
    OverflowConfigId overflowConfigId;
};

class OperatorConfig {
public:
    OperatorConfig()
        : spillConfig(std::make_shared<SpillConfig>()), overflowConfig(std::make_shared<OverflowConfig>()), isSkipVerify(false),
          adaptivityThreshold(-1)
    {}

    OperatorConfig(const OperatorConfig &operatorConfig);

    OperatorConfig(SpillConfig *spillConfig, OverflowConfig *overflowConfig, bool isSkipVerify,
        int adaptivityThreshold = -1, bool isRowOutput = false, bool isStatisticalAggregate = false)
        : spillConfig((spillConfig != nullptr)
                      ? std::shared_ptr<SpillConfig>(spillConfig)
                      : std::make_shared<SpillConfig>()),
        overflowConfig((overflowConfig != nullptr)
                       ? std::shared_ptr<OverflowConfig>(overflowConfig)
                       : std::make_shared<OverflowConfig>()), isSkipVerify(isSkipVerify),
        adaptivityThreshold(adaptivityThreshold), isRowOutput(isRowOutput),
        isStatisticalAggregate(isStatisticalAggregate)
    {}

    OperatorConfig(SpillConfig *spillConfig, OverflowConfig *overflowConfig)
        : spillConfig((spillConfig != nullptr)
                      ? std::shared_ptr<SpillConfig>(spillConfig)
                      : std::make_shared<SpillConfig>()),
        overflowConfig((overflowConfig != nullptr)
                       ? std::shared_ptr<OverflowConfig>(overflowConfig)
                       : std::make_shared<OverflowConfig>()), isSkipVerify(false) {}

    explicit OperatorConfig(const OverflowConfig &overflowConfig)
        : spillConfig(std::make_shared<SpillConfig>()),
        overflowConfig(std::make_shared<OverflowConfig>(overflowConfig)), isSkipVerify(false) {}

    explicit OperatorConfig(const SpillConfig &spillConfig)
        : spillConfig(std::make_shared<SpillConfig>(spillConfig)), overflowConfig(std::make_shared<OverflowConfig>()),
        isSkipVerify(false) {}

    explicit OperatorConfig(const SparkSpillConfig &sparkSpillConfig)
        : spillConfig(std::make_shared<SparkSpillConfig>(sparkSpillConfig)),
        overflowConfig(std::make_shared<OverflowConfig>()), isSkipVerify(false) {}

    ~OperatorConfig() {}

    SpillConfig *GetSpillConfig() const
    {
        return spillConfig.get();
    }

    void SetSpillConfig(SpillConfig *pSpillConfig)
    {
        this->spillConfig = std::shared_ptr<SpillConfig>(pSpillConfig);
    }

    OverflowConfig *GetOverflowConfig() const
    {
        return overflowConfig.get();
    }

    void SetOverflowConfig(OverflowConfig *pOverflowConfig)
    {
        this->overflowConfig = std::shared_ptr<OverflowConfig>(pOverflowConfig);
    }

    bool IsSkipVerify() const
    {
        return isSkipVerify;
    }

    void SetSkipVerify(bool needSkipVerify)
    {
        this->isSkipVerify = needSkipVerify;
    }

    int GetAdaptivityThreshold() const
    {
        return adaptivityThreshold;
    }

    bool IsRowOutput() const
    {
        return isRowOutput;
    }

    bool IsStatisticalAggregate() const
    {
        return isStatisticalAggregate;
    }

    bool PdqSortEnabled() const
    {
#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT
        return pdqSortEnabled;
#else
        return false;
#endif
    }

    bool InplacePdqSortEnabled() const
    {
#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT
        return inplacePdqSortEnabled;
#else
        return false;
#endif
    }

    bool TimSortEnabled() const
    {
#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT
        return timSortEnabled;
#else
        return false;
#endif
    }

    void SetSortAlgorithm(bool enablePdqSort, bool enableInplacePdqSort, bool enableTimSort)
    {
#ifdef OMNI_ENABLE_EXPERIMENTAL_SORT
        pdqSortEnabled = enablePdqSort;
        inplacePdqSortEnabled = enableInplacePdqSort;
        timSortEnabled = enableTimSort;
#else
        (void)enablePdqSort;
        (void)enableInplacePdqSort;
        (void)enableTimSort;
#endif
    }

    static OperatorConfig DeserializeOperatorConfig(const std::string &configString);

    static void CheckSpillConfig(SpillConfig *spillConfig);

private:
    std::shared_ptr<SpillConfig> spillConfig = nullptr;
    std::shared_ptr<OverflowConfig> overflowConfig = nullptr;
    bool isSkipVerify = false;
    int adaptivityThreshold = -1;
    bool isRowOutput = false;
    bool isStatisticalAggregate = false;
    bool pdqSortEnabled = false;
    bool inplacePdqSortEnabled = false;
    bool timSortEnabled = false;
};

struct PrefixSortConfig {
    PrefixSortConfig() = default;

    PrefixSortConfig(
            uint32_t _maxNormalizedKeyBytes,
            uint32_t _minNumRows,
            uint32_t _maxStringPrefixLength)
            : maxNormalizedKeyBytes(_maxNormalizedKeyBytes),
              minNumRows(_minNumRows),
              maxStringPrefixLength(_maxStringPrefixLength) {}

    /// Maximum bytes that can be used to store normalized keys in prefix-sort
    /// buffer per entry. Same with QueryConfig kPrefixSortNormalizedKeyMaxBytes.
    uint32_t maxNormalizedKeyBytes{128};

    /// Minimum number of rows to apply prefix sort. Prefix sort does not perform
    /// with small datasets.
    uint32_t minNumRows{128};

    /// Maximum number of bytes to be stored in prefix-sort buffer for a string
    /// column.
    uint32_t maxStringPrefixLength{16};
};
}
}

#endif // OMNI_RUNTIME_OPERATOR_CONFIG_H
