#include "BaseCelestialBody.hpp"

namespace skygate::ephemeris {
namespace {

template <typename Value>
[[nodiscard]] const std::optional<Value>& emptyOptionalValue() noexcept
{
    static const std::optional<Value> kEmptyValue;
    return kEmptyValue;
}

template <typename Value>
[[nodiscard]] const Value* optionalPointer(const std::optional<Value>& value) noexcept
{
    return value.has_value() ? &*value : nullptr;
}

}  // namespace

BaseCelestialBody::~BaseCelestialBody() = default;

const std::optional<skygate::core::EquatorialCoordinate>& BaseCelestialBody::fixedEquatorialValue() const noexcept
{
    return fixedEquatorial;
}

const std::optional<CatalogStarAstrometry>& BaseCelestialBody::starAstrometryValue() const noexcept
{
    return emptyOptionalValue<CatalogStarAstrometry>();
}

const std::optional<DeepSkyObjectInfo>& BaseCelestialBody::deepSkyObjectValue() const noexcept
{
    return emptyOptionalValue<DeepSkyObjectInfo>();
}

const skygate::core::EquatorialCoordinate* BaseCelestialBody::fixedEquatorialCoordinate() const noexcept
{
    return optionalPointer(fixedEquatorialValue());
}

const CatalogStarAstrometry* BaseCelestialBody::catalogStarAstrometry() const noexcept
{
    return optionalPointer(starAstrometryValue());
}

const DeepSkyObjectInfo* BaseCelestialBody::deepSkyObjectInfo() const noexcept
{
    return optionalPointer(deepSkyObjectValue());
}

bool BaseCelestialBody::isCatalogStar() const noexcept
{
    return kind == Kind::Star || fixedEquatorialValue().has_value();
}

void BaseCelestialBody::copySharedFieldsTo(BaseCelestialBody& target) const
{
    target.id = id;
    target.identity = identity;
    target.displayName = displayName;
    target.kind = kind;
    target.visualMagnitude = visualMagnitude;
    target.fixedEquatorial = fixedEquatorial;
}

}  // namespace skygate::ephemeris
