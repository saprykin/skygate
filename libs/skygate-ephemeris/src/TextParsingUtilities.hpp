#pragma once

#include "time/CivilDateTime.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class TextParsingUtilities final {
public:
    TextParsingUtilities() = delete;

    [[nodiscard]] static bool parseBool(std::string_view text, bool& value) noexcept;
    [[nodiscard]] static std::vector<std::string_view> splitAsciiWhitespace(std::string_view text);
    // Keeps empty fields and trims them, unlike StringUtilities::splitView; quotes are data.
    [[nodiscard]] static std::vector<std::string_view> splitCommaSeparated(std::string_view text);
    // Returns a trimmed column, clipped to the available text.
    [[nodiscard]] static std::string_view
    fixedColumn(std::string_view text, std::size_t offset, std::size_t length) noexcept;
    // Parses an English three-letter month abbreviation such as "Jan".
    [[nodiscard]] static std::optional<int> parseEnglishMonthAbbreviation(std::string_view text) noexcept;
    [[nodiscard]] static std::optional<skygate::core::CivilDateTime> parseUtcDate(std::string_view text) noexcept;
    [[nodiscard]] static std::optional<std::string> metadataValue(std::string_view line, std::string_view key);
};

}  // namespace skygate::ephemeris
