/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "vector/array_vector.h"
#include "vectorization/VectorReaders.h"
#include "util/debug.h"
#include <vector>
#include <string_view>
#include <re2/re2.h>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class SplitFunction final : public VectorFunction {
public:
    explicit SplitFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        auto limitArg = args.top();
        args.pop();
        auto delimiterArg = args.top();
        args.pop();
        auto inputArg = args.top();
        args.pop();

        int32_t rowSize = inputArg->GetSize();
        result = new ArrayVector(rowSize, context->GetPool());

        auto *arrayResult = dynamic_cast<ArrayVector *>(result);
        if (!arrayResult) {
            OMNI_THROW("SplitFunction Error:", "Result vector is not an ArrayVector");
        }

        ProcessAllRows(arrayResult, rowSize, inputArg, delimiterArg, limitArg);

        if (inputArg != nullptr) {
            delete inputArg;
        }
        if (delimiterArg != nullptr) {
            delete delimiterArg;
        }
        if (limitArg != nullptr) {
            delete limitArg;
        }
    }

private:
    /*
     * Single-pass approach: collect all split parts into a flat vector of string_views,
     * then build the ArrayVector at the end. For the common case of a constant delimiter,
     * the regex is compiled once and reused for all rows.
     */
    void ProcessAllRows(ArrayVector *arrayResult, int32_t rowSize, BaseVector *inputVector,
        BaseVector *delimiterVector, BaseVector *limitVector) const
    {
        std::vector<int64_t> offsets;
        offsets.reserve(rowSize + 1);
        offsets.push_back(0);

        std::vector<std::string_view> allParts;
        size_t totalBytesNeeded = 0;

        // Pre-compile regex for constant delimiter (common case optimization)
        std::unique_ptr<re2::RE2> cachedRegex;
        if (delimiterVector->GetEncoding() == OMNI_ENCODING_CONST && !IsRowNull(delimiterVector, 0)) {
            std::string_view constDelim = GetStringValue(delimiterVector, 0);
            if (!constDelim.empty()) {
                cachedRegex = CompileRegex(constDelim);
            }
        }

        for (int32_t row = 0; row < rowSize; ++row) {
            if (IsRowNull(inputVector, row) || IsRowNull(delimiterVector, row)) {
                arrayResult->SetNull(row);
                offsets.push_back(offsets.back());
                continue;
            }

            std::string_view inputStr = GetStringValue(inputVector, row);
            std::string_view delimiter = GetStringValue(delimiterVector, row);
            int32_t limit = GetInt32Value(limitVector, row);
            int32_t effectiveLimit = (limit <= 0) ? -1 : limit;

            size_t partsBefore = allParts.size();
            SplitRow(inputStr, delimiter, effectiveLimit, cachedRegex.get(), allParts);

            for (size_t i = partsBefore; i < allParts.size(); ++i) {
                totalBytesNeeded += allParts[i].size();
            }
            offsets.push_back(offsets.back() + static_cast<int64_t>(allParts.size() - partsBefore));
        }

        int64_t totalElements = static_cast<int64_t>(allParts.size());
        int32_t estimatedCapacity = static_cast<int32_t>(
            std::min(totalBytesNeeded + 64, static_cast<size_t>(INT32_MAX)));

        auto elementVector = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(
            totalElements, estimatedCapacity);
        for (int64_t i = 0; i < totalElements; ++i) {
            elementVector->SetValue(static_cast<int>(i), allParts[i]);
        }

        SetupArrayVector(arrayResult, offsets, elementVector, rowSize, inputVector, delimiterVector);
    }

    void SplitRow(std::string_view str, std::string_view delimiter, int32_t limit,
        const re2::RE2 *cachedRegex, std::vector<std::string_view> &parts) const
    {
        if (str.empty()) {
            parts.emplace_back("", 0);
            return;
        }

        if (delimiter.empty()) {
            SplitByEmptyDelimiter(str, limit, parts);
            return;
        }

        const re2::RE2 *regex = cachedRegex;
        std::unique_ptr<re2::RE2> localRegex;
        if (!regex) {
            localRegex = CompileRegex(delimiter);
            regex = localRegex.get();
        }

        if (regex && regex->ok()) {
            SplitByRegex(str, *regex, limit, parts);
        } else {
            SplitByLiteral(str, delimiter, limit, parts);
        }
    }

    void SplitByRegex(std::string_view str, const re2::RE2 &regex, int32_t limit,
        std::vector<std::string_view> &parts) const
    {
        re2::StringPiece input(str.data(), str.size());
        re2::StringPiece match;
        int32_t count = 0;
        size_t lastEnd = 0;

        while (limit == -1 || count < limit - 1) {
            if (!regex.Match(input, lastEnd, str.size(), re2::RE2::UNANCHORED, &match, 1)) {
                break;
            }

            size_t matchStart = static_cast<size_t>(match.data() - str.data());
            size_t matchLen = match.size();

            if (matchLen == 0) {
                // Zero-length match (e.g. regex "x*" matching empty): take one char and advance
                if (lastEnd < str.size()) {
                    parts.push_back(str.substr(lastEnd, 1));
                    lastEnd++;
                    count++;
                } else {
                    break;
                }
                continue;
            }

            parts.push_back(str.substr(lastEnd, matchStart - lastEnd));
            lastEnd = matchStart + matchLen;
            count++;
        }

        if (lastEnd <= str.size() && (limit == -1 || count < limit)) {
            parts.push_back(str.substr(lastEnd));
        }
    }

    void SplitByLiteral(std::string_view str, std::string_view delimiter, int32_t limit,
        std::vector<std::string_view> &parts) const
    {
        int32_t count = 0;
        size_t start = 0;
        size_t end = str.find(delimiter);

        while (end != std::string_view::npos && (limit == -1 || count < limit - 1)) {
            parts.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
            count++;
        }

        if (start <= str.length() && (limit == -1 || count < limit)) {
            parts.push_back(str.substr(start));
        }
    }

    void SplitByEmptyDelimiter(std::string_view str, int32_t limit,
        std::vector<std::string_view> &parts) const
    {
        int32_t len = static_cast<int32_t>(str.length());
        int32_t countToProcess = (limit == -1 || limit >= len) ? len : limit;
        for (int32_t i = 0; i < countToProcess; ++i) {
            parts.push_back(str.substr(i, 1));
        }
    }

    std::unique_ptr<re2::RE2> CompileRegex(std::string_view pattern) const
    {
        std::string patternStr(pattern);
        auto regex = std::make_unique<re2::RE2>(patternStr, re2::RE2::Quiet);
        if (!regex->ok()) {
            return nullptr;
        }
        return regex;
    }

    bool IsRowNull(BaseVector *vector, int32_t row) const
    {
        if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
            return vector->IsNull(0);
        }
        return vector->IsNull(row);
    }

    std::string_view GetStringValue(BaseVector *vector, int32_t row) const
    {
        switch (vector->GetEncoding()) {
            case OMNI_FLAT: {
                auto *stringVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
                return stringVector->GetValue(row);
            }
            case OMNI_DICTIONARY: {
                auto *dictVector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(
                        vector);
                return dictVector->GetValue(row);
            }
            case OMNI_ENCODING_CONST: {
                auto *constVector = static_cast<ConstVector<std::string_view> *>(vector);
                return constVector->GetConstValue();
            }
            default: OMNI_THROW("SplitFunction Error:", "Unsupported encoding type for string");
        }
    }

    int32_t GetInt32Value(BaseVector *vector, int32_t row) const
    {
        switch (vector->GetEncoding()) {
            case OMNI_FLAT: {
                return static_cast<Vector<int32_t> *>(vector)->GetValue(row);
            }
            case OMNI_ENCODING_CONST: {
                return static_cast<ConstVector<int32_t> *>(vector)->GetConstValue();
            }
            default: OMNI_THROW("SplitFunction Error:", "Unsupported encoding type for limit");
        }
    }

    void SetupArrayVector(ArrayVector *arrayResult, const std::vector<int64_t> &offsets,
        const std::shared_ptr<BaseVector> &elementVector, int32_t rowSize,
        BaseVector *inputVector, BaseVector *delimiterVector) const
    {
        for (size_t i = 0; i < offsets.size(); ++i) {
            arrayResult->SetOffset(i, offsets[i]);
        }
        arrayResult->SetElementVector(elementVector);
        for (int32_t row = 0; row < rowSize; ++row) {
            if (IsRowNull(inputVector, row) || IsRowNull(delimiterVector, row)) {
                arrayResult->SetNull(row);
            } else {
                arrayResult->SetNotNull(row);
            }
        }
    }
};
}
