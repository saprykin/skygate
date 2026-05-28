#include "AtmosphericRefractionCalculator.hpp"
#include "EphemerisMetadataMerge.hpp"
#include "math/AngleMath.hpp"

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

[[nodiscard]] bool isRequested(const EphemerisRequest& request) noexcept
{
    return request.options.enableAtmosphericRefraction()
           && skygate::ephemeris::EphemerisCorrectionFlags::has(
               request.options.correctionFlags(), EphemerisCorrectionFlags::atmosphericRefraction()
           );
}

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
    return std::isfinite(altitudeDeg) && altitudeDeg >= kMinimumModelAltitudeDeg && altitudeDeg < 90.0;
}

[[nodiscard]] bool isBelowModelAltitude(const double altitudeDeg) noexcept
{
    return std::isfinite(altitudeDeg) && altitudeDeg < kMinimumModelAltitudeDeg;
}

void markUnavailable(EphemerisEngineQueryResult& metadata) noexcept
{
    EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::atmosphericRefraction());
}

[[nodiscard]] double pressureScale(const EphemerisEngineOptions& options) noexcept
{
    return options.atmosphericPressureHpa() / 1010.0;
}

[[nodiscard]] double temperatureScale(const EphemerisEngineOptions& options) noexcept
{
    return 283.0 / (273.0 + options.atmosphericTemperatureC());
}

[[nodiscard]] double
refractionCorrectionDegrees(const double altitudeDeg, const EphemerisEngineOptions& options) noexcept
{
    const double modelAltitudeDeg = std::clamp(altitudeDeg, kMinimumModelAltitudeDeg, kMaximumModelAltitudeDeg);
    const double refractionArgumentDeg = modelAltitudeDeg + 10.3 / (modelAltitudeDeg + 5.11);
    const double refractionArcminutes = pressureScale(options) * temperatureScale(options) * 1.02
                                        / std::tan(core::AngleMath::toRadians(refractionArgumentDeg));
    return refractionArcminutes / 60.0;
}

}  // namespace

HighPrecisionCalculatorResult AtmosphericRefractionCalculator::apply(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    HighPrecisionCalculatorResult result = calculatorResult;
    if (!isRequested(input.request)) {
        return result;
    }

    if (!result.horizontal.has_value() || !input.request.context.observer.isValid()
        || !hasValidAtmosphere(input.request.options)) {
        markUnavailable(result.metadata);
        return result;
    }
    if (isBelowModelAltitude(result.horizontal->altitudeDeg)) {
        return result;
    }
    if (!hasModelAltitude(result.horizontal->altitudeDeg)) {
        markUnavailable(result.metadata);
        return result;
    }

    result.horizontal->altitudeDeg = std::min(
        90.0,
        result.horizontal->altitudeDeg
            + refractionCorrectionDegrees(result.horizontal->altitudeDeg, input.request.options)
    );
    result.metadata.appliedCorrections |= EphemerisCorrectionFlags::atmosphericRefraction();
    return result;
}

}  // namespace skygate::ephemeris::highprecision
