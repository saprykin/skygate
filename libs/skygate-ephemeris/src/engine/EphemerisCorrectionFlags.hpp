#pragma once

#include "BitFlagSetOperations.hpp"

#include <cstdint>

namespace skygate::ephemeris {

class EphemerisCorrectionFlags {
public:
    enum class Type : std::uint32_t {
        NoCorrections = 0U,
        Geometric = 0U,
        LightTime = 1U << 0U,
        StellarAberration = 1U << 1U,
        GravitationalLightDeflection = 1U << 2U,
        AnnualParallax = 1U << 3U,
        DiurnalParallax = 1U << 4U,
        PrecessionNutation = 1U << 5U,
        EarthOrientation = 1U << 6U,
        AtmosphericRefraction = 1U << 7U,
        ProperMotion = 1U << 8U,
        RadialVelocity = 1U << 9U,
        StellarParallax = 1U << 10U,
        Astrometric =
            (1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U) | (1U << 10U),
        Apparent =
            ((1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U) | (1U << 10U)
             | (1U << 6U)),
        Topocentric =
            ((1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U) | (1U << 10U)
             | (1U << 6U) | (1U << 4U)),
        ApparentTopocentric =
            ((1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U) | (1U << 10U)
             | (1U << 6U) | (1U << 4U) | (1U << 7U))
    };

    constexpr EphemerisCorrectionFlags() noexcept = default;

    constexpr EphemerisCorrectionFlags(const Type type) noexcept : m_type(type) {}

    explicit constexpr EphemerisCorrectionFlags(const std::uint32_t bits) noexcept : m_type(static_cast<Type>(bits)) {}

    [[nodiscard]] constexpr Type type() const noexcept
    {
        return m_type;
    }

    [[nodiscard]] constexpr std::uint32_t bits() const noexcept
    {
        return static_cast<std::uint32_t>(m_type);
    }

    explicit constexpr operator std::uint32_t() const noexcept
    {
        return bits();
    }

    constexpr operator Type() const noexcept
    {
        return m_type;
    }

    [[nodiscard]] constexpr EphemerisCorrectionFlags operator|(const EphemerisCorrectionFlags rhs) const noexcept
    {
        return EphemerisCorrectionFlags(bits() | rhs.bits());
    }

    [[nodiscard]] constexpr EphemerisCorrectionFlags operator&(const EphemerisCorrectionFlags rhs) const noexcept
    {
        return EphemerisCorrectionFlags(bits() & rhs.bits());
    }

    constexpr EphemerisCorrectionFlags& operator|=(const EphemerisCorrectionFlags rhs) noexcept
    {
        *this = *this | rhs;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const EphemerisCorrectionFlags rhs) const noexcept
    {
        return bits() == rhs.bits();
    }

    [[nodiscard]] constexpr bool operator!=(const EphemerisCorrectionFlags rhs) const noexcept
    {
        return !(*this == rhs);
    }

    [[nodiscard]] constexpr bool has(const EphemerisCorrectionFlags flag) const noexcept
    {
        return BitFlagSetOperations<EphemerisCorrectionFlags>::hasFlag(*this, flag);
    }

    [[nodiscard]] constexpr EphemerisCorrectionFlags without(const EphemerisCorrectionFlags removedFlags) const noexcept
    {
        return BitFlagSetOperations<EphemerisCorrectionFlags>::withoutFlags(*this, removedFlags);
    }

    [[nodiscard]] static constexpr bool
    has(const EphemerisCorrectionFlags flags, const EphemerisCorrectionFlags flag) noexcept
    {
        return BitFlagSetOperations<EphemerisCorrectionFlags>::hasFlag(flags, flag);
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags
    without(const EphemerisCorrectionFlags flags, const EphemerisCorrectionFlags removedFlags) noexcept
    {
        return BitFlagSetOperations<EphemerisCorrectionFlags>::withoutFlags(flags, removedFlags);
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags noCorrections() noexcept
    {
        return Type::NoCorrections;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags geometric() noexcept
    {
        return Type::Geometric;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags lightTime() noexcept
    {
        return Type::LightTime;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags stellarAberration() noexcept
    {
        return Type::StellarAberration;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags gravitationalLightDeflection() noexcept
    {
        return Type::GravitationalLightDeflection;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags annualParallax() noexcept
    {
        return Type::AnnualParallax;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags diurnalParallax() noexcept
    {
        return Type::DiurnalParallax;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags precessionNutation() noexcept
    {
        return Type::PrecessionNutation;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags earthOrientation() noexcept
    {
        return Type::EarthOrientation;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags atmosphericRefraction() noexcept
    {
        return Type::AtmosphericRefraction;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags properMotion() noexcept
    {
        return Type::ProperMotion;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags radialVelocity() noexcept
    {
        return Type::RadialVelocity;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags stellarParallax() noexcept
    {
        return Type::StellarParallax;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags astrometric() noexcept
    {
        return Type::Astrometric;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags apparent() noexcept
    {
        return Type::Apparent;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags topocentric() noexcept
    {
        return Type::Topocentric;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags apparentTopocentric() noexcept
    {
        return Type::ApparentTopocentric;
    }

private:
    Type m_type = Type::NoCorrections;
};

}  // namespace skygate::ephemeris
