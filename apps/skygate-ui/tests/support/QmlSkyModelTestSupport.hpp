#pragma once

#include "CatalogBodyQuery.hpp"
#include "ITimeSource.hpp"
#include "SkyEphemerisTestSupport.hpp"
#include "app/SkyContextController.hpp"
#include "time/SkyQtTimeCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "scene/SkySceneModel.hpp"
#include "search/SkyObjectSearchModel.hpp"

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
        return fixedNowUtc();
    }

    [[nodiscard]] static const skygate::core::ITimeSource& shared()
    {
        static const FixedTimeSource source;
        return source;
    }

    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return SkyQtTimeCodec::toUtcTimePoint(defaultUtc());
    }
};

inline std::unique_ptr<SkyContextController> makeController()
{
    auto starCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(*starCatalog);
    if (!skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(ephemerisEngineResult.status)) {
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

}  // namespace skygate::ui::tests
