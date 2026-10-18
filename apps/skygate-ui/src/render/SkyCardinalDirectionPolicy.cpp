#include "SkyCardinalDirectionPolicy.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

namespace skygate::ui::internal {

QColor SkyCardinalDirectionPolicy::lineColor(const double azimuthDeg, const SkyThemeRenderPalette& renderPalette)
{
    if (azimuthDeg == 0.0) {
        return renderPalette.cardinalNorthLine;
    }
    if (azimuthDeg == 90.0) {
        return renderPalette.cardinalEastLine;
    }
    if (azimuthDeg == 180.0) {
        return renderPalette.cardinalSouthLine;
    }
    if (azimuthDeg == 270.0) {
        return renderPalette.cardinalWestLine;
    }

    return renderPalette.gridAzimuthLine;
}

QString SkyCardinalDirectionPolicy::label(const double azimuthDeg)
{
    if (azimuthDeg == 0.0) {
        return QStringLiteral("N");
    }
    if (azimuthDeg == 90.0) {
        return QStringLiteral("E");
    }
    if (azimuthDeg == 180.0) {
        return QStringLiteral("S");
    }
    if (azimuthDeg == 270.0) {
        return QStringLiteral("W");
    }

    return {};
}

}  // namespace skygate::ui::internal
