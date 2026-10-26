#pragma once

#include "ILocationPermissionGate.hpp"

#include <functional>

namespace skygate::ui::internal {

// Asks the running application for the precise, when-in-use location
// permission.
class SkyLocationPermissionGate final : public ILocationPermissionGate {
public:
    [[nodiscard]] Qt::PermissionStatus status() const override;
    void request(QObject* context, std::function<void(Qt::PermissionStatus)> completion) override;
};

}  // namespace skygate::ui::internal
