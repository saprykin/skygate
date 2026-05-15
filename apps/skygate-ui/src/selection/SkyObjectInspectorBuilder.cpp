#include "SkyObjectInspectorBuilder.hpp"

#include "SkyObjectInspectorFormatters.hpp"
#include "SkySceneShared.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/ObservationEventCalculator.hpp"

#include <QElapsedTimer>
#include <QLoggingCategory>

#include <array>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {

Q_LOGGING_CATEGORY(skygatePerfLog, "skygate.perf")

bool performanceLoggingEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("SKYGATE_PERF_LOG");
    return enabled;
}

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
    const SkySelectionOverlayInput& input, const skygate::ephemeris::CelestialBody& body, std::uint32_t bodyIndex
);

void appendObservationEventFields(
    std::vector<SkyInspectorField>& fields,
    const SkySelectionOverlayInput& input,
    const skygate::ephemeris::CelestialBody& body,
    const std::uint32_t bodyIndex
)
{
    if (input.ephemerisEngine == nullptr || !input.skyContext.has_value()) {
        return;
    }

    QElapsedTimer timer;
    if (performanceLoggingEnabled()) {
        timer.start();
    }

    const auto events = observationEventsForInspector(input, body, bodyIndex);
    if (performanceLoggingEnabled()) {
        qCInfo(skygatePerfLog) << "object inspector events elapsedMs=" << timer.nsecsElapsed() / 1000000.0
                               << "bodyId=" << QString::fromStdString(body.id)
                               << "request=" << input.ephemerisRequest.has_value();
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
           && input.ephemerisRequest->options.engineKind == skygate::ephemeris::EphemerisEngineKind::HighPrecision;
}

bool shouldComputeHighPrecisionInspectorState(const SkySelectionOverlayInput& input) noexcept
{
    return input.ephemerisEngine != nullptr
           && input.ephemerisEngine->kind() == skygate::ephemeris::EphemerisEngineKind::HighPrecision
           && shouldShowHighPrecisionDetails(input);
}

skygate::ephemeris::ObservationEventSummary observationEventsForInspector(
    const SkySelectionOverlayInput& input, const skygate::ephemeris::CelestialBody& body, const std::uint32_t bodyIndex
)
{
    const skygate::ephemeris::ObservationEventCalculator calculator;
    if (shouldShowHighPrecisionDetails(input) && input.ephemerisRequest.has_value()) {
        const std::array<skygate::ephemeris::CelestialBody, 1> bodies{body};
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> guidanceEngine =
            skygate::ephemeris::createEphemerisEngine(
                std::span<const skygate::ephemeris::CelestialBody>{bodies.data(), bodies.size()}
            );
        if (guidanceEngine != nullptr) {
            return calculator.compute(*guidanceEngine, input.ephemerisRequest->context, 0U, body);
        }
    }

    return input.ephemerisRequest.has_value()
               ? calculator.compute(*input.ephemerisEngine, *input.ephemerisRequest, bodyIndex, body)
               : calculator.compute(*input.ephemerisEngine, *input.skyContext, bodyIndex, body);
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
    const skygate::ephemeris::EphemerisResultMetadata& metadata, const bool includeHighPrecisionDetails
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
    const skygate::ephemeris::EphemerisResultStatus status
)
{
    if (status != skygate::ephemeris::EphemerisResultStatus::Valid || !metadata.warningText.isEmpty()) {
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
    if (performanceLoggingEnabled()) {
        timer.start();
    }

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

    const qint64 lookupNs = performanceLoggingEnabled() ? timer.nsecsElapsed() : 0;
    const skygate::ephemeris::CelestialBodyState state = detailedInspectorState(input, sceneState);
    const qint64 detailStateNs = performanceLoggingEnabled() ? timer.nsecsElapsed() : lookupNs;

    std::vector<SkyInspectorField> fields;
    fields.push_back(inspectorField("Type", skygate::ui::internal::celestialBodyTypeText(body)));
    fields.push_back(inspectorField("Magnitude", skygate::ui::internal::formatMagnitude(body.visualMagnitude)));
    fields.push_back(inspectorField("Alt / Az", skygate::ui::internal::formatHorizontalCoordinate(state.horizontal)));
    fields.push_back(inspectorField("RA / Dec", skygate::ui::internal::formatEquatorialCoordinate(state.equatorial)));
    const EphemerisInspectorMetadata ephemerisMetadata =
        buildEphemerisInspectorMetadata(state.metadata, shouldShowHighPrecisionDetails(input));
    appendEphemerisMetadataFields(fields, ephemerisMetadata, state.metadata.status);
    appendObservationEventFields(fields, input, body, sceneState.bodyIndex);

    if (body.deepSkyObject.has_value()) {
        const QString sizeText = skygate::ui::internal::angularSizeText(*body.deepSkyObject);
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
    const qint64 fieldsNs = performanceLoggingEnabled() ? timer.nsecsElapsed() : detailStateNs;

    if (performanceLoggingEnabled()) {
        qCInfo(skygatePerfLog) << "object inspector build elapsedMs=" << timer.nsecsElapsed() / 1000000.0
                               << "lookupMs=" << lookupNs / 1000000.0
                               << "detailStateMs=" << (detailStateNs - lookupNs) / 1000000.0
                               << "fieldsMs=" << (fieldsNs - detailStateNs) / 1000000.0
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
