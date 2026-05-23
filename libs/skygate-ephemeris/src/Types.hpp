#pragma once

#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "SkyContext.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"
#include "time/TimeScale.hpp"

#include <bit>
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

[[nodiscard]] constexpr EphemerisCorrectionFlags
withoutCorrectionFlags(const EphemerisCorrectionFlags flags, const EphemerisCorrectionFlags removedFlags) noexcept
{
    return static_cast<EphemerisCorrectionFlags>(
        static_cast<std::uint32_t>(flags) & ~static_cast<std::uint32_t>(removedFlags)
    );
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

enum class EphemerisResultStatus : std::uint8_t {
    Valid,
    Degraded,
    Unsupported,
    OutOfRange,
    Failed
};

[[nodiscard]] constexpr std::size_t ephemerisResultStatusCount() noexcept
{
    return 5U;
}

[[nodiscard]] constexpr std::string_view displayName(const EphemerisResultStatus status) noexcept
{
    switch (status) {
    case EphemerisResultStatus::Valid:
        return "valid";
    case EphemerisResultStatus::Degraded:
        return "degraded";
    case EphemerisResultStatus::Unsupported:
        return "unsupported";
    case EphemerisResultStatus::OutOfRange:
        return "out of range";
    case EphemerisResultStatus::Failed:
        return "failed";
    }

    return {};
}

enum class EphemerisWarningCode : std::uint8_t {
    AccuracyDegraded,
    UnsupportedBody,
    DataOutOfRange,
    MissingEphemerisData,
    MissingObserver,
    TimeScaleDataUnavailable,
    CorrectionUnavailable,
    ComputationFailed,
    BarycenterFallback
};

[[nodiscard]] constexpr std::string_view ephemerisWarningText(const EphemerisWarningCode code) noexcept
{
    switch (code) {
    case EphemerisWarningCode::AccuracyDegraded:
        return "Result accuracy is degraded for this request.";
    case EphemerisWarningCode::UnsupportedBody:
        return "This body is not supported by the selected ephemeris engine.";
    case EphemerisWarningCode::DataOutOfRange:
        return "The request is outside the effective date range of the available ephemeris data.";
    case EphemerisWarningCode::MissingEphemerisData:
        return "Required ephemeris data is unavailable.";
    case EphemerisWarningCode::MissingObserver:
        return "Observer information is missing or invalid, so topocentric coordinates are unavailable.";
    case EphemerisWarningCode::TimeScaleDataUnavailable:
        return "Required time-scale data is unavailable.";
    case EphemerisWarningCode::CorrectionUnavailable:
        return "One or more requested correction terms could not be applied.";
    case EphemerisWarningCode::ComputationFailed:
        return "The ephemeris computation failed.";
    case EphemerisWarningCode::BarycenterFallback:
        return "A planetary-system barycenter was used because the requested body center is unavailable.";
    }

    return "Ephemeris warning.";
}

struct EphemerisWarning {
    EphemerisWarningCode code = EphemerisWarningCode::AccuracyDegraded;

    EphemerisWarning() = default;

    explicit constexpr EphemerisWarning(const EphemerisWarningCode warningCode) noexcept : code(warningCode) {}

    [[nodiscard]] constexpr std::string_view displayText() const noexcept
    {
        return ephemerisWarningText(code);
    }
};

[[nodiscard]] constexpr std::uint32_t ephemerisWarningMask(const EphemerisWarningCode code) noexcept
{
    return 1U << static_cast<std::uint8_t>(code);
}

struct EphemerisResultMetadata {
    EphemerisResultStatus status = EphemerisResultStatus::Valid;
    std::uint32_t warningCodeMask = 0U;
    std::string dataSourceProvenance;
    std::optional<EphemerisDateRange> effectiveDataValidityRange;
    std::optional<double> estimatedAngularUncertaintyArcsec;
    EphemerisCorrectionFlags requestedCorrections = EphemerisCorrectionFlags::NoCorrections;
    EphemerisCorrectionFlags appliedCorrections = EphemerisCorrectionFlags::NoCorrections;
    EphemerisCorrectionFlags skippedCorrections = EphemerisCorrectionFlags::NoCorrections;
    EphemerisCorrectionFlags unavailableCorrections = EphemerisCorrectionFlags::NoCorrections;

    [[nodiscard]] bool isSuccessful() const noexcept
    {
        return status == EphemerisResultStatus::Valid || status == EphemerisResultStatus::Degraded;
    }

    void addWarning(const EphemerisWarningCode code) noexcept
    {
        warningCodeMask |= ephemerisWarningMask(code);
    }

    void addUnavailableCorrection(const EphemerisCorrectionFlags correction) noexcept
    {
        unavailableCorrections |= correction;
        addWarning(EphemerisWarningCode::CorrectionUnavailable);
    }

    void finalizeCorrectionTracking(const EphemerisCorrectionFlags requested) noexcept
    {
        requestedCorrections = requested;
        const EphemerisCorrectionFlags accountedCorrections = appliedCorrections | unavailableCorrections;
        skippedCorrections = withoutCorrectionFlags(requestedCorrections, accountedCorrections);
    }

    [[nodiscard]] bool hasWarning(const EphemerisWarningCode code) const noexcept
    {
        return (warningCodeMask & ephemerisWarningMask(code)) != 0U;
    }

    [[nodiscard]] std::size_t warningCount() const noexcept
    {
        return static_cast<std::size_t>(std::popcount(warningCodeMask));
    }

    [[nodiscard]] bool hasWarnings() const noexcept
    {
        return warningCodeMask != 0U;
    }
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

struct CatalogStarAstrometry {
    core::EquatorialCoordinate referenceEquatorial;
    AstronomicalEpoch referenceEpoch;
    // Tangent-plane RA proper motion, mu_alpha * cos(delta), in mas/year.
    std::optional<double> properMotionRightAscensionMasPerYear;
    std::optional<double> properMotionDeclinationMasPerYear;
    std::optional<double> stellarParallaxMas;
    std::optional<double> radialVelocityKmPerSecond;
    std::optional<EphemerisDateRange> validityRange;
};

struct CelestialBody {
    std::string id;
    std::string displayName;
    CelestialBodyType type = CelestialBodyType::Star;
    CelestialBodyEphemerisSource ephemerisSource = CelestialBodyEphemerisSource::Unresolved;
    double visualMagnitude = 0.0;
    std::optional<core::EquatorialCoordinate> fixedEquatorial;
    std::optional<CatalogStarAstrometry> starAstrometry;
    std::optional<DeepSkyObjectInfo> deepSkyObject;
};

struct CelestialBodyState {
    std::uint32_t bodyIndex = 0;
    core::EquatorialCoordinate equatorial;
    core::HorizontalCoordinate horizontal;
    EphemerisResultMetadata metadata;
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
