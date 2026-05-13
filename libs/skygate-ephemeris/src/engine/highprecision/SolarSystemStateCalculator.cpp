#include "engine/highprecision/SolarSystemStateCalculator.hpp"

#include "StringUtilities.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr int kNaifEarth = 399;
constexpr double kHoursPerRadian = 12.0 / 3.141592653589793238462643383279502884;
constexpr double kDegreesPerRadian = 180.0 / 3.141592653589793238462643383279502884;

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] std::optional<int> naifIdForBody(const CelestialBody& body) noexcept
{
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Sun || body.type == CelestialBodyType::Sun) {
        return 10;
    }
    if (body.ephemerisSource == CelestialBodyEphemerisSource::Moon || body.type == CelestialBodyType::Moon) {
        return 301;
    }
    if (body.ephemerisSource != CelestialBodyEphemerisSource::Planet && body.type != CelestialBodyType::Planet) {
        return std::nullopt;
    }

    if (strings::equalsIgnoreAsciiCase(body.id, "mercury")) {
        return 199;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "venus")) {
        return 299;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "mars")) {
        return 499;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "jupiter")) {
        return 599;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "saturn")) {
        return 699;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "uranus")) {
        return 799;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "neptune")) {
        return 899;
    }
    if (strings::equalsIgnoreAsciiCase(body.id, "pluto")) {
        return 999;
    }

    return std::nullopt;
}

[[nodiscard]] HighPrecisionCalculatorResult
makeStatusResult(const EphemerisResultStatus status, const EphemerisWarningCode warningCode)
{
    HighPrecisionCalculatorResult result;
    result.metadata.status = status;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    return result;
}

[[nodiscard]] std::optional<core::EquatorialCoordinate> equatorialFromVector(const SolarSystemKernelVector& vector
) noexcept
{
    if (!std::isfinite(vector.xAu) || !std::isfinite(vector.yAu) || !std::isfinite(vector.zAu)) {
        return std::nullopt;
    }

    const double xyDistance = std::hypot(vector.xAu, vector.yAu);
    const double distance = std::hypot(xyDistance, vector.zAu);
    if (distance <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    double rightAscensionHours = std::atan2(vector.yAu, vector.xAu) * kHoursPerRadian;
    if (rightAscensionHours < 0.0) {
        rightAscensionHours += 24.0;
    }

    return core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = std::atan2(vector.zAu, xyDistance) * kDegreesPerRadian,
    };
}

}  // namespace

SolarSystemStateCalculator::SolarSystemStateCalculator(std::shared_ptr<const ICalcephKernelProvider> kernelProvider)
    : m_kernelProvider(std::move(kernelProvider))
{
}

HighPrecisionCalculatorResult SolarSystemStateCalculator::calculate(const HighPrecisionComputationInput& input) const
{
    if (!isFiniteEpoch(input.request.epoch)) {
        return makeStatusResult(EphemerisResultStatus::Failed, EphemerisWarningCode::ComputationFailed);
    }
    if (input.request.epoch.timeScale != TimeScale::Tdb) {
        return makeStatusResult(EphemerisResultStatus::Failed, EphemerisWarningCode::TimeScaleDataUnavailable);
    }
    if (m_kernelProvider == nullptr) {
        return makeStatusResult(EphemerisResultStatus::Failed, EphemerisWarningCode::MissingEphemerisData);
    }

    const std::optional<int> targetNaifId = naifIdForBody(input.body);
    if (!targetNaifId.has_value() || *targetNaifId == kNaifEarth) {
        return makeStatusResult(EphemerisResultStatus::Unsupported, EphemerisWarningCode::UnsupportedBody);
    }

    SolarSystemKernelStateResult kernelResult =
        m_kernelProvider->computeGeometricState(input.request.epoch, *targetNaifId, kNaifEarth);
    HighPrecisionCalculatorResult result;
    result.metadata = kernelResult.metadata;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::Geometric;
    if (result.metadata.dataSourceProvenance.empty()) {
        result.metadata.dataSourceProvenance = "CALCEPH geometric solar-system state";
    }

    if (!kernelResult.positionAu.has_value()) {
        if (result.metadata.status == EphemerisResultStatus::Valid) {
            result.metadata.status = EphemerisResultStatus::Failed;
            result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        }
        return result;
    }

    result.equatorial = equatorialFromVector(*kernelResult.positionAu);
    if (!result.equatorial.has_value()) {
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
    }

    return result;
}

}  // namespace skygate::ephemeris::highprecision
