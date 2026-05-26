#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace skygate::ui::internal {

class SkyCatalogConstellationStore final {
public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationAnchorGroup = skygate::ephemeris::ConstellationAnchorGroup;

    void clear();
    void setLineRefs(std::vector<ConstellationLineRef> lineRefs);
    void setAnchorGroups(std::vector<ConstellationAnchorGroup> anchorGroups);
    void setCount(std::size_t count) noexcept;

    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> lineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> anchorGroups() const noexcept;
    [[nodiscard]] const std::vector<ConstellationLineRef>& lineRefVector() const noexcept;
    [[nodiscard]] const std::vector<ConstellationAnchorGroup>& anchorGroupVector() const noexcept;

private:
    std::vector<ConstellationLineRef> m_lineRefs;
    std::vector<ConstellationAnchorGroup> m_anchorGroups;
    std::size_t m_count = 0;
};

}  // namespace skygate::ui::internal
