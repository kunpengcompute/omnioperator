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

#include "SelectiveStructColumnReader.hh"

#include <numeric>

#include "util/omni_exception.h"
#include "SelectiveIntegerColumnReader.hh"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;

SelectiveStructColumnReader::SelectiveStructColumnReader(const ::orc::Type &rootType,
                                                         ::orc::StripeStreams &stripe,
                                                         codegen::ScanSpec *rootSpec,
                                                         common::JulianGregorianRebase *julian)
{
    // Columns aligned by index i: getSelectedType subtype / omniTypeId[i] / ScanSpec children[i].
    const uint64_t n = rootType.getSubtypeCount();
    const auto &specChildren = rootSpec->children();
    for (uint64_t i = 0; i < n; ++i) {
        const ::orc::Type *childOrcType = rootType.getSubtype(i);
        codegen::ScanSpec *childSpec = (i < specChildren.size()) ? specChildren[i].get() : nullptr;
        if (childSpec == nullptr) {
            throw omniruntime::exception::OmniException(
                "OPERATOR_RUNTIME_ERROR",
                "SelectiveStructColumnReader: ScanSpec child missing at index " + std::to_string(i));
        }

        auto inner = omniBuildReader(*childOrcType, stripe, julian);
        auto reader = std::make_unique<SelectiveIntegerColumnReader>(childSpec, childOrcType, std::move(inner));

        int idx = static_cast<int>(children_.size());
        if (reader->hasFilter()) {
            filterOrder_.push_back(idx);
        } else {
            projectOrder_.push_back(idx);
        }
        if (childSpec->projectOut()) {
            numOutputChannels_ = std::max(numOutputChannels_,
                                          static_cast<int>(childSpec->channel()) + 1);
        }
        children_.push_back(std::move(reader));
    }
}

uint64_t SelectiveStructColumnReader::read(uint64_t rowsToRead,
                                           std::vector<BaseVector *> &outBatch,
                                           int *omniTypeId)
{
    active_.resize(rowsToRead);
    std::iota(active_.begin(), active_.end(), 0);
    common::RowSet rows(active_.data(), active_.size());

    size_t fi = 0;
    for (; fi < filterOrder_.size(); ++fi) {
        int k = filterOrder_[fi];
        children_[k]->read(rowsToRead, rows, omniTypeId[k]);
        const auto &out = children_[k]->outputRows();
        rows = common::RowSet(out.data(), out.size());
        if (rows.empty()) {
            ++fi;
            break;
        }
    }

    survivors_.assign(rows.begin(), rows.end());
    common::RowSet survivors(survivors_.data(), survivors_.size());

    for (size_t j = fi; j < filterOrder_.size(); ++j) {
        children_[filterOrder_[j]]->skipBatch(rowsToRead, omniTypeId[filterOrder_[j]]);
    }

    if (survivors.empty()) {
        for (int k : projectOrder_) {
            children_[k]->skipBatch(rowsToRead, omniTypeId[k]);
        }
        outBatch.clear();
        return 0;
    }

    for (int k : projectOrder_) {
        children_[k]->read(rowsToRead, survivors, omniTypeId[k]);
    }

    outBatch.assign(numOutputChannels_, nullptr);
    for (auto &child : children_) {
        if (child->projectOut()) {
            outBatch[child->channel()] = child->getValues(survivors);
        }
    }
    return survivors.size();
}

void SelectiveStructColumnReader::seekToRowGroup(
    std::unordered_map<uint64_t, ::orc::PositionProvider> &positions)
{
    for (auto &child : children_) {
        child->seekToRowGroup(positions);
    }
}

} // namespace omniruntime::reader
