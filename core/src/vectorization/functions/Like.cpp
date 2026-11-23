/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: LIKE function implementation.
 */

#include "Like.h"
#include "vector/vector.h"
#include "util/utf8_util.h"
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <re2/re2.h>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
constexpr char kLikeWildcardMany = '%';
constexpr char kLikeWildcardOne = '_';

/// Default SQL escape (Spark / 2-arg LIKE).
constexpr char kDefaultEscapeBytes[] = "\\";
constexpr std::string_view kDefaultLikeEscape(kDefaultEscapeBytes, 1);

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
    regexStr.reserve(pattern.size() * 2 + 6);
    // RE2::ANCHOR_BOTH anchors the full match; (?s) makes wildcards include newlines.
    regexStr += "(?s)";
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
    return regexStr;
}

// RE2 treats input as UTF-8; `.` therefore matches one code point for `_`.
constexpr int64_t kLikeMaxMemPerRegex = 256 * 1024;
constexpr size_t kLikeTlsCacheSize = 32;

/// Bound candidate checks before falling back to RE2.
constexpr int32_t kRelaxedMaxStartAttempts = 64;

std::shared_ptr<const re2::RE2> CompileLikeRegex(const std::string& regexStr) {
    re2::RE2::Options options;
    options.set_log_errors(false);
    options.set_max_mem(kLikeMaxMemPerRegex);
    return std::make_shared<re2::RE2>(re2::StringPiece(regexStr.data(), regexStr.size()), options);
}

struct LikeRegexCacheEntry {
    std::string regexKey;
    std::shared_ptr<const re2::RE2> re;
};

// Per-thread hot cache: LIKE is normally evaluated with a handful of repeated
// patterns, so this avoids recompiling the same pattern on every row.
static thread_local std::vector<LikeRegexCacheEntry> tls_like_regex_cache;

std::shared_ptr<const re2::RE2> GetOrCompileLikeRegex(const std::string_view& pattern,
                                                      const std::string_view& escapeSeq) {
    std::string regexStr = LikePatternToRegex(pattern, escapeSeq);
    for (const auto& entry : tls_like_regex_cache) {
        if (entry.regexKey == regexStr) {
            return entry.re;
        }
    }
    std::shared_ptr<const re2::RE2> re = CompileLikeRegex(regexStr);
    if (!re->ok()) {
        OMNI_THROW("Like regex Error: ", "{}", re->error());
    }
    if (tls_like_regex_cache.size() >= kLikeTlsCacheSize) {
        tls_like_regex_cache.erase(tls_like_regex_cache.begin());
    }
    tls_like_regex_cache.push_back(LikeRegexCacheEntry{std::move(regexStr), re});
    return re;
}

/// Full-string match: ANCHOR_BOTH is the RE2 equivalent of std::regex_match.
bool MatchWhole(const std::shared_ptr<const re2::RE2>& re, const std::string_view& value) {
    const re2::StringPiece input(value.data(), value.size());
    return re->Match(input, 0, input.size(), re2::RE2::ANCHOR_BOTH, nullptr, 0);
}

/// Parsed pattern and fast-path classification for constant LIKE patterns.
enum class LikePatternKind {
    kFixed,             // no wildcard: literal equality
    kPrefix,            // "<literal>%...%"
    kSuffix,            // "%...%<literal>"
    kSubstring,         // "%...%<literal>%...%"
    kRelaxedFixed,      // literals + `_` only: relaxed equality
    kRelaxedPrefix,     // "<body>%...%" where body mixes literals and `_`
    kRelaxedSuffix,     // "%...%<body>" where body mixes literals and `_`
    kRelaxedSubstring,  // "%...%<body>%...%" where body mixes literals and `_`
    kGeneric,           // anything else: fall back to RE2
};

