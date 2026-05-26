#include "SkyCatalogConstellationStore.hpp"

#include <utility>

namespace skygate::ui::internal {

void SkyCatalogConstellationStore::clear()
{
    m_lineRefs.clear();
    m_anchorGroups.clear();
    m_count = 0;
}

void SkyCatalogConstellationStore::setLineRefs(std::vector<ConstellationLineRef> lineRefs)
{
    if (lineRefs.empty()) {
        clear();
        return;
    }

    m_lineRefs = std::move(lineRefs);
}

void SkyCatalogConstellationStore::setAnchorGroups(std::vector<ConstellationAnchorGroup> anchorGroups)
{
    m_anchorGroups = std::move(anchorGroups);
}

void SkyCatalogConstellationStore::setCount(const std::size_t count) noexcept
{
    m_count = count;
}

std::size_t SkyCatalogConstellationStore::count() const noexcept
{
    return m_count;
}

std::span<const SkyCatalogConstellationStore::ConstellationLineRef>
SkyCatalogConstellationStore::lineRefs() const noexcept
{
    return std::span<const ConstellationLineRef>(m_lineRefs);
}

std::span<const SkyCatalogConstellationStore::ConstellationAnchorGroup>
SkyCatalogConstellationStore::anchorGroups() const noexcept
{
    return std::span<const ConstellationAnchorGroup>(m_anchorGroups);
}

const std::vector<SkyCatalogConstellationStore::ConstellationLineRef>&
SkyCatalogConstellationStore::lineRefVector() const noexcept
{
    return m_lineRefs;
}

const std::vector<SkyCatalogConstellationStore::ConstellationAnchorGroup>&
SkyCatalogConstellationStore::anchorGroupVector() const noexcept
{
    return m_anchorGroups;
}

}  // namespace skygate::ui::internal
