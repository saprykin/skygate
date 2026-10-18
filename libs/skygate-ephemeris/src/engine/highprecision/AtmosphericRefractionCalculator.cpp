#include "AtmosphericRefractionCalculator.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "HorizontalCoordinate.hpp"
#include "math/AngleMath.hpp"
#include "math/MathConstants.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr double kMinimumModelAltitudeDeg = -1.0;
constexpr double kMaximumModelAltitudeDeg = 89.9;
constexpr double kMinimumPressureHpa = 0.0;
constexpr double kMaximumPressureHpa = 1200.0;
constexpr double kMinimumTemperatureC = -100.0;
constexpr double kMaximumTemperatureC = 80.0;
constexpr double kMinimumRelativeHumidity = 0.0;
constexpr double kMaximumRelativeHumidity = 1.0;
constexpr double kMinimumWavelengthMicrometers = 0.1;
constexpr double kMaximumWavelengthMicrometers = 100.0;

// Bennett refraction fit for the model altitude band. The correction in
// arcminutes is R = kRefractionScale / tan(h + 10.3 / (h + 5.11)) with the
// altitude h in degrees.
constexpr double kRefractionScale = 1.02;
constexpr double kRefractionArgumentNumeratorDeg = 10.3;
constexpr double kRefractionArgumentOffsetDeg = 5.11;

// The fit is scaled with the actual atmosphere relative to 1010 hPa and
// 283 K (10 degrees Celsius). The denominator keeps the rounded 273 K ice
// point used by the original formulation.
constexpr double kReferencePressureHpa = 1010.0;
constexpr double kReferenceTemperatureKelvin = 283.0;
constexpr double kCelsiusToKelvinOffset = 273.0;

[[nodiscard]] bool hasValidAtmosphere(const EphemerisEngineOptions& options) noexcept
{
    return std::isfinite(options.atmosphericPressureHpa()) && options.atmosphericPressureHpa() > kMinimumPressureHpa
           && options.atmosphericPressureHpa() <= kMaximumPressureHpa
           && std::isfinite(options.atmosphericTemperatureC())
           && options.atmosphericTemperatureC() >= kMinimumTemperatureC
           && options.atmosphericTemperatureC() <= kMaximumTemperatureC && std::isfinite(options.relativeHumidity())
           && options.relativeHumidity() >= kMinimumRelativeHumidity
           && options.relativeHumidity() <= kMaximumRelativeHumidity
           && std::isfinite(options.observingWavelengthMicrometers())
           && options.observingWavelengthMicrometers() >= kMinimumWavelengthMicrometers
           && options.observingWavelengthMicrometers() <= kMaximumWavelengthMicrometers;
}

[[nodiscard]] bool hasModelAltitude(const double altitudeDeg) noexcept
{
    return std::isfinite(altitudeDeg) && altitudeDeg >= kMinimumModelAltitudeDeg
           && altitudeDeg < skygate::core::HorizontalCoordinate::kAltitudeMaxDeg;
}

[[nodiscard]] bool isBelowModelAltitude(const double altitudeDeg) noexcept
{
    return std::isfinite(altitudeDeg) && altitudeDeg < kMinimumModelAltitudeDeg;
}

[[nodiscard]] double pressureScale(const EphemerisEngineOptions& options) noexcept
{
    return options.atmosphericPressureHpa() / kReferencePressureHpa;
}

[[nodiscard]] double temperatureScale(const EphemerisEngineOptions& options) noexcept
{
    return kReferenceTemperatureKelvin / (kCelsiusToKelvinOffset + options.atmosphericTemperatureC());
}

[[nodiscard]] double
refractionCorrectionDegrees(const double altitudeDeg, const EphemerisEngineOptions& options) noexcept
{
    const double modelAltitudeDeg = std::clamp(altitudeDeg, kMinimumModelAltitudeDeg, kMaximumModelAltitudeDeg);
    const double refractionArgumentDeg =
        modelAltitudeDeg + kRefractionArgumentNumeratorDeg / (modelAltitudeDeg + kRefractionArgumentOffsetDeg);
    const double refractionArcminutes = pressureScale(options) * temperatureScale(options) * kRefractionScale
                                        / std::tan(skygate::core::AngleMath::toRadians(refractionArgumentDeg));
    return refractionArcminutes / skygate::core::MathConstants::kArcminutesPerDegree;
}

}  // namespace

HighPrecisionCalculatorResult AtmosphericRefractionCalculator::apply(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    HighPrecisionCalculatorResult result = calculatorResult;
    if (!input.request.options.requestsAtmosphericRefraction()) {
        return result;
    }

    if (!result.horizontal.has_value() || !input.request.context.observer.isValid()
        || !hasValidAtmosphere(input.request.options)) {
        EphemerisMetadataMerger::markCorrectionUnavailable(
            result.metadata, EphemerisCorrectionFlags::atmosphericRefraction()
        );
        return result;
    }
    if (isBelowModelAltitude(result.horizontal->altitudeDeg)) {
        return result;
    }
    if (!hasModelAltitude(result.horizontal->altitudeDeg)) {
        EphemerisMetadataMerger::markCorrectionUnavailable(
            result.metadata, EphemerisCorrectionFlags::atmosphericRefraction()
        );
        return result;
    }

    result.horizontal->altitudeDeg = std::min(
        skygate::core::HorizontalCoordinate::kAltitudeMaxDeg,
        result.horizontal->altitudeDeg
            + refractionCorrectionDegrees(result.horizontal->altitudeDeg, input.request.options)
    );
    EphemerisMetadataMerger::markCorrectionApplied(result.metadata, EphemerisCorrectionFlags::atmosphericRefraction());
    return result;
}

}  // namespace skygate::ephemeris::highprecision