/// One token of a relaxed body: either a run of literal bytes or a run of `_` wildcards.
struct RelaxedSegment {
    bool isWildcard = false;  // true: `_` run; false: literal run
    int32_t length = 0;       // wildcard: number of `_`; literal: byte length
    int32_t offset = 0;       // literal only: offset into LikePatternInfo::literal
};

struct LikePatternInfo {
    LikePatternKind kind = LikePatternKind::kGeneric;
    /// Escape-resolved literal bytes contributing to the fast-path comparison (wildcards and the
    /// escape characters themselves are stripped). For relaxed kinds this is the concatenation of
    /// every literal segment, described positionally by `segments`.
    std::string literal;
    /// Layout of the relaxed body for the `kRelaxed*` kinds (empty for the pure-literal kinds).
    std::vector<RelaxedSegment> segments;
    /// Byte span of the relaxed body: a `_` consumes 1..4 UTF-8 bytes and a literal consumes its
    /// byte length. Used to restrict the candidate start offsets of the suffix / substring matchers
    /// instead of probing every byte position.
    int32_t minBodyBytes = 0;
    int32_t maxBodyBytes = 0;
};

/// Parse a LIKE pattern and classify supported fast-path shapes.
LikePatternInfo ClassifyLikePattern(const std::string_view& pattern, const std::string_view& escapeSeq) {
    LikePatternInfo info;
    std::string& literal = info.literal;
    std::vector<RelaxedSegment>& segments = info.segments;

    std::string pendingLiteral;   // literal bytes not yet flushed into a segment
    int32_t pendingWildcard = 0;  // consecutive `_` not yet flushed into a segment
    bool sawUnderscore = false;
    int percentRuns = 0;           // number of separate runs of consecutive '%'
    bool firstRunLeading = false;  // the leading '%' run begins at the start of the pattern
    bool endsWithPercent = false;  // the last token consumed was a '%' run
    bool inPercentRun = false;

    auto flushLiteral = [&]() {
        if (!pendingLiteral.empty()) {
            segments.push_back(RelaxedSegment{false, static_cast<int32_t>(pendingLiteral.size()),
                                              static_cast<int32_t>(literal.size())});
            literal += pendingLiteral;
            pendingLiteral.clear();
        }
    };
    auto flushWildcard = [&]() {
        if (pendingWildcard > 0) {
            segments.push_back(RelaxedSegment{true, pendingWildcard, 0});
            pendingWildcard = 0;
        }
    };

    size_t i = 0;
    while (i < pattern.size()) {
        if (!escapeSeq.empty() && pattern.size() - i >= escapeSeq.size() &&
            pattern.compare(i, escapeSeq.size(), escapeSeq) == 0) {
            i += escapeSeq.size();
            flushWildcard();
            if (i < pattern.size()) {
                int32_t litLen = Utf8LiteralLenAfterEscape(pattern.data() + i, pattern.size() - i);
                pendingLiteral.append(pattern.data() + i, static_cast<size_t>(litLen));
                i += static_cast<size_t>(litLen);
            } else {
                // Trailing escape with no following char: keep the escape bytes literal, matching
                // LikePatternToRegex.
                pendingLiteral.append(escapeSeq);
            }
            inPercentRun = false;
            endsWithPercent = false;
            continue;
        }
        char c = pattern[i];
        if (c == kLikeWildcardMany) {
            flushLiteral();
            flushWildcard();
            if (!inPercentRun) {
                ++percentRuns;
                if (i == 0) {
                    firstRunLeading = true;
                }
                inPercentRun = true;
            }
            endsWithPercent = (i + 1 == pattern.size());
            ++i;
        } else if (c == kLikeWildcardOne) {
            sawUnderscore = true;
            flushLiteral();
            ++pendingWildcard;
            inPercentRun = false;
            endsWithPercent = false;
            ++i;
        } else {
            flushWildcard();
            pendingLiteral += c;
            inPercentRun = false;
            endsWithPercent = false;
            ++i;
        }
    }
    flushLiteral();
    flushWildcard();

    for (const RelaxedSegment& seg : segments) {
        info.minBodyBytes += seg.length;
        info.maxBodyBytes += seg.isWildcard ? seg.length * 4 : seg.length;
    }

    if (segments.empty()) {
        // Only `%` runs (or an empty pattern): "%"-only matches everything, "" matches only "".
        info.kind = (percentRuns == 0) ? LikePatternKind::kFixed : LikePatternKind::kPrefix;
        info.literal.clear();
        return info;
    }

    if (percentRuns == 0) {
        info.kind = sawUnderscore ? LikePatternKind::kRelaxedFixed : LikePatternKind::kFixed;
    } else if (percentRuns == 1 && firstRunLeading && !endsWithPercent) {
        info.kind = sawUnderscore ? LikePatternKind::kRelaxedSuffix : LikePatternKind::kSuffix;  // "%<body>"
    } else if (percentRuns == 1 && !firstRunLeading && endsWithPercent) {
        info.kind = sawUnderscore ? LikePatternKind::kRelaxedPrefix : LikePatternKind::kPrefix;  // "<body>%"
    } else if (percentRuns == 2 && firstRunLeading && endsWithPercent) {
        info.kind = sawUnderscore ? LikePatternKind::kRelaxedSubstring : LikePatternKind::kSubstring;  // "%<body>%"
    } else {
        // Internal '%' (e.g. "<lit>%<lit>") or more than two boundary runs: no fast path.
        info.kind = LikePatternKind::kGeneric;
        info.segments.clear();
        info.literal.clear();
    }
    return info;
}

