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

#include "SelectiveByteColumnReader.hh"

#include "reader/common/Filter.h"
#include "vector/vector.h"

namespace omniruntime::reader {

void SelectiveByteColumnReader::read(
    uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    decoded_ = makeNewVector(
        rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId), taskPool_);
    inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
    decodedBase_ = 0;
    mat_ = Materialization::kBatchIndexed;

    if (!hasFilter()) {
        // Whole batch decoded and every row still active, so decoded_[i] is row activeRows[i].
        // Publishing kDense lets getValues move decoded_ instead of gathering an identical copy.
        if (activeRows.size() == rowsToRead) {
            mat_ = Materialization::kDense;
            visitedRows_ = activeRows;
        }
        return;
    }

    const auto *filter = spec_->filter();
    const auto *values = static_cast<vec::Vector<int8_t> *>(decoded_.get());
    outputRows_.clear();
    outputRows_.reserve(activeRows.size());
    for (auto row : activeRows) {
        if (decoded_->IsNull(row)) {
            if (filter->testNull()) {
                outputRows_.push_back(row);
            }
            continue;
        }
        if (filter->testInt64(values->GetValue(row))) {
            outputRows_.push_back(row);
        }
    }
}

} // namespace omniruntime::reader
