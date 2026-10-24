#pragma once

#include <QString>

#include <cstddef>
#include <string_view>

namespace skygate::ui::internal {

// Catalog status, progress, and failure texts and the summaries composed from
// them. The wording and the status prefix live here instead of at the call
// sites, so source rows and summaries present the same text.
class SkyCatalogText final {
public:
    // Prefix carried by the catalog status texts. Places that present a status
    // in their own context drop it with withoutStatusPrefix.
    static constexpr std::string_view kStatusPrefix = "Catalog: ";

    [[nodiscard]] static QString withoutStatusPrefix(QString statusText);

    [[nodiscard]] static QString datasetInfo(std::size_t bodyCount, std::size_t constellationCount);
    [[nodiscard]] static QString deepSkyCatalogInfo(std::size_t foundObjectCount);
    [[nodiscard]] static QString collectionSummary(
        const QString& participationSummary,
        std::size_t bodyCount,
        std::size_t deepSkyObjectCount,
        std::size_t constellationCount
    );
    [[nodiscard]] static QString collectionRejected(const QString& detail);
    [[nodiscard]] static QString savedCollectionUnreadable(const QString& diagnostic);

    [[nodiscard]] static QString loadFailed();
    [[nodiscard]] static QString networkUnavailable();
    [[nodiscard]] static QString downloadCanceled();
    [[nodiscard]] static QString downloadFailed();
    [[nodiscard]] static QString invalidUrl();
    [[nodiscard]] static QString invalidSourceUrl(const QString& urlText);
    [[nodiscard]] static QString sourceDownloadStarted(const QString& urlText);
    [[nodiscard]] static QString
    sourceDownloadFailed(const QString& urlText, const QString& errorText, int httpStatusCode);
    // maxBytes is the download cap the message states in MiB.
    [[nodiscard]] static QString sourceTooLarge(const QString& urlText, std::size_t maxBytes);
    [[nodiscard]] static QString sourceEmptyPayload(const QString& urlText);
    [[nodiscard]] static QString sourceParseFailed(const QString& sourceUrl, const QString& description);
    [[nodiscard]] static QString processingDownloadedCatalog();
    [[nodiscard]] static QString processingParsedObjects(std::size_t parsedObjectCount);
    [[nodiscard]] static QString callbackContextUnavailable();
    [[nodiscard]] static QString emptyDeepSkyCatalog();
    [[nodiscard]] static QString missingConstellationData(const QString& reason);

    [[nodiscard]] static QString unknownSourcePreset(const QString& presetId);
    [[nodiscard]] static QString cacheClearBlocked();
    [[nodiscard]] static QString starCacheClearResult(bool cacheCleared);
    [[nodiscard]] static QString deepSkyCacheClearResult(bool cacheCleared);
    [[nodiscard]] static QString sourceCacheClearResult(bool cacheCleared);
    [[nodiscard]] static QString sourcePayloadUnavailable();
    [[nodiscard]] static QString
    unavailableSourceSummary(const QString& baseStatusText, std::size_t unavailableSourceCount);
    [[nodiscard]] static QString
    brightnessFilterSummary(const QString& baseStatusText, std::size_t selectedBodyCount, std::size_t parsedBodyCount);
    [[nodiscard]] static QString constellationLineSummary(const QString& catalogSummaryText, QString statusText);
    [[nodiscard]] static QString
    deepSkyFoundSummary(const QString& baseStatusText, const QString& sourceLabel, std::size_t foundObjectCount);
};

}  // namespace skygate::ui::internal
