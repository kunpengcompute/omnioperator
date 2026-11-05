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

#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#include "util/error_code.h"
#include "operator/spill/spill_tracker.h"
#include "util/omni_exception.h"
#include "memory/memory_manager.h"
#include "operator_config.h"

namespace omniruntime {
namespace op {
constexpr int32_t GB_UNIT = (1 << 30);
constexpr double DEFAULT_AVAILABLE_THRESHOLD = 0.9;

bool SparkSpillConfig::NeedSpill(MemoryBuilder *memoryBuilder)
{
    auto usedMemorySize = mem::MemoryManager::GetGlobalAccountedMemory();
    if (IsSpillEnabled() &&
        (memoryBuilder->GetRowCount() >= GetSpillRowThreshold() || usedMemorySize >= GetSpillMemThreshold())) {
        LogDebug("spill get row count %d, row threshold %d, and get memory usage %lld, memory threshold %lld.",
            memoryBuilder->GetRowCount(), GetSpillRowThreshold(), usedMemorySize, GetSpillMemThreshold());
        return true;
    } else {
        return false;
    }
}

bool SparkSpillConfig::NeedSpill(size_t elementsSize)
{
    auto usedMemorySize = mem::MemoryManager::GetGlobalAccountedMemory();
    if (IsSpillEnabled() &&
        (usedMemorySize >= GetSpillMemThreshold() || static_cast<int32_t>(elementsSize) >= GetSpillRowThreshold())) {
        LogDebug("spill get row count %d, row threshold %d, and get memory usage %lld, memory threshold %lld.",
            elementsSize, GetSpillRowThreshold(), usedMemorySize, GetSpillMemThreshold());
        return true;
    } else {
        return false;
    }
}

OperatorConfig::OperatorConfig(const OperatorConfig &operatorConfig)
{
    auto inputSpillConfig = operatorConfig.GetSpillConfig();
    auto configId = inputSpillConfig->GetSpillConfigId();
    switch (configId) {
        case SPILL_CONFIG_NONE:
        case SPILL_CONFIG_OLK:
        case SPILL_CONFIG_INVALID: {
            spillConfig = std::make_shared<SpillConfig>(*inputSpillConfig);
            break;
        }
        case SPILL_CONFIG_SPARK: {
            auto sparkSpillConfig = static_cast<SparkSpillConfig *>(inputSpillConfig);
            spillConfig = std::make_shared<SparkSpillConfig>(*sparkSpillConfig);
            break;
        }
    }
    this->overflowConfig = std::make_shared<OverflowConfig>(operatorConfig.GetOverflowConfig()->GetOverflowConfigId());
    this->adaptivityThreshold = operatorConfig.GetAdaptivityThreshold();
    this->isRowOutput = operatorConfig.IsRowOutput();
}

OperatorConfig OperatorConfig::DeserializeOperatorConfig(const std::string &configString)
{
    SpillConfig *resultSpillConfig = nullptr;
    OverflowConfig *resultOverflowConfig = nullptr;
    bool needSkipVerify = false;
    int adaptThreshold = -1;
    bool curIsRowOutput = false;
    bool isStatisticalAggregate = false;

    auto result = nlohmann::json::parse(configString);
    if (result.contains("overflowConfig")) {
        auto overflowConfigId = result.at("overflowConfig").at("overflowConfigId").get<OverflowConfigId>();
        resultOverflowConfig = new OverflowConfig(overflowConfigId);
    }

    if (result.contains("spillConfig")) {
        auto spillConfigId = result.at("spillConfig").at("spillConfigId").get<SpillConfigId>();
        auto spillEnabled = result.at("spillConfig").at("spillEnabled").get<bool>();
        auto spillPath = result.at("spillConfig").at("spillPath").get<std::string>();
        auto maxSpillBytes = result.at("spillConfig").at("maxSpillBytes").get<uint64_t>();
        auto writeBufferSize = result.at("spillConfig").at("writeBufferSize").get<uint64_t>();

        switch (spillConfigId) {
            case SPILL_CONFIG_NONE:
            case SPILL_CONFIG_OLK:
            case SPILL_CONFIG_INVALID: {
                resultSpillConfig =
                    new SpillConfig(spillConfigId, spillEnabled, spillPath, maxSpillBytes, writeBufferSize, false);
                break;
            }
            case SPILL_CONFIG_SPARK: {
                auto numElementsForSpillThreshold =
                    result.at("spillConfig").at("numElementsForSpillThreshold").get<int32_t>();
                auto memUsagePctForSpillThreshold =
                    result.at("spillConfig").at("memUsagePctForSpillThreshold").get<int32_t>();
                // Optional run-based spill knobs: absent in configs produced by older
                // planners, in which case the constructor defaults keep the legacy
                // single-run spill behavior.
                auto hashAggSpillRunFraction = 1.0;
                if (result.at("spillConfig").contains("hashAggSpillRunFraction")) {
                    hashAggSpillRunFraction =
                        result.at("spillConfig").at("hashAggSpillRunFraction").get<double>();
                }
                auto hashAggSpillMaxRunRows = 4UL << 20;
                if (result.at("spillConfig").contains("hashAggSpillMaxRunRows")) {
                    hashAggSpillMaxRunRows =
                        result.at("spillConfig").at("hashAggSpillMaxRunRows").get<uint64_t>();
                }
                resultSpillConfig = new SparkSpillConfig(spillEnabled, spillPath, maxSpillBytes,
                    numElementsForSpillThreshold, memUsagePctForSpillThreshold, writeBufferSize, false,
                    hashAggSpillRunFraction, hashAggSpillMaxRunRows);
                break;
            }
            default: {
                std::string omniExceptionInfo = "In fucntion DeserializeOperatorConfig, no such data type " +
                    std::to_string(static_cast<int>(spillConfigId));
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
            }
        }
    }

    if (result.contains("skipExpressionVerify")) {
        needSkipVerify = result.at("skipExpressionVerify").get<bool>();
    }
    if (result.contains("adaptivityThreshold")) {
        adaptThreshold = result.at("adaptivityThreshold").get<int>();
    }
    if (result.contains("isRowOutput")) {
        curIsRowOutput = result.at("isRowOutput").get<bool>();
    }
    if (result.contains("statisticalAggregate")) {
        isStatisticalAggregate = result.at("statisticalAggregate").get<bool>();
    }
    return OperatorConfig{resultSpillConfig, resultOverflowConfig, needSkipVerify, adaptThreshold, curIsRowOutput, isStatisticalAggregate};
}

void CheckHasEnoughDiskSpace(const char *spillPathChars, SpillConfig &spillConfig)
{
    struct statfs diskInfo;
    auto result = statfs(spillPathChars, &diskInfo);
    if (result != 0) {
        std::string message = GetErrorMessage(ErrorCode::DISK_STAT_FAILED) + "Get stat for " + spillPathChars +
            " failed since " + strerror(errno) + ".";
        throw exception::OmniException(GetErrorCode(ErrorCode::DISK_STAT_FAILED), message);
    }

    auto availableDiskSize =
        static_cast<uint64_t>(static_cast<double>(diskInfo.f_bavail * diskInfo.f_bsize) * DEFAULT_AVAILABLE_THRESHOLD);
    auto maxSpillBytes = spillConfig.GetMaxSpillBytes();
    if (availableDiskSize < maxSpillBytes) {
        std::string message = GetErrorMessage(ErrorCode::DISK_SPACE_NOT_ENOUGH) +
            "The available size of the disk where the spill directory " + spillPathChars +
            " located:" + std::to_string(availableDiskSize / GB_UNIT) +
            "GB and the max spill size:" + std::to_string(maxSpillBytes / GB_UNIT) + "GB.";
        throw exception::OmniException(GetErrorCode(ErrorCode::DISK_SPACE_NOT_ENOUGH), message);
    }
}

static void CreateSpillDirectory(const char *spillPathChars)
{
    mkdir(spillPathChars, 0750);
    if (access(spillPathChars, 0) != 0) {
        std::string message = GetErrorMessage(ErrorCode::MKDIR_FAILED) + "Create spill directory " + spillPathChars +
            " failed since " + strerror(errno) + ".";
        throw exception::OmniException(GetErrorCode(ErrorCode::MKDIR_FAILED), message);
    }
}

static void CreateSpillDirectories(std::string &spillPath)
{
    size_t curPos = 0;
    size_t len = spillPath.size();
    while (curPos < len) {
        // create parent directory if it does not exist
        if (spillPath[curPos] == '/') {
            size_t nextPos = curPos + 1;
            auto tmpChar = spillPath[nextPos];
            spillPath[nextPos] = '\0';
            CreateSpillDirectory(spillPath.c_str());
            spillPath[nextPos] = tmpChar;
            curPos = nextPos;
        } else {
            curPos++;
        }
    }
    CreateSpillDirectory(spillPath.c_str());
}

void OperatorConfig::CheckSpillConfig(SpillConfig *spillConfig)
{
    auto &spillPath = spillConfig->GetSpillPath();
    if (spillPath.empty()) {
        // enable spill but the spill path is invalid
        throw exception::OmniException(GetErrorCode(ErrorCode::EMPTY_PATH), GetErrorMessage(ErrorCode::EMPTY_PATH));
    }
    CreateSpillDirectories(spillPath);
    CheckHasEnoughDiskSpace(spillPath.c_str(), *spillConfig);
}
}
}
