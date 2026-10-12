#include "SkyViewController.hpp"
#include "ProjectionFactory.hpp"
#include "SkyViewState.hpp"
#include "math/ViewportMath.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace skygate::ui::internal {

SkyViewController::SkyViewController()
{
    m_state.projection = skygate::core::createProjection(m_state.projectionType);
}

double SkyViewController::magnitudeCutoff() const noexcept
{
    return m_state.magnitudeCutoff;
}

double SkyViewController::centerAltitudeDeg() const noexcept
{
    return m_state.centerAltitudeDeg;
}

double SkyViewController::centerAzimuthDeg() const noexcept
{
    return m_state.centerAzimuthDeg;
}

double SkyViewController::fieldOfViewDeg() const noexcept
{
    return m_state.fieldOfViewDeg;
}

skygate::core::ProjectionType SkyViewController::projectionType() const noexcept
{
    return m_state.projectionType;
}

bool SkyViewController::setMagnitudeCutoff(const double magnitudeCutoff) noexcept
{
    const double clamped =
        std::clamp(magnitudeCutoff, SkyViewState::kMagnitudeCutoffMin, SkyViewState::kMagnitudeCutoffMax);
    if (std::abs(m_state.magnitudeCutoff - clamped) < 1e-9) {
        return false;
    }

    m_state.magnitudeCutoff = clamped;
    return true;
}

bool SkyViewController::setCenter(const double altitudeDeg, const double azimuthDeg) noexcept
{
    const double nextAltitudeDeg = skygate::core::ViewportMath::clampAltitudeDeg(altitudeDeg);
    const double nextAzimuthDeg = skygate::core::ViewportMath::normalizeAzimuthDeg(azimuthDeg);
    if (std::abs(m_state.centerAltitudeDeg - nextAltitudeDeg) < 1e-9
        && std::abs(m_state.centerAzimuthDeg - nextAzimuthDeg) < 1e-9) {
        return false;
    }

    m_state.centerAltitudeDeg = nextAltitudeDeg;
    m_state.centerAzimuthDeg = nextAzimuthDeg;
    return true;
}

bool SkyViewController::setFieldOfViewDeg(const double fieldOfViewDeg) noexcept
{
    const double nextFieldOfViewDeg = skygate::core::ViewportMath::clampFieldOfViewDeg(fieldOfViewDeg);
    if (std::abs(m_state.fieldOfViewDeg - nextFieldOfViewDeg) < 1e-9) {
        return false;
    }

    m_state.fieldOfViewDeg = nextFieldOfViewDeg;
    return true;
}

bool SkyViewController::setProjectionType(const skygate::core::ProjectionType projectionType)
{
    if (m_state.projectionType == projectionType) {
        return false;
    }

    auto projection = skygate::core::createProjection(projectionType);
    if (projection == nullptr) {
        return false;
    }

    m_state.projectionType = projectionType;
    m_state.projection = std::move(projection);
    return true;
}

}  // namespace skygate::ui::internal
