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

#include "SelectiveStringDirectColumnReader.hh"

#include <algorithm>
#include <cstring>

#include "reader/common/Filter.h"
#include "util/bit_util.h"
#include "util/omni_exception.h"
#include "vector/vector.h"

namespace omniruntime::reader {

using omniruntime::vec::BaseVector;
using omniruntime::vec::LargeStringContainer;
using omniruntime::vec::Vector;

namespace {

inline void TrimTrailingSpaces(char *chars, int64_t &len)
{
    while (len > 0 && chars[len - 1] == ' ') {
        --len;
    }
}

std::string_view ReadString(BaseVector *v, int32_t row)
{
    return static_cast<Vector<LargeStringContainer<std::string_view>> *>(v)->GetValue(row);
}

bool IsActiveRow(common::RowSet activeRows, size_t &activeIdx, common::vector_size_t row)
{
    if (activeIdx >= activeRows.size()) {
        return false;
    }
    if (activeRows[activeIdx] != row) {
        return false;
    }
    ++activeIdx;
    return true;
}

} // namespace

SelectiveStringDirectColumnReader::SelectiveStringDirectColumnReader(codegen::ScanSpec *spec,
                                                                     const ::orc::Type *orcType,
                                                                     std::unique_ptr<::orc::ColumnReader> inner)
    : SelectiveColumnReader(spec, orcType, std::move(inner))
{
    directInner_ = dynamic_cast<OmniStringDirectColumnReader *>(inner_);
    if (directInner_ == nullptr) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR",
            "SelectiveStringDirectColumnReader requires OmniStringDirectColumnReader as inner");
    }
}

void SelectiveStringDirectColumnReader::RejectUnsupportedValueFilter() const
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
                "SelectiveStringDirectColumnReader unsupported value filter kind: " +
                    std::to_string(static_cast<int>(kind)) +
                    " (string comparisons are not pushed from Gluten yet)");
    }
}

void SelectiveStringDirectColumnReader::EnsureNullsScratch(uint64_t rowsToRead)
{
    const size_t words = static_cast<size_t>(BitUtil::Nwords(static_cast<int32_t>(rowsToRead))) + 1;
    if (nullsScratch_.size() < words) {
        nullsScratch_.resize(words);
    }
}

void SelectiveStringDirectColumnReader::ReadProjectionDense(uint64_t rowsToRead, common::RowSet activeRows,
                                                            int omniTypeId)
{
    if (activeRows.empty()) {
        skipBatch(rowsToRead, omniTypeId);
        mat_ = Materialization::kDense;
        visitedRows_ = activeRows;
        return;
    }

    if (projectOut() && activeRows.size() == rowsToRead) {
        decoded_ = makeNewVector(rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
        inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
        mat_ = Materialization::kDense;
        visitedRows_ = activeRows;
        return;
    }

    EnsureNullsScratch(rowsToRead);
    const bool hasNulls = directInner_->readNullsForBatch(rowsToRead, nullsScratch_.data());
    const uint64_t *nulls = hasNulls ? nullsScratch_.data() : nullptr;

    const int32_t outSize = static_cast<int32_t>(activeRows.size());
    decoded_ = makeNewVector(static_cast<uint64_t>(outSize), orcType_,
                             static_cast<omniruntime::type::DataTypeId>(omniTypeId));
    auto *varcharVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(decoded_.get());

    const common::vector_size_t lastActive = activeRows.back();
    size_t activeIdx = 0;
    int32_t denseIdx = 0;
    size_t pendingSkip = 0;
    OmniRleDecoderV2 *lengthDec = directInner_->lengthDecoder();
    const bool trimChar = directInner_->isCharType();

    auto flushSkip = [&]() {
        if (pendingSkip > 0) {
            directInner_->skipDataBytes(pendingSkip);
            pendingSkip = 0;
        }
    };

    for (common::vector_size_t row = 0; row <= lastActive; ++row) {
        const bool active = IsActiveRow(activeRows, activeIdx, row);
        const bool isNull = hasNulls && BitUtil::IsBitSet(nulls, row);

        if (isNull) {
            if (active) {
                flushSkip();
                varcharVector->SetNull(denseIdx++);
            }
            continue;
        }

        // Non-null: consume one LENGTH (needed even for dead rows to know DATA size).
        int64_t len = 0;
        lengthDec->next(&len, 1, nullptr);
        if (len < 0) {
            throw ::orc::ParseError("negative length in SelectiveStringDirectColumnReader");
        }
        const size_t ulen = static_cast<size_t>(len);

        if (!active) {
            pendingSkip += ulen;
            continue;
        }

        flushSkip();
        if (ulen == 0) {
            varcharVector->SetValue(denseIdx++, std::string_view());
            continue;
        }
        if (dataScratch_.size() < ulen) {
            dataScratch_.resize(ulen);
        }
        directInner_->readDataBytes(ulen, dataScratch_.data());
        int64_t viewLen = len;
        if (trimChar) {
            TrimTrailingSpaces(dataScratch_.data(), viewLen);
        }
        varcharVector->SetValue(denseIdx++, std::string_view(dataScratch_.data(), static_cast<size_t>(viewLen)));
    }
    flushSkip();

    // PRESENT already fully consumed by readNullsForBatch(rowsToRead).
    // LENGTH/DATA for rows after lastActive must still advance.
    if (static_cast<uint64_t>(lastActive) + 1 < rowsToRead) {
        size_t remainBytes = 0;
        for (uint64_t r = static_cast<uint64_t>(lastActive) + 1; r < rowsToRead; ++r) {
            if (hasNulls && BitUtil::IsBitSet(nulls, static_cast<int32_t>(r))) {
                continue;
            }
            int64_t len = 0;
            lengthDec->next(&len, 1, nullptr);
            remainBytes += static_cast<size_t>(len);
        }
        if (remainBytes > 0) {
            directInner_->skipDataBytes(remainBytes);
        }
    }

    if (denseIdx != outSize) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR",
            "SelectiveStringDirectColumnReader: wrote " + std::to_string(denseIdx) + " values, expected " +
                std::to_string(outSize));
    }

    mat_ = Materialization::kDense;
    visitedRows_ = activeRows;
}

