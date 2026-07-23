/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include <string>
#include "../functions/IsNanFunction.h"
#include "../functions/IsBooleanFunction.h"
#include "../functions/IsNotUnknownFunction.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterPredicateFunctions(const std::string &prefix)
{
    auto isNanFunction = std::make_shared<IsNanFunction>();
    VectorFunction::RegisterVectorFunction(prefix + "isnan", {OMNI_FLOAT}, OMNI_BOOLEAN, isNanFunction);
    VectorFunction::RegisterVectorFunction(prefix + "isnan", {OMNI_DOUBLE}, OMNI_BOOLEAN, isNanFunction);

    // IS NOT TRUE: NULL->true, non-NULL->!value
    auto isNotTrueFunction = std::make_shared<IsBooleanFunction>(true, true);
    VectorFunction::RegisterVectorFunction(prefix + "is_not_true", {OMNI_BOOLEAN}, OMNI_BOOLEAN, isNotTrueFunction);

    // IS FALSE: NULL->false, non-NULL->!value
    auto isFalseFunction = std::make_shared<IsBooleanFunction>(false, true);
    VectorFunction::RegisterVectorFunction(prefix + "is_false", {OMNI_BOOLEAN}, OMNI_BOOLEAN, isFalseFunction);

    // IS NOT FALSE: NULL->true, non-NULL->value
    auto isNotFalseFunction = std::make_shared<IsBooleanFunction>(true, false);
    VectorFunction::RegisterVectorFunction(prefix + "is_not_false", {OMNI_BOOLEAN}, OMNI_BOOLEAN, isNotFalseFunction);

    // IS NOT UNKNOWN: NULL->false, non-NULL->true
    auto isNotUnknownFunction = std::make_shared<IsNotUnknownFunction>();
    VectorFunction::RegisterVectorFunction(prefix + "is_not_unknown", {OMNI_BOOLEAN}, OMNI_BOOLEAN, isNotUnknownFunction);
}
}
