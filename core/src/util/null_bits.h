/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: NullBits compact encoding for mixed row storage
 * 
 * NullBits uses a compact bitmap to encode NULL flags for key/state columns.
 * Layout: each byte encodes 8 columns' nullness (bit 0-7).
 * Total bytes = ceil(numColumns / 8)
 */

#ifndef OMNI_RUNTIME_NULL_BITS_H
#define OMNI_RUNTIME_NULL_BITS_H

#include <cstdint>
#include <cstring>

namespace omniruntime::util {

class NullBits {
public:
    static int32_t NumBytes(int32_t numColumns) {
        return (numColumns + 7) / 8;
    }
    
    static void SetNull(uint8_t* nullBits, int32_t colIdx) {
        int32_t byteIdx = colIdx / 8;
        int32_t bitIdx = colIdx % 8;
        nullBits[byteIdx] |= (1 << bitIdx);
    }
    
    static void ClearNull(uint8_t* nullBits, int32_t colIdx) {
        int32_t byteIdx = colIdx / 8;
        int32_t bitIdx = colIdx % 8;
        nullBits[byteIdx] &= ~(1 << bitIdx);
    }
    
    static bool IsNull(const uint8_t* nullBits, int32_t colIdx) {
        int32_t byteIdx = colIdx / 8;
        int32_t bitIdx = colIdx % 8;
        return (nullBits[byteIdx] & (1 << bitIdx)) != 0;
    }
    
    static void Init(uint8_t* nullBits, int32_t numBytes) {
        memset(nullBits, 0, numBytes);
    }
};

}  // namespace omniruntime::util

#endif  // OMNI_RUNTIME_NULL_BITS_H