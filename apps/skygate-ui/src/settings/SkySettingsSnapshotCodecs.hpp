#pragma once

#include "SkyPersistedStateSnapshot.hpp"
#include "SkyStateSettingsSnapshot.hpp"

#include <optional>

class QSettings;

namespace skygate::ui::internal {

[[nodiscard]] SkyStateSettingsSnapshot splitStateSnapshot(const SkyPersistedStateSnapshot& snapshot);
[[nodiscard]] SkyPersistedStateSnapshot mergeStateSnapshot(const SkyStateSettingsSnapshot& snapshot);
void saveStateSnapshot(QSettings& settings, const SkyPersistedStateSnapshot& snapshot);
[[nodiscard]] std::optional<SkyPersistedStateSnapshot> loadStateSnapshot(QSettings& settings);

}  // namespace skygate::ui::internal
