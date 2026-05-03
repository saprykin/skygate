#include "common/StringUtilities.hpp"

#include <algorithm>
#include <cctype>

namespace skygate::ephemeris::strings {

char toLowerAscii(const char character) noexcept
{
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character - 'A' + 'a')
        : character;
}

std::string toLowerAscii(const std::string_view value)
{
    std::string lowered(value);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](const char character) {
            return toLowerAscii(character);
        }
    );
    return lowered;
}

std::string_view trimAsciiWhitespace(std::string_view value) noexcept
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }

    return value;
}

bool equalsIgnoreAsciiCase(
    const std::string_view lhs,
    const std::string_view rhs
) noexcept
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

bool containsIgnoreAsciiCase(
    const std::string_view value,
    const std::string_view token
)
{
    return toLowerAscii(value).find(toLowerAscii(token)) != std::string::npos;
}

std::string normalizedLookupKey(const std::string_view value)
{
    return toLowerAscii(trimAsciiWhitespace(value));
}

std::string normalizedAlnumKey(const std::string_view value)
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

bool appendUnique(std::vector<std::string>& values, std::string value)
{
    if (value.empty() || std::find(values.begin(), values.end(), value) != values.end()) {
        return false;
    }

    values.push_back(std::move(value));
    return true;
}

bool appendUniqueIgnoreAsciiCase(std::vector<std::string>& values, std::string value)
{
    if (value.empty()) {
        return false;
    }

    const auto existingIt = std::find_if(
        values.begin(),
        values.end(),
        [&value](const std::string& existing) {
            return equalsIgnoreAsciiCase(existing, value);
        }
    );
    if (existingIt != values.end()) {
        return false;
    }

    values.push_back(std::move(value));
    return true;
}

}  // namespace skygate::ephemeris::strings