bool MatchFixed(const std::string_view& str, const std::string_view& literal) {
    return str.size() == literal.size() &&
        (literal.empty() || memcmp(str.data(), literal.data(), literal.size()) == 0);
}

bool MatchPrefix(const std::string_view& str, const std::string_view& literal) {
    return str.size() >= literal.size() &&
        (literal.empty() || memcmp(str.data(), literal.data(), literal.size()) == 0);
}

bool MatchSuffix(const std::string_view& str, const std::string_view& literal) {
    return str.size() >= literal.size() &&
        (literal.empty() || memcmp(str.data() + str.size() - literal.size(), literal.data(), literal.size()) == 0);
}

bool MatchSubstring(const std::string_view& str, const std::string_view& literal) {
    return literal.empty() || str.find(literal) != std::string_view::npos;
}

/// Match a relaxed body at `start`; `_` consumes one UTF-8 code point.
bool MatchRelaxedForward(const std::string_view& str, size_t start,
                         const LikePatternInfo& info, size_t& endPos) {
    size_t cursor = start;
    for (const RelaxedSegment& seg : info.segments) {
        if (seg.isWildcard) {
            for (int32_t k = 0; k < seg.length; ++k) {
                if (cursor >= str.size()) {
                    return false;
                }
                int32_t cpLen = omniruntime::Utf8Util::LengthOfCodePoint(str[cursor]);
                if (cpLen < 1) {
                    cpLen = 1;
                }
                if (cursor + static_cast<size_t>(cpLen) > str.size()) {
                    return false;  // truncated multi-byte code point
                }
                cursor += static_cast<size_t>(cpLen);
            }
        } else {
            const size_t len = static_cast<size_t>(seg.length);
            if (cursor + len > str.size() ||
                (len > 0 && memcmp(str.data() + cursor, info.literal.data() + seg.offset, len) != 0)) {
                return false;
            }
            cursor += len;
        }
    }
    endPos = cursor;
    return true;
}

/// "<body>%...%": the relaxed body must match a prefix.
bool MatchRelaxedPrefix(const std::string_view& str, const LikePatternInfo& info) {
    size_t endPos = 0;
    return MatchRelaxedForward(str, 0, info, endPos);
}

/// "<body>" only: the relaxed body must match the whole string.
bool MatchRelaxedFixed(const std::string_view& str, const LikePatternInfo& info) {
    size_t endPos = 0;
    return MatchRelaxedForward(str, 0, info, endPos) && endPos == str.size();
}

