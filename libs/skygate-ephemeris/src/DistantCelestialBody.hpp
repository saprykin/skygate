#pragma once

#include "BaseCelestialBody.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"

#include <optional>

namespace skygate::ephemeris {

class DistantCelestialBody : public BaseCelestialBody {
public:
    [[nodiscard]] const std::optional<core::EquatorialCoordinate>& fixedEquatorialValue() const noexcept override;
    [[nodiscard]] const std::optional<DeepSkyObjectInfo>& deepSkyObjectValue() const noexcept override;

    std::optional<core::EquatorialCoordinate> fixedEquatorial;
    std::optional<DeepSkyObjectInfo> deepSkyObject;
};

}  // namespace skygate::ephemeris
