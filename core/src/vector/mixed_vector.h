/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MixedVectorBatch for row-oriented storage with automatic memory management
 */

#ifndef OMNI_RUNTIME_MIXED_VECTOR_H
#define OMNI_RUNTIME_MIXED_VECTOR_H

#include <cstring>
#include <limits>
#include "vector_batch.h"
#include "row_segment.h"
#include "memory/thread_memory_manager.h"
#include "memory/memory_trace.h"
#include "memory/simple_arena_allocator.h"

namespace omniruntime::vec {

inline constexpr int64_t DEFAULT_ROW_ARENA_CAPACITY = 4096;

enum MixedBatchMode {
    HYBRID_ROW_COLUMN = 0,
    COMPLETE_ROW_ONLY = 1
};

class MixedVectorBatch : public VectorBatch {
public:
    using Deleter = void (*)(MixedVectorBatch *);

    MixedVectorBatch(size_t rowCnt)
            : VectorBatch(rowCnt) {
        rows.resize(rowCnt);
        
        int64_t batchCapacity = sizeof(MixedVectorBatch) + rowCnt * sizeof(RowSegment);
        mem::ThreadMemoryManager::ReportMemory(batchCapacity);
        mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), batchCapacity);
    }

    MixedVectorBatch(int32_t rowCount, const std::vector<DataTypeId> &typeIds)
            : VectorBatch(rowCount), rowTypes(typeIds) {
        rows.resize(rowCount);
        
        int64_t batchCapacity = sizeof(MixedVectorBatch) + 
                                 rowCount * sizeof(RowSegment) + 
                                 typeIds.size() * sizeof(DataTypeId);
        mem::ThreadMemoryManager::ReportMemory(batchCapacity);
        mem::MemoryTrace::AddVectorMemory(reinterpret_cast<uintptr_t>(this), batchCapacity);
    }

    ~MixedVectorBatch() override {
        int64_t batchCapacity = sizeof(MixedVectorBatch) + 
                                 rows.size() * sizeof(RowSegment) + 
                                 rowTypes.size() * sizeof(DataTypeId);
        
        rows.clear();
        rowTypes.clear();
        rowArena.reset();
        
        mem::ThreadMemoryManager::ReclaimMemory(batchCapacity);
        mem::MemoryTrace::SubVectorMemory(reinterpret_cast<uintptr_t>(this), batchCapacity);
    }

    size_t MixType() override {
        return 1;
    }

    void Resize(int32_t rowCount) {
        rows.resize(rowCount);
        VectorBatch::Resize(rowCount);
        
        if (rowArena) {
            rowArena.reset();
        }
    }
    
    void PrepareRowArena(int64_t estimatedCapacity) {
        if (estimatedCapacity > 0) {
            rowArena = std::make_unique<mem::SimpleArenaAllocator>(estimatedCapacity, mem::Allocator::GetAllocator());
        } else {
            rowArena = std::make_unique<mem::SimpleArenaAllocator>(DEFAULT_ROW_ARENA_CAPACITY, mem::Allocator::GetAllocator());
        }
        
        rows.clear();
        rows.resize(rowCnt);
    }
    
    mem::SimpleArenaAllocator* GetRowArena() { 
        return rowArena.get(); 
    }

    int32_t GetColumnCount() {
        return static_cast<int32_t>(rowTypes.size());
    }

    int32_t GetVarcharSlotOffset() const { return varcharSlotOffset_; }
    void SetVarcharSlotOffset(int32_t offset) { varcharSlotOffset_ = offset; }

    Deleter getDeleter() const {
        return deleter;
    }

    void setDeleter(Deleter newDeleter) {
        deleter = newDeleter;
    }

    std::vector<DataTypeId> GetRowTypes() {
        return this->rowTypes;
    }

    void SetRowTypes(std::vector<DataTypeId> typeIds) {
        rowTypes = std::move(typeIds);
    }

    void SetMode(MixedBatchMode mode) {
        mode_ = mode;
    }

    MixedBatchMode GetMode() const {
        return mode_;
    }

    RowSegment* GetRow(int32_t index) {
        return &rows[index];
    }

    void SetOwnedRow(int32_t index, const uint8_t *data, int32_t size) {
        if (size > 0 && data == nullptr) {
            throw exception::OmniException("MixedVectorBatch", "non-zero size but null row data");
        }
        rows[index] = RowSegment(size);
        if (size > 0 && data != nullptr) {
            memcpy(rows[index].data, data, size);
        }
    }

    void SetArenaRow(int32_t index, uint8_t *data, int32_t size) {
        rows[index].Reset(data, size);
    }

    void SetArenaRow(int32_t index, uint8_t *data, int32_t keyLength, int32_t stateOffset, int32_t size) {
        rows[index].Reset(data, keyLength, stateOffset, size);
    }

private:
    std::vector<RowSegment> rows;
    std::vector<DataTypeId> rowTypes;
    std::unique_ptr<mem::SimpleArenaAllocator> rowArena;
    int32_t varcharSlotOffset_ = 0;
    Deleter deleter = nullptr;
    MixedBatchMode mode_ = HYBRID_ROW_COLUMN;
};

}  // namespace omniruntime::vec

#endif  // OMNI_RUNTIME_MIXED_VECTOR_H
