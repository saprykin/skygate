#pragma once

#include "time/CivilDateTime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class TextParsingUtilities final {
public:
    TextParsingUtilities() = delete;

    [[nodiscard]] static std::string_view trimAsciiWhitespace(std::string_view text) noexcept;
    [[nodiscard]] static bool parseBool(std::string_view text, bool& value) noexcept;
    [[nodiscard]] static bool parseInt(std::string_view text, int& value) noexcept;
    [[nodiscard]] static bool parseUint64(std::string_view text, std::uint64_t& value) noexcept;
    [[nodiscard]] static bool parseFiniteDouble(std::string_view text, double& value) noexcept;
    [[nodiscard]] static std::vector<std::string_view> splitAsciiWhitespace(std::string_view text);
    // Trims fields and preserves empty fields; quotation marks have no special meaning.
    [[nodiscard]] static std::vector<std::string_view> splitCommaSeparated(std::string_view text);
    // Returns a trimmed column, clipped to the available text.
    [[nodiscard]] static std::string_view
    fixedColumn(std::string_view text, std::size_t offset, std::size_t length) noexcept;
    // Consumes one LF-delimited line, removing a trailing CR but preserving other whitespace.
    [[nodiscard]] static std::string_view takeLine(std::string_view& remaining) noexcept;
    // Parses an English three-letter month abbreviation such as "Jan".
    [[nodiscard]] static std::optional<int> parseEnglishMonthAbbreviation(std::string_view text) noexcept;
    [[nodiscard]] static std::optional<skygate::core::CivilDateTime> parseUtcDate(std::string_view text) noexcept;
    [[nodiscard]] static std::optional<std::string> metadataValue(std::string_view line, std::string_view key);
};

}  // namespace skygate::ephemeris
