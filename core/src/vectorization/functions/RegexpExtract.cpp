/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RegexpExtract function implementation
 */
#include <algorithm>
#include <array>
#include <limits>
#include <cstring>
#include <memory>
#include <string_view>
#include <re2/re2.h>

#include "RegexpExtract.h"
#include "vector/vector.h"
#include "type/string_Impl.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

    // Per-regex RE2 memory budget.
    static constexpr int64_t RE2_MAX_MEM_PER_REGEX = 256 * 1024;

    // Skip prefix pre-location for short literals.
    static constexpr size_t RE2_MIN_PREFIX_ACCEL = 3;

    static constexpr std::array<bool, 128> GetMetaCharTable() {
        std::array<bool, 128> arr{};
        const char meta[] = R"(\.+*?^$()[]{}|)";
        for (char c : meta) {
            arr[static_cast<uint8_t>(c)] = true;
        }
        return arr;
    }
    static constexpr std::array<bool, 128> g_re_meta_chars = GetMetaCharTable();

namespace {
    // Small per-thread cache for frequently used patterns.
    constexpr size_t TLS_RE2_CACHE_SIZE = 8;

    struct TlsRe2Entry {
        std::string key;
        // Thread-private RE2 avoids sharing its lazily built DFA across workers.
        std::shared_ptr<const re2::RE2> re;
    };

    static thread_local std::vector<TlsRe2Entry> tls_re2_cache;

    // Reused per thread to avoid per-row allocation.
    static thread_local std::vector<re2::StringPiece> tls_submatch;

    std::shared_ptr<const re2::RE2> CompileRe2(const std::string &key)
    {
        re2::RE2::Options options;
        options.set_log_errors(false);
        options.set_max_mem(RE2_MAX_MEM_PER_REGEX);
        auto re_uptr = std::make_shared<re2::RE2>(stringImpl::toStringPiece(key), options);
        if (!re_uptr->ok()) {
            OMNI_THROW("regex expr compile failed, patternStr: ", key);
        }
        return re_uptr;
    }

    std::shared_ptr<const re2::RE2> GetOrCompileRe2(const std::string_view &pattern)
    {
        for (const auto &entry : tls_re2_cache) {
            if (entry.key.size() == pattern.size() &&
                memcmp(entry.key.data(), pattern.data(), pattern.size()) == 0) {
                return entry.re;
            }
        }

        // Compile a thread-private RE2.
        std::string key(pattern);
        std::shared_ptr<const re2::RE2> re_sp = CompileRe2(key);
        if (tls_re2_cache.size() >= TLS_RE2_CACHE_SIZE) {
            tls_re2_cache.erase(tls_re2_cache.begin());
        }
        tls_re2_cache.push_back(TlsRe2Entry{std::move(key), re_sp});
        return re_sp;
    }

    bool IsLiteralRegex(std::string_view pattern)
    {
        for (char c : pattern) {
            const auto uc = static_cast<uint8_t>(c);
            // Non-ASCII input still goes through RE2 so malformed UTF-8 is
            // rejected consistently with the generic path.
            if (uc >= 128 || g_re_meta_chars[uc]) {
                return false;
            }
        }
        return true;
    }

    // Reused storage for prefixes spanning `\Q...\E` quoted segments.
    static thread_local std::string tls_quoted_literal_prefix;

