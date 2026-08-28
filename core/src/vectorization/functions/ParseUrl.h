/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ParseUrl function for vectorization framework
 */

#pragma once
#include "util/compiler_util.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {

template <typename T>
struct ParseUrlFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &url,
        const std::string_view &part)
    {
        ParsedUrl parsed;
        if (!parse(url, parsed)) {
            return false;
        }
        return extractPart(result, parsed, part);
    }

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &url,
        const std::string_view &part, const std::string_view &key)
    {
        if (part != "QUERY") {
            return false;
        }

        ParsedUrl parsed;
        if (!parse(url, parsed) || !parsed.hasQuery) {
            return false;
        }
        return extractQueryValue(result, parsed.query, key);
    }

private:
    struct ParsedUrl {
        std::string_view protocol;
        std::string_view authority;
        std::string_view userInfo;
        std::string_view host;
        std::string_view path;
        std::string_view query;
        std::string_view ref;
        bool hasAuthority = false;
        bool hasUserInfo = false;
        bool hasQuery = false;
        bool hasRef = false;
    };

    static ALWAYS_INLINE bool isAsciiAlpha(char value)
    {
        return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
    }

    static ALWAYS_INLINE bool isAsciiDigit(char value)
    {
        return value >= '0' && value <= '9';
    }

    static ALWAYS_INLINE bool isAsciiHexDigit(char value)
    {
        return isAsciiDigit(value) || (value >= 'A' && value <= 'F') ||
            (value >= 'a' && value <= 'f');
    }

    static ALWAYS_INLINE char toAsciiLower(char value)
    {
        return (value >= 'A' && value <= 'Z') ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    static ALWAYS_INLINE bool equalsIgnoreAsciiCase(const std::string_view &left,
        const std::string_view &right)
    {
        if (left.size() != right.size()) {
            return false;
        }
        for (size_t i = 0; i < left.size(); ++i) {
            if (toAsciiLower(left[i]) != right[i]) {
                return false;
            }
        }
        return true;
    }

    static ALWAYS_INLINE bool isSupportedProtocol(const std::string_view &protocol)
    {
        return equalsIgnoreAsciiCase(protocol, "http") ||
            equalsIgnoreAsciiCase(protocol, "https") ||
            equalsIgnoreAsciiCase(protocol, "ftp") ||
            equalsIgnoreAsciiCase(protocol, "file") ||
            equalsIgnoreAsciiCase(protocol, "jar") ||
            equalsIgnoreAsciiCase(protocol, "mailto");
    }

    static ALWAYS_INLINE bool isValidScheme(const std::string_view &scheme)
    {
        if (scheme.empty() || !isAsciiAlpha(scheme.front())) {
            return false;
        }
        for (size_t i = 1; i < scheme.size(); ++i) {
            char value = scheme[i];
            if (!isAsciiAlpha(value) && !isAsciiDigit(value) &&
                value != '+' && value != '-' && value != '.') {
                return false;
            }
        }
        return true;
    }

    static ALWAYS_INLINE bool parseAuthority(const std::string_view &authority, ParsedUrl &parsed)
    {
        parsed.authority = authority;
        parsed.hasAuthority = true;

        size_t hostStart = 0;
        size_t userInfoEnd = authority.find('@');
        if (userInfoEnd != std::string_view::npos) {
            // OpenJDK treats an authority containing multiple '@' characters as non-server-based.
            if (userInfoEnd != authority.rfind('@')) {
                parsed.host = std::string_view();
                return true;
            }
            parsed.userInfo = authority.substr(0, userInfoEnd);
            parsed.hasUserInfo = true;
            hostStart = userInfoEnd + 1;
        }

        std::string_view hostAndPort = authority.substr(hostStart);
        if (hostAndPort.empty()) {
            parsed.host = hostAndPort;
            return true;
        }

        if (hostAndPort.front() == '[') {
            size_t bracketEnd = hostAndPort.find(']');
            if (bracketEnd == std::string_view::npos) {
                return false;
            }
            if (!isValidIpv6Address(hostAndPort.substr(1, bracketEnd - 1))) {
                return false;
            }
            parsed.host = hostAndPort.substr(0, bracketEnd + 1);
            if (bracketEnd + 1 == hostAndPort.size()) {
                return true;
            }
            if (hostAndPort[bracketEnd + 1] != ':') {
                return false;
            }
            return isValidPort(hostAndPort.substr(bracketEnd + 2));
        }

        size_t portSeparator = hostAndPort.find(':');
        if (portSeparator == std::string_view::npos) {
            parsed.host = hostAndPort;
            return true;
        }
        if (!isValidPort(hostAndPort.substr(portSeparator + 1))) {
            return false;
        }
        parsed.host = hostAndPort.substr(0, portSeparator);
        return true;
    }

    static ALWAYS_INLINE bool isValidPort(const std::string_view &port)
    {
        if (port.empty()) {
            return true;
        }

        size_t cursor = 0;
        bool negative = false;
        if (port.front() == '+' || port.front() == '-') {
            negative = port.front() == '-';
            cursor = 1;
        }
        if (cursor == port.size()) {
            return false;
        }

        uint32_t value = 0;
        // OpenJDK parses ports as signed 32-bit integers and only permits negative one.
        constexpr uint32_t kMaxInt = 2147483647U;
        for (; cursor < port.size(); ++cursor) {
            if (!isAsciiDigit(port[cursor])) {
                return false;
            }
            uint32_t digit = static_cast<uint32_t>(port[cursor] - '0');
            if (value > (kMaxInt - digit) / 10U) {
                return false;
            }
            value = value * 10U + digit;
        }
        return !negative || value <= 1U;
    }

    static ALWAYS_INLINE bool isValidIpv4Address(const std::string_view &address)
    {
        size_t fieldStart = 0;
        size_t fieldCount = 0;
        while (fieldStart <= address.size()) {
            size_t fieldEnd = address.find('.', fieldStart);
            if (fieldEnd == std::string_view::npos) {
                fieldEnd = address.size();
            }
            if (fieldEnd == fieldStart || fieldEnd - fieldStart > 3) {
                return false;
            }
            uint32_t value = 0;
            for (size_t i = fieldStart; i < fieldEnd; ++i) {
                if (!isAsciiDigit(address[i])) {
                    return false;
                }
                value = value * 10U + static_cast<uint32_t>(address[i] - '0');
            }
            if (value > 255U || ++fieldCount > 4) {
                return false;
            }
            if (fieldEnd == address.size()) {
                break;
            }
            fieldStart = fieldEnd + 1;
        }
        return fieldCount == 4;
    }

    static ALWAYS_INLINE bool countIpv6Units(const std::string_view &part, bool allowIpv4,
        size_t &unitCount)
    {
        if (part.empty()) {
            return true;
        }

        size_t fieldStart = 0;
        while (fieldStart <= part.size()) {
            size_t fieldEnd = part.find(':', fieldStart);
            if (fieldEnd == std::string_view::npos) {
                fieldEnd = part.size();
            }
            if (fieldEnd == fieldStart) {
                return false;
            }
            std::string_view field = part.substr(fieldStart, fieldEnd - fieldStart);
            if (field.find('.') != std::string_view::npos) {
                if (!allowIpv4 || fieldEnd != part.size() || !isValidIpv4Address(field)) {
                    return false;
                }
                unitCount += 2;
            } else {
                if (field.size() > 4) {
                    return false;
                }
                for (char value : field) {
                    if (!isAsciiHexDigit(value)) {
                        return false;
                    }
                }
                ++unitCount;
            }
            if (fieldEnd == part.size()) {
                break;
            }
            fieldStart = fieldEnd + 1;
        }
        return true;
    }

    static ALWAYS_INLINE bool isValidIpv6Address(std::string_view address)
    {
        size_t zoneStart = address.find('%');
        if (zoneStart != std::string_view::npos) {
            if (zoneStart == 0 || zoneStart + 1 == address.size()) {
                return false;
            }
            address = address.substr(0, zoneStart);
        }
        if (address.empty()) {
            return false;
        }

        size_t compression = address.find("::");
        if (compression == std::string_view::npos) {
            size_t unitCount = 0;
            return countIpv6Units(address, true, unitCount) && unitCount == 8;
        }
        if (address.find("::", compression + 2) != std::string_view::npos) {
            return false;
        }

        size_t unitCount = 0;
        std::string_view left = address.substr(0, compression);
        std::string_view right = address.substr(compression + 2);
        if (!countIpv6Units(left, false, unitCount) ||
            !countIpv6Units(right, true, unitCount)) {
            return false;
        }
        return unitCount < 8;
    }

    static ALWAYS_INLINE std::string_view normalizeUrl(std::string_view url)
    {
        while (!url.empty() && static_cast<unsigned char>(url.front()) <=
            static_cast<unsigned char>(' ')) {
            url.remove_prefix(1);
        }
        while (!url.empty() && static_cast<unsigned char>(url.back()) <=
            static_cast<unsigned char>(' ')) {
            url.remove_suffix(1);
        }
        if (url.size() >= 4 && equalsIgnoreAsciiCase(url.substr(0, 4), "url:")) {
            url.remove_prefix(4);
        }
        return url;
    }

    static inline bool parseJar(const std::string_view &url, size_t cursor,
        ParsedUrl &parsed)
    {
        size_t refStart = url.find('#', cursor);
        size_t fileEnd = refStart == std::string_view::npos ? url.size() : refStart;
        std::string_view file = url.substr(cursor, fileEnd - cursor);

        size_t bangSlash = std::string_view::npos;
        size_t searchFrom = file.size();
        // The JAR handler uses the last "!/" separator and validates the enclosed URL.
        while (searchFrom > 0) {
            size_t bang = file.rfind('!', searchFrom - 1);
            if (bang == std::string_view::npos) {
                break;
            }
            if (bang + 1 < file.size() && file[bang + 1] == '/') {
                bangSlash = bang;
                break;
            }
            searchFrom = bang;
        }
        if (bangSlash == std::string_view::npos) {
            return false;
        }

        std::string_view innerUrl = file.substr(0, bangSlash);
        ParsedUrl innerParsed;
        if (!parse(innerUrl, innerParsed) ||
            equalsIgnoreAsciiCase(innerParsed.protocol, "jar")) {
            return false;
        }

        size_t queryStart = file.rfind('?');
        if (queryStart != std::string_view::npos) {
            parsed.hasQuery = true;
            parsed.query = file.substr(queryStart + 1);
            parsed.path = file.substr(0, queryStart);
        } else {
            parsed.path = file;
        }
        if (refStart != std::string_view::npos) {
            parsed.hasRef = true;
            parsed.ref = url.substr(refStart + 1);
        }
        return true;
    }

    static ALWAYS_INLINE bool parseMailto(const std::string_view &url, size_t cursor,
        ParsedUrl &parsed)
    {
        size_t refStart = url.find('#', cursor);
        size_t fileEnd = refStart == std::string_view::npos ? url.size() : refStart;
        std::string_view file = url.substr(cursor, fileEnd - cursor);
        bool allWhitespace = file.empty();
        for (char value : file) {
            if (static_cast<unsigned char>(value) > static_cast<unsigned char>(' ')) {
                allWhitespace = false;
                break;
            }
        }
        if (allWhitespace) {
            return false;
        }

        size_t queryStart = file.rfind('?');
        if (queryStart != std::string_view::npos) {
            parsed.hasQuery = true;
            parsed.query = file.substr(queryStart + 1);
            parsed.path = file.substr(0, queryStart);
        } else {
            parsed.path = file;
        }
        return true;
    }

    static inline bool parse(const std::string_view &input, ParsedUrl &parsed)
    {
        std::string_view url = normalizeUrl(input);
        size_t schemeEnd = url.find(':');
        size_t firstDelimiter = url.find_first_of("/?#");
        if (schemeEnd == std::string_view::npos ||
            (firstDelimiter != std::string_view::npos && schemeEnd > firstDelimiter)) {
            return false;
        }

        parsed.protocol = url.substr(0, schemeEnd);
        if (!isValidScheme(parsed.protocol) || !isSupportedProtocol(parsed.protocol)) {
            return false;
        }

        size_t cursor = schemeEnd + 1;
        if (equalsIgnoreAsciiCase(parsed.protocol, "jar")) {
            return parseJar(url, cursor, parsed);
        }
        if (equalsIgnoreAsciiCase(parsed.protocol, "mailto")) {
            return parseMailto(url, cursor, parsed);
        }

        bool isUncName = cursor + 3 < url.size() &&
            url.substr(cursor, 4) == "////";
        if (!isUncName && cursor + 1 < url.size() &&
            url[cursor] == '/' && url[cursor + 1] == '/') {
            size_t authorityStart = cursor + 2;
            size_t authorityEnd = url.find_first_of("/?#", authorityStart);
            if (authorityEnd == std::string_view::npos) {
                authorityEnd = url.size();
            }
            if (!parseAuthority(url.substr(authorityStart, authorityEnd - authorityStart), parsed)) {
                return false;
            }
            cursor = authorityEnd;
        }

        size_t refStart = url.find('#', cursor);
        size_t queryStart = url.find('?', cursor);
        if (queryStart != std::string_view::npos &&
            (refStart == std::string_view::npos || queryStart < refStart)) {
            parsed.hasQuery = true;
            size_t queryEnd = refStart == std::string_view::npos ? url.size() : refStart;
            parsed.query = url.substr(queryStart + 1, queryEnd - queryStart - 1);
        } else {
            queryStart = std::string_view::npos;
        }

        size_t pathEnd = url.size();
        if (queryStart != std::string_view::npos) {
            pathEnd = queryStart;
        } else if (refStart != std::string_view::npos) {
            pathEnd = refStart;
        }
        parsed.path = url.substr(cursor, pathEnd - cursor);

        if (refStart != std::string_view::npos) {
            parsed.hasRef = true;
            parsed.ref = url.substr(refStart + 1);
        }
        return true;
    }

    static ALWAYS_INLINE void assign(std::string &result, const std::string_view &value)
    {
        if (value.empty()) {
            result.clear();
            return;
        }
        result.assign(value.data(), value.size());
    }

    static ALWAYS_INLINE bool extractPart(std::string &result, const ParsedUrl &parsed,
        const std::string_view &part)
    {
        if (part == "HOST") {
            assign(result, parsed.host);
            return true;
        }
        if (part == "PATH") {
            assign(result, parsed.path);
            return true;
        }
        if (part == "QUERY") {
            if (!parsed.hasQuery) {
                return false;
            }
            assign(result, parsed.query);
            return true;
        }
        if (part == "REF") {
            if (!parsed.hasRef) {
                return false;
            }
            assign(result, parsed.ref);
            return true;
        }
        if (part == "PROTOCOL") {
            result.resize(parsed.protocol.size());
            for (size_t i = 0; i < parsed.protocol.size(); ++i) {
                result[i] = toAsciiLower(parsed.protocol[i]);
            }
            return true;
        }
        if (part == "AUTHORITY") {
            if (!parsed.hasAuthority) {
                return false;
            }
            assign(result, parsed.authority);
            return true;
        }
        if (part == "FILE") {
            assign(result, parsed.path);
            if (parsed.hasQuery) {
                result.push_back('?');
                if (!parsed.query.empty()) {
                    result.append(parsed.query.data(), parsed.query.size());
                }
            }
            return true;
        }
        if (part == "USERINFO") {
            if (!parsed.hasUserInfo) {
                return false;
            }
            assign(result, parsed.userInfo);
            return true;
        }
        return false;
    }

    static ALWAYS_INLINE bool extractQueryValue(std::string &result, const std::string_view &query,
        const std::string_view &key)
    {
        size_t matchStart = 0;
        // Flink's regex starts matching only at query start or immediately after '&'.
        while (matchStart <= query.size()) {
            size_t remaining = query.size() - matchStart;
            if (key.size() < remaining &&
                query.substr(matchStart, key.size()) == key &&
                query[matchStart + key.size()] == '=') {
                size_t valueStart = matchStart + key.size() + 1;
                size_t valueEnd = query.find('&', valueStart);
                if (valueEnd == std::string_view::npos) {
                    valueEnd = query.size();
                }
                assign(result, query.substr(valueStart, valueEnd - valueStart));
                return true;
            }

            size_t separator = query.find('&', matchStart);
            if (separator == std::string_view::npos) {
                break;
            }
            matchStart = separator + 1;
        }
        return false;
    }
};

}
