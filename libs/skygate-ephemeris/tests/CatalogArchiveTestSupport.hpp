#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::tests {

struct ZipEntrySpec final {
    std::string path;
    std::string data;
    std::uint16_t compressionMethod = 0;
    std::uint16_t generalPurposeFlag = 0;
};

inline void appendLe16(std::string& data, const std::uint16_t value)
{
    data.push_back(static_cast<char>(value & 0xffU));
    data.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

inline void appendLe32(std::string& data, const std::uint32_t value)
{
    appendLe16(data, static_cast<std::uint16_t>(value & 0xffffU));
    appendLe16(data, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

[[nodiscard]] inline std::string makeZip(
    const std::vector<ZipEntrySpec>& entries,
    const std::string_view comment = {}
)
{
    std::string zipData;
    std::vector<std::uint32_t> localHeaderOffsets;
    localHeaderOffsets.reserve(entries.size());

    for (const ZipEntrySpec& entry : entries) {
        localHeaderOffsets.push_back(static_cast<std::uint32_t>(zipData.size()));
        appendLe32(zipData, 0x04034b50U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, entry.generalPurposeFlag);
        appendLe16(zipData, entry.compressionMethod);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(zipData, static_cast<std::uint16_t>(entry.path.size()));
        appendLe16(zipData, 0U);
        zipData += entry.path;
        zipData += entry.data;
    }

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t>(zipData.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const ZipEntrySpec& entry = entries[index];
        appendLe32(zipData, 0x02014b50U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, entry.generalPurposeFlag);
        appendLe16(zipData, entry.compressionMethod);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(zipData, static_cast<std::uint16_t>(entry.path.size()));
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, localHeaderOffsets[index]);
        zipData += entry.path;
    }

    const std::uint32_t centralDirectorySize =
        static_cast<std::uint32_t>(zipData.size()) - centralDirectoryOffset;
    appendLe32(zipData, 0x06054b50U);
    appendLe16(zipData, 0U);
    appendLe16(zipData, 0U);
    appendLe16(zipData, static_cast<std::uint16_t>(entries.size()));
    appendLe16(zipData, static_cast<std::uint16_t>(entries.size()));
    appendLe32(zipData, centralDirectorySize);
    appendLe32(zipData, centralDirectoryOffset);
    appendLe16(zipData, static_cast<std::uint16_t>(comment.size()));
    zipData.append(comment.data(), comment.size());
    return zipData;
}

}  // namespace skygate::ephemeris::tests
