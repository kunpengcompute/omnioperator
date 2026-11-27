/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

std::unique_ptr<::omniruntime::vec::Vector<int32_t>> HashUtil::ComputePartitionIds(
        std::vector<::omniruntime::vec::BaseVector *> &vecs, int32_t partitionNum, int32_t rowCount,
        ::omniruntime::mem::OmniTaskMemoryPool *taskPool)
{
    auto colCount = vecs.size();
    std::vector<uint32_t> partitionIds(rowCount, MM3HASH_SEED);
    for (auto col = 0; col < colCount; col++) {
        switch (vecs[col]->GetTypeId()) {
            case type::OMNI_BYTE:
                Mm3Byte(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_SHORT:
                Mm3Short(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_FLOAT:
                Mm3Float(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_INT:
            case type::OMNI_DATE32:
                Mm3Int(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_DOUBLE:
                Mm3Double(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_LONG:
            case type::OMNI_TIMESTAMP:
            case type::OMNI_DECIMAL64:
                Mm3Long(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_CHAR:
            case type::OMNI_VARCHAR:
                Mm3String(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_DECIMAL128:
                Mm3Decimal128(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_BOOLEAN:
                Mm3Boolean(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_ARRAY:
                Mm3Array(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_MAP:
                Mm3Map(vecs[col], rowCount, partitionIds);
                break;
            case type::OMNI_ROW:
                Mm3Struct(vecs[col], rowCount, partitionIds);
                break;
            default:
                std::string omniExceptionInfo =
                        "Error in shuffle hash, not support type: " +
                        std::to_string(vecs[col]->GetTypeId());
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
    // Use the pool-bound 3-arg ctor when a per-task pool was supplied (production
    // gluten path via the JNI helper). Falls back to the legacy 2-arg ctor for
    // callers that have not been migrated (tests / null-pool case).
    auto ret = (taskPool != nullptr)
        ? std::make_unique<omniruntime::vec::Vector<int>>(rowCount, taskPool, type::OMNI_INT)
        : std::make_unique<omniruntime::vec::Vector<int>>(rowCount, type::OMNI_INT);

    for (int32_t row = 0; row < rowCount; row++) {
        ret->SetValue(row, Pmod(partitionIds[row], partitionNum));
    }
    return ret;
}

}
}
