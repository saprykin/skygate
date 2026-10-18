#include "SkyLocationStatusText.hpp"

namespace skygate::ui::internal {

QString SkyLocationStatusText::currentDevice()
{
    return "Location: Current device";
}

QString SkyLocationStatusText::city()
{
    return "Location: City";
}

QString SkyLocationStatusText::cityNamed(const QString& cityDisplayText)
{
    return QString("Location: City - %1").arg(cityDisplayText);
}

QString SkyLocationStatusText::customCoordinates()
{
    return "Location: Custom coordinates";
}

QString SkyLocationStatusText::usingCurrentLocation()
{
    return "Location: Using current location";
}

QString SkyLocationStatusText::initializing()
{
    return "Location: Initializing";
}

QString SkyLocationStatusText::deviceSourceUnavailable()
{
    return "Location: Device source unavailable";
}

QString SkyLocationStatusText::positioningUnavailable()
{
    return "Location: Positioning unavailable";
}

QString SkyLocationStatusText::waitingForPermission()
{
    return "Location: Waiting for permission";
}

QString SkyLocationStatusText::permissionDenied()
{
    return "Location: Permission denied";
}

QString SkyLocationStatusText::locating()
{
    return "Location: Locating";
}

QString SkyLocationStatusText::updateFailed()
{
    return "Location: Update failed";
}

}  // namespace skygate::ui::internal
