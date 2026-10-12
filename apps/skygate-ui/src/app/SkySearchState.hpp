#pragma once

#include <QString>

namespace skygate::ui::internal {

struct SkySearchState final {
    bool toolbarCollapsed = false;
    QString selectedTargetKind;
    QString selectedTargetId;
    QString trackedTargetKind;
    QString trackedTargetId;
    QString trackedTargetDisplayText;
};

}  // namespace skygate::ui::internal
