#include "EphemerisDataActivation.hpp"
#include "EphemerisDataFileUtilities.hpp"
#include "EphemerisDataManifest.hpp"
#include "EphemerisDataPayloadReader.hpp"
#include "EphemerisPayloadDiagnostics.hpp"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace skygate::ephemeris {

namespace {

using PayloadDiagnostics = EphemerisPayloadDiagnostics;

constexpr std::string_view kCanceledDiagnostic = "Ephemeris data activation was canceled.";

[[nodiscard]] std::string pathToString(const std::filesystem::path& path)
{
    return path.generic_string();
}

[[nodiscard]] bool startsWithQtResourcePrefix(const std::string_view path) noexcept
{
    return path.starts_with(":") || path.starts_with("qrc:");
}

[[nodiscard]] std::optional<std::filesystem::path> activeRelativePath(const EphemerisDataManifest::Asset& asset)
{
    if (asset.relativePath.empty()) {
        return std::nullopt;
    }

    std::filesystem::path profilePath(asset.profileId);
    if (EphemerisDataFileUtilities::hasUnsafePathComponent(profilePath)) {
        return std::nullopt;
    }

    std::filesystem::path relativePath(asset.relativePath);
    if (EphemerisDataFileUtilities::hasUnsafePathComponent(relativePath)) {
        return std::nullopt;
    }

    if (asset.compression.kind == EphemerisDataManifest::CompressionKind::Zstd && relativePath.extension() == ".zst") {
        relativePath.replace_extension();
    }

    if (relativePath.empty() || EphemerisDataFileUtilities::hasUnsafePathComponent(relativePath)) {
        return std::nullopt;
    }

    return profilePath / relativePath;
}

[[nodiscard]] bool isLargeQtResourceKernel(const EphemerisDataActivationRequest& request)
{
    const EphemerisDataManifest::Asset& asset = *request.asset;
    if (request.allowQtResourceKernelAssets || asset.kind != EphemerisDataManifest::AssetKind::SolarSystemKernel) {
        return false;
    }

    const std::uint64_t activeSize = asset.compression.uncompressedSizeBytes.value_or(0U);
    if (activeSize < request.largeKernelResourceThresholdBytes) {
        return false;
    }

    return startsWithQtResourcePrefix(pathToString(request.bundledResourceRoot))
           || startsWithQtResourcePrefix(asset.relativePath);
}

[[nodiscard]] bool
verifySha256File(const QString& path, const std::string& expectedHexDigest, EphemerisDataActivationResult& result)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        PayloadDiagnostics::addDiagnostic(
            result, "Unable to open active ephemeris data asset for checksum verification."
        );
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, EphemerisDataPayloadReader::kIoBufferBytes> buffer{};
    while (!file.atEnd()) {
        const qint64 bytesRead = file.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            PayloadDiagnostics::addDiagnostic(
                result, "Unable to read active ephemeris data asset for checksum verification."
            );
            return false;
        }
        hash.addData(QByteArrayView(buffer.data(), bytesRead));
    }

    const std::string actualHexDigest = hash.result().toHex().toStdString();
    return EphemerisDataFileUtilities::checksumsMatch(actualHexDigest, expectedHexDigest);
}

[[nodiscard]] bool existingTargetIsCurrent(
    const EphemerisDataManifest::Asset& asset, const QString& targetPath, EphemerisDataActivationResult& result
)
{
    if (asset.live) {
        return false;
    }
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.exists() || !targetInfo.isFile()) {
        return false;
    }
    if (asset.compression.uncompressedSizeBytes.has_value()
        && targetInfo.size() != static_cast<qint64>(*asset.compression.uncompressedSizeBytes)) {
        return false;
    }
    if (asset.checksum.algorithm != EphemerisDataFileUtilities::kSha256Algorithm) {
        return false;
    }

    return verifySha256File(targetPath, asset.checksum.value, result);
}

