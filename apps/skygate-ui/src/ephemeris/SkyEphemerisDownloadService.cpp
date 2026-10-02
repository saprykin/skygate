#include "SkyEphemerisDownloadService.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

Q_LOGGING_CATEGORY(skygateEphemerisDataLog, "skygate.ephemeris.data")

using skygate::ephemeris::EphemerisDataManifest;
using StagedUpdateDownloadRequest = SkyEphemerisDownloadService::StagedUpdateDownloadRequest;
using StagedUpdateDownloadResult = SkyEphemerisDownloadService::StagedUpdateDownloadResult;
using StagedUpdateDownloadStatus = SkyEphemerisDownloadService::StagedUpdateDownloadStatus;

constexpr std::size_t kDownloadBufferBytes = 64U * 1024U;

std::filesystem::path pathFromQString(const QString& path)
{
    return std::filesystem::path(path.toStdString());
}

QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

QString stringToQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

void addDiagnostic(StagedUpdateDownloadResult& result, QString diagnostic)
{
    if (!diagnostic.trimmed().isEmpty()) {
        qCWarning(skygateEphemerisDataLog).noquote() << diagnostic;
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

bool cancellationRequested(const std::function<bool()>& callback)
{
    return callback != nullptr && callback();
}

void markCanceled(StagedUpdateDownloadResult& result, QString diagnostic)
{
    result.status = StagedUpdateDownloadStatus::Canceled;
    addDiagnostic(result, std::move(diagnostic));
}

void reportDownloadProgress(
    const StagedUpdateDownloadRequest& request,
    const std::uint64_t stagedBytes,
    const std::optional<std::uint64_t> totalBytes
)
{
    if (request.progressHandler != nullptr) {
        request.progressHandler(stagedBytes, totalBytes);
    }
}

[[nodiscard]] QString sourceUrlForRequest(const StagedUpdateDownloadRequest& request)
{
    if (!request.sourceUrl.trimmed().isEmpty()) {
        return request.sourceUrl.trimmed();
    }
    if (request.asset == nullptr || request.asset->sourceUrl.empty()) {
        return {};
    }
    return stringToQString(request.asset->sourceUrl);
}

[[nodiscard]] bool prepareStagedFileDirectory(const QString& stagedPath, StagedUpdateDownloadResult& result)
{
    const QFileInfo stagedFileInfo(stagedPath);
    if (QDir().mkpath(stagedFileInfo.absolutePath())) {
        return true;
    }

    result.status = StagedUpdateDownloadStatus::IoError;
    addDiagnostic(result, QStringLiteral("Unable to create ephemeris update staging directory."));
    return false;
}

void cleanupCanceledDownload(const StagedUpdateDownloadRequest& request, const StagedUpdateDownloadResult& result)
{
    if (request.retainPartialStagingOnCancellation || result.stagedPath.trimmed().isEmpty()) {
        return;
    }
    if (!QFile::remove(result.stagedPath) && QFileInfo::exists(result.stagedPath)) {
        // The caller already has a cancellation result; preserve that primary
        // status and avoid adding a misleading hard failure.
    }
}

[[nodiscard]] bool copyFileToStaging(
    QFile& sourceFile,
    QFile& stagedFile,
    const std::function<bool()>& isCanceled,
    const StagedUpdateDownloadRequest& request,
    StagedUpdateDownloadResult& result
)
{
    std::array<char, kDownloadBufferBytes> buffer{};
    const std::optional<std::uint64_t> totalBytes =
        sourceFile.size() >= 0 ? std::optional<std::uint64_t>{static_cast<std::uint64_t>(sourceFile.size())}
                               : std::nullopt;
    reportDownloadProgress(request, result.stagedBytes, totalBytes);
    while (!sourceFile.atEnd()) {
        if (isCanceled()) {
            stagedFile.close();
            markCanceled(result, QStringLiteral("Ephemeris update download was canceled during transfer."));
            cleanupCanceledDownload(request, result);
            return false;
        }

        const qint64 bytesRead = sourceFile.read(buffer.data(), static_cast<qint64>(buffer.size()));
        if (bytesRead < 0) {
            result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(result, QStringLiteral("Unable to read ephemeris update source file."));
            return false;
        }
        if (bytesRead == 0) {
            continue;
        }
        if (stagedFile.write(buffer.data(), bytesRead) != bytesRead) {
            result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(result, QStringLiteral("Unable to write ephemeris update staging file."));
            return false;
        }
        result.stagedBytes += static_cast<std::uint64_t>(bytesRead);
        reportDownloadProgress(request, result.stagedBytes, totalBytes);
    }
    return true;
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

struct NetworkDownloadState final {
    StagedUpdateDownloadRequest request;
    std::function<void(StagedUpdateDownloadResult)> completionHandler;
    StagedUpdateDownloadResult result;
    std::shared_ptr<QFile> stagedFile;
    QNetworkReply* reply = nullptr;
    QTimer* cancellationTimer = nullptr;
    bool completed = false;
};

}  // namespace

SkyEphemerisDownloadService::SkyEphemerisDownloadService(QNetworkAccessManager* networkAccessManager, QObject* parent)
    : QObject(parent), m_networkAccessManager(networkAccessManager)
{
    if (m_networkAccessManager == nullptr) {
        m_networkAccessManager = new QNetworkAccessManager(this);
    }
}

SkyEphemerisDownloadService::~SkyEphemerisDownloadService() = default;

SkyEphemerisDownloadService::StagedUpdateDownloadResult
SkyEphemerisDownloadService::stage(const StagedUpdateDownloadRequest& request)
{
    StagedUpdateDownloadResult result;
    stageInternal(
        request, [&result](StagedUpdateDownloadResult stagedResult) { result = std::move(stagedResult); }, false
    );
    return result;
}

void SkyEphemerisDownloadService::stageAsync(
    const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
)
{
    stageInternal(request, std::move(completionHandler), true);
}

void SkyEphemerisDownloadService::stageInternal(
    const StagedUpdateDownloadRequest& request,
    const std::function<void(StagedUpdateDownloadResult)>& completionHandler,
    const bool allowNetwork
)
{
    const auto isCanceled = [&request] { return cancellationRequested(request.cancellationRequested); };

    if (isCanceled()) {
        StagedUpdateDownloadResult result;
        markCanceled(result, QStringLiteral("Ephemeris update download was canceled before transfer."));
        completionHandler(std::move(result));
        return;
    }
    if (request.asset == nullptr) {
        StagedUpdateDownloadResult result;
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires an asset."));
        completionHandler(std::move(result));
        return;
    }
    if (request.stagedResourceRoot.trimmed().isEmpty()) {
        StagedUpdateDownloadResult result;
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires a staging root."));
        completionHandler(std::move(result));
        return;
    }

    const std::filesystem::path relativePath(request.asset->relativePath);
    if (hasUnsafePathComponent(relativePath)) {
        StagedUpdateDownloadResult result;
        addDiagnostic(result, QStringLiteral("Ephemeris update download requires a safe relative asset path."));
        completionHandler(std::move(result));
        return;
    }

    if (!request.sourceResourceRoot.trimmed().isEmpty()) {
        const std::filesystem::path sourcePath = pathFromQString(request.sourceResourceRoot) / relativePath;
        completionHandler(stageLocalFile(request, pathToQString(sourcePath)));
        return;
    }

    const QString sourceUrlText = sourceUrlForRequest(request);
    if (sourceUrlText.isEmpty()) {
        StagedUpdateDownloadResult result;
        result.status = StagedUpdateDownloadStatus::MissingSource;
        addDiagnostic(result, QStringLiteral("Ephemeris update download source URL is missing."));
        completionHandler(std::move(result));
        return;
    }

    const QUrl sourceUrl(sourceUrlText);
    if (sourceUrl.isLocalFile()) {
        completionHandler(stageLocalFile(request, sourceUrl.toLocalFile()));
        return;
    }
    if (sourceUrl.scheme().isEmpty()) {
        completionHandler(stageLocalFile(request, sourceUrlText));
        return;
    }
    if (sourceUrl.scheme() != QStringLiteral("http") && sourceUrl.scheme() != QStringLiteral("https")) {
        StagedUpdateDownloadResult result;
        result.status = StagedUpdateDownloadStatus::MissingSource;
        addDiagnostic(result, QStringLiteral("Ephemeris update download source URL scheme is unsupported."));
        completionHandler(std::move(result));
        return;
    }
    if (!allowNetwork) {
        StagedUpdateDownloadResult result;
        addDiagnostic(result, QStringLiteral("Ephemeris update network downloads must be requested asynchronously."));
        completionHandler(std::move(result));
        return;
    }

    stageNetworkUrl(request, completionHandler);
}

SkyEphemerisDownloadService::StagedUpdateDownloadResult
SkyEphemerisDownloadService::stageLocalFile(const StagedUpdateDownloadRequest& request, const QString& sourcePath)
{
    StagedUpdateDownloadResult result;
    const auto isCanceled = [&request] { return cancellationRequested(request.cancellationRequested); };

    const std::filesystem::path relativePath(request.asset->relativePath);
    result.stagedPath = pathToQString(pathFromQString(request.stagedResourceRoot) / relativePath);

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        result.status = StagedUpdateDownloadStatus::MissingSource;
        addDiagnostic(result, QStringLiteral("Ephemeris update download source file is missing."));
        return result;
    }

    if (!prepareStagedFileDirectory(result.stagedPath, result)) {
        return result;
    }

    QFile stagedFile(result.stagedPath);
    if (!stagedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.status = StagedUpdateDownloadStatus::IoError;
        addDiagnostic(result, QStringLiteral("Unable to create ephemeris update staging file."));
        return result;
    }

    if (!copyFileToStaging(sourceFile, stagedFile, isCanceled, request, result)) {
        return result;
    }
    if (isCanceled()) {
        stagedFile.close();
        markCanceled(result, QStringLiteral("Ephemeris update download was canceled after transfer."));
        cleanupCanceledDownload(request, result);
        return result;
    }
    if (!stagedFile.flush()) {
        result.status = StagedUpdateDownloadStatus::IoError;
        addDiagnostic(result, QStringLiteral("Unable to flush ephemeris update staging file."));
        return result;
    }

    result.status = StagedUpdateDownloadStatus::Downloaded;
    return result;
}

void SkyEphemerisDownloadService::stageNetworkUrl(
    const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
)
{
    auto state = std::make_shared<NetworkDownloadState>();
    state->request = request;
    state->completionHandler = std::move(completionHandler);

    const std::filesystem::path relativePath(request.asset->relativePath);
    state->result.stagedPath = pathToQString(pathFromQString(request.stagedResourceRoot) / relativePath);

    if (!prepareStagedFileDirectory(state->result.stagedPath, state->result)) {
        state->completionHandler(std::move(state->result));
        return;
    }

    state->stagedFile = std::make_shared<QFile>(state->result.stagedPath);
    if (!state->stagedFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        state->result.status = StagedUpdateDownloadStatus::IoError;
        addDiagnostic(state->result, QStringLiteral("Unable to create ephemeris update staging file."));
        state->completionHandler(std::move(state->result));
        return;
    }

    const QString sourceUrlText = sourceUrlForRequest(request);
    QNetworkRequest networkRequest{QUrl(sourceUrlText)};
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    state->reply = m_networkAccessManager->get(networkRequest);

    QObject::connect(state->reply, &QNetworkReply::readyRead, this, [state] {
        if (state->completed) {
            return;
        }
        if (cancellationRequested(state->request.cancellationRequested)) {
            state->reply->abort();
            return;
        }

        const QByteArray payload = state->reply->readAll();
        if (payload.isEmpty()) {
            return;
        }
        if (state->stagedFile->write(payload) != payload.size()) {
            state->result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(state->result, QStringLiteral("Unable to write ephemeris update staging file."));
            state->reply->abort();
            return;
        }
        state->result.stagedBytes += static_cast<std::uint64_t>(payload.size());
        reportDownloadProgress(state->request, state->result.stagedBytes, std::nullopt);
    });

    QObject::connect(
        state->reply,
        &QNetworkReply::downloadProgress,
        this,
        [state](const qint64 bytesReceived, const qint64 bytesTotal) {
            if (state->completed) {
                return;
            }
            const std::optional<std::uint64_t> totalBytes =
                bytesTotal > 0 ? std::optional<std::uint64_t>{static_cast<std::uint64_t>(bytesTotal)} : std::nullopt;
            reportDownloadProgress(
                state->request,
                bytesReceived > 0 ? static_cast<std::uint64_t>(bytesReceived) : state->result.stagedBytes,
                totalBytes
            );
        }
    );

    state->cancellationTimer = new QTimer(this);
    QObject::connect(state->cancellationTimer, &QTimer::timeout, this, [state] {
        if (!state->completed && cancellationRequested(state->request.cancellationRequested)) {
            state->reply->abort();
        }
    });
    state->cancellationTimer->start(50);

    QObject::connect(state->reply, &QNetworkReply::finished, this, [this, state] {
        if (state->completed) {
            return;
        }
        state->completed = true;
        state->cancellationTimer->stop();
        state->cancellationTimer->deleteLater();

        const QByteArray remainingPayload = state->reply->readAll();
        if (!remainingPayload.isEmpty() && state->result.status != StagedUpdateDownloadStatus::IoError) {
            if (state->stagedFile->write(remainingPayload) != remainingPayload.size()) {
                state->result.status = StagedUpdateDownloadStatus::IoError;
                addDiagnostic(state->result, QStringLiteral("Unable to write ephemeris update staging file."));
            } else {
                state->result.stagedBytes += static_cast<std::uint64_t>(remainingPayload.size());
                reportDownloadProgress(state->request, state->result.stagedBytes, state->result.stagedBytes);
            }
        }

        const QNetworkReply::NetworkError networkError = state->reply->error();
        const QString networkErrorText = state->reply->errorString();
        state->reply->deleteLater();

        if (cancellationRequested(state->request.cancellationRequested)) {
            state->stagedFile->close();
            markCanceled(state->result, QStringLiteral("Ephemeris update download was canceled during transfer."));
            cleanupCanceledDownload(state->request, state->result);
            state->completionHandler(std::move(state->result));
            return;
        }
        if (state->result.status == StagedUpdateDownloadStatus::IoError) {
            state->completionHandler(std::move(state->result));
            return;
        }
        if (networkError != QNetworkReply::NoError) {
            state->result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(state->result, QStringLiteral("Ephemeris update download failed: %1").arg(networkErrorText));
            state->completionHandler(std::move(state->result));
            return;
        }
        if (!state->stagedFile->flush()) {
            state->result.status = StagedUpdateDownloadStatus::IoError;
            addDiagnostic(state->result, QStringLiteral("Unable to flush ephemeris update staging file."));
            state->completionHandler(std::move(state->result));
            return;
        }

        state->result.status = StagedUpdateDownloadStatus::Downloaded;
        state->completionHandler(std::move(state->result));
    });
}