    // Return a guaranteed literal prefix, or empty if none exists. The view may use thread-local
    // storage and is valid only until the next call on this thread.
    std::string_view LiteralPrefixOf(std::string_view pattern)
    {
        // Top-level alternation prevents deriving a prefix; quoted metacharacters are literals.
        int32_t depth = 0;
        bool inClass = false;
        bool inQuote = false;
        for (size_t i = 0; i < pattern.size(); ++i) {
            const char c = pattern[i];
            if (inQuote) {
                if (c == '\\' && i + 1 < pattern.size() && pattern[i + 1] == 'E') {
                    inQuote = false;
                    ++i;
                }
                continue;
            }
            if (inClass) {
                if (c == '\\') {
                    ++i;
                } else if (c == ']') {
                    inClass = false;
                }
                continue;
            }
            if (c == '\\') {
                if (i + 1 < pattern.size() && pattern[i + 1] == 'Q') {
                    inQuote = true;
                }
                ++i;
                continue;
            }
            if (c == '[') {
                inClass = true;
            } else if (c == '(') {
                ++depth;
            } else if (c == ')' && depth > 0) {
                --depth;
            } else if (c == '|' && depth == 0) {
                return std::string_view();
            }
        }

        // Expand quoted literals while collecting the leading run.
        tls_quoted_literal_prefix.clear();
        bool quoted = false;  // set once `\Q` is seen, i.e. the run is not contiguous in pattern
        inQuote = false;
        size_t i = 0;
        while (i < pattern.size()) {
            const char c = pattern[i];
            if (inQuote) {
                if (c == '\\' && i + 1 < pattern.size() && pattern[i + 1] == 'E') {
                    inQuote = false;
                    i += 2;
                    continue;
                }
                tls_quoted_literal_prefix.push_back(c);
                ++i;
                continue;
            }
            if (c == '\\' && i + 1 < pattern.size() && pattern[i + 1] == 'Q') {
                if (!quoted) {
                    tls_quoted_literal_prefix.assign(pattern.data(), i);
                    quoted = true;
                }
                inQuote = true;
                i += 2;
                continue;
            }
            const auto uc = static_cast<uint8_t>(c);
            if (uc >= 128 || g_re_meta_chars[uc]) {
                break;
            }
            if (quoted) {
                tls_quoted_literal_prefix.push_back(c);
            }
            ++i;
        }

        // Drop a byte made optional by '?', '*' or '{'.
        const bool optionalNext = i < pattern.size() &&
            (pattern[i] == '?' || pattern[i] == '*' || pattern[i] == '{');
        if (quoted) {
            if (optionalNext && !tls_quoted_literal_prefix.empty()) {
                tls_quoted_literal_prefix.pop_back();
            }
            return tls_quoted_literal_prefix;
        }
        if (i == 0 || i == pattern.size()) {
            return pattern.substr(0, i);
        }
        if (optionalNext) {
            --i;
        }
        return pattern.substr(0, i);
    }

    // For a sole outer capture and group 1, unwrap only for prefix detection; retain the original
    // pattern for RE2 capture matching.
    bool TryStripSingleOuterGroup(std::string_view pattern, int32_t idx, std::string_view &inner)
    {
        if (idx != 1 || pattern.size() < 3 || pattern.front() != '(' || pattern[1] == '?') {
            return false;
        }
        int32_t depth = 0;
        bool inClass = false;
        for (size_t i = 0; i < pattern.size(); ++i) {
            const char c = pattern[i];
            if (c == '\\') {
                ++i;
                continue;
            }
            if (inClass) {
                if (c == ']') {
                    inClass = false;
                }
                continue;
            }
            if (c == '[') {
                inClass = true;
            } else if (c == '(') {
                ++depth;
            } else if (c == ')') {
                --depth;
                if (depth == 0) {
                    // The outer group must close exactly at the end for it to span everything.
                    if (i != pattern.size() - 1) {
                        return false;
                    }
                    inner = pattern.substr(1, pattern.size() - 2);
                    return !inner.empty();
                }
                if (depth < 0) {
                    return false;
                }
            }
        }
        return false;
    }

    std::string_view ExtractWithCompiledRe2(std::string_view input, const re2::RE2 &re, int32_t idx,
        std::string_view prefix = std::string_view())
    {
        if (idx < 0 || idx > re.NumberOfCapturingGroups()) {
            return {};
        }

        const size_t groupCount = static_cast<size_t>(idx) + 1;
        if (tls_submatch.size() < groupCount) {
            tls_submatch.resize(groupCount);
        }
        re2::StringPiece *groups = tls_submatch.data();
        for (size_t i = 0; i < groupCount; ++i) {
            groups[i] = re2::StringPiece();
        }

        const re2::StringPiece inputPiece(input.data(), input.size());
        bool matched = false;
        if (prefix.empty()) {
            matched = re.Match(inputPiece, 0, input.size(), RE2::UNANCHORED, groups,
                static_cast<int>(groupCount));
        } else {
            // Try anchored matches at prefix occurrences, then use one unanchored search after the
            // candidate budget is exhausted.
            constexpr int32_t MAX_ANCHORED_CANDIDATES = 64;
            size_t searchFrom = 0;
            size_t lastCandidate = 0;
            int32_t attempts = 0;
            while (!matched && attempts < MAX_ANCHORED_CANDIDATES) {
                const size_t pos = input.find(prefix, searchFrom);
                if (pos == std::string_view::npos) {
                    return {};
                }
                ++attempts;
                lastCandidate = pos;
                matched = re.Match(inputPiece, pos, input.size(), RE2::ANCHOR_START, groups,
                    static_cast<int>(groupCount));
                searchFrom = pos + 1;
            }
            if (!matched) {
                matched = re.Match(inputPiece, lastCandidate, input.size(), RE2::UNANCHORED, groups,
                    static_cast<int>(groupCount));
            }
        }

        if (!matched) {
            return {};
        }
        const re2::StringPiece extracted = groups[idx];
        return extracted.empty() ? std::string_view() :
            std::string_view(extracted.data(), extracted.size());
    }
} // namespace