[[nodiscard]] bool pathIsWithinCanonicalRoot(const std::filesystem::path& root, const std::filesystem::path& candidate)
{
    std::error_code error;
    const std::filesystem::path canonicalRoot = std::filesystem::canonical(root, error);
    if (error) {
        return false;
    }
    const std::filesystem::path canonicalCandidate = std::filesystem::canonical(candidate, error);
    if (error) {
        return false;
    }

    auto rootIt = canonicalRoot.begin();
    auto candidateIt = canonicalCandidate.begin();
    for (; rootIt != canonicalRoot.end() && candidateIt != canonicalCandidate.end(); ++rootIt, ++candidateIt) {
        if (*rootIt != *candidateIt) {
            return false;
        }
    }

    return rootIt == canonicalRoot.end();
}

[[nodiscard]] bool prepareTargetDirectory(const QString& targetPath, EphemerisDataActivationResult& result)
{
    const QFileInfo targetInfo(targetPath);
    QDir targetDirectory(targetInfo.absolutePath());
    if (targetDirectory.exists()) {
        return true;
    }
    if (!targetDirectory.mkpath(QStringLiteral("."))) {
        PayloadDiagnostics::addDiagnostic(result, "Unable to create writable ephemeris data cache directory.");
        return false;
    }

    return true;
}

[[nodiscard]] QString sourcePathForRequest(const EphemerisDataActivationRequest& request)
{
    const std::filesystem::path sourceRelativePath(request.asset->relativePath);
    if (startsWithQtResourcePrefix(pathToString(request.bundledResourceRoot))) {
        QString root = EphemerisDataFileUtilities::pathToQString(request.bundledResourceRoot);
        if (!root.endsWith('/')) {
            root += '/';
        }
        return root + EphemerisDataFileUtilities::pathToQString(sourceRelativePath);
    }

    return EphemerisDataFileUtilities::pathToQString(request.bundledResourceRoot / sourceRelativePath);
}

}  // namespace

