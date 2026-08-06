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
#include "SelectiveBooleanColumnReader.hh"
#include "SelectiveByteColumnReader.hh"
#include "SelectiveDecimalColumnReader.hh"
#include "SelectiveFloatingPointColumnReader.hh"
#include "SelectiveIntegerColumnReader.hh"
#include "SelectiveStringDictionaryColumnReader.hh"
#include "SelectiveStringDirectColumnReader.hh"
#include "SelectiveTimestampColumnReader.hh"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;

namespace {

std::unique_ptr<SelectiveColumnReader> MakeSelectiveChild(codegen::ScanSpec *childSpec, const ::orc::Type *childOrcType,
                                                          ::orc::StripeStreams &stripe,
                                                          common::JulianGregorianRebase *julian)
{
    auto inner = omniBuildReader(*childOrcType, stripe, julian);
    switch (static_cast<int64_t>(childOrcType->getKind())) {
        case ::orc::DATE:
        case ::orc::INT:
        case ::orc::LONG:
        case ::orc::SHORT:
            return std::make_unique<SelectiveIntegerColumnReader>(childSpec, childOrcType, std::move(inner));
        case ::orc::BOOLEAN:
            return std::make_unique<SelectiveBooleanColumnReader>(childSpec, childOrcType, std::move(inner));
        case ::orc::BYTE:
            return std::make_unique<SelectiveByteColumnReader>(childSpec, childOrcType, std::move(inner));
        case ::orc::DOUBLE:
        case ::orc::FLOAT:
            return std::make_unique<SelectiveFloatingPointColumnReader>(
                childSpec, childOrcType, std::move(inner));
        case ::orc::DECIMAL:
            return std::make_unique<SelectiveDecimalColumnReader>(childSpec, childOrcType, std::move(inner));
        case ::orc::TIMESTAMP:
        case ::orc::TIMESTAMP_INSTANT:
            return std::make_unique<SelectiveTimestampColumnReader>(
                childSpec, childOrcType, std::move(inner));
        case ::orc::BINARY:
        case ::orc::STRING:
        case ::orc::VARCHAR:
        case ::orc::CHAR: {
            // BINARY shares ORC's byte-sequence encodings with string types. It supports
            // projection and null-bitmap filters, but not value comparisons in this phase.
            const auto enc = stripe.getEncoding(childOrcType->getColumnId()).kind();
            if (enc == ::orc::proto::ColumnEncoding_Kind_DICTIONARY ||
                enc == ::orc::proto::ColumnEncoding_Kind_DICTIONARY_V2) {
                return std::make_unique<SelectiveStringDictionaryColumnReader>(childSpec, childOrcType,
                                                                               std::move(inner));
            }
            return std::make_unique<SelectiveStringDirectColumnReader>(childSpec, childOrcType, std::move(inner));
        }
        default:
            throw omniruntime::exception::OmniException(
                "EXPRESSION_NOT_SUPPORT",
                "SelectiveStructColumnReader unsupported ORC type kind: " +
                    std::to_string(static_cast<int>(childOrcType->getKind())));
    }
}

} // namespace

SelectiveStructColumnReader::SelectiveStructColumnReader(const ::orc::Type &rootType, ::orc::StripeStreams &stripe,
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

        auto reader = MakeSelectiveChild(childSpec, childOrcType, stripe, julian);
        int idx = static_cast<int>(children_.size());
        if (reader->hasFilter()) {
            filterOrder_.push_back(idx);
        } else {
            projectOrder_.push_back(idx);
        }
        if (childSpec->projectOut()) {
            numOutputChannels_ =
                std::max(numOutputChannels_, static_cast<int>(childSpec->channel()) + 1);
        }
        children_.push_back(std::move(reader));
    }
}

uint64_t SelectiveStructColumnReader::read(uint64_t rowsToRead, std::vector<BaseVector *> &outBatch, int *omniTypeId)
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
