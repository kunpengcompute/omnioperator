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
 * @Description: hash util implementations
 */

#include "hash_util.h"
#include <cmath>
#include "operator/util/mm3_util.h"

namespace omniruntime {
namespace op {

uint64_t HashUtil::NextPowerOfTwo(uint64_t x)
{
    if (x == 0) {
        return 1;
    } else {
        --x;
        x |= x >> ROTATE_DISTANCE_1;
        x |= x >> ROTATE_DISTANCE_2;
        x |= x >> ROTATE_DISTANCE_4;
        x |= x >> ROTATE_DISTANCE_8;
        x |= x >> ROTATE_DISTANCE_16;
        return (x | (x >> ROTATE_DISTANCE_32)) + 1;
    }
}

uint32_t HashUtil::HashArraySize(uint32_t expected, float f)
{
    double result = static_cast<double>(expected) / static_cast<double>(f);
    auto s = static_cast<uint64_t>(std::ceil(result));
    s = NextPowerOfTwo(s);
    if (s > MAX_ARRAY_SIZE) {
        return expected;
    } else {
        return static_cast<uint32_t>(s);
    }
}

std::unique_ptr<omniruntime::vec::Vector<int32_t>>
HashUtil::ComputePartitionIds(std::vector<omniruntime::vec::BaseVector *> &vecs, int32_t partitionNum, int32_t rowCount)
{
    auto colCount = vecs.size();
    std::vector<uint32_t> partitionIds(rowCount, MM3HASH_SEED);
    for (auto col = 0; col < colCount; col++) {
        switch (vecs[col]->GetTypeId()) {
            case type::OMNI_BYTE:
            case type::OMNI_SHORT:
            case type::OMNI_INT:
            case type::OMNI_DATE32: Mm3Int(vecs[col], rowCount, partitionIds); break;
            case type::OMNI_LONG:
            case type::OMNI_TIMESTAMP:
            case type::OMNI_DECIMAL64:
            case type::OMNI_DOUBLE: Mm3Long(vecs[col], rowCount, partitionIds); break;
            case type::OMNI_CHAR:
            case type::OMNI_VARCHAR: Mm3String(vecs[col], rowCount, partitionIds); break;
            case type::OMNI_DECIMAL128: Mm3Decimal128(vecs[col], rowCount, partitionIds); break;
            case type::OMNI_BOOLEAN: Mm3Boolean(vecs[col], rowCount, partitionIds); break;
            default:
                std::string omniExceptionInfo =
                    "Error in shuffle hash, not support type: " + std::to_string(vecs[col]->GetTypeId());
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
    auto ret = std::make_unique<omniruntime::vec::Vector<int>>(rowCount, type::OMNI_INT);

    for (int32_t row = 0; row < rowCount; row++) {
        ret->SetValue(row, Pmod(partitionIds[row], partitionNum));
    }
    return ret;
}

} // namespace op
} // namespace omniruntime
