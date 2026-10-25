#include "EphemerisStagedUpdateVerification.hpp"
#include "EphemerisAssetMetadataPolicy.hpp"
#include "EphemerisDataFileUtilities.hpp"
#include "EphemerisDataPayloadReader.hpp"
#include "EphemerisPayloadDiagnostics.hpp"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>

#include <algorithm>
#include <optional>
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

using PayloadDiagnostics = EphemerisPayloadDiagnostics;

constexpr std::string_view kCanceledDiagnostic = "Staged ephemeris update verification was canceled.";

[[nodiscard]] QString
stagedSourcePath(const std::filesystem::path& stagedResourceRoot, const EphemerisDataManifest::Asset& asset)
{
    return EphemerisDataFileUtilities::pathToQString(stagedResourceRoot / std::filesystem::path(asset.relativePath));
}

[[nodiscard]] bool
validityRangeCovers(const EphemerisDateRange& availableRange, const EphemerisDateRange& requiredRange) noexcept
{
    return availableRange.interval().contains(requiredRange.interval());
}

[[nodiscard]] bool hasKind(
    const std::vector<EphemerisDataManifest::AssetKind>& kinds, const EphemerisDataManifest::AssetKind kind
) noexcept
{
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

[[nodiscard]] bool hasAssetId(const std::vector<std::string>& assetIds, const std::string_view assetId) noexcept
{
    return std::find(assetIds.begin(), assetIds.end(), assetId) != assetIds.end();
}

[[nodiscard]] bool
validateAssetMetadata(const EphemerisDataManifest::Asset& asset, EphemerisStagedUpdateVerificationResult& result)
{
    bool valid = true;
    if (asset.id.empty() || asset.profileId.empty() || asset.version.empty() || asset.relativePath.empty()) {
        PayloadDiagnostics::addDiagnostic(
            result, "Staged ephemeris asset metadata requires id, profile, version, and relative path."
        );
        valid = false;
    }
    if (EphemerisAssetMetadataPolicy::requiresChecksum(asset)
        && (asset.checksum.algorithm != EphemerisDataFileUtilities::kSha256Algorithm || asset.checksum.value.empty())) {
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris asset metadata requires a sha256 checksum.");
        valid = false;
    }
    const std::filesystem::path relativePath(asset.relativePath);
    if (EphemerisDataFileUtilities::hasUnsafePathComponent(relativePath)) {
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris asset metadata contains an unsafe relative path.");
        valid = false;
    }
    if (!EphemerisAssetMetadataPolicy::hasUsableCompressionSizes(asset.compression)) {
        PayloadDiagnostics::addDiagnostic(
            result, "zstd staged ephemeris assets require positive compressed and uncompressed sizes."
        );
        valid = false;
    }
    if (!asset.validityRange.isValid()) {
        PayloadDiagnostics::addDiagnostic(
            result, "Staged ephemeris asset metadata requires an ordered finite validity range."
        );
        valid = false;
    }
    if (asset.validityRange.id.empty() || asset.validityRange.displayName.empty()) {
        PayloadDiagnostics::addDiagnostic(
            result, "Staged ephemeris asset metadata requires validity range id and display name."
        );
        valid = false;
    }

    return valid;
}

[[nodiscard]] bool verifyAssetPayload(
    const EphemerisDataManifest::Asset& asset,
    const QString& sourcePath,
    EphemerisStagedUpdateVerificationResult& result,
    const std::function<bool()>& cancellationCallback
)
{
    if (EphemerisDataFileUtilities::cancellationRequested(cancellationCallback)) {
        PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
        return false;
    }

    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        result.status = EphemerisStagedUpdateVerificationStatus::MissingAsset;
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris asset file is missing.");
        return false;
    }
    if (!asset.live && !EphemerisDataFileUtilities::sourceSizeMatchesMetadata(sourceInfo, asset)) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris asset size does not match manifest metadata.");
        return false;
    }

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        result.status = EphemerisStagedUpdateVerificationStatus::IoError;
        PayloadDiagnostics::addDiagnostic(result, "Unable to open staged ephemeris asset for verification.");
        return false;
    }

    HashingWriteDevice sink;
    if (!sink.openForWrite()) {
        result.status = EphemerisStagedUpdateVerificationStatus::IoError;
        PayloadDiagnostics::addDiagnostic(result, "Unable to initialize staged ephemeris verification sink.");
        return false;
    }

    const EphemerisDataPayloadReader::Result payload = EphemerisDataPayloadReader::read(
        asset.compression.kind,
        sourceFile,
        sink,
        cancellationCallback,
        asset.live ? std::nullopt : asset.compression.uncompressedSizeBytes
    );
    if (payload.status != EphemerisDataPayloadReader::Status::Read) {
        result.status =
            EphemerisDataFileUtilities::payloadStatus<EphemerisStagedUpdateVerificationStatus>(payload.status);
        result.diagnostics.insert(result.diagnostics.end(), payload.diagnostics.begin(), payload.diagnostics.end());
        return false;
    }
    const EphemerisDataFileUtilities::PayloadVerification payloadVerification =
        EphemerisDataFileUtilities::verifyDecodedPayload(asset, payload.outputBytes, payload.checksum);
    if (payloadVerification != EphemerisDataFileUtilities::PayloadVerification::Verified) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        PayloadDiagnostics::addDiagnostic(
            result,
            EphemerisDataFileUtilities::payloadVerificationDiagnostic(
                payloadVerification, EphemerisDataFileUtilities::kStagedPayloadWording
            )
        );
        return false;
    }

    return true;
}

}  // namespace

