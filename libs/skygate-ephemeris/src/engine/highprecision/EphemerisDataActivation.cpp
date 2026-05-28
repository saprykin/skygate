#include "EphemerisDataActivation.hpp"

#include <QCryptographicHash>
#include <QByteArrayView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLibrary>
#include <QSaveFile>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kIoBufferBytes = 1U << 16U;

struct ZstdInBuffer {
    const void* src = nullptr;
    std::size_t size = 0U;
    std::size_t pos = 0U;
};

struct ZstdOutBuffer {
    void* dst = nullptr;
    std::size_t size = 0U;
    std::size_t pos = 0U;
};

struct ZstdDStream;

class ZstdRuntime final {
public:
    using CreateDStream = ZstdDStream* (*)();
    using FreeDStream = std::size_t (*)(ZstdDStream*);
    using InitDStream = std::size_t (*)(ZstdDStream*);
    using DecompressStream = std::size_t (*)(ZstdDStream*, ZstdOutBuffer*, ZstdInBuffer*);
    using IsError = unsigned int (*)(std::size_t);

    [[nodiscard]] static const ZstdRuntime& instance()
    {
        static const ZstdRuntime runtime;
        return runtime;
    }

    [[nodiscard]] bool isAvailable() const noexcept
    {
        return m_available;
    }

    [[nodiscard]] ZstdDStream* createDStream() const
    {
        return m_createDStream == nullptr ? nullptr : m_createDStream();
    }

    void freeDStream(ZstdDStream* stream) const
    {
        if (stream != nullptr && m_freeDStream != nullptr) {
            (void)m_freeDStream(stream);
        }
    }

    [[nodiscard]] bool initDStream(ZstdDStream* stream) const
    {
        return m_initDStream != nullptr && !isError(m_initDStream(stream));
    }

    [[nodiscard]] std::size_t decompressStream(ZstdDStream* stream, ZstdOutBuffer& output, ZstdInBuffer& input) const
    {
        return m_decompressStream(stream, &output, &input);
    }

    [[nodiscard]] bool isError(const std::size_t code) const
    {
        return m_isError == nullptr || m_isError(code) != 0U;
    }

private:
    ZstdRuntime()
    {
        for (const QString& libraryName :
             {QStringLiteral("zstd"), QStringLiteral("libzstd"), QStringLiteral("libzstd.so.1")}) {
            m_library.setFileName(libraryName);
            if (!m_library.load()) {
                continue;
            }

            m_createDStream = reinterpret_cast<CreateDStream>(m_library.resolve("ZSTD_createDStream"));
            m_freeDStream = reinterpret_cast<FreeDStream>(m_library.resolve("ZSTD_freeDStream"));
            m_initDStream = reinterpret_cast<InitDStream>(m_library.resolve("ZSTD_initDStream"));
            m_decompressStream = reinterpret_cast<DecompressStream>(m_library.resolve("ZSTD_decompressStream"));
            m_isError = reinterpret_cast<IsError>(m_library.resolve("ZSTD_isError"));
            m_available = m_createDStream != nullptr && m_freeDStream != nullptr && m_initDStream != nullptr
                          && m_decompressStream != nullptr && m_isError != nullptr;
            if (m_available) {
                return;
            }

            m_library.unload();
        }
    }

    mutable QLibrary m_library;
    CreateDStream m_createDStream = nullptr;
    FreeDStream m_freeDStream = nullptr;
    InitDStream m_initDStream = nullptr;
    DecompressStream m_decompressStream = nullptr;
    IsError m_isError = nullptr;
    bool m_available = false;
};

class ScopedZstdDStream final {
public:
    explicit ScopedZstdDStream(const ZstdRuntime& runtime) : m_runtime(runtime), m_stream(runtime.createDStream()) {}

    ~ScopedZstdDStream()
    {
        m_runtime.freeDStream(m_stream);
    }

    ScopedZstdDStream(const ScopedZstdDStream&) = delete;
    ScopedZstdDStream& operator=(const ScopedZstdDStream&) = delete;

    [[nodiscard]] ZstdDStream* get() const noexcept
    {
        return m_stream;
    }

private:
    const ZstdRuntime& m_runtime;
    ZstdDStream* m_stream = nullptr;
};

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

[[nodiscard]] std::optional<std::filesystem::path> activeRelativePath(const EphemerisDataManifestAsset& asset)
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

    if (asset.compression.kind == EphemerisDataManifestCompressionKind::Zstd && relativePath.extension() == ".zst") {
        relativePath.replace_extension();
    }

    if (relativePath.empty() || hasUnsafePathComponent(relativePath)) {
        return std::nullopt;
    }

    return profilePath / relativePath;
}

