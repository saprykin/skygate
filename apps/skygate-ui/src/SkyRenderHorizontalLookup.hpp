#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace skygate::ui::internal {

class SkyRenderHorizontalLookup final {
public:
    SkyRenderHorizontalLookup(
        std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
        std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs
    );

    void capture(
        const skygate::ephemeris::CelestialBody& body,
        const skygate::core::HorizontalCoordinate& horizontal
    );

    [[nodiscard]] const skygate::core::HorizontalCoordinate* findHorizontal(
        std::string_view bodyId
    ) const;

private:
    void trackBodyId(std::string_view bodyId);

private:
    std::unordered_set<std::string> m_requiredBodyIds;
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalByBodyId;
};

}  // namespace skygate::ui::internal
