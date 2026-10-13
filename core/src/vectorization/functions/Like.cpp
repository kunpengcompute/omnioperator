/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: LIKE function implementation.
 */

#include "Like.h"
#include "vector/vector.h"
#include "util/utf8_util.h"
#include <codecvt>
#include <regex>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
constexpr char kLikeWildcardMany = '%';
constexpr char kLikeWildcardOne = '_';

/// Default SQL escape (Spark / 2-arg LIKE).
constexpr char kDefaultEscapeBytes[] = "\\";
constexpr std::string_view kDefaultLikeEscape(kDefaultEscapeBytes, 1);

static std::wstring ToWideString(const std::string& s) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return convert.from_bytes(s);
}

/// Escape a single char for regex (if special). Append to out.
static void AppendRegexEscaped(char c, std::string& out) {
    if (c == '\\' || c == '.' || c == '^' || c == '$' || c == '[' || c == ']' ||
        c == '(' || c == ')' || c == '*' || c == '+' || c == '?' || c == '{' || c == '}' || c == '|') {
        out += '\\';
    }
    out += c;
}

/// Spark / Substrait may pass ESCAPE as CHAR(n): '/' padded with trailing ASCII spaces — trim before validation.
static std::string_view RTrimAsciiSpaces(std::string_view s) {
    while (!s.empty() && s.back() == ' ') {
        s.remove_suffix(1);
    }
    return s;
}

static bool ValidateSingleCharEscape(std::string_view esc, std::string_view& outSeq) {
    esc = RTrimAsciiSpaces(esc);
    if (esc.empty()) {
        return false;
    }
    int32_t cps = omniruntime::Utf8Util::CountCodePoints(esc.data(), static_cast<int32_t>(esc.size()));
    if (cps != 1) {
        return false;
    }
    outSeq = esc;
    return true;
}

static int32_t Utf8LiteralLenAfterEscape(const char* p, size_t remaining) {
    if (remaining == 0) {
        return 0;
    }
    int32_t len = omniruntime::Utf8Util::LengthOfCodePoint(*p);
    if (len < 1) {
        return 1;
    }
    if (static_cast<size_t>(len) > remaining) {
        return static_cast<int32_t>(remaining);
    }
    return len;
}

/// Convert SQL LIKE pattern to regex string. % -> .*, _ -> ., escape + next char -> literal.
static std::string LikePatternToRegex(const std::string_view& pattern, const std::string_view& escapeSeq) {
    std::string regexStr;
    regexStr.reserve(pattern.size() * 2 + 4);
    regexStr += '^';
    size_t i = 0;
    while (i < pattern.size()) {
        if (!escapeSeq.empty() && i + escapeSeq.size() <= pattern.size() &&
            pattern.compare(i, escapeSeq.size(), escapeSeq) == 0) {
            if (i + escapeSeq.size() >= pattern.size()) {
                for (size_t j = 0; j < escapeSeq.size(); ++j) {
                    AppendRegexEscaped(pattern[i + j], regexStr);
                }
                i += escapeSeq.size();
                continue;
            }
            i += escapeSeq.size();
            int32_t litLen = Utf8LiteralLenAfterEscape(pattern.data() + i, pattern.size() - i);
            for (int32_t k = 0; k < litLen; ++k) {
                AppendRegexEscaped(pattern[i + static_cast<size_t>(k)], regexStr);
            }
            i += static_cast<size_t>(litLen);
            continue;
        }
        char c = pattern[i];
        if (c == kLikeWildcardMany) {
            regexStr += ".*";
        } else if (c == kLikeWildcardOne) {
            regexStr += '.';
        } else {
            AppendRegexEscaped(c, regexStr);
        }
        ++i;
    }
    regexStr += '$';
    return regexStr;
}
}  // namespace

