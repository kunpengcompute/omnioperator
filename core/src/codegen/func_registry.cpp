/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: registry function
 */
#include "func_registry.h"
#include <algorithm>
#include "util/debug.h"
#include "util/config_util.h"

namespace omniruntime::codegen {
using namespace std;

const std::string INVALID_HIVE_UDF = "";

#ifndef EXCLUDE_BATCH_FUNCTIONS
vector<Function> FunctionRegistry::registeredBatchFunctions = InitializeBatchFunc();
#endif
vector<Function> FunctionRegistry::registeredRowFunctions = InitializeRowFunc();
FunctionMapPtr FunctionRegistry::functionRegistry;
FunctionMapPtr FunctionRegistry::functionNullRegistry;
HiveUdfMapPtr FunctionRegistry::hiveUdfMap;
std::once_flag FunctionRegistry::initHiveUdfMap;

vector<unique_ptr<BaseFunctionRegistry>> FunctionRegistry::GetRowFunctionRegistries()
{
    vector<unique_ptr<BaseFunctionRegistry>> functionRegistries;
    functionRegistries.push_back(make_unique<ContextFunctionRegistry>());
    functionRegistries.push_back(make_unique<DecimalFunctionRegistry>());
    functionRegistries.push_back(make_unique<DictionaryFunctionRegistry>());
    functionRegistries.push_back(make_unique<MathFunctionRegistry>());
    functionRegistries.push_back(make_unique<HashFunctionRegistry>());
    functionRegistries.push_back(make_unique<MightContainFunctionRegistry>());
    functionRegistries.push_back(make_unique<VarcharVectorFunctionRegistry>());
    functionRegistries.push_back(make_unique<HiveUdfRegistry>());
    functionRegistries.push_back(make_unique<StringFunctionRegistry>());
#ifndef EXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS
    functionRegistries.push_back(make_unique<DateTimeFunctionRegistry>());
    functionRegistries.push_back(make_unique<JsonFunctionRegistry>());
#endif

    auto policy = GetProperties().GetPolicy();
    if (policy->GetRoundingRule() == RoundingRule::HALF_UP) {
        functionRegistries.push_back(make_unique<MathFunctionRegistryHalfUp>());
        functionRegistries.push_back(make_unique<DecimalFunctionRegistryHalfUp>());
    } else {
        functionRegistries.push_back(make_unique<MathFunctionRegistryDown>());
        functionRegistries.push_back(make_unique<DecimalFunctionRegistryDown>());
    }

    if (policy->GetCheckReScaleRule() == CheckReScaleRule::NOT_CHECK_RESCALE) {
        functionRegistries.push_back(make_unique<DecimalFunctionRegistryNotReScale>());
    } else {
        functionRegistries.push_back(make_unique<DecimalFunctionRegistryReScale>());
    }

    if (policy->GetEmptySearchStrReplaceRule() == EmptySearchStrReplaceRule::REPLACE) {
        functionRegistries.push_back(make_unique<StringFunctionRegistryReplace>());
    } else {
        functionRegistries.push_back(make_unique<StringFunctionRegistryNotReplace>());
    }

    if (policy->GetStringToDateFormatRule() == StringToDateFormatRule::NOT_ALLOW_REDUCED_PRECISION) {
        functionRegistries.push_back(make_unique<StringFunctionRegistryNotAllowReducePrecison>());
    } else {
        functionRegistries.push_back(make_unique<StringFunctionRegistryAllowReducePrecison>());
    }

    if (policy->GetStringToDecimalRule() == StringToDecimalRule::OVERFLOW_AS_ROUND_UP) {
        functionRegistries.push_back(make_unique<StringToDecimalFunctionRegistryAllowRoundUp>());
    } else {
        functionRegistries.push_back(make_unique<StringToDecimalFunctionRegistry>());
    }

    if (policy->GetNegativeStartIndexOutOfBoundsRule() == NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_SUPPORT) {
        functionRegistries.push_back(make_unique<StringFunctionRegistrySupportNegativeAndZeroIndex>());
    } else if (policy->GetNegativeStartIndexOutOfBoundsRule() == NegativeStartIndexOutOfBoundsRule::EMPTY_STRING &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_SUPPORT) {
        functionRegistries.push_back(make_unique<StringFunctionRegistrySupportNotNegativeAndZeroIndex>());
    } else if (policy->GetNegativeStartIndexOutOfBoundsRule() ==
        NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_NOT_SUPPORT) {
        functionRegistries.push_back(make_unique<StringFunctionRegistrySupportNegativeAndNotZeroIndex>());
    } else if (policy->GetNegativeStartIndexOutOfBoundsRule() == NegativeStartIndexOutOfBoundsRule::EMPTY_STRING &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_NOT_SUPPORT) {
        functionRegistries.push_back(make_unique<StringFunctionRegistrySupportNotNegativeAndNotZeroIndex>());
    }

    return functionRegistries;
}

#ifndef EXCLUDE_BATCH_FUNCTIONS
vector<unique_ptr<BaseFunctionRegistry>> FunctionRegistry::GetBatchFunctionRegistries()
{
    vector<unique_ptr<BaseFunctionRegistry>> functionRegistries;
    functionRegistries.push_back(make_unique<BatchDecimalFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchDictionaryFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchMathFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchStringFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchHashFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchVarcharVectorFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchUtilFunctionRegistry>());
    functionRegistries.push_back(make_unique<BatchDateTimeFunctionRegistry>());

    auto policy = GetProperties().GetPolicy();
    if (policy->GetRoundingRule() == RoundingRule::HALF_UP) {
        functionRegistries.push_back(make_unique<BatchMathFunctionRegistryHalfUp>());
        functionRegistries.push_back(make_unique<BatchDecimalFunctionRegistryHalfUp>());
    } else {
        functionRegistries.push_back(make_unique<BatchMathFunctionRegistryDown>());
        functionRegistries.push_back(make_unique<BatchDecimalFunctionRegistryDown>());
    }

    if (policy->GetCheckReScaleRule() == CheckReScaleRule::NOT_CHECK_RESCALE) {
        functionRegistries.push_back(make_unique<BatchDecimalFunctionRegistryNotReScale>());
    } else {
        functionRegistries.push_back(make_unique<BatchDecimalFunctionRegistryReScale>());
    }

    if (policy->GetStringToDateFormatRule() == StringToDateFormatRule::NOT_ALLOW_REDUCED_PRECISION) {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistryNotAllowReducePrecison>());
    } else {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistryAllowReducePrecison>());
    }

    if (policy->GetEmptySearchStrReplaceRule() == EmptySearchStrReplaceRule::REPLACE) {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistryReplace>());
    } else {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistryNotReplace>());
    }

    if (policy->GetStringToDecimalRule() == StringToDecimalRule::OVERFLOW_AS_ROUND_UP) {
        functionRegistries.push_back(make_unique<BatchStringToDecimalFunctionRegistryAllowRoundUp>());
    } else {
        functionRegistries.push_back(make_unique<BatchStringToDecimalFunctionRegistry>());
    }

    if (policy->GetNegativeStartIndexOutOfBoundsRule() == NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_SUPPORT) {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistrySupportNegativeAndZeroIndex>());
    } else if (policy->GetNegativeStartIndexOutOfBoundsRule() == NegativeStartIndexOutOfBoundsRule::EMPTY_STRING &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_SUPPORT) {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistrySupportNotNegativeAndZeroIndex>());
    } else if (policy->GetNegativeStartIndexOutOfBoundsRule() ==
        NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_NOT_SUPPORT) {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistrySupportNegativeAndNotZeroIndex>());
    } else if (policy->GetNegativeStartIndexOutOfBoundsRule() == NegativeStartIndexOutOfBoundsRule::EMPTY_STRING &&
        policy->GetZeroStartIndexSupportRule() == ZeroStartIndexSupportRule::IS_NOT_SUPPORT) {
        functionRegistries.push_back(make_unique<BatchStringFunctionRegistrySupportNotNegativeAndNotZeroIndex>());
    }

    return functionRegistries;
}
#endif // EXCLUDE_BATCH_FUNCTIONS

