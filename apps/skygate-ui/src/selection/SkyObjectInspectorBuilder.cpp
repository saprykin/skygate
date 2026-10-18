#include "SkyObjectInspectorBuilder.hpp"
#include "EphemerisRequestFactory.hpp"
#include "ObservationEventCalculator.hpp"
#include "SkyObjectInspectorFormatters.hpp"
#include "SkySelectionOverlayInput.hpp"
#include "SkySelectionTargetKind.hpp"
#include "diagnostics/SkyPerformanceLogging.hpp"
#include "diagnostics/SkyPerformanceStageTimer.hpp"
#include "engine/EphemerisComputationPolicy.hpp"
#include "scene/SkyInspectorField.hpp"
#include "scene/SkySelectedObjectInspector.hpp"

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

    skygate::ui::SkyPerformanceStageTimer timer;

    const auto events = observationEventsForInspector(input, body, bodyIndex);
    if (skygate::ui::SkyPerformanceLogging::enabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "object inspector events elapsedMs=" << timer.elapsedMilliseconds()
            << "bodyId=" << QString::fromStdString(body.id) << "request=" << input.ephemerisRequest.has_value();
    }
    fields.push_back(inspectorField(
        "Rise",
        skygate::ui::internal::SkyObjectInspectorFormatters::formatObservationEvent(
            events.nextRise, input.timeController
        )
    ));
    fields.push_back(inspectorField(
        "Set",
        skygate::ui::internal::SkyObjectInspectorFormatters::formatObservationEvent(
            events.nextSet, input.timeController
        )
    ));
    fields.push_back(inspectorField(
        "Culmination",
        skygate::ui::internal::SkyObjectInspectorFormatters::formatObservationCulmination(
            events.culmination, input.timeController
        )
    ));
}

skygate::ephemeris::ObservationEventCalculator::SearchMode observationEventSearchModeForInspector(
    const SkySelectionOverlayInput& input, const skygate::ephemeris::BaseCelestialBody& body
) noexcept
{
    if (input.ephemerisEngine == nullptr || !input.ephemerisRequest.has_value()) {
        return skygate::ephemeris::ObservationEventCalculator::SearchMode::Guided;
    }

    return skygate::ephemeris::EphemerisComputationPolicy::inspectorEventSearchMode(
        *input.ephemerisEngine, *input.ephemerisRequest, &body
    );
}

struct EphemerisInspectorMetadata final {
    QString status;
    QString warningText;
    QString provenance;
    QString dataRange;
    QString uncertainty;
    QString corrections;
};

bool shouldComputeInspectorDetailState(const SkySelectionOverlayInput& input) noexcept
{
    return input.ephemerisEngine != nullptr && input.ephemerisRequest.has_value()
           && skygate::ephemeris::EphemerisComputationPolicy::shouldRecomputeInspectorDetail(
               *input.ephemerisEngine, *input.ephemerisRequest
           );
}

skygate::ephemeris::ObservationEventSummary observationEventsForInspector(
    const SkySelectionOverlayInput& input,
    const skygate::ephemeris::BaseCelestialBody& body,
    const std::uint32_t bodyIndex
)
{
    const skygate::ephemeris::ObservationEventCalculator calculator(input.guidanceStrategy);
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
    if (!shouldComputeInspectorDetailState(input)) {
        return sceneState;
    }

    const auto preciseState = input.ephemerisEngine->computeBodyState(
        *input.ephemerisRequest, static_cast<std::size_t>(sceneState.bodyIndex)
    );
    return preciseState.value_or(sceneState);
}

