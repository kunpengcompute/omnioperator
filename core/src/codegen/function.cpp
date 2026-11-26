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
 *
 * Description: Maps a function in function expression to a precompiled function
 */

#include "function.h"

using namespace omniruntime::type;

namespace omniruntime::codegen {
Function::Function(void *address, const std::string &name, const std::vector<std::string> &aliases,
                   const std::vector<DataTypeId> &paramTypes, const DataTypeId &retType,
                   NullableResultType nullableResultType, bool setExecutionContext)
{
    this->address = address;
    this->nullableResultType = nullableResultType;
    this->isExecContextSet = setExecutionContext;
    // create function sig to register for codegen
    this->signatures.emplace_back(name, paramTypes, retType, address);
    // create function sigs for different functions calls in omni-runtime
    for (auto &alias : aliases) {
        this->signatures.emplace_back(alias, paramTypes, retType, address);
    }
}

Function::~Function() = default;

const std::vector<FunctionSignature> &Function::GetSignatures() const { return this->signatures; }

std::string Function::GetId() const { return this->signatures.at(0).ToString(); }

DataTypeId Function::GetReturnType() const { return this->signatures.at(0).GetReturnType(); }

const std::vector<DataTypeId> &Function::GetParamTypes() const { return this->signatures.at(0).GetParams(); }

const void *Function::GetAddress() const { return this->address; }

const NullableResultType Function::GetNullableResultType() const { return this->nullableResultType; }

bool Function::IsExecutionContextSet() const { return this->isExecContextSet; }
} // namespace omniruntime::codegen
