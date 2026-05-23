#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class StringUtilities {
public:
    StringUtilities() = delete;

    [[nodiscard]] static char toLowerAscii(char character) noexcept;
    [[nodiscard]] static std::string toLowerAscii(std::string_view value);
    [[nodiscard]] static std::string_view trimAsciiWhitespace(std::string_view value) noexcept;
    [[nodiscard]] static bool equalsIgnoreAsciiCase(std::string_view lhs, std::string_view rhs) noexcept;
    [[nodiscard]] static bool containsIgnoreAsciiCase(std::string_view value, std::string_view token);
    [[nodiscard]] static std::string normalizedLookupKey(std::string_view value);
    [[nodiscard]] static std::string normalizedAlnumKey(std::string_view value);
    [[nodiscard]] static std::vector<std::string_view> splitView(std::string_view text, char delimiter);

    static bool appendUnique(std::vector<std::string>& values, std::string value);
    static bool appendUniqueIgnoreAsciiCase(std::vector<std::string>& values, std::string value);
};

}  // namespace skygate::ephemeris
