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

// Selective column reader: thin orchestration/filter layer; decode delegated to inner_
// (OmniColumnReader). T0 filter/projection columns both use full-batch next; skip is unused
// (incompatible with next run-length state).

#ifndef OMNI_READER_ORC_SELECTIVE_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_COLUMN_READER_HH

#include <memory>
#include <vector>

#include "orc/ColumnReader.hh"
#include "orc/Type.hh"
#include "codegen/ScanSpec.h"
#include "reader/common/RowSet.h"
#include "vector/vector.h"
#include "OmniColReader.hh"

namespace omniruntime::reader {

class SelectiveColumnReader {
public:
    SelectiveColumnReader(codegen::ScanSpec *spec,
                          const ::orc::Type *orcType,
                          std::unique_ptr<::orc::ColumnReader> inner)
        : spec_(spec),
          orcType_(orcType),
          owned_(std::move(inner)),
          inner_(static_cast<OmniColumnReader *>(owned_.get()))
    {}

    virtual ~SelectiveColumnReader() = default;

    // Filter cols: decode then apply filter into outputRows_; projection cols: full-batch decode.
    // Both advance inner_ by rowsToRead.
    virtual void read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId) = 0;

    // Compact output by rows; caller takes ownership.
    virtual vec::BaseVector *getValues(common::RowSet rows) = 0;

    // When the batch yields nothing, still advance with next() (do not use skip).
    void skipBatch(uint64_t rowsToRead, int omniTypeId)
    {
        auto scratch = makeNewVector(rowsToRead, orcType_,
                                     static_cast<type::DataTypeId>(omniTypeId));
        inner_->next(scratch.get(), rowsToRead, /*incomingNulls*/nullptr, omniTypeId);
        decoded_.reset();
    }

    void seekToRowGroup(std::unordered_map<uint64_t, ::orc::PositionProvider> &positions)
    {
        inner_->seekToRowGroup(positions);
    }

    const std::vector<common::vector_size_t> &outputRows() const { return outputRows_; }

    codegen::ScanSpec *scanSpec() const { return spec_; }
    bool hasFilter() const { return spec_ != nullptr && spec_->hasFilter(); }
    bool projectOut() const { return spec_ != nullptr && spec_->projectOut(); }
    type::column_index_t channel() const { return spec_->channel(); }

protected:
    codegen::ScanSpec *spec_;
    const ::orc::Type *orcType_;
    std::unique_ptr<::orc::ColumnReader> owned_;
    OmniColumnReader *inner_;

    std::unique_ptr<vec::BaseVector> decoded_;
    uint64_t decodedBase_ = 0; // Always 0 in T0
    std::vector<common::vector_size_t> outputRows_;
    std::vector<int32_t> positions_;
};

} // namespace omniruntime::reader

#endif // OMNI_READER_ORC_SELECTIVE_COLUMN_READER_HH
