#include "SkyContextDomainControllers.hpp"

#include "SkyContextControllerSupport.hpp"

#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/core/math/ViewportMath.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace skygate::ui::internal {

bool SkyTimelineController::live() const noexcept
{
    return m_state.live;
}

bool SkyTimelineController::toolbarCollapsed() const noexcept
{
    return m_state.toolbarCollapsed;
}

double SkyTimelineController::speedMultiplier() const noexcept
{
    return m_state.speedMultiplier;
}

int SkyTimelineController::stepSeconds() const noexcept
{
    return m_state.stepSeconds;
}

bool SkyTimelineController::catchingUpToCurrentUtc() const noexcept
{
    return m_state.catchingUpToCurrentUtc;
}

bool SkyTimelineController::setLive(const bool live) noexcept
{
    if (m_state.live == live) {
        return false;
    }

    m_state.live = live;
    resetSpeedProgress();
    m_state.catchingUpToCurrentUtc = false;
    return true;
}

bool SkyTimelineController::setToolbarCollapsed(const bool toolbarCollapsed) noexcept
{
    if (m_state.toolbarCollapsed == toolbarCollapsed) {
        return false;
    }

    m_state.toolbarCollapsed = toolbarCollapsed;
    return true;
}

bool SkyTimelineController::setSpeedMultiplier(const double speedMultiplier) noexcept
{
    if (std::abs(m_state.speedMultiplier - speedMultiplier) < 1e-9) {
        return false;
    }

    m_state.speedMultiplier = speedMultiplier;
    resetSpeedProgress();
    return true;
}

bool SkyTimelineController::setStepSeconds(const int stepSeconds) noexcept
{
    if (m_state.stepSeconds == stepSeconds) {
        return false;
    }

    m_state.stepSeconds = stepSeconds;
    return true;
}

void SkyTimelineController::setCatchingUpToCurrentUtc(
    const bool catchingUpToCurrentUtc
) noexcept
{
    m_state.catchingUpToCurrentUtc = catchingUpToCurrentUtc;
}

void SkyTimelineController::resetSpeedProgress() noexcept
{
    m_state.speedRemainderSeconds = 0.0;
}

void SkyTimelineController::addSpeedRemainderSeconds(const double seconds) noexcept
{
    m_state.speedRemainderSeconds += seconds;
}

int SkyTimelineController::takeWholeSpeedRemainderSeconds() noexcept
{
    const int wholeSeconds = static_cast<int>(std::floor(m_state.speedRemainderSeconds));
    if (wholeSeconds <= 0) {
        return 0;
    }

    m_state.speedRemainderSeconds -= wholeSeconds;
    return wholeSeconds;
}

void SkyTimelineController::startLiveAtCurrentUtc() noexcept
{
    m_state.live = true;
    m_state.catchingUpToCurrentUtc = false;
    resetSpeedProgress();
}

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
    const double clamped = std::clamp(
        magnitudeCutoff,
        SkyContextControllerConstants::kMagnitudeCutoffMin,
        SkyContextControllerConstants::kMagnitudeCutoffMax
    );
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
    if (
        std::abs(m_state.centerAltitudeDeg - nextAltitudeDeg) < 1e-9
        && std::abs(m_state.centerAzimuthDeg - nextAzimuthDeg) < 1e-9
    ) {
        return false;
    }

    m_state.centerAltitudeDeg = nextAltitudeDeg;
    m_state.centerAzimuthDeg = nextAzimuthDeg;
    return true;
}

