/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Slice function implementation
 */

#include "Slice.h"
#include "vector/array_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "util/compiler_util.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

void SliceImpl::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    // Extract arguments from stack: length, start, array
    auto* lengthArg = args.top(); 
    args.pop();
    auto* startArg = args.top(); 
    args.pop();
    auto* arrayArg = args.top(); 
    args.pop();

    auto size = arrayArg->GetSize();
    auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(arrayArg));
    auto rows = SelectivityVector(size);
    rows.setFromBitsNegate(nullBits, size);

    // Check if start and length arguments are the same type
    if (startArg->GetTypeId() != lengthArg->GetTypeId()) {
        OMNI_THROW("Slice error:", "Start and length arguments must have the same type");
    }

    switch (startArg->GetTypeId()) {
        case OMNI_INT:
            result = applySliceTyped<int32_t>(rows, arrayArg, startArg, lengthArg, context);
            break;
        case OMNI_LONG:
            result = applySliceTyped<int64_t>(rows, arrayArg, startArg, lengthArg, context);
            break;
        default:
            OMNI_THROW("Slice error:", "Unsupported type for start/length arguments: {}", 
                std::to_string(startArg->GetTypeId()));
    }

    // Clean up arguments
    if (lengthArg != nullptr) {
        delete lengthArg;
    }
    if (startArg != nullptr) {
        delete startArg;
    }
    if (arrayArg != nullptr) {
        delete arrayArg;
    }
}

template <typename I>
BaseVector *SliceImpl::applySliceTyped(const SelectivityVector &rows, BaseVector *arrayArg, BaseVector *startArg,
    BaseVector *lengthArg, ExecutionContext *context) const
{
    auto arrayVector = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVector == nullptr) {
        OMNI_THROW("Slice error:", "First argument to slice must be ARRAY, but got type id: {}",
            arrayArg ? arrayArg->GetTypeId() : -1);
    }

    auto rowSize = arrayVector->GetSize();
    auto offsets = arrayVector->GetOffsets();
    auto elementVector = arrayVector->GetElementVector();

    // Create result array vector
    auto result = new ArrayVector(rowSize, context->GetPool());
    auto resultOffsets = result->GetOffsets();

    // Helper function to adjust start index (1-based to 0-based, handle negative indices)
    auto adjustStart = [](I start, int64_t arraySize) -> int64_t {
        // Spark semantics: start is 1-based, 0 is invalid
        if (start == 0) {
            OMNI_THROW("Slice error:", "Array indices start at 1, got 0");
        }
        // Convert 1-based to 0-based
        if (start > 0) {
            start = start - 1;
        }
        // Handle negative indices (wrap around)
        if (start < 0) {
            start = arraySize + start;
        }
        return static_cast<int64_t>(start);
    };

    // Get start and length values (assuming const for now)
    I startValue = 0;
    I lengthValue = 0;

    if (auto startConst = dynamic_cast<ConstVector<I> *>(startArg)) {
        startValue = startConst->GetConstValue();
    } else {
        OMNI_THROW("Slice error:", "Start argument must be constant, but got type id: {}",
            startArg ? startArg->GetTypeId() : -1);
    }

    if (auto lengthConst = dynamic_cast<ConstVector<I> *>(lengthArg)) {
        lengthValue = lengthConst->GetConstValue();
    } else {
        OMNI_THROW("Slice error:", "Length argument must be constant, but got type id: {}",
            lengthArg ? lengthArg->GetTypeId() : -1);
    }

    // Validate length
    if (lengthValue < 0) {
        OMNI_THROW("Slice error:", "Length argument must be non-negative, got {}", lengthValue);
    }

    // Copy implementation: create a new element vector and copy sliced elements
    // Collect all element positions that need to be copied
    std::vector<int> elementPositions;
    int64_t currentOffset = 0;
    
    rows.applyToSelected([&](auto row) {
        // Get array size for this row
        int64_t arraySize = offsets[row + 1] - offsets[row];
        
        // Adjust start index
        int64_t adjustedStart = adjustStart(startValue, arraySize);
        
        // Calculate actual start and end positions in the element vector
        int64_t elementStart = offsets[row] + adjustedStart;
        int64_t elementEnd = offsets[row + 1];
        
        // Calculate actual length (don't exceed array bounds)
        int64_t actualLength = std::min(static_cast<int64_t>(lengthValue), elementEnd - elementStart);
        
        // Copy null bits if the original array was null
        if (arrayVector->IsNull(row)) {
            result->SetNull(row);
            resultOffsets[row] = currentOffset;
            resultOffsets[row + 1] = currentOffset;  // Empty array (size = 0)
            // No elements to add for null arrays
            return;
        }
        
        // If start is out of bounds or length is 0, return empty array (not NULL)
        if (adjustedStart < 0 || adjustedStart >= arraySize || actualLength <= 0) {
            resultOffsets[row] = currentOffset;
            resultOffsets[row + 1] = currentOffset;  // Empty array (size = 0)
            // No elements to add for empty arrays
            return;
        }
        
        // Set offsets for the new element vector (starting from 0)
        resultOffsets[row] = currentOffset;
        resultOffsets[row + 1] = currentOffset + actualLength;
        
        // Collect element positions to copy from the original element vector
        for (int64_t i = 0; i < actualLength; ++i) {
            elementPositions.push_back(static_cast<int>(elementStart + i));
        }
        
        currentOffset += actualLength;
    });
    
    // Create new element vector by copying selected positions
    int elementLength = static_cast<int>(elementPositions.size());
    if (elementLength == 0) {
        // Create empty element vector with the same type
        auto elementDataType = VectorHelper::GetDataType(elementVector.get());
        result->SetElementVector(std::shared_ptr<BaseVector>(
            VectorHelper::CreateComplexVector(context->GetPool(), elementDataType.get(), 0)));
    } else {
        // Copy elements from original element vector
        auto newElementVector = VectorHelper::CopyPositionsVector(
            elementVector.get(), elementPositions.data(), 0, elementLength);
        result->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
    }

    return result;
}

// Explicit template instantiation
template BaseVector *SliceImpl::applySliceTyped<int32_t>(const SelectivityVector &rows, BaseVector *arrayArg,
    BaseVector *startArg, BaseVector *lengthArg, ExecutionContext *context) const;

template BaseVector *SliceImpl::applySliceTyped<int64_t>(const SelectivityVector &rows, BaseVector *arrayArg,
    BaseVector *startArg, BaseVector *lengthArg, ExecutionContext *context) const;
}