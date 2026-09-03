//
// Created by root on 12/11/25.
//

#include "TimeZoneMap.h"
#include <utility>
#include <unordered_map>
#include <set>
#include <boost/algorithm/string.hpp>
#include "type/tzdb/exception.h"
#include "type/tzdb/time_zone.h"
#include "type/tzdb/zoned_time.h"
#include "util/omni_exception.h"

namespace omniruntime::tz {
using TTimeZoneDatabase = std::vector<std::unique_ptr<TimeZone>>;
using TTimeZoneIndex = std::unordered_map<std::string, const TimeZone *>;

extern const std::vector<std::pair<int16_t, std::string>> &getTimeZoneEntries();

namespace {
// Returns the offset in minutes for a specific time zone offset in the
// database. Do not call for tzID 0 (UTC / "+00:00").
inline std::chrono::minutes getTimeZoneOffset(int16_t tzID)
{
    return std::chrono::minutes{(tzID <= 840) ? (tzID - 841) : (tzID - 840)};
}

const tzdb::time_zone *locateZoneImpl(std::string_view tz_name)
{
    const tzdb::time_zone *zone = tzdb::locate_zone(tz_name);
    return zone;
}

template <typename TDuration>
tzdb::zoned_time<TDuration> getZonedTime(const tzdb::time_zone *tz, date::local_time<TDuration> timestamp,
        TimeZone::TChoose choose)
{
    if (choose == TimeZone::TChoose::kFail) {
        // By default, throws.
        return tzdb::zoned_time{tz, timestamp};
    }

    auto dateChoose = (choose == TimeZone::TChoose::kEarliest) ? tzdb::choose::earliest : tzdb::choose::latest;
    return tzdb::zoned_time{tz, timestamp, dateChoose};
}

template <typename TDuration>
TDuration toSysImpl(const TDuration &timestamp, const TimeZone::TChoose choose, const tzdb::time_zone *tz,
    const std::chrono::minutes offset)
{
    date::local_time<TDuration> timePoint{timestamp};
    validateRange(date::sys_time<TDuration>{timestamp});

    if (tz == nullptr) {
        // We can ignore `choose` as time offset conversions are always linear.
        return (timePoint - offset).time_since_epoch();
    }

    return getZonedTime(tz, timePoint, choose).get_sys_time().time_since_epoch();
}

template <typename TDuration>
TDuration toLocalImpl(const TDuration &timestamp, const tzdb::time_zone *tz, const std::chrono::minutes offset)
{
    date::sys_time<TDuration> timePoint{timestamp};
    validateRange(timePoint);

    // If this is an offset time zone.
    if (tz == nullptr) {
        return (timePoint + offset).time_since_epoch();
    }
    return tzdb::zoned_time{tz, timePoint}.get_local_time().time_since_epoch();
}

// Reverses the vector of pairs into a map key'ed by the timezone name for
// reverse look ups.
TTimeZoneIndex buildTimeZoneIndex(const TTimeZoneDatabase &tzDatabase)
{
    TTimeZoneIndex reversed;
    reversed.reserve(tzDatabase.size() + 2);

    for (int16_t i = 0; i < tzDatabase.size(); ++i) {
        if (tzDatabase[i] != nullptr) {
            reversed.emplace(boost::algorithm::to_lower_copy(tzDatabase[i]->name()), tzDatabase[i].get());
        }
    }

    // Add aliases to UTC.
    reversed.emplace("+00:00", tzDatabase.front().get());
    reversed.emplace("-00:00", tzDatabase.front().get());
    return reversed;
}

// Flattens the input vector of pairs into a vector, assuming that the
// timezoneIDs are (mostly) sequential. Note that since they are "mostly"
// senquential, the vector can have holes. But it is still more efficient than
// looking up on a map.
TTimeZoneDatabase buildTimeZoneDatabase(const std::vector<std::pair<int16_t, std::string>> &dbInput)
{
    TTimeZoneDatabase tzDatabase;
    tzDatabase.resize(dbInput.back().first + 1);

    for (const auto &entry : dbInput) {
        std::unique_ptr<TimeZone> timeZonePtr;

        if (entry.first == 0) {
            const tzdb::time_zone *utcZone = nullptr;
            try {
                utcZone = locateZoneImpl("UTC");
            } catch (const tzdb::invalid_time_zone &) {
                // Some tz databases name it "Etc/UTC" instead of "UTC".
                try {
                    utcZone = locateZoneImpl("Etc/UTC");
                } catch (const tzdb::invalid_time_zone &) {
                    // Cannot resolve UTC at all – create an offset-based UTC (offset = 0).
                }
            }
            if (utcZone != nullptr) {
                timeZonePtr = std::make_unique<TimeZone>("UTC", entry.first, utcZone);
            } else {
                timeZonePtr = std::make_unique<TimeZone>("UTC", entry.first, std::chrono::minutes(0));
            }
        } else if (entry.first <= 1680) {
            std::chrono::minutes offset = getTimeZoneOffset(entry.first);
            timeZonePtr = std::make_unique<TimeZone>(entry.second, entry.first, offset);
        }
        // Every single other time zone entry (outside of offsets) needs to be
        // available in external/date or this will throw.
        else {
            const tzdb::time_zone *zone;
            try {
                zone = locateZoneImpl(entry.second);
            } catch (const tzdb::invalid_time_zone &err) {
                // When this exception is thrown, it typically means the time zone name
                // we are trying to locate cannot be found from OS's time zone database.
                // Thus, we just command a "continue;" to skip the creation of the
                // corresponding TimeZone object.
                //
                // Then, once this time zone is requested at runtime, a runtime error
                // will be thrown and caller is expected to handle that error in code.
                continue;
            }
            timeZonePtr = std::make_unique<TimeZone>(entry.second, entry.first, zone);
        }
        tzDatabase[entry.first] = std::move(timeZonePtr);
    }
    return tzDatabase;
}

const TTimeZoneDatabase &getTimeZoneDatabase()
{
    static TTimeZoneDatabase timeZoneDatabase = buildTimeZoneDatabase(getTimeZoneEntries());
    return timeZoneDatabase;
}

const TTimeZoneIndex &getTimeZoneIndex()
{
    static TTimeZoneIndex timeZoneIndex = buildTimeZoneIndex(getTimeZoneDatabase());
    return timeZoneIndex;
}

inline bool isTimeZoneOffset(std::string_view str)
{
    return str.size() >= 3 && (str[0] == '+' || str[0] == '-');
}

inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

inline bool startsWith(std::string_view str, const char *prefix)
{
    return str.rfind(prefix, 0) == 0;
}

template <typename TDuration>
void validateRangeImpl(time_point<TDuration> timePoint)
{
    using namespace omniruntime::date;
    static constexpr auto kMinYear = year::min();
    static constexpr auto kMaxYear = year::max();

    if (timePoint.time_since_epoch() < std::chrono::seconds(std::numeric_limits<int64_t>::min() / 1000) || timePoint.
        time_since_epoch() > std::chrono::seconds(std::numeric_limits<int64_t>::max() / 1000)) {
        OMNI_FAIL("Timepoint is outside of supported timestamp seconds since epoch range: [{}, {}], got {}",
            std::chrono::seconds(std::numeric_limits<int64_t>::min() / 1000).count(),
            std::chrono::seconds(std::numeric_limits<int64_t>::max() / 1000).count(),
            static_cast<int64_t>(timePoint.time_since_epoch().count()));
    }
    auto year = year_month_day(floor<days>(timePoint)).year();

    if (year < kMinYear || year > kMaxYear) {
        // This is a special case where we intentionally throw
        // VeloxRuntimeError to avoid it being suppressed by TRY().
        OMNI_FAIL("Timepoint is outside of supported year range: [{}, {}], got {}", static_cast<int64_t>(kMinYear),
            static_cast<int64_t>(kMaxYear), static_cast<int64_t>(year));
    }
}

// The timezone parsing logic follows what is defined here:
//   https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
inline bool isUtcEquivalentName(std::string_view zone)
{
    static std::set<std::string_view> utcSet = {"utc", "uct", "gmt", "gmt0", "greenwich", "universal", "zulu", "z"};
    return utcSet.find(zone) != utcSet.end();
}

// This function tries to apply two normalization rules to time zone offsets:
//
// 1. If the offset only defines the hours portion, assume minutes are zeroed
// out (e.g. "+00" -> "+00:00")
//
// 2. Check if the ':' in between in missing; if so, correct the offset string
// (e.g. "+0000" -> "+00:00").
//
// This function assumes the first character is either '+' or '-'.
std::string normalizeTimeZoneOffset(const std::string &zoneOffset)
{
    if (zoneOffset.size() == 3 && isDigit(zoneOffset[1]) && isDigit(zoneOffset[2])) {
        return zoneOffset + ":00";
    } else if (zoneOffset.size() == 5 && isDigit(zoneOffset[1]) && isDigit(zoneOffset[2]) && isDigit(zoneOffset[3]) &&
        isDigit(zoneOffset[4])) {
        return zoneOffset.substr(0, 3) + ':' + zoneOffset.substr(3, 2);
    }
    return zoneOffset;
}

std::string normalizeTimeZone(const std::string &originalZoneId)
{
    // If this is an offset that hasn't matched, check if this is an incomplete
    // offset.
    if (isTimeZoneOffset(originalZoneId)) {
        return normalizeTimeZoneOffset(originalZoneId);
    }

    // Otherwise, try other time zone name normalizations.
    std::string_view zoneId = originalZoneId;
    const bool startsWithEtc = startsWith(zoneId, "etc/");

    if (startsWithEtc) {
        zoneId = zoneId.substr(4);
    }

    // ETC/GMT, ETC/GREENWICH, and others are all valid and link to GMT.
    if (isUtcEquivalentName(zoneId)) {
        return "utc";
    }

    bool startsWithUtc = startsWith(zoneId, "utc");
    bool startsWithGmt = startsWith(zoneId, "gmt");
    bool startsWithUt = !startsWithUtc && startsWith(zoneId, "ut");

    // Check for Etc/GMT(+/-)H[H] UTC(+/-)H[H] GMT(+/-)H[H] UT(+/-)H[H] patterns.
    if ((zoneId.size() > 4 && (startsWithUtc || startsWithGmt)) || (zoneId.size() > 3 && startsWithUt)) {
        if (startsWithUtc || startsWithGmt) {
            zoneId = zoneId.substr(3);
        } else {
            OMNI_CHECK(startsWithUt, "startsWithUt error!");
            zoneId = zoneId.substr(2);
        }

        char signChar = zoneId[0];

        if (signChar == '+' || signChar == '-') {
            // ETC flips the sign for GMT.
            if (startsWithEtc && startsWithGmt) {
                signChar = (signChar == '-') ? '+' : '-';
            }

            // Extract the tens and ones characters for the hour.
            char hourTens;
            char hourOnes;

            if (zoneId.size() == 2) {
                hourTens = '0';
                hourOnes = zoneId[1];
            } else {
                hourTens = zoneId[1];
                hourOnes = zoneId[2];
            }

            // Prevent it from returning -00:00, which is just utc.
            if (hourTens == '0' && hourOnes == '0') {
                return "utc";
            }

            if (isDigit(hourTens) && isDigit(hourOnes)) {
                return std::string() + signChar + hourTens + hourOnes + ":00";
            }
        }
    }

    return originalZoneId;
}
}

int16_t getTimeZoneID(std::string_view timeZone, bool failOnError)
{
    const TimeZone *tz = locateZone(timeZone, failOnError);
    return tz == nullptr ? -1 : tz->id();
}

void validateRange(time_point<std::chrono::seconds> timePoint)
{
    validateRangeImpl(timePoint);
}

void validateRange(time_point<std::chrono::milliseconds> timePoint)
{
    validateRangeImpl(timePoint);
}

TimeZone::seconds TimeZone::to_sys(TimeZone::seconds timestamp, TimeZone::TChoose choose) const
{
    return toSysImpl(timestamp, choose, tz_, offset_);
}

TimeZone::milliseconds TimeZone::to_sys(TimeZone::milliseconds timestamp, TimeZone::TChoose choose) const
{
    return toSysImpl(timestamp, choose, tz_, offset_);
}

TimeZone::seconds TimeZone::to_local(TimeZone::seconds timestamp) const
{
    return toLocalImpl(timestamp, tz_, offset_);
}

TimeZone::milliseconds TimeZone::to_local(TimeZone::milliseconds timestamp) const
{
    return toLocalImpl(timestamp, tz_, offset_);
}

TimeZoneInfo TimeZone::getInfo(TimeZone::seconds timestamp, bool includeAbbreviation) const
{
    date::sys_time<seconds> timePoint{timestamp};
    validateRange(timePoint);
    if (tz_ == nullptr) {
        TimeZoneInfo result{
            seconds::min(),
            seconds::max(),
            std::chrono::duration_cast<seconds>(offset_),
            std::chrono::minutes{0},
            {}};
        if (includeAbbreviation) {
            result.abbreviation = timeZoneName_;
        }
        return result;
    }
    auto info = tz_->get_info(timePoint);
    TimeZoneInfo result{
        std::chrono::duration_cast<seconds>(info.begin.time_since_epoch()),
        std::chrono::duration_cast<seconds>(info.end.time_since_epoch()),
        info.offset,
        info.save,
        {}};
    if (includeAbbreviation) {
        result.abbreviation = std::move(info.abbrev);
    }
    return result;
}

TimeZone::seconds TimeZone::correct_nonexistent_time(TimeZone::seconds timestamp) const
{
    if (tz_ == nullptr) {
        return timestamp;
    }

    const auto localInfo = tz_->get_info(date::local_time<seconds>{timestamp});

    if (localInfo.result != tzdb::local_info::nonexistent) {
        return timestamp;
    }

    const auto adjustment = localInfo.second.offset - localInfo.first.offset;

    return timestamp + adjustment;
}

const TimeZone *locateZone(std::string_view timeZone, bool failOnError)
{
    const auto &timeZoneIndex = getTimeZoneIndex();

    std::string timeZoneLowered;
    boost::algorithm::to_lower_copy(std::back_inserter(timeZoneLowered), timeZone);

    auto it = timeZoneIndex.find(timeZoneLowered);
    if (it != timeZoneIndex.end()) {
        return it->second;
    }

    // If an exact match wasn't found, try to normalize the timezone name.
    it = timeZoneIndex.find(normalizeTimeZone(timeZoneLowered));
    if (it != timeZoneIndex.end()) {
        return it->second;
    }

    if (failOnError) {
        OMNI_FAIL("Unknown time zone: '{}'", timeZone);
    }
    return nullptr;
}
}
