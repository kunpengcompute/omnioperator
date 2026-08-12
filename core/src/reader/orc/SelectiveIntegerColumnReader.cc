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
#include "util/bit_util.h"
#include "util/omni_exception.h"
#include "vector/unsafe_vector.h"
#include "reader/common/Filter.h"
#include "ColumnVisitor.h"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;

SelectiveIntegerColumnReader::SelectiveIntegerColumnReader(codegen::ScanSpec *spec, const ::orc::Type *orcType,
                                                          std::unique_ptr<::orc::ColumnReader> inner)
    : SelectiveColumnReader(spec, orcType, std::move(inner))
{
    intInner_ = dynamic_cast<OmniIntegerColumnReader *>(inner_);
    if (intInner_ == nullptr) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR",
            "SelectiveIntegerColumnReader requires OmniIntegerColumnReader as inner reader");
    }
}

void SelectiveIntegerColumnReader::EnsureNullsScratch(uint64_t rowsToRead)
{
    // +1 spare word: PRESENT decoder writes whole bytes.
    const size_t words = static_cast<size_t>(BitUtil::Nwords(static_cast<int32_t>(rowsToRead))) + 1;
    if (nullsScratch_.size() < words) {
        nullsScratch_.resize(words);
    }
}

void SelectiveIntegerColumnReader::read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    // Full-batch pure projection: visitor cannot skip anything; bulk next() is faster.
    // Still publish as kDense so getValues can move decoded_ instead of CopyPositions.
    if (!hasFilter() && projectOut() && activeRows.size() == rowsToRead) {
        decoded_ = makeNewVector(rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
        inner_->next(decoded_.get(), rowsToRead, /*incomingNulls*/nullptr, omniTypeId);
        mat_ = Materialization::kDense;
        visitedRows_ = activeRows;
        return;
    }

    int valueBytes = 0;
    switch (static_cast<omniruntime::type::DataTypeId>(omniTypeId)) {
        case omniruntime::type::OMNI_SHORT:
            valueBytes = 2;
            break;
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_DATE32:
            valueBytes = 4;
            break;
        case omniruntime::type::OMNI_LONG:
            valueBytes = 8;
            break;
        default:
            throw omniruntime::exception::OmniException(
                "EXPRESSION_NOT_SUPPORT",
                "SelectiveIntegerColumnReader unsupported omniTypeId: " + std::to_string(omniTypeId));
    }

    EnsureNullsScratch(rowsToRead);
    const bool hasNulls = intInner_->readNullsForBatch(rowsToRead, nullsScratch_.data());

    if (valueBytes == 2) {
        DispatchFilterKind<int16_t>(rowsToRead, activeRows, omniTypeId, hasNulls);
    } else if (valueBytes == 4) {
        DispatchFilterKind<int32_t>(rowsToRead, activeRows, omniTypeId, hasNulls);
    } else {
        DispatchFilterKind<int64_t>(rowsToRead, activeRows, omniTypeId, hasNulls);
    }
}

template <typename T>
void SelectiveIntegerColumnReader::DispatchFilterKind(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId,
                                                     bool hasNulls)
{
    if (!hasFilter()) {
        const auto *always = static_cast<const ::common::AlwaysTrue *>(::common::AlwaysTrue::instance().get());
        RunVisitor<T, ::common::AlwaysTrue>(always, rowsToRead, activeRows, omniTypeId, hasNulls,
                                           /*isFilterColumn=*/false);
        return;
    }

    ::common::Filter *filter = spec_->filter();
    switch (filter->kind()) {
        case ::common::FilterKind::kAlwaysTrue:
            RunVisitor<T, ::common::AlwaysTrue>(filter->as<::common::AlwaysTrue>(), rowsToRead, activeRows,
                                                omniTypeId, hasNulls, true);
            return;
        case ::common::FilterKind::kAlwaysFalse:
            RunVisitor<T, ::common::AlwaysFalse>(filter->as<::common::AlwaysFalse>(), rowsToRead, activeRows,
                                                 omniTypeId, hasNulls, true);
            return;
        case ::common::FilterKind::kIsNull:
            RunVisitor<T, ::common::IsNull>(filter->as<::common::IsNull>(), rowsToRead, activeRows, omniTypeId,
                                            hasNulls, true);
            return;
        case ::common::FilterKind::kIsNotNull:
            RunVisitor<T, ::common::IsNotNull>(filter->as<::common::IsNotNull>(), rowsToRead, activeRows,
                                               omniTypeId, hasNulls, true);
            return;
        case ::common::FilterKind::kBigintRange:
            RunVisitor<T, ::common::BigintRange>(filter->as<::common::BigintRange>(), rowsToRead, activeRows,
                                                 omniTypeId, hasNulls, true);
            return;
        case ::common::FilterKind::kNegatedBigintRange:
            RunVisitor<T, ::common::NegatedBigintRange>(filter->as<::common::NegatedBigintRange>(), rowsToRead,
                                                        activeRows, omniTypeId, hasNulls, true);
            return;
        case ::common::FilterKind::kBigintValuesUsingHashTable:
            RunVisitor<T, ::common::BigintValues>(filter->as<::common::BigintValues>(), rowsToRead, activeRows,
                                                  omniTypeId, hasNulls, true);
            return;
        case ::common::FilterKind::kBigintMultiRange:
            RunVisitor<T, ::common::BigintMultiRange>(filter->as<::common::BigintMultiRange>(), rowsToRead,
                                                       activeRows, omniTypeId, hasNulls, true);
            return;
        default:
            RunVisitor<T, ::common::Filter>(filter, rowsToRead, activeRows, omniTypeId, hasNulls, true);
            return;
    }
}

