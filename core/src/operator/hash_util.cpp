/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * @Description: hash util implementations
 */

#include "hash_util.h"
#include <cmath>
#include "operator/util/mm3_util.h"
#include "util/null_bits.h"
#include "vector/mixed_vector.h"
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

std::unique_ptr<omniruntime::vec::Vector<int32_t>> HashUtil::ComputePartitionIds(
        std::vector<omniruntime::vec::BaseVector *> &vecs, int32_t partitionNum, int32_t rowCount)
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
            case type::OMNI_VARBINARY:
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
    auto ret = std::make_unique<omniruntime::vec::Vector<int>>(rowCount, type::OMNI_INT);

    for (int32_t row = 0; row < rowCount; row++) {
        ret->SetValue(row, Pmod(partitionIds[row], partitionNum));
    }
    return ret;
}

std::unique_ptr<omniruntime::vec::Vector<int32_t>> HashUtil::ComputePartitionIdsMixed(
        omniruntime::vec::MixedVectorBatch &mixedBatch, int32_t partitionNum, int32_t rowCount)
{
    auto ret = std::make_unique<omniruntime::vec::Vector<int>>(rowCount, type::OMNI_INT);
    auto rowTypes = mixedBatch.GetRowTypes();
    int32_t varcharSlotOff = mixedBatch.GetVarcharSlotOffset();
    bool hasMergedVarchar = (varcharSlotOff >= 0);

    // 整段 Mm3 hash（累积式，替代逐列 hash），把 key 部分当成连续字节串计算
    for (int32_t row = 0; row < rowCount; row++) {
        auto rowInfo = mixedBatch.GetRow(row);
        if (rowInfo == nullptr || rowInfo->data == nullptr || rowInfo->length <= 0) {
            ret->SetValue(row, Pmod(static_cast<int32_t>(MM3HASH_SEED), partitionNum));
            continue;
        }

        uint32_t hash = MM3HASH_SEED;
        if (hasMergedVarchar) {
            // merged: key = [0, stateOffset) + [slotDataOff, slotDataOff+varcharTotalSz)
            // 两段用 Mm3 累积式 hash（第二段以第一段结果为 seed），无需 FastHashMix
            int32_t fixedEnd = rowInfo->stateOffset > rowInfo->length ? rowInfo->length : rowInfo->stateOffset;
            if (fixedEnd > 0) {
                hash = HashUnsafeBytes(
                    reinterpret_cast<char*>(const_cast<uint8_t*>(rowInfo->data)), fixedEnd, hash);
            }
            int32_t slotDataOff = 0;
            int32_t varcharTotalSz = 0;
            if (varcharSlotOff >= 0 &&
                static_cast<int64_t>(varcharSlotOff) + 2 * static_cast<int64_t>(sizeof(int32_t)) <= rowInfo->length) {
                varcharTotalSz = *reinterpret_cast<const int32_t*>(rowInfo->data + varcharSlotOff);
                slotDataOff = *reinterpret_cast<const int32_t*>(rowInfo->data + varcharSlotOff + sizeof(int32_t));
            }
            if (varcharTotalSz > 0 && slotDataOff >= 0 &&
                static_cast<int64_t>(slotDataOff) + static_cast<int64_t>(varcharTotalSz) <= rowInfo->length) {
                hash = HashUnsafeBytes(
                    reinterpret_cast<char*>(const_cast<uint8_t*>(rowInfo->data + slotDataOff)), varcharTotalSz, hash);
            }
        } else {
            // non-merged: key = [0, keyLength)，连续
            int32_t keyLen = rowInfo->keyLength > rowInfo->length ? rowInfo->length : rowInfo->keyLength;
            if (keyLen > 0) {
                hash = HashUnsafeBytes(
                    reinterpret_cast<char*>(const_cast<uint8_t*>(rowInfo->data)), keyLen, hash);
            }
        }
        ret->SetValue(row, Pmod(static_cast<int32_t>(hash), partitionNum));
    }
    return ret;
}

}
}
