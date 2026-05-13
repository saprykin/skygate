#pragma once

#include "skygate/core/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

enum class EphemerisEngineKind : std::uint8_t {
    Simple,
    HighPrecision
};

enum class TimeScale : std::uint8_t {
    Utc,
    Tai,
    Tt,
    Tdb,
    Ut1
};

struct AstronomicalEpoch {
    double julianDatePart1 = 0.0;
    double julianDatePart2 = 0.0;
    TimeScale timeScale = TimeScale::Utc;
};

enum class EphemerisCorrectionFlags : std::uint32_t {
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

[[nodiscard]] constexpr EphemerisCorrectionFlags
operator|(const EphemerisCorrectionFlags lhs, const EphemerisCorrectionFlags rhs) noexcept
{
    return static_cast<EphemerisCorrectionFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr EphemerisCorrectionFlags
operator&(const EphemerisCorrectionFlags lhs, const EphemerisCorrectionFlags rhs) noexcept
{
    return static_cast<EphemerisCorrectionFlags>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr EphemerisCorrectionFlags&
operator|=(EphemerisCorrectionFlags& lhs, const EphemerisCorrectionFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr bool
hasCorrectionFlag(const EphemerisCorrectionFlags flags, const EphemerisCorrectionFlags flag) noexcept
{
    return (flags & flag) != EphemerisCorrectionFlags::NoCorrections;
}

struct EphemerisEngineOptions {
    EphemerisEngineKind engineKind = EphemerisEngineKind::Simple;
    EphemerisCorrectionFlags correctionFlags = EphemerisCorrectionFlags::ApparentTopocentric;
    bool fallbackToSimpleEngine = true;
    bool enableAtmosphericRefraction = true;
    double atmosphericPressureHpa = 1013.25;
    double atmosphericTemperatureC = 10.0;
    double relativeHumidity = 0.0;
    double observingWavelengthMicrometers = 0.55;
};

struct EphemerisCapabilities {
    EphemerisEngineKind engineKind = EphemerisEngineKind::Simple;
    EphemerisCorrectionFlags supportedCorrections = EphemerisCorrectionFlags::NoCorrections;
    bool supportsSolarSystemBodies = false;
    bool supportsCatalogStars = false;
    bool supportsTopocentricPositions = false;
    bool supportsAtmosphericRefraction = false;
    bool supportsExtendedHistoricalRange = false;
};

struct EphemerisDateRange {
    std::string id;
    std::string displayName;
    AstronomicalEpoch start;
    AstronomicalEpoch end;
};

struct EphemerisDataSetInfo {
    std::string id;
    std::string displayName;
    std::string version;
    std::string provenance;
    std::vector<EphemerisDateRange> dateRanges;
};

struct EphemerisRequest {
    AstronomicalEpoch epoch;
    core::SkyContext context;
    EphemerisEngineOptions options;
};

[[nodiscard]] constexpr std::size_t ephemerisEngineKindCount() noexcept
{
    return 2U;
}

[[nodiscard]] constexpr std::string_view displayName(const EphemerisEngineKind kind) noexcept
{
    switch (kind) {
    case EphemerisEngineKind::Simple:
        return "Simple";
    case EphemerisEngineKind::HighPrecision:
        return "High precision";
    }

    return {};
}

enum class CelestialBodyType : std::uint8_t {
    Star,
    Planet,
    Moon,
    Sun,
    Constellation,
    DeepSkyObject
};

enum class CelestialBodyEphemerisSource : std::uint8_t {
    Unresolved,
    FixedEquatorial,
    Sun,
    Moon,
    Planet,
    Star,
    Constellation
};

enum class DeepSkyObjectKind : std::uint8_t {
    Unknown,
    Galaxy,
    OpenCluster,
    GlobularCluster,
    Nebula,
    PlanetaryNebula,
    Asterism
};

struct DeepSkyObjectInfo {
    DeepSkyObjectKind kind = DeepSkyObjectKind::Unknown;
    std::vector<std::string> aliases;
    std::optional<double> majorAxisArcmin;
    std::optional<double> minorAxisArcmin;
    std::optional<double> positionAngleDeg;
};

struct CelestialBody {
    std::string id;
    std::string displayName;
    CelestialBodyType type = CelestialBodyType::Star;
    CelestialBodyEphemerisSource ephemerisSource = CelestialBodyEphemerisSource::Unresolved;
    double visualMagnitude = 0.0;
    std::optional<core::EquatorialCoordinate> fixedEquatorial;
    std::optional<DeepSkyObjectInfo> deepSkyObject;
};

struct CelestialBodyState {
    std::uint32_t bodyIndex = 0;
    core::EquatorialCoordinate equatorial;
    core::HorizontalCoordinate horizontal;
};

struct SkySnapshot {
    core::SkyContext context;
    std::shared_ptr<const std::vector<CelestialBody>> catalogBodies;
    std::vector<CelestialBodyState> states;

    [[nodiscard]] std::span<const CelestialBody> bodies() const noexcept
    {
        if (catalogBodies == nullptr) {
            return {};
        }

        return std::span<const CelestialBody>(*catalogBodies);
    }

    [[nodiscard]] const CelestialBody& bodyAt(const std::size_t bodyIndex) const noexcept
    {
        return (*catalogBodies)[bodyIndex];
    }
};

}  // namespace skygate::ephemeris
