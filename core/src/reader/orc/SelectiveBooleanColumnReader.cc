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

#include "SelectiveBooleanColumnReader.hh"

#include "reader/common/Filter.h"
#include "vector/vector.h"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;

void SelectiveBooleanColumnReader::read(
    uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    decoded_ = makeNewVector(
        rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
    inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
    decodedBase_ = 0;

    if (!hasFilter()) {
        return;
    }

    const auto *filter = spec_->filter();
    const auto *values = static_cast<vec::Vector<bool> *>(decoded_.get());
    outputRows_.clear();
    outputRows_.reserve(activeRows.size());
    for (auto row : activeRows) {
        if (decoded_->IsNull(row)) {
            if (filter->testNull()) {
                outputRows_.push_back(row);
            }
            continue;
        }
        if (filter->testBool(values->GetValue(row))) {
            outputRows_.push_back(row);
        }
    }
}

BaseVector *SelectiveBooleanColumnReader::getValues(common::RowSet rows)
{
    positions_.clear();
    positions_.reserve(rows.size());
    for (auto row : rows) {
        positions_.push_back(
            static_cast<int32_t>(row - static_cast<common::vector_size_t>(decodedBase_)));
    }
    return decoded_->CopyPositions(positions_.data(), 0, static_cast<int>(positions_.size()));
}

} // namespace omniruntime::reader
