#include "SkyViewportSnapshotPublisher.hpp"

#include <utility>

std::shared_ptr<const SkyViewportSnapshot> SkyViewportSnapshotPublisher::snapshot() const noexcept
{
    return m_snapshot;
}

bool SkyViewportSnapshotPublisher::publish(const SkyViewportSnapshotInput& input)
{
    if (m_snapshot != nullptr && input.renderFrameGeneration == m_renderFrameGeneration
        && input.trailTargetBodyIndex == m_trailTargetBodyIndex) {
        return false;
    }

    m_renderFrameGeneration = input.renderFrameGeneration;
    m_trailTargetBodyIndex = input.trailTargetBodyIndex;

    auto snapshot = std::make_shared<SkyViewportSnapshot>();
    snapshot->generation = ++m_snapshotGeneration;
    snapshot->viewportWidth = input.viewportWidth;
    snapshot->viewportHeight = input.viewportHeight;
    snapshot->preparedProjection = input.preparedProjection;
    snapshot->observationContext = input.observationContext;
    snapshot->renderTheme = input.renderTheme;
    snapshot->overlayLayers = input.overlayLayers;
    snapshot->points.assign(input.points.begin(), input.points.end());
    snapshot->lines.assign(input.lines.begin(), input.lines.end());
    snapshot->glyphs.assign(input.glyphs.begin(), input.glyphs.end());
    m_snapshot = std::move(snapshot);
    return true;
}

bool SkyViewportSnapshotPublisher::clear()
{
    if (m_snapshot == nullptr) {
        return false;
    }

    m_snapshot.reset();
    m_renderFrameGeneration = 0;
    m_trailTargetBodyIndex.reset();
    return true;
}

void SkyViewportSnapshotPublisher::invalidatePublishedFrame()
{
    m_renderFrameGeneration = 0;
    m_trailTargetBodyIndex.reset();
}
