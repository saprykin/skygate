#include "SkyCatalogText.hpp"

#include <QLocale>

namespace skygate::ui::internal {

QString SkyCatalogText::datasetInfo(
    const std::size_t bodyCount,
    const std::size_t constellationCount
)
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1 | Constellations: %2").arg(
        locale.toString(static_cast<qulonglong>(bodyCount)),
        locale.toString(static_cast<qulonglong>(constellationCount))
    );
}

QString SkyCatalogText::deepSkyCatalogInfo(const std::size_t foundObjectCount)
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1").arg(
        locale.toString(static_cast<qulonglong>(foundObjectCount))
    );
}

QString SkyCatalogText::unknownCatalogPreset(const QString& presetId)
{
    return QString("Catalog: Unknown preset '%1'").arg(presetId);
}

QString SkyCatalogText::unknownDeepSkyPreset(const QString& presetId)
{
    return QString("Catalog: Unknown deep-sky preset '%1'").arg(presetId);
}

QString SkyCatalogText::cacheClearBlocked()
{
    return "Catalog: Cannot clear cache while download is in progress";
}

QString SkyCatalogText::starCacheClearResult(const bool cacheCleared)
{
    return cacheCleared
        ? "Catalog: Star catalog cache cleared"
        : "Catalog: Star catalog cache clear failed";
}

QString SkyCatalogText::deepSkyCacheClearResult(const bool cacheCleared)
{
    return cacheCleared
        ? "Catalog: Deep-sky catalog cache cleared"
        : "Catalog: Deep-sky catalog cache clear failed";
}

bool SkyCatalogText::isProcessingStatus(const QString& statusText)
{
    return statusText.startsWith("Catalog: Processing", Qt::CaseInsensitive);
}

QString SkyCatalogText::brightnessFilterSummary(
    const QString& baseStatusText,
    const std::size_t selectedBodyCount,
    const std::size_t parsedBodyCount
)
{
    const QLocale locale = QLocale::system();
    return QString("%1 | Brightness filter kept %2 of %3 objects").arg(
        baseStatusText,
        locale.toString(static_cast<qulonglong>(selectedBodyCount)),
        locale.toString(static_cast<qulonglong>(parsedBodyCount))
    );
}

QString SkyCatalogText::constellationLineSummary(
    const QString& catalogSummaryText,
    QString statusText
)
{
    if (statusText.startsWith("Catalog: ")) {
        statusText.remove(0, 9);
    }

    return QString("%1 | Constellation lines: %2").arg(catalogSummaryText, statusText);
}

QString SkyCatalogText::deepSkyFoundSummary(
    const QString& baseStatusText,
    const QString& sourceLabel,
    const std::size_t foundObjectCount
)
{
    const QLocale locale = QLocale::system();
    return QString("%1 | %2 found %3 objects").arg(
        baseStatusText,
        sourceLabel,
        locale.toString(static_cast<qulonglong>(foundObjectCount))
    );
}

}  // namespace skygate::ui::internal
