/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayMinMax function implementation for finding max/min value in array
 */

#include "ArrayMinMaxFunction.h"
#include <limits>
#include <cmath>
#include "vectorization/SelectivityVector.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

// ============================================================================
// ArrayMaxFunction Implementation
// ============================================================================

void ArrayMaxFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("ArrayMaxFunction Error:", "No arguments provided");
    }

    // Get input vector (array)
    auto inputArg = args.top();
    args.pop();

    if (inputArg == nullptr) {
        OMNI_THROW("ArrayMaxFunction Error:", "Input vector is null");
    }

    if (inputArg->GetTypeId() != OMNI_ARRAY) {
        delete inputArg;
        OMNI_THROW("ArrayMaxFunction Error:", "Input is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(inputArg);
    if (!arrayVec) {
        delete inputArg;
        OMNI_THROW("ArrayMaxFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    DataTypeId elementTypeId = outputType->GetId();
    
    // Create result vector based on element type
    result = VectorHelper::CreateFlatVector(elementTypeId, rowSize);
    if (!result) {
        delete inputArg;
        OMNI_THROW("ArrayMaxFunction Error:", "Failed to create result vector");
    }

    // Process based on element type
    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessArrayMax<int8_t>(arrayVec, result, rowSize);
            break;
        case OMNI_SHORT:
            ProcessArrayMax<int16_t>(arrayVec, result, rowSize);
            break;
        case OMNI_INT:
            ProcessArrayMax<int32_t>(arrayVec, result, rowSize);
            break;
        case OMNI_LONG:
            ProcessArrayMax<int64_t>(arrayVec, result, rowSize);
            break;
        case OMNI_FLOAT:
            ProcessArrayMax<float>(arrayVec, result, rowSize);
            break;
        case OMNI_DOUBLE:
            ProcessArrayMax<double>(arrayVec, result, rowSize);
            break;
        case OMNI_BOOLEAN:
            ProcessArrayMax<bool>(arrayVec, result, rowSize);
            break;
        case OMNI_DECIMAL128:
            ProcessArrayMax<Decimal128>(arrayVec, result, rowSize);
            break;
        default:
            delete inputArg;
            delete result;
            OMNI_THROW("ArrayMaxFunction Error:", "Unsupported element type: " + std::to_string(elementTypeId));
    }
    
    // Clean up input argument (ownership transferred from ExprEval stack)
    delete inputArg;
}

template <typename T>
void ArrayMaxFunction::ProcessArrayMax(ArrayVector *arrayVec, BaseVector *result, int32_t rowSize) const
{
    auto *resultVec = dynamic_cast<Vector<T> *>(result);
    if (!resultVec) {
        OMNI_THROW("ArrayMaxFunction Error:", "Result vector type mismatch");
    }

    auto elementVec = arrayVec->GetElementVector();
    auto *typedElementVec = dynamic_cast<Vector<T> *>(elementVec.get());
    if (!typedElementVec) {
        OMNI_THROW("ArrayMaxFunction Error:", "Element vector type mismatch");
    }

    int64_t *offsets = arrayVec->GetOffsets();

    for (int32_t row = 0; row < rowSize; ++row) {
        // If array itself is null, result is null
        if (arrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];
        int64_t arrayLength = endOffset - startOffset;

        // Empty array returns null
        if (arrayLength == 0) {
            resultVec->SetNull(row);
            continue;
        }

        // Find the first non-null element
        int64_t firstNonNullIdx = -1;
        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (!typedElementVec->IsNull(static_cast<int32_t>(i))) {
                firstNonNullIdx = i;
                break;
            }
        }

        // If all elements are null, result is null
        if (firstNonNullIdx == -1) {
            resultVec->SetNull(row);
            continue;
        }

        // Find max value starting from first non-null element
        T maxValue = typedElementVec->GetValue(static_cast<int32_t>(firstNonNullIdx));
        for (int64_t i = firstNonNullIdx + 1; i < endOffset; ++i) {
            if (!typedElementVec->IsNull(static_cast<int32_t>(i))) {
                T candidateValue = typedElementVec->GetValue(static_cast<int32_t>(i));
                
                // Special handling for floating point NaN
                if constexpr (std::is_floating_point_v<T>) {
                    // NaN is considered greater than any non-NaN value
                    if (std::isnan(candidateValue) ||
                        (!std::isnan(maxValue) && candidateValue > maxValue)) {
                        maxValue = candidateValue;
                    }
                } else {
                    if (candidateValue > maxValue) {
                        maxValue = candidateValue;
                    }
                }
            }
        }

        resultVec->SetValue(row, maxValue);
    }
}

// Explicit template instantiations
template void ArrayMaxFunction::ProcessArrayMax<int8_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<int16_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<int32_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<int64_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<float>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<double>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<bool>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMaxFunction::ProcessArrayMax<Decimal128>(ArrayVector *, BaseVector *, int32_t) const;

// ============================================================================
// ArrayMinFunction Implementation
// ============================================================================

void ArrayMinFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("ArrayMinFunction Error:", "No arguments provided");
    }

    // Get input vector (array)
    auto inputArg = args.top();
    args.pop();

    if (inputArg == nullptr) {
        OMNI_THROW("ArrayMinFunction Error:", "Input vector is null");
    }

    if (inputArg->GetTypeId() != OMNI_ARRAY) {
        delete inputArg;
        OMNI_THROW("ArrayMinFunction Error:", "Input is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(inputArg);
    if (!arrayVec) {
        delete inputArg;
        OMNI_THROW("ArrayMinFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    DataTypeId elementTypeId = outputType->GetId();
    
    // Create result vector based on element type
    result = VectorHelper::CreateFlatVector(elementTypeId, rowSize);
    if (!result) {
        delete inputArg;
        OMNI_THROW("ArrayMinFunction Error:", "Failed to create result vector");
    }

    // Process based on element type
    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessArrayMin<int8_t>(arrayVec, result, rowSize);
            break;
        case OMNI_SHORT:
            ProcessArrayMin<int16_t>(arrayVec, result, rowSize);
            break;
        case OMNI_INT:
            ProcessArrayMin<int32_t>(arrayVec, result, rowSize);
            break;
        case OMNI_LONG:
            ProcessArrayMin<int64_t>(arrayVec, result, rowSize);
            break;
        case OMNI_FLOAT:
            ProcessArrayMin<float>(arrayVec, result, rowSize);
            break;
        case OMNI_DOUBLE:
            ProcessArrayMin<double>(arrayVec, result, rowSize);
            break;
        case OMNI_BOOLEAN:
            ProcessArrayMin<bool>(arrayVec, result, rowSize);
            break;
        case OMNI_DECIMAL128:
            ProcessArrayMin<Decimal128>(arrayVec, result, rowSize);
            break;
        default:
            delete inputArg;
            delete result;
            OMNI_THROW("ArrayMinFunction Error:", "Unsupported element type: " + std::to_string(elementTypeId));
    }
    
    // Clean up input argument (ownership transferred from ExprEval stack)
    delete inputArg;
}

template <typename T>
void ArrayMinFunction::ProcessArrayMin(ArrayVector *arrayVec, BaseVector *result, int32_t rowSize) const
{
    auto *resultVec = dynamic_cast<Vector<T> *>(result);
    if (!resultVec) {
        OMNI_THROW("ArrayMinFunction Error:", "Result vector type mismatch");
    }

    auto elementVec = arrayVec->GetElementVector();
    auto *typedElementVec = dynamic_cast<Vector<T> *>(elementVec.get());
    if (!typedElementVec) {
        OMNI_THROW("ArrayMinFunction Error:", "Element vector type mismatch");
    }

    int64_t *offsets = arrayVec->GetOffsets();

    for (int32_t row = 0; row < rowSize; ++row) {
        // If array itself is null, result is null
        if (arrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];
        int64_t arrayLength = endOffset - startOffset;

        // Empty array returns null
        if (arrayLength == 0) {
            resultVec->SetNull(row);
            continue;
        }

        // Find the first non-null element
        int64_t firstNonNullIdx = -1;
        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (!typedElementVec->IsNull(static_cast<int32_t>(i))) {
                firstNonNullIdx = i;
                break;
            }
        }

        // If all elements are null, result is null
        if (firstNonNullIdx == -1) {
            resultVec->SetNull(row);
            continue;
        }

        // Find min value starting from first non-null element
        T minValue = typedElementVec->GetValue(static_cast<int32_t>(firstNonNullIdx));
        for (int64_t i = firstNonNullIdx + 1; i < endOffset; ++i) {
            if (!typedElementVec->IsNull(static_cast<int32_t>(i))) {
                T candidateValue = typedElementVec->GetValue(static_cast<int32_t>(i));
                
                // Special handling for floating point NaN
                if constexpr (std::is_floating_point_v<T>) {
                    // For min: if current is NaN, replace with non-NaN candidate
                    // NaN is considered greater than any non-NaN value
                    if (std::isnan(minValue) ||
                        (!std::isnan(candidateValue) && candidateValue < minValue)) {
                        minValue = candidateValue;
                    }
                } else {
                    if (candidateValue < minValue) {
                        minValue = candidateValue;
                    }
                }
            }
        }

        resultVec->SetValue(row, minValue);
    }
}

// Explicit template instantiations
template void ArrayMinFunction::ProcessArrayMin<int8_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<int16_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<int32_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<int64_t>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<float>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<double>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<bool>(ArrayVector *, BaseVector *, int32_t) const;
template void ArrayMinFunction::ProcessArrayMin<Decimal128>(ArrayVector *, BaseVector *, int32_t) const;

} // namespace omniruntime::vectorization
