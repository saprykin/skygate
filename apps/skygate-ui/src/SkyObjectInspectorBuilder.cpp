#include "SkyObjectInspectorBuilder.hpp"

#include "SkyObjectInspectorFormatters.hpp"
#include "SkySceneShared.hpp"

#include "skygate/ephemeris/ObservationEventCalculator.hpp"

#include <utility>
#include <vector>

namespace {

SkyInspectorField inspectorField(const QString& label, const QString& value)
{
    return SkyInspectorField {
        .label = label,
        .value = value
    };
}

bool hasSelectionInputs(const SkySelectionOverlayInput& input)
{
    return input.snapshot != nullptr
        && input.preparedProjection != nullptr
        && input.stateIndexByBodyId != nullptr;
}

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

    const skygate::ephemeris::ObservationEventCalculator calculator;
    const auto events = calculator.compute(
        *input.ephemerisEngine,
        *input.skyContext,
        bodyIndex,
        body
    );
    fields.push_back(inspectorField(
        "Rise",
        skygate::ui::internal::formatObservationEvent(events.nextRise, input.timeController)
    ));
    fields.push_back(inspectorField(
        "Set",
        skygate::ui::internal::formatObservationEvent(events.nextSet, input.timeController)
    ));
    fields.push_back(inspectorField(
        "Culmination",
        skygate::ui::internal::formatObservationCulmination(
            events.culmination,
            input.timeController
        )
    ));
}

}  // namespace

SkySelectedObjectInspector SkyObjectInspectorBuilder::build(
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

    std::vector<SkyInspectorField> fields;
    fields.push_back(inspectorField(
        "Type",
        skygate::ui::internal::celestialBodyTypeText(body)
    ));
    fields.push_back(inspectorField(
        "Magnitude",
        skygate::ui::internal::formatMagnitude(body.visualMagnitude)
    ));
    fields.push_back(inspectorField(
        "Alt / Az",
        skygate::ui::internal::formatHorizontalCoordinate(state.horizontal)
    ));
    fields.push_back(inspectorField(
        "RA / Dec",
        skygate::ui::internal::formatEquatorialCoordinate(state.equatorial)
    ));
    appendObservationEventFields(fields, input, body, state.bodyIndex);

    if (body.deepSkyObject.has_value()) {
        const QString sizeText = skygate::ui::internal::angularSizeText(*body.deepSkyObject);
        if (!sizeText.isEmpty()) {
            fields.push_back(inspectorField("Angular size", sizeText));
        }
    }

    fields.push_back(inspectorField(
        "Source",
        skygate::ui::internal::sourceLabelForBodyIndex(
            input.catalogSourceIds,
            input.catalogSourceLabels,
            state.bodyIndex
        )
    ));

    return SkySelectedObjectInspector {
        .visible = true,
        .x = inspectorX,
        .y = inspectorY,
        .targetKind = "body",
        .targetId = QString::fromStdString(body.id),
        .title = QString::fromStdString(body.displayName),
        .pinned = input.inspectorPinned,
        .fields = std::move(fields),
        .aliases = skygate::ui::internal::aliasesText(body)
    };
}
