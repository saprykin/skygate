#include "SkyObjectInspectorFormatters.hpp"

#include "SkyTimeController.hpp"

#include <QDateTime>
#include <QTimeZone>

#include <array>
#include <cmath>
#include <string_view>

namespace skygate::ui::internal {

QString celestialBodyTypeText(const ephemeris::CelestialBody& body)
{
    using ephemeris::CelestialBodyType;
    using ephemeris::DeepSkyObjectKind;

    if (body.type == CelestialBodyType::DeepSkyObject && body.deepSkyObject.has_value()) {
        switch (body.deepSkyObject->kind) {
        case DeepSkyObjectKind::Galaxy:
            return "Galaxy";
        case DeepSkyObjectKind::OpenCluster:
            return "Open cluster";
        case DeepSkyObjectKind::GlobularCluster:
            return "Globular cluster";
        case DeepSkyObjectKind::Nebula:
            return "Nebula";
        case DeepSkyObjectKind::PlanetaryNebula:
            return "Planetary nebula";
        case DeepSkyObjectKind::Asterism:
            return "Asterism";
        case DeepSkyObjectKind::Unknown:
            return "Deep sky object";
        }
    }

    switch (body.type) {
    case CelestialBodyType::Sun:
        return "Sun";
    case CelestialBodyType::Moon:
        return "Moon";
    case CelestialBodyType::Planet:
        return "Planet";
    case CelestialBodyType::Star:
        return "Star";
    case CelestialBodyType::Constellation:
        return "Constellation";
    case CelestialBodyType::DeepSkyObject:
        return "Deep sky object";
    }

    return "Object";
}

QString formatFiniteNumber(const double value, const int precision)
{
    return std::isfinite(value) ? QString::number(value, 'f', precision) : QString("--");
}

QString formatMagnitude(const double value)
{
    return formatFiniteNumber(value, 1);
}

QString formatHorizontalCoordinate(const core::HorizontalCoordinate& horizontal)
{
    return QString("%1 / %2 deg")
        .arg(formatFiniteNumber(horizontal.altitudeDeg, 1), formatFiniteNumber(horizontal.azimuthDeg, 1));
}

namespace {

QString formatRightAscension(const double rightAscensionHours)
{
    if (!std::isfinite(rightAscensionHours)) {
        return "--";
    }

    int totalSeconds = static_cast<int>(std::llround(rightAscensionHours * 3600.0));
    totalSeconds %= 24 * 3600;
    if (totalSeconds < 0) {
        totalSeconds += 24 * 3600;
    }

    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;
    return QString("%1h %2m %3s").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

QString formatDeclination(const double declinationDeg)
{
    if (!std::isfinite(declinationDeg)) {
        return "--";
    }

    const QString sign = declinationDeg < 0.0 ? "-" : "+";
    int totalSeconds = static_cast<int>(std::llround(std::abs(declinationDeg) * 3600.0));
    const int degrees = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;
    return QString("%1%2d %3m %4s")
        .arg(sign)
        .arg(degrees)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString formatObservationStatus(const ephemeris::ObservationEventStatus status)
{
    using ephemeris::ObservationEventStatus;

    switch (status) {
    case ObservationEventStatus::Available:
        return {};
    case ObservationEventStatus::AlwaysAbove:
        return "Always above";
    case ObservationEventStatus::AlwaysBelow:
        return "Always below";
    case ObservationEventStatus::NoEventInSearchWindow:
        return "No event in next 72h";
    case ObservationEventStatus::InvalidInput:
    case ObservationEventStatus::Unresolved:
        return "--";
    }

    return "--";
}

QString formatArcmin(const double value)
{
    if (!std::isfinite(value)) {
        return "--";
    }

    const int precision = value >= 10.0 ? 0 : 1;
    return QString("%1 arcmin").arg(QString::number(value, 'f', precision));
}

QString titleCaseAscii(const std::string_view value)
{
    QString text = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    if (!text.isEmpty()) {
        text[0] = text[0].toUpper();
    }
    return text;
}

QString correctionName(const ephemeris::EphemerisCorrectionFlags correction)
{
    using ephemeris::EphemerisCorrectionFlags;

    switch (correction) {
    case EphemerisCorrectionFlags::LightTime:
        return "light-time";
    case EphemerisCorrectionFlags::StellarAberration:
        return "stellar aberration";
    case EphemerisCorrectionFlags::GravitationalLightDeflection:
        return "gravitational deflection";
    case EphemerisCorrectionFlags::AnnualParallax:
        return "annual parallax";
    case EphemerisCorrectionFlags::DiurnalParallax:
        return "diurnal parallax";
    case EphemerisCorrectionFlags::PrecessionNutation:
        return "precession/nutation";
    case EphemerisCorrectionFlags::EarthOrientation:
        return "Earth orientation";
    case EphemerisCorrectionFlags::AtmosphericRefraction:
        return "atmospheric refraction";
    case EphemerisCorrectionFlags::ProperMotion:
        return "proper motion";
    case EphemerisCorrectionFlags::RadialVelocity:
        return "radial velocity";
    case EphemerisCorrectionFlags::StellarParallax:
        return "stellar parallax";
    case EphemerisCorrectionFlags::NoCorrections:
    case EphemerisCorrectionFlags::Astrometric:
    case EphemerisCorrectionFlags::Apparent:
    case EphemerisCorrectionFlags::Topocentric:
    case EphemerisCorrectionFlags::ApparentTopocentric:
        return {};
    }

    return {};
}

QStringList correctionNames(const ephemeris::EphemerisCorrectionFlags flags)
{
    using ephemeris::EphemerisCorrectionFlags;

    constexpr std::array corrections{
        EphemerisCorrectionFlags::LightTime,
        EphemerisCorrectionFlags::StellarAberration,
        EphemerisCorrectionFlags::GravitationalLightDeflection,
        EphemerisCorrectionFlags::AnnualParallax,
        EphemerisCorrectionFlags::DiurnalParallax,
        EphemerisCorrectionFlags::PrecessionNutation,
        EphemerisCorrectionFlags::EarthOrientation,
        EphemerisCorrectionFlags::AtmosphericRefraction,
        EphemerisCorrectionFlags::ProperMotion,
        EphemerisCorrectionFlags::RadialVelocity,
        EphemerisCorrectionFlags::StellarParallax,
    };

    QStringList names;
    for (const EphemerisCorrectionFlags correction : corrections) {
        if (hasCorrectionFlag(flags, correction)) {
            names.push_back(correctionName(correction));
        }
    }
    return names;
}

QString formatDate(const ephemeris::AstronomicalEpoch& epoch)
{
    const auto dateTime = ephemeris::civilDateTimeFromAstronomicalEpoch(epoch);
    if (!dateTime.has_value()) {
        return "--";
    }

    return QString("%1-%2-%3")
        .arg(dateTime->astronomicalYear, 4, 10, QChar('0'))
        .arg(dateTime->month, 2, 10, QChar('0'))
        .arg(dateTime->day, 2, 10, QChar('0'));
}

}  // namespace

QString formatEquatorialCoordinate(const core::EquatorialCoordinate& equatorial)
{
    return QString("%1 / %2").arg(
        formatRightAscension(equatorial.rightAscensionHours), formatDeclination(equatorial.declinationDeg)
    );
}

QString formatUtcTime(const core::UtcTimePoint& utcTime)
{
    return QDateTime::fromSecsSinceEpoch(utcTime.time_since_epoch().count(), QTimeZone::UTC)
        .toString("yyyy-MM-dd HH:mm:ss 'UTC'");
}

QString formatObservationEvent(const ephemeris::ObservationEvent& event)
{
    if (event.status == ephemeris::ObservationEventStatus::Available && event.utcTime.has_value()) {
        return formatUtcTime(*event.utcTime);
    }

    return formatObservationStatus(event.status);
}

QString formatObservationEvent(const ephemeris::ObservationEvent& event, const SkyTimeController* timeController)
{
    if (event.status == ephemeris::ObservationEventStatus::Available && event.utcTime.has_value()
        && timeController != nullptr) {
        return timeController->formatUtcTime(*event.utcTime);
    }

    return formatObservationEvent(event);
}

QString formatObservationCulmination(
    const ephemeris::ObservationCulmination& culmination, const SkyTimeController* timeController
)
{
    if (culmination.status == ephemeris::ObservationEventStatus::Available && culmination.utcTime.has_value()
        && culmination.altitudeDeg.has_value()) {
        return QString("%1 at %2 deg")
            .arg(
                timeController != nullptr ? timeController->formatUtcTime(*culmination.utcTime)
                                          : formatUtcTime(*culmination.utcTime),
                formatFiniteNumber(*culmination.altitudeDeg, 1)
            );
    }

    return formatObservationStatus(culmination.status);
}

QString angularSizeText(const ephemeris::DeepSkyObjectInfo& deepSkyObject)
{
    if (deepSkyObject.majorAxisArcmin.has_value() && deepSkyObject.minorAxisArcmin.has_value()) {
        return QString("%1 x %2").arg(
            formatArcmin(*deepSkyObject.majorAxisArcmin), formatArcmin(*deepSkyObject.minorAxisArcmin)
        );
    }
    if (deepSkyObject.majorAxisArcmin.has_value()) {
        return formatArcmin(*deepSkyObject.majorAxisArcmin);
    }
    if (deepSkyObject.minorAxisArcmin.has_value()) {
        return formatArcmin(*deepSkyObject.minorAxisArcmin);
    }

    return {};
}

QString aliasesText(const ephemeris::CelestialBody& body)
{
    if (!body.deepSkyObject.has_value()) {
        return {};
    }

    QStringList aliases;
    const QString displayName = QString::fromStdString(body.displayName);
    for (const std::string& alias : body.deepSkyObject->aliases) {
        const QString aliasText = QString::fromStdString(alias).trimmed();
        if (aliasText.isEmpty() || aliasText.compare(displayName, Qt::CaseInsensitive) == 0
            || aliases.contains(aliasText, Qt::CaseInsensitive)) {
            continue;
        }

        aliases.push_back(aliasText);
    }

    return aliases.join(", ");
}

QString formatEphemerisStatus(const ephemeris::EphemerisResultStatus status)
{
    return titleCaseAscii(ephemeris::displayName(status));
}

QString formatEphemerisWarnings(const ephemeris::EphemerisResultMetadata& metadata)
{
    using ephemeris::EphemerisWarningCode;

    constexpr std::array warningCodes{
        EphemerisWarningCode::AccuracyDegraded,
        EphemerisWarningCode::UnsupportedBody,
        EphemerisWarningCode::DataOutOfRange,
        EphemerisWarningCode::MissingEphemerisData,
        EphemerisWarningCode::MissingObserver,
        EphemerisWarningCode::TimeScaleDataUnavailable,
        EphemerisWarningCode::CorrectionUnavailable,
        EphemerisWarningCode::ComputationFailed,
    };

    QStringList warnings;
    for (const EphemerisWarningCode code : warningCodes) {
        if (metadata.hasWarning(code)) {
            const std::string_view warningText = ephemeris::ephemerisWarningText(code);
            warnings.push_back(QString::fromUtf8(warningText.data(), static_cast<qsizetype>(warningText.size())));
        }
    }
    return warnings.join("\n");
}

QString formatEphemerisDateRange(const ephemeris::EphemerisDateRange& range)
{
    const QString rangeText = QString("%1 to %2").arg(formatDate(range.start), formatDate(range.end));
    if (range.displayName.empty()) {
        return rangeText;
    }

    return QString("%1: %2").arg(QString::fromStdString(range.displayName), rangeText);
}

QString formatAngularUncertaintyArcsec(const double arcsec)
{
    if (!std::isfinite(arcsec)) {
        return {};
    }

    if (arcsec < 0.01) {
        return QString("%1 mas").arg(QString::number(arcsec * 1000.0, 'f', 1));
    }
    return QString("%1 arcsec").arg(QString::number(arcsec, 'f', arcsec >= 10.0 ? 1 : 2));
}

QString formatCorrectionSummary(const ephemeris::EphemerisResultMetadata& metadata)
{
    QStringList parts;

    const QStringList applied = correctionNames(metadata.appliedCorrections);
    if (!applied.isEmpty()) {
        parts.push_back(QString("Applied: %1").arg(applied.join(", ")));
    }

    const QStringList unavailable = correctionNames(metadata.unavailableCorrections);
    if (!unavailable.isEmpty()) {
        parts.push_back(QString("Unavailable: %1").arg(unavailable.join(", ")));
    }

    const QStringList skipped = correctionNames(metadata.skippedCorrections);
    if (!skipped.isEmpty()) {
        parts.push_back(QString("Skipped: %1").arg(skipped.join(", ")));
    }

    return parts.join("; ");
}

QString sourceLabelForBodyIndex(
    const std::span<const std::uint8_t> sourceIds, const QStringList& sourceLabels, const std::uint32_t bodyIndex
)
{
    if (bodyIndex >= sourceIds.size()) {
        return "Catalog";
    }

    const int sourceId = sourceIds[bodyIndex];
    if (sourceId < 0 || sourceId >= sourceLabels.size()) {
        return "Catalog";
    }

    const QString label = sourceLabels.at(sourceId).trimmed();
    return label.isEmpty() ? QString("Catalog") : label;
}

}  // namespace skygate::ui::internal
