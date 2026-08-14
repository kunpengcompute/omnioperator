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

#include "SelectiveTimestampColumnReader.hh"

#include "reader/common/Filter.h"
#include "util/omni_exception.h"

namespace omniruntime::reader {

void SelectiveTimestampColumnReader::read(
    uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    const auto *filter = hasFilter() ? spec_->filter() : nullptr;
    bool acceptsNull = false;
    bool acceptsNonNull = false;
    if (filter != nullptr) {
        // Timestamp value filters are not implemented yet. Fail before decoding if a future
        // ScanSpec change starts pushing one without adding the matching reader evaluation.
        switch (filter->kind()) {
            case ::common::FilterKind::kAlwaysFalse:
                break;
            case ::common::FilterKind::kAlwaysTrue:
                acceptsNull = true;
                acceptsNonNull = true;
                break;
            case ::common::FilterKind::kIsNull:
                acceptsNull = true;
                break;
            case ::common::FilterKind::kIsNotNull:
                acceptsNonNull = true;
                break;
            default:
                throw omniruntime::exception::OmniException(
                    "EXPRESSION_NOT_SUPPORT",
                    "SelectiveTimestampColumnReader unsupported filter kind: " +
                        std::to_string(static_cast<int>(filter->kind())) +
                        ", omniTypeId: " + std::to_string(omniTypeId));
        }
    }

    decoded_ = makeNewVector(
        rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
    inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
    decodedBase_ = 0;

    if (filter == nullptr) {
        return;
    }

    outputRows_.clear();
    outputRows_.reserve(activeRows.size());
    for (auto row : activeRows) {
        if (decoded_->IsNull(row)) {
            if (acceptsNull) {
                outputRows_.push_back(row);
            }
            continue;
        }
        if (acceptsNonNull) {
            outputRows_.push_back(row);
        }
    }
}

} // namespace omniruntime::reader