/// Match a suffix within the body's byte-length window; set gaveUp when the budget runs out.
bool MatchRelaxedSuffix(const std::string_view& str, const LikePatternInfo& info, bool& gaveUp) {
    const size_t minBody = static_cast<size_t>(info.minBodyBytes);
    const size_t maxBody = static_cast<size_t>(info.maxBodyBytes);
    if (str.size() < minBody) {
        return false;
    }
    const size_t lastStart = str.size() - minBody;
    const size_t firstStart = str.size() > maxBody ? str.size() - maxBody : 0;
    size_t endPos = 0;
    int32_t attempts = 0;
    for (size_t start = firstStart; start <= lastStart; ++start) {
        if (++attempts > kRelaxedMaxStartAttempts) {
            gaveUp = true;
            return false;
        }
        if (MatchRelaxedForward(str, start, info, endPos) && endPos == str.size()) {
            return true;
        }
    }
    return false;
}

/// "%...%<body>%...%": the relaxed body must occur somewhere inside the string.
bool MatchRelaxedSubstring(const std::string_view& str, const LikePatternInfo& info, bool& gaveUp) {
    const size_t minBody = static_cast<size_t>(info.minBodyBytes);
    if (str.size() < minBody) {
        return false;
    }
    const size_t lastStart = str.size() - minBody;
    size_t endPos = 0;
    int32_t attempts = 0;
    const RelaxedSegment& first = info.segments.front();
    if (!first.isWildcard && first.length > 0) {
        // Anchor on the leading literal to skip most byte positions.
        const std::string_view anchor(info.literal.data() + first.offset, static_cast<size_t>(first.length));
        size_t from = 0;
        while (from <= lastStart) {
            const size_t pos = str.find(anchor, from);
            if (pos == std::string_view::npos || pos > lastStart) {
                return false;
            }
            if (++attempts > kRelaxedMaxStartAttempts) {
                gaveUp = true;
                return false;
            }
            if (MatchRelaxedForward(str, pos, info, endPos)) {
                return true;
            }
            from = pos + 1;
        }
        return false;
    }
    for (size_t start = 0; start <= lastStart; ++start) {
        if (++attempts > kRelaxedMaxStartAttempts) {
            gaveUp = true;
            return false;
        }
        if (MatchRelaxedForward(str, start, info, endPos)) {
            return true;
        }
    }
    return false;
}

/// Evaluate one row using the classified fast path or RE2 fallback.
bool MatchWithPatternInfo(const std::string_view& str, const LikePatternInfo& info,
                          const std::string_view& pattern, const std::string_view& escapeSeq,
                          std::shared_ptr<const re2::RE2>& matcher) {
    switch (info.kind) {
        case LikePatternKind::kFixed:
            return MatchFixed(str, info.literal);
        case LikePatternKind::kPrefix:
            return MatchPrefix(str, info.literal);
        case LikePatternKind::kSuffix:
            return MatchSuffix(str, info.literal);
        case LikePatternKind::kSubstring:
            return MatchSubstring(str, info.literal);
        case LikePatternKind::kRelaxedFixed:
            return MatchRelaxedFixed(str, info);
        case LikePatternKind::kRelaxedPrefix:
            return MatchRelaxedPrefix(str, info);
        case LikePatternKind::kRelaxedSuffix:
        case LikePatternKind::kRelaxedSubstring: {
            bool gaveUp = false;
            const bool hit = (info.kind == LikePatternKind::kRelaxedSuffix) ?
                MatchRelaxedSuffix(str, info, gaveUp) : MatchRelaxedSubstring(str, info, gaveUp);
            if (hit || !gaveUp) {
                return hit;
            }
            // Fall back to RE2 when candidate checks run out.
            if (!matcher) {
                matcher = GetOrCompileLikeRegex(pattern, escapeSeq);
            }
            return MatchWhole(matcher, str);
        }
        case LikePatternKind::kGeneric:
        default:
            if (!matcher) {
                matcher = GetOrCompileLikeRegex(pattern, escapeSeq);
            }
            return MatchWhole(matcher, str);
    }
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
        ApplyLikeWithEscape(strVec, patternVec, escapeVec, result, outputType);
        delete escapeVec;
        delete patternVec;
        delete strVec;
        return;
    }
    BaseVector* patternVec = args.top();
    args.pop();
    BaseVector* strVec = args.top();
    args.pop();
    ApplyLike(strVec, patternVec, result, outputType);

    delete patternVec;
    delete strVec;
}

