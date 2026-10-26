#pragma once

#include <QObject>

#include <functional>

namespace skygate::ui::internal {

// Platform side of the location permission handshake. The interface keeps the
// permission outcome replaceable, which lets tests cover the acquisition paths
// without touching the live location permission of the host.
class ILocationPermissionGate {
public:
    virtual ~ILocationPermissionGate() = default;

    [[nodiscard]] virtual Qt::PermissionStatus status() const = 0;
    virtual void request(QObject* context, std::function<void(Qt::PermissionStatus)> completion) = 0;
};

}  // namespace skygate::ui::internal
