#include "EphemerisDataFileUtilities.hpp"
#include "StringUtilities.hpp"

#include <QFileInfo>

namespace skygate::ephemeris {

QString EphemerisDataFileUtilities::pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

std::filesystem::path EphemerisDataFileUtilities::pathFromQString(const QString& path)
{
    return std::filesystem::path(path.toStdString());
}

QString EphemerisDataFileUtilities::stringToQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

bool EphemerisDataFileUtilities::hasUnsafePathComponent(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute()) {
        return true;
    }

    for (const std::filesystem::path& component : path) {
        if (component.empty() || component == "." || component == "..") {
            return true;
        }
    }

    return false;
}

bool EphemerisDataFileUtilities::sourceSizeMatchesMetadata(
    const QFileInfo& sourceInfo, const EphemerisDataManifest::Asset& asset
)
{
    return !asset.compression.compressedSizeBytes.has_value()
           || sourceInfo.size() == static_cast<qint64>(*asset.compression.compressedSizeBytes);
}

EphemerisDataFileUtilities::PayloadVerification EphemerisDataFileUtilities::verifyDecodedPayload(
    const EphemerisDataManifest::Asset& asset, const std::uint64_t outputBytes, const std::string_view decodedChecksum
) noexcept
{
    if (asset.live) {
        return outputBytes == 0U ? PayloadVerification::EmptyLivePayload : PayloadVerification::Verified;
    }
    if (asset.compression.uncompressedSizeBytes.has_value()
        && outputBytes != *asset.compression.uncompressedSizeBytes) {
        return PayloadVerification::UncompressedSizeMismatch;
    }
    if (!checksumsMatch(decodedChecksum, asset.checksum.value)) {
        return PayloadVerification::ChecksumMismatch;
    }

    return PayloadVerification::Verified;
}

bool EphemerisDataFileUtilities::cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

bool EphemerisDataFileUtilities::checksumsMatch(const std::string_view actual, const std::string_view expected) noexcept
{
    return StringUtilities::equalsIgnoreAsciiCase(actual, expected);
}

}  // namespace skygate::ephemeris
