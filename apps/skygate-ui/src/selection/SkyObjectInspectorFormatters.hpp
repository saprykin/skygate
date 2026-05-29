#pragma once

#include "BaseCelestialBody.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "ObservationEvent.hpp"
#include "UtcTimePoint.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <span>

class SkyTimeController;

namespace skygate::ui::internal {

[[nodiscard]] QString celestialBodyTypeText(const skygate::ephemeris::BaseCelestialBody& body);
[[nodiscard]] QString formatFiniteNumber(double value, int precision);
[[nodiscard]] QString formatMagnitude(double value);
[[nodiscard]] QString formatHorizontalCoordinate(const skygate::core::HorizontalCoordinate& horizontal);
[[nodiscard]] QString formatEquatorialCoordinate(const skygate::core::EquatorialCoordinate& equatorial);
[[nodiscard]] QString formatUtcTime(const skygate::core::UtcTimePoint& utcTime);
[[nodiscard]] QString formatObservationEvent(const skygate::ephemeris::ObservationEvent& event);
[[nodiscard]] QString
formatObservationEvent(const skygate::ephemeris::ObservationEvent& event, const SkyTimeController* timeController);
[[nodiscard]] QString formatObservationCulmination(
    const skygate::ephemeris::ObservationEvent& culmination, const SkyTimeController* timeController
);
[[nodiscard]] QString angularSizeText(const skygate::ephemeris::DeepSkyObjectInfo& deepSkyObject);
[[nodiscard]] QString aliasesText(const skygate::ephemeris::BaseCelestialBody& body);
[[nodiscard]] QString formatEphemerisStatus(skygate::ephemeris::EphemerisEngineQueryStatus::Type status);
[[nodiscard]] QString formatEphemerisWarnings(const skygate::ephemeris::EphemerisEngineQueryResult& metadata);
[[nodiscard]] QString formatEphemerisDateRange(const skygate::ephemeris::EphemerisDateRange& range);
[[nodiscard]] QString formatAngularUncertaintyArcsec(double arcsec);
[[nodiscard]] QString formatCorrectionSummary(const skygate::ephemeris::EphemerisEngineQueryResult& metadata);
[[nodiscard]] QString sourceLabelForBodyIndex(
    std::span<const std::uint8_t> sourceIds, const QStringList& sourceLabels, std::uint32_t bodyIndex
);

}  // namespace skygate::ui::internal
