#pragma once

#include "SkySearchState.hpp"

#include <QString>

namespace skygate::ui::internal {

class SkySearchController final {
public:
    [[nodiscard]] bool toolbarCollapsed() const noexcept;
    [[nodiscard]] QString selectedTargetKind() const;
    [[nodiscard]] QString selectedTargetId() const;
    [[nodiscard]] bool hasTrackedTarget() const;
    [[nodiscard]] QString trackedTargetKind() const;
    [[nodiscard]] QString trackedTargetId() const;
    [[nodiscard]] QString trackedTargetDisplayText() const;

    [[nodiscard]] bool setToolbarCollapsed(bool toolbarCollapsed) noexcept;
    [[nodiscard]] bool setSelectedTarget(const QString& targetKind, const QString& targetId);
    [[nodiscard]] bool setTrackedTarget(const QString& targetKind, const QString& targetId, const QString& displayText);

private:
    SkySearchState m_state;
};

}  // namespace skygate::ui::internal
