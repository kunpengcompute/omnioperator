/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "chunk.h"

namespace omniruntime {
namespace mem {
Chunk::Chunk(Allocator *allocator, void *address, uint64_t sizeInBytes)
    : address(address), sizeInBytes(sizeInBytes), allocator(allocator)
{
}

Chunk::~Chunk()
{
    if (address == nullptr) {
        std::cerr << "address is null in chunk." << std::endl;
        return;
    }

    allocator->Free(address, static_cast<int64_t>(sizeInBytes));
    address = nullptr;
}

void *Chunk::GetAddress() const { return address; }

uint64_t Chunk::GetSizeInBytes() { return sizeInBytes; }
} // namespace mem
} // namespace omniruntime
