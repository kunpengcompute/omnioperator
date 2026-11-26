/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: Omni config util source file.
 */

#include <iostream>
#include <fstream>
#include <functional>
#include "config_util.h"

inline std::map<std::string, std::string> ConfigUtil::configMap;

static void Trim(std::string &value)
{
    value.erase(0, value.find_first_not_of(' '));
    value.erase(value.find_last_not_of(' ') + 1);
}

static std::string GetOmniHome()
{
    auto omniHome = std::getenv("OMNI_HOME");
    if (omniHome != nullptr && omniHome[0] != '\0') {
        std::string confDir{omniHome};
        Trim(confDir);
        return confDir;
    } else {
        return "/opt";
    }
}

static std::string GetConfigFilePath()
{
    static std::string confFile = "/conf/omni.conf";
    auto omniConf = std::getenv("OMNI_CONF");
    if (omniConf != nullptr && omniConf[0] != '\0') {
        std::string confDir{omniConf};
        Trim(confDir);
        return confDir + confFile;
    }
    return GetOmniHome() + confFile;
}

static std::map<std::string, std::string> LoadConf()
{
    std::map<std::string, std::string> omniConfigMap;

    auto configFilePath = GetConfigFilePath();
    auto configFileRealPath = realpath(configFilePath.c_str(), nullptr);
    if (configFileRealPath == nullptr) {
        return omniConfigMap;
    }

    std::ifstream confInput(configFileRealPath);
    if (!confInput.is_open()) {
        free(configFileRealPath);
        return omniConfigMap;
    }

    std::string s;
    while (std::getline(confInput, s)) {
        Trim(s);
        if (s.empty() || s[0] == '#') {
            continue;
        }
        auto pos = s.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        auto key = s.substr(0, pos);
        auto value = s.substr(pos + 1);
        Trim(key);
        Trim(value);

        omniConfigMap[key] = value;
    }
    confInput.close();
    free(configFileRealPath);
    return omniConfigMap;
}

Properties ConfigUtil::CreateProperties()
{
    // first, load omni.conf
    configMap = LoadConf();

    // second, set properties
    static Properties tmpProperties;
    SetProperties(tmpProperties);

    return tmpProperties;
}

void ConfigUtil::SetProperties(Properties &tmpProperties)
{
    // set the property policy using omni.conf
    tmpProperties.SetPolicy(InitializePolicy());

    // set the property enableBatchExprEvaluate
    bool isEnableBatchExprEvaluate = false;
    GetProperty<bool>("enableBatchExprEvaluate", isEnableBatchExprEvaluate);
    tmpProperties.SetEnableBatchExprEvaluate(isEnableBatchExprEvaluate);

    // set the property hiveUdfPath
    std::string hiveUdfPropertyFilePath;
    auto ret = GetProperty<std::string &>("hiveUdfPropertyFilePath", hiveUdfPropertyFilePath);
    if (ret) {
        if (hiveUdfPropertyFilePath[0] == '.') {
            hiveUdfPropertyFilePath = GetOmniHome() + "/" + hiveUdfPropertyFilePath;
        }
        auto realPathRes = realpath(hiveUdfPropertyFilePath.c_str(), nullptr);
        if (realPathRes != nullptr) {
            tmpProperties.SetHiveUdfPropertyFilePath(realPathRes);
            free(realPathRes);
        }
    }
}

static void Convert(const std::string &value, bool &property)
{
    if (value == "true") {
        property = true;
    } else {
        property = false;
    }
}

static void Convert(const std::string &value, std::string &property) { property = value; }

template <typename T> bool ConfigUtil::GetProperty(const char *key, T &value)
{
    const auto &iter = configMap.find(key);
    if (iter == configMap.end()) {
        return false;
    }
    Convert(iter->second, value);
    return true;
}

bool ConfigUtil::IsEnableBatchExprEvaluate() { return g_properties.IsEnableBatchExprEvaluate(); }

std::string &ConfigUtil::GetHiveUdfPropertyFilePath() { return g_properties.GetHiveUdfPropertyFilePath(); }

void ConfigUtil::SetEnableBatchExprEvaluate(bool isEnable) { g_properties.SetEnableBatchExprEvaluate(isEnable); }

void ConfigUtil::SetRoundingRule(RoundingRule rule) { g_properties.GetPolicy()->SetRoundingRule(rule); }

