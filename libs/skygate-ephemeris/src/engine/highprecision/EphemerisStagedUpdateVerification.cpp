#include "EphemerisStagedUpdateVerification.hpp"
#include "EphemerisDataPayloadReader.hpp"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace skygate::ephemeris {

namespace {

class HashingWriteDevice final : public QIODevice {
public:
    explicit HashingWriteDevice(QObject* parent = nullptr) : QIODevice(parent) {}

    [[nodiscard]] bool openForWrite()
    {
        return open(QIODevice::WriteOnly);
    }

protected:
    [[nodiscard]] qint64 readData(char*, qint64) override
    {
        return -1;
    }

    [[nodiscard]] qint64 writeData(const char*, const qint64 maxSize) override
    {
        return maxSize;
    }
};

[[nodiscard]] QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

[[nodiscard]] bool hasUnsafePathComponent(const std::filesystem::path& path)
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

void addDiagnostic(EphemerisStagedUpdateVerificationResult& result, const std::string_view diagnostic)
{
    result.diagnostics.emplace_back(diagnostic);
}

[[nodiscard]] bool cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

void markCanceled(EphemerisStagedUpdateVerificationResult& result)
{
    result.status = EphemerisStagedUpdateVerificationStatus::Canceled;
    addDiagnostic(result, "Staged ephemeris update verification was canceled.");
}

[[nodiscard]] bool sourceSizeMatchesMetadata(const QFileInfo& sourceInfo, const EphemerisDataManifestAsset& asset)
{
    return !asset.compression.compressedSizeBytes.has_value()
           || sourceInfo.size() == static_cast<qint64>(*asset.compression.compressedSizeBytes);
}

[[nodiscard]] QString
stagedSourcePath(const std::filesystem::path& stagedResourceRoot, const EphemerisDataManifestAsset& asset)
{
    return pathToQString(stagedResourceRoot / std::filesystem::path(asset.relativePath));
}

[[nodiscard]] bool isValidDateRange(const EphemerisDateRange& range) noexcept
{
    const double rangeStart = range.start.julianDatePart1 + range.start.julianDatePart2;
    const double rangeEnd = range.end.julianDatePart1 + range.end.julianDatePart2;
    return std::isfinite(rangeStart) && std::isfinite(rangeEnd) && rangeStart <= rangeEnd;
}

[[nodiscard]] bool
validityRangeCovers(const EphemerisDateRange& availableRange, const EphemerisDateRange& requiredRange) noexcept
{
    const double availableStart = availableRange.start.julianDatePart1 + availableRange.start.julianDatePart2;
    const double availableEnd = availableRange.end.julianDatePart1 + availableRange.end.julianDatePart2;
    const double requiredStart = requiredRange.start.julianDatePart1 + requiredRange.start.julianDatePart2;
    const double requiredEnd = requiredRange.end.julianDatePart1 + requiredRange.end.julianDatePart2;
    return availableStart <= requiredStart && requiredEnd <= availableEnd;
}

[[nodiscard]] bool
hasKind(const std::vector<EphemerisDataManifestAssetKind>& kinds, const EphemerisDataManifestAssetKind kind) noexcept
{
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

[[nodiscard]] bool hasAssetId(const std::vector<std::string>& assetIds, const std::string_view assetId) noexcept
{
    return std::find(assetIds.begin(), assetIds.end(), assetId) != assetIds.end();
}

[[nodiscard]] bool
validateAssetMetadata(const EphemerisDataManifestAsset& asset, EphemerisStagedUpdateVerificationResult& result)
{
    bool valid = true;
    if (asset.id.empty() || asset.profileId.empty() || asset.version.empty() || asset.relativePath.empty()) {
        addDiagnostic(result, "Staged ephemeris asset metadata requires id, profile, version, and relative path.");
        valid = false;
    }
    if (asset.checksum.algorithm != "sha256" || asset.checksum.value.empty()) {
        addDiagnostic(result, "Staged ephemeris asset metadata requires a sha256 checksum.");
        valid = false;
    }
    const std::filesystem::path relativePath(asset.relativePath);
    if (hasUnsafePathComponent(relativePath)) {
        addDiagnostic(result, "Staged ephemeris asset metadata contains an unsafe relative path.");
        valid = false;
    }
    if (asset.compression.kind == EphemerisDataManifestCompressionKind::Zstd
        && (!asset.compression.compressedSizeBytes.has_value() || !asset.compression.uncompressedSizeBytes.has_value()
            || *asset.compression.compressedSizeBytes == 0U || *asset.compression.uncompressedSizeBytes == 0U)) {
        addDiagnostic(result, "zstd staged ephemeris assets require positive compressed and uncompressed sizes.");
        valid = false;
    }
    if (!isValidDateRange(asset.validityRange)) {
        addDiagnostic(result, "Staged ephemeris asset metadata requires an ordered finite validity range.");
        valid = false;
    }
    if (asset.validityRange.id.empty() || asset.validityRange.displayName.empty()) {
        addDiagnostic(result, "Staged ephemeris asset metadata requires validity range id and display name.");
        valid = false;
    }

    return valid;
}

[[nodiscard]] EphemerisStagedUpdateVerificationStatus
mappedVerificationStatus(const EphemerisDataPayloadReader::Status status) noexcept
{
    switch (status) {
    case EphemerisDataPayloadReader::Status::UnsupportedCompression:
        return EphemerisStagedUpdateVerificationStatus::UnsupportedCompression;
    case EphemerisDataPayloadReader::Status::CorruptArchive:
        return EphemerisStagedUpdateVerificationStatus::CorruptArchive;
    case EphemerisDataPayloadReader::Status::Canceled:
        return EphemerisStagedUpdateVerificationStatus::Canceled;
    case EphemerisDataPayloadReader::Status::IoError:
        return EphemerisStagedUpdateVerificationStatus::IoError;
    case EphemerisDataPayloadReader::Status::Read:
        break;
    }

    return EphemerisStagedUpdateVerificationStatus::IoError;
}

[[nodiscard]] bool verifyAssetPayload(
    const EphemerisDataManifestAsset& asset,
    const QString& sourcePath,
    EphemerisStagedUpdateVerificationResult& result,
    const std::function<bool()>& cancellationCallback
)
{
    if (cancellationRequested(cancellationCallback)) {
        markCanceled(result);
        return false;
    }

    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        result.status = EphemerisStagedUpdateVerificationStatus::MissingAsset;
        addDiagnostic(result, "Staged ephemeris asset file is missing.");
        return false;
    }
    if (!sourceSizeMatchesMetadata(sourceInfo, asset)) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        addDiagnostic(result, "Staged ephemeris asset size does not match manifest metadata.");
        return false;
    }

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        result.status = EphemerisStagedUpdateVerificationStatus::IoError;
        addDiagnostic(result, "Unable to open staged ephemeris asset for verification.");
        return false;
    }

    HashingWriteDevice sink;
    if (!sink.openForWrite()) {
        result.status = EphemerisStagedUpdateVerificationStatus::IoError;
        addDiagnostic(result, "Unable to initialize staged ephemeris verification sink.");
        return false;
    }

    const EphemerisDataPayloadReader::Result payload =
        EphemerisDataPayloadReader::read(asset.compression.kind, sourceFile, sink, cancellationCallback);
    if (payload.status != EphemerisDataPayloadReader::Status::Read) {
        result.status = mappedVerificationStatus(payload.status);
        result.diagnostics.insert(result.diagnostics.end(), payload.diagnostics.begin(), payload.diagnostics.end());
        return false;
    }
    if (asset.compression.uncompressedSizeBytes.has_value()
        && payload.outputBytes != *asset.compression.uncompressedSizeBytes) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        addDiagnostic(result, "Staged ephemeris asset uncompressed size does not match manifest metadata.");
        return false;
    }
    if (payload.checksum != asset.checksum.value) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        addDiagnostic(result, "Staged ephemeris asset checksum does not match manifest metadata.");
        return false;
    }

    return true;
}

}  // namespace

