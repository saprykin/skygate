#include "SkyObjectInspectorBuilder.hpp"
#include "EphemerisRequestFactory.hpp"
#include "ObservationEventCalculator.hpp"
#include "SkyObjectInspectorFormatters.hpp"
#include "SkyPerformanceLogging.hpp"
#include "SkySceneShared.hpp"

#include <QElapsedTimer>

#include <utility>
#include <vector>

namespace {

SkyInspectorField inspectorField(const QString& label, const QString& value)
{
    return SkyInspectorField{.label = label, .value = value};
}

SkyInspectorField inspectorField(const QString& label, const QString& value, const QString& tooltip)
{
    return SkyInspectorField{.label = label, .value = value, .tooltip = tooltip};
}

bool hasSelectionInputs(const SkySelectionOverlayInput& input)
{
    return input.snapshot != nullptr && input.preparedProjection != nullptr && input.stateIndexByBodyId != nullptr;
}

skygate::ephemeris::ObservationEventSummary observationEventsForInspector(
    const SkySelectionOverlayInput& input, const skygate::ephemeris::BaseCelestialBody& body, std::uint32_t bodyIndex
);

void appendObservationEventFields(
    std::vector<SkyInspectorField>& fields,
    const SkySelectionOverlayInput& input,
    const skygate::ephemeris::BaseCelestialBody& body,
    const std::uint32_t bodyIndex
)
{
    if (input.ephemerisEngine == nullptr || !input.skyContext.has_value()) {
        return;
    }

    QElapsedTimer timer;
    skygate::ui::startPerformanceTimer(timer);

    const auto events = observationEventsForInspector(input, body, bodyIndex);
    if (skygate::ui::performanceLoggingEnabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "object inspector events elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer)
            << "bodyId=" << QString::fromStdString(body.id) << "request=" << input.ephemerisRequest.has_value();
    }
    fields.push_back(
        inspectorField("Rise", skygate::ui::internal::formatObservationEvent(events.nextRise, input.timeController))
    );
    fields.push_back(
        inspectorField("Set", skygate::ui::internal::formatObservationEvent(events.nextSet, input.timeController))
    );
    fields.push_back(inspectorField(
        "Culmination", skygate::ui::internal::formatObservationCulmination(events.culmination, input.timeController)
    ));
}

skygate::ephemeris::ObservationEventCalculator::SearchMode observationEventSearchModeForInspector(
    const SkySelectionOverlayInput& input, const skygate::ephemeris::BaseCelestialBody& body
) noexcept
{
    if (input.ephemerisEngine != nullptr
        && input.ephemerisEngine->kind() == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
        && input.ephemerisRequest.has_value()
        && input.ephemerisRequest->options.engineKind() == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
        && !body.fixedEquatorialValue().has_value()) {
        return skygate::ephemeris::ObservationEventCalculator::SearchMode::GuidedApproximate;
    }

    return skygate::ephemeris::ObservationEventCalculator::SearchMode::Guided;
}

struct EphemerisInspectorMetadata final {
    QString status;
    QString warningText;
    QString provenance;
    QString dataRange;
    QString uncertainty;
    QString corrections;
};

bool shouldShowHighPrecisionDetails(const SkySelectionOverlayInput& input) noexcept
{
    return input.ephemerisRequest.has_value()
           && input.ephemerisRequest->options.engineKind()
                  == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
}

bool shouldComputeHighPrecisionInspectorState(const SkySelectionOverlayInput& input) noexcept
{
    return input.ephemerisEngine != nullptr
           && input.ephemerisEngine->kind() == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
           && shouldShowHighPrecisionDetails(input);
}

skygate::ephemeris::ObservationEventSummary observationEventsForInspector(
    const SkySelectionOverlayInput& input,
    const skygate::ephemeris::BaseCelestialBody& body,
    const std::uint32_t bodyIndex
)
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    const skygate::ephemeris::ObservationEventCalculator::SearchMode searchMode =
        observationEventSearchModeForInspector(input, body);
    if (input.ephemerisRequest.has_value()) {
        return calculator.compute(*input.ephemerisEngine, *input.ephemerisRequest, bodyIndex, &body, 0.0, searchMode);
    }

