#pragma once

#include <QString>

#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

// Shared text helpers of the ephemeris and catalog code: ASCII classification
// and normalization, UTF-8 payload bridges, and unique-value unions. Payload
// text is UTF-8, so every codec converts through one bridge here instead of
// repeating the conversion at each boundary.
class StringUtilities {
public:
    StringUtilities() = delete;

    [[nodiscard]] static char toLowerAscii(char character) noexcept;
    [[nodiscard]] static std::string toLowerAscii(std::string_view value);
    [[nodiscard]] static bool isAsciiWhitespace(char character) noexcept;
    [[nodiscard]] static bool isAsciiDigit(char character) noexcept;
    [[nodiscard]] static bool isAsciiLetter(char character) noexcept;
    [[nodiscard]] static std::string_view trimAsciiWhitespace(std::string_view value) noexcept;
    [[nodiscard]] static std::string_view takeLine(std::string_view& remaining) noexcept;
    [[nodiscard]] static bool equalsIgnoreAsciiCase(std::string_view lhs, std::string_view rhs) noexcept;
    [[nodiscard]] static bool containsIgnoreAsciiCase(std::string_view value, std::string_view token);

    // The UTF-8 bridge of a decoded payload text: the bytes a std::string field
    // stores and the Qt text a codec manipulates are the same UTF-8 payload text.
    [[nodiscard]] static std::string toUtf8(const QString& text);
    [[nodiscard]] static QString fromUtf8(std::string_view text);

    [[nodiscard]] static std::string normalizedLookupKey(std::string_view value);
    [[nodiscard]] static std::string normalizedAlnumKey(std::string_view value);
    // Normalizes a display label: trims, turns `_` and `-` separators into
    // single spaces, and collapses whitespace runs.
    [[nodiscard]] static QString normalizedLabelText(QString text);

    [[nodiscard]] static std::vector<std::string_view> splitView(std::string_view text, char delimiter);

    static bool appendUnique(std::vector<std::string>& values, std::string value);
    static bool appendUniqueIgnoreAsciiCase(std::vector<std::string>& values, std::string value);
};

}  // namespace skygate::ephemeris
