/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: registry hive udf.
 */
#include <fstream>
#include <algorithm>
#include "util/debug.h"
#include "util/config_util.h"
#include "functions/udffunctions.h"
#include "func_registry_hive_udf.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

std::vector<Function> HiveUdfRegistry::GetFunctions()
{
    std::vector<Function> hiveUdfFunctions = { Function(reinterpret_cast<void *>(EvaluateHiveUdfSingle),
        "EvaluateHiveUdfSingle", {}, std::vector<DataTypeId> {}, OMNI_INT),
#ifndef EXCLUDE_BATCH_FUNCTIONS
        Function(reinterpret_cast<void *>(EvaluateHiveUdfBatch), "EvaluateHiveUdfBatch", {}, std::vector<DataTypeId> {},
        OMNI_INT)
#endif
        };
    return hiveUdfFunctions;
}

static void Trim(std::string &value)
{
    value.erase(0, value.find_first_not_of(' '));
    value.erase(value.find_last_not_of(' ') + 1);
}

void HiveUdfRegistry::GenerateHiveUdfMap(std::unordered_map<std::string, std::string> &hiveUdfMap)
{
    std::string propertyFile = ConfigUtil::GetHiveUdfPropertyFilePath();
    if (propertyFile.empty()) {
        LogWarn("No hive udf properties file.");
        return;
    }
    Trim(propertyFile);
    auto realPathRes = realpath(propertyFile.c_str(), nullptr);
    if (realPathRes == nullptr) {
        LogWarn("realpath failed.");
        return;
    }

    // the property file has been normalized in ConfigUtil
    std::ifstream file(realPathRes);
    if (!file.good()) {
        LogWarn("%s does not exist.", realPathRes);
        free(realPathRes);
        return;
    }
    free(realPathRes);

    std::string s;
    while (getline(file, s)) {
        Trim(s);
        auto pos = s.find(' ');
        if (pos == std::string::npos) {
            continue;
        }
        std::string udfName = s.substr(0, pos);
        std::string udfClass = s.substr(pos + 1);
        Trim(udfName);
        Trim(udfClass);
        std::transform(udfName.begin(), udfName.end(), udfName.begin(), ::tolower);
        hiveUdfMap.insert(std::make_pair(udfName, udfClass));
    }
    file.close();
}
}