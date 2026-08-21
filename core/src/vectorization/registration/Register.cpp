/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#include <mutex>
#include "Register.h"
#include "SimpleFunctionRegistry.h"

namespace omniruntime::vectorization {
extern void RegisterArrayFunctions(const std::string &prefix);

extern void RegisterBinaryFunctions(const std::string &prefix);

extern void RegisterBitwiseFunctions(const std::string &prefix);

extern void RegisterCompareFunctions(const std::string &prefix);

extern void RegisterConditionalFunctions(const std::string &prefix);

extern void RegisterConversionFunctions(const std::string &prefix);

extern void RegisterDatetimeFunctions(const std::string &prefix);

extern void RegisterFlinkDatetimeFunctions(const std::string &prefix);

extern void RegisterJsonFunctions(const std::string &prefix);

extern void RegisterMapFunctions(const std::string &prefix);

extern void RegisterMathFunctions(const std::string &prefix);

extern void RegisterPredicateFunctions(const std::string &prefix);

extern void RegisterMiscFunctions(const std::string &prefix);

extern void RegisterRegexpFunctions(const std::string &prefix);

extern void RegisterSpecialFormGeneralFunctions(const std::string &prefix);

extern void RegisterStringFunctions(const std::string &prefix);

extern void RegisterUrlFunctions(const std::string &prefix);

extern void RegisterLambdaFunctions(const std::string &prefix);

extern void RegisterHashFunctions(const std::string &prefix);

extern void RegisterCollectionFunctions(const std::string &prefix);

// Functions whose Flink semantics differ from Spark's; see RegisterFlink*.cpp.
extern void RegisterFlinkCompareFunctions(const std::string &prefix);

extern void RegisterFlinkStringFunctions(const std::string &prefix);

int RegisterFunctions::Register()
{
    RegisterAllFunctions();
    return 1;
}

void RegisterFunctions::RegisterAllFunctions(const std::string &prefix)
{
    RegisterArrayFunctions(prefix);
    RegisterCompareFunctions(prefix);
    RegisterConditionalFunctions(prefix);
    RegisterConversionFunctions(prefix);
    RegisterDatetimeFunctions(prefix);
    RegisterFlinkDatetimeFunctions(prefix);
    RegisterJsonFunctions(prefix);
    RegisterMapFunctions(prefix);
    RegisterMathFunctions(prefix);
    RegisterMiscFunctions(prefix);
    RegisterPredicateFunctions(prefix);
    RegisterStringFunctions(prefix);
    RegisterBitwiseFunctions(prefix);
    RegisterLambdaFunctions(prefix);
    RegisterRegexpFunctions(prefix);
    RegisterHashFunctions(prefix);
    RegisterCollectionFunctions(prefix);
    RegisterFlinkCompareFunctions(prefix);
    RegisterFlinkStringFunctions(prefix);
}

// Called from CreateProjections/ProjectionOperatorFactory so that ProjectVec can resolve
// VectorFunction::Find (e.g. element_at). Not done in RegisterFunctions::state_ static
// init because: (1) state_ only runs when RegisterFunctions is odr-used, and the operator
// path (sort/join/window) never references it; (2) explicit call avoids static init order
// issues with VectorFunction::functionMap_. call_once ensures registration runs exactly once.
void link_register_functions()
{
    static std::once_flag once;
    std::call_once(once, []() { RegisterFunctions::RegisterAllFunctions(""); });
}
}
