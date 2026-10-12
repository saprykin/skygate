#pragma once

#include "EphemerisRequest.hpp"
#include "SkySceneFramePipelineInput.hpp"

#include <QHash>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <span>

class SkyTimeController;

namespace skygate::ephemeris {
class IEphemerisGuidanceStrategy;
}  // namespace skygate::ephemeris

struct SkySceneCompositionInput final {
    SkySceneFramePipelineInput frameInput;
    std::optional<skygate::ephemeris::EphemerisRequest> selectionEphemerisRequest;
    std::span<const QString> catalogSourceIds;
    const std::vector<QStringList>* catalogSourceContributors = nullptr;
    QHash<QString, QString> catalogSourceTitles;
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
    std::shared_ptr<skygate::ephemeris::IEphemerisGuidanceStrategy> guidanceStrategy;
};
