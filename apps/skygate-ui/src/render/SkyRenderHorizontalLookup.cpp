#include "SkyRenderHorizontalLookup.hpp"

namespace skygate::ui::internal {

SkyRenderHorizontalLookup::SkyRenderHorizontalLookup(
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs
)
{
    for (const auto& lineRef : lineRefs) {
        trackBodyId(lineRef.first);
        trackBodyId(lineRef.second);
    }

    for (const auto& labelRef : labelRefs) {
        for (const std::string& hipId : labelRef.second) {
            trackBodyId(hipId);
        }
    }

    m_horizontalByBodyId.reserve(m_requiredBodyIds.size());
}

void SkyRenderHorizontalLookup::capture(
    const skygate::ephemeris::CelestialBody& body,
    const skygate::core::HorizontalCoordinate& horizontal
)
{
    if (!m_requiredBodyIds.empty() && m_requiredBodyIds.contains(body.id)) {
        m_horizontalByBodyId.try_emplace(body.id, horizontal);
    }
}

const skygate::core::HorizontalCoordinate* SkyRenderHorizontalLookup::findHorizontal(
    const std::string_view bodyId
) const
{
    if (const auto idIt = m_horizontalByBodyId.find(bodyId); idIt != m_horizontalByBodyId.end()) {
        return &idIt->second;
    }

    return nullptr;
}

void SkyRenderHorizontalLookup::trackBodyId(const std::string_view bodyId)
{
    if (bodyId.empty()) {
        return;
    }

    m_requiredBodyIds.emplace(bodyId);
}

}  // namespace skygate::ui::internal
