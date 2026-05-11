#include "catalog/io/zip/ZipDirectoryReader.hpp"

#include "catalog/io/zip/ZipBinaryUtilities.hpp"

#include <cstddef>
#include <cstdint>

namespace skygate::ephemeris {
namespace {

constexpr std::uint32_t kZipCentralDirectoryHeaderSignature = 0x02014b50U;
constexpr std::uint32_t kZipEndOfCentralDirectorySignature = 0x06054b50U;
constexpr std::size_t kZipCentralDirectoryHeaderSize = 46U;
constexpr std::size_t kZipEndOfCentralDirectoryMinSize = 22U;
constexpr std::size_t kZipMaxCommentBytes = 0xffffU;

[[nodiscard]] std::optional<std::size_t> findEndOfCentralDirectoryOffset(
    const std::string_view zipData
)
{
    if (zipData.size() < kZipEndOfCentralDirectoryMinSize) {
        return std::nullopt;
    }

    const std::size_t searchWindow = kZipEndOfCentralDirectoryMinSize + kZipMaxCommentBytes;
    const std::size_t minOffset = zipData.size() > searchWindow
        ? zipData.size() - searchWindow
        : 0U;
    for (std::size_t offset = zipData.size() - kZipEndOfCentralDirectoryMinSize;; --offset) {
        const auto signature = zip_binary::readLe32(zipData, offset);
        if (signature.has_value() && *signature == kZipEndOfCentralDirectorySignature) {
            return offset;
        }
        if (offset == minOffset) {
            break;
        }
    }

    return std::nullopt;
}

}  // namespace

bool ZipEntryMetadata::isDirectory() const
{
    return !path.empty() && path.back() == '/';
}

bool ZipEntryMetadata::isEncrypted() const
{
    return (generalPurposeFlag & 0x1U) != 0U;
}

std::optional<std::vector<ZipEntryMetadata>> ZipDirectoryReader::readEntries(
    const std::string_view zipData
)
{
    const auto endOfCentralDirectoryOffset = findEndOfCentralDirectoryOffset(zipData);
    if (!endOfCentralDirectoryOffset.has_value()) {
        return std::nullopt;
    }

    const auto centralDirectorySize = zip_binary::readLe32(
        zipData,
        *endOfCentralDirectoryOffset + 12U
    );
    const auto centralDirectoryOffset = zip_binary::readLe32(
        zipData,
        *endOfCentralDirectoryOffset + 16U
    );
    if (!centralDirectorySize.has_value() || !centralDirectoryOffset.has_value()) {
        return std::nullopt;
    }

    const std::size_t centralStart = static_cast<std::size_t>(*centralDirectoryOffset);
    const std::size_t centralSize = static_cast<std::size_t>(*centralDirectorySize);
    if (centralStart > zipData.size() || centralSize > (zipData.size() - centralStart)) {
        return std::nullopt;
    }

    const std::size_t centralEnd = centralStart + centralSize;
    std::vector<ZipEntryMetadata> entries;

    std::size_t cursor = centralStart;
    while (cursor < centralEnd) {
        if (cursor + kZipCentralDirectoryHeaderSize > centralEnd) {
            return std::nullopt;
        }

        const auto signature = zip_binary::readLe32(zipData, cursor);
        if (!signature.has_value() || *signature != kZipCentralDirectoryHeaderSignature) {
            return std::nullopt;
        }

        const auto generalPurposeFlag = zip_binary::readLe16(zipData, cursor + 8U);
        const auto compressionMethod = zip_binary::readLe16(zipData, cursor + 10U);
        const auto compressedSize = zip_binary::readLe32(zipData, cursor + 20U);
        const auto uncompressedSize = zip_binary::readLe32(zipData, cursor + 24U);
        const auto fileNameLength = zip_binary::readLe16(zipData, cursor + 28U);
        const auto extraFieldLength = zip_binary::readLe16(zipData, cursor + 30U);
        const auto commentLength = zip_binary::readLe16(zipData, cursor + 32U);
        const auto localHeaderOffset = zip_binary::readLe32(zipData, cursor + 42U);
        if (
            !generalPurposeFlag.has_value()
            || !compressionMethod.has_value()
            || !compressedSize.has_value()
            || !uncompressedSize.has_value()
            || !fileNameLength.has_value()
            || !extraFieldLength.has_value()
            || !commentLength.has_value()
            || !localHeaderOffset.has_value()
        ) {
            return std::nullopt;
        }

        const std::size_t nameOffset = cursor + kZipCentralDirectoryHeaderSize;
        const std::size_t nameLength = static_cast<std::size_t>(*fileNameLength);
        const std::size_t extraLength = static_cast<std::size_t>(*extraFieldLength);
        const std::size_t fileCommentLength = static_cast<std::size_t>(*commentLength);
        const std::size_t nextCursor = nameOffset + nameLength + extraLength + fileCommentLength;
        if (nextCursor < nameOffset || nextCursor > centralEnd) {
            return std::nullopt;
        }

        entries.push_back(ZipEntryMetadata {
            .path = std::string(zipData.substr(nameOffset, nameLength)),
            .localHeaderOffset = static_cast<std::size_t>(*localHeaderOffset),
            .compressedSize = static_cast<std::size_t>(*compressedSize),
            .uncompressedSize = static_cast<std::size_t>(*uncompressedSize),
            .compressionMethod = *compressionMethod,
            .generalPurposeFlag = *generalPurposeFlag
        });

        cursor = nextCursor;
    }

    return entries;
}

}  // namespace skygate::ephemeris
