#include "TestSupport.hpp"
#include "skygate/core/Types.hpp"

#include <limits>

namespace skygate::core::tests {

bool runGeoLocationTests()
{
    bool success = true;

    skygate::core::GeoLocation baseLocation;
    baseLocation.latitudeDeg = 0.0;
    baseLocation.longitudeDeg = 0.0;
    baseLocation.elevationMeters = 0.0;
    success = expectTrue(baseLocation.isValid(), "Origin geolocation should be valid") && success;

    skygate::core::GeoLocation boundaryLocation;
    boundaryLocation.latitudeDeg = -90.0;
    boundaryLocation.longitudeDeg = 180.0;
    boundaryLocation.elevationMeters = -430.0;
    success = expectTrue(boundaryLocation.isValid(), "Boundary latitude/longitude should be valid") && success;

    skygate::core::GeoLocation invalidLatitudeHigh = baseLocation;
    invalidLatitudeHigh.latitudeDeg = 90.01;
    success = expectTrue(!invalidLatitudeHigh.isValid(), "Latitude above 90 should be invalid") && success;

    skygate::core::GeoLocation invalidLatitudeLow = baseLocation;
    invalidLatitudeLow.latitudeDeg = -90.01;
    success = expectTrue(!invalidLatitudeLow.isValid(), "Latitude below -90 should be invalid") && success;

    skygate::core::GeoLocation invalidLongitudeHigh = baseLocation;
    invalidLongitudeHigh.longitudeDeg = 180.01;
    success = expectTrue(!invalidLongitudeHigh.isValid(), "Longitude above 180 should be invalid") && success;

    skygate::core::GeoLocation invalidLongitudeLow = baseLocation;
    invalidLongitudeLow.longitudeDeg = -180.01;
    success = expectTrue(!invalidLongitudeLow.isValid(), "Longitude below -180 should be invalid") && success;

    skygate::core::GeoLocation invalidLatitudeNan = baseLocation;
    invalidLatitudeNan.latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    success = expectTrue(!invalidLatitudeNan.isValid(), "NaN latitude should be invalid") && success;

    skygate::core::GeoLocation invalidLongitudeInf = baseLocation;
    invalidLongitudeInf.longitudeDeg = std::numeric_limits<double>::infinity();
    success = expectTrue(!invalidLongitudeInf.isValid(), "Infinite longitude should be invalid") && success;

    skygate::core::GeoLocation invalidElevationNan = baseLocation;
    invalidElevationNan.elevationMeters = std::numeric_limits<double>::quiet_NaN();
    success = expectTrue(!invalidElevationNan.isValid(), "NaN elevation should be invalid") && success;

    return success;
}

}  // namespace skygate::core::tests
