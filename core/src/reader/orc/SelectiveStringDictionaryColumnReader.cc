/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "SelectiveStringDictionaryColumnReader.hh"

#include "ColumnVisitor.h"
#include "reader/common/Filter.h"
#include "util/bit_util.h"
#include "util/omni_exception.h"
#include "vector/dictionary_container.h"
#include "vector/unsafe_vector.h"

namespace omniruntime::reader {

using omniruntime::vec::DictionaryContainer;
using omniruntime::vec::NullsBuffer;
using omniruntime::vec::Vector;

SelectiveStringDictionaryColumnReader::SelectiveStringDictionaryColumnReader(
    codegen::ScanSpec *spec, const ::orc::Type *orcType, std::unique_ptr<::orc::ColumnReader> inner)
    : SelectiveColumnReader(spec, orcType, std::move(inner))
{
    dictInner_ = dynamic_cast<OmniStringDictionaryColumnReader *>(inner_);
    if (dictInner_ == nullptr) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR",
            "SelectiveStringDictionaryColumnReader requires OmniStringDictionaryColumnReader as inner");
    }
}

void SelectiveStringDictionaryColumnReader::RejectUnsupportedValueFilter() const
{
    if (!hasFilter()) {
        return;
    }
    const auto kind = spec_->filter()->kind();
    switch (kind) {
        case ::common::FilterKind::kAlwaysFalse:
        case ::common::FilterKind::kAlwaysTrue:
        case ::common::FilterKind::kIsNull:
        case ::common::FilterKind::kIsNotNull:
            return;
        default:
            throw omniruntime::exception::OmniException(
                "EXPRESSION_NOT_SUPPORT",
                "SelectiveStringDictionaryColumnReader unsupported value filter kind: " +
                    std::to_string(static_cast<int>(kind)) +
                    " (string comparisons are not pushed from Gluten yet)");
    }
}

void SelectiveStringDictionaryColumnReader::EnsureNullsScratch(uint64_t rowsToRead)
{
    const size_t words = static_cast<size_t>(BitUtil::Nwords(static_cast<int32_t>(rowsToRead))) + 1;
    if (nullsScratch_.size() < words) {
        nullsScratch_.resize(words);
    }
}

void SelectiveStringDictionaryColumnReader::RebuildAcceptedIds(::common::Filter *filter)
{
    const int32_t n = dictInner_->GetOmniDictSize();
    acceptedIds_.assign(static_cast<size_t>(n), 0);
    for (int32_t i = 0; i < n; ++i) {
        auto sv = dictInner_->GetOmniDictValue(i);
        acceptedIds_[static_cast<size_t>(i)] =
            filter->testBytes(sv.data(), static_cast<int32_t>(sv.size())) ? 1 : 0;
    }
    acceptedReady_ = true;
}

void SelectiveStringDictionaryColumnReader::ReadProjectionDense(uint64_t rowsToRead, common::RowSet activeRows,
                                                               int omniTypeId)
{
    if (activeRows.empty()) {
        skipBatch(rowsToRead, omniTypeId);
        mat_ = Materialization::kDense;
        visitedRows_ = activeRows;
        return;
    }

    // Full-batch projection: nextAsDictionary is fine; publish kDense for zero-copy getValues.
    if (projectOut() && activeRows.size() == rowsToRead) {
        decoded_.reset(dictInner_->nextAsDictionary(rowsToRead, nullptr, omniTypeId));
        mat_ = Materialization::kDense;
        visitedRows_ = activeRows;
        return;
    }

    EnsureNullsScratch(rowsToRead);
    const bool hasNulls = dictInner_->readNullsForBatch(rowsToRead, nullsScratch_.data());

    const int32_t n = static_cast<int32_t>(activeRows.size());
    std::vector<int32_t> indices(static_cast<size_t>(n), 0);
    auto nullsBuf = std::make_unique<NullsBuffer>(n);
    auto *outNulls = nullsBuf->GetNulls();

    const auto *always =
        static_cast<const ::common::AlwaysTrue *>(::common::AlwaysTrue::instance().get());
    IntColumnVisitor<int32_t, ::common::AlwaysTrue, /*kExtract=*/true> visitor(
        always, activeRows, /*outputRows=*/nullptr, indices.data(), outNulls, /*stats=*/nullptr);

    if (hasNulls) {
        dictInner_->dataDecoder()->readWithVisitor</*hasNulls=*/true>(nullsScratch_.data(), rowsToRead, visitor);
    } else {
        dictInner_->dataDecoder()->readWithVisitor</*hasNulls=*/false>(nullptr, rowsToRead, visitor);
    }

    const uint32_t written = visitor.numValues();
    if (static_cast<int32_t>(written) != n) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR",
            "SelectiveStringDictionaryColumnReader: extracted " + std::to_string(written) + " values, expected " +
                std::to_string(n));
    }

    const int32_t dictSize = dictInner_->GetOmniDictSize();
    for (int32_t i = 0; i < n; ++i) {
        if (BitUtil::IsBitSet(outNulls, i)) {
            continue;
        }
        if (indices[static_cast<size_t>(i)] < 0 || indices[static_cast<size_t>(i)] >= dictSize) {
            throw ::orc::ParseError("Entry index out of range in SelectiveStringDictionaryColumnReader");
        }
    }

    auto container = std::make_shared<DictionaryContainer<std::string_view>>(
        indices, n, dictInner_->GetOmniDict(), dictSize);
    decoded_.reset(new Vector<DictionaryContainer<std::string_view>>(
        n, container, nullsBuf.get(), static_cast<omniruntime::type::DataTypeId>(omniTypeId)));
    mat_ = Materialization::kDense;
    visitedRows_ = activeRows;
}

void SelectiveStringDictionaryColumnReader::read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    RejectUnsupportedValueFilter();

    if (!hasFilter()) {
        ReadProjectionDense(rowsToRead, activeRows, omniTypeId);
        return;
    }

    // Null-only / Always* filters: full-batch nextAsDictionary + post-filter
    // (Gluten may still push IS NULL onto string cols).
    decoded_.reset(dictInner_->nextAsDictionary(rowsToRead, nullptr, omniTypeId));
    decodedBase_ = 0;
    mat_ = Materialization::kBatchIndexed;

    auto *filter = spec_->filter();
    const bool nullOnly =
        filter->is(::common::FilterKind::kIsNull) || filter->is(::common::FilterKind::kIsNotNull);
    if (!nullOnly && (!acceptedReady_ || acceptedForFilter_ != filter)) {
        RebuildAcceptedIds(filter);
        acceptedForFilter_ = filter;
    }

    auto *dv = static_cast<Vector<DictionaryContainer<std::string_view>> *>(decoded_.get());
    const int *ids = nullOnly ? nullptr : vec::unsafe::UnsafeDictionaryVector::GetIds(dv);

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
            if (filter->testNonNull()) {
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

void SelectiveStringDictionaryColumnReader::skipBatch(uint64_t rowsToRead, int omniTypeId)
{
    (void)omniTypeId;
    EnsureNullsScratch(rowsToRead);
    const bool hasNulls = dictInner_->readNullsForBatch(rowsToRead, nullsScratch_.data());
    const uint64_t values = hasNulls ? OmniRleDecoderV2::CountNonNull(nullsScratch_.data(), 0, rowsToRead) : rowsToRead;
    dictInner_->dataDecoder()->skipValues(values);
    decoded_.reset();
}

} // namespace omniruntime::reader
