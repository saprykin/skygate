#pragma once

#include "SkyContextController.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkySceneModel.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"

#include <QString>

#include <algorithm>
#include <memory>
#include <span>

namespace skygate::ui::tests {

inline std::unique_ptr<SkyContextController> makeController()
{
    auto starCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(*starCatalog);
    if (!ephemerisEngineResult.isSuccess()) {
        return {};
    }
    auto ephemerisEngine = std::move(ephemerisEngineResult.engine);
    if (ephemerisEngine == nullptr) {
        return {};
    }

    return std::make_unique<SkyContextController>(std::move(starCatalog), std::move(ephemerisEngine));
}

inline std::unique_ptr<SkySceneModel> makeSceneModel(SkyContextController& controller)
{
    auto sceneModel = std::make_unique<SkySceneModel>();
    sceneModel->setSkyContextController(&controller);
    return sceneModel;
}

inline bool catalogContainsDisplayName(
    const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const QString& displayName
)
{
    return std::any_of(bodies.begin(), bodies.end(), [&displayName](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->displayName) == displayName;
    });
}

inline bool
catalogContainsAlias(const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const QString& alias)
{
    return std::any_of(bodies.begin(), bodies.end(), [&alias](const skygate::ephemeris::BaseCelestialBody* body) {
        if (body == nullptr || !body->deepSkyObjectValue().has_value()) {
            return false;
        }
        return std::any_of(
            body->deepSkyObjectValue()->aliases.begin(),
            body->deepSkyObjectValue()->aliases.end(),
            [&alias](const std::string& bodyAlias) { return QString::fromStdString(bodyAlias) == alias; }
        );
    });
}

}  // namespace skygate::ui::tests
