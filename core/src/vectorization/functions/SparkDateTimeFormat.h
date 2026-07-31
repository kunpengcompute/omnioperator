/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Precompiled Spark date-time format helpers.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace omniruntime::vectorization::datetime {

struct CalendarTime;
struct ResolvedTimeZone;

enum class ParseFormatKind {
    GENERAL,
    YMD,
    YMD_HMS,
};

struct CompiledParseFormat {
    std::string strptimeFormat;
    ParseFormatKind parseFormatKind{ParseFormatKind::GENERAL};
    bool hasFractional{false};
    bool allowTrailingWhitespace{true};
    bool emptySourceFormat{false};
};

CompiledParseFormat CompileParseFormat(std::string_view jodaFormat, bool allowTrailingWhitespace);

bool ParseDateTimeString(
    std::string_view input,
    const CompiledParseFormat &format,
    int64_t &resultMicros);

enum class FormatterKind {
    DATE_FORMAT,
    FROM_UNIXTIME,
};

enum class CompiledTokenKind {
    PATTERN,
    LITERAL,
    PERCENT,
};

enum class FastFormatKind {
    NONE,
    YMD_HMS,
};

struct CompiledFormatToken {
    CompiledTokenKind kind{CompiledTokenKind::PATTERN};
    char symbol{0};
    uint32_t width{0};
    std::string literal;
};

struct CompiledFormatPattern {
    std::vector<CompiledFormatToken> tokens;
    FormatterKind formatterKind{FormatterKind::DATE_FORMAT};
    bool legacyPolicy{false};
    bool requiresZoneName{false};
    size_t maxResultSize{0};
    FastFormatKind fastFormatKind{FastFormatKind::NONE};
    size_t fixedResultSize{0};
};

CompiledFormatPattern CompileFormatPattern(
    std::string_view format,
    FormatterKind formatterKind,
    bool legacyPolicy);

// Returns the number of bytes written, or -1 if output capacity is insufficient.
int32_t FormatDateTimeToBuffer(
    const CalendarTime &calendarTime,
    const ResolvedTimeZone &timeZone,
    const CompiledFormatPattern &format,
    char *output,
    size_t capacity);

std::string FormatDateTime(
    const CalendarTime &calendarTime,
    const ResolvedTimeZone &timeZone,
    const CompiledFormatPattern &format);

} // namespace omniruntime::vectorization::datetime