void LikeFunction::Apply(std::stack<BaseVector*>& args, const DataTypePtr& outputType,
                         BaseVector*& result, ExecutionContext* context) const {
    if (argumentCount_ != 2 && argumentCount_ != 3) {
        OMNI_THROW("Like function Error", "Invalid LIKE function registration arity: {}", argumentCount_);
    }
    if (args.size() < static_cast<size_t>(argumentCount_)) {
        OMNI_THROW("Like function Error", "LIKE requires {} arguments; got {}", argumentCount_, args.size());
    }
    if (argumentCount_ == 3) {
        BaseVector* escapeVec = args.top();
        args.pop();
        BaseVector* patternVec = args.top();
        args.pop();
        BaseVector* strVec = args.top();
        args.pop();
        ApplyLikeWithEscape(strVec, patternVec, escapeVec, result, outputType, context->GetPool());
        delete escapeVec;
        delete patternVec;
        delete strVec;
        return;
    }
    BaseVector* patternVec = args.top();
    args.pop();
    BaseVector* strVec = args.top();
    args.pop();
    ApplyLike(strVec, patternVec, result, outputType, context->GetPool());

    delete patternVec;
    delete strVec;
}

void LikeFunction::ApplyLike(BaseVector* strVec, BaseVector* patternVec, BaseVector*& result,
                             const DataTypePtr& outputType,
                             omniruntime::mem::OmniTaskMemoryPool *pool) const {
    int32_t size = strVec->GetSize();
    result = VectorHelper::CreateFlatVector(pool, outputType->GetId(), size);

    if (patternVec->GetEncoding() == OMNI_ENCODING_CONST) {
        std::string_view patternSv;
        if (patternVec->GetTypeId() == OMNI_STRING_VIEW) {
            const StringView& pattern = static_cast<ConstVector<StringView>*>(patternVec)->GetConstValueRef();
            patternSv = pattern;
        } else {
            patternSv = reinterpret_cast<ConstVector<std::string_view>*>(patternVec)->GetConstValue();
        }
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view str = GetStringValueFromVector(strVec, row);
            bool matches = MatchLike(str, patternSv);
            SetBooleanValueToVector(result, row, matches);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row) || patternVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view str = GetStringValueFromVector(strVec, row);
            std::string_view patternSv = GetStringValueFromVector(patternVec, row);
            bool matches = MatchLike(str, patternSv);
            SetBooleanValueToVector(result, row, matches);
        }
    }
}

