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

[[nodiscard]] QString celestialBodyTypeText(const ephemeris::BaseCelestialBody& body);
[[nodiscard]] QString formatFiniteNumber(double value, int precision);
[[nodiscard]] QString formatMagnitude(double value);
[[nodiscard]] QString formatHorizontalCoordinate(const core::HorizontalCoordinate& horizontal);
[[nodiscard]] QString formatEquatorialCoordinate(const core::EquatorialCoordinate& equatorial);
[[nodiscard]] QString formatUtcTime(const core::UtcTimePoint& utcTime);
[[nodiscard]] QString formatObservationEvent(const ephemeris::ObservationEvent& event);
[[nodiscard]] QString
formatObservationEvent(const ephemeris::ObservationEvent& event, const SkyTimeController* timeController);
[[nodiscard]] QString
formatObservationCulmination(const ephemeris::ObservationEvent& culmination, const SkyTimeController* timeController);
[[nodiscard]] QString angularSizeText(const ephemeris::DeepSkyObjectInfo& deepSkyObject);
[[nodiscard]] QString aliasesText(const ephemeris::BaseCelestialBody& body);
[[nodiscard]] QString formatEphemerisStatus(ephemeris::EphemerisEngineQueryStatus::Type status);
[[nodiscard]] QString formatEphemerisWarnings(const ephemeris::EphemerisEngineQueryResult& metadata);
[[nodiscard]] QString formatEphemerisDateRange(const ephemeris::EphemerisDateRange& range);
[[nodiscard]] QString formatAngularUncertaintyArcsec(double arcsec);
[[nodiscard]] QString formatCorrectionSummary(const ephemeris::EphemerisEngineQueryResult& metadata);
[[nodiscard]] QString sourceLabelForBodyIndex(
    std::span<const std::uint8_t> sourceIds, const QStringList& sourceLabels, std::uint32_t bodyIndex
);

}  // namespace skygate::ui::internal
