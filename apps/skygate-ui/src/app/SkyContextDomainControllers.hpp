#pragma once

#include "SkyContextState.hpp"

#include "skygate/core/ProjectionTypes.hpp"
#include "skygate/core/Types.hpp"

#include <QString>

class QGeoPositionInfoSource;

namespace skygate::ui::internal {

struct SkySelectedCityChange final {
    bool idChanged = false;
    bool displayTextChanged = false;
};

class SkyTimelineController final {
public:
    [[nodiscard]] bool live() const noexcept;
    [[nodiscard]] bool toolbarCollapsed() const noexcept;
    [[nodiscard]] double speedMultiplier() const noexcept;
    [[nodiscard]] int stepSeconds() const noexcept;
    [[nodiscard]] bool catchingUpToCurrentUtc() const noexcept;

    [[nodiscard]] bool setLive(bool live) noexcept;
    [[nodiscard]] bool setToolbarCollapsed(bool toolbarCollapsed) noexcept;
    [[nodiscard]] bool setSpeedMultiplier(double speedMultiplier) noexcept;
    [[nodiscard]] bool setStepSeconds(int stepSeconds) noexcept;
    void setCatchingUpToCurrentUtc(bool catchingUpToCurrentUtc) noexcept;
    void resetSpeedProgress() noexcept;
    void addSpeedRemainderSeconds(double seconds) noexcept;
    [[nodiscard]] int takeWholeSpeedRemainderSeconds() noexcept;
    void startLiveAtCurrentUtc() noexcept;

private:
    SkyTimelineState m_state;
};

class SkyViewController final {
public:
    SkyViewController();

    [[nodiscard]] double magnitudeCutoff() const noexcept;
    [[nodiscard]] double centerAltitudeDeg() const noexcept;
    [[nodiscard]] double centerAzimuthDeg() const noexcept;
    [[nodiscard]] double fieldOfViewDeg() const noexcept;
    [[nodiscard]] skygate::core::ProjectionType projectionType() const noexcept;

    [[nodiscard]] bool setMagnitudeCutoff(double magnitudeCutoff) noexcept;
    [[nodiscard]] bool setCenter(double altitudeDeg, double azimuthDeg) noexcept;
    [[nodiscard]] bool setFieldOfViewDeg(double fieldOfViewDeg) noexcept;
    [[nodiscard]] bool setProjectionType(skygate::core::ProjectionType projectionType);

private:
    SkyViewState m_state;
};

class SkyLocationController final {
public:
    [[nodiscard]] const skygate::core::SkyContext& context() const noexcept;
    [[nodiscard]] skygate::core::GeoLocation observer() const noexcept;
    [[nodiscard]] skygate::core::UtcTimePoint utcTime() const noexcept;
    [[nodiscard]] SkyContextLocationSource source() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString selectedCityId() const;
    [[nodiscard]] QString selectedCityDisplayText() const;
    [[nodiscard]] QGeoPositionInfoSource* positionSource() const noexcept;
    [[nodiscard]] bool positionSourceConnected() const noexcept;
    [[nodiscard]] bool requestLocationPermission() const noexcept;

    void setPositionSource(QGeoPositionInfoSource* positionSource) noexcept;
    void setPositionSourceConnected(bool positionSourceConnected) noexcept;
    void setRequestLocationPermission(bool requestLocationPermission) noexcept;
    void setUtcTime(const skygate::core::UtcTimePoint& utcTime) noexcept;
    void setObserver(const skygate::core::GeoLocation& observer) noexcept;
    [[nodiscard]] bool setSource(SkyContextLocationSource source) noexcept;
    [[nodiscard]] bool setStatusText(const QString& statusText);
    [[nodiscard]] SkySelectedCityChange clearSelectedCity();
    [[nodiscard]] SkySelectedCityChange setSelectedCity(
        const QString& cityId,
        const QString& displayText
    );

private:
    SkyLocationState m_state;
};

class SkySearchController final {
public:
    [[nodiscard]] bool toolbarCollapsed() const noexcept;
    [[nodiscard]] QString selectedTargetKind() const;
    [[nodiscard]] QString selectedTargetId() const;
    [[nodiscard]] bool hasTrackedTarget() const;
    [[nodiscard]] QString trackedTargetKind() const;
    [[nodiscard]] QString trackedTargetId() const;
    [[nodiscard]] QString trackedTargetDisplayText() const;

    [[nodiscard]] bool setToolbarCollapsed(bool toolbarCollapsed) noexcept;
    [[nodiscard]] bool setSelectedTarget(const QString& targetKind, const QString& targetId);
    [[nodiscard]] bool setTrackedTarget(
        const QString& targetKind,
        const QString& targetId,
        const QString& displayText
    );

private:
    SkySearchState m_state;
};

}  // namespace skygate::ui::internal
