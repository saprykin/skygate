#pragma once

#include "BitFlagSet.hpp"
#include "EphemerisCorrectionFlag.hpp"

#include <cstdint>

namespace skygate::ephemeris {

// Correction bit set with the named flags the rest of the code spells out.
class EphemerisCorrectionFlags final : public BitFlagSet<EphemerisCorrectionFlag> {
public:
    constexpr EphemerisCorrectionFlags() noexcept = default;

    constexpr EphemerisCorrectionFlags(const EphemerisCorrectionFlag flag) noexcept
        : BitFlagSet<EphemerisCorrectionFlag>(flag)
    {
    }

    explicit constexpr EphemerisCorrectionFlags(const std::uint32_t bits) noexcept
        : BitFlagSet<EphemerisCorrectionFlag>(bits)
    {
    }

    constexpr EphemerisCorrectionFlags(const BitFlagSet<EphemerisCorrectionFlag> flags) noexcept
        : BitFlagSet<EphemerisCorrectionFlag>(flags)
    {
    }

    constexpr EphemerisCorrectionFlags& operator=(const BitFlagSet<EphemerisCorrectionFlag> flags) noexcept
    {
        BitFlagSet<EphemerisCorrectionFlag>::operator=(flags);
        return *this;
    }

    using BitFlagSet<EphemerisCorrectionFlag>::has;
    using BitFlagSet<EphemerisCorrectionFlag>::without;

    [[nodiscard]] constexpr Type type() const noexcept
    {
        return static_cast<Type>(bits());
    }

    constexpr operator Type() const noexcept
    {
        return type();
    }

    // Flags that ask for more than the plain geometric position.
    [[nodiscard]] constexpr bool hasCorrections() const noexcept
    {
        return without(geometric()) != noCorrections();
    }

    [[nodiscard]] static constexpr bool
    has(const EphemerisCorrectionFlags flags, const EphemerisCorrectionFlags flag) noexcept
    {
        return flags.has(flag);
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags
    without(const EphemerisCorrectionFlags flags, const EphemerisCorrectionFlags removedFlags) noexcept
    {
        return flags.without(removedFlags);
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags noCorrections() noexcept
    {
        return {};
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags geometric() noexcept
    {
        return EphemerisCorrectionFlag::Geometric;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags lightTime() noexcept
    {
        return EphemerisCorrectionFlag::LightTime;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags stellarAberration() noexcept
    {
        return EphemerisCorrectionFlag::StellarAberration;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags gravitationalLightDeflection() noexcept
    {
        return EphemerisCorrectionFlag::GravitationalLightDeflection;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags annualParallax() noexcept
    {
        return EphemerisCorrectionFlag::AnnualParallax;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags diurnalParallax() noexcept
    {
        return EphemerisCorrectionFlag::DiurnalParallax;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags precessionNutation() noexcept
    {
        return EphemerisCorrectionFlag::PrecessionNutation;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags earthOrientation() noexcept
    {
        return EphemerisCorrectionFlag::EarthOrientation;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags atmosphericRefraction() noexcept
    {
        return EphemerisCorrectionFlag::AtmosphericRefraction;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags properMotion() noexcept
    {
        return EphemerisCorrectionFlag::ProperMotion;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags radialVelocity() noexcept
    {
        return EphemerisCorrectionFlag::RadialVelocity;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags stellarParallax() noexcept
    {
        return EphemerisCorrectionFlag::StellarParallax;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags astrometric() noexcept
    {
        return EphemerisCorrectionFlag::Astrometric;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags apparent() noexcept
    {
        return EphemerisCorrectionFlag::Apparent;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags topocentric() noexcept
    {
        return EphemerisCorrectionFlag::Topocentric;
    }

    [[nodiscard]] static constexpr EphemerisCorrectionFlags apparentTopocentric() noexcept
    {
        return EphemerisCorrectionFlag::ApparentTopocentric;
    }
};

}  // namespace skygate::ephemeris
