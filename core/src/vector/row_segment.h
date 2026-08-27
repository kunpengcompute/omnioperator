/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Row segment for mixed vector batch memory management
 */
#ifndef OMNI_RUNTIME_ROW_SEGMENT_H
#define OMNI_RUNTIME_ROW_SEGMENT_H

#include "memory/allocator.h"
#include "util/omni_exception.h"
#include <cstdint>

namespace omniruntime::vec {

class RowSegment {
public:
    uint8_t *data;
    int32_t length;
    int32_t keyLength;
    int32_t stateOffset;
    
    RowSegment() : data(nullptr), length(0), keyLength(0), stateOffset(0), allocatedSize(0), ownsMemory(false) {}
    
    explicit RowSegment(int32_t size) 
        : data(nullptr), length(size), keyLength(size), stateOffset(size), allocatedSize(size), ownsMemory(true) {
        if (size > 0) {
            data = static_cast<uint8_t*>(mem::Allocator::GetAllocator()->Alloc(size));
        }
    }
    
    RowSegment(uint8_t *externalData, int32_t size)
        : data(externalData), length(size), keyLength(size), stateOffset(size), allocatedSize(0), ownsMemory(false) {}
    
    ~RowSegment() {
        if (ownsMemory && data != nullptr && allocatedSize > 0) {
            mem::Allocator::GetAllocator()->Free(data, allocatedSize);
            data = nullptr;
        }
    }
    
    void Reset(uint8_t *newData, int32_t newSize) {
        if (ownsMemory && data != nullptr && allocatedSize > 0) {
            mem::Allocator::GetAllocator()->Free(data, allocatedSize);
        }
        data = newData;
        length = newSize;
        keyLength = newSize;
        stateOffset = newSize;
        allocatedSize = 0;
        ownsMemory = false;
    }

    void Reset(uint8_t *newData, int32_t newKeyLength, int32_t newStateOffset, int32_t newSize) {
        if (newKeyLength < 0 || newStateOffset < newKeyLength || newSize < newStateOffset) {
            throw exception::OmniException("RowSegment", "invalid mixed row layout");
        }
        Reset(newData, newSize);
        keyLength = newKeyLength;
        stateOffset = newStateOffset;
    }
    
    bool OwnsMemory() const { return ownsMemory; }
    
    RowSegment(const RowSegment&) = delete;
    RowSegment& operator=(const RowSegment&) = delete;
    
    RowSegment(RowSegment&& other) noexcept
        : data(other.data), length(other.length), keyLength(other.keyLength), stateOffset(other.stateOffset),
          allocatedSize(other.allocatedSize), ownsMemory(other.ownsMemory) {
        other.data = nullptr;
        other.length = 0;
        other.keyLength = 0;
        other.stateOffset = 0;
        other.allocatedSize = 0;
        other.ownsMemory = false;
    }
    
    RowSegment& operator=(RowSegment&& other) noexcept {
        if (this != &other) {
            if (ownsMemory && data != nullptr && allocatedSize > 0) {
                mem::Allocator::GetAllocator()->Free(data, allocatedSize);
            }
            data = other.data;
            length = other.length;
            keyLength = other.keyLength;
            stateOffset = other.stateOffset;
            allocatedSize = other.allocatedSize;
            ownsMemory = other.ownsMemory;
            
            other.data = nullptr;
            other.length = 0;
            other.keyLength = 0;
            other.stateOffset = 0;
            other.allocatedSize = 0;
            other.ownsMemory = false;
        }
        return *this;
    }
    
private:
    int32_t allocatedSize;
    bool ownsMemory;
};

}  // namespace omniruntime::vec

#endif  // OMNI_RUNTIME_ROW_SEGMENT_H
