#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QObject;

class CatalogCoordinator final {
public:
    struct DownloadResult {
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
        QString errorText;
    };

    struct RawDownloadResult {
        QByteArray payload;
        QString sourceUrl;
        QString errorText;
    };

    using StatusHandler = std::function<void(const QString&)>;
    using CompletionHandler = std::function<void(DownloadResult)>;
    using RawCompletionHandler = std::function<void(RawDownloadResult)>;

public:
    explicit CatalogCoordinator(QNetworkAccessManager* networkAccessManager);

    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog> ensureCoreSolarSystemBodies(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog
    );

    void downloadCatalogFromUrls(
        const QStringList& urlTexts,
        QObject* callbackContext,
        StatusHandler statusHandler,
        CompletionHandler completionHandler
    ) const;

    void downloadRawDataFromUrls(
        const QStringList& urlTexts,
        QObject* callbackContext,
        StatusHandler statusHandler,
        RawCompletionHandler completionHandler
    ) const;

private:
    QNetworkAccessManager* m_networkAccessManager = nullptr;
};
