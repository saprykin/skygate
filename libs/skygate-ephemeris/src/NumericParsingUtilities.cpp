#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {
namespace {

// Trims ASCII whitespace and accepts only text that fully consumes as `Value`.
template <typename Value>
[[nodiscard]] std::optional<Value> parseStrictValue(const std::string_view text) noexcept
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    Value value{};
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }

    return value;
}

}  // namespace

std::optional<double> NumericParsingUtilities::parseFiniteDouble(const std::string_view text) noexcept
{
    const std::optional<double> value = parseStrictValue<double>(text);
    if (!value.has_value() || !std::isfinite(*value)) {
        return std::nullopt;
    }

    return value;
}

std::optional<int> NumericParsingUtilities::parseInt(const std::string_view text) noexcept
{
    return parseStrictValue<int>(text);
}

std::optional<std::uint64_t> NumericParsingUtilities::parseUint64(const std::string_view text) noexcept
{
    return parseStrictValue<std::uint64_t>(text);
}

}  // namespace skygate::ephemeris
