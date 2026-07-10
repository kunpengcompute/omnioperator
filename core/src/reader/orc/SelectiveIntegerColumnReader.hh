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

// Selective column reader for int/bigint/smallint/date.

#ifndef OMNI_READER_ORC_SELECTIVE_INTEGER_COLUMN_READER_HH
#define OMNI_READER_ORC_SELECTIVE_INTEGER_COLUMN_READER_HH

#include "SelectiveColumnReader.hh"

namespace omniruntime::reader {

class SelectiveIntegerColumnReader final : public SelectiveColumnReader {
public:
    using SelectiveColumnReader::SelectiveColumnReader;

    void read(uint64_t rowsToRead, common::RowSet activeRows, int omniTypeId) override;

    vec::BaseVector *getValues(common::RowSet rows) override;

private:
    static int64_t ReadInt64(vec::BaseVector *v, int32_t row, int omniTypeId);
};

} // namespace omniruntime::reader

#endif // OMNI_READER_ORC_SELECTIVE_INTEGER_COLUMN_READER_HH