void LikeFunction::ApplyLike(BaseVector* strVec, BaseVector* patternVec, BaseVector*& result,
                             const DataTypePtr& outputType) const {
    int32_t size = strVec->GetSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
    if (size == 0) {
        return;
    }

    if (patternVec->GetEncoding() == OMNI_ENCODING_CONST) {
        if (patternVec->IsNull(0)) {
            for (int32_t row = 0; row < size; ++row) {
                result->SetNull(row);
            }
            return;
        }
        std::string_view patternSv = reinterpret_cast<ConstVector<std::string_view>*>(patternVec)->GetConstValue();
        // Classify once; compile RE2 lazily only for generic patterns.
        const LikePatternInfo patInfo = ClassifyLikePattern(patternSv, kDefaultLikeEscape);
        std::shared_ptr<const re2::RE2> matcher;
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view str = GetStringValueFromVector(strVec, row);
            SetBooleanValueToVector(result, row,
                MatchWithPatternInfo(str, patInfo, patternSv, kDefaultLikeEscape, matcher));
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
                                       BaseVector*& result, const DataTypePtr& outputType) const {
    int32_t size = strVec->GetSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
    if (size == 0) {
        return;
    }

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
        std::string_view esc = reinterpret_cast<ConstVector<std::string_view>*>(escapeVec)->GetConstValue();
        std::string_view escSeq;
        if (!ValidateSingleCharEscape(esc, escSeq)) {
            OMNI_THROW("Like function Error", "Escape string must be a single character");
        }
        constEscape = escSeq;
    }

    // Only a constant pattern plus a constant escape can share one compiled
    // matcher. Resolve it lazily on the first non-null input row.
    std::shared_ptr<const re2::RE2> constMatcher;

    if (patternVec->GetEncoding() == OMNI_ENCODING_CONST) {
        std::string_view patternSv = reinterpret_cast<ConstVector<std::string_view>*>(patternVec)->GetConstValue();
        // When both pattern and escape are constant the pattern is classified once and the
        // byte-comparison fast paths are used; a per-row escape falls back to RE2 via MatchLike.
        std::optional<LikePatternInfo> constPatInfo;
        if (constEscape.has_value()) {
            constPatInfo = ClassifyLikePattern(patternSv, *constEscape);
        }
        for (int32_t row = 0; row < size; ++row) {
            if (strVec->IsNull(row) || patternVec->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view str = GetStringValueFromVector(strVec, row);
            bool matches;
            if (constPatInfo.has_value()) {
                matches = MatchWithPatternInfo(str, *constPatInfo, patternSv, *constEscape, constMatcher);
            } else {
                if (escapeVec->IsNull(row)) {
                    result->SetNull(row);
                    continue;
                }
                std::string_view esc = GetStringValueFromVector(escapeVec, row);
                std::string_view escSeq;
                if (!ValidateSingleCharEscape(esc, escSeq)) {
                    OMNI_THROW("Like function Error", "Escape string must be a single character");
                }
                matches = MatchLike(str, patternSv, escSeq);
            }
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
    return MatchWhole(GetOrCompileLikeRegex(pattern, escapeSeq), str);
}

std::string_view LikeFunction::GetStringValueFromVector(BaseVector* vec, int32_t row) {
    Encoding encoding = vec->GetEncoding();
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