EphemerisInspectorMetadata
buildEphemerisInspectorMetadata(const skygate::ephemeris::EphemerisEngineQueryResult& metadata)
{
    EphemerisInspectorMetadata result;
    result.status = skygate::ui::internal::SkyObjectInspectorFormatters::formatEphemerisStatus(metadata.status);
    result.warningText = skygate::ui::internal::SkyObjectInspectorFormatters::formatEphemerisWarnings(metadata);
    if (!metadata.dataSourceProvenance.empty()) {
        result.provenance = QString::fromStdString(metadata.dataSourceProvenance);
    }
    if (metadata.effectiveDataValidityRange.has_value()) {
        result.dataRange = skygate::ui::internal::SkyObjectInspectorFormatters::formatEphemerisDateRange(
            *metadata.effectiveDataValidityRange
        );
    }
    if (metadata.estimatedAngularUncertaintyArcsec.has_value()) {
        result.uncertainty = skygate::ui::internal::SkyObjectInspectorFormatters::formatAngularUncertaintyArcsec(
            *metadata.estimatedAngularUncertaintyArcsec
        );
    }
    result.corrections = skygate::ui::internal::SkyObjectInspectorFormatters::formatCorrectionSummary(metadata);
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
    skygate::ui::SkyPerformanceStageTimer timer;

    if (!input.hasRequiredSnapshotInputs()) {
        return {};
    }

    const QString targetId =
        !input.selectedObjectTargetId.isEmpty()
            ? input.selectedObjectTargetId
            : (skygate::ui::internal::SkySelectionTargetKind::isBody(input.selectedSearchTargetKind)
                   ? input.selectedSearchTargetId
                   : QString());
    if (targetId.trimmed().isEmpty()) {
        return {};
    }

    const auto stateIndex = input.stateIndexForBodyId(targetId);
    if (!stateIndex.has_value()) {
        return {};
    }

    const auto& sceneState = input.snapshot->states.at(*stateIndex);
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
        if (!projected.isVisible()) {
            return {};
        }

        inspectorX = projected.x + 18.0;
        inspectorY = projected.y + 18.0;
    }

    const double lookupMs = timer.stageMilliseconds();
    const skygate::ephemeris::CelestialBodyState state = detailedInspectorState(input, sceneState);
    const double detailStateMs = timer.stageMilliseconds();

    std::vector<SkyInspectorField> fields;
    fields.push_back(
        inspectorField("Type", skygate::ui::internal::SkyObjectInspectorFormatters::celestialBodyTypeText(body))
    );
    fields.push_back(inspectorField(
        "Magnitude", skygate::ui::internal::SkyObjectInspectorFormatters::formatMagnitude(body.visualMagnitude)
    ));
    fields.push_back(inspectorField(
        "Alt / Az", skygate::ui::internal::SkyObjectInspectorFormatters::formatHorizontalCoordinate(state.horizontal)
    ));
    fields.push_back(inspectorField(
        "RA / Dec", skygate::ui::internal::SkyObjectInspectorFormatters::formatEquatorialCoordinate(state.equatorial)
    ));
    const EphemerisInspectorMetadata ephemerisMetadata = buildEphemerisInspectorMetadata(state.metadata);
    appendEphemerisMetadataFields(fields, ephemerisMetadata, state.metadata.status);
    appendObservationEventFields(fields, input, body, sceneState.bodyIndex);

    if (body.deepSkyObjectValue().has_value()) {
        const QString sizeText =
            skygate::ui::internal::SkyObjectInspectorFormatters::angularSizeText(*body.deepSkyObjectValue());
        if (!sizeText.isEmpty()) {
            fields.push_back(inspectorField("Angular size", sizeText));
        }
    }

    fields.push_back(inspectorField(
        "Source",
        skygate::ui::internal::SkyObjectInspectorFormatters::sourceLabelForBodyIndex(
            input.catalogSourceIds, input.catalogSourceContributors, input.catalogSourceTitles, sceneState.bodyIndex
        )
    ));
    const double fieldsMs = timer.stageMilliseconds();

    if (skygate::ui::SkyPerformanceLogging::enabled()) {
        qCInfo(skygate::ui::skygatePerfLog) << "object inspector build elapsedMs=" << timer.elapsedMilliseconds()
                                            << "lookupMs=" << lookupMs << "detailStateMs=" << detailStateMs
                                            << "fieldsMs=" << fieldsMs << "bodyId=" << QString::fromStdString(body.id);
    }

    return SkySelectedObjectInspector{
        .visible = true,
        .x = inspectorX,
        .y = inspectorY,
        .targetKind = skygate::ui::internal::SkySelectionTargetKind::token(
            skygate::ui::internal::SkySelectionTargetKind::Type::Body
        ),
        .targetId = QString::fromStdString(body.id),
        .title = QString::fromStdString(body.displayName),
        .pinned = input.inspectorPinned,
        .fields = std::move(fields),
        .aliases = skygate::ui::internal::SkyObjectInspectorFormatters::aliasesText(body),
        .ephemerisStatus = ephemerisMetadata.status,
        .ephemerisWarningText = ephemerisMetadata.warningText,
        .ephemerisProvenance = ephemerisMetadata.provenance,
        .ephemerisDataRange = ephemerisMetadata.dataRange,
        .ephemerisUncertainty = ephemerisMetadata.uncertainty,
        .ephemerisCorrections = ephemerisMetadata.corrections
    };
}