RoundingRule ConfigUtil::GetRoundingRule() { return g_properties.GetPolicy()->GetRoundingRule(); }

void ConfigUtil::SetCheckReScaleRule(CheckReScaleRule rule) { g_properties.GetPolicy()->SetCheckReScaleRule(rule); }

CheckReScaleRule ConfigUtil::GetCheckReScaleRule() { return g_properties.GetPolicy()->GetCheckReScaleRule(); }

void ConfigUtil::SetEmptySearchStrReplaceRule(EmptySearchStrReplaceRule rule)
{
    g_properties.GetPolicy()->SetEmptySearchStrReplaceRule(rule);
}

EmptySearchStrReplaceRule ConfigUtil::GetEmptySearchStrReplaceRule()
{
    return g_properties.GetPolicy()->GetEmptySearchStrReplaceRule();
}

void ConfigUtil::SetCastDecimalToDoubleRule(CastDecimalToDoubleRule rule)
{
    g_properties.GetPolicy()->SetCastDecimalToDoubleRule(rule);
}

CastDecimalToDoubleRule ConfigUtil::GetCastDecimalToDoubleRule()
{
    return g_properties.GetPolicy()->GetCastDecimalToDoubleRule();
}

void ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule rule)
{
    g_properties.GetPolicy()->SetNegativeStartIndexOutOfBoundsRule(rule);
}

NegativeStartIndexOutOfBoundsRule ConfigUtil::GetNegativeStartIndexOutOfBoundsRule()
{
    return g_properties.GetPolicy()->GetNegativeStartIndexOutOfBoundsRule();
}

void ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule rule)
{
    g_properties.GetPolicy()->SetZeroStartIndexSupportRule(rule);
}

ZeroStartIndexSupportRule ConfigUtil::GetZeroStartIndexSupportRule()
{
    return g_properties.GetPolicy()->GetZeroStartIndexSupportRule();
}

void ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule rule)
{
    g_properties.GetPolicy()->SetSupportContainerVecRule(rule);
}

SupportContainerVecRule ConfigUtil::GetSupportContainerVecRule()
{
    return g_properties.GetPolicy()->GetSupportContainerVecRule();
}

AggHashTableRule ConfigUtil::GetAggHashTableRule() { return g_properties.GetPolicy()->GetAggHashTableRule(); }

void ConfigUtil::SetAggHashTableRule(AggHashTableRule rule) { g_properties.GetPolicy()->SetAggHashTableRule(rule); }

void ConfigUtil::SetStringToDateFormatRule(StringToDateFormatRule rule)
{
    g_properties.GetPolicy()->SetStringToDateFormatRule(rule);
}

StringToDateFormatRule ConfigUtil::GetStringToDateFormatRule()
{
    return g_properties.GetPolicy()->GetStringToDateFormatRule();
}

void ConfigUtil::SetSupportDecimalPrecisionImprovementRule(SupportDecimalPrecisionImprovementRule rule)
{
    g_properties.GetPolicy()->SetSupportDecimalPrecisionImprovementRule(rule);
}

SupportDecimalPrecisionImprovementRule ConfigUtil::GetSupportDecimalPrecisionImprovementRule()
{
    return g_properties.GetPolicy()->GetSupportDecimalPrecisionImprovementRule();
}

Policy *ConfigUtil::GetPolicy() { return g_properties.GetPolicy(); }

Policy *ConfigUtil::InitializePolicy()
{
    static Policy policy;
    std::map<const char *, std::function<void(Policy *, std::string)>> initFunctionMap = {
        {"RoundingRule", InitRoundingRule},
        {"CheckReScaleRule", InitCheckReScaleRule},
        {"EmptySearchStrReplaceRule", InitEmptySearchStrReplaceRule},
        {"CastDecimalToDoubleRule", InitCastDecimalToDoubleRule},
        {"NegativeStartIndexOutOfBoundsRule", InitNegativeStartIndexOutOfBoundsRule},
        {"ZeroStartIndexSupportRule", InitZeroStartIndexSupportRule},
        {"SupportContainerVecRule", InitSupportContainerVecRule},
        {"StringToDateFormatRule", InitStringToDateFormatRule},
        {"StringToDecimalRule", InitStringToDecimalRule},
        {"SupportDecimalPrecisionImprovementRule", InitSupportDecimalPrecisionImprovementRule},
        {"AggHashTableRule", InitAggHashTableRule}};
    std::string ruleValueStr;
    for (const auto &item : initFunctionMap) {
        const char *ruleKeyStr = item.first;
        if (GetProperty<std::string &>(ruleKeyStr, ruleValueStr)) {
            auto initFunc = item.second;
            initFunc(&policy, ruleValueStr);
        }
    }
    return &policy;
}

