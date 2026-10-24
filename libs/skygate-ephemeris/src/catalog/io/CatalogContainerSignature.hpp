#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

// Container and archive-structure magic bytes shared by catalog payload
// routing, ZIP directory parsing, and ZIP entry extraction. Keeping the three
// ZIP record signatures in one set means a reader and the router never disagree
// about what a valid record starts with.
class CatalogContainerSignature final {
public:
    CatalogContainerSignature() = delete;

    static constexpr std::uint32_t kZipLocalFileHeaderSignature = 0x04034b50U;
    static constexpr std::uint32_t kZipCentralDirectoryHeaderSignature = 0x02014b50U;
    static constexpr std::uint32_t kZipEndOfCentralDirectorySignature = 0x06054b50U;

    [[nodiscard]] static constexpr bool isGzip(const std::string_view payload) noexcept
    {
        if (payload.size() < 2U) {
            return false;
        }

        const auto firstByte = static_cast<unsigned char>(payload[0]);
        const auto secondByte = static_cast<unsigned char>(payload[1]);
        return firstByte == 0x1fU && secondByte == 0x8bU;
    }

    [[nodiscard]] static constexpr bool isZip(const std::string_view payload) noexcept
    {
        if (payload.size() < 4U) {
            return false;
        }

        const auto firstByte = static_cast<unsigned char>(payload[0]);
        const auto secondByte = static_cast<unsigned char>(payload[1]);
        const auto thirdByte = static_cast<unsigned char>(payload[2]);
        const auto fourthByte = static_cast<unsigned char>(payload[3]);
        return firstByte == 0x50U && secondByte == 0x4bU
               && ((thirdByte == 0x03U && fourthByte == 0x04U) || (thirdByte == 0x05U && fourthByte == 0x06U)
                   || (thirdByte == 0x07U && fourthByte == 0x08U));
    }
};

}  // namespace skygate::ephemeris
