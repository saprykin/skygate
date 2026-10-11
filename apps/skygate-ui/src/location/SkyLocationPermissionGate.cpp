#include "SkyLocationPermissionGate.hpp"

#include <QCoreApplication>
#include <QPermission>
#if SKYGATE_HAS_POSITIONING
#include <QLocationPermission>
#endif

#include <utility>

namespace skygate::ui::internal {
namespace {

#if SKYGATE_HAS_POSITIONING
[[nodiscard]] QLocationPermission locationPermission()
{
    QLocationPermission permission;
    permission.setAccuracy(QLocationPermission::Precise);
    permission.setAvailability(QLocationPermission::WhenInUse);
    return permission;
}
#endif

}  // namespace

Qt::PermissionStatus SkyLocationPermissionGate::status() const
{
#if SKYGATE_HAS_POSITIONING
    QCoreApplication* application = QCoreApplication::instance();
    if (application == nullptr) {
        return Qt::PermissionStatus::Denied;
    }

    return application->checkPermission(locationPermission());
#else
    return Qt::PermissionStatus::Denied;
#endif
}

void SkyLocationPermissionGate::request(QObject* context, std::function<void(Qt::PermissionStatus)> completion)
{
#if SKYGATE_HAS_POSITIONING
    QCoreApplication* application = QCoreApplication::instance();
    if (application == nullptr) {
        completion(Qt::PermissionStatus::Denied);
        return;
    }

    application->requestPermission(
        locationPermission(), context, [completion = std::move(completion)](const QPermission& permission) {
            completion(permission.status());
        }
    );
#else
    static_cast<void>(context);
    completion(Qt::PermissionStatus::Denied);
#endif
}

}  // namespace skygate::ui::internal