std::vector<Function> FunctionRegistry::InitializeRowFunc()
{
    hiveUdfMap = std::make_unique<std::unordered_map<std::string, std::string>>();

    std::vector<Function> allFunctions;
    functionRegistry =
        std::make_unique<std::unordered_map<const FunctionSignature *, const Function *, Hash, Equals>>();
    functionNullRegistry =
        std::make_unique<std::unordered_map<const FunctionSignature *, const Function *, Hash, Equals>>();

    auto registries = GetRowFunctionRegistries();
    for (auto const & registry : registries) {
        auto functions = registry->GetFunctions();
        allFunctions.insert(std::end(allFunctions), functions.begin(), functions.end());
    }
    for (auto &function : allFunctions) {
        for (auto &signature : function.GetSignatures()) {
            if (functionRegistry->find(&signature) != functionRegistry->end()) {
                LogWarn("Trying to register functions with same signature: %s", signature.ToString().c_str());
            }
            functionRegistry->insert(std::make_pair(&signature, &function));
            if (function.GetNullableResultType() == INPUT_DATA_AND_OVERFLOW_NULL ||
                (function.GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL &&
                 signature.GetName().find("_null") != string::npos)) {
                functionNullRegistry->insert(std::make_pair(&signature, &function));
            }
        }
    }

    return allFunctions;
}

