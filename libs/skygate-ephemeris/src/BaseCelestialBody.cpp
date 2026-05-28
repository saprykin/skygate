#include "BaseCelestialBody.hpp"

namespace skygate::ephemeris {
namespace {

const std::optional<core::EquatorialCoordinate> kNoFixedEquatorial;
const std::optional<CatalogStarAstrometry> kNoStarAstrometry;
const std::optional<DeepSkyObjectInfo> kNoDeepSkyObject;

}  // namespace

BaseCelestialBody::~BaseCelestialBody() = default;

const std::optional<core::EquatorialCoordinate>& BaseCelestialBody::fixedEquatorialValue() const noexcept
{
    return kNoFixedEquatorial;
}

const std::optional<CatalogStarAstrometry>& BaseCelestialBody::starAstrometryValue() const noexcept
{
    return kNoStarAstrometry;
}

const std::optional<DeepSkyObjectInfo>& BaseCelestialBody::deepSkyObjectValue() const noexcept
{
    return kNoDeepSkyObject;
}

const core::EquatorialCoordinate* BaseCelestialBody::fixedEquatorialCoordinate() const noexcept
{
    const std::optional<core::EquatorialCoordinate>& value = fixedEquatorialValue();
    return value.has_value() ? &*value : nullptr;
}

const CatalogStarAstrometry* BaseCelestialBody::catalogStarAstrometry() const noexcept
{
    const std::optional<CatalogStarAstrometry>& value = starAstrometryValue();
    return value.has_value() ? &*value : nullptr;
}

const DeepSkyObjectInfo* BaseCelestialBody::deepSkyObjectInfo() const noexcept
{
    const std::optional<DeepSkyObjectInfo>& value = deepSkyObjectValue();
    return value.has_value() ? &*value : nullptr;
}

}  // namespace skygate::ephemeris
