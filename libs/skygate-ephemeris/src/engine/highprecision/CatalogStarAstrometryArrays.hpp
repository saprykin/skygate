#pragma once

#include "EquatorialCoordinate.hpp"
#include "engine/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace skygate::ephemeris {

class BaseCelestialBody;

}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

class CatalogStarAstrometryArrays final {
public:
    CatalogStarAstrometryArrays() = default;
    explicit CatalogStarAstrometryArrays(std::span<const BaseCelestialBody* const> bodies);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] std::span<const std::size_t> bodyIndices() const noexcept;
    [[nodiscard]] std::optional<std::size_t> arrayIndexForBodyIndex(std::size_t bodyIndex) const noexcept;

    [[nodiscard]] bool hasCatalogAstrometry(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] bool hasFixedEquatorialFallback(std::size_t arrayIndex) const noexcept;

    [[nodiscard]] skygate::core::EquatorialCoordinate referenceEquatorial(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] AstronomicalEpoch referenceEpoch(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
    fixedEquatorialFallback(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] std::optional<EphemerisDateRange> validityRange(std::size_t arrayIndex) const;

    [[nodiscard]] std::optional<double> properMotionRightAscensionMasPerYear(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] std::optional<double> properMotionDeclinationMasPerYear(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] std::optional<double> stellarParallaxMas(std::size_t arrayIndex) const noexcept;
    [[nodiscard]] std::optional<double> radialVelocityKmPerSecond(std::size_t arrayIndex) const noexcept;

    [[nodiscard]] std::span<const double> referenceRightAscensionHours() const noexcept;
    [[nodiscard]] std::span<const double> referenceDeclinationDegrees() const noexcept;
    [[nodiscard]] std::span<const double> referenceEpochJulianDatePart1() const noexcept;
    [[nodiscard]] std::span<const double> referenceEpochJulianDatePart2() const noexcept;
    [[nodiscard]] std::span<const TimeScale> referenceEpochTimeScales() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasCatalogAstrometryMask() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasFixedEquatorialFallbackMask() const noexcept;
    [[nodiscard]] std::span<const double> fixedRightAscensionHours() const noexcept;
    [[nodiscard]] std::span<const double> fixedDeclinationDegrees() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasProperMotionRightAscensionMask() const noexcept;
    [[nodiscard]] std::span<const double> properMotionRightAscensionMasPerYearValues() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasProperMotionDeclinationMask() const noexcept;
    [[nodiscard]] std::span<const double> properMotionDeclinationMasPerYearValues() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasStellarParallaxMask() const noexcept;
    [[nodiscard]] std::span<const double> stellarParallaxMasValues() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasRadialVelocityMask() const noexcept;
    [[nodiscard]] std::span<const double> radialVelocityKmPerSecondValues() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> hasValidityRangeMask() const noexcept;
    [[nodiscard]] std::span<const EphemerisDateRange> validityRanges() const noexcept;

private:
    void reserveColumns(std::size_t bodyCount);
    void appendBody(std::size_t bodyIndex, const BaseCelestialBody& body);

    [[nodiscard]] bool hasValueAt(const std::vector<std::uint8_t>& mask, std::size_t arrayIndex) const noexcept;
    [[nodiscard]] std::optional<double> optionalValueAt(
        const std::vector<std::uint8_t>& mask, const std::vector<double>& values, std::size_t arrayIndex
    ) const noexcept;

    std::vector<std::size_t> m_bodyIndices;
    std::vector<std::uint8_t> m_hasCatalogAstrometry;

    std::vector<double> m_referenceRightAscensionHours;
    std::vector<double> m_referenceDeclinationDegrees;
    std::vector<double> m_referenceEpochJulianDatePart1;
    std::vector<double> m_referenceEpochJulianDatePart2;
    std::vector<TimeScale> m_referenceEpochTimeScales;

    std::vector<std::uint8_t> m_hasFixedEquatorialFallback;
    std::vector<double> m_fixedRightAscensionHours;
    std::vector<double> m_fixedDeclinationDegrees;

    std::vector<std::uint8_t> m_hasProperMotionRightAscension;
    std::vector<double> m_properMotionRightAscensionMasPerYear;
    std::vector<std::uint8_t> m_hasProperMotionDeclination;
    std::vector<double> m_properMotionDeclinationMasPerYear;
    std::vector<std::uint8_t> m_hasStellarParallax;
    std::vector<double> m_stellarParallaxMas;
    std::vector<std::uint8_t> m_hasRadialVelocity;
    std::vector<double> m_radialVelocityKmPerSecond;

    std::vector<std::uint8_t> m_hasValidityRange;
    std::vector<EphemerisDateRange> m_validityRanges;
};

}  // namespace skygate::ephemeris::highprecision
