#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

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

    using StatusHandler = std::function<void(const QString&)>;
    using CompletionHandler = std::function<void(DownloadResult)>;

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

private:
    QNetworkAccessManager* m_networkAccessManager = nullptr;
};
