/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RLike function implementation
 */

#include "RLike.h"
#include "vector/vector.h"
#include <limits>
#include <cstring>
#include <regex>
#include <re2/re2.h>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

bool RLikeFunction::HasRegexMetaChars(const std::string_view &pattern) const {
    for (char c : pattern) {
        switch (c) {
            case '\\':
            case '.':
            case '^':
            case '$':
            case '|':
            case '?':
            case '*':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
                return true;
            default:
                break;
        }
    }
    return false;
}

void RLikeFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                          BaseVector *&result, ExecutionContext *context) const {
    auto patternVec = args.top();
    args.pop();
    auto strVec = args.top();
    args.pop();

    ApplyRLike(strVec, patternVec, result, outputType);

    delete patternVec;
    delete strVec;
}

void RLikeFunction::ApplyRLike(BaseVector *strVec, BaseVector *patternVec, BaseVector *&result,
                               const DataTypePtr &outputType) const {
    auto size = strVec->GetSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);

    if(patternVec->GetEncoding() == OMNI_ENCODING_CONST) {
        std::string_view pattern = reinterpret_cast<ConstVector<std::string_view> *>(patternVec)->GetConstValue();
        bool useLiteralContains = !pattern.empty() && !HasRegexMetaChars(pattern);
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view str = GetStringValueFromVector(strVec, row);

            bool matches = useLiteralContains ? (str.find(pattern) != std::string_view::npos) : MatchRegex(str, pattern);
            SetBooleanValueToVector(result, row, matches);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row) || patternVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            std::string_view str = GetStringValueFromVector(strVec, row);
            std::string_view pattern = GetStringValueFromVector(patternVec, row);

            bool matches = MatchRegex(str, pattern);
            SetBooleanValueToVector(result, row, matches);
        }
    }
}

bool RLikeFunction::MatchRegex(const std::string_view &sv, const std::string_view &pattern) const {
    if (pattern.empty()) {
        return true;
    }
    thread_local std::string cachedPattern;
    thread_local std::unique_ptr<RE2> cachedRegex;
    if (std::string_view(cachedPattern) != pattern) {
        cachedPattern.assign(pattern.data(), pattern.size());
        cachedRegex = std::make_unique<RE2>(re2::StringPiece(cachedPattern.data(), cachedPattern.length()), RE2::Quiet);
        if (!cachedRegex->ok()) {
            OMNI_THROW("RLike regex search Error: ", cachedRegex->error());
        }
    }
    return RE2::PartialMatch(re2::StringPiece(sv.data(), sv.length()), *cachedRegex.get());
}

std::string_view RLikeFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
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
        OMNI_THROW("RLike function Error", "Unsupported encoding type for string");
    }
}

void RLikeFunction::SetBooleanValueToVector(BaseVector *vec, int32_t row, bool value) const {
    auto *resultVec = static_cast<Vector<bool> *>(vec);
    resultVec->SetValue(row, value);
}
}
