#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;
class QObject;

class CatalogDownloadService final {
public:
    struct DownloadResult {
        QByteArray payload;
        QString sourceUrl;
        QString errorText;
    };

    using StatusHandler = std::function<void(const QString&)>;
    using CompletionHandler = std::function<void(DownloadResult)>;

public:
    explicit CatalogDownloadService(QNetworkAccessManager* networkAccessManager);

    void downloadFirstSuccessfulFromUrls(
        const QStringList& urlTexts,
        QObject* callbackContext,
        StatusHandler statusHandler,
        CompletionHandler completionHandler
    ) const;

private:
    QNetworkAccessManager* m_networkAccessManager = nullptr;
};
