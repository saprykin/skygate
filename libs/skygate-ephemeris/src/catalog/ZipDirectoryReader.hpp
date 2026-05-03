#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

struct ZipEntryMetadata final {
    std::string path;
    std::size_t localHeaderOffset = 0;
    std::size_t compressedSize = 0;
    std::size_t uncompressedSize = 0;
    std::uint16_t compressionMethod = 0;
    std::uint16_t generalPurposeFlag = 0;

    [[nodiscard]] bool isDirectory() const;
    [[nodiscard]] bool isEncrypted() const;
};

class ZipDirectoryReader final {
public:
    [[nodiscard]] static std::optional<std::vector<ZipEntryMetadata>> readEntries(
        std::string_view zipData
    );
};

}  // namespace skygate::ephemeris
