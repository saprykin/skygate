#pragma once

#include <QColor>

namespace skygate::ui::internal {

#define SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(X)                                                                    \
    X(skyGradientTop)                                                                                                  \
    X(skyGradientBottom)                                                                                               \
    X(gridAltitudeLine)                                                                                                \
    X(gridAzimuthLine)                                                                                                 \
    X(constellationLine)                                                                                               \
    X(horizonLine)                                                                                                     \
    X(eclipticLine)                                                                                                    \
    X(celestialEquatorLine)                                                                                            \
    X(circumpolarBoundaryLine)                                                                                         \
    X(cardinalNorthLine)                                                                                               \
    X(cardinalEastLine)                                                                                                \
    X(cardinalSouthLine)                                                                                               \
    X(cardinalWestLine)                                                                                                \
    X(bodySun)                                                                                                         \
    X(bodyMoon)                                                                                                        \
    X(bodyPlanet)                                                                                                      \
    X(bodyStar)                                                                                                        \
    X(bodyConstellation)                                                                                               \
    X(bodyDeepSkyObject)                                                                                               \
    X(labelSun)                                                                                                        \
    X(labelMoon)                                                                                                       \
    X(labelPlanet)                                                                                                     \
    X(labelConstellation)                                                                                              \
    X(labelDeepSkyObject)                                                                                              \
    X(labelDefault)                                                                                                    \
    X(overlayTooltipBackground)                                                                                        \
    X(overlayTooltipBorder)                                                                                            \
    X(overlayTooltipText)                                                                                              \
    X(overlayLabelBackground)                                                                                          \
    X(overlayCardinalLabelBackground)                                                                                  \
    X(overlayLabelText)                                                                                                \
    X(selectionMarkerFill)                                                                                             \
    X(selectionMarkerBorder)                                                                                           \
    X(selectionMarkerOuterBorder)                                                                                      \
    X(selectionMarkerCenter)

struct SkyThemeRenderPalette final {
#define SKYGATE_DECLARE_RENDER_COLOR_MEMBER(name) QColor name;
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_DECLARE_RENDER_COLOR_MEMBER)
#undef SKYGATE_DECLARE_RENDER_COLOR_MEMBER
};

}  // namespace skygate::ui::internal
