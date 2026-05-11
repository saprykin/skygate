#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/ObservationEventCalculator.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <span>

class SkyTimeController;

namespace skygate::ui::internal {

[[nodiscard]] QString celestialBodyTypeText(const ephemeris::CelestialBody& body);
[[nodiscard]] QString formatFiniteNumber(double value, int precision);
[[nodiscard]] QString formatMagnitude(double value);
[[nodiscard]] QString formatHorizontalCoordinate(const core::HorizontalCoordinate& horizontal);
[[nodiscard]] QString formatEquatorialCoordinate(const core::EquatorialCoordinate& equatorial);
[[nodiscard]] QString formatUtcTime(const core::UtcTimePoint& utcTime);
[[nodiscard]] QString formatObservationEvent(const ephemeris::ObservationEvent& event);
[[nodiscard]] QString formatObservationEvent(
    const ephemeris::ObservationEvent& event,
    const SkyTimeController* timeController
);
[[nodiscard]] QString formatObservationCulmination(
    const ephemeris::ObservationCulmination& culmination,
    const SkyTimeController* timeController
);
[[nodiscard]] QString angularSizeText(const ephemeris::DeepSkyObjectInfo& deepSkyObject);
[[nodiscard]] QString aliasesText(const ephemeris::CelestialBody& body);
[[nodiscard]] QString sourceLabelForBodyIndex(
    std::span<const std::uint8_t> sourceIds,
    const QStringList& sourceLabels,
    std::uint32_t bodyIndex
);

}  // namespace skygate::ui::internal
