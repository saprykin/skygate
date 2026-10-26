#include "SkyCatalogConstellationStore.hpp"

#include <utility>

namespace skygate::ui::internal {

bool SkyCatalogConstellationStore::setDataset(
    std::vector<ConstellationLineRef> lineRefs,
    std::vector<ConstellationAnchorGroup> anchorGroups,
    const std::size_t count
)
{
    if (m_lineRefs == lineRefs && m_anchorGroups == anchorGroups && m_count == count) {
        return false;
    }

    m_lineRefs = std::move(lineRefs);
    m_anchorGroups = std::move(anchorGroups);
    m_count = count;
    ++m_revision;
    return true;
}

bool SkyCatalogConstellationStore::clear()
{
    if (m_lineRefs.empty() && m_anchorGroups.empty() && m_count == 0U) {
        return false;
    }

    m_lineRefs.clear();
    m_anchorGroups.clear();
    m_count = 0U;
    ++m_revision;
    return true;
}

std::uint64_t SkyCatalogConstellationStore::revision() const noexcept
{
    return m_revision;
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
