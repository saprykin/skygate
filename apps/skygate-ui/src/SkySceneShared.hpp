#pragma once

#include "skygate/core/ProjectionTypes.hpp"

#include <QString>

#include <cmath>
#include <string>

[[nodiscard]] inline QString normalizedSceneLookupKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

[[nodiscard]] inline QString normalizedSceneLookupKey(const std::string& value)
{
    return normalizedSceneLookupKey(QString::fromStdString(value));
}

[[nodiscard]] inline bool skySceneHorizontalIsFinite(
    const skygate::core::HorizontalCoordinate& horizontal
) noexcept
{
    return std::isfinite(horizontal.altitudeDeg) && std::isfinite(horizontal.azimuthDeg);
}

[[nodiscard]] inline bool skySceneScreenPointIsFinite(
    const skygate::core::ScreenPoint& point
) noexcept
{
    return point.isVisible && std::isfinite(point.x) && std::isfinite(point.y);
}
