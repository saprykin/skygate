#pragma once

#include "skygate/core/Types.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

// CivilDateTime uses astronomical year numbering; use the historical-year
// helpers below when converting from BCE/CE input that has no year zero.
struct CivilDateTime {
    int astronomicalYear = 2000;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    std::uint32_t nanosecond = 0U;
    TimeScale timeScale = TimeScale::Utc;
};

using AstronomicalEpochResult = std::optional<AstronomicalEpoch>;
using CivilDateTimeResult = std::optional<CivilDateTime>;

namespace detail {

constexpr double kJulianDateUnixEpoch = 2'440'587.5;
constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000LL;
constexpr std::int64_t kSecondsPerDay = 86'400LL;
constexpr std::int64_t kNanosecondsPerDay = kSecondsPerDay * kNanosecondsPerSecond;

[[nodiscard]] constexpr bool isGregorianLeapYear(const int astronomicalYear) noexcept
{
    return astronomicalYear % 4 == 0 && (astronomicalYear % 100 != 0 || astronomicalYear % 400 == 0);
}

[[nodiscard]] constexpr int daysInGregorianMonth(const int astronomicalYear, const int month) noexcept
{
    switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return isGregorianLeapYear(astronomicalYear) ? 29 : 28;
    default:
        return 0;
    }
}

[[nodiscard]] constexpr std::int64_t
daysFromCivilDate(const int astronomicalYear, const int month, const int day) noexcept
{
    int year = astronomicalYear;
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned monthPrime = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned dayOfYear = (153U * monthPrime + 2U) / 5U + static_cast<unsigned>(day) - 1U;
    const unsigned dayOfEra = yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
    return static_cast<std::int64_t>(era) * 146'097LL + static_cast<std::int64_t>(dayOfEra) - 719'468LL;
}

[[nodiscard]] constexpr CivilDateTime civilDateFromDays(const std::int64_t daysSinceUnixEpoch) noexcept
{
    const std::int64_t z = daysSinceUnixEpoch + 719'468LL;
    const std::int64_t era = (z >= 0 ? z : z - 146'096LL) / 146'097LL;
    const unsigned dayOfEra = static_cast<unsigned>(z - era * 146'097LL);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1'460U + dayOfEra / 36'524U - dayOfEra / 146'096U) / 365U;
    const int year = static_cast<int>(yearOfEra) + static_cast<int>(era) * 400;
    const unsigned dayOfYear = dayOfEra - (365U * yearOfEra + yearOfEra / 4U - yearOfEra / 100U);
    const unsigned monthPrime = (5U * dayOfYear + 2U) / 153U;
    const unsigned day = dayOfYear - (153U * monthPrime + 2U) / 5U + 1U;
    const int month = static_cast<int>(monthPrime) + (monthPrime < 10U ? 3 : -9);

    return CivilDateTime{
        .astronomicalYear = year + (month <= 2 ? 1 : 0),
        .month = month,
        .day = static_cast<int>(day),
    };
}

}  // namespace detail

[[nodiscard]] constexpr std::optional<int> astronomicalYearFromHistoricalYear(const int historicalYear) noexcept
{
    if (historicalYear == 0) {
        return std::nullopt;
    }

    if (historicalYear < 0) {
        return historicalYear + 1;
    }

    return historicalYear;
}

[[nodiscard]] constexpr int historicalYearFromAstronomicalYear(const int astronomicalYear) noexcept
{
    if (astronomicalYear <= 0) {
        return astronomicalYear - 1;
    }

    return astronomicalYear;
}

[[nodiscard]] constexpr bool isValidCivilDateTime(const CivilDateTime& dateTime) noexcept
{
    if (dateTime.month < 1 || dateTime.month > 12) {
        return false;
    }
    if (dateTime.day < 1 || dateTime.day > detail::daysInGregorianMonth(dateTime.astronomicalYear, dateTime.month)) {
        return false;
    }
    if (dateTime.hour < 0 || dateTime.hour > 23 || dateTime.minute < 0 || dateTime.minute > 59) {
        return false;
    }
    if (dateTime.second < 0 || dateTime.second > 60) {
        return false;
    }
    if (dateTime.second == 60
        && (dateTime.timeScale != TimeScale::Utc || dateTime.hour != 23 || dateTime.minute != 59)) {
        return false;
    }

    return dateTime.nanosecond < detail::kNanosecondsPerSecond;
}

