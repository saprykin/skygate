#include "SkySceneFramePipeline.hpp"

#include "SkySceneShared.hpp"

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

bool SkySceneFramePipeline::SnapshotCacheKey::equals(
    const SnapshotCacheKey& other
) const noexcept
{
    return catalogRevision == other.catalogRevision
        && observer.latitudeDeg == other.observer.latitudeDeg
        && observer.longitudeDeg == other.observer.longitudeDeg
        && observer.elevationMeters == other.observer.elevationMeters
        && utcTime == other.utcTime;
}

bool SkySceneFramePipeline::RenderFrameKey::equals(
    const RenderFrameKey& other
) const noexcept
{
    return snapshotGeneration == other.snapshotGeneration
        && projectionType == other.projectionType
        && viewportWidth == other.viewportWidth
        && viewportHeight == other.viewportHeight
        && viewCenterAltitudeDeg == other.viewCenterAltitudeDeg
        && viewCenterAzimuthDeg == other.viewCenterAzimuthDeg
        && viewFieldOfViewDeg == other.viewFieldOfViewDeg
        && magnitudeCutoff == other.magnitudeCutoff
        && themeId == other.themeId
        && overlayLayers.equals(other.overlayLayers);
}

std::optional<SkySceneFramePipelineResult> SkySceneFramePipeline::rebuild(
    const SkySceneFramePipelineInput& input,
    const double viewportWidth,
    const double viewportHeight
)
{
    if (
        input.ephemerisEngine == nullptr
        || viewportWidth <= 0.0
        || viewportHeight <= 0.0
    ) {
        return std::nullopt;
    }

    bool updated = false;
    const skygate::core::ProjectionParams projectionParams =
        skygate::core::ViewportMath::buildProjectionParams(
            viewportWidth,
            viewportHeight,
            input.viewCenterAltitudeDeg,
            input.viewCenterAzimuthDeg,
            input.viewFieldOfViewDeg
        );
    auto preparedProjection = skygate::core::PreparedProjection::create(
        input.projectionType,
        projectionParams
    );
    if (!preparedProjection.has_value()) {
        return std::nullopt;
    }

    const SnapshotCacheKey snapshotKey {
        .catalogRevision = input.catalogRevision,
        .observer = input.skyContext.observer,
        .utcTime = input.skyContext.utcTime
    };
    if (
        m_cachedEphemerisEngine != input.ephemerisEngine
        || !m_snapshotCacheKey.has_value()
        || !m_snapshotCacheKey.value().equals(snapshotKey)
    ) {
        m_snapshot = input.ephemerisEngine->compute(input.skyContext);
        m_stateIndexByBodyId.clear();
        m_stateIndexByBodyId.reserve(static_cast<qsizetype>(m_snapshot.states.size()));
        for (std::size_t stateIndex = 0; stateIndex < m_snapshot.states.size(); ++stateIndex) {
            const auto& state = m_snapshot.states[stateIndex];
            const auto& body = m_snapshot.bodyAt(state.bodyIndex);
            m_stateIndexByBodyId.insert(normalizedSceneLookupKey(body.id), stateIndex);
        }
        m_cachedEphemerisEngine = input.ephemerisEngine;
        m_snapshotCacheKey = snapshotKey;
        m_renderFrameKey.reset();
        ++m_snapshotGeneration;
        updated = true;
    }

    m_preparedProjection = std::move(preparedProjection);

    const RenderFrameKey renderFrameKey {
        .snapshotGeneration = m_snapshotGeneration,
        .projectionType = input.projectionType,
        .viewportWidth = viewportWidth,
        .viewportHeight = viewportHeight,
        .viewCenterAltitudeDeg = input.viewCenterAltitudeDeg,
        .viewCenterAzimuthDeg = input.viewCenterAzimuthDeg,
        .viewFieldOfViewDeg = input.viewFieldOfViewDeg,
        .magnitudeCutoff = input.magnitudeCutoff,
        .themeId = input.themeId,
        .overlayLayers = input.overlayLayers
    };
    if (!m_renderFrameKey.has_value() || !m_renderFrameKey.value().equals(renderFrameKey)) {
        const SkyRenderFrameBuilder frameBuilder;
        m_frame = frameBuilder.buildFrame(
            m_snapshot,
            *m_preparedProjection,
            input.constellationLineRefs,
            input.constellationLabelRefs,
            input.magnitudeCutoff,
            viewportWidth,
            viewportHeight,
            input.renderTheme,
            input.overlayLayers
        );
        m_renderFrameKey = renderFrameKey;
        ++m_renderFrameGeneration;
        updated = true;
    }

    return SkySceneFramePipelineResult {
        .updated = updated,
        .snapshotGeneration = m_snapshotGeneration,
        .renderFrameGeneration = m_renderFrameGeneration,
        .preparedProjection = &*m_preparedProjection,
        .snapshot = &m_snapshot,
        .frame = &m_frame,
        .stateIndexByBodyId = &m_stateIndexByBodyId
    };
}

bool SkySceneFramePipeline::clear()
{
    const bool hadScene = m_preparedProjection.has_value()
        || m_snapshot.catalogBodies != nullptr
        || !m_frame.points.empty()
        || !m_frame.lines.empty()
        || !m_frame.glyphs.empty()
        || !m_frame.labels.isEmpty();
    m_cachedEphemerisEngine = nullptr;
    m_snapshotCacheKey.reset();
    m_renderFrameKey.reset();
    m_preparedProjection.reset();
    m_snapshot = {};
    m_frame = {};
    m_stateIndexByBodyId.clear();
    m_snapshotGeneration = 0;
    m_renderFrameGeneration = 0;
    return hadScene;
}

std::uint64_t SkySceneFramePipeline::snapshotGeneration() const noexcept
{
    return m_snapshotGeneration;
}
