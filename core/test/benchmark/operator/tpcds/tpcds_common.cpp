/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <dirent.h>
#include <fstream>
#include "../common/vector_util.h"
#include "type/data_type_serializer.h"
#include "tpcds_common.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;
using namespace omniruntime::expressions;

namespace om_benchmark {
namespace tpc_ds_data_loader {
static const int FOLDER_FLAG = 4;

nlohmann::json LoadBenchmarkMetaFromPath(const std::string &path)
{
    std::string fileName = path + "/operator_meta.json";
    std::ifstream in(fileName);
    std::string lines;
    std::string line;
    while (getline(in, line)) {
        lines += line;
    }
    return nlohmann::json::parse(lines);
}

std::string LoadDependentDataPathFromPath(const std::string &path)
{
    std::string fileName = path + "/operator_dep_meta.link";
    std::ifstream in(fileName);
    std::string lines;
    std::string line;
    while (getline(in, line)) {
        lines += line;
    }
    return path + "/" + lines;
}

std::vector<VectorBatchSupplier> LoadVectorBatchFromPath(const std::string &path,
                                                         const std::vector<DataTypePtr> &dataTypes)
{
    std::vector<std::string> files = {};
    auto *dir = opendir(path.c_str());
    dirent *file;
    if (dir != nullptr) {
        while ((file = readdir(dir)) != nullptr) {
            if (strstr(file->d_name, ".tbl") && file->d_type != FOLDER_FLAG) {
                files.emplace_back(file->d_name);
            }
        }
        closedir(dir);
    }

    std::vector<VectorBatchSupplier> vectorSuppliers;
    for (const auto &item : files) {
        vectorSuppliers.emplace_back([dataTypes, path, item]() {
            std::string fileName = path + std::string("/").append(item);
            std::ifstream in(fileName);
            std::string line;
            int lineCount = 0;
            while (getline(in, line)) {
                lineCount++;
            }
            auto vectorBatch = new VectorBatch(lineCount);
            auto dataTypeIds = std::vector<DataTypeId>((int)dataTypes.size());
            for (uint32_t i = 0; i < dataTypes.size(); ++i) {
                auto *vector = VectorHelper::CreateVector(OMNI_FLAT, dataTypes[i]->GetId(), lineCount);
                vectorBatch->Append(vector);
                dataTypeIds[i] = dataTypes[i]->GetId();
            }
            in = std::ifstream(fileName);
            int rowIndex = 0;
            while (getline(in, line)) {
                if (rowIndex == lineCount) {
                    break;
                }
                auto value_array = std::vector<std::string>();
                std::istringstream f(line);
                std::string s;
                char sep = '|';
                while (getline(f, s, sep)) {
                    value_array.emplace_back(s);
                }
                SetVectorBatchRow(vectorBatch, dataTypeIds, rowIndex++, value_array);
            }
            return vectorBatch;
        });
    }

    return vectorSuppliers;
}

std::vector<DataTypePtr> LoadDataTypesFromJson(const nlohmann::json &metaJson, const std::string &filedName)
{
    std::vector<omniruntime::type::DataTypePtr> dataTypes;

    for (const auto &item : metaJson[filedName].items()) {
        dataTypes.push_back(omniruntime::type::DataTypeJsonParser(item.value()));
    }

    return dataTypes;
}

std::vector<Expr *> GetExprsFromJson(const std::vector<std::string> &exprs)
{
    auto expressions = std::vector<omniruntime::expressions::Expr *>();
    for (const auto &item : exprs) {
        if (item.empty()) {
            expressions.push_back(nullptr);
            continue;
        }
        auto jsonExpression = nlohmann::json::parse(item);
        auto expression = JSONParser::ParseJSON(jsonExpression);
        if (expression == nullptr) {
            omniruntime::expressions::Expr::DeleteExprs(expressions);
            throw omniruntime::exception::OmniException(
                "EXPRESSION_NOT_SUPPORT", "The expression is not supported yet: " + jsonExpression.dump());
        }
        expressions.push_back(expression);
    }
    return expressions;
}

void getJsonMeta(const std::string &dir, std::vector<std::string> &paths, const std::string &identifier)
{
    auto *dirScan = opendir(dir.c_str());

    dirent *file;
    if (dirScan != nullptr) {
        while ((file = readdir(dirScan)) != nullptr) {
            if (file->d_type == FOLDER_FLAG && file->d_name[0] != '.') {
                getJsonMeta(dir + "/" + file->d_name, paths, identifier);
                continue;
            }
            if (strstr(file->d_name, "operator_meta.json") &&
                std::ifstream(dir + std::string("/").append(identifier)).good()) {
                paths.emplace_back(dir);
                break;
            }
        }
        closedir(dirScan);
    }
}
} // namespace tpc_ds_data_loader

omniruntime::op::OperatorFactory *TpcDsOperatorFixture::createOperatorFactory(const benchmark::State &state)
{
    if (dataPathList.empty()) {
        return nullptr;
    }

    return createOperatorFactory(
        tpc_ds_data_loader::LoadBenchmarkMetaFromPath(GetRealDataPath(dataPathList[state.range(0)])));
}

std::vector<VectorBatchSupplier> TpcDsOperatorFixture::createVecBatch(const benchmark::State &state)
{
    if (dataPathList.empty()) {
        return {};
    }

    auto metaData = tpc_ds_data_loader::LoadBenchmarkMetaFromPath(GetRealDataPath(dataPathList[state.range(0)]));

    return tpc_ds_data_loader::LoadVectorBatchFromPath(GetRealDataPath(dataPathList[state.range(0)]),
                                                       loadVectorBatchTypes(metaData));
}

void TpcDsOperatorFixture::Initialize()
{
    std::vector<std::string> jsonMetas;
    tpc_ds_data_loader::getJsonMeta(".", jsonMetas, OperatorIdentifier());
    this->dataPathList = jsonMetas;
    for (uint32_t i = 0; i < this->dataPathList.size(); ++i) {
        this->Arg(i);
    }
}

void TpcDsOperatorFixture::SetUp(benchmark::State &state)
{
    if (!dataPathList.empty()) {
        SetUpPath(dataPathList[state.range(0)]);
    }
    BaseOperatorFixture::SetUp(state);
    if (MessageWhenSkip(state).empty()) {
        state.SetLabel("DataPath:" + dataPathList[state.range(0)]);
    }
}

std::string TpcDsOperatorFixture::MessageWhenSkip(const State &state)
{
    return dataPathList.empty() ? "No " + OperatorIdentifier() + " in current path"
                                : BaseOmniFixture::MessageWhenSkip(state);
}
} // namespace om_benchmark
