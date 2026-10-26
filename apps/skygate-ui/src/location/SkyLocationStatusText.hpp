#pragma once

#include <QString>

namespace skygate::ui::internal {

// Location status texts of the observer location selection and the device
// acquisition. The wording lives here instead of at the call sites, so the
// status footer presents one vocabulary.
class SkyLocationStatusText final {
public:
    [[nodiscard]] static QString currentDevice();
    [[nodiscard]] static QString city();
    [[nodiscard]] static QString cityNamed(const QString& cityDisplayText);
    [[nodiscard]] static QString customCoordinates();
    [[nodiscard]] static QString usingCurrentLocation();

    [[nodiscard]] static QString initializing();
    [[nodiscard]] static QString deviceSourceUnavailable();
    [[nodiscard]] static QString positioningUnavailable();
    [[nodiscard]] static QString waitingForPermission();
    [[nodiscard]] static QString permissionDenied();
    [[nodiscard]] static QString locating();
    [[nodiscard]] static QString updateFailed();
};

}  // namespace skygate::ui::internal
