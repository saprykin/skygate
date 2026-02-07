#include "CatalogCoordinator.hpp"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr std::size_t kMaxDownloadedCatalogBytes = 128U << 20;

class InMemoryStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit InMemoryStarCatalog(std::vector<skygate::ephemeris::CelestialBody> bodies)
        : m_bodies(std::move(bodies))
    {
    }

    [[nodiscard]] std::vector<skygate::ephemeris::CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    std::vector<skygate::ephemeris::CelestialBody> m_bodies;
};

std::string toLowerId(const std::string& value)
{
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lower;
}

bool isSunOrMoonType(const skygate::ephemeris::CelestialBodyType type)
{
    return type == skygate::ephemeris::CelestialBodyType::Sun
        || type == skygate::ephemeris::CelestialBodyType::Moon;
}

bool containsBodyIdCaseInsensitive(
    const std::vector<skygate::ephemeris::CelestialBody>& bodies,
    const std::string& id
)
{
    const std::string loweredId = toLowerId(id);
    return std::any_of(bodies.begin(), bodies.end(), [&loweredId](const auto& body) {
        return toLowerId(body.id) == loweredId;
    });
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> parseCatalogPayload(const QByteArray& payload)
{
    const std::string_view rows(payload.constData(), static_cast<std::size_t>(payload.size()));
    std::unique_ptr<skygate::ephemeris::IStarCatalog> downloadedCatalog =
        skygate::ephemeris::createStarCatalogFromRows(rows);
    if (downloadedCatalog == nullptr) {
        downloadedCatalog = skygate::ephemeris::createStarCatalogFromHygCsv(rows);
    }
    if (downloadedCatalog == nullptr) {
        downloadedCatalog = skygate::ephemeris::createStarCatalogFromHygCsvGzip(rows);
    }
    return downloadedCatalog;
}

}  // namespace

CatalogCoordinator::CatalogCoordinator(QNetworkAccessManager* networkAccessManager)
    : m_networkAccessManager(networkAccessManager)
{
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogCoordinator::ensureCoreSolarSystemBodies(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog
)
{
    if (catalog == nullptr) {
        return nullptr;
    }

    std::vector<skygate::ephemeris::CelestialBody> mergedBodies = catalog->bodies();
    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCatalog =
        skygate::ephemeris::createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return catalog;
    }

    bool injectedCoreBody = false;
    for (const auto& body : bundledCatalog->bodies()) {
        if (
            !isSunOrMoonType(body.type)
            && body.type != skygate::ephemeris::CelestialBodyType::Planet
        ) {
            continue;
        }

        if (containsBodyIdCaseInsensitive(mergedBodies, body.id)) {
            continue;
        }

        mergedBodies.push_back(body);
        injectedCoreBody = true;
    }

    if (!injectedCoreBody) {
        return catalog;
    }

    return std::make_unique<InMemoryStarCatalog>(std::move(mergedBodies));
}

void CatalogCoordinator::downloadCatalogFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    CompletionHandler completionHandler
) const
{
    QStringList candidateUrls;
    candidateUrls.reserve(urlTexts.size());
    for (const QString& urlText : urlTexts) {
        const QString trimmed = urlText.trimmed();
        if (!trimmed.isEmpty()) {
            candidateUrls.push_back(trimmed);
        }
    }

    if (candidateUrls.isEmpty()) {
        DownloadResult result;
        result.errorText = "Catalog: Invalid URL";
        completionHandler(std::move(result));
        return;
    }

    if (m_networkAccessManager == nullptr) {
        DownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    const auto lastErrorText = std::make_shared<QString>("Catalog: Download failed");
    const auto tryDownloadNextUrl = std::make_shared<std::function<void(int)>>();
    *tryDownloadNextUrl = [this, candidateUrls, callbackContext, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl](const int index) {
        if (index >= candidateUrls.size()) {
            DownloadResult result;
            result.errorText = *lastErrorText;
            completionHandler(std::move(result));
            return;
        }

        const QUrl url = QUrl::fromUserInput(candidateUrls[index]);
        if (!url.isValid() || url.scheme().isEmpty()) {
            *lastErrorText = QString("Catalog: Invalid source URL %1").arg(candidateUrls[index]);
            (*tryDownloadNextUrl)(index + 1);
            return;
        }

        if (statusHandler) {
            statusHandler(QString("Catalog: Downloading %1 (%2/%3)").arg(
                url.toString(),
                QString::number(index + 1),
                QString::number(candidateUrls.size())
            ));
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(300000);
        request.setRawHeader("User-Agent", "Skygate/1.0");
        request.setRawHeader("Accept", "text/plain,text/csv,application/gzip,application/octet-stream,*/*");

        QNetworkReply* reply = m_networkAccessManager->get(request);
        QObject* const contextObject = callbackContext != nullptr ? callbackContext : reply;
        QObject::connect(
            reply,
            &QNetworkReply::finished,
            contextObject,
            [reply, index, candidateUrls, statusHandler, completionHandler, lastErrorText, tryDownloadNextUrl]() {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    const int httpStatusCode =
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    *lastErrorText = QString("Catalog: Source %1 failed (%2, HTTP %3)").arg(
                        candidateUrls[index],
                        reply->errorString(),
                        QString::number(httpStatusCode)
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                const QByteArray payload = reply->readAll();
                if (payload.isEmpty()) {
                    *lastErrorText = QString("Catalog: Source %1 returned empty data").arg(candidateUrls[index]);
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                if (static_cast<std::size_t>(payload.size()) > kMaxDownloadedCatalogBytes) {
                    *lastErrorText = QString("Catalog: Source %1 file too large (max 128 MiB)").arg(
                        candidateUrls[index]
                    );
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                std::unique_ptr<skygate::ephemeris::IStarCatalog> downloadedCatalog =
                    parseCatalogPayload(payload);
                if (downloadedCatalog == nullptr) {
                    *lastErrorText = QString(
                        "Catalog: Source %1 parse failed (supported: pipe rows, HYG CSV, or HYG .csv.gz)"
                    ).arg(candidateUrls[index]);
                    if (statusHandler) {
                        statusHandler(*lastErrorText);
                    }
                    (*tryDownloadNextUrl)(index + 1);
                    return;
                }

                DownloadResult result;
                result.catalog = std::move(downloadedCatalog);
                completionHandler(std::move(result));
            }
        );
    };

    (*tryDownloadNextUrl)(0);
}
