/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Reverse function implementation for both String and Array types
 */

#include "ReverseFunction.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "type/string_Impl.h"
#include "util/utf8_util.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void ReverseFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("Reverse function Error", "Reverse requires 1 argument");
    }

    BaseVector *argVector = args.top();
    args.pop();

    if (argVector == nullptr) {
        OMNI_THROW("Reverse function Error", "Input vector is null");
    }

    // Dispatch based on input type
    DataTypeId inputTypeId = argVector->GetTypeId();
    if (inputTypeId == OMNI_ARRAY) {
        ApplyReverseArray(argVector, result, outputType, context);
    } else {
        // Default: string type processing (VARCHAR, CHAR, etc.)
        ApplyReverseString(argVector, result, outputType, context);
    }

    delete argVector;
}

void ReverseFunction::ApplyReverseString(BaseVector *argVector, BaseVector *&result,
    const DataTypePtr &outputType, ExecutionContext *context) const
{
    // Get size from context (handles constant vectors correctly)
    int32_t size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);

    // Process each row
    for (int32_t row = 0; row < size; ++row) {
        // Check if input is NULL
        if (argVector->IsNull(row)) {
            result->SetNull(row);
            continue;
        }

        // Get input string value
        std::string_view inputStr = GetStringValueFromVector(argVector, row);

        // Reverse the string
        std::string reversed = ReverseString(inputStr);

        // Set the reversed result
        std::string_view resultView(reversed);
        SetStringValueToVector(result, row, const_cast<std::string_view &>(resultView));
    }
}

void ReverseFunction::ApplyReverseArray(BaseVector *argVector, BaseVector *&result,
    const DataTypePtr &outputType, ExecutionContext *context) const
{
    auto *arrayVec = dynamic_cast<ArrayVector *>(argVector);
    if (arrayVec == nullptr) {
        OMNI_THROW("Reverse function Error", "Input is not an ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();
    // Get the element vector from input array
    auto inputElementVector = arrayVec->GetElementVector();

    // Create a new ArrayVector for the result
    auto *resultArray = new ArrayVector(rowSize, context->GetPool());

    // Handle case where element vector is null (empty arrays)
    // According to Spark behavior, reverse of empty array should return empty array
    if (inputElementVector == nullptr) {
        // All arrays are empty, just set offsets to 0
        for (int32_t row = 0; row < rowSize; ++row) {
            resultArray->SetOffset(row, 0);
            if (arrayVec->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetNotNull(row);
            }
        }
        resultArray->SetOffset(rowSize, 0);
        result = resultArray;
        return;
    }

    int64_t totalElements = inputElementVector->GetSize();

    // Copy elements in reversed order for each array row
    // We need to build a new element vector with elements reordered
    BaseVector *newElementVector = nullptr;
    DataTypeId elementTypeId = inputElementVector->GetTypeId();

    // Create new element vector of the same type
    if (elementTypeId == OMNI_VARCHAR || elementTypeId == OMNI_CHAR) {
        newElementVector = VectorHelper::CreateStringVector(context->GetPool(), totalElements);
    } else {
        newElementVector = VectorHelper::CreateFlatVector(context->GetPool(), elementTypeId, totalElements);
    }

    int64_t newElementIndex = 0;

    // Process each row
    for (int32_t row = 0; row < rowSize; ++row) {
        // Set the start offset for this row in the result
        resultArray->SetOffset(row, newElementIndex);

        // Check if input is NULL
        if (arrayVec->IsNull(row)) {
            resultArray->SetNull(row);
            // For NULL arrays, we still need to advance the offset correctly
            // but we don't add any elements
            continue;
        }

        resultArray->SetNotNull(row);

        // Get the array size and offset for this row
        int64_t arraySize = arrayVec->GetSize(row);
        int64_t startOffset = arrayVec->GetOffset(row);

        // Copy elements in reverse order
        for (int64_t i = 0; i < arraySize; ++i) {
            int64_t srcIndex = startOffset + arraySize - 1 - i;  // Source index (reversed)

            // Copy element from source to destination
            if (inputElementVector->IsNull(srcIndex)) {
                newElementVector->SetNull(newElementIndex);
            } else {
                newElementVector->SetNotNull(newElementIndex);
                VectorHelper::CopyValue(inputElementVector.get(), srcIndex, newElementVector, newElementIndex);
            }
            newElementIndex++;
        }
    }

    // Set the final offset (total elements)
    resultArray->SetOffset(rowSize, newElementIndex);

    // Set the element vector for the result
    resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));

    result = resultArray;
}

std::string ReverseFunction::ReverseString(const std::string_view &input) const
{
    if (input.empty()) {
        return std::string();
    }

    // Reverse UTF-8 string character by character (not byte by byte)
    // First, collect all UTF-8 code points with their byte sequences
    std::vector<std::pair<const char*, int32_t>> codePoints;
    const char* data = input.data();
    int32_t len = static_cast<int32_t>(input.size());
    int32_t pos = 0;

    while (pos < len) {
        int32_t codePointLen = omniruntime::Utf8Util::LengthOfCodePoint(data[pos]);

        // Handle invalid UTF-8 sequences (treat as single byte)
        if (codePointLen < 0 || pos + codePointLen > len) {
            codePointLen = 1;
        }

        codePoints.push_back({data + pos, codePointLen});
        pos += codePointLen;
    }

    // Build reversed string by copying code points in reverse order
    std::string result;
    result.reserve(input.size());

    for (int64_t i = static_cast<int64_t>(codePoints.size()) - 1; i >= 0; --i) {
        const char* start = codePoints[i].first;
        int32_t size = codePoints[i].second;
        result.append(start, size);
    }

    return result;
}

std::string_view ReverseFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("Reverse function Error", "Unsupported encoding type for string");
    }
}

void ReverseFunction::SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const
{
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
}

} // namespace omniruntime::vectorization
