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

#include "SelectiveFloatingPointColumnReader.hh"

#include "reader/common/Filter.h"
#include "util/omni_exception.h"
#include "vector/vector.h"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;

void SelectiveFloatingPointColumnReader::read(
    uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    decoded_ = makeNewVector(
        rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
    inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
    decodedBase_ = 0;

    if (!hasFilter()) {
        return;
    }

    const auto typeId = static_cast<type::DataTypeId>(omniTypeId);
    if (typeId != type::OMNI_DOUBLE && typeId != type::OMNI_FLOAT) {
        throw omniruntime::exception::OmniException(
            "EXPRESSION_NOT_SUPPORT",
            "SelectiveFloatingPointColumnReader unsupported omniTypeId: " + std::to_string(omniTypeId));
    }

    const auto *filter = spec_->filter();
    outputRows_.clear();
    outputRows_.reserve(activeRows.size());
    for (auto row : activeRows) {
        if (decoded_->IsNull(row)) {
            if (filter->testNull()) {
                outputRows_.push_back(row);
            }
            continue;
        }
        const bool passed = typeId == type::OMNI_DOUBLE
            ? filter->testDouble(static_cast<vec::Vector<double> *>(decoded_.get())->GetValue(row))
            : filter->testFloat(static_cast<vec::Vector<float> *>(decoded_.get())->GetValue(row));
        if (passed) {
            outputRows_.push_back(row);
        }
    }
}

BaseVector *SelectiveFloatingPointColumnReader::getValues(common::RowSet rows)
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
