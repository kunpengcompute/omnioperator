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

#ifndef OMNI_READER_ORC_SELECTIVE_BOOLEAN_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_BOOLEAN_COLUMN_READER_HH

#include "SelectiveColumnReader.hh"

namespace omniruntime::reader {

class SelectiveBooleanColumnReader final : public SelectiveColumnReader {
public:
    using SelectiveColumnReader::SelectiveColumnReader;

    void read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId) override;
    vec::BaseVector *getValues(common::RowSet rows) override;
};

} // namespace omniruntime::reader

#endif // OMNI_READER_ORC_SELECTIVE_BOOLEAN_COLUMN_READER_HH
