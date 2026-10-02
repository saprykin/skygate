#include "StringUtilities.hpp"

#include <algorithm>
#include <cctype>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] bool isAsciiWhitespace(const char character) noexcept
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

}  // namespace

char StringUtilities::toLowerAscii(const char character) noexcept
{
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
}

std::string StringUtilities::toLowerAscii(const std::string_view value)
{
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const char character) {
        return toLowerAscii(character);
    });
    return lowered;
}

std::string_view StringUtilities::trimAsciiWhitespace(const std::string_view value) noexcept
{
    std::string_view trimmed = value;
    while (!trimmed.empty() && isAsciiWhitespace(trimmed.front())) {
        trimmed.remove_prefix(1U);
    }
    while (!trimmed.empty() && isAsciiWhitespace(trimmed.back())) {
        trimmed.remove_suffix(1U);
    }
    return trimmed;
}

bool StringUtilities::equalsIgnoreAsciiCase(const std::string_view lhs, const std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (toLowerAscii(lhs[index]) != toLowerAscii(rhs[index])) {
            return false;
        }
    }

    return true;
}

bool StringUtilities::containsIgnoreAsciiCase(const std::string_view value, const std::string_view token)
{
    return toLowerAscii(value).find(toLowerAscii(token)) != std::string::npos;
}

std::string StringUtilities::normalizedLookupKey(const std::string_view value)
{
    return toLowerAscii(trimAsciiWhitespace(value));
}

std::string StringUtilities::normalizedAlnumKey(const std::string_view value)
{
    std::string key;
    key.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            key.push_back(toLowerAscii(static_cast<char>(character)));
        }
    }
    return key;
}

std::vector<std::string_view> StringUtilities::splitView(const std::string_view text, const char delimiter)
{
    std::vector<std::string_view> tokens;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const std::size_t next = text.find(delimiter, cursor);
        const std::size_t end = (next == std::string_view::npos) ? text.size() : next;
        const std::string_view token = text.substr(cursor, end - cursor);
        if (!token.empty()) {
            tokens.push_back(token);
        }
        if (next == std::string_view::npos) {
            break;
        }
        cursor = next + 1U;
    }
    return tokens;
}

bool StringUtilities::appendUnique(std::vector<std::string>& values, std::string value)
{
    if (value.empty() || std::find(values.begin(), values.end(), value) != values.end()) {
        return false;
    }

    values.push_back(std::move(value));
    return true;
}

bool StringUtilities::appendUniqueIgnoreAsciiCase(std::vector<std::string>& values, std::string value)
{
    if (value.empty()) {
        return false;
    }

    const auto existingIt = std::find_if(values.begin(), values.end(), [&value](const std::string& existing) {
        return equalsIgnoreAsciiCase(existing, value);
    });
    if (existingIt != values.end()) {
        return false;
    }

    values.push_back(std::move(value));
    return true;
}

}  // namespace skygate::ephemeris
