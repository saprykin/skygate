#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class CatalogDownloadCancellation;
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

    // Starts the first candidate that succeeds and returns the handle that
    // cancels the operation. An explicit cancellation stops the candidate
    // chain without completing; an ordinary transfer failure still moves to
    // the next candidate.
    std::shared_ptr<CatalogDownloadCancellation> downloadFirstSuccessfulFromUrls(
        const QStringList& urlTexts,
        QObject* callbackContext,
        StatusHandler statusHandler,
        CompletionHandler completionHandler
    ) const;

private:
    QNetworkAccessManager* m_networkAccessManager = nullptr;
};