void ConfigUtil::InitRoundingRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "DOWN") {
        policy->SetRoundingRule(RoundingRule::DOWN);
    } else if (ruleValueStr == "HALF_UP") {
        policy->SetRoundingRule(RoundingRule::HALF_UP);
    }
}

void ConfigUtil::InitCheckReScaleRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "CHECK_RESCALE") {
        policy->SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);
    } else if (ruleValueStr == "NOT_CHECK_RESCALE") {
        policy->SetCheckReScaleRule(CheckReScaleRule::NOT_CHECK_RESCALE);
    }
}

void ConfigUtil::InitEmptySearchStrReplaceRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "NOT_REPLACE") {
        policy->SetEmptySearchStrReplaceRule(EmptySearchStrReplaceRule::NOT_REPLACE);
    } else if (ruleValueStr == "REPLACE") {
        policy->SetEmptySearchStrReplaceRule(EmptySearchStrReplaceRule::REPLACE);
    }
}

void ConfigUtil::InitCastDecimalToDoubleRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "CONVERT_WITH_STRING") {
        policy->SetCastDecimalToDoubleRule(CastDecimalToDoubleRule::CONVERT_WITH_STRING);
    } else if (ruleValueStr == "CAST") {
        policy->SetCastDecimalToDoubleRule(CastDecimalToDoubleRule::CAST);
    }
}

void ConfigUtil::InitNegativeStartIndexOutOfBoundsRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "INTERCEPT_FROM_BEYOND") {
        policy->SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND);
    } else if (ruleValueStr == "EMPTY_STRING") {
        policy->SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    }
}

void ConfigUtil::InitZeroStartIndexSupportRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "IS_SUPPORT") {
        policy->SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_SUPPORT);
    } else if (ruleValueStr == "IS_NOT_SUPPORT") {
        policy->SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_NOT_SUPPORT);
    }
}

void ConfigUtil::InitSupportContainerVecRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "NOT_SUPPORT") {
        policy->SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    } else if (ruleValueStr == "SUPPORT") {
        policy->SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    }
}

void ConfigUtil::InitStringToDateFormatRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "ALLOW_REDUCED_PRECISION") {
        policy->SetStringToDateFormatRule(StringToDateFormatRule::ALLOW_REDUCED_PRECISION);
    } else if (ruleValueStr == "NOT_ALLOW_REDUCED_PRECISION") {
        policy->SetStringToDateFormatRule(StringToDateFormatRule::NOT_ALLOW_REDUCED_PRECISION);
    }
}

void ConfigUtil::InitSupportDecimalPrecisionImprovementRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "IS_SUPPORT") {
        policy->SetSupportDecimalPrecisionImprovementRule(SupportDecimalPrecisionImprovementRule::IS_SUPPORT);
    } else if (ruleValueStr == "IS_NOT_SUPPORT") {
        policy->SetSupportDecimalPrecisionImprovementRule(SupportDecimalPrecisionImprovementRule::IS_NOT_SUPPORT);
    }
}

void ConfigUtil::InitAggHashTableRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "ARRAY") {
        policy->SetAggHashTableRule(AggHashTableRule::ARRAY);
    } else if (ruleValueStr == "NORMAL") {
        policy->SetAggHashTableRule(AggHashTableRule::NORMAL);
    }
}

void ConfigUtil::InitStringToDecimalRule(Policy *policy, const std::string &ruleValueStr)
{
    if (ruleValueStr == "OVERFLOW_AS_ROUND_UP") {
        policy->SetStringToDecimalRule(StringToDecimalRule::OVERFLOW_AS_ROUND_UP);
    } else if (ruleValueStr == "OVERFLOW_AS_NULL") {
        policy->SetStringToDecimalRule(StringToDecimalRule::OVERFLOW_AS_NULL);
    }
}
