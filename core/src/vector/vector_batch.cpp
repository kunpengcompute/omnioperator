/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "unsafe_vector.h"
#include "vector_batch.h"

namespace omniruntime::vec {
/**
 * @param rowCnt
 */
VectorBatch::VectorBatch(size_t rowCnt) : capacity(rowCnt), rowCnt(rowCnt) {}

VectorBatch::~VectorBatch() { FreeAllVectors(); }

/**
 * Set the vector at the indicated index, need ResizeVectorCount before SetVector
 * @param vector
 */
void VectorBatch::SetVector(int32_t index, BaseVector *vector) { vectors[index] = vector; }

/**
 * @param vector
 */
void VectorBatch::Append(BaseVector *vector) { vectors.emplace_back(vector); }

void VectorBatch::AppendFlat(BaseVector *vector)
{
    vectors.emplace_back(vector);
    ++flatSize;
}

BaseVector **VectorBatch::GetVectors() { return vectors.data(); }

int32_t VectorBatch::GetRowCount() const { return static_cast<int32_t>(rowCnt); }

int32_t VectorBatch::GetVectorCount() { return static_cast<int32_t>(vectors.size()); }

/**
 * @param vectorCnt
 */
void VectorBatch::ResizeVectorCount(size_t vectorCnt) { vectors.resize(vectorCnt); }

void VectorBatch::FreeAllVectors()
{
    int32_t vectorSize = static_cast<int32_t>(vectors.size()) - flatSize;
    std::unordered_set<BaseVector *> deleted;
    for (size_t vecIndex = 0; vecIndex < vectorSize; ++vecIndex) {
        if (deleted.count(vectors[vecIndex])) {
            continue;
        }
        try {
            deleted.insert(vectors[vecIndex]);
        } catch (const std::exception &e) {
            throw std::runtime_error("fail to insert");
        }
        delete vectors[vecIndex];
        vectors[vecIndex] = nullptr;
    }
    vectors.clear();
}

// This API is used to reuse the vector batch memory.
// The caller must ensure that the row count does not exceed the row count of the first memory allocation
void VectorBatch::Resize(size_t rowCount)
{
    if (rowCount > capacity) {
        throw OmniException("UNSUPPORTED_ERROR", "Can only resize vector batch to a smaller value.");
    }

    for (auto *vector : vectors) {
        vector->SetNulls(0, false, static_cast<int32_t>(rowCount));
        unsafe::UnsafeBaseVector::SetSize(vector, static_cast<int32_t>(rowCount));
    }

    rowCnt = rowCount;
}

void VectorBatch::ClearVectors() { vectors.clear(); }

size_t VectorBatch::GetCapacity() { return capacity; }

uint64_t VectorBatch::CalculateTotalSize() const
{
    if (vectors.empty()) {
        return 0;
    }

    uint64_t totalSize = 0;
    for (const auto &vector : vectors) {
        totalSize += static_cast<uint64_t>(vector->GetSize());
    }

    return totalSize;
}
} // namespace omniruntime::vec