namespace {
// Deletes a vector only when Apply actually owns it; field references are borrowed and must not
// be freed. Shared by the input guards and the result guard so every error path is leak-free.
void DeleteOwnedVector(BaseVector *v)
{
    if (v != nullptr && !v->GetIsField()) {
        delete v;
    }
}
} // namespace

void RegexpExtractFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const {
    BaseVector *groupIdxVec = nullptr;
    BaseVector *patternVec = nullptr;
    BaseVector *strVec = nullptr;
    groupIdxVec = args.top();
    args.pop();
    patternVec = args.top();
    args.pop();
    strVec = args.top();
    args.pop();

    // The vector layer transfers ownership of the inputs to Apply. Release them on every path,
    // including the exception thrown for an invalid regex, instead of leaking on the error path.
    using VectorGuard = std::unique_ptr<BaseVector, void (*)(BaseVector *)>;
    VectorGuard groupIdxGuard(groupIdxVec, &DeleteOwnedVector);
    VectorGuard patternGuard(patternVec, &DeleteOwnedVector);
    VectorGuard strGuard(strVec, &DeleteOwnedVector);

    ApplyRegexpExtract(strVec, patternVec, groupIdxVec, result, outputType, context);
}

void RegexpExtractFunction::ApplyRegexpExtract(BaseVector *strVec, BaseVector *patternVec,
    BaseVector *groupIdxVec, BaseVector *&result, const DataTypePtr &outputType, ExecutionContext *context) const {
    auto size = context->GetResultRowSize();
    // Hold the freshly created result in a guard until every row has been processed, so an
    // invalid-regex exception does not leak the partially filled output vector either.
    std::unique_ptr<BaseVector, void (*)(BaseVector *)> resultGuard(
        VectorHelper::CreateFlatVector(outputType->GetId(), size), &DeleteOwnedVector);
    BaseVector *created = resultGuard.get();

    // Nothing to evaluate: return the empty result before touching row 0 of the pattern / group
    // vectors (a zero-length flat vector has no row 0), compiling the regex or rejecting an empty
    // pattern. LikeFunction::ApplyLike does the same.
    if (size == 0) {
        result = resultGuard.release();
        return;
    }

    // Accumulate views into the still-live input vectors, then reserve/copy the
    // output payload once.  SetValue row-by-row repeatedly grows the string
    // container and is especially expensive for large batches.
    std::vector<std::string_view> extractedValues(static_cast<size_t>(size));
    std::vector<uint8_t> nulls(static_cast<size_t>(size), 0);
    uint64_t totalBytes = 0;

    const bool hasConstPatternAndGroup = patternVec->GetEncoding() == OMNI_ENCODING_CONST &&
        groupIdxVec->GetEncoding() == OMNI_ENCODING_CONST;

    if (hasConstPatternAndGroup && (patternVec->IsNull(0) || groupIdxVec->IsNull(0))) {
        std::fill(nulls.begin(), nulls.end(), 1);
    } else if (hasConstPatternAndGroup) {
        // This is the normal SQL shape: both regex and group index are
        // literals.  Classify/compile them once outside the row loop.
        const std::string_view pattern = GetStringValueFromVector(patternVec, 0);
        const int32_t groupIdx = GetIntValueFromVector(groupIdxVec, 0);
        if (pattern.empty()) {
            throw omniruntime::exception::OmniException("patternStr must not empty");
        }

        // Keep the original pattern and requested capture group so RE2 runs its capture/one-pass
        // engine (the same engine Velox uses) instead of the pure-DFA path, which pays per-byte
        // atomic state loads, DFA read/write locks and SearchFFT/state-flush overhead. The outer
        // group is unwrapped only to derive the literal prefix, because a leading "(" would
        // otherwise hide the literal start from LiteralPrefixOf.
        std::string_view patternForPrefix = pattern;
        std::string_view innerPattern;
        if (TryStripSingleOuterGroup(pattern, groupIdx, innerPattern)) {
            patternForPrefix = innerPattern;
        }

        const bool isLiteral = IsLiteralRegex(pattern);
        std::shared_ptr<const re2::RE2> compiled;
        if (groupIdx >= 0 && (!isLiteral || groupIdx == 0)) {
            if (!isLiteral) {
                compiled = GetOrCompileRe2(pattern);
            }
        }

        // Pre-locate the leading literal (e.g. "mr_attempt_", "application_") so the search only
        // starts from real candidate positions instead of running the engine at every character.
        std::string_view literalPrefix;
        if (!isLiteral) {
            const std::string_view candidate = LiteralPrefixOf(patternForPrefix);
            if (candidate.size() >= RE2_MIN_PREFIX_ACCEL) {
                literalPrefix = candidate;
            }
        }

        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row)) {
                nulls[static_cast<size_t>(row)] = 1;
                continue;
            }

            const std::string_view input = GetStringValueFromVector(strVec, row);
            std::string_view extracted;
            if (groupIdx >= 0) {
                if (isLiteral) {
                    if (groupIdx == 0) {
                        const size_t position = input.find(pattern);
                        if (position != std::string_view::npos) {
                            extracted = input.substr(position, pattern.size());
                        }
                    }
                } else {
                    extracted = ExtractWithCompiledRe2(input, *compiled, groupIdx, literalPrefix);
                }
            }
            extractedValues[static_cast<size_t>(row)] = extracted;
            totalBytes += extracted.size();
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            // A NULL in any input propagates to the result, including the group index.
            if (strVec->IsNull(row) || patternVec->IsNull(row) || groupIdxVec->IsNull(row)) {
                nulls[static_cast<size_t>(row)] = 1;
                continue;
            }
            const std::string_view input = GetStringValueFromVector(strVec, row);
            const std::string_view pattern = GetStringValueFromVector(patternVec, row);
            const int32_t groupIdx = GetIntValueFromVector(groupIdxVec, row);

            const std::string_view extracted = ExtractRegex(input, pattern, groupIdx);
            extractedValues[static_cast<size_t>(row)] = extracted;
            totalBytes += extracted.size();
        }
    }

    auto *resultVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(created);
    resultVector->SetValues(0, extractedValues.data(), nulls.data(), size, totalBytes);
    result = resultGuard.release();
}

