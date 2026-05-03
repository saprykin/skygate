#include "catalog/ZipArchiveReader.hpp"

#include "catalog/CompressedDataInflater.hpp"
#include "common/StringUtilities.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {
namespace {

constexpr std::uint32_t kZipLocalFileHeaderSignature = 0x04034b50U;
constexpr std::uint32_t kZipCentralDirectoryHeaderSignature = 0x02014b50U;
constexpr std::uint32_t kZipEndOfCentralDirectorySignature = 0x06054b50U;
constexpr std::size_t kZipCentralDirectoryHeaderSize = 46U;
constexpr std::size_t kZipLocalFileHeaderSize = 30U;
constexpr std::size_t kZipEndOfCentralDirectoryMinSize = 22U;
constexpr std::size_t kZipMaxCommentBytes = 0xffffU;

struct ZipEntry final {
    std::size_t localHeaderOffset = 0;
    std::size_t compressedSize = 0;
    std::size_t uncompressedSize = 0;
    std::uint16_t compressionMethod = 0;
};

[[nodiscard]] std::optional<std::uint16_t> readLe16(
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

[[nodiscard]] std::optional<std::uint32_t> readLe32(
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

[[nodiscard]] bool looksLikeCsvPath(const std::string_view path)
{
    if (path.empty()) {
        return false;
    }

    std::size_t leafStart = path.find_last_of('/');
    if (leafStart == std::string_view::npos) {
        leafStart = 0;
    } else {
        ++leafStart;
    }
    const std::string_view leafName = path.substr(leafStart);
    if (leafName.size() < 4U) {
        return false;
    }

    return strings::equalsIgnoreAsciiCase(
        leafName.substr(leafName.size() - 4U),
        ".csv"
    );
}

[[nodiscard]] std::optional<std::size_t> findEndOfCentralDirectoryOffset(
    const std::string_view zipData
)
{
    if (zipData.size() < kZipEndOfCentralDirectoryMinSize) {
        return std::nullopt;
    }

    const std::size_t minOffset = zipData.size() > (kZipEndOfCentralDirectoryMinSize + kZipMaxCommentBytes)
        ? zipData.size() - (kZipEndOfCentralDirectoryMinSize + kZipMaxCommentBytes)
        : 0U;
    for (std::size_t offset = zipData.size() - kZipEndOfCentralDirectoryMinSize;; --offset) {
        const auto signature = readLe32(zipData, offset);
        if (signature.has_value() && *signature == kZipEndOfCentralDirectorySignature) {
            return offset;
        }
        if (offset == minOffset) {
            break;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<ZipEntry> chooseZipEntry(const std::string_view zipData)
{
    const auto endOfCentralDirectoryOffset = findEndOfCentralDirectoryOffset(zipData);
    if (!endOfCentralDirectoryOffset.has_value()) {
        return std::nullopt;
    }

    const auto centralDirectorySize = readLe32(zipData, *endOfCentralDirectoryOffset + 12U);
    const auto centralDirectoryOffset = readLe32(zipData, *endOfCentralDirectoryOffset + 16U);
    if (!centralDirectorySize.has_value() || !centralDirectoryOffset.has_value()) {
        return std::nullopt;
    }

    const std::size_t centralStart = static_cast<std::size_t>(*centralDirectoryOffset);
    const std::size_t centralSize = static_cast<std::size_t>(*centralDirectorySize);
    if (centralStart > zipData.size() || centralSize > (zipData.size() - centralStart)) {
        return std::nullopt;
    }

    const std::size_t centralEnd = centralStart + centralSize;
    std::optional<ZipEntry> csvCandidate;
    std::optional<ZipEntry> firstCandidate;

    std::size_t cursor = centralStart;
    while (cursor < centralEnd) {
        if (cursor + kZipCentralDirectoryHeaderSize > centralEnd) {
            return std::nullopt;
        }

        const auto signature = readLe32(zipData, cursor);
        if (!signature.has_value() || *signature != kZipCentralDirectoryHeaderSignature) {
            return std::nullopt;
        }

        const auto generalPurposeFlag = readLe16(zipData, cursor + 8U);
        const auto compressionMethod = readLe16(zipData, cursor + 10U);
        const auto compressedSize = readLe32(zipData, cursor + 20U);
        const auto uncompressedSize = readLe32(zipData, cursor + 24U);
        const auto fileNameLength = readLe16(zipData, cursor + 28U);
        const auto extraFieldLength = readLe16(zipData, cursor + 30U);
        const auto commentLength = readLe16(zipData, cursor + 32U);
        const auto localHeaderOffset = readLe32(zipData, cursor + 42U);
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

        const std::string_view fileName = zipData.substr(nameOffset, nameLength);
        const bool isDirectory = !fileName.empty() && fileName.back() == '/';
        const bool isEncrypted = (*generalPurposeFlag & 0x1U) != 0U;
        if (!isDirectory && !isEncrypted) {
            ZipEntry entry;
            entry.localHeaderOffset = static_cast<std::size_t>(*localHeaderOffset);
            entry.compressedSize = static_cast<std::size_t>(*compressedSize);
            entry.uncompressedSize = static_cast<std::size_t>(*uncompressedSize);
            entry.compressionMethod = *compressionMethod;
            if (!firstCandidate.has_value()) {
                firstCandidate = entry;
            }
            if (looksLikeCsvPath(fileName)) {
                csvCandidate = entry;
                break;
            }
        }

        cursor = nextCursor;
    }

    if (csvCandidate.has_value()) {
        return csvCandidate;
    }
    return firstCandidate;
}

[[nodiscard]] std::optional<std::string> extractZipEntry(
    const std::string_view zipData,
    const ZipEntry& entry
)
{
    if (entry.localHeaderOffset + kZipLocalFileHeaderSize > zipData.size()) {
        return std::nullopt;
    }

    const auto localHeaderSignature = readLe32(zipData, entry.localHeaderOffset);
    const auto fileNameLength = readLe16(zipData, entry.localHeaderOffset + 26U);
    const auto extraFieldLength = readLe16(zipData, entry.localHeaderOffset + 28U);
    if (
        !localHeaderSignature.has_value()
        || *localHeaderSignature != kZipLocalFileHeaderSignature
        || !fileNameLength.has_value()
        || !extraFieldLength.has_value()
    ) {
        return std::nullopt;
    }

    const std::size_t fileDataOffset = entry.localHeaderOffset
        + kZipLocalFileHeaderSize
        + static_cast<std::size_t>(*fileNameLength)
        + static_cast<std::size_t>(*extraFieldLength);
    if (fileDataOffset > zipData.size() || entry.compressedSize > (zipData.size() - fileDataOffset)) {
        return std::nullopt;
    }

    const std::string_view compressedData = zipData.substr(fileDataOffset, entry.compressedSize);
    if (entry.compressionMethod == 0U) {
        if (entry.uncompressedSize > 0U && compressedData.size() != entry.uncompressedSize) {
            return std::nullopt;
        }
        if (compressedData.empty()) {
            return std::nullopt;
        }
        return std::string(compressedData);
    }

    if (entry.compressionMethod == 8U) {
        return CompressedDataInflater::inflate(
            compressedData,
            CompressedDataFormat::RawDeflate,
            CompressedDataInflateOptions {
                .expectedOutputBytes = entry.uncompressedSize,
                .allowEmptyOutput = false
            }
        );
    }

    return std::nullopt;
}

}  // namespace

std::optional<std::string> ZipArchiveReader::extractFirstCsvEntry(
    const std::string_view zipData
)
{
    const auto entry = chooseZipEntry(zipData);
    if (!entry.has_value()) {
        return std::nullopt;
    }

    const auto entryData = extractZipEntry(zipData, *entry);
    if (!entryData.has_value() || entryData->empty()) {
        return std::nullopt;
    }

    return entryData;
}

}  // namespace skygate::ephemeris
