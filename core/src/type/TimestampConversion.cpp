//
// Created by root on 12/12/25.
//

#include "TimestampConversion.h"
#include "base_operations.h"

namespace omniruntime::type::util {
using namespace vectorization;

constexpr int32_t kLeapDays[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
constexpr int32_t kNormalDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
constexpr int32_t kCumulativeDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
constexpr int32_t kCumulativeLeapDays[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};

constexpr int32_t kCumulativeYearDays[] = {
    0,
    365,
    730,
    1096,
    1461,
    1826,
    2191,
    2557,
    2922,
    3287,
    3652,
    4018,
    4383,
    4748,
    5113,
    5479,
    5844,
    6209,
    6574,
    6940,
    7305,
    7670,
    8035,
    8401,
    8766,
    9131,
    9496,
    9862,
    10227,
    10592,
    10957,
    11323,
    11688,
    12053,
    12418,
    12784,
    13149,
    13514,
    13879,
    14245,
    14610,
    14975,
    15340,
    15706,
    16071,
    16436,
    16801,
    17167,
    17532,
    17897,
    18262,
    18628,
    18993,
    19358,
    19723,
    20089,
    20454,
    20819,
    21184,
    21550,
    21915,
    22280,
    22645,
    23011,
    23376,
    23741,
    24106,
    24472,
    24837,
    25202,
    25567,
    25933,
    26298,
    26663,
    27028,
    27394,
    27759,
    28124,
    28489,
    28855,
    29220,
    29585,
    29950,
    30316,
    30681,
    31046,
    31411,
    31777,
    32142,
    32507,
    32872,
    33238,
    33603,
    33968,
    34333,
    34699,
    35064,
    35429,
    35794,
    36160,
    36525,
    36890,
    37255,
    37621,
    37986,
    38351,
    38716,
    39082,
    39447,
    39812,
    40177,
    40543,
    40908,
    41273,
    41638,
    42004,
    42369,
    42734,
    43099,
    43465,
    43830,
    44195,
    44560,
    44926,
    45291,
    45656,
    46021,
    46387,
    46752,
    47117,
    47482,
    47847,
    48212,
    48577,
    48942,
    49308,
    49673,
    50038,
    50403,
    50769,
    51134,
    51499,
    51864,
    52230,
    52595,
    52960,
    53325,
    53691,
    54056,
    54421,
    54786,
    55152,
    55517,
    55882,
    56247,
    56613,
    56978,
    57343,
    57708,
    58074,
    58439,
    58804,
    59169,
    59535,
    59900,
    60265,
    60630,
    60996,
    61361,
    61726,
    62091,
    62457,
    62822,
    63187,
    63552,
    63918,
    64283,
    64648,
    65013,
    65379,
    65744,
    66109,
    66474,
    66840,
    67205,
    67570,
    67935,
    68301,
    68666,
    69031,
    69396,
    69762,
    70127,
    70492,
    70857,
    71223,
    71588,
    71953,
    72318,
    72684,
    73049,
    73414,
    73779,
    74145,
    74510,
    74875,
    75240,
    75606,
    75971,
    76336,
    76701,
    77067,
    77432,
    77797,
    78162,
    78528,
    78893,
    79258,
    79623,
    79989,
    80354,
    80719,
    81084,
    81450,
    81815,
    82180,
    82545,
    82911,
    83276,
    83641,
    84006,
    84371,
    84736,
    85101,
    85466,
    85832,
    86197,
    86562,
    86927,
    87293,
    87658,
    88023,
    88388,
    88754,
    89119,
    89484,
    89849,
    90215,
    90580,
    90945,
    91310,
    91676,
    92041,
    92406,
    92771,
    93137,
    93502,
    93867,
    94232,
    94598,
    94963,
    95328,
    95693,
    96059,
    96424,
    96789,
    97154,
    97520,
    97885,
    98250,
    98615,
    98981,
    99346,
    99711,
    100076,
    100442,
    100807,
    101172,
    101537,
    101903,
    102268,
    102633,
    102998,
    103364,
    103729,
    104094,
    104459,
    104825,
    105190,
    105555,
    105920,
    106286,
    106651,
    107016,
    107381,
    107747,
    108112,
    108477,
    108842,
    109208,
    109573,
    109938,
    110303,
    110669,
    111034,
    111399,
    111764,
    112130,
    112495,
    112860,
    113225,
    113591,
    113956,
    114321,
    114686,
    115052,
    115417,
    115782,
    116147,
    116513,
    116878,
    117243,
    117608,
    117974,
    118339,
    118704,
    119069,
    119435,
    119800,
    120165,
    120530,
    120895,
    121260,
    121625,
    121990,
    122356,
    122721,
    123086,
    123451,
    123817,
    124182,
    124547,
    124912,
    125278,
    125643,
    126008,
    126373,
    126739,
    127104,
    127469,
    127834,
    128200,
    128565,
    128930,
    129295,
    129661,
    130026,
    130391,
    130756,
    131122,
    131487,
    131852,
    132217,
    132583,
    132948,
    133313,
    133678,
    134044,
    134409,
    134774,
    135139,
    135505,
    135870,
    136235,
    136600,
    136966,
    137331,
    137696,
    138061,
    138427,
    138792,
    139157,
    139522,
    139888,
    140253,
    140618,
    140983,
    141349,
    141714,
    142079,
    142444,
    142810,
    143175,
    143540,
    143905,
    144271,
    144636,
    145001,
    145366,
    145732,
    146097,
};

namespace {
bool isLeapYear(int32_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool isValidDate(int32_t year, int32_t month, int32_t day)
{
    if (month < 1 || month > 12) {
        return false;
    }
    if (year < kMinYear || year > kMaxYear) {
        return false;
    }
    if (day < 1) {
        return false;
    }
    return isLeapYear(year) ? day <= kLeapDays[month] : day <= kNormalDays[month];
}
}

Timestamp fromDatetime(int64_t daysSinceEpoch, int64_t microsSinceMidnight)
{
    int64_t secondsSinceEpoch = static_cast<int64_t>(daysSinceEpoch) * kSecsPerDay;
    secondsSinceEpoch += microsSinceMidnight / kMicrosPerSec;
    return Timestamp(secondsSinceEpoch, (microsSinceMidnight % kMicrosPerSec) * kNanosPerMicro);
}

int64_t fromTime(int32_t hour, int32_t minute, int32_t second, int32_t microseconds)
{
    int64_t result = hour; // hours
    result = result * kMinsPerHour + minute; // hours -> minutes
    result = result * kSecsPerMinute + second; // minutes -> seconds
    result = result * kMicrosPerSec + microseconds; // seconds -> microseconds
    return result;
}

Expected<int64_t> daysSinceEpochFromDate(int32_t year, int32_t month, int32_t day)
{
    int64_t daysSinceEpoch = 0;

    if (!isValidDate(year, month, day)) {
        return folly::makeUnexpected(vectorization::Status::UserError("Date out of range: {}-{}-{}", year, month, day));
    }
    while (year < 1970) {
        year += kYearInterval;
        daysSinceEpoch -= kDaysPerYearInterval;
    }
    while (year >= 2370) {
        year -= kYearInterval;
        daysSinceEpoch += kDaysPerYearInterval;
    }
    daysSinceEpoch += kCumulativeYearDays[year - 1970];
    daysSinceEpoch += isLeapYear(year) ? kCumulativeLeapDays[month - 1] : kCumulativeDays[month - 1];
    daysSinceEpoch += day - 1;
    return daysSinceEpoch;
}

namespace {
inline bool validDate(int64_t daysSinceEpoch)
{
    return daysSinceEpoch >= std::numeric_limits<int32_t>::min() && daysSinceEpoch <= std::numeric_limits<
        int32_t>::max();
}

void parseTimeSeparator(const char *buf, size_t &pos)
{
    if (buf[pos] == ' ' || buf[pos] == 'T') {
        pos++;
    }
}

inline bool characterIsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

inline bool characterIsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Skip leading spaces.
inline void skipSpaces(const char *buf, size_t len, size_t &pos)
{
    while (pos < len && characterIsSpace(buf[pos])) {
        pos++;
    }
}

bool parseDoubleDigit(const char *buf, size_t len, size_t &pos, int32_t &result)
{
    if (pos < len && characterIsDigit(buf[pos])) {
        result = buf[pos++] - '0';
        if (pos < len && characterIsDigit(buf[pos])) {
            result = (buf[pos++] - '0') + result * 10;
        }
        return true;
    }
    return false;
}

// String format is hh:mm:ss.microseconds (seconds and microseconds are
// optional).
// ISO 8601
bool tryParseTimeString(const char *buf, size_t len, size_t &pos, int64_t &result)
{
    static constexpr int sep = ':';
    int32_t hour = 0, min = 0, sec = 0, micros = 0;
    pos = 0;

    if (len == 0) {
        return false;
    }

    skipSpaces(buf, len, pos);

    if (pos >= len) {
        return false;
    }

    if (!characterIsDigit(buf[pos])) {
        return false;
    }

    // Read the hours.
    if (!parseDoubleDigit(buf, len, pos, hour)) {
        return false;
    }
    if (hour < 0 || hour >= 24) {
        return false;
    }

    if (pos >= len || buf[pos] != sep) {
        return false;
    }

    // Fetch the separator.
    if (buf[pos++] != sep) {
        // Invalid separator.
        return false;
    }

    // Read the minutes.
    if (!parseDoubleDigit(buf, len, pos, min)) {
        return false;
    }
    if (min < 0 || min >= 60) {
        return false;
    }

    // Try to read seconds.
    if (pos < len && buf[pos] == sep) {
        ++pos;
        if (!parseDoubleDigit(buf, len, pos, sec)) {
            return false;
        }
        if (sec < 0 || sec > 60) {
            return false;
        }

        // Try to read microseconds.
        if (pos < len) {
            if (buf[pos] == '.') {
                ++pos;
            }

            if (pos >= len) {
                return false;
            }

            // We expect microseconds.
            int32_t mult = 100000;
            for (; pos < len && characterIsDigit(buf[pos]); pos++, mult /= 10) {
                if (mult > 0) {
                    micros += (buf[pos] - '0') * mult;
                }
            }
        }
    }

    result = fromTime(hour, min, sec, micros);
    return true;
}

bool tryParseDateString(const char *buf, size_t len, size_t &pos, int64_t &daysSinceEpoch)
{
    pos = 0;
    if (len == 0) {
        return false;
    }

    int32_t day = 0;
    int32_t month = -1;
    int32_t year = 0;
    bool yearneg = false;
    // Whether a sign is included in the date string.
    bool sign = false;
    skipSpaces(buf, len, pos);

    if (pos >= len) {
        return false;
    }
    if (buf[pos] == '-') {
        sign = true;
        yearneg = true;
        pos++;
        if (pos >= len) {
            return false;
        }
    } else if (buf[pos] == '+') {
        sign = true;
        pos++;
        if (pos >= len) {
            return false;
        }
    }

    if (!characterIsDigit(buf[pos])) {
        return false;
    }
    // First parse the year.
    try {
        for (; pos < len && characterIsDigit(buf[pos]); pos++) {
            year = checkedPlus((buf[pos] - '0'), checkedMultiply(year, 10));
            if (year > kMaxYear) {
                break;
            }
        }
    } catch (const omniruntime::exception::OmniException &) {
        // Treat integer overflow (or other checked* errors) as parse failure,
        // so callers get an Expected<T> error instead of an exception.
        return false;
    }
    /// Spark digits of year must >= 4. The following formats are allowed:
    /// `[+-]yyyy*`
    /// `[+-]yyyy*-[m]m`
    /// `[+-]yyyy*-[m]m-[d]d`
    /// `[+-]yyyy*-[m]m-[d]d `
    /// `[+-]yyyy*-[m]m-[d]d *`
    /// `[+-]yyyy*-[m]m-[d]dT*`
    /// A bare year is only 4-7 digits (Spark DateTimeUtils.stringToDate): 8+ digits
    /// (e.g. "00000000") are invalid and must be NULL, not year 0.
    if (pos - sign < 4 || pos - sign > 7) {
        return false;
    }
    if (yearneg) {
        year = checkedNegate(year);
        if (year < kMinYear) {
            return false;
        }
    }

    // No month or day.
    if ((pos == len || buf[pos] == 'T')) {
        Expected<int64_t> expected = daysSinceEpochFromDate(year, 1, 1);
        if (expected.hasError()) {
            return false;
        }
        daysSinceEpoch = expected.value();
        return validDate(daysSinceEpoch);
    }

    if (pos >= len) {
        return false;
    }

    // Fetch the separator.
    int sep = buf[pos++];
    // Only '-' is valid for cast.
    if (sep != '-') {
        return false;
    }

    // Parse the month.
    if (!parseDoubleDigit(buf, len, pos, month)) {
        return false;
    }

    // No day.
    if ((pos == len || buf[pos] == 'T')) {
        Expected<int64_t> expected = daysSinceEpochFromDate(year, month, 1);
        if (expected.hasError()) {
            return false;
        }
        daysSinceEpoch = expected.value();
        return validDate(daysSinceEpoch);
    }

    if (pos >= len) {
        return false;
    }

    if (buf[pos++] != sep) {
        return false;
    }

    if (pos >= len) {
        return false;
    }

    // Now parse the day.
    if (!parseDoubleDigit(buf, len, pos, day)) {
        return false;
    }

    Expected<int64_t> expected = daysSinceEpochFromDate(year, month, day);
    if (expected.hasError()) {
        return false;
    }
    daysSinceEpoch = expected.value();

    if (!validDate(daysSinceEpoch)) {
        return false;
    }

    if (pos == len) {
        return true;
    }

    if (buf[pos] == 'T' || buf[pos] == ' ') {
        return true;
    }
    return false;
}

// Parses a variety of timestamp strings, depending on the value of
// `parseMode`. Consumes as much of the string as it can and sets `result` to
// the timestamp from whatever it successfully parses. `pos` is set to the
// position of first character that was not consumed. Returns true if it
// successfully parsed at least a date, `result` is only set if true is
// returned.
bool tryParseTimestampString(const char *buf, size_t len, size_t &pos, Timestamp &result)
{
    int64_t daysSinceEpoch = 0;
    int64_t microsSinceMidnight = 0;

    if (!tryParseDateString(buf, len, pos, daysSinceEpoch)) {
        return false;
    }

    if (pos == len) {
        // No time: only a date.
        result = fromDatetime(daysSinceEpoch, 0);
        return true;
    }

    // Try to parse a time field.
    parseTimeSeparator(buf, pos);

    size_t timePos = 0;
    if (!tryParseTimeString(buf + pos, len - pos, timePos, microsSinceMidnight)) {
        // The rest of the string is not a valid time, but it could be relevant to
        // the caller (e.g. it could be a time zone), return the date we parsed
        // and let them decide what to do with the rest.
        result = fromDatetime(daysSinceEpoch, 0);
        return true;
    }

    pos += timePos;
    result = fromDatetime(daysSinceEpoch, microsSinceMidnight);
    return true;
}

// String format is [+/-]hh:mm:ss.MMM
// * minutes, seconds, and milliseconds are optional.
// * all separators are optional.
// * . may be replaced with ,
bool tryParsePrestoTimeOffsetString(const char *buf, size_t len, size_t &pos, int64_t &result)
{
    static constexpr int sep = ':';
    int32_t hour = 0, min = 0, sec = 0, millis = 0;
    pos = 0;
    result = 0;

    if (len == 0) {
        return false;
    }

    if (buf[pos] != '+' && buf[pos] != '-') {
        return false;
    }

    bool positive = buf[pos++] == '+';

    if (pos >= len) {
        return false;
    }

    // Read the hours.
    if (!parseDoubleDigit(buf, len, pos, hour)) {
        return false;
    }
    if (hour < 0 || hour >= 24) {
        return false;
    }

    result += hour * kMillisPerHour;

    if (pos >= len || (buf[pos] != sep && !characterIsDigit(buf[pos]))) {
        result *= positive ? 1 : -1;
        return pos == len;
    }

    // Skip the separator.
    if (buf[pos] == sep) {
        pos++;
    }

    // Read the minutes.
    if (!parseDoubleDigit(buf, len, pos, min)) {
        return false;
    }
    if (min < 0 || min >= 60) {
        return false;
    }

    result += min * kMillisPerMinute;

    if (pos >= len || (buf[pos] != sep && !characterIsDigit(buf[pos]))) {
        result *= positive ? 1 : -1;
        return pos == len;
    }

    // Skip the separator.
    if (buf[pos] == sep) {
        pos++;
    }

    // Try to read seconds.
    if (!parseDoubleDigit(buf, len, pos, sec)) {
        return false;
    }
    if (sec < 0 || sec >= 60) {
        return false;
    }

    result += sec * kMillisPerSecond;

    if (pos >= len || (buf[pos] != '.' && buf[pos] != ',' && !characterIsDigit(buf[pos]))) {
        result *= positive ? 1 : -1;
        return pos == len;
    }

    // Skip the decimal.
    if (buf[pos] == '.' || buf[pos] == ',') {
        pos++;
    }

    // Try to read microseconds.
    if (pos >= len) {
        return false;
    }

    // We expect milliseconds.
    int32_t mult = 100;
    for (; pos < len && mult > 0 && characterIsDigit(buf[pos]); pos++, mult /= 10) {
        millis += (buf[pos] - '0') * mult;
    }

    result += millis;
    result *= positive ? 1 : -1;
    return pos == len;
}

vectorization::Status ParserError(const char *str, size_t len)
{
    return vectorization::Status::UserError(
        "Unable to parse timestamp value: \"{}\", " "expected format is (YYYY-MM-DD HH:MM:SS[.MS])",
        std::string(str, len));
}
}

Expected<int64_t> FromTimestampString(const char *str, size_t len)
{
    size_t pos = 0;
    Timestamp resultTimestamp;

    if (!tryParseTimestampString(str, len, pos, resultTimestamp)) {
        return folly::makeUnexpected(ParserError(str, len));
    }
    skipSpaces(str, len, pos);

    // If not all input was consumed.
    if (pos < len) {
        return folly::makeUnexpected(ParserError(str, len));
    }
    return resultTimestamp.toMicros();
}

Expected<int32_t> fromDateString(const char *str, size_t len)
{
    int64_t daysSinceEpoch;
    size_t pos = 0;

    if (!tryParseDateString(str, len, pos, daysSinceEpoch)) {
        return folly::makeUnexpected(vectorization::Status::UserError(
            "Unable to parse date value: \"{}\". " "Valid date string pattern is (YYYY-MM-DD), "
            "and can be prefixed with [+-]", std::string(str, len)));
    }
    return daysSinceEpoch;
}

namespace {
vectorization::Status parserError(const char *str, size_t len)
{
    return vectorization::Status::UserError(
        "Unable to parse timestamp value: \"{}\", " "expected format is (YYYY-MM-DD HH:MM:SS[.MS])",
        std::string(str, len));
}
}

Expected<ParsedTimestampWithTimeZone> fromTimestampWithTimezoneString(const char *str, size_t len)
{
    size_t pos = 0;
    Timestamp resultTimestamp;

    if (!tryParseTimestampString(str, len, pos, resultTimestamp)) {
        return folly::makeUnexpected(parserError(str, len));
    }

    const tz::TimeZone *timeZone = nullptr;
    std::optional<int64_t> offset = std::nullopt;

    if (pos < len && characterIsSpace(str[pos])) {
        pos++;
    }

    // If there is anything left to parse, it must be a timezone definition.
    if (pos < len) {
        size_t timezonePos = pos;
        while (timezonePos < len && !characterIsSpace(str[timezonePos])) {
            timezonePos++;
        }

        std::string_view timeZoneName(str + pos, timezonePos - pos);

        if (timeZoneName == "Z" || timeZoneName == "z") {
            timeZone = tz::locateZone("UTC", false);
        } else if ((timeZone = tz::locateZone(timeZoneName, false)) == nullptr) {
            int64_t offsetMillis = 0;
            size_t offsetPos = 0;
            if (tryParsePrestoTimeOffsetString(str + pos, timezonePos - pos, offsetPos, offsetMillis)) {
                offset = offsetMillis;
            } else {
                return folly::makeUnexpected(parserError(str, len));
            }
        }

        // Skip any spaces at the end.
        pos = timezonePos;
        skipSpaces(str, len, pos);

        if (pos < len) {
            return folly::makeUnexpected(parserError(str, len));
        }
    }
    return {{resultTimestamp, timeZone, offset}};
}

Timestamp fromParsedTimestampWithTimeZone(ParsedTimestampWithTimeZone parsed, const tz::TimeZone *sessionTimeZone)
{
    if (parsed.timeZone) {
        parsed.timestamp.toGMT(*parsed.timeZone);
    } else if (parsed.offsetMillis.has_value()) {
        auto seconds = parsed.timestamp.getSeconds();
        // use int128_t to avoid overflow.
        __int128_t nanos = parsed.timestamp.getNanos();
        seconds -= parsed.offsetMillis.value() / util::kMillisPerSecond;
        nanos -= (parsed.offsetMillis.value() % util::kMillisPerSecond) * util::kNanosPerMicro * util::kMicrosPerMsec;
        if (nanos < 0) {
            seconds -= 1;
            nanos += Timestamp::kNanosInSecond;
        } else if (nanos > Timestamp::kMaxNanos) {
            seconds += 1;
            nanos -= Timestamp::kNanosInSecond;
        }
        parsed.timestamp = Timestamp(seconds, nanos);
    } else {
        if (sessionTimeZone) {
            parsed.timestamp.toGMT(*sessionTimeZone);
        }
    }
    return parsed.timestamp;
}

std::string ToIso8601(int32_t days)
{
    // Find the number of seconds for the days_;
    // Casting 86400 to int64 to handle overflows gracefully.
    int64_t daySeconds = days * static_cast<int64_t>(86400);
    std::tm tmValue{};
    OMNI_CHECK(Timestamp::epochToCalendarUtc(daySeconds, tmValue), "Can't convert days to dates: {}", days);
    TimestampToStringOptions options;
    options.mode = TimestampToStringOptions::Mode::kDateOnly;
    // Enable zero-padding for year, to ensure compliance with 'YYYY' format.
    options.zeroPaddingYear = true;
    std::string result;
    result.resize(getMaxStringLength(options));
    const auto view = Timestamp::tmToStringView(tmValue, 0, options, result.data());
    result.resize(view.size());
    return result;
}

int32_t toDate(const Timestamp &timestamp, const tz::TimeZone *timeZone_)
{
    auto convertToDate = [](const Timestamp &t) -> int32_t {
        static const int32_t kSecsPerDay{86'400};
        auto seconds = t.getSeconds();
        if (seconds >= 0 || seconds % kSecsPerDay == 0) {
            return seconds / kSecsPerDay;
        }
        // For division with negatives, minus 1 to compensate the discarded
        // fractional part. e.g. -1/86'400 yields 0, yet it should be considered
        // as -1 day.
        return seconds / kSecsPerDay - 1;
    };

    if (timeZone_ != nullptr) {
        Timestamp copy = timestamp;
        copy.toTimezone(*timeZone_);
        return convertToDate(copy);
    }

    return convertToDate(timestamp);
}

std::string valueToString(int64_t value)
{
    constexpr long kMillisInSecond = 1000;
    constexpr long kMillisInMinute = 60 * kMillisInSecond;
    constexpr long kMillisInHour = 60 * kMillisInMinute;
    constexpr long kMillisInDay = 24 * kMillisInHour;
    static const char *kIntervalFormat = "%s%lld %02d:%02d:%02d.%03d";

    int128_t remainMillis = value;
    std::string sign{};
    if (remainMillis < 0) {
        sign = "-";
        remainMillis = -remainMillis;
    }
    const int64_t days = remainMillis / kMillisInDay;
    remainMillis -= days * kMillisInDay;
    const int64_t hours = remainMillis / kMillisInHour;
    remainMillis -= hours * kMillisInHour;
    const int64_t minutes = remainMillis / kMillisInMinute;
    remainMillis -= minutes * kMillisInMinute;
    const int64_t seconds = remainMillis / kMillisInSecond;
    remainMillis -= seconds * kMillisInSecond;
    char buf[64];
    snprintf(buf, sizeof(buf), kIntervalFormat, sign.c_str(), days, hours, minutes, seconds, remainMillis);

    return buf;
}
}
