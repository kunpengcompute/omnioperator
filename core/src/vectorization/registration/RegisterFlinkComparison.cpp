/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: registration of comparison functions whose Flink semantics differ from Spark's
 *
 * Kept apart from RegisterComparison.cpp so that adapting a Flink expression never touches the
 * Spark registrations. Names are prefixed with "flink_" and are emitted by RexNodeUtil's
 * simpleFunctionNameMap.
 */

#include <string>
#include "vectorization/functions/LeastGreatest.h"

namespace omniruntime::vectorization {
void RegisterFlinkCompareFunctions(const std::string &prefix)
{
    // Flink GREATEST/LEAST (ScalarOperatorGens.generateGreatestLeast) return NULL as soon as
    // one argument is NULL, while Spark skips NULL arguments. Same implementation class with
    // propagateNull enabled.
    VectorFunction::RegisterVectorFunctionFactory(FlinkGreatestSignatures(), makeFlinkGreatest);
    VectorFunction::RegisterVectorFunctionFactory(FlinkLeastSignatures(), makeFlinkLeast);
}
}