EphemerisStagedUpdateVerificationResult
EphemerisStagedUpdateVerification::verify(const EphemerisStagedUpdateVerificationRequest& request)
{
    EphemerisStagedUpdateVerificationResult result;
    if (cancellationRequested(request.cancellationRequested)) {
        markCanceled(result);
        return result;
    }
    if (request.manifest == nullptr) {
        addDiagnostic(result, "Staged ephemeris update verification requires a manifest.");
        return result;
    }
    if (request.profileId.empty()) {
        addDiagnostic(result, "Staged ephemeris update verification requires a profile id.");
        return result;
    }
    if (request.stagedResourceRoot.empty()) {
        addDiagnostic(result, "Staged ephemeris update verification requires a staging root.");
        return result;
    }

    const EphemerisDataManifestProfile* profile = request.manifest->profile(request.profileId);
    if (profile == nullptr || profile->assetIds.empty()) {
        result.status = EphemerisStagedUpdateVerificationStatus::UnsupportedProfile;
        addDiagnostic(result, "Requested ephemeris update profile is not present in the manifest.");
        return result;
    }

    std::unordered_set<std::string> seenAssetIds;
    std::vector<EphemerisDataManifestAssetKind> presentKinds;
    for (const std::string& assetId : profile->assetIds) {
        if (cancellationRequested(request.cancellationRequested)) {
            markCanceled(result);
            return result;
        }
        const EphemerisDataManifestAsset* asset = request.manifest->asset(assetId);
        if (asset == nullptr) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            addDiagnostic(result, "Selected ephemeris update profile references a missing manifest asset.");
            return result;
        }
        if (!seenAssetIds.insert(asset->id).second) {
            result.status = EphemerisStagedUpdateVerificationStatus::MalformedMetadata;
            addDiagnostic(result, "Selected ephemeris update profile contains duplicate asset ids.");
            return result;
        }
        if (asset->profileId != request.profileId) {
            result.status = EphemerisStagedUpdateVerificationStatus::MalformedMetadata;
            addDiagnostic(result, "Selected ephemeris update asset belongs to a different profile.");
            return result;
        }
        if (!validateAssetMetadata(*asset, result)) {
            result.status = EphemerisStagedUpdateVerificationStatus::MalformedMetadata;
            return result;
        }

        presentKinds.push_back(asset->kind);
    }

    for (const EphemerisStagedUpdateVerificationRequest::ExpectedComponent& component : request.expectedComponents) {
        if (cancellationRequested(request.cancellationRequested)) {
            markCanceled(result);
            return result;
        }
        const EphemerisDataManifestAsset* asset = request.manifest->asset(component.assetId);
        if (asset == nullptr || !hasAssetId(profile->assetIds, component.assetId)) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            addDiagnostic(result, "Selected ephemeris update profile is missing an expected component.");
            return result;
        }
        if (asset->kind != component.kind) {
            result.status = EphemerisStagedUpdateVerificationStatus::WrongComponentKind;
            addDiagnostic(result, "Selected ephemeris update component kind does not match the expected kind.");
            return result;
        }
        if (component.expectedVersion.has_value() && component.expectedVersion->empty()) {
            result.status = EphemerisStagedUpdateVerificationStatus::InvalidRequest;
            addDiagnostic(result, "Expected staged ephemeris component versions must be non-empty.");
            return result;
        }
        if (component.expectedVersion.has_value() && asset->version != *component.expectedVersion) {
            result.status = EphemerisStagedUpdateVerificationStatus::MismatchedMetadata;
            addDiagnostic(result, "Selected ephemeris update component version does not match the expected version.");
            return result;
        }
        if (component.requiredValidityRange.has_value() && !isValidDateRange(*component.requiredValidityRange)) {
            result.status = EphemerisStagedUpdateVerificationStatus::InvalidRequest;
            addDiagnostic(result, "Required staged ephemeris component validity ranges must be ordered and finite.");
            return result;
        }
        if (component.requiredValidityRange.has_value()
            && !validityRangeCovers(asset->validityRange, *component.requiredValidityRange)) {
            result.status = EphemerisStagedUpdateVerificationStatus::MismatchedMetadata;
            addDiagnostic(
                result, "Selected ephemeris update component validity range does not cover the required range."
            );
            return result;
        }
    }

    for (const EphemerisDataManifestAssetKind kind : request.requiredKinds) {
        if (cancellationRequested(request.cancellationRequested)) {
            markCanceled(result);
            return result;
        }
        if (!hasKind(presentKinds, kind)) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            addDiagnostic(result, "Selected ephemeris update profile is missing a required component kind.");
            return result;
        }
    }

    for (const std::string& assetId : profile->assetIds) {
        if (cancellationRequested(request.cancellationRequested)) {
            markCanceled(result);
            return result;
        }
        const EphemerisDataManifestAsset* asset = request.manifest->asset(assetId);
        if (asset == nullptr) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            addDiagnostic(result, "Selected ephemeris update profile references a missing manifest asset.");
            return result;
        }
        if (!verifyAssetPayload(
                *asset, stagedSourcePath(request.stagedResourceRoot, *asset), result, request.cancellationRequested
            )) {
            return result;
        }

        result.verifiedAssetIds.push_back(asset->id);
    }

    result.status = EphemerisStagedUpdateVerificationStatus::Verified;
    return result;
}

}  // namespace skygate::ephemeris