void LikeFunction::ApplyLikeWithEscape(BaseVector* strVec, BaseVector* patternVec, BaseVector* escapeVec,
                                       BaseVector*& result, const DataTypePtr& outputType,
                                       omniruntime::mem::OmniTaskMemoryPool *pool) const {
    int32_t size = strVec->GetSize();
    result = VectorHelper::CreateFlatVector(pool, outputType->GetId(), size);

    if (patternVec->GetEncoding() == OMNI_ENCODING_CONST && patternVec->IsNull(0)) {
        for (int32_t row = 0; row < size; ++row) {
            result->SetNull(row);
        }
        return;
    }

    std::optional<std::string_view> constEscape;
    if (escapeVec->GetEncoding() == OMNI_ENCODING_CONST) {
        if (escapeVec->IsNull(0)) {
            for (int32_t row = 0; row < size; ++row) {
                result->SetNull(row);
            }
            return;
        }
        std::string_view esc;
        if (escapeVec->GetTypeId() == OMNI_STRING_VIEW) {
            const StringView& escape = static_cast<ConstVector<StringView>*>(escapeVec)->GetConstValueRef();
            esc = escape;
        } else {
            esc = reinterpret_cast<ConstVector<std::string_view>*>(escapeVec)->GetConstValue();
        }
        std::string_view escSeq;
        if (!ValidateSingleCharEscape(esc, escSeq)) {
            OMNI_THROW("Like function Error", "Escape string must be a single character");
        }
        constEscape = escSeq;
    }

    if (patternVec->GetEncoding() == OMNI_ENCODING_CONST) {
        std::string_view patternSv;
        if (patternVec->GetTypeId() == OMNI_STRING_VIEW) {
            const StringView& pattern = static_cast<ConstVector<StringView>*>(patternVec)->GetConstValueRef();
            patternSv = pattern;
        } else {
            patternSv = reinterpret_cast<ConstVector<std::string_view>*>(patternVec)->GetConstValue();
        }
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row) || patternVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view str = GetStringValueFromVector(strVec, row);
            std::string_view escSeq;
            if (constEscape.has_value()) {
                escSeq = *constEscape;
            } else {
                if (escapeVec->IsNull(row)) {
                    result->SetNull(row);
                    continue;
                }
                std::string_view esc = GetStringValueFromVector(escapeVec, row);
                if (!ValidateSingleCharEscape(esc, escSeq)) {
                    OMNI_THROW("Like function Error", "Escape string must be a single character");
                }
            }
            bool matches = MatchLike(str, patternSv, escSeq);
            SetBooleanValueToVector(result, row, matches);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row) || patternVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view escSeq;
            if (constEscape.has_value()) {
                escSeq = *constEscape;
            } else {
                if (escapeVec->IsNull(row)) {
                    result->SetNull(row);
                    continue;
                }
                std::string_view esc = GetStringValueFromVector(escapeVec, row);
                if (!ValidateSingleCharEscape(esc, escSeq)) {
                    OMNI_THROW("Like function Error", "Escape string must be a single character");
                }
            }
            std::string_view str = GetStringValueFromVector(strVec, row);
            std::string_view patternSv = GetStringValueFromVector(patternVec, row);
            bool matches = MatchLike(str, patternSv, escSeq);
            SetBooleanValueToVector(result, row, matches);
        }
    }
}

bool LikeFunction::MatchLike(const std::string_view& str, const std::string_view& pattern) const {
    return MatchLike(str, pattern, kDefaultLikeEscape);
}

bool LikeFunction::MatchLike(const std::string_view& str, const std::string_view& pattern,
                             const std::string_view& escapeSeq) const {
    std::string strCopy(str);
    std::string regexStr = LikePatternToRegex(pattern, escapeSeq);
    try {
        std::wstring wRegex = ToWideString(regexStr);
        std::wstring wStr = ToWideString(strCopy);
        std::wregex re(wRegex);
        return std::regex_match(wStr, re);
    } catch (const std::regex_error& e) {
        OMNI_THROW("Like regex Error: ", e.what());
    }
}

std::string_view LikeFunction::GetStringValueFromVector(BaseVector* vec, int32_t row) {
    Encoding encoding = vec->GetEncoding();
    if (vec->GetTypeId() == OMNI_STRING_VIEW) {
        // StringView column/constant: a 16B fixed-width value type. Take a reference into the buffer/const and then
        // convert to std::string_view — converting a copy (GetValue/GetConstValue) would leave the string_view of an
        // inline (<=12B) string dangling.
        if (encoding == OMNI_ENCODING_CONST) {
            return static_cast<ConstVector<StringView>*>(vec)->GetConstValueRef();
        }
        if (encoding == OMNI_FLAT) {
            return static_cast<Vector<StringView>*>(vec)->GetValueRef(row);
        }
        OMNI_THROW("Like function Error", "Unsupported encoding for OMNI_STRING_VIEW string");
    }
    if (encoding == OMNI_ENCODING_CONST) {
        auto* constVec = static_cast<ConstVector<std::string_view>*>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto* flatVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto* dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>>*>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("Like function Error", "Unsupported encoding type for string");
}

void LikeFunction::SetBooleanValueToVector(BaseVector* vec, int32_t row, bool value) {
    auto* resultVec = static_cast<Vector<bool>*>(vec);
    resultVec->SetValue(row, value);
}
}  // namespace omniruntime::vectorization
