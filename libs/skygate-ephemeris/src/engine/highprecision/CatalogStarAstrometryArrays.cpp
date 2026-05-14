#include "engine/highprecision/CatalogStarAstrometryArrays.hpp"

#include <algorithm>
#include <limits>

namespace skygate::ephemeris::highprecision {
namespace {

[[nodiscard]] bool isCatalogStarBody(const CelestialBody& body) noexcept
{
    return body.type == CelestialBodyType::Star || body.ephemerisSource == CelestialBodyEphemerisSource::Star
           || body.ephemerisSource == CelestialBodyEphemerisSource::FixedEquatorial;
}

[[nodiscard]] double optionalOrQuietNaN(const std::optional<double> value) noexcept
{
    return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

[[nodiscard]] std::uint8_t maskForOptional(const std::optional<double> value) noexcept
{
    return value.has_value() ? 1U : 0U;
}

}  // namespace

CatalogStarAstrometryArrays::CatalogStarAstrometryArrays(const std::span<const CelestialBody> bodies)
{
    m_bodyIndices.reserve(bodies.size());
    m_hasCatalogAstrometry.reserve(bodies.size());
    m_referenceRightAscensionHours.reserve(bodies.size());
    m_referenceDeclinationDegrees.reserve(bodies.size());
    m_referenceEpochJulianDatePart1.reserve(bodies.size());
    m_referenceEpochJulianDatePart2.reserve(bodies.size());
    m_referenceEpochTimeScales.reserve(bodies.size());
    m_hasFixedEquatorialFallback.reserve(bodies.size());
    m_fixedRightAscensionHours.reserve(bodies.size());
    m_fixedDeclinationDegrees.reserve(bodies.size());
    m_hasProperMotionRightAscension.reserve(bodies.size());
    m_properMotionRightAscensionMasPerYear.reserve(bodies.size());
    m_hasProperMotionDeclination.reserve(bodies.size());
    m_properMotionDeclinationMasPerYear.reserve(bodies.size());
    m_hasStellarParallax.reserve(bodies.size());
    m_stellarParallaxMas.reserve(bodies.size());
    m_hasRadialVelocity.reserve(bodies.size());
    m_radialVelocityKmPerSecond.reserve(bodies.size());
    m_hasValidityRange.reserve(bodies.size());
    m_validityRanges.reserve(bodies.size());

    for (std::size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex) {
        const CelestialBody& body = bodies[bodyIndex];
        if (!isCatalogStarBody(body) || (!body.starAstrometry.has_value() && !body.fixedEquatorial.has_value())) {
            continue;
        }

        const CatalogStarAstrometry* astrometry = body.starAstrometry.has_value() ? &*body.starAstrometry : nullptr;
        const core::EquatorialCoordinate referenceEquatorial =
            astrometry != nullptr ? astrometry->referenceEquatorial : *body.fixedEquatorial;
        const AstronomicalEpoch referenceEpoch =
            astrometry != nullptr ? astrometry->referenceEpoch : AstronomicalEpoch{};

        m_bodyIndices.push_back(bodyIndex);
        m_hasCatalogAstrometry.push_back(astrometry != nullptr ? 1U : 0U);
        m_referenceRightAscensionHours.push_back(referenceEquatorial.rightAscensionHours);
        m_referenceDeclinationDegrees.push_back(referenceEquatorial.declinationDeg);
        m_referenceEpochJulianDatePart1.push_back(referenceEpoch.julianDatePart1);
        m_referenceEpochJulianDatePart2.push_back(referenceEpoch.julianDatePart2);
        m_referenceEpochTimeScales.push_back(referenceEpoch.timeScale);

        m_hasFixedEquatorialFallback.push_back(body.fixedEquatorial.has_value() ? 1U : 0U);
        m_fixedRightAscensionHours.push_back(
            body.fixedEquatorial.has_value() ? body.fixedEquatorial->rightAscensionHours
                                             : std::numeric_limits<double>::quiet_NaN()
        );
        m_fixedDeclinationDegrees.push_back(
            body.fixedEquatorial.has_value() ? body.fixedEquatorial->declinationDeg
                                             : std::numeric_limits<double>::quiet_NaN()
        );

        const std::optional<double> properMotionRightAscension =
            astrometry != nullptr ? astrometry->properMotionRightAscensionMasPerYear : std::nullopt;
        const std::optional<double> properMotionDeclination =
            astrometry != nullptr ? astrometry->properMotionDeclinationMasPerYear : std::nullopt;
        const std::optional<double> stellarParallax =
            astrometry != nullptr ? astrometry->stellarParallaxMas : std::nullopt;
        const std::optional<double> radialVelocity =
            astrometry != nullptr ? astrometry->radialVelocityKmPerSecond : std::nullopt;

        m_hasProperMotionRightAscension.push_back(maskForOptional(properMotionRightAscension));
        m_properMotionRightAscensionMasPerYear.push_back(optionalOrQuietNaN(properMotionRightAscension));
        m_hasProperMotionDeclination.push_back(maskForOptional(properMotionDeclination));
        m_properMotionDeclinationMasPerYear.push_back(optionalOrQuietNaN(properMotionDeclination));
        m_hasStellarParallax.push_back(maskForOptional(stellarParallax));
        m_stellarParallaxMas.push_back(optionalOrQuietNaN(stellarParallax));
        m_hasRadialVelocity.push_back(maskForOptional(radialVelocity));
        m_radialVelocityKmPerSecond.push_back(optionalOrQuietNaN(radialVelocity));

        const std::optional<EphemerisDateRange> validityRange =
            astrometry != nullptr ? astrometry->validityRange : std::nullopt;
        m_hasValidityRange.push_back(validityRange.has_value() ? 1U : 0U);
        m_validityRanges.push_back(validityRange.value_or(EphemerisDateRange{}));
    }
}

std::size_t CatalogStarAstrometryArrays::size() const noexcept
{
    return m_bodyIndices.size();
}

bool CatalogStarAstrometryArrays::empty() const noexcept
{
    return m_bodyIndices.empty();
}

std::span<const std::size_t> CatalogStarAstrometryArrays::bodyIndices() const noexcept
{
    return m_bodyIndices;
}

std::optional<std::size_t> CatalogStarAstrometryArrays::arrayIndexForBodyIndex(const std::size_t bodyIndex
) const noexcept
{
    const auto iterator = std::find(m_bodyIndices.begin(), m_bodyIndices.end(), bodyIndex);
    if (iterator == m_bodyIndices.end()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::distance(m_bodyIndices.begin(), iterator));
}

bool CatalogStarAstrometryArrays::hasCatalogAstrometry(const std::size_t arrayIndex) const noexcept
{
    return hasValueAt(m_hasCatalogAstrometry, arrayIndex);
}

bool CatalogStarAstrometryArrays::hasFixedEquatorialFallback(const std::size_t arrayIndex) const noexcept
{
    return hasValueAt(m_hasFixedEquatorialFallback, arrayIndex);
}

core::EquatorialCoordinate CatalogStarAstrometryArrays::referenceEquatorial(const std::size_t arrayIndex) const noexcept
{
    return {
        .rightAscensionHours = m_referenceRightAscensionHours[arrayIndex],
        .declinationDeg = m_referenceDeclinationDegrees[arrayIndex],
    };
}

AstronomicalEpoch CatalogStarAstrometryArrays::referenceEpoch(const std::size_t arrayIndex) const noexcept
{
    return {
        .julianDatePart1 = m_referenceEpochJulianDatePart1[arrayIndex],
        .julianDatePart2 = m_referenceEpochJulianDatePart2[arrayIndex],
        .timeScale = m_referenceEpochTimeScales[arrayIndex],
    };
}

std::optional<core::EquatorialCoordinate>
CatalogStarAstrometryArrays::fixedEquatorialFallback(const std::size_t arrayIndex) const noexcept
{
    if (!hasFixedEquatorialFallback(arrayIndex)) {
        return std::nullopt;
    }

    return core::EquatorialCoordinate{
        .rightAscensionHours = m_fixedRightAscensionHours[arrayIndex],
        .declinationDeg = m_fixedDeclinationDegrees[arrayIndex],
    };
}

std::optional<EphemerisDateRange> CatalogStarAstrometryArrays::validityRange(const std::size_t arrayIndex) const
{
    if (!hasValueAt(m_hasValidityRange, arrayIndex)) {
        return std::nullopt;
    }

    return m_validityRanges[arrayIndex];
}

std::optional<double> CatalogStarAstrometryArrays::properMotionRightAscensionMasPerYear(const std::size_t arrayIndex
) const noexcept
{
    return optionalValueAt(m_hasProperMotionRightAscension, m_properMotionRightAscensionMasPerYear, arrayIndex);
}

std::optional<double> CatalogStarAstrometryArrays::properMotionDeclinationMasPerYear(const std::size_t arrayIndex
) const noexcept
{
    return optionalValueAt(m_hasProperMotionDeclination, m_properMotionDeclinationMasPerYear, arrayIndex);
}

std::optional<double> CatalogStarAstrometryArrays::stellarParallaxMas(const std::size_t arrayIndex) const noexcept
{
    return optionalValueAt(m_hasStellarParallax, m_stellarParallaxMas, arrayIndex);
}

std::optional<double> CatalogStarAstrometryArrays::radialVelocityKmPerSecond(const std::size_t arrayIndex
) const noexcept
{
    return optionalValueAt(m_hasRadialVelocity, m_radialVelocityKmPerSecond, arrayIndex);
}

std::span<const double> CatalogStarAstrometryArrays::referenceRightAscensionHours() const noexcept
{
    return m_referenceRightAscensionHours;
}

std::span<const double> CatalogStarAstrometryArrays::referenceDeclinationDegrees() const noexcept
{
    return m_referenceDeclinationDegrees;
}

std::span<const double> CatalogStarAstrometryArrays::referenceEpochJulianDatePart1() const noexcept
{
    return m_referenceEpochJulianDatePart1;
}

std::span<const double> CatalogStarAstrometryArrays::referenceEpochJulianDatePart2() const noexcept
{
    return m_referenceEpochJulianDatePart2;
}

std::span<const TimeScale> CatalogStarAstrometryArrays::referenceEpochTimeScales() const noexcept
{
    return m_referenceEpochTimeScales;
}

bool CatalogStarAstrometryArrays::hasValueAt(const std::vector<std::uint8_t>& mask, const std::size_t arrayIndex)
    const noexcept
{
    return arrayIndex < mask.size() && mask[arrayIndex] != 0U;
}

std::optional<double> CatalogStarAstrometryArrays::optionalValueAt(
    const std::vector<std::uint8_t>& mask, const std::vector<double>& values, const std::size_t arrayIndex
) const noexcept
{
    if (!hasValueAt(mask, arrayIndex)) {
        return std::nullopt;
    }

    return values[arrayIndex];
}

}  // namespace skygate::ephemeris::highprecision
