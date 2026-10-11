#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"

#include <charconv>
#include <cmath>

namespace skygate::ephemeris {

std::optional<double> NumericParsingUtilities::parseFiniteDouble(const std::string_view text) noexcept
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    double value = 0.0;
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) {
        return std::nullopt;
    }

    return value;
}

std::optional<int> NumericParsingUtilities::parseInt(const std::string_view text) noexcept
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    int value = 0;
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }

    return value;
}

std::optional<std::uint64_t> NumericParsingUtilities::parseUint64(const std::string_view text) noexcept
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::uint64_t value = 0U;
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }

    return value;
}

}  // namespace skygate::ephemeris
