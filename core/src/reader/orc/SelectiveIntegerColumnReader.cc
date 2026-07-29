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

#include "SelectiveIntegerColumnReader.hh"

#include "type/data_type.h"
#include "util/omni_exception.h"
#include "reader/common/Filter.h"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;

int64_t SelectiveIntegerColumnReader::ReadInt64(BaseVector *v, int32_t row, int omniTypeId)
{
    switch (static_cast<omniruntime::type::DataTypeId>(omniTypeId)) {
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_DATE32:
            return static_cast<Vector<int32_t> *>(v)->GetValue(row);
        case omniruntime::type::OMNI_LONG:
            return static_cast<Vector<int64_t> *>(v)->GetValue(row);
        case omniruntime::type::OMNI_SHORT:
            return static_cast<Vector<int16_t> *>(v)->GetValue(row);
        default:
            // T0 only supports the int family; other types must not reach here (capability gate).
            throw omniruntime::exception::OmniException(
                "EXPRESSION_NOT_SUPPORT",
                "SelectiveIntegerColumnReader unsupported omniTypeId: " + std::to_string(omniTypeId));
    }
}

void SelectiveIntegerColumnReader::read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    if (hasFilter()) {
        // ---- Filter column: full-batch decode (reuses existing decode; advances rowsToRead) ----
        decoded_ = makeNewVector(rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
        inner_->next(decoded_.get(), rowsToRead, /*incomingNulls*/nullptr, omniTypeId);
        decodedBase_ = 0;

        // Apply Filter only on activeRows and shrink to survivors.
        auto *filter = spec_->filter();
        outputRows_.clear();
        outputRows_.reserve(activeRows.size());
        for (auto row : activeRows) {
            if (decoded_->IsNull(row)) {
                if (filter->testNull()) {
                    outputRows_.push_back(row);
                }
                continue;
            }
            if (filter->testInt64(ReadInt64(decoded_.get(), row, omniTypeId))) {
                outputRows_.push_back(row);
            }
        }
    } else {
        // ---- Projection column: full-batch decode; getValues compacts by survivors ----
        // Do not use "range decode + inner_->skip()": OmniRleDecoderV2 overrides next() but
        // inherits ::orc::RleDecoderV2::skip(); their run-length remainders are not shared, so
        // interleaving desynchronizes the data stream ("bad read in RleDecoderV2::readByte").
        // Full-batch next() stays on Omni's own decode path and is self-consistent; range decode
        // does not save RLE byte work either, so full decode is equally cheap and safer.
        decoded_ = makeNewVector(rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
        inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
        decodedBase_ = 0;
    }
}

vec::BaseVector *SelectiveIntegerColumnReader::getValues(common::RowSet rows)
{
    // Compact decoded_ by survivors: row r → index (r - decodedBase_) inside decoded_.
    positions_.clear();
    positions_.reserve(rows.size());
    for (auto r : rows) {
        positions_.push_back(static_cast<int32_t>(r - static_cast<common::vector_size_t>(decodedBase_)));
    }
    return decoded_->CopyPositions(positions_.data(), 0, static_cast<int>(positions_.size()));
}

} // namespace omniruntime::reader
