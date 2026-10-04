#include "SkyActiveCatalogBuilder.hpp"

#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"

#include <QString>

#include <string>
#include <utility>

namespace skygate::ui::internal {
namespace {

constexpr const char* kPrimarySourceId = "primary";
constexpr const char* kDeepSkySourceId = "deep-sky";
constexpr const char* kBundledCoreSourceId = "bundled-core";

QString normalizedSourceLabel(const QString& sourceLabel, const QString& fallbackLabel)
{
    const QString normalized = sourceLabel.trimmed();
    return normalized.isEmpty() ? fallbackLabel : normalized;
}

}  // namespace

bool SkyActiveCatalogBuildResult::isSuccess() const noexcept
{
    return catalog != nullptr;
}

SkyActiveCatalogBuildResult SkyActiveCatalogBuilder::build(const SkyActiveCatalogBuildRequest& request)
{
    SkyActiveCatalogBuildResult result;

    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCatalog =
        skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    const skygate::ephemeris::IStarCatalog* deepSkyCatalog = request.deepSkyCatalog;
    if (deepSkyCatalog == nullptr && request.useBundledDeepSkyCatalog) {
        deepSkyCatalog = bundledCatalog.get();
    }

    skygate::ephemeris::CatalogCompositionRequest collectionRequest;
    collectionRequest.currentConstellationCount = request.currentConstellationCount;
    collectionRequest.knownDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    collectionRequest.sources.push_back(
        skygate::ephemeris::CatalogCompositionSourceEntry{
            .sourceId = std::string(kPrimarySourceId),
            .enabled = true,
            .catalog = &request.sourceCatalog,
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
        }
    );
    if (bundledCatalog != nullptr) {
        collectionRequest.sources.push_back(
            skygate::ephemeris::CatalogCompositionSourceEntry{
                .sourceId = std::string(kBundledCoreSourceId),
                .enabled = true,
                .catalog = bundledCatalog.get(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::AugmentCore,
            }
        );
    }
    if (deepSkyCatalog != nullptr) {
        collectionRequest.sources.push_back(
            skygate::ephemeris::CatalogCompositionSourceEntry{
                .sourceId = std::string(kDeepSkySourceId),
                .enabled = true,
                .catalog = deepSkyCatalog,
                .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
            }
        );
    }

    skygate::ephemeris::CatalogCompositionResult composed =
        skygate::ephemeris::CatalogComposer::composeCollection(collectionRequest);
    if (!composed.isSuccess()) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }

    result.sourceIds.reserve(composed.sourceIds.size());
    for (const std::string& sourceId : composed.sourceIds) {
        result.sourceIds.push_back(QString::fromStdString(sourceId));
    }
    result.contributorSourceIds.reserve(composed.contributorSourceIds.size());
    for (const std::vector<std::string>& contributors : composed.contributorSourceIds) {
        QStringList contributorIds;
        contributorIds.reserve(static_cast<int>(contributors.size()));
        for (const std::string& sourceId : contributors) {
            contributorIds.push_back(QString::fromStdString(sourceId));
        }
        result.contributorSourceIds.push_back(std::move(contributorIds));
    }

    result.sourceTitles.insert(
        QString::fromLatin1(kPrimarySourceId), normalizedSourceLabel(request.sourceLabel, QStringLiteral("Catalog"))
    );
    result.sourceTitles.insert(
        QString::fromLatin1(kDeepSkySourceId),
        normalizedSourceLabel(request.deepSkySourceLabel, QStringLiteral("Deep sky catalog"))
    );
    result.sourceTitles.insert(QString::fromLatin1(kBundledCoreSourceId), QStringLiteral("Bundled core"));

    result.bodyCount = composed.bodyCount;
    result.constellationCount = composed.constellationCount;
    result.deepSkyObjectCount = composed.deepSkyObjectCount;
    result.foundDeepSkyObjectCount = composed.foundDeepSkyObjectCount;
    result.statusText = QString("Catalog: %1 + %2 (%3 objects, %4 deep sky, %5 constellations)")
                            .arg(
                                result.sourceTitles.value(QString::fromLatin1(kPrimarySourceId)),
                                result.sourceTitles.value(QString::fromLatin1(kDeepSkySourceId)),
                                QString::number(static_cast<qulonglong>(result.bodyCount)),
                                QString::number(static_cast<qulonglong>(result.deepSkyObjectCount)),
                                QString::number(static_cast<qulonglong>(result.constellationCount))
                            );
    result.catalog = std::move(composed.catalog);
    return result;
}

}  // namespace skygate::ui::internal
