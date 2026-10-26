#include "TextParsingUtilities.hpp"
#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"
#include "time/CalendarTime.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace skygate::ephemeris {

std::vector<std::string_view> TextParsingUtilities::splitAsciiWhitespace(std::string_view text)
{
    std::vector<std::string_view> tokens;
    while (true) {
        text = StringUtilities::trimAsciiWhitespace(text);
        if (text.empty()) {
            break;
        }

        std::size_t end = 0U;
        while (end < text.size() && !StringUtilities::isAsciiWhitespace(text[end])) {
            ++end;
        }
        tokens.push_back(text.substr(0U, end));
        text.remove_prefix(end);
    }
    return tokens;
}

bool TextParsingUtilities::parseBool(const std::string_view text, bool& value) noexcept
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
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

std::vector<std::string_view> TextParsingUtilities::splitCommaSeparated(std::string_view line)
{
    std::vector<std::string_view> columns;
    while (true) {
        const std::size_t comma = line.find(',');
        columns.push_back(
            StringUtilities::trimAsciiWhitespace(comma == std::string_view::npos ? line : line.substr(0U, comma))
        );
        if (comma == std::string_view::npos) {
            break;
        }
        line.remove_prefix(comma + 1U);
    }
    return columns;
}

std::string_view TextParsingUtilities::fixedColumn(
    const std::string_view line, const std::size_t offset, const std::size_t length
) noexcept
{
    if (offset >= line.size()) {
        return {};
    }

    return StringUtilities::trimAsciiWhitespace(line.substr(offset, std::min(length, line.size() - offset)));
}

std::optional<int> TextParsingUtilities::parseEnglishMonthAbbreviation(const std::string_view text) noexcept
{
    constexpr std::array<std::string_view, 12> kMonthAbbreviations{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const auto monthIt = std::find(kMonthAbbreviations.begin(), kMonthAbbreviations.end(), text);
    if (monthIt == kMonthAbbreviations.end()) {
        return std::nullopt;
    }

    return static_cast<int>(monthIt - kMonthAbbreviations.begin()) + 1;
}

std::optional<skygate::core::CivilDateTime> TextParsingUtilities::parseUtcDate(std::string_view text) noexcept
{
    text = StringUtilities::trimAsciiWhitespace(text);
    const std::size_t yearSearchStart = text.starts_with("-") ? 1U : 0U;
    const std::size_t firstDash = text.find('-', yearSearchStart);
    const std::size_t secondDash =
        firstDash == std::string_view::npos ? std::string_view::npos : text.find('-', firstDash + 1U);
    if (firstDash == std::string_view::npos || secondDash == std::string_view::npos) {
        return std::nullopt;
    }

    const std::optional<int> year = NumericParsingUtilities::parseInt(text.substr(0U, firstDash));
    const std::optional<int> month =
        NumericParsingUtilities::parseInt(text.substr(firstDash + 1U, secondDash - firstDash - 1U));
    const std::optional<int> day = NumericParsingUtilities::parseInt(text.substr(secondDash + 1U));
    if (!year.has_value() || !month.has_value() || !day.has_value()) {
        return std::nullopt;
    }

    return skygate::core::CalendarTime::civilDateFromUtcParts(*year, *month, *day);
}

std::optional<std::string> TextParsingUtilities::metadataValue(const std::string_view line, const std::string_view key)
{
    const std::string prefix = "#@ " + std::string(key);
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    if (line.size() > prefix.size() && !StringUtilities::isAsciiWhitespace(line[prefix.size()])) {
        return std::nullopt;
    }

    return std::string(StringUtilities::trimAsciiWhitespace(line.substr(prefix.size())));
}

}  // namespace skygate::ephemeris