[[nodiscard]] inline AstronomicalEpoch normalizedAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept
{
    if (!std::isfinite(epoch.julianDatePart1) || !std::isfinite(epoch.julianDatePart2)) {
        return epoch;
    }

    const double part1Whole = std::floor(epoch.julianDatePart1);
    const double part2WithPart1Fraction = (epoch.julianDatePart1 - part1Whole) + epoch.julianDatePart2;
    const double part2Whole = std::floor(part2WithPart1Fraction);
    return AstronomicalEpoch{
        .julianDatePart1 = part1Whole + part2Whole,
        .julianDatePart2 = part2WithPart1Fraction - part2Whole,
        .timeScale = epoch.timeScale,
    };
}

[[nodiscard]] inline AstronomicalEpochResult astronomicalEpochFromCivilDateTime(const CivilDateTime& dateTime) noexcept
{
    if (!isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }
    if (dateTime.second == 60) {
        return std::nullopt;
    }

    const std::int64_t daysSinceUnixEpoch =
        detail::daysFromCivilDate(dateTime.astronomicalYear, dateTime.month, dateTime.day);
    const std::int64_t nanosecondsSinceMidnight =
        ((static_cast<std::int64_t>(dateTime.hour) * 60LL + static_cast<std::int64_t>(dateTime.minute)) * 60LL
         + static_cast<std::int64_t>(dateTime.second))
            * detail::kNanosecondsPerSecond
        + static_cast<std::int64_t>(dateTime.nanosecond);
    const double dayFraction =
        static_cast<double>(nanosecondsSinceMidnight) / static_cast<double>(detail::kNanosecondsPerDay);

    return normalizedAstronomicalEpoch(AstronomicalEpoch{
        .julianDatePart1 = detail::kJulianDateUnixEpoch + static_cast<double>(daysSinceUnixEpoch),
        .julianDatePart2 = dayFraction,
        .timeScale = dateTime.timeScale,
    });
}

[[nodiscard]] inline CivilDateTimeResult civilDateTimeFromAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept
{
    if (!std::isfinite(epoch.julianDatePart1) || !std::isfinite(epoch.julianDatePart2)) {
        return std::nullopt;
    }

    const AstronomicalEpoch normalizedEpoch = normalizedAstronomicalEpoch(epoch);
    const double daysSinceUnixEpochDouble =
        (normalizedEpoch.julianDatePart1 - detail::kJulianDateUnixEpoch) + normalizedEpoch.julianDatePart2;
    if (!std::isfinite(daysSinceUnixEpochDouble)
        || daysSinceUnixEpochDouble < static_cast<double>(std::numeric_limits<std::int64_t>::min())
        || daysSinceUnixEpochDouble > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }

    auto wholeDays = static_cast<std::int64_t>(std::floor(daysSinceUnixEpochDouble));
    double dayFraction = daysSinceUnixEpochDouble - static_cast<double>(wholeDays);
    if (dayFraction < 0.0) {
        --wholeDays;
        dayFraction += 1.0;
    }

    auto nanosecondsSinceMidnight =
        static_cast<std::int64_t>(std::llround(dayFraction * static_cast<double>(detail::kNanosecondsPerDay)));
    if (nanosecondsSinceMidnight >= detail::kNanosecondsPerDay) {
        ++wholeDays;
        nanosecondsSinceMidnight = 0;
    }

    CivilDateTime dateTime = detail::civilDateFromDays(wholeDays);
    dateTime.hour = static_cast<int>(nanosecondsSinceMidnight / (3'600LL * detail::kNanosecondsPerSecond));
    nanosecondsSinceMidnight %= 3'600LL * detail::kNanosecondsPerSecond;
    dateTime.minute = static_cast<int>(nanosecondsSinceMidnight / (60LL * detail::kNanosecondsPerSecond));
    nanosecondsSinceMidnight %= 60LL * detail::kNanosecondsPerSecond;
    dateTime.second = static_cast<int>(nanosecondsSinceMidnight / detail::kNanosecondsPerSecond);
    dateTime.nanosecond = static_cast<std::uint32_t>(nanosecondsSinceMidnight % detail::kNanosecondsPerSecond);
    dateTime.timeScale = normalizedEpoch.timeScale;
    return dateTime;
}

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
    ComputationFailed
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
    EphemerisCorrectionFlags appliedCorrections = EphemerisCorrectionFlags::NoCorrections;

    [[nodiscard]] bool isSuccessful() const noexcept
    {
        return status == EphemerisResultStatus::Valid || status == EphemerisResultStatus::Degraded;
    }

    void addWarning(const EphemerisWarningCode code) noexcept
    {
        warningCodeMask |= ephemerisWarningMask(code);
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
