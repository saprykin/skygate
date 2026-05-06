#pragma once

#include <QString>

#include <cstddef>

namespace skygate::ui::internal {

class SkyCatalogText final {
public:
    [[nodiscard]] static QString datasetInfo(
        std::size_t bodyCount,
        std::size_t constellationCount
    );
    [[nodiscard]] static QString deepSkyCatalogInfo(std::size_t foundObjectCount);
    [[nodiscard]] static QString unknownCatalogPreset(const QString& presetId);
    [[nodiscard]] static QString unknownDeepSkyPreset(const QString& presetId);
    [[nodiscard]] static QString cacheClearBlocked();
    [[nodiscard]] static QString starCacheClearResult(bool cacheCleared);
    [[nodiscard]] static QString deepSkyCacheClearResult(bool cacheCleared);
    [[nodiscard]] static bool isProcessingStatus(const QString& statusText);
    [[nodiscard]] static QString brightnessFilterSummary(
        const QString& baseStatusText,
        std::size_t selectedBodyCount,
        std::size_t parsedBodyCount
    );
    [[nodiscard]] static QString constellationLineSummary(
        const QString& catalogSummaryText,
        QString statusText
    );
    [[nodiscard]] static QString deepSkyFoundSummary(
        const QString& baseStatusText,
        const QString& sourceLabel,
        std::size_t foundObjectCount
    );
};

}  // namespace skygate::ui::internal
