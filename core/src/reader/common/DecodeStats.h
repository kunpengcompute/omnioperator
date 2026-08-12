/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// Build with -DOMNI_SCAN_DECODE_STATS=1 to enable; otherwise zero overhead.

#ifndef OMNI_READER_COMMON_DECODE_STATS_H
#define OMNI_READER_COMMON_DECODE_STATS_H

#include <cstdint>

namespace omniruntime::reader {

#if defined(OMNI_SCAN_DECODE_STATS) && OMNI_SCAN_DECODE_STATS

struct DecodeStats {
    uint64_t rowsVisited = 0;
    uint64_t valuesMaterialized = 0;
    uint64_t nullsVisited = 0;
    uint64_t sliceCalls = 0;
    uint64_t sliceValues = 0;

    void Reset() { *this = DecodeStats(); }
};

#define OMNI_DECODE_STATS_BUMP(statsPtr, field, delta) \
    do {                                              \
        if ((statsPtr) != nullptr) {                  \
            (statsPtr)->field += (delta);             \
        }                                             \
    } while (false)

#else

struct DecodeStats {
    void Reset() {}
};

#define OMNI_DECODE_STATS_BUMP(statsPtr, field, delta) \
    do {                                              \
        (void)(statsPtr);                             \
    } while (false)

#endif

} // namespace omniruntime::reader

#endif // OMNI_READER_COMMON_DECODE_STATS_H
