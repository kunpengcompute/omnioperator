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

#ifndef OMNI_READER_ORC_SELECTIVE_STRING_DICTIONARY_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_STRING_DICTIONARY_COLUMN_READER_HH

#include <vector>

#include "SelectiveColumnReader.hh"

namespace common {
class Filter;
}

namespace omniruntime::reader {

// Dictionary/Dictionary_V2: nextAsDictionary + dict-entry prefilter (id accel) + DictionaryVector out.
class SelectiveStringDictionaryColumnReader final : public SelectiveColumnReader {
public:
    using SelectiveColumnReader::SelectiveColumnReader;

    // getValues comes from the base class: this reader stays on the kBatchIndexed shape.
    void read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId) override;

private:
    void RebuildAcceptedIds(::common::Filter *filter);
    // Built once per Selective reader lifetime (one stripe); reused across batches.
    std::vector<uint8_t> acceptedIds_;
    ::common::Filter *acceptedForFilter_ = nullptr;
    bool acceptedReady_ = false;
};

} // namespace omniruntime::reader

#endif