EphemerisDataActivationResult EphemerisDataActivation::activate(const EphemerisDataActivationRequest& request)
{
    EphemerisDataActivationResult result;
    if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
        PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
        return result;
    }
    if (request.asset == nullptr) {
        PayloadDiagnostics::addDiagnostic(result, "Ephemeris data activation requires an asset.");
        return result;
    }
    if (request.writableCacheRoot.empty()) {
        PayloadDiagnostics::addDiagnostic(result, "Ephemeris data activation requires a writable cache root.");
        return result;
    }
    if (!request.asset->live
        && (request.asset->checksum.algorithm != EphemerisDataFileUtilities::kSha256Algorithm
            || request.asset->checksum.value.empty())) {
        PayloadDiagnostics::addDiagnostic(
            result, "Ephemeris data activation requires a sha256 checksum for the active asset bytes."
        );
        return result;
    }
    if (request.asset->compression.kind == EphemerisDataManifest::CompressionKind::Zstd
        && !request.asset->compression.uncompressedSizeBytes.has_value()) {
        PayloadDiagnostics::addDiagnostic(result, "zstd ephemeris data assets require an expected uncompressed size.");
        return result;
    }
    if (isLargeQtResourceKernel(request)) {
        result.status = EphemerisDataActivationStatus::LargeKernelInQtResource;
        PayloadDiagnostics::addDiagnostic(
            result, "Large solar-system kernels must be bundled as external files, not Qt resources."
        );
        return result;
    }

    const std::optional<std::filesystem::path> relativeTargetPath = activeRelativePath(*request.asset);
    if (!relativeTargetPath.has_value()) {
        PayloadDiagnostics::addDiagnostic(result, "Ephemeris data activation requires a safe relative asset path.");
        return result;
    }

    const std::filesystem::path targetPath = request.writableCacheRoot / *relativeTargetPath;
    const QString targetPathString = EphemerisDataFileUtilities::pathToQString(targetPath);
    result.activePath = targetPath;
    if (existingTargetIsCurrent(*request.asset, targetPathString, result)) {
        result.status = EphemerisDataActivationStatus::AlreadyActive;
        return result;
    }

    const QString sourcePath = sourcePathForRequest(request);
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        result.status = EphemerisDataActivationStatus::MissingSource;
        PayloadDiagnostics::addDiagnostic(result, "Bundled ephemeris data asset source file is missing.");
        return result;
    }
    if (!request.asset->live && !EphemerisDataFileUtilities::sourceSizeMatchesMetadata(sourceInfo, *request.asset)) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        PayloadDiagnostics::addDiagnostic(
            result, "Bundled ephemeris data asset compressed size does not match manifest metadata."
        );
        return result;
    }
    if (!prepareTargetDirectory(targetPathString, result)) {
        result.status = EphemerisDataActivationStatus::IoError;
        return result;
    }
    if (!pathIsWithinCanonicalRoot(request.writableCacheRoot, targetPath.parent_path())) {
        result.status = EphemerisDataActivationStatus::IoError;
        PayloadDiagnostics::addDiagnostic(
            result, "Ephemeris data activation target path escapes the writable cache root."
        );
        return result;
    }

    QFile sourceFile(sourcePath);
    QSaveFile targetFile(targetPathString);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        result.status = EphemerisDataActivationStatus::MissingSource;
        PayloadDiagnostics::addDiagnostic(result, "Unable to open bundled ephemeris data asset source file.");
        return result;
    }
    if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.status = EphemerisDataActivationStatus::IoError;
        PayloadDiagnostics::addDiagnostic(result, "Unable to create temporary ephemeris data cache file.");
        return result;
    }

    const EphemerisDataPayloadReader::Result payload = EphemerisDataPayloadReader::read(
        request.asset->compression.kind,
        sourceFile,
        targetFile,
        request.cancellationRequested,
        request.asset->live ? std::nullopt : request.asset->compression.uncompressedSizeBytes
    );
    result.diagnostics.insert(result.diagnostics.end(), payload.diagnostics.begin(), payload.diagnostics.end());
    if (payload.status != EphemerisDataPayloadReader::Status::Read) {
        result.status = EphemerisDataFileUtilities::payloadStatus<EphemerisDataActivationStatus>(payload.status);
        targetFile.cancelWriting();
        return result;
    }
    if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
        PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
        targetFile.cancelWriting();
        return result;
    }
    if (!targetFile.flush()) {
        result.status = EphemerisDataActivationStatus::IoError;
        PayloadDiagnostics::addDiagnostic(result, "Unable to flush activated ephemeris data cache file.");
        targetFile.cancelWriting();
        return result;
    }
    if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
        PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
        targetFile.cancelWriting();
        return result;
    }

    const EphemerisDataFileUtilities::PayloadVerification payloadVerification =
        EphemerisDataFileUtilities::verifyDecodedPayload(*request.asset, payload.outputBytes, payload.checksum);
    if (payloadVerification != EphemerisDataFileUtilities::PayloadVerification::Verified) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        PayloadDiagnostics::addDiagnostic(
            result,
            EphemerisDataFileUtilities::payloadVerificationDiagnostic(
                payloadVerification, EphemerisDataFileUtilities::kActivatedPayloadWording
            )
        );
        targetFile.cancelWriting();
        return result;
    }

    if (EphemerisDataFileUtilities::cancellationRequested(request.cancellationRequested)) {
        PayloadDiagnostics::markCanceled(result, kCanceledDiagnostic);
        targetFile.cancelWriting();
        return result;
    }
    if (!targetFile.commit()) {
        result.status = EphemerisDataActivationStatus::IoError;
        PayloadDiagnostics::addDiagnostic(result, "Unable to atomically promote activated ephemeris data cache file.");
        return result;
    }

    result.status = EphemerisDataActivationStatus::Activated;
    return result;
}

}  // namespace skygate::ephemeris
