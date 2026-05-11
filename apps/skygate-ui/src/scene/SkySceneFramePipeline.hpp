#pragma once

#include <QHash>
#include <QString>

#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderBuilders.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionTypes.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace skygate::ephemeris {
class IEphemerisEngine;
}

struct SkySceneFramePipelineInput final {
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
    skygate::core::SkyContext skyContext;
    std::uint64_t catalogRevision = 0;
    skygate::core::ProjectionType projectionType =
        skygate::core::ProjectionType::Stereographic;
    double viewCenterAltitudeDeg = 0.0;
    double viewCenterAzimuthDeg = 0.0;
    double viewFieldOfViewDeg = 0.0;
    double magnitudeCutoff = 0.0;
    QString themeId;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    SkyOverlayLayerVisibility overlayLayers;
    std::span<const skygate::ephemeris::ConstellationLineRef> constellationLineRefs;
    std::span<const skygate::ephemeris::ConstellationLabelRef> constellationLabelRefs;
};

struct SkySceneFramePipelineResult final {
    bool updated = false;
    std::uint64_t snapshotGeneration = 0;
    std::uint64_t renderFrameGeneration = 0;
    const skygate::core::PreparedProjection* preparedProjection = nullptr;
    const skygate::ephemeris::SkySnapshot* snapshot = nullptr;
    const SkyRenderFrame* frame = nullptr;
    const QHash<QString, std::size_t>* stateIndexByBodyId = nullptr;
};

class SkySceneFramePipeline final {
public:
    [[nodiscard]] std::optional<SkySceneFramePipelineResult> rebuild(
        const SkySceneFramePipelineInput& input,
        double viewportWidth,
        double viewportHeight
    );
    [[nodiscard]] bool clear();
    [[nodiscard]] std::uint64_t snapshotGeneration() const noexcept;

private:
    struct SnapshotCacheKey final {
        std::uint64_t catalogRevision = 0;
        skygate::core::GeoLocation observer;
        skygate::core::UtcTimePoint utcTime {};

        [[nodiscard]] bool equals(const SnapshotCacheKey& other) const noexcept;
    };

    struct RenderFrameKey final {
        std::uint64_t snapshotGeneration = 0;
        skygate::core::ProjectionType projectionType =
            skygate::core::ProjectionType::Stereographic;
        double viewportWidth = 0.0;
        double viewportHeight = 0.0;
        double viewCenterAltitudeDeg = 0.0;
        double viewCenterAzimuthDeg = 0.0;
        double viewFieldOfViewDeg = 0.0;
        double magnitudeCutoff = 0.0;
        QString themeId;
        SkyOverlayLayerVisibility overlayLayers;

        [[nodiscard]] bool equals(const RenderFrameKey& other) const noexcept;
    };

    const skygate::ephemeris::IEphemerisEngine* m_cachedEphemerisEngine = nullptr;
    std::optional<SnapshotCacheKey> m_snapshotCacheKey;
    std::optional<RenderFrameKey> m_renderFrameKey;
    std::optional<skygate::core::PreparedProjection> m_preparedProjection;
    skygate::ephemeris::SkySnapshot m_snapshot;
    SkyRenderFrame m_frame;
    QHash<QString, std::size_t> m_stateIndexByBodyId;
    std::uint64_t m_snapshotGeneration = 0;
    std::uint64_t m_renderFrameGeneration = 0;
};
