#pragma once

#include "CelestialReferenceCalculator.hpp"
#include "SkyContextController.hpp"
#include "SkyEphemerisTestSupport.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkySceneModel.hpp"
#include "SkySceneModelTestHarness.hpp"
#include "SkyTheme.hpp"
#include "SkyTimeController.hpp"
#include "catalog/SkyActiveCatalogBuilder.hpp"
#include "math/ViewportMath.hpp"

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace skygate::ui::tests {

inline bool overlayItemsContainText(const QVariantList& overlayItems, const QString& text)
{
    for (const QVariant& overlayItem : overlayItems) {
        if (overlayItem.toMap().value("text").toString() == text) {
            return true;
        }
    }

    return false;
}

inline bool overlayItemsContainText(const std::vector<SkyRenderLabel>& overlayItems, const QString& text)
{
    for (const SkyRenderLabel& overlayItem : overlayItems) {
        if (overlayItem.text == text) {
            return true;
        }
    }

    return false;
}

inline bool overlayItemsContainKind(const QVariantList& overlayItems, const QString& kind)
{
    for (const QVariant& overlayItem : overlayItems) {
        if (overlayItem.toMap().value("kind").toString() == kind) {
            return true;
        }
    }

    return false;
}

inline QString inspectorFieldValue(const QVariantMap& inspector, const QString& label)
{
    const QVariantList fields = inspector.value("fields").toList();
    for (const QVariant& fieldValue : fields) {
        const QVariantMap field = fieldValue.toMap();
        if (field.value("label").toString() == label) {
            return field.value("value").toString();
        }
    }

    return {};
}

inline SkyRenderFrame buildDeepSkyRenderFrame(
    std::vector<skygate::ephemeris::DistantCelestialBody> bodies,
    std::vector<skygate::core::HorizontalCoordinate> horizontalCoordinates,
    const double fovDeg
)
{
    skygate::ephemeris::EphemerisSnapshot snapshot;
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.reserve(bodies.size());
    for (std::uint32_t index = 0U; index < bodies.size(); ++index) {
        order.push_back({.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = index});
    }
    auto catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{}, std::move(bodies), std::move(order)
    );
    snapshot.catalogBodies = catalogBodies;
    for (std::uint32_t index = 0; index < catalogBodies->size(); ++index) {
        snapshot.states.push_back(
            skygate::ephemeris::CelestialBodyState{.bodyIndex = index, .horizontal = horizontalCoordinates.at(index)}
        );
    }

    const auto projectionParams =
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 700.0, 45.0, 180.0, fovDeg);
    const auto projection =
        skygate::core::PreparedProjection::create(skygate::core::ProjectionType::Stereographic, projectionParams);
    Q_ASSERT(projection.has_value());

    const skygate::ui::internal::SkyThemeRepository themeRepository;
    const SkyRenderFrameBuilder frameBuilder;
    return frameBuilder.buildFrame(
        snapshot,
        *projection,
        {},
        {},
        12.0,
        1000.0,
        700.0,
        themeRepository.defaultTheme().render,
        SkyOverlayLayerVisibility{}
    );
}

}  // namespace skygate::ui::tests
