#pragma once

#include "ITimeSource.hpp"
#include "SkyContextController.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkySceneModel.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"

#include <QDateTime>
#include <QString>
#include <QTimeZone>

#include <algorithm>
#include <memory>
#include <span>

namespace skygate::ui::tests {

// Fixed scene time for QML tests. The rendered sky depends on the observer
// time, so without a pinned time source tests drift with the wall clock and
// the main-window rendering test failed at times of day where a bright star
// projected close enough to the 560x640 window edge to push its
// SkyOverlayLabel out of bounds.
class FixedTimeSource final : public skygate::core::ITimeSource {
public:
    [[nodiscard]] static QDateTime defaultUtc()
    {
        return QDateTime(QDate(2026, 5, 6), QTime(9, 30, 0), QTimeZone::UTC);
    }

    [[nodiscard]] static const skygate::core::ITimeSource& shared()
    {
        static const FixedTimeSource source;
        return source;
    }

    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(defaultUtc());
    }
};

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

    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.timeSource = &FixedTimeSource::shared();
    return std::make_unique<SkyContextController>(
        std::move(starCatalog), std::move(ephemerisEngine), initializationOptions, nullptr
    );
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
