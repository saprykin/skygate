#include "EphemerisDataActivation.hpp"
#include "EphemerisDataManifest.hpp"
#include "EphemerisDataPayloadReader.hpp"

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
#include <string_view>
#include <system_error>

namespace skygate::ephemeris {

namespace {

constexpr std::size_t kIoBufferBytes = 1U << 16U;

[[nodiscard]] QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

[[nodiscard]] std::string pathToString(const std::filesystem::path& path)
{
    return path.generic_string();
}

[[nodiscard]] bool startsWithQtResourcePrefix(const std::string_view path) noexcept
{
    return path.starts_with(":") || path.starts_with("qrc:");
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

[[nodiscard]] std::optional<std::filesystem::path> activeRelativePath(const EphemerisDataManifest::Asset& asset)
{
    if (asset.relativePath.empty()) {
        return std::nullopt;
    }

    std::filesystem::path profilePath(asset.profileId);
    if (hasUnsafePathComponent(profilePath)) {
        return std::nullopt;
    }

    std::filesystem::path relativePath(asset.relativePath);
    if (hasUnsafePathComponent(relativePath)) {
        return std::nullopt;
    }

    if (asset.compression.kind == EphemerisDataManifest::CompressionKind::Zstd && relativePath.extension() == ".zst") {
        relativePath.replace_extension();
    }

    if (relativePath.empty() || hasUnsafePathComponent(relativePath)) {
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

void addDiagnostic(EphemerisDataActivationResult& result, const std::string_view diagnostic)
{
    result.diagnostics.emplace_back(diagnostic);
}

[[nodiscard]] bool cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

void markCanceled(EphemerisDataActivationResult& result)
{
    result.status = EphemerisDataActivationStatus::Canceled;
    addDiagnostic(result, "Ephemeris data activation was canceled.");
}

[[nodiscard]] bool
verifySha256File(const QString& path, const std::string& expectedHexDigest, EphemerisDataActivationResult& result)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        addDiagnostic(result, "Unable to open active ephemeris data asset for checksum verification.");
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, kIoBufferBytes> buffer{};
    while (!file.atEnd()) {
        const qint64 bytesRead = file.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            addDiagnostic(result, "Unable to read active ephemeris data asset for checksum verification.");
            return false;
        }
        hash.addData(QByteArrayView(buffer.data(), bytesRead));
    }

    const QByteArray actualHexDigest = hash.result().toHex().toLower();
    const QByteArray expectedHexDigestLower = QByteArray::fromStdString(expectedHexDigest).toLower();
    return actualHexDigest == expectedHexDigestLower;
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
    if (asset.checksum.algorithm != "sha256") {
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
        addDiagnostic(result, "Unable to create writable ephemeris data cache directory.");
        return false;
    }

    return true;
}

[[nodiscard]] bool sourceSizeMatchesMetadata(const QFileInfo& sourceInfo, const EphemerisDataManifest::Asset& asset)
{
    return !asset.compression.compressedSizeBytes.has_value()
           || sourceInfo.size() == static_cast<qint64>(*asset.compression.compressedSizeBytes);
}

[[nodiscard]] QString sourcePathForRequest(const EphemerisDataActivationRequest& request)
{
    const std::filesystem::path sourceRelativePath(request.asset->relativePath);
    if (startsWithQtResourcePrefix(pathToString(request.bundledResourceRoot))) {
        QString root = pathToQString(request.bundledResourceRoot);
        if (!root.endsWith('/')) {
            root += '/';
        }
        return root + pathToQString(sourceRelativePath);
    }

    return pathToQString(request.bundledResourceRoot / sourceRelativePath);
}

[[nodiscard]] EphemerisDataActivationStatus
mappedActivationStatus(const EphemerisDataPayloadReader::Status status) noexcept
{
    switch (status) {
    case EphemerisDataPayloadReader::Status::UnsupportedCompression:
        return EphemerisDataActivationStatus::UnsupportedCompression;
    case EphemerisDataPayloadReader::Status::CorruptArchive:
        return EphemerisDataActivationStatus::CorruptArchive;
    case EphemerisDataPayloadReader::Status::Canceled:
        return EphemerisDataActivationStatus::Canceled;
    case EphemerisDataPayloadReader::Status::IoError:
        return EphemerisDataActivationStatus::IoError;
    case EphemerisDataPayloadReader::Status::OutputLimitExceeded:
        return EphemerisDataActivationStatus::ChecksumMismatch;
    case EphemerisDataPayloadReader::Status::Read:
        break;
    }

    return EphemerisDataActivationStatus::IoError;
}

}  // namespace

EphemerisDataActivationResult EphemerisDataActivation::activate(const EphemerisDataActivationRequest& request)
{
    EphemerisDataActivationResult result;
    if (cancellationRequested(request.cancellationRequested)) {
        markCanceled(result);
        return result;
    }
    if (request.asset == nullptr) {
        addDiagnostic(result, "Ephemeris data activation requires an asset.");
        return result;
    }
    if (request.writableCacheRoot.empty()) {
        addDiagnostic(result, "Ephemeris data activation requires a writable cache root.");
        return result;
    }
    if (!request.asset->live
        && (request.asset->checksum.algorithm != "sha256" || request.asset->checksum.value.empty())) {
        addDiagnostic(result, "Ephemeris data activation requires a sha256 checksum for the active asset bytes.");
        return result;
    }
    if (request.asset->compression.kind == EphemerisDataManifest::CompressionKind::Zstd
        && !request.asset->compression.uncompressedSizeBytes.has_value()) {
        addDiagnostic(result, "zstd ephemeris data assets require an expected uncompressed size.");
        return result;
    }
    if (isLargeQtResourceKernel(request)) {
        result.status = EphemerisDataActivationStatus::LargeKernelInQtResource;
        addDiagnostic(result, "Large solar-system kernels must be bundled as external files, not Qt resources.");
        return result;
    }

    const std::optional<std::filesystem::path> relativeTargetPath = activeRelativePath(*request.asset);
    if (!relativeTargetPath.has_value()) {
        addDiagnostic(result, "Ephemeris data activation requires a safe relative asset path.");
        return result;
    }

    const std::filesystem::path targetPath = request.writableCacheRoot / *relativeTargetPath;
    const QString targetPathString = pathToQString(targetPath);
    result.activePath = targetPath;
    if (existingTargetIsCurrent(*request.asset, targetPathString, result)) {
        result.status = EphemerisDataActivationStatus::AlreadyActive;
        return result;
    }

    const QString sourcePath = sourcePathForRequest(request);
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        result.status = EphemerisDataActivationStatus::MissingSource;
        addDiagnostic(result, "Bundled ephemeris data asset source file is missing.");
        return result;
    }
    if (!request.asset->live && !sourceSizeMatchesMetadata(sourceInfo, *request.asset)) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        addDiagnostic(result, "Bundled ephemeris data asset compressed size does not match manifest metadata.");
        return result;
    }
    if (!prepareTargetDirectory(targetPathString, result)) {
        result.status = EphemerisDataActivationStatus::IoError;
        return result;
    }
    if (!pathIsWithinCanonicalRoot(request.writableCacheRoot, targetPath.parent_path())) {
        result.status = EphemerisDataActivationStatus::IoError;
        addDiagnostic(result, "Ephemeris data activation target path escapes the writable cache root.");
        return result;
    }

