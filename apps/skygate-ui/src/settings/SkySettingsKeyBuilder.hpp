#pragma once

#include <QString>

namespace skygate::ui::internal {

// Builds the QSettings keys the UI persists. The persisted state lives under
// the skyContext group; the main window size keeps its own app group.
class SkySettingsKeyBuilder final {
public:
    [[nodiscard]] static QString key(const QString& name);
    [[nodiscard]] static QString key(const char* name);
    [[nodiscard]] static QString nestedKey(const char* prefix, const char* name);
    [[nodiscard]] static QString appKey(const char* name);
};

}  // namespace skygate::ui::internal
