/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: registry  function  implementation
 */
#include "mightcontain.h"
#include "codegen/bloom_filter.h"

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT bool MightContain(int64_t bloomFilterAddr, int64_t hashValue, bool isNull)
{
    /*
     * Limited by the current processing framework, bloomFilterAddr is set to null by the engine when the value of
     * bloomFilterAddr is 0.
     */
    if (!isNull) {
        auto bloomFilter = reinterpret_cast<omniruntime::op::BloomFilter *>(bloomFilterAddr);
        return bloomFilter->MightContainLong(hashValue);
    }
    return false;
}
} // namespace omniruntime::codegen::function
