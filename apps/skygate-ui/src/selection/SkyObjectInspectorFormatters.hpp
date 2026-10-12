#pragma once

#include "BaseCelestialBody.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "ObservationEvent.hpp"
#include "UtcTimePoint.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"

#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <span>
#include <vector>

class SkyTimeController;

namespace skygate::ui::internal {

class SkyObjectInspectorFormatters final {
public:
    [[nodiscard]] static QString celestialBodyTypeText(const skygate::ephemeris::BaseCelestialBody& body);
    [[nodiscard]] static QString formatFiniteNumber(double value, int precision);
    [[nodiscard]] static QString formatMagnitude(double value);
    [[nodiscard]] static QString formatHorizontalCoordinate(const skygate::core::HorizontalCoordinate& horizontal);
    [[nodiscard]] static QString formatEquatorialCoordinate(const skygate::core::EquatorialCoordinate& equatorial);
    [[nodiscard]] static QString formatUtcTime(const skygate::core::UtcTimePoint& utcTime);
    [[nodiscard]] static QString formatObservationEvent(const skygate::ephemeris::ObservationEvent& event);
    [[nodiscard]] static QString
    formatObservationEvent(const skygate::ephemeris::ObservationEvent& event, const SkyTimeController* timeController);
    [[nodiscard]] static QString formatObservationCulmination(
        const skygate::ephemeris::ObservationEvent& culmination, const SkyTimeController* timeController
    );
    [[nodiscard]] static QString angularSizeText(const skygate::ephemeris::DeepSkyObjectInfo& deepSkyObject);
    [[nodiscard]] static QString aliasesText(const skygate::ephemeris::BaseCelestialBody& body);
    [[nodiscard]] static QString formatEphemerisStatus(skygate::ephemeris::EphemerisEngineQueryStatus::Type status);
    [[nodiscard]] static QString
    formatEphemerisWarnings(const skygate::ephemeris::EphemerisEngineQueryResult& metadata);
    [[nodiscard]] static QString formatEphemerisDateRange(const skygate::ephemeris::EphemerisDateRange& range);
    [[nodiscard]] static QString formatAngularUncertaintyArcsec(double arcsec);
    [[nodiscard]] static QString
    formatCorrectionSummary(const skygate::ephemeris::EphemerisEngineQueryResult& metadata);
    [[nodiscard]] static QString sourceLabelForBodyIndex(
        std::span<const QString> sourceIds,
        const std::vector<QStringList>* contributorSourceIds,
        const QHash<QString, QString>& sourceTitles,
        std::uint32_t bodyIndex
    );
};

}  // namespace skygate::ui::internal
