#pragma once

#include <cstdint>

namespace skygate::ephemeris {

class EphemerisCapabilities {
public:
    enum class Type : std::uint8_t {
        NoCapabilities = 0U,
        SolarSystemBodies = 1U << 0U,
        CatalogStars = 1U << 1U,
        TopocentricPositions = 1U << 2U,
        AtmosphericRefraction = 1U << 3U,
        ExtendedHistoricalRange = 1U << 4U
    };

    constexpr EphemerisCapabilities() noexcept = default;

    constexpr EphemerisCapabilities(const Type type) noexcept : m_type(type) {}

    explicit constexpr EphemerisCapabilities(const std::uint8_t bits) noexcept : m_type(static_cast<Type>(bits)) {}

    [[nodiscard]] constexpr Type type() const noexcept
    {
        return m_type;
    }

    [[nodiscard]] constexpr std::uint8_t bits() const noexcept
    {
        return static_cast<std::uint8_t>(m_type);
    }

    explicit constexpr operator std::uint8_t() const noexcept
    {
        return bits();
    }

    constexpr operator Type() const noexcept
    {
        return m_type;
    }

    [[nodiscard]] constexpr EphemerisCapabilities operator|(const EphemerisCapabilities rhs) const noexcept
    {
        return EphemerisCapabilities(static_cast<std::uint8_t>(bits() | rhs.bits()));
    }

    [[nodiscard]] constexpr EphemerisCapabilities operator&(const EphemerisCapabilities rhs) const noexcept
    {
        return EphemerisCapabilities(static_cast<std::uint8_t>(bits() & rhs.bits()));
    }

    constexpr EphemerisCapabilities& operator|=(const EphemerisCapabilities rhs) noexcept
    {
        *this = *this | rhs;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const EphemerisCapabilities rhs) const noexcept
    {
        return bits() == rhs.bits();
    }

    [[nodiscard]] constexpr bool operator!=(const EphemerisCapabilities rhs) const noexcept
    {
        return !(*this == rhs);
    }

    [[nodiscard]] constexpr bool has(const EphemerisCapabilities capability) const noexcept
    {
        return (*this & capability) != noCapabilities();
    }

    [[nodiscard]] static constexpr bool
    has(const EphemerisCapabilities capabilities, const EphemerisCapabilities capability) noexcept
    {
        return capabilities.has(capability);
    }

    [[nodiscard]] static constexpr EphemerisCapabilities noCapabilities() noexcept
    {
        return Type::NoCapabilities;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities solarSystemBodies() noexcept
    {
        return Type::SolarSystemBodies;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities catalogStars() noexcept
    {
        return Type::CatalogStars;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities topocentricPositions() noexcept
    {
        return Type::TopocentricPositions;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities atmosphericRefraction() noexcept
    {
        return Type::AtmosphericRefraction;
    }

    [[nodiscard]] static constexpr EphemerisCapabilities extendedHistoricalRange() noexcept
    {
        return Type::ExtendedHistoricalRange;
    }

private:
    Type m_type = Type::NoCapabilities;
};

}  // namespace skygate::ephemeris