EphemerisStagedUpdateVerificationResult
EphemerisStagedUpdateVerification::verify(const EphemerisStagedUpdateVerificationRequest& request)
{
    EphemerisStagedUpdateVerificationResult result;
    if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
        PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
        return result;
    }
    if (request.manifest == nullptr) {
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris update verification requires a manifest.");
        return result;
    }
    if (request.profileId.empty()) {
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris update verification requires a profile id.");
        return result;
    }
    if (request.stagedResourceRoot.empty()) {
        PayloadDiagnostics::addDiagnostic(result, "Staged ephemeris update verification requires a staging root.");
        return result;
    }

    const EphemerisDataManifest::Profile* profile = request.manifest->profile(request.profileId);
    if (profile == nullptr || profile->assetIds.empty()) {
        result.status = EphemerisStagedUpdateVerificationStatus::UnsupportedProfile;
        PayloadDiagnostics::addDiagnostic(result, "Requested ephemeris update profile is not present in the manifest.");
        return result;
    }

    std::unordered_set<std::string> seenAssetIds;
    std::vector<EphemerisDataManifest::AssetKind> presentKinds;
    for (const std::string& assetId : profile->assetIds) {
        if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
            PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
            return result;
        }
        const EphemerisDataManifest::Asset* asset = request.manifest->asset(assetId);
        if (asset == nullptr) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update profile references a missing manifest asset."
            );
            return result;
        }
        if (!seenAssetIds.insert(asset->id).second) {
            result.status = EphemerisStagedUpdateVerificationStatus::MalformedMetadata;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update profile contains duplicate asset ids."
            );
            return result;
        }
        if (asset->profileId != request.profileId) {
            result.status = EphemerisStagedUpdateVerificationStatus::MalformedMetadata;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update asset belongs to a different profile."
            );
            return result;
        }
        if (!validateAssetMetadata(*asset, result)) {
            result.status = EphemerisStagedUpdateVerificationStatus::MalformedMetadata;
            return result;
        }

        presentKinds.push_back(asset->kind);
    }

    for (const EphemerisStagedUpdateVerificationRequest::ExpectedComponent& component : request.expectedComponents) {
        if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
            PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
            return result;
        }
        const EphemerisDataManifest::Asset* asset = request.manifest->asset(component.assetId);
        if (asset == nullptr || !hasAssetId(profile->assetIds, component.assetId)) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update profile is missing an expected component."
            );
            return result;
        }
        if (asset->kind != component.kind) {
            result.status = EphemerisStagedUpdateVerificationStatus::WrongComponentKind;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update component kind does not match the expected kind."
            );
            return result;
        }
        if (component.expectedVersion.has_value() && component.expectedVersion->empty()) {
            result.status = EphemerisStagedUpdateVerificationStatus::InvalidRequest;
            PayloadDiagnostics::addDiagnostic(
                result, "Expected staged ephemeris component versions must be non-empty."
            );
            return result;
        }
        if (component.expectedVersion.has_value() && asset->version != *component.expectedVersion) {
            result.status = EphemerisStagedUpdateVerificationStatus::MismatchedMetadata;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update component version does not match the expected version."
            );
            return result;
        }
        if (component.requiredValidityRange.has_value() && !component.requiredValidityRange->isValid()) {
            result.status = EphemerisStagedUpdateVerificationStatus::InvalidRequest;
            PayloadDiagnostics::addDiagnostic(
                result, "Required staged ephemeris component validity ranges must be ordered and finite."
            );
            return result;
        }
        if (component.requiredValidityRange.has_value()
            && !validityRangeCovers(asset->validityRange, *component.requiredValidityRange)) {
            result.status = EphemerisStagedUpdateVerificationStatus::MismatchedMetadata;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update component validity range does not cover the required range."
            );
            return result;
        }
    }

    for (const EphemerisDataManifest::AssetKind kind : request.requiredKinds) {
        if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
            PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
            return result;
        }
        if (!hasKind(presentKinds, kind)) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update profile is missing a required component kind."
            );
            return result;
        }
    }

    for (const std::string& assetId : profile->assetIds) {
        if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
            PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
            return result;
        }
        const EphemerisDataManifest::Asset* asset = request.manifest->asset(assetId);
        if (asset == nullptr) {
            result.status = EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet;
            PayloadDiagnostics::addDiagnostic(
                result, "Selected ephemeris update profile references a missing manifest asset."
            );
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
