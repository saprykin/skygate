#include "HighPrecisionTextParser.hpp"
#include "time/CalendarTime.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <string>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] bool isAsciiWhitespace(const char character) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

template <typename Integer>
[[nodiscard]] bool
parseInteger(const HighPrecisionTextParser& parser, const std::string_view text, Integer& value) noexcept
{
    const std::string_view trimmed = parser.trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return false;
    }

    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

}  // namespace

std::string_view HighPrecisionTextParser::trimAsciiWhitespace(std::string_view text) const noexcept
{
    while (!text.empty() && isAsciiWhitespace(text.front())) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && isAsciiWhitespace(text.back())) {
        text.remove_suffix(1U);
    }
    return text;
}

bool HighPrecisionTextParser::startsWith(const std::string_view text, const std::string_view prefix) const noexcept
{
    return text.size() >= prefix.size() && text.substr(0U, prefix.size()) == prefix;
}

bool HighPrecisionTextParser::parseInt(const std::string_view text, int& value) const noexcept
{
    return parseInteger(*this, text, value);
}

bool HighPrecisionTextParser::parseUint64(const std::string_view text, std::uint64_t& value) const noexcept
{
    return parseInteger(*this, text, value);
}

bool HighPrecisionTextParser::parseFiniteDouble(const std::string_view text, double& value) const noexcept
{
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return false;
    }

    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && std::isfinite(value);
}

std::vector<std::string_view> HighPrecisionTextParser::splitAsciiWhitespace(std::string_view text) const
{
    std::vector<std::string_view> tokens;
    while (true) {
        text = trimAsciiWhitespace(text);
        if (text.empty()) {
            break;
        }

        std::size_t end = 0U;
        while (end < text.size() && !isAsciiWhitespace(text[end])) {
            ++end;
        }
        tokens.push_back(text.substr(0U, end));
        text.remove_prefix(end);
    }
    return tokens;
}

bool HighPrecisionTextParser::parseBool(const std::string_view text, bool& value) const noexcept
{
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed == "true" || trimmed == "1") {
        value = true;
        return true;
    }
    if (trimmed == "false" || trimmed == "0") {
        value = false;
        return true;
    }

    return false;
}

std::vector<std::string_view> HighPrecisionTextParser::splitCommaSeparated(std::string_view line) const
{
    std::vector<std::string_view> columns;
    while (true) {
        const std::size_t comma = line.find(',');
        columns.push_back(trimAsciiWhitespace(comma == std::string_view::npos ? line : line.substr(0U, comma)));
        if (comma == std::string_view::npos) {
            break;
        }
        line.remove_prefix(comma + 1U);
    }
    return columns;
}

std::string_view HighPrecisionTextParser::fixedColumn(
    const std::string_view line, const std::size_t offset, const std::size_t length
) const noexcept
{
    if (offset >= line.size()) {
        return {};
    }

    return trimAsciiWhitespace(line.substr(offset, std::min(length, line.size() - offset)));
}

std::string_view HighPrecisionTextParser::takeLine(std::string_view& remaining) const noexcept
{
    const std::size_t newline = remaining.find('\n');
    std::string_view line = newline == std::string_view::npos ? remaining : remaining.substr(0U, newline);
    remaining = newline == std::string_view::npos ? std::string_view{} : remaining.substr(newline + 1U);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1U);
    }
    return line;
}

std::optional<skygate::core::CivilDateTime> HighPrecisionTextParser::parseUtcDate(std::string_view text) const noexcept
{
    text = trimAsciiWhitespace(text);
    const std::size_t yearSearchStart = startsWith(text, "-") ? 1U : 0U;
    const std::size_t firstDash = text.find('-', yearSearchStart);
    const std::size_t secondDash =
        firstDash == std::string_view::npos ? std::string_view::npos : text.find('-', firstDash + 1U);
    if (firstDash == std::string_view::npos || secondDash == std::string_view::npos) {
        return std::nullopt;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (!parseInt(text.substr(0U, firstDash), year)
        || !parseInt(text.substr(firstDash + 1U, secondDash - firstDash - 1U), month)
        || !parseInt(text.substr(secondDash + 1U), day)) {
        return std::nullopt;
    }

    skygate::core::CivilDateTime dateTime;
    dateTime.astronomicalYear = year;
    dateTime.month = month;
    dateTime.day = day;
    dateTime.timeScale = skygate::core::TimeScale::Utc;
    if (!skygate::core::CalendarTime::isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }

    return dateTime;
}

std::optional<std::string>
HighPrecisionTextParser::metadataValue(const std::string_view line, const std::string_view key) const
{
    const std::string prefix = "#@ " + std::string(key);
    if (!startsWith(line, prefix)) {
        return std::nullopt;
    }
    if (line.size() > prefix.size() && !isAsciiWhitespace(line[prefix.size()])) {
        return std::nullopt;
    }

    return std::string(trimAsciiWhitespace(line.substr(prefix.size())));
}

}  // namespace skygate::ephemeris
