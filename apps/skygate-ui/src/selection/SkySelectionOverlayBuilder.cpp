#include "SkySelectionOverlayBuilder.hpp"

#include "SkyObjectInspectorBuilder.hpp"
#include "SkySelectionMarkerBuilder.hpp"
#include "SkyTrailTargetResolver.hpp"

SkySelectionMarker SkySelectionOverlayBuilder::buildSelectionMarkerData(
    const SkySelectionOverlayInput& input
) const
{
    const SkySelectionMarkerBuilder builder;
    return builder.build(input);
}

SkySelectedObjectInspector SkySelectionOverlayBuilder::buildSelectedObjectInspectorData(
    const SkySelectionOverlayInput& input
) const
{
    const SkyObjectInspectorBuilder builder;
    return builder.build(input);
}

QString SkySelectionOverlayBuilder::activeTrailTargetBodyId(
    const SkySelectionOverlayInput& input
) const
{
    const SkyTrailTargetResolver resolver;
    return resolver.activeTrailTargetBodyId(input);
}

std::optional<std::uint32_t> SkySelectionOverlayBuilder::activeTrailTargetBodyIndex(
    const SkySelectionOverlayInput& input
) const
{
    const SkyTrailTargetResolver resolver;
    return resolver.activeTrailTargetBodyIndex(input);
}
