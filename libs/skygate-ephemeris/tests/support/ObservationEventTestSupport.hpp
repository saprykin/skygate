#pragma once

#include "EphemerisRequestFactory.hpp"
#include "catalog/CatalogFactory.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace skygate::ephemeris::tests {

// Observation-event search tests model altitude with one sinusoidal daily
// sweep, so they share the same angle constant and fixture context.
inline constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] inline skygate::core::ObservationContext
makeContext(const double latitudeDeg = 47.0, const double longitudeDeg = 8.0)
{
    skygate::core::ObservationContext context;
    context.observer = {.latitudeDeg = latitudeDeg, .longitudeDeg = longitudeDeg, .elevationMeters = 400.0};
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return context;
}

[[nodiscard]] inline OwnGalaxyCelestialBody makeFixedBody(const skygate::core::EquatorialCoordinate& equatorial)
{
    OwnGalaxyCelestialBody body;
    body.id = "target";
    body.displayName = "Target";
    body.kind = BaseCelestialBody::Kind::Star;
    body.visualMagnitude = 1.0;
    body.fixedEquatorial = equatorial;
    return body;
}

[[nodiscard]] inline EphemerisRequest
makeRequest(const skygate::core::ObservationContext& context, const IEphemerisEngine& engine)
{
    return EphemerisRequestFactory::requestFromContext(context, engine.options());
}

[[nodiscard]] inline std::unique_ptr<IEphemerisEngine> makeEngineForBody(const OwnGalaxyCelestialBody& body)
{
    auto catalog = CatalogFactory::createStarCatalogFromBodies(std::vector<OwnGalaxyCelestialBody>{body});
    assert(catalog != nullptr);
    return std::move(EphemerisEngineFactory::create(*catalog).engine);
}

}  // namespace skygate::ephemeris::tests
