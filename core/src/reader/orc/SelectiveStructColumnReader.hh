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

// Selective struct orchestration: filter cols shrink rows → project cols decode → compact by channel.

#ifndef OMNI_READER_ORC_SELECTIVE_STRUCT_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_STRUCT_COLUMN_READER_HH

#include <memory>
#include <vector>

// StripeStream.hh needs ColumnReader etc. forward decls; include after SelectiveColumnReader.hh.
#include "SelectiveColumnReader.hh"
#include "orc/StripeStream.hh"
#include "orc/Type.hh"
#include "codegen/ScanSpec.h"
#include "reader/common/JulianGregorianRebase.h"

namespace omniruntime::reader {

class SelectiveStructColumnReader {
public:
    SelectiveStructColumnReader(const ::orc::Type &rootType,
                                ::orc::StripeStreams &stripe,
                                codegen::ScanSpec *rootSpec,
                                common::JulianGregorianRebase *julian);

    uint64_t read(uint64_t rowsToRead, std::vector<vec::BaseVector *> &outBatch, int *omniTypeId);

    void seekToRowGroup(std::unordered_map<uint64_t, ::orc::PositionProvider> &positions);

private:
    std::vector<std::unique_ptr<SelectiveColumnReader>> children_;
    std::vector<int> filterOrder_;
    std::vector<int> projectOrder_;
    int numOutputChannels_ = 0;
    std::vector<common::vector_size_t> active_;
    std::vector<common::vector_size_t> survivors_;
};

} // namespace omniruntime::reader

#endif // OMNI_READER_ORC_SELECTIVE_STRUCT_COLUMN_READER_HH
