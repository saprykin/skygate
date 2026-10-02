#pragma once

#include "time/CivilDateTime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class HighPrecisionTextParser final {
public:
    [[nodiscard]] std::string_view trimAsciiWhitespace(std::string_view text) const noexcept;
    [[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix) const noexcept;
    [[nodiscard]] bool parseBool(std::string_view text, bool& value) const noexcept;
    [[nodiscard]] bool parseInt(std::string_view text, int& value) const noexcept;
    [[nodiscard]] bool parseUint64(std::string_view text, std::uint64_t& value) const noexcept;
    [[nodiscard]] bool parseFiniteDouble(std::string_view text, double& value) const noexcept;
    [[nodiscard]] std::vector<std::string_view> splitAsciiWhitespace(std::string_view text) const;
    // Trims fields and preserves empty fields; quotation marks have no special meaning.
    [[nodiscard]] std::vector<std::string_view> splitCommaSeparated(std::string_view text) const;
    // Returns a trimmed column, clipped to the available text.
    [[nodiscard]] std::string_view
    fixedColumn(std::string_view text, std::size_t offset, std::size_t length) const noexcept;
    // Consumes one LF-delimited line, removing a trailing CR but preserving other whitespace.
    [[nodiscard]] std::string_view takeLine(std::string_view& remaining) const noexcept;
    [[nodiscard]] std::optional<skygate::core::CivilDateTime> parseUtcDate(std::string_view text) const noexcept;
    [[nodiscard]] std::optional<std::string> metadataValue(std::string_view line, std::string_view key) const;
};

}  // namespace skygate::ephemeris
