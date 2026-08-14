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

void SelectiveFloatingPointColumnReader::read(
    uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    const auto typeId = static_cast<type::DataTypeId>(omniTypeId);
    const auto *filter = hasFilter() ? spec_->filter() : nullptr;
    if (filter != nullptr && typeId == type::OMNI_FLOAT) {
        // Float value filters are not implemented yet. Fail before decoding if a future
        // ScanSpec change starts pushing one without adding the matching reader evaluation.
        switch (filter->kind()) {
            case ::common::FilterKind::kAlwaysFalse:
            case ::common::FilterKind::kAlwaysTrue:
            case ::common::FilterKind::kIsNull:
            case ::common::FilterKind::kIsNotNull:
                break;
            default:
                throw omniruntime::exception::OmniException(
                    "EXPRESSION_NOT_SUPPORT",
                    "SelectiveFloatingPointColumnReader unsupported filter kind: " +
                        std::to_string(static_cast<int>(filter->kind())) +
                        ", omniTypeId: " + std::to_string(omniTypeId));
        }
    }

    decoded_ = makeNewVector(
        rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
    inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
    decodedBase_ = 0;

    if (!hasFilter()) {
        return;
    }

    if (typeId != type::OMNI_DOUBLE && typeId != type::OMNI_FLOAT) {
        throw omniruntime::exception::OmniException(
            "EXPRESSION_NOT_SUPPORT",
            "SelectiveFloatingPointColumnReader unsupported omniTypeId: " + std::to_string(omniTypeId));
    }

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

} // namespace omniruntime::reader