void SelectiveStringDirectColumnReader::read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId)
{
    RejectUnsupportedValueFilter();

    if (!hasFilter()) {
        ReadProjectionDense(rowsToRead, activeRows, omniTypeId);
        return;
    }

    // Null-only / Always*: full-batch next + post-filter.
    decoded_ = makeNewVector(rowsToRead, orcType_, static_cast<omniruntime::type::DataTypeId>(omniTypeId));
    inner_->next(decoded_.get(), rowsToRead, nullptr, omniTypeId);
    decodedBase_ = 0;
    mat_ = Materialization::kBatchIndexed;

    auto *filter = spec_->filter();
    outputRows_.clear();
    outputRows_.reserve(activeRows.size());
    for (auto row : activeRows) {
        if (decoded_->IsNull(row)) {
            if (filter->testNull()) {
                outputRows_.push_back(row);
            }
            continue;
        }
        if (filter->is(::common::FilterKind::kIsNull) || filter->is(::common::FilterKind::kIsNotNull)) {
            if (filter->testNonNull()) {
                outputRows_.push_back(row);
            }
            continue;
        }
        // Value filters should have been rejected above.
        auto sv = ReadString(decoded_.get(), row);
        if (filter->testBytes(sv.data(), static_cast<int32_t>(sv.size()))) {
            outputRows_.push_back(row);
        }
    }
}

void SelectiveStringDirectColumnReader::skipBatch(uint64_t rowsToRead, int omniTypeId)
{
    (void)omniTypeId;
    EnsureNullsScratch(rowsToRead);
    const bool hasNulls = directInner_->readNullsForBatch(rowsToRead, nullsScratch_.data());
    const uint64_t *nulls = hasNulls ? nullsScratch_.data() : nullptr;
    OmniRleDecoderV2 *lengthDec = directInner_->lengthDecoder();

    size_t totalBytes = 0;
    constexpr uint64_t kBuf = 1024;
    int64_t buf[kBuf];
    uint64_t done = 0;
    while (done < rowsToRead) {
        // Walk row-by-row for null-aware length consumption when needed.
        if (!hasNulls) {
            const uint64_t step = std::min(kBuf, rowsToRead - done);
            lengthDec->next(buf, step, nullptr);
            for (uint64_t i = 0; i < step; ++i) {
                totalBytes += static_cast<size_t>(buf[i]);
            }
            done += step;
            continue;
        }
        if (BitUtil::IsBitSet(nulls, static_cast<int32_t>(done))) {
            ++done;
            continue;
        }
        int64_t len = 0;
        lengthDec->next(&len, 1, nullptr);
        totalBytes += static_cast<size_t>(len);
        ++done;
    }
    directInner_->skipDataBytes(totalBytes);
    decoded_.reset();
}

} // namespace omniruntime::reader
