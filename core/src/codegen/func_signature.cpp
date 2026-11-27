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

/*
 * Description:
 */

#include <utility>
#include "util/type_util.h"
#include "func_signature.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;

FunctionSignature::FunctionSignature() = default;

FunctionSignature::FunctionSignature(const std::string &name, std::vector<DataTypeId> params,
                                     const omniruntime::type::DataTypeId &returnType, void *address)
    : funcName(name), paramTypes(std::move(params)), retType(returnType), funcAddress(address)
{
}

// Copy constructor
FunctionSignature::FunctionSignature(const FunctionSignature &fs)
    : funcName(fs.funcName), paramTypes(fs.paramTypes), retType(fs.retType), funcAddress(fs.funcAddress)
{
}

FunctionSignature::~FunctionSignature() = default;

std::string FunctionSignature::GetName() const { return this->funcName; }

const std::vector<DataTypeId> &FunctionSignature::GetParams() const { return this->paramTypes; }

DataTypeId FunctionSignature::GetReturnType() const { return this->retType; }

void *FunctionSignature::GetFunctionAddress() const { return this->funcAddress; }

FunctionSignature &FunctionSignature::operator=(FunctionSignature other)
{
    std::swap(funcName, other.funcName);
    std::swap(paramTypes, other.paramTypes);
    std::swap(retType, other.retType);
    std::swap(funcAddress, other.funcAddress);
    return *this;
}

bool FunctionSignature::operator==(const FunctionSignature &other) const
{
    if (this->funcName != other.funcName || this->retType != other.retType ||
        this->paramTypes.size() != other.paramTypes.size()) {
        return false;
    }

    for (uint32_t i = 0; i < this->paramTypes.size(); i++) {
        if (this->paramTypes.at(i) != other.paramTypes.at(i)) {
            return false;
        }
    }
    return true;
}

size_t FunctionSignature::HashCode() const
{
    auto hashName = std::hash<std::string>{}(this->funcName);
    auto hashReturnType = std::hash<int>{}(static_cast<int>(this->retType));
    auto combinedHash = hashName ^ (hashReturnType << 1);
    for (auto param : this->paramTypes) {
        auto hashParamType = std::hash<int>{}(static_cast<int>(param));
        combinedHash = hashParamType ^ (combinedHash << 1);
    }
    return combinedHash;
}

std::string FunctionSignature::ToString() const
{
    auto result = this->funcName;
    for (auto const &param : this->paramTypes) {
        result += "_";
        result += TypeUtil::TypeToString(param);
    }
    result = result + "_" + TypeUtil::TypeToString(this->retType);
    return result;
}

std::string FunctionSignature::ToString(omniruntime::op::OverflowConfig *overflowConfig) const
{
    auto result = this->funcName;
    if (overflowConfig != nullptr && overflowConfig->GetOverflowConfigId() == omniruntime::op::OVERFLOW_CONFIG_NULL) {
        result += "_null";
    }
    for (auto const &param : this->paramTypes) {
        result += "_";
        result += TypeUtil::TypeToString(param);
    }
    result = result + "_" + TypeUtil::TypeToString(this->retType);
    return result;
}
} // namespace omniruntime::codegen
