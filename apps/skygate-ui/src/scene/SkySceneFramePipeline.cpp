#include "SkySceneFramePipeline.hpp"

#include "SkySceneShared.hpp"

#include "math/ViewportMath.hpp"
#include "engine/IEphemerisEngine.hpp"

namespace {

[[nodiscard]] bool
epochsEqual(const skygate::ephemeris::AstronomicalEpoch& lhs, const skygate::ephemeris::AstronomicalEpoch& rhs) noexcept
{
    return lhs.julianDatePart1 == rhs.julianDatePart1 && lhs.julianDatePart2 == rhs.julianDatePart2
           && lhs.timeScale == rhs.timeScale;
}

[[nodiscard]] bool optionalEpochsEqual(
    const std::optional<skygate::ephemeris::AstronomicalEpoch>& lhs,
    const std::optional<skygate::ephemeris::AstronomicalEpoch>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || epochsEqual(*lhs, *rhs);
}

[[nodiscard]] bool optionsEqual(
    const skygate::ephemeris::EphemerisEngineOptions& lhs, const skygate::ephemeris::EphemerisEngineOptions& rhs
) noexcept
{
    return lhs.engineKind == rhs.engineKind && lhs.correctionFlags == rhs.correctionFlags
           && lhs.fallbackToSimpleEngine == rhs.fallbackToSimpleEngine
           && lhs.enableAtmosphericRefraction == rhs.enableAtmosphericRefraction
           && lhs.atmosphericPressureHpa == rhs.atmosphericPressureHpa
           && lhs.atmosphericTemperatureC == rhs.atmosphericTemperatureC && lhs.relativeHumidity == rhs.relativeHumidity
           && lhs.observingWavelengthMicrometers == rhs.observingWavelengthMicrometers;
}

[[nodiscard]] bool optionalOptionsEqual(
    const std::optional<skygate::ephemeris::EphemerisEngineOptions>& lhs,
    const std::optional<skygate::ephemeris::EphemerisEngineOptions>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || optionsEqual(*lhs, *rhs);
}

}  // namespace

bool SkySceneFramePipeline::SnapshotCacheKey::equals(const SnapshotCacheKey& other) const noexcept
{
    return catalogRevision == other.catalogRevision && engineKind == other.engineKind
           && engineOptionsRevision == other.engineOptionsRevision
           && ephemerisDataRevision == other.ephemerisDataRevision
           && earthOrientationDataRevision == other.earthOrientationDataRevision
           && leapSecondDataRevision == other.leapSecondDataRevision
           && observer.latitudeDeg == other.observer.latitudeDeg && observer.longitudeDeg == other.observer.longitudeDeg
           && observer.elevationMeters == other.observer.elevationMeters && utcTime == other.utcTime
           && optionalEpochsEqual(requestEpoch, other.requestEpoch)
           && optionalOptionsEqual(requestOptions, other.requestOptions);
}

bool SkySceneFramePipeline::RenderFrameKey::equals(const RenderFrameKey& other) const noexcept
{
    return snapshotGeneration == other.snapshotGeneration && projectionType == other.projectionType
           && viewportWidth == other.viewportWidth && viewportHeight == other.viewportHeight
           && viewCenterAltitudeDeg == other.viewCenterAltitudeDeg && viewCenterAzimuthDeg == other.viewCenterAzimuthDeg
           && viewFieldOfViewDeg == other.viewFieldOfViewDeg && magnitudeCutoff == other.magnitudeCutoff
           && themeId == other.themeId && overlayLayers.equals(other.overlayLayers);
}

std::optional<SkySceneFramePipelineResult> SkySceneFramePipeline::rebuild(
    const SkySceneFramePipelineInput& input, const double viewportWidth, const double viewportHeight
)
{
    if (input.ephemerisEngine == nullptr || viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return std::nullopt;
    }

    bool updated = false;
    const skygate::core::ProjectionParams projectionParams = skygate::core::ViewportMath::buildProjectionParams(
        viewportWidth, viewportHeight, input.viewCenterAltitudeDeg, input.viewCenterAzimuthDeg, input.viewFieldOfViewDeg
    );
    auto preparedProjection = skygate::core::PreparedProjection::create(input.projectionType, projectionParams);
    if (!preparedProjection.has_value()) {
        return std::nullopt;
    }

    const skygate::core::SkyContext& snapshotContext =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    const SnapshotCacheKey snapshotKey{
        .catalogRevision = input.catalogRevision,
        .engineKind = input.engineKind,
        .engineOptionsRevision = input.engineOptionsRevision,
        .ephemerisDataRevision = input.ephemerisDataRevision,
        .earthOrientationDataRevision = input.earthOrientationDataRevision,
        .leapSecondDataRevision = input.leapSecondDataRevision,
        .observer = snapshotContext.observer,
        .utcTime = snapshotContext.utcTime,
        .requestEpoch = input.ephemerisRequest.has_value() ? std::make_optional(input.ephemerisRequest->epoch)
                                                           : std::optional<skygate::ephemeris::AstronomicalEpoch>{},
        .requestOptions = input.ephemerisRequest.has_value()
                              ? std::make_optional(input.ephemerisRequest->options)
                              : std::optional<skygate::ephemeris::EphemerisEngineOptions>{}
    };
    if (m_cachedEphemerisEngine != input.ephemerisEngine || !m_snapshotCacheKey.has_value()
        || !m_snapshotCacheKey.value().equals(snapshotKey)) {
        m_snapshot = input.ephemerisRequest.has_value() ? input.ephemerisEngine->compute(*input.ephemerisRequest)
                                                        : input.ephemerisEngine->compute(input.skyContext);
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

    const RenderFrameKey renderFrameKey{
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
            input.constellationAnchorGroups,
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

    return SkySceneFramePipelineResult{
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
    const bool hadScene = m_preparedProjection.has_value() || m_snapshot.catalogBodies != nullptr
                          || !m_frame.points.empty() || !m_frame.lines.empty() || !m_frame.glyphs.empty()
                          || !m_frame.labels.empty();
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
