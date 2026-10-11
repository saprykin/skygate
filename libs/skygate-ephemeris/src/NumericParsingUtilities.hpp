#pragma once

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

// Shared numeric text primitive: both the text parser and the catalog parsing
// utilities funnel their conversions through these functions so the accepted
// syntax stays identical across parser stacks.
class NumericParsingUtilities final {
public:
    NumericParsingUtilities() = delete;

    // Trims ASCII whitespace and accepts only text that fully consumes as a
    // finite double.
    [[nodiscard]] static std::optional<double> parseFiniteDouble(std::string_view text) noexcept;
};

}  // namespace skygate::ephemeris
