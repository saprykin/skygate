#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::strings {

[[nodiscard]] char toLowerAscii(char character) noexcept;
[[nodiscard]] std::string toLowerAscii(std::string_view value);
[[nodiscard]] std::string_view trimAsciiWhitespace(std::string_view value) noexcept;
[[nodiscard]] bool equalsIgnoreAsciiCase(std::string_view lhs, std::string_view rhs) noexcept;
[[nodiscard]] bool containsIgnoreAsciiCase(std::string_view value, std::string_view token);
[[nodiscard]] std::string normalizedLookupKey(std::string_view value);
[[nodiscard]] std::string normalizedAlnumKey(std::string_view value);

bool appendUnique(std::vector<std::string>& values, std::string value);
bool appendUniqueIgnoreAsciiCase(std::vector<std::string>& values, std::string value);

}  // namespace skygate::ephemeris::strings
