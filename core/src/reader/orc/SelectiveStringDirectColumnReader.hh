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

#ifndef OMNI_READER_ORC_SELECTIVE_STRING_DIRECT_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_STRING_DIRECT_COLUMN_READER_HH

#include <vector>

#include "SelectiveColumnReader.hh"

namespace omniruntime::reader {

// Direct/Direct_V2: selective projection materializes only survivor rows' DATA bytes.
// LENGTH for non-null rows in [0, lastActive] is still consumed (needed for DATA offsets).
class SelectiveStringDirectColumnReader final : public SelectiveColumnReader {
public:
    SelectiveStringDirectColumnReader(codegen::ScanSpec *spec, const ::orc::Type *orcType,
                                      std::unique_ptr<::orc::ColumnReader> inner);

    void read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId) override;
    void skipBatch(uint64_t rowsToRead, int omniTypeId) override;

private:
    void RejectUnsupportedValueFilter() const;
    void EnsureNullsScratch(uint64_t rowsToRead);
    void ReadProjectionDense(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId);

    OmniStringDirectColumnReader *directInner_ = nullptr;
    std::vector<uint64_t> nullsScratch_;
    std::vector<char> dataScratch_;
};

} // namespace omniruntime::reader

#endif
