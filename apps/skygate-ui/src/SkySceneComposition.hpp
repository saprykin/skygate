#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "SkyObjectTrailBuilder.hpp"
#include "SkySceneFramePipeline.hpp"
#include "SkySelectionOverlayBuilder.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <span>

class SkyTimeController;

struct SkySceneFrameData final {
    std::optional<skygate::core::PreparedProjection> preparedProjection;
    const skygate::ephemeris::SkySnapshot* snapshot = nullptr;
    SkyRenderFrame frame;
    QVariantList overlayItems;
    QVariantMap selectionMarker;
    QVariantMap selectedObjectInspector;
};

struct SkySceneCompositionInput final {
    SkySceneFramePipelineInput frameInput;
    std::span<const std::uint8_t> catalogSourceIds;
    QStringList catalogSourceLabels;
    const SkyTimeController* timeController = nullptr;
    QString selectedObjectTargetId;
    QString selectedSearchTargetKind;
    QString selectedSearchTargetId;
    QString trackedTargetKind;
    QString trackedTargetId;
    double inspectorPinnedX = 0.0;
    double inspectorPinnedY = 0.0;
    bool inspectorPinned = false;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
};

struct SkySceneCompositionResult final {
    bool changed = false;
    bool frameContentChanged = false;
};

class SkySceneComposer final {
public:
    void reset();

    [[nodiscard]] SkySceneCompositionResult rebuild(
        SkySceneFrameData& sceneFrame,
        const SkySceneCompositionInput& input,
        const SkySceneFramePipelineResult& frameResult
    );

private:
    struct CompositionKey final {
        std::uint64_t renderFrameGeneration = 0;
        std::optional<std::uint32_t> trailTargetBodyIndex;
        QString selectedObjectTargetId;
        QString selectedSearchTargetKind;
        QString selectedSearchTargetId;
        QString trackedTargetKind;
        QString trackedTargetId;
        double inspectorPinnedX = 0.0;
        double inspectorPinnedY = 0.0;
        bool inspectorPinned = false;

        [[nodiscard]] bool equals(const CompositionKey& other) const noexcept;
    };

    [[nodiscard]] QVariantList buildOverlayItems(
        const SkySceneFrameData& sceneFrame,
        const SkySceneCompositionInput& input
    ) const;
    [[nodiscard]] SkySelectionOverlayInput buildSelectionInput(
        const SkySceneCompositionInput& input,
        const SkySceneFramePipelineResult& frameResult
    ) const;

private:
    SkySelectionOverlayBuilder m_selectionOverlayBuilder;
    SkyObjectTrailBuilder m_objectTrailBuilder;
    std::optional<CompositionKey> m_compositionKey;
};