    const auto request = skygate::ephemeris::EphemerisRequestFactory::requestFromContext(
        *input.skyContext, input.ephemerisEngine->options()
    );
    return calculator.compute(*input.ephemerisEngine, request, bodyIndex, &body, 0.0, searchMode);
}

skygate::ephemeris::CelestialBodyState
detailedInspectorState(const SkySelectionOverlayInput& input, const skygate::ephemeris::CelestialBodyState& sceneState)
{
    if (!shouldComputeHighPrecisionInspectorState(input)) {
        return sceneState;
    }

    const auto preciseState = input.ephemerisEngine->computeBodyState(
        *input.ephemerisRequest, static_cast<std::size_t>(sceneState.bodyIndex)
    );
    return preciseState.value_or(sceneState);
}

EphemerisInspectorMetadata buildEphemerisInspectorMetadata(
    const skygate::ephemeris::EphemerisEngineQueryResult& metadata, const bool includeHighPrecisionDetails
)
{
    EphemerisInspectorMetadata result;
    result.status = skygate::ui::internal::formatEphemerisStatus(metadata.status);
    result.warningText = skygate::ui::internal::formatEphemerisWarnings(metadata);
    if (includeHighPrecisionDetails && !metadata.dataSourceProvenance.empty()) {
        result.provenance = QString::fromStdString(metadata.dataSourceProvenance);
    }
    if (includeHighPrecisionDetails && metadata.effectiveDataValidityRange.has_value()) {
        result.dataRange = skygate::ui::internal::formatEphemerisDateRange(*metadata.effectiveDataValidityRange);
    }
    if (includeHighPrecisionDetails && metadata.estimatedAngularUncertaintyArcsec.has_value()) {
        result.uncertainty =
            skygate::ui::internal::formatAngularUncertaintyArcsec(*metadata.estimatedAngularUncertaintyArcsec);
    }
    if (includeHighPrecisionDetails) {
        result.corrections = skygate::ui::internal::formatCorrectionSummary(metadata);
    }
    return result;
}

void appendEphemerisMetadataFields(
    std::vector<SkyInspectorField>& fields,
    const EphemerisInspectorMetadata& metadata,
    const skygate::ephemeris::EphemerisEngineQueryStatus::Type status
)
{
    if (status != skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid || !metadata.warningText.isEmpty()) {
        fields.push_back(inspectorField("Ephemeris", metadata.status, metadata.warningText));
    }
    if (!metadata.provenance.isEmpty()) {
        fields.push_back(inspectorField("Provenance", metadata.provenance));
    }
    if (!metadata.dataRange.isEmpty()) {
        fields.push_back(inspectorField("Data range", metadata.dataRange));
    }
    if (!metadata.uncertainty.isEmpty()) {
        fields.push_back(inspectorField("Uncertainty", metadata.uncertainty));
    }
    if (!metadata.corrections.isEmpty()) {
        fields.push_back(inspectorField("Corrections", metadata.corrections));
    }
}

}  // namespace

