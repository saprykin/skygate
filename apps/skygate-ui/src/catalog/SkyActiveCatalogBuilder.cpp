#include "SkyActiveCatalogBuilder.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace skygate::ui::internal {
namespace {

std::size_t countDeepSkyObjects(
    const std::span<const skygate::ephemeris::CelestialBody> bodies
)
{
    return static_cast<std::size_t>(std::count_if(
        bodies.begin(),
        bodies.end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject;
        }
    ));
}

QString normalizedSourceLabel(const QString& sourceLabel, const QString& fallbackLabel)
{
    const QString normalized = sourceLabel.trimmed();
    return normalized.isEmpty() ? fallbackLabel : normalized;
}

bool isAnalyticSolarSystemBody(const skygate::ephemeris::CelestialBody& body)
{
    return body.ephemerisSource == skygate::ephemeris::CelestialBodyEphemerisSource::Sun
        || body.ephemerisSource == skygate::ephemeris::CelestialBodyEphemerisSource::Moon
        || body.ephemerisSource == skygate::ephemeris::CelestialBodyEphemerisSource::Planet;
}

std::string toLowerId(const std::string& value)
{
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lower;
}

void appendDeepSkyAliasKey(
    std::vector<std::string>& keys,
    std::string value
)
{
    value = toLowerId(value);
    value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char character) {
        return !std::isalnum(character);
    }), value.end());
    if (!value.empty() && std::find(keys.begin(), keys.end(), value) == keys.end()) {
        keys.push_back(std::move(value));
    }
}

std::vector<std::string> deepSkyAliasKeys(const skygate::ephemeris::CelestialBody& body)
{
    std::vector<std::string> keys;
    appendDeepSkyAliasKey(keys, body.id);
    appendDeepSkyAliasKey(keys, body.displayName);
    if (!body.deepSkyObject.has_value()) {
        return keys;
    }

    for (const auto& alias : body.deepSkyObject->aliases) {
        appendDeepSkyAliasKey(keys, alias);
    }
    return keys;
}

bool sharesDeepSkyAlias(
    const skygate::ephemeris::CelestialBody& lhs,
    const skygate::ephemeris::CelestialBody& rhs
)
{
    const std::vector<std::string> lhsKeys = deepSkyAliasKeys(lhs);
    const std::vector<std::string> rhsKeys = deepSkyAliasKeys(rhs);
    return std::any_of(lhsKeys.begin(), lhsKeys.end(), [&rhsKeys](const std::string& key) {
        return std::find(rhsKeys.begin(), rhsKeys.end(), key) != rhsKeys.end();
    });
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
    auto activeCatalog = skygate::ephemeris::createCatalogWithCoreSolarSystemBodies(
        request.sourceCatalog
    );
    if (activeCatalog == nullptr) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }

    const skygate::ephemeris::IStarCatalog* deepSkyCatalog = request.deepSkyCatalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledDeepSkyCatalog;
    if (deepSkyCatalog == nullptr && request.useBundledDeepSkyCatalog) {
        bundledDeepSkyCatalog = skygate::ephemeris::createBundledStarCatalog();
        deepSkyCatalog = bundledDeepSkyCatalog.get();
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

    std::vector<skygate::ephemeris::CelestialBody> activeBodies(
        activeCatalog->bodies().begin(),
        activeCatalog->bodies().end()
    );
    std::vector<std::uint8_t> activeSourceIds;
    activeSourceIds.reserve(activeBodies.size());
    for (const auto& body : activeBodies) {
        activeSourceIds.push_back(
            isAnalyticSolarSystemBody(body) ? builtInSourceId : primarySourceId
        );
    }

    result.foundDeepSkyObjectCount = request.knownDeepSkyObjectCount;
    if (deepSkyCatalog != nullptr) {
        if (result.foundDeepSkyObjectCount == 0U || request.useBundledDeepSkyCatalog) {
            result.foundDeepSkyObjectCount = countDeepSkyObjects(deepSkyCatalog->bodies());
        }

        std::vector<skygate::ephemeris::CelestialBody> mergedBodies;
        std::vector<std::uint8_t> mergedSourceIds;
        mergedBodies.reserve(activeBodies.size() + deepSkyCatalog->bodies().size());
        mergedSourceIds.reserve(activeBodies.size() + deepSkyCatalog->bodies().size());

        for (std::size_t index = 0; index < activeBodies.size(); ++index) {
            const auto& body = activeBodies[index];
            if (body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
                continue;
            }

            mergedBodies.push_back(body);
            mergedSourceIds.push_back(activeSourceIds[index]);
        }

        for (std::size_t index = 0; index < activeBodies.size(); ++index) {
            const auto& body = activeBodies[index];
            if (body.type != skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
                continue;
            }

            const bool replacedByDeepSkyObject = std::any_of(
                deepSkyCatalog->bodies().begin(),
                deepSkyCatalog->bodies().end(),
                [&body](const skygate::ephemeris::CelestialBody& deepSkyBody) {
                    return deepSkyBody.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject
                        && sharesDeepSkyAlias(body, deepSkyBody);
                }
            );
            if (replacedByDeepSkyObject) {
                continue;
            }

            mergedBodies.push_back(body);
            mergedSourceIds.push_back(activeSourceIds[index]);
        }

        for (const auto& body : deepSkyCatalog->bodies()) {
            if (body.type != skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
                continue;
            }

            mergedBodies.push_back(body);
            mergedSourceIds.push_back(deepSkySourceId);
        }

        activeCatalog = skygate::ephemeris::createStarCatalogFromBodies(std::move(mergedBodies));
        activeSourceIds = std::move(mergedSourceIds);
    }

    if (activeCatalog == nullptr) {
        result.errorText = "Catalog: Failed to merge deep-sky catalog";
        return result;
    }

    const auto bodies = activeCatalog->bodies();
    std::size_t catalogConstellationCount = 0;
    std::size_t deepSkyObjectCount = 0;
    for (const auto& body : bodies) {
        if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            ++catalogConstellationCount;
        }
        if (body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject) {
            ++deepSkyObjectCount;
        }
    }

    result.bodyCount = bodies.size();
    result.constellationCount = std::max(
        catalogConstellationCount,
        request.currentConstellationCount
    );
    result.deepSkyObjectCount = deepSkyObjectCount;
    result.ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*activeCatalog);
    if (result.ephemerisEngine == nullptr) {
        result.errorText = "Catalog: Failed to load";
        return result;
    }

    result.sourceIds = std::move(activeSourceIds);

    result.statusText =
        QString("Catalog: %1 + %2 (%3 objects, %4 deep sky, %5 constellations)").arg(
            request.sourceLabel,
            request.deepSkySourceLabel,
            QString::number(static_cast<qulonglong>(result.bodyCount)),
            QString::number(static_cast<qulonglong>(result.deepSkyObjectCount)),
            QString::number(static_cast<qulonglong>(result.constellationCount))
        );
    result.catalog = std::move(activeCatalog);
    return result;
}

}  // namespace skygate::ui::internal
