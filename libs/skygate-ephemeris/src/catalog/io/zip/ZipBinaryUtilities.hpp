#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class ZipBinaryUtilities final {
public:
    ZipBinaryUtilities() = delete;

    [[nodiscard]] static std::optional<std::uint16_t> readLe16(std::string_view data, std::size_t offset);
    [[nodiscard]] static std::optional<std::uint32_t> readLe32(std::string_view data, std::size_t offset);
};

}  // namespace skygate::ephemeris
