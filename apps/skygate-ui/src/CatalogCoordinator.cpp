#include "CatalogCoordinator.hpp"

#include "catalog/CatalogDownloadService.hpp"
#include "catalog/CatalogPayloadParseService.hpp"

#include <QLocale>
#include <QNetworkAccessManager>
#include <QObject>

#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

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

bool isReferenceLineStarId(const std::string& id)
{
    constexpr std::array<std::string_view, 8> kReferenceLineStarIds = {{
        "sirius",
        "canopus",
        "arcturus",
        "vega",
        "capella",
        "rigel",
        "procyon",
        "betelgeuse",
    }};

    const std::string loweredId = toLowerId(id);
    return std::any_of(kReferenceLineStarIds.begin(), kReferenceLineStarIds.end(), [&loweredId](const auto value) {
        return loweredId == value;
    });
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

}  // namespace

CatalogCoordinator::CatalogCoordinator(QNetworkAccessManager* networkAccessManager)
    : m_downloadService(std::make_unique<CatalogDownloadService>(networkAccessManager))
    , m_parseService(std::make_unique<CatalogPayloadParseService>())
    , m_networkAccessManager(networkAccessManager)
{
}

CatalogCoordinator::~CatalogCoordinator() = default;

std::unique_ptr<skygate::ephemeris::IStarCatalog> CatalogCoordinator::ensureCoreSolarSystemBodies(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog
)
{
    if (catalog == nullptr) {
        return nullptr;
    }

    std::vector<skygate::ephemeris::CelestialBody> mergedBodies = catalog->bodies();
    const std::size_t starCount = static_cast<std::size_t>(std::count_if(
        mergedBodies.begin(),
        mergedBodies.end(),
        [](const auto& body) {
            return body.type == skygate::ephemeris::CelestialBodyType::Star;
        }
    ));

    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCatalog =
        skygate::ephemeris::createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return catalog;
    }

    bool injectedCoreBody = false;
    for (const auto& body : bundledCatalog->bodies()) {
        const bool isReferenceLineStar = body.type == skygate::ephemeris::CelestialBodyType::Star
            && isReferenceLineStarId(body.id)
            && starCount == 0U;
        if (
            !isSunOrMoonType(body.type)
            && body.type != skygate::ephemeris::CelestialBodyType::Planet
            && !isReferenceLineStar
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
    if (m_downloadService == nullptr || m_parseService == nullptr) {
        DownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_downloadService->downloadFirstSuccessfulFromUrls(
        urlTexts,
        callbackContext,
        statusHandler,
        [this, callbackContext, statusHandler, completionHandler](CatalogDownloadService::DownloadResult downloadResult) mutable {
            if (downloadResult.payload.isEmpty()) {
                DownloadResult result;
                result.errorText = downloadResult.errorText.isEmpty()
                    ? QString("Catalog: Download failed")
                    : downloadResult.errorText;
                completionHandler(std::move(result));
                return;
            }

            if (statusHandler) {
                statusHandler("Catalog: Processing downloaded catalog...");
            }

            QObject* const contextObject = callbackContext != nullptr
                ? callbackContext
                : static_cast<QObject*>(m_networkAccessManager);
            if (contextObject == nullptr) {
                DownloadResult result;
                result.errorText = "Catalog: Callback context unavailable";
                completionHandler(std::move(result));
                return;
            }

            const QString sourceUrl = downloadResult.sourceUrl;
            m_parseService->parseAsync(
                downloadResult.payload,
                contextObject,
                [statusHandler](const std::size_t parsedObjectCount) {
                    if (!statusHandler) {
                        return;
                    }

                    const QString countText =
                        QLocale::system().toString(static_cast<qulonglong>(parsedObjectCount));
                    statusHandler(QString("Catalog: Processing... %1 objects parsed").arg(countText));
                },
                [sourceUrl, statusHandler, completionHandler = std::move(completionHandler)](
                    std::unique_ptr<skygate::ephemeris::IStarCatalog> downloadedCatalog
                ) mutable {
                    if (downloadedCatalog == nullptr) {
                        DownloadResult result;
                        result.errorText = QString(
                            "Catalog: Source %1 parse failed (supported: pipe rows, HYG CSV, HYG .csv.gz, or HYG .zip)"
                        ).arg(sourceUrl);
                        if (statusHandler) {
                            statusHandler(result.errorText);
                        }
                        completionHandler(std::move(result));
                        return;
                    }

                    DownloadResult result;
                    result.catalog = std::move(downloadedCatalog);
                    completionHandler(std::move(result));
                }
            );
        }
    );
}

void CatalogCoordinator::downloadRawDataFromUrls(
    const QStringList& urlTexts,
    QObject* callbackContext,
    StatusHandler statusHandler,
    RawCompletionHandler completionHandler
) const
{
    if (m_downloadService == nullptr) {
        RawDownloadResult result;
        result.errorText = "Catalog: Network unavailable";
        completionHandler(std::move(result));
        return;
    }

    m_downloadService->downloadFirstSuccessfulFromUrls(
        urlTexts,
        callbackContext,
        statusHandler,
        [completionHandler](CatalogDownloadService::DownloadResult downloadResult) mutable {
            RawDownloadResult result;
            result.payload = std::move(downloadResult.payload);
            result.sourceUrl = std::move(downloadResult.sourceUrl);
            result.errorText = std::move(downloadResult.errorText);
            completionHandler(std::move(result));
        }
    );
}
