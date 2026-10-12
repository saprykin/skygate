#pragma once

#include "SkyOverlayItem.hpp"
#include "SkySceneCompositionInput.hpp"
#include "SkySceneCompositionResult.hpp"
#include "SkySceneFrameData.hpp"
#include "SkySceneFramePipelineResult.hpp"
#include "selection/SkyObjectTrailBuilder.hpp"
#include "selection/SkySelectionOverlayBuilder.hpp"
#include "selection/SkySelectionOverlayInput.hpp"

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

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

    [[nodiscard]] std::vector<SkyOverlayItem> buildOverlayItems(
        const SkySceneFrameData& sceneFrame,
        const SkySceneCompositionInput& input,
        const SkySceneFramePipelineResult& frameResult
    ) const;
    [[nodiscard]] SkySelectionOverlayInput
    buildSelectionInput(const SkySceneCompositionInput& input, const SkySceneFramePipelineResult& frameResult) const;

private:
    SkySelectionOverlayBuilder m_selectionOverlayBuilder;
    SkyObjectTrailBuilder m_objectTrailBuilder;
    std::optional<CompositionKey> m_compositionKey;
};
