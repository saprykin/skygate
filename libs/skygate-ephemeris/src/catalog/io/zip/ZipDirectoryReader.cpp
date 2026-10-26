#include "ZipDirectoryReader.hpp"
#include "ZipBinaryUtilities.hpp"
#include "ZipFieldReader.hpp"
#include "catalog/io/CatalogContainerSignature.hpp"

#include <cstddef>
#include <cstdint>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kZipCentralDirectoryHeaderSize = 46U;
constexpr std::size_t kZipEndOfCentralDirectoryMinSize = 22U;
constexpr std::size_t kZipMaxCommentBytes = 0xffffU;

[[nodiscard]] std::optional<std::size_t> findEndOfCentralDirectoryOffset(const std::string_view zipData)
{
    if (zipData.size() < kZipEndOfCentralDirectoryMinSize) {
        return std::nullopt;
    }

    const std::size_t searchWindow = kZipEndOfCentralDirectoryMinSize + kZipMaxCommentBytes;
    const std::size_t minOffset = zipData.size() > searchWindow ? zipData.size() - searchWindow : 0U;
    for (std::size_t offset = zipData.size() - kZipEndOfCentralDirectoryMinSize;; --offset) {
        const auto signature = ZipBinaryUtilities::readLe32(zipData, offset);
        if (signature.has_value() && *signature == CatalogContainerSignature::kZipEndOfCentralDirectorySignature) {
            return offset;
        }
        if (offset == minOffset) {
            break;
        }
    }

    return std::nullopt;
}

}  // namespace

std::optional<std::vector<ZipEntryMetadata>> ZipDirectoryReader::readEntries(const std::string_view zipData)
{
    const auto endOfCentralDirectoryOffset = findEndOfCentralDirectoryOffset(zipData);
    if (!endOfCentralDirectoryOffset.has_value()) {
        return std::nullopt;
    }

    const auto centralDirectorySize = ZipBinaryUtilities::readLe32(zipData, *endOfCentralDirectoryOffset + 12U);
    const auto centralDirectoryOffset = ZipBinaryUtilities::readLe32(zipData, *endOfCentralDirectoryOffset + 16U);
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

        ZipFieldReader fields(zipData);
        if (fields.le32(cursor) != CatalogContainerSignature::kZipCentralDirectoryHeaderSignature) {
            return std::nullopt;
        }

        const std::uint16_t generalPurposeFlag = fields.le16(cursor + 8U);
        const std::uint16_t compressionMethod = fields.le16(cursor + 10U);
        const std::uint32_t compressedSize = fields.le32(cursor + 20U);
        const std::uint32_t uncompressedSize = fields.le32(cursor + 24U);
        const std::uint16_t fileNameLength = fields.le16(cursor + 28U);
        const std::uint16_t extraFieldLength = fields.le16(cursor + 30U);
        const std::uint16_t commentLength = fields.le16(cursor + 32U);
        const std::uint32_t localHeaderOffset = fields.le32(cursor + 42U);
        if (!fields.isComplete()) {
            return std::nullopt;
        }

        const std::size_t nameOffset = cursor + kZipCentralDirectoryHeaderSize;
        const std::size_t nameLength = static_cast<std::size_t>(fileNameLength);
        const std::size_t extraLength = static_cast<std::size_t>(extraFieldLength);
        const std::size_t fileCommentLength = static_cast<std::size_t>(commentLength);
        const std::size_t nextCursor = nameOffset + nameLength + extraLength + fileCommentLength;
        if (nextCursor < nameOffset || nextCursor > centralEnd) {
            return std::nullopt;
        }

        entries.push_back(
            ZipEntryMetadata{
                .path = std::string(zipData.substr(nameOffset, nameLength)),
                .localHeaderOffset = static_cast<std::size_t>(localHeaderOffset),
                .compressedSize = static_cast<std::size_t>(compressedSize),
                .uncompressedSize = static_cast<std::size_t>(uncompressedSize),
                .compressionMethod = compressionMethod,
                .generalPurposeFlag = generalPurposeFlag
            }
        );

        cursor = nextCursor;
    }

    return entries;
}

}  // namespace skygate::ephemeris