SkySelectedObjectInspector SkyObjectInspectorBuilder::build(const SkySelectionOverlayInput& input) const
{
    QElapsedTimer timer;
    skygate::ui::startPerformanceTimer(timer);

    if (!hasSelectionInputs(input)) {
        return {};
    }

    const QString targetId =
        !input.selectedObjectTargetId.isEmpty()
            ? input.selectedObjectTargetId
            : (normalizedSceneLookupKey(input.selectedSearchTargetKind) == "body" ? input.selectedSearchTargetId
                                                                                  : QString());
    if (targetId.trimmed().isEmpty()) {
        return {};
    }

    const auto stateIndexIt = input.stateIndexByBodyId->constFind(normalizedSceneLookupKey(targetId));
    if (stateIndexIt == input.stateIndexByBodyId->cend()) {
        return {};
    }

    const auto& sceneState = input.snapshot->states.at(*stateIndexIt);
    const auto& body = input.snapshot->bodyAt(sceneState.bodyIndex);
    if (body.displayName.empty()) {
        return {};
    }

    double inspectorX = input.inspectorPinnedX;
    double inspectorY = input.inspectorPinnedY;
    if (!input.inspectorPinned) {
        if (!sceneState.horizontal.isFinite()) {
            return {};
        }

        const auto projected = input.preparedProjection->project(sceneState.horizontal);
        if (!projected.isVisible) {
            return {};
        }

        inspectorX = projected.x + 18.0;
        inspectorY = projected.y + 18.0;
    }

    const qint64 lookupNs = skygate::ui::performanceElapsedNanoseconds(timer);
    const skygate::ephemeris::CelestialBodyState state = detailedInspectorState(input, sceneState);
    const qint64 detailStateNs = skygate::ui::performanceLoggingEnabled() ? timer.nsecsElapsed() : lookupNs;

    std::vector<SkyInspectorField> fields;
    fields.push_back(inspectorField("Type", skygate::ui::internal::celestialBodyTypeText(body)));
    fields.push_back(inspectorField("Magnitude", skygate::ui::internal::formatMagnitude(body.visualMagnitude)));
    fields.push_back(inspectorField("Alt / Az", skygate::ui::internal::formatHorizontalCoordinate(state.horizontal)));
    fields.push_back(inspectorField("RA / Dec", skygate::ui::internal::formatEquatorialCoordinate(state.equatorial)));
    const EphemerisInspectorMetadata ephemerisMetadata =
        buildEphemerisInspectorMetadata(state.metadata, shouldShowHighPrecisionDetails(input));
    appendEphemerisMetadataFields(fields, ephemerisMetadata, state.metadata.status);
    appendObservationEventFields(fields, input, body, sceneState.bodyIndex);

    if (body.deepSkyObjectValue().has_value()) {
        const QString sizeText = skygate::ui::internal::angularSizeText(*body.deepSkyObjectValue());
        if (!sizeText.isEmpty()) {
            fields.push_back(inspectorField("Angular size", sizeText));
        }
    }

    fields.push_back(inspectorField(
        "Source",
        skygate::ui::internal::sourceLabelForBodyIndex(
            input.catalogSourceIds, input.catalogSourceLabels, sceneState.bodyIndex
        )
    ));
    const qint64 fieldsNs = skygate::ui::performanceLoggingEnabled() ? timer.nsecsElapsed() : detailStateNs;

    if (skygate::ui::performanceLoggingEnabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "object inspector build elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer)
            << "lookupMs=" << skygate::ui::performanceMilliseconds(lookupNs)
            << "detailStateMs=" << skygate::ui::performanceMilliseconds(detailStateNs - lookupNs)
            << "fieldsMs=" << skygate::ui::performanceMilliseconds(fieldsNs - detailStateNs)
            << "bodyId=" << QString::fromStdString(body.id);
    }

    return SkySelectedObjectInspector{
        .visible = true,
        .x = inspectorX,
        .y = inspectorY,
        .targetKind = "body",
        .targetId = QString::fromStdString(body.id),
        .title = QString::fromStdString(body.displayName),
        .pinned = input.inspectorPinned,
        .fields = std::move(fields),
        .aliases = skygate::ui::internal::aliasesText(body),
        .ephemerisStatus = ephemerisMetadata.status,
        .ephemerisWarningText = ephemerisMetadata.warningText,
        .ephemerisProvenance = ephemerisMetadata.provenance,
        .ephemerisDataRange = ephemerisMetadata.dataRange,
        .ephemerisUncertainty = ephemerisMetadata.uncertainty,
        .ephemerisCorrections = ephemerisMetadata.corrections
    };
}
