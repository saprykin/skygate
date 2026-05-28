#pragma once

#include "time/CivilDateTime.hpp"

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
    [[nodiscard]] bool parseInt(std::string_view text, int& value) const noexcept;
    [[nodiscard]] bool parseUint64(std::string_view text, std::uint64_t& value) const noexcept;
    [[nodiscard]] bool parseFiniteDouble(std::string_view text, double& value) const noexcept;
    [[nodiscard]] std::vector<std::string_view> splitAsciiWhitespace(std::string_view text) const;
    [[nodiscard]] std::optional<CivilDateTime> parseUtcDate(std::string_view text) const noexcept;
    [[nodiscard]] std::optional<std::string> metadataValue(std::string_view line, std::string_view key) const;
};

}  // namespace skygate::ephemeris
