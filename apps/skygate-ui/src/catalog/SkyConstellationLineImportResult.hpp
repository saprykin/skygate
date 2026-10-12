#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <QString>

#include <cstddef>
#include <vector>

namespace skygate::ui::internal {

struct SkyConstellationLineImportResult final {
    std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs;
    std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups;
    std::size_t constellationCount = 0;
    QString statusSuffix;

    [[nodiscard]] bool hasCustomLines() const noexcept;
};

}  // namespace skygate::ui::internal
