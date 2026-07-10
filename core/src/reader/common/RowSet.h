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

// Lightweight view over surviving row indices (does not own memory).

#ifndef OMNI_READER_COMMON_ROWSET_H
#define OMNI_READER_COMMON_ROWSET_H

#include <cstddef>
#include <cstdint>

namespace common {

using vector_size_t = int32_t;

class RowSet {
public:
    RowSet() = default;
    RowSet(const vector_size_t *data, size_t size) : data_(data), size_(size) {}

    const vector_size_t *begin() const { return data_; }
    const vector_size_t *end() const { return data_ + size_; }
    const vector_size_t *data() const { return data_; }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    vector_size_t front() const { return data_[0]; }
    vector_size_t back() const { return data_[size_ - 1]; }
    vector_size_t operator[](size_t i) const { return data_[i]; }

private:
    const vector_size_t *data_ = nullptr;
    size_t size_ = 0;
};

} // namespace common

#endif // OMNI_READER_COMMON_ROWSET_H