#ifndef EXCLUDE_BATCH_FUNCTIONS
std::vector<Function> FunctionRegistry::InitializeBatchFunc()
{
    hiveUdfMap = std::make_unique<std::unordered_map<std::string, std::string>>();

    std::vector<Function> allFunctions;
    functionRegistry =
        std::make_unique<std::unordered_map<const FunctionSignature *, const Function *, Hash, Equals>>();
    functionNullRegistry =
        std::make_unique<std::unordered_map<const FunctionSignature *, const Function *, Hash, Equals>>();

    auto registries = GetBatchFunctionRegistries();
    for (auto const & registry : registries) {
        auto functions = registry->GetFunctions();
        allFunctions.insert(std::end(allFunctions), functions.begin(), functions.end());
    }
    for (auto &function : allFunctions) {
        for (auto &signature : function.GetSignatures()) {
            if (functionRegistry->find(&signature) != functionRegistry->end()) {
                LogWarn("Trying to register functions with same signature: %s", signature.ToString().c_str());
            }
            functionRegistry->insert(std::make_pair(&signature, &function));
            if (function.GetNullableResultType() == INPUT_DATA_AND_OVERFLOW_NULL ||
                (function.GetNullableResultType() == INPUT_DATA_AND_NULL_AND_RETURN_NULL &&
                 signature.GetName().find("_null") != string::npos)) {
                functionNullRegistry->insert(std::make_pair(&signature, &function));
            }
        }
    }

    return allFunctions;
}
#endif // EXCLUDE_BATCH_FUNCTIONS

FunctionRegistry::~FunctionRegistry() = default;

const Function *FunctionRegistry::LookupFunction(FunctionSignature *signature)
{
    auto result = functionRegistry->find(signature);
    if (result == functionRegistry->end()) {
        return nullptr;
    }
    return result->second;
}

bool FunctionRegistry::LookupNullFunction(FunctionSignature *signature)
{
    auto signatureNull = FunctionSignature(signature->GetName() + "_null", signature->GetParams(),
        signature->GetReturnType(), signature->GetFunctionAddress());
    auto result = functionNullRegistry->find(&signatureNull);
    return result != functionNullRegistry->end();
}

// Some functions such as CastDecimal128ToStringRetNull(), it needs both contextPtr and overflowConfig as parameters.
// The purpose of the below function is to find this functions.
bool FunctionRegistry::IsNullExecutionContextSet(FunctionSignature *signature)
{
    auto signatureNull = FunctionSignature(signature->GetName() + "_null", signature->GetParams(),
        signature->GetReturnType(), signature->GetFunctionAddress());
    auto result = functionNullRegistry->find(&signatureNull);
    return result->second->IsExecutionContextSet();
}

void FunctionRegistry::InitHiveUdfMap()
{
    HiveUdfRegistry::GenerateHiveUdfMap(*(hiveUdfMap.get()));
}

const std::string &FunctionRegistry::LookupHiveUdf(const std::string &udfName)
{
    std::call_once(initHiveUdfMap, InitHiveUdfMap);
    string lowerStr = udfName;
    transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), [](unsigned char c) { return tolower(c); });
    auto result = hiveUdfMap->find(lowerStr);
    if (result == hiveUdfMap->end()) {
        return INVALID_HIVE_UDF;
    }
    return result->second;
}

std::vector<Function> &FunctionRegistry::GetRowFunctions()
{
    return registeredRowFunctions;
}

#ifndef EXCLUDE_BATCH_FUNCTIONS
std::vector<Function> &FunctionRegistry::GetBatchFunctions()
{
    return registeredBatchFunctions;
}
#endif
}
