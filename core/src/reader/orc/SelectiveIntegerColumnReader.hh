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

// Selective reader for int/bigint/smallint/date. Filter-while-decode produces kDense results.

#ifndef OMNI_READER_ORC_SELECTIVE_INTEGER_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_INTEGER_COLUMN_READER_HH

#include <vector>

#include "reader/common/DecodeStats.h"
#include "SelectiveColumnReader.hh"

namespace omniruntime::reader {

class SelectiveIntegerColumnReader final : public SelectiveColumnReader {
public:
    SelectiveIntegerColumnReader(codegen::ScanSpec *spec, const ::orc::Type *orcType,
                                 std::unique_ptr<::orc::ColumnReader> inner);

    void read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId) override;

    void skipBatch(uint64_t rowsToRead, int omniTypeId) override;

private:
    // Bind FilterKind to a concrete TFilter so testInt64 can be inlined in the visitor.
    template <typename T>
    void DispatchFilterKind(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId, bool hasNulls);

    template <typename T, typename TFilter>
    void RunVisitor(const TFilter *filter, uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId,
                    bool hasNulls, bool isFilterColumn);

    template <typename T, typename TFilter, bool kExtract, bool hasNulls>
    uint32_t Visit(const TFilter *filter, uint64_t rowsToRead, common::RowSet activeRows,
                   std::vector<common::vector_size_t> *outRows, T *out, uint64_t *outNulls);

    void EnsureNullsScratch(uint64_t rowsToRead);

    OmniIntegerColumnReader *intInner_ = nullptr;
    std::vector<uint64_t> nullsScratch_;
    DecodeStats stats_;
};

} // namespace omniruntime::reader

#endif // OMNI_READER_ORC_SELECTIVE_INTEGER_COLUMN_READER_HH
