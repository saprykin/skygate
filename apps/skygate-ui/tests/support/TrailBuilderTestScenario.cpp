#include "TrailBuilderTestScenario.hpp"
#include "math/ViewportMath.hpp"
#include "selection/SkyObjectTrailInput.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>

namespace skygate::ui::tests {

namespace {

skygate::ui::internal::SkyThemeRenderPalette makeRenderTheme()
{
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    renderTheme.selectionMarkerBorder = QColor("#80c0ff");
    return renderTheme;
}

}  // namespace

std::optional<skygate::core::PreparedProjection> TrailBuilderTestScenario::makeProjection(const double fovDeg)
{
    return skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 800.0, 45.0, 180.0, fovDeg)
    );
}

SkyObjectTrailInput TrailBuilderTestScenario::makeInput(
    const skygate::ephemeris::IEphemerisEngine& engine, const skygate::core::PreparedProjection& projection
)
{
    SkyObjectTrailInput input;
    input.ephemerisEngine = &engine;
    input.preparedProjection = &projection;
    input.skyContext.observer = {.latitudeDeg = 47.0, .longitudeDeg = 8.0, .elevationMeters = 400.0};
    input.renderTheme = makeRenderTheme();
    input.targetBodyIndex = 7U;
    input.viewportWidth = 1000.0;
    input.viewportHeight = 800.0;
    return input;
}

}  // namespace skygate::ui::tests
