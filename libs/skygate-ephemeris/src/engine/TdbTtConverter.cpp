#include "TdbTtConverter.hpp"
#include "TimeScaleConversionWarningCode.hpp"
#include "math/AngleMath.hpp"
#include "math/PhysicalConstants.hpp"
#include "time/EpochCodec.hpp"
// TdbTtConverter is a shared time-scale service used by both the simple-only
// and high-precision configurations. When the high-precision engine is
// enabled it prefers the ERFA TDB-TT adapter and otherwise falls back to the
// analytic approximation below. This is a deliberate, build-gated crossing of
// the engine boundary; it is tracked by the engine header isolation check and
// is the only non-engine ephemeris source allowed to reach a concrete
// high-precision implementation header.
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include "engine/highprecision/ErfaAstrometry.hpp"
#endif

#include <cmath>
#include <optional>

namespace skygate::ephemeris {

namespace {

using skygate::core::AngleMath;
using skygate::core::EpochCodec;
using skygate::core::PhysicalConstants;

[[nodiscard]] double approximateTdbMinusTtSeconds(const skygate::core::AstronomicalEpoch& terrestrialTime) noexcept
{
    const double daysSinceJ2000 = EpochCodec::daysSinceJ2000(terrestrialTime);
    const double meanAnomalyDegrees = AngleMath::normalizeDegrees(
        PhysicalConstants::kSolarMeanAnomalyDegAtJ2000 + PhysicalConstants::kSolarMeanAnomalyDegPerDay * daysSinceJ2000
    );
    const double meanAnomalyRadians = AngleMath::toRadians(meanAnomalyDegrees);
    return PhysicalConstants::kTdbMinusTtPrincipalCoefficientSeconds * std::sin(meanAnomalyRadians)
           + PhysicalConstants::kTdbMinusTtSecondHarmonicCoefficientSeconds * std::sin(2.0 * meanAnomalyRadians);
}

[[nodiscard]] std::optional<double> tdbMinusTtSeconds(const skygate::core::AstronomicalEpoch& terrestrialTime) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    // The observer is geocentric (elongation, spin-axis distance, and
    // equatorial-plane distance are all zero), so eraDtdb ignores the UT1
    // fraction of day. Pass a neutral zero instead of a TT day fraction that is
    // not valid UT1.
    const std::optional<double> erfaResult =
        skygate::ephemeris::highprecision::ErfaAstrometry::tdbMinusTtSeconds(terrestrialTime, 0.0);
    if (erfaResult.has_value()) {
        return erfaResult;
    }
#endif

    if (!terrestrialTime.isFinite()) {
        return std::nullopt;
    }

    return approximateTdbMinusTtSeconds(terrestrialTime);
}

void addTdbApproximationWarning(TimeScaleConversionResult& result) noexcept
{
    result.status = TimeScaleConversionStatus::Type::Degraded;
    result.addWarning(TimeScaleConversionWarningCode::Type::TdbApproximationApplied);
}

}  // namespace

TimeScaleConversionResult TdbTtConverter::ttToTdb(const skygate::core::AstronomicalEpoch& terrestrialTime)
{
    const std::optional<double> tdbMinusTt = tdbMinusTtSeconds(terrestrialTime);
    if (!tdbMinusTt.has_value()) {
        TimeScaleConversionResult result = TimeScaleConversionResult::failure(
            terrestrialTime, skygate::core::TimeScale::Tdb, "TT to TDB conversion input is invalid."
        );
        result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    TimeScaleConversionResult result = TimeScaleConversionResult::success(
        terrestrialTime.addSeconds(*tdbMinusTt, skygate::core::TimeScale::Tdb),
        TimeScaleConversionStatus::Type::Degraded,
        TimeScaleConversionResult::WarningMask::fromBits(
            TimeScaleConversionResult::WarningMask::bit(TimeScaleConversionWarningCode::Type::TdbApproximationApplied)
        ),
        "TT to TDB conversion used the configured high-precision approximation."
    );
    addTdbApproximationWarning(result);
    return result;
}

TimeScaleConversionResult TdbTtConverter::tdbToTt(const skygate::core::AstronomicalEpoch& tdbEpoch)
{
    skygate::core::AstronomicalEpoch tt = tdbEpoch;
    tt.timeScale = skygate::core::TimeScale::Tt;
    for (int iteration = 0; iteration < 3; ++iteration) {
        const std::optional<double> tdbMinusTt = tdbMinusTtSeconds(tt);
        if (!tdbMinusTt.has_value()) {
            TimeScaleConversionResult result = TimeScaleConversionResult::failure(
                tdbEpoch, skygate::core::TimeScale::Tt, "TDB to TT conversion input is invalid."
            );
            result.addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
            return result;
        }
        tt = tdbEpoch.addSeconds(-*tdbMinusTt, skygate::core::TimeScale::Tt);
    }

    TimeScaleConversionResult result = TimeScaleConversionResult::success(
        tt,
        TimeScaleConversionStatus::Type::Degraded,
        TimeScaleConversionResult::WarningMask::fromBits(
            TimeScaleConversionResult::WarningMask::bit(TimeScaleConversionWarningCode::Type::TdbApproximationApplied)
        ),
        "TDB to TT conversion used the configured high-precision approximation."
    );
    addTdbApproximationWarning(result);
    return result;
}

}  // namespace skygate::ephemeris
