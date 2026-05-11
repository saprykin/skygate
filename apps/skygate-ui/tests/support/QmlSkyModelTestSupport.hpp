#pragma once

#include "SkyContextController.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkySceneModel.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include <QString>

#include <algorithm>
#include <memory>
#include <span>

namespace skygate::ui::tests {

inline std::unique_ptr<SkyContextController> makeController()
{
    auto starCatalog = skygate::ephemeris::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    if (ephemerisEngine == nullptr) {
        return {};
    }

    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine)
    );
}

inline std::unique_ptr<SkySceneModel> makeSceneModel(SkyContextController& controller)
{
    auto sceneModel = std::make_unique<SkySceneModel>();
    sceneModel->setSkyContextController(&controller);
    return sceneModel;
}

inline bool catalogContainsDisplayName(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const QString& displayName
)
{
    return std::any_of(
        bodies.begin(),
        bodies.end(),
        [&displayName](const skygate::ephemeris::CelestialBody& body) {
            return QString::fromStdString(body.displayName) == displayName;
        }
    );
}

inline bool catalogContainsAlias(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const QString& alias
)
{
    return std::any_of(
        bodies.begin(),
        bodies.end(),
        [&alias](const skygate::ephemeris::CelestialBody& body) {
            if (!body.deepSkyObject.has_value()) {
                return false;
            }
            return std::any_of(
                body.deepSkyObject->aliases.begin(),
                body.deepSkyObject->aliases.end(),
                [&alias](const std::string& bodyAlias) {
                    return QString::fromStdString(bodyAlias) == alias;
                }
            );
        }
    );
}

}  // namespace skygate::ui::tests
