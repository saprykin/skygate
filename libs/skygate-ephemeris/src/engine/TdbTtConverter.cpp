#include "TdbTtConverter.hpp"
#include "TimeScaleConversionDiagnostics.hpp"
#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/TimeConstants.hpp"
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include "engine/highprecision/ErfaAstrometry.hpp"
#endif

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

using skygate::core::MathConstants;
using skygate::core::PhysicalConstants;
using skygate::core::TimeConstants;

[[nodiscard]] double epochJulianDate(const skygate::core::AstronomicalEpoch& epoch) noexcept
{
    return epoch.julianDatePart1 + epoch.julianDatePart2;
}

[[nodiscard]] double approximateTdbMinusTtSeconds(const skygate::core::AstronomicalEpoch& terrestrialTime) noexcept
{
    const double daysSinceJ2000 = epochJulianDate(terrestrialTime) - TimeConstants::kJulianDateJ2000;
    const double meanAnomalyRadians = std::fmod(
                                          PhysicalConstants::kSolarMeanAnomalyDegAtJ2000
                                              + PhysicalConstants::kSolarMeanAnomalyDegPerDay * daysSinceJ2000,
                                          360.0
                                      )
                                      * MathConstants::kDegreesToRadians;
    return 0.001657 * std::sin(meanAnomalyRadians) + 0.00001385 * std::sin(2.0 * meanAnomalyRadians);
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

[[nodiscard]] TimeScaleConversionResult successResult(
    skygate::core::AstronomicalEpoch epoch,
    const TimeScaleConversionStatus status = TimeScaleConversionStatus::Valid,
    const std::uint32_t warningCodeMask = 0U,
    std::string diagnosticText = {}
)
{
    if (diagnosticText.empty()) {
        diagnosticText = status == TimeScaleConversionStatus::Valid ? "Time-scale conversion succeeded."
                                                                    : "Time-scale conversion degraded.";
    }

    return TimeScaleConversionResult{
        .epoch = epoch.normalized(),
        .status = status,
        .warningCodeMask = warningCodeMask,
        .diagnosticText = std::move(diagnosticText),
    };
}

[[nodiscard]] TimeScaleConversionResult failureResult(
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale,
    std::string diagnosticText
)
{
    TimeScaleConversionResult result;
    result.epoch = epoch;
    result.epoch.timeScale = targetScale;
    result.status = TimeScaleConversionStatus::Failed;
    result.diagnosticText = std::move(diagnosticText);
    return result;
}

void addTdbApproximationWarning(TimeScaleConversionResult& result) noexcept
{
    result.status = TimeScaleConversionStatus::Degraded;
    result.addWarning(TimeScaleConversionWarningCode::TdbApproximationApplied);
}

}  // namespace

TimeScaleConversionResult TdbTtConverter::ttToTdb(const skygate::core::AstronomicalEpoch& terrestrialTime)
{
    const std::optional<double> tdbMinusTt = tdbMinusTtSeconds(terrestrialTime);
    if (!tdbMinusTt.has_value()) {
        TimeScaleConversionResult result =
            failureResult(terrestrialTime, skygate::core::TimeScale::Tdb, "TT to TDB conversion input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    TimeScaleConversionResult result = successResult(
        terrestrialTime.addSeconds(*tdbMinusTt, skygate::core::TimeScale::Tdb),
        TimeScaleConversionStatus::Degraded,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::TdbApproximationApplied),
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
            TimeScaleConversionResult result =
                failureResult(tdbEpoch, skygate::core::TimeScale::Tt, "TDB to TT conversion input is invalid.");
            result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
            return result;
        }
        tt = tdbEpoch.addSeconds(-*tdbMinusTt, skygate::core::TimeScale::Tt);
    }

    TimeScaleConversionResult result = successResult(
        tt,
        TimeScaleConversionStatus::Degraded,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::TdbApproximationApplied),
        "TDB to TT conversion used the configured high-precision approximation."
    );
    addTdbApproximationWarning(result);
    return result;
}

}  // namespace skygate::ephemeris
