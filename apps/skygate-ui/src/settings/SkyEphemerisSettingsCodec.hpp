#pragma once

#include "SkyEphemerisUserSettingsSnapshot.hpp"

class QSettings;

namespace skygate::ui::internal {

// Persists the ephemeris engine, correction, atmosphere, and update
// preferences under the ephemeris settings keys. The engine kind is stored by
// its stable descriptor id, and a stored data profile id is migrated to the
// canonical spelling on load.
class SkyEphemerisSettingsCodec final {
public:
    static void save(QSettings& settings, const SkyEphemerisUserSettingsSnapshot& snapshot);
    [[nodiscard]] static SkyEphemerisUserSettingsSnapshot load(QSettings& settings);
    [[nodiscard]] static bool hasStoredSettings(QSettings& settings);
};

}  // namespace skygate::ui::internal
