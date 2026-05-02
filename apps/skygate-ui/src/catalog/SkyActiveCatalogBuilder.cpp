#include "SkyActiveCatalogBuilder.hpp"

#include "skygate/ephemeris/CatalogComposer.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <cstdint>
#include <utility>

namespace skygate::ui::internal {
namespace {

QString normalizedSourceLabel(const QString& sourceLabel, const QString& fallbackLabel)
{
    const QString normalized = sourceLabel.trimmed();
    return normalized.isEmpty() ? fallbackLabel : normalized;
}

}  // namespace

bool SkyActiveCatalogBuildResult::isSuccess() const noexcept
{
    return catalog != nullptr && ephemerisEngine != nullptr;
}

SkyActiveCatalogBuildResult SkyActiveCatalogBuilder::build(
    const SkyActiveCatalogBuildRequest& request
)
{
    SkyActiveCatalogBuildResult result;
    auto activeCatalog = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = request.sourceCatalog,
        .deepSkyCatalog = request.deepSkyCatalog,
        .useBundledDeepSkyCatalog = request.useBundledDeepSkyCatalog,
        .currentConstellationCount = request.currentConstellationCount,
        .knownDeepSkyObjectCount = request.knownDeepSkyObjectCount
    });
    if (!activeCatalog.isSuccess()) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }

    const auto sourceIdForLabel = [&result](const QString& label) {
        const int existingIndex = result.sourceLabels.indexOf(label);
        if (existingIndex >= 0) {
            return static_cast<std::uint8_t>(existingIndex);
        }

        result.sourceLabels.push_back(label);
        return static_cast<std::uint8_t>(result.sourceLabels.size() - 1);
    };
    const std::uint8_t primarySourceId = sourceIdForLabel(
        normalizedSourceLabel(request.sourceLabel, "Catalog")
    );
    const std::uint8_t deepSkySourceId = sourceIdForLabel(
        normalizedSourceLabel(request.deepSkySourceLabel, "Deep sky catalog")
    );
    const std::uint8_t builtInSourceId = sourceIdForLabel("Built-in ephemeris");

    result.sourceIds.reserve(activeCatalog.sourceKinds.size());
    for (const auto sourceKind : activeCatalog.sourceKinds) {
        switch (sourceKind) {
        case skygate::ephemeris::CatalogCompositionSource::Primary:
            result.sourceIds.push_back(primarySourceId);
            break;
        case skygate::ephemeris::CatalogCompositionSource::DeepSky:
            result.sourceIds.push_back(deepSkySourceId);
            break;
        case skygate::ephemeris::CatalogCompositionSource::BuiltInEphemeris:
            result.sourceIds.push_back(builtInSourceId);
            break;
        }
    }

    result.bodyCount = activeCatalog.bodyCount;
    result.constellationCount = activeCatalog.constellationCount;
    result.deepSkyObjectCount = activeCatalog.deepSkyObjectCount;
    result.foundDeepSkyObjectCount = activeCatalog.foundDeepSkyObjectCount;
    result.ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*activeCatalog.catalog);
    if (result.ephemerisEngine == nullptr) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }

    result.statusText =
        QString("Catalog: %1 + %2 (%3 objects, %4 deep sky, %5 constellations)").arg(
            request.sourceLabel,
            request.deepSkySourceLabel,
            QString::number(static_cast<qulonglong>(result.bodyCount)),
            QString::number(static_cast<qulonglong>(result.deepSkyObjectCount)),
            QString::number(static_cast<qulonglong>(result.constellationCount))
        );
    result.catalog = std::move(activeCatalog.catalog);
    return result;
}

}  // namespace skygate::ui::internal