    QFile sourceFile(sourcePath);
    QSaveFile targetFile(targetPathString);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        result.status = EphemerisDataActivationStatus::MissingSource;
        addDiagnostic(result, "Unable to open bundled ephemeris data asset source file.");
        return result;
    }
    if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.status = EphemerisDataActivationStatus::IoError;
        addDiagnostic(result, "Unable to create temporary ephemeris data cache file.");
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
        result.status = mappedActivationStatus(payload.status);
        targetFile.cancelWriting();
        return result;
    }
    if (cancellationRequested(request.cancellationRequested)) {
        markCanceled(result);
        targetFile.cancelWriting();
        return result;
    }
    if (!targetFile.flush()) {
        result.status = EphemerisDataActivationStatus::IoError;
        addDiagnostic(result, "Unable to flush activated ephemeris data cache file.");
        targetFile.cancelWriting();
        return result;
    }
    if (cancellationRequested(request.cancellationRequested)) {
        markCanceled(result);
        targetFile.cancelWriting();
        return result;
    }

    if (request.asset->live && payload.outputBytes == 0U) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        addDiagnostic(result, "Activated live ephemeris data asset payload is empty.");
        targetFile.cancelWriting();
        return result;
    }
    if (!request.asset->live && request.asset->compression.uncompressedSizeBytes.has_value()
        && payload.outputBytes != *request.asset->compression.uncompressedSizeBytes) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        addDiagnostic(result, "Activated ephemeris data asset size does not match manifest metadata.");
        targetFile.cancelWriting();
        return result;
    }
    if (!request.asset->live
        && QByteArray::fromStdString(payload.checksum).toLower()
               != QByteArray::fromStdString(request.asset->checksum.value).toLower()) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        addDiagnostic(result, "Activated ephemeris data asset checksum does not match manifest metadata.");
        targetFile.cancelWriting();
        return result;
    }

    if (cancellationRequested(request.cancellationRequested)) {
        markCanceled(result);
        targetFile.cancelWriting();
        return result;
    }
    if (!targetFile.commit()) {
        result.status = EphemerisDataActivationStatus::IoError;
        addDiagnostic(result, "Unable to atomically promote activated ephemeris data cache file.");
        return result;
    }

    result.status = EphemerisDataActivationStatus::Activated;
    return result;
}

}  // namespace skygate::ephemeris
