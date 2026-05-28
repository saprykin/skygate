#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "HorizontalCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/constellation/ConstellationData.hpp"

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
        std::span<const skygate::ephemeris::ConstellationAnchorGroup> anchorGroups
    );

    void
    capture(const skygate::ephemeris::BaseCelestialBody& body, const skygate::core::HorizontalCoordinate& horizontal);

    [[nodiscard]] const skygate::core::HorizontalCoordinate* findHorizontal(std::string_view bodyId) const;

private:
    void trackBodyId(std::string_view bodyId);

private:
    std::unordered_set<std::string> m_requiredBodyIds;
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalByBodyId;
};

}  // namespace skygate::ui::internal