bool SkyViewController::setFieldOfViewDeg(const double fieldOfViewDeg) noexcept
{
    const double nextFieldOfViewDeg =
        skygate::core::ViewportMath::clampFieldOfViewDeg(fieldOfViewDeg);
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

const skygate::core::SkyContext& SkyLocationController::context() const noexcept
{
    return m_state.context;
}

skygate::core::GeoLocation SkyLocationController::observer() const noexcept
{
    return m_state.context.observer;
}

skygate::core::UtcTimePoint SkyLocationController::utcTime() const noexcept
{
    return m_state.context.utcTime;
}

SkyContextLocationSource SkyLocationController::source() const noexcept
{
    return m_state.source;
}

QString SkyLocationController::statusText() const
{
    return m_state.statusText;
}

QString SkyLocationController::selectedCityId() const
{
    return m_state.selectedCityId;
}

QString SkyLocationController::selectedCityDisplayText() const
{
    return m_state.selectedCityDisplayText;
}

QGeoPositionInfoSource* SkyLocationController::positionSource() const noexcept
{
    return m_state.positionSource;
}

bool SkyLocationController::positionSourceConnected() const noexcept
{
    return m_state.positionSourceConnected;
}

bool SkyLocationController::requestLocationPermission() const noexcept
{
    return m_state.requestLocationPermission;
}

void SkyLocationController::setPositionSource(QGeoPositionInfoSource* positionSource) noexcept
{
    m_state.positionSource = positionSource;
}

void SkyLocationController::setPositionSourceConnected(
    const bool positionSourceConnected
) noexcept
{
    m_state.positionSourceConnected = positionSourceConnected;
}

void SkyLocationController::setRequestLocationPermission(
    const bool requestLocationPermission
) noexcept
{
    m_state.requestLocationPermission = requestLocationPermission;
}

void SkyLocationController::setUtcTime(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    m_state.context.utcTime = utcTime;
}

void SkyLocationController::setObserver(const skygate::core::GeoLocation& observer) noexcept
{
    m_state.context.observer = observer;
}

bool SkyLocationController::setSource(const SkyContextLocationSource source) noexcept
{
    if (m_state.source == source) {
        return false;
    }

    m_state.source = source;
    return true;
}

bool SkyLocationController::setStatusText(const QString& statusText)
{
    if (m_state.statusText == statusText) {
        return false;
    }

    m_state.statusText = statusText;
    return true;
}

SkySelectedCityChange SkyLocationController::clearSelectedCity()
{
    SkySelectedCityChange change {
        .idChanged = !m_state.selectedCityId.isEmpty(),
        .displayTextChanged = !m_state.selectedCityDisplayText.isEmpty()
    };
    m_state.selectedCityId.clear();
    m_state.selectedCityDisplayText.clear();
    return change;
}

SkySelectedCityChange SkyLocationController::setSelectedCity(
    const QString& cityId,
    const QString& displayText
)
{
    SkySelectedCityChange change {
        .idChanged = m_state.selectedCityId != cityId,
        .displayTextChanged = m_state.selectedCityDisplayText != displayText
    };
    m_state.selectedCityId = cityId;
    m_state.selectedCityDisplayText = displayText;
    m_state.source = SkyContextLocationSource::City;
    return change;
}

bool SkySearchController::toolbarCollapsed() const noexcept
{
    return m_state.toolbarCollapsed;
}

QString SkySearchController::selectedTargetKind() const
{
    return m_state.selectedTargetKind;
}

QString SkySearchController::selectedTargetId() const
{
    return m_state.selectedTargetId;
}

bool SkySearchController::hasTrackedTarget() const
{
    return !m_state.trackedTargetKind.trimmed().isEmpty()
        && !m_state.trackedTargetId.trimmed().isEmpty();
}

QString SkySearchController::trackedTargetKind() const
{
    return m_state.trackedTargetKind;
}

QString SkySearchController::trackedTargetId() const
{
    return m_state.trackedTargetId;
}

QString SkySearchController::trackedTargetDisplayText() const
{
    return m_state.trackedTargetDisplayText;
}

bool SkySearchController::setToolbarCollapsed(const bool toolbarCollapsed) noexcept
{
    if (m_state.toolbarCollapsed == toolbarCollapsed) {
        return false;
    }

    m_state.toolbarCollapsed = toolbarCollapsed;
    return true;
}

bool SkySearchController::setSelectedTarget(const QString& targetKind, const QString& targetId)
{
    const QString normalizedTargetKind = targetKind.trimmed();
    const QString normalizedTargetId = targetId.trimmed();
    if (
        m_state.selectedTargetKind == normalizedTargetKind
        && m_state.selectedTargetId == normalizedTargetId
    ) {
        return false;
    }

    m_state.selectedTargetKind = normalizedTargetKind;
    m_state.selectedTargetId = normalizedTargetId;
    return true;
}

bool SkySearchController::setTrackedTarget(
    const QString& targetKind,
    const QString& targetId,
    const QString& displayText
)
{
    const QString normalizedTargetKind = targetKind.trimmed();
    const QString normalizedTargetId = targetId.trimmed();
    const QString normalizedDisplayText = displayText.trimmed();
    if (
        m_state.trackedTargetKind == normalizedTargetKind
        && m_state.trackedTargetId == normalizedTargetId
        && m_state.trackedTargetDisplayText == normalizedDisplayText
    ) {
        return false;
    }

    m_state.trackedTargetKind = normalizedTargetKind;
    m_state.trackedTargetId = normalizedTargetId;
    m_state.trackedTargetDisplayText = normalizedDisplayText;
    return true;
}

}  // namespace skygate::ui::internal
