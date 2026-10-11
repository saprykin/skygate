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

}  // namespace skygate::ephemeris