std::string_view RegexpExtractFunction::ExtractRegex(const std::string_view &plainStr,
    const std::string_view &patternStr, int32_t idx) const {
    if (patternStr.size() == 0) {
        throw omniruntime::exception::OmniException("patternStr must not empty");
    }
    if (idx < 0) {
        return std::string_view();
    }

    // Keep the original pattern and requested group so RE2 uses its capture/one-pass engine
    // (matching Velox) rather than the pure-DFA path; unwrap the outer group only to derive the
    // literal prefix used for pre-location.
    std::string_view patternForPrefix = patternStr;
    std::string_view innerPattern;
    if (TryStripSingleOuterGroup(patternStr, idx, innerPattern)) {
        patternForPrefix = innerPattern;
    }

    // Fast path: a pure literal pattern needs no regex engine. The returned
    // view points into the input, matching the generic RE2 path's lifetime.
    if (IsLiteralRegex(patternStr)) {
        if (idx == 0) {
            const size_t position = plainStr.find(patternStr);
            if (position != std::string_view::npos) {
                return plainStr.substr(position, patternStr.size());
            }
        }
        return std::string_view();
    }

    auto re_sp = GetOrCompileRe2(patternStr);
    const std::string_view candidate = LiteralPrefixOf(patternForPrefix);
    const std::string_view prefix =
        candidate.size() >= RE2_MIN_PREFIX_ACCEL ? candidate : std::string_view();
    return ExtractWithCompiledRe2(plainStr, *re_sp, idx, prefix);
}

std::string_view RegexpExtractFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
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
        OMNI_THROW("RegexpExtract function Error", "Unsupported encoding type for string");
    }
}

int32_t RegexpExtractFunction::GetIntValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<int32_t> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<int32_t> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<int32_t>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("RegexpExtract function Error", "Unsupported encoding type for integer");
    }
}

void RegexpExtractFunction::SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const {
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
}
}