[[nodiscard]] bool isLargeQtResourceKernel(const EphemerisDataActivationRequest& request)
{
    const EphemerisDataManifestAsset& asset = *request.asset;
    if (request.allowQtResourceKernelAssets || asset.kind != EphemerisDataManifestAssetKind::SolarSystemKernel) {
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

void addDiagnostic(EphemerisStagedUpdateVerificationResult& result, const std::string_view diagnostic)
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

void markCanceled(EphemerisStagedUpdateVerificationResult& result)
{
    result.status = EphemerisStagedUpdateVerificationStatus::Canceled;
    addDiagnostic(result, "Staged ephemeris update verification was canceled.");
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

    return hash.result().toHex().toStdString() == expectedHexDigest;
}

[[nodiscard]] bool existingTargetIsCurrent(
    const EphemerisDataManifestAsset& asset, const QString& targetPath, EphemerisDataActivationResult& result
)
{
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

[[nodiscard]] bool copyUncompressedAsset(
    QFile& sourceFile,
    QIODevice& targetFile,
    QCryptographicHash& hash,
    std::uint64_t& outputBytes,
    EphemerisDataActivationResult& result,
    const std::function<bool()>& cancellationCallback
)
{
    std::array<char, kIoBufferBytes> buffer{};
    while (!sourceFile.atEnd()) {
        if (cancellationRequested(cancellationCallback)) {
            markCanceled(result);
            return false;
        }
        const qint64 bytesRead = sourceFile.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            addDiagnostic(result, "Unable to read bundled ephemeris data asset.");
            return false;
        }
        if (bytesRead == 0) {
            continue;
        }
        if (targetFile.write(buffer.data(), bytesRead) != bytesRead) {
            addDiagnostic(result, "Unable to write ephemeris data asset into the writable cache.");
            return false;
        }
        hash.addData(QByteArrayView(buffer.data(), bytesRead));
        outputBytes += static_cast<std::uint64_t>(bytesRead);
    }

    return true;
}

[[nodiscard]] bool decompressZstdAsset(
    QFile& sourceFile,
    QIODevice& targetFile,
    QCryptographicHash& hash,
    std::uint64_t& outputBytes,
    EphemerisDataActivationResult& result,
    const std::function<bool()>& cancellationCallback
)
{
    const ZstdRuntime& runtime = ZstdRuntime::instance();
    if (!runtime.isAvailable()) {
        result.status = EphemerisDataActivationStatus::UnsupportedCompression;
        addDiagnostic(result, "zstd runtime library is not available.");
        return false;
    }

    ScopedZstdDStream stream(runtime);
    if (stream.get() == nullptr || !runtime.initDStream(stream.get())) {
        result.status = EphemerisDataActivationStatus::UnsupportedCompression;
        addDiagnostic(result, "Unable to initialize zstd decompression.");
        return false;
    }

    std::array<char, kIoBufferBytes> inputBuffer{};
    std::array<char, kIoBufferBytes> outputBuffer{};
    std::size_t remainingHint = 1U;
    bool sawFrameEnd = false;
    while (!sourceFile.atEnd()) {
        if (cancellationRequested(cancellationCallback)) {
            markCanceled(result);
            return false;
        }
        const qint64 bytesRead = sourceFile.read(inputBuffer.data(), static_cast<qint64>(inputBuffer.size()));
        if (bytesRead < 0) {
            addDiagnostic(result, "Unable to read compressed ephemeris data asset.");
            return false;
        }

        ZstdInBuffer input{inputBuffer.data(), static_cast<std::size_t>(bytesRead), 0U};
        while (input.pos < input.size) {
            if (cancellationRequested(cancellationCallback)) {
                markCanceled(result);
                return false;
            }
            ZstdOutBuffer output{outputBuffer.data(), outputBuffer.size(), 0U};
            remainingHint = runtime.decompressStream(stream.get(), output, input);
            if (runtime.isError(remainingHint)) {
                result.status = EphemerisDataActivationStatus::CorruptArchive;
                addDiagnostic(result, "zstd ephemeris data asset is corrupt or unsupported.");
                return false;
            }
            if (output.pos > 0U) {
                if (targetFile.write(outputBuffer.data(), static_cast<qint64>(output.pos))
                    != static_cast<qint64>(output.pos)) {
                    addDiagnostic(result, "Unable to write decompressed ephemeris data into the writable cache.");
                    return false;
                }
                hash.addData(QByteArrayView(outputBuffer.data(), static_cast<qsizetype>(output.pos)));
                outputBytes += static_cast<std::uint64_t>(output.pos);
            }
            sawFrameEnd = remainingHint == 0U;
        }
    }

    if (!sawFrameEnd || remainingHint != 0U) {
        result.status = EphemerisDataActivationStatus::CorruptArchive;
        addDiagnostic(result, "zstd ephemeris data asset ended before a complete frame was decoded.");
        return false;
    }

    return true;
}

[[nodiscard]] bool sourceSizeMatchesMetadata(const QFileInfo& sourceInfo, const EphemerisDataManifestAsset& asset)
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
mappedVerificationStatus(const EphemerisDataActivationStatus status) noexcept
{
    switch (status) {
    case EphemerisDataActivationStatus::UnsupportedCompression:
        return EphemerisStagedUpdateVerificationStatus::UnsupportedCompression;
    case EphemerisDataActivationStatus::CorruptArchive:
        return EphemerisStagedUpdateVerificationStatus::CorruptArchive;
    case EphemerisDataActivationStatus::ChecksumMismatch:
        return EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
    case EphemerisDataActivationStatus::Canceled:
        return EphemerisStagedUpdateVerificationStatus::Canceled;
    case EphemerisDataActivationStatus::IoError:
    case EphemerisDataActivationStatus::MissingSource:
        return EphemerisStagedUpdateVerificationStatus::IoError;
    case EphemerisDataActivationStatus::Activated:
    case EphemerisDataActivationStatus::AlreadyActive:
    case EphemerisDataActivationStatus::InvalidRequest:
    case EphemerisDataActivationStatus::LargeKernelInQtResource:
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

    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::uint64_t outputBytes = 0U;
    bool payloadValid = false;
    EphemerisDataActivationResult activationResult;
    switch (asset.compression.kind) {
    case EphemerisDataManifestCompressionKind::None:
        payloadValid =
            copyUncompressedAsset(sourceFile, sink, hash, outputBytes, activationResult, cancellationCallback);
        break;
    case EphemerisDataManifestCompressionKind::Zstd:
        payloadValid = decompressZstdAsset(sourceFile, sink, hash, outputBytes, activationResult, cancellationCallback);
        break;
    }

    if (!payloadValid) {
        result.status = activationResult.status == EphemerisDataActivationStatus::Canceled
                            ? EphemerisStagedUpdateVerificationStatus::Canceled
                            : mappedVerificationStatus(activationResult.status);
        result.diagnostics.insert(
            result.diagnostics.end(), activationResult.diagnostics.begin(), activationResult.diagnostics.end()
        );
        return false;
    }
    if (asset.compression.uncompressedSizeBytes.has_value()
        && outputBytes != *asset.compression.uncompressedSizeBytes) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        addDiagnostic(result, "Staged ephemeris asset uncompressed size does not match manifest metadata.");
        return false;
    }
    if (hash.result().toHex().toStdString() != asset.checksum.value) {
        result.status = EphemerisStagedUpdateVerificationStatus::ChecksumMismatch;
        addDiagnostic(result, "Staged ephemeris asset checksum does not match manifest metadata.");
        return false;
    }

    return true;
}

}  // namespace

EphemerisDataActivationResult activateEphemerisDataAsset(const EphemerisDataActivationRequest& request)
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
    if (request.asset->checksum.algorithm != "sha256" || request.asset->checksum.value.empty()) {
        addDiagnostic(result, "Ephemeris data activation requires a sha256 checksum for the active asset bytes.");
        return result;
    }
    if (request.asset->compression.kind == EphemerisDataManifestCompressionKind::Zstd
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
    if (!sourceSizeMatchesMetadata(sourceInfo, *request.asset)) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        addDiagnostic(result, "Bundled ephemeris data asset compressed size does not match manifest metadata.");
        return result;
    }
    if (!prepareTargetDirectory(targetPathString, result)) {
        result.status = EphemerisDataActivationStatus::IoError;
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

    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::uint64_t outputBytes = 0U;
    bool activated = false;
    switch (request.asset->compression.kind) {
    case EphemerisDataManifestCompressionKind::None:
        activated =
            copyUncompressedAsset(sourceFile, targetFile, hash, outputBytes, result, request.cancellationRequested);
        break;
    case EphemerisDataManifestCompressionKind::Zstd:
        activated =
            decompressZstdAsset(sourceFile, targetFile, hash, outputBytes, result, request.cancellationRequested);
        break;
    }

    if (!activated) {
        if (result.status == EphemerisDataActivationStatus::InvalidRequest) {
            result.status = EphemerisDataActivationStatus::IoError;
        }
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

    if (request.asset->compression.uncompressedSizeBytes.has_value()
        && outputBytes != *request.asset->compression.uncompressedSizeBytes) {
        result.status = EphemerisDataActivationStatus::ChecksumMismatch;
        addDiagnostic(result, "Activated ephemeris data asset size does not match manifest metadata.");
        targetFile.cancelWriting();
        return result;
    }
    if (hash.result().toHex().toStdString() != request.asset->checksum.value) {
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

EphemerisStagedUpdateVerificationResult
verifyEphemerisStagedUpdateSet(const EphemerisStagedUpdateVerificationRequest& request)
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
