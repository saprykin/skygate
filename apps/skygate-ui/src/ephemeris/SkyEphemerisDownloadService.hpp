#pragma once

#include "engine/highprecision/EphemerisDataManifest.hpp"

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QNetworkAccessManager;

class SkyEphemerisDownloadService final : public QObject {
    Q_OBJECT

public:
    enum class StagedUpdateDownloadStatus : std::uint8_t {
        Downloaded,
        InvalidRequest,
        MissingSource,
        IoError,
        Canceled
    };

    struct StagedUpdateDownloadRequest final {
        const skygate::ephemeris::EphemerisDataManifest::Asset* asset = nullptr;
        QString sourceUrl;
        QString sourceResourceRoot;
        QString stagedResourceRoot;
        std::function<bool()> cancellationRequested;
        std::function<void(std::uint64_t stagedBytes, std::optional<std::uint64_t> totalBytes)> progressHandler;
        bool retainPartialStagingOnCancellation = true;
    };

    struct StagedUpdateDownloadResult final {
        StagedUpdateDownloadStatus status = StagedUpdateDownloadStatus::InvalidRequest;
        QString stagedPath;
        std::uint64_t stagedBytes = 0U;
        std::vector<QString> diagnostics;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == StagedUpdateDownloadStatus::Downloaded;
        }
    };

    explicit SkyEphemerisDownloadService(
        QNetworkAccessManager* networkAccessManager = nullptr, QObject* parent = nullptr
    );
    ~SkyEphemerisDownloadService() override;

    [[nodiscard]] StagedUpdateDownloadResult stage(const StagedUpdateDownloadRequest& request);
    void stageAsync(
        const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
    );

private:
    void stageInternal(
        const StagedUpdateDownloadRequest& request,
        const std::function<void(StagedUpdateDownloadResult)>& completionHandler,
        bool allowNetwork
    );
    [[nodiscard]] StagedUpdateDownloadResult
    stageLocalFile(const StagedUpdateDownloadRequest& request, const QString& sourcePath);
    void stageNetworkUrl(
        const StagedUpdateDownloadRequest& request, std::function<void(StagedUpdateDownloadResult)> completionHandler
    );

    QNetworkAccessManager* m_networkAccessManager = nullptr;
};