template <typename T, typename TFilter, bool kExtract, bool hasNulls>
uint32_t SelectiveIntegerColumnReader::Visit(const TFilter *filter, uint64_t rowsToRead, common::RowSet activeRows,
                                             std::vector<common::vector_size_t> *outRows, T *out, uint64_t *outNulls)
{
    IntColumnVisitor<T, TFilter, kExtract> visitor(filter, activeRows, outRows, out, outNulls, &stats_);
    intInner_->dataDecoder()->readWithVisitor<hasNulls>(nullsScratch_.data(), rowsToRead, visitor);
    return visitor.numValues();
}

template <typename T, typename TFilter>
void SelectiveIntegerColumnReader::RunVisitor(const TFilter *filter, uint64_t rowsToRead,
                                              common::RowSet activeRows, int omniTypeId, bool hasNulls,
                                              bool isFilterColumn)
{
    std::vector<common::vector_size_t> *outRows = nullptr;
    if (isFilterColumn) {
        outputRows_.clear();
        outputRows_.reserve(activeRows.size());
        outRows = &outputRows_;
    }

    const bool extract = projectOut();
    T *out = nullptr;
    uint64_t *outNulls = nullptr;
    if (extract) {
        decoded_ = makeNewVector(activeRows.size(), orcType_,
                                 static_cast<omniruntime::type::DataTypeId>(omniTypeId));
        out = vec::unsafe::UnsafeVector::GetRawValues(static_cast<Vector<T> *>(decoded_.get()));
        outNulls = reinterpret_cast<uint64_t *>(vec::unsafe::UnsafeBaseVector::GetNulls(decoded_.get()));
    } else {
        decoded_.reset();
    }

    uint32_t written = 0;
    if (extract) {
        written = hasNulls ? Visit<T, TFilter, true, true>(filter, rowsToRead, activeRows, outRows, out, outNulls)
                           : Visit<T, TFilter, true, false>(filter, rowsToRead, activeRows, outRows, out, outNulls);
    } else if (hasNulls) {
        Visit<T, TFilter, false, true>(filter, rowsToRead, activeRows, outRows, nullptr, nullptr);
    } else {
        Visit<T, TFilter, false, false>(filter, rowsToRead, activeRows, outRows, nullptr, nullptr);
    }

    mat_ = Materialization::kDense;
    visitedRows_ = isFilterColumn ? common::RowSet(outputRows_.data(), outputRows_.size()) : activeRows;

    if (extract) {
        vec::unsafe::UnsafeBaseVector::SetSize(decoded_.get(), static_cast<int>(written));
    }
}

void SelectiveIntegerColumnReader::skipBatch(uint64_t rowsToRead, int omniTypeId)
{
    EnsureNullsScratch(rowsToRead);
    const bool hasNulls = intInner_->readNullsForBatch(rowsToRead, nullsScratch_.data());
    const uint64_t values = hasNulls
        ? OmniRleDecoderV2::CountNonNull(nullsScratch_.data(), 0, rowsToRead)
        : rowsToRead;
    intInner_->dataDecoder()->skipValues(values);
    decoded_.reset();
}

} // namespace omniruntime::reader
