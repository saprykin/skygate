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
    return StringUtilities::fromUtf8(value);
}

bool EphemerisDataFileUtilities::hasUnsafePathComponent(const std::filesystem::path& path)
{
    if (path.empty() || path.has_root_name() || path.has_root_directory()) {
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

std::string EphemerisDataFileUtilities::payloadVerificationDiagnostic(
    const PayloadVerification verification, const PayloadVerificationWording& wording
)
{
    const std::string subject = std::string(wording.activity) + ' ' + std::string(wording.subject);
    switch (verification) {
    case PayloadVerification::Verified:
        break;
    case PayloadVerification::EmptyLivePayload:
        return std::string(wording.activity) + " live " + std::string(wording.subject) + " payload is empty.";
    case PayloadVerification::UncompressedSizeMismatch:
        return subject + ' ' + std::string(wording.sizeDetail) + " does not match manifest metadata.";
    case PayloadVerification::ChecksumMismatch:
        return subject + " checksum does not match manifest metadata.";
    }

    return {};
}

bool EphemerisDataFileUtilities::cancellationRequested(const std::function<bool()>& callback)
{
    return EphemerisDataPayloadReader::cancellationRequested(callback);
}

bool EphemerisDataFileUtilities::checksumsMatch(const std::string_view actual, const std::string_view expected) noexcept
{
    return StringUtilities::equalsIgnoreAsciiCase(actual, expected);
}

}  // namespace skygate::ephemeris
