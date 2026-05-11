#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris::zip_binary {

[[nodiscard]] inline std::optional<std::uint16_t> readLe16(
    const std::string_view data,
    const std::size_t offset
)
{
    if (offset + 2U > data.size()) {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset]))
        | (static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1U])) << 8U)
    );
}

[[nodiscard]] inline std::optional<std::uint32_t> readLe32(
    const std::string_view data,
    const std::size_t offset
)
{
    if (offset + 4U > data.size()) {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 1U])) << 8U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 2U])) << 16U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 3U])) << 24U)
    );
}

}  // namespace skygate::ephemeris::zip_binary
