#pragma once

#include "BitFlagSet.hpp"
#include "EphemerisCapability.hpp"

#include <cstdint>

namespace skygate::ephemeris {

// Capability bit set with the named capabilities the rest of the code spells
// out.
class EphemerisCapabilities final : public BitFlagSet<EphemerisCapability> {
public:
    constexpr EphemerisCapabilities() noexcept = default;

    constexpr EphemerisCapabilities(const EphemerisCapability capability) noexcept
        : BitFlagSet<EphemerisCapability>(capability)
    {
    }

    explicit constexpr EphemerisCapabilities(const std::uint32_t bits) noexcept : BitFlagSet<EphemerisCapability>(bits)
    {
    }

    constexpr EphemerisCapabilities(const BitFlagSet<EphemerisCapability> capabilities) noexcept
        : BitFlagSet<EphemerisCapability>(capabilities)
    {
    }

    constexpr EphemerisCapabilities& operator=(const BitFlagSet<EphemerisCapability> capabilities) noexcept
    {
        BitFlagSet<EphemerisCapability>::operator=(capabilities);
        return *this;
    }

    using BitFlagSet<EphemerisCapability>::has;

    [[nodiscard]] constexpr Type type() const noexcept
    {
        return static_cast<Type>(bits());
    }

    constexpr operator Type() const noexcept
    {
        return type();
    }

    [[nodiscard]] static constexpr bool
    has(const EphemerisCapabilities capabilities, const EphemerisCapabilities capability) noexcept
    {
        return capabilities.has(capability);
    }

    [[nodiscard]] static constexpr EphemerisCapabilities noCapabilities() noexcept
    {
        return {};
    }

    [[nodiscard]] static constexpr EphemerisCapabilities simpleEngine() noexcept
    {
        return solarSystemBodies() | catalogStars() | topocentricPositions();
    }

    [[nodiscard]] static constexpr EphemerisCapabilities solarSystemBodies() noexcept
    {
        return EphemerisCapability::SolarSystemBodies;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities catalogStars() noexcept
    {
        return EphemerisCapability::CatalogStars;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities topocentricPositions() noexcept
    {
        return EphemerisCapability::TopocentricPositions;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities atmosphericRefraction() noexcept
    {
        return EphemerisCapability::AtmosphericRefraction;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities extendedHistoricalRange() noexcept
    {
        return EphemerisCapability::ExtendedHistoricalRange;
    }
};

}  // namespace skygate::ephemeris
