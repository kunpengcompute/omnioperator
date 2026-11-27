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

#include "vector_analyzer.h"

namespace omniruntime {
namespace op {

// 2M entries, i.e. 16MB is the largest array based hash table.
static constexpr uint32_t RANGE_UPPER_BOUND = 2 << 20;
;

template <typename T> bool VectorAnalyzer::CheckArrayMap(Vector<T> *vector, size_t length)
{
    int64_t min = groupbyColsRange.min;
    int64_t max = groupbyColsRange.max;
    auto valuePtr = unsafe::UnsafeVector::GetRawValues(vector);
    int64_t value;
    if (vector->HasNull()) {
        for (size_t i = 0; i < length; i++) {
            if (vector->IsNull(i)) {
                continue;
            }
            value = *(valuePtr + i);
            if (max < value) {
                max = value;
            }
            if (min > value) {
                min = value;
            }
        }
    } else {
        auto pair = std::minmax_element(valuePtr, valuePtr + length);
        min = *(pair.first);
        max = *(pair.second);
    }
    if (max < min) {
        return false;
    }

    bool willOverflow = (max >= 0 && min < max - std::numeric_limits<int64_t>::max());
    if (willOverflow) {
        return false;
    }

    // range length max - min + 1 and null slot
    if (max - min > RANGE_UPPER_BOUND - 2) {
        return false;
    }

    auto reserveSize = std::max(50L, max - min);
    // Update range and expand size
    if (!groupbyColsRange.init || groupbyColsRange.max < max) {
        if (groupbyColsRange.max == std::numeric_limits<T>::max()) {
            return false;
        }
        minMaxChanged = true;
        if (max <= std::numeric_limits<T>::max() - reserveSize) {
            groupbyColsRange.max = max + reserveSize;
        } else {
            groupbyColsRange.max = std::numeric_limits<T>::max();
        }
    }
    if (!groupbyColsRange.init || groupbyColsRange.min > min) {
        if (groupbyColsRange.min == std::numeric_limits<T>::min()) {
            return false;
        }
        minMaxChanged = true;
        if (min >= std::numeric_limits<T>::min() + reserveSize) {
            groupbyColsRange.min = min - reserveSize;
        } else {
            groupbyColsRange.min = std::numeric_limits<T>::min();
        }
    }
    groupbyColsRange.init = true;
    // prevent memcpy 34 errorno
    if (GetRange() > RANGE_UPPER_BOUND) {
        return false;
    }
    return true;
}

bool VectorAnalyzer::DecideHashMode(omniruntime::vec::VectorBatch *&vectorBatch)
{
    // skip analyze value
    if (hashMode == HashTableType::NORMAL_HASH_TABLE) {
        return false;
    }
    if (ConfigUtil::GetAggHashTableRule() == AggHashTableRule::NORMAL) {
        hashMode = HashTableType::NORMAL_HASH_TABLE;
        return false;
    }
    minMaxChanged = false;
    auto groupColNum = static_cast<int32_t>(this->groupByCols.size());
    // current only support one columnar for array mode
    if (groupColNum != 1) {
        hashMode = HashTableType::NORMAL_HASH_TABLE;
        return false;
    }

    bool blChangeMap;
    auto omniId = groupByCols[0].input->GetId();
    int32_t idx = groupByCols[0].idx;
    switch (omniId) {
        case OMNI_INT:
        case OMNI_DATE32: blChangeMap = HandleInputValues<OMNI_INT>(vectorBatch, idx); break;
        case OMNI_BYTE: blChangeMap = HandleInputValues<OMNI_BYTE>(vectorBatch, idx); break;
        case OMNI_SHORT: blChangeMap = HandleInputValues<OMNI_SHORT>(vectorBatch, idx); break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: blChangeMap = HandleInputValues<OMNI_LONG>(vectorBatch, idx); break;
        default:
            hashMode = HashTableType::NORMAL_HASH_TABLE;
            blChangeMap = false;
            break;
    }
    return blChangeMap;
}

template <type::DataTypeId typeId>
bool VectorAnalyzer::HandleInputValues(omniruntime::vec::VectorBatch *&vectorBatch, int32_t idx)
{
    using RawDataType = typename NativeAndVectorType<typeId>::type;
    auto vector = vectorBatch->Get(idx);

    auto rowCount = vectorBatch->GetRowCount();
    if (vector->GetEncoding() != OMNI_DICTIONARY) {
        if (!CheckArrayMap<RawDataType>(static_cast<Vector<RawDataType> *>(vector), rowCount)) {
            hashMode = HashTableType::NORMAL_HASH_TABLE;
            return false;
        }
        return true;
    } else {
        hashMode = HashTableType::NORMAL_HASH_TABLE;
        return false;
    }
}

bool VectorAnalyzer::IsArrayHashTableType() { return hashMode == HashTableType::ARRAY_HASH_TABLE; }

} // namespace op
} // namespace omniruntime
