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

#ifndef OMNI_READER_ORC_COLUMN_VISITOR_H
#define OMNI_READER_ORC_COLUMN_VISITOR_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "reader/common/DecodeStats.h"
#include "reader/common/Filter.h"
#include "reader/common/RowSet.h"
#include "util/bit_util.h"
#include "OmniRLEv2.hh"

namespace omniruntime::reader {

// Processes run slices from OmniRleDecoderV2::readWithVisitor.
// TFilter is a template so filter tests inline; kExtract=false skips materialization entirely.
template <typename T, typename TFilter, bool kExtract>
class IntColumnVisitor {
public:
    // outputRows == nullptr: pure projection (every visited row is kept).
    // out / outNulls unused when kExtract is false.
    IntColumnVisitor(const TFilter *filter, common::RowSet rows,
                     std::vector<common::vector_size_t> *outputRows, T *out, uint64_t *outNulls,
                     DecodeStats *stats)
        : filter_(filter),
          rows_(rows),
          outputRows_(outputRows),
          out_(out),
          outNulls_(outNulls),
          stats_(stats),
          nullPasses_(filter->testNull()),
          // RowSet is strictly increasing: span == size-1 means contiguous.
          rowsAreDense_(!rows.empty() &&
                        rows.back() - rows.front() == static_cast<common::vector_size_t>(rows.size()) - 1)
    {}

    bool atEnd() const { return rowIndex_ >= rows_.size(); }

    common::vector_size_t currentRow() const { return rows_[rowIndex_]; }

    uint32_t numValues() const { return numValues_; }

    void processNull()
    {
        OMNI_DECODE_STATS_BUMP(stats_, nullsVisited, 1);
        if (nullPasses_) {
            if (outputRows_ != nullptr) {
                outputRows_->push_back(rows_[rowIndex_]);
            }
            if constexpr (kExtract) {
                BitUtil::SetBit(outNulls_, numValues_);
                ++numValues_;
                OMNI_DECODE_STATS_BUMP(stats_, valuesMaterialized, 1);
            }
        }
        ++rowIndex_;
    }

    // Longest contiguous, null-free prefix of pending rows, clipped to maxRows (current run capacity).
    // Caller guarantees rows_[rowIndex_] is non-null, so result is always >= 1.
    template <bool hasNulls>
    uint64_t denseRunLength(const uint64_t *nulls, uint64_t maxRows) const
    {
        const uint64_t limit = std::min(static_cast<uint64_t>(rows_.size() - rowIndex_), maxRows);

        if (rowsAreDense_) {
            if constexpr (hasNulls) {
                if (limit > 1) {
                    const common::vector_size_t first = rows_[rowIndex_];
                    const int32_t nullAt = BitUtil::FindFirstBit(nulls, first + 1,
                        first + static_cast<int32_t>(limit));
                    if (nullAt >= 0) {
                        return static_cast<uint64_t>(nullAt - first);
                    }
                }
            }
            return limit;
        }

        const common::vector_size_t first = rows_[rowIndex_];
        uint64_t j = 1;
        while (j < limit && rows_[rowIndex_ + j] == first + static_cast<common::vector_size_t>(j)) {
            if (hasNulls && BitUtil::IsBitSet(nulls, rows_[rowIndex_ + j])) {
                break;
            }
            ++j;
        }
        return j;
    }

    // slice.values[0..k) map to rows firstRow..firstRow+k, all non-null and contiguous.
    void processSlice(const OmniRleDecoderV2::RunSlice &slice, uint64_t k, common::vector_size_t firstRow)
    {
        OMNI_DECODE_STATS_BUMP(stats_, sliceCalls, 1);
        OMNI_DECODE_STATS_BUMP(stats_, sliceValues, k);
        OMNI_DECODE_STATS_BUMP(stats_, rowsVisited, k);

        if constexpr (std::is_same_v<TFilter, ::common::AlwaysTrue>) {
            if constexpr (kExtract) {
                if (slice.repeated) {
                    const T v = static_cast<T>(slice.values[0]);
                    for (uint64_t j = 0; j < k; ++j) {
                        out_[numValues_ + j] = v;
                    }
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    std::memcpy(out_ + numValues_, slice.values, k * sizeof(int64_t));
                } else {
                    for (uint64_t j = 0; j < k; ++j) {
                        out_[numValues_ + j] = static_cast<T>(slice.values[j]);
                    }
                }
                numValues_ += static_cast<uint32_t>(k);
                OMNI_DECODE_STATS_BUMP(stats_, valuesMaterialized, k);
            }
            if (outputRows_ != nullptr) {
                for (uint64_t j = 0; j < k; ++j) {
                    outputRows_->push_back(firstRow + static_cast<common::vector_size_t>(j));
                }
            }
        } else if (slice.repeated) {
            if (filter_->testInt64(slice.values[0])) {
                if (outputRows_ != nullptr) {
                    for (uint64_t j = 0; j < k; ++j) {
                        outputRows_->push_back(firstRow + static_cast<common::vector_size_t>(j));
                    }
                }
                if constexpr (kExtract) {
                    const T v = static_cast<T>(slice.values[0]);
                    for (uint64_t j = 0; j < k; ++j) {
                        out_[numValues_ + j] = v;
                    }
                    numValues_ += static_cast<uint32_t>(k);
                    OMNI_DECODE_STATS_BUMP(stats_, valuesMaterialized, k);
                }
            }
        } else {
            for (uint64_t j = 0; j < k; ++j) {
                const int64_t v = slice.values[j];
                if (filter_->testInt64(v)) {
                    if (outputRows_ != nullptr) {
                        outputRows_->push_back(firstRow + static_cast<common::vector_size_t>(j));
                    }
                    if constexpr (kExtract) {
                        out_[numValues_++] = static_cast<T>(v);
                        OMNI_DECODE_STATS_BUMP(stats_, valuesMaterialized, 1);
                    }
                }
            }
        }

        rowIndex_ += static_cast<size_t>(k);
    }

private:
    const TFilter *filter_;
    common::RowSet rows_;
    std::vector<common::vector_size_t> *outputRows_;
    T *out_;
    uint64_t *outNulls_;
    DecodeStats *stats_;
    const bool nullPasses_;
    const bool rowsAreDense_;

    size_t rowIndex_ = 0;
    uint32_t numValues_ = 0;
};

} // namespace omniruntime::reader

#endif // OMNI_READER_ORC_COLUMN_VISITOR_H
