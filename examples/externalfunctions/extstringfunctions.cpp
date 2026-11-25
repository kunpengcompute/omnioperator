/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "extstringfunctions.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

using namespace std;

extern "C" DLLEXPORT int32_t LengthStr(int64_t s)
{
    auto tmps = s;
    void *sAddr = &s;
    auto cs = static_cast<uint8_t **>(sAddr);
    string str(*cs);
    return str.size();
}

extern "C" DLLEXPORT int32_t FirstCharStr(int64_t s)
{
    auto tmps = s;
    void *sAddr = &s;
    auto cs = static_cast<uint8_t **>(sAddr);
    return (*cs)[0];
}
