#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/Types.hpp"

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
    std::span<const skygate::ephemeris::ConstellationLabelRef> constellationLabelRefs;
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
    [[nodiscard]] QVariantMap buildSelectionMarker(
        const SkySelectionOverlayInput& input
    ) const;
    [[nodiscard]] QVariantMap buildSelectedObjectInspector(
        const SkySelectionOverlayInput& input
    ) const;
    [[nodiscard]] QString activeTrailTargetBodyId(
        const SkySelectionOverlayInput& input
    ) const;
    [[nodiscard]] std::optional<std::uint32_t> activeTrailTargetBodyIndex(
        const SkySelectionOverlayInput& input
    ) const;
};
