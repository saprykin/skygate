#include "CatalogStarAstrometryArrays.hpp"
#include "BaseCelestialBody.hpp"

#include <algorithm>
#include <limits>

namespace skygate::ephemeris::highprecision {
namespace {

[[nodiscard]] bool hasAstrometryArrayInput(const BaseCelestialBody& body) noexcept
{
    return body.isCatalogStar() && (body.starAstrometryValue().has_value() || body.hasFixedEquatorial());
}

[[nodiscard]] double optionalOrQuietNaN(const std::optional<double> value) noexcept
{
    return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

}  // namespace

CatalogStarAstrometryArrays::CatalogStarAstrometryArrays(const std::span<const BaseCelestialBody* const> bodies)
{
    reserveColumns(bodies.size());

    for (std::size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex) {
        const BaseCelestialBody& body = *bodies[bodyIndex];
        if (!hasAstrometryArrayInput(body)) {
            continue;
        }

        appendBody(bodyIndex, body);
    }
}

void CatalogStarAstrometryArrays::reserveColumns(const std::size_t bodyCount)
{
    m_bodyIndices.reserve(bodyCount);
    m_hasCatalogAstrometry.reserve(bodyCount);
    m_referenceRightAscensionHours.reserve(bodyCount);
    m_referenceDeclinationDegrees.reserve(bodyCount);
    m_referenceEpochJulianDatePart1.reserve(bodyCount);
    m_referenceEpochJulianDatePart2.reserve(bodyCount);
    m_referenceEpochTimeScales.reserve(bodyCount);
    m_hasFixedEquatorialFallback.reserve(bodyCount);
    m_fixedRightAscensionHours.reserve(bodyCount);
    m_fixedDeclinationDegrees.reserve(bodyCount);
    m_hasProperMotionRightAscension.reserve(bodyCount);
    m_properMotionRightAscensionMasPerYear.reserve(bodyCount);
    m_hasProperMotionDeclination.reserve(bodyCount);
    m_properMotionDeclinationMasPerYear.reserve(bodyCount);
    m_hasStellarParallax.reserve(bodyCount);
    m_stellarParallaxMas.reserve(bodyCount);
    m_hasRadialVelocity.reserve(bodyCount);
    m_radialVelocityKmPerSecond.reserve(bodyCount);
    m_hasValidityRange.reserve(bodyCount);
    m_validityRanges.reserve(bodyCount);
}

void CatalogStarAstrometryArrays::appendBody(const std::size_t bodyIndex, const BaseCelestialBody& body)
{
    const CatalogStarAstrometry* astrometry =
        body.starAstrometryValue().has_value() ? &*body.starAstrometryValue() : nullptr;
    const skygate::core::EquatorialCoordinate referenceEquatorial =
        astrometry != nullptr ? astrometry->referenceEquatorial : *body.fixedEquatorialValue();
    const skygate::core::AstronomicalEpoch referenceEpoch =
        astrometry != nullptr ? astrometry->referenceEpoch : skygate::core::AstronomicalEpoch{};

    m_bodyIndices.push_back(bodyIndex);
    m_hasCatalogAstrometry.push_back(astrometry != nullptr ? 1U : 0U);
    m_referenceRightAscensionHours.push_back(referenceEquatorial.rightAscensionHours);
    m_referenceDeclinationDegrees.push_back(referenceEquatorial.declinationDeg);
    m_referenceEpochJulianDatePart1.push_back(referenceEpoch.julianDatePart1);
    m_referenceEpochJulianDatePart2.push_back(referenceEpoch.julianDatePart2);
    m_referenceEpochTimeScales.push_back(referenceEpoch.timeScale);

    m_hasFixedEquatorialFallback.push_back(body.hasFixedEquatorial() ? 1U : 0U);
    m_fixedRightAscensionHours.push_back(
        body.hasFixedEquatorial() ? body.fixedEquatorialValue()->rightAscensionHours
                                  : std::numeric_limits<double>::quiet_NaN()
    );
    m_fixedDeclinationDegrees.push_back(
        body.hasFixedEquatorial() ? body.fixedEquatorialValue()->declinationDeg
                                  : std::numeric_limits<double>::quiet_NaN()
    );

    const std::optional<double> properMotionRightAscension =
        astrometry != nullptr ? astrometry->properMotionRightAscensionMasPerYear : std::nullopt;
    const std::optional<double> properMotionDeclination =
        astrometry != nullptr ? astrometry->properMotionDeclinationMasPerYear : std::nullopt;
    const std::optional<double> stellarParallax = astrometry != nullptr ? astrometry->stellarParallaxMas : std::nullopt;
    const std::optional<double> radialVelocity =
        astrometry != nullptr ? astrometry->radialVelocityKmPerSecond : std::nullopt;

    const bool hasAstrometry = astrometry != nullptr;
    m_hasProperMotionRightAscension.push_back(
        (hasAstrometry && astrometry->hasUsableProperMotionRightAscension()) ? 1U : 0U
    );
    m_properMotionRightAscensionMasPerYear.push_back(optionalOrQuietNaN(properMotionRightAscension));
    m_hasProperMotionDeclination.push_back((hasAstrometry && astrometry->hasUsableProperMotionDeclination()) ? 1U : 0U);
    m_properMotionDeclinationMasPerYear.push_back(optionalOrQuietNaN(properMotionDeclination));
    m_hasStellarParallax.push_back((hasAstrometry && astrometry->hasUsableParallax()) ? 1U : 0U);
    m_stellarParallaxMas.push_back(optionalOrQuietNaN(stellarParallax));
    m_hasRadialVelocity.push_back((hasAstrometry && astrometry->hasUsableRadialVelocity()) ? 1U : 0U);
    m_radialVelocityKmPerSecond.push_back(optionalOrQuietNaN(radialVelocity));

    const std::optional<EphemerisDateRange> validityRange =
        astrometry != nullptr ? astrometry->validityRange : std::nullopt;
    m_hasValidityRange.push_back(validityRange.has_value() ? 1U : 0U);
    m_validityRanges.push_back(validityRange.value_or(EphemerisDateRange{}));
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

std::optional<std::size_t>
CatalogStarAstrometryArrays::arrayIndexForBodyIndex(const std::size_t bodyIndex) const noexcept
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

skygate::core::EquatorialCoordinate
CatalogStarAstrometryArrays::referenceEquatorial(const std::size_t arrayIndex) const noexcept
{
    return {
        .rightAscensionHours = m_referenceRightAscensionHours[arrayIndex],
        .declinationDeg = m_referenceDeclinationDegrees[arrayIndex],
    };
}

skygate::core::AstronomicalEpoch
CatalogStarAstrometryArrays::referenceEpoch(const std::size_t arrayIndex) const noexcept
{
    return {
        .julianDatePart1 = m_referenceEpochJulianDatePart1[arrayIndex],
        .julianDatePart2 = m_referenceEpochJulianDatePart2[arrayIndex],
        .timeScale = m_referenceEpochTimeScales[arrayIndex],
    };
}

std::optional<skygate::core::EquatorialCoordinate>
CatalogStarAstrometryArrays::fixedEquatorialFallback(const std::size_t arrayIndex) const noexcept
{
    if (!hasFixedEquatorialFallback(arrayIndex)) {
        return std::nullopt;
    }

    return skygate::core::EquatorialCoordinate{
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

std::optional<double>
CatalogStarAstrometryArrays::properMotionRightAscensionMasPerYear(const std::size_t arrayIndex) const noexcept
{
    return optionalValueAt(m_hasProperMotionRightAscension, m_properMotionRightAscensionMasPerYear, arrayIndex);
}

std::optional<double>
CatalogStarAstrometryArrays::properMotionDeclinationMasPerYear(const std::size_t arrayIndex) const noexcept
{
    return optionalValueAt(m_hasProperMotionDeclination, m_properMotionDeclinationMasPerYear, arrayIndex);
}

std::optional<double> CatalogStarAstrometryArrays::stellarParallaxMas(const std::size_t arrayIndex) const noexcept
{
    return optionalValueAt(m_hasStellarParallax, m_stellarParallaxMas, arrayIndex);
}

std::optional<double>
CatalogStarAstrometryArrays::radialVelocityKmPerSecond(const std::size_t arrayIndex) const noexcept
{
    return optionalValueAt(m_hasRadialVelocity, m_radialVelocityKmPerSecond, arrayIndex);
}

bool CatalogStarAstrometryArrays::hasValueAt(
    const std::vector<std::uint8_t>& mask, const std::size_t arrayIndex
) const noexcept
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
