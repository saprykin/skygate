#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

// Shared numeric text primitives for the ephemeris parsers and the catalog
// readers. They trim ASCII whitespace, require full consumption, and reject a
// leading '+'. The catalog parsing utilities strip one leading '+' from
// doubles before calling through, so catalog input accepts forms these
// primitives reject.
class NumericParsingUtilities final {
public:
    NumericParsingUtilities() = delete;

    // Trims ASCII whitespace and accepts only text that fully consumes as a
    // finite double.
    [[nodiscard]] static std::optional<double> parseFiniteDouble(std::string_view text) noexcept;

    // Trims ASCII whitespace and accepts only text that fully consumes as a
    // signed integer.
    [[nodiscard]] static std::optional<int> parseInt(std::string_view text) noexcept;

    // Trims ASCII whitespace and accepts only text that fully consumes as an
    // unsigned integer.
    [[nodiscard]] static std::optional<std::uint64_t> parseUint64(std::string_view text) noexcept;
};

}  // namespace skygate::ephemeris
