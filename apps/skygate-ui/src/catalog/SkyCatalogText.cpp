#include "SkyCatalogText.hpp"

#include <QLatin1StringView>
#include <QLocale>

#include <cstddef>
#include <string_view>
#include <utility>

namespace skygate::ui::internal {
namespace {

constexpr std::size_t kBytesPerMebibyte = 1024U * 1024U;

// Wraps a status body in the class prefix, so the texts produced here and the
// presentations that strip the prefix read the same constant.
[[nodiscard]] QString withStatusPrefix(const QString& body)
{
    constexpr std::string_view prefix = SkyCatalogText::kStatusPrefix;
    return QString::fromLatin1(prefix.data(), static_cast<qsizetype>(prefix.size())) + body;
}

}  // namespace

QString SkyCatalogText::withoutStatusPrefix(QString statusText)
{
    const QLatin1StringView prefix(kStatusPrefix.data(), static_cast<qsizetype>(kStatusPrefix.size()));
    if (statusText.startsWith(prefix)) {
        statusText.remove(0, prefix.size());
    }
    return statusText;
}

QString SkyCatalogText::datasetInfo(const std::size_t bodyCount, const std::size_t constellationCount)
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1 | Constellations: %2")
        .arg(
            locale.toString(static_cast<qulonglong>(bodyCount)),
            locale.toString(static_cast<qulonglong>(constellationCount))
        );
}

QString SkyCatalogText::deepSkyCatalogInfo(const std::size_t foundObjectCount)
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1").arg(locale.toString(static_cast<qulonglong>(foundObjectCount)));
}

QString SkyCatalogText::collectionSummary(
    const QString& participationSummary,
    const std::size_t bodyCount,
    const std::size_t deepSkyObjectCount,
    const std::size_t constellationCount
)
{
    const QLocale locale = QLocale::system();
    return withStatusPrefix(QStringLiteral("%1 (%2 objects, %3 deep sky, %4 constellations)")
                                .arg(
                                    participationSummary,
                                    locale.toString(static_cast<qulonglong>(bodyCount)),
                                    locale.toString(static_cast<qulonglong>(deepSkyObjectCount)),
                                    locale.toString(static_cast<qulonglong>(constellationCount))
                                ));
}

QString SkyCatalogText::collectionRejected(const QString& detail)
{
    return withStatusPrefix(QStringLiteral("Collection rejected: %1").arg(detail));
}

QString SkyCatalogText::savedCollectionUnreadable(const QString& diagnostic)
{
    const QString bodyTemplate = QString(
        "Saved catalog configuration unreadable; the active collection stays in use. "
        "Add, update, or reorder a catalog source to rewrite the saved configuration: %1"
    );
    return withStatusPrefix(bodyTemplate.arg(diagnostic));
}

QString SkyCatalogText::loadFailed()
{
    return withStatusPrefix(QStringLiteral("Failed to load"));
}

QString SkyCatalogText::networkUnavailable()
{
    return withStatusPrefix(QStringLiteral("Network unavailable"));
}

QString SkyCatalogText::downloadCanceled()
{
    return withStatusPrefix(QStringLiteral("Download canceled."));
}

QString SkyCatalogText::downloadFailed()
{
    return withStatusPrefix(QStringLiteral("Download failed"));
}

QString SkyCatalogText::invalidUrl()
{
    return withStatusPrefix(QStringLiteral("Invalid URL"));
}

QString SkyCatalogText::invalidSourceUrl(const QString& urlText)
{
    return withStatusPrefix(QStringLiteral("Invalid source URL %1").arg(urlText));
}

QString SkyCatalogText::sourceDownloadStarted(const QString& urlText)
{
    return withStatusPrefix(QStringLiteral("Downloading %1...").arg(urlText));
}

QString SkyCatalogText::sourceDownloadFailed(const QString& urlText, const QString& errorText, const int httpStatusCode)
{
    return withStatusPrefix(
        QStringLiteral("Source %1 failed (%2, HTTP %3)").arg(urlText, errorText, QString::number(httpStatusCode))
    );
}

QString SkyCatalogText::sourceTooLarge(const QString& urlText, const std::size_t maxBytes)
{
    const qulonglong maxMebibytes = static_cast<qulonglong>(maxBytes / kBytesPerMebibyte);
    return withStatusPrefix(
        QStringLiteral("Source %1 file too large (max %2 MiB)").arg(urlText, QString::number(maxMebibytes))
    );
}

