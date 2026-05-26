#pragma once

#include "PreparedProjection.hpp"
#include "SkyContext.hpp"
#include "SkySceneOverlayData.hpp"
#include "Types.hpp"

#include "engine/IEphemerisEngine.hpp"

#include "catalog/constellation/ConstellationData.hpp"

#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <span>

class SkyTimeController;

struct SkySelectionOverlayInput final {
    const skygate::ephemeris::SkySnapshot* snapshot = nullptr;
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
    const SkyTimeController* timeController = nullptr;
    const skygate::core::PreparedProjection* preparedProjection = nullptr;
    const QHash<QString, std::size_t>* stateIndexByBodyId = nullptr;
    std::optional<skygate::core::SkyContext> skyContext;
    std::optional<skygate::ephemeris::EphemerisRequest> ephemerisRequest;
    std::span<const skygate::ephemeris::ConstellationAnchorGroup> constellationAnchorGroups;
    std::span<const std::uint8_t> catalogSourceIds;
    QStringList catalogSourceLabels;
    QString selectedObjectTargetId;
    QString selectedSearchTargetKind;
    QString selectedSearchTargetId;
    QString trackedTargetKind;
    QString trackedTargetId;
    double inspectorPinnedX = 0.0;
    double inspectorPinnedY = 0.0;
    bool inspectorPinned = false;
};

class SkySelectionOverlayBuilder final {
public:
    [[nodiscard]] SkySelectionMarker buildSelectionMarkerData(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] SkySelectedObjectInspector
    buildSelectedObjectInspectorData(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] QString activeTrailTargetBodyId(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] std::optional<std::uint32_t> activeTrailTargetBodyIndex(const SkySelectionOverlayInput& input) const;
};
