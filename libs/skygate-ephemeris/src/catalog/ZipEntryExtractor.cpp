#include "catalog/ZipEntryExtractor.hpp"

#include "catalog/CompressedDataInflater.hpp"
#include "catalog/ZipBinaryUtilities.hpp"

#include <cstddef>
#include <cstdint>

namespace skygate::ephemeris {
namespace {

constexpr std::uint32_t kZipLocalFileHeaderSignature = 0x04034b50U;
constexpr std::size_t kZipLocalFileHeaderSize = 30U;

}  // namespace

std::optional<std::string> ZipEntryExtractor::extract(
    const std::string_view zipData,
    const ZipEntryMetadata& entry
)
{
    if (entry.localHeaderOffset + kZipLocalFileHeaderSize > zipData.size()) {
        return std::nullopt;
    }

    const auto localHeaderSignature = zip_binary::readLe32(zipData, entry.localHeaderOffset);
    const auto fileNameLength = zip_binary::readLe16(zipData, entry.localHeaderOffset + 26U);
    const auto extraFieldLength = zip_binary::readLe16(zipData, entry.localHeaderOffset + 28U);
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
    if (
        fileDataOffset > zipData.size()
        || entry.compressedSize > (zipData.size() - fileDataOffset)
    ) {
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

}  // namespace skygate::ephemeris