QString SkyCatalogText::sourceEmptyPayload(const QString& urlText)
{
    return withStatusPrefix(QStringLiteral("Source %1 returned empty data").arg(urlText));
}

QString SkyCatalogText::sourceParseFailed(const QString& sourceUrl, const QString& description)
{
    return withStatusPrefix(QStringLiteral("Source %1 parse failed: %2").arg(sourceUrl, description));
}

QString SkyCatalogText::processingDownloadedCatalog()
{
    return withStatusPrefix(QStringLiteral("Processing downloaded catalog..."));
}

QString SkyCatalogText::processingParsedObjects(const std::size_t parsedObjectCount)
{
    const QLocale locale = QLocale::system();
    return withStatusPrefix(
        QString("Processing... %1 objects parsed").arg(locale.toString(static_cast<qulonglong>(parsedObjectCount)))
    );
}

QString SkyCatalogText::callbackContextUnavailable()
{
    return withStatusPrefix(QStringLiteral("Callback context unavailable"));
}

QString SkyCatalogText::emptyDeepSkyCatalog()
{
    return withStatusPrefix(QStringLiteral("Downloaded deep-sky catalog contains no DSOs"));
}

QString SkyCatalogText::missingConstellationData(const QString& reason)
{
    return QString("no constellation data (%1)").arg(reason);
}

QString SkyCatalogText::unknownSourcePreset(const QString& presetId)
{
    return withStatusPrefix(QStringLiteral("Unknown source preset '%1'").arg(presetId));
}

QString SkyCatalogText::cacheClearBlocked()
{
    return withStatusPrefix(QStringLiteral("Cannot clear cache while download is in progress"));
}

QString SkyCatalogText::starCacheClearResult(const bool cacheCleared)
{
    return withStatusPrefix(
        cacheCleared ? QStringLiteral("Star catalog cache cleared") : QStringLiteral("Star catalog cache clear failed")
    );
}

QString SkyCatalogText::deepSkyCacheClearResult(const bool cacheCleared)
{
    return withStatusPrefix(
        cacheCleared ? QStringLiteral("Deep-sky catalog cache cleared")
                     : QStringLiteral("Deep-sky catalog cache clear failed")
    );
}

QString SkyCatalogText::sourceCacheClearResult(const bool cacheCleared)
{
    return withStatusPrefix(
        cacheCleared ? QStringLiteral("Source cache cleared") : QStringLiteral("Source cache clear failed")
    );
}

QString SkyCatalogText::sourcePayloadUnavailable()
{
    return "Payload unavailable";
}

QString
SkyCatalogText::unavailableSourceSummary(const QString& baseStatusText, const std::size_t unavailableSourceCount)
{
    const QLocale locale = QLocale::system();
    return QString("%1 | Unavailable sources: %2 (cached payload missing or unreadable)")
        .arg(baseStatusText, locale.toString(static_cast<qulonglong>(unavailableSourceCount)));
}

QString SkyCatalogText::brightnessFilterSummary(
    const QString& baseStatusText, const std::size_t selectedBodyCount, const std::size_t parsedBodyCount
)
{
    const QLocale locale = QLocale::system();
    return QString("%1 | Brightness filter kept %2 of %3 objects")
        .arg(
            baseStatusText,
            locale.toString(static_cast<qulonglong>(selectedBodyCount)),
            locale.toString(static_cast<qulonglong>(parsedBodyCount))
        );
}

QString SkyCatalogText::constellationLineSummary(const QString& catalogSummaryText, QString statusText)
{
    statusText = withoutStatusPrefix(std::move(statusText));

    return QString("%1 | Constellation lines: %2").arg(catalogSummaryText, statusText);
}

QString SkyCatalogText::deepSkyFoundSummary(
    const QString& baseStatusText, const QString& sourceLabel, const std::size_t foundObjectCount
)
{
    const QLocale locale = QLocale::system();
    return QString("%1 | %2 found %3 objects")
        .arg(baseStatusText, sourceLabel, locale.toString(static_cast<qulonglong>(foundObjectCount)));
}

}  // namespace skygate::ui::internal
