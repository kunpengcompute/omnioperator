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

#include "SelectiveStringDictionaryColumnReader.hh"

#include "reader/common/Filter.h"
#include "util/omni_exception.h"
#include "vector/dictionary_container.h"
#include "vector/unsafe_vector.h"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;
using omniruntime::vec::DictionaryContainer;
using omniruntime::vec::Vector;
using omniruntime::vec::unsafe::UnsafeDictionaryVector;

namespace {

OmniStringDictionaryColumnReader *AsDictReader(OmniColumnReader *inner)
{
    auto *r = dynamic_cast<OmniStringDictionaryColumnReader *>(inner);
    if (r == nullptr) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR", "SelectiveStringDictionaryColumnReader: inner is not dictionary reader");
    }
    return r;
}

} // namespace

void SelectiveStringDictionaryColumnReader::RebuildAcceptedIds(::common::Filter *filter)
{
    auto *dictReader = AsDictReader(inner_);
    const int32_t n = dictReader->GetOmniDictSize();
    acceptedIds_.assign(static_cast<size_t>(n), 0);
    for (int32_t i = 0; i < n; ++i) {
        auto sv = dictReader->GetOmniDictValue(i);
        acceptedIds_[static_cast<size_t>(i)] =
            filter->testBytes(sv.data(), static_cast<int32_t>(sv.size())) ? 1 : 0;
    }
    acceptedReady_ = true;
}

void SelectiveStringDictionaryColumnReader::read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    auto *dictReader = AsDictReader(inner_);
    decoded_.reset(dictReader->nextAsDictionary(rowsToRead, nullptr, omniTypeId));
    decodedBase_ = 0;

    if (!hasFilter()) {
        return;
    }

    auto *filter = spec_->filter();
    const bool nullOnly =
        filter->is(::common::FilterKind::kIsNull) || filter->is(::common::FilterKind::kIsNotNull);
    // Dictionary is stripe-scoped; this reader is recreated each stripe. Rebuild acceptedIds_
    // only on first use or if the Filter pointer changes; reuse across batches in the stripe.
    if (!nullOnly && (!acceptedReady_ || acceptedForFilter_ != filter)) {
        RebuildAcceptedIds(filter);
        acceptedForFilter_ = filter;
    }

    auto *dv = static_cast<Vector<DictionaryContainer<std::string_view>> *>(decoded_.get());
    const int *ids = nullOnly ? nullptr : UnsafeDictionaryVector::GetIds(dv);

    outputRows_.clear();
    outputRows_.reserve(activeRows.size());
    for (auto row : activeRows) {
        if (decoded_->IsNull(row)) {
            if (filter->testNull()) {
                outputRows_.push_back(row);
            }
            continue;
        }
        if (nullOnly) {
            if (!filter->is(::common::FilterKind::kIsNull)) {
                outputRows_.push_back(row);
            }
            continue;
        }
        const int32_t id = ids[row];
        if (id >= 0 && static_cast<size_t>(id) < acceptedIds_.size() && acceptedIds_[static_cast<size_t>(id)]) {
            outputRows_.push_back(row);
        }
    }
}

BaseVector *SelectiveStringDictionaryColumnReader::getValues(common::RowSet rows)
{
    positions_.clear();
    positions_.reserve(rows.size());
    for (auto r : rows) {
        positions_.push_back(static_cast<int32_t>(r - static_cast<common::vector_size_t>(decodedBase_)));
    }
    return decoded_->CopyPositions(positions_.data(), 0, static_cast<int>(positions_.size()));
}

} // namespace omniruntime::reader
