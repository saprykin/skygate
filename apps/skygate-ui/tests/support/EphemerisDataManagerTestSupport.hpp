#pragma once

#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "ephemeris/SkyEphemerisStatusText.hpp"

#include <QString>

namespace skygate::ui::tests {

// The data manager publishes the source its active data came from only through
// the status line it renders, so the suites read the same production-visible
// statement the UI shows instead of a separate accessor.
[[nodiscard]] inline bool usesInstalledData(const SkyEphemerisDataManager& manager)
{
    return manager.statusText() == skygate::ui::internal::SkyEphemerisStatusText::installedDataActive();
}

}  // namespace skygate::ui::tests
