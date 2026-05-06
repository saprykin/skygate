#include "SkySelectionOverlayBuilder.hpp"

#include "SkySceneShared.hpp"
#include "SkyTimeController.hpp"

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/SphericalGeometry.hpp"
#include "skygate/ephemeris/ObservationEventCalculator.hpp"

#include <QDateTime>
#include <QPointF>
#include <QTimeZone>
#include <QVariantList>

#include <algorithm>
#include <cmath>

namespace {

QVariantMap selectionMarkerEntry(const double x, const double y)
{
    QVariantMap entry;
    entry.insert("kind", "searchSelection");
    entry.insert("x", x);
    entry.insert("y", y);
    return entry;
}

QVariantMap inspectorField(const QString& label, const QString& value)
{
    QVariantMap field;
    field.insert("label", label);
    field.insert("value", value);
    return field;
}

std::optional<QPointF> selectedBodyPoint(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QHash<QString, std::size_t>& stateIndexByBodyId,
    const skygate::core::PreparedProjection& preparedProjection,
    const QString& targetId
)
{
    const auto stateIndexIt = stateIndexByBodyId.constFind(normalizedSceneLookupKey(targetId));
    if (stateIndexIt == stateIndexByBodyId.cend()) {
        return std::nullopt;
    }

    const auto& state = snapshot.states.at(*stateIndexIt);
    if (!state.horizontal.isFinite()) {
        return std::nullopt;
    }

    const auto projected = preparedProjection.project(state.horizontal);
    if (!projected.isVisible) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

std::optional<QPointF> selectedConstellationPoint(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QHash<QString, std::size_t>& stateIndexByBodyId,
    const skygate::core::PreparedProjection& preparedProjection,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
    const QString& targetId
)
{
    const QString normalizedTargetId = normalizedSceneLookupKey(targetId);
    if (normalizedTargetId.isEmpty()) {
        return std::nullopt;
    }

    const auto labelRefIt = std::find_if(
        labelRefs.begin(),
        labelRefs.end(),
        [&normalizedTargetId](const skygate::ephemeris::ConstellationLabelRef& labelRef) {
            return normalizedSceneLookupKey(labelRef.first) == normalizedTargetId;
        }
    );
    if (labelRefIt == labelRefs.end()) {
        return std::nullopt;
    }

    skygate::core::SphericalGeometry::Vector3d sum {0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : labelRefIt->second) {
        const auto stateIndexIt = stateIndexByBodyId.constFind(normalizedSceneLookupKey(hipId));
        if (stateIndexIt == stateIndexByBodyId.cend()) {
            continue;
        }

        const auto& horizontal = snapshot.states.at(*stateIndexIt).horizontal;
        if (!horizontal.isFinite()) {
            continue;
        }

        const auto vector = skygate::core::SphericalGeometry::horizontalToUnitVector(horizontal);
        sum[0] += vector[0];
        sum[1] += vector[1];
        sum[2] += vector[2];
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = skygate::core::SphericalGeometry::normalize(sum);
    if (skygate::core::SphericalGeometry::length(normalizedVector) <= 0.0) {
        return std::nullopt;
    }

    const skygate::core::HorizontalCoordinate center {
        .altitudeDeg = skygate::core::AngleMath::toDegrees(std::asin(std::clamp(
            normalizedVector[2],
            -1.0,
            1.0
        ))),
        .azimuthDeg = skygate::core::AngleMath::normalizeDegrees(
            skygate::core::AngleMath::toDegrees(
                std::atan2(normalizedVector[0], normalizedVector[1])
            )
        )
    };
    const auto projected = preparedProjection.project(center);
    if (!projected.isVisible) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

QString celestialBodyTypeText(const skygate::ephemeris::CelestialBody& body)
{
    using skygate::ephemeris::CelestialBodyType;
    using skygate::ephemeris::DeepSkyObjectKind;

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
    return std::isfinite(value)
        ? QString::number(value, 'f', precision)
        : QString("--");
}

QString formatMagnitude(const double value)
{
    return formatFiniteNumber(value, 1);
}

QString formatHorizontalCoordinate(const skygate::core::HorizontalCoordinate& horizontal)
{
    return QString("%1 / %2 deg").arg(
        formatFiniteNumber(horizontal.altitudeDeg, 1),
        formatFiniteNumber(horizontal.azimuthDeg, 1)
    );
}

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
    return QString("%1h %2m %3s")
        .arg(hours)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
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

QString formatEquatorialCoordinate(const skygate::core::EquatorialCoordinate& equatorial)
{
    return QString("%1 / %2").arg(
        formatRightAscension(equatorial.rightAscensionHours),
        formatDeclination(equatorial.declinationDeg)
    );
}

QString formatUtcTime(const skygate::core::UtcTimePoint& utcTime)
{
    return QDateTime::fromSecsSinceEpoch(
        utcTime.time_since_epoch().count(),
        QTimeZone::UTC
    ).toString("yyyy-MM-dd HH:mm:ss 'UTC'");
}

QString formatObservationStatus(
    const skygate::ephemeris::ObservationEventStatus status
)
{
    using skygate::ephemeris::ObservationEventStatus;

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

QString formatObservationEvent(const skygate::ephemeris::ObservationEvent& event)
{
    if (
        event.status == skygate::ephemeris::ObservationEventStatus::Available
        && event.utcTime.has_value()
    ) {
        return formatUtcTime(*event.utcTime);
    }

    return formatObservationStatus(event.status);
}

QString formatObservationEvent(
    const skygate::ephemeris::ObservationEvent& event,
    const SkyTimeController* timeController
)
{
    if (
        event.status == skygate::ephemeris::ObservationEventStatus::Available
        && event.utcTime.has_value()
        && timeController != nullptr
    ) {
        return timeController->formatUtcTime(*event.utcTime);
    }

    return formatObservationEvent(event);
}

QString formatObservationCulmination(
    const skygate::ephemeris::ObservationCulmination& culmination,
    const SkyTimeController* timeController
)
{
    if (
        culmination.status == skygate::ephemeris::ObservationEventStatus::Available
        && culmination.utcTime.has_value()
        && culmination.altitudeDeg.has_value()
    ) {
        return QString("%1 at %2 deg").arg(
            timeController != nullptr
                ? timeController->formatUtcTime(*culmination.utcTime)
                : formatUtcTime(*culmination.utcTime),
            formatFiniteNumber(*culmination.altitudeDeg, 1)
        );
    }

    return formatObservationStatus(culmination.status);
}

void appendObservationEventFields(
    QVariantList& fields,
    const SkySelectionOverlayInput& input,
    const skygate::ephemeris::CelestialBody& body,
    const std::uint32_t bodyIndex
)
{
    if (input.ephemerisEngine == nullptr || !input.skyContext.has_value()) {
        return;
    }

    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto events = calculator.compute(
        *input.ephemerisEngine,
        *input.skyContext,
        bodyIndex,
        body
    );
    fields.push_back(inspectorField(
        "Rise",
        formatObservationEvent(events.nextRise, input.timeController)
    ));
    fields.push_back(inspectorField(
        "Set",
        formatObservationEvent(events.nextSet, input.timeController)
    ));
    fields.push_back(
        inspectorField(
            "Culmination",
            formatObservationCulmination(events.culmination, input.timeController)
        )
    );
}

QString formatArcmin(const double value)
{
    if (!std::isfinite(value)) {
        return "--";
    }

    const int precision = value >= 10.0 ? 0 : 1;
    return QString("%1 arcmin").arg(QString::number(value, 'f', precision));
}

QString angularSizeText(const skygate::ephemeris::DeepSkyObjectInfo& deepSkyObject)
{
    if (deepSkyObject.majorAxisArcmin.has_value() && deepSkyObject.minorAxisArcmin.has_value()) {
        return QString("%1 x %2").arg(
            formatArcmin(*deepSkyObject.majorAxisArcmin),
            formatArcmin(*deepSkyObject.minorAxisArcmin)
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

QString aliasesText(const skygate::ephemeris::CelestialBody& body)
{
    if (!body.deepSkyObject.has_value()) {
        return {};
    }

    QStringList aliases;
    const QString displayName = QString::fromStdString(body.displayName);
    for (const std::string& alias : body.deepSkyObject->aliases) {
        const QString aliasText = QString::fromStdString(alias).trimmed();
        if (
            aliasText.isEmpty()
            || aliasText.compare(displayName, Qt::CaseInsensitive) == 0
            || aliases.contains(aliasText, Qt::CaseInsensitive)
        ) {
            continue;
        }

        aliases.push_back(aliasText);
    }

    return aliases.join(", ");
}

QString sourceLabelForBodyIndex(
    const std::span<const std::uint8_t> sourceIds,
    const QStringList& sourceLabels,
    const std::uint32_t bodyIndex
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

bool hasSelectionInputs(const SkySelectionOverlayInput& input)
{
    return input.snapshot != nullptr
        && input.preparedProjection != nullptr
        && input.stateIndexByBodyId != nullptr;
}

}  // namespace

QVariantMap SkySelectionOverlayBuilder::buildSelectionMarker(
    const SkySelectionOverlayInput& input
) const
{
    if (!hasSelectionInputs(input)) {
        return {};
    }

    QString targetKind;
    QString targetId;
    if (!input.selectedObjectTargetId.isEmpty()) {
        targetKind = "body";
        targetId = input.selectedObjectTargetId;
    } else if (!input.trackedTargetId.trimmed().isEmpty()) {
        targetKind = input.trackedTargetKind;
        targetId = input.trackedTargetId;
    } else {
        targetKind = input.selectedSearchTargetKind;
        targetId = input.selectedSearchTargetId;
    }
    if (targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return {};
    }

    std::optional<QPointF> markerPoint;
    if (normalizedSceneLookupKey(targetKind) == "body") {
        markerPoint = selectedBodyPoint(
            *input.snapshot,
            *input.stateIndexByBodyId,
            *input.preparedProjection,
            targetId
        );
    } else if (normalizedSceneLookupKey(targetKind) == "constellationlabel") {
        markerPoint = selectedConstellationPoint(
            *input.snapshot,
            *input.stateIndexByBodyId,
            *input.preparedProjection,
            input.constellationLabelRefs,
            targetId
        );
    }

    if (!markerPoint.has_value()) {
        return {};
    }

    return selectionMarkerEntry(markerPoint->x(), markerPoint->y());
}

QVariantMap SkySelectionOverlayBuilder::buildSelectedObjectInspector(
    const SkySelectionOverlayInput& input
) const
{
    if (!hasSelectionInputs(input)) {
        return {};
    }

    const QString targetId = !input.selectedObjectTargetId.isEmpty()
        ? input.selectedObjectTargetId
        : (normalizedSceneLookupKey(input.selectedSearchTargetKind) == "body"
            ? input.selectedSearchTargetId
            : QString());
    if (targetId.trimmed().isEmpty()) {
        return {};
    }

    const auto stateIndexIt = input.stateIndexByBodyId->constFind(
        normalizedSceneLookupKey(targetId)
    );
    if (stateIndexIt == input.stateIndexByBodyId->cend()) {
        return {};
    }

    const auto& state = input.snapshot->states.at(*stateIndexIt);
    const auto& body = input.snapshot->bodyAt(state.bodyIndex);
    if (body.displayName.empty()) {
        return {};
    }

    double inspectorX = input.inspectorPinnedX;
    double inspectorY = input.inspectorPinnedY;
    if (!input.inspectorPinned) {
        if (!state.horizontal.isFinite()) {
            return {};
        }

        const auto projected = input.preparedProjection->project(state.horizontal);
        if (!projected.isVisible) {
            return {};
        }

        inspectorX = projected.x + 18.0;
        inspectorY = projected.y + 18.0;
    }

    QVariantList fields;
    fields.push_back(inspectorField("Type", celestialBodyTypeText(body)));
    fields.push_back(inspectorField("Magnitude", formatMagnitude(body.visualMagnitude)));
    fields.push_back(inspectorField("Alt / Az", formatHorizontalCoordinate(state.horizontal)));
    fields.push_back(inspectorField("RA / Dec", formatEquatorialCoordinate(state.equatorial)));
    appendObservationEventFields(fields, input, body, state.bodyIndex);

    if (body.deepSkyObject.has_value()) {
        const QString sizeText = angularSizeText(*body.deepSkyObject);
        if (!sizeText.isEmpty()) {
            fields.push_back(inspectorField("Angular size", sizeText));
        }
    }

    fields.push_back(inspectorField(
        "Source",
        sourceLabelForBodyIndex(input.catalogSourceIds, input.catalogSourceLabels, state.bodyIndex)
    ));

    QVariantMap inspector;
    inspector.insert("visible", true);
    inspector.insert("x", inspectorX);
    inspector.insert("y", inspectorY);
    inspector.insert("targetKind", "body");
    inspector.insert("targetId", QString::fromStdString(body.id));
    inspector.insert("title", QString::fromStdString(body.displayName));
    inspector.insert("pinned", input.inspectorPinned);
    inspector.insert("fields", fields);
    inspector.insert("aliases", aliasesText(body));
    return inspector;
}

QString SkySelectionOverlayBuilder::activeTrailTargetBodyId(
    const SkySelectionOverlayInput& input
) const
{
    if (!input.selectedObjectTargetId.trimmed().isEmpty()) {
        return input.selectedObjectTargetId;
    }

    if (
        normalizedSceneLookupKey(input.trackedTargetKind) == "body"
        && !input.trackedTargetId.trimmed().isEmpty()
    ) {
        return input.trackedTargetId;
    }

    if (
        normalizedSceneLookupKey(input.selectedSearchTargetKind) == "body"
        && !input.selectedSearchTargetId.trimmed().isEmpty()
    ) {
        return input.selectedSearchTargetId;
    }

    return {};
}

std::optional<std::uint32_t> SkySelectionOverlayBuilder::activeTrailTargetBodyIndex(
    const SkySelectionOverlayInput& input
) const
{
    if (input.snapshot == nullptr || input.stateIndexByBodyId == nullptr) {
        return std::nullopt;
    }

    const QString targetBodyId = activeTrailTargetBodyId(input);
    if (targetBodyId.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const auto stateIndexIt = input.stateIndexByBodyId->constFind(
        normalizedSceneLookupKey(targetBodyId)
    );
    if (stateIndexIt == input.stateIndexByBodyId->cend()) {
        return std::nullopt;
    }

    return input.snapshot->states.at(*stateIndexIt).bodyIndex;
}
