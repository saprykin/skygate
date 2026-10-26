#pragma once

#include "SkyPersistedStateSnapshot.hpp"
#include "SkyStateSettingsSnapshot.hpp"

#include <optional>

class QSettings;

namespace skygate::ui::internal {

class SkySettingsSnapshotCodecs final {
public:
    [[nodiscard]] static SkyStateSettingsSnapshot splitStateSnapshot(const SkyPersistedStateSnapshot& snapshot);
    [[nodiscard]] static SkyPersistedStateSnapshot mergeStateSnapshot(const SkyStateSettingsSnapshot& snapshot);
    static void saveStateSnapshot(QSettings& settings, const SkyPersistedStateSnapshot& snapshot);
    [[nodiscard]] static std::optional<SkyPersistedStateSnapshot> loadStateSnapshot(QSettings& settings);
    [[nodiscard]] static SkyLoggingSettingsSnapshot loadLoggingSettings(QSettings& settings);
};

}  // namespace skygate::ui::internal
