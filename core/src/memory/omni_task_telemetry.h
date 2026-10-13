/* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved. */
#ifndef OMNI_RUNTIME_OMNI_TASK_TELEMETRY_H
#define OMNI_RUNTIME_OMNI_TASK_TELEMETRY_H

#include <cstdint>
#include <string>

namespace omniruntime {
namespace mem {

class OmniTaskMemoryPool;

struct OmniTaskTelemetryContext {
    std::string runId{"unknown"};
    std::string queryId{"unknown"};
    std::string round{"unknown"};
    int64_t taskAttemptId{-1};
};

// Kept outside OmniTaskMemoryPool's public layout so benchmark-only metadata
// does not change the pool ABI or force every native operator to rebuild.
void SetOmniTaskPoolTelemetry(
    OmniTaskMemoryPool *pool,
    const std::string &runId,
    const std::string &queryId,
    const std::string &round,
    int64_t taskAttemptId);
OmniTaskTelemetryContext GetOmniTaskPoolTelemetry(const OmniTaskMemoryPool *pool);
void ClearOmniTaskPoolTelemetry(const OmniTaskMemoryPool *pool);

} // namespace mem
} // namespace omniruntime

#endif // OMNI_RUNTIME_OMNI_TASK_TELEMETRY_H
