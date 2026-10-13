/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Runtime telemetry switches
 */
#ifndef OMNI_RUNTIME_TELEMETRY_H
#define OMNI_RUNTIME_TELEMETRY_H

#include <cstdlib>

namespace omniruntime {
namespace telemetry {

inline bool IsEnvironmentSwitchEnabled(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && !(value[0] == '0' && value[1] == '\0');
}

inline bool IsReclaimVerboseEnabled()
{
    // Do not cache this value: unit tests and embedded runtimes may toggle the
    // diagnostic switch between queries in the same process.
    return IsEnvironmentSwitchEnabled("OMNI_RECLAIM_VERBOSE");
}

inline bool IsReclaimSuccessTelemetryEnabled()
{
    return IsReclaimVerboseEnabled() || IsEnvironmentSwitchEnabled("OMNI_RECLAIM_VERBOSE_SUCCESS");
}

// Operator lifecycle diagnostics are intentionally independent from reclaim
// telemetry. Reservation, arbitration, spill and reservation-release events
// use IsReclaimVerboseEnabled(); this switch is for unrelated operator traces.
inline bool IsOperatorVerboseEnabled()
{
    static const bool enabled = IsEnvironmentSwitchEnabled("OMNI_OPERATOR_VERBOSE");
    return enabled;
}

} // namespace telemetry
} // namespace omniruntime

#endif // OMNI_RUNTIME_TELEMETRY_H
