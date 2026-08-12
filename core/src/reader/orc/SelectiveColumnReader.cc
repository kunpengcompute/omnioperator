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

#include "SelectiveColumnReader.hh"

#include "util/omni_exception.h"

namespace omniruntime::reader {

vec::BaseVector *SelectiveColumnReader::getValues(common::RowSet rows)
{
    if (decoded_ == nullptr) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR",
            "SelectiveColumnReader::getValues called without a decoded batch (called twice?)");
    }

    if (mat_ == Materialization::kBatchIndexed) {
        positions_.clear();
        positions_.reserve(rows.size());
        for (auto r : rows) {
            positions_.push_back(static_cast<int32_t>(r - static_cast<common::vector_size_t>(decodedBase_)));
        }
        return decoded_->CopyPositions(positions_.data(), 0, static_cast<int>(positions_.size()));
    }

    // kDense common case: rows == visitedRows_, zero-copy.
    if (rows.size() == visitedRows_.size()) {
        return decoded_.release();
    }

    // rows is a strict subset of visitedRows_ (later filters shrank the set). Merge both
    // ascending sequences to recover dense indices.
    positions_.clear();
    positions_.reserve(rows.size());
    size_t j = 0;
    const size_t visited = visitedRows_.size();
    for (auto r : rows) {
        while (j < visited && visitedRows_[j] < r) {
            ++j;
        }
        if (j >= visited || visitedRows_[j] != r) {
            throw omniruntime::exception::OmniException(
                "OPERATOR_RUNTIME_ERROR",
                "SelectiveColumnReader::getValues: requested row " + std::to_string(r) +
                    " was never materialized");
        }
        positions_.push_back(static_cast<int32_t>(j));
        ++j;
    }
    return decoded_->CopyPositions(positions_.data(), 0, static_cast<int>(positions_.size()));
}

} // namespace omniruntime::reader
